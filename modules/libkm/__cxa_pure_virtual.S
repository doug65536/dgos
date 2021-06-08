
.balign 16
.global __cxa_pure_virtual
.hidden __cxa_pure_virtual
__cxa_pure_virtual:
#if defined(__x86_64__) || defined(__i386__)
    jmp *__cxa_pure_virtual@got
#elif defined(__aarch64__)
    b __cxa_pure_virtual
#else
#error Unknown processor
#endif

