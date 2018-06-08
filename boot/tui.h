#pragma once
#include "types.h"

struct tui_str_t
{
    template<size_t sz>
    constexpr tui_str_t(tchar const(&txt)[sz])
        : len(sz-1)
        , str(txt)
    {
    }

    constexpr operator tchar const *() const
    {
        return str;
    }

    constexpr operator size_t() const
    {
        return len;
    }

    size_t const len;
    tchar const * const str;
};

template<typename T>
struct tui_list_t {
    template<int sz>
    constexpr tui_list_t(T(&items)[sz])
        : count(sz)
        , items(items)
    {
    }

    T& operator[](size_t index)
    {
        return items[index];
    }

    int const count;
    T * const items;
};

struct tui_menu_item_t {
    tui_str_t title;
    tui_list_t<tui_str_t> options;
    int index;
};

class tui_menu_renderer_t {
public:
    tui_menu_renderer_t(tui_list_t<tui_menu_item_t> *items);

    void center();
    void position(int x, int y);
    void draw(int selection);
    void interact_timeout(int ms);

private:
    void measure();
    void resize(int width, int height);

    tui_list_t<tui_menu_item_t> *items;
    int left;
    int top;
    int right;
    int bottom;
    int max_title;
    int max_value;
};

int readkey();
int systime();
void idle();
bool pollkey();


