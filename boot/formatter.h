#pragma once
#include "types.h"


class formatter_emitter_t {
public:
    virtual void buffer_char(tchar c) = 0;
    virtual void flush() = 0;
};
