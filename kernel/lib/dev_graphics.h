#pragma once

class dev_graphics_t {
public:
    ~dev_graphics_t();

    virtual int render_batch() = 0;
};
