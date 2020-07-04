#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void __assert_failed(char const *filename, int line, char const *expr)
{
    printf("ASSERTION FAILED: %s %s(%d)\n", expr, filename, line);
    abort();
}
