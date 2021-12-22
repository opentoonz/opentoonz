

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifndef _DEBUG
#undef _STLP_DEBUG
#else
#define _STLP_DEBUG 1
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* brutto! Forse dovremmo metterlo in include */
#include "../image/compatibility/tnz4.h"
#include "../image/compatibility/inforegion.h"

#include "filebmp.h"

#define UNUSED_REDUCE_COLORS

/*---------------------------------------------------------------------------*/
/*-- Defines ----------------------------------------------------------------*/

#define BMP_BI_RGB 0
#define BMP_BI_RLE8 1
#define BMP_BI_RLE4 2

#define BMP_WIN_NEW 40
#define BMP_OS2_NEW 64

#define BMP_READ_INFO 1
#define BMP_READ_IMAGE 2

#define BMP_FERROR(fp) (ferror(fp) || feof(fp))

#define BMP_RMAP(x) (x & 0xe0) | ((x & 0xe0) >> 3) | ((x & 0xc0) >> 6)
#define BMP_GMAP(x) ((x & 0x1c) << 3) | (x & 0x1c) | ((x & 0x18) >> 3)
#define BMP_BMAP(x)                                                            \
  ((x & 0x03) << 6) | ((x & 0x03) << 4) | ((x & 0x03) << 2) | (x & 0x03)

#define BMP_RMAP16(x)                                                          \
  ((x & 0xc0) << 0) | ((x & 0xc0) >> 2) | ((x & 0xc0) >> 4) | ((x & 0xc0) >> 6)
#define BMP_GMAP16(x)                                                          \
  ((x & 0x60) << 1) | ((x & 0x60) >> 1) | ((x & 0x60) >> 3) | ((x & 0x60) >> 5)
#define BMP_BMAP16(x)                                                          \
  ((x & 0x30) << 2) | ((x & 0x30) << 0) | ((x & 0x30) >> 2) | ((x & 0x30) >> 4)

#define BMP_CUT(a, b, c)                                                       \
  {                                                                            \
    if (a < b)                                                                 \
      a = b;                                                                   \
    else if (a > c)                                                            \
      a = c;                                                                   \
  }

#define BMP_REDUCE_COLORS(r, g, b)                                             \
  ((r & 0xe0) | ((g & 0xe0) >> 3) | ((b & 0xc0) >> 6))

#define BMP_ADD_ERROR(pix, weight)                                             \
  {                                                                            \
    tmp = (pix).r + ((r1 * weight) >> 4);                                      \
    BMP_CUT(tmp, 0, 255);                                                      \
    (pix).r = tmp;                                                             \
    tmp     = (pix).g + ((g1 * weight) >> 4);                                  \
    BMP_CUT(tmp, 0, 255);                                                      \
    (pix).g = tmp;                                                             \
    tmp     = (pix).b + ((b1 * weight) >> 4);                                  \
    BMP_CUT(tmp, 0, 255);                                                      \
    (pix).b = tmp;                                                             \
  }

#define BMP_ADD_ERROR_BW(pix, error)                                           \
  {                                                                            \
    tmp = (pix).r + (error);                                                   \
    BMP_CUT(tmp, 0, 255);                                                      \
    (pix).r = tmp;                                                             \
    tmp     = (pix).g + (error);                                               \
    BMP_CUT(tmp, 0, 255);                                                      \
    (pix).g = tmp;                                                             \
    tmp     = (pix).b + (error);                                               \
    BMP_CUT(tmp, 0, 255);                                                      \
    (pix).b = tmp;                                                             \
  }

/*---------------------------------------------------------------------------*/
/*-- Structures and Enums ---------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*-- Prototypes -------------------------------------------------------------*/

static BMP_SUBTYPE bmp_get_colorstyle(IMAGE *img);

/*---------------------------------------------------------------------------*/
/*-- Local Prototypes -------------------------------------------------------*/

static UINT getshort(FILE *fp);
static UINT getint(FILE *fp);
static void putshort(FILE *fp, int value);
static void putint(FILE *fp, int value);

/*static int img_read_bmp_generic(const MYSTRING fname, int type, IMAGE **);*/
#ifndef UNUSED_REDUCE_COLORS
static UCHAR *reduce_colors(UCHAR *buffin, int xsize, int ysize, UCHAR *rmap,
                            UCHAR *gmap, UCHAR *bmap, int nc);

#endif

#ifndef __LIBSIMAGE__
#ifdef CICCIO

static int loadBMP1(FILE *fp, LPIXEL *pic, UINT w, UINT h, UCHAR *r, UCHAR *g,
                    UCHAR *b);
static int loadBMP4(FILE *fp, LPIXEL *pic, UINT w, UINT h, UINT comp, UCHAR *r,
                    UCHAR *g, UCHAR *b);
static int loadBMP8(FILE *fp, LPIXEL *pic, UINT w, UINT h, UINT comp, UCHAR *r,
                    UCHAR *g, UCHAR *b);
static int loadBMP24(FILE *fp, LPIXEL *pic, UINT w, UINT h);

#endif
#endif /* __LIBSIMAGE__ */

static int writeBMP1(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *map);
static int writeBMP4(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *map);
static int writeBMPC4(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *map);
static int writeBMP8(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *map);
static int writeBMPC8(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *map);
static int writeBMP24(FILE *fp, UCHAR *pic24, UINT w, UINT h, UCHAR *map);

static int load_lineBMP1(FILE *fp, LPIXEL *pic, UINT w, UINT padw, UCHAR **map);
static int load_lineBMP4(FILE *fp, LPIXEL *pic, UINT w, UINT padw, UCHAR **map);
static int load_lineBMPC4(FILE *fp, LPIXEL *pic, UINT w, UINT y, UCHAR **map);
static int load_lineBMP8(FILE *fp, LPIXEL *pic, UINT w, UINT padw, UCHAR **map);
static int load_lineBMPC8(FILE *fp, LPIXEL *pic, UINT w, UINT y, UCHAR **map);
static int load_lineBMP24(FILE *fp, LPIXEL *pic, UINT w, UINT padb);

static int line_writeBMP1(FILE *fp, UCHAR *pic8, UINT w, UINT padw,
                          UCHAR *pc2nc);
static int line_writeBMP4(FILE *fp, UCHAR *pic8, UINT w, UINT padw,
                          UCHAR *pc2nc);
static int line_writeBMPC4(FILE *fp, UCHAR *pic8, UINT w, UINT row,
                           UCHAR *pc2nc);
static int line_writeBMP8(FILE *fp, UCHAR *pic8, UINT w, UINT padw,
                          UCHAR *pc2nc);
static int line_writeBMPC8(FILE *fp, UCHAR *pic8, UINT w, UINT row,
                           UCHAR *pc2nc);
static int line_writeBMP24(FILE *fp, LPIXEL *pp, UINT w, UINT padb);

static int skip_rowsBMP1(FILE *fp, UINT w, UINT pad, UINT rows, int whence);
static int skip_rowsBMP4(FILE *fp, UINT w, UINT pad, UINT rows, int whence);
static int skip_rowsBMPC4(FILE *fp, UINT rows);
static int skip_rowsBMP8(FILE *fp, UINT w, UINT pad, UINT rows, int whence);
static int skip_rowsBMPC8(FILE *fp, UINT rows);
static int skip_rowsBMP24(FILE *fp, UINT w, UINT pad, UINT rows, int whence);

#define BMP_DEBUG 0
#define __BMP_WRITE_LINE_BY_LINE
#define __BMP_READ_LINE_BY_LINE

/*---------------------------------------------------------------------------*/
int read_bmp_line(FILE *fp, void *line_buffer, UINT w, UINT row, UCHAR **map,
                  BMP_SUBTYPE type)
/*---------------------------------------------------------------------------*/
{
  LPIXEL *pic = (LPIXEL *)line_buffer;
  unsigned int pad;
  int rv = 0;

  switch (type) {
  case BMP_BW:
    pad = ((w + 31) / 32) * 32;
    rv  = load_lineBMP1(fp, pic, w, (UINT)pad, map);
    break;
  case BMP_GREY16:
  case BMP_CMAPPED16:
    pad = ((w + 7) / 8) * 8;
    rv  = load_lineBMP4(fp, pic, w, (UINT)pad, map);
    break;
  case BMP_GREY16C:
  case BMP_CMAPPED16C:
    rv = load_lineBMPC4(fp, pic, w, row, map);
    if (rv == -1)
      rv = 1;
    else if (rv == -2)
      rv = 0;
    else if (rv == -3)
      rv = 0;
    else
      rv = 0;
    break;
  case BMP_GREY256:
  case BMP_CMAPPED256:
    pad = ((w + 3) / 4) * 4;
    rv  = load_lineBMP8(fp, pic, w, (UINT)pad, map);
    break;
  case BMP_GREY256C:
  case BMP_CMAPPED256C:
    rv = load_lineBMPC8(fp, pic, w, row, map);
    if (rv == -1)
      rv = 1;
    else if (rv == -2)
      rv = 0;
    else if (rv == -3)
      rv = 0;
    else
      rv = 0;
    break;
  case BMP_RGB:
    pad = (4 - ((w * 3) % 4)) & 0x03;
    rv  = load_lineBMP24(fp, pic, w, (UINT)pad);
    break;
  default:
    break;
  }

  return !rv; /* return 0 for unsuccess */
}

