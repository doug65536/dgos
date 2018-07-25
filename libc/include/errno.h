#pragma once

typedef int errno_t;

extern "C" int *__errno_location();

#define errno (*__errno_location())
