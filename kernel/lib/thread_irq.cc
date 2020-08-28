#include "thread_irq.h"
#include "mutex.h"
#include "cxxstring.h"

class threaded_irq_worker_t {
public:
    threaded_irq_worker_t() = default;
    ~threaded_irq_worker_t() = default;
    threaded_irq_worker_t(threaded_irq_worker_t const&) = delete;
    threaded_irq_worker_t& operator=(threaded_irq_worker_t) = delete;

    isr_context_t *irq_handler(int irq, isr_context_t *ctx);

private:
    intr_handler_t handler;
    ext::spinlock worker_lock;
    ext::condition_variable work;
    ext::string name;
};

isr_context_t *threaded_irq_thread_fn(void *arg)
{

}

int threaded_irq_request(int irq, int cpu, intr_handler_t handler_fn,
                         char const *devname)
{

    return -int(errno_t::ENOSYS);
}
