#pragma once

#if !defined(TOONZ_PLUGIN_PARAM_TRAITS_H__)
#define TOONZ_PLUGIN_PARAM_TRAITS_H__

#include <toonz_params.h>
#include <functional>

template <typename First, typename Second>
struct param_bind_t {
  typedef First traittype;
  typedef Second realtype;
  typedef typename std::is_compound<typename First::valuetype>::value_type
      complextype;
  typedef typename First::valuetype valuetype;
  static const int RANGED       = First::RANGED;
  static const size_t valuesize = sizeof(typename First::valuetype);
};

typedef param_bind_t<toonz_param_traits_double_t, TDoubleParam> tpbind_dbl_t;
typedef param_bind_t<toonz_param_traits_range_t, TRangeParam> tpbind_rng_t;
typedef param_bind_t<toonz_param_traits_color_t, TPixelParam> tpbind_col_t;
typedef param_bind_t<toonz_param_traits_point_t, TPointParam> tpbind_pnt_t;
typedef param_bind_t<toonz_param_traits_enum_t, TIntEnumParam> tpbind_enm_t;
typedef param_bind_t<toonz_param_traits_int_t, TIntParam> tpbind_int_t;
typedef param_bind_t<toonz_param_traits_bool_t, TBoolParam> tpbind_bool_t;
typedef param_bind_t<toonz_param_traits_spectrum_t, TSpectrumParam>
    tpbind_spc_t;
typedef param_bind_t<toonz_param_traits_string_t, TStringParam> tpbind_str_t;
typedef param_bind_t<toonz_param_traits_tonecurve_t, TToneCurveParam>
    tpbind_tcv_t;

template <typename T>
inline bool is_type_of(const toonz_param_desc_t *desc) {
  if (desc->traits_tag == T::E) return true;
  return false;
}

/* Complex なパラメータは直接 setRangeValue()
 * などを持たず、集約しているサブタイプを返すものがあるので、そのサブタイプを得る関数を取得する
 */
template <typename RT, typename F = TDoubleParamP>
inline F &get_func_a(RT *t) {
  assert(false);
}
template <typename RT, typename F = TDoubleParamP>
inline F &get_func_b(RT *t) {
  assert(false);
}

/* TRangeParam */
template <>
inline TDoubleParamP &get_func_a<TRangeParam, TDoubleParamP>(TRangeParam *t) {
  printf("get_func_a< TRangeParam, TDoubleParamP& >(TRangeParam* t)\n");
  return std::mem_fn(&TRangeParam::getMin)(t);
}

template <>
inline TDoubleParamP &get_func_b<TRangeParam, TDoubleParamP>(TRangeParam *t)
// template<> std::mem_fun_ref_t< TDoubleParamP&, TRangeParam > get_func_b<
// TRangeParam, std::mem_fun_ref_t< TDoubleParamP&, TRangeParam > >(TRangeParam*
// t)
{
  printf("get_func_b< TRangeParam, TDoubleParamP& >(TRangeParam* t)\n");
  return std::mem_fn(&TRangeParam::getMax)(t);
}

/* TPointParam */
template <>
inline TDoubleParamP &get_func_a<TPointParam, TDoubleParamP>(TPointParam *t)
// template<> std::mem_fun_ref_t< TDoubleParamP&, TPointParam > get_func_a<
// TPointParam, std::mem_fun_ref_t< TDoubleParamP&, TPointParam > >(TPointParam*
// t)
{
  printf("get_func_a< TPointParam, TDoubleParamP& >(TPointParam* t)\n");
  return std::mem_fn(&TPointParam::getX)(t);
}

template <>
inline TDoubleParamP &get_func_b<TPointParam, TDoubleParamP>(TPointParam *t)
// template<> std::mem_fun_ref_t< TDoubleParamP&, TPointParam > get_func_b<
// TPointParam, std::mem_fun_ref_t< TDoubleParamP&, TPointParam > >(TPointParam*
// t)
{
  printf("get_func_b< TPointParam, TDoubleParamP& >(TPointParam* t)\n");
  return std::mem_fn(&TPointParam::getY)(t);
}

/* valuetype が集約型の場合、 スカラを取得するための関数 */
template <typename T, typename V>
inline V get_1st_value(const T &) {}
template <typename T, typename V>
inline V get_2nd_value(const T &) {}

