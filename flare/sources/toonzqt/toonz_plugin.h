#pragma once

#ifndef TOONZ_PLUGIN_H__
#define TOONZ_PLUGIN_H__

#include <stdint.h>

#ifdef _WIN32
#define TOONZ_EXPORT __declspec(dllexport)
#define TOONZ_PACK
#else
#define TOONZ_EXPORT
#define TOONZ_PACK __attribute__((packed))
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

struct toonz_UUID_ {
  uint32_t uid0;
  uint16_t uid1;
  uint16_t uid2;
  uint16_t uid3;
  uint64_t uid4;
} TOONZ_PACK;

typedef toonz_UUID_ toonz_UUID;

struct toonz_if_version_t_ {
  int32_t major;
  int32_t minor;
} TOONZ_PACK;

typedef toonz_if_version_t_ toonz_if_version_t;

struct toonz_plugin_version_t_ {
  int32_t major;
  int32_t minor;
} TOONZ_PACK;

typedef toonz_plugin_version_t_ toonz_plugin_version_t;

struct toonz_nodal_rasterfx_handler_t_;

struct toonz_plugin_probe_t_ {
  toonz_if_version_t ver;            /* version of the structure */
  toonz_plugin_version_t plugin_ver; /* version of the plugin */
  const char *name;
  const char *vendor;
  const char *id;
  const char *note;
  const char *helpurl;
  void *reserved_ptr_[3];
  uint32_t clss;
  uint32_t reserved_int_[7];
  toonz_nodal_rasterfx_handler_t_ *handler;
  void *reserved_ptr_trail_[3];
} TOONZ_PACK;

typedef toonz_plugin_probe_t_ toonz_plugin_probe_t;

struct toonz_plugin_probe_list_t_ {
  toonz_if_version_t ver; /* a version of toonz_plugin_probe_t */
  struct toonz_plugin_probe_t_ *begin;
  struct toonz_plugin_probe_t_ *end;
} TOONZ_PACK;

typedef toonz_plugin_probe_list_t_ toonz_plugin_probe_list_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(__cplusplus)
namespace toonz {
typedef toonz_UUID UUID;
typedef toonz_plugin_probe_t plugin_probe_t;
typedef toonz_plugin_probe_list_t plugin_probe_list_t;
}
#endif

#define TOONZ_IF_VER(major, minor)                                             \
  { (major), (minor) } /* interface or type version */
#define TOONZ_PLUGIN_VER(major, minor)                                         \
  { (major), (minor) } /* plugin version */

#define TOONZ_PLUGIN_PROBE_BEGIN(ver)                                          \
  static const toonz_if_version_t toonz_plugin_probe_ver_ = ver;               \
  struct toonz_plugin_probe_t_ toonz_plugin_info_begin_[] = {
#define TOONZ_PLUGIN_PROBE_DEFINE(plugin_ver, nm, vendor, ident, note,         \
                                  helpurl, cls, handler)                       \
  {                                                                            \
    toonz_plugin_probe_ver_, plugin_ver, (nm), (vendor), (ident), (note),      \
        (helpurl), {0}, /* reserved null ptrs */                               \
        ((cls)), {0},   /* reserved 32bit-integers */                          \
        (handler), {                                                           \
      0                                                                        \
    } /* reserved null ptrs */                                                 \
  }

#define TOONZ_PLUGIN_PROBE_END                                                 \
  , { 0 } /* delim */                                                          \
  }                                                                            \
  ;                                                                            \
  TOONZ_EXPORT struct toonz_plugin_probe_list_t_ toonz_plugin_info_list = {    \
      toonz_plugin_probe_ver_, toonz_plugin_info_begin_,                       \
      &toonz_plugin_info_begin_[sizeof(toonz_plugin_info_begin_) /             \
                                    sizeof(struct toonz_plugin_probe_t_) -     \
                                1]};

