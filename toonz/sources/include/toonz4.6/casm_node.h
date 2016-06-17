#pragma once

#ifndef _CASM_NODE_DEFINED_H
#define _CASM_NODE_DEFINED_H

#include "raster.h"
#include "casm.h"
#include "tmacro.h"
#include "affine.h"
#include "toonzproc.h"

#define CASM_NODE_HD                                                           \
  struct CASM_NODEPROC *proc;                                                  \
  char bg_flag, bad_branch;                                                    \
  ULONG mask;                                                                  \
  int label;

#define CASM_UNARY_NODE_HD                                                     \
  CASM_NODE_HD                                                                 \
  CASM_NODE *arg;

#define CASM_BINARY_NODE_HD                                                    \
  CASM_NODE_HD                                                                 \
  CASM_NODE *arg_dn, *arg_up;                                                  \
  int marker_dn, marker_up;

#define CASM_N_ARY_NODE_HD                                                     \
  CASM_NODE_HD                                                                 \
  int num_args;                                                                \
  CASM_NODE **args;

typedef struct CASM_NODE { CASM_NODE_HD } CASM_NODE;

typedef struct CASM_TILE {
  RASTER r;
  int x1, y1, x2, y2;
} CASM_TILE;

typedef struct CASM_NODESIZE {
  float x1, y1, x2, y2;
  float sbx1, sby1, sbx2, sby2;
} CASM_NODESIZE;

typedef struct USR_IMAGE {
  void *buffer;
  enum img_type type;
  int wrap, lx, ly;
  struct {
    LPIXEL *buffer;
    int offset, size;
  } cmap;
} USR_IMAGE;

enum CASM_NODE_TYPE {
  OVR_TRANSF = 0,
  ADD_TRANSF,
  BACKLIT_TRANSF,
  MATTE_TRANSF,
  WARP_TRANSF,
  BLUR_TRANSF,
  MBLUR_TRANSF,
  UDIT_TRANSF,
  READ_TRANSF,
  FLIPX_TRANSF,
  FLIPY_TRANSF,
  EXTOP_TRANSF,
  COLOR_TRANSF,
  COLORCARD_TRANSF,
  AFFINE_TRANSF,
  SHARPEN_TRANSF,
  LOCALBLUR_TRANSF,
  LOCALTRANSP_TRANSF,
  BODY_SHADOW_TRANSF,
  REFLECTION_TRANSF,
  CASM_NODE_TYPE_HOW_MANY
};

typedef int CASM_COMPUTE_FUNCTION(CASM_NODE *node, int x1, int y1, int x2,
                                  int y2, float dx, float dy, RASTER *raster,
                                  CASM_TILE *out);

typedef void CASM_CE_FUNCTION(float *pars, int cx, int cy, RASTER *rin,
                              RASTER *rout, int shrink);

typedef struct CASM_RENDER_INFO {
  float dx, dy;
  int shrink;
  int frame;
  int column;
} CASM_RENDER_INFO;

typedef void CASM_CLC_FUNCTION(float *pars, CASM_RENDER_INFO *info,
                               RASTER *rout);

typedef int CASM_COMPARE_FUNCTION(CASM_NODE *n1, CASM_NODE *n2);

typedef void CASM_WRITE_FUNCTION(CASM_NODE *node, T_CHAN fp);

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODEPROC {
  CASM_COMPUTE_FUNCTION *compute;
  CASM_COMPARE_FUNCTION *compare;
  CASM_WRITE_FUNCTION *write;
  int record_size;
} CASM_NODEPROC;

/*----------------------------------------------------------------------------*/

