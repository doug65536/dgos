#pragma once

#include "types.h"
#include "cpu/nontemporal.h"

extern "C" {

size_t strlen(char const *src);
void *memchr(void const *mem, int ch, size_t count);
void *memrchr(void const *mem, int ch, size_t count);
char *strchr(char const *s, int ch);
char *strrchr(char const *s, int ch);

int strcmp(char const *lhs, char const *rhs);
int strncmp(char const *lhs, char const *rhs, size_t count);
int memcmp(void const *lhs, void const *rhs, size_t count);
char *strstr(char const *str, char const *substr);

void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, void const *src, size_t n);
void *memmove(void *dest, void const *src, size_t n);

char *strcpy(char *dest, char const *src);
char *strcat(char *dest, char const *src);

char *strncpy(char *dest, char const *src, size_t n);
char *strncat(char *dest, char const *src, size_t n);

int ucs4_to_utf8(char *out, int in);
int ucs4_to_utf16(uint16_t *out, int in);
int utf8_to_ucs4(char const *in, char const **ret_end);

int utf16_to_ucs4(uint16_t const *in, uint16_t const **ret_end);
int utf16be_to_ucs4(uint16_t const *in, uint16_t const **ret_end);

size_t utf8_count(char const *in);
size_t utf16_count(uint16_t const *in);

void *aligned16_memset(void *dest, int c, size_t n);

// Aligned fill of naturally aligned values
void *memfill_16(void *dest, uint16_t v, size_t count);
void *memfill_32(void *dest, uint32_t v, size_t count);
void *memfill_64(void *dest, uint64_t v, size_t count);

}

template<typename T>
void memzero(T &obj)
{
    memset(&obj, 0, sizeof(obj));
}
