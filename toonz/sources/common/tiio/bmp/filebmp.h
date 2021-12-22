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

typedef enum {
  BMP_NONE,
  BMP_BW,
  BMP_GREY16,
  BMP_GREY16C,
  BMP_GREY256,
  BMP_GREY256C,
  BMP_CMAPPED16,
  BMP_CMAPPED16C,
  BMP_CMAPPED256,
  BMP_CMAPPED256C,
  BMP_RGB

} BMP_SUBTYPE;

typedef struct {
  UINT bfSize;
  UINT bfOffBits;
  UINT biSize;
  UINT biWidth;
  UINT biHeight;
  UINT biPlanes;
  UINT biBitCount;
  UINT biCompression;
  UINT biSizeImage;
  UINT biXPelsPerMeter;
  UINT biYPelsPerMeter;
  UINT biClrUsed;
  UINT biClrImportant;
  UINT biFilesize;
  UINT biPad;

} BMP_HEADER;

int load_bmp_header(FILE *fp, BMP_HEADER **pHd);
int write_bmp_header(FILE *fp, BMP_HEADER *hd);
void release_bmp_header(BMP_HEADER *hd);

int write_bmp_palette(FILE *fp, int nc, UCHAR *b, UCHAR *g, UCHAR *r);
int make_bmp_palette(int colors, int grey, UCHAR *r, UCHAR *g, UCHAR *b);

/*BMP_SUBTYPE
     bmp_get_colorstyle(IMAGE *img);*/

int error_checking_bmp(BMP_HEADER *hd);
int read_bmp_line(FILE *fp, void *line, UINT w, UINT h, UCHAR **mp,
                  BMP_SUBTYPE type);
int write_bmp_line(FILE *fp, void *line_buffer, UINT w, UINT row, UCHAR *mp,
                   BMP_SUBTYPE type);
int skip_bmp_lines(FILE *fp, UINT w, UINT rows, int whence, BMP_SUBTYPE type);

#define BMP_WIN_OS2_OLD 12

#ifdef __cplusplus
}
#endif

#endif
