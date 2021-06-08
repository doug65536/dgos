#pragma once

#include "types.h"

class serial_logger_t {
public:
    virtual bool init(int port) = 0;
    virtual bool write(int port, char const *data, size_t size) = 0;

    static serial_logger_t *instance;
};

void arch_serial_init(int port);
