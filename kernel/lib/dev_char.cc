#include "dev_char.h"

dev_char_t::~dev_char_t()
{
}

ssize_t dev_char_t::read(void *data, size_t count)
{
    blocking_iocp_t iocp;
    errno_t err = read_async(data, count, &iocp);
    if (unlikely(err != errno_t::OK))
        return -int(err);
    iocp.set_expect(1);
    return iocp.wait_and_return<int>();
}

ssize_t dev_char_t::write(void const *data, size_t count)
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
