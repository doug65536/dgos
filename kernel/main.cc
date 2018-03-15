#include "main.h"
#include "cpu.h"
#include "mm.h"
#include "printk.h"
#include "cpu/halt.h"
#include "thread.h"
#include "device/pci.h"
#include "device/keyb8042.h"
#include "callout.h"
#include "time.h"
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
//#include "zlib/zlib.h"
//#include "zlib_helper.h"
#include "stdlib.h"
#include "png.h"
#include "framebuffer.h"
#include "math.h"
#include "dev_storage.h"
#include "unique_ptr.h"
#include "priorityqueue.h"
#include "device/serial-uart.h"
#include "numeric_limits.h"
#include "vector.h"
#include "process.h"
#include "gdbstub.h"
#include "conio.h"

#include "bootloader.h"

kernel_params_t *kernel_params;

size_t constexpr kernel_stack_size = 16384;
char kernel_stack[kernel_stack_size] __section(".bspstk");

static void smp_main(void *)
{
    printdbg("AP in smp_main...\n");
    cpu_init(1);
}

REGISTER_CALLOUT(smp_main, 0, callout_type_t::smp_start, "100");

// Pull in the device constructors
// to cause them to be initialized
//void (** volatile device_list)(void) = device_constructor_list;

#define TEST_FORMAT(f, t, v) \
    printk("Test %8s -> '" f \
    "' 99=%d\t\t", f, (t)v, 99)

#define ENABLE_SHELL_THREAD         1
#define ENABLE_READ_STRESS_THREAD   0
#define ENABLE_SLEEP_THREAD         0
#define ENABLE_MUTEX_THREAD         0
#define ENABLE_REGISTER_THREAD      0
#define ENABLE_MMAP_STRESS_THREAD   0
#define ENABLE_CTXSW_STRESS_THREAD  0
#define ENABLE_HEAP_STRESS_THREAD   0
#define ENABLE_FRAMEBUFFER_THREAD   0
#define ENABLE_FILESYSTEM_TEST      0
#define ENABLE_SPAWN_STRESS         0

#define ENABLE_STRESS_HEAP_SMALL    0
#define ENABLE_STRESS_HEAP_LARGE    0
#define ENABLE_STRESS_HEAP_BOTH     1

#if ENABLE_STRESS_HEAP_SMALL
#define STRESS_HEAP_MINSIZE         64
#define STRESS_HEAP_MAXSIZE         4080
#elif ENABLE_STRESS_HEAP_LARGE
#define STRESS_HEAP_MINSIZE         65536
#define STRESS_HEAP_MAXSIZE         65536 //262144
#elif ENABLE_STRESS_HEAP_BOTH
#define STRESS_HEAP_MINSIZE         64
#define STRESS_HEAP_MAXSIZE         65536
#elif ENABLE_HEAP_STRESS_THREAD
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

    printk("Shell running\n");

    for (;;) {
        keyboard_event_t event = keybd_waitevent();

        if (event.codepoint > 0)
            printk("%c", event.codepoint);
    }

    return 0;
}
#endif

