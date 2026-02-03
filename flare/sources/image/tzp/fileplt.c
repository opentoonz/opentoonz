

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "toonz.h"
#include "tmsg.h"
#include "file.h"
#include "tiff.h"
#include "tiffio.h"
#include "version.h"
#include "history.h"
#include "colorsdb.h"
#include "ImageP/img_security.h"

#define LATEST_PLT_TYPE 4

static int Next_img_read_plt_without_buffer = FALSE;
static int Read_without_buffer              = FALSE;
#define SET_READ_WITHOUT_BUFFER                                                \
  {                                                                            \
    Read_without_buffer              = Next_img_read_plt_without_buffer;       \
    Next_img_read_plt_without_buffer = FALSE;                                  \
  }

#ifdef VECCHIA_MANIERA
static char *build_string_names(IMAGE *image);
static void set_color_names(IMAGE *image, char *names);
static char *plt_find_color_name(IMAGE *img, int color);
static char *plt_find_pencil_name(IMAGE *img, int color);
static void plt_set_color_name(IMAGE *img, int color, char *name);
static void plt_set_pencil_name(IMAGE *img, int pencil, char *name);
#endif

/*---------------------------------------------------------------------------*/

int img_write_plt(char *filename, IMAGE *image) {
  TIFF *tfp;
  UCHAR *buffer;
  int i, j, scanline, width, rows_per_strip;
  LPIXEL *cmap, *color, *pencil;
  USHORT palette[TOONZPALETTE_COUNT];
  char *names, *history;
  int plt_type, cmap_size;

  CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR

  names = NIL;

  if (image->type != CMAP) return FALSE;

  tfp = TIFFOpen(filename, "w");
  if (!tfp) return FALSE;

  plt_type = (image->cmap.offset > 0) ? 4 : 3;

  if (!image->cmap.info.n_colors) {
    assert(plt_type == 4);
    image->cmap.info           = Tcm_old_default_info;
    image->cmap.info.n_colors  = image->cmap.color_n;
    image->cmap.info.n_pencils = image->cmap.pencil_n;
    cmap_size                  = TCM_MIN_CMAP_BUFFER_SIZE(image->cmap.info);
  } else {
    cmap_size = TCM_CMAP_BUFFER_SIZE(image->cmap.info);
  }
  palette[0]  = plt_type;
  palette[1]  = image->cmap.offset;
  palette[2]  = cmap_size <= 0xffff ? cmap_size : 0;
  palette[3]  = 0 /* image->cmap.info.tone_offs */;
  palette[4]  = image->cmap.info.tone_bits;
  palette[5]  = image->cmap.info.color_offs;
  palette[6]  = image->cmap.info.color_bits;
  palette[7]  = image->cmap.info.pencil_offs;
  palette[8]  = image->cmap.info.pencil_bits;
  palette[9]  = image->cmap.info.offset_mask;
  palette[10] = image->cmap.info.n_colors;
  palette[11] = image->cmap.info.n_pencils;

  for (i = 12; i < TOONZPALETTE_COUNT - 1; i++) palette[i] = 0;

  palette[TOONZPALETTE_COUNT - 1] = (Img_license_attr & TA_TOONZ_EDU) != 0;

  TIFFSetField(tfp, TIFFTAG_TOONZPALETTE, palette);

  if (plt_type == 3) {
    assert(image->cmap.color && image->cmap.pencil);
    width = image->cmap.color_n + image->cmap.pencil_n;
  } else {
    if (image->cmap.color && image->cmap.pencil)
      width = cmap_size + image->cmap.color_n + image->cmap.pencil_n;
    else
      width = cmap_size;
  }
  TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 4);
  TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(tfp, TIFFTAG_IMAGELENGTH, 1);
  TIFFSetField(tfp, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

#ifdef VECCHIA_MANIERA

  names_pointer = (TREE *)image->cmap.names;
  image->cmap.names =
      cdb_encode_names((TREE *)image->cmap.names, &(image->cmap.names_max));
  str                  = build_string_names(image);
  names_string_pointer = (char *)image->cmap.names;
  image->cmap.names    = (USHORT *)names_pointer;
  if (names_string_pointer) free(names_string_pointer);

#endif

  names = cdb_encode_all(image->cmap.names);

  TIFFSetField(tfp, TIFFTAG_TOONZCOLORNAMES, names);

  /* Aggiungo History */
  history = build_history();
  if (image->history) {
    switch (check_history(image->history, history)) {
      CASE APPEND : image->history = append_history(image->history, history);
      CASE REPLACE : image->history =
          replace_last_history(image->history, history);
    DEFAULT:
      printf("Internal error: bad history type; aborting...\n");
      abort();
    }
    free(history);
  } else
    image->history = history;

  TIFFSetField(tfp, TIFFTAG_TOONZHISTORY, image->history);

  scanline = TIFFScanlineSize(tfp);

  /*
* massima lunghezza di bytes in una strip e' 8k
* vedi Graphics File Formats pag.48
*/
  rows_per_strip = (8 * 1024) / scanline;

  TIFFSetField(tfp, TIFFTAG_ROWSPERSTRIP,
               rows_per_strip == 0 ? 1L : rows_per_strip);

  TMALLOC(buffer, scanline);
  cmap   = image->cmap.buffer;
  color  = image->cmap.color;
  pencil = image->cmap.pencil;

  switch (plt_type) {
    CASE 1
        : printf(
              "img_rwite_plt error: type 1 .plt's are not written any more\n");
    abort();
    CASE 2 : __OR 4 : for (i = 0, j = 0; i < cmap_size; i++, j += 4) {
      buffer[j + 0] = cmap[i].r;
      buffer[j + 1] = cmap[i].g;
      buffer[j + 2] = cmap[i].b;
      buffer[j + 3] = cmap[i].m;
    }
    if (width > cmap_size) {
      for (i = 0; i < image->cmap.color_n; i++, j += 4) {
        buffer[j + 0] = color[i].r;
        buffer[j + 1] = color[i].g;
        buffer[j + 2] = color[i].b;
        buffer[j + 3] = color[i].m;
      }
      for (i = 0; i < image->cmap.pencil_n; i++, j += 4) {
        buffer[j + 0] = pencil[i].r;
        buffer[j + 1] = pencil[i].g;
        buffer[j + 2] = pencil[i].b;
        buffer[j + 3] = pencil[i].m;
      }
    }
    CASE 3 : for (i = 0, j = 0; i < image->cmap.color_n; i++, j += 4) {
      buffer[j + 0] = color[i].m; /* different from type 2 */
      buffer[j + 1] = color[i].b;
      buffer[j + 2] = color[i].g;
      buffer[j + 3] = color[i].r;
    }
    for (i = 0; i < image->cmap.pencil_n; i++, j += 4) {
      buffer[j + 0] = pencil[i].m;
      buffer[j + 1] = pencil[i].b;
      buffer[j + 2] = pencil[i].g;
      buffer[j + 3] = pencil[i].r;
    }
  DEFAULT:
    assert(!"bad palette type");
  }

  if (TIFFWriteScanline(tfp, buffer, 0, 0) < 0) goto bad;
  TFREE(buffer);
  TIFFClose(tfp);
  TFREE(names);
  return TRUE;

bad:
  TFREE(buffer);
  TIFFClose(tfp);
  TFREE(names);
  return FALSE;
}

