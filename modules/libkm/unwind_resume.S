.section .text

.hidden _Unwind_Resume
.global _Unwind_Resume
_Unwind_Resume:
    jmp __module_unwind_resume
