#include "thread.h"
#include "memory.h"
#include "algorithm.h"
#include "bitsearch.h"

struct thread_run_data_t
{
    union {
        // Procedure with 0 or 1 arg
        void (*p0)();
        void (*p1)(void*);
        // Function with 0 or 1 arg
        int (*f0)();
        int (*f1)(void*);
    } fn;
    void *arg;
    int arg_count;
    int ret_int;

    thread_run_data_t(void (*f)());
    thread_run_data_t(void (*f)(void*), void *a);
    thread_run_data_t(int (*f)());
    thread_run_data_t(int (*f)(void*), void *a);

    _use_result
    thread_t spawn_thread(thread_run_data_t *data) const;
    int invoke() const;
};

// Thread function
static int thread_run_start(void *p)
{
    ext::unique_ptr<thread_run_data_t> data =
            reinterpret_cast<thread_run_data_t*>(p);

    data->invoke();

    return 0;
}

thread_t thread_proc_0(void (*fn)())
{
    auto data = new (ext::nothrow) thread_run_data_t(fn);
    return data->spawn_thread(data);
}

thread_t thread_proc_1(void (*fn)(void *), void *arg)
{
    auto data = new (ext::nothrow) thread_run_data_t(fn, arg);
    return data->spawn_thread(data);
}

thread_t thread_func_0(int (*fn)())
{
    auto data = new (ext::nothrow) thread_run_data_t(fn);
    return data->spawn_thread(data);
}

thread_t thread_func_1(int (*fn)(void*), void *arg)
{
    auto data = new (ext::nothrow) thread_run_data_t(fn, arg);
    return data->spawn_thread(data);
}

thread_run_data_t::thread_run_data_t(void (*f)())
{
    fn.p0 = f;
    arg = nullptr;
    arg_count = 0;
    ret_int = 0;
}

thread_run_data_t::thread_run_data_t(void (*f)(void *), void *a)
{
    fn.p1 = f;
    arg = a;
    arg_count = 1;
    ret_int = 0;
}

thread_run_data_t::thread_run_data_t(int (*f)())
{
    fn.f0 = f;
    arg = nullptr;
    arg_count = 0;
    ret_int = 1;
}

thread_run_data_t::thread_run_data_t(int (*f)(void *), void *a)
{
    fn.f1 = f;
    arg = a;
    arg_count = 1;
    ret_int = 1;
}

thread_t thread_run_data_t::spawn_thread(thread_run_data_t *p) const
{
    ext::unique_ptr<thread_run_data_t> data(p);
    thread_t tid = thread_create(nullptr,
                                 thread_run_start, data,
                                 "spawn_thread", 0, false, false);
    if (tid >= 0)
        data.release();
    return tid;
}

int thread_run_data_t::invoke() const
{
    if (!ret_int) {
        if (arg_count == 0) {
            fn.p0();
            return 0;
        } else {
            fn.p1(arg);
            return 0;
        }
    } else {
        if (arg_count == 0) {
            return fn.f0();
        } else {
            return fn.f1(arg);
        }
    }
}



EXPORT thread_cpu_mask_t &thread_cpu_mask_t::operator+=(size_t bit)
{
    bitmap[(bit >> 6)] |= (UINT64_C(1) << (bit & 63));
    return *this;
}

EXPORT thread_cpu_mask_t &thread_cpu_mask_t::atom_set(size_t bit)
{
    atomic_or(&bitmap[(bit >> 6)], (UINT64_C(1) << (bit & 63)));
    return *this;
}

