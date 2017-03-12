#include "main.h"
#include "cpu.h"
#include "mm.h"
#include "printk.h"
#include "cpu/halt.h"
#include "thread.h"
#include "device/pci.h"
#include "device/keyb8042.h"
#include "callout.h"
#include "device/ahci.h"
#include "time.h"
#include "dev_storage.h"
#include "rbtree.h"
#include "keyboard.h"
#include "threadsync.h"
#include "assert.h"
#include "cpu/atomic.h"
#include "cpu/control_regs.h"
#include "rand.h"
#include "string.h"
#include "heap.h"
#include "elf64.h"
#include "fileio.h"
#include "bootdev.h"
#include "zlib/zlib.h"
#include "zlib_helper.h"
#include "stdlib.h"
#include "png.h"
#include "framebuffer.h"
#include "math.h"

size_t const kernel_stack_size = 16384;
char kernel_stack[16384];

extern void (*device_constructor_list[])(void);

void volatile *trick;

static void smp_main(void *arg)
{
    (void)arg;
    cpu_init(1);
}

REGISTER_CALLOUT(smp_main, 0, 'S', "100");

// Pull in the device constructors
// to cause them to be initialized
void (** volatile device_list)(void) = device_constructor_list;

#define TEST_FORMAT(f, t, v) \
    printk("Test %8s -> '" f \
    "' 99=%d\t\t", f, (t)v, 99)

#define ENABLE_SHELL_THREAD         1
#define ENABLE_AHCI_STRESS_THREAD   0
#define ENABLE_SLEEP_THREAD         0
#define ENABLE_MUTEX_THREAD         0
#define ENABLE_REGISTER_THREAD      0
#define ENABLE_STRESS_MMAP_THREAD   0
#define ENABLE_CTXSW_STRESS_THREAD  0
#define ENABLE_STRESS_HEAP_THREAD   0

#define ENABLE_STRESS_HEAP_SMALL    0
#define ENABLE_STRESS_HEAP_LARGE    1
#define ENABLE_STRESS_HEAP_BOTH     0

#if ENABLE_STRESS_HEAP_SMALL
#define STRESS_HEAP_MINSIZE     64
#define STRESS_HEAP_MAXSIZE     4080
#elif ENABLE_STRESS_HEAP_LARGE
#define STRESS_HEAP_MINSIZE     65536
#define STRESS_HEAP_MAXSIZE     262144
#elif ENABLE_STRESS_HEAP_BOTH
#define STRESS_HEAP_MINSIZE     64
#define STRESS_HEAP_MAXSIZE     65536
#elif ENABLE_STRESS_HEAP_THREAD
#error Must enable a size range
#endif

#if ENABLE_CTXSW_STRESS_THREAD > 0
static int ctx_sw_thread(void *p)
{
    (void)p;
    while (1)
        thread_yield();
    return 0;
}

#endif

#if ENABLE_SHELL_THREAD > 0
static int shell_thread(void *p)
{
    (void)p;

    for (;;) {
        keyboard_event_t event = keybd_waitevent();

        if (event.codepoint > 0)
            printk("%c", event.codepoint);
    }

    //printk("From shell thread!! %016lx", (uint64_t)p);
    //

    return 0;
}
#endif

#if ENABLE_AHCI_STRESS_THREAD > 0
static int read_stress(void *p)
{
    (void)p;

    storage_dev_base_t *drive = open_storage_dev(0);

    // FIXME: lock the pages in the driver instead of
    // requiring MAP_POPULATE
    char *data = mmap(0, 65536, PROT_READ | PROT_WRITE,
                      MAP_POPULATE, -1, 0);

    printk("read buffer at %lx\n", (uint64_t)data);

    uint64_t lba = 16;//rand_range(16, 32);
    while (1) {
        ++*(char*)p;

        drive->vtbl->read_blocks(drive, data, 1, lba);

        if (++lba > 32)
            lba = 16;
    }

    return 0;
}
#endif

#if ENABLE_SLEEP_THREAD
typedef struct test_thread_param_t {
    uint16_t *p;
    int sleep;
} test_thread_param_t;

static int other_thread(void *p)
{
    test_thread_param_t *tp = p;
    while (1) {
        int odd = ++(*tp->p) & 1;
        if (tp->sleep)
            thread_sleep_for(tp->sleep);
        else
            thread_set_affinity(thread_get_id(), 1 << odd);
    }
    return 0;
}
#endif

#if ENABLE_MUTEX_THREAD
mutex_t stress_lock;

