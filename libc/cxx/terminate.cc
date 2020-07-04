#include <stdlib.h>

namespace std {

__attribute__((__noreturn__))
void terminate()
{
    abort();
}

}