EXPORT thread_cpu_mask_t &thread_cpu_mask_t::operator-=(size_t bit)
{
    bitmap[(bit >> 6)] &= ~(UINT64_C(1) << (bit & 63));
    return *this;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator-(
        thread_cpu_mask_t const& rhs) const
{
    thread_cpu_mask_t result{*this};
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = bitmap[i] & ~rhs.bitmap[i];
    return result;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator+(
        thread_cpu_mask_t const& rhs) const
{
    thread_cpu_mask_t result{*this};
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = bitmap[i] | rhs.bitmap[i];
    return result;
}

EXPORT void thread_cpu_mask_t::atom_clr(size_t bit) volatile
{
    atomic_and(&bitmap[(bit >> 6)], ~(UINT64_C(1) << (bit & 63)));
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator&(
        thread_cpu_mask_t const& rhs) const
{
    thread_cpu_mask_t result;
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = bitmap[i] & rhs.bitmap[i];
    return result;
}

EXPORT thread_cpu_mask_t &thread_cpu_mask_t::operator&=(
        thread_cpu_mask_t const& rhs)
{
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        bitmap[i] &= rhs.bitmap[i];
    return *this;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::atom_and(
        thread_cpu_mask_t const& rhs) volatile
{
    thread_cpu_mask_t result;
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = atomic_and(&bitmap[i], rhs.bitmap[i]);
    return result;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator|(
        thread_cpu_mask_t const& rhs) const
{
    thread_cpu_mask_t result;
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = bitmap[i] | rhs.bitmap[i];
    return result;
}

EXPORT thread_cpu_mask_t &thread_cpu_mask_t::operator|=(
        thread_cpu_mask_t const& rhs)
{
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        bitmap[i] |= rhs.bitmap[i];
    return *this;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::atom_or(
        thread_cpu_mask_t const& rhs) volatile
{
    thread_cpu_mask_t result;
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = atomic_or(&bitmap[i], rhs.bitmap[i]);
    return result;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator^(
        thread_cpu_mask_t const& rhs) const
{
    thread_cpu_mask_t result;
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = bitmap[i] ^ rhs.bitmap[i];
    return result;
}

EXPORT thread_cpu_mask_t &thread_cpu_mask_t::operator^=(
        thread_cpu_mask_t const& rhs)
{
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        bitmap[i] ^= rhs.bitmap[i];
    return *this;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::atom_xor(
        thread_cpu_mask_t const& rhs) volatile
{
    thread_cpu_mask_t result;
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        result.bitmap[i] = atomic_xor(&bitmap[i], rhs.bitmap[i]);
    return result;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator~() const
{
    thread_cpu_mask_t comp{};
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        comp.bitmap[i] = ~bitmap[i];
    return comp;
}

EXPORT bool thread_cpu_mask_t::operator!() const
{
    uint64_t un = bitmap[0];
    for (size_t i = 1, e = countof(bitmap); i != e; ++i)
        un |= bitmap[i];
    return un == 0;
}

EXPORT bool thread_cpu_mask_t::operator[](size_t bit) const
{
    return bitmap[(bit >> 6)] & (UINT64_C(1) << (bit & 63));
}

EXPORT size_t thread_cpu_mask_t::lsb_set() const
{
    for (size_t i = 0; i < bitmap_entries; ++i) {
        if (bitmap[i]) {
            return bit_lsb_set(bitmap[i]) +
                    i * (sizeof(*bitmap) * CHAR_BIT);
        }
    }
    return ~size_t(0);
}

EXPORT thread_cpu_mask_t &thread_cpu_mask_t::set_all()
{
    for (size_t i = 0, e = countof(bitmap); i != e; ++i)
        bitmap[i] = ~(UINT64_C(0));
    return *this;
}

EXPORT bool thread_cpu_mask_t::operator==(thread_cpu_mask_t const& rhs) const
{
    bool is_equal = bitmap[0] == rhs.bitmap[0];
    for (size_t i = 1, e = countof(bitmap); i != e; ++i)
        is_equal = is_equal & (bitmap[i] == rhs.bitmap[i]);
    return is_equal;
}

EXPORT bool thread_cpu_mask_t::operator!=(thread_cpu_mask_t const& rhs) const
{
    bool not_equal = bitmap[0] != rhs.bitmap[0];
    for (size_t i = 1, e = countof(bitmap); i != e; ++i)
        not_equal = not_equal | (bitmap[i] != rhs.bitmap[i]);
    return not_equal;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator-(size_t bit)
{
    thread_cpu_mask_t result{*this};
    result -= bit;
    return result;
}

EXPORT thread_cpu_mask_t thread_cpu_mask_t::operator+(size_t bit)
{
    thread_cpu_mask_t result{*this};
    result += bit;
    return result;
}