template <>
inline double get_1st_value(const toonz_param_traits_range_t::valuetype &r) {
  return r.a;
}
template <>
inline double get_2nd_value(const toonz_param_traits_range_t::valuetype &r) {
  return r.b;
}

template <>
inline double get_1st_value(const toonz_param_traits_point_t::valuetype &p) {
  return p.x;
}
template <>
inline double get_2nd_value(const toonz_param_traits_point_t::valuetype &p) {
  return p.y;
}

template <typename Bind,
          typename Comp =
              typename std::is_compound<typename Bind::valuetype>::type,
          int Ranged = Bind::RANGED>
// template < int Ranged, typename Comp, typename Bind >
struct set_param_range_t {
  static bool set_param_range(Param *param, const toonz_param_desc_t *desc) {
    /* 範囲を持たない(Ranged == std::false_type)なら何もすることはない */
    printf("(none)set_param_range: p:%p type:%s (Comp:%s Ranged:%d)\n", param,
           typeid(Bind).name(), typeid(Comp).name(), Ranged);
    return false;
  }
};

// static_assert(std::is_compound< const char* >(), "false");

/* ranged complextype */
template <typename Bind>
struct set_param_range_t<Bind, std::true_type, 1> {
  static bool set_param_range(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    typename Bind::realtype *p =
        reinterpret_cast<typename Bind::realtype *>(smartptr.getPointer());
    if (p) {
      const typename Bind::traittype &t =
          *reinterpret_cast<const typename Bind::traittype *>(&desc->traits.d);
      auto subtype_a = get_func_a<typename Bind::realtype>(p);
      auto subtype_b = get_func_b<typename Bind::realtype>(p);
      auto a_minval  = get_1st_value<typename Bind::valuetype, double>(t.min);
      auto a_maxval  = get_2nd_value<typename Bind::valuetype, double>(t.min);
      auto b_minval  = get_1st_value<typename Bind::valuetype, double>(t.max);
      auto b_maxval  = get_2nd_value<typename Bind::valuetype, double>(t.max);
      printf("a->set_param_range: (%g, %g)\n", a_minval, a_maxval);
      printf("b->set_param_range: (%g, %g)\n", b_minval, b_maxval);
      (subtype_a)->setValueRange(a_minval, a_maxval);
      (subtype_b)->setValueRange(b_minval, b_maxval);
    }
    return true;
  }
};

/* range
   のとき、スライダの左と右それぞれに限界が設定できるように見えるが、そうではない.
   getMin(), getMax() の結果それぞれに range を設定できるように見えて
   実際は getMin() には (min, max) のうち min, getMax() には (min, max) のうち
   max しか有効でないように見える.
   このため range に対しても特殊版を用意するハメになった. */
template <>
struct set_param_range_t<tpbind_rng_t, std::true_type, 1> {
  static bool set_param_range(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    tpbind_rng_t::realtype *p =
        reinterpret_cast<tpbind_rng_t::realtype *>(smartptr.getPointer());
    if (p) {
      const tpbind_rng_t::traittype &t = desc->traits.rd;
      auto subtype_a                   = get_func_a<tpbind_rng_t::realtype>(p);
      auto subtype_b                   = get_func_b<tpbind_rng_t::realtype>(p);
      auto a_minval = get_1st_value<tpbind_rng_t::valuetype, double>(t.minmax);
      auto a_maxval = get_2nd_value<tpbind_rng_t::valuetype, double>(t.minmax);
      (subtype_a)->setValueRange(a_minval, a_maxval);
      (subtype_b)->setValueRange(a_minval, a_maxval);
    }
    return true;
  }
};

