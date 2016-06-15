#pragma once

#if !defined(TOONZ_PARAMS_H__)
#define TOONZ_PARAMS_H__

#include <toonz_plugin.h>
//#include <toonz_hostif.h>

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

#if defined(DEFAULT)
/* remove creepy macro in tcommon.h */
#undef DEFAULT
#endif

struct toonz_param_traits_double_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_DOUBLE;
  static const int RANGED  = 1;
  static const int DEFAULT = 1;
  typedef double valuetype;
  typedef double iovaluetype;
#endif
  double def;
  double min, max;
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_double_t_ toonz_param_traits_double_t;

struct toonz_param_traits_int_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_INT;
  static const int RANGED  = 1;
  static const int DEFAULT = 1;
  typedef int valuetype;
  typedef int iovaluetype;
#endif
  int def;
  int min, max;
  int reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_int_t_ toonz_param_traits_int_t;

struct toonz_param_range_t_ {
  double a, b;
} TOONZ_PACK;
typedef toonz_param_range_t_ toonz_param_range_t;

struct toonz_param_traits_range_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_RANGE;
  static const int RANGED  = 1;
  static const int DEFAULT = 1;
  typedef toonz_param_range_t valuetype;
  typedef toonz_param_range_t iovaluetype;
#endif
  toonz_param_range_t def;
  toonz_param_range_t minmax;
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_range_t_ toonz_param_traits_range_t;

struct toonz_param_color_t_ {
  int c0, c1, c2, m;
} TOONZ_PACK;
typedef toonz_param_color_t_ toonz_param_color_t;

struct toonz_param_traits_color_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_PIXEL;
  static const int RANGED  = 0;
  static const int DEFAULT = 1;
  typedef toonz_param_color_t valuetype;
  typedef toonz_param_color_t iovaluetype;
#endif
  toonz_param_color_t def;
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_color_t_ toonz_param_traits_color_t;

struct toonz_param_point_t_ {
  double x, y;
} TOONZ_PACK;
typedef toonz_param_point_t_ toonz_param_point_t;

struct toonz_param_traits_point_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_POINT;
  static const int RANGED  = 1;
  static const int DEFAULT = 1;
  typedef toonz_param_point_t valuetype;
  typedef toonz_param_point_t iovaluetype;
#endif
  toonz_param_point_t def;
  toonz_param_point_t min;
  toonz_param_point_t max;
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_point_t_ toonz_param_traits_point_t;

struct toonz_param_traits_enum_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_ENUM;
  static const int RANGED  = 0;
  static const int DEFAULT = 1;
  typedef int valuetype;
  typedef int iovaluetype;
#endif
  int def;
  int enums;
  const char **array;
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_enum_t_ toonz_param_traits_enum_t;

struct toonz_param_traits_bool_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_BOOL;
  static const int RANGED  = 0;
  static const int DEFAULT = 1;
  typedef int valuetype;
  typedef int iovaluetype;
#endif
  int def;
  int reserved_;
} TOONZ_PACK;
typedef toonz_param_traits_bool_t_ toonz_param_traits_bool_t;

struct toonz_param_spectrum_t_ {
  double w;
  double c0, c1, c2, m;
} TOONZ_PACK;
typedef toonz_param_spectrum_t_ toonz_param_spectrum_t;

struct toonz_param_traits_spectrum_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_SPECTRUM;
  static const int RANGED  = 0;
  static const int DEFAULT = 1;
  typedef toonz_param_spectrum_t valuetype;
  typedef toonz_param_spectrum_t iovaluetype;
#endif
  double def;
  int points;
  toonz_param_spectrum_t *array;
  int reserved_;
} TOONZ_PACK;
typedef toonz_param_traits_spectrum_t_ toonz_param_traits_spectrum_t;

struct toonz_param_traits_string_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_STRING;
  static const int RANGED  = 0;
  static const int DEFAULT = 1;
  typedef const char *valuetype;
  typedef char iovaluetype;
#endif
  const char *def;
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_string_t_ toonz_param_traits_string_t;

struct toonz_param_tonecurve_point_t_ {
  double x, y;
  double c0, c1, c2, c3;
} TOONZ_PACK;
typedef toonz_param_tonecurve_point_t_ toonz_param_tonecurve_point_t;

struct toonz_param_tonecurve_value_t_ {
  double x;
  double y;
  int channel;
  int interp;
} TOONZ_PACK;
typedef toonz_param_tonecurve_value_t_ toonz_param_tonecurve_value_t;

struct toonz_param_traits_tonecurve_t_ {
#if defined(__cplusplus)
  static const int E       = TOONZ_PARAM_TYPE_TONECURVE;
  static const int RANGED  = 0;
  static const int DEFAULT = 0;
  typedef toonz_param_tonecurve_point_t valuetype;
  typedef toonz_param_tonecurve_value_t iovaluetype;
#endif
  double reserved_;
} TOONZ_PACK;

typedef toonz_param_traits_tonecurve_t_ toonz_param_traits_tonecurve_t;

#define TOONZ_PARAM_DESC_TYPE_PARAM (0)
#define TOONZ_PARAM_DESC_TYPE_PAGE (1)
#define TOONZ_PARAM_DESC_TYPE_GROUP (2)

struct toonz_param_base_t_ {
  toonz_if_version_t ver;
  int type;
  const char *label;
} TOONZ_PACK;

struct toonz_param_desc_t_ {
  struct toonz_param_base_t_ base;
  const char *key;
  const char *note;
  void *reserved_[2];
  int traits_tag;
  union {
    toonz_param_traits_double_t d;
    toonz_param_traits_int_t i;
    toonz_param_traits_enum_t e;
    toonz_param_traits_range_t rd;
    toonz_param_traits_bool_t b;
    toonz_param_traits_color_t c;
    toonz_param_traits_point_t p;
    toonz_param_traits_spectrum_t g;
    toonz_param_traits_string_t s;
    toonz_param_traits_tonecurve_t t;
  } traits;
} TOONZ_PACK;

typedef struct toonz_param_desc_t_ toonz_param_desc_t;

struct toonz_param_group_t_ {
  struct toonz_param_base_t_ base;
  int num;
  toonz_param_desc_t *array;
} TOONZ_PACK;

typedef struct toonz_param_group_t_ toonz_param_group_t;

struct toonz_param_page_t_ {
  struct toonz_param_base_t_ base;
  int num;
  toonz_param_group_t *array;
} TOONZ_PACK;

typedef struct toonz_param_page_t_ toonz_param_page_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif
