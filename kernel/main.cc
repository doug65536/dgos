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
#include "keyboard.h"
#include "threadsync.h"
#include "assert.h"
#include "cpu/atomic.h"
#include "rand.h"
#include "string.h"
#include "heap.h"
#include "elf64.h"
#include "fileio.h"
//#include "zlib/zlib.h"
//#include "zlib_helper.h"
#include "stdlib.h"
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
#include "inttypes.h"
#include "work_queue.h"
#include "cpu/except_asm.h"
#include "fs/tmpfs.h"
#include "bootloader.h"
#include "engunit.h"
#include "stacktrace.h"
#include "bootinfo.h"
#include "cpu/perf.h"

kernel_params_t *kernel_params;

size_t const kernel_stack_size = (16 << 10);
char kernel_stack[kernel_stack_size] _section(".bspstk");

#define TEST_FORMAT(f, t, v) \
    printk("Test %8s -> '" f \
    "' 99=%d\t\t", f, (t)v, 99)

#define ENABLE_SHELL_THREAD         0
#define ENABLE_READ_STRESS_THREAD   0
#define ENABLE_SLEEP_THREAD         0 // no affinity or moving across cpus yet
#define ENABLE_MUTEX_THREAD         0
#define ENABLE_REGISTER_THREAD      0
#define ENABLE_MMAP_STRESS_THREAD   0
#define ENABLE_CTXSW_STRESS_THREAD  0
#define ENABLE_HEAP_STRESS_THREAD   0
#define ENABLE_FRAMEBUFFER_THREAD   0
#define ENABLE_FILESYSTEM_WR_TEST   0
#define ENABLE_SPAWN_STRESS         0
#define ENABLE_SHELL                0
#define ENABLE_STRESS_HEAP_SMALL    1
#define ENABLE_STRESS_HEAP_LARGE    0
#define ENABLE_STRESS_HEAP_BOTH     0
#define ENABLE_FIND_VBE             0
#define ENABLE_UNWIND               0

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

    printk("Shell running: %#.4x\n", 42);

    for (;;) {
        keyboard_event_t event = keybd_waitevent();

        if (event.codepoint > 0)
            printk("%c", event.codepoint);
    }

    return 0;
}
#endif

#if ENABLE_READ_STRESS_THREAD > 0
class read_stress_thread_t {
public:
    thread_t start(dev_t devid, uint16_t *indicator, int index)
    {
        this->devid = devid;
        this->indicator = indicator;
        this->index = index;
        tid = thread_create(&read_stress_thread_t::worker, this,
                            "read_stress", 0,
                            false, false);
        return tid;
    }

private:
    static int worker(void *arg)
    {
        return reinterpret_cast<read_stress_thread_t*>(arg)->worker();
    }

