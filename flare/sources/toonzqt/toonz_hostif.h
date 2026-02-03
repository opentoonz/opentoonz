#pragma once

#ifndef TOONZ_HOSTIF_H__
#define TOONZ_HOSTIF_H__

#include <stdint.h>
#include <stddef.h>
#include "toonz_plugin.h"
#include "toonz_params.h"

#define TOONZ_PLUGIN_CLASS_MODIFIER_MASK (0xffUL << (32 - 8))
#define TOONZ_PLUGIN_CLASS_MODIFIER_GEOMETRIC (1UL << (32 - 1))
#define TOONZ_PLUGIN_CLASS_POSTPROCESS_ (0x10UL)
#define TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB                                    \
  (TOONZ_PLUGIN_CLASS_POSTPROCESS_ | 0UL)

#define TOONZ_PORT_TYPE_RASTER (0)

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

static const toonz_UUID toonz_uuid_node__ = {0xCC14EA21, 0x13D8, 0x4A3B, 0x9375,
                                             0xAA4F68C9DDDD};
static const toonz_UUID toonz_uuid_port__ = {0x2F89A423, 0x1D2D, 0x433F, 0xB93E,
                                             0xCFFD83745F6F};
static const toonz_UUID toonz_uuid_tile__ = {0x882BD525, 0x937E, 0x427C, 0x9D68,
                                             0x4ECA651F6562};
static const toonz_UUID toonz_uuid_fx_node__ = {0x26F9FC53, 0x632B, 0x422F,
                                                0x87A0, 0x8A4547F55474};
static const toonz_UUID toonz_uuid_param__ = {0x2E3E4A55, 0x8539, 0x4520,
                                              0xA266, 0x15D32189EC4D};
static const toonz_UUID toonz_uuid_setup__ = {0xcfde9107, 0xc59d, 0x414c,
                                              0xae4a, 0x3d115ba97933};

#define TOONZ_UUID_NODE (&toonz_uuid_node__)
#define TOONZ_UUID_PORT (&toonz_uuid_port__)
#define TOONZ_UUID_TILE (&toonz_uuid_tile__)
#define TOONZ_UUID_FXNODE (&toonz_uuid_fx_node__)
#define TOONZ_UUID_PARAM (&toonz_uuid_param__)
#define TOONZ_UUID_SETUP (&toonz_uuid_setup__)

/* Host Interface */
struct toonz_host_interface_t_ {
  toonz_if_version_t ver;
  int (*query_interface)(const toonz_UUID *, void **);
  void (*release_interface)(void *);
} TOONZ_PACK;
typedef toonz_host_interface_t_ toonz_host_interface_t;

struct toonz_rect_t_ {
  double x0;
  double y0;
  double x1;
  double y1;
} TOONZ_PACK;

typedef struct toonz_rect_t_ toonz_rect_t;

typedef void *toonz_param_handle_t;

struct toonz_param_interface_t_ {
  toonz_if_version_t ver;
  int (*get_type)(toonz_param_handle_t param, double frame, int *type,
                  int *counts);
  int (*get_value)(toonz_param_handle_t param, double frame, int *pcounts,
                   void *pvalue);
  int (*set_value)(toonz_param_handle_t param, double frame, int counts,
                   const void *pvalue);
  int (*get_string_value)(toonz_param_handle_t param, int *wholesize,
                          int rcvbufsize, char *pvalue);
  int (*get_spectrum_value)(toonz_param_handle_t param, double frame, double x,
                            toonz_param_spectrum_t *pvalue);
} TOONZ_PACK;

typedef toonz_param_interface_t_ toonz_param_interface_t;

typedef void *toonz_fxnode_handle_t;

typedef void *toonz_port_handle_t;

struct toonz_port_interface_t_ {
  toonz_if_version_t ver;
  int (*is_connected)(toonz_port_handle_t port, int *is_connected);
  int (*get_fx)(toonz_port_handle_t port, toonz_fxnode_handle_t *fxnode);
} TOONZ_PACK;

typedef toonz_port_interface_t_ toonz_port_interface_t;

typedef void *toonz_tile_handle_t;

