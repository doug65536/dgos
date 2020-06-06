#pragma once
#include "types.h"

// https://uclibc.org/docs/elf-64-gen.pdf

// e_machine
#define EM_NO_SPEC 0x00
#define EM_SPARC   0x02
#define EM_X86     0x03
#define EM_MIPS    0x08
#define EM_POWERPC 0x14
#define EM_S390    0x16
#define EM_ARM     0x28
#define EM_SUPERH  0x2A
#define EM_IA_64   0x32
#define EM_AMD64   0x3E
#define EM_AARCH64 0xB7
#define EM_RISC_V  0xF3

// Type names used in documentation
struct Elf64_Policy {
    using Addr = uint64_t;
    using Off = uint64_t;
    using Half = uint16_t;
    using Word = uint32_t;
    using Sword = int32_t;
    using Xword = uint64_t;
    using Sxword = int64_t;
    static constexpr Half e_machine = EM_AMD64;

    // Program header
    struct Phdr {
        Word p_type; /* Type of segment */
        Word p_flags; /* Segment attributes */
        Off p_offset; /* Offset in file */
        Addr p_vaddr; /* Virtual address in memory */
        Addr p_paddr; /* Reserved */
        Xword p_filesz; /* Size of segment in file */
        Xword p_memsz; /* Size of segment in memory */
        Xword p_align; /* Alignment of segment */
    };

    // File header
    struct Ehdr {
        unsigned char e_ident[16]; /* ELF identification */
        Half e_type; /* Object file type */
        Half e_machine; /* Machine type */
        Word e_version; /* Object file version */
        Addr e_entry; /* Entry point address */
        Off e_phoff; /* Program header offset */
        Off e_shoff; /* Section header offset */
        Word e_flags; /* Processor-specific flags */
        Half e_ehsize; /* ELF header size */
        Half e_phentsize; /* Size of program header entry */
        Half e_phnum; /* Number of program header entries */
        Half e_shentsize; /* Size of section header entry */
        Half e_shnum; /* Number of section header entries */
        Half e_shstrndx; /* Section name string table index */
    };

};

struct Elf32_Policy {
    using Addr = uint32_t;
    using Off = uint32_t;
    using Half = uint16_t;
    using Word = uint32_t;
    using Sword = int32_t;
    using Xword = uint32_t;
    using Sxword = int32_t;
    static constexpr Half e_machine = EM_X86;

    // File header
    struct Ehdr {
        unsigned char e_ident[16]; /* ELF identification */
        Half e_type; /* Object file type */
        Half e_machine; /* Machine type */
        Word e_version; /* Object file version */
        Addr e_entry; /* Entry point address */
        Off e_phoff; /* Program header offset */
        Off e_shoff; /* Section header offset */
        Word e_flags; /* Processor-specific flags */
        Half e_ehsize; /* ELF header size */
        Half e_phentsize; /* Size of program header entry */
        Half e_phnum; /* Number of program header entries */
        Half e_shentsize; /* Size of section header entry */
        Half e_shnum; /* Number of section header entries */
        Half e_shstrndx; /* Section name string table index */
    } _packed;

    // Program header
    struct Phdr {
        Word p_type; /* Type of segment */
        Off p_offset; /* Offset in file */
        Addr p_vaddr; /* Virtual address in memory */
        Addr p_paddr; /* Reserved */
        Xword p_filesz; /* Size of segment in file */
        Xword p_memsz; /* Size of segment in memory */
        Word p_flags; /* Segment attributes */
        Xword p_align; /* Alignment of segment */
    };
};

#define EI_MAG0 0 // File identification
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4 // File class
#define EI_DATA 5 // Data encoding
#define EI_VERSION 6 // File version
#define EI_OSABI 7 // OS/ABI identification
#define EI_ABIVERSION 8 // ABI version
#define EI_PAD 9 // Start of padding bytes
#define EI_NIDENT 16 // Size of e_ident[]

static constexpr char const elf_magic[] = { '\x7f', 'E', 'L', 'F' };

