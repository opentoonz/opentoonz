#pragma once

#ifndef TOONZ_STDFX_PLUGIN_HOST_H__
#define TOONZ_STDFX_PLUGIN_HOST_H__
#include <QThread>
#include <string>
#include <memory>
#include "tzeraryfx.h"
#if defined(_WIN32) || defined(_CYGWIN_)
#include <windows.h>
#endif
#include "toonz_plugin.h"
#include "toonz_hostif.h"

/*
namespace toonz {
        struct nodal_rasterfx_handler_t;
        struct host_interface_t;
        struct plugin_probe_t;
}
*/

/* probe で得られる静的な plugin 情報 */
class PluginDescription {
public:
  std::string name_;
  std::string vendor_;
  std::string id_;
  std::string note_;
  std::string url_;
  std::string fullname_;
  int clss_;
  toonz_plugin_version_t plugin_ver_;

public:
  PluginDescription(const toonz::plugin_probe_t *const probe);

  /* 'geometric' is known as 'Zerary' on toonz. we avoid using the word because
   * nobody did not understand a meaning of the word */
  bool is_geometric() const {
    return clss_ & TOONZ_PLUGIN_CLASS_MODIFIER_GEOMETRIC;
  }
};

class PluginInformation;

/* エフェクトのインスタンスを構築するためのクラス */
struct PluginDeclaration final : public TFxDeclaration {
  PluginDeclaration(PluginInformation *pi);
  TPersist *create() const final override;

private:
  PluginInformation *pi_;
};

class UIPage;
class Param;
class ParamView;

class ParamsPageSet;

struct port_description_t {
  bool input_;
  std::string name_;
  int type_;

public:
  port_description_t(bool input, const char *nm, int type)
      : input_(input), name_(nm), type_(type) {}
};

#if defined(_WIN32) || defined(_CYGWIN_)
typedef std::shared_ptr<std::remove_pointer<HMODULE>::type> library_t;
#else
typedef std::shared_ptr<void> library_t;
#endif

class PluginInformation {
public:
  PluginDeclaration *decl_;
  PluginDescription *desc_;

  library_t library_;

  toonz::nodal_rasterfx_handler_t *handler_;
  toonz::host_interface_t *host_;
  int (*ini_)(toonz::host_interface_t *);
  void (*fin_)(void);
  int ref_count_;
  int param_page_num_;
  std::unique_ptr<toonz_param_page_t[]> param_pages_;

  std::vector<UIPage *> ui_pages_;
  std::vector<ParamView *> param_views_;
  std::map<std::string, port_description_t> port_mapper_;

  std::vector<std::shared_ptr<void>>
      param_resources_; /* deep-copy に使う scratch area */
  std::vector<std::shared_ptr<std::string>>
      param_string_tbl_; /* shared_ptr< void > では non-virtual destructor
                            が呼ばれないので  */

public:
  PluginInformation()
      : desc_(NULL)
      , library_(NULL)
      , handler_(NULL)
      , host_(NULL)
      , ini_(NULL)
      , fin_(NULL)
      , ref_count_(1)
      , param_page_num_(0) {}

  ~PluginInformation();

  void add_ref();
  void release();
};

class Loader final : public QObject {
  Q_OBJECT;

public:
  Loader();

protected:
  void doLoad(const QString &file);
  void walkDirectory_(const QString &file);
public slots:
  void walkDirectory(const QString &file);
  void walkDictionary(const QString &file);
signals:
  void load_finished(PluginInformation *pi);
  void fixup();
};

class PluginLoadController final : public QObject {
  Q_OBJECT;
  QThread work_entity;

public:
  PluginLoadController(const std::string &basedir, QObject *listener);
  bool wait(int timeout_in_ms) { return work_entity.wait(timeout_in_ms); };
public slots:
  void result(PluginInformation *pi);
  void finished();

signals:
  void start(const QString &filepath);
};

class RasterFxPluginHost final : public TRasterFx, public TPluginInterface {
  PluginInformation *pi_;

  std::vector<std::shared_ptr<TFxPort>> inputs_;
  std::vector<std::shared_ptr<Param>> params_;
  void *user_data_;

  static bool validateKeyName(const char *name);

protected:
  virtual RasterFxPluginHost *newInstance(PluginInformation *pi) const;

public:
  RasterFxPluginHost(PluginInformation *pinfo);
  ~RasterFxPluginHost();

  void notify();

  const TPersistDeclaration *getDeclaration() const override;
  std::string getPluginId() const override;

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override;
  bool addInputPort(const std::string &nm, std::shared_ptr<TFxPort> port);
  bool addOutputPort(const std::string &nm, TRasterFxPort *port);

  TFx *clone(bool recursive) const override;

  // UIPage
  UIPage *createUIPage(const char *name);
  void build(ParamsPageSet *);
  std::string getUrl() const;

  // setup
  bool setParamStructure(int n, toonz_param_page_t *descs, int &err,
                         void *&pos);
  bool addPortDesc(port_description_t &&);

  Param *createParam(const toonz_param_desc_t *);
  Param *createParam(const char *name, toonz_param_type_enum e);
  Param *getParam(const char *name) const;
  ParamView *createParamView();

  bool isPlugin() const override { return true; }
  bool isPluginZerary() const override { return pi_->desc_->is_geometric(); }

  bool isZerary() const override { return isPluginZerary(); };
  void callStartRenderHandler() override;
  void callEndRenderHandler() override;
  void callStartRenderFrameHandler(const TRenderSettings *rs,
                                   double frame) override;
  void callEndRenderFrameHandler(const TRenderSettings *rs,
                                 double frame) override;
  void *getUserData();
  void setUserData(void *user_data);

  void createParamsByDesc();
  void createPortsByDesc();
};

#endif
