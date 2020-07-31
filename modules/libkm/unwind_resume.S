.section .text, "ax", @progbits

.hidden _Unwind_Resume
.global _Unwind_Resume
_Unwind_Resume:
    jmp *__module_unwind_resume@GOT

.hidden _Unwind_DebugHook
.global _Unwind_DebugHook
_Unwind_DebugHook:
    ret
