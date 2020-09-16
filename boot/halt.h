#pragma once
#include "types.h"

__BEGIN_DECLS

_noreturn
void panic(tchar const *s);

#define PANIC(msg) panic(TSTR msg)

#define PANIC_OOM() panic(TSTR "Out of memory!")

__END_DECLS