#if ENABLE_READ_STRESS_THREAD > 0
static int read_stress(void *p)
{
    static uint8_t counts[ENABLE_READ_STRESS_THREAD << 6];
    static int next_id;
    static int completion_count;
    int id = atomic_xadd(&next_id, 1);
    assert(id < ENABLE_READ_STRESS_THREAD);

    static size_t constexpr queue_depth = 4;
    blocking_iocp_t iocp[queue_depth];

    (void)p;
    thread_t tid = thread_get_id();

    storage_dev_base_t *drive = open_storage_dev(1);

    if (!drive)
        return 0;

    size_t data_size = 4096;

    char *data[queue_depth];
    for (size_t i = 0; i < queue_depth; ++i)
        data[i] = (char*)mmap(0, data_size, PROT_READ | PROT_WRITE, 0, -1, 0);

    size_t data_blocks = data_size / drive->info(STORAGE_INFO_BLOCKSIZE);

    printk("read buffer at %lx\n", (uint64_t)data);
    printk("read stress iocp list at %p\n", (void*)iocp);

    uint64_t last_time = time_ns();
    uint64_t last_completions = completion_count;

    errno_t status;

    // Prime the queue
    for (size_t i = 0; i < queue_depth; ++i) {
        status = drive->read_async(data[i], 1, i, &iocp[i]);
        if (status != errno_t::OK)
            printdbg("(%3d) Storage read (completion failed) status=%d\n",
                     tid, (int)status);
    }

    size_t slot = 0;

    uint64_t seed = 42;
    char buf[ENABLE_READ_STRESS_THREAD * 3 + 2 + 64];
    while (1) {
        ++*(short*)p;

        uint64_t lba = rand_r_range(&seed, 16, 1000 - data_blocks);
        //int64_t count = rand_r_range(&seed, 1, data_blocks);

        status = iocp[slot].wait();
        if (status != errno_t::OK)
            printdbg("(%3d) Storage read (completion failed) status=%d\n",
                     tid, (int)status);
        iocp[slot].reset();
        int64_t count = data_blocks;
        status = drive->read_async(data, count, lba, &iocp[slot]);
        if (++slot == queue_depth)
            slot = 0;

        if (status != errno_t::OK)
            printdbg("(%3d) Storage read (issue failed) status=%d\n",
                     tid, (int)status);

        atomic_inc(counts + (id << 6));

        uint64_t completions = atomic_xadd(&completion_count, 1);

        if ((completions & 32767) == 32767) {
            uint64_t now = time_ns();
            uint64_t delta_time = now - last_time;
            int ofs = 0;
            if (delta_time >= 1000000000) {
                for (int s = 0; s < ENABLE_READ_STRESS_THREAD; ++s) {
                    ofs += snprintf(buf + ofs, sizeof(buf) - ofs, "%2x ",
                                    counts[s << 6]);
                }

                uint64_t completion_delta = completions - last_completions;
                last_completions = completions;

                ofs += snprintf(buf + ofs, sizeof(buf) - ofs, "%lu",
                                completion_delta);

                ofs += snprintf(buf + ofs, sizeof(buf) - ofs, " %lu ms",
                                (now - last_time) / 1000000);

                last_time = now;
            }

            if (ofs) {
                buf[ofs++] = 0;
                printdbg("%s\n", buf);
            }
        }
    }

    return 0;
}
#endif

#if ENABLE_SLEEP_THREAD
struct test_thread_param_t {
    uint16_t *p;
    int sleep;
};

