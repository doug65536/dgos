#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <fcntl.h>
#include <spawn.h>
#include <dirent.h>
#include <pthread.h>
#include <surface.h>
#include <signal.h>
#include <setjmp.h>

#include <sys/mman.h>
#include <sys/likely.h>
#include <sys/module.h>

#include "frameserver.h"

__attribute__((__format__(__printf__, 1, 0), __noreturn__))
void verr(char const *format, va_list ap)
{
    printf("Error:\n");
    vprintf(format, ap);
    exit(1);
}

__attribute__((__format__(__printf__, 1, 2), __noreturn__))
void err(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    verr(format, ap);
    va_end(ap);
}

void load_module(char const *path, char const *parameters = nullptr)
{
    if (!parameters)
        parameters = "";

    int fd = open(path, O_EXCL | O_RDONLY);
    if (unlikely(fd < 0))
        err("Cannot open %s\n", path);

    off_t sz = lseek(fd, 0, SEEK_END);
    if (unlikely(sz < 0))
        err("Cannot seek to end of module\n");

    if (unlikely(lseek(fd, 0, SEEK_SET) != 0))
        err("Cannot seek to start of module\n");

    void *mem = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    if (unlikely(mem == MAP_FAILED))
        err("Cannot allocate %" PRIu64 "d bytes\n", sz);

    if (unlikely(sz != read(fd, mem, sz)))
        err("Cannot read %" PRIu64 " bytes\n", sz);

    close(fd);

    int status;
    char *needed = (char*)malloc(NAME_MAX);
    do {
        needed[0] = 0;
        status = init_module(mem, sz, path, nullptr, parameters, needed);

        if (needed[0] != 0) {
            size_t len = strlen(needed);
            memmove(needed + 5, needed, len + 1);
            memcpy(needed, "boot/", 5);
            load_module(needed);
        }
    } while (needed[0]);
    free(needed);

    if (unlikely(status < 0))
        err("Module failed to initialize with %d %d\n", status, errno);
}


static void *stress_fs(void *)
{
    DIR *dir = opendir("/");

    dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        printf("%s\n", ent->d_name);
    }

    closedir(dir);

    // Create this many files
    size_t iters = 10000;

    // Keep the number of files that exist less than or equal to this many
    size_t depth = 400;

    int mds = mkdir("stress", 0755);

    if (mds < 0 && errno == EROFS) {
        printf("Cannot run mkdir test on readonly filesystem\n");
        return nullptr;
    }

    if (mds < 0) {
        printf("mkdir failed\n");
        return nullptr;
    }

    for (size_t i = 0; i < iters + depth; ++i) {
        char name[NAME_MAX];
        if (i < iters) {
            int name_len = snprintf(name, sizeof(name), "stress/name%zu", i);
            printf("Creating %s\n", name);
            int fd = open(name, O_CREAT | O_EXCL);
            if (write(fd, name, name_len) != name_len)
                printf("Write error writing \"%s\" to fd %d\n", name, fd);
            close(fd);
        }

        if (i >= depth) {
            snprintf(name, sizeof(name), "stress/name%zu", i - depth);
            printf("Unlinking %s\n", name);
            unlink(name);
        }
    }

    return nullptr;
}

void start_fs_stress()
{
    pthread_t stress_tid{};
    int sts = pthread_create(&stress_tid, nullptr, stress_fs, nullptr);

    if (sts != 0)
        printf("pthread_create failed\n");
}

//void start_mouse_thread()
//{
//    pthread_t mouse_thread{};
//    int err = pthread_create(&mouse_thread, nullptr, mouse_test, nullptr);
//    if (unlikely(err))
//        printf("Error creating mouse thread\n");
//}

void syscall_perf_test()
{
    for (size_t iter = 0; iter < 16; ++iter) {
        uint64_t st = __builtin_ia32_rdtsc();
        for (size_t i = 0; i < 1000000; ++i)
            raise(42);
        uint64_t en = __builtin_ia32_rdtsc();
        uint64_t el = en - st;
        printf("One million syscalls in %lu rdtsc ticks (%lu/call)\n",
               el, el/1000000);
    }
}

void *do_nothing(void *arg)
{
    return (void*)(uintptr_t(arg) + 42);
}