/*---------------------------------------------------------------------------*/
int write_bmp_line(FILE *fp, void *line_buffer, UINT w, UINT row, UCHAR *map,
                   BMP_SUBTYPE type)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pic  = (UCHAR *)line_buffer;
  LPIXEL *p24 = (LPIXEL *)line_buffer;
  unsigned int pad;
  int rv = 0;

  switch (type) {
  case BMP_BW:
    pad = ((w + 31) / 32) * 32;
    rv  = line_writeBMP1(fp, pic, w, (UINT)pad, map);
    break;
  case BMP_GREY16:
  case BMP_CMAPPED16:
    pad = ((w + 7) / 8) * 8;
    rv  = line_writeBMP4(fp, pic, w, (UINT)pad, map);
    break;
  case BMP_GREY16C:
  case BMP_CMAPPED16C:
    rv = line_writeBMPC4(fp, pic, w, row, map);
    break;
  case BMP_GREY256:
  case BMP_CMAPPED256:
    pad = ((w + 3) / 4) * 4;
    rv  = line_writeBMP8(fp, pic, w, (UINT)pad, map);
    break;
  case BMP_GREY256C:
  case BMP_CMAPPED256C:
    rv = line_writeBMPC8(fp, pic, w, row, map);
    break;
  case BMP_RGB:
    pad = (4 - ((w * 3) % 4)) & 0x03;
    rv  = line_writeBMP24(fp, p24, w, (UINT)pad);
    break;
  default:
    break;
  }

  return rv; /* 0 for unsuccess */
}

/*---------------------------------------------------------------------------*/
int skip_bmp_lines(FILE *fp, UINT w, UINT rows, int whence, BMP_SUBTYPE type)
/*---------------------------------------------------------------------------*/
{
  unsigned int pad;
  int rv = 0;

  switch (type) {
  case BMP_BW:
    pad = ((w + 31) / 32) * 32;
    rv  = skip_rowsBMP1(fp, w, (UINT)pad, rows, whence);
    break;
  case BMP_GREY16:
  case BMP_CMAPPED16:
    pad = ((w + 7) / 8) * 8;
    rv  = skip_rowsBMP4(fp, w, (UINT)pad, rows, whence);
    break;
  case BMP_GREY16C:
  case BMP_CMAPPED16C:
    rv = skip_rowsBMPC4(fp, rows);
    break;
  case BMP_GREY256:
  case BMP_CMAPPED256:
    pad = ((w + 3) / 4) * 4;
    rv  = skip_rowsBMP8(fp, w, (UINT)pad, rows, whence);
    break;
  case BMP_GREY256C:
  case BMP_CMAPPED256C:
    rv = skip_rowsBMPC8(fp, rows);
    break;
  case BMP_RGB:
    pad = (4 - ((w * 3) % 4)) & 0x03;
    rv  = skip_rowsBMP24(fp, w, (UINT)pad, rows, whence);
    break;
  default:
    break;
  }

  return !rv;
}

/*---------------------------------------------------------------------------*/
int error_checking_bmp(BMP_HEADER *hd)
/*---------------------------------------------------------------------------*/
{
  /* error checking */
  if ((hd->biBitCount != 1 && hd->biBitCount != 4 && hd->biBitCount != 8 &&
       hd->biBitCount != 24) ||
      hd->biPlanes != 1 || hd->biCompression > BMP_BI_RLE4) {
    return UNSUPPORTED_BMP_FORMAT;
  }

  /* error checking */
  if (((hd->biBitCount == 1 || hd->biBitCount == 24) &&
       hd->biCompression != BMP_BI_RGB) ||
      (hd->biBitCount == 4 && hd->biCompression == BMP_BI_RLE8) ||
      (hd->biBitCount == 8 && hd->biCompression == BMP_BI_RLE4)) {
    return UNSUPPORTED_BMP_FORMAT;
  }

  return OK;
}

/*---------------------------------------------------------------------------*/
int load_bmp_header(FILE *fp, BMP_HEADER **header)
/*---------------------------------------------------------------------------*/
{
  BMP_HEADER *hd = NULL;
  int c, c1;

  *header = NULL;

  hd = (BMP_HEADER *)calloc((size_t)1, sizeof(BMP_HEADER));
  if (!hd) return OUT_OF_MEMORY;

  /* figure out the file size */
  fseek(fp, 0L, SEEK_END);
  hd->biFilesize = ftell(fp);
  fseek(fp, 0L, 0);

  /* read the file type (first two bytes) */
  c  = getc(fp);
  c1 = getc(fp);
  if (c != 'B' || c1 != 'M') {
    free(hd);
    return UNSUPPORTED_BMP_FORMAT;
  }

  hd->bfSize = getint(fp);

  /* reserved and ignored */
  getshort(fp);
  getshort(fp);

  hd->bfOffBits = getint(fp);
  hd->biSize    = getint(fp);

  if (hd->biSize == BMP_WIN_NEW || hd->biSize == BMP_OS2_NEW) {
    hd->biWidth         = getint(fp);
    hd->biHeight        = getint(fp);
    hd->biPlanes        = getshort(fp);
    hd->biBitCount      = getshort(fp);
    hd->biCompression   = getint(fp);
    hd->biSizeImage     = getint(fp);
    hd->biXPelsPerMeter = getint(fp);
    hd->biYPelsPerMeter = getint(fp);
    hd->biClrUsed       = getint(fp);
    hd->biClrImportant  = getint(fp);
  } else /* old bitmap format */
  {
    hd->biWidth    = getshort(fp);
    hd->biHeight   = getshort(fp);
    hd->biPlanes   = getshort(fp);
    hd->biBitCount = getshort(fp);

    /* Not in old versions so have to compute them */
    hd->biSizeImage =
        (((hd->biPlanes * hd->biBitCount * hd->biWidth) + 31) / 32) * 4 *
        hd->biHeight;

    hd->biCompression   = BMP_BI_RGB;
    hd->biXPelsPerMeter = 0;
    hd->biYPelsPerMeter = 0;
    hd->biClrUsed       = 0;
    hd->biClrImportant  = 0;
  }

  if (BMP_DEBUG) {
    printf("\nLoadBMP:\tbfSize=%u, bfOffBits=%u\n", hd->bfSize, hd->bfOffBits);
    printf("\t\tbiSize=%u, biWidth=%u, biHeight=%u, biPlanes=%u\n", hd->biSize,
           hd->biWidth, hd->biHeight, hd->biPlanes);
    printf("\t\tbiBitCount=%u, biCompression=%u, biSizeImage=%u\n",
           hd->biBitCount, hd->biCompression, hd->biSizeImage);
    printf("\t\tbiX,YPelsPerMeter=%u,%u  biClrUsed=%u, biClrImp=%u\n",
           hd->biXPelsPerMeter, hd->biYPelsPerMeter, hd->biClrUsed,
           hd->biClrImportant);
  }

  if (BMP_FERROR(fp)) {
    free(hd);
    return UNEXPECTED_EOF;
  }

  *header = hd;
  return OK;
}

/*---------------------------------------------------------------------------*/
void release_bmp_header(BMP_HEADER *hd)
/*---------------------------------------------------------------------------*/
{
  if (hd) free(hd);
}

#ifndef __LIBSIMAGE__

#ifdef CICCIO

/*---------------------------------------------------------------------------*/
int img_read_bmp(const MYSTRING fname, IMAGE **pimg)
/*---------------------------------------------------------------------------*/
{
  return img_read_bmp_generic(fname, BMP_READ_IMAGE, pimg);
}

