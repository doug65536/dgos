#include "types.h"

void scroll_screen();
void copy_to_screen(uint16_t offset, char const *message, uint8_t attr);
void print_line(char const* format, ...);

extern char const hexlookup[];
