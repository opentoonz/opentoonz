#pragma once

#ifndef _FILE_H_
#define _FILE_H_

#include <stdio.h>
#include "img.h"

#undef TNZAPI
#undef TNZVAR
#ifdef TNZ_IS_IMAGELIB
#define TNZAPI TNZ_EXPORT_API
#define TNZVAR TNZ_EXPORT_VAR
#else
#define TNZAPI TNZ_IMPORT_API
#define TNZVAR TNZ_IMPORT_VAR
#endif

/* to avoid collision with img_read and img_write in libimage.a */
#define img_read img_read_xxx
#define img_write img_write_xxx

/* byte-ordering dependent i/o */
TNZAPI int fread_long(long *ptr, FILE *stream);
TNZAPI int fread_short(short *ptr, FILE *stream);
TNZAPI int fwrite_long(long *ptr, FILE *stream);
TNZAPI int fwrite_short(short *ptr, FILE *stream);

#ifndef __LIBSIMAGE__

TNZAPI void next_img_read_plt_without_buffer(void);
TNZAPI void next_img_read_with_extra(void);
TNZAPI void next_img_read_tzr_cmapped(void);

TNZAPI void img_enable_64_bits_input(int enable);
TNZAPI int img_type_current_bpp(char *type);

TNZAPI IMAGE *img_read(char *filename), *img_read_ciak(char *filename),
    *img_read_ciak_icon(char *filename), *img_read_rgb(char *filename),
    *img_read_png(char *filename), *img_read_tga(char *filename),
    /* *img_read_bw    (char *filename),        */
    *img_read_cmap(char *filename, IMAGE *image), *img_read_cmap_1(IMAGE *img),
    *img_read_rla(char *filename), *img_read_sdl(char *filename),
    *img_read_yuv(char *filename), *img_read_qtl(char *filename),
    *img_read_qnt(char *filename), *img_read_gif(char *filename),
    *img_read_tif(char *filename),
    /* *img_read_rl7   (char *filename),       */
    *img_read_pic(char *filename), *img_read_tzup(char *filename),
    *img_read_tzup_icon(char *filename), *img_read_jpg(char *filename),
#ifndef WIN32
    *img_read_cpg(char *filename),
#else
#ifndef ISALPHA
    *img_read_pct(char *filename),
#endif
#endif
    *img_read_plt(char *filename), *img_read_bmp(char *filename),
    *img_read_vpb(char *filename), *img_read_tzr(char *filename);

TNZAPI IMAGE *img_read_region(char *filename, int x1, int y1, int x2, int y2,
                              int scale),
    *img_read_region_ciak(char *filename, int x1, int y1, int x2, int y2,
                          int scale),
    *img_read_region_rgb(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_png(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_rla(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_tif(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_tzup(char *filename, int x1, int y1, int x2, int y2,
                          int scale),
    *img_read_region_tga(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_pic(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_gif(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_yuv(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_sdl(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_qnt(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_qtl(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_jpg(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
#ifndef WIN32
    *img_read_region_cpg(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
#else
#ifndef ISALPHA
    *img_read_region_pct(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
#endif
#endif
    *img_read_region_bmp(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_vpb(char *filename, int x1, int y1, int x2, int y2,
                         int scale),
    *img_read_region_tzr(char *filename, int x1, int y1, int x2, int y2,
                         int scale);

TNZAPI IMAGE *img_read_info(char *filename),
    *img_read_ciak_info(char *filename), *img_read_rgb_info(char *filename),
    *img_read_png_info(char *filename), *img_read_tga_info(char *filename),
    /* *img_read_bw_info  (char *filename),    */
    *img_read_cmap_info(char *filename), *img_read_tif_info(char *filename),
    *img_read_yuv_info(char *filename), *img_read_sdl_info(char *filename),
    *img_read_qnt_info(char *filename), *img_read_qtl_info(char *filename),
    *img_read_rla_info(char *filename), *img_read_gif_info(char *filename),
    /* *img_read_rl7_info (char *filename),    */
    *img_read_pic_info(char *filename), *img_read_tzup_info(char *filename),
    *img_read_jpg_info(char *filename),
#ifndef WIN32
    *img_read_cpg_info(char *filename),
#else
#ifndef ISALPHA
    *img_read_pct_info(char *filename),
#endif
#endif
    *img_read_plt_info(char *filename), *img_read_bmp_info(char *filename),
    *img_read_vpb_info(char *filename), *img_read_tzr_info(char *filename);

TNZAPI IMAGE *img_read_colormap(char *filename);

TNZAPI int img_write(char *filename, IMAGE *image),
    img_write_ciak(char *filename, IMAGE *image),
    img_write_rla(char *filename, IMAGE *image),
    /* img_write_bw  (char *filename, IMAGE *image),    */
    img_write_rgb(char *filename, IMAGE *image),
    img_write_png(char *filename, IMAGE *image),
    img_write_tif(char *filename, IMAGE *image),
    img_write_yuv(char *filename, IMAGE *image),
    img_write_sdl(char *filename, IMAGE *image),
    img_write_qtl(char *filename, IMAGE *image),
    img_write_qnt(char *filename, IMAGE *image),
    img_write_tga(char *filename, IMAGE *image),
    img_write_gif(char *filename, IMAGE *image),
    /* img_write_rl7 (char *filename, IMAGE *image),    */
    img_write_pic(char *filename, IMAGE *image),
    img_write_tzup(char *filename, IMAGE *image),
    img_write_jpg(char *filename, IMAGE *image),
#ifndef WIN32
    img_write_cpg(char *filename, IMAGE *image),
#else
#ifndef ISALPHA
    img_write_pct(char *filename, IMAGE *image),
#endif
#endif
    img_write_plt(char *filename, IMAGE *image),
    img_write_bmp(char *filename, IMAGE *image),
    img_write_vpb(char *filename, IMAGE *image),
    img_write_tzr(char *filename, IMAGE *image);

#endif /* __LIBSIMAGE__ */

#endif /* _FILE_H_ */
