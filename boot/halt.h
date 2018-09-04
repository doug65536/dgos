#pragma once
#include "types.h"

extern "C" _noreturn void panic(tchar const *s);

#define PANIC(msg) panic(TSTR msg)

#define PANIC_OOM() panic(TSTR "Out of memory!")