    int worker()
    {
        static uint8_t counts[256 << 6];
        static int volatile next_id;
        static int completion_count;
        int id = atomic_xadd(&next_id, 1);

        //thread_set_affinity(tid, UINT64_C(1) << (index % thread_cpu_count()));

        storage_dev_base_t *drive = storage_dev_open(devid);

        if (!drive) {
            printk("(devid %d) failed to open\n", devid);
            return 0;
        }

        size_t data_size = 4096;

        for (size_t i = 0; i < queue_depth; ++i) {
            data[i] = (char*)mmap(nullptr, data_size,
                                  PROT_READ | PROT_WRITE, 0);
            printk("(devid %d) read buffer at %#" PRIx64 "\n",
                   devid, (uint64_t)data[i]);
        }

        size_t data_blocks = data_size / drive->info(STORAGE_INFO_BLOCKSIZE);
        printk("(dev %d) read stress iocp list at %p\n",
               devid, (void*)iocp);

        uint64_t last_time = time_ns();
        uint64_t last_completions = completion_count;

        errno_t status;

        // Prime the queue
        for (size_t i = 0; i < queue_depth; ++i) {
            status = drive->read_async(data[i], 1, i, &iocp[i]);
            if (status != errno_t::OK)
                printdbg("(devid %d) (tid %3d)"
                         " Storage read (completion failed) status=%d\n",
                         devid, tid, (int)status);
        }

        size_t slot = 0;

        uint64_t seed = 42;
        while (1) {
            //++*indicator;

            uint64_t lba = rand_r_range(&seed, 16, data_blocks);
            //int64_t count = rand_r_range(&seed, 1, data_blocks);

            status = iocp[slot].wait().first;
            iocp[slot].reset();
            if (status != errno_t::OK) {
                printdbg("(devid %d) (tid %3d)"
                         " Storage read (completion failed asynchronously)"
                         " status=%d\n",
                         devid, tid, (int)status);
                return 0;
            }
            int64_t count = data_blocks;
            status = drive->read_async(data[slot], count, lba, &iocp[slot]);
            if (++slot == queue_depth)
                slot = 0;

            //thread_sleep_for(100);

            if (status != errno_t::OK) {
                printdbg("(devid %d) (%3d)"
                         " Storage read (issue failed) status=%d\n",
                         devid, tid, (int)status);
                return 0;
            }

            atomic_inc(counts + (id << 6));

            uint64_t completions = atomic_xadd(&completion_count, 1);

            if ((completions & 32767) == 32767) {
                uint64_t now = time_ns();
                uint64_t delta_time = now - last_time;
                int ofs = 0;
                if (delta_time >= 1000000000) {
                    for (size_t s = 0; s < ENABLE_READ_STRESS_THREAD; ++s) {
                        ofs += snprintf(buf + ofs, sizeof(buf) - ofs, "%#2x ",
                                        counts[s << 6]);
                    }

                    uint64_t completion_delta = completions - last_completions;
                    last_completions = completions;

                    ofs += snprintf(buf + ofs, sizeof(buf) - ofs,
                                    "delta=%" PRIu64, completion_delta);

                    auto ms = (now - last_time) / 1000000;

                    ofs += snprintf(buf + ofs, sizeof(buf) - ofs,
                                    " %" PRId64 " ms, %" PRIu64 "/sec",
                                    ms, 1000 * completion_delta / ms);

                    last_time = now;
                }

                if (ofs) {
                    buf[ofs++] = 0;
                    putsdbg(buf);
                }
            }
        }

        return 0;
    }

    static size_t constexpr queue_depth = 16;
    blocking_iocp_t iocp[queue_depth];
    char *data[queue_depth];
    char buf[ENABLE_READ_STRESS_THREAD * 3 + 2 + 128];

    uint16_t *indicator;
    dev_t devid;
    thread_t tid;
    int index;
};

void test_read_stress()
{
    int dev_cnt = storage_dev_count();
    std::vector<read_stress_thread_t*> *read_stress_threads =
            new (std::nothrow) std::vector<read_stress_thread_t*>();
    if (unlikely(!read_stress_threads->reserve(
                     dev_cnt * ENABLE_READ_STRESS_THREAD)))
        panic("Out of memory");

    for (int i = 0; i < ENABLE_READ_STRESS_THREAD; ++i) {
        for (int devid = 0; devid < dev_cnt; ++devid) {
            printk("(devid %d, worker %d)"
                   " Running block read stress\n",
                     devid, i);
            read_stress_thread_t *thread =
                    new (std::nothrow) read_stress_thread_t();
            if (unlikely(!read_stress_threads->push_back(thread)))
                panic_oom();
            uint16_t *indicator = (uint16_t*)0xb8000 + 80*devid + i;
            thread_t tid = thread->start(devid, indicator, i);
            printk("(devid %d) Read stress id[%d]=%d\n", devid, i, tid);
        }
    }
}
#endif

struct test_thread_param_t {
    int v;
    int sleep;
};

#if ENABLE_SLEEP_THREAD
static int other_thread(void *p)
{
    test_thread_param_t *tp = (test_thread_param_t *)p;
    while (1) {
        int odd = ++tp->v;
        if (tp->sleep)
            thread_sleep_for(tp->sleep);

        thread_set_affinity(thread_get_id(),
                            thread_cpu_mask_t(odd % thread_get_cpu_count()));
    }
    return 0;
}

static test_thread_param_t ttp[ENABLE_SLEEP_THREAD];