#define ELFCLASS32 1 // 32-bit objects
#define ELFCLASS64 2 // 64-bit objects

#define ELFDATA2LSB 1 // Object file data structures are littleendian
#define ELFDATA2MSB 2 // Object file data structures are bigendian

#define ELFOSABI_SYSV 0 // System V ABI
#define ELFOSABI_HPUX 1 // HP-UX operating system
#define ELFOSABI_STANDALONE 255 // Standalone (embedded) application

#define ET_NONE 0 // No file type
#define ET_REL 1 // Relocatable object file
#define ET_EXEC 2 // Executable file
#define ET_DYN 3 // Shared object file
#define ET_CORE 4 // Core file
#define ET_LOOS 0xFE00 // Environment-specific use
#define ET_HIOS 0xFEFF
#define ET_LOPROC 0xFF00 // Processor-specific use
#define ET_HIPROC 0xFFFF

// Used to mark an undefined or meaningless section reference
#define SHN_UNDEF 0

// Processor-specific use
#define SHN_LOPROC 0xFF00
#define SHN_HIPROC 0xFF1F

// Environment-specific use
#define SHN_LOOS 0xFF20
#define SHN_HIOS 0xFF3F

// Indicates that the corresponding reference is an absolute value
#define SHN_ABS 0xFFF1

// Indicates a symbol that has been declared as a common block
#define SHN_COMMON 0xFFF2

template<typename P>
struct Elf_Shdr {
    typename P::Word sh_name; /* Section name */
    typename P::Word sh_type; /* Section type */
    typename P::Xword sh_flags; /* Section attributes */
    typename P::Addr sh_addr; /* Virtual address in memory */
    typename P::Off sh_offset; /* Offset in file */
    typename P::Xword sh_size; /* Size of section */
    typename P::Word sh_link; /* Link to other section */
    typename P::Word sh_info; /* Miscellaneous information */
    typename P::Xword sh_addralign; /* Address alignment boundary */
    typename P::Xword sh_entsize; /* Size of entries, if section has table */
} _packed;

//
// sh_type

// Marks an unused section header
#define SHT_NULL 0

// Contains information defined by the program
#define SHT_PROGBITS 1

// Contains a linker symbol table
#define SHT_SYMTAB 2

// Contains a string table
#define SHT_STRTAB 3

// Contains “Rela” type relocation entries
#define SHT_RELA 4

// Contains a symbol hash table
#define SHT_HASH 5

// Contains dynamic linking tables
#define SHT_DYNAMIC 6

// Contains note information
#define SHT_NOTE 7

// Contains uninitialized space; does not occupy any space in the file
#define SHT_NOBITS 8

// Contains “Rel” type relocation entries
#define SHT_REL 9

// Reserved
#define SHT_SHLIB 10

// Contains a dynamic loader symbol table
#define SHT_DYNSYM 11

// Environment-specific use
#define SHT_LOOS 0x60000000
#define SHT_HIOS 0x6FFFFFFF

// Processor-specific use
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7FFFFFFF

// sh_flags

// Section contains writable data
#define SHF_WRITE 0x1

// Section is allocated in memory image of program
#define SHF_ALLOC 0x2

// Section contains executable instructions
#define SHF_EXECINSTR 0x4

// Environment-specific use
#define SHF_MASKOS 0x0F000000

// Processor-specific use
#define SHF_MASKPROC 0xF0000000

// ADDED: Apparent flags from looking at build executables
#define SHF_TLS 0x400

// sh_link
// SHT_DYNAMIC // String table used by entries in this section
// SHT_HASH // Symbol table to which the hash table applies
// SHT_REL // Symbol table referenced by relocations
// SHT_RELA // Symbol table referenced by relocations
// SHT_SYMTAB // String table used by entries in this section
// SHT_DYNSYM // String table used by entries in this section
// SHN_UNDEF   // other