struct toonz_tile_interface_t_ {
  toonz_if_version_t ver;
  int (*get_raw_address_unsafe)(toonz_tile_handle_t handle, void **);
  int (*get_raw_stride)(toonz_tile_handle_t handle, int *stride);
  int (*get_element_type)(toonz_tile_handle_t handle, int *element);
  int (*copy_rect)(toonz_tile_handle_t handle, int left, int top, int width,
                   int height, void *dst, int dststride);

  int (*create_from)(toonz_tile_handle_t handle,
                     toonz_tile_handle_t *newhandle);
  int (*create)(toonz_tile_handle_t *newhandle);
  int (*destroy)(toonz_tile_handle_t handle);

  int (*get_rectangle)(toonz_tile_handle_t handle, toonz_rect_t *rect);
  int (*safen)(toonz_tile_handle_t handle);
} TOONZ_PACK;

/*	Pixel types of tile
        Correspond to TRaster32P/64P/GR8P/GR16P/GRDP/YUV422P
*/
enum {
  TOONZ_TILE_TYPE_NONE,
  TOONZ_TILE_TYPE_32P,
  TOONZ_TILE_TYPE_64P,
  TOONZ_TILE_TYPE_GR8P,
  TOONZ_TILE_TYPE_GR16P,
  TOONZ_TILE_TYPE_GRDP,
  TOONZ_TILE_TYPE_YUV422P,
};

typedef void *toonz_node_handle_t;

struct toonz_node_interface_t_ {
  toonz_if_version_t ver;
  int (*get_input_port)(toonz_node_handle_t node, const char *name,
                        toonz_port_handle_t *port);

  int (*get_rect)(toonz_rect_t *rect, double *x0, double *y0, double *x1,
                  double *y1);
  int (*set_rect)(toonz_rect_t *rect, double x0, double y0, double x1,
                  double y1);

  int (*get_param)(toonz_node_handle_t node, const char *name,
                   toonz_param_handle_t *param);

  int (*set_user_data)(toonz_node_handle_t node, void *user_data);
  int (*get_user_data)(toonz_node_handle_t node, void **user_data);

} TOONZ_PACK;

typedef toonz_node_interface_t_ toonz_node_interface_t;

typedef void *toonz_handle_t;
typedef toonz_tile_interface_t_ toonz_tile_interface_t;
struct toonz_affine_t_ {
  double a11;
  double a12;
  double a13;
  double a21;
  double a22;
  double a23;
} TOONZ_PACK;

typedef toonz_affine_t_ toonz_affine_t;

typedef void *toonz_node_handle_t;

struct toonz_rendering_setting_t_ {
  toonz_if_version_t ver;
  /** コピー元の TRenderSettings のスタック上アドレス */
  const void *context;
  /** エフェクトが描画された後に適用されるアフィン変換 */
  toonz_affine_t affine;
  /**	出力フレームのためのガンマ修飾子 */
  double gamma;
  /** 入力の fps を変動させる変数 */
  double time_stretch_from;
  /** 出力の fps を変動させる変数 */
  double time_stretch_to;
  /** stereoscpy(ステレオ投影?)のためのカメラのx軸方向のシフト量 (単位は inch)
   */
  double stereo_scopic_shift;
  /**
  出力フレームで要求される bits per pixel の値。
  この値は適切なタイプのタイルと共に付随しなければならない。
  */
  int bpp;
  /**
  描画プロセスの間のタイルのキャッシュされる最大サイズ(MegaByte単位)。
  タイルのFxの計算の再分割のために予測キャッシュマネージャに用いられる(?)
  */
  int max_tile_size;
  /** アフィン変換におけるフィルタの品質 */
  int quality;
  /** ??? */
  int field_prevalence;
  /** ステレオ投影(3D効果)が用いられているかどうか */
  int stereoscopic;
  /** 描画インスタンスが swatch viewer から発生したものかどうか */
  int is_swatch;
  /** 描画リクエストの際にユーザーが手動でキャッシュするかどうか */
  int user_cachable;
  /** 縮小を必ず考慮しなければならないかどうか */
  int apply_shrink_to_viewer;
  /** カメラサイズ */
  toonz_rect_t camera_box;
  /** 途中で Preview の計算がキャンセルされた時のフラグ */
  volatile int *is_canceled;
} TOONZ_PACK;

