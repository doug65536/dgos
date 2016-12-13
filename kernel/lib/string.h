#pragma once

#include "types.h"

#if 0 && !defined(__NO_BUILTIN) && !defined(__NO_STRING_BUILTIN)

#define __extern_always_inline static inline

#define DECLARE_BUILTIN_1(ret, name, t1) \
    __extern_always_inline ret name(t1 a) { \
        return __builtin_##name(a); \
    }

#define DECLARE_BUILTIN_2(ret, name, t1, t2) \
    __extern_always_inline ret name(t1 a, t2 b) { \
        return __builtin_##name(a, b); \
    }

#define DECLARE_BUILTIN_3(ret, name, t1, t2, t3) \
    __extern_always_inline ret name(t1 a, t2 b, t3 c) { \
        return __builtin_##name (a, b, c); \
    }

DECLARE_BUILTIN_1(size_t, strlen, char const *)
DECLARE_BUILTIN_3(void *, memchr, void const *, int, size_t)
DECLARE_BUILTIN_2(void *, strchr, char const *, int)

DECLARE_BUILTIN_2(int , strcmp, char const *, char const *)
DECLARE_BUILTIN_3(int , strncmp, char const *, char const *, size_t)
DECLARE_BUILTIN_3(int , memcmp, void const *restrict, void const *restrict, size_t)
DECLARE_BUILTIN_2(char *, strstr, char const *, char const *)

DECLARE_BUILTIN_3(void *, memset, void *, int, size_t)
DECLARE_BUILTIN_3(void *, memcpy, void *restrict, void const *restrict, size_t)
DECLARE_BUILTIN_3(void *, memmove, void *, const void *, size_t)

DECLARE_BUILTIN_2(char *, strcpy, char *restrict, char const *restrict)
DECLARE_BUILTIN_2(char *, strcat, char *restrict, char const *restrict)

DECLARE_BUILTIN_3(char *, strncpy, char *, char const *, size_t)
DECLARE_BUILTIN_3(char *, strncat, char *, char const *, size_t)

#else

size_t strlen(char const *src);
void *memchr(void const *mem, int ch, size_t count);
char *strchr(char const *s, int ch);

int strcmp(char const *lhs, char const *rhs);
int strncmp(char const *lhs, char const *rhs, size_t count);
int memcmp(void const *lhs, void const *rhs, size_t count);
char *strstr(char const *str, char const *substr);

void *memset(void *dest, int c, size_t n);
void *memcpy(void *restrict dest, void const *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);

char *strcpy(char *restrict dest, char const *restrict src);
char *strcat(char *restrict dest, char const *restrict src);

char *strncpy(char *dest, char const *src, size_t n);
char *strncat(char *dest, char const *src, size_t n);

int ucs4_to_utf8(char *out, int in);
int utf8_to_ucs4(char *in, char **ret_end);

#endif

void *aligned16_memset(void *dest, int c, size_t n);
