

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#pragma warning(disable : 4996)
#endif

#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "../compatibility/tnz4.h"
#include "../compatibility/inforegion.h"
#include "filequantel.h"
#include "filequantelP.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifdef _WIN32
#define STAT_BUF struct _stat
#else
#define STAT_BUF struct stat
#endif

static IMAGE *img_read_region_quantel_no_interlaced(T_CHAR *fname, int x1,
                                                    int y1, int x2, int y2,
                                                    int scale, int type);
static IMAGE *img_read_region_quantel_interlaced(T_CHAR *fname, int x1, int y1,
                                                 int x2, int y2, int scale,
                                                 int type);

/*static char *Quantel_error[] = { "qnt locked format 720x576",
                                 "qtl locked format 720x486",
                                 "yuv locked format 720x486 / 720x576",
                                 "sdl locked format 720x486 / 720x576" };
*/

#define QUANTEL_BUILD_ROW                                                      \
                                                                               \
  for (x = 0; x < region.scanNcol / 2; x++) {                                  \
    u                   = *lineBuf++ - 128;                                    \
    j                   = *lineBuf++ - 16;                                     \
    if (j < 0) j        = 0;                                                   \
    v                   = *lineBuf++ - 128;                                    \
    k                   = *lineBuf++ - 16;                                     \
    r                   = 76310 * j + 104635 * v;                              \
    if (r > 0xFFFFFF) r = 0xFFFFFF;                                            \
    if (r <= 0xFFFF) r  = 0;                                                   \
    g                   = 76310 * j + -25690 * u + -53294 * v;                 \
    if (g > 0xFFFFFF) g = 0xFFFFFF;                                            \
    if (g <= 0xFFFF) g  = 0;                                                   \
    b                   = 76310 * j + 132278 * u;                              \
    if (b > 0xFFFFFF) b = 0xFFFFFF;                                            \
    if (b <= 0xFFFF) b  = 0;                                                   \
    bufRGB->r           = (unsigned char)(r >> 16);                            \
    bufRGB->g           = (unsigned char)(g >> 16);                            \
    bufRGB->b           = (unsigned char)(b >> 16);                            \
    bufRGB->m           = 255;                                                 \
    bufRGB++;                                                                  \
    r                   = 76310 * k + 104635 * v;                              \
    if (r > 0xFFFFFF) r = 0xFFFFFF;                                            \
    if (r <= 0xFFFF) r  = 0;                                                   \
    g                   = 76310 * k + -25690 * u + -53294 * v;                 \
    if (g > 0xFFFFFF) g = 0xFFFFFF;                                            \
    if (g <= 0xFFFF) g  = 0;                                                   \
    b                   = 76310 * k + 132278 * u;                              \
    if (b > 0xFFFFFF) b = 0xFFFFFF;                                            \
    if (b <= 0xFFFF) b  = 0;                                                   \
    bufRGB->r           = (unsigned char)(r >> 16);                            \
    bufRGB->g           = (unsigned char)(g >> 16);                            \
    bufRGB->b           = (unsigned char)(b >> 16);                            \
    bufRGB->m           = 255;                                                 \
    bufRGB++;                                                                  \
    lineBuf += ((region.step - 1) * 4);                                        \
  }

#define QUANTEL_GET_YSIZE(ysize)                                               \
  (abs((ysize - QNT_PAL_YSIZE)) > abs((ysize - QTL_NTSC_YSIZE)))               \
      ? QTL_NTSC_YSIZE                                                         \
      : QNT_PAL_YSIZE;

/*---------------------------------------------------------------------------*/

static void vpb_string(const char *str, int field_type, char **p_h,
                       char *stop) {
  char *h;
  int len;

  h   = *p_h;
  len = strlen(str);
  NOT_MORE_THAN(255, len)
  if (h + 3 + len >= stop) return;
  *h++ = field_type;
  *h++ = 0;
  *h++ = len;
  strncpy(h, str, (size_t)len);
  h += len;
  *p_h = h;
}

/*---------------------------------------------------------------------------*/

static void vpb_int(int val, char **p_h) {
  char *h;

  h    = *p_h;
  *h++ = (char)(val >> 24);
  *h++ = (char)(val >> 16);
  *h++ = (char)(val >> 8);
  *h++ = (char)(val);
  *p_h = h;
}

