#pragma once

extern "C" int *__errno_location();

#define errno (*__errno_location())