void test_sleep()
{
    printk("Running sleep stress with %d threads\n",
             ENABLE_SLEEP_THREAD);

    for (int i = 0; i < ENABLE_SLEEP_THREAD; ++i) {
        ttp[i].sleep = i * 100;
        ttp[i].p = (uint16_t*)0xb8000 + 4 + i;
        thread_create(other_thread, ttp + i, "test_sleep", 0, false, false);
    }
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

#if 0
static int mprotect_test(void *)
{
//    char *mem = (char*)mmap(nullptr, 256 << 20, PROT_NONE, 0);

//    __try {
//        *mem = 'H';
//    }
//    __catch {
//        printk("Caught!!\n");
//    }

//    if (-1 != mprotect(nullptr, 42, PROT_NONE))
//        assert_msg(0, "Expected error");

//    if (1 != mprotect(mem, 42, PROT_NONE))
//        assert_msg(0, "Expected success");

//    if (1 != mprotect(mem + 8192, 16384, PROT_NONE))
//        assert_msg(0, "Expected success");

//    if (1 != mprotect(mem, 256 << 20, PROT_READ | PROT_WRITE))
//        assert_msg(0, "Expected success");

//    memset(mem, 0, 2 << 20);

//    munmap(mem, 256 << 20);

    return 0;
}
#endif

#if ENABLE_REGISTER_THREAD
static int register_check(void *p)
{
    return 0;
}
#endif

#if ENABLE_MMAP_STRESS_THREAD > 0
static int stress_mmap_thread(void *p)
{
    uintptr_t id = uintptr_t(p);

    void *block;
    //uint64_t last_free = mm_memory_remaining();

    size_t block_count = 16;
    std::unique_ptr<std::pair<uintptr_t, uintptr_t>[]> blocks(
                new (std::nothrow)
                std::pair<uintptr_t, uintptr_t>[block_count]);
    size_t current = 0;

    rand_lfs113_t rand;
    rand.seed(id);

    size_t total_sz = 0;
    size_t sz;
    int divisor = 2000000;
    for (;;) {
        uint64_t time_st = time_ns();
        for (unsigned iter = 0; iter < 1; ++iter) {
            sz = rand.lfsr113_rand();
            sz = 42 + (sz >> (32 - 17));
            total_sz += sz;

            std::pair<uintptr_t, uintptr_t> &
                    current_block = blocks[current];
            if (blocks[current].second)
                munmap((void*)current_block.first, current_block.second);

            block = mmap(nullptr, sz,
                         PROT_READ | PROT_WRITE,
                         MAP_UNINITIALIZED | MAP_NOCOMMIT);

            blocks[current] = {uintptr_t(block), sz};

            if (++current == block_count)
                current = 0;

            //memset(block, 0xcc, sz);

//            if (!--divisor) {
//                divisor = 20;
//                uint64_t free_pages = mm_memory_remaining();
//                if (last_free != free_pages) {
//                    last_free = free_pages;
//                    printdbg("Free: %" PRIu64 " pages\n",
//                             free_pages);
//                }
//            }
        }

        uint64_t time_el = time_ns() - time_st;
        if (--divisor == 0) {
            divisor = 2000000;
            printk("%2zu: Ran mmap test iteration"
                   ", sz: %4sB"
                   ", time: %4ss\n",
                   id,
                   engineering_t<uint64_t>(total_sz, 0, true).ptr(),
                   engineering_t<uint64_t>(time_el, -3).ptr());
        }
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

            printk("heap_alloc+memset+heap_free:"
                     " mna=%8" PRId64 " (%5" PRIu64 "ns @ 3.2GHz)"
                     ", mxa=%8" PRId64 ", ava=%8" PRId64 ",\n"
                     "                            "
                     " mnf=%8" PRId64 " (%5" PRIu64 "ns @ 3.2GHz)"
                     ", mxf=%8" PRId64 ", avf=%8" PRId64 ",\n"
                     "                            "
                     " withfree=%8" PRId64 " cycles\n",
                     mina_el, mina_el * 10 / 32, maxa_el, tota_el / iters,
                     minf_el, minf_el * 10 / 32, maxf_el, totf_el / iters,
                     overall / iters);
        }
    }
    heap_destroy(heap);
    return 0;
}
#endif

_noreturn
int clks_unhalted(void *cpu)
{
    auto cpu_nr = size_t(cpu);
    uint64_t last = thread_get_usage(-1);
    while (1) {
        thread_sleep_for(1000);
        uint64_t curr = thread_get_usage(-1);
        printk("CPU %#zx: %" PRIu64 " clocks\n", cpu_nr, curr - last);
        last = curr;
    }
}

#if ENABLE_FIND_VBE
static uint8_t sum_bytes(char *st, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += st[i];
    return sum;
}

