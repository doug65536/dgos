#include "testassert.h"
#include <string.h>

TEST_CASE(test_memset) {
    char buf[8] = "XXXXXXX";
    char tc1[8] = "aaaaXXX";
    
    memset(buf, 'a', 4);
}
