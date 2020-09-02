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

// Represents an array of strings, indexed by token
using perf_token_table_t = ext::vector<ext::string>;

// Key is address, value is symbol token
using perf_symbol_table_t = ext::fast_map<uintptr_t, uintptr_t>;

// Key is address, value is index into line vector
using perf_line_table_t = ext::fast_map<uintptr_t, uintptr_t>;

struct perf_line_detail_t {
    uintptr_t addr;
    uint32_t filename_token;
    uint32_t line_nr;
};

using perf_line_vector_t = ext::vector<perf_line_detail_t>;

struct perf_module_t {
    size_t size = 0;
    // The keys in the symbol and line table are relative to min_addr
    uintptr_t min_addr = 0;
    perf_symbol_table_t syms;
    perf_line_table_t lines;
    size_t name_token;
};

using perf_module_lookup_t = ext::fast_map<uintptr_t, perf_module_t*>;

// key is token
using perf_module_by_name_table_t = ext::fast_map<size_t, perf_module_t>;

static perf_token_table_t perf_tokens;
static perf_module_by_name_table_t perf_modules_by_name;

// Keyed by module min_addr
static perf_module_lookup_t perf_module_lookup;

static perf_line_table_t perf_line_lookup;
static perf_line_vector_t perf_line_detail;
static perf_module_t *first_module;


// When bit 0 is set, update is in progress
// Incremented before and after updates

// One dedicated for each CPU
// Collect ring_cnt raw ips into a burst
struct perf_trace_cpu_t {
    static constexpr const size_t ring_cnt = 1 << 8;
    size_t level = 0;
    uint64_t last_aperf = 0;
    uint64_t last_tsc = 0;
    uintptr_t *ips = nullptr;
    uint64_t *samples = nullptr;
};

static uintptr_t *perf_trace_buf;
static uint64_t *perf_sample_buf;
static bool perf_zeroing = false;

struct perf_work_value_t {
    size_t line_token;
    uint64_t value;
};

static perf_work_value_t *perf_work_buf;

static ext::vector<perf_trace_cpu_t> perf_data;

static ext::vector<padded_rand_lfs113_t> perf_rand;

static uint32_t volatile perf_event = 0x76;
static uint32_t volatile perf_unit_mask = 0x00;
static uint32_t volatile perf_count_mask = 0;
static uint64_t volatile perf_event_divisor = 1048576;
static bool perf_event_invert = false;
static bool perf_event_edge = false;

using token_map_t = ext::map<ext::string, size_t>;
static token_map_t token_map;

// can 'force' it to be a new token, even if it exists
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
    if (unlikely(!perf_tokens.push_back(ext::move(text))))
        panic_oom();

    if (!force) {
        if (unlikely(token_map.emplace(
                         perf_tokens.back(), token).first
                     == token_map_t::iterator()))
            panic_oom();
    }

    return token;
}

static bool perf_load_file(ext::unique_ptr<char[]> &buf, off_t &sz,
                           char const *filename)
{
    file_t fid;

    fid = file_openat(AT_FDCWD, filename, O_RDONLY);
    if (unlikely(fid < 0))
        return false;

    sz = file_seek(fid, 0, SEEK_END);
    if (unlikely(sz < 0))
        return false;

    buf.reset(new (ext::nothrow) char[sz]);
    if (unlikely(!buf))
        return false;

    off_t zero_pos = file_seek(fid, 0, SEEK_SET);
    if (unlikely(zero_pos < 0))
        return false;

    ssize_t did_read = file_read(fid, buf.get(), sz);
    if (unlikely(did_read < 0))
        return false;

    int close_result = fid.close();
    if (unlikely(close_result < 0))
        return false;

    return true;
}

static ext::string perf_basename(char const *filename)
{
    char const *filename_end = filename + strlen(filename);
    char const *slash = (char const *)memrchr(
                filename, '/', filename_end - filename);
    slash = slash ? slash + 1 : filename;

    return ext::string(slash, filename_end);
}

static void perf_remove_suffix(ext::string &name, char const *suffix)
{
    size_t suffix_len = strlen(suffix);

    if (likely(suffix_len < name.length() &&
               !memcmp(name.data() + name.length() - suffix_len,
                       suffix, suffix_len)))
        name.resize(name.length() - suffix_len);
}

static perf_module_t &perf_module_by_name(ext::string const& module_name)
{
    size_t module_name_token = perf_tokenize(module_name.data(),
                                             module_name.data() +
                                             module_name.size(),
                                             false);

    perf_module_t &mod = perf_modules_by_name[module_name_token];

    return mod;
}