thread_t volatile mutex_check = -1;

static int stress_mutex(void *p)
{
    (void)p;
    thread_t this_thread = thread_get_id();

    for (;;) {
        mutex_lock(&stress_lock);

        assert(mutex_check == -1);

        mutex_check = this_thread;

        for (int i = 0; i < 16000; ++i) {
            assert(mutex_check == this_thread);
            pause();
        }

        mutex_check = -1;

        mutex_unlock(&stress_lock);
    }
    return 0;
}
#endif

#include "cpu/except.h"

#if 1
static int mprotect_test(void *p)
{
    (void)p;
    return 0;

    char *mem = mmap(0, 256 << 20, PROT_NONE, 0, -1, 0);

    __try {
        *mem = 'H';
    }
    __catch {
        printdbg("Caught!!\n");
    }

    if (-1 != mprotect(0, 42, PROT_NONE))
        assert_msg(0, "Expected error");

    if (1 != mprotect(mem, 42, PROT_NONE))
        assert_msg(0, "Expected success");

    if (1 != mprotect(mem + 8192, 16384, PROT_NONE))
        assert_msg(0, "Expected success");

    if (1 != mprotect(mem, 256 << 20, PROT_READ | PROT_WRITE))
        assert_msg(0, "Expected success");

    memset(mem, 0, 2 << 20);

    munmap(mem, 256 << 20);

    return 0;
}
#endif

