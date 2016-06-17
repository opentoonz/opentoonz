#pragma once

#ifndef PLUGIN_PARAM_VIEW_INTERFACE
#define PLUGIN_PARAM_VIEW_INTERFACE

#include <vector>
#include <memory>
#include <QWidget>
#include "tfx.h"
#include "toonz_hostif.h"
#include "../include/toonzqt/fxsettings.h"

/* 公開ヘッダからは引っ込められた都合上ここで宣言(内部ではまだ使っているので) */
typedef void *toonz_param_view_handle_t;

/* あるパラメータの表示形式 */
class ParamView {
public:
  struct Component {
    virtual ~Component() {}
    virtual QWidget *make_widget(TFx *fx, ParamsPage *page,
                                 const char *name) const = 0;
  };

#define TOONZ_DEFINE_COMPONENT(NAME)                                           \
  struct NAME : public Component {                                             \
    QWidget *make_widget(TFx *fx, ParamsPage *page,                            \
                         const char *name) const final override {              \
      return page->new##NAME(fx, name);                                        \
    }                                                                          \
  }

  TOONZ_DEFINE_COMPONENT(ParamField);
  TOONZ_DEFINE_COMPONENT(LineEdit);
  TOONZ_DEFINE_COMPONENT(Slider);
  TOONZ_DEFINE_COMPONENT(SpinBox);
  TOONZ_DEFINE_COMPONENT(CheckBox);
  TOONZ_DEFINE_COMPONENT(RadioButton);
  TOONZ_DEFINE_COMPONENT(ComboBox);

#undef TOONZ_DEFINE_COMPONENT

public:
  ParamView() {}

  ParamView(const ParamView &rhs) : components_(rhs.components_) {}

  inline void add_component(std::shared_ptr<Component> component) {
    components_.push_back(std::move(component));
  }

  ParamView *clone(TFx *) const { return new ParamView(*this); }

  void build(TFx *fx, ParamsPage *page, const char *name) const {
    for (const auto &component : components_) {
      page->addWidget(component->make_widget(fx, page, name));
    }
  }

private:
  std::vector<std::shared_ptr<Component>> components_;
};

typedef void *toonz_param_field_handle_t;
typedef void *toonz_lineedit_handle_t;
typedef void *toonz_slider_handle_t;
typedef void *toonz_spinbox_handle_t;
typedef void *toonz_checkbox_handle_t;
typedef void *toonz_radiobutton_handle_t;
typedef void *toonz_combobox_handle_t;

int add_param_field(toonz_param_view_handle_t view,
                    toonz_param_field_handle_t *ParamField);
int add_lineedit(toonz_param_view_handle_t view,
                 toonz_lineedit_handle_t *LineEdit);
int add_slider(toonz_param_view_handle_t view, toonz_slider_handle_t *Slider);
int add_spinbox(toonz_param_view_handle_t view,
                toonz_spinbox_handle_t *CheckBox);
int add_checkbox(toonz_param_view_handle_t view,
                 toonz_checkbox_handle_t *CheckBox);
int add_radiobutton(toonz_param_view_handle_t view,
                    toonz_radiobutton_handle_t *RadioButton);
int add_combobox(toonz_param_view_handle_t view,
                 toonz_combobox_handle_t *Combobox);

#undef TOONZ_DEFINE_ADD_COMPONENT

#endif