/*===========================================================================*/

void next_img_read_plt_without_buffer(void) {
  Next_img_read_plt_without_buffer = TRUE;
}

/*---------------------------------------------------------------------------*/

IMAGE *img_read_plt(char *filename) {
  int with_cmap_buffer;
  IMAGE *image = NIL;
  TIFF *tfp;
  UCHAR *buffer = NIL;
  int i, j, scanline;
  LPIXEL *cmap;
  USHORT *palette; /*  [TOONZPALETTE_COUNT] */
  USHORT spp, bps, comp, plan_con, photom;
  int width, height;
  char *names;
  int plt_type, m, cmap_file_size, cmap_alloc_size;
  int colpen_cmap, colbuf_alloc_size, penbuf_alloc_size;
  int max_n_colors, max_n_pencils;
  int act_n_colors, act_n_pencils, index;
  LPIXEL grey, black;
  TBOOL edu_file;

  CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR

  SET_READ_WITHOUT_BUFFER

  tfp = TIFFOpen(filename, "r");
  if (!tfp) return NIL;

  TIFFGetField(tfp, TIFFTAG_TOONZPALETTE, &palette);
  TIFFGetField(tfp, TIFFTAG_TOONZCOLORNAMES, &names);

  TIFFGetField(tfp, TIFFTAG_BITSPERSAMPLE, &bps);
  TIFFGetField(tfp, TIFFTAG_SAMPLESPERPIXEL, &spp);
  TIFFGetField(tfp, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tfp, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tfp, TIFFTAG_COMPRESSION, &comp);
  TIFFGetField(tfp, TIFFTAG_PLANARCONFIG, &plan_con);
  TIFFGetField(tfp, TIFFTAG_PHOTOMETRIC, &photom);

  if (photom != PHOTOMETRIC_RGB || plan_con != PLANARCONFIG_CONTIG) goto bad;

  edu_file = palette[TOONZPALETTE_COUNT - 1] & 1;
  if (edu_file && !(Img_license_attr & TA_TOONZ_EDU)) {
    char str[1024];
    BUILD_EDU_ERROR_STRING(str)
    tmsg_error(str);
    goto bad;
  }

  scanline = TIFFScanlineSize(tfp);

  image = new_img();
  if (!image) goto bad;
  image->type = CMAP;

  /* Leggo history */
  if (!TIFFGetField(tfp, TIFFTAG_TOONZHISTORY, &image->history))
    image->history = "";
  image->history   = strsave(image->history);

  plt_type             = palette[0];
  image->cmap.offset   = palette[1];
  image->cmap.color_n  = palette[10];
  image->cmap.pencil_n = palette[11];

  image->cmap.info = Tcm_old_default_info;
  if (palette[6]) {
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
  }
  image->cmap.info.default_val =
      (image->cmap.info.n_tones - 1) | image->cmap.info.offset_mask;

  cmap_alloc_size   = TCM_CMAP_BUFFER_SIZE(image->cmap.info);
  cmap_file_size    = palette[2] ? palette[2] : cmap_alloc_size;
  colpen_cmap       = image->cmap.info.n_tones > 16;
  colbuf_alloc_size = TCM_CMAP_COLBUFFER_SIZE(image->cmap.info);
  penbuf_alloc_size = TCM_CMAP_PENBUFFER_SIZE(image->cmap.info);

  TMALLOC(buffer, scanline);
  if (!buffer) goto bad;

  with_cmap_buffer = !Read_without_buffer || (plt_type <= 2) || (plt_type == 4);
  if (with_cmap_buffer) {
    if (colpen_cmap) {
      TMALLOC(image->cmap.colbuffer, colbuf_alloc_size);
      TMALLOC(image->cmap.penbuffer, penbuf_alloc_size);
      if (!image->cmap.colbuffer || !image->cmap.penbuffer) goto bad;
    } else {
      TMALLOC(image->cmap.buffer, cmap_alloc_size);
      if (!image->cmap.buffer) goto bad;
    }
  }
  TCALLOC(image->cmap.color, 1 << image->cmap.info.color_bits);
  if (!image->cmap.color) goto bad;
  TCALLOC(image->cmap.pencil, 1 << image->cmap.info.pencil_bits);
  if (!image->cmap.pencil) goto bad;

  if (TIFFReadScanline(tfp, buffer, 0, 0) < 0) goto bad;

  switch (plt_type) {
    CASE 1 : if (!with_cmap_buffer) {
      TCALLOC(image->cmap.buffer, cmap_alloc_size)
      if (!image->cmap.buffer) goto bad;
    }
    for (i = 0, j = 0; i < cmap_file_size; i++, j += 4) {
      image->cmap.buffer[i].r = buffer[j + 0];
      image->cmap.buffer[i].g = buffer[j + 1];
      image->cmap.buffer[i].b = buffer[j + 2];
    }
    to_new_cmap(&image->cmap);
    set_colors_and_pencils(&image->cmap);
    if (!with_cmap_buffer) TFREE(image->cmap.buffer)

    CASE 2 : __OR 4
             : if (with_cmap_buffer) for (i = 0, j = 0; i < cmap_file_size;
                                          i++, j += 4) {
      image->cmap.buffer[i].r = buffer[j + 0];
      image->cmap.buffer[i].g = buffer[j + 1];
      image->cmap.buffer[i].b = buffer[j + 2];
      image->cmap.buffer[i].m = buffer[j + 3];
    }
    else j = 4 * cmap_file_size;

    if (width > cmap_file_size) {
      for (i = 0; i < image->cmap.color_n; i++, j += 4) {
        image->cmap.color[i].r = buffer[j + 0];
        image->cmap.color[i].g = buffer[j + 1];
        image->cmap.color[i].b = buffer[j + 2];
        image->cmap.color[i].m = buffer[j + 3];
      }
      for (i = 0; i < image->cmap.pencil_n; i++, j += 4) {
        image->cmap.pencil[i].r = buffer[j + 0];
        image->cmap.pencil[i].g = buffer[j + 1];
        image->cmap.pencil[i].b = buffer[j + 2];
        image->cmap.pencil[i].m = buffer[j + 3];
      }
    } else
      set_colors_and_pencils(&image->cmap);

    CASE 3 : for (i = 0, j = 0; i < image->cmap.color_n; i++, j += 4) {
      image->cmap.color[i].m = buffer[j + 0]; /* different from type 2 */
      image->cmap.color[i].b = buffer[j + 1];
      image->cmap.color[i].g = buffer[j + 2];
      image->cmap.color[i].r = buffer[j + 3];
    }
    for (i = 0; i < image->cmap.pencil_n; i++, j += 4) {
      image->cmap.pencil[i].m = buffer[j + 0];
      image->cmap.pencil[i].b = buffer[j + 1];
      image->cmap.pencil[i].g = buffer[j + 2];
      image->cmap.pencil[i].r = buffer[j + 3];
    }
    if (with_cmap_buffer)
      if (colpen_cmap) {
        memset(image->cmap.colbuffer, 0, colbuf_alloc_size * sizeof(LPIXEL));
        memset(image->cmap.penbuffer, 0, penbuf_alloc_size * sizeof(LPIXEL));
        fill_cmap_colbuffer(image->cmap.colbuffer, image->cmap.info,
                            image->cmap.color, FALSE);
        fill_cmap_penbuffer(image->cmap.penbuffer, image->cmap.info,
                            image->cmap.pencil, FALSE);
      } else {
        memset(image->cmap.buffer, 0, cmap_alloc_size * sizeof(LPIXEL));
        fill_cmap_buffer(image->cmap.buffer, image->cmap.info,
                         image->cmap.color, image->cmap.pencil, FALSE);
      }
  DEFAULT:
    goto bad;
  }

#ifdef VECCHIA_MANIERA

  set_color_names(image, names);
  names_pointer = image->cmap.names;
  image->cmap.names =
      (USHORT *)cdb_decode_names(image->cmap.names, image->cmap.names_max);
  TFREE(names_pointer);

#endif

  image->cmap.names = cdb_decode_all(names, image->cmap.info);

  /* estendo la palette */

  max_n_colors  = 1 << image->cmap.info.color_bits;
  max_n_pencils = 1 << image->cmap.info.pencil_bits;
  if (max_n_colors > image->cmap.info.n_colors ||
      max_n_pencils > image->cmap.info.n_pencils) {
    act_n_colors               = image->cmap.info.n_colors;
    act_n_pencils              = image->cmap.info.n_pencils;
    image->cmap.info.n_colors  = max_n_colors;
    image->cmap.info.n_pencils = max_n_pencils;
    image->cmap.color_n        = image->cmap.info.n_colors;
    image->cmap.pencil_n       = image->cmap.info.n_pencils;
    grey.r = grey.g = grey.b = 127;
    grey.m                   = 255;
    black.r = black.g = black.b = 0;
    black.m                     = 255;
    for (i = act_n_colors; i < max_n_colors; i++) {
      image->cmap.color[i] = grey;
      index                = TCM_COLOR_INDEX(image->cmap.info, i);
      cdb_set_group(image, index, UNUSED_COLOR_PAGE_NAME);
      if (with_cmap_buffer && colpen_cmap)
        fill_cmap_colramp(image->cmap.colbuffer, image->cmap.info,
                          image->cmap.color[i], i, FALSE);
    }
    for (i = act_n_pencils; i < max_n_pencils; i++) {
      image->cmap.pencil[i] = black;
      index                 = TCM_PENCIL_INDEX(image->cmap.info, i);
      cdb_set_group(image, index, UNUSED_COLOR_PAGE_NAME);
      if (with_cmap_buffer && colpen_cmap)
        fill_cmap_penramp(image->cmap.penbuffer, image->cmap.info,
                          image->cmap.pencil[i], i, FALSE);
    }
    if (with_cmap_buffer && !colpen_cmap)
      fill_cmap_buffer(image->cmap.buffer, image->cmap.info, image->cmap.color,
                       image->cmap.pencil, FALSE);
  }

  if (buffer) TFREE(buffer);
  TIFFClose(tfp);
  return image;

bad:
  if (image) free_img(image);
  TFREE(buffer);
  TIFFClose(tfp);
  return NIL;
}

