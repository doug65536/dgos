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
#include "cpu.h"
#include "callout.h"
#include "cxxstring.h"
#include "cpu/control_regs.h"

#define ELF64_DEBUG     1
#if ELF64_DEBUG
#define ELF64_TRACE(...) printdbg(__VA_ARGS__)
#else
#define ELF64_TRACE(...) ((void)0)
#endif

static std::vector<std::unique_ptr<module_t>> loaded_modules;

// Keep this in sync with __module_dynlink_thunk
struct plt_stub_data_t {
    uintptr_t rax;
    uintptr_t rdi;
    uintptr_t rsi;
    uintptr_t rdx;
    uintptr_t rcx;
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t r10;
    uintptr_t r11;
    uintptr_t rflags;
    uintptr_t result;
    module_t *plt_ctx;
    uintptr_t plt_index;
};

// Relocatable module loader

extern Elf64_Sym const ___dynsym_st[];
extern Elf64_Sym const ___dynsym_en[];
extern char const ___dynstr_st[];
extern char const ___dynstr_en[];
extern Elf64_Word const ___hash_st[];
extern Elf64_Word const ___hash_en[];

struct kernel_ht_t {
    Elf64_Word hash_nbucket = 0;
    Elf64_Word hash_nchain = 0;
    Elf64_Word const * hash_buckets = nullptr;
    Elf64_Word const * hash_chains = nullptr;
};

static kernel_ht_t export_ht;

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

static std::string module_reloc_type(size_t sym_type)
{
    switch (sym_type) {
    case R_AMD64_NONE: return "R_AMD64_NONE";
    case R_AMD64_64: return "R_AMD64_64";
    case R_AMD64_PC32: return "R_AMD64_PC32";
    case R_AMD64_GOT32: return "R_AMD64_GOT32";
    case R_AMD64_PLT32: return "R_AMD64_PLT32";
    case R_AMD64_COPY: return "R_AMD64_COPY";
    case R_AMD64_GLOB_DAT: return "R_AMD64_GLOB_DAT";
    case R_AMD64_JUMP_SLOT: return "R_AMD64_JUMP_SLOT";
    case R_AMD64_RELATIVE: return "R_AMD64_RELATIVE";
    case R_AMD64_GOTPCREL: return "R_AMD64_GOTPCREL";
    case R_AMD64_32: return "R_AMD64_32";
    case R_AMD64_32S: return "R_AMD64_32S";
    case R_AMD64_16: return "R_AMD64_16";
    case R_AMD64_PC16: return "R_AMD64_PC16";
    case R_AMD64_8: return "R_AMD64_8";
    case R_AMD64_PC8: return "R_AMD64_PC8";
    case R_AMD64_PC64: return "R_AMD64_PC64";
    case R_AMD64_GOTOFF64: return "R_AMD64_GOTOFF64";
    case R_AMD64_GOTPC32: return "R_AMD64_GOTPC32";
    case R_AMD64_SIZE32: return "R_AMD64_SIZE32";
    case R_AMD64_SIZE64: return "R_AMD64_SIZE64";
    }
    return "Unknown";
}

class module_t {
public:
    bool load(char const *path);
    bool load_image(void const *module, size_t module_sz, const char *module_name);
    int run();

    ~module_t();

    Elf64_Ehdr file_hdr{};
    std::vector<Elf64_Phdr> phdrs;
    Elf64_Addr min_vaddr = ~Elf64_Addr(0);
    Elf64_Addr max_vaddr = 0;
    void *image = nullptr;
    Elf64_Sxword base_adj = 0;
    Elf64_Phdr *dyn_seg = nullptr;
    size_t dyn_entries = 0;
    std::vector<Elf64_Dyn> dyn;

    std::vector<Elf64_Xword> dt_needed;
    Elf64_Addr dt_strtab = 0;
    Elf64_Xword dt_strsz = 0;

    Elf64_Addr dt_symtab = 0;

    Elf64_Addr dt_pltgot = 0;
    Elf64_Addr dt_jmprel = 0;

    Elf64_Addr dt_rela = 0;
    Elf64_Xword dt_relasz = 0;