void test_thread_create_join()
{
    int pterr;

    uint64_t st = __builtin_ia32_rdtsc();

    size_t it;

    for (it = 0; it < 4096; ++it) {
        pthread_t test_thread = 0;
        pterr = pthread_create(&test_thread, nullptr, do_nothing, (void*)it);

        if (pterr != 0)
            printf("Thread creation failed with %d\n", pterr);

        void *test_return = nullptr;

        pterr = pthread_join(test_thread, &test_return);
        test_thread = 0;

        //pthread_detach(test_thread);

        if (unlikely(test_return != (void*)(uintptr_t((void*)it) + 42))) {
            printf("Thread exit code is wrong"
                   ", expected %p but got %p on iter %zu\n",
                   (void*)(uintptr_t((void*)it) + 42), test_return, it);
        }

        if (pterr != 0)
            printf("Thread join failed with %d\n", pterr);
    }

    uint64_t en = __builtin_ia32_rdtsc();
    uint64_t el = en - st;
    printf("%" PRIu64 " threads ran and joined"
           " in %" PRIu64 "u rdtsc ticks"
           " (%" PRIu64 "/thread)\n",
           it, el, el/it);

    printf("Completed create/join stress loop\n");
}

static uint64_t significand_to_u64(uint16_t const *src)
{
    uint64_t value;
    memcpy(&value, src, sizeof(value));
    return value;
}

void dump_context(mcontext_t *ctx)
{
    mcontext_x86_fpu_t *fpu = (mcontext_x86_fpu_t *)ctx->__fpu;

    printf("   rax=%#18" PRIx64 "\n", ctx->__regs[R_RAX]);
    printf("   rbx=%#18" PRIx64 "\n", ctx->__regs[R_RBX]);
    printf("   rcx=%#18" PRIx64 "\n", ctx->__regs[R_RCX]);
    printf("   rdx=%#18" PRIx64 "\n", ctx->__regs[R_RDX]);
    printf("   rsi=%#18" PRIx64 "\n", ctx->__regs[R_RSI]);
    printf("   rdi=%#18" PRIx64 "\n", ctx->__regs[R_RDI]);
    printf("   rbp=%#18" PRIx64 "\n", ctx->__regs[R_RBP]);
    printf("   rbp=%#18" PRIx64 "\n", ctx->__regs[R_RBP]);
    printf("    r8=%#18" PRIx64 "\n", ctx->__regs[R_R8]);
    printf("    r9=%#18" PRIx64 "\n", ctx->__regs[R_R9]);
    printf("   r10=%#18" PRIx64 "\n", ctx->__regs[R_R10]);
    printf("   r11=%#18" PRIx64 "\n", ctx->__regs[R_R11]);
    printf("   r12=%#18" PRIx64 "\n", ctx->__regs[R_R12]);
    printf("   r13=%#18" PRIx64 "\n", ctx->__regs[R_R13]);
    printf("   r14=%#18" PRIx64 "\n", ctx->__regs[R_R14]);
    printf("   r15=%#18" PRIx64 "\n", ctx->__regs[R_R15]);
    printf("   rip=%#18" PRIx64 "\n", ctx->__rip);
    printf("    cs=%#6" PRIx64 "\n", ctx->__cs);
    printf("rflags=%#18" PRIx64 "\n", ctx->__rflags);
    printf("   rsp=%#18" PRIx64 "\n", ctx->__rsp);
    printf("    ss=%#6" PRIx64 "\n", ctx->__ss);

    for (size_t i = 0; i < 8; ++i) {
        printf("st[%zu]  e=%#6x m=%#18" PRIx64 "\n",
               i, fpu->__st[i].__exponent[0],
               significand_to_u64(fpu->__st[i].__significand));
    }

    for (size_t i = 0; i < 16; ++i) {
        printf("xmm[%2zu] %08x %08x %08x %08x\n", i,
               fpu->__xmm[i].__word[3],
               fpu->__xmm[i].__word[2],
               fpu->__xmm[i].__word[1],
               fpu->__xmm[i].__word[0]);
    }

    printf("mxcsr_mask=%#10x\n", fpu->__mxcsr_mask);
    printf("     mxcsr=%#10x\n", fpu->__mxcsr);

    printf("cwd=%#6x\n", fpu->__cwd);
    printf("swd=%#6x\n", fpu->__swd);
    printf("ftw=%#6x\n", fpu->__ftw);
    printf("fop=%#6x\n", fpu->__fop);
    printf("fip=%#18" PRIx64 "\n", fpu->__rip);
    printf("rdp=%#18" PRIx64 "\n", fpu->__rdp);

    printf("------------------------------\n");
}

