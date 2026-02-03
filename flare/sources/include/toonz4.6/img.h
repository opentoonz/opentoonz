#pragma once

#ifndef _IMG_H_
#define _IMG_H_

#include "tcm.h"
#include "machine.h"
#include "pixel.h"
#include "tnzmovie.h"

#undef TNZAPI
#ifdef TNZ_IS_IMAGELIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

#define IMG_ICON_WIDTH 100
#define IMG_ICON_HEIGHT 90

typedef void *(*MALLOC_TYPE)(int size);
typedef void *(*REALLOC_TYPE)(void *ptr, int size);
typedef void (*FREE_TYPE)(void *ptr);

/* Tipi di  IMAGE */
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

typedef enum { CM_NONEB = -1, CM_STANDARD, CM_CUSTOM } IMG_CM_ALGORITHM;

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

  TNZMOVIE_QUALITY pct_quality;
  TNZMOVIE_COMPRESSION pct_compression;

  /*---------------------------------------------------------------*/

  TNZMOVIE_QUALITY mov_quality;
  TNZMOVIE_COMPRESSION mov_compression;

  int avi_bpp;
  char *avi_compression;
} IMG_IO_SETTINGS;

/* Struttura IMAGE */
typedef struct {
  enum img_type type;
  char *filename;
  char *history;
  struct s_pixmap icon;   /* Icon */
  struct s_cmap cmap;     /* Colormap */
  struct s_pixmap pixmap; /* Pixel Map */
  IMG_IO_SETTINGS io_settings;
} IMAGE;

TNZAPI void img_set_mem_functions(MALLOC_TYPE new_malloc_func,
                                  REALLOC_TYPE new_realloc_func,
                                  FREE_TYPE new_free_func);
TNZAPI void img_get_mem_functions(MALLOC_TYPE *new_malloc_func,
                                  REALLOC_TYPE *new_realloc_func,
                                  FREE_TYPE *new_free_func);
TNZAPI IMAGE *new_img(void);
TNZAPI IMAGE *new_img_f(char *filename);
TNZAPI int allocate_pixmap(IMAGE *img, int xsize, int ysize);
TNZAPI int free_img(IMAGE *img);

TNZAPI int control_image(IMAGE *ctrl_img);
TNZAPI int make_icon(IMAGE *img, int rxsize, int rysize);
TNZAPI int make_icon_and_update_sb(IMAGE *img, int rxsize, int rysize);
TNZAPI int clear_cmap(IMAGE *img);
TNZAPI int attach_cmap(IMAGE *img, char *filename);
TNZAPI int force_to_rgb(IMAGE *img, char *path);
TNZAPI int rgb_to_grey(IMAGE *img, int cmap_size, int cmap_offset);
TNZAPI int apply_cmap(IMAGE *img);

TNZAPI void add_white(struct s_cmap *cmap);
TNZAPI void remove_white(struct s_cmap *cmap);
TNZAPI LPIXEL premult(LPIXEL color);
TNZAPI LPIXEL unpremult(LPIXEL color);
TNZAPI void check_premultiplied(struct s_cmap *cmap);
TNZAPI void fill_cmap_ramp(LPIXEL *buffer, TCM_INFO info, LPIXEL color,
                           LPIXEL pencil, int color_index, int pencil_index,
                           int already_premultiplied);
TNZAPI void fill_cmap_colramp(LPIXEL *colbuffer, TCM_INFO info, LPIXEL color,
                              int color_index, int already_premultiplied);
TNZAPI void fill_cmap_penramp(LPIXEL *penbuffer, TCM_INFO info, LPIXEL pencil,
                              int pencil_index, int already_premultiplied);
TNZAPI void fill_cmap_buffer(LPIXEL *buffer, TCM_INFO info, LPIXEL *color,
                             LPIXEL *pencil, int already_premultiplied);
TNZAPI void fill_cmap_colbuffer(LPIXEL *colbuffer, TCM_INFO info, LPIXEL *color,
                                int already_premultiplied);
TNZAPI void fill_cmap_penbuffer(LPIXEL *penbuffer, TCM_INFO info,
                                LPIXEL *pencil, int already_premultiplied);
TNZAPI void convert_cmap(struct s_cmap *cmap, TCM_INFO new_tcm);

/* funzioni per cmap di vecchio tipo */
TNZAPI void to_new_cmap(struct s_cmap *cmap);
TNZAPI void set_colors_and_pencils(struct s_cmap *cmap);

TNZAPI void apply_transparency_color(IMAGE *img, TBOOL according_setup_value);
TNZAPI void do_apply_transparency_color(IMAGE *img);

#endif
