#include "messagebar.h"
#include "screen.h"
#include "string.h"

void message_bar_draw(int left, int top, int right, tchar const *message)
{
    print_box(left, top, right, top + 2, 0x8, true);
    size_t message_sz = strlen(message);
    int w = right - left;
    int c = w >> 1;
    int x = c - (message_sz >> 1);
    print_at(x, top + 1, 0x7, message_sz, message);
}
