#pragma once

#include "thread.h"

int process_fexecve(int fd, char const * const * const argv,
                    char const * const * const envp);