#if ENABLE_REGISTER_THREAD
static int register_check(void *p)
{
    (void)p;
    __asm__ __volatile__ (
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
        "xor %%rax,%%rbx\n\t"
        "xor %%rax,%%rcx\n\t"
        "xor %%rax,%%rdx\n\t"
        "xor %%rax,%%rsi\n\t"
        "xor %%rax,%%rdi\n\t"
        "xor %%rax,%%rbp\n\t"
        "xor %%rax,%%r8 \n\t"
        "xor %%rax,%%r9 \n\t"
        "xor %%rax,%%r10\n\t"
        "xor %%rax,%%r11\n\t"
        "xor %%rax,%%r12\n\t"
        "xor %%rax,%%r13\n\t"
        "xor %%rax,%%r14\n\t"
        "xor %%rax,%%r15\n\t"
        "andq $-16,%%rsp\n\t"
        "push %%rbx\n\t"
        "push %%rcx\n\t"
        "push %%rdx\n\t"
        "push %%rsi\n\t"
        "push %%rdi\n\t"
        "push %%rbp\n\t"
        "push %%r8 \n\t"
        "push %%r9 \n\t"
        "push %%r10\n\t"
        "push %%r11\n\t"
        "push %%r12\n\t"
        "push %%r13\n\t"
        "push %%r14\n\t"
        "push %%r15\n\t"

        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm15\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm14\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm13\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm12\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm11\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm10\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm9\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm8\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm7\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm6\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm5\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm4\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm3\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm2\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm1\n\t"
        "rol %%r8\n\t"
        "ror %%r9\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "movdqa (%%rsp),%%xmm0\n\t"
        "push $0\n\t"
        "push $0\n\t"
        "mov 17*16+ 6*8(%%rsp),%%r9\n\t"
        "mov 17*16+ 7*8(%%rsp),%%r8\n\t"
        "0:\n\t"
        "psubd 1 *16(%%rsp),%%xmm0 \n\t"
        "psubd 2 *16(%%rsp),%%xmm1 \n\t"
        "psubd 3 *16(%%rsp),%%xmm2 \n\t"
        "psubd 4 *16(%%rsp),%%xmm3 \n\t"
        "psubd 5 *16(%%rsp),%%xmm4 \n\t"
        "psubd 6 *16(%%rsp),%%xmm5 \n\t"
        "psubd 7 *16(%%rsp),%%xmm6 \n\t"
        "psubd 8 *16(%%rsp),%%xmm7 \n\t"
        "psubd 9 *16(%%rsp),%%xmm8 \n\t"
        "psubd 10*16(%%rsp),%%xmm9 \n\t"
        "psubd 11*16(%%rsp),%%xmm10\n\t"
        "psubd 12*16(%%rsp),%%xmm11\n\t"
        "psubd 13*16(%%rsp),%%xmm12\n\t"
        "psubd 14*16(%%rsp),%%xmm13\n\t"
        "psubd 15*16(%%rsp),%%xmm14\n\t"
        "psubd 16*16(%%rsp),%%xmm15\n\t"
        "pcmpeqd (%%rsp),%%xmm0 \n\t"
        "pcmpeqd (%%rsp),%%xmm1 \n\t"
        "pcmpeqd (%%rsp),%%xmm2 \n\t"
        "pcmpeqd (%%rsp),%%xmm3 \n\t"
        "pcmpeqd (%%rsp),%%xmm4 \n\t"
        "pcmpeqd (%%rsp),%%xmm5 \n\t"
        "pcmpeqd (%%rsp),%%xmm6 \n\t"
        "pcmpeqd (%%rsp),%%xmm7 \n\t"
        "pcmpeqd (%%rsp),%%xmm8 \n\t"
        "pcmpeqd (%%rsp),%%xmm9 \n\t"
        "pcmpeqd (%%rsp),%%xmm10\n\t"
        "pcmpeqd (%%rsp),%%xmm11\n\t"
        "pcmpeqd (%%rsp),%%xmm12\n\t"
        "pcmpeqd (%%rsp),%%xmm13\n\t"
        "pcmpeqd (%%rsp),%%xmm14\n\t"
        "pcmpeqd (%%rsp),%%xmm15\n\t"
        "movmskps %%xmm0,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm0,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm1,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm2,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm3,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm4,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm5,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm6,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm7,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm8,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm9,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm10,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm11,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm12,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm13,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm14,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "movmskps %%xmm15,%%eax\n\t"
        "cmp $0x0F,%%eax\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 0*8(%%rsp),%%r15\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 1*8(%%rsp),%%r14\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 2*8(%%rsp),%%r13\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 3*8(%%rsp),%%r12\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 4*8(%%rsp),%%r11\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 5*8(%%rsp),%%r10\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 6*8(%%rsp),%%r9\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 7*8(%%rsp),%%r8\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 8*8(%%rsp),%%rbp\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 9*8(%%rsp),%%rdi\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 10*8(%%rsp),%%rsi\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 11*8(%%rsp),%%rdx\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 12*8(%%rsp),%%rcx\n\t"
        "jnz 0f\n\t"
        "cmp 17*16+ 13*8(%%rsp),%%rbx\n\t"
        "jnz 0f\n\t"
        "movdqa 1 *16(%%rsp),%%xmm0 \n\t"
        "movdqa 2 *16(%%rsp),%%xmm1 \n\t"
        "movdqa 3 *16(%%rsp),%%xmm2 \n\t"
        "movdqa 4 *16(%%rsp),%%xmm3 \n\t"
        "movdqa 5 *16(%%rsp),%%xmm4 \n\t"
        "movdqa 6 *16(%%rsp),%%xmm5 \n\t"
        "movdqa 7 *16(%%rsp),%%xmm6 \n\t"
        "movdqa 8 *16(%%rsp),%%xmm7 \n\t"
        "movdqa 9 *16(%%rsp),%%xmm8 \n\t"
        "movdqa 10*16(%%rsp),%%xmm9 \n\t"
        "movdqa 11*16(%%rsp),%%xmm10\n\t"
        "movdqa 12*16(%%rsp),%%xmm11\n\t"
        "movdqa 13*16(%%rsp),%%xmm12\n\t"
        "movdqa 14*16(%%rsp),%%xmm13\n\t"
        "movdqa 15*16(%%rsp),%%xmm14\n\t"
        "movdqa 16*16(%%rsp),%%xmm15\n\t"
        "jmp 0b\n\t"
        "0:\n\t"
        "ud2\n\t"
        :
        : "a" (p)
    );
    return 0;
}
#endif

#if ENABLE_STRESS_MMAP_THREAD > 0
static int stress_mmap_thread(void *p)
{
    (void)p;
    void *block;
    uint64_t seed = 42;
    for (;;) {
        for (unsigned iter = 0; iter < 50; ++iter) {
            int size = rand_r_range(&seed, 1, 131072);
            assert(size >= 1);
            assert(size < 131072);

            block = mmap(0, size,
                         PROT_READ | PROT_WRITE,
                         0, -1, 0);

            memset(block, 0, size);

            munmap(block, size);

            //printdbg("Ran mmap test iteration %u\n", iter);
        }
    }
    return 0;
}
#endif

