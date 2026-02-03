#pragma once

#ifndef PLUGIN_PARAM_INTERFACE
#define PLUGIN_PARAM_INTERFACE

#include "toonz_hostif.h"
#include "tparam.h"
#include "tfx.h"
#include "tfxparam.h"
#include "toonz_params.h"

/* ひとつのパラメータに対応 */
class Param {
  TFx *fx_;
  std::string name_;
  toonz_param_type_enum type_;
  // std::shared_ptr< toonz_param_desc_t > desc_;
  toonz_param_desc_t *desc_;

public:
  // inline Param(TFx* fx, const std::string& name, toonz_param_type_enum type,
  // std::shared_ptr< toonz_param_desc_t >&& desc)
  //	: fx_(fx), name_(name), type_(type), desc_(std::move(desc)) {
  inline Param(TFx *fx, const std::string &name, toonz_param_type_enum type,
               const toonz_param_desc_t *desc)
      : fx_(fx)
      , name_(name)
      , type_(type)
      , desc_(const_cast<toonz_param_desc_t *>(desc)) {}

  inline const std::string &name() const { return name_; }

  inline toonz_param_type_enum type() const { return type_; }

  inline TParamP param() const { return fx_->getParams()->getParam(name_); }

  inline const toonz_param_desc_t *desc() const { return desc_; }
  inline toonz_param_desc_t *desc() { return desc_; }
};

int hint_default_value(toonz_param_handle_t param, int size_in_bytes,
                       const void *default_value);
int hint_value_range(toonz_param_handle_t param, const void *minvalue,
                     const void *maxvalue);
int hint_unit(toonz_param_handle_t param, int unit);
int hint_item(toonz_param_handle_t param, int item, const char *caption);
int set_description(toonz_param_handle_t param, const char *description);

int get_type(toonz_param_handle_t param, double, int *, int *);
int get_value_type(toonz_param_handle_t param, int *pvalue_type);
int get_value(toonz_param_handle_t param, double frame, int *psize_in_bytes,
              void *pvalue);
int set_value(toonz_param_handle_t param, double frame, int size_in_bytes,
              const void *pvalue);

int get_string_value(toonz_param_handle_t param, int *wholesize, int rcvbufsize,
                     char *pvalue);
int get_spectrum_value(toonz_param_handle_t param, double frame, double x,
                       toonz_param_spectrum_t *pvalue);

#endif