/*---------------------------------------------------------------------------*/

static void vpb_short(int val, char **p_h) {
  char *h;

  h    = *p_h;
  *h++ = (char)(val >> 8);
  *h++ = (char)(val);
  *p_h = h;
}

/*---------------------------------------------------------------------------*/

static TBOOL write_vpb_header(FILE *file, int xsize, int ysize) {
  char header[1024], *h, *stop;
  int bytes, sqlx, sqly;

  h    = header;
  stop = header + 1024;
  vpb_string("Picture", 0x03, &h, stop);
  vpb_string("Toonz", 0x97, &h, stop);
  if (ysize == 486) {
    sqlx = 2133; /* 711 * 3   NTSC */
    sqly = 1940; /* 485 * 4        */
  } else {
    sqlx = 2103; /* 701 * 3   PAL  */
    sqly = 2300; /* 575 * 4        */
  }
  bytes = xsize * ysize * 2;
  if (h + 3 + 20 < stop) {
    *h++ = 0x10;
    *h++ = 1;
    *h++ = 20;
    vpb_short(0x10, &h);  /* format code */
    vpb_short(0x00, &h);  /* format code modifier */
    vpb_short(xsize, &h); /* width of picture in pixels */
    vpb_short(ysize, &h); /* height of picture in pixels */
    vpb_short(sqlx, &h);  /* width of arbitrary square in pixels */
    vpb_short(sqly, &h);  /* height of arbitrary square in pixels */
    vpb_int(0, &h);       /* offset from end of 1st K for start of image */
    vpb_int(bytes, &h);   /* length in bytes of image within file */
  }
  *h++ = 0x04;
  *h++ = 1;
  *h++ = 4;
  vpb_int(bytes, &h);
  while (h < stop) *h++ = (char)0xFF;
  return fwrite(header, 1, 1024, file) == 1024;
}

/*---------------------------------------------------------------------------*/