/*---------------------------------------------------------------------------*/
static int img_read_bmp_generic(const MYSTRING fname, int type, IMAGE **pimg)
/*---------------------------------------------------------------------------*/
{
  int retCode = OK;

  UCHAR r[256], g[256], b[256];
  BMP_HEADER *hd = NULL;
  IMAGE *img     = NULL;

  int rv, c, i;
  LPIXEL *pic;
  FILE *fp;

  *pimg = 0;

  /* returns  'NULL' on unsuccess */

  pic = (LPIXEL *)NULL;

  /* open image file */
  fp = _wfopen(fname, L"rb");
  if (!fp) return CANT_OPEN_FILE;

  /* load up the image header */
  retCode = load_bmp_header(fp, &hd);
  if (retCode != OK) goto ERROR;

  /* error checking */
  if ((hd->biBitCount != 1 && hd->biBitCount != 4 && hd->biBitCount != 8 &&
       hd->biBitCount != 24) ||
      hd->biPlanes != 1 || hd->biCompression > BMP_BI_RLE4) {
    retCode = UNSUPPORTED_BMP_FORMAT;
    goto ERROR;
  }

  /* error checking */
  if (((hd->biBitCount == 1 || hd->biBitCount == 24) &&
       hd->biCompression != BMP_BI_RGB) ||
      (hd->biBitCount == 4 && hd->biCompression == BMP_BI_RLE8) ||
      (hd->biBitCount == 8 && hd->biCompression == BMP_BI_RLE4)) {
    retCode = UNSUPPORTED_BMP_FORMAT;
    goto ERROR;
  }

  img       = new_img();
  img->type = TOONZRGB;

  if (type == BMP_READ_INFO) {
    fclose(fp);
    img->xSBsize = img->xsize = hd->biWidth;
    img->ySBsize = img->ysize = hd->biHeight;
    release_bmp_header(hd);
    *pimg = img;
    return OK;
  }

  allocate_pixmap(img, (int)hd->biWidth, (int)hd->biHeight);

  /* hd->biPad; */
  if (hd->biSize != BMP_WIN_OS2_OLD) {
    /* skip ahead to colormap, using biSize */
    c = hd->biSize - 40; /* 40 bytes read from biSize to biClrImportant */
    for (i    = 0; i < c; i++) getc(fp);
    hd->biPad = hd->bfOffBits - (hd->biSize + 14);
  }

  /* load up colormap, if any */
  if (hd->biBitCount != 24) {
    int i, cmaplen;

    /*cmaplen = (hd->biClrUsed) ? hd->biClrUsed : 1 << hd->biBitCount;*/
    if (hd->biClrUsed)
      cmaplen = hd->biClrUsed;
    else
      cmaplen = 1 << hd->biBitCount;

    for (i = 0; i < cmaplen; i++) {
      b[i] = getc(fp);
      g[i] = getc(fp);
      r[i] = getc(fp);
      if (hd->biSize != BMP_WIN_OS2_OLD) {
        getc(fp);
        hd->biPad -= 4;
      }
    }

    if (BMP_FERROR(fp)) {
      retCode = UNEXPECTED_EOF;
      goto ERROR;
    }

    if (BMP_DEBUG) {
      printf("LoadBMP:  BMP colormap:  (RGB order)\n");
      for (i = 0; i < cmaplen; i++) printf("%02x%02x%02x  ", r[i], g[i], b[i]);
      printf("\n\n");
    }
  }

  if (hd->biSize != BMP_WIN_OS2_OLD) {
    /* Waste any unused bytes between the colour map (if present)
and the start of the actual bitmap data.
*/

    while (hd->biPad > 0) {
      (void)getc(fp);
      hd->biPad--;
    }
  }

  /* create 32 bit image buffer */

  pic = (LPIXEL *)img->buffer;

  /* load up the image */
  switch (hd->biBitCount) {
  case 1:
    rv = loadBMP1(fp, pic, hd->biWidth, hd->biHeight, r, g, b);
    break;
  case 4:
    rv = loadBMP4(fp, pic, hd->biWidth, hd->biHeight, hd->biCompression, r, g,
                  b);
    break;
  case 8:
    rv = loadBMP8(fp, pic, hd->biWidth, hd->biHeight, hd->biCompression, r, g,
                  b);
    break;
  case 24:
    rv = loadBMP24(fp, pic, hd->biWidth, hd->biHeight);
    break;
  default:
    retCode = UNSUPPORTED_BMP_FORMAT;
    goto ERROR;
  }

  if (rv) {
    retCode = UNEXPECTED_EOF;
    goto ERROR;
  }

  fclose(fp);
  release_bmp_header(hd);

  *pimg = img;
  return OK;

ERROR:

  fclose(fp);
  release_bmp_header(hd);

  if (img) {
    TFREE(img->buffer)
    TFREE(img)
  }

  return retCode;
}

#endif

#endif /* __LIBSIMAGE__ */

#ifdef CICCIO
static int img_read_bmp_region(const MYSTRING fname, IMAGE **pimg, int x1,
                               int y1, int x2, int y2, int scale) {
  UCHAR r[256], g[256], b[256] /*  ,*map[3]  */;
  LPIXEL *line   = NULL;
  UINT line_size = 0;
  BMP_HEADER *hd = NULL;
  EXT_INFO_REGION info;
  BMP_SUBTYPE subtype;
  LPIXEL *pic, *appo;
  IMAGE *img = NULL;
  UINT start_offset;
  UINT c, i, j;
  char buf[512];
  FILE *fp;
  UINT pad;
  enum BMP_ERROR_CODE bmp_error = OK;

  /* initialize some variables */
  i = pad = 0;

  /* returns  'NULL' on unsuccess */
  pic = (LPIXEL *)NULL;

  /* open image file */
  fp = _wfopen(fname, L"rb");
  if (!fp) {
    return CANT_OPEN_FILE;
  }

  /* load up the image header */
  load_bmp_header(fp, &hd);
  if (!hd) goto ERROR;

  /* error checking */
  if ((hd->biBitCount != 1 && hd->biBitCount != 4 && hd->biBitCount != 8 &&
       hd->biBitCount != 24) ||
      hd->biPlanes != 1 || hd->biCompression > BMP_BI_RLE4) {
    snprintf(buf, sizeof(buf),
             "Bogus BMP File! (bitCount=%d, Planes=%d, Compression=%d)",
             hd->biBitCount, hd->biPlanes, hd->biCompression);

    bmp_error = UNSUPPORTED_BMP_FORMAT;
    goto ERROR;
  }

  /* error checking */
  if (((hd->biBitCount == 1 || hd->biBitCount == 24) &&
       hd->biCompression != BMP_BI_RGB) ||
      (hd->biBitCount == 4 && hd->biCompression == BMP_BI_RLE8) ||
      (hd->biBitCount == 8 && hd->biCompression == BMP_BI_RLE4)) {
    snprintf(buf, sizeof(buf),
             "Bogus BMP File!  (bitCount=%d, Compression=%d)",
             hd->biBitCount, hd->biCompression);
    bmp_error = UNSUPPORTED_BMP_FORMAT;
    goto ERROR;
  }

  img = new_img();

  img->type = TOONZRGB;

  img->xsize   = hd->biWidth;
  img->ysize   = hd->biHeight;
  img->xSBsize = hd->biWidth;
  img->ySBsize = hd->biHeight;
  img->x_dpi   = (double)(hd->biXPelsPerMeter / 39);
  img->y_dpi   = (double)(hd->biYPelsPerMeter / 39);

  hd->biPad = 0;
  if (hd->biSize != BMP_WIN_OS2_OLD) {
    /* skip ahead to colormap, using biSize */
    c = hd->biSize - 40; /* 40 bytes read from biSize to biClrImportant */
    for (i    = 0; i < c; i++) getc(fp);
    hd->biPad = hd->bfOffBits - (hd->biSize + 14);
  }

  /* load up colormap, if any */
  if (hd->biBitCount != 24) {
    int i, cmaplen;

    /*cmaplen = (hd->biClrUsed) ? hd->biClrUsed : 1 << hd->biBitCount;*/
    if (hd->biClrUsed)
      cmaplen = hd->biClrUsed;
    else
      cmaplen = hd->biBitCount;

    for (i = 0; i < cmaplen; i++) {
      b[i] = getc(fp);
      g[i] = getc(fp);
      r[i] = getc(fp);
      if (hd->biSize != BMP_WIN_OS2_OLD) {
        getc(fp);
        hd->biPad -= 4;
      }
    }

    if (BMP_FERROR(fp)) {
      bmp_error = UNEXPECTED_EOF;
      goto ERROR;
    }

    if (BMP_DEBUG) {
      printf("LoadBMP:  BMP colormap:  (RGB order)\n");
      for (i = 0; i < cmaplen; i++) printf("%02x%02x%02x  ", r[i], g[i], b[i]);
      printf("\n\n");
    }
  }

  if (hd->biSize != BMP_WIN_OS2_OLD) {
    /* Waste any unused bytes between the colour map (if present)
and the start of the actual bitmap data.
*/

    while (hd->biPad > 0) {
      (void)getc(fp);
      hd->biPad--;
    }
  }

  /* get information about the portion of the image to load */
  get_info_region(&info, x1, y1, x2, y2, scale, (int)hd->biWidth,
                  (int)hd->biHeight, TNZ_BOTLEFT);

  /* create 32 bit image buffer */
  if (!allocate_pixmap(img, info.xsize, info.ysize)) {
    bmp_error = OUT_OF_MEMORY;
    goto ERROR;
  }

  start_offset = info.y_offset * info.xsize + info.x_offset;
  pic          = ((LPIXEL *)img->buffer) + start_offset;

  if (line_size < hd->biWidth + 32) {
    line_size = hd->biWidth + 32;
    if (!line)
      TCALLOC((LPIXEL *)line, (size_t)line_size)
    else
      TREALLOC(line, line_size)
  }
  if (!line) {
    bmp_error = OUT_OF_MEMORY;
    goto ERROR;
  }

  switch (hd->biBitCount) {
  case 1:
    pad = ((hd->biWidth + 31) / 32) * 32;
    break;
  case 4:
    pad = ((hd->biWidth + 7) / 8) * 8;
    break;
  case 8:
    pad = ((hd->biWidth + 3) / 4) * 4;
    break;
  case 24:
    pad = (4 - ((hd->biWidth * 3) % 4)) & 0x03;
    break;
  default:
    /* segnala errore ed esci */
    break;
  }

  subtype = bmp_get_colorstyle(img);
  if (subtype == BMP_NONE) goto ERROR;

  if (info.y_offset > 0) info.scanNrow++;
  if (info.x_offset > 0) info.scanNcol++;

  /*  print_info_region(&info);      */

  if (info.startScanRow > 0)
    skip_bmp_lines(fp, hd->biWidth, (unsigned int)(info.startScanRow - 1),
                   (unsigned int)SEEK_CUR, subtype);

  for (i = 0; i < (UINT)info.scanNrow; i++) {
    if (load_lineBMP24(fp, line, hd->biWidth, pad)) goto ERROR;

    for (appo = pic, j = c = 0; j < (UINT)info.scanNcol; j++, c += info.step)
      *appo++ = *(line + info.startScanCol + c);
    pic += info.xsize;

    skip_bmp_lines(fp, hd->biWidth, (unsigned int)(info.step - 1),
                   (unsigned int)SEEK_CUR, subtype);
  }

  /*
if (BMP_FERROR(fp))
{
bmp_error(fname, "File appears truncated.  Winging it.\n");
goto ERROR;
}
*/

  fclose(fp);
  release_bmp_header(hd);
  TFREE(line);
  *pimg = img;
  return OK;

ERROR:

  printf("error: (row=%d)\n", i);

  fclose(fp);
  release_bmp_header(hd);

  if (img) free_img(img);
  TFREE(line);
  return bmp_error;
}

