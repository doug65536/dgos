
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

// 32 bit
.macro pushl_cfi val:vararg
    pushl \val
    .cfi_adjust_cfa_offset 4
.endm

// 32 bit
.macro pushfl_cfi
    pushfl
    .cfi_adjust_cfa_offset 4
.endm

// 32 bit
.macro popl_cfi val:vararg
    popl \val
    .cfi_adjust_cfa_offset -4
.endm

// 32 bit
.macro popfl_cfi
    popfl
    .cfi_adjust_cfa_offset -4
.endm


// Preserves flags
.macro adj_rsp_cfi	ofs:vararg
    lea (\ofs)(%rsp),%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

// Clobbers flags
.macro add_rsp_cfi	ofs:vararg
    add $ (\ofs),%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

// 32 bit
.macro adj_esp_cfi	ofs:vararg
    lea (\ofs)(%esp),%esp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

// 32 bit
.macro add_esp_cfi	ofs:vararg
    add $ (\ofs),%esp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

.macro no_caller_cfi
    .cfi_def_cfa rsp,0
    .cfi_undefined rip
    .cfi_undefined rsp
    .cfi_undefined rbp
    .cfi_undefined rbx
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15
.endm
