#include "perf.h"
#include "callout.h"
#include "cpu/control_regs.h"
#include "perf_reg.bits.h"
#include "apic.h"
#include "work_queue.h"
#include "fileio.h"
#include "basic_set.h"
#include "elf64.h"
#include "main.h"
#include "isr.h"
#include "rand.h"
#include "control_regs_constants.h"
#include "stacktrace.h"

static stacktrace_xlat_fn_t stacktrace_xlat_fn;
static void *stacktrace_xlat_fn_arg;

using perf_token_table_t = std::vector<ext::string>;

using perf_symbol_table_t = ext::fast_map<uintptr_t, size_t>;

struct perf_module_t {
    size_t size = 0;
    uintptr_t base = 0;
    perf_symbol_table_t syms;
    size_t name_token;
};

using perf_module_lookup_t = ext::fast_map<uintptr_t, perf_module_t*>;

using perf_module_table_t = ext::fast_map<ext::string, perf_module_t>;

static perf_token_table_t perf_tokens;
static perf_module_table_t perf_module_syms;
static perf_module_lookup_t perf_module_lookup;

// When bit 0 is set, update is in progress
// Incremented before and after updates

struct perf_trace_cpu_t {
    static constexpr const size_t ring_cnt = 1 << 12;
    size_t level = 0;
    uint64_t last_aperf = 0;
    uint64_t last_tsc = 0;
    uintptr_t *ips = nullptr;
    uint64_t *samples = nullptr;
};

static uintptr_t *perf_trace_buf;
static uint64_t *perf_sample_buf;
static bool perf_zeroing;

struct perf_work_value_t {
    size_t name_token;
    uint64_t value;
};

static perf_work_value_t *perf_work_buf;

static std::vector<perf_trace_cpu_t> perf_data;

static std::vector<padded_rand_lfs113_t> perf_rand;

static uint32_t volatile perf_event = 0xC0;
static uint8_t volatile perf_event_scale = 21;

using token_map_t = std::map<ext::string, size_t>;
static token_map_t token_map;

static size_t perf_tokenize(char const *st, char const *en, bool force = true)
{
    if (st && !en)
        en = st + strlen(st);

    ext::string text(st, en);

    if (!force) {
        token_map_t::iterator existing_it = token_map.find(text);
        if (existing_it != token_map.end())
            return existing_it->second;
    }

    size_t token = perf_tokens.size();
    perf_tokens.push_back(std::move(text));

    if (!force)
        token_map.emplace(perf_tokens.back(), token);

    return token;
}