/*---------------------------------------------------------------------------*/

IMAGE *img_read_plt_info(char *filename) {
  IMAGE *image = NIL;
  TIFF *tfp;
  USHORT *palette; /*  [TOONZPALETTE_COUNT] */
  USHORT plan_con, photom;
  char *names;
  int max_n_colors, max_n_pencils;
  TBOOL edu_file;

  CHECK_IMAGEDLL_LICENSE_AND_GET_IMG_LICENSE_ATTR

  tfp = TIFFOpen(filename, "r");
  if (!tfp) return NIL;

  TIFFGetField(tfp, TIFFTAG_TOONZPALETTE, &palette);
  TIFFGetField(tfp, TIFFTAG_TOONZCOLORNAMES, &names);
  TIFFGetField(tfp, TIFFTAG_PLANARCONFIG, &plan_con);
  TIFFGetField(tfp, TIFFTAG_PHOTOMETRIC, &photom);

  if (photom != PHOTOMETRIC_RGB || plan_con != PLANARCONFIG_CONTIG) goto bad;

  edu_file = palette[TOONZPALETTE_COUNT - 1] & 1;
  if (edu_file && !(Img_license_attr & TA_TOONZ_EDU)) {
    char str[1024];
    BUILD_EDU_ERROR_STRING(str)
    tmsg_warning(str);
  }

  image = new_img();
  if (!image) goto bad;
  image->type = CMAP;

  if (palette[0] < 1 || palette[0] > LATEST_PLT_TYPE) goto bad;

  image->cmap.offset   = palette[1];
  image->cmap.color_n  = palette[10];
  image->cmap.pencil_n = palette[11];

  image->cmap.info = Tcm_old_default_info;
  if (palette[6]) {
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
  }
  image->cmap.info.default_val =
      (image->cmap.info.n_tones - 1) | image->cmap.info.offset_mask;

  /* Leggo history */
  if (!TIFFGetField(tfp, TIFFTAG_TOONZHISTORY, &image->history))
    image->history = "";
  image->history   = strsave(image->history);

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

  image->cmap.names = cdb_decode_all(names, image->cmap.info);

  TIFFClose(tfp);
  return image;

bad:
  if (image) free_img(image);
  TIFFClose(tfp);
  return NIL;
}