static int find_vbe(void *p)
{
    char *bios = (char*)mmap(p, 0x10000, PROT_READ, MAP_PHYSICAL);

    int i;
    for (i = 0; i < 0x8000 - 20; ++i) {
        if (bios[i] == 'D' &&
                bios[i+1] == 'I' &&
                bios[i+2] == 'M' &&
                bios[i+3] == 'P' &&
                sum_bytes(bios + i, 20) == 0) {
            printk("Found VBE PM Interface at %#x!\n",
                     (uint32_t)(uintptr_t)p + i);
            break;
        }
    }

    if (i == 0x8000 - 20)
        printk("No VBE PM Interface at %p\n", p);

    munmap(bios, 0x8000);
    return 0;
}
#endif

#if ENABLE_FRAMEBUFFER_THREAD > 0
static int draw_test(void *p)
{
    (void)p;

    int frames = 0;

    // 1280x800
    std::unique_ptr<png_image_t> img(png_load("background.png"));
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
            //printk("Line draw %" PRId64 " cycles\n", line_en - line_st);

            fb_update();
            ++frames;
        }
    }
    uint64_t en_tm = time_ns();
    printk("Benchmark time: %d frames, %" PRId64 "ms\n", frames,
             (en_tm - st_tm) / 1000000);

    return 0;
}
#endif

#if ENABLE_UNWIND
__exception_jmp_buf_t test_unwind_jmpbuf;

class test_unwind_cls {
public:
    ~test_unwind_cls()
    {
        printdbg("~test_unwind_cls called\n");
    }
};

void test_unwind_nest(int level)
{
    test_unwind_cls inst;

    if (level < 3)
        test_unwind_nest(level + 1);
    else
        __exception_longjmp_unwind(&test_unwind_jmpbuf, 1);
}

void test_unwind()
{
    if (!__exception_setjmp(&test_unwind_jmpbuf)) {
        test_unwind_nest(0);
    } else {
        printdbg("Unwind ended\n");
    }
}

void test_catch_nest(int level)
{
    test_unwind_cls inst;

    if (level < 3)
        test_catch_nest(level + 1);
    else {
//        cpu_crash();
        char * volatile crashme = nullptr;
        *crashme = 42;
    }
}

void test_catch()
{
    __try {
        test_catch_nest(0);
    } __catch {
        printk("Caught\n");
    }
}
#endif