typedef struct CASM_FRAME {
  struct CASM_FRAME *next;
  CASM_NODE *node;
  char *filename;
} CASM_FRAME;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_ADD {
  CASM_BINARY_NODE_HD
  int diff_values;
  float value;
  TBOOL is_add, dont_use_matte;
} CASM_NODE_ADD;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_LOCALBLUR {
  CASM_BINARY_NODE_HD
  float value;
} CASM_NODE_LOCALBLUR;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_LOCALTRANSP {
  CASM_BINARY_NODE_HD
  float value;
} CASM_NODE_LOCALTRANSP;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_BLUR {
  CASM_UNARY_NODE_HD
  float blur;
  int backlit;
} CASM_NODE_BLUR;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_MBLUR {
  CASM_UNARY_NODE_HD
  double bx;
  double by;
} CASM_NODE_MBLUR;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_BODY_SHADOW {
  CASM_UNARY_NODE_HD
  TPOINT p;
  int blur;
  float transp;
  TBOOL is_highlight;
} CASM_NODE_BODY_SHADOW;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_REFLECTION {
  CASM_UNARY_NODE_HD
  float stretch_x;
  float stretch_y;
  float skew;
  float blur_min;
  float blur_max;
  float transp_min;
  float transp_max;
  TBOOL is_shadow;
} CASM_NODE_REFLECTION;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_SHARPEN {
  CASM_UNARY_NODE_HD
  int value;
} CASM_NODE_SHARPEN;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_PREMULTIPLY {
  CASM_UNARY_NODE_HD
} CASM_NODE_PREMULTIPLY;

/*----------------------------------------------------------------------------*/

typedef struct OLD_USR_IMAGE {
  void *buffer;
  enum img_type type;
  int wrap, lx, ly;
  struct {
    LPIXEL *buffer;
    int offset, size;
  } cmap;
} OLD_USR_IMAGE;

typedef struct CASM_NODE_UDIT {
  CASM_N_ARY_NODE_HD
  char *keyword;
  char *op_name;
  char *type;
  int argc;
  char **argv;
  void (*op_function)(OLD_USR_IMAGE *img_in,
                      OLD_USR_IMAGE *img_out, /* only for old udit*/
                      int argc, char **argv);
  int plugin_index;
  int fx_index;
} CASM_NODE_UDIT;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_MATTE {
  CASM_BINARY_NODE_HD
  int revert;
} CASM_NODE_MATTE;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_WARP {
  CASM_BINARY_NODE_HD
  int grid_step;
  float intensity;
} CASM_NODE_WARP;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_OVR { CASM_BINARY_NODE_HD } CASM_NODE_OVR;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_BACKLIT {
  CASM_BINARY_NODE_HD
  int diff_values;
  float value;
} CASM_NODE_BACKLIT;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_READ {
  CASM_NODE_HD
  char *filename;
  char *cmapname;
  char *op_name;
  int *no_ink, *no_paint;
  TBOOL keep_inks, keep_paints;
  int premultiply;
  int out_size;
  int argc;
  char **argv;
  void (*op_function)(OLD_USR_IMAGE *img_in, OLD_USR_IMAGE *img_out, int argc,
                      char **argv);
  enum img_type type;
  int xsize, ysize, border;
  int sbx1, sby1, sbx2, sby2;
  double h_pos;
  UCHAR patch;
  CASM_WINDOW *bg_info;
  int occurrence;
  int full_read;
} CASM_NODE_READ;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_COLOR {
  CASM_UNARY_NODE_HD
  char *keyword;
  int plugin_index;
  int fx_index;
  int num_pars;
  float *pars;
} CASM_NODE_COLOR;

/*----------------------------------------------------------------------------*/
typedef struct CASM_NODE_COLORCARD {
  CASM_NODE_HD
  char *keyword;
  int plugin_index;
  int fx_index;
  int num_pars;
  float *pars;
} CASM_NODE_COLORCARD;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_AFFINE {
  CASM_UNARY_NODE_HD
  AFFINE aff;
  enum CASM_RESAMPLE_TYPE resample_type;
} CASM_NODE_AFFINE;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_S_FLIP {
  CASM_UNARY_NODE_HD
  double c;
} CASM_NODE_S_FLIP;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_EXTOP {
  CASM_UNARY_NODE_HD
  char *op_filename, *format_in, *format_out, *arguments;
} CASM_NODE_EXTOP;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_NO_INK {
  CASM_UNARY_NODE_HD
  int which_ink;
} CASM_NODE_NO_INK;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_UNARY { CASM_UNARY_NODE_HD } CASM_NODE_UNARY;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_BINARY { CASM_BINARY_NODE_HD } CASM_NODE_BINARY;

/*----------------------------------------------------------------------------*/

typedef struct CASM_NODE_N_ARY { CASM_N_ARY_NODE_HD } CASM_NODE_N_ARY;

/*----------------------------------------------------------------------------*/

#endif
