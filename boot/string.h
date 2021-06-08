#pragma once

#include "types.h"

_access(read_only, 1)
_pure size_t strlen(char const *src);

_access(read_only, 1)
_pure size_t strlen(char16_t const *src);

_access(read_only, 1, 3)
_pure char *strnchr(char const *src, int ch, size_t count);

_access(read_only, 1, 3)
_pure char16_t *strnchr(char16_t const *src, int ch, size_t count);

_access(read_only, 1, 3)
_pure void *memchr(void const *mem, int ch, size_t count);

_pure char *strchr(char const *s, int ch);
_pure char16_t *strchr(char16_t const *s, int ch);

_access(read_only, 1) _access(read_only, 2)
_pure int strcmp(char const *lhs, char const *rhs);

_access(read_only, 1) _access(read_only, 2)
_pure int strcmp(char16_t const *lhs, char16_t const *rhs);

_access(read_only, 1, 3) _access(read_only, 2, 3)
_pure int strncmp(char const *lhs, char const *rhs, size_t count);

_access(read_only, 1, 3) _access(read_only, 2, 3)
_pure int strncmp(char16_t const *lhs, char16_t const *rhs, size_t count);

_access(read_write, 1) _access(read_only, 2, 3)
_pure char *strncat(char * restrict lhs,
                    char const * restrict rhs, size_t count);

_access(read_only, 2, 3)
_pure char16_t *strncat(char16_t * restrict lhs,
                        char16_t const * restrict rhs, size_t count);

_access(read_only, 1, 3) _access(read_only, 2, 3)
_pure int memcmp(void const *lhs, void const *rhs, size_t count);


_access(read_only, 1) _access(read_only, 2)
_pure char *strstr(tchar const *str, tchar const *substr);

__BEGIN_DECLS

_access(write_only, 1, 3)
void *memset(void *dest, int c, size_t n);

_access(write_only, 1, 3) _access(read_only, 2, 3)
void *memcpy(void * restrict dest,
             void const * restrict src, size_t n);

_access(write_only, 1, 3) _access(read_only, 2, 3)
void *memmove(void *dest, void const *src, size_t n);

_access(read_write, 1) _access(read_only, 2)
char *strcat(char * restrict dest, char const * restrict src);

// Copies up to n bytes to the destination
// If the input is too long, the resulting string is not null terminated
// In all cases the unused excess portion of the destination is zero cleared
_access(write_only, 1, 3) _access(read_only, 2, 3)
char *strncpy(char * restrict dest, char const * restrict src, size_t n);

__END_DECLS

_access(write_only, 1, 3) _access(read_only, 2, 3)
char16_t *strncpy(char16_t * restrict dest,
                  char16_t const * restrict src, size_t n);

_access(write_only, 1) _access(read_only, 2)
char *strcpy(char * restrict dest, char const * restrict src);

_access(write_only, 1) _access(read_only, 2)
char16_t *strcpy(char16_t *dest, char16_t const *src);

_access(read_only, 1)
char *utf8_from_tchar(char16_t *block);

_access(read_only, 1)
char *utf8_from_tchar(char *block);
