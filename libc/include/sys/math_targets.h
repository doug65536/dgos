#ifdef __x86_64__
#define __TARGET_CLONES \
    __attribute__((__target_clones__("default,avx")))
#else
#define __TARGET_CLONES
#endif
