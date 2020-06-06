#pragma once

// https://uclibc.org/docs/elf-64-gen.pdf

// Type names used in documentation
typedef uint64_t Elf64_Addr;
typedef int64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

// File header
struct Elf64_Ehdr {
    unsigned char e_ident[16]; /* ELF identification */
    Elf64_Half e_type; /* Object file type */
    Elf64_Half e_machine; /* Machine type */
    Elf64_Word e_version; /* Object file version */
    Elf64_Addr e_entry; /* Entry point address */
    Elf64_Off e_phoff; /* Program header offset */
    Elf64_Off e_shoff; /* Section header offset */
    Elf64_Word e_flags; /* Processor-specific flags */
    Elf64_Half e_ehsize; /* ELF header size */
    Elf64_Half e_phentsize; /* Size of program header entry */
    Elf64_Half e_phnum; /* Number of program header entries */
    Elf64_Half e_shentsize; /* Size of section header entry */
    Elf64_Half e_shnum; /* Number of section header entries */
    Elf64_Half e_shstrndx; /* Section name string table index */
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

// 32-bit objects
#define ELFCLASS32 1

// 64-bit objects
#define ELFCLASS64 2

// Object file data structures are littleendian
#define ELFDATA2LSB 1

// Object file data structures are bigendian
#define ELFDATA2MSB 2

// System V ABI
#define ELFOSABI_SYSV 0

// HP-UX operating system
#define ELFOSABI_HPUX 1

// Standalone (embedded) application
#define ELFOSABI_STANDALONE 255

// No file type
#define ET_NONE 0

// Relocatable object file
#define ET_REL 1

// Executable file
#define ET_EXEC 2

// Shared object file
#define ET_DYN 3

// Core file
#define ET_CORE 4

// Environment-specific use
#define ET_LOOS 0xFE00
#define ET_HIOS 0xFEFF

// Processor-specific use
#define ET_LOPROC 0xFF00
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

struct Elf64_Shdr {
    Elf64_Word sh_name; /* Section name */
    Elf64_Word sh_type; /* Section type */
    Elf64_Xword sh_flags; /* Section attributes */
    Elf64_Addr sh_addr; /* Virtual address in memory */
    Elf64_Off sh_offset; /* Offset in file */
    Elf64_Xword sh_size; /* Size of section */
    Elf64_Word sh_link; /* Link to other section */
    Elf64_Word sh_info; /* Miscellaneous information */
    Elf64_Xword sh_addralign; /* Address alignment boundary */
    Elf64_Xword sh_entsize; /* Size of entries, if section has table */
};

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
#define SHF_WRITE 0x1 // Section contains writable data
#define SHF_ALLOC 0x2 // Section is allocated in memory image of program
#define SHF_EXECINSTR 0x4 // Section contains executable instructions
#define SHF_MASKOS 0x0F000000 // Environment-specific use
#define SHF_MASKPROC 0xF0000000 // Processor-specific use

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

struct Elf64_Sym {
    Elf64_Word st_name; /* Symbol name */
    unsigned char st_info; /* Type and Binding attributes */
    unsigned char st_other; /* Reserved */
    Elf64_Half st_shndx; /* Section table index */
    Elf64_Addr st_value; /* Symbol value */
    Elf64_Xword st_size; /* Size of object (e.g., common) */
};

// Symbol bindings
#define STB_LOCAL 0 Not visible outside the object file
#define STB_GLOBAL 1 Global symbol, visible to all object files
#define STB_WEAK 2 Global scope, but with lower precedence than global symbols
#define STB_LOOS 10 Environment-specific use
#define STB_HIOS 12
#define STB_LOPROC 13 Processor-specific use
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
struct Elf64_Rel {
    Elf64_Addr r_offset; /* Address of reference */
    Elf64_Xword r_info; /* Symbol index and type of relocation */
};

struct Elf64_Rela {
    Elf64_Addr r_offset; /* Address of reference */
    Elf64_Xword r_info; /* Symbol index and type of relocation */
    Elf64_Sxword r_addend; /* Constant part of expression */
};

// Program header
struct Elf64_Phdr {
    Elf64_Word p_type; /* Type of segment */
    Elf64_Word p_flags; /* Segment attributes */
    Elf64_Off p_offset; /* Offset in file */
    Elf64_Addr p_vaddr; /* Virtual address in memory */
    Elf64_Addr p_paddr; /* Reserved */
    Elf64_Xword p_filesz; /* Size of segment in file */
    Elf64_Xword p_memsz; /* Size of segment in memory */
    Elf64_Xword p_align; /* Alignment of segment */
};

#define PT_NULL 0 // Unused entry
#define PT_LOAD 1 // Loadable segment
#define PT_DYNAMIC 2 // Dynamic linking tables
#define PT_INTERP 3 // Program interpreter path name
#define PT_NOTE 4 // Note sections
#define PT_SHLIB 5 // Reserved
#define PT_PHDR 6 // Program header table
#define PT_LOOS 0x60000000 // Environment-specific use
#define PT_HIOS 0x6FFFFFFF
#define PT_LOPROC 0x70000000 // Processor-specific use
#define PT_HIPROC 0x7FFFFFFF

// Segment attributes
#define PF_R 0x4 // Read permission
#define PF_W 0x2 // Write permission
#define PF_X 0x1 // Execute permission
#define PF_MASKOS 0x00FF0000 // Reserved for environment-specific use
#define PF_MASKPROC 0xFF000000 // Reserved for processor-specific use

// Dynamic table
struct Elf64_Dyn {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr d_ptr;
    } d_un;
};

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

// ignored The presence of this dynamic table entry modifies the symbol
// resolution algorithm for references within the library. Symbols defined
// within the library are used to resolve references before the dynamic
// linker searches the usual search path.
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

// ignored The presence of this dynamic table entry signals
// that the relocation table contains relocations for a non-writable segment.
#define DT_TEXTREL 22

// d_ptr Address of the relocations associated with
// the procedure linkage table.
#define DT_JMPREL 23

// ignored The presence of this dynamic table entry signals that
// the dynamic loader should process all relocations for this object
// before transferring control to the program.
#define DT_BIND_NOW 24

// d_ptr Pointer to an array of pointers to initialization functions.
#define DT_INIT_ARRAY 25

// d_ptr Pointer to an array of pointers to termination functions.
#define DT_FINI_ARRAY 26

// d_val Size, in bytes, of the array of initialization functions.
#define DT_INIT_ARRAYSZ 27

// d_val Size, in bytes, of the array of termination functions.
#define DT_FINI_ARRAYSZ 28

// Defines a range of dynamic table tags that are
// reserved for environment-specific use.
#define DT_LOOS 0x60000000
#define DT_HIOS 0x6FFFFFFF

// Defines a range of dynamic table tags that are
// reserved for processor-specific use.
#define DT_LOPROC 0x70000000
#define DT_HIPROC 0x7FFFFFFF