static perf_module_t &perf_module_by_symbol_filename(
        char const *filename, char const *suffix)
{
    ext::string symbol_filename = perf_basename(filename);

    perf_remove_suffix(symbol_filename, suffix);

    perf_module_t &mod = perf_module_by_name(symbol_filename);

    return mod;
}

static bool perf_load_symbols(char const *filename,
                              ptrdiff_t size,
                              uintptr_t min_addr,
                              uintptr_t base_adj = 0,
                              uintptr_t natural_load_addr = 0,
                              bool has_type_field = true)
{
    ext::unique_ptr<char[]> buf;
    off_t sz;

    if (unlikely(!perf_load_file(buf, sz, filename)))
        return false;

    perf_module_t &mod = perf_module_by_symbol_filename(filename, "-kallsyms");

    // If it was new
    if (first_module && !mod.size) {
        // Share allocators with first module
        mod.lines.share_allocator(first_module->lines);
        mod.syms.share_allocator(first_module->syms);
    } else {
        first_module = &mod;
    }

    mod.size = size;
    mod.min_addr = natural_load_addr + base_adj;

    // Add entry to module base lookup
    perf_module_lookup[mod.min_addr] = &mod;

    // Parse the lines
    char *end = buf.get() + sz;
    char *next;

    for (char *line = buf; line < end; line = next) {
        char *eol = (char*)memchr(line, '\n', end - line);
        eol = eol ? eol : end;
        next = eol + 1;

        char *line_iter = line;

        unsigned long addr = strtoul(line_iter, &line_iter, 16);

        // Fail if strtoul parse failed
        if (unlikely(!line_iter))
            return false;

        bool is_absolute = false;

        if (has_type_field) {
            while (*line_iter == ' ')
                ++line_iter;

            for ( ; line_iter < eol; ++line_iter) {
                if (*line_iter == 'a' || *line_iter == 'A') {
                    is_absolute = true;
                    break;
                } else if (*line_iter != ' ') {
                    break;
                }
            }

            ++line_iter;
        }

        // Discard absolute symbols
        if (unlikely(is_absolute))
            continue;

        // A symbol right at the beginning can't be a function
        if (unlikely(!addr || addr < natural_load_addr))
            continue;

        addr -= natural_load_addr;
        //addr += base;

        while (likely(*line_iter == ' '))
            ++line_iter;

        char const *name_end = (char const *)memchr(
                    line_iter, '[', eol - line_iter);
        name_end = name_end ? line_iter - 1 : eol;

        size_t symbol_token = perf_tokenize(line_iter, eol, false);
        mod.syms[addr] = symbol_token;
    }

//    printdbg("%s symbols:\n", module_name.c_str());
//    for (perf_symbol_table_t::value_type const& entry: mod.syms)
//        printdbg("0x%016zx %s\n", entry.first, entry.second.c_str());

    return true;
}

