#include "plugin_param_view_interface.h"

static int add_component(toonz_param_view_handle_t view, void **handle,
                         std::shared_ptr<ParamView::Component> component) {
  if (ParamView *_ = reinterpret_cast<ParamView *>(view)) {
    if (handle) {
      *handle = component.get();
    }
    _->add_component(std::move(component));
  } else {
    return TOONZ_ERROR_INVALID_HANDLE;
  }

  return TOONZ_OK;
}

#define TOONZ_DEFINE_ADD_COMPONENT(NAME, HANDLE, FIELD)                        \
  int NAME(toonz_param_view_handle_t view, HANDLE *handle) {                   \
    return add_component(view, handle, std::make_shared<ParamView::FIELD>());  \
  }

TOONZ_DEFINE_ADD_COMPONENT(add_param_field, toonz_param_field_handle_t,
                           ParamField);
TOONZ_DEFINE_ADD_COMPONENT(add_lineedit, toonz_lineedit_handle_t, LineEdit);
TOONZ_DEFINE_ADD_COMPONENT(add_slider, toonz_slider_handle_t, Slider);
TOONZ_DEFINE_ADD_COMPONENT(add_spinbox, toonz_spinbox_handle_t, SpinBox);
TOONZ_DEFINE_ADD_COMPONENT(add_checkbox, toonz_checkbox_handle_t, CheckBox);
TOONZ_DEFINE_ADD_COMPONENT(add_radiobutton, toonz_radiobutton_handle_t,
                           RadioButton);
TOONZ_DEFINE_ADD_COMPONENT(add_combobox, toonz_combobox_handle_t, ComboBox);

#undef TOONZ_DEFINE_ADD_COMPONENT
