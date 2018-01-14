#pragma once

// Careful, this is included in assembly code

#define CPU_INFO_SELF_OFS           0
#define CPU_INFO_CURTHREAD_OFS      8
#define CPU_INFO_TSS_PTR_OFS        16
#define TSS_RSP0_OFS                8
#define THREAD_XSAVE_PTR_OFS        8
#define THREAD_FSBASE_OFS           24
#define THREAD_GSBASE_OFS           32
#define THREAD_STACK_OFS            96
#define THREAD_SYSCALL_STACK_OFS    40
#define THREAD_PROCESS_PTR_OFS      80
#define SYSCALL_COUNT   315
#define SYSCALL_ENOSYS  -81
#define SYSCALL_RFLAGS  0x202
