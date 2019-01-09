#include "dev_char.h"

int dev_char_t::read(void *data, int64_t count)
{
    blocking_iocp_t iocp;
    errno_t err = read_async(data, count, &iocp);
    if (unlikely(err != errno_t::OK))
        return -int(err);
    iocp.set_expect(1);
    return iocp.wait_and_return<int>();
}

int dev_char_t::write(void *data, int64_t count)
{
    blocking_iocp_t iocp;
    errno_t err = write_async(data, count, &iocp);
    if (unlikely(err != errno_t::OK))
        return -int(err);
    iocp.set_expect(1);
    return iocp.wait_and_return<int>();
}

int dev_char_t::flush()
{
    blocking_iocp_t iocp;
    errno_t err = flush_async(&iocp);
    if (unlikely(err != errno_t::OK))
        return -int(err);
    iocp.set_expect(1);
    return iocp.wait_and_return<int>();
}