enum ill_mode_t {
    no_ill,
    gen_reg,
    lo_xmm,
    hi_xmm,
    lo_mm,
    hi_mm,
};

__thread ill_mode_t volatile ill_mode;

void ill_handler(int sig, siginfo_t *info, void *ucp)
{
    mcontext_t *ctx = (mcontext_t*)ucp;

    printf("Context at #UD:\n");
    dump_context(ctx);

    switch (ill_mode) {
    case no_ill:
        assert(false);
        break;

    case lo_xmm:

        mcontext_x86_fpu_t *fpu;
        fpu = (mcontext_x86_fpu_t*)ctx->__fpu;

        for (size_t i = 0; i < 8; ++i) {
            fpu->__xmm[i].__word[0] ^= -1;
            fpu->__xmm[i].__word[1] ^= -1;
            fpu->__xmm[i].__word[2] ^= -1;
            fpu->__xmm[i].__word[3] ^= -1;
        }

        break;

    case hi_xmm:

        fpu = (mcontext_x86_fpu_t*)ctx->__fpu;

        for (size_t i = 8; i < 16; ++i) {
            fpu->__xmm[i].__word[0] ^= -1;
            fpu->__xmm[i].__word[1] ^= -1;
            fpu->__xmm[i].__word[2] ^= -1;
            fpu->__xmm[i].__word[3] ^= -1;
        }

        break;

    case lo_mm:
        fpu = (mcontext_x86_fpu_t*)ctx->__fpu;
        for (size_t i = 0; i < 4; ++i) {
            fpu->__st[i].__significand[0] ^= -1;
            fpu->__st[i].__significand[1] ^= -1;
            fpu->__st[i].__significand[2] ^= -1;
            fpu->__st[i].__significand[3] ^= -1;
        }

        break;

    case hi_mm:
        fpu = (mcontext_x86_fpu_t*)ctx->__fpu;
        for (size_t i = 4; i < 8; ++i) {
            fpu->__st[i].__significand[0] ^= -1;
            fpu->__st[i].__significand[1] ^= -1;
            fpu->__st[i].__significand[2] ^= -1;
            fpu->__st[i].__significand[3] ^= -1;
        }

        break;

    case gen_reg:
        for (size_t i = 0; i < R_REGS; ++i) {
            ctx->__regs[i] ^= -1;
        }

        break;
    }

    // Skip over the ud2
    ctx->__rip += 2;

    printf("Modified context before returning:\n");
    dump_context(ctx);

    ill_mode = no_ill;
}

#include <xmmintrin.h>

static _always_inline uint64_t testreg(uint64_t base)
{
    return ((base + 3) << 48) |
            ((base + 2) << 32) |
            ((base + 1) << 16) |
            base;
}

static _always_inline __m128i testxmm(uint8_t base)
{
    return _mm_set_epi32(base + 3, base + 2, base + 1, base);
}

static _always_inline __m64 testmm(uint8_t base)
{
    return _mm_set_pi16(base + 3, base + 2, base + 1, base);
}

static _always_inline __m128i invxmm(__m128i r)
{
    return _mm_xor_si128(r, _mm_cmpeq_epi32(r, r));
}

static _always_inline __m64 invmm(__m64 r)
{
    return _mm_xor_si64(r, _mm_cmpeq_pi16(r, r));
}

static _always_inline uint64_t invreg(uint64_t r)
{
    return ~r;
}