static bool perf_add_symbols(char const *filename,
                             size_t size, uintptr_t base = 0,
                             intptr_t natural_load_addr = 0)
{
    file_t syms_fid;

    syms_fid = file_openat(AT_FDCWD, filename, O_RDONLY);
    if (unlikely(syms_fid < 0))
        return false;

    off_t sz = file_seek(syms_fid, 0, SEEK_END);
    if (unlikely(sz < 0))
        return false;

    std::unique_ptr<char[]> buf(new (ext::nothrow) char[sz]);
    if (unlikely(!buf))
        return false;

    off_t zero_pos = file_seek(syms_fid, 0, SEEK_SET);
    if (unlikely(zero_pos < 0))
        return false;

    ssize_t did_read = file_read(syms_fid, buf.get(), sz);
    if (unlikely(did_read < 0))
        return false;

    int close_result = syms_fid.close();
    if (unlikely(close_result < 0))
        return false;

    // Parse the lines
    char *end = buf.get() + sz;
    char *next;

    char const *filename_end = filename + strlen(filename);
    char const *slash = (char const *)memrchr(
                filename, '/', filename_end - filename);
    slash = slash ? slash + 1 : filename;

    ext::string module_name(slash, filename_end);

    // Cut off -kallsyms suffix
    if (module_name.length() > 9 &&
            !memcmp(module_name.data() + module_name.length() - 9,
                    "-kallsyms", 9))
        module_name.resize(module_name.length() - 9);

    perf_module_t &mod = perf_module_syms[module_name];
    perf_module_lookup[base] = &mod;

    size_t module_name_token = perf_tokenize(module_name.data(),
                                             module_name.data() +
                                             module_name.size());

    mod.size = size;
    mod.base = base;
    mod.name_token = module_name_token;

    for (char *line = buf; line < end; line = next) {
        char *eol = (char*)memchr(line, '\n', end - line);
        eol = eol ? eol : end;
        next = eol + 1;

        char *line_iter = line;

        unsigned long addr = strtoul(line_iter, &line_iter, 16);
        if (unlikely(!line_iter))
            return false;
        bool is_absolute = false;
        for ( ; line_iter < eol; ++line_iter) {
            if (*line_iter == 'a' || *line_iter == 'A') {
                is_absolute = true;
                break;
            } else if (*line_iter != ' ') {
                break;
            }
        }

        // Discard absolute symbols
        if (is_absolute)
            continue;

        addr -= natural_load_addr;

        if (*line_iter)
            ++line_iter;

        while (*line_iter == ' ')
            ++line_iter;

        char const *name_end = (char const *)memchr(
                    line_iter, '[', eol - line_iter);
        name_end = name_end ? line_iter - 1 : eol;

        size_t symbol_token = perf_tokenize(line_iter, eol);
        mod.syms[addr] = symbol_token;
    }

//    printdbg("%s symbols:\n", module_name.c_str());
//    for (perf_symbol_table_t::value_type const& entry: mod.syms)
//        printdbg("0x%016zx %s\n", entry.first, entry.second.c_str());

    return true;
}

static int last_batch_time;
static int rate_adj;

void setup_sample(size_t cpu_nr)
{
    if (perf_rand.empty()) {
        printdbg("No performance counters, can't select event\n");
        return;

    }
    cpu_msr_set(CPU_MSR_PERFEVTSEL_BASE, 0);

    // Lower result = higher rate
    // higher shift = higher rage

    // Random value in (0, 1<<10] range (1024)
    //int8_t shift = 32 - (13 - (rate_adj >> 3));
    int8_t shift = 32 - perf_event_scale;

    int32_t random_interval = perf_rand[cpu_nr].lfsr113_rand() >> shift;

    cpu_msr_set(CPU_MSR_PERFCTR_BASE, -250 - random_interval);

    uint32_t evt = perf_event;
    cpu_msr_set(CPU_MSR_PERFEVTSEL_BASE,
                CPU_MSR_PERFEVTSEL_EN |
                CPU_MSR_PERFEVTSEL_IRQ |
                CPU_MSR_PERFEVTSEL_USR |
                CPU_MSR_PERFEVTSEL_OS |
                CPU_MSR_PERFEVTSEL_EVT_SEL_LO_8_n(evt & 0xFF) |
                CPU_MSR_PERFEVTSEL_EVT_SEL_HI_4_n(evt >> 8));

//    perf_data[cpu_nr].last_aperf = cpu_msr_get(CPU_MSR_APERF);
//    perf_data[cpu_nr].last_tsc = cpu_rdtsc();
}

perf_symbol_table_t::const_iterator lookup_symbol(uintptr_t addr)
{
    perf_module_lookup_t::const_iterator
            it = perf_module_lookup.lower_bound(addr);

    if (likely(it == perf_module_lookup.end() ||
               it->first != addr))
        --it;

    if (unlikely(it == perf_module_lookup.end()))
        return perf_symbol_table_t::const_iterator();

    perf_module_t &mod = *it->second;

    // If it lies outside the module, drop it
    if (unlikely(addr > mod.base + mod.size))
        return perf_symbol_table_t::const_iterator();

    // Find offset relative to module base
    uintptr_t ofs = addr - mod.base;

    // Lookup symbol at that place
    perf_symbol_table_t::const_iterator
            sym_it = mod.syms.lower_bound(ofs);

    // If ahead of it, step back one
    if (likely(sym_it == mod.syms.end() ||
               sym_it->first > ofs))
        --sym_it;

    if (sym_it == mod.syms.end())
        sym_it = perf_symbol_table_t::const_iterator();

    return sym_it;
}

