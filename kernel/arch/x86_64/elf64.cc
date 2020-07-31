#include "kmodule.h"
#include "elf64.h"
#include "elf64_decl.h"
#include "debug.h"
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
#include "user_mem.h"
#include "kmodule.h"
#include "mutex.h"
#include "basic_set.h"
#include "uleb.h"
#include "vector.h"
#include "main.h"

#define ELF64_DEBUG     0
#if ELF64_DEBUG
#define ELF64_TRACE(...) printdbg(__VA_ARGS__)
#else
#define ELF64_TRACE(...) ((void)0)
#endif

using lock_type = std::shared_mutex;
using ex_lock = std::unique_lock<lock_type>;
using sh_lock = std::shared_lock<lock_type>;
static lock_type loaded_modules_lock;
using module_list_t = std::vector<std::unique_ptr<module_t>>;
static module_list_t loaded_modules;

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

// Shared module loader

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
    Elf64_Sym const * symtab = nullptr;
    char const *strtab = nullptr;

    // Assigned zero after relocations applied
    uintptr_t base_adj = 0;
};

static kernel_ht_t export_ht;

void modload_init(void)
{
    for (Elf64_Sym const *sym = ___dynsym_st + 1;
         sym < ___dynsym_en; ++sym) {
        ELF64_TRACE("addr=%p symbol=%s\n",
                 (void*)sym->st_value,
                 ___dynstr_st + sym->st_name);
    }

    export_ht.hash_nbucket = ___hash_st[0];
    export_ht.hash_nchain = ___hash_st[1];
    export_ht.hash_buckets = ___hash_st + 2;
    export_ht.hash_chains = export_ht.hash_buckets +
            export_ht.hash_nbucket;
    export_ht.symtab = ___dynsym_st;
    export_ht.strtab = ___dynstr_st;
    export_ht.base_adj = 0;
}

_unused
static ext::string module_reloc_type(size_t sym_type)
{
    switch (sym_type) {
	case R_AMD64_NONE: return "R_AMD64_NONE";

	case R_AMD64_64: return "R_AMD64_64";
	case R_AMD64_32S: return "R_AMD64_32S";
	case R_AMD64_32: return "R_AMD64_32";
	case R_AMD64_16: return "R_AMD64_16";
	case R_AMD64_8: return "R_AMD64_8";

	case R_AMD64_PC64: return "R_AMD64_PC64";
	case R_AMD64_PC32: return "R_AMD64_PC32";
	case R_AMD64_PC16: return "R_AMD64_PC16";
	case R_AMD64_PC8: return "R_AMD64_PC8";

	case R_X86_64_DTPMOD64: return "R_X86_64_DTPMOD64";
	case R_X86_64_DTPOFF64: return "R_X86_64_DTPOFF64";
	case R_X86_64_TPOFF64: return "R_X86_64_TPOFF64";
	case R_X86_64_TLSGD: return "R_X86_64_TLSGD";
	case R_X86_64_TLSLD: return "R_X86_64_TLSLD";
	case R_X86_64_DTPOFF32: return "R_X86_64_DTPOFF32";
	case R_X86_64_GOTTPOFF: return "R_X86_64_GOTTPOFF";
	case R_X86_64_TPOFF32: return "R_X86_64_TPOFF32";
	case R_X86_64_GOTPC32_TLSDESC: return "R_X86_64_GOTPC32_TLSDESC";
	case R_X86_64_TLSDESC_CALL: return "R_X86_64_TLSDESC_CALL";
	case R_X86_64_TLSDESC: return "R_X86_64_TLSDESC";

	case R_AMD64_GOT32: return "R_AMD64_GOT32";
	case R_AMD64_PLT32: return "R_AMD64_PLT32";
	case R_AMD64_COPY: return "R_AMD64_COPY";

	case R_AMD64_GLOB_DAT: return "R_AMD64_GLOB_DAT";
	case R_AMD64_JUMP_SLOT: return "R_AMD64_JUMP_SLOT";

	case R_AMD64_RELATIVE: return "R_AMD64_RELATIVE";
	case R_AMD64_GOTPCREL: return "R_AMD64_GOTPCREL";

	case R_AMD64_GOTOFF64: return "R_AMD64_GOTOFF64";
	case R_AMD64_GOTPC32: return "R_AMD64_GOTPC32";

	case R_AMD64_SIZE32: return "R_AMD64_SIZE32";
	case R_AMD64_SIZE64: return "R_AMD64_SIZE64";

	case R_AMD64_REX_GOTPCRELX: return "R_AMD64_REX_GOTPCRELX";
    }
    return "Unknown";
}