void test_reg_ctx()
{
    // We can just barely do them all at once, right at the 30 constraint limit
    // Excluding rsp brings it just within the max

    uint64_t x_r0  = testreg(0x00);
    uint64_t x_r1  = testreg(0x10);
    uint64_t x_r2  = testreg(0x20);
    uint64_t x_r3  = testreg(0x30);
    uint64_t x_r4  = testreg(0x40);
    uint64_t x_r5  = testreg(0x50);
    uint64_t x_r6  = testreg(0x60);
    uint64_t x_r8  = testreg(0x80);
    uint64_t x_r9  = testreg(0x90);
    uint64_t x_r10 = testreg(0xA0);
    uint64_t x_r11 = testreg(0xB0);
    uint64_t x_r12 = testreg(0xC0);
    uint64_t x_r13 = testreg(0xD0);
    uint64_t x_r14 = testreg(0xE0);
    uint64_t x_r15 = testreg(0xF0);

    register uint64_t r0  __asm__("rax") = invreg(x_r0 );
    register uint64_t r1  __asm__("rbx") = invreg(x_r1 );
    register uint64_t r2  __asm__("rcx") = invreg(x_r2 );
    register uint64_t r3  __asm__("rdx") = invreg(x_r3 );
    register uint64_t r4  __asm__("rsi") = invreg(x_r4 );
    register uint64_t r5  __asm__("rdi") = invreg(x_r5 );
    register uint64_t r6  __asm__("rbp") = invreg(x_r6 );
    register uint64_t r8  __asm__("r8")  = invreg(x_r8 );
    register uint64_t r9  __asm__("r9")  = invreg(x_r9 );
    register uint64_t r10 __asm__("r10") = invreg(x_r10);
    register uint64_t r11 __asm__("r11") = invreg(x_r11);
    register uint64_t r12 __asm__("r12") = invreg(x_r12);
    register uint64_t r13 __asm__("r13") = invreg(x_r13);
    register uint64_t r14 __asm__("r14") = invreg(x_r14);
    register uint64_t r15 __asm__("r15") = invreg(x_r15);

    // Tell signal handler to invert all general registers (except rsp)
    ill_mode = gen_reg;

    __asm__ __volatile__ (
        "ud2\n\t"
        : "+r" (r0)
        , "+r" (r1)
        , "+r" (r2)
        , "+r" (r3)
        , "+r" (r4)
        , "+r" (r5)
        , "+r" (r6)
        , "+r" (r8)
        , "+r" (r9)
        , "+r" (r10)
        , "+r" (r11)
        , "+r" (r12)
        , "+r" (r13)
        , "+r" (r14)
        , "+r" (r15)
    );

    if ((r0 ^ x_r0) ||
            (r1 ^ x_r1) ||
            (r2 ^ x_r2) ||
            (r3 ^ x_r3) ||
            (r4 ^ x_r4) ||
            (r5 ^ x_r5) ||
            (r6 ^ x_r6) ||
            (r8 ^ x_r8) ||
            (r9 ^ x_r9) ||
            (r10 ^ x_r10) ||
            (r11 ^ x_r11) ||
            (r12 ^ x_r12) ||
            (r13 ^ x_r13) ||
            (r14 ^ x_r14) ||
            (r15 ^ x_r15)) {
        printf("General register context test FAILED\n");
    } else {
        printf("General register context test passed\n");
    }
}

void test_mm_lo_ctx()
{
    printf("Testing mm lo ctx\n");

    // Expect everything ones complement
    __m64 x_mm0 = testmm(0x00);
    __m64 x_mm1 = testmm(0x10);
    __m64 x_mm2 = testmm(0x20);
    __m64 x_mm3 = testmm(0x30);

    __m64 i_mm0 = invmm(x_mm0);
    __m64 i_mm1 = invmm(x_mm1);
    __m64 i_mm2 = invmm(x_mm2);
    __m64 i_mm3 = invmm(x_mm3);

    __m64 r_mm0;
    __m64 r_mm1;
    __m64 r_mm2;
    __m64 r_mm3;

    {
        register __m64 mm0 __asm__("mm0") = i_mm0;
        register __m64 mm1 __asm__("mm1") = i_mm1;
        register __m64 mm2 __asm__("mm2") = i_mm2;
        register __m64 mm3 __asm__("mm3") = i_mm3;

        ill_mode = lo_mm;

        __asm__ __volatile__ (
            "ud2"
            : "+y" (mm0)
            , "+y" (mm1)
            , "+y" (mm2)
            , "+y" (mm3)
        );

        r_mm0 = mm0;
        r_mm1 = mm1;
        r_mm2 = mm2;
        r_mm3 = mm3;
    }

    __m64 c0 = _mm_cmpeq_pi16(r_mm0, x_mm0);
    __m64 c1 = _mm_cmpeq_pi16(r_mm1, x_mm1);
    c0 = _mm_and_si64(c0, _mm_cmpeq_pi16(r_mm2,  x_mm2));
    c1 = _mm_and_si64(c1, _mm_cmpeq_pi16(r_mm3,  x_mm3));
    c0 = _mm_and_si64(c0, c1);

    if (_mm_movemask_pi8(c0) == 0xFF)
        printf("MMX context test (lo) passed\n");
    else
        printf("MMX context test (lo) FAILED\n");
}

