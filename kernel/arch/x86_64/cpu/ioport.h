#pragma once

#include "types.h"
#include "type_traits.h"

typedef uint16_t ioport_t;

static _always_inline uint8_t inb(ioport_t port)
{
    uint8_t result;
    __asm__ __volatile__ (
        "inb %w[port],%b[result]\n\t"
        : [result] "=a" (result)
        : [port] "Nd" (port)
    );
    return result;
}

static _always_inline uint16_t inw(ioport_t port)
{
    uint16_t result;
    __asm__ __volatile__ (
        "inw %w[port],%w[result]"
        : [result] "=a" (result)
        : [port] "Nd" (port)
    );
    return result;
}

static _always_inline uint32_t ind(ioport_t port)
{
    uint32_t result;
    __asm__ __volatile__ (
        "inl %w[port],%k[result]"
        : [result] "=a" (result)
        : [port] "Nd" (port)
    );
    return result;
}

static _always_inline void outb(ioport_t port, uint8_t value)
{
    __asm__ __volatile__ (
        "outb %b[value],%w[port]\n\t"
        :
        : [value] "a" (value)
        , [port] "Nd" (port)
    );
}

static _always_inline void outw(ioport_t port, uint16_t value)
{
    __asm__ __volatile__ (
        "outw %w[value],%w[port]\n\t"
        :
        : [value] "a" (value)
        , [port] "Nd" (port)
    );
}

static _always_inline void outd(ioport_t port, uint32_t value)
{
    __asm__ __volatile__ (
        "outl %k[value],%w[port]\n\t"
        :
        : [value] "a" (value)
        , [port] "Nd" (port)
    );
}

//
// Block I/O

static _always_inline void insb(
        ioport_t port, void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep insb\n\t"
        : [value] "+D" (values)
        , [count] "+c" (count)
        : [port] "d" (port)
        : "memory"
    );
}

static _always_inline void insw(
        ioport_t port, void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep insw\n\t"
        : [value] "+D" (values)
        , [count] "+c" (count)
        : [port] "d" (port)
        : "memory"
    );
}

static _always_inline void insd(
        ioport_t port, void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep insl\n\t"
        : [value] "+D" (values)
        , [count] "+c" (count)
        : [port] "d" (port)
        : "memory"
    );
}

static _always_inline void outsb(
        ioport_t port, void const *values, size_t count)
{
    __asm__ __volatile__ (
        "rep outsb\n\t"
        : "+S" (values)
        , "+c" (count)
        : "d" (port)
        : "memory"
    );
}

static _always_inline void outsw(
        ioport_t port, void const *values, size_t count)
{
    __asm__ __volatile__ (
        "rep outsw\n\t"
        : "+S" (values)
        , "+c" (count)
        : "d" (port)
        : "memory"
    );
}

static _always_inline void outsd(
        ioport_t port, void const *values, size_t count)
{
    __asm__ __volatile__ (
        "rep outsl\n\t"
        : "+S" (values)
        , "+c" (count)
        : "d" (port)
        : "memory"
    );
}

template<int size>
struct io_helper
{
};

template<>
struct io_helper<1> {
    typedef uint8_t value_type;

    static _always_inline value_type inp(ioport_t port)
    {
        return inb(port);
    }

    static _always_inline void outp(ioport_t port, value_type val)
    {
        return outb(port, val);
    }

    static _always_inline void ins(
            ioport_t port, void const *values, size_t count)
    {
        insb(port, values, count);
    }

    static _always_inline void outs(
            ioport_t port, void const *values, size_t count)
    {
        outsb(port, values, count);
    }
};

template<>
struct io_helper<2>
{
    typedef uint16_t value_type;

    static _always_inline value_type inp(ioport_t port)
    {
        return inw(port);
    }

    static _always_inline void outp(ioport_t port, value_type val)
    {
        return outw(port, val);
    }

    static _always_inline void ins(
            ioport_t port, void const *values, size_t count)
    {
        insw(port, values, count);
    }

    static _always_inline void outs(
            ioport_t port, void const *values, size_t count)
    {
        outsw(port, values, count);
    }
};

template<>
struct io_helper<4>
{
    typedef uint32_t value_type;

    static _always_inline value_type inp(ioport_t port)
    {
        return ind(port);
    }

    static _always_inline void outp(ioport_t port, value_type val)
    {
        return outd(port, val);
    }

    static _always_inline void ins(
            ioport_t port, void const *values, size_t count)
    {
        insd(port, values, count);
    }

    static _always_inline void outs(
            ioport_t port, void const *values, size_t count)
    {
        outsd(port, values, count);
    }
};

template<int size>
static _always_inline typename type_from_size<size>::type inp(ioport_t port)
{
    return io_helper<size>::inp(port);
}

template<int size>
static _always_inline void outp(
        ioport_t port, typename io_helper<size>::value_type val)
{
    return io_helper<size>::outp(port, val);
}

template<int size>
static _always_inline void insp(
        ioport_t port, void const *values, size_t count)
{
    return io_helper<size>::insp(port, values, count);
}

template<int size>
static _always_inline void outsp(
        ioport_t port, void const *values, size_t count)
{
    return io_helper<size>::outsp(port, values, count);
}
