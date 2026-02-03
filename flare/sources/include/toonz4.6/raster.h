#pragma once

#ifndef _RASTER_H_
#define _RASTER_H_

#include "tmacro.h"
//#include "img.h"
#include "tcm.h"
//#include "affine.h"
#include "machine.h"
#include "pixel.h"

#include <string>

#undef TNZAPI
#ifdef TNZ_IS_RASTERLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

typedef enum {
  RAS_NONE,
  RAS_GR8,    /* grey tones, 8 bits */
  RAS_GR16,   /* grey tones, 16 bits */
  RAS_RGBM,   /* LPIXEL, matte channel considered */
  RAS_RGBM64, /* SPIXEL, matte channel considered */
  RAS_RGB_64, /* SPIXEL, matte channel ignored */
  RAS_CM32,   /* cmapped, 12+12+8 bits (ink, paint, ramp) */
  RAS_HOW_MANY
} RAS_TYPE;

#ifdef VECCHIO
typedef enum {
  RAS_NONE,
  RAS_BW,       // 1 bit, B=0, W=1, first pixel in bit 7
  RAS_WB,       // "    , W=0, B=1, "
  RAS_GR8,      // grey tones, 8 bits
  RAS_CM8,      // color-mapped,  8 bits
  RAS_GR16,     // grey tones, 16 bits
  RAS_RGB16,    // RGB 5+6+5 bits, red most significant
  RAS_RLEBW,    // 1 bit compressed(quick casm format)
  RAS_RGB,      // 1 byte per color channel/
  RAS_RGB_,     // LPIXEL, matte channel ignored, for display
  RAS_RGBM,     // LPIXEL, matte channel considered
  RAS_RGBM64,   // SPIXEL, matte channel considered
  RAS_RGB_64,   // SPIXEL, matte channel ignored
  RAS_MBW16,    // 16 images of 1 bit per pixel, B=0, W=1
  RAS_CM8S4,    // cmapped,  8 bits, standard SGI  16-color colormap
  RAS_CM8S8,    // cmapped,  8 bits, standard SGI 256-color colormap
  RAS_CM16S4,   // cmapped, 16 bits, standard SGI  16-color colormap
  RAS_CM16S8,   // cmapped, 16 bits, standard SGI 256-color colormap
  RAS_CM16S12,  // cmapped, 16 bits, standard SGI+Toonz 4096-color colormap
  // RAS_CM24, // cmapped, 8+8+8 bits (ink, paint, ramp), +8 bits extra (MSB)
  RAS_CM16,  // color-mapped, 16 bits
  RAS_CM32,  // cmapped, 12+12+8 bits (ink, paint, ramp)
  RAS_HOW_MANY
} RAS_TYPE;
#endif

typedef struct {
  LPIXEL *buffer; /* colormap buffer for <= 16 bits or for 32 bits (otherwise
                     NIL) */
  TCM_INFO info;  /* toonz colormap info, see tcm.h */
} RAS_CMAP;

typedef struct _RASTER {
  RAS_TYPE type;       /* image type */
  void *native_buffer; /* allocated buffer, if != NIL free this one */
  void *buffer;        /* (sub)image start address */
  UCHAR *native_extra; /* allocated extra, if != NIL free this one */
  UCHAR *extra;        /* (sub)extra information (patches...) start addr */
  UCHAR extra_mask;    /* used extra bits in buffer (for CM24) or extra */
  int bit_offs;        /* for bw/wb only: bit offset of first pixel */
  int multimask;       /* for multibw only: ith bit==1 -> img in ith pos */
  int wrap;            /* pixel wrap for y direction */
  int lx, ly;          /* x and y lengths */
  RAS_CMAP cmap;
  char *cacheId;
  int cacheIdLength;
} RASTER;

