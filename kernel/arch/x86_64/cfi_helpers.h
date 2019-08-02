
.macro push_cfi val
    pushq \val
    .cfi_adjust_cfa_offset 8
.endm

.macro pushfq_cfi
    pushfq
    .cfi_adjust_cfa_offset 8
.endm

.macro pop_cfi val
    popq \val
    .cfi_adjust_cfa_offset -8
.endm

.macro popfq_cfi
    popfq
    .cfi_adjust_cfa_offset -8
.endm

.macro adj_rsp	ofs
    lea (\ofs)(%rsp),%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm
