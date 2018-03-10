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
    *symtab = 0;
    *strtab = 0;

    Elf64_Word symtab_idx = scn_hdrs[scn].sh_link;
    Elf64_Word strtab_idx = scn_hdrs[symtab_idx].sh_link;

    *symtab = scn_hdrs + symtab_idx;
    *strtab = scn_hdrs + strtab_idx;
}

static Elf64_Sym const *modload_lookup_name(kernel_ht_t *ht, char const *name)
{
    Elf64_Word hash = elf64_hash((unsigned char*)name);
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

    return 0;
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
    return 0;
}

module_entry_fn_t modload_load(char const *path)
{
    file_t fd = file_open(path, O_RDONLY);

    if (!fd.is_open()) {
        printdbg("Failed to open module \"%s\"\n", path);
        return 0;
    }

    Elf64_Ehdr file_hdr;

    if (sizeof(file_hdr) != file_read(fd, &file_hdr, sizeof(file_hdr))) {
        printdbg("Failed to read module file header\n");
        return 0;
    }

    ssize_t scn_hdr_size = sizeof(Elf64_Shdr) * file_hdr.e_shnum;
    unique_ptr<Elf64_Shdr> scn_hdrs = new Elf64_Shdr[file_hdr.e_shnum];

    if (scn_hdr_size != file_pread(
                fd, scn_hdrs, scn_hdr_size,
                file_hdr.e_shoff)) {
        printdbg("Error reading program headers\n");
        return 0;
    }

    size_t mod_str_scn = 0;
    unique_ptr_free<Elf64_Sym> mod_sym;
    Elf64_Sym *mod_sym_end = 0;
    unique_ptr_free<char> mod_str;

    Elf64_Xword max_addr = 0;
    Elf64_Xword min_addr = ~0U;
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
            mod_str_scn = hdr->sh_link;

            mod_sym.reset((Elf64_Sym *)malloc(hdr->sh_size));
            mod_sym_end = mod_sym + hdr->sh_size;
            if ((ssize_t)hdr->sh_size != file_pread(
                        fd, mod_sym, hdr->sh_size, hdr->sh_offset)) {
                printdbg("Error reading module symbols\n");
                return 0;
            }
            break;
        case SHT_STRTAB:
            if (i != mod_str_scn)
                break;
            mod_str.reset((char*)malloc(hdr->sh_size));
            if ((ssize_t)hdr->sh_size != file_pread(
                        fd, mod_str, hdr->sh_size, hdr->sh_offset)) {
                printdbg("Error reading module strings\n");
                return 0;
            }
            break;
        }
    }

    char *module = (char *)mmap(0, max_addr - min_addr,
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
                return 0;
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

    unique_ptr_free<void> rel_buf(malloc(max_rel));

    Elf64_Rel const * const rel = (Elf64_Rel*)rel_buf.get();
    Elf64_Rela const * const rela = (Elf64_Rela*)rel_buf.get();

    // Process relocations
    for (size_t i = 1; i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr const * const hdr = scn_hdrs + i;

        // Can't process relocations that do not refer to a section
        if (hdr->sh_info == 0)
            continue;

        // Can't process relocations for a section that is not allocated
        if (!(scn_hdrs[hdr->sh_info].sh_flags & SHF_ALLOC))
            continue;

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
            return 0;
        }

        Elf64_Shdr const * const target_scn = scn_hdrs + hdr->sh_info;
        void *end;

        Elf64_Shdr const *symtab;
        Elf64_Shdr const *strtab;

        modload_find_syms(&symtab, &strtab, scn_hdrs, i);

        unique_ptr_free<Elf64_Sym> symdata(
                    (Elf64_Sym *)malloc(symtab->sh_size));
        unique_ptr_free<char> strdata((char*)malloc(strtab->sh_size));

        if ((ssize_t)symtab->sh_size != file_pread(
                    fd, symdata, symtab->sh_size, symtab->sh_offset)) {
            printdbg("Error reading symbol table\n");
            return 0;
        }

        if ((ssize_t)strtab->sh_size != file_pread(
                    fd, strdata, strtab->sh_size, strtab->sh_offset)) {
            printdbg("Error reading string table\n");
            return 0;
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
                symtab_idx = ELF64_R_SYM(r->r_info);
                rel_type = ELF64_R_TYPE(r->r_info);
                assert(!"Unhandled relocation type!");
                return 0;
            }
            break;

        case SHT_RELA:
            end = (char*)rela + hdr->sh_size;
            for (Elf64_Rela const *r = rela; (void*)r < end; ++r) {
                symtab_idx = ELF64_R_SYM(r->r_info);
                rel_type = ELF64_R_TYPE(r->r_info);

                Elf64_Sym *sym = symdata + symtab_idx;
                char const *name = strdata + sym->st_name;

                Elf64_Sym const *match = 0;

                int internal = 0;

                if (name[0]) {
                    if (sym->st_shndx == SHN_UNDEF) {
                        match = modload_lookup_name(&export_ht, name);
                    } else  {
                        internal = 1;
                        match = modload_lookup_in_module(
                                    mod_sym, mod_sym_end, mod_str, name);
                    }
                }

                void *fixup_addr = (void*)(scn_base + r->r_offset);

                printdbg("Fixup at %p (%x), match=%p,"
                         " name=%s, type=%d, symvalue=%lx\n",
                         fixup_addr, *(uint32_t*)fixup_addr, (void*)match,
                         name, rel_type, sym->st_value);

                switch (rel_type) {
                case R_AMD64_PLT32: // L + A - P
                    *(uint32_t*)fixup_addr = match->st_value +
                            r->r_addend - (Elf64_Addr)fixup_addr;
                    break;

                case R_AMD64_32S:   // S + A
                    *(uint32_t*)fixup_addr = (uint64_t)module +
                            scn_hdrs[sym->st_shndx].sh_addr +
                            r->r_addend;
                    break;

                case R_AMD64_PC32:  // S + A - P
                    if (!match) {
                        // ???
                        *(uint32_t*)fixup_addr = (Elf64_Addr)module +
                                scn_hdrs[sym->st_shndx].sh_addr +
                                r->r_addend;
                    } else {
                        *(uint32_t*)fixup_addr = match->st_value +
                                scn_hdrs[match->st_shndx].sh_addr +
                                (internal
                                 ? 0
                                 : -scn_base) +
                                r->r_addend -
                                ((Elf64_Addr)r->r_offset);
                    }
                    break;

                case R_AMD64_64:
                    *(uint64_t*)fixup_addr =
                            (internal
                             ? uint64_t(module) +
                               scn_hdrs[sym->st_shndx].sh_addr
                             : (uint64_t(module) +
                                scn_hdrs[match->st_shndx].sh_addr)) +
                            r->r_addend + match->st_value;
                    break;

                default:
                    printdbg("Unhandled relocation, type=%d\n", rel_type);
                    break;

                }
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

    printdbg("add-symbol-file %s 0x%p\n", path, (void*)module);

    // Work around pedantic warning
    module_entry_fn_t fn;
    void *entry = module + file_hdr.e_entry;
    memcpy(&fn, &entry, sizeof(fn));

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

void dl_debug_state(void);
EXPORT void dl_debug_state(void)
{
}
