#include "unittest.h"
#include "thread.h"
#include "mutex.h"

struct test_thread_variation_t {
    size_t seed;
    bool volatile stop;
};

UNITTEST(test_condition_variable_wait_timeout)
{
    std::mutex m;
    std::condition_variable v;
    std::unique_lock<std::mutex> lock(m);

    std::chrono::steady_clock::time_point st_tp =
            std::chrono::steady_clock::now();

    std::cv_status status = v.wait_until(lock, st_tp +
                                         std::chrono::seconds(1));
    eq(true, std::cv_status::timeout == status);

    std::chrono::steady_clock::time_point en_tp =
            std::chrono::steady_clock::now();

    std::chrono::milliseconds elap = en_tp - st_tp;

    // Wide 50ms tolerance to avoid spurious test failures
    le(true, 950 <= elap.count());
    le(true, 1050 >= elap.count());
}

static int test_thread_worker(void *variation)
{
    test_thread_variation_t *var = (test_thread_variation_t*)variation;

    __asm__ __volatile__ (
        "cli\n\t"
        // Save parameter
        "push %%rdi\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        // Get different known value into every register
        "movabs $0x123456789ABCDEF0,%%rax\n\t"
        "movabs $0x0123456789ABCDEF,%%rbx\n\t"
        "movabs $0xF0123456789ABCDE,%%rcx\n\t"
        "movabs $0xEF0123456789ABCD,%%rdx\n\t"
        "movabs $0xDEF0123456789ABC,%%rsi\n\t"
        "movabs $0xCDEF0123456789AB,%%rdi\n\t"
        "movabs $0xBCDEF0123456789A,%%rbp\n\t"
        "movabs $0xABCDEF0123456789,%%r8 \n\t"
        "movabs $0x9ABCDEF012345678,%%r9 \n\t"
        "movabs $0x89ABCDEF01234567,%%r10\n\t"
        "movabs $0x789ABCDEF0123456,%%r11\n\t"
        "movabs $0x6789ABCDEF012345,%%r12\n\t"
        "movabs $0x56789ABCDEF01234,%%r13\n\t"
        "movabs $0x456789ABCDEF0123,%%r14\n\t"
        "movabs $0x3456789ABCDEF012,%%r15\n\t"
        // Get different known value into every register
        "xor (%%rsp),%%rax\n\t"
        "xor (%%rsp),%%rcx\n\t"
        "xor (%%rsp),%%rdx\n\t"
        "xor (%%rsp),%%rsi\n\t"
        "xor (%%rsp),%%rdi\n\t"
        "xor (%%rsp),%%rbp\n\t"
        "xor (%%rsp),%%r8 \n\t"
        "xor (%%rsp),%%r9 \n\t"
        "xor (%%rsp),%%r10\n\t"
        "xor (%%rsp),%%r11\n\t"
        "xor (%%rsp),%%r12\n\t"
        "xor (%%rsp),%%r13\n\t"
        "xor (%%rsp),%%r14\n\t"
        "xor (%%rsp),%%r15\n\t"
        "push %%rax\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rbx\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rcx\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rdx\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rsi\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rdi\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rbp\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r8 \n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r9 \n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r10\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r11\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r12\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r13\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r14\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%r15\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "push %%rsp\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "subq $8,(%%rsp)\n\t"
        "sti\n\t"
        ".balign 16\n\t"
        "0:\n\t"
        "cmp 0*8(%%rsp),%%rsp\n\t"
        "jnz 0f\n\t"
        "cmp 1*8(%%rsp),%%r15\n\t"
        "jnz 0f\n\t"
        "cmp 2*8(%%rsp),%%r14\n\t"
        "jnz 0f\n\t"
        "cmp 3*8(%%rsp),%%r13\n\t"
        "jnz 0f\n\t"
        "cmp 4*8(%%rsp),%%r12\n\t"
        "jnz 0f\n\t"
        "cmp 5*8(%%rsp),%%r11\n\t"
        "jnz 0f\n\t"
        "cmp 6*8(%%rsp),%%r10\n\t"
        "jnz 0f\n\t"
        "cmp 7*8(%%rsp),%%r9\n\t"
        "jnz 0f\n\t"
        "cmp 8*8(%%rsp),%%r8\n\t"
        "jnz 0f\n\t"
        "cmp 9*8(%%rsp),%%rbp\n\t"
        "jnz 0f\n\t"
        "cmp 10*8(%%rsp),%%rdi\n\t"
        "jnz 0f\n\t"
        "cmp 11*8(%%rsp),%%rsi\n\t"
        "jnz 0f\n\t"
        "cmp 12*8(%%rsp),%%rdx\n\t"
        "jnz 0f\n\t"
        "cmp 13*8(%%rsp),%%rcx\n\t"
        "jnz 0f\n\t"
        "cmp 14*8(%%rsp),%%rbx\n\t"
        "jnz 0f\n\t"
        "cmp 15*8(%%rsp),%%rax\n\t"
        "jnz 0f\n\t"
        "jmp 0b\n\t"
        "0:\n\t"
        "ud2\n\t"
        "call *cpu_debug_break@GOT\n\t"
        "jmp 0b\n\t"
        :
        : "D" (var)
    );
    return 0;
}

UNITTEST(test_thread_context)
{
    size_t count = thread_get_cpu_count();
    for (size_t i = 0; i < count; ++i)
        thread_create(test_thread_worker, (char*)0xFEEDBEEFFACEF00D +
                      i * 18446744073709551557U, "context_stress",
                      0,
                      false, false);
    thread_sleep_for(1000);
}
