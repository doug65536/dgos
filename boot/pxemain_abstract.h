#pragma once
#include "types.h"

void pxe_init_tftp();
int pxe_api_tftp_open(char const *filename, uint16_t packet_size);
bool pxe_api_tftp_close();
int pxe_api_tftp_read(void *buffer, uint16_t sequence, uint16_t size);
int64_t pxe_api_tftp_get_fsize(char const *filename);