/*
template <>
struct set_param_range_t< tpbind_pnt_t, std::true_type, 1 >  {
        static bool set_param_range(Param* param, const toonz_param_desc_t*
desc)
        {
                auto smartptr = param->param();
                tpbind_pnt_t::realtype* p = reinterpret_cast<
tpbind_pnt_t::realtype* >(smartptr.getPointer());
                if (p) {
                        const tpbind_pnt_t::traittype& t = *reinterpret_cast<
const tpbind_pnt_t::traittype* >(&desc->traits.d);
                        auto subtype_a = get_func_a< tpbind_pnt_t::realtype
>(p);
                        auto subtype_b = get_func_b< tpbind_pnt_t::realtype
>(p);
                        auto a_minval = get_1st_value< tpbind_pnt_t::valuetype,
double >(t.min);
                        auto a_maxval = get_2nd_value< tpbind_pnt_t::valuetype,
double >(t.min);
                        auto b_minval = get_1st_value< tpbind_pnt_t::valuetype,
double >(t.max);
                        auto b_maxval = get_2nd_value< tpbind_pnt_t::valuetype,
double >(t.max);
                        printf("a->set_param_range: pnt(%g, %g)\n", a_minval,
a_maxval);
                        printf("b->set_param_range: pnt(%g, %g)\n", b_minval,
b_maxval);
                        (subtype_a)->setValueRange(a_minval, a_maxval);
                        (subtype_b)->setValueRange(b_minval, b_maxval);
                }
                return true;
        }
};
*/

/* ranged primitive: */
template <typename Bind>
struct set_param_range_t<Bind, std::false_type, 1> {
  static bool set_param_range(Param *param, const toonz_param_desc_t *desc) {
    if (!is_type_of<typename Bind::traittype>(desc)) return false;
    auto smartptr = param->param();
    typename Bind::realtype *p =
        reinterpret_cast<typename Bind::realtype *>(smartptr.getPointer());
    if (p) {
      const typename Bind::traittype &t =
          *reinterpret_cast<const typename Bind::traittype *>(&desc->traits.d);
      printf("p(%p)->set_param_range: typeid:%s desc:%p (%p)\n", p,
             typeid(typename Bind::traittype).name(), desc, &desc->traits.d);
      p->setValueRange(t.min, t.max);
    }
    return true;
  }
};

template <typename Bind>
bool set_param_range(Param *param, const toonz_param_desc_t *desc) {
  if (!is_type_of<typename Bind::traittype>(desc)) return false;
  return set_param_range_t<Bind>::set_param_range(param, desc);
}

template <typename Bind,
          typename Comp =
              typename std::is_compound<typename Bind::valuetype>::type>
struct set_param_default_t {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    return false;
  }
};

/* Default complextype */
/* Point/Range */
template <typename Bind>
struct set_param_default_t<Bind, std::true_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    typename Bind::realtype *p =
        reinterpret_cast<typename Bind::realtype *>(smartptr.getPointer());
    if (p) {
      const typename Bind::traittype &t =
          *reinterpret_cast<const typename Bind::traittype *>(&desc->traits.d);
      auto subtype_a = get_func_a<typename Bind::realtype>(p);
      auto subtype_b = get_func_b<typename Bind::realtype>(p);
      auto a_defval  = get_1st_value<typename Bind::valuetype, double>(t.def);
      auto b_defval  = get_2nd_value<typename Bind::valuetype, double>(t.def);
      printf("a->set_param_default: double (%g, %g)\n", a_defval, b_defval);
      (subtype_a)->setDefaultValue(a_defval);
      (subtype_b)->setDefaultValue(b_defval);
    }
    return true;
  }
};

/* Default Color */
template <>
struct set_param_default_t<tpbind_col_t, std::true_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    tpbind_col_t::realtype *p =
        reinterpret_cast<tpbind_col_t::realtype *>(smartptr.getPointer());
    if (p) {
      const tpbind_col_t::traittype &t =
          *reinterpret_cast<const tpbind_col_t::traittype *>(&desc->traits.d);
      p->setDefaultValue(TPixel32(t.def.c0, t.def.c1, t.def.c2, t.def.m));
    }
    return true;
  }
};

/* Default String */
template <>
struct set_param_default_t<tpbind_str_t, std::true_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    tpbind_str_t::realtype *p =
        reinterpret_cast<tpbind_str_t::realtype *>(smartptr.getPointer());
    if (p) {
      const tpbind_str_t::traittype &t =
          *reinterpret_cast<const tpbind_str_t::traittype *>(&desc->traits.d);
      printf("a->set_param_default: str\n");
      std::wstring wstr = QString::fromStdString(t.def).toStdWString();
      p->setDefaultValue(wstr);
      p->setValue(wstr, false);
    }
    return true;
  }
};

/* Default Spectrum */
template <>
struct set_param_default_t<tpbind_spc_t, std::true_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    /* unfortunately, TSpectrumParam's default values must be set within the
     constructor, for now.
     see param_factory_< TSpectrumParam >() */
    return false;
  }
};