// sh_info
//SHT_REL // Section index of section to which the relocations apply
//SHT_RELA // Section index of section to which the relocations apply
//SHT_SYMTAB // Index of first non-local symbol (i.e., number of local symbols
//SHT_DYNSYM // Index of first non-local symbol (i.e., number of local symbols

template<typename P>
struct Elf_Sym {
    typename P::Word st_name; /* Symbol name */
    unsigned char st_info; /* Type and Binding attributes */
    unsigned char st_other; /* Reserved */
    typename P::Half st_shndx; /* Section table index */
    typename P::Addr st_value; /* Symbol value */
    typename P::Xword st_size; /* Size of object (e.g., common) */
} _packed;

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xF)
#define ELF64_ST_INFO(b, t) (((b) << 4) | ((t) & 0xf))

// Symbol bindings
#define STB_LOCAL 0 //Not visible outside the object file
#define STB_GLOBAL 1 //Global symbol, visible to all object files
#define STB_WEAK 2 //Global scope, but with lower precedence than global symbols
#define STB_LOOS 10 //Environment-specific use
#define STB_HIOS 12
#define STB_LOPROC 13 //Processor-specific use
#define STB_HIPROC 15

// Symbol types
#define STT_NOTYPE 0 // No type specified (e.g., an absolute symbol)
#define STT_OBJECT 1 // Data object
#define STT_FUNC 2 // Function entry point
#define STT_SECTION 3 // Symbol is associated with a section
#define STT_FILE 4 // Source file associated with the object file
#define STT_LOOS 10 // Environment-specific use
#define STT_HIOS 12
#define STT_LOPROC 13 // Processor-specific use
#define STT_HIPROC 15

// Relocation
template<typename P>
struct Elf_Rel {
    typename P::Addr r_offset; /* Address of reference */
    typename P::Xword r_info; /* Symbol index and type of relocation */
} _packed;

template<typename P>
struct Elf_Rela {
    typename P::Addr r_offset; /* Address of reference */
    typename P::Xword r_info; /* Symbol index and type of relocation */
    typename P::Sxword r_addend; /* Constant part of expression */
} _packed;

#define ELF64_R_SYM(i)      ((i) >> 32)
#define ELF64_R_TYPE(i)     ((i) & 0xFFFFFFFFL)
#define ELF64_R_INFO(s, t)  (((s) << 32) + ((t) & 0xFFFFFFFFL))

#define PT_NULL 0 // Unused entry
#define PT_LOAD 1 // Loadable segment
#define PT_DYNAMIC 2 // Dynamic linking tables
#define PT_INTERP 3 // Program interpreter path name
#define PT_NOTE 4 // Note sections
#define PT_SHLIB 5 // Reserved
#define PT_PHDR 6 // Program header table
#define PT_TLS 7 // TLS template location
#define PT_LOOS 0x60000000 // Environment-specific use
#define PT_HIOS 0x6FFFFFFF
#define PT_LOPROC 0x70000000 // Processor-specific use
#define PT_HIPROC 0x7FFFFFFF

// Segment attributes

// Read permission
#define PF_R 0x4

// Write permission
#define PF_W 0x2

// Execute permission
#define PF_X 0x1

// These flag bits are reserved for environment-specific use
#define PF_MASKOS 0x00FF0000

// These flag bits are reserved for processor-specific use
#define PF_MASKPROC 0xFF000000

// Dynamic table
template<typename P>
struct Elf_Dyn {
    typename P::Sxword d_tag;
    union {
        typename P::Xword d_val;
        typename P::Addr d_ptr;
    } d_un;
} _packed;


// ignored Marks the end of the dynamic array
#define DT_NULL 0

// d_val The string table offset of the name of a needed library
#define DT_NEEDED 1

// d_val Total size, in bytes, of the relocation entries
// associated with the procedure linkage table.
#define DT_PLTRELSZ 2

// d_ptr Contains an address associated with the linkage table.
// The specific meaning of this field is processor-dependent.
#define DT_PLTGOT 3