static int quantel_get_info(const T_CHAR *fname, int type, int *xsize,
                            int *ysize) {
  STAT_BUF f_stat;

  if (_wstat(fname, &f_stat) == -1) return FALSE;
  switch (type) {
  case QNT_FORMAT:
    *xsize = QNT_PAL_XSIZE;
    *ysize = QNT_PAL_YSIZE;
    break;
  case QTL_FORMAT:
    *xsize = QTL_NTSC_XSIZE;
    *ysize = QTL_NTSC_YSIZE;
    break;
  case YUV_FORMAT:
    *xsize = QUANTEL_XSIZE;
    switch (f_stat.st_size) {
    case QNT_PAL_FILE_SIZE:
    case QNT_PAL_W_FILE_SIZE:
      *ysize = QNT_PAL_YSIZE;
      break;
    case QTL_NTSC_FILE_SIZE:
    case QTL_NTSC_W_FILE_SIZE:
      *ysize = QTL_NTSC_YSIZE;
      break;
    default:
      *ysize = f_stat.st_size / (QUANTEL_XSIZE * sizeof(short));
      break;
    }
    break;
  case SDL_FORMAT:
    *xsize = QNT_PAL_XSIZE;
    switch (f_stat.st_size) {
    case QNT_PAL_FILE_SIZE:
      *ysize = QNT_PAL_YSIZE;
      break;
    case QTL_NTSC_FILE_SIZE:
      *ysize = QTL_NTSC_YSIZE;
      break;
    default:
      /*printf("error: bad file dimension\n");*/
      return FALSE;
    }
    break;
  default:
    /*printf("error: bad file format\n");*/
    return FALSE;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int vpb_get_info(FILE *file, int *xsize, int *ysize, int *imgoffs) {
  UCHAR buf[1024];
  unsigned int len;

  *xsize = *ysize = *imgoffs = 0;
  while (fread(buf, 1, 3, file) == 3) {
    if (buf[1] > 1) return FALSE;
    if (buf[0] == 0x10) {
      len = 20;
      if (fread(buf, 1, len, file) != len) return FALSE;
      *xsize   = buf[4] << 8 | buf[5];
      *ysize   = buf[6] << 8 | buf[7];
      *imgoffs = buf[12] << 24 | buf[13] << 16 | buf[14] << 8 | buf[15];
      *imgoffs += 1024;
      return TRUE;
    }
    len = buf[2];
    if (fread(buf, 1, len, file) != len) return FALSE;
  }
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int quantel_write_buffer(FILE *outf, UCHAR *buf, int ysize) {
  int n;

  n = fwrite(buf, 1, ysize * BYTESPERROW, outf);
  if (n <= 0) {
    /*printf("quantel_write_frame error: write failed\n");*/
    return FALSE;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static void quantel_rgb_to_yuv(USHORT *rp, USHORT *gp, USHORT *bp, UCHAR *ap) {
  int i, r, g, b;
  TINT32 y1, y2, u, v, u1, u2, v1, v2;

  y1 = y2 = 0;

  ap -= BYTESPERROW - 1;

  for (i = QUANTEL_XSIZE / 2; i > 0; i--) {
    /* first pixel gives Y and 0.5 of chroma */
    r = *rp++;
    g = *gp++;
    b = *bp++;

    y1 = 16829 * r + 33039 * g + 6416 * b;
    u1 = -4831 * r + -9488 * g + 14319 * b;
    v1 = 14322 * r + -11992 * g + -2330 * b;

    /* second pixel gives Y and 0.5 of chroma */
    r = *rp++;
    g = *gp++;
    b = *bp++;

    y2 = 16829 * r + 33039 * g + 6416 * b;
    u2 = -4831 * r + -9488 * g + 14319 * b;
    v2 = 14322 * r + -11992 * g + -2330 * b;

    /* average the chroma */
    u = u1 + u2;
    v = v1 + v2;

    /* round the chroma */
    u1 = (u + 0x008000) >> 16;
    v1 = (v + 0x008000) >> 16;

    /* limit the chroma */
    if (u1 < -112) u1 = -112;
    if (u1 > 111) u1  = 111;
    if (v1 < -112) v1 = -112;
    if (v1 > 111) v1  = 111;

    /* limit the lum */
    if (y1 > 0x00dbffff) y1 = 0x00dbffff;
    if (y2 > 0x00dbffff) y2 = 0x00dbffff;

    /* save the results */
    *ap++ = (u1 + 128);
    *ap++ = (y1 >> 16) + 16;
    *ap++ = (v1 + 128);
    *ap++ = (y2 >> 16) + 16;
  }
}

/*---------------------------------------------------------------------------*/
void img_read_quantel_info(const T_CHAR *fname, int *w, int *h, int type) {
  int xsize, ysize, imgoffs;
  FILE *file;

  *w = 0;
  *h = 0;

  if (type == VPB_FORMAT) {
    file = _wfopen(fname, L"rb");
    if (!file) {
      /*printf("img_read_quantel_info error: unable to open file %s\n",
       * fname);*/
      return;
    }
    if (!vpb_get_info(file, &xsize, &ysize, &imgoffs)) {
      fclose(file);
      return;
    }
    fclose(file);
  } else {
    if (!quantel_get_info(fname, type, &xsize, &ysize)) return;
  }

  *w = xsize;
  *h = ysize;
}

/*---------------------------------------------------------------------------*/

void *img_read_quantel(const T_CHAR *fname, int *w, int *h, int type) {
  FILE *fileyuv;
  IMAGE *image;
  LPIXEL *bufout1, *bufout2, *buf;
  int xsize, ysize, imgoffs, quantel_xsize;
  int y, exit = 0;

  fileyuv = _wfopen(fname, L"rb");
  if (fileyuv == NULL) {
    /*printf("img_read_quantel error: unable to open file %s\n", fname);*/
    return NIL;
  }
  if (type == VPB_FORMAT) {
    if (!vpb_get_info(fileyuv, &xsize, &ysize, &imgoffs)) {
      fclose(fileyuv);
      return NIL;
    }
    fseek(fileyuv, imgoffs, SEEK_SET);
    quantel_xsize = xsize;
  } else {
    if (!quantel_get_info(fname, type, &xsize, &ysize)) {
      fclose(fileyuv);
      return NIL;
    }
    quantel_xsize = QUANTEL_XSIZE;
  }
  image       = new_img();
  image->type = TOONZRGB;
  if (!allocate_pixmap(image, xsize, ysize)) return NIL;
  bufout1 = bufout2 = (LPIXEL *)image->buffer;
  bufout1 += ysize * xsize - 1;
  bufout2 += (ysize - 1) * xsize - 1;

  if (type == SDL_FORMAT) {
    for (y = 0; y < ysize; y += 2) {
      buf = bufout1 - (quantel_xsize - 1);
      QUANTEL_GET_YUV_LINE(fileyuv, buf, quantel_xsize)
      if (exit) break;
      bufout1 -= quantel_xsize * 2;
    }
    for (y = 1; y < ysize; y += 2) {
      buf = bufout2 - (quantel_xsize - 1);
      QUANTEL_GET_YUV_LINE(fileyuv, buf, quantel_xsize)
      if (exit) break;
      bufout2 -= quantel_xsize * 2;
    }
  } else {
    for (y = 0; y < ysize; y++) {
      buf = bufout1 - (quantel_xsize - 1);
      QUANTEL_GET_YUV_LINE(fileyuv, buf, quantel_xsize)
      if (exit) break;
      bufout1 -= quantel_xsize;
    }
  }
  if (ferror(fileyuv) || feof(fileyuv)) {
    fclose(fileyuv);
    return NIL;
  }

  fclose(fileyuv);
  *w = image->xsize;
  *h = image->ysize;
  return image->buffer;
}

/*---------------------------------------------------------------------------*/

int img_write_quantel(const T_CHAR *fname, void *buffer, int w, int h,
                      int type) {
  FILE *outf;
  UCHAR *picbuf, *ap;
  USHORT rbuffer[8192], gbuffer[8192], bbuffer[8192];
  USHORT *rbuf, *gbuf, *bbuf;
  int y, yuv_flag = 0;
  int xmarg, ymarg;
  LPIXEL *RGBbuf, *appo;
  int xsize, true_ysize, max_ysize = 0, ysize, ret, interlace = 0;

  rbuf = (USHORT *)&rbuffer;
  gbuf = (USHORT *)&gbuffer;
  bbuf = (USHORT *)&bbuffer;

  xsize = w;
  ysize = h;

  if (xsize > QUANTEL_XSIZE) {
    /* printf("error: bad X size (%s)\n", Quantel_error[type-1]);*/
    return FALSE;
  } else if (xsize < QUANTEL_XSIZE)
    xmarg = (QUANTEL_XSIZE - xsize) / 2;
  else
    xmarg = 0;

  switch (type) {
  case QNT_FORMAT:
    interlace = FALSE;
    yuv_flag  = 0;
    max_ysize = QNT_PAL_YSIZE;
    break;
  case QTL_FORMAT:
    interlace = FALSE;
    yuv_flag  = 0;
    max_ysize = QTL_NTSC_YSIZE;
    break;
  case YUV_FORMAT:
  case VPB_FORMAT:
    interlace = FALSE;
    yuv_flag  = 0;
    max_ysize = QUANTEL_GET_YSIZE(ysize);
    break;
  case SDL_FORMAT:
    interlace = TRUE;
    yuv_flag  = 0;
    max_ysize = QUANTEL_GET_YSIZE(ysize);
    break;
  default:
    /*printf("error: %d bad file format\n", fname);*/
    return 0;
  }

  if (ysize > max_ysize) {
    /*printf("error: bad Y size (%s)\n", Quantel_error[type-1]);*/
    return FALSE;
  } else if (!yuv_flag && ysize < max_ysize) {
    ymarg      = (max_ysize - ysize) / 2;
    true_ysize = max_ysize;
  } else {
    ymarg      = 0;
    true_ysize = ysize;
  }

  outf = _wfopen(fname, L"wb");
  if (outf == NULL) {
    /*printf("error: unable to open %s for writing\n",  fname);*/
    return FALSE;
  }

  if (type == VPB_FORMAT) {
    write_vpb_header(outf, QUANTEL_XSIZE, true_ysize);
  }

  picbuf = (UCHAR *)malloc(true_ysize * QUANTEL_XSIZE * sizeof(short));
  if (picbuf == NIL) {
    /*printf("img_write_quantel error: out of memory\n");*/
    return 0;
  }
  ap = picbuf + (true_ysize * QUANTEL_XSIZE * sizeof(short)) - 1;

  RGBbuf = (LPIXEL *)buffer;

  if (interlace) {
    appo = RGBbuf;
    for (y = 0; y < true_ysize; y += 2) {
      if (y < ymarg || y > (true_ysize - ymarg - 1))
        QUANTEL_FILL_LINE_OF_BLACK(rbuf, gbuf, bbuf, QUANTEL_XSIZE)
      else {
        QUANTEL_FILL_LINE_OF_RGB(xmarg, xsize, rbuf, gbuf, bbuf, appo)
        appo += xsize;
      }
      quantel_rgb_to_yuv(rbuf, gbuf, bbuf, ap);
      ap -= BYTESPERROW;
    }
    appo = RGBbuf + xsize;
    for (y = 1; y < true_ysize; y += 2) {
      if (y < ymarg || y > (true_ysize - ymarg - 1))
        QUANTEL_FILL_LINE_OF_BLACK(rbuf, gbuf, bbuf, QUANTEL_XSIZE)
      else {
        QUANTEL_FILL_LINE_OF_RGB(xmarg, xsize, rbuf, gbuf, bbuf, appo)
        appo += xsize;
      }
      quantel_rgb_to_yuv(rbuf, gbuf, bbuf, ap);
      ap -= BYTESPERROW;
    }
  } else {
    for (y = 0; y < true_ysize; y++) {
      if (y < ymarg || y > (true_ysize - ymarg - 1))
        QUANTEL_FILL_LINE_OF_BLACK(rbuf, gbuf, bbuf, QUANTEL_XSIZE)
      else
        QUANTEL_FILL_LINE_OF_RGB(xmarg, xsize, rbuf, gbuf, bbuf, RGBbuf)
      quantel_rgb_to_yuv(rbuf, gbuf, bbuf, ap);
      ap -= BYTESPERROW;
    }
  }

  ret = quantel_write_buffer(outf, picbuf, true_ysize);

  if (picbuf) free(picbuf);
  fclose(outf);

  return ret; /*  ??  */
}

/*---------------------------------------------------------------------------*/

static IMAGE *img_read_region_quantel(T_CHAR *fname, int x1, int y1, int x2,
                                      int y2, int scale, int type) {
  if (type == SDL_FORMAT)
    return (img_read_region_quantel_no_interlaced(fname, x1, y1, x2, y2, scale,
                                                  type));
  else
    return (
        img_read_region_quantel_interlaced(fname, x1, y1, x2, y2, scale, type));
}

/*---------------------------------------------------------------------------*/

static IMAGE *img_read_region_quantel_no_interlaced(T_CHAR *fname, int x1,
                                                    int y1, int x2, int y2,
                                                    int scale, int type) {
  int offset, xsize, ysize, imgoffs;
  int rows_field1, rows_field2, ret, step, off_row;
  LPIXEL *bufout1, *bufout2, *buff;
  TINT32 r, g, b, j, k, u, v, x, y;
  unsigned char *linebuf, *head_linebuf = NULL;
  INFO_REGION region;
  IMAGE *image = NULL;
  FILE *fileyuv;

  fileyuv = _wfopen(fname, L"rb");
  if (!fileyuv) return NIL;
  if (type == VPB_FORMAT) {
    if (!vpb_get_info(fileyuv, &xsize, &ysize, &imgoffs)) {
      fclose(fileyuv);
      return NIL;
    }
  } else {
    if (!quantel_get_info(fname, type, &xsize, &ysize)) {
      fclose(fileyuv);
      return NIL;
    }
    imgoffs = 0;
  }
  image       = new_img();
  image->type = TOONZRGB;
  getInfoRegion(&region, x1, y1, x2, y2, scale, xsize, ysize);
  if (!allocate_pixmap(image, region.xsize, region.ysize)) return NIL;

  bufout1 = bufout2 = (LPIXEL *)image->buffer;
  bufout1 += (region.ysize - 1) * region.xsize + region.x_offset;
  bufout2 += (region.ysize - 2) * region.xsize + region.x_offset;

  linebuf = (unsigned char *)calloc(1, BYTESPERROW);
  if (!linebuf) {
    /*
printf("error: out of memory\n");
exit(1);
*/
    return 0;
  }
  if (region.scanNrow & 0x1) /* se dispare */
  {
    rows_field1 = region.scanNrow / 2 + 1;
    rows_field2 = region.scanNrow / 2;
  } else
    rows_field1 = rows_field2 = region.scanNrow / 2;
  offset = (region.ly_in - (region.scanNrow * scale + max(y1, 0))) / 2;
  offset *= BYTESPERROW;

  if (fseek(fileyuv, imgoffs + offset, SEEK_SET)) {
    /* printf("error: seek failed; inconsistent data\n");*/
    goto error;
  }
  for (y = 0; y < rows_field1; y++) {
    ret = fread(linebuf, 1, BYTESPERROW, fileyuv);
    if (!ret) goto end;
    head_linebuf = linebuf;
    off_row      = region.startScanCol * sizeof(TUINT32) / 2;
    if (off_row & 3) off_row += 2;
    linebuf += off_row;
    buff = bufout1;
    for (x = 0; x < region.scanNcol / 2; x++) {
      u                   = *linebuf++ - 128;
      j                   = *linebuf++ - 16;
      if (j < 0) j        = 0;
      v                   = *linebuf++ - 128;
      k                   = *linebuf++ - 16;
      r                   = 76310 * j + 104635 * v;
      if (r > 0xffffff) r = 0xffffff;
      if (r <= 0xffff) r  = 0;
      g                   = 76310 * j + -25690 * u + -53294 * v;
      if (g > 0xffffff) g = 0xffffff;
      if (g <= 0xffff) g  = 0;
      b                   = 76310 * j + 132278 * u;
      if (b > 0xffffff) b = 0xffffff;
      if (b <= 0xffff) b  = 0;
      buff->r             = (unsigned char)(r >> 16);
      buff->g             = (unsigned char)(g >> 16);
      buff->b             = (unsigned char)(b >> 16);
      buff->m             = 255;
      buff++;
      r                   = 76310 * k + 104635 * v;
      if (r > 0xffffff) r = 0xffffff;
      if (r <= 0xffff) r  = 0;
      g                   = 76310 * k + -25690 * u + -53294 * v;
      if (g > 0xffffff) g = 0xffffff;
      if (g <= 0xffff) g  = 0;
      b                   = 76310 * k + 132278 * u;
      if (b > 0xffffff) b = 0xffffff;
      if (b <= 0xffff) b  = 0;
      buff->r             = (unsigned char)(r >> 16);
      buff->g             = (unsigned char)(g >> 16);
      buff->b             = (unsigned char)(b >> 16);
      buff->m             = 255;
      buff++;
      linebuf += (region.step - 1) * sizeof(TUINT32);
    }
    linebuf = head_linebuf;
    if (y * 2 > region.ly_in) break;
    for (step = 1; step < region.step; step++) {
      if (fseek(fileyuv, BYTESPERROW, SEEK_CUR)) {
        printf("error: seek failed; inconsistent data\n");
        goto error;
      }
    }
    bufout1 -= region.xsize * 2;
  }
  offset = (region.ly_in - (region.scanNrow * scale + max(y1, 0))) / 2;
  offset = (region.ly_in / 2 + offset);
  offset *= BYTESPERROW;
  if (fseek(fileyuv, imgoffs + offset, SEEK_SET)) {
    /*printf("error: seek failed; inconsistent data\n");*/
    goto error;
  }
  for (y = 0; y < rows_field2; y++) {
    ret = fread(linebuf, 1, BYTESPERROW, fileyuv);
    if (!ret) goto end;
    head_linebuf = linebuf;
    off_row      = region.startScanCol * sizeof(TUINT32) / 2;
    if (off_row & 3) off_row += 2;
    linebuf += off_row;
    buff = bufout2;
    for (x = 0; x < region.scanNcol / 2; x++) {
      u                   = *linebuf++ - 128;
      j                   = *linebuf++ - 16;
      if (j < 0) j        = 0;
      v                   = *linebuf++ - 128;
      k                   = *linebuf++ - 16;
      r                   = 76310 * j + 104635 * v;
      if (r > 0xffffff) r = 0xffffff;
      if (r <= 0xffff) r  = 0;
      g                   = 76310 * j + -25690 * u + -53294 * v;
      if (g > 0xffffff) g = 0xffffff;
      if (g <= 0xffff) g  = 0;
      b                   = 76310 * j + 132278 * u;
      if (b > 0xffffff) b = 0xffffff;
      if (b <= 0xffff) b  = 0;
      buff->r             = (unsigned char)(r >> 16);
      buff->g             = (unsigned char)(g >> 16);
      buff->b             = (unsigned char)(b >> 16);
      buff->m             = 255;
      buff++;
      r                   = 76310 * k + 104635 * v;
      if (r > 0xffffff) r = 0xffffff;
      if (r <= 0xffff) r  = 0;
      g                   = 76310 * k + -25690 * u + -53294 * v;
      if (g > 0xffffff) g = 0xffffff;
      if (g <= 0xffff) g  = 0;
      b                   = 76310 * k + 132278 * u;
      if (b > 0xffffff) b = 0xffffff;
      if (b <= 0xffff) b  = 0;
      buff->r             = (unsigned char)(r >> 16);
      buff->g             = (unsigned char)(g >> 16);
      buff->b             = (unsigned char)(b >> 16);
      buff->m             = 255;
      buff++;
      linebuf += (region.step - 1) * sizeof(TUINT32);
    }
    linebuf = head_linebuf;
    if (y * 2 + 1 > region.ly_in) break;
    for (step = 1; step < region.step; step++) {
      if (fseek(fileyuv, BYTESPERROW, SEEK_CUR)) {
        printf("error: seek failed; inconsistent data\n");
        goto error;
      }
    }
    bufout2 -= region.xsize * 2;
  }

  if (ferror(fileyuv) || feof(fileyuv)) goto error;

end:

  if (head_linebuf) free(head_linebuf);
  fclose(fileyuv);

  return image;

error:

  if (head_linebuf) free(head_linebuf);
  free_img(image);
  fclose(fileyuv);

  return NIL;
}

/*---------------------------------------------------------------------------*/

static IMAGE *img_read_region_quantel_interlaced(T_CHAR *fname, int x1, int y1,
                                                 int x2, int y2, int scale,
                                                 int type) {
  int offset, rgbbuf_offset, xsize, ysize, imgoffs, ret, step, x, y;
  LPIXEL *bufRGB, *headBufRGB;
  unsigned char *headLineBuf, *lineBuf;
  TINT32 r, g, b, j, k, u, v;
  INFO_REGION region;
  IMAGE *image = NULL;
  FILE *fileyuv;

  fileyuv = _wfopen(fname, L"rb");
  if (!fileyuv) return NIL;
  if (type == VPB_FORMAT) {
    if (!vpb_get_info(fileyuv, &xsize, &ysize, &imgoffs)) {
      fclose(fileyuv);
      return NIL;
    }
  } else {
    if (!quantel_get_info(fname, type, &xsize, &ysize)) {
      fclose(fileyuv);
      return NIL;
    }
    imgoffs = 0;
  }
  image       = new_img();
  image->type = TOONZRGB;
  getInfoRegion(&region, x1, y1, x2, y2, scale, xsize, ysize);
  if (!allocate_pixmap(image, region.xsize, region.ysize)) return NIL;
  rgbbuf_offset = (region.x_offset + (region.xsize * region.y_offset));
  headBufRGB    = (LPIXEL *)image->buffer;
  headLineBuf = lineBuf = (unsigned char *)calloc(1, BYTESPERROW);
  if (!headLineBuf) {
    return 0;
  }
  if (y2 < ysize - 1)
    offset = (ysize - 1 - y2) * BYTESPERROW;
  else
    offset = 0;
  fseek(fileyuv, imgoffs + offset, SEEK_SET);
  for (y = 0; y < region.scanNrow; y++) {
    bufRGB =
        headBufRGB + rgbbuf_offset + (region.xsize * (region.scanNrow - y - 1));
    ret = fread(lineBuf, 1, BYTESPERROW, fileyuv);
    if (!ret) goto end;
    lineBuf += region.startScanCol * sizeof(LPIXEL);
    QUANTEL_BUILD_ROW; /* sta in testa al file */
    if (y >= region.scanNrow - 1) break;
    for (step = 1; step < region.step; step++) {
      if (fseek(fileyuv, BYTESPERROW, SEEK_CUR)) {
        /*printf("error: seek failed; inconsistent data\n");*/
        goto error;
      }
    }
    lineBuf = headLineBuf;
  }

  if (ferror(fileyuv) || feof(fileyuv)) goto error;

end:
  if (headLineBuf) free(headLineBuf);
  fclose(fileyuv);
  return image;

error:
  if (headLineBuf) free(headLineBuf);
  free_img(image);
  fclose(fileyuv);
  return NIL;
}
