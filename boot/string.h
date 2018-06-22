#pragma once

#include "types.h"

_pure size_t strlen(char const *src);
_pure size_t strlen(char16_t const *src);
_pure void *memchr(void const *mem, int ch, size_t count);
_pure void *strchr(tchar const *s, int ch);

_pure int strcmp(tchar const *lhs, tchar const *rhs);
_pure int strncmp(tchar const *lhs, tchar const *rhs, size_t count);
_pure int memcmp(void const *lhs, void const *rhs, size_t count);
_pure char *strstr(tchar const *str, tchar const *substr);

void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, void const *src, size_t n);
void *memmove(void *dest, void const *src, size_t n);

char *strcpy(char *dest, char const *src);
char *strcat(char *dest, char const *src);

char *strncpy(char *dest, char const *src, size_t n);
char *strncat(char *dest, char const *src, size_t n);