// d_ptr Address of the symbol hash table, described below.
#define DT_HASH 4

// d_ptr Address of the dynamic string table.
#define DT_STRTAB 5

// d_ptr Address of the dynamic symbol table.
#define DT_SYMTAB 6

// d_ptr Address of a relocation table with Elf64_Rela entries.
#define DT_RELA 7

// d_val Total size, in bytes, of the DT_RELA relocation table.
#define DT_RELASZ 8

// d_val Size, in bytes, of each DT_RELA relocation entry.
#define DT_RELAENT 9

// d_val Total size, in bytes, of the string table.
#define DT_STRSZ 10

// d_val Size, in bytes, of each symbol table entry.
#define DT_SYMENT 11

// d_ptr Address of the initialization function.
#define DT_INIT 12

// d_ptr Address of the termination function.
#define DT_FINI 13

// d_val The string table offset of the name of this shared object.
#define DT_SONAME 14

// d_val The string table offset of a shared library search path string.
#define DT_RPATH 15

// ignored The presence of this dynamic table entry modifies the
// symbol resolution algorithm for references within the library.
// Symbols defined within the library are used to resolve references
// before the dynamic linker searches the usual search path.
#define DT_SYMBOLIC 16

// d_ptr Address of a relocation table with Elf64_Rel entries.
#define DT_REL 17

// d_val Total size, in bytes, of the DT_REL relocation table.
#define DT_RELSZ 18

// d_val Size, in bytes, of each DT_REL relocation entry.
#define DT_RELENT 19

// d_val Type of relocation entry used for the procedure linkage table.
// The d_val member contains either DT_REL or DT_RELA.
#define DT_PLTREL 20

// d_ptr Reserved for debugger use.
#define DT_DEBUG 21

// ignored The presence of this dynamic table entry signals that the
// relocation table contains relocations for a non-writable segment.
#define DT_TEXTREL 22

// d_ptr Address of the relocations associated with the procedure linkage table.
#define DT_JMPREL 23

// ignored The presence of this dynamic table entry signals that the
// dynamic loader should process all relocations for this object before
// transferring control to the program.
#define DT_BIND_NOW 24

// d_ptr Pointer to an array of pointers to initialization functions.
#define DT_INIT_ARRAY 25

// d_ptr Pointer to an array of pointers to termination functions.
#define DT_FINI_ARRAY 26

// d_val Size, in bytes, of the array of initialization functions.
#define DT_INIT_ARRAYSZ 27

// d_val Size, in bytes, of the array of termination functions.
#define DT_FINI_ARRAYSZ 28

// d_val
#define DT_FLAGS_1 0x6ffffffb

// d_val
#define DT_RELACOUNT 0x6ffffff9

// Defines a range of dynamic table tags that are reserved
// for environment-specific use.
#define DT_LOOS 0x60000000
#define DT_HIOS 0x6FFFFFFF

// Defines a range of dynamic table tags that are reserved
// for processor-specific use.
#define DT_LOPROC 0x70000000
#define DT_HIPROC 0x7FFFFFFF

//
// Relocations

// A: The addend used to compute the value of the relocatable field.
// B: The base address at which a shared object is loaded into memory during
//    execution. Generally, a shared object file is built with a base virtual
//    address of 0. However, the execution address of the shared object is
//    different. See Program Header.
// G: The offset into the global offset table at which the address of the
//    relocation entry's symbol resides during execution.
//    See Global Offset Table (Processor-Specific).
// GOT: The address of the global offset table.
//    See Global Offset Table (Processor-Specific).
// L: The section offset or address of the procedure linkage table entry for
//    a symbol. See Procedure Linkage Table (Processor-Specific).
// P: The section offset or address of the storage unit being relocated,
//    computed using r_offset.
// S: The value of the symbol whose index resides in the relocation entry.
// Z: The size of the symbol whose index resides in the relocation entry.

#define R_AMD64_NONE        0   // None    None