    Elf64_Xword dt_pltrelsz = 0;
    Elf64_Xword dt_hash = 0;
    Elf64_Xword dt_soname = 0;
    Elf64_Xword dt_rpath = 0;
    Elf64_Xword dt_symbolic = 0;
    Elf64_Xword dt_relaent = 0;
    Elf64_Xword dt_textrel = 0;
    Elf64_Xword dt_bind_now = 0;
    Elf64_Addr dt_init_array = 0;
    Elf64_Addr dt_fini_array = 0;
    Elf64_Xword dt_init_arraysz = 0;
    Elf64_Xword dt_fini_arraysz = 0;
    size_t unknown_count = 0;

    Elf64_Sym const *syms = nullptr;
    Elf64_Addr *plt_slots = nullptr;

    module_entry_fn_t entry = nullptr;
    int run_result = 0;

    Elf64_Addr first_exec = 0;

    kernel_ht_t ht;
};

// Shared module loader
module_t *modload_load(char const *path, bool run)
{
    std::unique_ptr<module_t> module(new module_t{});
    if (likely(module->load(path)))
        return module.release();
    return nullptr;
}

// Shared module loader
module_t *modload_load_image(const void *image, size_t image_sz,
                             char const *module_name, bool run)
{
    std::unique_ptr<module_t> module(new module_t{});
    if (likely(module->load_image(image, image_sz, module_name)))
        return module.release();
    return nullptr;
}

int modload_run(module_t *module)
{
    return module->run();
}

static char const * const relocation_types_0_15[16] = {
    "NONE",
    "64",
    "PC32",
    "GOT32",
    "PLT32",
    "COPY",
    "GLOB_DAT",
    "JUMP_SLOT",
    "RELATIVE",
    "GOTPCREL",
    "32",
    "32S",
    "16",
    "PC16",
    "8",
    "PC8"
};

static char const * const relocation_types_24_26[3] = {
    "PC64",
    "GOTOFF64",
    "GOTPC32"
};

static char const * const relocation_types_32_33[2] = {
    "SIZE32",
    "SIZE64"
};

static char const * const relocation_types_42_42[1] = {
    "REX_GOTPCRELX"
};

static constexpr char const *get_relocation_type(size_t type) {
    if (type < 16)
        return relocation_types_0_15[type];
    if (type >= 24 && type <= 26)
        return relocation_types_24_26[type - 24];
    if (type >= 32 && type <= 33)
        return relocation_types_32_33[type - 32];
    if (type >= 42 && type <= 42)
        return relocation_types_42_42[type - 42];
    return "<unrecognized relocation!>";
}

// This function's purpose is to act as a place to put a breakpoint
// with commands that magically load the symbols for a module when loaded
void modload_load_symbols(char const *path, uintptr_t addr)
{
    // Seriously compliler, please don't elide, thanks!
    // The debugger is actually going to dereference path and get addr
    // So that memory clobber is really needed, I need the caller to write
    // the actual path string into memory there (flow analysis could discard
    // it without the memory clobber)
    __asm__ __volatile__ ("" : : "r" (path), "r" (addr) : "memory");
}

static bool load_failed()
{
    cpu_debug_break();
    return false;
}