static bool perf_load_line_symbols(char const *filename,
                                   ptrdiff_t size,
                                   uintptr_t min_addr,
                                   uintptr_t base_adj,
                                   intptr_t natural_min_addr = 0)
{
    ext::unique_ptr<char[]> buf;
    off_t sz;

    if (unlikely(!perf_load_file(buf, sz, filename)))
        return false;

    perf_module_t &mod = perf_module_by_symbol_filename(
                filename, "-klinesyms");

    char *line = buf;
    char *end = buf + sz;

    // Parse a sequence of lines with this format:
    // filename linenum address...whitespace...possible garbage...newline

    char *line_file_st;
    char *line_file_en;

    size_t previous_line_sym_count = perf_line_detail.size();

    while (line < end) {
        char *name_st = line;

        while (line < end && (unsigned)*line > ' ')
            ++line;

        if (line > name_st && line[-1] == ':') {
            line_file_st = name_st;
            line_file_en = line - 1;

            while (line < end && *line <= ' ')
                ++line;

            continue;
        }

        char *name_en = line;

        while (line < end && *line <= ' ')
            line++;

        size_t line_nr = 0;
        while (line < end && *line >= '0' && *line <= '9') {
            line_nr *= 10;
            line_nr += *line++ - '0';
        }

        while (line < end && *line <= ' ')
            ++line;

        uintptr_t address = 0;
        while (line < end &&
               ((*line >= '0' && *line <= '9') ||
                (*line >= 'a' && *line <= 'f') ||
                (*line >= 'A' && *line <= 'F') ||
                (address == 0 && *line == 'x'))) {
            char c = *line++;

            size_t addend;

            if (c >= '0' && c <= '9')
                addend = c - '0';
            else if (c >= 'a' && c <= 'f')
                addend = 10 + c - 'a';
            else if (c >= 'A' && c <= 'F')
                addend = 10 + c - 'A';
            else
                addend = 0;

            address <<= 4;
            address |= addend;
        }

        while (line < end && *line != '\n')
            ++line;

        while (line < end && *line == '\n')
            ++line;

        if (likely(name_st < name_en && name_st[0] == '*')) {
            name_st = line_file_st;
            name_en = line_file_en;
        }

        // Drop weird addresses


        if (address < min_addr || address >= min_addr + size) {
//            printdbg("Dropped out of range symbol: %" PRIx64
//                     " not in range %#" PRIx64
//                     " - "
//                     "%#" PRIx64 "\n",
//                     address, min_addr, min_addr + size - 1);
            continue;
        }

        address -= natural_min_addr;

        size_t filename_token = perf_tokenize(name_st, name_en, false);

        uintptr_t absolute_addr = address + natural_min_addr + base_adj;

        perf_line_detail_t line_detail = {
            absolute_addr,
            uint32_t(filename_token),
            uint32_t(line_nr)
        };

        ext::pair<perf_line_table_t::iterator, bool> ins =
                mod.lines.emplace(address, perf_line_detail.size());

        if (unlikely(ins.first == perf_line_table_t::iterator()))
            panic_oom();

        // Exact duplicates are fine and expected
        if (!ins.second) {
            perf_line_detail_t &existing = perf_line_detail[ins.first->second];

            if (existing.filename_token == line_detail.filename_token &&
                    existing.line_nr == line_detail.line_nr) {
                continue;
            }
        }

        // Duplicate address on different line? Really?
        if (unlikely(!ins.second))
            continue;

        assert(filename_token <= UINT32_MAX);
        assert(line_nr <= UINT32_MAX);

        if (unlikely(!perf_line_detail.push_back(line_detail)))
            panic_oom();
    }

    size_t added_line_sym_count = perf_line_detail.size() -
            previous_line_sym_count;

    printdbg("Added %zu line symbols\n", added_line_sym_count);

    return true;
}


static void setup_sample(size_t cpu_nr)
{
    if (unlikely(perf_rand.empty())) {
        printdbg("No performance counters, can't select event\n");
        return;
    }

    // higher scale = higher rate
    // lower shift = higher rate

    //int32_t random_offset = (perf_rand[cpu_nr].lfsr113_rand() & 0x3F) - 0x20;

    int64_t count = - perf_event_divisor;// - random_offset;

    count = count <= -1 ? count : -1;

    uint32_t evt = perf_event;

    uint64_t event_sel = CPU_MSR_PERFEVTSEL_EN |
            CPU_MSR_PERFEVTSEL_IRQ |
            CPU_MSR_PERFEVTSEL_USR |
            CPU_MSR_PERFEVTSEL_OS |
            CPU_MSR_PERFEVTSEL_INV_n(perf_event_invert) |
            CPU_MSR_PERFEVTSEL_EDGE_n(perf_event_edge) |
            CPU_MSR_PERFEVTSEL_UNIT_MASK_n(
                perf_unit_mask & CPU_MSR_PERFEVTSEL_UNIT_MASK_MASK) |
            CPU_MSR_PERFEVTSEL_CNT_MASK_n(
                perf_count_mask & CPU_MSR_PERFEVTSEL_CNT_MASK) |
            CPU_MSR_PERFEVTSEL_EVT_SEL_LO_8_n(evt & 0xFF) |
            CPU_MSR_PERFEVTSEL_EVT_SEL_HI_4_n(
                (evt >> 8) & CPU_MSR_PERFEVTSEL_EVT_SEL_HI_4_MASK);

    cpu_msr_set(CPU_MSR_PERFEVTSEL_BASE+0, 0);
    cpu_msr_set(CPU_MSR_PERFCTR_BASE, count);
    cpu_msr_set(CPU_MSR_PERFEVTSEL_BASE+0, event_sel);
}

static perf_module_lookup_t::iterator perf_module_from_addr(uintptr_t addr)
{
    perf_module_lookup_t::iterator
            it = perf_module_lookup.lower_bound(addr);

    if (likely(it == perf_module_lookup.end() ||
               it->first > addr))
        --it;

    return it;
}