static void perf_sample(int (*ip)(void*), size_t cpu_nr)
{
    uintptr_t addr = uintptr_t((void*)ip);

    perf_trace_cpu_t &trace = perf_data[cpu_nr];
    size_t i = trace.level;
    size_t curr = i;
    size_t next = (i + 1);
    trace.level = next;
    trace.ips[curr] = addr;
    if (next == perf_trace_cpu_t::ring_cnt) {
        trace.level = 0;

        // Make adjustment to sample rate if necessary
        uint64_t now = time_ns();

        uint64_t elap_ns = now - last_batch_time;

        if (last_batch_time) {
            if (elap_ns > 1000000000)
                ++rate_adj;
            else if (elap_ns < 400000000)
                --rate_adj;
        }

        last_batch_time = now;

        // Process batch
        for (i = 0; i < perf_trace_cpu_t::ring_cnt; ++i) {
            addr = trace.ips[i];

            // Lookup module by base address
            perf_symbol_table_t::const_iterator sym_it = lookup_symbol(addr);

            if (likely(sym_it != perf_symbol_table_t::const_iterator())) {
                size_t symbol_token = sym_it->second;
                // Don't need fancy atomic thing, this is per-cpu
                ++trace.samples[symbol_token];
            } else {
                //printdbg("Lost in space perf sample at %" PRIx64 "\n", addr);
            }
        }
    }
}

EXPORT uint64_t perf_gather_samples(
        void (*callback)(void *, int, int, char const *), void *arg)
{
    if (unlikely(perf_data.empty()))
        return 0;

    size_t cpu_count = thread_get_cpu_count();

    bool zero = perf_zeroing;

    uint64_t grand = 0;
    for (size_t i = 0; i < perf_tokens.size(); ++i) {
        uint64_t total = 0;
        for (size_t k = 0; k < cpu_count; ++k) {
            //size_t usage = thread_cpu_usage_x1k(k);
            //total += usage * perf_data[k].samples[i] / 100000;
            total += perf_data[k].samples[i];

            if (zero)
                perf_data[k].samples[i] = 0;
        }
        perf_work_buf[i].name_token = i;
        perf_work_buf[i].value = total;
        grand += total;
    }

    std::sort(perf_work_buf, perf_work_buf + perf_tokens.size(),
              [&](perf_work_value_t const& lhs, perf_work_value_t const& rhs) {
        return rhs.value < lhs.value;
    });

    for (size_t i = 0; i < perf_tokens.size(); ++i) {
        perf_work_value_t &item = perf_work_buf[i];
        if (item.value == 0)
            break;
        uint64_t fixed = grand ? 100000 * item.value / grand : 0;
        callback(arg, int(fixed / 1000), int(fixed % 1000),
                 perf_tokens[item.name_token].c_str());
    }

    return grand;
}

static isr_context_t *perf_nmi_handler(int intr, isr_context_t *ctx)
{
    uint32_t cpu_nr = thread_cpu_number();
    perf_sample(ISR_CTX_REG_RIP(ctx), cpu_nr);
    setup_sample(cpu_nr);

    return ctx;
}

static void stacktrace_xlat(void *arg, void * const *ips, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        perf_symbol_table_t::const_iterator sym_it =
                lookup_symbol(uintptr_t(ips[i]));

        if (unlikely(sym_it == perf_symbol_table_t::const_iterator())) {
            printdbg("??? (%#" PRIx64 ")\n", uint64_t(ips[i]));
            continue;
        }

        size_t sym_token = sym_it->second;

        if (perf_tokens.size() > sym_token) {
            printdbg("%s (%#" PRIx64 ")\n",
                     perf_tokens[sym_token].c_str(), uint64_t(ips[i]));
        }
    }
}

