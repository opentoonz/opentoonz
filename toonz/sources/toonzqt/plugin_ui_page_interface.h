#pragma once

#ifndef PLUGIN_UI_PAGE_INTERFACE
#define PLUGIN_UI_PAGE_INTERFACE

#include "toonz_hostif.h"
#include "tfx.h"
#include "plugin_param_interface.h"
#include "plugin_param_view_interface.h"
#include "pluginhost.h"

/* 公開ヘッダからは引っ込められた都合上ここで宣言(内部ではまだ使っているので) */
typedef void *toonz_ui_page_handle_t;

/* あるエフェクトのパラメータ画面 */
class UIPage {
  class Group {
    std::string name_;
    std::vector<std::pair<std::string, ParamView *>> params_;

  public:
    Group(const std::string &name) : name_(name) {}

    Group(const Group &group) : name_(group.name_), params_(group.params_) {}

    inline const std::string &name() const { return name_; }

    inline void add_param(const std::string &param, ParamView *components) {
      params_.push_back(std::make_pair(param, components));
    }

    inline ParamsPage *build(RasterFxPluginHost *fx, ParamsPage *page) {
      page->beginGroup(name_.c_str());
      for (std::size_t i = 0, size = params_.size(); i < size; i++) {
        params_[i].second->build(fx, page, params_[i].first.c_str());
      }
      page->endGroup();
      return page;
    }
  };

  std::string name_;
  std::vector<Group *> groups_;

public:
  inline UIPage(const char *name) : name_(name) {}

  inline ~UIPage() {
    for (std::size_t i = 0, size = groups_.size(); i < size; ++i) {
      delete groups_[i];
    }
    groups_.clear();
  }

  inline int begin_group(const char *name) {
    groups_.push_back(NULL);
    groups_.back() = new Group(name);

    return TOONZ_OK;
  }

  inline int end_group(const char *name) {
    if (groups_.back()->name() != name) {
      return TOONZ_ERROR_NOT_FOUND;
    }

    return TOONZ_OK;
  }

  int bind_param(Param *param, ParamView *pt) {
    if (groups_.empty()) {
      return TOONZ_ERROR_PREREQUISITE;
    }

    groups_.back()->add_param(param->name(), pt);

    return TOONZ_OK;
  }

  void build(RasterFxPluginHost *fx, ParamsPageSet *pages) const {
    ParamsPage *page = pages->createParamsPage();
    for (std::size_t i = 0, size = groups_.size(); i < size; ++i) {
      groups_[i]->build(fx, page);
    }
    pages->addParamsPage(page, name_.c_str());
    page->setPageSpace();
  }

  UIPage *clone(TFx *fx) const {
    UIPage *page = new UIPage(name_.c_str());
    for (std::size_t i = 0, size = groups_.size(); i < size; ++i) {
      page->groups_.push_back(new Group(*groups_[i]));
    }
    return page;
  }
};

int begin_group(toonz_ui_page_handle_t page, const char *name);
int end_group(toonz_ui_page_handle_t page, const char *name);
int bind_param(toonz_ui_page_handle_t page, toonz_param_handle_t param,
               toonz_param_view_handle_t traits);

#endif
