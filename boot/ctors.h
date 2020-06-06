#pragma once

// Constructor ordering
enum constructor_order_t {
    ctor_malloc = 101,
    ctor_console,
    ctor_graphics,
    ctor_fs,
    ctor_physmem,
    ctor_paging
};

extern "C" void ctors_invoke();
extern "C" void dtors_invoke();
