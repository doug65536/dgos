#pragma once

#include "x86/gdt_sel.h"

#define GDT_SEL_PXE_1ST     GDT_SEL_AVAIL_4
#define GDT_SEL_PXE_STACK   (GDT_SEL_PXE_1ST+0*8)
#define GDT_SEL_PXE_UD      (GDT_SEL_PXE_1ST+1*8)
#define GDT_SEL_PXE_UC      (GDT_SEL_PXE_1ST+2*8)
#define GDT_SEL_PXE_UCW     (GDT_SEL_PXE_1ST+3*8)
#define GDT_SEL_PXE_BD      (GDT_SEL_PXE_1ST+4*8)
#define GDT_SEL_PXE_BC      (GDT_SEL_PXE_1ST+5*8)
#define GDT_SEL_PXE_BCW     (GDT_SEL_PXE_1ST+6*8)
#define GDT_SEL_PXE_ENTRY   (GDT_SEL_PXE_1ST+7*8)
#define GDT_SEL_PXE_TEMP    (GDT_SEL_PXE_1ST+8*8)
