
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

.macro add_rsp	ofs
    add $\ofs,%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

.macro sub_rsp	ofs
    sub $\ofs,%rsp
    .cfi_adjust_cfa_offset (\ofs)
.endm