typedef toonz_rendering_setting_t_ toonz_rendering_setting_t;

struct toonz_nodal_rasterfx_handler_t_ {
  toonz_if_version_t ver;
  void (*do_compute)(toonz_node_handle_t node,
                     const toonz_rendering_setting_t *, double frame,
                     toonz_tile_handle_t tile);
  int (*do_get_bbox)(toonz_node_handle_t node,
                     const toonz_rendering_setting_t *, double frame,
                     toonz_rect_t *rect);
  int (*can_handle)(toonz_node_handle_t node, const toonz_rendering_setting_t *,
                    double frame);
  size_t (*get_memory_requirement)(toonz_node_handle_t node,
                                   const toonz_rendering_setting_t *,
                                   double frame, const toonz_rect_t *rect);

  void (*on_new_frame)(toonz_node_handle_t node,
                       const toonz_rendering_setting_t *, double frame);
  void (*on_end_frame)(toonz_node_handle_t node,
                       const toonz_rendering_setting_t *, double frame);

  int (*create)(toonz_node_handle_t node);
  int (*destroy)(toonz_node_handle_t node);
  int (*setup)(toonz_node_handle_t node);
  int (*start_render)(toonz_node_handle_t node);
  int (*end_render)(toonz_node_handle_t node);

  void *reserved_ptr_[5];
} TOONZ_PACK;

typedef toonz_nodal_rasterfx_handler_t_ toonz_nodal_rasterfx_handler_t;

struct toonz_fxnode_interface_t_ {
  toonz_if_version_t ver;
  /* TRasterFx の振る舞い */
  int (*get_bbox)(toonz_fxnode_handle_t fxnode,
                  const toonz_rendering_setting_t *, double frame,
                  toonz_rect_t *rect, int *get_bbox);
  int (*can_handle)(toonz_fxnode_handle_t fxnode,
                    const toonz_rendering_setting_t *, double frame,
                    int *can_handle);
  /* TFx の振る舞い */
  int (*get_input_port_count)(toonz_fxnode_handle_t fxnode, int *count);
  int (*get_input_port)(toonz_fxnode_handle_t fxnode, int index,
                        toonz_port_handle_t *port);
  int (*compute_to_tile)(toonz_fxnode_handle_t fxnode,
                         const toonz_rendering_setting_t *, double frame,
                         const toonz_rect_t *rect, toonz_tile_handle_t intile,
                         toonz_tile_handle_t outtile);
} TOONZ_PACK;

typedef struct toonz_fxnode_interface_t_ toonz_fxnode_interface_t;

struct toonz_setup_interface_t_ {
  toonz_if_version_t ver;
  int (*set_parameter_pages)(toonz_node_handle_t node, int num,
                             toonz_param_page_t *pages);
  int (*set_parameter_pages_with_error)(toonz_node_handle_t node, int num,
                                        toonz_param_page_t *pages, int *reason,
                                        void **position);
  int (*add_input_port)(toonz_node_handle_t node, const char *name, int type);

} TOONZ_PACK;
typedef struct toonz_setup_interface_t_ toonz_setup_interface_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(__cplusplus)
namespace toonz {
typedef toonz_rendering_setting_t rendering_setting_t;
typedef toonz_affine_t affine_t;
typedef toonz_rect_t rect_t;

typedef toonz_param_handle_t param_handle_t;

typedef toonz_node_handle_t node_handle_t;
typedef toonz_port_handle_t port_handle_t;
typedef toonz_tile_handle_t tile_handle_t;
typedef toonz_rendering_setting_t rendering_setting_t;
typedef toonz_fxnode_handle_t fxnode_handle_t;

typedef toonz_param_interface_t param_interface_t;

typedef toonz_node_interface_t node_interface_t;
typedef toonz_host_interface_t host_interface_t;
typedef toonz_tile_interface_t tile_interface_t;
typedef toonz_port_interface_t port_interface_t;
typedef toonz_nodal_rasterfx_handler_t nodal_rasterfx_handler_t;
typedef toonz_fxnode_interface_t fxnode_interface_t;
typedef toonz_setup_interface_t setup_interface_t;
}
#endif

#endif
