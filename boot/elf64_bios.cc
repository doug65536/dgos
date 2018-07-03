#include "elf64_abstract.h"
#include "paging.h"
#include "string.h"
#include "fs.h"
#include "cpu.h"
#include "progressbar.h"
#include "malloc.h"
#include "physmap.h"
#include "halt.h"
#include "likely.h"
#include "assert.h"

elf64_context_t *load_kernel_begin()
{
    elf64_context_t *ctx = new elf64_context_t{};
    return ctx;
}

void load_kernel_end(elf64_context_t *ctx)
{
    delete ctx;
}

bool load_kernel_chunk(Elf64_Phdr *blk, int file, elf64_context_t *ctx)
{
    alloc_page_factory_t allocator;

    paging_map_range(&allocator, blk->p_vaddr, blk->p_memsz,
                     ctx->page_flags);

    iovec_t *iovec;
    int iovec_count = paging_iovec(&iovec, blk->p_vaddr,
                                   blk->p_filesz, 1 << 30);

    uint64_t offset = 0;
    for (int i = 0; i < iovec_count; ++i) {
        assert(iovec[i].base < 0x100000000);

        ssize_t read = boot_pread(
                    file, (void*)iovec[i].base, iovec[i].size,
                    blk->p_offset + offset);

        if (unlikely(read != ssize_t(iovec[i].size)))
            PANIC("Disk error");

        offset += iovec[i].size;
    }

    if (offset < blk->p_memsz) {
        free(iovec);
        iovec_count = paging_iovec(&iovec, blk->p_vaddr + blk->p_filesz,
                                   blk->p_memsz - offset, 1 << 30);
        for (int i = 0; i < iovec_count; ++i) {
            assert(iovec[i].base < 0x100000000);
            memset((void*)iovec[i].base, 0, iovec[i].size);
        }
    }

    free(iovec);

    return true;
}
