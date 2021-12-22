

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tiio_tzp.h"

extern "C" {
#include "../tif/tifimage/tiff.h"
#include "../tif/tifimage/tiffio.h"
}

//#include "toonz.h"
//#include "tmsg.h"
//#include "file.h"
//#include "img.h"
//#include "tiff.h"
//#include "tiffio.h"
//#include "tim.h"
//#include "history.h"
//#include "../infoRegionP.h"
//#include "version.h"
//#include "machine.h"
//#include "tenv.h"
//#include "ImageP/img_security.h"

#define ICON_WIDTH 100
#define ICON_HEIGHT 90

#define TBOOL int
#define FALSE 0
#define TRUE 1

#define UCHAR unsigned char
#define USHORT unsigned short
#define ULONG unsigned long
#define UINT unsigned int

#define CASE                                                                   \
  break;                                                                       \
  case
#define __OR case
#define DEFAULT                                                                \
  break;                                                                       \
  default

#ifndef MAXINT
#define MAXINT ((int)((~0U) >> 1))
#endif

#define NOT_LESS_THAN(A, B)
typedef struct { UCHAR r, g, b, m; } LPIXEL;

#define TMALLOC(A, C) A = malloc(C * sizeof(A[0]));
#define TFREE(A) free(A);
#define NIL 0

typedef struct {
  int x1, y1, x2, y2;
  int x_offset, y_offset;         /*  offset all'interno della regione   */
  int xsize, ysize;               /*      dimensioni della regione       */
  int scanNrow, scanNcol;         /* righe e col. dell'immagine da scan. */
  int startScanRow, startScanCol; /*   offset nell'immagine da scandire  */
  int step;                       /*          fattore di scale           */
  int lx_in, ly_in;               /*    dimensioni immag. da scandire    */
  int verso_x, verso_y;           /* verso di scrittura nel buffer dest. */
  int buf_inc;                    /* incremento tra due pix. consecutivi */
  int sxpix, expix, sypix, eypix; /* pixel estremi del buffer di input   */
} INFO_REGION;

typedef struct {
  /*UCHAR    tone_offs; sempre 0 */
  UCHAR tone_bits;
  UCHAR color_offs;
  UCHAR color_bits;
  UCHAR pencil_offs;
  UCHAR pencil_bits;
  USHORT offset_mask;  // fa allo stesso tempo sia da offset che da maschera
  USHORT default_val;  // da utilizzare, p.es., per pixel fuori dall'immagine
  short n_tones;
  short n_colors;
  short n_pencils;
} TCM_INFO;

static const TCM_INFO Tcm_old_default_info = {
    /*0,*/ 4, 4, 5, 9, 2, 0x0800, 0x080f, 16, 32, 4};
static const TCM_INFO Tcm_new_default_info = {
    /*0,*/ 4, 4, 7, 11, 5, 0x0000, 0x000f, 16, 128, 32};
static const TCM_INFO Tcm_24_default_info = {
    /*0,*/ 8, 8, 8, 16, 8, 0x0000, 0x00ff, 256, 256, 256};

#define TCM_TONE_MASK(TCM) ((1U << (TCM).tone_bits) - 1U)
#define TCM_COLOR_MASK(TCM)                                                    \
  (((1U << (TCM).color_bits) - 1U) << (TCM).color_offs)
#define TCM_PENCIL_MASK(TCM)                                                   \
  (((1U << (TCM).pencil_bits) - 1U) << (TCM).pencil_offs)

#define TCM_COLOR_INDEX(TCM, ID)                                               \
  ((ID) << (TCM).color_offs | ((TCM).n_tones - 1) | (TCM).offset_mask)
#define TCM_PENCIL_INDEX(TCM, ID)                                              \
  ((ID) << (TCM).pencil_offs | (TCM).offset_mask)

#define TCM_INDEX_IS_COLOR_ONLY(TCM, INDEX)                                    \
  (((INDEX)&TCM_TONE_MASK(TCM)) == TCM_TONE_MASK(TCM))
#define TCM_INDEX_IS_PENCIL_ONLY(TCM, INDEX) (((INDEX)&TCM_TONE_MASK(TCM)) == 0)

#define TCM_COLOR_ID(TCM, INDEX)                                               \
  ((int)(((INDEX) >> (TCM).color_offs) & ((1U << (TCM).color_bits) - 1U)))
#define TCM_PENCIL_ID(TCM, INDEX)                                              \
  ((int)(((INDEX) >> (TCM).pencil_offs) & ((1U << (TCM).pencil_bits) - 1U)))

#define TCM_MIN_CMAP_BUFFER_SIZE(TCM)                                          \
  ((((TCM).n_pencils - 1) << (TCM).pencil_offs |                               \
    ((TCM).n_colors - 1) << (TCM).color_offs | (TCM).n_tones - 1) +            \
   1)

#define TCM_MIN_CMAP_COLBUFFER_SIZE(TCM) ((TCM).n_colors * (TCM).n_tones)

#define TCM_MIN_CMAP_PENBUFFER_SIZE(TCM) ((TCM).n_pencils * (TCM).n_tones)

#define TCM_CMAP_BUFFER_SIZE(TCM)                                              \
  (1 << ((TCM).pencil_bits + (TCM).color_bits + (TCM).tone_bits))

#define TCM_CMAP_COLBUFFER_SIZE(TCM) (1 << ((TCM).color_bits + (TCM).tone_bits))

#define TCM_CMAP_PENBUFFER_SIZE(TCM)                                           \
  (1 << ((TCM).pencil_bits + (TCM).tone_bits))

static int Next_img_read_with_extra = FALSE;
static int Read_with_extra          = FALSE;
#define SET_READ_WITH_EXTRA                                                    \
  {                                                                            \
    Read_with_extra          = Next_img_read_with_extra;                       \
    Next_img_read_with_extra = FALSE;                                          \
  }

typedef struct {
  USHORT bits_per_sample, samples_per_pixel, photometric;
  int xsize, ysize, xSBsize, ySBsize, x0, y0;
  double x_dpi, y_dpi;
  double h_pos;  // in pixel, mentre TIFFTAG_XPOSITION e' in RESUNITS
  UCHAR extra_mask;
  TBOOL edu_file;
} TZUP_FIELDS;

// Tipi di  IMAGE
enum img_type {
  IMG_NONE,
  CMAPPED,   /* fa riferimento a una color-map */
  CMAPPED24, /* 3 x 8 bit (ink, paint, ramp) + 8 bit extra (nel MSB) */
  RGB,       /* ogni pixel e' una terna red-green-blue   - 8bit per canale */
  RGB64,     /* ogni pixel e' una terna red-green-blue - 16bit per canale */
  GR8,       /* a toni di grigio */
  CMAP       /* color map */
};

/* pixel-map */
struct s_pixmap {
  USHORT *buffer;
  UCHAR *extra;     /* patches etc. */
  UCHAR extra_mask; /* bits extra usati in buffer (CMAPPED24) o in extra */
  int xsize, ysize;
  int xSBsize, ySBsize, xD, yD; /* savebox */
  double x_dpi, y_dpi;
  double h_pos; /* in pixel */
};

/* color-map */
struct s_cmap {
  char *name;        /* riferimento a CMAP esterna */
  LPIXEL *buffer;    /* buffer della colormap (premoltiplicata) */
  LPIXEL *penbuffer; /* buffer della componente di pencil per > 16 bits */
  LPIXEL *colbuffer; /* buffer della componente di color  per > 16 bits */
  LPIXEL *pencil;    /* i colori non premoltiplicati */
  LPIXEL *color;     /* tanti quanto dicono info.n_pencils e n_colors */
  TCM_INFO info;     /* vedi tcm.h */
  void *names;       /* nomi dei colori, gestiti da colorsdb */

  /* seguono i campi che andrebbero eliminati perche' doppioni */
  int offset;            /* == info.offset_mask */
  int pencil_n, color_n; /* == info.n_pencils e n_colors */
};

/* Enum per l'identificazione dell' algoritmo di riduzione dei colori */

typedef enum { CM_NONE = -1, CM_STANDARD, CM_CUSTOM } IMG_CM_ALGORITHM;

/* Struttura per le modalita' di scrittura dei formati di file supportati  */

typedef struct {
  char rgb_is_compressed;
  char rgb_write_matte;
  char rgb_64_bits;
  char rgb_colorstyle; /* full color or greyscale */
  char tga_bytes_per_pixel;
  char tga_is_colormap;
  char tga_is_compressed;

  /*
* Microsoft Windows Bitmap (BMP and DIB)
*
*                         | compression | colorstyle | numcolors |
* ---------------------------------------------------------------
*       BLack & White     |      0      |     0      |     2     |
* ----------------------------------------------------------------
*    16 Grey Tones        |      0      |    GR8     |     16    |
* ---------------------------------------------------------------
*    16 Grey Tones Comp.  |      1      |    GR8     |     16    |
* ---------------------------------------------------------------
*   256 Grey Tones        |      0      |    GR8     |     256   |
* ---------------------------------------------------------------
*   256 Grey Tones Comp.  |      1      |    GR8     |     256   |
* ---------------------------------------------------------------
*    16 Color Mapped      |      0      |  CMAPPED   |     16    |
* ---------------------------------------------------------------
*    16 Color Mapped Comp.|      1      |  CMAPPED   |     16    |
* ---------------------------------------------------------------
*   256 Color Mapped      |      0      |  CMAPPED   |     256   |
* ---------------------------------------------------------------
*   256 Color Mapped Comp.|      1      |  CMAPPED   |     256   |
* ---------------------------------------------------------------
*       Full Color        |      0      |    RGB     |     0     |
* ---------------------------------------------------------------
*
*/
  unsigned short bmp_compression;
  unsigned short bmp_colorstyle;
  unsigned short bmp_numcolors;

  /*---------------------------------------------------------------*/

  unsigned int jpg_quality;
  unsigned int jpg_smoothing;
  unsigned int jpg_components;

  /*---------------------------------------------------------------*/

  unsigned short tif_compression;
  unsigned short tif_orientation;
  unsigned short tif_photometric;
  unsigned short tif_bits_per_sample;
  unsigned short tif_samples_per_pixel;

  /*---------------------------------------------------------------*/

  IMG_CM_ALGORITHM cm_algorithm;

  /*---------------------------------------------------------------*/

  /*TNZMOVIE_QUALITY      pct_quality;
TNZMOVIE_COMPRESSION  pct_compression;
*/
  /*---------------------------------------------------------------*/
  /*
TNZMOVIE_QUALITY     mov_quality;
TNZMOVIE_COMPRESSION mov_compression;
*/
  int avi_bpp;
  char *avi_compression;
} IMG_IO_SETTINGS;