bool module_t::load_image(void const *module, size_t module_sz,
                          char const *module_name)
{
    auto pread = [&](void *buf, size_t sz, off_t ofs) -> ssize_t {
        if (unlikely(ofs < 0 || size_t(ofs) >= module_sz))
            return 0;
        // If the read is truncated
        if (off_t(sz) > off_t(module_sz) - ofs)
            sz = module_sz > ofs ? module_sz - ofs : 0;
        if (unlikely(!mm_copy_user(buf, (char *)module + ofs, sz)))
            return -int(errno_t::EFAULT);
        return sz;
    };

    if (unlikely(sizeof(file_hdr) != pread(&file_hdr, sizeof(file_hdr), 0))) {
        printdbg("Failed to read module file header\n");
        return load_failed();
    }

    if (unlikely(file_hdr.e_phentsize != sizeof(Elf64_Phdr))) {
        printdbg("Unrecognized program header record size\n");
        return load_failed();
    }

    if (unlikely(!phdrs.resize(file_hdr.e_phnum)))
        panic_oom();

    if (unlikely(ssize_t(sizeof(Elf64_Phdr) * file_hdr.e_phnum) != pread(
                     phdrs.data(), sizeof(Elf64_Phdr) * file_hdr.e_phnum,
                     file_hdr.e_phoff))) {
        printdbg("Failed to read %u program headers\n", file_hdr.e_phnum);
        return load_failed();
    }

    // Calculate address space needed

    // Pass 1, compute address range covered by loadable segments
    for (Elf64_Phdr& phdr : phdrs) {
        if (phdr.p_type != PT_LOAD)
            continue;

        min_vaddr = std::min(min_vaddr, phdr.p_vaddr);
        max_vaddr = std::max(max_vaddr, phdr.p_vaddr + phdr.p_memsz);
    }

    image = mm_alloc_space(max_vaddr - min_vaddr);

    // Compute relocation distance
    base_adj = Elf64_Sxword(Elf64_Addr(image) - min_vaddr);

    dyn_seg = nullptr;

    // Pass 2, map sections
    for (Elf64_Phdr& phdr : phdrs) {
        if (phdr.p_type != PT_LOAD)
            continue;

        void *addr = mmap((void*)(phdr.p_vaddr + base_adj),
                          phdr.p_memsz, PROT_READ | PROT_WRITE,
                          MAP_NOCOMMIT, -1, 0);
        if (unlikely(addr == MAP_FAILED)) {
            printdbg("Failed to map section\n");
            return load_failed();
        }

        ELF64_TRACE("%s: Mapped %#zx bytes at %#zx\n", module_name,
                    ((phdr.p_memsz) + 4095) & -4096,
                    uintptr_t(addr));
    }

    // Pass 3, load sections
    for (Elf64_Phdr& phdr : phdrs) {
        if (phdr.p_type == PT_DYNAMIC)
            dyn_seg = &phdr;

        if (phdr.p_type != PT_LOAD)
            continue;

        void *addr = (void*)(phdr.p_vaddr + base_adj);

        if (unlikely(ssize_t(phdr.p_filesz) != pread(
                         addr, phdr.p_filesz, phdr.p_offset))) {
            printdbg("Failed to read segment\n");
            return load_failed();
        }

        size_t zeroed = phdr.p_memsz - phdr.p_filesz;
        void *zeroed_addr = (void*)(phdr.p_vaddr + base_adj + phdr.p_filesz);
        memset(zeroed_addr, 0, zeroed);
    }

    dyn_entries = dyn_seg->p_memsz / sizeof(Elf64_Dyn);

    if (unlikely(dyn_entries * sizeof(Elf64_Dyn) != dyn_seg->p_memsz)) {
        printdbg("Dynamic segment has unexpected size\n");
        return load_failed();
    }

    if (unlikely(!dyn.resize(dyn_entries)))
        panic_oom();

    if (unlikely(ssize_t(dyn_seg->p_filesz) != pread(
                     dyn.data(), dyn_seg->p_filesz, dyn_seg->p_offset))) {
        // FIXME: LEAK!
        printdbg("Dynamic segment read failed\n");
        return load_failed();
    }

    //
    // Collect information from dynamic section

    for (Elf64_Dyn const& dyn_ent : dyn) {
        switch (dyn_ent.d_tag) {
        case DT_NULL:
            continue;

        case DT_STRTAB:
            dt_strtab = dyn_ent.d_un.d_ptr;
            continue;

        case DT_SYMTAB:
            dt_symtab = dyn_ent.d_un.d_ptr;
            continue;

        case DT_STRSZ:
            dt_strsz = dyn_ent.d_un.d_val;
            continue;

        case DT_SYMENT:
            if (unlikely(dyn_ent.d_un.d_val != sizeof(Elf64_Sym))) {
                printdbg("Unexpected symbol record size"
                         ", expect=%zu, got=%zu\n", sizeof(Elf64_Sym),
                         dyn_ent.d_un.d_val);
                return load_failed();
            }
            continue;

        case DT_PLTGOT:
            dt_pltgot = dyn_ent.d_un.d_ptr;
            continue;

        case DT_PLTRELSZ:
            dt_pltrelsz = dyn_ent.d_un.d_val;
            continue;

        case DT_PLTREL:
            if (unlikely(dyn_ent.d_un.d_val != DT_RELA)) {
                printdbg("Unexpected relocation type, expecting RELA\n");
                return load_failed();
            }
            continue;

        case DT_JMPREL:
            dt_jmprel = dyn_ent.d_un.d_val;
            continue;

        case DT_RELA:
            dt_rela = dyn_ent.d_un.d_ptr;
            continue;

        case DT_RELASZ:
            dt_relasz = dyn_ent.d_un.d_val;
            continue;

        case DT_RELAENT:
            if (unlikely(dyn_ent.d_un.d_val != sizeof(Elf64_Rela))) {
                printdbg("Relocations have unexpected size\n");
                return load_failed();
            }
            continue;

        case DT_NEEDED:
            dt_needed.push_back(dyn_ent.d_un.d_val);
            continue;

        case DT_HASH:
            dt_hash = dyn_ent.d_un.d_ptr;

            Elf64_Word const *hash;
            hash = (Elf64_Word const *)(dt_hash + base_adj);

            ht.hash_nbucket = hash[0];
            ht.hash_nchain = hash[1];
            ht.hash_buckets = hash + 2;
            ht.hash_chains = ht.hash_buckets + ht.hash_nbucket;

            continue;

        case DT_INIT:
            dt_init_array = dyn_ent.d_un.d_ptr;
            continue;

        case DT_FINI:
            dt_fini_array = dyn_ent.d_un.d_ptr;
            continue;

        case DT_SONAME:
            dt_soname = dyn_ent.d_un.d_val;
            continue;

        case DT_RPATH:
            dt_rpath = dyn_ent.d_un.d_val;
            continue;

        case DT_SYMBOLIC:
            dt_symbolic = dyn_ent.d_un.d_val;
            continue;

        case DT_REL:    // fallthru
        case DT_RELSZ:  // fallthru
        case DT_RELENT:
            printdbg("Unhandled relocation type\n");
            continue;

        case DT_DEBUG:
            continue;

        case DT_TEXTREL:
            dt_textrel = dyn_ent.d_un.d_val;
            continue;

        case DT_BIND_NOW:
            dt_bind_now = 1;
            continue;

        case DT_INIT_ARRAY:
            dt_init_array = dyn_ent.d_un.d_val;
            continue;

        case DT_FINI_ARRAY:
            dt_fini_array = dyn_ent.d_un.d_val;
            continue;

        case DT_INIT_ARRAYSZ:
            dt_init_arraysz = dyn_ent.d_un.d_val;
            continue;

        case DT_FINI_ARRAYSZ:
            dt_fini_arraysz = dyn_ent.d_un.d_val;
            continue;

        default:
            printdbg("encountered unknown .dynamic entry type=%#" PRIx64 "\n",
                     dyn_ent.d_tag);
            ++unknown_count;
            continue;
        }
    }

    if (unknown_count) {
        printdbg("encountered %zu unrecognized .dynamic entries\n",
                 unknown_count);
    }

    dt_relaent = dt_relasz / sizeof(Elf64_Rela);

    syms = (Elf64_Sym const *)(dt_symtab + base_adj);

    Elf64_Rela const *rela_ptrs[] = {
        (Elf64_Rela*)(dt_rela + base_adj),
        (Elf64_Rela*)(dt_jmprel + base_adj)
    };

    size_t const rela_cnts[] = {
        dt_relaent,
        dt_pltrelsz / sizeof(Elf64_Rela)
    };

    static_assert(countof(rela_cnts) == countof(rela_ptrs),
                  "Arrays must have corresponding values");

    for (size_t rel_idx = 0; rel_idx < countof(rela_ptrs); ++rel_idx) {
        Elf64_Rela const *rela_ptr = rela_ptrs[rel_idx];
        size_t ent_count = rela_cnts[rel_idx];
        for (size_t i = 0; i < ent_count; ++i) {
            void const *operand = (void*)(rela_ptr[i].r_offset + base_adj);
            Elf64_Word sym_idx = ELF64_R_SYM(rela_ptr[i].r_info);
            Elf64_Word sym_type = ELF64_R_TYPE(rela_ptr[i].r_info);

            auto const& sym = syms[sym_idx];

            if (sym.st_info == 0)
                continue;

            printdbg("Fixup at %#zx + %#zx (%#zx), type=%s\n",
                     uintptr_t(base_adj), uintptr_t(rela_ptr[i].r_offset),
                     base_adj + rela_ptr[i].r_offset,
                     module_reloc_type(sym_type).c_str());

            // A addend
            // B base address
            // G GOT offset
            // L PLT entry address
            // P place of relocation
            // S symbol value
            // Z size of relocation

            auto const A = intptr_t(rela_ptr[i].r_addend);
            auto const& B = base_adj;
            auto const P = intptr_t(operand);
            auto const& G = dt_pltgot + base_adj;

            auto const Z = sym.st_size;

            auto S = sym.st_value + base_adj;

            auto const strs = (char const *)(dt_strtab + base_adj);
            char const *name = strs + sym.st_name;

            //assert(!name ^ !sym.st_value);

            if (sym.st_name) {
                // Lookup name in kernel
                ELF64_TRACE("%s lookup %s\n", module_name, name);
                Elf64_Sym const *addr = modload_lookup_name(&export_ht, name);

                if (addr)
                    S = intptr_t(addr->st_value);

                if (!S) {
                    printk("module link error in %s:"
                           " Symbol \"%s\" not found", module_name, name);
                    return load_failed();
                }
            }

            uint64_t value;

            char const * const type_txt = get_relocation_type(sym_type);

            switch (sym_type) {
            case R_AMD64_JUMP_SLOT://7
                // word64 S

                if (dt_bind_now) {
                    // Link it all right now
                    value = S;
                    *(int64_t*)operand = S;
                } else {
                    // Just add base to point to lazy resolution thunk
                    value = *(int64_t*)operand + base_adj;
                    *(int64_t*)operand = value;
                }

                printdbg("R_AMD64_JUMP_SLOT=%#" PRIx64 ", name=%s\n",
                         value, name);
                break;

            // === 64 bit ===

            case R_AMD64_64:    //1
                // word64 S + A
                value = S + A;
                goto int64_common;

            case R_AMD64_GLOB_DAT://6
                // word64 S
                value = S;
                goto int64_common;

            case R_AMD64_RELATIVE://8
                // word64 B + A
                value = B + A;
                goto int64_common;

            case R_AMD64_GOTOFF64://25
                // word64 S + A - GOT
                value = S + A - dt_pltgot;
                goto int64_common;

            case R_AMD64_PC64:  //24
                // word64 S + A - P
                value = S + A - P;
                goto int64_common;

            case R_AMD64_SIZE64://33
                // word64 Z + A
                value = Z + A;
                goto int64_common;

            // === 32 bit ===

            case R_AMD64_PC32:  //2
                // word32 S + A - P
                value = S + A - P;
                goto int32_common;

            case R_AMD64_GOT32: //3
                // word32 G + A
                value = G + A;
                goto uint32_common;

//            case R_AMD64_COPY:  //5
//                // Refer to the explanation following this table. ???
//                printdbg("R_AMD64_COPY\n");

//                break;

            case R_AMD64_GOTPC32:// 26
                // word32 GOT + A + P
                value = G + A + P;
                goto int32_common;

            case R_AMD64_SIZE32:// 32
                // word32 Z + A
                cpu_debug_break();
                value = Z + A;
                goto uint32_common;

//            case R_AMD64_PLT32: //4
//                // word32 L + A - P
//                value = L + A - P;
//                goto int32_common;

            case R_AMD64_GOTPCREL://9
                // word32 G + GOT + A - P
                value = G + A - P;
                goto int32_common;

            case R_AMD64_32:    //10
                // word32 S + A
                value = S + A;
                goto uint32_common;

            case R_AMD64_32S:   //11
                // word32 S + A
                value = S + A;
                goto int32_common;

            // === 16 bit ===

            case R_AMD64_16:    //12
                // word16 S + A
                value = S + A;
                goto uint16_common;

            case R_AMD64_PC16:  //13
                // word16 S + A - P
                value = S + A - P;
                goto int16_common;

            // === 8 bit ===

            case R_AMD64_8:     //14
                // word8 S + A
                value = S + A;
                goto uint8_common;

            case R_AMD64_PC8:   //15
                // word8 S + A - P
                value = S + A - P;
                goto int8_common;

            // === No operation ===

            case R_AMD64_NONE:
                printdbg("%s\n", type_txt);
                continue;

            default:
                printdbg("Unknown relocation type %#x\n", sym_type);
                cpu_debug_break();
                break;

int32_common:
                *(int32_t*)operand = value;
                if (unlikely(int64_t(value) != int32_t(value)))
                    goto truncated_common;
                goto all_common;

uint32_common:
                *(uint32_t*)operand = uint32_t(value);
                if (unlikely(value != uint32_t(value)))
                    goto truncated_common;
                goto all_common;

int16_common:
                *(int16_t*)operand = int16_t(value);
                if (unlikely(int64_t(value) != int16_t(value)))
                    goto truncated_common;
                goto all_common;

uint16_common:
                *(uint16_t*)operand = uint16_t(value);
                if (unlikely(value != uint16_t(value)))
                    goto truncated_common;
                goto all_common;

int8_common:
                *(int8_t*)operand = int8_t(value);
                if (unlikely(int64_t(value) != int8_t(value)))
                    goto truncated_common;
                goto all_common;

uint8_common:
                *(uint8_t*)operand = uint8_t(value);
                printdbg("%s=%#" PRIx64 "\n", type_txt, value);
                if (unlikely(value != uint8_t(value)))
                    goto truncated_common;
                goto all_common;

int64_common:
                *(int64_t*)operand = int64_t(value);
                goto all_common;

all_common:
                printdbg("Wrote type=%s, value=%#" PRIx64 " to vaddr=%#zx\n",
                         type_txt, value, uintptr_t(operand));
                break;

truncated_common:
                printdbg("%s: %s relocation truncated to fit!\n",
                         module_name, type_txt);
                return load_failed();
            }
        }
    }

    // Install PLT handler
    plt_slots = (Elf64_Addr*)(dt_pltgot + base_adj);

    plt_slots[0] += base_adj;
    plt_slots[1] = uintptr_t(this);
    plt_slots[2] = Elf64_Addr((void*)__module_dynlink_plt_thunk);

    // Apply permissions
    for (Elf64_Phdr const& phdr : phdrs) {
        if (phdr.p_type != PT_LOAD)
            continue;

        int prot = 0;
        prot |= -((phdr.p_flags & PF_R) != 0) & PROT_READ;
        prot |= -((phdr.p_flags & PF_W) != 0) & PROT_WRITE;
        prot |= -((phdr.p_flags & PF_X) != 0) & PROT_EXEC;
        if (mprotect((void*)(phdr.p_vaddr + base_adj), phdr.p_memsz, prot) != 0)
            printdbg("mprotect failed\n");
    }

    // Find first executable program header

    for (Elf64_Phdr const& phdr : phdrs) {
        if (phdr.p_flags & PF_X) {
            first_exec = phdr.p_vaddr + base_adj;
            break;
        }
    }

    printdbg("Module %s loaded at %#" PRIx64 "\n", module_name, base_adj);
    printdbg("gdb: add-symbol-file %s %#" PRIx64 "\n", module_name, first_exec);

    modload_load_symbols(module_name, first_exec);

    // Run the init array
    uintptr_t const *fn_addrs = (uintptr_t const *)(dt_init_array + base_adj);
    for (size_t i = 0, e = dt_init_arraysz / sizeof(uintptr_t); i != e; ++i) {
        auto fn = (void(*)())(fn_addrs[i] + base_adj);
        fn();
    }

    entry = module_entry_fn_t(file_hdr.e_entry + base_adj);

    return true;
}