static perf_line_table_t::iterator lookup_line(uintptr_t addr)
{
    perf_module_lookup_t::iterator it = perf_module_from_addr(addr);

    if (unlikely(it == perf_module_lookup.end()))
        return perf_symbol_table_t::iterator();

    perf_module_t &mod = *it->second;

    // If it lies outside the module, drop it
    if (unlikely(addr >= mod.min_addr + mod.size))
        return perf_symbol_table_t::iterator();

    // Find offset relative to module base
    uintptr_t ofs = addr - mod.min_addr;

    // Lookup symbol at that place
    perf_symbol_table_t::iterator
            line_it = mod.lines.lower_bound(ofs);

    // If ahead of it, step back one
    if (likely(line_it == mod.lines.end() ||
               line_it->first > ofs))
        --line_it;

    if (unlikely(line_it == mod.lines.end()))
        line_it = perf_symbol_table_t::iterator();

    return line_it;
}

static perf_symbol_table_t::iterator lookup_symbol(uintptr_t addr)
{
    perf_module_lookup_t::iterator it = perf_module_from_addr(addr);

    if (unlikely(it == perf_module_lookup.end()))
        return perf_symbol_table_t::iterator();

    perf_module_t &mod = *it->second;

    // If it lies outside the module, drop it
    if (unlikely(addr > mod.min_addr + mod.size))
        return perf_symbol_table_t::iterator();

    // Find offset relative to module base
    uintptr_t ofs = addr - mod.min_addr;

    // Lookup symbol at that place
    perf_symbol_table_t::iterator
            sym_it = mod.syms.lower_bound(ofs);

    // If ahead of it, step back one
    if (likely(sym_it == mod.syms.end() ||
               sym_it->first > ofs))
        --sym_it;

    if (sym_it == mod.syms.end())
        sym_it = perf_symbol_table_t::iterator();

    return sym_it;
}

// Called in NMI handler, careful!
_hot
static void perf_sample(int (*ip)(void*), size_t cpu_nr)
{
    uintptr_t addr = uintptr_t((void*)ip);

    // Lookup the batch ring for this CPU
    perf_trace_cpu_t &trace = perf_data[cpu_nr];

    // Compute location within the ring
    size_t i = trace.level;
    size_t curr = i;
    size_t next = (i + 1);
    trace.level = next;

    // Write the value to the ring
    trace.ips[curr] = addr;

    // If we filled up the ring
    if (next == perf_trace_cpu_t::ring_cnt) {
        trace.level = 0;

        // Process batch in reverse
        // Starting at most likely to be in cache
        while (next > 0) {
            // Fetch the trace IP
            addr = trace.ips[--next];

            // Lookup line number by address
            perf_line_table_t::iterator line_it = lookup_line(addr);

            // If we found the symbol
            if (likely(line_it != perf_line_table_t::iterator())) {
                // Increment the sample count for the symbol's token
                size_t line_token = line_it->second;

                // Don't need fancy atomic thing, this is per-cpu
                ++trace.samples[line_token];
            }
        }
    }
}

EXPORT uint64_t perf_gather_samples(
        void (*callback)(void *arg, int percent, int frac,
                         char const *filename, int line_nr,
                         char const *),
        void *arg)
{
    if (unlikely(perf_data.empty()))
        return 0;

    size_t cpu_count = thread_get_cpu_count();

    bool zero = perf_zeroing;

    size_t line_detail_size = perf_line_detail.size();

    // 128 bytes at a time
    uint64_t row_totals[16];

    uint64_t grand = 0;
    size_t line_detail_used = 0;
    for (size_t i = 0; i < line_detail_size; i += 16) {
        size_t remain = ext::min(line_detail_size - i, size_t(16));

        // Clear whole cache line of totals
        for (size_t c = 0; c < remain; ++c)
            row_totals[c] = 0;

        for (size_t k = 0; k < cpu_count; ++k) {
            auto samples = (uint64_t*)__builtin_assume_aligned(
                        &perf_data[k].samples[i], 2048);

            // Add (and conditionally zero) whole cache line of inputs
            for (size_t c = 0; c < remain; ++c)
                row_totals[c] += samples[c];

            if (zero) {
                for (size_t c = 0; c < remain; ++c)
                    samples[c] = 0;
            }
        }

        // Process whole cache line worth of values
        for (size_t c = 0; c < remain; ++c) {
            if (row_totals[c]) {
                grand += row_totals[c];
                perf_work_buf[line_detail_used].line_token = i + c;
                perf_work_buf[line_detail_used].value = row_totals[c];
                ++line_detail_used;
            }
        }
    }

    ext::sort(perf_work_buf, perf_work_buf + line_detail_used,
              [&](perf_work_value_t const& lhs, perf_work_value_t const& rhs) {
        return rhs.value < lhs.value;
    });

    for (size_t i = 0; i < 16 && i < line_detail_used; ++i) {
        perf_work_value_t &item = perf_work_buf[i];

//        if (item.value == 0)
//            break;

        uint64_t fixed = grand ? 100000000 * item.value / grand : 0;

        perf_line_detail_t &detail = perf_line_detail[item.line_token];
        perf_symbol_table_t::iterator sym_it = lookup_symbol(detail.addr);

        char const *filename = perf_tokens[detail.filename_token].c_str();
        char const *function = sym_it != perf_symbol_table_t::iterator()
                ? perf_tokens[sym_it->second].c_str()
                : "???";

        callback(arg, int(fixed / 1000000), int(fixed % 1000000),
                 filename, detail.line_nr, function);
    }

    return grand;
}

