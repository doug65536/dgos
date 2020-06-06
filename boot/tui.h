#pragma once
#include "types.h"

struct tui_str_t
{
    constexpr tui_str_t()
        : len(0)
        , str(nullptr)
    {
    }

    template<size_t sz>
    constexpr tui_str_t(tchar const(&txt)[sz])
        : len(sz-1)
        , str(txt)
    {
    }

    constexpr tui_str_t(tchar const *txt, size_t sz)
        : len(sz)
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

    template<size_t sz>
    constexpr tui_list_t(T(&items)[sz])
        : count(sz)
        , items(items)
    {
    }

    T& operator[](size_t index)
    {
        return items[index];
    }

    size_t count;
    T * items;
};

struct tui_menu_item_t {
    tui_str_t title;
    tui_list_t<tui_str_t> options;
    size_t index;
};

class tui_menu_renderer_t {
public:
    constexpr tui_menu_renderer_t(tui_list_t<tui_menu_item_t> *items);

    void center(int offset = 0);
    void position(int x, int y);
    void draw(size_t index, size_t selected, bool full);
    void interact_timeout(int ms);

private:
    void measure();
    void resize(int width, int height);

    tui_list_t<tui_menu_item_t> *items;

    int dirty_st = -1;
    int dirty_en = -1;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int max_title = 0;
    int max_value = 0;
    int scroll_pos = 0;
};

constexpr tui_menu_renderer_t::tui_menu_renderer_t(
        tui_list_t<tui_menu_item_t> *items)
    : items(items)
{
}

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

extern tui_str_t tui_dis_ena[2];
