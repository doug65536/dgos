#pragma once

#include "errno.h"
#include "device/iocp.h"

class dev_char_t {
public:
    virtual ~dev_char_t() = 0;

    virtual errno_t read_async(void *data, size_t count,
                               iocp_t *iocp) = 0;
    virtual errno_t write_async(void const *data, size_t count,
                                iocp_t *iocp) = 0;
    virtual errno_t flush_async(iocp_t *iocp) = 0;

    virtual ssize_t read(void *data, size_t count);
    virtual ssize_t write(void const *data, size_t count);
    virtual int flush();
};

#define DEV_CHAR_IMPL \
    errno_t read_async(void *data, size_t count, \
        iocp_t *iocp) override = 0; \
    errno_t write_async(void const *data, \
        size_t count, iocp_t *iocp) override = 0; \
    errno_t flush_async(iocp_t *iocp) override = 0; \
    \
    ssize_t read(void *data, size_t count) override; \
    ssize_t write(void const *data, size_t count) override; \
    int flush() override;
