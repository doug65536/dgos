#pragma once

#include "errno.h"
#include "device/iocp.h"

class dev_char_t {
public:
    virtual ~dev_char_t() {}

    virtual errno_t read_async(void *data, int64_t count, iocp_t *iocp) = 0;
    virtual errno_t write_async(void *data, int64_t count, iocp_t *iocp) = 0;
    virtual errno_t flush_async(iocp_t *iocp) = 0;

    virtual int read(void *data, int64_t count);
    virtual int write(void *data, int64_t count);
    virtual int flush();
};
