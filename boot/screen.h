#include "types.h"

#ifdef __GNUC__
#define ATTRIBUTE_FORMAT(m,n) __attribute__((format(printf, m, n)))
#else
#define ATTRIBUTE_FORMAT(m,n)
#endif

void scroll_screen();
void copy_to_screen(uint16_t offset, char const *message, uint8_t attr);

void print_line(char const* format, ...) ATTRIBUTE_FORMAT(1, 2);

extern char const hexlookup[];