_hot
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
        perf_symbol_table_t::iterator sym_it =
                lookup_symbol(uintptr_t(ips[i]));

        if (unlikely(sym_it == perf_symbol_table_t::iterator())) {
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

static void perf_update_all_cpus()
{
    workq::enqueue_on_all_barrier([](size_t cpu_nr) {
        setup_sample(cpu_nr);
    });
}

EXPORT void perf_init()
{
    size_t kernel_sz;

    kernel_sz = kernel_get_size();

    uintptr_t kernel_min_addr = uintptr_t(__image_start);
    uintptr_t kernel_natural_addr = 0xffffffff80000000;
    uintptr_t kernel_adj = kernel_min_addr - kernel_natural_addr;

    perf_load_symbols("sym/kernel-generic-kallsyms",
                      kernel_sz,
                      kernel_min_addr,
                      kernel_adj,
                      kernel_natural_addr);

    perf_load_line_symbols("sym/kernel-generic-klinesyms",
                           kernel_sz,
                           kernel_min_addr,
                           kernel_adj,
                           kernel_natural_addr);

    uintptr_t init_sz = 4 << 20;
    uintptr_t init_min_addr = 0x400000;
    uintptr_t init_natural_addr = 0x400000;
    uintptr_t init_adj = 0;

    perf_load_symbols("sym/init-kallsyms",
                      init_sz,
                      init_min_addr,
                      init_adj,
                      init_natural_addr);

    perf_load_line_symbols("sym/init-klinesyms",
                           init_sz,
                           init_min_addr,
                           init_adj,
                           init_natural_addr);

    size_t mod_count = modload_get_count();

    for (size_t i = 0; i < mod_count; ++i) {
        module_t *m = modload_get_index(i);

        ext::string name = modload_get_name(m);
        uintptr_t min_addr = modload_get_vaddr_min(m);
        uintptr_t base_adj = modload_get_base_adj(m);
        size_t size = modload_get_size(m);

        printdbg("Loading %s symbols and line number information"
                 ", base=%#zx, size=%#zx\n",
                 name.c_str(), base_adj, size);

        ext::string symname;
        symname.append("sym/")
                .append(name)
                .append("-kallsyms");

        perf_load_symbols(symname.c_str(), min_addr, size, base_adj);

        symname.resize(symname.length() - 9);
        symname.append("-klinesyms");

        perf_load_line_symbols(symname.c_str(),
                               size,
                               min_addr,
                               base_adj);
    }

    printdbg("Loaded symbols for %zu modules"
             ", %zu line symbols\n", modload_get_count(),
             perf_line_detail.size());

    perf_set_stacktrace_xlat_fn(stacktrace_xlat, nullptr);

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
                perf_line_detail.size(),
                PROT_READ | PROT_WRITE, MAP_POPULATE);

    if (unlikely(perf_sample_buf == MAP_FAILED))
        panic_oom();

    if (!unlikely(perf_data.resize(cpu_count)))
        panic_oom();

    for (size_t i = 0; i < cpu_count; ++i) {
        perf_data[i].ips = perf_trace_buf + (perf_trace_cpu_t::ring_cnt * i);
        perf_data[i].samples = perf_sample_buf + (perf_line_detail.size() * i);
    }

    // Place to store results
    perf_work_buf = (perf_work_value_t *)
            (perf_sample_buf + (perf_line_detail.size() * cpu_count));

    if (!cpuid_has_perf_ctr()) {
        printdbg("No performance counters!\n");
        return;
    }

    apic_hook_perf_local_irq(perf_nmi_handler, "perf_nmi", true);

    perf_rand.resize(cpu_count);

    uint64_t setup_st = time_ns();

    perf_update_all_cpus();

    uint64_t setup_en = time_ns();

    setup_en -= setup_st;

    printdbg("Setup perf sampling on all cpus took %" PRIu64 "ns\n", setup_en);
}