typedef enum {
  FLT_NONE,

  FLT_TRIANGLE, /* triangle filter */
  FLT_MITCHELL, /* Mitchell-Netravali filter */
  FLT_CUBIC_5,  /* cubic convolution, a = .5 */
  FLT_CUBIC_75, /* cubic convolution, a = .75 */
  FLT_CUBIC_1,  /* cubic convolution, a = 1 */
  FLT_HANN2,    /* Hann window, rad = 2 */
  FLT_HANN3,    /* Hann window, rad = 3 */
  FLT_HAMMING2, /* Hamming window, rad = 2 */
  FLT_HAMMING3, /* Hamming window, rad = 3 */
  FLT_LANCZOS2, /* Lanczos window, rad = 2 */
  FLT_LANCZOS3, /* Lanczos window, rad = 3 */
  FLT_GAUSS,    /* Gaussian convolution */

  FLT_HOW_MANY
} FLT_TYPE;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define RASRAS(type1, type2) ((type1)*RAS_HOW_MANY + (type2))

#define RASRASRAS(type1, type2, type3) RASRAS(RASRAS((type1), (type2)), (type3))

#define RASRASRASRAS(type1, type2, type3, type4)                               \
  RASRAS(RASRASRAS((type1), (type2), (type3)), (type4))

/*===========================================================================*/

/* About extra information:
 * If the raster is of type RAS_CM24, the extra information is contained
 * in the extra upper byte (XUB) of the longword containing pixel information.
 * For all other buffer types, the extra information is contained in the extra
 * buffer, which is a byte buffer. If a bit in the extra_mask is 0, then all
 * corresponding bits of the extra are to be considered as 0, even if they are
 * not 0. In other words, the extra_mask must be ANDed with all extra bytes
 * before they are used. If the whole extra_mask is 0, then the extra buffer
 * may be NIL (unless the type is RAS_CM24, in which case the extra buffer
 * is always NIL because the XUB is used).
 * No raster operation may change the extra_mask of an argument.
 * Unless otherwise stated, raster operations having one input raster
 * and one output raster copy or transform those bits of extra information
 * which are set in both input and output extra_masks, and zero those bits
 * which are set in the output extra_mask but not in the input extra_mask.
 * All other raster operations leave the extra information of the output
 * raster untouched (unless otherwise stated).
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef LEVO

/* The RASTER structures in these functions must have already been allocated
 * (can be automatic vars)
 */