/* Default ToneCurve */
template <>
struct set_param_default_t<tpbind_tcv_t, std::true_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    /*
    auto smartptr = param->param();
    tpbind_tcv_t::realtype* p = reinterpret_cast< tpbind_tcv_t::realtype*
    >(smartptr.getPointer());
    if (p) {
            const tpbind_tcv_t::traittype& t = *reinterpret_cast< const
    tpbind_tcv_t::traittype* >(&desc->traits.d);
            printf("a->set_param_default: spec\n");
            QList< TPointD > pt;
            for (int i = 0; i < t.cps; i ++) {
                    pt.push_back(TPointD(t.array[i].x, t.array[i].y));
            }
            p->setDefaultValue(pt);
            p->setIsLinear(!(t.intep));
            }*/
    return true;
  }
};

/* primitive: TDoubleParam */
template <>
struct set_param_default_t<tpbind_dbl_t, std::false_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    tpbind_dbl_t::realtype *p =
        reinterpret_cast<tpbind_dbl_t::realtype *>(smartptr.getPointer());
    if (p) {
      const tpbind_dbl_t::traittype &t =
          *reinterpret_cast<const tpbind_dbl_t::traittype *>(&desc->traits.d);
      printf("p(%p)->set_param_default: typeid:%s desc:%p (%p)\n", p,
             typeid(tpbind_dbl_t::traittype).name(), desc, &desc->traits.d);
      p->setDefaultValue(t.def);
    }
    return true;
  }
};

/* primitive: TNotAnimatableParam */
template <typename Bind>
struct set_param_default_t<Bind, std::false_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    typename Bind::realtype *p =
        reinterpret_cast<typename Bind::realtype *>(smartptr.getPointer());
    if (p) {
      const typename Bind::traittype &t =
          *reinterpret_cast<const typename Bind::traittype *>(&desc->traits.d);
      printf("p(%p)->set_param_default: typeid:%s desc:%p (%p)\n", p,
             typeid(typename Bind::traittype).name(), desc, &desc->traits.d);
      p->setDefaultValue(t.def);
      p->setValue(t.def, false);
    }
    return true;
  }
};

/* Default Enum */
template <>
struct set_param_default_t<tpbind_enm_t, std::false_type> {
  static bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
    auto smartptr = param->param();
    tpbind_enm_t::realtype *p =
        reinterpret_cast<tpbind_enm_t::realtype *>(smartptr.getPointer());
    if (p) {
      const tpbind_enm_t::traittype &t =
          *reinterpret_cast<const tpbind_enm_t::traittype *>(&desc->traits.d);
      for (int i = 0; i < t.enums; i++) {
        p->addItem(i, t.array[i]);
      }
      p->setValue(t.def, false);
    }
    return true;
  }
};

template <typename Bind>
bool set_param_default(Param *param, const toonz_param_desc_t *desc) {
  if (!is_type_of<typename Bind::traittype>(desc)) return false;
  return set_param_default_t<Bind>::set_param_default(param, desc);
}

template <typename T>
inline T *param_factory_(const toonz_param_desc_t *desc) {
  return new T;
}

template <>
inline TPointParam *param_factory_(const toonz_param_desc_t *desc) {
  return new TPointParam(TPointD(), true /* instantiate from plugin */);
}

template <>
inline TSpectrumParam *param_factory_(const toonz_param_desc_t *desc) {
  const toonz_param_traits_spectrum_t &t = desc->traits.g;
  if (t.points) {
    std::vector<TSpectrum::ColorKey> keys(t.points);
    for (int i = 0; i < t.points; i++) {
      keys[i].first  = t.array[i].w;
      keys[i].second = toPixel32(
          TPixelD(t.array[i].c0, t.array[i].c1, t.array[i].c2, t.array[i].m));
    }
    return new TSpectrumParam(keys);
  } else {
    return new TSpectrumParam(); /* use default constructor: デフォルトでは
                                    [black:white] の単純なものが設定される */
  }
}

