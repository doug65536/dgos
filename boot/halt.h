#pragma once
#include "types.h"

extern "C" __noreturn void panic(tchar const *s);

#define PANIC(msg) panic(TSTR msg)
