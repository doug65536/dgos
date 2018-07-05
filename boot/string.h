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
_pure int strncat(char const *lhs, char const *rhs, size_t count);
_pure int strncat(char16_t const *lhs, char16_t const *rhs, size_t count);
_pure int memcmp(void const *lhs, void const *rhs, size_t count);
_pure char *strstr(tchar const *str, tchar const *substr);

void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, void const *src, size_t n);
void *memmove(void *dest, void const *src, size_t n);

char *strcpy(char *dest, char const *src);
char *strcat(char *dest, char const *src);

char *strncpy(char *dest, char const *src, size_t n);
char *strncat(char *dest, char const *src, size_t n);
