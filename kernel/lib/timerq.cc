#include "timerq.h"
#include "basic_set.h"
#include "thread.h"
#include "mutex.h"

__BEGIN_ANONYMOUS

class timer_queue_t {
public:
    using timer_id_t = uint64_t;
    using timestamp_t = int64_t;
    using duration_t = int64_t;

private:
    // Set keyed on timer timeout time, value points to timer instance
    using time_set_t = ext::fast_set<timestamp_t, timer_instance_t*>;

    // Set keyed on timer id, value is timer instance
    using timer_set_t = ext::fast_set<timer_id_t, timer_instance_t>;

    using lock_type = ext::spinlock;
    using scoped_lock = ext::unique_lock<lock_type>;
    using condition_t = ext::condition_variable;

    using handler_type_t = ext::function<void()>;

    class timer_instance_t {
    public:
        explicit Timer(timer_id_t id,
                       timestamp_t timestamp,
                       duration_t duration,
                       handler_type_t handler);

        ext::unique_ptr<condition_t> wait_cond;

        bool running;
    };

    lock_type queue_lock;
    timer_id_t last_id;
    time_set_t timers;
};

__END_ANONYMOUS

int timer_create(void (*handler)(void *), void *arg,
                 int64_t when, bool periodic)
{

}