#endif

/*---------------------------------------------------------------------------*/

#ifndef __LIBSIMAGE__
#ifdef CICCIO

static int loadBMP1(FILE *fp, LPIXEL *pic, UINT w, UINT h, UCHAR *r, UCHAR *g,
                    UCHAR *b)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, c, bitnum, padw, rv;
  UCHAR byte;
  LPIXEL *pp;
#ifdef BMP_READ_LINE_BY_LINE
  UCHAR *map[3];

  map[0] = r;
  map[1] = g;
  map[2] = b;
#endif

  rv = c = 0;
  padw   = ((w + 31) / 32) * 32; /* 'w', padded to be a multiple of 32 */

  for (i = 0; i < h; i++) {
#ifdef BMP_READ_LINE_BY_LINE
    pp = pic + (i * w);
    rv = load_lineBMP1(fp, pp, w, padw, map);
    if (rv) break;
#else
    pp = pic + (i * w);
    for (j = bitnum = 0; j < padw; j++, bitnum++) {
      if ((bitnum & 7) == 0) /* read the next byte */
      {
        c      = getc(fp);
        bitnum = 0;
      }
      if (j < w) {
        byte = (c & 0x80) ? 1 : 0;
        c <<= 1;

        pp->r = r[byte];
        pp->g = g[byte];
        pp->b = b[byte];
        pp->m = 255;

        pp++;
      }
    }
    if (BMP_FERROR(fp)) break;
#endif
  }

  if (BMP_FERROR(fp)) rv = 1;

  return rv;
}

#endif
#endif /* __LIBSIMAGE__ */

