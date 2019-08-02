#include <stdio.h>
#include <unistd.h>
#include "bits/cfile.h"

FILE *stdin = fdopen(STDIN_FILENO, "r");
FILE *stdout = fdopen(STDOUT_FILENO, "w");
FILE *stderr = fdopen(STDERR_FILENO, "w");

//extern "C" __attribute__((__constructor__))
//void __init_stdio()
//{
//    stdin = fdopen(STDIN_FILENO, "r");
//    stdout = fdopen(STDOUT_FILENO, "w");
//    stderr = fdopen(STDERR_FILENO, "w");

//    if (!stdin || !stdout || !stderr)
//        abort();
//}