inline TParam *parameter_factory(const toonz_param_desc_t *desc) {
  switch (desc->traits_tag) {
  case TOONZ_PARAM_TYPE_DOUBLE:
    return param_factory_<TDoubleParam>(desc);
  case TOONZ_PARAM_TYPE_RANGE:
    return param_factory_<TRangeParam>(desc);
  case TOONZ_PARAM_TYPE_PIXEL:
    return param_factory_<TPixelParam>(desc);
  case TOONZ_PARAM_TYPE_POINT:
    return param_factory_<TPointParam>(desc);
  case TOONZ_PARAM_TYPE_ENUM:
    return param_factory_<TIntEnumParam>(desc);
  case TOONZ_PARAM_TYPE_INT:
    return param_factory_<TIntParam>(desc);
  case TOONZ_PARAM_TYPE_BOOL:
    return param_factory_<TBoolParam>(desc);
  case TOONZ_PARAM_TYPE_SPECTRUM:
    return param_factory_<TSpectrumParam>(desc);
  case TOONZ_PARAM_TYPE_STRING:
    return param_factory_<TStringParam>(desc);
  case TOONZ_PARAM_TYPE_TONECURVE:
    return param_factory_<TToneCurveParam>(desc);
  default:
    break;
  }
  return NULL;
}

template <typename T>
inline int check_pollution_(const T &t) {
  if (t.reserved_) return TOONZ_PARAM_ERROR_POLLUTED;
  return 0;
}

template <typename T>
inline int check_traits_sanity_(const toonz_param_desc_t *desc) {
  const T &t = reinterpret_cast<const T &>(desc->traits.d);
  return check_pollution_<T>(t);
}

template <>
inline int check_traits_sanity_<toonz_param_traits_double_t>(
    const toonz_param_desc_t *desc) {
  int err                              = 0;
  const toonz_param_traits_double_t &t = desc->traits.d;
  err |= check_pollution_(t);
  if (t.min > t.max) err |= TOONZ_PARAM_ERROR_MIN_MAX;
  return 0;
}

template <>
inline int check_traits_sanity_<toonz_param_traits_range_t>(
    const toonz_param_desc_t *desc) {
  int err                             = 0;
  const toonz_param_traits_range_t &t = desc->traits.rd;
  err |= check_pollution_(t);
  if (t.minmax.a == 0 && t.minmax.b == 0)
    return err; /* range に興味がない場合の 0,0 を許す */
  if (t.minmax.a > t.minmax.b) err |= TOONZ_PARAM_ERROR_MIN_MAX;
  return err;
}

template <>
inline int check_traits_sanity_<toonz_param_traits_enum_t>(
    const toonz_param_desc_t *desc) {
  int err                            = 0;
  const toonz_param_traits_enum_t &t = desc->traits.e;
  err |= check_pollution_(t);
  if (t.enums == 0) return err;
  if (t.enums < 0) err |= TOONZ_PARAM_ERROR_ARRAY_NUM;
  if (t.array == NULL) err |= TOONZ_PARAM_ERROR_ARRAY;
  return err;
}

template <>
inline int check_traits_sanity_<toonz_param_traits_spectrum_t>(
    const toonz_param_desc_t *desc) {
  int err                                = 0;
  const toonz_param_traits_spectrum_t &t = desc->traits.g;
  err |= check_pollution_(t);
  if (t.points == 0) return err;
  if (t.points < 0) err |= TOONZ_PARAM_ERROR_ARRAY_NUM;
  if (t.array == NULL) err |= TOONZ_PARAM_ERROR_ARRAY;
  return err;
}

/*
template <> int check_traits_sanity_< toonz_param_traits_tonecurve_t >(const
toonz_param_desc_t* desc)
{
        int err = 0;
        const toonz_param_traits_tonecurve_t& t = desc->traits.tcv;
        err |= check_pollution_(t);
        if (t.points == 0)
                return err;
        if (t.points < 0)
                err |= TOONZ_PARAM_ERROR_ARRAY_NUM;
        if (t.array == NULL)
                err |= TOONZ_PARAM_ERROR_ARRAY;
        return err;
}
*/