/*---------------------------------------------------------------------------*/
int load_lineBMP1(FILE *fp, LPIXEL *pic, UINT w, UINT padw, UCHAR **map)
/*---------------------------------------------------------------------------*/
{
  UINT j, c, bitnum;
  UCHAR byte;
  LPIXEL *pp;

  for (c = 0, pp = pic, j = bitnum = 0; j < padw; j++, bitnum++) {
    if ((bitnum & 7) == 0) /* read the next byte */
    {
      c      = getc(fp);
      bitnum = 0;
    }
    if (j < w) {
      byte = (c & 0x80) ? 1 : 0;
      c <<= 1;

      pp->r = map[0][byte];
      pp->g = map[1][byte];
      pp->b = map[2][byte];
      pp->m = 255;

      pp++;
    }
  }

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/
static int skip_rowsBMP1(FILE *fp, UINT w, UINT pad, UINT rows, int whence)
/*---------------------------------------------------------------------------*/
{
  UINT offset = pad * rows;
  UINT i, bitnum;

  for (i = bitnum = 0; i < offset; i++, bitnum++) {
    if ((bitnum & 7) == 0) {
      getc(fp);
      bitnum = 0;
    }
  }

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/

#ifndef __LIBSIMAGE__
#ifdef CICCIO

static int loadBMP4(FILE *fp, LPIXEL *pic, UINT w, UINT h, UINT comp, UCHAR *r,
                    UCHAR *g, UCHAR *b)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, c, c1, x, y, nybnum, padw, rv;
  UCHAR byte;
  LPIXEL *pp;
#ifdef BMP_READ_LINE_BY_LINE
  UCHAR *map[3];

  map[0] = r;
  map[1] = g;
  map[2] = b;
#endif

  rv = 0;
  c = c1 = 0;

  if (comp == BMP_BI_RGB) /* read uncompressed data */
  {
    padw = ((w + 7) / 8) * 8; /* 'w' padded to a multiple of 8pix (32 bits) */
    for (i = 0; i < h; i++) {
      pp = pic + (i * w);
#ifdef BMP_READ_LINE_BY_LINE
      rv = load_lineBMP4(fp, pp, w, padw, map);
      if (rv) break;
#else
      for (j = nybnum = 0; j < padw; j++, nybnum++) {
        if ((nybnum & 1) == 0) /* read next byte */
        {
          c      = getc(fp);
          nybnum = 0;
        }
        if (j < w) {
          byte = (c & 0xf0) >> 4;
          c <<= 4;

          pp->r = r[byte];
          pp->g = g[byte];
          pp->b = b[byte];
          pp->m = 255;

          pp++;
        }
      }
      if (BMP_FERROR(fp)) break;
#endif
    }
  } else if (comp == BMP_BI_RLE4) /* read RLE4 compressed data */
  {
    x = y = 0;
    pp    = pic + x + (y)*w;
    while (y < h) {
#ifdef BMP_READ_LINE_BY_LINE

      rv = load_lineBMPC4(fp, pp, w, y, map);
      if (rv == -1) {
        rv = 1;
        break;
      } else if (rv == -2) {
        rv = 0;
        y++;
        pp = pic + y * w;
      } else if (rv == -3) {
        rv = 0;
        break;
      } else {
        y += (rv / w);
        pp = pic + rv;
        rv = 0;
      }

#else
      c = getc(fp);
      if ((int)c == EOF) {
        rv = 1;
        break;
      }

      if (c) /* encoded mode */
      {
        c1 = getc(fp);
        for (i = 0; i < c; i++, x++, pp++) {
          byte  = (i & 1) ? (c1 & 0x0f) : ((c1 >> 4) & 0x0f);
          pp->r = r[byte];
          pp->g = g[byte];
          pp->b = b[byte];
          pp->m = 255;
        }
      } else /* c==0x00  :  escape codes */
      {
        c = getc(fp);
        if ((int)c == EOF) {
          rv = 1;
          break;
        }

        if (c == 0x00) /* end of line */
        {
          x = 0;
          y++;
          if (y < h) pp = pic + x + (y)*w;
        } else if (c == 0x01)
          break;            /* end of  pic */
        else if (c == 0x02) /* delta */
        {
          c = getc(fp);
          x += c;
          c = getc(fp);
          y += c;
          if (y < h) pp = pic + x + (y)*w;
        } else /* absolute mode */
        {
          for (i = 0; i < c; i++, x++, pp++) {
            if ((i & 1) == 0) c1 = getc(fp);
            byte                 = (i & 1) ? (c1 & 0x0f) : ((c1 >> 4) & 0x0f);
            pp->r                = r[byte];
            pp->g                = g[byte];
            pp->b                = b[byte];
            pp->m                = 255;
          }
          if (((c & 3) == 1) || ((c & 3) == 2)) /* read pad byte */
            getc(fp);
        }
      }
      if (BMP_FERROR(fp)) break;
#endif
    }
  } else {
    return 1;
  }

  if (BMP_FERROR(fp)) rv = 1;

  return rv;
}

#endif
#endif /* __LIBSIMAGE__ */

/*---------------------------------------------------------------------------*/
int load_lineBMP4(FILE *fp, LPIXEL *pic, UINT w, UINT padw, UCHAR **map)
/*---------------------------------------------------------------------------*/
{
  UINT nybnum, j, c;
  UCHAR byte;
  LPIXEL *pp;

  for (c = 0, pp = pic, j = nybnum = 0; j < padw; j++, nybnum++) {
    if ((nybnum & 1) == 0) /* read next byte */
    {
      c      = getc(fp);
      nybnum = 0;
    }
    if (j < w) {
      byte = (c & 0xf0) >> 4;
      c <<= 4;

      pp->r = map[0][byte];
      pp->g = map[1][byte];
      pp->b = map[2][byte];
      pp->m = 255;

      pp++;
    }
  }

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/
static int skip_rowsBMP4(FILE *fp, UINT w, UINT pad, UINT rows, int whence)
/*---------------------------------------------------------------------------*/
{
  UINT offset = pad * rows;
  UINT i, nybnum;

  for (i = nybnum = 0; i < offset; i++, nybnum++) {
    if ((nybnum & 1) == 0) {
      getc(fp);
      nybnum = 0;
    }
  }

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/
int load_lineBMPC4(FILE *fp, LPIXEL *pic, UINT w, UINT y, UCHAR **map)
/*---------------------------------------------------------------------------*/
{
  UINT i, c, c1, x;
  UCHAR byte;
  LPIXEL *pp;

  /*
*  Codici di ritorno:
*
*     -1:   incontrata la file del file       (EOF)
*     -2:   incontrata la fine della linea    (Escape code 0x00 0x00)
*     -3:   incontrata la fine dell' immagine (Escape code 0x00 0x01)
*    altro:   incontrato un delta               (Escape code 0x00 0x02)
*/

  /* initialize some variables */
  x  = 0;
  pp = pic;
  c = c1 = 0;

  while (1) {
    c = getc(fp);
    if ((int)c == EOF) return -1;
    if (c) { /* encoded mode */
      c1 = getc(fp);
      for (i = 0; i < c; i++, x++, pp++) {
        byte  = (i & 1) ? (c1 & 0x0f) : ((c1 >> 4) & 0x0f);
        pp->r = map[0][byte];
        pp->g = map[1][byte];
        pp->b = map[2][byte];
        pp->m = 255;
      }
    } else /* c==0x00  :  escape codes */
    {
      c = getc(fp);
      if ((int)c == EOF) return -1;
      if (c == 0x00) /* end of line */
        return -2;
      else if (c == 0x01) /* end of pic */
        return -3;
      else if (c == 0x02) /* delta */
      {
        c = getc(fp);
        x += c;
        c = getc(fp);
        y += c;

        return (x + y * w);
      } else /* absolute mode */
      {
        for (i = 0; i < c; i++, x++, pp++) {
          if ((i & 1) == 0) c1 = getc(fp);
          byte                 = (i & 1) ? (c1 & 0x0f) : ((c1 >> 4) & 0x0f);
          pp->r                = map[0][byte];
          pp->g                = map[1][byte];
          pp->b                = map[2][byte];
          pp->m                = 255;
        }
        if (((c & 3) == 1) || ((c & 3) == 2)) /* read pad byte */
          getc(fp);
      }
    }
    if (BMP_FERROR(fp)) break;
  }

  return -1;
}

/*---------------------------------------------------------------------------*/
static int skip_rowsBMPC4(FILE *fp, UINT rows)
/*---------------------------------------------------------------------------*/
{
  UINT i, c, c1, rv = 0;

  while (rows > 0) {
    c = getc(fp);
    switch (c) {
    case 0x00:
      c = getc(fp);
      switch (c) {
      case 0x00:
        rows--;
        break;
      case 0x01:
        rows = 0;
        break;
      case 0x02:
        c1 = getc(fp); /* x buffer offset */
        c1 = getc(fp); /* y buffer offset */
        rows -= c1;
        break;
      default:
        for (i = 0; i < c; i++) {
          if ((i & 1) == 0) getc(fp);
        }
        if (((c & 3) == 1) || ((c & 3) == 2)) getc(fp);
        break;
      }
      break;
    default:
      c1 = getc(fp);
    }
  }

  if (BMP_FERROR(fp)) rv = 1;

  return rv;
}

/*---------------------------------------------------------------------------*/

#ifndef __LIBSIMAGE__
#ifdef CICCIO

static int loadBMP8(FILE *fp, LPIXEL *pic, UINT w, UINT h, UINT comp, UCHAR *r,
                    UCHAR *g, UCHAR *b)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, c, c1, padw, x, y, rv;

  LPIXEL *pp;
#ifdef BMP_READ_LINE_BY_LINE
  UCHAR *map[3];

  map[0] = r;
  map[1] = g;
  map[2] = b;
#endif

  rv = 0;

  if (comp == BMP_BI_RGB) /* read uncompressed data */
  {
    padw = ((w + 3) / 4) * 4; /* 'w' padded to a multiple of 4pix (32 bits) */
    for (i = 0; i < h; i++) {
#ifdef BMP_READ_LINE_BY_LINE
      pp = pic + (i * w);
      rv = load_lineBMP8(fp, pp, w, padw, map);
      if (rv) break;
#else
      pp = pic + (i * w);
      for (j = 0; j < padw; j++) {
        c                     = getc(fp);
        if ((int)c == EOF) rv = 1;
        if (j < w) {
          pp->r = r[c];
          pp->g = g[c];
          pp->b = b[c];
          pp->m = 255;

          pp++;
        }
      }
      if (BMP_FERROR(fp)) break;
#endif
    }
  } else if (comp == BMP_BI_RLE8) /* read RLE8 compressed data */
  {
    x = y = 0;
    pp    = pic + x + y * w;
    while (y < h) {
#ifdef BMP_READ_LINE_BY_LINE

      rv = load_lineBMPC8(fp, pp, w, y, map);
      if (rv == -1) {
        rv = 1;
        break;
      } else if (rv == -2) {
        rv = 0;
        y++;
        pp = pic + y * w;
      } else if (rv == -3) {
        rv = 0;
        break;
      } else {
        y += (rv / w);
        pp = pic + rv;
        rv = 0;
      }

#else
      c = getc(fp);
      if ((int)c == EOF) {
        rv = 1;
        break;
      }

      if (c) { /* encoded mode */
        c1 = getc(fp);
        for (i = 0; i < c; i++, x++, pp++) {
          pp->r = r[c1];
          pp->g = g[c1];
          pp->b = b[c1];
          pp->m = 255;
        }
      } else /* c==0x00  :  escape codes */
      {
        c = getc(fp);
        if ((int)c == EOF) {
          rv = 1;
          break;
        }

        if (c == 0x00) /* end of line */
        {
          x = 0;
          y++;
          pp = pic + x + y * w;
        } else if (c == 0x01)
          break;            /* end of pic */
        else if (c == 0x02) /* delta */
        {
          c = getc(fp);
          x += c;
          c = getc(fp);
          y += c;
          pp = pic + x + y * w;
        } else /* absolute mode */
        {
          for (i = 0; i < c; i++, x++, pp++) {
            c1 = getc(fp);

            pp->r = r[c1];
            pp->g = g[c1];
            pp->b = b[c1];
            pp->m = 255;
          }
          if (c & 1) /* odd length run: read an extra pad byte */
            getc(fp);
        }
      }
      if (BMP_FERROR(fp)) break;
#endif
    }
  } else {
    return 1;
  }

  if (BMP_FERROR(fp)) rv = 1;

  return rv;
}

#endif
#endif /* __LIBSIMAGE__ */

/*---------------------------------------------------------------------------*/
int load_lineBMP8(FILE *fp, LPIXEL *pic, UINT w, UINT padw, UCHAR **map)
/*---------------------------------------------------------------------------*/
{
  UINT j, c, rv = 0;
  LPIXEL *pp;

  for (pp = pic, j = 0; j < padw; j++) {
    c = getc(fp);
    if ((int)c == EOF) {
      rv = 1;
      break;
    }
    if (j < w) {
      pp->r = map[0][c];
      pp->g = map[1][c];
      pp->b = map[2][c];
      pp->m = 255;

      pp++;
    }
  }
  if (BMP_FERROR(fp)) rv = 1;

  return rv;
}

/*---------------------------------------------------------------------------*/
static int skip_rowsBMP8(FILE *fp, UINT w, UINT pad, UINT rows, int whence)
/*---------------------------------------------------------------------------*/
{
  UINT offset = pad * rows;

  fseek(fp, (long)offset, whence);

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/
int load_lineBMPC8(FILE *fp, LPIXEL *pic, UINT w, UINT y, UCHAR **map)
/*---------------------------------------------------------------------------*/
{
  int i, c, c1, x;
  LPIXEL *pp;

  /*
*  Codici di ritorno:
*
*     -1:   incontrata la file del file       (EOF)
*     -2:   incontrata la fine della linea    (Escape code 0x00 0x00)
*     -3:   incontrata la fine dell' immagine (Escape code 0x00 0x01)
*  altro:   incontrato un delta               (Escape code 0x00 0x02)
*/

  x  = 0;
  pp = pic;

  while (1) {
    c = getc(fp);
    if (c == EOF) return -1;
    if (c) { /* encoded mode */
      c1 = getc(fp);
      for (i = 0; i < c; i++, x++, pp++) {
        pp->r = map[0][c1];
        pp->g = map[1][c1];
        pp->b = map[2][c1];
        pp->m = 255;
      }
    } else /* c==0x00  :  escape codes */
    {
      c = getc(fp);
      if (c == EOF) return -1;
      if (c == 0x00) /* end of line */
        return -2;
      else if (c == 0x01) /* end of pic */
        return -3;
      else if (c == 0x02) /* delta */
      {
        c = getc(fp);
        x += c;
        c = getc(fp);
        y += c;

        return (x + y * w);
      } else /* absolute mode */
      {
        for (i = 0; i < c; i++, x++, pp++) {
          c1 = getc(fp);

          pp->r = map[0][c1];
          pp->g = map[1][c1];
          pp->b = map[2][c1];
          pp->m = 255;
        }
        if (c & 1) /* odd length run: read an extra pad byte */
          getc(fp);
      }
    }
  }
}

/*---------------------------------------------------------------------------*/
static int skip_rowsBMPC8(FILE *fp, UINT rows)
/*---------------------------------------------------------------------------*/
{
  int i, c, c1, rv = 0;

  while (rows > 0) {
    c = getc(fp);
    switch (c) {
    case 0x00:
      c = getc(fp);
      switch (c) {
      case 0x00:
        rows--;
        break;
      case 0x01:
        rows = 0;
        break;
      case 0x02:
        c1 = getc(fp); /* x buffer offset */
        c1 = getc(fp); /* y buffer offset */
        rows -= c1;
        break;
      default:
        for (i = 0; i < c; i++) getc(fp);
        if (c & 1) getc(fp);
        break;
      }
      break;
    default:
      c1 = getc(fp);
    }
  }

  if (BMP_FERROR(fp)) rv = 1;

  return rv;
}

/*---------------------------------------------------------------------------*/

#ifndef __LIBSIMAGE__
#ifdef CICCIO

static int loadBMP24(FILE *fp, LPIXEL *pic, UINT w, UINT h)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, padb, rv;
  LPIXEL *pp;

  rv = 0;

  padb = (4 - ((w * 3) % 4)) & 0x03; /* # of pad bytes to read at EOscanline */

  for (i = 0; i < h; i++) {
#ifdef BMP_READ_LINE_BY_LINE
    pp = pic + i * w;
    rv = load_lineBMP24(fp, pp, w, padb);
#else
    for (pp = pic + i * w, j = 0; j < w; j++, pp++) {
      pp->b = getc(fp); /* blue  */
      pp->g = getc(fp); /* green */
      pp->r = getc(fp); /* red   */
      pp->m = 255;
    }
    for (j = 0; j < padb; j++) getc(fp);

    rv = (BMP_FERROR(fp));
#endif
    if (rv) break;
  }

  return rv;
}

#endif
#endif /* __LIBSIMAGE__ */

/*---------------------------------------------------------------------------*/
int load_lineBMP24(FILE *fp, LPIXEL *pic, UINT w, UINT padb)
/*---------------------------------------------------------------------------*/
{
  LPIXEL *pp;
  UINT j;

  for (pp = pic, j = 0; j < w; j++, pp++) {
    pp->b = getc(fp); /* blue  */
    pp->g = getc(fp); /* green */
    pp->r = getc(fp); /* red   */
    pp->m = 255;
  }
  for (j = 0; j < padb; j++) getc(fp);

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/
static int skip_rowsBMP24(FILE *fp, UINT w, UINT pad, UINT rows, int whence)
/*---------------------------------------------------------------------------*/
{
  UINT offset = (w * 3 + pad) * rows;

  fseek(fp, (long)offset, whence);

  return (BMP_FERROR(fp));
}

/*---------------------------------------------------------------------------*/
/*-- BMP WRITE --------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
BMP_SUBTYPE bmp_get_colorstyle(IMAGE *img)
/*---------------------------------------------------------------------------*/
{
  return img->type;
}

/*---------------------------------------------------------------------------*/
int write_bmp_header(FILE *fp, BMP_HEADER *hd)
/*---------------------------------------------------------------------------*/
{
  putc('B', fp);
  putc('M', fp); /* BMP file magic number           */

  putint(fp, (int)hd->bfSize);
  putshort(fp, 0); /* reserved1                       */
  putshort(fp, 0); /* reserved2                       */

  putint(fp, (int)hd->bfOffBits); /* offset from BOfile to BObitmap */

  putint(fp, (int)hd->biSize);       /* size of bitmap info header     */
  putint(fp, (int)hd->biWidth);      /* width                          */
  putint(fp, (int)hd->biHeight);     /* height                         */
  putshort(fp, (int)hd->biPlanes);   /* must be '1'                    */
  putshort(fp, (int)hd->biBitCount); /* 1,4,8, or 24                   */
  putint(fp,
         (int)hd->biCompression);   /* BMP_BI_RGB, BMP_BI_RLE8 or BMP_BI_RLE4 */
  putint(fp, (int)hd->biSizeImage); /* size of raw image data         */
  putint(fp, (int)hd->biXPelsPerMeter); /* dpi * 39" per meter            */
  putint(fp, (int)hd->biYPelsPerMeter); /* dpi * 39" per meter            */
  putint(fp, (int)hd->biClrUsed);       /* colors used in cmap            */
  putint(fp, (int)hd->biClrImportant);  /* same as above                  */

  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
int write_bmp_palette(FILE *fp, int nc, UCHAR *b, UCHAR *g, UCHAR *r)
/*---------------------------------------------------------------------------*/
{
  int i;

  for (i = 0; i < nc; i++) {
    putc(b[i], fp);
    putc(g[i], fp);
    putc(r[i], fp);
    putc(0, fp);
  }

  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

#ifndef __LIBSIMAGE__

/*---------------------------------------------------------------------------*/
static int img_write_bmp(const MYSTRING fname, IMAGE *img)
/*---------------------------------------------------------------------------*/
{
  int (*write_function)(FILE * fp, UCHAR * pic, UINT w, UINT h, UCHAR * map);
  int h, w, i, nc, nbits, bperlin, comp;
  UCHAR val;
  UCHAR pc2nc[256], r1[256], g1[256], b1[256];
  UCHAR *pic, *graypic;
  BMP_SUBTYPE subtype;
  BMP_HEADER hd;
  FILE *fp;

  subtype = bmp_get_colorstyle(img);
  if (subtype == BMP_NONE) return UNSUPPORTED_BMP_FORMAT;

  fp = _wfopen(fname, L"wb");
  if (!fp) return CANT_OPEN_FILE;

  graypic = NULL;
  nc      = 0;
  nbits   = 0;
  comp    = 0;
  h       = img->ysize;
  w       = img->xsize;
  pic     = (UCHAR *)img->buffer;

  switch (subtype) {
  case BMP_BW:
  case BMP_GREY16:
  case BMP_GREY16C:
  case BMP_CMAPPED256:
  case BMP_CMAPPED256C:
    return UNSUPPORTED_BMP_FORMAT;
  case BMP_GREY256:
  case BMP_GREY256C:
    nbits = 8;
    break;
  case BMP_RGB:
    nbits = 24;
    break;
  default:
    goto BMP_WRITE_ERROR;
  }

  /* number bytes written per line */
  bperlin = ((w * nbits + 31) / 32) * 4;

  /* compute filesize and write it */
  i = 14 +         /* size of bitmap file header */
      40 +         /* size of bitmap info header */
      (nc * 4) +   /* size of colormap */
      bperlin * h; /* size of image data */

  switch (nbits) {
  case 4:
    comp = (comp == TRUE) ? BMP_BI_RLE4 : BMP_BI_RGB;
    break;
  case 8:
    comp = (comp == TRUE) ? BMP_BI_RLE8 : BMP_BI_RGB;
    break;
  default:
    comp = BMP_BI_RGB;
    break;
  }

  /* fill image header */
  hd.bfSize          = i;
  hd.bfOffBits       = 14 + 40 + (nc * 4);
  hd.biSize          = 40;
  hd.biWidth         = w;
  hd.biHeight        = h;
  hd.biPlanes        = 1;
  hd.biBitCount      = nbits;
  hd.biCompression   = comp;
  hd.biSizeImage     = bperlin * h;
  hd.biXPelsPerMeter = 0 * 39;
  hd.biYPelsPerMeter = 0 * 39;
  hd.biClrUsed       = nc;
  hd.biClrImportant  = nc;

  if (!write_bmp_header(fp, &hd)) goto BMP_WRITE_ERROR;

  write_bmp_palette(fp, nc, b1, g1, r1);

  switch (nbits) {
  case 1:
    write_function = writeBMP1;
    break;
  case 4:
    if (comp == BMP_BI_RGB)
      write_function = writeBMP4;
    else
      write_function = writeBMPC4;
    break;
  case 8:
    if (comp == BMP_BI_RGB)
      write_function = writeBMP8;
    else
      write_function = writeBMPC8;
    break;
  case 24:
    write_function = writeBMP24;
    break;
  default:
    goto BMP_WRITE_ERROR;
  }

  /* write out the image */
  val = write_function(fp, pic, w, h, pc2nc);

  if (graypic) free(graypic);
  fclose(fp);

  /* 0 failed , 1 ok */
  return val == 1 ? OK : WRITE_ERROR;

BMP_WRITE_ERROR:

  fclose(fp);
  if (graypic) free(graypic);

  _wremove(fname);

  return WRITE_ERROR;
}

#endif /* __LIBSIMAGE__ */

/*---------------------------------------------------------------------------*/
static int writeBMP1(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, c, bitnum, padw;
  UCHAR *pp;

  padw = ((w + 31) / 32) * 32; /* 'w', padded to be a multiple of 32 */

  for (i = 0; i < h; i++) {
    pp = pic8 + (i * w);
#ifdef BMP_WRITE_LINE_BY_LINE
    if (line_writeBMP1(fp, pp, w, padw, pc2nc) == FALSE) return FALSE;
#else
    for (j = bitnum = c = 0; j <= padw; j++, bitnum++) {
      if (bitnum == 8) /* write the next byte */
      {
        putc((int)c, fp);
        bitnum = c = 0;
      }
      c <<= 1;
      if (j < w) {
        c |= (pc2nc[*pp++] & 0x01);
      }
    }
#endif
  }
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int line_writeBMP1(FILE *fp, UCHAR *pic8, UINT w, UINT padw,
                          UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pp = pic8;
  UINT j, c, bitnum;

  for (j = bitnum = c = 0; j <= padw; j++, bitnum++) {
    if (bitnum == 8) /* write the next byte */
    {
      putc((int)c, fp);
      bitnum = c = 0;
    }
    c <<= 1;
    if (j < w) {
      c |= (pc2nc[*pp++] & 0x01);
    }
  }
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int writeBMP4(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, c, nybnum, padw;
  UCHAR *pp;

  padw = ((w + 7) / 8) * 8; /* 'w' padded to a multiple of 8pix (32 bits) */

  for (i = 0; i < h; i++) {
    pp = pic8 + (i * w);
#ifdef BMP_WRITE_LINE_BY_LINE
    if (line_writeBMP4(fp, pp, w, padw, pc2nc) == FALSE) return FALSE;
#else
    for (j = nybnum = c = 0; j <= padw; j++, nybnum++) {
      if (nybnum == 2) /* write next byte */
      {
        putc((int)(c & 0xff), fp);
        nybnum = c = 0;
      }
      c <<= 4;
      if (j < w) {
        c |= (pc2nc[*pp] & 0x0f);
        pp++;
      }
    }
#endif
  }
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int line_writeBMP4(FILE *fp, UCHAR *pic8, UINT w, UINT padw,
                          UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UINT j, c, nybnum;
  UCHAR *pp = pic8;

  for (j = nybnum = c = 0; j <= padw; j++, nybnum++) {
    if (nybnum == 2) /* write next byte */
    {
      putc((int)(c & 0xff), fp);
      nybnum = c = 0;
    }
    c <<= 4;
    if (j < w) {
      c |= (pc2nc[*pp] & 0x0f);
      pp++;
    }
  }
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int writeBMPC4(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pp1, *pp2, *pp3, byte1, byte2;
  UINT i, cnt;

  for (i = 0; i < h; i++) {
    pp1 = pic8 + i * w;
    pp2 = pp1 + 2;
    pp3 = pp1 + w + 2;

    for (; pp2 < pp3; pp2 += 2) {
      cnt = 2;

      byte1 = ((pc2nc[*pp1] << 4) & 0xf0) | (pc2nc[*(pp1 + 1)] & 0x0f);
      byte2 = ((pc2nc[*pp2] << 4) & 0xf0) | (pc2nc[*(pp2 + 1)] & 0x0f);

      if (byte1 != byte2) {
        putc((int)cnt, fp);
        putc(byte1, fp);
        pp1 = pp2;
      } else {
        while (cnt <= 254 && pp2 < pp3) {
          cnt += 2;
          pp2 += 2;
          byte2 = ((pc2nc[*pp2] << 4) & 0xf0) | (pc2nc[*(pp2 + 1)] & 0x0f);
          if (byte1 != byte2 || cnt >= 254 || pp2 + 2 > pp3) {
            if (pp2 + 2 > pp3) cnt -= 2;
            putc((int)cnt, fp);
            putc(byte1, fp);
            break;
          }
        }
        pp1 = pp2;
      }
    }
    putc(0x00, fp);
    putc(0x00, fp);
    if (BMP_FERROR(fp)) return FALSE;
  }
  putc(0x00, fp);
  putc(0x01, fp);

  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int line_writeBMPC4(FILE *fp, UCHAR *pic8, UINT w, UINT row,
                           UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pp1, *pp2, *pp3, byte1, byte2;
  UINT cnt;

  pp1 = pic8 + row * w;
  pp2 = pp1 + 2;
  pp3 = pp1 + w + 2;

  for (; pp2 < pp3; pp2 += 2) {
    cnt = 2;

    byte1 = ((pc2nc[*pp1] << 4) & 0xf0) | (pc2nc[*(pp1 + 1)] & 0x0f);
    byte2 = ((pc2nc[*pp2] << 4) & 0xf0) | (pc2nc[*(pp2 + 1)] & 0x0f);

    if (byte1 != byte2) {
      putc((int)cnt, fp);
      putc(byte1, fp);
      pp1 = pp2;
    } else {
      while (cnt <= 254 && pp2 < pp3) {
        cnt += 2;
        pp2 += 2;
        byte2 = ((pc2nc[*pp2] << 4) & 0xf0) | (pc2nc[*(pp2 + 1)] & 0x0f);
        if (byte1 != byte2 || cnt >= 254 || pp2 + 2 > pp3) {
          if (pp2 + 2 > pp3) cnt -= 2;
          putc((int)cnt, fp);
          putc(byte1, fp);
          break;
        }
      }
      pp1 = pp2;
    }
  }
  putc(0x00, fp);
  putc(0x00, fp);

  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int writeBMP8(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, padw;
  UCHAR *pp;

  padw = ((w + 3) / 4) * 4; /* 'w' padded to a multiple of 4pix (32 bits) */

  for (i = 0; i < h; i++) {
    pp = pic8 + (i * w);
#ifdef BMP_WRITE_LINE_BY_LINE
    if (line_writeBMP8(fp, pp, w, padw, pc2nc) == FALSE) return FALSE;
#else
    /* for (j=0; j<w; j++) putc(pc2nc[*pp++], fp);*/
    for (j = 0; j < w; j++) {
      putc(*pp, fp);
      pp++;
    }
    for (; j < padw; j++) putc(0, fp);
#endif
  }
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int line_writeBMP8(FILE *fp, UCHAR *pic8, UINT w, UINT padw,
                          UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pp = pic8;
  UINT j;

  for (j = 0; j < w; j++) putc(pc2nc[*pp++], fp);
  for (; j < padw; j++) putc(0, fp);
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int writeBMPC8(FILE *fp, UCHAR *pic8, UINT w, UINT h, UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pp1, *pp2, *pp3, byte1, byte2;
  UINT i, cnt;

  for (i = 0; i < h; i++) {
    pp1 = pic8 + i * w;
    pp2 = pp1 + 1;
    pp3 = pp1 + w + 1;

    for (; pp2 < pp3; pp2++) {
      cnt = 1;

      byte1 = pc2nc[*pp1];
      byte2 = pc2nc[*pp2];

      if (byte1 != byte2) {
        putc((int)cnt, fp);
        putc(byte1, fp);
        pp1 = pp2;
      } else {
        while (cnt <= 254 && pp2 < pp3) {
          cnt++;
          pp2++;
          byte2 = pc2nc[*pp2];
          if (byte1 != byte2 || cnt >= 254 || pp2 + 1 > pp3) {
            if (pp2 + 1 > pp3) cnt--;
            putc((int)cnt, fp);
            putc(byte1, fp);
            break;
          }
        }
        pp1 = pp2;
      }
    }
    putc(0x00, fp);
    putc(0x00, fp);
    if (BMP_FERROR(fp)) return FALSE;
  }
  putc(0x00, fp);
  putc(0x01, fp);

  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int line_writeBMPC8(FILE *fp, UCHAR *pic8, UINT w, UINT row,
                           UCHAR *pc2nc)
/*---------------------------------------------------------------------------*/
{
  UCHAR *pp1, *pp2, *pp3, byte1, byte2;
  UINT cnt;

  pp1 = pic8 + row * w;
  pp2 = pp1 + 1;
  pp3 = pp1 + w + 1;

  for (; pp2 < pp3; pp2++) {
    cnt = 1;

    byte1 = pc2nc[*pp1];
    byte2 = pc2nc[*pp2];

    if (byte1 != byte2) {
      putc((int)cnt, fp);
      putc(byte1, fp);
      pp1 = pp2;
    } else {
      while (cnt <= 254 && pp2 < pp3) {
        cnt++;
        pp2++;
        byte2 = pc2nc[*pp2];
        if (byte1 != byte2 || cnt >= 254 || pp2 + 1 > pp3) {
          if (pp2 + 1 > pp3) cnt--;
          putc((int)cnt, fp);
          putc(byte1, fp);
          break;
        }
      }
      pp1 = pp2;
    }
  }
  putc(0x00, fp);
  putc(0x00, fp);

  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int writeBMP24(FILE *fp, UCHAR *pic24, UINT w, UINT h, UCHAR *whence)
/*---------------------------------------------------------------------------*/
{
  UINT i, j, padb;
  LPIXEL *pixel;
  UCHAR *pp;

  /* pc2nc not used */

  padb = (4 - ((w * 3) % 4)) & 0x03; /* # of pad bytes to write at EOscanline */

  for (i = 0; i < h; i++) {
    pp    = pic24 + (i * w * 4);
    pixel = (LPIXEL *)pp;
#ifdef BMP_WRITE_LINE_BY_LINE
    if (line_writeBMP24(fp, pixel, w, padb) == FALSE) return FALSE;
#else
    for (j = 0; j < w; j++) {
      putc(pixel->b, fp);
      putc(pixel->g, fp);
      putc(pixel->r, fp);

      pixel++;
    }
    for (j = 0; j < padb; j++) putc(0, fp);
#endif
  }
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static int line_writeBMP24(FILE *fp, LPIXEL *pp, UINT w, UINT padb)
/*---------------------------------------------------------------------------*/
{
  UINT j;

  for (j = 0; j < w; j++) {
    putc(pp->b, fp);
    putc(pp->g, fp);
    putc(pp->r, fp);

    pp++;
  }
  for (j = 0; j < padb; j++) putc(0, fp);
  if (BMP_FERROR(fp)) return FALSE;

  return TRUE;
}

#ifndef __LIBSIMAGE__

#ifndef UNUSED_REDUCE_COLORS
/*---------------------------------------------------------------------------*/
static UCHAR *reduce_colors(UCHAR *buffin, int xsize, int ysize, UCHAR *rmap,
                            UCHAR *gmap, UCHAR *bmap, int nc)
/*---------------------------------------------------------------------------*/
{
  LPIXEL *curr_pix, *next_pix, *prev_pix, *buffer;
  static LPIXEL *mbuffer = NULL;
  static UCHAR *ret_buf  = NULL;
  static int outbuf_size = 0;
  static int buffin_size = 0;
  int r1, g1, b1, dim;
  int i, j, tmp;
  int imax, jmax;
  UCHAR *outbuf;
  USHORT val;

  dim = xsize * ysize;

  if (dim > outbuf_size) {
    if (!ret_buf)
      TCALLOC(ret_buf, dim)
    else
      TREALLOC(ret_buf, dim);
    if (!ret_buf) return NULL;
    outbuf_size = dim;
  }

  if (dim > buffin_size) {
    if (!mbuffer)
      TCALLOC(mbuffer, dim)
    else
      TREALLOC(mbuffer, dim);
    if (!ret_buf) return NULL;
    buffin_size = dim;
  }

  memcpy(mbuffer, buffin, dim * sizeof(LPIXEL));
  buffer = mbuffer;
  outbuf = ret_buf;

  imax = ysize - 1;
  jmax = xsize - 1;

  for (i = 0; i < ysize; i++) {
    curr_pix = buffer;
    buffer += xsize;
    next_pix = buffer;
    prev_pix = NIL;

    for (j = 0; j < xsize; j++) {
      r1 = curr_pix->r;
      g1 = curr_pix->g;
      b1 = curr_pix->b;

      val = BMP_REDUCE_COLORS(r1, g1, b1);

      *(outbuf++) = (unsigned char)val;

      /* errors on colors */
      r1 -= rmap[val];
      g1 -= gmap[val];
      b1 -= bmap[val];

      if (j != jmax) BMP_ADD_ERROR(curr_pix[1], 7) /*  RIGHT   */
      if (i != imax)                               /*  UP      */
      {
        BMP_ADD_ERROR(*next_pix, 5)
        if (j > 0) BMP_ADD_ERROR(*prev_pix, 3)       /* UP LEFT  */
        if (j != jmax) BMP_ADD_ERROR(next_pix[1], 1) /* UP RIGHT */
        prev_pix = next_pix;
        next_pix++;
      }
      curr_pix++;
    }
  }

  return ret_buf;
}
#endif

#endif /* __LIBSIMAGE__ */

/*---------------------------------------------------------------------------*/
int make_bmp_palette(int colors, int grey, UCHAR *r, UCHAR *g, UCHAR *b)
/*---------------------------------------------------------------------------*/
{
  int i, j, ind, val;

  switch (colors) {
  case 2:
    for (i = 0; i < 2; i++) r[i] = g[i] = b[i] = i * 255;
    break;
  case 16:
    for (i = 0; i < 16; i++) {
      for (j = 0; j < 16; j++) {
        ind    = i * 16 + j;
        val    = i * 16;
        r[ind] = g[ind] = b[ind] = val;
      }
    }
    break;
  case 256:
    if (grey) {
      for (i = 0; i < 256; i++) r[i] = g[i] = b[i] = i;
    } else {
      for (i = 0; i < 256; i++) {
        r[i] = BMP_RMAP(i);
        g[i] = BMP_GMAP(i);
        b[i] = BMP_BMAP(i);
      }
    }
    break;
  default:
    return FALSE;
  }

  return TRUE;
}

/*---------------------------------------------------------------------------*/
static UINT getshort(FILE *fp)
/*---------------------------------------------------------------------------*/
{
  int c = getc(fp), c1 = getc(fp);

  return ((UINT)c) + (((UINT)c1) << 8);
}

/*---------------------------------------------------------------------------*/
static UINT getint(FILE *fp)
/*---------------------------------------------------------------------------*/
{
  int c = getc(fp), c1 = getc(fp), c2 = getc(fp), c3 = getc(fp);

  return (((UINT)c) << 0) + (((UINT)c1) << 8) + (((UINT)c2) << 16) +
         (((UINT)c3) << 24);
}

/*---------------------------------------------------------------------------*/
static void putshort(FILE *fp, int i)
/*---------------------------------------------------------------------------*/
{
  int c = (((UINT)i)) & 0xff, c1 = (((UINT)i) >> 8) & 0xff;

  putc(c, fp);
  putc(c1, fp);
}

/*---------------------------------------------------------------------------*/
static void putint(FILE *fp, int i)
/*---------------------------------------------------------------------------*/
{
  int c = ((UINT)i) & 0xff, c1 = (((UINT)i) >> 8) & 0xff,
      c2 = (((UINT)i) >> 16) & 0xff, c3 = (((UINT)i) >> 24) & 0xff;

  putc(c, fp);
  putc(c1, fp);
  putc(c2, fp);
  putc(c3, fp);
}

/*---------------------------------------------------------------------------*/

int writebmp(const MYSTRING filename, int xsize, int ysize, void *buffer,
             int bpp) {
  IMAGE img;
  img.xsize  = xsize;
  img.ysize  = ysize;
  img.buffer = buffer;
  switch (bpp) {
  case 8:
    img.type = BMP_GREY256C;
    break;
  case 32:
    img.type = BMP_RGB;
    break;
  }
  return img_write_bmp(filename, &img);
}

/*---------------------------------------------------------------------------*/

#ifdef CICCIO

int readbmp(const MYSTRING filename, int *xsize, int *ysize, void **buffer) {
  IMAGE *img;
  int retCode = img_read_bmp(filename, &img);
  if (retCode != OK) {
    *xsize = *ysize = 0;
    *buffer         = 0;
  } else {
    *xsize      = img->xsize;
    *ysize      = img->ysize;
    *buffer     = img->buffer;
    img->buffer = 0;
    free_img(img);
  }
  return retCode;
}

/*---------------------------------------------------------------------------*/

int readbmpregion(const MYSTRING filename, void **pimg, int x1, int y1, int x2,
                  int y2, int scale) {
  IMAGE *img;

  int retCode = img_read_bmp_region(filename, &img, x1, y1, x2, y2, scale);

  if (retCode != OK) {
    *pimg = 0;
  } else {
    *pimg = img->buffer;
    free(img);
  }
  return retCode;
}

/*---------------------------------------------------------------------------*/

int readbmp_size(const MYSTRING fname, int *lx, int *ly) {
  IMAGE *img;
  int retCode = img_read_bmp_generic(fname, BMP_READ_INFO, &img);
  if (retCode == OK) {
    *lx = img->xsize;
    *ly = img->ysize;
    free(img);
  }
  return retCode;
}

/*---------------------------------------------------------------------------*/

int readbmp_bbox(const MYSTRING fname, int *x0, int *y0, int *x1, int *y1) {
  IMAGE *img;
  int retCode = img_read_bmp_generic(fname, BMP_READ_INFO, &img);
  if (retCode == OK) {
    *x0 = 0;
    *x1 = 0;
    *x1 = img->xsize - 1;
    *y1 = img->ysize - 1;
    free(img);
  }
  return retCode;
}

#endif
