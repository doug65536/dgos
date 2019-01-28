#include "thread.h"
#include "memory.h"

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

    thread_t spawn_thread(thread_run_data_t *data) const;
    int invoke() const;
};

// Thread function
static int thread_run_start(void *p)
{
    std::unique_ptr<thread_run_data_t> data =
            reinterpret_cast<thread_run_data_t*>(p);

    data->invoke();

    return 0;
}

thread_t thread_proc_0(void (*fn)())
{
    auto data = new thread_run_data_t(fn);
    return data->spawn_thread(data);
}

thread_t thread_proc_1(void (*fn)(void *), void *arg)
{
    auto data = new thread_run_data_t(fn, arg);
    return data->spawn_thread(data);
}

thread_t thread_func_0(int (*fn)())
{
    auto data = new thread_run_data_t(fn);
    return data->spawn_thread(data);
}

thread_t thread_func_1(int (*fn)(void*), void *arg)
{
    auto data = new thread_run_data_t(fn, arg);
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
    std::unique_ptr<thread_run_data_t> data(p);
    thread_t tid = thread_create(thread_run_start, data, 0, false);
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
