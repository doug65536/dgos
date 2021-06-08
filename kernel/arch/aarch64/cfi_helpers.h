
// function parameters
#define CFI_X0 0
#define CFI_X1 1
#define CFI_X2 2
#define CFI_X3 3
#define CFI_X4 4
#define CFI_X5 5
#define CFI_X6 6
#define CFI_X7 7

// indirect return value pointer
#define CFI_X8 8

// clobbered
#define CFI_X9 9
#define CFI_X10 10
#define CFI_X11 11
#define CFI_X12 12
#define CFI_X13 13
#define CFI_X14 14
#define CFI_X15 15

// intra-procedure scratch
#define CFI_X16 16
#define CFI_X17 17

// platform register
#define CFI_X18 18

// preserved
#define CFI_X19 19
#define CFI_X20 20
#define CFI_X21 21
#define CFI_X22 22
#define CFI_X23 23
#define CFI_X24 24
#define CFI_X25 25
#define CFI_X26 26
#define CFI_X27 27
#define CFI_X28 28
#define CFI_X29 29

// LR
#define CFI_X30 30

// Not really accessible
#define CFI_SP 31
#define CFI_PC 32

// The current mode exception link register
#define CFI_ELR_mode 33

// Return address signed state pseudo-register (Note 8)
#define CFI_RA_SIGN_STATE 34

// EL0 Read-Only Software Thread ID register
#define CFI_TPIDRRO_ELO 35

// EL0 Read/Write Software Thread ID register
#define CFI_TPIDR_ELO 36

// EL1 Software Thread ID register
#define CFI_TPIDR_EL1 37

// EL2 Software Thread ID register
#define CFI_TPIDR_EL2 38

// EL3 Software Thread ID register
#define CFI_TPIDR_EL3 39

// Reserved
// 40-45
// -

// 64-bit SVE vector granule pseudo-register (Note 2, Note 3)
#define CFI_VG 46
#define CFI_FFR 47

// P0-P15 (Beta)
#define CFI_VG_P0 48
#define CFI_VG_P1 49
#define CFI_VG_P2 50
#define CFI_VG_P3 51
#define CFI_VG_P4 52
#define CFI_VG_P5 53
#define CFI_VG_P6 54
#define CFI_VG_P7 55
#define CFI_VG_P8 56
#define CFI_VG_P9 57
#define CFI_VG_P10 58
#define CFI_VG_P11 59
#define CFI_VG_P12 60
#define CFI_VG_P13 61
#define CFI_VG_P14 62
#define CFI_VG_P15 63

// VG × 8-bit SVE predicate registers (Note 4)
#define CFI_VG_V0 64
#define CFI_VG_V1 65
#define CFI_VG_V2 66
#define CFI_VG_V3 67
#define CFI_VG_V4 68
#define CFI_VG_V5 69
#define CFI_VG_V6 70
#define CFI_VG_V7 71
#define CFI_VG_V8 72
#define CFI_VG_V9 73
#define CFI_VG_V10 74
#define CFI_VG_V11 75
#define CFI_VG_V12 76
#define CFI_VG_V13 77
#define CFI_VG_V14 78
#define CFI_VG_V15 79
#define CFI_VG_V16 80
#define CFI_VG_V17 81
#define CFI_VG_V18 82
#define CFI_VG_V19 83
#define CFI_VG_V20 84
#define CFI_VG_V21 85
#define CFI_VG_V22 86
#define CFI_VG_V23 87
#define CFI_VG_V24 88
#define CFI_VG_V25 89
#define CFI_VG_V26 90
#define CFI_VG_V27 91
#define CFI_VG_V28 92
#define CFI_VG_V29 93
#define CFI_VG_V30 94
#define CFI_VG_V31 95

// 128-bit FP/Advanced SIMD registers (Note 5, Note 7)
// VG × 64-bit SVE vector registers (Note 6, Note 7)
#define CFI_VG_Z0 96
#define CFI_VG_Z1 97
#define CFI_VG_Z2 98
#define CFI_VG_Z3 99
#define CFI_VG_Z4 100
#define CFI_VG_Z5 101
#define CFI_VG_Z6 102
#define CFI_VG_Z7 103
#define CFI_VG_Z8 104
#define CFI_VG_Z9 105
#define CFI_VG_Z10 106
#define CFI_VG_Z11 107
#define CFI_VG_Z12 108
#define CFI_VG_Z13 109
#define CFI_VG_Z14 110
#define CFI_VG_Z15 111
#define CFI_VG_Z16 112
#define CFI_VG_Z17 113
#define CFI_VG_Z18 114
#define CFI_VG_Z19 115
#define CFI_VG_Z20 116
#define CFI_VG_Z21 117
#define CFI_VG_Z22 118
#define CFI_VG_Z23 119
#define CFI_VG_Z24 120
#define CFI_VG_Z25 121
#define CFI_VG_Z26 122
#define CFI_VG_Z27 123
#define CFI_VG_Z28 124
#define CFI_VG_Z29 125
#define CFI_VG_Z30 126
#define CFI_VG_Z31 127

.macro push_cfi val:vararg
    pushq \val
    .cfi_adjust_cfa_offset 8
.endm

.macro pushfq_cfi
    pushfq
    .cfi_adjust_cfa_offset 8
.endm

.macro pop_cfi val:vararg
    popq \val
    .cfi_adjust_cfa_offset -8
.endm

.macro popfq_cfi
    popfq
    .cfi_adjust_cfa_offset -8
.endm

.macro adj_rsp_cfi	ofs:vararg
    lea (\ofs)(%rsp),%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

.macro add_rsp_cfi	ofs:vararg
    add $ (\ofs),%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

.macro no_caller_cfi
    .cfi_undefined CFI_X30
    .cfi_undefined CFI_X29
    .cfi_undefined CFI_X28
    .cfi_undefined CFI_X27
    .cfi_undefined CFI_X26
    .cfi_undefined CFI_X25
    .cfi_undefined CFI_X24
    .cfi_undefined CFI_X23
    .cfi_undefined CFI_X22
    .cfi_undefined CFI_X21
    .cfi_undefined CFI_X20
    .cfi_undefined CFI_X19
    .cfi_undefined CFI_PC
.endm

.macro push_lr_fp_adj sp_adj
    sub sp,sp,sp_adj
    stp x30,x29,[sp, stp_adj-16]
    mov x29,sp
.endm

.macro pop_lr_fp_adj sp_adj
    ldp x30,x29,[sp, stp_adj-16]
    mov sp,x29
.endm

.macro push_lr_fp
    stp x30,x29,[sp, -16]!
    mov x29,sp
.endm

.macro pop_lr_fp
    ldp x30,x29,[sp],16
    mov sp,x29
.endm
