#pragma once

#include "types.h"

typedef uint16_t ioport_t;

static inline uint8_t inb(ioport_t port)
{
    uint8_t result;
    __asm__ __volatile__ (
        "inb %w[port],%b[result]\n\t"
        : [result] "=a" (result)
        : [port] "Nd" (port)
    );
    return result;
}

static inline uint16_t inw(ioport_t port)
{
    uint16_t result;
    __asm__ __volatile__ (
        "inw %w[port],%w[result]"
        : [result] "=a" (result)
        : [port] "Nd" (port)
    );
    return result;
}

static inline uint32_t ind(ioport_t port)
{
    uint32_t result;
    __asm__ __volatile__ (
        "inl %w[port],%[result]"
        : [result] "=a" (result)
        : [port] "Nd" (port)
    );
    return result;
}

static inline void outb(ioport_t port, uint8_t value)
{
    __asm__ __volatile__ (
        "outb %b[value],%w[port]\n\t"
        :
        : [value] "a" (value),
          [port] "Nd" (port)
    );
}

static inline void outw(ioport_t port, uint16_t value)
{
    __asm__ __volatile__ (
        "outw %w[value],%w[port]\n\t"
        :
        : [value] "a" (value),
          [port] "Nd" (port)
    );
}

static inline void outd(ioport_t port, uint32_t value)
{
    __asm__ __volatile__ (
        "outl %[value],%w[port]\n\t"
        :
        : [value] "a" (value),
          [port] "Nd" (port)
    );
}

//
// Block I/O

static inline void insb(ioport_t port,
                         void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep insb\n\t"
        :
        : [value] "D" (values),
          [count] "c" (count),
          [port] "Nd" (port)
    );
}

static inline void insw(ioport_t port,
                         void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep insw\n\t"
        :
        : [value] "D" (values),
          [count] "c" (count),
          [port] "Nd" (port)
    );
}

static inline void insd(ioport_t port,
                         void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep insl\n\t"
        :
        : [value] "D" (values),
          [count] "c" (count),
          [port] "Nd" (port)
    );
}

static inline void outsb(ioport_t port,
                         void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep outsb\n\t"
        :
        : [value] "S" (values),
          [count] "c" (count),
          [port] "Nd" (port)
    );
}

static inline void outsw(ioport_t port,
                         void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep outsw\n\t"
        :
        : [value] "S" (values),
          [count] "c" (count),
          [port] "Nd" (port)
    );
}

static inline void outsd(ioport_t port,
                         void const *values, intptr_t count)
{
    __asm__ __volatile__ (
        "rep outsl\n\t"
        :
        : [value] "S" (values),
          [count] "c" (count),
          [port] "Nd" (port)
    );
}
