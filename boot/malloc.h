#include "types.h"

void *malloc(uint16_t bytes);
void *calloc(uint16_t num, uint16_t size);
void free(void *p);

void test_malloc(void);