EXPORT void perf_set_zeroing(bool zeroing_enabled)
{
    perf_zeroing = zeroing_enabled;
}

EXPORT bool perf_get_zeroing()
{
    return perf_zeroing;
}

EXPORT uint32_t perf_set_event(uint32_t event)
{
    perf_event = event;

    perf_update_all_cpus();

    return event;
}

EXPORT uint64_t perf_set_divisor(uint64_t event_divisor)
{
    perf_event_divisor = event_divisor;

    perf_update_all_cpus();

    return event_divisor;
}

EXPORT uint32_t perf_set_event_mask(uint32_t event_mask)
{
    perf_unit_mask = event_mask;

    perf_update_all_cpus();

    return event_mask;
}

EXPORT uint32_t perf_set_count_mask(uint32_t count_mask)
{
    perf_count_mask = count_mask;

    perf_update_all_cpus();

    return count_mask;
}

EXPORT uint32_t perf_get_event()
{
    return perf_event;
}

EXPORT uint32_t perf_get_unit_mask()
{
    return perf_unit_mask;
}

EXPORT uint32_t perf_get_count_mask()
{
    return perf_count_mask;
}

EXPORT void perf_set_stacktrace_xlat_fn(stacktrace_xlat_fn_t fn, void *arg)
{
    stacktrace_xlat_fn = fn;
    stacktrace_xlat_fn_arg = arg;
}

EXPORT void perf_stacktrace_xlat(void * const *ips, size_t count)
{
    if (stacktrace_xlat_fn)
        stacktrace_xlat_fn(stacktrace_xlat_fn_arg, ips, count);
}

EXPORT void perf_stacktrace_decoded()
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

EXPORT uint64_t perf_adj_divisor(int64_t adjustment)
{
    perf_event_divisor += adjustment;

    if (adjustment)
        return perf_set_divisor(perf_event_divisor);

    return perf_event_divisor;
}

EXPORT uint64_t perf_get_divisor()
{
    return perf_event_divisor;
}

EXPORT bool perf_set_invert(bool invert)
{
    perf_event_invert = invert;

    return invert;
}

EXPORT bool perf_get_invert()
{
    return perf_event_invert;
}

EXPORT uint64_t perf_get_all()
{
    return CPU_MSR_PERFEVTSEL_EN |
            CPU_MSR_PERFEVTSEL_OS |
            CPU_MSR_PERFEVTSEL_USR |
            //no effect CPU_MSR_PERFEVTSEL_HG_ONLY_GUEST |
            CPU_MSR_PERFEVTSEL_EDGE_n(perf_event_edge) |
            CPU_MSR_PERFEVTSEL_INV_n(perf_event_invert) |
            CPU_MSR_PERFEVTSEL_CNT_MASK_n(perf_count_mask) |
            CPU_MSR_PERFEVTSEL_UNIT_MASK_n(perf_unit_mask) |
            CPU_MSR_PERFEVTSEL_EVT_SEL_LO_8_n(perf_event) |
            CPU_MSR_PERFEVTSEL_EVT_SEL_HI_4_n(perf_event >> 8);
}

EXPORT uint64_t perf_set_all(uint64_t value)
{
    perf_event_invert = CPU_MSR_PERFEVTSEL_INV_GET(value);
    perf_unit_mask = CPU_MSR_PERFEVTSEL_UNIT_MASK_GET(value);
    perf_event = CPU_MSR_PERFEVTSEL_EVT_SEL_LO_8_GET(value) |
            (CPU_MSR_PERFEVTSEL_EVT_SEL_HI_4_GET(value) << 8);
    perf_count_mask = CPU_MSR_PERFEVTSEL_CNT_MASK_GET(value);

    perf_update_all_cpus();

    return perf_get_all();
}

EXPORT bool perf_set_edge(bool edge)
{
    perf_event_edge = edge;
    return edge;
}

EXPORT bool perf_get_edge()
{
    return perf_event_edge;
}
