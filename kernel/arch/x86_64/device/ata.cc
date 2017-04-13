#include "ata.h"

format_flag_info_t const ide_flags_error[] = {
    { "AMNF",  ATA_REG_ERROR_AMNF_BIT,  1, 0 },
    { "TKONF", ATA_REG_ERROR_TKONF_BIT, 1, 0 },
    { "ABRT",  ATA_REG_ERROR_ABRT_BIT,  1, 0 },
    { "MCR",   ATA_REG_ERROR_MCR_BIT,   1, 0 },
    { "IDNF",  ATA_REG_ERROR_IDNF_BIT,  1, 0 },
    { "MC",    ATA_REG_ERROR_MC_BIT,    1, 0 },
    { "UNC",   ATA_REG_ERROR_UNC_BIT,   1, 0 },
    { "BBK",   ATA_REG_ERROR_BBK_BIT,   1, 0 },
    { 0,      -1,                      0, 0 }
};
