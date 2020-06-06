#pragma once

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)
#define assume(x) if (unlikely(!(x))) __builtin_unreachable();