EXPORT void perf_init()
{
    size_t kernel_sz;
    kernel_sz = kernel_get_size();

    perf_add_symbols("sym/kernel-generic-kallsyms",
                     kernel_sz, uintptr_t(__image_start),
                     0xffffffff80000000);

    size_t mod_count = modload_get_count();

    for (size_t i = 0; i < mod_count; ++i) {
        module_t *m = modload_get_index(i);

        ext::string name = modload_get_name(m);
        uintptr_t base = modload_get_base(m);
        size_t size = modload_get_size(m);

        ext::string symname;
        symname.append("sym/")
                .append(name)
                .append("-kallsyms");

        perf_add_symbols(symname.c_str(), size, base);
    }

    perf_set_stacktrace_xlat_fn(stacktrace_xlat, nullptr);

//    printdbg("Modules:\n");
//    for (perf_module_table_t::value_type const& item: perf_module_syms) {
//        printdbg("base=0x%016zx +0x%08zx %s\n",
//                 item.second.base, item.second.size, item.first.c_str());
//    }

    size_t cpu_count = thread_get_cpu_count();

    perf_trace_buf = (uintptr_t*)mmap(
                nullptr, sizeof(*perf_trace_buf) *
                cpu_count *
                perf_trace_cpu_t::ring_cnt,
                PROT_READ | PROT_WRITE, MAP_POPULATE);

    if (unlikely(perf_trace_buf == MAP_FAILED))
        panic_oom();

    perf_sample_buf = (uint64_t*)mmap(
                nullptr, sizeof(*perf_sample_buf) *
                (cpu_count + 2) *
                perf_tokens.size(),
                PROT_READ | PROT_WRITE, MAP_POPULATE);

    if (unlikely(perf_sample_buf == MAP_FAILED))
        panic_oom();

    if (!unlikely(perf_data.resize(cpu_count)))
        panic_oom();

    for (size_t i = 0; i < cpu_count; ++i) {
        perf_data[i].ips = perf_trace_buf + (perf_trace_cpu_t::ring_cnt * i);
        perf_data[i].samples = perf_sample_buf + (perf_tokens.size() * i);
    }

    // Place to store results
    perf_work_buf = (perf_work_value_t *)
            (perf_sample_buf + (perf_tokens.size() * cpu_count));

    if (!cpuid_has_perf_ctr()) {
        printdbg("No performance counters!\n");
        return;
    }

    apic_hook_perf_local_irq(perf_nmi_handler, "perf_nmi");

    perf_rand.resize(cpu_count);

    workq::enqueue_on_all_barrier([](size_t cpu_nr) {
        setup_sample(cpu_nr);
    });
}

EXPORT void perf_set_event(uint32_t event, uint8_t event_scale)
{
    perf_event = event;
    perf_event_scale = event_scale;

    workq::enqueue_on_all_barrier([](size_t cpu_nr) {
        setup_sample(cpu_nr);
    });
}

void perf_set_stacktrace_xlat_fn(stacktrace_xlat_fn_t fn, void *arg)
{
    stacktrace_xlat_fn = fn;
    stacktrace_xlat_fn_arg = arg;
}

void perf_stacktrace_xlat(void * const *ips, size_t count)
{
    if (stacktrace_xlat_fn)
        stacktrace_xlat_fn(stacktrace_xlat_fn_arg, ips, count);
}

void perf_stacktrace_decoded()
{
    printdbg("-------------------------------------------\n");

    void *stacktrace_addrs[32];
    size_t frame_cnt = stacktrace(stacktrace_addrs,
                                  countof(stacktrace_addrs));

    //    for (size_t i = 0; i < frame_cnt; ++i)
    //        printdbg("[%zu] rip=%#zx\n",
    //                 i, uintptr_t(stacktrace_addrs[i]));

    //    printdbg("- - - - - - - - - - - - - - - - - - - - - -\n");

    perf_stacktrace_xlat(stacktrace_addrs, frame_cnt);

    printdbg("-------------------------------------------\n");
}
