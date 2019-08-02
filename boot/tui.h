#pragma once
#include "types.h"

struct tui_str_t
{
    constexpr tui_str_t()
        : len(0)
        , str(nullptr)
    {
    }

    constexpr tui_str_t(tchar const *txt, size_t sz)
        : len(sz)
        , str(txt)
    {
    }

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

    size_t len;
    tchar const * str;
};

template<typename T>
struct tui_list_t {
    constexpr tui_list_t()
        : count{}
        , items{}
    {
    }

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

    int count;
    T * items;
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
int64_t systime();
void idle();
bool pollkey();
bool wait_input(uint32_t ms_timeout);

struct mouse_evt {
    int32_t x;
    int32_t y;
    int lmb;
    int rmb;
};
