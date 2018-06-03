#pragma once

#include "types.h"

__pure size_t strlen(char const *src);
__pure void *memchr(void const *mem, int ch, size_t count);
__pure void *strchr(char const *s, int ch);

__pure int strcmp(char const *lhs, char const *rhs);
__pure int strncmp(char const *lhs, char const *rhs, size_t count);
__pure int memcmp(void const *lhs, void const *rhs, size_t count);
__pure char *strstr(char const *str, char const *substr);

void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, void const *src, size_t n);
void *memmove(void *dest, void const *src, size_t n);

char *strcpy(char *dest, char const *src);
char *strcat(char *dest, char const *src);

char *strncpy(char *dest, char const *src, size_t n);
char *strncat(char *dest, char const *src, size_t n);

