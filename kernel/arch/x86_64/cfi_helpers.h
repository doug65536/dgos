
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
