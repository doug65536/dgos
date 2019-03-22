#pragma once

// Careful, this is included in assembly code

#define CPU_INFO_SELF_OFS           0
#define CPU_INFO_CURTHREAD_OFS      8
#define CPU_INFO_TSS_PTR_OFS        16
#define CPU_INFO_PF_COUNT_OFS       40
#define CPU_INFO_LOCKS_HELD_OFS     48
#define CPU_INFO_CSW_DEFERRED_OFS   52
#define CPU_INFO_TLB_SHOOTDOWN_COUNT_OFS    56
#define CPU_INFO_AFTER_CSW_FN_OFS   112
#define CPU_INFO_AFTER_CSW_VP_OFS   120
#define TSS_RSP0_OFS                8
#define THREAD_XSAVE_PTR_OFS        8
#define THREAD_FSBASE_OFS           16
#define THREAD_GSBASE_OFS           24
#define THREAD_STACK_OFS            88
#define THREAD_SYSCALL_STACK_OFS    32
#define THREAD_PROCESS_PTR_OFS      72
#define THREAD_THREAD_ID_OFS        184
#define SYSCALL_COUNT   315
#define SYSCALL_ENOSYS  -81
#define SYSCALL_RFLAGS  0x202