static int other_thread(void *p)
{
    test_thread_param_t *tp = (test_thread_param_t *)p;
    while (1) {
        int odd = ++(*tp->p);
        if (tp->sleep)
            thread_sleep_for(tp->sleep);
        //else

        thread_set_affinity(thread_get_id(),
                            (1UL << (odd % thread_get_cpu_count())));
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
    uint64_t seed = this_thread * 6364136223846793005;

    for (;;) {
        mutex_lock(&stress_lock);

        assert(mutex_check == -1);

        mutex_check = this_thread;

        thread_sleep_for(1 + ((uint64_t(rand_r(&seed)) * 100) >> 31));

        assert(mutex_check == this_thread);

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

    char *mem = (char*)mmap(0, 256 << 20, PROT_NONE, 0, -1, 0);

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
        "cli\n\t"
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
        "push %%rax\n\t"
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
        "sti\n\t"
        "0:\n\t"
        "cmp 0*8(%%rsp),%%r15\n\t"
        "jnz 0f\n\t"
        "cmp 1*8(%%rsp),%%r14\n\t"
        "jnz 0f\n\t"
        "cmp 2*8(%%rsp),%%r13\n\t"
        "jnz 0f\n\t"
        "cmp 3*8(%%rsp),%%r12\n\t"
        "jnz 0f\n\t"
        "cmp 4*8(%%rsp),%%r11\n\t"
        "jnz 0f\n\t"
        "cmp 5*8(%%rsp),%%r10\n\t"
        "jnz 0f\n\t"
        "cmp 6*8(%%rsp),%%r9\n\t"
        "jnz 0f\n\t"
        "cmp 7*8(%%rsp),%%r8\n\t"
        "jnz 0f\n\t"
        "cmp 8*8(%%rsp),%%rbp\n\t"
        "jnz 0f\n\t"
        "cmp 9*8(%%rsp),%%rdi\n\t"
        "jnz 0f\n\t"
        "cmp 10*8(%%rsp),%%rsi\n\t"
        "jnz 0f\n\t"
        "cmp 11*8(%%rsp),%%rdx\n\t"
        "jnz 0f\n\t"
        "cmp 12*8(%%rsp),%%rcx\n\t"
        "jnz 0f\n\t"
        "cmp 13*8(%%rsp),%%rbx\n\t"
        "jnz 0f\n\t"
        "cmp 14*8(%%rsp),%%rax\n\t"
        "jnz 0f\n\t"
        "jmp 0b\n\t"
        "0:\n\t"
        "ud2\n\t"
        "call cpu_debug_break\n\t"
        "jmp 0b\n\t"
        :
        : "a" (p)
    );
    return 0;
}
#endif

#if ENABLE_MMAP_STRESS_THREAD > 0
static int stress_mmap_thread(void *p)
{
    (void)p;
    void *block;
    uint64_t seed = 42;
    for (;;) {
        //uint64_t time_st = time_ns();
        for (unsigned iter = 0; iter < 50; ++iter) {
            int size = rand_r_range(&seed, 1, 131072);
            assert(size >= 1);
            assert(size < 131072);

            block = mmap(0, size,
                         PROT_READ | PROT_WRITE,
                         0, -1, 0);

            memset(block, 0xcc, size);

            munmap(block, size);

        }
        //uint64_t time_en = time_ns();
        //printdbg("Ran mmap test iteration %luns\n", (time_en - time_st)/50);
    }
    return 0;
}
#endif

#if ENABLE_HEAP_STRESS_THREAD > 0
static int stress_heap_thread(void *p)
{
    (void)p;

    heap_t *heap = heap_create();
    uint64_t mina_el, maxa_el, tota_el;
    uint64_t minf_el, maxf_el, totf_el;
    uint64_t st, el;
    uint64_t seed = 42;
    while (1) {
        for (int pass = 0; pass < 16; ++pass) {
            tota_el = 0;
            totf_el = 0;
            maxa_el = 0;
            maxf_el = 0;
            mina_el = ~0L;
            minf_el = ~0L;

            void *history[16];
            int history_index = 0;
            memset(history, 0, sizeof(history));

            unsigned count = 0;
            int size;
            static constexpr unsigned iters = 0x100000;
            uint64_t overall = cpu_rdtsc();
            for (count = 0; count < iters; ++count) {
                size = rand_r_range(&seed, STRESS_HEAP_MINSIZE,
                                    STRESS_HEAP_MAXSIZE);

                cpu_irq_disable();

                if (likely(history[history_index])) {
                    st = cpu_rdtsc();
                    heap_free(heap, history[history_index]);
                    history[history_index] = nullptr;
                    el = cpu_rdtsc() - st;

                    if (maxf_el < el)
                        maxf_el = el;
                    if (minf_el > el)
                        minf_el = el;
                    totf_el += el;
                }

                st = cpu_rdtsc();
                void *block = heap_alloc(heap, size);
                el = cpu_rdtsc() - st;

                cpu_irq_enable();

                history[history_index++] = block;
                history_index &= countof(history)-1;

                if (maxa_el < el)
                    maxa_el = el;
                if (mina_el > el)
                    mina_el = el;
                tota_el += el;
            }

            for (unsigned i = 0; i < (unsigned)countof(history); ++i) {
                st = cpu_rdtsc();
                heap_free(heap, history[i]);
                history[i] = nullptr;
                el = cpu_rdtsc() - st;

                if (maxf_el < el)
                    maxf_el = el;
                if (minf_el > el)
                    minf_el = el;
                totf_el += el;
            }

            overall = cpu_rdtsc() - overall;

            printdbg("heap_alloc+memset+heap_free:"
                     " mna=%8ld (%5luns @ 3.2GHz), mxa=%8ld, ava=%8ld,\n"
                     "                            "
                     " mnf=%8ld (%5luns @ 3.2GHz), mxf=%8ld, avf=%8ld,\n"
                     "                            "
                     " withfree=%8ld cycles\n",
                     mina_el, mina_el * 10 / 32, maxa_el, tota_el / iters,
                     minf_el, minf_el * 10 / 32, maxf_el, totf_el / iters,
                     overall / iters);
        }
    }
    heap_destroy(heap);
    return 0;
}
#endif

int clks_unhalted(void *cpu)
{
    //auto cpu_nr = size_t(cpu);
    //uint64_t last = thread_get_usage(-1);
    //while (1) {
    //    thread_sleep_for(1000);
    //    uint64_t curr = thread_get_usage(-1);
    //    printdbg("CPU %zx: %lu clocks\n", cpu_nr, curr - last);
    //    last = curr;
    //}
    return 0;
}

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
    char *bios = (char*)mmap(p, 0x10000, PROT_READ, MAP_PHYSICAL, -1, 0);

    int i;
    for (i = 0; i < 0x8000 - 20; ++i) {
        if (bios[i] == 'D' &&
                bios[i+1] == 'I' &&
                bios[i+2] == 'M' &&
                bios[i+3] == 'P' &&
                sum_bytes(bios + i, 20) == 0) {
            printdbg("Found VBE PM Interface at %x!\n",
                     (uint32_t)(uintptr_t)p + i);
            break;
        }
    }

    if (i == 0x8000 - 20)
        printdbg("No VBE PM Interface at %p\n", p);

    munmap(bios, 0x8000);
    return 0;
}