void test_mm_hi_ctx()
{
    printf("Testing mm hi ctx\n");

    // Expect everything ones complement
    __m64 x_mm4 = testmm(0x40);
    __m64 x_mm5 = testmm(0x50);
    __m64 x_mm6 = testmm(0x60);
    __m64 x_mm7 = testmm(0x70);

    __m64 i_mm4 = invmm(x_mm4);
    __m64 i_mm5 = invmm(x_mm5);
    __m64 i_mm6 = invmm(x_mm6);
    __m64 i_mm7 = invmm(x_mm7);

    __m64 r_mm4;
    __m64 r_mm5;
    __m64 r_mm6;
    __m64 r_mm7;

    {
        register __m64 mm4 __asm__("mm4") = i_mm4;
        register __m64 mm5 __asm__("mm5") = i_mm5;
        register __m64 mm6 __asm__("mm6") = i_mm6;
        register __m64 mm7 __asm__("mm7") = i_mm7;

        ill_mode = hi_mm;

        __asm__ __volatile__ (
            "ud2"
            : "+y" (mm4)
            , "+y" (mm5)
            , "+y" (mm6)
            , "+y" (mm7)
        );

        r_mm4 = mm4;
        r_mm5 = mm5;
        r_mm6 = mm6;
        r_mm7 = mm7;
    }

    __m64 c0 = _mm_cmpeq_pi16(r_mm4, x_mm4);
    __m64 c1 = _mm_cmpeq_pi16(r_mm5, x_mm5);
    c0 = _mm_and_si64(c0, _mm_cmpeq_pi16(r_mm6,  x_mm6));
    c1 = _mm_and_si64(c1, _mm_cmpeq_pi16(r_mm7,  x_mm7));
    c0 = _mm_and_si64(c0, c1);

    if (_mm_movemask_pi8(c0) == 0xFF)
        printf("MMX context test (hi) passed\n");
    else
        printf("MMX context test (hi) FAILED\n");
}

