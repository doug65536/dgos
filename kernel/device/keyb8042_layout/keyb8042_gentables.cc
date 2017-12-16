#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>

static char platform_url[] =
        "http://www.unicode.org/repos/cldr"
        "/tags/latest/keyboards/windows/_platform.xml";

static char *fmt_str_va(char const *format, va_list ap)
{
    int len = vsnprintf(0, 0, format, ap);
    char *str = (char*)malloc(len+1);
    vsnprintf(str, len+1, format, ap);
    return str;
}

static FILE *fmt_popen_va(
        char const *mode, char const *format, va_list ap)
{
    char *command = fmt_str_va(format, ap);
    printf("Invoking %s\n", command);
    FILE *stream = popen(command, mode);
    free(command);
    return stream;
}

static FILE *fmt_popen(
        char const *mode, char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    FILE *stream = fmt_popen_va(mode, format, ap);
    va_end(ap);
    return stream;
}

struct iso_key_t {
    char iso_code[4];
    int key_code;
};

iso_key_t iso_keys[256];

int mapping[256];
int mapping_shift[256];
int mapping_caps[256];
int mapping_caps_shift[256];
int mapping_altr[256];

int mapping_transform[256][2][4];

int cmp_iso_key(void const *a, void const *b)
{
    return memcmp(a, b, sizeof(iso_key_t));
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");

    FILE *s = fmt_popen(
                "r",
                "curl '%s' | "
                "grep -oP '<\\s*map\\s+keycode="
                "\"\\d+\"\\s+iso=\"[^\"]+\"\\s*/>'",
                platform_url);

    size_t line_buf_size = 64 << 10;
    char *line = (char*)malloc(line_buf_size);
    size_t lines = 0;
    while (fgets(line, line_buf_size, s)) {
        if (sscanf(line, "<map keycode=\"%d\" iso=\"%3s\"/>",
                   &iso_keys[lines].key_code,
                   iso_keys[lines].iso_code) == 2)
            ++lines;
    }
    pclose(s);

    qsort(iso_keys, lines, sizeof(*iso_keys), cmp_iso_key);

    for (size_t i = 0; i < lines; ++i)
        printf("iso_code=%s key_code=%d\n",
               iso_keys[i].iso_code, iso_keys[i].key_code);

    printf("lines = %zd\n", lines);
    free(line);

    return 1;
}

