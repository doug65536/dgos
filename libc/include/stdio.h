#pragma once

#include <sys/types.h>
#include <stddef.h>
#include <stdarg.h>

__BEGIN_DECLS

typedef struct _FILE FILE;

typedef struct fpos_t {
    int64_t seek_pos;
} fpos_t;

#define EOF -1
#define FOPEN_MAX       16
#define TMP_MAX 10000
#define L_tmpnam 17
#define L_ctermid 17

#define BUFSIZ  8192

#define _IOFBF 3
#define _IOLBF 2
#define _IONBF 1

#define SEEK_CUR 0
#define SEEK_SET 1
#define SEEK_END 2

#define FILENAME_MAX    4096

#define P_tmpdir

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

void clearerr(FILE *stream);
char *ctermid(char *buf);
int feof(FILE *stream);
char *fgets(char *restrict buf, int sz, FILE *restrict stream);
int fileno(FILE *stream);
int fscanf(FILE *restrict stream, char const *restrict format, ...);
int fseek(FILE *stream, off_t dist, int rel);
long ftell(FILE *stream);
int pclose(FILE *stream);

FILE *fopen(char const *restrict filename, char const *restrict mode);
FILE *fdopen(int fd, char const *mode);
FILE *freopen(char const *restrict filename,
              char const *restrict mode,
              FILE *restrict stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
void setbuf(FILE *restrict stream, char *restrict buffer);
int setvbuf(FILE *restrict stream, char *restrict buffer,
            int mode, size_t size);
size_t fread(void *restrict buffer, size_t size,
          size_t count, FILE *restrict stream);
size_t fwrite(void const *restrict buffer, size_t size,
              size_t count, FILE *restrict stream);
int fgetc(FILE *stream);
int getc(FILE *stream);
char *fgets(char *restrict str, int count, FILE *restrict stream);
int fputc(int ch, FILE *stream);
int putc(int ch, FILE *stream);
int fputs(char const *restrict str, FILE *restrict stream);
int getchar();
char *gets(char *str);
int putchar(int ch);
int puts(char const *str);
int ungetc(int ch, FILE *stream);
int scanf(char const *restrict format, ...);
int vscanf(char const *restrict format, va_list ap);
int fscanf(FILE *restrict stream, char const *restrict format, ...);
int vfscanf(FILE *restrict stream, char const *restrict format, ...);

int printf(char const *restrict format, ...);
int fprintf(FILE *restrict stream, char const *restrict format, ...);
int sprintf(char *restrict buffer, char const *restrict format, ...);
int snprintf(char *restrict buffer, size_t buffer_sz,
             char const *restrict format, ...);

int vfprintf(FILE *restrict stream, char const *restrict format, va_list ap);

int vprintf(char const *restrict format, va_list ap);
int vsprintf(char *restrict buffer, char const *restrict format, va_list ap);
int vsnprintf(char *restrict buffer, size_t buffer_sz,
              char const *restrict format, va_list ap);
int vfprintf(FILE *restrict stream, char const *restrict format, va_list ap);

long ftell(FILE *stream);
int fgetpos(FILE *restrict stream, fpos_t *restrict pos);
int fseek(FILE *stream, long offset, int origin);
int fsetpos(FILE *stream, fpos_t const* pos);
void rewind(FILE *stream);
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(char const *s);

int remove(char const *fname);
int rename(char const *old_filename, char const *new_filename);
FILE *tmpfile();
char *tmpname(char *filename);

char *ctermid(char *s);

__END_DECLS
