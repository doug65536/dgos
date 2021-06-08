.section .text, "ax", @progbits

.hidden _Unwind_Resume
.global _Unwind_Resume
_Unwind_Resume:
#if defined(__x86_64__) || defined(__i386__)
    jmp *__module_unwind_resume@GOT
#elif defined(__aarch64__)
    b __module_unwind_resume
#else
#error Unknown processor
#endif

.hidden _Unwind_DebugHook
.global _Unwind_DebugHook
_Unwind_DebugHook:
    ret