class module_t
{
public:
    bool load(char const *path);
    errno_t load_image(void const *module, size_t module_sz,
                       char const *module_name,
                       std::vector<ext::string> parameters,
                       char *ret_needed);
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
    Elf64_Xword dt_flags_1 = 0;
    Elf64_Xword dt_relacount = 0;
    size_t unknown_count = 0;

    Elf64_Sym const *syms = nullptr;
    Elf64_Addr *plt_slots = nullptr;

    module_entry_fn_t entry = nullptr;
    int run_result = 0;

    Elf64_Addr first_exec = 0;

    kernel_ht_t ht;

    char const *strs = nullptr;

    ext::string module_name;
    std::vector<ext::string> param;
    std::vector<char const *> argv;

    void *dso_handle = nullptr;

private:
    class module_reader_t {
    public:
        module_reader_t(void const *module, size_t module_sz)
            : module(module)
            , module_sz(module_sz)
        {
        }

        ssize_t operator()(void *buf, size_t sz, off_t ofs) const;
    private:
        void const *module;
        size_t module_sz;
    };

    void run_ctors();
    errno_t parse_dynamic();
    void infer_vaddr_range();
    errno_t map_sections();
    errno_t load_sections(module_reader_t const& pread);
    errno_t load_dynamic(module_reader_t const& pread);
    void find_1st_exec();
    void apply_ph_perm();
    void install_plt_handler();
    errno_t apply_relocs();
};

// Shared module loader
module_t *modload_load(char const *path, bool run)
{
    std::unique_ptr<module_t> module(new (ext::nothrow) module_t{});
    if (likely(module->load(path)))
        return module.release();
    return nullptr;
}

// Shared module loader
module_t *modload_load_image(void const *image, size_t image_sz,
                             char const *module_name,
                             std::vector<ext::string> parameters,
                             char *ret_needed,
                             errno_t *ret_errno)
{
    std::unique_ptr<module_t> module(new (ext::nothrow) module_t());

    if (unlikely(!module)) {
        if (ret_errno)
            *ret_errno = errno_t::ENOMEM;
        return nullptr;
    }

    if (unlikely(parameters.insert(parameters.begin(), module_name) ==
                 std::vector<ext::string>::iterator())) {
        if (ret_errno)
            *ret_errno = errno_t::ENOMEM;
        return nullptr;
    }

    errno_t err = module->load_image(image, image_sz, module_name,
                                     std::move(parameters), ret_needed);

    if (likely(err == errno_t::OK))
        return module.release();

    if (ret_errno)
        *ret_errno = err;

    return nullptr;
}

int modload_run(module_t *module)
{
    return module->run();
}

static char const * const relocation_types_0_23[27] = {
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
    "PC8",
    "DTPMOD64",
    "DTPOFF64",
    "TPOFF64",
    "TLSGD",
    "TLSLD",
    "DTPOFF32",
    "GOTTPOFF",
    "TPOFF32"
    "PC64",
    "GOTOFF64",
    "GOTPC32"
};

static char const * const relocation_types_32_36[5] = {
    "SIZE32",
    "SIZE64",
    "GOTPC32_TLSDESC",
    "TLSDESC_CALL",
    "TLSDESC"
};

static char const * const relocation_types_42_42[1] = {
    "REX_GOTPCRELX"
};