/* Struttura IMAGE */
typedef struct s_image {
  enum img_type type;
  char *filename;
  char *history;
  struct s_pixmap icon;   /* Icon */
  struct s_cmap cmap;     /* Colormap */
  struct s_pixmap pixmap; /* Pixel Map */
  IMG_IO_SETTINGS io_settings;
} IMAGE;

extern int Silent_tiff_print_error;
extern int Tiff_ignore_missing_internal_colormap;

static int write_rgb_image(IMAGE *image, TIFF *tif),
    write_cmapped_image(IMAGE *image, TIFF *tif),
    write_cmapped24_image(IMAGE *image, TIFF *tif),
    write_extra(IMAGE *image, TIFF *tif),
    get_tzup_fields(TIFF *tif, TZUP_FIELDS *tzup_f),
    get_bits_per_sample(TIFF *tif, USHORT *bps),
    get_samples_per_pixel(TIFF *tif, USHORT *spp),
    get_image_sizes(TIFF *tif, int *xsize, int *ysize),
    get_photometric(TIFF *tif, USHORT *pm),
    get_resolutions(TIFF *tif, double *x_dpi, double *y_dpi),
    get_history(TIFF *tif, char **history),
    get_compression(TIFF *tif, int *compression),
    get_rows_per_strip(TIFF *tif, long *rowperstrip),
    get_tag_software(TIFF *tif, char *tag_software),
    get_orientation(TIFF *tif, int *orientation);

static bool scanline_needs_swapping(TIFF *tfp);

static void get_planarconfig(TIFF *tif, USHORT *planargonfig);

static void get_image_offsets_and_dimensions(TIFF *tif, int xSBsize,
                                             int ySBsize, int *x0, int *y0,
                                             int *xsize, int *ysize,
                                             double *h_pos, UCHAR *extra_mask,
                                             TBOOL *edu_file),

    get_plt_name(char *filename, char *pltname),

    clear_image_buffer_16(IMAGE *img, USHORT bg_val),
    clear_image_buffer_24(IMAGE *img, ULONG bg_val),
    clear_extra(IMAGE *img, UCHAR bg_val),
    clear_image_region_buffer_16(USHORT *buffer, int x0, int y0,
                                 int clear_xsize, int clear_ysize, int wrap_x,
                                 USHORT bg_val),
    clear_image_region_buffer_24(ULONG *buffer, int x0, int y0, int clear_xsize,
                                 int clear_ysize, int wrap_x, ULONG bg_val),
    clear_extra_region(UCHAR *extra, int x0, int y0, int clear_xsize,
                       int clear_ysize, int wrap_x, UCHAR bg_val);

static int get_image(TIFF *tif, IMAGE *image),
    get_image_contig_16(TIFF *tif, IMAGE *image),
    get_image_contig_24(TIFF *tif, IMAGE *image),
    get_icon(TIFF *tif, IMAGE *image), get_extra(TIFF *tif, IMAGE *image);

static char Verbose = 0; /* Variabile per debug */

/*---------------------------------------------------------------------------*/

static UINT get_output_compression(void) {
  static UINT output_compression = 0;
  /*
char *s;
if ( !output_compression)
{
if (tenv_get_var_s("TOONZ_TZUP_COMPRESSION", &s))
{
if (strstr (s, "rle") || strstr (s, "RLE"))
output_compression = COMPRESSION_TOONZ1;
else
output_compression = COMPRESSION_LZW;
}
else
output_compression = COMPRESSION_LZW;
}
*/
  return output_compression;
}

/*---------------------------------------------------------------------------*/

#ifdef CICCIO
int img_write_tzup(unsigned short *filename, IMAGE *image) {
  TIFF *tfp;
  int bits_per_sample, samples_per_pixel, photometric, planar_config;
  int orientation, rows_per_strip, bytes_per_line;
  int width, height, row, scanline, cmap_size, i;
  char str[200], *history;
  UCHAR *outbuf;
  USHORT window[TOONZWINDOW_COUNT];
  USHORT palette[TOONZPALETTE_COUNT];
  TCM_INFO *tcm;
  UINT tif_compression;

  /* CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR */

  Silent_tiff_print_error               = 1;
  Tiff_ignore_missing_internal_colormap = 1;
  tfp                                   = TIFFOpen(filename, "w");
  if (!tfp) {
    /*throw "unable to open file for output"; , filename);*/
    return FALSE;
  }

  switch (image->type) {
    CASE RGB : bits_per_sample       = 8;
    samples_per_pixel                = 4;
    photometric                      = PHOTOMETRIC_RGB;
    planar_config                    = PLANARCONFIG_CONTIG;
    CASE CMAPPED : bits_per_sample   = 16;
    samples_per_pixel                = 1;
    photometric                      = PHOTOMETRIC_PALETTE;
    planar_config                    = PLANARCONFIG_CONTIG;
    CASE CMAPPED24 : bits_per_sample = 32;
    samples_per_pixel                = 1;
    photometric                      = PHOTOMETRIC_PALETTE;
    planar_config                    = PLANARCONFIG_CONTIG;
  DEFAULT:
    /*tmsg_error("bad image type writing file %s", filename);*/
    goto bad;
  }

  orientation = ORIENTATION_BOTLEFT;
  /*
width  = image->pixmap.xsize;
height = image->pixmap.ysize;
*/
  width  = image->pixmap.xSBsize;
  height = image->pixmap.ySBsize;

  if (image->cmap.info.n_pencils && image->cmap.info.n_colors)
    tcm = &image->cmap.info;
  else if (image->type == CMAPPED24)
    tcm = (TCM_INFO *)&Tcm_24_default_info;
  else
    tcm = (TCM_INFO *)&Tcm_new_default_info;

  if (image->cmap.offset)
    cmap_size = TCM_MIN_CMAP_BUFFER_SIZE(*tcm);
  else
    cmap_size = TCM_CMAP_BUFFER_SIZE(*tcm);
  palette[0]  = (image->cmap.offset > 0) ? 4 : 3;
  palette[1]  = image->cmap.offset;
  palette[2]  = cmap_size <= 0xffff ? cmap_size : 0;
  palette[3]  = 0 /* tcm->tone_offs */;
  palette[4]  = tcm->tone_bits;
  palette[5]  = tcm->color_offs;
  palette[6]  = tcm->color_bits;
  palette[7]  = tcm->pencil_offs;
  palette[8]  = tcm->pencil_bits;
  palette[9]  = tcm->offset_mask;
  palette[10] = tcm->n_colors;
  palette[11] = tcm->n_pencils;

  for (i = 12; i < TOONZPALETTE_COUNT; i++) palette[i] = 0;

  tif_compression = get_output_compression();

  TIFFSetField(tfp, TIFFTAG_TOONZPALETTE, palette);
  TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
  TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(tfp, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(tfp, TIFFTAG_ORIENTATION, orientation);
  TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, planar_config);
  TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, photometric);
  TIFFSetField(tfp, TIFFTAG_COMPRESSION, tif_compression);

  /*
* NOTARE CHE VA COMPLETATA LA PARTE RELAITVA AL SETTAGGIO DELLA RISOLUZIONE
*
*/
  TIFFSetField(tfp, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
  TIFFSetField(tfp, TIFFTAG_XRESOLUTION, image->pixmap.x_dpi);
  TIFFSetField(tfp, TIFFTAG_YRESOLUTION, image->pixmap.y_dpi);
  switch (orientation) {
    CASE ORIENTATION_BOTLEFT
        : __OR ORIENTATION_BOTRIGHT
          : __OR ORIENTATION_TOPLEFT
            : __OR ORIENTATION_TOPRIGHT
              : if (image->pixmap.x_dpi) TIFFSetField(
                    tfp, TIFFTAG_XPOSITION,
                    image->pixmap.h_pos / image->pixmap.x_dpi + 8.0);
    CASE ORIENTATION_LEFTBOT
        : __OR ORIENTATION_RIGHTBOT
          : __OR ORIENTATION_LEFTTOP
            : __OR ORIENTATION_RIGHTTOP
              : if (image->pixmap.y_dpi) TIFFSetField(
                    tfp, TIFFTAG_XPOSITION,
                    image->pixmap.h_pos / image->pixmap.y_dpi + 8.0);
  }
  /*snprintf(str, sizeof(str), "TOONZ %s", versione_del_software);*/
  TIFFSetField(tfp, TIFFTAG_SOFTWARE, str);

  /* Aggiungo le informazioni relative alla savebox a all'history */

  window[0] = image->pixmap.xD;
  window[1] = image->pixmap.yD;
  window[2] = image->pixmap.xsize;
  window[3] = image->pixmap.ysize;
  window[4] = image->pixmap.extra_mask;

  for (i = 5; i < TOONZWINDOW_COUNT - 1; i++) window[i] = 0;

  window[TOONZWINDOW_COUNT - 1] = 0;
  /*  (Img_license_attr & TA_TOONZ_EDU) != 0;*/

  TIFFSetField(tfp, TIFFTAG_TOONZWINDOW, window);

  history = build_history();
  /*
if (image->history)
{
switch(check_history(image->history, history))
{
CASE APPEND:
image->history = append_history(image->history, history);
CASE REPLACE:
image->history = replace_last_history(image->history, history);
DEFAULT:
tmsg_error("Internal error: bad history type");
abort();
}
free (history);
}
else
*/
  image->history = history;

  TIFFSetField(tfp, TIFFTAG_TOONZHISTORY, image->history);

  bytes_per_line = TIFFScanlineSize(tfp);

  /*
* massima lunghezza di bytes in una strip e' 8k
* vedi Graphics File Formats pag.48
*/
  if (planar_config == PLANARCONFIG_CONTIG)
    rows_per_strip = (8 * 1024) / bytes_per_line;
  else
    rows_per_strip = 1L;

  TIFFSetField(tfp, TIFFTAG_ROWSPERSTRIP,
               rows_per_strip == 0 ? 1L : rows_per_strip);

  switch (image->type) {
    CASE RGB : if (!write_rgb_image(image, tfp)) {
      // tmsg_error("unable to write buffer to file %s", filename);
      goto bad;
    }
    CASE CMAPPED : if (!write_cmapped_image(image, tfp)) {
      // tmsg_error("unable to write buffer to file %s", filename);
      goto bad;
    }
    CASE CMAPPED24 : if (!write_cmapped24_image(image, tfp)) {
      // tmsg_error("unable to write buffer to file %s", filename);
      goto bad;
    }
  DEFAULT:
    // tmsg_error("bad image type writing file %s", filename);
    goto bad;
  }

  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  return TRUE;

bad:
  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  return FALSE;
}
#endif

