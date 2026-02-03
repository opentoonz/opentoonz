#pragma once

#ifndef _READCASM_H_
#define _READCASM_H_

#include "toonzproc.h"
#include "casm_node.h"

#undef TNZAPI
#undef TNZVAR
#ifdef TNZ_IS_CASMLIB
#define TNZAPI TNZ_EXPORT_API
#define TNZVAR TNZ_EXPORT_VAR
#else
#define TNZAPI TNZ_IMPORT_API
#define TNZVAR TNZ_IMPORT_VAR
#endif

TNZAPI int n_compute(CASM_NODE *node, int x1, int y1, int x2, int y2, float dx,
                     float dy, RASTER *raster, CASM_TILE *out);

TNZAPI int n_compare(CASM_NODE *n1, CASM_NODE *n2);

TNZAPI void n_write(CASM_NODE *n, T_CHAN fp);

TNZAPI CASM_NODE *n_insert(CASM_NODE *node);

TNZAPI CASM_FRAME *readcasm(char *filename, int first_frame, int last_frame,
                            int frame_step, int shrink,
                            CASM_COMPUTE_FUNCTION **compute_functions,
                            TBOOL is_central_affine, TBOOL is_tv_licence);

TNZAPI int readcasm_get_camera_size(int *lx, int *ly);

TNZAPI char *readcasm_get_sink_script(char ***argv);

TNZAPI int readcasm_get_audio_info(int *from_audio, int *to_audio,
                                   int *from_video, char **audioname);

TNZAPI int readcasm_set_camera_size(int lx, int ly, int shrink);

TNZAPI int readcasm_get_field(void);

TNZAPI void readcasm_get_camera_dpi(double *camera_x_dpi, double *camera_y_dpi);

TNZAPI int readcasm_get_frames_n(void);

TNZAPI int is_binary_op(CASM_NODE *arg, CASM_NODE **arg2, CASM_NODE **arg3);

TNZAPI int is_special_binary_op(CASM_NODE *arg, CASM_NODE **arg2,
                                CASM_NODE **arg3);

TNZAPI int is_unary_op(CASM_NODE *arg, CASM_NODE **arg2);

TNZAPI int is_n_ary_op(CASM_NODE *arg, int *num_args, CASM_NODE ***args);

TNZAPI int is_affine_op(CASM_NODE *n, CASM_NODE **arg);

TNZAPI int get_affine_args(CASM_NODE *n, AFFINE *aff, CASM_NODE **arg);

TNZAPI CASM_NODE *create_affine_op(AFFINE *aff, CASM_NODE *n);

TNZAPI int n_compare_ric(CASM_NODE *tree1, CASM_NODE *tree2);

TNZAPI void make_border(RASTER *raster);

TNZAPI void readcasm_create_new_frame(CASM_NODE *node, char *filename);

TNZAPI void readcasm_do_for_all_frames(void (*fun)(CASM_FRAME *f));

TNZAPI void readcasm_touch_frames(void);

TNZAPI int is_read_op(CASM_NODE *arg);

TNZAPI int is_backlit_op(CASM_NODE *arg, CASM_NODE **arg2, CASM_NODE **arg3);

TNZAPI int n_compare_ric(CASM_NODE *tree1, CASM_NODE *tree2);

TNZAPI void make_border(RASTER *raster);

TNZAPI void reset_camera_size(void);

TNZAPI CASM_NODE *go_to_leaf_node(CASM_NODE *node);

TNZAPI void apply_extcmap(CASM_NODE *node, char *cmapname);

TNZAPI int readcasm_frame(CASM_FRAME **frame);

TNZAPI void init_readcasm_functions(CASM_COMPUTE_FUNCTION **casm_functions,
                                    int pipeid, int shrink);

TNZAPI void init_readcasm_input_channel(void);

TNZAPI CASM_FRAME *get_tree_pointer(void);

TNZAPI void readcasm_set_msg_trace(void);
TNZAPI TBOOL readcasm_open_log_file(char *casmname, char *filename);
TNZAPI TBOOL readcasm_close_log_file(void);
TNZAPI void readcasm_set_frame(int frame_num);
TNZAPI int readcasm_get_frame(void);

TNZAPI void readcasm_get_render_tile(int *x1, int *y1, int *x2, int *y2);

TNZAPI int is_matte_op(CASM_NODE *n, CASM_NODE **arg_dn, CASM_NODE **arg_up);
TNZAPI int is_localblur_op(CASM_NODE *n, CASM_NODE **arg_dn,
                           CASM_NODE **arg_up);
TNZAPI int is_localtransp_op(CASM_NODE *n, CASM_NODE **arg_dn,
                             CASM_NODE **arg_up);
TNZAPI int is_ovr_op(CASM_NODE *n, CASM_NODE **arg_dn, CASM_NODE **arg_up);
TNZAPI int is_add_op(CASM_NODE *n, CASM_NODE **arg_dn, CASM_NODE **arg_up);
TNZAPI int is_warp_op(CASM_NODE *n, CASM_NODE **arg_dn, CASM_NODE **arg_up);
TNZAPI int is_colorcard_op(CASM_NODE *n);
TNZAPI int get_add_args(CASM_NODE *n, CASM_NODE **arg_dn, CASM_NODE **arg_up,
                        float *value);
TNZAPI int there_is_colorcard(CASM_NODE *arg);
TNZAPI int there_is_standard_colorcard(CASM_NODE *arg);
TNZAPI int there_is_colorcard_with_ovr(CASM_NODE *arg);
TNZAPI TBOOL get_colorcard_rgbm(CASM_NODE *arg, LPIXEL *color);

TNZAPI enum CASM_RESAMPLE_TYPE get_resample_type(void);
TNZAPI void casm_set_resample_type(enum CASM_RESAMPLE_TYPE type);

TNZAPI void free_casmtree(CASM_NODE *n);
TNZAPI CASM_NODE *dup_casmtree(CASM_NODE *n);
TNZAPI CASM_NODE *make_subtree(CASM_NODE *n, int from_column, int to_column);
TNZAPI int split_tree(CASM_NODE *tree, CASM_NODE **lefttree,
                      CASM_NODE **righttree, int occurrence);
TNZAPI int writecasm(CASM_FRAME *frames_list, T_CHAN fp, int start_frame,
                     int write_header, TBOOL is_central_affine);
TNZAPI int get_level_list(CASM_NODE *tree, char ***levels);
TNZAPI CASM_NODE *change_leaf(CASM_NODE *tree, int occurrence,
                              char *newlevelname, char *newpalette, int dx,
                              int dy);
TNZAPI CASM_NODE *make_a_sandwich(CASM_NODE *bread_dn_tree, char *newlevelname,
                                  char *newpalette, int dx, int dy,
                                  CASM_NODE *bread_up_tree);

TNZAPI void write_raster(char *filename, RASTER *raster);
TNZAPI int read_raster(char *filename, RASTER *raster, int shrink, int border,
                       double *x_dpi, double *y_dpi);

/*
#undef  TNZAPI
#ifdef WIN32
#ifdef TNZ_IS_COMMONLIB
  #define TNZAPI TNZ_EXPORT_API
#else
  #define TNZAPI TNZ_IMPORT_API
#endif
#else
  #define TNZAPI extern
#endif
*/

TNZVAR int Is_casm;
TNZVAR int Always_scale_opt;

#endif
