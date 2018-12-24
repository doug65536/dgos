#pragma once

#include "types.h"

_pure size_t strlen(char const *src);
_pure size_t strlen(char16_t const *src);
_pure void *memchr(void const *mem, int ch, size_t count);
_pure void *strchr(char const *s, int ch);
_pure void *strchr(char16_t const *s, int ch);

_pure int strcmp(char const *lhs, char const *rhs);
_pure int strcmp(char16_t const *lhs, char16_t const *rhs);
_pure int strncmp(char const *lhs, char const *rhs, size_t count);
_pure int strncmp(char16_t const *lhs, char16_t const *rhs, size_t count);
_pure int strncat(char const * restrict lhs,
                  char const * restrict rhs, size_t count);
_pure int strncat(char16_t const * restrict lhs,
                  char16_t const * restrict rhs, size_t count);
_pure int memcmp(void const *lhs, void const *rhs, size_t count);
_pure char *strstr(tchar const *str, tchar const *substr);

void *memset(void *dest, int c, size_t n);
void *memcpy(void * restrict dest, void const * restrict src, size_t n);
void *memmove(void *dest, void const *src, size_t n);

char *strcpy(char * restrict dest, char const * restrict src);
char *strcat(char * restrict dest, char const * restrict src);

char *strncpy(char * restrict dest, char const * restrict src, size_t n);
char *strncat(char * restrict dest, char const * restrict src, size_t n);