/*---------------------------------------------------------------------------*/

static int write_rgb_image(IMAGE *image, TIFF *tfp) {
  int scanlinesize;
  UCHAR *outbuf, *buf;
  int row, lx, ly, x, wrapx, x0, y0;
  LPIXEL *gl_buf, *tmp;

  lx    = image->pixmap.xSBsize;
  ly    = image->pixmap.ySBsize;
  x0    = image->pixmap.xD;
  y0    = image->pixmap.yD;
  wrapx = image->pixmap.xsize;

  tmp          = (LPIXEL *)image->pixmap.buffer + y0 * wrapx + x0;
  scanlinesize = TIFFScanlineSize(tfp);
  outbuf       = new UCHAR[scanlinesize];

  if (!outbuf) return FALSE;

  for (row = 0; row < ly; row++) {
    buf    = outbuf;
    gl_buf = tmp;
    for (x = 0; x < lx; x++) {
      *buf++ = gl_buf->r;
      *buf++ = gl_buf->g;
      *buf++ = gl_buf->b;
      *buf++ = gl_buf->m;
      gl_buf++;
    }
    if (TIFFWriteScanline(tfp, outbuf, row, 0) < 0) goto bad;
    tmp += wrapx;
  }

  delete[] outbuf;

  if (image->pixmap.extra_mask) write_extra(image, tfp);

  return TRUE;

bad:
  // tmsg_error("write_scan_line failed at row %d", row);
  delete[] outbuf;
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int write_cmapped_image(IMAGE *image, TIFF *tfp) {
  int scanline, lx, ly, x0, y0, wrapx;
  int row;
  UCHAR *outbuf;
  USHORT *tmp;
  int tmp_icon;
  struct s_pixmap ori_icon;
  UINT tif_compression;

  tif_compression = get_output_compression();

  tmp_icon = FALSE;

  lx    = image->pixmap.xSBsize;
  ly    = image->pixmap.ySBsize;
  x0    = image->pixmap.xD;
  y0    = image->pixmap.yD;
  wrapx = image->pixmap.xsize;

  tmp    = image->pixmap.buffer + y0 * wrapx + x0;
  outbuf = 0;
  for (row = 0; row < ly; row++) {
    outbuf = (UCHAR *)tmp;
    if (TIFFWriteScanline(tfp, outbuf, row, 0) < 0) goto bad;
    tmp += wrapx;
  }
  if (image->pixmap.extra_mask && !image->icon.buffer) {
    ori_icon = image->icon;
    // make_icon (image, ICON_WIDTH, ICON_HEIGHT);
    tmp_icon = TRUE;
  }
  if (image->icon.buffer) {
    if (!TIFFFlush(tfp)) {
      // tmsg_error("Unable to flush data to .tz(up) file");
      goto bad;
    }
    lx = image->icon.xsize;
    ly = image->icon.ysize;

    TIFFSetField(tfp, TIFFTAG_SUBFILETYPE, 1);
    TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, lx);
    TIFFSetField(tfp, TIFFTAG_IMAGELENGTH, ly);
    TIFFSetField(tfp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
    TIFFSetField(tfp, TIFFTAG_COMPRESSION, tif_compression);

    scanline = TIFFScanlineSize(tfp);
    outbuf   = (UCHAR *)image->icon.buffer;
    for (row = 0; row < ly; row++) {
      if (TIFFWriteScanline(tfp, outbuf, row, 0) < 0) {
        // tmsg_error("error writing icon to .tz(up) file");
        goto bad;
      }
      outbuf += scanline;
    }
  }
  if (image->pixmap.extra_mask) write_extra(image, tfp);

  if (tmp_icon && image->icon.buffer) {
    TFREE(image->icon.buffer)
    image->icon = ori_icon;
  }
  return TRUE;

bad:
  if (tmp_icon && image->icon.buffer) {
    TFREE(image->icon.buffer)
    image->icon = ori_icon;
  }
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int write_cmapped24_image(IMAGE *image, TIFF *tfp) {
  int scanline, lx, ly, x0, y0, wrapx;
  int row;
  UCHAR *outbuf;
  ULONG *tmp;
  UINT tif_compression;

  tif_compression = get_output_compression();

  lx    = image->pixmap.xSBsize;
  ly    = image->pixmap.ySBsize;
  x0    = image->pixmap.xD;
  y0    = image->pixmap.yD;
  wrapx = image->pixmap.xsize;

  tmp    = (ULONG *)image->pixmap.buffer + y0 * wrapx + x0;
  outbuf = 0;
  for (row = 0; row < ly; row++) {
    outbuf = (UCHAR *)tmp;
    if (TIFFWriteScanline(tfp, outbuf, row, 0) < 0) goto bad;
    tmp += wrapx;
  }

  if (image->icon.buffer) {
    if (!TIFFFlush(tfp)) {
      // tmsg_error("Unable to flush data to .tz(up) file");
      goto bad;
    }
    lx = image->icon.xsize;
    ly = image->icon.ysize;

    TIFFSetField(tfp, TIFFTAG_SUBFILETYPE, 1);
    TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 32);
    TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, lx);
    TIFFSetField(tfp, TIFFTAG_IMAGELENGTH, ly);
    TIFFSetField(tfp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
    TIFFSetField(tfp, TIFFTAG_COMPRESSION, tif_compression);

    scanline = TIFFScanlineSize(tfp);
    outbuf   = (UCHAR *)image->icon.buffer;
    for (row = 0; row < ly; row++) {
      if (TIFFWriteScanline(tfp, outbuf, row, 0) < 0) {
        // tmsg_error("error writing icon to .tz(up) file");
        goto bad;
      }
      outbuf += scanline;
    }
  }
  return TRUE;

bad:
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int write_extra(IMAGE *image, TIFF *tfp) {
  int lx, ly, x0, y0, wrapx, row;
  UCHAR *extra;
  UINT tif_compression;
  int rowsperstrip;

  if (!image->pixmap.extra_mask) return TRUE;

  tif_compression = get_output_compression();

  if (!TIFFFlush(tfp)) {
    // tmsg_error("Unable to flush data to .tz(up) file");
    goto bad;
  }
  lx    = image->pixmap.xSBsize;
  ly    = image->pixmap.ySBsize;
  x0    = image->pixmap.xD;
  y0    = image->pixmap.yD;
  wrapx = image->pixmap.xsize;

  TIFFSetField(tfp, TIFFTAG_SUBFILETYPE, 1);
  TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 1);
  TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, lx);
  TIFFSetField(tfp, TIFFTAG_IMAGELENGTH, ly);
  TIFFSetField(tfp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
  TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  TIFFSetField(tfp, TIFFTAG_COMPRESSION, tif_compression);

  rowsperstrip = (8 * 1024) / lx; /* contig, 1 byte per pixel */
  NOT_LESS_THAN(1, rowsperstrip)
  TIFFSetField(tfp, TIFFTAG_ROWSPERSTRIP, rowsperstrip);

  extra = image->pixmap.extra + y0 * wrapx + x0;
  for (row = 0; row < ly; row++) {
    if (TIFFWriteScanline(tfp, extra, row, 0) < 0) goto bad;
    extra += wrapx;
  }
  return TRUE;

bad:
  // tmsg_error ("error writing extra information to .tz(up) file");
  return FALSE;
}

/*===========================================================================*/

void next_img_read_with_extra(void) { Next_img_read_with_extra = TRUE; }

/*===========================================================================*/

TImageP TImageReaderTZP::load() {
  /*
FILE *fp;
if ((fp = _wfopen(getFilePath().getWideString().c_str(), L"rb")) == NULL)
{
throw TImageException(getFilePath(),"can't open file");
}

//{
//  fclose(fp);
//  throw TImageException(getFilePath(),"invalid file format");
//}

TRaster32P raster(lx,ly);
TPixel32* row;

TRasterImageP rasImage(raster);

TImageP image(rasImage);

*/

  wstring fn = getFilePath().getWideString();
  TIFF *tfp;
  IMAGE *image = NIL;
  TZUP_FIELDS tzup_f;
  // char pltname[1024];
  USHORT *window = NIL;
  USHORT *palette; /*  [TOONZPALETTE_COUNT] */
  // int max_n_colors, max_n_pencils;

  /*
CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR
SET_READ_WITH_EXTRA
*/

  Silent_tiff_print_error               = 1;
  Tiff_ignore_missing_internal_colormap = 1;
  tfp                                   = TIFFOpen(fn.c_str(), "r");
  if (!tfp) return TImageP();

  if (!get_tzup_fields(tfp, &tzup_f)) return TImageP();

  /*
if (tzup_f.edu_file && !(Img_license_attr & TA_TOONZ_EDU))
{
char str[1024];
BUILD_EDU_ERROR_STRING(str)
tmsg_error (str);
goto bad;
}
*/

  // image = new IMAGE; // new_img();
  // if (!image)
  //  goto bad;

  if (!TIFFGetField(tfp, TIFFTAG_TOONZPALETTE, &palette)) {
    // image->cmap.info = Tcm_old_default_info;
  } else {
    //// image->cmap.info.tone_offs   = palette[3]; sempre 0
    /*
image->cmap.info.tone_bits   = (UCHAR)palette[4];
image->cmap.info.color_offs  = (UCHAR)palette[5];
image->cmap.info.color_bits  = (UCHAR)palette[6];
image->cmap.info.pencil_offs = (UCHAR)palette[7];
image->cmap.info.pencil_bits = (UCHAR)palette[8];
image->cmap.info.offset_mask = palette[9];
image->cmap.info.n_tones     = 1 << palette[4];
image->cmap.info.n_colors    = palette[10];
image->cmap.info.n_pencils   = palette[11];
image->cmap.info.default_val = (image->cmap.info.n_tones-1) |
                      image->cmap.info.offset_mask;
*/
  }

  /* estendo la palette */

  /*
max_n_colors  = 1 << image->cmap.info.color_bits;
max_n_pencils = 1 << image->cmap.info.pencil_bits;
if (max_n_colors  > image->cmap.info.n_colors ||
max_n_pencils > image->cmap.info.n_pencils)
{
image->cmap.info.n_colors  = max_n_colors;
image->cmap.info.n_pencils = max_n_pencils;
image->cmap.color_n  = image->cmap.info.n_colors;
image->cmap.pencil_n = image->cmap.info.n_pencils;
}
*/

  TRasterCM16P raster(tzup_f.xsize, tzup_f.ysize);

  ///===========
  {
    bool swapNeeded = scanline_needs_swapping(tfp);

    int x0    = tzup_f.x0;
    int y0    = tzup_f.y0;
    int lx    = tzup_f.xSBsize;
    int ly    = tzup_f.ySBsize;
    int x1    = x0 + lx - 1;
    int y1    = y0 + ly - 1;
    int xsize = tzup_f.xsize;
    int ysize = tzup_f.ysize;

    int wrap = tzup_f.xsize;

    assert(raster->getBounds().contains(TRect(x0, y0, x1, y1)));
    raster->fillOutside(TRect(x0, y0, x1, y1), TPixelCM16(0, 0, 15));
    raster->lock();
    for (int y = y0; y <= y1; y++) {
      TPixelCM16 *row = raster->pixels(y) + x0;
      if (TIFFReadScanline(tfp, (UCHAR *)row, y - y0, 0) < 0)
        if (swapNeeded) TIFFSwabArrayOfShort((USHORT *)row, lx);
    }
    raster->unlock();
    /*
buf   = image->pixmap.buffer;
default_val = image->cmap.info.default_val;
for (y = 0; y < y0; y++)
{
pix = buf + y * wrap;
for (x = 0; x < xsize; x++)
*pix++ = default_val;
}
for ( ; y <= y1; y++)
{
pix = buf + y * wrap;
for (x = 0; x < x0; x++)
*pix++ = default_val;
if (TIFFReadScanline (tf, (UCHAR *)pix, y - y0, 0) < 0)
{
static int gia_dato = FALSE;
if ( !gia_dato)
{
//tmsg_error("bad data read on line %d", y);
gia_dato = TRUE;
}
memset (pix, 0, lx * sizeof(*pix));
}
if (swap_needed)
TIFFSwabArrayOfShort((USHORT *)pix, lx);
pix += lx;
for (x = x1 + 1; x < xsize; x++)
*pix++ = default_val;
}
for ( ; y < ysize; y++)
{
pix = buf + y * wrap;
for (x = 0; x < xsize; x++)
*pix++ = default_val;
}
*/
  }
  ///===========

  switch (tzup_f.photometric) {
    CASE PHOTOMETRIC_MINISBLACK : __OR PHOTOMETRIC_MINISWHITE
                                  : __OR PHOTOMETRIC_RGB
                                    :;  // image->type = RGB;

    CASE PHOTOMETRIC_PALETTE :
        /*
if (tzup_f.bits_per_sample == 32)
image->type = CMAPPED24;
else
image->type = CMAPPED;
*/

        DEFAULT:
        // tmsg_error("bad photometric interpretation in file %s", filename);
        // goto bad;
        ;
  }

  // if (!get_history(tfp, &image->history))
  //  image->history = NIL;

  // image->pixmap.extra_mask = tzup_f.extra_mask;

  // if (!allocate_pixmap(image, tzup_f.xsize, tzup_f.ysize))
  // image->pixmap.buffer = new USHORT[tzup_f.xsize*tzup_f.ysize];
  // if(!image->pixmap.buffer)
  //  goto bad;

  // image->pixmap.xD      = tzup_f.x0;
  // image->pixmap.yD      = tzup_f.y0;
  // image->pixmap.xSBsize = tzup_f.xSBsize;
  // image->pixmap.ySBsize = tzup_f.ySBsize;

  /*
if (!get_image(tfp, image))
{
//tmsg_error("no image while reading file %s", filename);
goto bad;
}
if (!get_icon(tfp, image))
{
//make_icon(image, ICON_WIDTH, ICON_HEIGHT);
}
if (image->pixmap.extra)
if ( !get_extra (tfp, image))
{
//tmsg_error("missing extra information while reading file %s", filename);
goto bad;
}
image->pixmap.x_dpi = tzup_f.x_dpi;
image->pixmap.y_dpi = tzup_f.y_dpi;
image->pixmap.h_pos = tzup_f.h_pos;

Silent_tiff_print_error = 0;
Tiff_ignore_missing_internal_colormap = 0;
*/

  TIFFClose(tfp);

  // image->filename = 0; // strsave(filename);

  // get_plt_name(filename, pltname);
  // image->cmap.name = 0; // strsave(pltname);

  assert(0);
  return 0;
  // TToonzImageP toonzImage(raster);
  // return TImageP(toonzImage);

  /*
bad:
if (image)
{
//free_img(image);
}
Silent_tiff_print_error = 0;
Tiff_ignore_missing_internal_colormap = 0;
if (tfp)
TIFFClose(tfp);
return NIL;

*/
}

/*===========================================================================*/

static bool scanline_needs_swapping(TIFF *tfp) {
  USHORT compression;

  TIFFGetField(tfp, TIFFTAG_COMPRESSION, &compression);
  return compression == COMPRESSION_LZW && TIFFNeedSwab(tfp);
}

/*---------------------------------------------------------------------------*/

static int read_region_tzup_16(IMAGE *image, TIFF *tfp, char *filename,
                               INFO_REGION *region, int scale, UCHAR *buf,
                               int scanline_size, int rowperstrip, int wrap_out,
                               int xD_offset, int yD_offset) {
  USHORT *inp = NIL, *outp = NIL, *appo_outp = NIL;
  int row, nrow, rrow;
  TBOOL swap_needed;

  swap_needed = scanline_needs_swapping(tfp);

  appo_outp = image->pixmap.buffer + yD_offset * wrap_out + xD_offset;

  if (Verbose) printf("Posizione in uscita: %d, %d\n", xD_offset, yD_offset);

  /* Puntatore per avanzamento nel buffer della regione */
  outp = appo_outp;

  row = region->startScanRow;
  /*
* Questa serie di scanline viene fatta perche' non viene
* accettato un accesso random alle righe del file.
*/
  if (row > 0) {
    int c;
    c = (row / rowperstrip) * rowperstrip;
    for (; c < row; c++) {
      if (TIFFReadScanline(tfp, buf, c, 0) < 0) {
        // tmsg_error("bad image data read on line %d of file %s", c, filename);
        return FALSE;
      }
    }
  }
  for (nrow = 0; nrow < region->scanNrow; nrow++) {
    appo_outp = outp;
    if (TIFFReadScanline(tfp, buf, row, 0) < 0) {
      // tmsg_error("bad image data read at line %d of file %s", row, filename);
      return FALSE;
    }
    if (swap_needed)
      TIFFSwabArrayOfShort((USHORT *)buf, scanline_size / sizeof(USHORT));

    inp = (USHORT *)buf + region->startScanCol;
    for (rrow = 0; rrow < region->scanNcol; rrow++) {
      *outp++ = *inp;
      inp += scale;
    }
    if (scale > 1) {
      register currRow = 0, stepRow = 1, nextRow = 0;
      if (row + scale > region->ly_in)
        break;
      else
        nextRow = row + scale;
      stepRow   = (nextRow / rowperstrip) * rowperstrip;
      for (currRow = stepRow; currRow < nextRow; currRow++) {
        if (TIFFReadScanline(tfp, buf, currRow, 0) < 0) {
          // tmsg_error("bad image data in file %s at line %d", filename,
          // currRow);
          return FALSE;
        }
      }
    }
    outp = appo_outp + wrap_out;
    row += scale;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int read_region_tzup_24(IMAGE *image, TIFF *tfp, char *filename,
                               INFO_REGION *region, int scale, UCHAR *buf,
                               int scanline_size, int rowperstrip, int wrap_out,
                               int xD_offset, int yD_offset) {
  ULONG *inp = NIL, *outp = NIL, *appo_outp = NIL;
  int row, nrow, rrow;
  TBOOL swap_needed;

  swap_needed = scanline_needs_swapping(tfp);

  appo_outp = (ULONG *)image->pixmap.buffer + yD_offset * wrap_out + xD_offset;

  if (Verbose) printf("Posizione in uscita: %d, %d\n", xD_offset, yD_offset);

  /* Puntatore per avanzamento nel buffer della regione */
  outp = appo_outp;

  row = region->startScanRow;
  /*
* Questa serie di scanline viene fatta perche' non viene
* accettato un accesso random alle righe del file.
*/
  if (row > 0) {
    int c;
    c = (row / rowperstrip) * rowperstrip;
    for (; c < row; c++) {
      if (TIFFReadScanline(tfp, buf, c, 0) < 0) {
        // tmsg_error("bad image data read on line %d of file %s", c, filename);
        return FALSE;
      }
    }
  }
  for (nrow = 0; nrow < region->scanNrow; nrow++) {
    appo_outp = outp;
    if (TIFFReadScanline(tfp, buf, row, 0) < 0) {
      // tmsg_error("bad image data read at line %d of file %s", row, filename);
      return FALSE;
    }
    if (swap_needed)
      TIFFSwabArrayOfLong((TUINT32 *)buf, scanline_size / sizeof(ULONG));

    inp = (ULONG *)buf + region->startScanCol;
    for (rrow = 0; rrow < region->scanNcol; rrow++) {
      *outp++ = *inp;
      inp += scale;
    }
    if (scale > 1) {
      register currRow = 0, stepRow = 1, nextRow = 0;
      if (row + scale > region->ly_in)
        break;
      else
        nextRow = row + scale;
      stepRow   = (nextRow / rowperstrip) * rowperstrip;
      for (currRow = stepRow; currRow < nextRow; currRow++) {
        if (TIFFReadScanline(tfp, buf, currRow, 0) < 0) {
          // tmsg_error("bad image data in file %s at line %d", filename,
          // currRow);
          return FALSE;
        }
      }
    }
    outp = appo_outp + wrap_out;
    row += scale;
  }
  return TRUE;
}

/*===========================================================================*/

static int read_region_extra(IMAGE *image, TIFF *tfp, char *filename,
                             INFO_REGION *region, int scale, UCHAR *buf,
                             int wrap_out, int xD_offset, int yD_offset) {
  UCHAR *inp = NIL, *outp = NIL, *appo_outp = NIL;
  int row, nrow, rrow;
  int scanline_size, rowperstrip;

  if (!TIFFReadDirectory(tfp)) return FALSE;

  scanline_size = TIFFScanlineSize(tfp);
  if (!TIFFGetField(tfp, TIFFTAG_ROWSPERSTRIP, &rowperstrip))
    rowperstrip = MAXINT / 2;

  appo_outp = image->pixmap.extra + yD_offset * wrap_out + xD_offset;

  if (Verbose)
    printf("Posizione in uscita (extra): %d, %d\n", xD_offset, yD_offset);

  /* Puntatore per avanzamento nel buffer della regione */
  outp = appo_outp;

  row = region->startScanRow;
  /*
* Questa serie di scanline viene fatta perche' non viene
* accettato un accesso random alle righe del file.
*/
  if (row > 0) {
    int c;
    c = (row / rowperstrip) * rowperstrip;
    for (; c < row; c++) {
      if (TIFFReadScanline(tfp, buf, c, 0) < 0) {
        // tmsg_error("bad extra data read on line %d of file %s", c, filename);
        return FALSE;
      }
    }
  }
  for (nrow = 0; nrow < region->scanNrow; nrow++) {
    appo_outp = outp;
    if (TIFFReadScanline(tfp, buf, row, 0) < 0) {
      // tmsg_error("bad extra data read at line %d of file %s", row, filename);
      return FALSE;
    }

    inp = buf + region->startScanCol;
    for (rrow = 0; rrow < region->scanNcol; rrow++) {
      *outp++ = *inp;
      inp += scale;
    }
    if (scale > 1) {
      register currRow = 0, stepRow = 1, nextRow = 0;
      if (row + scale > region->ly_in)
        break;
      else
        nextRow = row + scale;
      stepRow   = (nextRow / rowperstrip) * rowperstrip;
      for (currRow = stepRow; currRow < nextRow; currRow++) {
        if (TIFFReadScanline(tfp, buf, currRow, 0) < 0) {
          // tmsg_error("bad extra data in file %s at line %d", filename,
          // currRow);
          return FALSE;
        }
      }
    }
    outp = appo_outp + wrap_out;
    row += scale;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/
#ifdef CICCIO
IMAGE *img_read_region_tzup(unsigned short *filename, int x1, int y1, int x2,
                            int y2, int scale) {
  TIFF *tfp    = NIL;
  IMAGE *image = NIL;
  INFO_REGION region;
  TZUP_FIELDS tzup_f;
  char pltname[1024];
  UCHAR *buf = NIL;
  USHORT planarconfig;
  int rowperstrip;
  int xsize_out, ysize_out, clear_xsize, clear_ysize;
  int xD_offset, yD_offset;
  int x1_reg, y1_reg, x2_reg, y2_reg;
  int box_x1, box_y1, box_x2, box_y2;
  USHORT *palette; /*  [TOONZPALETTE_COUNT] */
  int scanline_size;
  int ret;
  int max_n_colors, max_n_pencils;

  /*
CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR
SET_READ_WITH_EXTRA
*/

  Silent_tiff_print_error               = 1;
  Tiff_ignore_missing_internal_colormap = 1;
  tfp                                   = TIFFOpen(filename, "r");
  if (!tfp) return NIL;

  if (!get_tzup_fields(tfp, &tzup_f)) goto bad;

  /*
if (tzup_f.edu_file && !(Img_license_attr & TA_TOONZ_EDU))
{
char str[1024];
BUILD_EDU_ERROR_STRING(str)
tmsg_error (str);
goto bad;
}
*/

  TIFFGetField(tfp, TIFFTAG_ROWSPERSTRIP, &rowperstrip);

  TIFFGetField(tfp, TIFFTAG_PLANARCONFIG, &planarconfig);
  if (planarconfig == PLANARCONFIG_SEPARATE) {
    // tmsg_error("separate buffer image in file %s not supported
    // yet",filename);
    goto bad;
  }

  image = new IMAGE;  // new_img();
  if (!image) goto bad;

  if (!TIFFGetField(tfp, TIFFTAG_TOONZPALETTE, &palette)) {
    image->cmap.info = Tcm_old_default_info;
  } else {
    /*image->cmap.info.tone_offs   = palette[3]; */
    image->cmap.info.tone_bits   = (UCHAR)palette[4];
    image->cmap.info.color_offs  = (UCHAR)palette[5];
    image->cmap.info.color_bits  = (UCHAR)palette[6];
    image->cmap.info.pencil_offs = (UCHAR)palette[7];
    image->cmap.info.pencil_bits = (UCHAR)palette[8];
    image->cmap.info.offset_mask = palette[9];
    image->cmap.info.n_tones     = 1 << palette[4];
    image->cmap.info.n_colors    = palette[10];
    image->cmap.info.n_pencils   = palette[11];
    image->cmap.info.default_val =
        (image->cmap.info.n_tones - 1) | image->cmap.info.offset_mask;
  }

  /* estendo la palette */

  max_n_colors  = 1 << image->cmap.info.color_bits;
  max_n_pencils = 1 << image->cmap.info.pencil_bits;
  if (max_n_colors > image->cmap.info.n_colors ||
      max_n_pencils > image->cmap.info.n_pencils) {
    image->cmap.info.n_colors  = max_n_colors;
    image->cmap.info.n_pencils = max_n_pencils;
    image->cmap.color_n        = image->cmap.info.n_colors;
    image->cmap.pencil_n       = image->cmap.info.n_pencils;
  }

  switch (tzup_f.photometric) {
    CASE PHOTOMETRIC_MINISBLACK : __OR PHOTOMETRIC_MINISWHITE
                                  : __OR PHOTOMETRIC_RGB : image->type = RGB;

    CASE PHOTOMETRIC_PALETTE : if (tzup_f.bits_per_sample == 32) image->type =
        CMAPPED24;
    else image->type = CMAPPED;

  DEFAULT:
    // tmsg_error("bad photometric interpretation in file %s", filename);
    goto bad;
  }

  x1_reg = x1 - tzup_f.x0;
  y1_reg = y1 - tzup_f.y0;

  x2_reg = x2 - tzup_f.x0;
  y2_reg = y2 - tzup_f.y0;

  xsize_out = (x2 - x1) / scale + 1;
  ysize_out = (y2 - y1) / scale + 1;

  getInfoRegion(&region, x1_reg, y1_reg, x2_reg, y2_reg, scale, tzup_f.xSBsize,
                tzup_f.ySBsize);

  xD_offset = region.x_offset;
  yD_offset = region.y_offset;

  // if (Verbose)
  //  printInfoRegion(&region);

  image->pixmap.extra_mask = tzup_f.extra_mask;

  // if (!allocate_pixmap(image, xsize_out, ysize_out))
  image->pixmap.buffer = new USHORT[xsize_out * ysize_out];
  if (image->pixmap.buffer) goto bad;

  if (TRUE /* x1 != 0 || x2 != tzup_f.xsize - 1 ||
             y1 != 0 || y2 != tzup_f.ysize - 1 */) {
    clear_xsize = xsize_out;
    clear_ysize = ysize_out;
    if (image->type == CMAPPED)
      clear_image_region_buffer_16(image->pixmap.buffer, 0, 0, clear_xsize,
                                   clear_ysize, xsize_out,
                                   image->cmap.info.offset_mask + 15);
    else if (image->type == CMAPPED24)
      clear_image_region_buffer_24((ULONG *)image->pixmap.buffer, 0, 0,
                                   clear_xsize, clear_ysize, xsize_out, 255);
    else
      abort();
  } else {
    clear_xsize = 0;
    clear_ysize = 0;
  }
  if (Verbose) {
    printf("Clear xsize/xsize_out: %d/%d in %d:\n", clear_xsize, xsize_out,
           tzup_f.xsize);
    printf("Clear ysize/ysize_out: %d/%d in %d:\n", clear_ysize, ysize_out,
           tzup_f.ysize);
  }
  if (image->pixmap.extra)
    clear_extra_region(image->pixmap.extra, 0, 0, clear_xsize, clear_ysize,
                       xsize_out, 0);
  box_x1 = tzup_f.x0;
  box_y1 = tzup_f.y0;
  box_x2 = tzup_f.x0 + tzup_f.xSBsize - 1;
  box_y2 = tzup_f.y0 + tzup_f.ySBsize - 1;
  if (x1 > box_x2 || x2 < box_x1 || y1 > box_y2 || y2 < box_y1) {
    image->pixmap.xsize   = xsize_out;
    image->pixmap.ysize   = ysize_out;
    image->pixmap.xSBsize = xsize_out;
    image->pixmap.ySBsize = ysize_out;
    image->pixmap.xD      = 0;
    image->pixmap.yD      = 0;
    image->pixmap.x_dpi   = tzup_f.x_dpi;
    image->pixmap.y_dpi   = tzup_f.y_dpi;
    image->pixmap.h_pos   = tzup_f.h_pos;
    image->filename       = strsave(filename);
    get_plt_name(filename, pltname);
    image->cmap.name = strsave(pltname);
    goto ok;
  }
  image->pixmap.xD      = xD_offset;
  image->pixmap.yD      = yD_offset;
  image->pixmap.xSBsize = region.scanNcol;
  image->pixmap.ySBsize = region.scanNrow;

  /* Buffer per la scanline */
  scanline_size = TIFFScanlineSize(tfp);
  TMALLOC(buf, scanline_size)
  if (!buf) goto bad;

  switch (image->type) {
    CASE CMAPPED : ret = read_region_tzup_16(
                       image, tfp, filename, &region, scale, buf, scanline_size,
                       rowperstrip, xsize_out, xD_offset, yD_offset);
    CASE CMAPPED24
        : ret = read_region_tzup_24(image, tfp, filename, &region, scale, buf,
                                    scanline_size, rowperstrip, xsize_out,
                                    xD_offset, yD_offset);
  DEFAULT:
    ret = FALSE;
    abort();
  }
  if (!ret) goto bad;

  image->pixmap.x_dpi = tzup_f.x_dpi;
  image->pixmap.y_dpi = tzup_f.y_dpi;
  image->pixmap.h_pos = tzup_f.h_pos;

  image->filename = strsave(filename);
  get_plt_name(filename, pltname);
  image->cmap.name = strsave(pltname);

  if (!get_icon(tfp, image)) make_icon(image, ICON_WIDTH, ICON_HEIGHT);

  if (image->pixmap.extra) {
    ret = read_region_extra(image, tfp, filename, &region, scale, buf,
                            xsize_out, xD_offset, yD_offset);
    if (!ret) goto bad;
  }

ok:
  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  TFREE(buf);
  return image;

bad:
  if (image) free_img(image);
  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  TFREE(buf);
  return NIL;
}

#endif

/*---------------------------------------------------------------------------*/

IMAGE *img_read_tzup_info(unsigned short *filename) {
  TIFF *tfp;
  IMAGE *image = NIL;
  TZUP_FIELDS tzup_f;
  USHORT *palette; /*  [TOONZPALETTE_COUNT] */
  int max_n_colors, max_n_pencils;

  /* CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR */

  Silent_tiff_print_error               = 1;
  Tiff_ignore_missing_internal_colormap = 1;

  tfp = TIFFOpen((wchar_t *)filename, "r");
  if (!tfp) goto bad;

  if (!get_tzup_fields(tfp, &tzup_f)) goto bad;

  /*if (tzup_f.edu_file && !(Img_license_attr & TA_TOONZ_EDU))
{
char str[1024];
BUILD_EDU_ERROR_STRING(str)
tmsg_warning (str);
}
*/

  image = new IMAGE;  // new_img();
  if (!image) goto bad;

  if (!TIFFGetField(tfp, TIFFTAG_TOONZPALETTE, &palette)) {
    image->cmap.info = Tcm_old_default_info;
  } else {
    /*image->cmap.info.tone_offs   = palette[3]; */
    image->cmap.info.tone_bits   = (UCHAR)palette[4];
    image->cmap.info.color_offs  = (UCHAR)palette[5];
    image->cmap.info.color_bits  = (UCHAR)palette[6];
    image->cmap.info.pencil_offs = (UCHAR)palette[7];
    image->cmap.info.pencil_bits = (UCHAR)palette[8];
    image->cmap.info.offset_mask = palette[9];
    image->cmap.info.n_tones     = 1 << palette[4];
    image->cmap.info.n_colors    = palette[10];
    image->cmap.info.n_pencils   = palette[11];
    image->cmap.info.default_val =
        (image->cmap.info.n_tones - 1) | image->cmap.info.offset_mask;
  }

  /* estendo la palette */

  max_n_colors  = 1 << image->cmap.info.color_bits;
  max_n_pencils = 1 << image->cmap.info.pencil_bits;
  if (max_n_colors > image->cmap.info.n_colors ||
      max_n_pencils > image->cmap.info.n_pencils) {
    image->cmap.info.n_colors  = max_n_colors;
    image->cmap.info.n_pencils = max_n_pencils;
    image->cmap.color_n        = image->cmap.info.n_colors;
    image->cmap.pencil_n       = image->cmap.info.n_pencils;
  }

  switch (tzup_f.photometric) {
    CASE PHOTOMETRIC_MINISBLACK : __OR PHOTOMETRIC_MINISWHITE
                                  : __OR PHOTOMETRIC_RGB : image->type = RGB;

    CASE PHOTOMETRIC_PALETTE : if (tzup_f.bits_per_sample == 32) image->type =
        CMAPPED24;
    else image->type = CMAPPED;

  DEFAULT:
    // tmsg_error("bad photometric interpretation in file %s", filename);
    goto bad;
  }
  image->pixmap.extra_mask = tzup_f.extra_mask;

  if (!get_history(tfp, &image->history)) {
    image->history = NIL;
  }
  image->pixmap.xsize   = tzup_f.xsize;
  image->pixmap.ysize   = tzup_f.ysize;
  image->pixmap.xD      = tzup_f.x0;
  image->pixmap.yD      = tzup_f.y0;
  image->pixmap.xSBsize = tzup_f.xSBsize;
  image->pixmap.ySBsize = tzup_f.ySBsize;
  image->pixmap.x_dpi   = tzup_f.x_dpi;
  image->pixmap.y_dpi   = tzup_f.y_dpi;
  image->pixmap.h_pos   = tzup_f.h_pos;

  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  return image;

bad:
  if (image) {
    // free_img(image);
  }
  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  if (tfp) TIFFClose(tfp);
  return NIL;
}

/*---------------------------------------------------------------------------*/

#ifdef CICCIO
IMAGE *img_read_tzup_icon(char *filename) {
  TIFF *tfp;
  IMAGE *image = NIL;
  TZUP_FIELDS tzup_f;
  char pltname[1024];
  USHORT *palette; /*  [TOONZPALETTE_COUNT] */
  int max_n_colors, max_n_pencils;

  Silent_tiff_print_error               = 1;
  Tiff_ignore_missing_internal_colormap = 1;
  tfp                                   = TIFFOpen(filename, "r");
  if (!tfp) return NIL;

  if (!get_tzup_fields(tfp, &tzup_f)) goto bad;

  image = new_img();
  if (!image) goto bad;

  image->type           = (tzup_f.bits_per_sample == 32) ? CMAPPED24 : CMAPPED;
  image->pixmap.xsize   = tzup_f.xsize;
  image->pixmap.ysize   = tzup_f.ysize;
  image->pixmap.xSBsize = tzup_f.xSBsize;
  image->pixmap.ySBsize = tzup_f.ySBsize;
  image->pixmap.xD      = tzup_f.x0;
  image->pixmap.yD      = tzup_f.y0;

  get_plt_name(filename, pltname);
  image->cmap.name = strsave(pltname);

  if (!TIFFGetField(tfp, TIFFTAG_TOONZPALETTE, &palette)) {
    image->cmap.info = Tcm_old_default_info;
  } else {
    /*image->cmap.info.tone_offs   = palette[3]; */
    image->cmap.info.tone_bits   = palette[4];
    image->cmap.info.color_offs  = palette[5];
    image->cmap.info.color_bits  = palette[6];
    image->cmap.info.pencil_offs = palette[7];
    image->cmap.info.pencil_bits = palette[8];
    image->cmap.info.offset_mask = palette[9];
    image->cmap.info.n_tones     = 1 << palette[4];
    image->cmap.info.n_colors    = palette[10];
    image->cmap.info.n_pencils   = palette[11];
    image->cmap.info.default_val =
        (image->cmap.info.n_tones - 1) | image->cmap.info.offset_mask;
  }

  /* estendo la palette */

  max_n_colors  = 1 << image->cmap.info.color_bits;
  max_n_pencils = 1 << image->cmap.info.pencil_bits;
  if (max_n_colors > image->cmap.info.n_colors ||
      max_n_pencils > image->cmap.info.n_pencils) {
    image->cmap.info.n_colors  = max_n_colors;
    image->cmap.info.n_pencils = max_n_pencils;
    image->cmap.color_n        = image->cmap.info.n_colors;
    image->cmap.pencil_n       = image->cmap.info.n_pencils;
  }

  if (!get_icon(tfp, image)) {
    // tmsg_error("unable to read icon image of file %s", filename);
    goto bad;
  }
  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  return image;

bad:
  if (image) free_img(image);
  Silent_tiff_print_error               = 0;
  Tiff_ignore_missing_internal_colormap = 0;
  TIFFClose(tfp);
  return NIL;
}
#endif

/*===========================================================================*/

static int get_tzup_fields(TIFF *tfp, TZUP_FIELDS *tzup_f) {
  if (!get_bits_per_sample(tfp, &tzup_f->bits_per_sample)) {
    // tmsg_error("can't handle %d-bit images", tzup_f->bits_per_sample);
    goto bad;
  }
  if (!get_samples_per_pixel(tfp, &tzup_f->samples_per_pixel)) {
    // tmsg_error("can't handle %d-channel images", tzup_f->samples_per_pixel);
    goto bad;
  }
  if (!get_image_sizes(tfp, &tzup_f->xSBsize, &tzup_f->ySBsize)) {
    // tmsg_error("bad image size %dx%d reading .tz(up) file",
    //           tzup_f->xSBsize, tzup_f->ySBsize);
    goto bad;
  }
  if (!get_photometric(tfp, &tzup_f->photometric)) {
    // tmsg_error("bad photometric interpretation reading .tz(up) file");
    goto bad;
  }
  if (!get_resolutions(tfp, &tzup_f->x_dpi, &tzup_f->y_dpi)) {
    // tmsg_error("bad resolutions xres=%lf, yres=%lf reading .tz(up) file",
    //           tzup_f->x_dpi, tzup_f->y_dpi);
    goto bad;
  }
  get_image_offsets_and_dimensions(tfp, tzup_f->xSBsize, tzup_f->ySBsize,
                                   &tzup_f->x0, &tzup_f->y0, &tzup_f->xsize,
                                   &tzup_f->ysize, &tzup_f->h_pos,
                                   &tzup_f->extra_mask, &tzup_f->edu_file);
  return TRUE;

bad:
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int get_bits_per_sample(TIFF *tif, USHORT *bps) {
  if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, bps)) {
    *bps = 0;
    return FALSE;
  }
  switch (*bps) {
    CASE 1 : __OR 2 : __OR 4 : __OR 8 : __OR 16 : __OR 32 : return TRUE;
  }
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int get_samples_per_pixel(TIFF *tif, USHORT *spp) {
  if (!TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, spp)) {
    *spp = 0;
    return FALSE;
  }
  switch (*spp) { CASE 1 : __OR 3 : __OR 4 : return TRUE; }
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int get_image_sizes(TIFF *tif, int *xsize, int *ysize) {
  *ysize = 0;
  if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, xsize)) return FALSE;
  if (!TIFFGetField(tif, TIFFTAG_IMAGELENGTH, ysize)) return FALSE;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static void get_image_offsets_and_dimensions(TIFF *tfp, int xSBsize,
                                             int ySBsize, int *x0, int *y0,
                                             int *xsize, int *ysize,
                                             double *h_pos, UCHAR *extra_mask,
                                             TBOOL *edu_file) {
  USHORT *window, orientation;
  float xposition, dpi;
  // int i;

  if (!TIFFGetField(tfp, TIFFTAG_TOONZWINDOW, &window)) {
    *x0         = 0;
    *y0         = 0;
    *xsize      = xSBsize;
    *ysize      = ySBsize;
    *extra_mask = 0;

    *edu_file = FALSE;
  } else {
    *x0                     = window[0];
    *y0                     = window[1];
    *xsize                  = window[2];
    *ysize                  = window[3];
    if (*xsize == 0) *xsize = xSBsize + *x0;
    if (*ysize == 0) *ysize = ySBsize + *y0;
    *extra_mask             = Read_with_extra ? window[4] : 0;

    *edu_file = window[TOONZWINDOW_COUNT - 1] & 1;
  }
  if (!TIFFGetField(tfp, TIFFTAG_XPOSITION, &xposition)) xposition = 8.0;
  if (!TIFFGetField(tfp, TIFFTAG_ORIENTATION, &orientation))
    orientation = ORIENTATION_TOPLEFT;
  switch (orientation) {
    CASE ORIENTATION_BOTLEFT
        : __OR ORIENTATION_BOTRIGHT
          : __OR ORIENTATION_TOPLEFT
            : __OR ORIENTATION_TOPRIGHT
              : if (!TIFFGetField(tfp, TIFFTAG_XRESOLUTION, &dpi)) dpi = 0.0;
    CASE ORIENTATION_LEFTBOT
        : __OR ORIENTATION_RIGHTBOT
          : __OR ORIENTATION_LEFTTOP
            : __OR ORIENTATION_RIGHTTOP
              : if (!TIFFGetField(tfp, TIFFTAG_YRESOLUTION, &dpi)) dpi = 0.0;
  }
  *h_pos = (xposition - 8.0) * dpi;
}

/*---------------------------------------------------------------------------*/

static int get_photometric(TIFF *tif, USHORT *pm) {
  if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, pm)) return FALSE;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_resolutions(TIFF *tif, double *x_dpi, double *y_dpi) {
  float xdpi, ydpi;
  // USHORT resunit;

  if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xdpi) ||
      !TIFFGetField(tif, TIFFTAG_YRESOLUTION, &ydpi)) {
    *x_dpi = *y_dpi = 0.0;
    return FALSE;
  }
  *x_dpi = (double)xdpi;
  *y_dpi = (double)ydpi;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_orientation(TIFF *tif, int *orientation) {
  if (!TIFFGetField(tif, TIFFTAG_ORIENTATION, &orientation)) return FALSE;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_rows_per_strip(TIFF *tif, long *rowperstrip) {
  if (!TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowperstrip)) return FALSE;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_compression(TIFF *tif, int *compression) {
  if (!TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression)) return FALSE;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_tag_software(TIFF *tif, char *tag_software) {
  if (!TIFFGetField(tif, TIFFTAG_SOFTWARE, tag_software)) return FALSE;
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static void get_planarconfig(TIFF *tif, USHORT *planarconfig) {
  TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconfig);
}

/*---------------------------------------------------------------------------*/

static int get_history(TIFF *tif, char **history) {
  if (!TIFFGetField(tif, TIFFTAG_TOONZHISTORY, history)) return FALSE;
  //*history = strsave(*history);
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_image(TIFF *tf, IMAGE *image) {
  USHORT planarconfig;
  // int i;

  TIFFGetField(tf, TIFFTAG_PLANARCONFIG, &planarconfig);
  if (planarconfig == PLANARCONFIG_SEPARATE) {
    // tmsg_error("separate buffer image not supported yet in .tz(up) files");
    return FALSE;
  }
  switch (image->type) {
    CASE CMAPPED : return get_image_contig_16(tf, image);
    CASE CMAPPED24 : return get_image_contig_24(tf, image);
  DEFAULT:
    abort();
  }
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int get_image_contig_16(TIFF *tf, IMAGE *image) {
  int x, y, x0, y0, x1, y1, lx, ly, xsize, ysize, wrap;
  USHORT *buf, *pix, default_val;
  TBOOL swap_needed;

  swap_needed = scanline_needs_swapping(tf);

  x0          = image->pixmap.xD;
  y0          = image->pixmap.yD;
  lx          = image->pixmap.xSBsize;
  ly          = image->pixmap.ySBsize;
  x1          = x0 + lx - 1;
  y1          = y0 + ly - 1;
  xsize       = image->pixmap.xsize;
  ysize       = image->pixmap.ysize;
  wrap        = image->pixmap.xsize;
  buf         = image->pixmap.buffer;
  default_val = image->cmap.info.default_val;
  for (y = 0; y < y0; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < xsize; x++) *pix++ = default_val;
  }
  for (; y <= y1; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < x0; x++) *pix++ = default_val;
    if (TIFFReadScanline(tf, (UCHAR *)pix, y - y0, 0) < 0) {
      static int gia_dato = FALSE;
      if (!gia_dato) {
        // tmsg_error("bad data read on line %d", y);
        gia_dato = TRUE;
      }
      memset(pix, 0, lx * sizeof(*pix));
    }
    if (swap_needed) TIFFSwabArrayOfShort((USHORT *)pix, lx);
    pix += lx;
    for (x = x1 + 1; x < xsize; x++) *pix++ = default_val;
  }
  for (; y < ysize; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < xsize; x++) *pix++ = default_val;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_image_contig_24(TIFF *tf, IMAGE *image) {
  int x, y, x0, y0, x1, y1, lx, ly, xsize, ysize, wrap;
  ULONG *buf, *pix, default_val;
  TBOOL swap_needed;

  swap_needed = scanline_needs_swapping(tf);

  x0          = image->pixmap.xD;
  y0          = image->pixmap.yD;
  lx          = image->pixmap.xSBsize;
  ly          = image->pixmap.ySBsize;
  x1          = x0 + lx - 1;
  y1          = y0 + ly - 1;
  xsize       = image->pixmap.xsize;
  ysize       = image->pixmap.ysize;
  wrap        = image->pixmap.xsize;
  buf         = (ULONG *)image->pixmap.buffer;
  default_val = image->cmap.info.default_val;
  for (y = 0; y < y0; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < xsize; x++) *pix++ = default_val;
  }
  for (; y <= y1; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < x0; x++) *pix++ = default_val;
    if (TIFFReadScanline(tf, (UCHAR *)pix, y - y0, 0) < 0) {
      static int gia_dato = FALSE;
      if (!gia_dato) {
        // tmsg_error("bad data read on line %d", y);
        gia_dato = TRUE;
      }
      memset(pix, 0, lx * sizeof(*pix));
    }
    if (swap_needed) TIFFSwabArrayOfLong((TUINT32 *)pix, lx);
    pix += lx;
    for (x = x1 + 1; x < xsize; x++) *pix++ = default_val;
  }
  for (; y < ysize; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < xsize; x++) *pix++ = default_val;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_icon_16(TIFF *tfp, IMAGE *image) {
  int lx, ly;
  UCHAR *buffer;
  USHORT *icon_buffer;
  int scanline, row;
  TBOOL swap_needed;

  if (!TIFFReadDirectory(tfp)) return FALSE;
  if (!get_image_sizes(tfp, &lx, &ly)) return FALSE;
  swap_needed = scanline_needs_swapping(tfp);

  scanline = TIFFScanlineSize(tfp);

  icon_buffer = new USHORT[lx * ly];

  if (!icon_buffer) return FALSE;
  image->icon.buffer  = icon_buffer;
  image->icon.xsize   = lx;
  image->icon.ysize   = ly;
  image->icon.xSBsize = lx;
  image->icon.ySBsize = ly;
  image->icon.xD      = 0;
  image->icon.yD      = 0;

  buffer = (UCHAR *)image->icon.buffer;
  for (row = 0; row < image->icon.ysize; row++) {
    if (TIFFReadScanline(tfp, buffer, row, 0) < 0) {
      // tmsg_error("bad data read on line %d", row);
      return FALSE;
    }
    if (swap_needed) TIFFSwabArrayOfShort((USHORT *)buffer, lx);

    buffer += scanline;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_icon_24(TIFF *tfp, IMAGE *image) {
  int lx, ly;
  UCHAR *buffer;
  ULONG *icon_buffer;
  int scanline, row;
  TBOOL swap_needed;

  if (!TIFFReadDirectory(tfp)) return FALSE;
  if (!get_image_sizes(tfp, &lx, &ly)) return FALSE;
  swap_needed = scanline_needs_swapping(tfp);

  scanline = TIFFScanlineSize(tfp);

  icon_buffer = new ULONG[lx * ly];
  if (!icon_buffer) return FALSE;
  image->icon.buffer  = (USHORT *)icon_buffer;
  image->icon.xsize   = lx;
  image->icon.ysize   = ly;
  image->icon.xSBsize = lx;
  image->icon.ySBsize = ly;
  image->icon.xD      = 0;
  image->icon.yD      = 0;

  buffer = (UCHAR *)image->icon.buffer;
  for (row = 0; row < image->icon.ysize; row++) {
    if (TIFFReadScanline(tfp, buffer, row, 0) < 0) {
      // tmsg_error("bad data read on line %d", row);
      return FALSE;
    }
    if (swap_needed) TIFFSwabArrayOfLong((TUINT32 *)buffer, lx);

    buffer += scanline;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static int get_icon(TIFF *tfp, IMAGE *image) {
  switch (image->type) {
    CASE CMAPPED : return get_icon_16(tfp, image);
    CASE CMAPPED24 : return get_icon_24(tfp, image);
  DEFAULT:
    abort();
  }
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static int get_extra(TIFF *tfp, IMAGE *image) {
  int x, y, x0, y0, x1, y1, lx, ly, xsize, ysize, wrap;
  UCHAR *buf, *pix;

  if (!TIFFReadDirectory(tfp)) return FALSE;

  x0    = image->pixmap.xD;
  y0    = image->pixmap.yD;
  lx    = image->pixmap.xSBsize;
  ly    = image->pixmap.ySBsize;
  x1    = x0 + lx - 1;
  y1    = y0 + ly - 1;
  xsize = image->pixmap.xsize;
  ysize = image->pixmap.ysize;
  wrap  = image->pixmap.xsize;
  buf   = image->pixmap.extra;
  for (y = 0; y < y0; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < xsize; x++) *pix++ = 0;
  }
  for (; y <= y1; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < x0; x++) *pix++ = 0;
    if (TIFFReadScanline(tfp, (UCHAR *)pix, y - y0, 0) < 0) {
      static int gia_dato = FALSE;
      if (!gia_dato) {
        // tmsg_error("bad data read on extra line %d", y);
        gia_dato = TRUE;
      }
      memset(pix, 0, lx * sizeof(*pix));
    }
    pix += lx;
    for (x = x1 + 1; x < xsize; x++) *pix++ = 0;
  }
  for (; y < ysize; y++) {
    pix = buf + y * wrap;
    for (x = 0; x < xsize; x++) *pix++ = 0;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static void get_plt_name(char *filename, char *pltname) {
  // sprintf (pltname, "%s.plt", tim_get_name_nodot((const char*)filename));
}

/*---------------------------------------------------------------------------*/

static void clear_image_buffer_16(IMAGE *img, USHORT bg_val) {
  clear_image_region_buffer_16(img->pixmap.buffer, 0, 0, img->pixmap.xsize,
                               img->pixmap.ysize, img->pixmap.xsize, bg_val);
}

/*---------------------------------------------------------------------------*/

static void clear_image_buffer_24(IMAGE *img, ULONG bg_val) {
  clear_image_region_buffer_24((ULONG *)img->pixmap.buffer, 0, 0,
                               img->pixmap.xsize, img->pixmap.ysize,
                               img->pixmap.xsize, bg_val);
}

/*---------------------------------------------------------------------------*/

static void clear_extra(IMAGE *img, UCHAR bg_val) {
  clear_extra_region(img->pixmap.extra, 0, 0, img->pixmap.xsize,
                     img->pixmap.ysize, img->pixmap.xsize, bg_val);
}

/*---------------------------------------------------------------------------*/

static void clear_image_region_buffer_16(USHORT *buffer, int x0, int y0,
                                         int xsize, int ysize, int wrap_x,
                                         USHORT bg_val) {
  USHORT *tmp = new USHORT[xsize];
  int x, y, bytes;

  if (!tmp) return;

  for (x = 0; x < xsize; x++) tmp[x] = bg_val;

  bytes = xsize * sizeof(USHORT);
  buffer += x0 + y0 * wrap_x;
  for (y = 0; y < ysize; y++) {
    memcpy(buffer, tmp, bytes);
    buffer += wrap_x;
  }
  delete[] tmp;
}

/*---------------------------------------------------------------------------*/

static void clear_image_region_buffer_24(ULONG *buffer, int x0, int y0,
                                         int xsize, int ysize, int wrap_x,
                                         ULONG bg_val) {
  ULONG *tmp = new ULONG[xsize];
  int x, y, bytes;

  if (!tmp) return;

  for (x = 0; x < xsize; x++) tmp[x] = bg_val;

  bytes = xsize * sizeof(ULONG);
  buffer += x0 + y0 * wrap_x;
  for (y = 0; y < ysize; y++) {
    memcpy(buffer, tmp, bytes);
    buffer += wrap_x;
  }
  delete tmp;
}

/*---------------------------------------------------------------------------*/

static void clear_extra_region(UCHAR *extra, int x0, int y0, int xsize,
                               int ysize, int wrap_x, UCHAR bg_val) {
  int y;

  extra += x0 + y0 * wrap_x;
  for (y = 0; y < ysize; y++) {
    memset(extra, bg_val, xsize);
    extra += wrap_x;
  }
}
