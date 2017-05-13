#include "stacktrace.h"

#if !defined(__x86_64__) && !defined(__i386__)
#error This code only works on x86 and x86-64
#endif

struct stack_frame_t {
    stack_frame_t *parent;
    void *return_addr;
};

size_t stacktrace(void **addresses, size_t max_frames)
{
    auto frame = (stack_frame_t const *)__builtin_frame_address(0);

    size_t count;
    for (count = 0; count < max_frames && frame;
         ++count, frame = frame->parent)
        addresses[count] = frame->return_addr;

    return count;
}