void test_xmm_lo_ctx()
{
    printf("Testing xmm lo ctx\n");

    // gcc can't handle them all at once, maximum 30 constraints in an __asm__
    // in/out "+" constraints take two, and we can't do 32 constraints
    // Do half at a time

    __m128i x_xmm0  = testxmm(0x00);
    __m128i x_xmm1  = testxmm(0x10);
    __m128i x_xmm2  = testxmm(0x20);
    __m128i x_xmm3  = testxmm(0x30);
    __m128i x_xmm4  = testxmm(0x40);
    __m128i x_xmm5  = testxmm(0x50);
    __m128i x_xmm6  = testxmm(0x60);
    __m128i x_xmm7  = testxmm(0x70);

    __m128i i_xmm0 = invxmm(x_xmm0);
    __m128i i_xmm1 = invxmm(x_xmm1);
    __m128i i_xmm2 = invxmm(x_xmm2);
    __m128i i_xmm3 = invxmm(x_xmm3);
    __m128i i_xmm4 = invxmm(x_xmm4);
    __m128i i_xmm5 = invxmm(x_xmm5);
    __m128i i_xmm6 = invxmm(x_xmm6);
    __m128i i_xmm7 = invxmm(x_xmm7);

    __m128i r_xmm0;
    __m128i r_xmm1;
    __m128i r_xmm2;
    __m128i r_xmm3;
    __m128i r_xmm4;
    __m128i r_xmm5;
    __m128i r_xmm6;
    __m128i r_xmm7;

    {
        register __m128i xmm0  __asm__("xmm0")  = i_xmm0;
        register __m128i xmm1  __asm__("xmm1")  = i_xmm1;
        register __m128i xmm2  __asm__("xmm2")  = i_xmm2;
        register __m128i xmm3  __asm__("xmm3")  = i_xmm3;
        register __m128i xmm4  __asm__("xmm4")  = i_xmm4;
        register __m128i xmm5  __asm__("xmm5")  = i_xmm5;
        register __m128i xmm6  __asm__("xmm6")  = i_xmm6;
        register __m128i xmm7  __asm__("xmm7")  = i_xmm7;

        ill_mode = lo_xmm;

        __asm__ __volatile__ (
            "ud2"
            : "+x" (xmm0)
            , "+x" (xmm1)
            , "+x" (xmm2)
            , "+x" (xmm3)
            , "+x" (xmm4)
            , "+x" (xmm5)
            , "+x" (xmm6)
            , "+x" (xmm7)
        );

        r_xmm0 = xmm0;
        r_xmm1 = xmm1;
        r_xmm2 = xmm2;
        r_xmm3 = xmm3;
        r_xmm4 = xmm4;
        r_xmm5 = xmm5;
        r_xmm6 = xmm6;
        r_xmm7 = xmm7;
    }

    __m128i c0 = _mm_cmpeq_epi32(r_xmm0, x_xmm0);
    __m128i c1 = _mm_cmpeq_epi32(r_xmm1, x_xmm1);
    __m128i c2 = _mm_cmpeq_epi32(r_xmm2, x_xmm2);
    __m128i c3 = _mm_cmpeq_epi32(r_xmm3, x_xmm3);

    c0 = _mm_and_si128(c0, c1);
    c2 = _mm_and_si128(c2, c3);

    __m128i c4 = _mm_cmpeq_epi32(r_xmm4, x_xmm4);
    __m128i c5 = _mm_cmpeq_epi32(r_xmm5, x_xmm5);
    __m128i c6 = _mm_cmpeq_epi32(r_xmm6, x_xmm6);
    __m128i c7 = _mm_cmpeq_epi32(r_xmm7, x_xmm7);

    c4 = _mm_and_si128(c4, c5);
    c6 = _mm_and_si128(c6, c7);

    c0 = _mm_and_si128(c0, c2);
    c4 = _mm_and_si128(c4, c6);

    c0 = _mm_and_si128(c0, c4);

    if (_mm_movemask_epi8(c0) == 0xFFFF)
        printf("XMM context test (lo) passed\n");
    else
        printf("XMM context test (lo) FAILED\n");
}

