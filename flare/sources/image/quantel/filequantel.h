#pragma once

#ifndef FILEQUANTEL_H
#define FILEQUANTEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define QNT_FORMAT 1
#define QTL_FORMAT 2
#define YUV_FORMAT 3
#define SDL_FORMAT 4
#define VPB_FORMAT 5

#define T_CHAR wchar_t

void *img_read_quantel(const T_CHAR *fname, int *w, int *h, int type);
int img_write_quantel(const T_CHAR *, void *buffer, int w, int h, int type);
void img_read_quantel_info(const T_CHAR *, int *w, int *h, int type);

#ifdef __cplusplus
}
#endif

#endif