#if ENABLE_FRAMEBUFFER_THREAD > 0
static int draw_test(void *p)
{
    (void)p;

    int frames = 0;

    // 1280x800
    unique_ptr<png_image_t> img(png_load("background.png"));
    uint64_t st_tm = time_ns();
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
            if (img) {
                fb_copy_to(sx, sy, img->width,
                           img->width, img->height, png_pixels(img));

                fb_fill_rect(sx, 0, sx + img->width, sy, 255);

                fb_fill_rect(sx, sy + img->height,
                             sx + img->width, 1080, 255*256);

                //uint64_t line_st = cpu_rdtsc();
            }

            fb_draw_aa_line(40, 60, sx+640, sy+300, 0xBFBFBF & -!(sx & 1));

            //uint64_t line_en = cpu_rdtsc();
            //printdbg("Line draw %ld cycles\n", line_en - line_st);

            fb_update();
            ++frames;
        }
    }
    uint64_t en_tm = time_ns();
    printdbg("Benchmark time: %d frames, %ldms\n", frames,
             (en_tm - st_tm) / 1000000);

    return 0;
}
#endif

static int init_thread(void *p)
{
    (void)p;

    printdbg("Initializing PCI\n");
    pci_init();

    printdbg("Initializing keyboard event queue\n");
    keybd_init();

    printdbg("Initializing 8042 keyboard\n");
    keyb8042_init();

    // Facilities needed by drivers
    printdbg("Initializing driver base\n");
    callout_call(callout_type_t::driver_base);

    // Run late initializations
    printdbg("Initializing late devices\n");
    callout_call(callout_type_t::late_dev);

    // Register USB interfaces
    printdbg("Initializing USB interfaces\n");
    callout_call(callout_type_t::usb);

    // Register filesystems
    printdbg("Initializing filesystems\n");
    callout_call(callout_type_t::reg_filesys);

    // Storage interfaces
    printdbg("Initializing storage devices\n");
    callout_call(callout_type_t::storage_dev);

    // Register partition schemes
    printdbg("Initializing partition probes\n");
    callout_call(callout_type_t::partition_probe);

    // Register network interfaces
    printdbg("Initializing network interfaces\n");
    callout_call(callout_type_t::nic);

    //bootdev_info(0, 0, 0);

#if ENABLE_SPAWN_STRESS > 0
    printdbg("Starting spawn stress with %d threads\n", ENABLE_SPAWN_STRESS);
    for (size_t i = 0; i < ENABLE_SPAWN_STRESS; ++i) {
        pid_t pid = 0;
        int spawn_result = process_t::spawn(
                    &pid, "user-shell", nullptr, nullptr);
        //thread_sleep_for(10000);
        printdbg("Started user mode process with PID=%d, status=%d\n",
                 pid, spawn_result);
        assert(spawn_result == 0 || pid < 0);
    }
#endif

    printdbg("Initializing framebuffer\n");
    fb_init();

    //priqueue_test.test();

#if ENABLE_FILESYSTEM_TEST
    printdbg("Starting filesystem test\n");
    for (int n = 0; n < 1000; ++n) {
        char name[16];
        snprintf(name, sizeof(name), "created_%d", n);
        printk("creating %s\n", name);
        int create_test = file_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        file_write(create_test, "Hello!", 6);
        file_close(create_test);
        printk(" created %s\n\n", name);
    }

//    for (int i = 0; i < 10000; ++i) {
//        printdbg("%d=%f\n", i, i / 1000.0);
//    }

    //void *user_test = mmap((void*)0x400000, 1<<20,
    //                       PROT_READ | PROT_WRITE, MAP_USER, -1, 0);
    //munmap(user_test, 1<<20);

    printdbg("Testing floating point formatter\n");
    printdbg("Float formatter: %%17.5f     42.8      -> %17.5f\n", 42.8);
    printdbg("Float formatter: %%17.5f     42.8e+60  -> %17.5f\n", 42.8e+60);
    printdbg("Float formatter: %%17.5f     42.8e-60  -> %17.5f\n", 42.8e-60);
    printdbg("Float formatter: %%+17.5f    42.8e-60  -> %+17.5f\n", 42.8e-60);
    printdbg("Float formatter: %%17.5f    -42.8      -> %17.5f\n", -42.8);
    printdbg("Float formatter: %%17.5f    -42.8e+60  -> %17.5f\n", -42.8e+60);
    printdbg("Float formatter: %%17.5f    -42.8e-60  -> %17.5f\n", -42.8e-60);
    printdbg("Float formatter: %%017.5f    42.8      -> %017.5f\n", 42.8);
    printdbg("Float formatter: %%017.5f   -42.8e+60  -> %017.5f\n", -42.8e+60);
    printdbg("Float formatter: %%+017.5f   42.8      -> %+017.5f\n", 42.8);
    printdbg("Float formatter: %%+017.5f  -42.8e+60  -> %+017.5f\n", -42.8e+60);

    printdbg("Float formatter: %%17.5e     42.8      -> %17.5e\n", 42.8);
    printdbg("Float formatter: %%17.5e     42.8e+60  -> %17.5e\n", 42.8e+60);
    printdbg("Float formatter: %%17.5e     42.8e-60  -> %17.5e\n", 42.8e-60);
    printdbg("Float formatter: %%+17.5e    42.8e-60  -> %+17.5e\n", 42.8e-60);
    printdbg("Float formatter: %%17.5e    -42.8      -> %17.5e\n", -42.8);
    printdbg("Float formatter: %%17.5e    -42.8e+60  -> %17.5e\n", -42.8e+60);
    printdbg("Float formatter: %%17.5e    -42.8e-60  -> %17.5e\n", -42.8e-60);
    printdbg("Float formatter: %%017.5e    42.8      -> %017.5e\n", 42.8);
    printdbg("Float formatter: %%017.5e   -42.8e+60  -> %017.5e\n", -42.8e+60);
    printdbg("Float formatter: %%+017.5e   42.8      -> %+017.5e\n", 42.8);
    printdbg("Float formatter: %%+017.5e  -42.8e+60  -> %+017.5e\n", -42.8e+60);

    printdbg("Opening root directory\n");

    int od = file_opendir("");
    dirent_t de;
    dirent_t *dep;
    while (file_readdir_r(od, &de, &dep) > 0) {
        printdbg("File: %s\n", de.d_name);
    }
    file_closedir(od);
#endif

    printdbg("Running mprotect self test\n");
    mprotect_test(0);

    printdbg("Running red-black tree self test\n");
    rbtree_t<>::test();

#if ENABLE_FRAMEBUFFER_THREAD > 0
    printdbg("Starting framebuffer stress\n");
    printdbg("Running framebuffer stress\n");
    thread_t draw_thread_id = thread_create(draw_test, 0, 0, 0);
    printdbg("draw thread id=%d\n", draw_thread_id);
#endif

    modload_init();

    module_entry_fn_t mod_entry = modload_load("hello.km");
    if (mod_entry)
        mod_entry();

    thread_create(find_vbe, (void*)0xC0000, 0, false);
    thread_create(find_vbe, (void*)0xF0000, 0, false);

#if ENABLE_CTXSW_STRESS_THREAD > 0
    printdbg("Running context switch stress with %d threads\n",
             ENABLE_CTXSW_STRESS_THREAD);
    for (int i = 0; i < ENABLE_CTXSW_STRESS_THREAD; ++i) {
        thread_create(ctx_sw_thread, 0, 0, false);
    }
#endif

#if ENABLE_SHELL_THREAD > 0
    printdbg("Running shell thread\n");
    thread_create(shell_thread, (void*)0xfeedbeeffacef00d, 0, false);
#endif

#if ENABLE_SLEEP_THREAD
    printdbg("Running sleep stress with %d threads\n",
             ENABLE_SLEEP_THREAD);

    static test_thread_param_t ttp[ENABLE_SLEEP_THREAD];

    for (int i = 0; i < ENABLE_SLEEP_THREAD; ++i) {
        ttp[i].sleep = i * 100;
        ttp[i].p = (uint16_t*)0xb8000 + 4 + i;
        thread_create(other_thread, ttp + i, 0, false);
    }
#endif

#if ENABLE_READ_STRESS_THREAD > 0
    printdbg("Running block read stress with %d threads\n",
             ENABLE_READ_STRESS_THREAD);
    for (int i = 0; i < ENABLE_READ_STRESS_THREAD; ++i) {
        thread_t tid = thread_create(read_stress, (char*)(uintptr_t)
                      (0xb8000+ 80*2 + 2*i), 0, false);
        printdbg("Read stress id[%d]=%d\n", i, tid);
    }
#endif

#if ENABLE_REGISTER_THREAD > 0
    printdbg("Running register stress with %d threads\n",
             ENABLE_READ_STRESS_THREAD);
    for (int i = 0; i < ENABLE_REGISTER_THREAD; ++i) {
        thread_create(register_check, (void*)
                      (0xDEADFEEDF00DD00D +
                       (1<<ENABLE_READ_STRESS_THREAD)), 0, false);
    }
#endif

#if ENABLE_MUTEX_THREAD > 0
    printdbg("Running mutex stress with %d threads\n", ENABLE_MUTEX_THREAD);
    mutex_init(&stress_lock);
    for (int i = 0; i < ENABLE_MUTEX_THREAD; ++i) {
        thread_create(stress_mutex, 0, 0, false);
    }
#endif

#if ENABLE_MMAP_STRESS_THREAD > 0
    printdbg("Running mmap stress with %d threads\n",
             ENABLE_MMAP_STRESS_THREAD);
    for (int i = 0; i < ENABLE_MMAP_STRESS_THREAD; ++i) {
        thread_create(stress_mmap_thread, 0, 0, false);
    }
#endif

#if ENABLE_HEAP_STRESS_THREAD > 0
    printdbg("Running heap stress with %d threads\n",
             ENABLE_HEAP_STRESS_THREAD);
    for (int i = 0; i < ENABLE_HEAP_STRESS_THREAD; ++i) {
        thread_create(stress_heap_thread, 0, 0, false);
    }
#endif

    return 0;
}

int debugger_thread(void *)
{
    gdb_init();

    thread_create(init_thread, 0, 0, false);

    return 0;
}

extern "C" __noreturn int main(void)
{
    thread_create(debugger_thread, 0, 0, false);

    thread_idle_set_ready();

    cpu_irq_enable();

    thread_idle();
}
