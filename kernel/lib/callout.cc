#include "callout.h"
#include "thread.h"

extern callout_t const ___callout_array_st[];
extern callout_t const ___callout_array_en[];

struct thread_callout_worker_t {
    void (*fn)(void*);
    void *arg;
    thread_t tid;
};

static int thread_callout_worker(void *arg)
{
    auto work = (thread_callout_worker_t*)arg;
    work->fn(work->arg);
    return 0;
}

size_t callout_call(callout_type_t type, bool as_thread)
{
    thread_callout_worker_t workers[16];
    size_t worker_count = 0;

    auto wait_pending = [&] {
        for (size_t i = 0; i < worker_count; ++i) {
            thread_wait(workers[i].tid);
            thread_close(workers[i].tid);
        }
        worker_count = 0;
    };

    for (callout_t const *callout = ___callout_array_st;
         callout < ___callout_array_en; ++callout) {
        if (callout->type == type) {
            if (!as_thread) {
                callout->fn(callout->userarg);
            } else {
                auto& work = workers[worker_count++] = {
                    callout->fn,
                    callout->userarg,
                    0
                };
                work.tid = thread_create(thread_callout_worker,
                                         &work, "callout_worker",
                                         0, false, false);

                if (worker_count == countof(workers))
                    wait_pending();
            }
        }
    }

    wait_pending();

    return ___callout_array_en - ___callout_array_st;
}