static constexpr char const *get_relocation_type(size_t type) {
    if (type <= 23)
        return relocation_types_0_23[type];

    if (type >= 32 && type <= 36)
        return relocation_types_32_36[type - 32];

    if (type >= 42 && type <= 42)
        return relocation_types_42_42[type - 42];

    return "<unrecognized relocation!>";
}

// This function's purpose is to act as a place to put a breakpoint
// with commands that magically load the symbols for a module when loaded

_noinline
void modload_load_symbols(char const *path,
                          uintptr_t text_addr, uintptr_t base_addr)
{
    uint32_t cpu_nr = thread_cpu_number();

    // Automated breakpoint is placed here to load symbols
    // bochs hack, autoload symbols using rdi and rsi as string ptr and adj
    __asm__ __volatile__ ("outl %%eax,%%dx\n"
                          ".global modload_symbols_autoloaded\n"
                          "modload_symbols_autoloaded:\n"
                          :
                          : "a" (cpu_nr), "d" (0x8A02), "D" (path)
                          , "S" (text_addr), "c" (base_addr)
                          : "memory");

    printdbg("gdb: add-symbol-file %s %#zx\n", path, text_addr);
}

static errno_t load_failed(errno_t err)
{
    cpu_debug_break();
    return err;
}

void module_t::run_ctors()
{
    uintptr_t const *fn_addrs = (uintptr_t const *)(dt_init_array + base_adj);
    for (size_t i = 0, e = dt_init_arraysz / sizeof(uintptr_t); i != e; ++i) {
        auto fn = (void(*)())(fn_addrs[i]);
        fn();
    }
}

