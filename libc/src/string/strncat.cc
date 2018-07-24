#include <string.h>

// The behavior is undefined if the destination array
// does not have enough space for the contents of both
// lhs and the first count characters of rhs, plus the
// terminating null character.
// The behavior is undefined if the rhs and lhs objects overlap.
// The behavior is undefined if either lhs is not a pointer
// to a null-terminated byte string or rhs is not a
// pointer to a character array.
char *strncat(char *restrict lhs, char const *restrict rhs, size_t sz)
{
    return strncpy(lhs + strlen(lhs), rhs, sz);
}
