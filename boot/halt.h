#pragma once
#include "types.h"

extern "C" _noreturn void panic(tchar const *s);

#define PANIC(msg) panic(TSTR msg)