#if ENABLE_STRESS_HEAP_THREAD > 0
static int stress_heap_thread(void *p)
{
    (void)p;


    heap_t *heap = heap_create();
    uint64_t min_el;
    uint64_t max_el;
    uint64_t tot_el;
    uint64_t seed = 42;
    while (1) {
        for (int pass = 0; pass < 16; ++pass) {
            tot_el = 0;
            max_el = 0;
            min_el = ~0L;

            void *history[16];
            int history_index = 0;
            memset(history, 0, sizeof(history));

            int count = 0;
            int size;
            for (count = 0; count < 0x1000; ++count) {
                size = rand_r_range(&seed,
                                    STRESS_HEAP_MINSIZE,
                                    STRESS_HEAP_MAXSIZE);

                heap_free(heap, history[history_index]);
                history[history_index] = 0;

                uint64_t st = cpu_rdtsc();
                void *block = heap_alloc(heap, size);
                //printdbg("Allocated size=%d\n", size);
                uint64_t el = cpu_rdtsc() - st;

                history[history_index++] = block;
                history_index &= countof(history)-1;

                if (max_el < el)
                    max_el = el;
                if (min_el > el)
                    min_el = el;
                tot_el += el;
            }

            for (int i = 0; i < (int)countof(history); ++i) {
                heap_free(heap, history[i]);
                history[i] = 0;
            }

            printdbg("heap_alloc+memset+heap_free:"
                     " min=%12ld,"
                     " max=%12ld,"
                     " avg=%12ld cycles\n",
                     min_el, max_el, tot_el / count);
        }
    }
    heap_destroy(heap);
    return 0;
}
#endif

extern void usbxhci_detect(void*);
void (*usbxhci_pull_in)(void*) = usbxhci_detect;

static uint8_t sum_bytes(char *st, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += st[i];
    return sum;
}

static int find_vbe(void *p)
{
    char *bios = mmap(p, 0x10000, PROT_READ, MAP_PHYSICAL, -1, 0);

    int i;
    for (i = 0; i < 0x8000 - 20; ++i) {
        if (bios[i] == 'D' &&
                bios[i+1] == 'I' &&
                bios[i+2] == 'M' &&
                bios[i+3] == 'P' &&
                sum_bytes(bios + i, 20) == 0) {
            printdbg("Found VBE PM Interface at %x!\n", (uint32_t)(uintptr_t)p + i);
            break;
        }
    }

    if (i == 0x8000 - 20)
        printdbg("No VBE PM Interface at %p\n", p);

    munmap(bios, 0x8000);
    return 0;
}

