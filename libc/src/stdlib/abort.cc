#include <stdlib.h>

void abort()
{
    _Exit(255);
}
