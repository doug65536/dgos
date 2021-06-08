#include "diskio.h"
#include "ctors.h"

#include "bootefi.h"

bool disk_support_64bit_addr()
{
    return true;
}

int disk_sector_size()
{
    return efi_blk_io->Media->BlockSize;
}

bool disk_read_lba(uint64_t addr, uint64_t lba,
                   uint8_t log2_sector_size, unsigned count)
{
    EFI_STATUS efi_status;

    efi_status = efi_blk_io->ReadBlocks(efi_blk_io,
        efi_blk_io->Media->MediaId, lba,
        uint64_t(count) << log2_sector_size,
        (VOID*)addr);

    return !EFI_ERROR(efi_status);
}
