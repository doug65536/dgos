#include "elf64.h"
#include "elf64_decl.h"
#include "printk.h"

extern Elf64_Sym ___dynsym_st[];
extern Elf64_Sym ___dynsym_en[];
extern char ___dynstr_st[];
extern char ___dynstr_en[];

void modload_init(void)
{
    for (Elf64_Sym *sym = ___dynsym_st; sym < ___dynsym_en; ++sym) {
        printdbg("Symbol: %s\n", ___dynstr_st + sym->st_name);
    }
}
