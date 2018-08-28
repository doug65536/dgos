#include "elf64.h"
#include "elf64_decl.h"
#include "printk.h"
#include "mm.h"
#include "fileio.h"
#include "elf64_decl.h"
#include "stdlib.h"
#include "string.h"
#include "export.h"
#include "assert.h"
#include "unique_ptr.h"
#include "likely.h"
#include "inttypes.h"
#include "numeric_limits.h"

#define ELF64_DEBUG     1
#if ELF64_DEBUG
#define ELF64_TRACE(...) printdbg(__VA_ARGS__)
#else
#define ELF64_TRACE(...) ((void)0)
#endif

extern Elf64_Sym const ___dynsym_st[];
extern Elf64_Sym const ___dynsym_en[];
extern char const ___dynstr_st[];
extern char const ___dynstr_en[];
extern Elf64_Word const ___hash_st[];
extern Elf64_Word const ___hash_en[];

struct kernel_ht_t {
    Elf64_Word hash_nbucket;
    Elf64_Word hash_nchain;
    Elf64_Word const * hash_buckets;
    Elf64_Word const * hash_chains;
};

static kernel_ht_t export_ht;

static void modload_find_syms(Elf64_Shdr const **symtab,
                              Elf64_Shdr const **strtab,
                              Elf64_Shdr const *scn_hdrs,
                              size_t scn)
{
    *symtab = nullptr;
    *strtab = nullptr;

    Elf64_Word symtab_idx = scn_hdrs[scn].sh_link;
    Elf64_Word strtab_idx = scn_hdrs[symtab_idx].sh_link;

    *symtab = scn_hdrs + symtab_idx;
    *strtab = scn_hdrs + strtab_idx;
}

static Elf64_Sym const *modload_lookup_name(kernel_ht_t *ht, char const *name)
{
    Elf64_Word hash = elf64_hash((unsigned char const*)name);
    Elf64_Word bucket = hash % ht->hash_nbucket;

    Elf64_Word i = ht->hash_buckets[bucket];
    do {
        Elf64_Sym const *chk_sym = ___dynsym_st + i;
        Elf64_Word name_index = chk_sym->st_name;
        char const *chk_name = ___dynstr_st + name_index;
        if (!strcmp(chk_name, name))
            return chk_sym;
        i = ht->hash_chains[i];
    } while (i != 0);

    return nullptr;
}

static Elf64_Sym const *modload_lookup_in_module(
        Elf64_Sym const *mod_sym,
        Elf64_Sym const *end,
        char const *mod_str,
        char const *name)
{
    for (Elf64_Sym const *s = mod_sym; s < end; ++s) {
        if (s->st_name != 0) {
            char const *sym_name = mod_str + s->st_name;
            if (!strcmp(sym_name, name))
                return s;
        }
    }
    return nullptr;
}

static char const * modload_rel_type_text(int type)
{
    switch (type) {
    case R_AMD64_NONE:          return "R_AMD64_NONE";
    case R_AMD64_64:            return "R_AMD64_64";
    case R_AMD64_32S:           return "R_AMD64_32S";
    case R_AMD64_32:            return "R_AMD64_32";
    case R_AMD64_16:            return "R_AMD64_16";
    case R_AMD64_8:             return "R_AMD64_8";
    case R_AMD64_PC64:          return "R_AMD64_PC64";
    case R_AMD64_PC32:          return "R_AMD64_PC32";
    case R_AMD64_PC16:          return "R_AMD64_PC16";
    case R_AMD64_PC8:           return "R_AMD64_PC8";
    case R_AMD64_GOT32:         return "R_AMD64_GOT32";
    case R_AMD64_PLT32:         return "R_AMD64_PLT32";
    case R_AMD64_COPY:          return "R_AMD64_COPY";
    case R_AMD64_GLOB_DAT:      return "R_AMD64_GLOB_DAT";
    case R_AMD64_JUMP_SLOT:     return "R_AMD64_JUMP_SLOT";
    case R_AMD64_RELATIVE:      return "R_AMD64_RELATIVE";
    case R_AMD64_GOTPCREL:      return "R_AMD64_GOTPCREL";
    case R_AMD64_GOTOFF64:      return "R_AMD64_GOTOFF64";
    case R_AMD64_GOTPC32:       return "R_AMD64_GOTPC32";
    case R_AMD64_SIZE32:        return "R_AMD64_SIZE32";
    case R_AMD64_SIZE64:        return "R_AMD64_SIZE64";
    case R_AMD64_REX_GOTPCRELX: return "R_AMD64_REX_GOTPCRELX";
    default:                    return "<?>";
    }
}

