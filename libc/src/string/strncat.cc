#include <string.h>

// The behavior is undefined if the destination array
// does not have enough space for the contents of both
// dest and the first count characters of src, plus the
// terminating null character. The behavior is undefined
// if the source and destination objects overlap. The
// behavior is undefined if either dest is not a pointer
// to a null-terminated byte string or src is not a
// pointer to a character array.
char *strncat(char *restrict lhs, char const *restrict rhs, size_t sz)
{
    return strncpy(lhs + strlen(lhs), rhs, sz);
}
