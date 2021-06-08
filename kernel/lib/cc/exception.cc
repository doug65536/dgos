#include "exception.h"
#include "assert.h"
#include "halt.h"

__BEGIN_NAMESPACE_STD

void terminate()
{
    assert(!"std::terminate called!");
    halt_forever();
}

__END_NAMESPACE_STD