inline int check_traits_sanity(const toonz_param_desc_t *desc) {
  int err = 0;
  switch (desc->traits_tag) {
  case TOONZ_PARAM_TYPE_DOUBLE:
    err = check_traits_sanity_<toonz_param_traits_double_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_RANGE:
    err = check_traits_sanity_<toonz_param_traits_range_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_PIXEL:
    err = check_traits_sanity_<toonz_param_traits_color_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_POINT:
    err = check_traits_sanity_<toonz_param_traits_point_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_ENUM:
    err = check_traits_sanity_<toonz_param_traits_enum_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_INT:
    err = check_traits_sanity_<toonz_param_traits_int_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_BOOL:
    err = check_traits_sanity_<toonz_param_traits_bool_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_SPECTRUM:
    err = check_traits_sanity_<toonz_param_traits_spectrum_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_STRING:
    err = check_traits_sanity_<toonz_param_traits_string_t>(desc);
    break;
  case TOONZ_PARAM_TYPE_TONECURVE:
    err = check_traits_sanity_<toonz_param_traits_tonecurve_t>(desc);
    break;
  default:
    err = TOONZ_PARAM_ERROR_TRAITS;
    break;
  }
  return err;
}

template <typename T>
inline bool param_type_check_(TParam *p, const toonz_param_desc_t *desc,
                              size_t &vsz) {
  if (typename T::realtype *d = dynamic_cast<typename T::realtype *>(p)) {
    if (is_type_of<typename T::traittype>(desc)) {
      vsz = sizeof(typename T::traittype::iovaluetype);
      return true;
    }
  }
  return false;
}

inline bool parameter_type_check(TParam *p, const toonz_param_desc_t *desc,
                                 size_t &vsz) {
  switch (desc->traits_tag) {
  case TOONZ_PARAM_TYPE_DOUBLE:
    return param_type_check_<tpbind_dbl_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_RANGE:
    return param_type_check_<tpbind_rng_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_PIXEL:
    return param_type_check_<tpbind_col_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_POINT:
    return param_type_check_<tpbind_pnt_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_ENUM:
    return param_type_check_<tpbind_enm_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_INT:
    return param_type_check_<tpbind_int_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_BOOL:
    return param_type_check_<tpbind_bool_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_SPECTRUM:
    return param_type_check_<tpbind_spc_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_STRING:
    return param_type_check_<tpbind_str_t>(p, desc, vsz);
  case TOONZ_PARAM_TYPE_TONECURVE:
    return param_type_check_<tpbind_tcv_t>(p, desc, vsz);
  default:
    break;
  }
  return false;
}

template <typename T>
inline bool param_read_value_(TParam *p, const toonz_param_desc_t *desc,
                              void *ptr, double frame, size_t isize,
                              size_t &osize) {
  /* isize は iovaluetype の size でなく count になったのでサイズチェックは無効
   */
  // if (isize == sizeof(typename T::traittype::iovaluetype)) {
  auto r = reinterpret_cast<typename T::realtype *>(p);
  auto v = r->getValue();
  *reinterpret_cast<typename T::traittype::iovaluetype *>(ptr) = v;
  osize                                                        = 1;
  return true;
  //}
  return false;
}

template <>
inline bool param_read_value_<tpbind_dbl_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  // if (isize == sizeof(tpbind_dbl_t::traittype::iovaluetype)) {
  auto r = reinterpret_cast<tpbind_dbl_t::realtype *>(p);
  auto v = r->getValue(frame);
  *reinterpret_cast<tpbind_dbl_t::traittype::iovaluetype *>(ptr) = v;
  osize                                                          = 1;
  return true;
  //}
  return false;
}

template <>
inline bool param_read_value_<tpbind_str_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  auto r                = reinterpret_cast<tpbind_str_t::realtype *>(p);
  const std::string str = QString::fromStdWString(r->getValue()).toStdString();
  std::size_t len       = str.length() + 1;
  /* get_type() の返す大きさも文字列長+1 を含んでいる */
  if (isize < len)
    len = isize; /* 要求サイズが実際の長さより短くても良いが切り詰める(ただし 1
                    以上であること) */

  if (len > 0) {
    auto dst = reinterpret_cast<char *>(ptr);
    strncpy(dst, str.c_str(), len - 1);
    dst[len - 1] = '\0';
    osize        = len;
    return true;
  }
  return false;
}

template <>
inline bool param_read_value_<tpbind_rng_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  // if (isize == sizeof(tpbind_rng_t::traittype::iovaluetype)) {
  auto r   = reinterpret_cast<tpbind_rng_t::realtype *>(p);
  auto v   = r->getValue(frame);
  auto dst = reinterpret_cast<tpbind_rng_t::traittype::iovaluetype *>(ptr);
  dst->a   = v.first;
  dst->b   = v.second;
  osize    = 1;
  return true;
  //}
  return false;
}