/*---------------------------------------------------------------------------*/
/* Attenzione: se si modifica questa funzione, occore riportare le
 *             variazioni nella libreria line x line (simg_plt.c)
 */

#ifdef VECCHIA_MANIERA

static char *plt_find_color_name(IMAGE *img, int color) {
  char *name;
  int index;

  index = TCM_COLOR_INDEX(img->cmap.info, color);
  name  = find_color_name(img, index);

  return name;
}

#endif

/*---------------------------------------------------------------------------*/
/* Attenzione: se si modifica questa funzione, occore riportare le
 *             variazioni nella libreria line x line (simg_plt.c)
 */

#ifdef VECCHIA_MANIERA

static char *plt_find_pencil_name(IMAGE *img, int pencil) {
  char *name;
  int index;

  index = TCM_PENCIL_INDEX(img->cmap.info, pencil);
  name  = find_color_name(img, index);

  return name;
}

#endif

/*---------------------------------------------------------------------------*/
/* Attenzione: se si modifica questa funzione, occore riportare le
 *             variazioni nella libreria line x line (simg_plt.c)
 */

#ifdef VECCHIA_MANIERA

static void plt_set_color_name(IMAGE *img, int color, char *name) {
  int index;

  index = TCM_COLOR_INDEX(img->cmap.info, color);
  set_color_name(img, index, name);
}