TNZAPI void create_raster(RASTER *raster, int xsize, int ysize, RAS_TYPE type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* also allocates the extra buffer if needed
 */
TNZAPI void create_raster_with_extra(RASTER *raster, int xsize, int ysize,
                                     RAS_TYPE type, UCHAR extra_mask);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* does NOT allocate a buffer for rout, only adjusts the pointer in the buffer
 * (and maybe in the extra buffer) of rin.
 */
TNZAPI int create_subraster(RASTER *rin, RASTER *rout, int x0, int y0, int x1,
                            int y1);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* DOES allocate a new buffer (and maybe a new extra buffer),
 * but does NOT create [a] new colormap buffer[s].
 */
TNZAPI void clone_raster(RASTER *rin, RASTER *rout);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void convert_raster(RASTER *r, RAS_TYPE type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* The RASTER structure is zeroed but not freed (can be an automatic var)
 * It is recommended to use release_raster if the raster was created with
 * the function create_raster.
 * Also calls rop_clear_extra()
 */
TNZAPI void release_raster(RASTER *raster);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Zeroes extra_mask and releases the extra buffer if present.
 */
TNZAPI void rop_clear_extra(RASTER *raster);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Zeroes the patch bits in extra_mask and releases the extra buffer
 * if extra_mask becomes zero and the extra buffer exists.
 */
TNZAPI void rop_clear_patches(RASTER *raster);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Zeroes the non-patch bits in extra_mask and releases the extra buffer
 * if extra_mask becomes zero and the extra buffer exists.
 */
TNZAPI void rop_clear_extra_but_not_patches(RASTER *raster);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* The following two functions do NOT copy image buffers, nor extra buffers.
 * For cmapped images, the pointers to the cmap buffers are left to NIL
 * unless build_cmap is TRUE, in which case [a] new cmap buffer[s] is/are
 * allocated and filled.
 */
TNZAPI void rop_image_to_raster(IMAGE *img, RASTER *ras, int build_cmap);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_subimage_to_raster(IMAGE *img, int img_x0, int img_y0,
                                   int img_x1, int img_y1, RASTER *ras,
                                   int build_cmap);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* number of bits per pixel
 */
TNZAPI int rop_pixbits(RAS_TYPE rastertype);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* number of bytes per pixel (rounded up)
 */
TNZAPI int rop_pixbytes(RAS_TYPE rastertype);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* whether or not there may be unused bits at the end of the line
 */
TNZAPI int rop_fillerbits(RAS_TYPE rastertype);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI RAS_TYPE rop_standard_ras_type_of_pix_type(PIX_TYPE pix_type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI PIX_TYPE rop_pix_type_of_ras_type(RAS_TYPE ras_type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* premultiply fullcolor rasters; lx and ly must be equal.
 */
TNZAPI void rop_premultiply(RASTER *rin, RASTER *rout);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* copy region delimited by (x1,y1) and (x2,y2) onto region
 * with first pixel in (newx,newy);
 * when used to color quantize images, image buffer of rout may be same
 * of rin;
 * also calls rop_copy_extra (or acts equivalently).
 */
TNZAPI void rop_copy(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                     int newx, int newy);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_copy, but only on the extra information
 * (extra buffers may be NIL).
 */
TNZAPI void rop_copy_extra(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                           int y2, int newx, int newy);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* copy region delimited by (x1,y1) and (x2,y2) onto region
 * with first pixel in (newx,newy),
 * mirroring horizontally if mirror is odd,
 * then rotating counterclockwise by given multiple of ninety (may be <0).
 */
TNZAPI void rop_copy_90(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                        int y2, int newx, int newy, int mirror, int ninety);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_copy, but reduce by the given linear factor by subsampling.
 */

TNZAPI void rop_shrink(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                       int y2, int newx, int newy, int factor);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_copy, but zoom out by averaging some of the pixels
 */
TNZAPI void rop_zoom_out(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                         int y2, int newx, int newy, int abs_zoom_level);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* see both rop_zoom_out and rop_copy_90 (combined effect)
 */
TNZAPI void rop_zoom_out_90(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                            int y2, int newx, int newy, int abs_zoom_level,
                            int mirror, int ninety);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* clear region delimited by (x1,y1) and (x2,y2) (extra is not altered)
 */
TNZAPI void rop_clear(RASTER *r, int x1, int y1, int x2, int y2);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* does not alter nor consider extra_mask,
 * simply returns if type is not RAS_CM24 and the extra buffer is NIL.
 */
void rop_and_extra_bits(RASTER *ras, UCHAR and_mask, int x1, int y1, int x2,
                        int y2);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_sharpen(RASTER *rin, RASTER *rout, int sharpen_max_corr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* perform a female matte operation, the result raster may coincide with
 * the bottom raster,lx and ly are the ones of the result raster,
 * rasters must have a matte channel.
 * female: pixmatte.m == 0 -> pixmatted.m = 0.
 */
TNZAPI void rop_female_matte(RASTER *rmatted, RASTER *rmatte, RASTER *rres);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* perform a male matte operation, the result raster may coincide with
 * the bottom raster, lx and ly are the ones of the result raster,
 * rasters must have a matte channel.
 * male: pixmatte.m == 0 -> pixmatted.m = 255.
 */
TNZAPI void rop_male_matte(RASTER *rmatted, RASTER *rmatte, RASTER *rres);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* overlay a raster on a color (a colorcard),
 * the result raster may coincide with the bottom raster,
 * lx and ly are the ones of the result raster.
 * From the point of view of extra information, behaves like a function
 * with three raster arguments.
 */
TNZAPI void rop_ovr_rgbm(int r, int g, int b, int m, RASTER *rtop,
                         RASTER *rovr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* overlay two rasters, the result raster may coincide with the bottom raster,
 * lx and ly are the ones of the result raster,
 * if the bottom raster is opaque but is RAS_RGBM, it should be temporarily
 * set to RAS_RGB_ for increased speed.
 */
TNZAPI void rop_ovr_2(RASTER *rbot, RASTER *rtop, RASTER *rovr);

/* overlay two rasters, one of them is a greytones one.
 * the result raster may coincide with the bottom raster,
 * lx and ly are the ones of the result raster,
 * if the bottom raster is opaque but is RAS_RGBM, it should be temporarily
 * set to RAS_RGB_ for increased speed.
 */

TNZAPI void rop_ovr_gr_2(RASTER *rbot, RASTER *rtop, RASTER *rovr,
                         LPIXEL color);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_ovr_2, but only those patches which correspond to set bits in
 * botmask and topmask are selected, unselected pixels are set to transparent
 * (rop_ovr_2 is the special case with both masks set to 0xff) (masks are not
 * to be confused with extra_masks, which have only 3 bits for patches).
 */
TNZAPI void rop_ovr_patches(RASTER *rbot, UCHAR botmask, RASTER *rtop,
                            UCHAR topmask, RASTER *rovr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_ovr_patches with a transparent second buffer:
 * only those patches which correspond to set bits in mask are selected.
 * Only non-patch extra information is copied, provided that the corresponding
 * bits are set in rout->extra_mask.
 */
TNZAPI void rop_filter_patches(RASTER *rin, UCHAR mask, RASTER *rout);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* overlay 3 rasters, the result raster may coincide with the bottom raster,
 * lx and ly are the ones of the result raster,
 * if the bottom raster is opaque but is RAS_RGBM, it should be temporarily
 * set to RAS_RGB_ for increased speed.
 */
TNZAPI void rop_ovr_3(RASTER *rbot, RASTER *rmid, RASTER *rtop, RASTER *rovr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* overlay n rasters, the result raster may coincide with the bottom raster,
 * rtop[n-1] is the topmost raster,
 * lx and ly are the ones of the result raster,
 * if the bottom raster is opaque but is RAS_RGBM, it should be temporarily
 * set to RAS_RGB_ for increased speed.
*/
TNZAPI void rop_ovr_n(RASTER *rbot, RASTER **rtop, int n, RASTER *rovr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_ovr_2, but zooming out at the same time (see rop_zoom_out)
 */
TNZAPI void rop_zoom_out_ovr_2(RASTER *rbot, RASTER *rtop, RASTER *rovr,
                               int abs_zoom_level);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_ovr_3, but zooming out at the same time (see rop_zoom_out)
 */
TNZAPI void rop_zoom_out_ovr_3(RASTER *rbot, RASTER *rmid, RASTER *rtop,
                               RASTER *rovr, int abs_zoom_level);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_ovr_n, but zooming out at the same time (see rop_zoom_out)
*/
TNZAPI void rop_zoom_out_ovr_n(RASTER *rbot, RASTER **rtop, int n, RASTER *rovr,
                               int abs_zoom_level);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_ovr_2, but simply computes 1/4*bottom + 3/4*top;
 * if the bottom buffer is NIL, white is assumed
 */
TNZAPI void rop_onion(RASTER *rbot, RASTER *rtop, RASTER *rovr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* like rop_zoom_out_ovr_2, but simply computes 1/4*bottom + 3/4*top;
 * if the bottom buffer is NIL, white is assumed
 */
TNZAPI void rop_zoom_out_onion(RASTER *rbot, RASTER *rtop, RASTER *rovr,
                               int abs_zoom_level);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* convolve with a 3x3 matrix, mapping point (dx,dy) onto (0,0),
 * using floating point calculations
 */
TNZAPI void rop_conv_3(RASTER *rin, RASTER *rout, int dx, int dy,
                       double conv[]);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* convolve with a 3x3 matrix, mapping point (dx,dy) onto (0,0),
 * using integer calculations
 */
TNZAPI void rop_conv_3_i(RASTER *rin, RASTER *rout, int dx, int dy,
                         double conv[]);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* move by dx in x direction and by dy in y direction
 */
TNZAPI void rop_fracmove(RASTER *rin, RASTER *rout, double dx, double dy);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* scale by sx in x direction and by sy in y direction,
 * mapping point (cxin,cyin) onto (cxout,cyout),
 * using given filtertype and blur factor (1 means normal filter width)
 */
TNZAPI void rop_scale(RASTER *rin, RASTER *rout, double sx, double sy,
                      double cxin, double cyin, double cxout, double cyout,
                      FLT_TYPE filtertype, double blur);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* rotate by angle in radians (infinite range), mapping point (cxin,cyin)
 * onto (cxout,cyout)
 */
TNZAPI void rop_rotate(RASTER *rin, RASTER *rout, double angle, double cxin,
                       double cyin, double cxout, double cyout);

TNZAPI void rop_rotate_sin(RASTER *rin, RASTER *rout, double sin, double cos,
                           double cxin, double cyin, double cxout,
                           double cyout);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void get_flt_fun_rad(FLT_TYPE flt_type, double (**flt_fun)(double),
                            double *flt_rad);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_resample(RASTER *rin, RASTER *rout, AFFINE aff,
                         FLT_TYPE flt_type, double blur);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_resample_tcm(RASTER *rin, RASTER *rout, AFFINE aff,
                             FLT_TYPE flt_type, double blur, TCM_INFO tcm_info);

/*---------------------------------------------------------------------------*/

TNZAPI void rop_add_white_to_cmap(RASTER *ras);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_remove_white_from_cmap(RASTER *ras);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI LPIXEL premult_lpixel(LPIXEL lpixel);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI LPIXEL unpremult_lpixel(LPIXEL lpixel);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_fill_cmap_ramp(RASTER *ras, TCM_INFO info, LPIXEL color,
                               LPIXEL pencil, int color_index, int pencil_index,
                               int already_premultiplied);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_fill_cmap_colramp(RASTER *ras, TCM_INFO info, LPIXEL color,
                                  int color_index, int already_premultiplied);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_fill_cmap_penramp(RASTER *ras, TCM_INFO info, LPIXEL pencil,
                                  int pencil_index, int already_premultiplied);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_custom_fill_cmap_ramp(RASTER *ras, TCM_INFO info, LPIXEL color,
                                      LPIXEL pencil, int color_index,
                                      int pencil_index,
                                      int already_premultiplied,
                                      int *custom_tone);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_custom_fill_cmap_colramp(RASTER *ras, TCM_INFO info,
                                         LPIXEL color, int color_index,
                                         int already_premultiplied,
                                         int *custom_tone);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_custom_fill_cmap_penramp(RASTER *ras, TCM_INFO info,
                                         LPIXEL pencil, int pencil_index,
                                         int already_premultiplied,
                                         int *custom_tone);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_fill_cmap_buffer(RASTER *ras, TCM_INFO info, LPIXEL *color,
                                 LPIXEL *pencil, int already_premultiplied);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_fill_cmap_colbuffer(RASTER *ras, TCM_INFO info, LPIXEL *color,
                                    int already_premultiplied);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_fill_cmap_penbuffer(RASTER *ras, TCM_INFO info, LPIXEL *pencil,
                                    int already_premultiplied);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_custom_fill_cmap_buffer(RASTER *ras, TCM_INFO info,
                                        LPIXEL *color, LPIXEL *pencil,
                                        int already_premultiplied,
                                        int *custom_tone);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_custom_fill_cmap_colbuffer(RASTER *ras, TCM_INFO info,
                                           LPIXEL *color,
                                           int already_premultiplied,
                                           int *custom_tone);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

TNZAPI void rop_custom_fill_cmap_penbuffer(RASTER *ras, TCM_INFO info,
                                           LPIXEL *pencil,
                                           int already_premultiplied,
                                           int *custom_tone);

/*---------------------------------------------------------------------------*/

TNZAPI TDRECT rop_transform_region(TDRECT src, AFFINE placement);

TNZAPI void rop_lineart_fullcolor(RASTER *r, LPIXEL *colors, int num_colors,
                                  TBOOL only_edges);

TNZAPI void rop_lineart_palette(IMAGE *plt, int threshold, TBOOL only_edges);

/*---------------------------------------------------------------------------*/

/* N.B rin and rout can be the same */

TNZAPI void rop_mirror(RASTER *rin, RASTER *rout, TBOOL is_vertical);
#endif

#endif