errno_t module_t::parse_dynamic()
{
    for (Elf64_Dyn const& dyn_ent : dyn) {
        switch (dyn_ent.d_tag) {
        case DT_NULL:
            continue;

        case DT_STRTAB:
            dt_strtab = dyn_ent.d_un.d_ptr;
            ht.strtab = (char const *)(dt_strtab + base_adj);
            continue;

        case DT_SYMTAB:
            dt_symtab = dyn_ent.d_un.d_ptr;
            ht.symtab = (Elf64_Sym*)(dt_symtab + base_adj);
            continue;

        case DT_STRSZ:
            dt_strsz = dyn_ent.d_un.d_val;
            continue;

        case DT_SYMENT:
            if (unlikely(dyn_ent.d_un.d_val != sizeof(Elf64_Sym))) {
                printdbg("Unexpected symbol record size"
                         ", expect=%zu, got=%zu\n", sizeof(Elf64_Sym),
                         dyn_ent.d_un.d_val);
                return load_failed(errno_t::ENOEXEC);
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
                return load_failed(errno_t::ENOEXEC);
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
                return load_failed(errno_t::ENOEXEC);
            }
            continue;

        case DT_NEEDED:
            if (unlikely(!dt_needed.push_back(dyn_ent.d_un.d_val)))
                panic_oom();
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

        case DT_FLAGS_1:
            dt_flags_1 = dyn_ent.d_un.d_val;
            continue;

        case DT_RELACOUNT:
            dt_relacount = dyn_ent.d_un.d_val;
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

    return errno_t::OK;
}

void module_t::infer_vaddr_range()
{
    for (Elf64_Phdr& phdr : phdrs) {
        if (phdr.p_type != PT_LOAD)
            continue;

        min_vaddr = std::min(min_vaddr, phdr.p_vaddr);
        max_vaddr = std::max(max_vaddr, phdr.p_vaddr + phdr.p_memsz);
    }
}

errno_t module_t::map_sections()
{
    for (Elf64_Phdr& phdr : phdrs) {
        if (phdr.p_type != PT_LOAD)
            continue;

        void *addr = mmap((void*)(phdr.p_vaddr + base_adj),
                          phdr.p_memsz, PROT_READ | PROT_WRITE, MAP_NOCOMMIT);
        if (unlikely(addr == MAP_FAILED)) {
            printdbg("Failed to map section\n");
            return load_failed(errno_t::ENOMEM);
        }

        ELF64_TRACE("%s: Mapped %#zx bytes at %#zx\n", module_name.c_str(),
                    ((phdr.p_memsz) + 4095) & -4096,
                    uintptr_t(addr));
    }

    return errno_t::OK;
}

errno_t module_t::load_sections(module_reader_t const& pread)
{
    for (Elf64_Phdr& phdr : phdrs) {
        if (phdr.p_type == PT_DYNAMIC)
            dyn_seg = &phdr;

        if (phdr.p_type != PT_LOAD)
            continue;

        void *addr = (void*)(phdr.p_vaddr + base_adj);

        madvise(addr, phdr.p_memsz, MADV_WILLNEED);

        size_t io_size = phdr.p_filesz;
        ssize_t io_result = pread(addr, phdr.p_filesz, phdr.p_offset);

        if (unlikely(ssize_t(io_size) != io_result)) {
            printdbg("Failed to read segment\n");
            return load_failed(io_result < 0
                               ? errno_t(-io_result)
                               : errno_t::ENOEXEC);
        }

        size_t zeroed = phdr.p_memsz - phdr.p_filesz;
        void *zeroed_addr = (void*)(phdr.p_vaddr + base_adj + phdr.p_filesz);
        memset(zeroed_addr, 0, zeroed);
    }

    return errno_t::OK;
}

errno_t module_t::load_dynamic(module_reader_t const& pread)
{
    dyn_entries = dyn_seg->p_memsz / sizeof(Elf64_Dyn);

    if (unlikely(dyn_entries * sizeof(Elf64_Dyn) != dyn_seg->p_memsz)) {
        printdbg("Dynamic segment has unexpected size\n");
        return load_failed(errno_t::ENOEXEC);
    }

    if (unlikely(!dyn.resize(dyn_entries)))
        return load_failed(errno_t::ENOMEM);

    size_t io_size = dyn_seg->p_filesz;
    ssize_t io_result = pread(dyn.data(), io_size, dyn_seg->p_offset);

    if (unlikely(ssize_t(io_size) != io_result)) {
        printdbg("Dynamic segment read failed\n");
        return load_failed(io_result < 0
                           ? errno_t(-io_result)
                           : errno_t::ENOEXEC);
    }

    return errno_t::OK;
}

ssize_t module_t::module_reader_t::operator()(
        void *buf, size_t sz, off_t ofs) const
{
    if (unlikely(ofs < 0 || size_t(ofs) >= module_sz))
        return 0;
    // If the read is truncated
    if (off_t(sz) > off_t(module_sz) - ofs)
        sz = module_sz > uint64_t(ofs) ? module_sz - ofs : 0;
    if (unlikely(!mm_copy_user(buf, (char *)module + ofs, sz)))
        return -int(errno_t::EFAULT);
    return sz;
}

void module_t::find_1st_exec()
{
    first_exec = 0;
    for (Elf64_Phdr const& phdr : phdrs) {
        if (phdr.p_flags & PF_X) {
            if (!first_exec || first_exec > phdr.p_vaddr + base_adj)
                first_exec = phdr.p_vaddr + base_adj;
        }
    }
}

void module_t::apply_ph_perm()
{
    for (Elf64_Phdr const& phdr : phdrs) {
        if (phdr.p_type != PT_LOAD)
            continue;

        int prot = 0;
        prot |= -((phdr.p_flags & PF_R) != 0) & PROT_READ;
        prot |= -((phdr.p_flags & PF_W) != 0) & PROT_WRITE;
        prot |= -((phdr.p_flags & PF_X) != 0) & PROT_EXEC;
        if (mprotect((void*)(phdr.p_vaddr + base_adj),
                     phdr.p_memsz, prot) != 0)
            printk("mprotect failed\n");
    }
}

void module_t::install_plt_handler()
{
    plt_slots = (Elf64_Addr*)(dt_pltgot + base_adj);

    plt_slots[0] += base_adj;
    plt_slots[1] = uintptr_t(this);
    plt_slots[2] = Elf64_Addr((void*)__module_dynlink_plt_thunk);
}

static Elf64_Addr modload_lookup_name(
        kernel_ht_t *ht, char const *name, Elf64_Word hash,
        bool expect_missing, bool recurse)
{
    Elf64_Word bucket = hash % ht->hash_nbucket;

    // Look in the passed hash table table for the name
    for (Elf64_Word i = ht->hash_buckets[bucket]; i != 0;
         i = ht->hash_chains[i]) {
        Elf64_Sym const *chk_sym = ht->symtab + i;
        // Ignore SHN_UNDEF
        if (chk_sym->st_shndx == SHN_UNDEF)
            continue;
        Elf64_Word name_index = chk_sym->st_name;
        char const *chk_name = ht->strtab + name_index;
        if (likely(!strcmp(chk_name, name))) {
//            int sym_bind = ELF64_ST_BIND(chk_sym->st_info);
//            int sym_type = ELF64_ST_TYPE(chk_sym->st_info);

            return chk_sym->st_value + ht->base_adj;
        }
    }

    // Look in each module
    if (recurse) {
        for (module_t *other: loaded_modules) {
            ELF64_TRACE("Looking for %s in %s\n",
                        name, other->module_name.c_str());
            kernel_ht_t *other_ht = &other->ht;
            Elf64_Addr other_sym =
                    modload_lookup_name(other_ht, name, hash, true, false);
            if (unlikely(other_sym))
                return other_sym;
        }
    }

    if (!expect_missing) {
        printdbg("Failed to find %s %s\n", name,
                 expect_missing ? " (expected)" : "");
        cpu_debug_break();
    }

    return 0;
}

static Elf64_Addr modload_lookup_name(kernel_ht_t *ht, char const *name,
                                      bool expect_missing = false,
                                      bool recurse = true)
{
    Elf64_Word hash = elf64_hash((unsigned char const*)name);
    return modload_lookup_name(ht, name, hash, expect_missing, recurse);
}

errno_t module_t::apply_relocs()
{
    Elf64_Rela const *rela_ptrs[] = {
        (Elf64_Rela*)(dt_rela + base_adj),
        (Elf64_Rela*)(dt_jmprel + base_adj)
    };

    size_t const rela_cnts[] = {
        dt_relasz / sizeof(Elf64_Rela),
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
            char const * const type_txt = get_relocation_type(sym_type);

            auto const& sym = syms[sym_idx];

            ELF64_TRACE("Fixup at %#zx + %#zx (%#zx), type=%s name=\"%s\"\n",
                     uintptr_t(base_adj), uintptr_t(rela_ptr[i].r_offset),
                     base_adj + rela_ptr[i].r_offset,
                     module_reloc_type(sym_type).c_str(),
                     sym.st_name
                        ? (char*)dt_strtab + base_adj + sym.st_name
                        : "");

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

            char const *name = strs + sym.st_name;

            if (sym.st_name) {
                // Lookup name in kernel
                ELF64_TRACE("%s lookup %s\n", module_name.c_str(), name);
                Elf64_Addr addr = modload_lookup_name(
                            &export_ht, name, false, true);

                if (unlikely(!addr)) {
                    printk("module link error in %s:"
                           " Symbol \"%s\" not found",
                           module_name.c_str(), name);
                    return load_failed(errno_t::ENOEXEC);
                }

                S = intptr_t(addr);
            }

            uint64_t value;

            switch (sym_type) {
            case R_AMD64_JUMP_SLOT://7
                // word64 S

                if (dt_bind_now || true) {// || true) {
                    // Link it all right now
                    value = S;
                    *(int64_t*)operand = S;
                } else {
                    // Just add base to point to lazy resolution thunk
                    value = *(int64_t*)operand + base_adj;
                    *(int64_t*)operand = value;
                }

                ELF64_TRACE("R_AMD64_JUMP_SLOT=%#" PRIx64 ", name=%s\n",
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
//                ELF64_TRACE("R_AMD64_COPY\n");

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

            // === TLS ===

//            case R_X86_64_DTPMOD64:
//                ELF64_TRACE("DTPMOD64, value=%#zx, operand=%#zx"
//                            ", *operand=%#zx\n",
//                            value, uintptr_t(operand),
//                            *(uintptr_t*)operand);
//                continue;

//            case R_X86_64_DTPOFF64:
//                ELF64_TRACE("DTPMOD64, value=%#zx, operand=%#zx"
//                            ", *operand=%#zx\n",
//                            value, uintptr_t(operand),
//                            *(uintptr_t*)operand);
//                continue;

            // === No operation ===

            case R_AMD64_NONE:
                ELF64_TRACE("%s\n", type_txt);
                continue;

            default:
                ELF64_TRACE("Unknown relocation type %#x\n", sym_type);
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
                ELF64_TRACE("%s=%#" PRIx64 "\n", type_txt, value);
                if (unlikely(value != uint8_t(value)))
                    goto truncated_common;
                goto all_common;

int64_common:
                *(int64_t*)operand = int64_t(value);
                goto all_common;

all_common:
                ELF64_TRACE("Wrote type=%s, value=%#" PRIx64 " to vaddr=%#zx\n",
                         type_txt, value, uintptr_t(operand));
                break;

truncated_common:
                printk("%s: %s relocation truncated to fit!\n",
                         module_name.c_str(), type_txt);
                return load_failed(errno_t::ENOEXEC);
            }
        }
    }

    // Symbol lookups do not need adjustment anymore
    //ht.base_adj = 0;

    return errno_t::OK;
}

errno_t module_t::load_image(void const *module, size_t module_sz,
                             char const *module_name,
                             std::vector<ext::string> parameters,
                             char *ret_needed)
{
    this->module_name = module_name;

    param = std::move(parameters);

    module_reader_t pread(module, module_sz);

    size_t io_size = sizeof(file_hdr);
    auto io_result = pread(&file_hdr, io_size, 0);

    if (unlikely(ssize_t(io_size) != io_result)) {
        printk("Failed to read module file header\n");
        return load_failed(io_result < 0
                           ? errno_t(-io_result)
                           : errno_t::ENOEXEC);
    }

    decltype(file_hdr.e_phentsize) expected_phdr_sz = sizeof(Elf64_Phdr);
    if (unlikely(file_hdr.e_phentsize != expected_phdr_sz)) {
        printk("Unrecognized program header record size\n");
        return load_failed(errno_t::ENOEXEC);
    }

    if (unlikely(!phdrs.resize(file_hdr.e_phnum)))
        return load_failed(errno_t::ENOMEM);

    io_size = sizeof(Elf64_Phdr) * file_hdr.e_phnum;
    io_result = pread(phdrs.data(), io_size, file_hdr.e_phoff);

    if (unlikely(ssize_t(io_size) != io_result)) {
        printk("Failed to read %u program headers\n", file_hdr.e_phnum);
        return load_failed(io_result < 0
                           ? errno_t(-io_result)
                           : errno_t::ENOEXEC);
    }

    // Calculate address space needed

    // Pass 1, compute address range covered by loadable segments
    infer_vaddr_range();

    image = mm_alloc_space(max_vaddr - min_vaddr);
    if (unlikely(!image))
        return load_failed(errno_t::ENOMEM);

    // Compute relocation distance
    base_adj = Elf64_Sxword(Elf64_Addr(image) - min_vaddr);

    // Remember adjustment for early fixups (before relocations are applied)
    ht.base_adj = base_adj;

    dyn_seg = nullptr;

    // Pass 2, map sections
    errno_t err = map_sections();
    if (unlikely(err != errno_t::OK))
        return err;

    // Pass 3, load sections
    err = load_sections(pread);

    if (unlikely(err != errno_t::OK))
        return err;

    err = load_dynamic(pread);

    if (unlikely(err != errno_t::OK))
        return err;

    //
    // Collect information from dynamic section
    err = parse_dynamic();

    if (unlikely(err != errno_t::OK))
        return err;

    syms = (Elf64_Sym const *)(dt_symtab + base_adj);
    strs = (char const *)(dt_strtab + base_adj);

    // Made it this far, we can put the module on the list
    ex_lock lock(loaded_modules_lock);
    if (unlikely(!loaded_modules.push_back(this)))
        return load_failed(errno_t::ENOMEM);
    lock.unlock();

    //
    // Load dependencies

    for (Elf64_Xword name_ofs: dt_needed) {
        char const *name = strs + name_ofs;
        bool already_loaded = false;
        for (module_t const* other: loaded_modules) {
            if (other->module_name == name) {
                already_loaded = true;
                break;
            }
        }
        if (!already_loaded) {
            module_list_t::reverse_iterator it = std::find(
                        loaded_modules.rbegin(), loaded_modules.rend(), this);
            assert(it != loaded_modules.rend());
            if (likely(it != loaded_modules.rend())) {
                // Prevent unique_ptr delete
                it->release();
                loaded_modules.erase(it.base() - 1);
            }
            size_t name_len = strlen(name);
            name_len = std::min(name_len + 1, size_t(NAME_MAX));
            if (unlikely(!mm_copy_user(ret_needed, name, name_len)))
                return errno_t::EFAULT;
            return errno_t::ENOENT;
        }
    }

    err = apply_relocs();

    // Lookup __dso_handle
//    Elf64_Addr sym;
//    sym = modload_lookup_name(&ht, "__dso_handle_export", true, false);

//    if (!sym) {
//        // WTF?

//        for (sym = syms; (void*)sym < (void*)strs; ++sym) {
//            size_t name_ofs = sym->st_name;
//            char const *name = strs + name_ofs;
//            if (!strcmp(name, "__dso_handle_export"))
//                break;
//        }
//        if (!sym->st_info)
//            sym = nullptr;

//        // Dereferemce the pointer to __dso_handle to get __dso_handle address
//        if (sym)
//            dso_handle = *(void**)(sym->st_value + base_adj);
//    }

    // Install PLT handler
    install_plt_handler();

    // Apply permissions
    apply_ph_perm();

    // Find first executable program header

    find_1st_exec();

    printk("Module %s loaded at %#" PRIx64 "\n", module_name, base_adj);
    //printk("gdb: add-symbol-file %s %#" PRIx64 "\n", module_name, first_exec);

    char const *filename = strrchr(module_name, '/');
    filename = filename ? filename + 1 : module_name;

    modload_load_symbols(filename, first_exec, base_adj);

    // Run the init array
    run_ctors();

    entry = module_entry_fn_t(file_hdr.e_entry + base_adj);

    if (unlikely(!argv.reserve(param.size() + 1)))
        return load_failed(errno_t::ENOMEM);

    for (ext::string const& parameter : param) {
        if (unlikely(!argv.push_back(parameter.c_str())))
            return load_failed(errno_t::ENOMEM);
    }

    run();

    return errno_t::OK;
}

bool module_t::load(char const *path)
{
    return false;
}

int module_t::run()
{
    run_result = entry(argv.size() - 1, argv.data());
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

    Elf64_Addr sym = modload_lookup_name(&export_ht, name);

    assert(sym);

    auto got = (uintptr_t *)(module->dt_pltgot + module->base_adj);

    cpu_patch_code(&got[data->plt_index], &sym,
            sizeof(got[data->plt_index]));

    data->result = sym;
}

class eh_frame_hdr_hdr_t {
public:
    uint8_t ver;
    uint8_t eh_frame_ptr_enc;
    uint8_t fde_count_enc;
    uint8_t table_enc;
    // followed by encoded eh_frame_ptr
    // followed by encoded fde_count
    // followed by binary search table

    struct ent_t {
        uint64_t initial_loc;
        uint64_t address;
    };
    std::vector<ent_t> search_table;

    eh_frame_hdr_hdr_t(uint8_t const *&input)
    {
        ver = *input++;
        eh_frame_ptr_enc = *input++;
        fde_count_enc = *input++;
        table_enc = *input++;

        uint64_t eh_frame_ptr = read_enc_val(input, eh_frame_ptr_enc);
        uint64_t eh_frame_cnt = read_enc_val(input, fde_count_enc);

        search_table.reserve(eh_frame_cnt);

        for (size_t i = 0; i < eh_frame_cnt; ++i) {
            ent_t ent;
            ent.initial_loc = read_enc_val(input, table_enc);
            ent.address = read_enc_val(input, table_enc);
            if (unlikely(!search_table.push_back(ent)))
                panic_oom();
        }
    }
};

ext::spinlock __module_register_frame_lock;

EXPORT void __module_register_frame(void const * const *__module_dso_handle,
                                    void *__frame, size_t __size)
{
    std::unique_lock<ext::spinlock> lock{__module_register_frame_lock};
    __register_frame(__frame, __size);
}

EXPORT void __module_unregister_frame(void const * const *__module_dso_handle,
                                      void *__frame)
{
    std::unique_lock<ext::spinlock> lock{__module_register_frame_lock};
    __deregister_frame(__frame);
}

struct early_atexit_t {
    void *dso_handle;
    void (*handler)(void*);
    void *arg;
};

// Lookup by dso_handle
using fn_list_item_t = std::pair<void (*)(void*),void*>;
using fn_list_t = std::vector<fn_list_item_t>;
using atexit_map_t = std::map<void *, fn_list_t>;
atexit_map_t atexit_lookup;
bool atexit_ready;

extern char kernel_stack[];
early_atexit_t *early_atexit = (early_atexit_t*)kernel_stack;
size_t early_atexit_count;

_constructor(ctor_mmu_init) static void atexit_init()
{
    for (size_t i = 0; i < early_atexit_count; ++i) {
        fn_list_t& list = atexit_lookup[early_atexit[i].dso_handle];
        if (unlikely(!list.push_back({early_atexit[i].handler,
                                     early_atexit[i].arg})))
            panic_oom();
    }
    early_atexit_count = 0;

    atexit_ready = true;
}

EXPORT int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle)
{
//    std::find_if(loaded_modules.begin(), loaded_modules.end(),
//                 [dso_handle](std::unique_ptr<module_t> const& mp) {
//        return mp->dso_handle == dso_handle;
//    });

    if (atexit_ready) {
        fn_list_t& list = atexit_lookup[dso_handle];
        if (unlikely(!list.push_back({func, arg})))
            panic_oom();
    } else {
        early_atexit[early_atexit_count++] = {dso_handle, func, arg};
    }

    return 0;
}

EXPORT module_t *modload_closest(ptrdiff_t address)
{
    if (address >= ptrdiff_t(__image_start) &&
            address < ptrdiff_t(___init_brk)) {
        // Not a module, it's the kernel
        return nullptr;
    }

    ptrdiff_t closest = PTRDIFF_MAX;
    module_t *closest_module = nullptr;

    sh_lock lock(loaded_modules_lock);

    for (std::unique_ptr<module_t>& module: loaded_modules) {
        if (address >= module->base_adj) {
            ptrdiff_t distance = address - module->base_adj;
            if (closest > distance) {
                closest = distance;
                closest_module = module.get();
            }
        }
    }

    return closest_module;
}

EXPORT ext::string const& modload_get_name(module_t *module)
{
    return module->module_name;
}

EXPORT uintptr_t modload_get_base(module_t *module)
{
    return module->base_adj;
}

EXPORT module_t *modload_get_index(size_t i)
{
    return i < loaded_modules.size()
            ? loaded_modules[i].get()
            : nullptr;
}

EXPORT size_t modload_get_count()
{
    return loaded_modules.size();
}

EXPORT size_t modload_get_size(module_t *module)
{
    return module->max_vaddr - module->min_vaddr;
}

//EXPORT void *__tls_get_addr(void *a, void *b)
//{
//    return nullptr;
//}