#endif

/*---------------------------------------------------------------------------*/
/* Attenzione: se si modifica questa funzione, occore riportare le
 *             variazioni nella libreria line x line (simg_plt.c)
 */

#ifdef VECCHIA_MANIERA

static void plt_set_pencil_name(IMAGE *img, int pencil, char *name) {
  int index;

  index = TCM_PENCIL_INDEX(img->cmap.info, pencil);
  set_color_name(img, index, name);
}

#endif

/*---------------------------------------------------------------------------*/
/* Attenzione: se si modifica questa funzione, occore riportare le
 *             variazioni nella libreria line x line (simg_plt.c)
 */

#ifdef VECCHIA_MANIERA

static char *build_string_names(IMAGE *image) {
  char *str, *name;
  int i, maxsize = 1000, ptr, len;

  str = (char *)malloc(maxsize);
  ptr = 0;

  for (i = 0; i < image->cmap.color_n; i++) {
    name = plt_find_color_name(image, i);
    if (name && *name) {
      len = strlen(name);
      if ((len + ptr) >= maxsize) {
        maxsize += 200;
        str = (char *)realloc(str, maxsize);
      }
      memmove(str + ptr, name, len);
      ptr += len;
    }
    if (ptr + 1 >= maxsize) {
      maxsize += 200;
      str = (char *)realloc(str, maxsize);
    }
    memmove(str + ptr, "|", 1);
    ptr++;
  }

  for (i = 0; i < image->cmap.pencil_n; i++) {
    name = plt_find_pencil_name(image, i);
    if (name && *name) {
      len = strlen(name);
      if ((len + ptr) >= maxsize) {
        maxsize += 200;
        str = (char *)realloc(str, maxsize);
      }
      memmove(str + ptr, name, len);
      ptr += len;
    }
    if (ptr + 1 >= maxsize) {
      maxsize += 200;
      str = (char *)realloc(str, maxsize);
    }
    memmove(str + ptr, "|", 1);
    ptr++;
  }

  str[ptr] = 0;
  return str;
}

#endif

/*---------------------------------------------------------------------------*/
/* Attenzione: se si modifica questa funzione, occore riportare le
 *             variazioni nella libreria line x line (simg_plt.c)
 */

#ifdef VECCHIA_MANIERA

static void set_color_names(IMAGE *image, char *names) {
  int i;
  char *s, *name;

  s = names;
  i = 0;
  while (*s) {
    name = NULL;
    if (*s != '|') {
      name = s;
      while (*s && *s != '|') s++;
      *s = 0;
    }

    if (name) {
      if (i < image->cmap.info.n_colors)
        plt_set_color_name(image, i, name);
      else
        plt_set_pencil_name(image, i - image->cmap.info.n_colors, name);
    }
    i++;
    s++;
  }
}

#endif
