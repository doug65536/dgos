#include <string.h>

char *strtok_r(char * restrict str,
               char const * restrict delim,
               char ** restrict strtok_ptr)
{
    if (str)
        *strtok_ptr = str;

    char *current = *strtok_ptr;

    // Search for the first character which is not contained in delim
    size_t skip = strspn(current, delim);

    // If no such character was found, return nullptr
    if (!current[skip])
        return nullptr;

    char *beginning = current + skip;

    // Scan non-delimiters
    size_t keep = strcspn(beginning, delim);

    // Start at next character next time (or at null if hit null)
    *strtok_ptr = beginning + keep + !!beginning[keep];

    beginning[keep] = 0;

    return beginning;
}