#if ENABLE_FILESYSTEM_WR_TEST
_noreturn
int test_filesystem_write_thread(void *p)
{
    int x = int(intptr_t(p));

    while (true) {
        printk("Starting filesystem test\n");
        for (int n = 0; n < 1000; ++n) {
            char name[16];
            snprintf(name, sizeof(name), "test_%d_%d", x, n);
            printk("creating %s\n", name);
            int create_test = file_open(
                        name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
            if (create_test == -int(errno_t::EROFS))
                continue;
            assert(create_test >= 0);
            file_write(create_test, "Hello!", 6);
            file_close(create_test);
            printk(" created %s\n\n", name);
        }
        for (int n = 0; n < 1000; ++n) {
            char name[16];
            snprintf(name, sizeof(name), "created_%d_%d", x, n);
            printk("deleting %s\n", name);
            int unlink_test = file_unlink(name);
            if (unlink_test == -int(errno_t::EROFS))
                printk("Delete %s failed with %d\n", name, unlink_test);
        }
    }
}

int test_filesystem_write()
{
    for (int n = 0; n < ENABLE_FILESYSTEM_WR_TEST; ++n)
        thread_create(test_filesystem_write_thread, (void*)intptr_t(n), 0, false);
    return 1;
}
#endif

void test_spawn()
{
#if ENABLE_SPAWN_STRESS
    printk("Starting spawn stress with %d threads\n", ENABLE_SPAWN_STRESS);
    for (size_t i = 0; i < ENABLE_SPAWN_STRESS; ++i) {
        pid_t pid = 0;
        int spawn_result = process_t::spawn(
                    &pid, "user-shell", nullptr, nullptr);
        //thread_sleep_for(10000);
        printk("Started user mode process with PID=%d, status=%d\n",
                 pid, spawn_result);
        assert(spawn_result == 0 || pid < 0);
    }
#endif

#if ENABLE_SHELL
    int shell_pid;
    int spawn_result = process_t::spawn(
                &shell_pid, "shell", nullptr, nullptr);
    printk("shell pid: %d\n", spawn_result);
#endif
}

static int init_thread(void *)
{
    if (bootinfo_parameter(bootparam_t::boot_debugger))
        cpu_breakpoint();

    perf_init();

#if ENABLE_UNWIND
    printk("Testing exception unwind\n");
    test_unwind();
    test_catch();
#endif

    printk("Initializing late MSRs\n");
    cpu_init_late_msrs();

    printk("Initializing PCI\n");
    pci_init();

    printk("Initializing keyboard event queue\n");
    keybd_init();

    modload_init();

    printk("Spawning init\n");

    pid_t init_pid = -1;
    if (unlikely(process_t::spawn(&init_pid, "init", {}, {}) != 0))
        panic("spawn init failed!");

    if (unlikely(process_t::wait_for_exit(init_pid) < 0))
        panic("failed to wait for init");

    printk("Initializing late devices\n");
    callout_call(callout_type_t::late_dev);


#if ENABLE_FILESYSTEM_WR_TEST
    test_filesystem_write();
#endif

#if ENABLE_FRAMEBUFFER_THREAD > 0
    printk("Starting framebuffer stress\n");
    printk("Running framebuffer stress\n");
    thread_t draw_thread_id = thread_create(draw_test, 0, 0, 0);
    printk("draw thread id=%d\n", draw_thread_id);
#endif

#if ENABLE_FIND_VBE
    thread_create(find_vbe, (void*)0xC0000, 0, false);
    thread_create(find_vbe, (void*)0xF0000, 0, false);
#endif

#if ENABLE_CTXSW_STRESS_THREAD > 0
    printk("Running context switch stress with %d threads\n",
             ENABLE_CTXSW_STRESS_THREAD);
    for (int i = 0; i < ENABLE_CTXSW_STRESS_THREAD; ++i) {
        thread_close(thread_create(ctx_sw_thread, nullptr,
                                   "ctxsw_stress", 0, false, false));
    }
#endif

#if ENABLE_SHELL_THREAD > 0
    printk("Running shell thread\n");
    thread_create(shell_thread, (void*)0xfeedbeeffacef00d,
                  "shell_thread", 0, false, false);
#endif

#if ENABLE_SLEEP_THREAD
    test_sleep();
#endif

#if ENABLE_READ_STRESS_THREAD > 0
    test_read_stress();
#endif

#if ENABLE_REGISTER_THREAD > 0
    printk("Running register stress with %d threads\n",
             ENABLE_READ_STRESS_THREAD);
    for (int i = 0; i < ENABLE_REGISTER_THREAD; ++i) {
        thread_create(register_check, (void*)
                      (0xDEADFEEDF00DD00D +
                       (1<<ENABLE_READ_STRESS_THREAD)),
                      "register_stress",
                      0, false, false);
    }
#endif

#if ENABLE_MUTEX_THREAD > 0
    printk("Running mutex stress with %d threads\n", ENABLE_MUTEX_THREAD);
    mutex_init(&stress_lock);
    for (int i = 0; i < ENABLE_MUTEX_THREAD; ++i) {
        thread_create(stress_mutex, nullptr,
                      "mutex_stress", 0, false, false);
    }
#endif

#if ENABLE_MMAP_STRESS_THREAD > 0
    printk("Running mmap stress with %d threads\n",
             ENABLE_MMAP_STRESS_THREAD);
    for (int i = 0; i < ENABLE_MMAP_STRESS_THREAD; ++i) {
        thread_create(stress_mmap_thread, (void*)uintptr_t(i),
                      "mmap_stress",
                      0, false, false);
    }
#endif

#if ENABLE_HEAP_STRESS_THREAD > 0
    printk("Running heap stress with %d threads\n",
             ENABLE_HEAP_STRESS_THREAD);
    for (int i = 0; i < ENABLE_HEAP_STRESS_THREAD; ++i) {
        thread_create(stress_heap_thread, nullptr,
                      "heap_stress", 0, false, false);
    }
#endif

    callout_call(callout_type_t::init_thread_done, false);

    printk("init_thread completed\n");

    return 0;
}

int debugger_thread(void *)
{
    printk("Starting GDB stub\n");
    gdb_init();
    thread_create(init_thread, nullptr, "init_thread", 0, false, true,
                  thread_cpu_mask_t(0));

    return 0;
}

class something {
public:
    int big = 0xbbb12166;
    long long enough = 0xeee000020001111;

    ~something()
    {
        printdbg("something destructed\n");
    }
};

class locked {
public:
    virtual void do_thing()
    {
        scoped_lock lock(some_lock);

        do_throw();

        x = 1;
    }

    virtual void do_throw()
    {
        if (x == 0)
            throw something();
    }

private:
    int x = 0;

    using lock_type = std::mutex;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type some_lock;
};


class test_object {
public:
    char const *name;

    test_object(char const *name)
    {
        this->name = name;
        printdbg("test_object constructed: %s\n", name);
    }

    ~test_object()
    {
        printdbg("test_object destructed: %s\n", name);
    }
};

class test_exception : public std::exception {
public:
    test_exception(int n) : std::exception() {}
};

_noreturn
void test4()
{
    test_object a("test3");
    printdbg("Throwing\n");
    throw test_exception(42);
}

_noreturn
void test3()
{
    test4();
}

_noreturn
void test2()
{
    test_object a("test2");
    test3();
}

_noreturn
void test1()
{
    test_object a("test1");
    test2();
}

void test_cxx_except()
{
    try {
        test1();
    } catch (test_exception const& ex) {
        printdbg("Caught\n");
    }
}

struct symbols_t {
    struct linenum_entry_t {
        uint32_t filename_index;
        uint32_t line_nr;
    };

    using linemap_t = std::map<uintptr_t, linenum_entry_t>;
    using funcmap_t = std::map<uintptr_t, size_t>;

    linemap_t line_lookup;
    funcmap_t func_lookup;
    std::vector<char> tokens;

    char const *token(size_t n) const noexcept
    {
        return tokens.data() + n;
    }

    using name_lookup_t = std::map<std::string, size_t>;

    static void symbol_xlat(void *arg, void * const *ips, size_t count)
    {
        reinterpret_cast<symbols_t*>(arg)->symbol_xlat(ips, count);
    }

    void symbol_xlat(void * const *ips, size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            uintptr_t ip = uintptr_t(ips[i]);

            funcmap_t::iterator fit = func_lookup.lower_bound(ip);
            if (likely(fit != func_lookup.begin() &&
                       fit != func_lookup.end() &&
                       fit->first != ip))
                --fit;

            linemap_t::iterator lit = line_lookup.lower_bound(ip);
            if (likely(lit != line_lookup.begin() &&
                       lit != line_lookup.end() &&
                       lit->first != ip))
                --lit;

            size_t ftok = fit->second;
            size_t fofs = ip - fit->first;

            linenum_entry_t line = lit->second;

            char const *fname = token(ftok);
            char const *file = token(line.filename_index);

            printdbg("%#zx %s%+zd (%s:%u)\n", ip, fname,
                     fofs, file, line.line_nr);
        }
    }

    void load()
    {
        // Load symbols from the initrd
        file_t symfd;

        symfd = file_openat(AT_FDCWD, "sym/kernel-generic-klinesyms",
                            O_EXCL | O_RDONLY);
        if (unlikely(!symfd))
            return;

        off_t len = file_seek(symfd, 0, SEEK_END);
        if (unlikely(len < 0))
            return;

        std::unique_ptr<char[]> buf = new (std::nothrow) char[len];
        if (unlikely(!buf))
            return;

        ssize_t sz = file_pread(symfd, buf, len, 0);
        if (unlikely(sz != len))
            return;

        if (unlikely(symfd.close()))
            return;

        // Destroyed after loading is complete
        name_lookup_t name_lookup;

        // Make 0 token an empty string
        tokens.push_back(0);

        // Make empty string map to zero token
        name_lookup.insert({"", 0});

        std::string name;

        char *src = buf;
        for (char *line_end, *end = buf + len; src < end; src = line_end + 1) {
            line_end = static_cast<char*>(memchr(src, '\n', end - src));

            if (unlikely(!line_end))
                line_end = end;

            // Get filename
            char *filename = src;

            while (src < line_end && *src != ' ')
                ++src;

            char *filename_end = src;

            while (src < line_end && (*src == ' ' || *src == '\t'))
                ++src;

            // Get line number
            uint32_t line_nr_value = parse_number(src, end);

            while (src < line_end && (*src == ' ' || *src == '\t'))
                ++src;

            // Get address
            uintptr_t addr_value = parse_address(src, end);

            if (unlikely(!name.assign_noexcept(filename, filename_end)))
                return;

            // Try to insert in name lookup, or find existing one
            name_lookup_t::iterator ins = name_insert(name_lookup, name);

            linenum_entry_t entry;
            entry.filename_index = ins->second;
            entry.line_nr = line_nr_value;

            // Insert the file/line lookup entry by address
            line_lookup.insert({ addr_value, entry });
        }

        symfd = file_openat(AT_FDCWD, "sym/kernel-generic-kallsyms",
                            O_EXCL | O_RDONLY);

        if (unlikely(!symfd))
            return;

        len = file_seek(symfd, 0, SEEK_END);

        if (unlikely(len < 0))
            return;

        buf.reset(new (std::nothrow) char[len]);

        if (unlikely(!buf))
            return;

        sz = file_pread(symfd, buf, len, 0);

        if (unlikely(sz != len))
            return;

        src = buf;
        for (char *line_end, *end = buf + len; src < end; src = line_end + 1) {
            line_end = static_cast<char*>(memchr(src, '\n', end - src));

            if (unlikely(!line_end))
                line_end = end;

            uintptr_t addr_value = parse_address(src, end);

            while (src < end && (*src == ' ' || *src == '\t'))
                ++src;

            while (src < end && (*src != ' ' && *src != '\t'))
                ++src;

            while (src < end && (*src == ' ' || *src == '\t'))
                ++src;

            char *symbol_name = src;

            if (!name.assign_noexcept(symbol_name, line_end))
                return;

            name_lookup_t::iterator ins = name_insert(name_lookup, name);

            func_lookup.insert({ addr_value, ins->second });
        }

        perf_set_stacktrace_xlat_fn(symbol_xlat, this);
    }

    name_lookup_t::iterator name_insert(
            name_lookup_t &name_lookup,
            std::string const& entry)
    {
        std::pair<name_lookup_t::iterator, bool> ins =
                name_lookup.insert({ entry, tokens.size() });

        if (ins.second) {
            // It got inserted, append the string to the token vector
            for (char c : ins.first->first) {
                assert(c != '\n');
                tokens.push_back(c);
            }
            tokens.push_back(0);
        }

        return ins.first;
    }

    static uintptr_t parse_address(char *&src, char *end)
    {
        uintptr_t addr_value;
        for (addr_value = 0; src < end; ++src) {
            if (*src >= '0' && *src <= '9')
                addr_value = (addr_value << 4) | (*src - '0');
            else if (*src >= 'a' && *src <= 'f')
                addr_value = (addr_value << 4) | (*src + 0xA - 'a');
            else if (*src >= 'A' && *src <= 'F')
                addr_value = (addr_value << 4) | (*src + 0xA - 'A');
            else if (*src == 'x')
                addr_value = 0;
            else
                break;
        }
        return addr_value;
    }

    static uint32_t parse_number(char *&src, char *end)
    {
        uint32_t value;
        for (value = 0; src < end; ++src) {
            if (*src >= '0' && *src <= '9')
                value = value * 10 + (*src - '0');
            else
                break;
        }
        return value;
    }
};

static symbols_t symbols;

extern "C" _noreturn int kernel_main(void)
{
#ifdef _ASAN_ENABLED
    __builtin___asan_storeN_noabort((void*)kernel_params->phys_mapping,
                                    kernel_params->phys_mapping_sz);
#endif

    //test_cxx_except();

    tmpfs_startup((void*)kernel_params->initrd_st, kernel_params->initrd_sz);

    callout_call(callout_type_t::tmpfs_up);

    symbols.load();

    void *warmup[16];
    perf_stacktrace_xlat(warmup, stacktrace(warmup, 16));

    // Become the idle thread for the boot processor

    if (likely(!kernel_params->wait_gdb))
        thread_create(init_thread, nullptr,
                      "init_thread", 0, false, false);
    else
        thread_create(debugger_thread, nullptr,
                      "debugger_thread", 0, false, false,
                      thread_cpu_mask_t(thread_cpu_count() - 1));

    thread_idle_set_ready();

    cpu_irq_enable();

    thread_yield();

    thread_idle();
}

extern "C" _noreturn void mp_main()
{
    cpu_init_ap();
}

extern char ___init_brk[];

EXPORT size_t kernel_get_size()
{
    return ___init_brk - __image_start;
}