void modload_load_symbols(char const *path, uintptr_t addr)
{
    // Force the compiler to believe that the call is necessary
    __asm__ __volatile__ ("" : : "r" (path), "r" (addr));
}

module_entry_fn_t modload_load(char const *path)
{
    file_t fd{file_open(path, O_RDONLY)};

    if (unlikely(!fd.is_open())) {
        printdbg("Failed to open module \"%s\"\n", path);
        return nullptr;
    }

    Elf64_Ehdr file_hdr;

    if (unlikely(sizeof(file_hdr) !=
                 file_read(fd, &file_hdr, sizeof(file_hdr)))) {
        printdbg("Failed to read module file header\n");
        return nullptr;
    }

    ssize_t scn_hdr_size = sizeof(Elf64_Shdr) * file_hdr.e_shnum;
    std::unique_ptr<Elf64_Shdr> scn_hdrs = new Elf64_Shdr[file_hdr.e_shnum];

    if (unlikely(scn_hdr_size != file_pread(
                     fd, scn_hdrs, scn_hdr_size,
                     file_hdr.e_shoff))) {
        printdbg("Error reading program headers\n");
        return nullptr;
    }

    size_t mod_str_scn = 0;
    std::unique_ptr_free<Elf64_Sym> mod_sym;
    Elf64_Sym *mod_sym_end = nullptr;
    std::unique_ptr_free<char> mod_str;

    Elf64_Xword max_addr = 0;
    Elf64_Xword min_addr = (Elf64_Xword)-1;
    for (size_t i = 1; i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr const * const hdr = scn_hdrs + i;

        if (hdr->sh_type == SHT_NULL || hdr->sh_size == 0)
            continue;

        Elf64_Xword addr;

        addr = hdr->sh_addr + hdr->sh_size;
        if (max_addr < addr)
            max_addr = addr;

        addr = hdr->sh_addr;
        if (min_addr > addr)
            min_addr = addr;

        switch (hdr->sh_type) {
        case SHT_SYMTAB:
            ELF64_TRACE("Found symbol table at offset %#" PRIx64
                        ", link=%#x, size=%#" PRIx64 "\n",
                        hdr->sh_offset, hdr->sh_link, hdr->sh_size);

            mod_str_scn = hdr->sh_link;

            mod_sym.reset((Elf64_Sym *)malloc(hdr->sh_size));
            mod_sym_end = mod_sym + hdr->sh_size;
            if ((ssize_t)hdr->sh_size != file_pread(
                        fd, mod_sym, hdr->sh_size, hdr->sh_offset)) {
                printdbg("Error reading module symbols\n");
                return nullptr;
            }
            break;
        case SHT_STRTAB:
            ELF64_TRACE("Found string table at offset %#" PRIx64
                        ", link=%#x, size=%#" PRIx64 "\n",
                        hdr->sh_offset, hdr->sh_link, hdr->sh_size);
            if (i != mod_str_scn)
                break;
            mod_str.reset((char*)malloc(hdr->sh_size));
            if ((ssize_t)hdr->sh_size != file_pread(
                        fd, mod_str, hdr->sh_size, hdr->sh_offset)) {
                printdbg("Error reading module strings\n");
                return nullptr;
            }
            break;
        }
    }

    char *module = (char *)mmap(nullptr, max_addr - min_addr,
                        PROT_READ | PROT_WRITE,
                        MAP_NEAR, -1, 0);

    for (size_t i = 1; i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr const * const hdr = scn_hdrs + i;
        Elf64_Xword vofs = hdr->sh_addr - min_addr;
        char *vaddr = module + vofs;

        if ((hdr->sh_type != SHT_NOBITS) && (hdr->sh_flags & SHF_ALLOC)) {
            if (hdr->sh_size != (size_t)file_pread(
                        fd, vaddr, hdr->sh_size,
                        hdr->sh_offset)) {
                printdbg("Error reading module\n");
                return nullptr;
            }
        }
    }

    // Find the largest relocation section
    size_t max_rel = 0;
    for (size_t i = 1; i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr const * const hdr = scn_hdrs + i;

        // Can't process relocations that do not refer to a section
        if (hdr->sh_info == 0)
            continue;

        switch (hdr->sh_type) {
        case SHT_REL:   // fall thru
        case SHT_RELA:
            if (max_rel < hdr->sh_size)
                max_rel = hdr->sh_size;
            break;
        }
    }

    std::unique_ptr_free<void> rel_buf(malloc(max_rel));

    Elf64_Rel const * const rel = (Elf64_Rel*)rel_buf.get();
    Elf64_Rela const * const rela = (Elf64_Rela*)rel_buf.get();

    // Process relocations
    for (size_t i = 1; i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr const * const hdr = scn_hdrs + i;

        switch (hdr->sh_type) {
        case SHT_REL:   // fall thru
        case SHT_RELA:
            break;

        default:
            continue;
        }

        if ((ssize_t)hdr->sh_size != file_pread(
                    fd, rel_buf, hdr->sh_size, hdr->sh_offset)) {
            printdbg("Error reading module relocations\n");
            return nullptr;
        }

        hex_dump(rel_buf, hdr->sh_size, 0);

        Elf64_Shdr const * const target_scn = scn_hdrs + hdr->sh_info;
        void *end;

        Elf64_Shdr const *symtab;
        Elf64_Shdr const *strtab;

        modload_find_syms(&symtab, &strtab, scn_hdrs, i);

        std::unique_ptr_free<Elf64_Sym> symdata(
                    (Elf64_Sym *)malloc(symtab->sh_size));
        std::unique_ptr_free<char> strdata((char*)malloc(strtab->sh_size));

        if (unlikely((ssize_t)symtab->sh_size != file_pread(
                    fd, symdata, symtab->sh_size, symtab->sh_offset))) {
            printdbg("Error reading symbol table\n");
            return nullptr;
        }

        if (unlikely((ssize_t)strtab->sh_size != file_pread(
                    fd, strdata, strtab->sh_size, strtab->sh_offset))) {
            printdbg("Error reading string table\n");
            return nullptr;
        }

        uint32_t symtab_idx;
        uint32_t rel_type;
        //void *patch;

        // L is the address of the section being relocated
        Elf64_Addr const scn_base = target_scn->sh_addr -
                min_addr + (Elf64_Addr)module;

        switch (hdr->sh_type) {
        case SHT_REL:
            end = (char*)rel + hdr->sh_size;
            for (Elf64_Rel const *r = rel; (void*)r < end; ++r) {
                //symtab_idx = ELF64_R_SYM(r->r_info);
                //rel_type = ELF64_R_TYPE(r->r_info);
                assert(!"Unhandled relocation type!");
                return nullptr;
            }
            break;

        case SHT_RELA:
            end = (char*)rela + hdr->sh_size;
            for (Elf64_Rela const *r = rela; (void*)r < end; ++r) {
                symtab_idx = ELF64_R_SYM(r->r_info);
                rel_type = ELF64_R_TYPE(r->r_info);

                Elf64_Sym *sym = symdata + symtab_idx;
                char const *name = strdata + sym->st_name;

                Elf64_Sym const *match = nullptr;

                int internal = 0;

                if (name[0]) {
                    if (sym->st_shndx == SHN_UNDEF) {
                        match = modload_lookup_name(&export_ht, name);
                    } else  {
                        internal = 1;
                        match = modload_lookup_in_module(
                                    mod_sym, mod_sym_end, mod_str, name);
                    }
                } else {
                    internal = 1;
                }

                Elf64_Addr fixup_addr = scn_base + r->r_offset;

                printdbg("Fixup at %#" PRIx64 ", match=%c,"
                         " name=%s, type=%s (%d), symvalue=%" PRIx64
                         ", addend=%+" PRId64 "\n",
                         fixup_addr, match ? 'y' : 'n',
                         name, modload_rel_type_text(rel_type),
                         rel_type, sym->st_value, r->r_addend);

                bool fixup_is_64 = false;
                bool fixup_is_unsigned = false;
                int64_t fixup64 = 0;
                switch (rel_type) {
                case R_AMD64_PLT32: // L + A - P
                    fixup64 = match->st_value +
                            r->r_addend - fixup_addr;
                    printdbg("...writing %#" PRIx32 "\n", int32_t(fixup64));
                    break;

                case R_AMD64_32S:   // S + A
                    fixup64 = int64_t(module) +
                            scn_hdrs[sym->st_shndx].sh_addr +
                            r->r_addend;
                    printdbg("...writing %#" PRIx32 "\n", int32_t(fixup64));
                    break;

                case R_AMD64_PC32:  // S + A - P
                    if (!match) {
                        fixup64 = (uint64_t)module +
                                scn_hdrs[sym->st_shndx].sh_addr +
                                r->r_addend - fixup_addr;
                    } else {
                        fixup64 = (uint64_t)module +
                                scn_hdrs[sym->st_shndx].sh_addr +
                                match->st_value +
                                r->r_addend - fixup_addr;
                    }
                    printdbg("...writing %#" PRIx32 "\n", int32_t(fixup64));
                    break;

                case R_AMD64_64:
                    fixup_is_64 = true;
                    if (!match) {
                        fixup64 = (uint64_t)module +
                                scn_hdrs[sym->st_shndx].sh_addr +
                                r->r_addend;
                    } else {
                        fixup64 = (uint64_t)module +
                                scn_hdrs[sym->st_shndx].sh_addr +
                                match->st_value + r->r_addend;
                    }
                    printdbg("...writing %#" PRIx64 "\n", fixup64);
                    break;

                case R_AMD64_32:
                    // Untested
                    fixup_is_unsigned = true;
                    if (internal) {
                        fixup64 = uint64_t(module) +
                                scn_hdrs[sym->st_shndx].sh_addr +
                                r->r_addend;
                    } else {
                        fixup64 = uint64_t(module) +
                                scn_hdrs[match->st_shndx].sh_addr +
                                r->r_addend + match->st_value;
                    }
                    printdbg("...writing %#" PRIx32 "\n", uint32_t(fixup64));
                    break;

                default:
                    printdbg("...unhandled relocation, type=%d!\n", rel_type);
                    break;

                }

                bool relocation_truncated = false;
                if (!fixup_is_64) {
                    if (fixup64 < std::numeric_limits<int32_t>::min() ||
                            fixup64 > std::numeric_limits<int32_t>::max()) {
                        relocation_truncated = true;
                    } else if (fixup_is_unsigned) {
                        if (fixup64 < 0 || fixup64 >
                                std::numeric_limits<uint32_t>::max()) {
                            relocation_truncated = true;
                        }
                    }
                }

                if (relocation_truncated) {
                    printdbg("Relocation truncated to fit!\n");
                    munmap(module, max_addr - min_addr);
                    return nullptr;
                }

                if (!fixup_is_64)
                    *(int32_t*)fixup_addr = int32_t(fixup64);
                else
                    *(int64_t*)fixup_addr = fixup64;
            }

            break;
        }
    }

    // Apply page permissions
    for (size_t i = 1; i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr * const hdr = scn_hdrs + i;
        Elf64_Xword vofs = hdr->sh_addr - min_addr;
        char *vaddr = module + vofs;

        if ((hdr->sh_flags & SHF_ALLOC) && hdr->sh_size) {
            int prot = PROT_READ |
                    ((hdr->sh_flags & SHF_WRITE) ? PROT_WRITE: 0) |
                    ((hdr->sh_flags & SHF_EXECINSTR) ? PROT_EXEC : 0);

            printdbg("Setting %c%c%c permissions for %zx bytes at %p\n",
                     (prot & PROT_READ) ? 'r' : '-',
                     (prot & PROT_WRITE) ? 'w' : '-',
                     (prot & PROT_EXEC) ? 'x' : '-',
                     hdr->sh_size, (void*)vaddr);

            mprotect(vaddr, hdr->sh_size, prot);
        } else if (hdr->sh_size) {
//            printdbg("Discarding %zx at %p\n",
//                     hdr->sh_size, (void*)vaddr);
//            madvise(vaddr, hdr->sh_size, MADV_DONTNEED);
//            mprotect(vaddr, hdr->sh_size, PROT_NONE);
        }
    }

    printdbg("add-symbol-file %s %p\n", path, (void*)module);

    // Work around pedantic warning
    module_entry_fn_t fn;
    void *entry = module + file_hdr.e_entry;
    memcpy(&fn, &entry, sizeof(fn));

    modload_load_symbols(path, uintptr_t(module));

    return fn;
}

void modload_init(void)
{
    for (Elf64_Sym const *sym = ___dynsym_st + 1;
         sym < ___dynsym_en; ++sym) {
        printdbg("addr=%p symbol=%s\n",
                 (void*)sym->st_value,
                 ___dynstr_st + sym->st_name);
    }

    export_ht.hash_nbucket = ___hash_st[0];
    export_ht.hash_nchain = ___hash_st[1];
    export_ht.hash_buckets = ___hash_st + 2;
    export_ht.hash_chains = export_ht.hash_buckets +
            export_ht.hash_nbucket;
}

extern "C" void dl_debug_state(void);
EXPORT void dl_debug_state(void)
{
}
