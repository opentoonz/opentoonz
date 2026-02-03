#pragma once

#ifndef __FILEBMP_INCLUDED__
#define __FILEBMP_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif
/*
#if defined(_WIN32)
typedef struct {unsigned char b,g,r,m;}  LPIXEL;
#elif defined(__sgi)
typedef struct { unsigned char  m,b,g,r; } LPIXEL;
#elif defined(LINUX) || defined(FREEBSD)
typedef struct { unsigned char  r,g,b,m; } LPIXEL;
#else
#error	Not yet implemented
#endif
*/
enum BMP_ERROR_CODE {
  OK,
  UNSUPPORTED_BMP_FORMAT = -1,
  OUT_OF_MEMORY          = -2,
  UNEXPECTED_EOF         = -3,
  CANT_OPEN_FILE         = -4,
  WRITE_ERROR            = -5
};

typedef const wchar_t *MYSTRING;

int writebmp(const MYSTRING filename, int xsize, int ysize, void *buffer,
             int bpp);
int readbmp(const MYSTRING filename, int *xsize, int *ysize, void **buffer);
int readbmpregion(const MYSTRING fname, void **pimg, int x1, int y1, int x2,
                  int y2, int scale);

int readbmp_size(const MYSTRING fname, int *lx, int *ly);
int readbmp_bbox(const MYSTRING fname, int *x0, int *y0, int *x1, int *y1);

#ifdef __cplusplus
}
#endif

#endif