/*! エラーコード */
#define TOONZ_OK (0)             /*!< 成功 */
#define TOONZ_ERROR_UNKNOWN (-1) /*!< 下記以外の不明なエラー */
#define TOONZ_ERROR_NOT_IMPLEMENTED (-2) /*!< 未実装 */
#define TOONZ_ERROR_VERSION_UNMATCH (-3) /*!< バージョン不整合 */
#define TOONZ_ERROR_INVALID_HANDLE                                             \
  (-4) /*!< 型が異なるなどの無効なハンドルが渡された */
#define TOONZ_ERROR_NULL (-5) /*!< NULLが許容されていない引数がNULL */
#define TOONZ_ERROR_POLLUTED                                                   \
  (-6) /*!< 0でなければならない予約フィールドが0ではない */
#define TOONZ_ERROR_OUT_OF_MEMORY (-7) /*!< メモリ不足 */
#define TOONZ_ERROR_INVALID_SIZE                                               \
  (-8) /*!< 引数で指定されたサイズが間違っている */
#define TOONZ_ERROR_INVALID_VALUE (-9) /*!< 定義されてない値 */
#define TOONZ_ERROR_BUSY (-10) /*!< 要求されたリソースが既に使用されている */
#define TOONZ_ERROR_NOT_FOUND (-11) /*!< 指定されたものが見つからなかった */
#define TOONZ_ERROR_FAILED_TO_CREATE (-12) /*!< オブジェクト等の作成に失敗 */
#define TOONZ_ERROR_PREREQUISITE                                               \
  (-13) /*!< 事前に他の関数を呼ぶなどの前提条件が満たされていない */

#define TOONZ_PARAM_ERROR_NONE (0)
#define TOONZ_PARAM_ERROR_VERSION (1 << 0) /* version unmatched */
#define TOONZ_PARAM_ERROR_LABEL (1 << 1)   /* the label is null */
#define TOONZ_PARAM_ERROR_TYPE                                                 \
  (1 << 2) /* type code does not match to structure */

#define TOONZ_PARAM_ERROR_PAGE_NUM (1 << 3)
#define TOONZ_PARAM_ERROR_GROUP_NUM (1 << 4)
#define TOONZ_PARAM_ERROR_TRAITS (1 << 5) /* traits is unknown*/

#define TOONZ_PARAM_ERROR_NO_KEY (1 << 8) /* the key is null */
#define TOONZ_PARAM_ERROR_KEY_DUP                                              \
  (1 << 9) /* the key must be unique in the plugin */
#define TOONZ_PARAM_ERROR_KEY_NAME                                             \
  (1 << 10) /* the key must be formed as '[:alpha:_][:alpha::number:_]* */
#define TOONZ_PARAM_ERROR_POLLUTED                                             \
  (1 << 11) /* reserved field must be zero. for future release */

#define TOONZ_PARAM_ERROR_MIN_MAX (1 << 12)
#define TOONZ_PARAM_ERROR_ARRAY_NUM (1 << 13)
#define TOONZ_PARAM_ERROR_ARRAY (1 << 14)
#define TOONZ_PARAM_ERROR_UNKNOWN (1 << 31) /* */

enum toonz_param_type_enum {
  TOONZ_PARAM_TYPE_DOUBLE,
  TOONZ_PARAM_TYPE_RANGE,
  TOONZ_PARAM_TYPE_PIXEL,
  TOONZ_PARAM_TYPE_POINT,
  TOONZ_PARAM_TYPE_ENUM,
  TOONZ_PARAM_TYPE_INT,
  TOONZ_PARAM_TYPE_BOOL,
  TOONZ_PARAM_TYPE_SPECTRUM,
  TOONZ_PARAM_TYPE_STRING,
  TOONZ_PARAM_TYPE_TONECURVE,

  TOONZ_PARAM_TYPE_NB,
  TOONZ_PARAM_TYPE_MAX = 0x7FFFFFFF
};

#endif
