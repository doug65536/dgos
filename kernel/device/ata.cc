#include "ata.h"

format_flag_info_t const ide_flags_error[] = {
    { "AMNF",  1, nullptr, ATA_REG_ERROR_AMNF_BIT  },
    { "TKONF", 1, nullptr, ATA_REG_ERROR_TKONF_BIT },
    { "ABRT",  1, nullptr, ATA_REG_ERROR_ABRT_BIT  },
    { "MCR",   1, nullptr, ATA_REG_ERROR_MCR_BIT   },
    { "IDNF",  1, nullptr, ATA_REG_ERROR_IDNF_BIT  },
    { "MC",    1, nullptr, ATA_REG_ERROR_MC_BIT    },
    { "UNC",   1, nullptr, ATA_REG_ERROR_UNC_BIT   },
    { "BBK",   1, nullptr, ATA_REG_ERROR_BBK_BIT   },
    { nullptr, 0, nullptr, -1                      }
};
