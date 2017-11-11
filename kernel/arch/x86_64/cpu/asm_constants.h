#pragma once

// Careful, this is included in assembly code

#define CPU_INFO_CURTHREAD_OFS      8
#define THREAD_XSAVE_PTR_OFS        8
#define THREAD_XSAVE_STACK_OFS      16
#define THREAD_SYSCALL_RIP          24
#define THREAD_SYSCALL_STACK_OFS    32
#define THREAD_PROCESS_PTR_OFS      40

#define SYSCALL_COUNT   314
#define SYSCALL_ENOSYS  -81
#define SYSCALL_RFLAGS  0x202