void test_xmm_hi_ctx()
{
    printf("Testing xmm hi ctx\n");

    __m128i x_xmm8  = testxmm(0x80);
    __m128i x_xmm9  = testxmm(0x90);
    __m128i x_xmm10 = testxmm(0xA0);
    __m128i x_xmm11 = testxmm(0xB0);
    __m128i x_xmm12 = testxmm(0xC0);
    __m128i x_xmm13 = testxmm(0xD0);
    __m128i x_xmm14 = testxmm(0xE0);
    __m128i x_xmm15 = testxmm(0xF0);

    __m128i i_xmm8  = invxmm(x_xmm8) ;
    __m128i i_xmm9  = invxmm(x_xmm9) ;
    __m128i i_xmm10 = invxmm(x_xmm10);
    __m128i i_xmm11 = invxmm(x_xmm11);
    __m128i i_xmm12 = invxmm(x_xmm12);
    __m128i i_xmm13 = invxmm(x_xmm13);
    __m128i i_xmm14 = invxmm(x_xmm14);
    __m128i i_xmm15 = invxmm(x_xmm15);

    __m128i r_xmm8 ;
    __m128i r_xmm9 ;
    __m128i r_xmm10;
    __m128i r_xmm11;
    __m128i r_xmm12;
    __m128i r_xmm13;
    __m128i r_xmm14;
    __m128i r_xmm15;

    {
        register __m128i xmm8  __asm__("xmm8")  = i_xmm8 ;
        register __m128i xmm9  __asm__("xmm9")  = i_xmm9 ;
        register __m128i xmm10 __asm__("xmm10") = i_xmm10;
        register __m128i xmm11 __asm__("xmm11") = i_xmm11;
        register __m128i xmm12 __asm__("xmm12") = i_xmm12;
        register __m128i xmm13 __asm__("xmm13") = i_xmm13;
        register __m128i xmm14 __asm__("xmm14") = i_xmm14;
        register __m128i xmm15 __asm__("xmm15") = i_xmm15;

        ill_mode = hi_xmm;

        __asm__ __volatile__ (
            "ud2"
            : "+x" (xmm8)
            , "+x" (xmm9)
            , "+x" (xmm10)
            , "+x" (xmm11)
            , "+x" (xmm12)
            , "+x" (xmm13)
            , "+x" (xmm14)
            , "+x" (xmm15)
        );

        r_xmm8  = xmm8 ;
        r_xmm9  = xmm9 ;
        r_xmm10 = xmm10;
        r_xmm11 = xmm11;
        r_xmm12 = xmm12;
        r_xmm13 = xmm13;
        r_xmm14 = xmm14;
        r_xmm15 = xmm15;
    }

    __m128i c0 = _mm_cmpeq_epi32(r_xmm8,  x_xmm8);
    __m128i c1 = _mm_cmpeq_epi32(r_xmm9,  x_xmm9);
    __m128i c2 = _mm_cmpeq_epi32(r_xmm10, x_xmm10);
    __m128i c3 = _mm_cmpeq_epi32(r_xmm11, x_xmm11);

    c0 = _mm_and_si128(c0, c1);
    c2 = _mm_and_si128(c2, c3);

    __m128i c4 = _mm_cmpeq_epi32(r_xmm12, x_xmm12);
    __m128i c5 = _mm_cmpeq_epi32(r_xmm13, x_xmm13);
    __m128i c6 = _mm_cmpeq_epi32(r_xmm14, x_xmm14);
    __m128i c7 = _mm_cmpeq_epi32(r_xmm15, x_xmm15);

    c4 = _mm_and_si128(c4, c5);
    c6 = _mm_and_si128(c6, c7);

    c0 = _mm_and_si128(c0, c2);
    c4 = _mm_and_si128(c4, c6);

    c0 = _mm_and_si128(c0, c4);

    if (_mm_movemask_epi8(c0) == 0xFFFF)
        printf("XMM context test (hi) passed\n");
    else
        printf("XMM context test (hi) FAILED\n");
}

int test_signal()
{
    struct sigaction ill_act = {};
    ill_act.sa_sigaction = ill_handler;

    sigaction(SIGILL, &ill_act, nullptr);

    test_reg_ctx();
    test_xmm_lo_ctx();
    test_xmm_hi_ctx();
    test_mm_lo_ctx();
    test_mm_hi_ctx();

    return 0;
}

int main(int argc, char **argv, char **envp)
{
    printf("init started\n");


    // fixme: check ACPI
    load_module("boot/keyb8042.km");

    load_module("boot/ext4.km");
    load_module("boot/fat32.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_SERIAL,
                      PCI_SUBCLASS_SERIAL_USB,
                      PCI_PROGIF_SERIAL_USB_XHCI) > 0)
        load_module("boot/usbxhci.km");

    load_module("boot/usbmsc.km");

    //mouse_test();

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_NVM,
                      PCI_PROGIF_STORAGE_NVM_NVME) > 0)
        load_module("boot/nvme.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_SATA,
                      PCI_PROGIF_STORAGE_SATA_AHCI) > 0)
        load_module("boot/ahci.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_STORAGE,
                      -1,
                      -1) > 0)
        load_module("boot/virtio-blk.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_DISPLAY,
                      -1,
                      -1) > 0)
        load_module("boot/virtio-gpu.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_ATA, -1))
        load_module("boot/ide.km");

    load_module("boot/iso9660.km");
    load_module("boot/gpt.km");
    load_module("boot/mbr.km");

    if (probe_pci_for(0x10EC, 0x8139,
                      PCI_DEV_CLASS_NETWORK,
                      PCI_SUBCLASS_NETWORK_ETHERNET, -1))
        load_module("boot/rtl8139.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_MULTIMEDIA,
                      PCI_SUBCLASS_MULTIMEDIA_AUDIO, -1))
        load_module("boot/ide.km");

    test_signal();

    //load_module("boot/symsrv.km");


    //syscall_perf_test();

    //start_fs_stress();

    //start_mouse_thread();

    //test_thread_create_join();

    load_module("boot/unittest.km");

    return start_framebuffer();
}

