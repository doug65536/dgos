#pragma once

#include <sys/cdefs.h>
#include <stddef.h>
#include <locale.h>

__BEGIN_DECLS

void *memccpy(void *restrict dest, void const *restrict src,
              int c, size_t sz);

void *memchr(void const *, int, size_t);
int memcmp(void const *, void const *, size_t);
void *memcpy(void *restrict, void const *restrict, size_t);
void *memmove(void *, void const *, size_t);
void *memset(void *, int, size_t);

char *stpcpy(char *restrict, char const *restrict);
char *stpncpy(char *restrict, char const *restrict, size_t);

char *strcat(char *restrict, char const *restrict);
char *strchr(char const *, int);
int strcmp(char const *, char const *);
int strcoll(char const *, char const *);

int strcoll_l(char const *, char const *, locale_t);

char *strcpy(char *restrict, char const *restrict);
size_t strcspn(char const *, char const *);

char *strdup(char const *);

char *strerror(int);

char *strerror_l(int, locale_t);
int strerror_r(int, char *, size_t);

size_t strlen(char const *);
char *strncat(char *restrict, char const *restrict, size_t);
int strncmp(char const *, char const *, size_t);
char *strncpy(char *restrict, char const *restrict, size_t);

char *strndup(char const *, size_t);
size_t strnlen(char const *, size_t);

char *strpbrk(char const *, char const *);
char *strrchr(char const *, int);

char *strsignal(int);

size_t strspn(char const *, char const *);
char *strstr(char const *haystack, char const *needle);
char *strtok(char *restrict, char const *restrict);

char *strtok_r(char *restrict, char const *restrict, char **restrict);

size_t strxfrm(char *restrict, char const *restrict, size_t);

size_t strxfrm_l(char *restrict, char const *restrict, size_t, locale_t);

__END_DECLS