#define R_AMD64_64          1   // word64  S + A
#define R_AMD64_32S         11  // word32  S + A
#define R_AMD64_32          10  // word32  S + A
#define R_AMD64_16          12  // word16  S + A
#define R_AMD64_8           14  // word8   S + A

#define R_AMD64_PC64        24  // word64  S + A - P
#define R_AMD64_PC32        2   // word32  S + A - P
#define R_AMD64_PC16        13  // word16  S + A - P
#define R_AMD64_PC8         15  // word8   S + A - P

#define R_X86_64_DTPMOD64   16
#define R_X86_64_DTPOFF64   17
#define R_X86_64_TPOFF64    18
#define R_X86_64_TLSGD      19
#define R_X86_64_TLSLD      20
#define R_X86_64_DTPOFF32   21
#define R_X86_64_GOTTPOFF   22
#define R_X86_64_TPOFF32    23
#define R_X86_64_GOTPC32_TLSDESC    34
#define R_X86_64_TLSDESC_CALL    35
#define R_X86_64_TLSDESC    36

#define R_AMD64_GOT32       3   // word32  G + A
#define R_AMD64_PLT32       4   // word32  L + A - P
#define R_AMD64_COPY        5   // None    Refer to the explanation

#define R_AMD64_GLOB_DAT    6   // word64  S
#define R_AMD64_JUMP_SLOT   7   // word64  S

#define R_AMD64_RELATIVE    8   // word64  B + A
#define R_AMD64_GOTPCREL    9   // word32  G + GOT + A - P

#define R_AMD64_GOTOFF64    25  // word64  S + A - GOT
#define R_AMD64_GOTPC32     26  // word32  GOT + A + P

#define R_AMD64_SIZE32      32  // word32  Z + A
#define R_AMD64_SIZE64      33  // word64  Z + A

#define R_AMD64_REX_GOTPCRELX   42  // ?

static _always_inline unsigned long elf64_hash(unsigned char const *name)
{
    unsigned long h = 0, g;
    while (*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        h ^= g >> 24;
        h &= 0x0fffffff;
    }
    return h;
}

// Primitive declarations

using Elf64_Addr   = Elf64_Policy::Addr;
using Elf64_Off    = Elf64_Policy::Off;
using Elf64_Half   = Elf64_Policy::Half;
using Elf64_Word   = Elf64_Policy::Word;
using Elf64_Sword  = Elf64_Policy::Sword;
using Elf64_Xword  = Elf64_Policy::Xword;
using Elf64_Sxword = Elf64_Policy::Sxword;

using Elf32_Addr   = Elf32_Policy::Addr;
using Elf32_Off    = Elf32_Policy::Off;
using Elf32_Half   = Elf32_Policy::Half;
using Elf32_Word   = Elf32_Policy::Word;
using Elf32_Sword  = Elf32_Policy::Sword;
using Elf32_Xword  = Elf32_Policy::Xword;
using Elf32_Sxword = Elf32_Policy::Sxword;

// Structure declarations

using Elf64_Ehdr   = Elf64_Policy::Ehdr;
using Elf64_Phdr   = Elf64_Policy::Phdr;
using Elf64_Shdr   = Elf_Shdr<Elf64_Policy>;
using Elf64_Sym    = Elf_Sym<Elf64_Policy>;
using Elf64_Dyn    = Elf_Dyn<Elf64_Policy>;
using Elf64_Rel    = Elf_Rel<Elf64_Policy>;
using Elf64_Rela   = Elf_Rela<Elf64_Policy>;

using Elf32_Ehdr   = Elf32_Policy::Ehdr;
using Elf32_Phdr   = Elf32_Policy::Phdr;
using Elf32_Shdr   = Elf_Shdr<Elf32_Policy>;
using Elf32_Sym    = Elf_Sym<Elf32_Policy>;
using Elf32_Dyn    = Elf_Dyn<Elf32_Policy>;
using Elf32_Rel    = Elf_Rel<Elf32_Policy>;
using Elf32_Rela   = Elf_Rela<Elf32_Policy>;