template <>
inline bool param_read_value_<tpbind_col_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  // if (isize == sizeof(tpbind_col_t::traittype::iovaluetype)) {
  auto r = reinterpret_cast<tpbind_col_t::realtype *>(p);
  /* getValueD() だと 16bit * 4 が返る */
  // auto v = r->getValueD(frame);
  auto v   = r->getValue(frame);
  auto dst = reinterpret_cast<tpbind_col_t::traittype::iovaluetype *>(ptr);
  dst->c0  = v.r;
  dst->c1  = v.g;
  dst->c2  = v.b;
  dst->m   = v.m;
  osize    = 1;
  return true;
  //}
  return false;
}

template <>
inline bool param_read_value_<tpbind_pnt_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  // if (isize == sizeof(tpbind_pnt_t::traittype::iovaluetype)) {
  auto r   = reinterpret_cast<tpbind_pnt_t::realtype *>(p);
  auto v   = r->getValue(frame);
  auto dst = reinterpret_cast<tpbind_pnt_t::traittype::iovaluetype *>(ptr);
  dst->x   = v.x;
  dst->y   = v.y;
  osize    = 1;
  return true;
  //}
  return false;
}

template <>
inline bool param_read_value_<tpbind_spc_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  // if (isize == sizeof(tpbind_spc_t::traittype::iovaluetype)) {
  auto r   = reinterpret_cast<tpbind_spc_t::realtype *>(p);
  auto dst = reinterpret_cast<tpbind_spc_t::traittype::iovaluetype *>(ptr);
  /* getValue64() だと 1channle 16bit が返るがデフォルト型に合わせる */
  auto v  = r->getValue(frame).getValue(dst->w);
  dst->c0 = v.r;
  dst->c1 = v.g;
  dst->c2 = v.b;
  dst->m  = v.m;
  osize   = 1;
  return true;
  //}
  return false;
}

template <>
inline bool param_read_value_<tpbind_tcv_t>(TParam *p,
                                            const toonz_param_desc_t *desc,
                                            void *ptr, double frame,
                                            size_t isize, size_t &osize) {
  auto r                = reinterpret_cast<tpbind_tcv_t::realtype *>(p);
  QList<TPointD> points = r->getValue(frame);
  size_t ps             = points.size();
  /* コントロールポイントのリストしか戻って来ない！ */
  if (isize >= ps) {
    int channel = r->getCurrentChannel();
    int interp  = !r->isLinear();
    int c       = isize < points.size() ? isize : points.size();
    for (int i = 0; i < c; i++) {
      auto dst = reinterpret_cast<tpbind_tcv_t::traittype::iovaluetype *>(ptr);
      dst[i].x = points[i].x;
      dst[i].y = points[i].y;
      dst[i].channel = channel;
      dst[i].interp  = interp;
    }
    osize = c;
    return true;
  }
  return false;
}

inline bool parameter_read_value(TParam *p, const toonz_param_desc_t *desc,
                                 void *ptr, double frame, size_t isize,
                                 size_t &osize) {
  size_t sz = 0;
  if (!parameter_type_check(p, desc, sz)) {  // typecheck
    return false;
  }

  switch (desc->traits_tag) {
  case TOONZ_PARAM_TYPE_DOUBLE:
    return param_read_value_<tpbind_dbl_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_RANGE:
    return param_read_value_<tpbind_rng_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_PIXEL:
    return param_read_value_<tpbind_col_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_POINT:
    return param_read_value_<tpbind_pnt_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_ENUM:
    return param_read_value_<tpbind_enm_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_INT:
    return param_read_value_<tpbind_int_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_BOOL:
    return param_read_value_<tpbind_bool_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_SPECTRUM:
    return param_read_value_<tpbind_spc_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_STRING:
    return param_read_value_<tpbind_str_t>(p, desc, ptr, frame, isize, osize);
  case TOONZ_PARAM_TYPE_TONECURVE:
    return param_read_value_<tpbind_tcv_t>(p, desc, ptr, frame, isize, osize);
  default:
    break;
  }
  return false;
}

#endif
