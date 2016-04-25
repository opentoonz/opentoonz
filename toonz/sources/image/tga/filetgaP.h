#pragma once

#ifndef TGA_P_H
#define TGA_P_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
void img_read_tga_info(const wchar_t *filename, int *w, int *h);
void *img_read_tga(const wchar_t *filename, int *w, int *h);
int img_write_tga(const wchar_t *fname, void *buffer, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
