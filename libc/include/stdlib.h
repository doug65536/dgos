#pragma once

#include <sys/cdefs.h>
#define __need_size_t
#include <stddef.h>
#include <sys/wait.h>

__BEGIN_DECLS

#define EXIT_FAILURE    1
#define EXIT_SUCCESS    0
#define RAND_MAX        0x7FFFFFFF
#define MB_CUR_MAX      1
#define _MALLOC_OVERHEAD 0

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

void          _Exit(int exitcode);

long          a64l(char const *);

void          abort(void);
int           abs(int);
int           atexit(void (*)(void));
double        atof(char const *);
int           atoi(char const *);
long          atol(char const *);
long long     atoll(char const *);
void         *bsearch(void const *, void const *, size_t, size_t,
                  int (*)(void const *, void const *));
void         *calloc(size_t, size_t);
div_t         div(int, int);

double        drand48(void);
double        erand48(unsigned short [3]);

__attribute__((__noreturn__))
void          exit(int);

void          free(void *);
char         *getenv(char const *);

int           getsubopt(char **, char *const *, char **);

int           grantpt(int);
char         *initstate(unsigned, char *, size_t);
long          jrand48(unsigned short [3]);
char         *l64a(long);

long          labs(long);

void          lcong48(unsigned short [7]);

ldiv_t        ldiv(long, long);
long long     llabs(long long);
lldiv_t       lldiv(long long, long long);

long          lrand48(void);

void         *malloc(size_t sz);
int           mblen(char const *, size_t);
size_t        mbstowcs(wchar_t *restrict, char const *restrict, size_t);
int           mbtowc(wchar_t *restrict, char const *restrict, size_t);

char         *mkdtemp(char *);
int           mkstemp(char *);

long          mrand48(void);
long          nrand48(unsigned short [3]);

int           posix_memalign(void **, size_t, size_t);

int           posix_openpt(int);
char         *ptsname(int);
int           putenv(char *);

void          qsort(void *, size_t, size_t, int (*)(void const *,
                  void const *));
int           rand(void);

int           rand_r(unsigned *);

long          random(void);

void         *realloc(void *, size_t);

char         *realpath(char const *restrict, char *restrict);
unsigned short *seed48(unsigned short [3]);

int           setenv(char const *, char const *, int);

void          setkey(char const *);
char         *setstate(char *);

void          srand(unsigned);

void          srand48(long);
void          srandom(unsigned);

double        strtod(char const *restrict, char **restrict);
float         strtof(char const *restrict, char **restrict);
long          strtol(char const *restrict, char **restrict, int);
long double   strtold(char const *restrict, char **restrict);
long long     strtoll(char const *restrict, char **restrict, int);
unsigned long strtoul(char const *restrict, char **restrict, int);
unsigned long long
              strtoull(char const *restrict, char **restrict, int);
int           system(char const *);

int           unlockpt(int);

int           unsetenv(char const *);

size_t        wcstombs(char *restrict, wchar_t const *restrict, size_t);
int           wctomb(char *, wchar_t);

__END_DECLS
