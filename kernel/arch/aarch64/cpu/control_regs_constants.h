#pragma once

#define MAX_CPUS 512

#define CPU_DAIF_D_BIT  9
#define CPU_DAIF_A_BIT  8
#define CPU_DAIF_I_BIT  7
#define CPU_DAIF_F_BIT  6

// How much to shift to place DAIF in LSBs or vice versa
#define CPU_DAIF_DAIF_BIT CPU_DAIF_F_BIT

#define CPU_DAIF_D      (1U << CPU_DAIF_D_BIT)
#define CPU_DAIF_A      (1U << CPU_DAIF_A_BIT)
#define CPU_DAIF_I      (1U << CPU_DAIF_I_BIT)
#define CPU_DAIF_F      (1U << CPU_DAIF_F_BIT)
#define CPU_DAIF        (CPU_DAIF_D | CPU_DAIF_A | CPU_DAIF_I | CPU_DAIF_F)
#define CPU_DAIF_MASK   (CPU_DAIF >> CPU_DAIF_DAIF_BIT)