static int init_thread(void *p)
{
    (void)p;

    // Run late initializations
    callout_call('L');

    // Register filesystems
    callout_call('F');

    // Register partition schemes
    callout_call('P');

    // Register network interfaces
    callout_call('N');

    // Register USB interfaces
    callout_call('U');

    bootdev_info(0, 0, 0);

    png_image_t *img = png_load("background.png");

    fb_init();

    printdbg("Floating point formatter: %%17.5f     42.8      -> %17.5f\n", 42.8);
    printdbg("Floating point formatter: %%17.5f     42.8e+60  -> %17.5f\n", 42.8e+60);
    printdbg("Floating point formatter: %%17.5f     42.8e-60  -> %17.5f\n", 42.8e-60);
    printdbg("Floating point formatter: %%+17.5f    42.8e-60  -> %+17.5f\n", 42.8e-60);
    printdbg("Floating point formatter: %%17.5f    -42.8      -> %17.5f\n", -42.8);
    printdbg("Floating point formatter: %%17.5f    -42.8e+60  -> %17.5f\n", -42.8e+60);
    printdbg("Floating point formatter: %%17.5f    -42.8e-60  -> %17.5f\n", -42.8e-60);
    printdbg("Floating point formatter: %%017.5f    42.8      -> %017.5f\n", 42.8);
    printdbg("Floating point formatter: %%017.5f   -42.8e+60  -> %017.5f\n", -42.8e+60);
    printdbg("Floating point formatter: %%+017.5f   42.8      -> %+017.5f\n", 42.8);
    printdbg("Floating point formatter: %%+017.5f  -42.8e+60  -> %+017.5f\n", -42.8e+60);

    printdbg("Floating point formatter: %%17.5e     42.8      -> %17.5e\n", 42.8);
    printdbg("Floating point formatter: %%17.5e     42.8e+60  -> %17.5e\n", 42.8e+60);
    printdbg("Floating point formatter: %%17.5e     42.8e-60  -> %17.5e\n", 42.8e-60);
    printdbg("Floating point formatter: %%+17.5e    42.8e-60  -> %+17.5e\n", 42.8e-60);
    printdbg("Floating point formatter: %%17.5e    -42.8      -> %17.5e\n", -42.8);
    printdbg("Floating point formatter: %%17.5e    -42.8e+60  -> %17.5e\n", -42.8e+60);
    printdbg("Floating point formatter: %%17.5e    -42.8e-60  -> %17.5e\n", -42.8e-60);
    printdbg("Floating point formatter: %%017.5e    42.8      -> %017.5e\n", 42.8);
    printdbg("Floating point formatter: %%017.5e   -42.8e+60  -> %017.5e\n", -42.8e+60);
    printdbg("Floating point formatter: %%+017.5e   42.8      -> %+017.5e\n", 42.8);
    printdbg("Floating point formatter: %%+017.5e  -42.8e+60  -> %+017.5e\n", -42.8e+60);

    uint64_t st_tm = time_ms();

    int frames = 0;

    int od = file_opendir("");
    dirent_t de;
    dirent_t *dep;
    while (file_readdir_r(od, &de, &dep)) {
        printdbg("File: %s\n", de.d_name);
    }
    file_closedir(od);

    // 1280x800
    for (int sx = 1280-1920; sx < 0; sx += 15) {
        int step, sy1, sy2;
        step = ((sx & 1) << 1) - 1;
        if (step > 0) {
            sy1 = 800-1080;
            sy2 = 0;
        } else {
            sy1 = 0;
            sy2 = 800-1080;
        }
        for (int sy = sy1; sy != sy2; sy += step) {
            fb_copy_to(sx, sy, img->width,
                       img->width, img->height, png_pixels(img));
            fb_fill_rect(sx, 0, sx + img->width, sy, 255);
            fb_fill_rect(sx, sy + img->height, sx + img->width, 1080, 255*256);
            fb_update();
            ++frames;
        }
    }

    uint64_t en_tm = time_ms();

    printdbg("Benchmark time: %d frames, %ldms\n", frames, en_tm - st_tm);

    free(img);

    //modload_init();

    thread_create(find_vbe, (void*)0xC0000, 0, 0);
    thread_create(find_vbe, (void*)0xF0000, 0, 0);

#if ENABLE_CTXSW_STRESS_THREAD > 0
    for (int i = 0; i < ENABLE_CTXSW_STRESS_THREAD; ++i) {
        thread_create(ctx_sw_thread, 0, 0, 0);
    }
#endif

#if ENABLE_SHELL_THREAD > 0
    thread_create(shell_thread, (void*)0xfeedbeeffacef00d, 0, 0);
#endif

#if ENABLE_SLEEP_THREAD
    static test_thread_param_t ttp[ENABLE_SLEEP_THREAD];

    for (int i = 0; i < ENABLE_SLEEP_THREAD; ++i) {
        ttp[i].sleep = i * 100;
        ttp[i].p = (uint16_t*)0xb8000 + 4 + i;
        thread_create(other_thread, ttp + i, 0, 0);
    }
#endif

#if ENABLE_AHCI_STRESS_THREAD > 0
    for (int i = 0; i < ENABLE_AHCI_STRESS_THREAD; ++i) {
        thread_create(read_stress, (char*)(uintptr_t)
                      (0xb8000+ 80*2 + 2*i), 0, 0);
    }
#endif

#if ENABLE_REGISTER_THREAD > 0
    for (int i = 0; i < ENABLE_REGISTER_THREAD; ++i) {
        thread_create(register_check, (void*)
                      (0xDEADFEEDF00DD00D +
                       (1<<ENABLE_AHCI_STRESS_THREAD)), 0, 0);
    }
#endif

#if ENABLE_MUTEX_THREAD > 0
    mutex_init(&stress_lock);
    for (int i = 0; i < ENABLE_MUTEX_THREAD; ++i) {
        thread_create(stress_mutex, 0, 0, 0);
    }
#endif

#if ENABLE_STRESS_MMAP_THREAD > 0
    for (int i = 0; ENABLE_STRESS_MMAP_THREAD; ++i) {
        thread_create(stress_mmap_thread, 0, 0, 0);
    }
#endif

#if ENABLE_STRESS_HEAP_THREAD > 0
    for (int i = 0; i < ENABLE_STRESS_HEAP_THREAD; ++i) {
        thread_create(stress_heap_thread, 0, 0, 0);
    }
#endif

    return 0;
}

int main(void)
{
    pci_init();
    keybd_init();
    keyb8042_init();

    mprotect_test(0);

    rbtree_test();

    thread_create(init_thread, 0, 0, 0);

    thread_idle_set_ready();

    while (1)
        halt();

    return 0;
}