bool module_t::load(char const *path)
{
    return false;
}

int module_t::run()
{
    run_result = entry();
    return run_result;
}

module_t::~module_t()
{
    if (image && (max_vaddr-min_vaddr)) {
        munmap(image, (max_vaddr - min_vaddr));
        image = nullptr;
    }
}

void __module_dynamic_linker(plt_stub_data_t *data)
{
    module_t *module = data->plt_ctx;

    auto jmprel = (Elf64_Rela const *)(module->dt_jmprel + module->base_adj);

    auto const& rela = jmprel[data->plt_index];
    Elf64_Word sym_idx = ELF64_R_SYM(rela.r_info);
    Elf64_Word sym_type = ELF64_R_TYPE(rela.r_info);

    assert(sym_type == R_AMD64_JUMP_SLOT);

    auto symtab = (Elf64_Sym const *)(module->dt_symtab + module->base_adj);
    auto strtab = (char const *)(module->dt_strtab + module->base_adj);

    char const *name = strtab + symtab[sym_idx].st_name;

    Elf64_Sym const *sym = modload_lookup_name(&export_ht, name);

    auto got = (uintptr_t *)(module->dt_pltgot + module->base_adj);

    cpu_patch_code(&got[data->plt_index], &sym->st_value,
            sizeof(got[data->plt_index]));

    data->result = sym->st_value;
}

