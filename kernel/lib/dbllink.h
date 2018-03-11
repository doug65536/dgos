#pragma once

template<typename T>
struct dbllink
{
    T *next;
    T *prev;
};

template<typename T, dbllink<T> T::*member>
struct dbllink_manip
{
    static void insertAfter(dbllink<T> *root, T *place, T *item)
    {
        (item->*member).prev = place;

        if (!(place->*member).next) {
            // Inserting at the end of the list
            (item->*member).next = nullptr;
            root->prev = item;
        } else {
            (item->*member).next = (place->*member).next;
            ((place->*member).next->*member).prev = item;
        }

        (place->*member).next = item;
    }

    static void insertBefore(dbllink<T> *root, T *place, T *item)
    {
        (item->*member).next = place;

        if (!(place->*member).prev) {
            // Inserting at the beginning of the list
            (item->*member).prev = nullptr;
            root->next = item;
        } else {
            (item->*member).prev = (place->*member).prev;
            ((place->*member).prev->*member).next = item;
        }

        (place->*member).prev = item;
    }

    static void prepend(dbllink<T> *root, T *item) {
        if (root->next) {
            // Insert before the first item
            insertBefore(root, root->next, item);
        } else {
            // Empty list
            root->next = item;
            root->prev = item;
            (item->*member).prev = nullptr;
            (item->*member).next = nullptr;
        }
    }

    static void append(dbllink<T> *root, T *item)
    {
        if (root->prev) {
            insertAfter(root, root->prev, item);
        } else {
            prepend(root, item);
        }
    }

    static void remove(dbllink<T> *root, T *item)
    {
        if ((item->*member).prev)
            ((item->*member).prev->*member).next = (item->*member).next;
        else
            root->next = (item->*member).next;

        if ((item->*member).next)
            ((item->*member).next->*member).prev = (item->*member).prev;
        else
            root->prev = (item->*member).prev;
    }
};
