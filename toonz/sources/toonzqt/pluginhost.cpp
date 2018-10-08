#include <sstream>
#include <string>
#include <utility>
#include <stdexcept>
#include <stdint.h>
#if defined(_WIN32) || defined(_CYGWIN_)
#define UUID UUID_
#include <windows.h>
#undef UUID
#else
#include <dlfcn.h>
#endif
#include <string>
#include <map>
#include <type_traits>
#include <functional>

// #include "tfxparam.h"
#include <toonzqt/addfxcontextmenu.h>  // as receiver
#include "tenv.h"
#include "../include/toonzqt/fxsettings.h"
#include "toonz/tcolumnfx.h"

#include "pluginhost.h"
#include "toonz_plugin.h"
#include "toonz_hostif.h"
#include "toonz_params.h"
#include "plugin_tile_interface.h"
#include "plugin_port_interface.h"
#include "plugin_fxnode_interface.h"
#include "plugin_param_interface.h"
#include "plugin_param_view_interface.h"
#include "plugin_ui_page_interface.h"
#include "plugin_utilities.h"
#include "toonz_params.h"
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QHBoxLayout>
#include <QFormLayout>
//#include <QtConcurrent>

#include "plugin_param_traits.h"
#include "../include/toonzqt/pluginloader.h"

using namespace toonz;  // plugin namespace

extern std::map<std::string, PluginInformation *> plugin_dict_;

/*
  PluginLoadController が main thread queue を使うことと,
  QThread で他スレッドの待ち合わせがしにくい(sendor thread が QThread::wait()
  でブロックしていると emit signal が処理できずデッドロックする)ので、
  大人しく polling にした.
 */
bool PluginLoader::load_entries(const std::string &basepath) {
  static PluginLoadController *aw = NULL; /* main() から一度だけ呼ばれる */
  if (!aw) {
    aw = new PluginLoadController(basepath, NULL);
  }
  bool ret    = aw->wait(16 /* ms */);
  if (ret) aw = NULL; /* deleteLater で消えるはず */
  return ret;
}

TFx *PluginLoader::create_host(const std::string &fxId) {
  std::string id = fxId.substr(5);
  auto it        = plugin_dict_.find(id);
  if (it != plugin_dict_.end()) {
    auto plugin = new RasterFxPluginHost(it->second);
    plugin->notify();
    return plugin;
  }
  return NULL;
}

std::map<std::string, QTreeWidgetItem *> PluginLoader::create_menu_items(
    std::function<void(QTreeWidgetItem *)> &&l1_handler,
    std::function<void(QTreeWidgetItem *)> &&l2_handler) {
  std::map<std::string, QTreeWidgetItem *> vendors;
  for (auto plugin : plugin_dict_) {
    PluginDescription *desc = plugin.second->desc_;
    if (vendors.count(desc->vendor_) == 0) {
      auto vendor = new QTreeWidgetItem(
          (QTreeWidget *)NULL,
          QStringList(QString::fromStdString(desc->vendor_)));
      vendors.insert(std::make_pair(desc->vendor_, vendor));
      l1_handler(vendor);
    }

    auto vendor = vendors[desc->vendor_];
    auto item   = new QTreeWidgetItem(
        (QTreeWidget *)NULL, QStringList(QString::fromStdString(desc->name_)));
    item->setData(0, Qt::UserRole,
                  QVariant("_plg_" + QString::fromStdString(desc->id_)));
    l2_handler(item);
    vendor->addChild(item);
  }
  return vendors;
}

static bool copy_rendering_setting(toonz_rendering_setting_t *dst,
                                   const TRenderSettings &src);

class PluginSetupMessage final : public TThread::Message {
  PluginInformation *pi_;

public:
  PluginSetupMessage(PluginInformation *pi) : pi_(pi) {}

  void onDeliver() override {
    RasterFxPluginHost *fx = new RasterFxPluginHost(pi_);
    if (pi_ && pi_->handler_) {
      pi_->handler_->setup(fx);
      /* fx は pi のラッパーとしてのみ構築されており、即座に削除される. 実
         instance に引き継がれないので ここで createParam()
         等を呼び出しても意味がない.
         ここで createParamsByDesc() などを呼び出しても、 instance の parameter
         は 0 になる.  */
    }
    delete fx;
  }

  TThread::Message *clone() const override {
    return new PluginSetupMessage(*this);
  }
};

PluginInformation::~PluginInformation() {
  if (library_) {
    if (library_.use_count() == 1 && fin_) {
      fin_();
    }
  }
}

void PluginInformation::add_ref() { ++ref_count_; }

void PluginInformation::release() {
  if (--ref_count_ == 0) {
    delete this;
  }
}

PluginDescription::PluginDescription(const plugin_probe_t *const probe) {
  name_       = probe->name ? probe->name : "unnamed-plugin";
  vendor_     = probe->vendor ? probe->vendor : "";
  id_         = probe->id ? probe->id : "unnamed-plugin.plugin";
  note_       = probe->note ? probe->note : "";
  url_        = probe->helpurl ? probe->helpurl : "";
  clss_       = probe->clss;
  fullname_   = id_ + "$" + name_ + "$" + vendor_;
  plugin_ver_ = probe->plugin_ver;
}

RasterFxPluginHost::RasterFxPluginHost(PluginInformation *pinfo)
    : TZeraryFx(), pi_(pinfo), user_data_(nullptr) {
  pi_->add_ref();
}

static int create_param_view(toonz_node_handle_t node,
                             toonz_param_view_handle_t *view) {
  ParamView *p = NULL;
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) {
      printf("create_param_view: invalid toonz_node_handle_t\n");
      return TOONZ_ERROR_INVALID_HANDLE;
    }

    if ((p = fx->createParamView())) {
      *view = p;
    } else {
      printf("create_param_view: invalid param name");
      return TOONZ_ERROR_FAILED_TO_CREATE;
    }
  } catch (const std::exception &e) {
    printf("create_param_view: exception: %s\n", e.what());
    delete p;
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int setup_input_port(toonz_node_handle_t node, const char *name,
                            int type) {
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) return TOONZ_ERROR_INVALID_HANDLE;
    if (!fx->addPortDesc({true, name, type})) {
      printf("add_input_port: failed to add: already have\n");
      return TOONZ_ERROR_BUSY;
    }
  } catch (const std::exception &e) {
    printf("setup_putput_port: exception: %s\n", e.what());
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int setup_output_port(toonz_node_handle_t node, const char *name,
                             int type) {
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) return TOONZ_ERROR_INVALID_HANDLE;
    if (!fx->addPortDesc({false, name, type})) {
      printf("add_input_port: failed to add: already have\n");
      return TOONZ_ERROR_BUSY;
    }
  } catch (const std::exception &e) {
    printf("setup_putput_port: exception: %s\n", e.what());
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int add_input_port(toonz_node_handle_t node, const char *name, int type,
                          toonz_port_handle_t *port) {
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) return TOONZ_ERROR_INVALID_HANDLE;
    auto p = std::make_shared<TRasterFxPort>();
    /* TRasterFxPort は non-copyable なスマートポインタなのでポインタで引き回す
     */
    if (!fx->addInputPort(name, p)) {  // overloaded version
      printf("add_input_port: failed to add: already have\n");
      return TOONZ_ERROR_BUSY;
    }
    *port = p.get();
  } catch (const std::exception &e) {
    printf("add_input_port: exception: %s\n", e.what());
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int get_input_port(toonz_node_handle_t node, const char *name,
                          toonz_port_handle_t *port) {
  if (!(node && port)) {
    return TOONZ_ERROR_NULL;
  }

  RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
  std::string portName(name);
  TFxPort *tfxport = fx->getInputPort(portName);
  if (!tfxport) {
    return TOONZ_ERROR_NOT_FOUND;
  }
  *port = tfxport;

  return TOONZ_OK;
}

static int add_output_port(toonz_node_handle_t node, const char *name, int type,
                           toonz_port_handle_t *port) {
  TRasterFxPort *p = NULL;
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) return TOONZ_ERROR_INVALID_HANDLE;
    p = new TRasterFxPort();
    /* TRasterFxPort は non-copyable なスマートポインタなのでポインタで引き回す
     */
    if (fx->addOutputPort(name, p)) {  // overloaded version
      delete p;
      return TOONZ_ERROR_BUSY;
    }
    *port = p;
  } catch (const std::exception &) {
    delete p;
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int get_rect(toonz_rect_t *rect, double *x0, double *y0, double *x1,
                    double *y1) {
  if (!(rect && x0 && y0 && x1 && y1)) {
    return -2;
  }
  *x0 = rect->x0;
  *y0 = rect->y0;
  *x1 = rect->x1;
  *y1 = rect->y1;
  return 0;
}

static int set_rect(toonz_rect_t *rect, double x0, double y0, double x1,
                    double y1) {
  if (!rect) {
    return -2;
  }
  rect->x0 = x0;
  rect->y0 = y0;
  rect->x1 = x1;
  rect->y1 = y1;
  return 0;
}

static int add_preference(toonz_node_handle_t node, const char *name,
                          toonz_ui_page_handle_t *ui) {
  UIPage *p = NULL;
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) {
      printf("add_preference: invalid toonz_node_handle_t\n");
      return TOONZ_ERROR_INVALID_HANDLE;
    }

    if ((p = fx->createUIPage(name))) {
      *ui = p;
    } else {
      printf("add_preference: failed to get UIPage\n");
      return TOONZ_ERROR_FAILED_TO_CREATE;
    }
  } catch (const std::exception &e) {
    printf("add_preference: exception: %s\n", e.what());
    delete p;
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int add_param(toonz_node_handle_t node, const char *name, int type,
                     toonz_param_handle_t *param) {
  Param *p = NULL;
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) {
      printf("add_param: invalid toonz_node_handle_t\n");
      return TOONZ_ERROR_INVALID_HANDLE;
    }

    if ((p = fx->createParam(name, toonz_param_type_enum(type)))) {
      *param = p;
    } else {
      printf("add_param: invalid type");
      return TOONZ_ERROR_FAILED_TO_CREATE;
    }
  } catch (const std::exception &e) {
    printf("add_param: exception: %s\n", e.what());
    delete p;
    return TOONZ_ERROR_UNKNOWN;
  }
  return TOONZ_OK;
}

static int get_param(toonz_node_handle_t node, const char *name,
                     toonz_param_handle_t *param) {
  try {
    RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
    if (!fx) {
      printf("get_param: invalid toonz_node_handle_t\n");
      return TOONZ_ERROR_INVALID_HANDLE;
    }

    if (Param *p = fx->getParam(name)) {
      *param = p;
    } else {
      printf("get_param: invalid type");
      return TOONZ_ERROR_NOT_FOUND;
    }
  } catch (const std::exception &) {
  }
  return TOONZ_OK;
}

static int set_user_data(toonz_node_handle_t node, void *user_data) {
  if (!node) {
    return TOONZ_ERROR_NULL;
  }
  RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
  fx->setUserData(user_data);
  return TOONZ_OK;
}

static int get_user_data(toonz_node_handle_t node, void **user_data) {
  if (!node || !user_data) {
    return TOONZ_ERROR_NULL;
  }
  RasterFxPluginHost *fx = reinterpret_cast<RasterFxPluginHost *>(node);
  *user_data             = fx->getUserData();
  return TOONZ_OK;
}

bool RasterFxPluginHost::addPortDesc(port_description_t &&desc) {
  printf("addPortDesc: name:%s dir:%d type:%d\n", desc.name_.c_str(),
         desc.input_, desc.type_);
  auto ret = pi_->port_mapper_.insert(std::make_pair(desc.name_, desc));
  return ret.second;
}

void RasterFxPluginHost::notify() {
  /* 最低限必要な setup をしてから通知する */
  QString nm = QString::fromStdString(pi_->desc_->name_.c_str());
  setName(nm.toStdWString());

  createParamsByDesc();
  createPortsByDesc();

  if (pi_ && pi_->handler_->create) pi_->handler_->create(this);
}

RasterFxPluginHost::~RasterFxPluginHost() {
  if (pi_ && pi_->handler_->destroy) {
    pi_->handler_->destroy(this);
    pi_->release();
  }
  inputs_.clear();
}

/*
 node を click するなどの要因で頻繁に呼ばれる.
 click した場合は FxsData::setFxs から呼ばれ、新しいインスタンスは
 FxsData::m_fxs に入れられ、 FxsData
 のインスタンスと同時に(大抵の場合は)即座に消される.
 */
TFx *RasterFxPluginHost::clone(bool recursive) const {
  RasterFxPluginHost *plugin = newInstance(pi_);
  plugin->user_data_         = user_data_;
  // clone ports before TFx::clone().
  for (auto &ip : pi_->port_mapper_) {
    if (ip.second.input_) {
#if 0
      /* addInputPort() 内で行われる port owner の更新は後勝ちだが,
         clone された新しいインスタンスのほうが先に消えてしまう場合に, 無効なポインタを示す owner が port に残ってしまう. この問題が解決したら共有できるようにしたい.
         (このため、 plugin 空間に通知される全ての handle には一貫性がない. ただし、後から一貫性がなくなるよりは遥かにいいだろう)
      */
      plugin->addInputPort(getInputPortName(i), ip);
#else
      plugin->addInputPort(ip.first,
                           std::shared_ptr<TFxPort>(new TRasterFxPort));
#endif
    }
  }

  printf("recursive:%d params:%d\n", (int)recursive, (int)params_.size());
  // clone params before TFx::clone().
  /* ui_pages_, param_views_ は pi に移ったが createParam
   * の呼び出しだけはしておかないと Fx Settings 構築時に assert failed になる */
  for (auto const &param : params_) {
    /* 古い createParam() は desc
     * をとらず、コンストラクト時にデフォルト値を持つタイプの T*Param
     * を再作成できない */
    plugin->createParam(param->desc());
  }

  return TFx::clone(plugin, recursive);
}

RasterFxPluginHost *RasterFxPluginHost::newInstance(
    PluginInformation *pi) const {
  return new RasterFxPluginHost(pi);
}

const TPersistDeclaration *RasterFxPluginHost::getDeclaration() const {
  printf("RasterFxPluginHost::getDeclaration()\n");
  return pi_->decl_;
}

PluginDeclaration::PluginDeclaration(PluginInformation *pi)
    : TFxDeclaration(TFxInfo(pi->desc_->id_, false)), pi_(pi) {}

TPersist *PluginDeclaration::create() const {
  RasterFxPluginHost *fx = new RasterFxPluginHost(pi_);
  fx->notify();
  return fx;
}

std::string RasterFxPluginHost::getPluginId() const { return pi_->desc_->id_; };

void *RasterFxPluginHost::getUserData() { return user_data_; }

void RasterFxPluginHost::setUserData(void *user_data) {
  user_data_ = user_data;
}

bool RasterFxPluginHost::doGetBBox(double frame, TRectD &bbox,
                                   const TRenderSettings &info) {
  using namespace plugin::utils;
  bool ret = true; /* 負論理 */
  if (pi_ && pi_->handler_->do_get_bbox) {
    rendering_setting_t info_;
    copy_rendering_setting(&info_, info);

    rect_t rc;
    copy_rect(&rc, bbox);

    ret  = ret && pi_->handler_->do_get_bbox(this, &info_, frame, &rc);
    bbox = restore_rect(&rc);
  }
  return !ret;
}

void RasterFxPluginHost::doCompute(TTile &tile, double frame,
                                   const TRenderSettings &info) {
  if (pi_ && pi_->handler_->do_compute) {
    rendering_setting_t info_;
    copy_rendering_setting(&info_, info);
    pi_->handler_->do_compute(this, &info_, frame, &tile);
  }
}

int RasterFxPluginHost::getMemoryRequirement(const TRectD &rect, double frame,
                                             const TRenderSettings &info) {
  using namespace plugin::utils;
  if (pi_ && pi_->handler_->get_memory_requirement) {
    rendering_setting_t rs;
    copy_rendering_setting(&rs, info);

    rect_t rc;
    copy_rect(&rc, rect);

    size_t ignore =
        pi_->handler_->get_memory_requirement(this, &rs, frame, &rc);
    return 0;
  }
  return 0;
}

bool RasterFxPluginHost::canHandle(const TRenderSettings &info, double frame) {
  if (pi_ && pi_->handler_->can_handle) {
    rendering_setting_t rs;
    copy_rendering_setting(&rs, info);
    return pi_->handler_->can_handle(this, &rs, frame);
  }
  /* 適切なデフォルト値は 'geometric' の場合とそうでない場合で異なる */
  return isPluginZerary(); /* better-default depends on that is 'geometric' or
                              not */
}

bool RasterFxPluginHost::addInputPort(const std::string &nm,
                                      std::shared_ptr<TFxPort> port) {
  /* setOwnFx は addInputPort 内で行われている. setFx()
   * は接続なので自分自身に呼んではダメ */
  // port->setFx(this);
  bool ret = TFx::addInputPort(nm, *port.get());
  if (ret) {
    inputs_.push_back(port);
  }
  return ret;
}

bool RasterFxPluginHost::addOutputPort(const std::string &nm,
                                       TRasterFxPort *port) {
  port->setFx(this);
  return TFx::addOutputConnection(port);
}

void RasterFxPluginHost::callStartRenderHandler() {
  if (pi_ && pi_->handler_ && pi_->handler_->start_render) {
    pi_->handler_->start_render(this);
  }
}

void RasterFxPluginHost::callEndRenderHandler() {
  if (pi_ && pi_->handler_ && pi_->handler_->end_render) {
    pi_->handler_->end_render(this);
  }
}

void RasterFxPluginHost::callStartRenderFrameHandler(const TRenderSettings *rs,
                                                     double frame) {
  toonz_rendering_setting_t trs;
  copy_rendering_setting(&trs, *rs);
  if (pi_ && pi_->handler_ && pi_->handler_->on_new_frame) {
    pi_->handler_->on_new_frame(this, &trs, frame);
  }
}

void RasterFxPluginHost::callEndRenderFrameHandler(const TRenderSettings *rs,
                                                   double frame) {
  toonz_rendering_setting_t trs;
  copy_rendering_setting(&trs, *rs);
  if (pi_ && pi_->handler_ && pi_->handler_->on_end_frame) {
    pi_->handler_->on_end_frame(this, &trs, frame);
  }
}

std::string RasterFxPluginHost::getUrl() const { return pi_->desc_->url_; }

UIPage *RasterFxPluginHost::createUIPage(const char *name) {
  pi_->ui_pages_.push_back(NULL);
  pi_->ui_pages_.back() = new UIPage(name);
  return pi_->ui_pages_.back();
}

// deprecated. for migration.
Param *RasterFxPluginHost::createParam(const char *name,
                                       toonz_param_type_enum e) {
  toonz_param_desc_t *desc = new toonz_param_desc_t;
  memset(desc, 0, sizeof(toonz_param_desc_t));
  desc->base.ver   = {1, 0};
  desc->key        = name;
  desc->traits_tag = e;
  return createParam(desc);
}

Param *RasterFxPluginHost::createParam(const toonz_param_desc_t *desc) {
  TParamP p = parameter_factory(desc);
  if (!p) return nullptr;

  p->setDescription(desc->note);
  p->setUILabel(desc->base.label);

  bindPluginParam(this, desc->key, p);

  params_.push_back(std::make_shared<Param>(
      this, desc->key, toonz_param_type_enum(desc->traits_tag), desc));
  return params_.back().get();
}

Param *RasterFxPluginHost::getParam(const char *name) const {
  for (auto &param : params_) {
    if (param->name() == name) {
      return param.get();
    }
  }
  return nullptr;
}

ParamView *RasterFxPluginHost::createParamView() {
  pi_->param_views_.push_back(NULL);
  pi_->param_views_.back() = new ParamView();
  return pi_->param_views_.back();
}

/* build で構築された GUI は plugin のインスタンスには紐づかない.
 * 通常一度だけ呼ばれ使い回される.  */
void RasterFxPluginHost::build(ParamsPageSet *pages) {
  printf(">>>> RasterFxPluginHost::build: ui_pages:%d\n",
         (int)pi_->ui_pages_.size());
  for (std::size_t i = 0, size = pi_->ui_pages_.size(); i < size; ++i) {
    pi_->ui_pages_[i]->build(this, pages);
  }
  auto aboutpage = pages->createParamsPage();

#if 1
  /* FIXME: fxsettings で大きさの測定のためにいろいろやっているので使える
     layout/widget に制限がありそう.
     しかしなぜか最後の widget しか出ない */
  aboutpage->beginGroup("Name");
  aboutpage->addWidget(new QLabel(pi_->desc_->name_.c_str(), aboutpage));
  aboutpage->endGroup();
  aboutpage->beginGroup("Vendor");
  aboutpage->addWidget(new QLabel(pi_->desc_->vendor_.c_str(), aboutpage));
  aboutpage->endGroup();
  aboutpage->beginGroup("Version");
  auto version =
      QString::fromStdString(std::to_string(pi_->desc_->plugin_ver_.major)) +
      "." +
      QString::fromStdString(std::to_string(pi_->desc_->plugin_ver_.minor));
  aboutpage->addWidget(new QLabel(version, aboutpage));
  aboutpage->endGroup();
  aboutpage->beginGroup("Note");
  aboutpage->addWidget(new QLabel(pi_->desc_->note_.c_str()), aboutpage);
  aboutpage->endGroup();
#endif
  pages->addParamsPage(aboutpage, "Version");
  aboutpage->setPageSpace();
}

template <typename T, uint32_t compat_maj, uint32_t compat_min>
static inline bool is_compatible(const T &d) {
  return d.ver.major == compat_maj && d.ver.minor == compat_min;
}

template <uint32_t compat_maj, uint32_t compat_min>
static inline bool is_compatible(const toonz_if_version_t &v) {
  return v.major == compat_maj && v.minor == compat_min;
}

static int check_base_sanity(const toonz_param_page_t *p) {
  int err = 0;
  if (!is_compatible<toonz_param_base_t_, 1, 0>(p->base))
    err |= TOONZ_PARAM_ERROR_VERSION;
  if (p->base.label == NULL) err |= TOONZ_PARAM_ERROR_LABEL;
  if (p->base.type != TOONZ_PARAM_DESC_TYPE_PAGE) err |= TOONZ_PARAM_ERROR_TYPE;
  return err;
}

static int check_base_sanity(const toonz_param_group_t *p) {
  int err = 0;
  if (!is_compatible<toonz_param_base_t_, 1, 0>(p->base))
    err |= TOONZ_PARAM_ERROR_VERSION;
  if (p->base.label == NULL) err |= TOONZ_PARAM_ERROR_LABEL;
  if (p->base.type != TOONZ_PARAM_DESC_TYPE_GROUP)
    err |= TOONZ_PARAM_ERROR_TYPE;
  return err;
}

static int check_base_sanity(const toonz_param_desc_t *p) {
  int err = 0;
  if (!is_compatible<toonz_param_base_t_, 1, 0>(p->base))
    err |= TOONZ_PARAM_ERROR_VERSION;
  if (p->base.label == NULL) err |= TOONZ_PARAM_ERROR_LABEL;
  if (p->base.type != TOONZ_PARAM_DESC_TYPE_PARAM)
    err |= TOONZ_PARAM_ERROR_TYPE;
  return err;
}

bool RasterFxPluginHost::setParamStructure(int n, toonz_param_page_t *p,
                                           int &err, void *&pos) {
  /* 適当に現実的な最大値: あまりに大きい場合は領域の破壊を疑う */
  static const int max_pages_  = 31;
  static const int max_groups_ = 32;
  static const int max_params_ = 65535;

  pos = p;
  if (pi_) {
    if (n > max_pages_ || p == NULL) {
      /* parameter が null
       * でないことは上位でチェックされているはずで、ここで返せるエラーは定義していない.
       */
      if (p == NULL) err |= TOONZ_PARAM_ERROR_UNKNOWN;
      err |= TOONZ_PARAM_ERROR_PAGE_NUM;
      return false;
    }

    /* SAN 値チェック */
    for (int k = 0; k < n; k++) {
      toonz_param_page_t *pg = &p[k];
      pos                    = pg;
      if (int e = check_base_sanity(pg)) {
        err = e;
        return false;
      }
      if (pg->num > max_groups_) {
        err |= TOONZ_PARAM_ERROR_GROUP_NUM;
        return false;
      }

      for (int l = 0; l < pg->num; l++) {
        toonz_param_group_t *grp = &pg->array[l];
        pos                      = grp;
        if (int e = check_base_sanity(grp)) {
          err = e;
          return false;
        }
        if (grp->num > max_params_) {
          err |= TOONZ_PARAM_ERROR_GROUP_NUM;
          return false;
        }
        for (int i = 0; i < grp->num; i++) {
          toonz_param_desc_t *desc = &grp->array[i];
          pos                      = desc;
          if (int e = check_base_sanity(desc)) err |= e;
          if (desc->key == NULL)
            err |= TOONZ_PARAM_ERROR_NO_KEY;
          else {
            if (!validateKeyName(desc->key)) err |= TOONZ_PARAM_ERROR_KEY_NAME;
            for (auto it : params_) {
              if (it->name() == desc->key) {
                err |= TOONZ_PARAM_ERROR_KEY_DUP;
                break;
              }
            }
          }
          if (desc->reserved_[0] ||
              desc->reserved_[1])  // reserved fields must be zero
            err |= TOONZ_PARAM_ERROR_POLLUTED;
          err |= check_traits_sanity(desc);

          if (err) return false;
        }
      }
    }

    if (err) return false;

    // deep copy param resources
    std::vector<std::shared_ptr<void>> &param_resources = pi_->param_resources_;
    std::vector<std::shared_ptr<std::string>> &strtbl = pi_->param_string_tbl_;

    auto patch_string = [&](const char *srcstr) {
      strtbl.push_back(std::shared_ptr<std::string>(new std::string("")));
      if (srcstr) strtbl.back()->assign(srcstr);
      return strtbl.back()->c_str();
    };

    auto deep_copy_base = [&](toonz_param_base_t_ &dst,
                              const toonz_param_base_t_ &src) {
      dst.ver   = src.ver;
      dst.type  = src.type;
      dst.label = patch_string(src.label);
    };

    param_resources.clear();

    std::unique_ptr<toonz_param_page_t[]> origin_pages(
        new toonz_param_page_t[n]);
    for (int i = 0; i < n; i++) {
      toonz_param_page_t &dst_page       = origin_pages[i];
      const toonz_param_page_t &src_page = p[i];

      deep_copy_base(dst_page.base, src_page.base);

      const int group_num = dst_page.num = src_page.num;
      dst_page.array                     = new toonz_param_group_t[group_num];

      for (int j = 0; j < group_num; j++) {
        toonz_param_group_t &dst_group       = dst_page.array[j];
        const toonz_param_group_t &src_group = src_page.array[j];

        deep_copy_base(dst_group.base, src_group.base);

        const int desc_num = dst_group.num = src_group.num;
        dst_group.array                    = new toonz_param_desc_t[desc_num];
        for (int k = 0; k < desc_num; k++) {
          toonz_param_desc_t &dst_desc       = dst_group.array[k];
          const toonz_param_desc_t &src_desc = src_group.array[k];

          deep_copy_base(dst_desc.base, src_desc.base);  // base

          dst_desc.key  = patch_string(src_desc.key);   // key
          dst_desc.note = patch_string(src_desc.note);  // note
          memcpy(dst_desc.reserved_, src_desc.reserved_,
                 sizeof(src_desc.reserved_));         // reserved fields
          dst_desc.traits_tag = src_desc.traits_tag;  // tag

          // traits
          if (dst_desc.traits_tag == TOONZ_PARAM_TYPE_ENUM) {
            dst_desc.traits.e.def = src_desc.traits.e.def;
            int enums = dst_desc.traits.e.enums = src_desc.traits.e.enums;
            auto a = std::shared_ptr<void>(new char *[enums]);
            param_resources.push_back(a);
            dst_desc.traits.e.array = static_cast<const char **>(a.get());
            for (int i = 0; i < enums; i++)
              dst_desc.traits.e.array[i] =
                  patch_string(src_desc.traits.e.array[i]);
          } else if (dst_desc.traits_tag == TOONZ_PARAM_TYPE_SPECTRUM) {
            int points = dst_desc.traits.g.points = src_desc.traits.g.points;
            auto ptr                              = std::shared_ptr<void>(
                new toonz_param_traits_spectrum_t::valuetype[points]);
            param_resources.push_back(ptr);
            dst_desc.traits.g.array =
                static_cast<toonz_param_traits_spectrum_t::valuetype *>(
                    ptr.get());

            for (int i = 0; i < dst_desc.traits.g.points; i++)
              memcpy(&dst_desc.traits.g.array[i], &src_desc.traits.g.array[i],
                     sizeof(toonz_param_traits_spectrum_t::valuetype));
          } else if (dst_desc.traits_tag == TOONZ_PARAM_TYPE_STRING) {
            dst_desc.traits.s.def = patch_string(src_desc.traits.s.def);
          } else if (dst_desc.traits_tag == TOONZ_PARAM_TYPE_TONECURVE) {
            /* has no default values */
          } else {
            memcpy(&dst_desc.traits, &src_desc.traits, sizeof(src_desc.traits));
          }
        }
      }
    }

    pi_->param_page_num_ = n;
    pi_->param_pages_    = std::move(origin_pages);
    return true;
  }
  return false;
}

void RasterFxPluginHost::createParamsByDesc() {
  printf("RasterFxPluginHost::createParamsByDesc: num:%d\n",
         pi_->param_page_num_);
  for (int k = 0; k < pi_->param_page_num_; k++) {
    toonz_param_page_t *pg = &pi_->param_pages_[k];
    void *page             = NULL;
    int r                  = add_preference(this, pg->base.label, &page);
    printf(
        "RasterFxPluginHost::createParamsByDesc: add_preference: r:0x%x "
        "page:%p\n",
        r, page);

    for (int l = 0; l < pg->num; l++) {
      toonz_param_group_t *grp = &pg->array[l];
      begin_group(page, grp->base.label);

      for (int i = 0; i < grp->num; i++) {
        toonz_param_desc_t *desc = &grp->array[i];
        Param *p                 = createParam(desc);
        printf("RasterFxPluginHost::createParam: p:%p key:%s tag:%d\n", p,
               desc->key, desc->traits_tag);
        if (p) {
          void *v = NULL;
          int r   = create_param_view(this, &v);
          printf(
              "RasterFxPluginHost::createParam: create_param_view: r:0x%x "
              "v:%p\n",
              r, v);
          r = add_param_field(v, NULL);
          printf(
              "RasterFxPluginHost::createParam: add_param_field: r:0x%x v:%p "
              "p:%p\n",
              r, v, p);
          /* set_param_range()
           * の中で型チェックをしているので全型について呼び出してよい */

          r = bind_param(page, p, v);

          set_param_default<tpbind_dbl_t>(p, desc);
          set_param_default<tpbind_int_t>(p, desc);
          set_param_default<tpbind_rng_t>(p, desc);
          set_param_default<tpbind_pnt_t>(p, desc);
          set_param_default<tpbind_enm_t>(p, desc);
          set_param_default<tpbind_col_t>(p, desc);
          set_param_default<tpbind_bool_t>(p, desc);
          set_param_default<tpbind_str_t>(p, desc);
          set_param_default<tpbind_spc_t>(p, desc);
          set_param_default<tpbind_tcv_t>(p, desc);

          set_param_range<tpbind_dbl_t>(p, desc);
          set_param_range<tpbind_int_t>(p, desc);
          set_param_range<tpbind_rng_t>(p, desc);
          set_param_range<tpbind_pnt_t>(p, desc);

          printf("RasterFxPluginHost::createParam: bind_param: r:0x%x\n", r);
        }
      }
      end_group(page, grp->base.label);
    }
  }
}

/*
パラメタのキー名として適切か確認
*/
bool RasterFxPluginHost::validateKeyName(const char *name) {
  if (name[0] == '\0') return false;
  if (!isalpha(name[0]) && name[0] != '_') return false;

  for (int i = 1; name[i] != '\0'; i++)
    if (!isalnum(name[i]) && name[i] != '_') return false;

  /* XMLの仕様ではXMLから始まるタグ名は認められないので、ここで弾く */
  if (strlen(name) >= 3 && (name[0] == 'X' || name[0] == 'x') &&
      (name[1] == 'M' || name[1] == 'm') && (name[2] == 'L' || name[2] == 'l'))
    return false;

  return true;
}

/*
 strict sanity check:
 未初期化値を受け入れて互換性が崩れないよう厳しくチェックする
 */
#define VERBOSE
static inline bool check(const plugin_probe_t *begin,
                         const plugin_probe_t *end) {
  /*
  printf("dump toonz_plugin_probe_t: ver:(%d, %d) (%s, %s, %s, %s) resv:[%p, %p,
  %p, %p, %p] clss:0x%x resv:[%d, %d, %d, %d, %d]\n",
             x->ver.major, x->ver.minor,
             x->name, x->id, x->note, x->url,
             x->reserved_ptr_[0], x->reserved_ptr_[1], x->reserved_ptr_[2],
  x->reserved_ptr_[3], x->reserved_ptr_[4],
             x->clss,
             x->reserved_int_[0], x->reserved_int_[1], x->reserved_int_[2],
  x->reserved_int_[3], x->reserved_int_[4], x->reserved_int_[5],
  x->reserved_int_[6], x->reserved_int_[7]);
  */
  int idx = 0;
  if (!is_compatible<plugin_probe_t, 1, 0>(*begin)) {
#if defined(VERBOSE)
    printf("sanity check(): first interface version is unknown\n");
#endif
    return false;
  }

  toonz_if_version_t v = begin->ver;
  for (auto x = begin; x < end; x++, idx++) {
    /* 異なるバージョンの構造体の混在はエラーとする.
       しかし toonz_plugin_probe_t は reservation filed
       を持っており、サイズが変わらない限りは混在も対応可能だが、まずは sanity
       check で落とす.

       For now we permit mixed versions. Not that we never support it since size
       of toonz_plugin_probe_t is constant.
    */
    if (!(x->ver.major == v.major && x->ver.minor == v.minor)) {
#if defined(VERBOSE)
      printf(
          "sanity check(): versions are ambiguous: first:(%d, %d) "
          "plugin[%d]:(%d, %d)\n",
          v.major, v.minor, idx, x->ver.major, x->ver.minor);
#endif
      return false;
    }

    if (!x->clss) {
#if defined(VERBOSE)
      printf("sanity check(): plugin[%d] class is zero\n", idx);
#endif
      return false;
    } else {
      uint32_t m = x->clss & TOONZ_PLUGIN_CLASS_MODIFIER_MASK;
      uint32_t c = x->clss & ~TOONZ_PLUGIN_CLASS_MODIFIER_MASK;
      if (!(m == 0 || m == TOONZ_PLUGIN_CLASS_MODIFIER_GEOMETRIC)) {
#if defined(VERBOSE)
        printf("sanity check(): plugin[%d] unknown modifier: 0x%x\n", idx, m);
#endif
        return false;  // we don't know the modifier
      }
      if (!(c == TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB)) {
#if defined(VERBOSE)
        printf("sanity check(): plugin[%d] unknown class: 0x%x\n", idx, c);
#endif
        return false;  // we don't know the class
      }
    }

    // reservations are must be zero
    for (int i = 0; i < 3; i++)
      if (x->reserved_ptr_[i]) {
#if defined(VERBOSE)
        printf(
            "sanity check(): plugin[%d] reserved_ptr_[%d] is NOT all zero-ed\n",
            idx, i);
#endif
        return false;
      }
    for (int i = 0; i < 7; i++)
      if (x->reserved_int_[i]) {
#if defined(VERBOSE)
        printf(
            "sanity check(): plugin[%d] reserved_int_[%d] is NOT all zero-ed\n",
            idx, i);
#endif
        return false;
      }
    for (int i = 0; i < 3; i++)
      if (x->reserved_ptr_trail_[i]) {
#if defined(VERBOSE)
        printf(
            "sanity check(): plugin[%d] reserved_ptr_trail_[%d] is NOT all "
            "zero-ed\n",
            idx, i);
#endif
        return false;
      }

    if (x->handler == NULL) {
#if defined(VERBOSE)
      printf("sanity check(): plugin[%d] handler is null\n", idx);
#endif
      return false;  // handler must be NOT null
    }
  }

  // check if the end is empty
  const char *b = reinterpret_cast<const char *>(end);
  for (int i = 0; i < sizeof(plugin_probe_t); i++) {
    if (b[i] != 0) {
#if defined(VERBOSE)
      printf("sanity check(): empty is NOT all zero-ed\n");
#endif
      return false;  // must be zero
    }
  }
  return true;
}

static inline bool check_and_copy(
    nodal_rasterfx_handler_t *__restrict dst,
    const nodal_rasterfx_handler_t *__restrict src) {
  // do we know the version?
  if (!(src->ver.major == 1 && src->ver.minor == 0)) return false;
  dst->ver                    = src->ver;
  dst->do_compute             = src->do_compute;
  dst->do_get_bbox            = src->do_get_bbox;
  dst->can_handle             = src->can_handle;
  dst->get_memory_requirement = src->get_memory_requirement;
  dst->on_new_frame           = src->on_new_frame;
  dst->on_end_frame           = src->on_end_frame;
  dst->create                 = src->create;
  dst->destroy                = src->destroy;
  dst->setup                  = src->setup;
  dst->start_render           = src->start_render;
  dst->end_render             = src->end_render;
  return true;
}

static inline bool uuid_matches(const UUID *x, const UUID *y) {
  return x->uid0 == y->uid0 && x->uid1 == y->uid1 && x->uid2 == y->uid2 &&
         x->uid3 == y->uid3 && x->uid4 == y->uid4;
}

static UUID uuid_nodal_ = {0xCC14EA21, 0x13D8, 0x4A3B, 0x9375, 0xAA4F68C9DDDD};
static UUID uuid_port_  = {0x2F89A423, 0x1D2D, 0x433F, 0xB93E, 0xCFFD83745F6F};
static UUID uuid_tile_  = {0x882BD525, 0x937E, 0x427C, 0x9D68, 0x4ECA651F6562};
// static UUID uuid_ui_page_ = {0xD2EF0310, 0x3414, 0x4753, 0x84CA,
// 0xD5447C70DD89};
static UUID uuid_fx_node_ = {0x26F9FC53, 0x632B, 0x422F, 0x87A0,
                             0x8A4547F55474};
// static UUID uuid_param_view_ = {0x5133A63A, 0xDD92, 0x41BD, 0xA255,
// 0x6F97BE7292EA};
static UUID uuid_param_ = {0x2E3E4A55, 0x8539, 0x4520, 0xA266, 0x15D32189EC4D};
static UUID uuid_setup_ = {0xcfde9107, 0xc59d, 0x414c, 0xae4a, 0x3d115ba97933};
static UUID uuid_null_  = {0, 0, 0, 0, 0};

template <typename T, int major, int minor>
T *base_interface_factory() {
  T *t = new T;
  memset(t, 0, sizeof(T));
  t->ver.major = major;
  t->ver.minor = minor;
  return t;
}

template <typename T, uint32_t major, uint32_t minor>
struct interface_t {
  static T *factory() {
    T *t = base_interface_factory<T, major, minor>();
    return t;
  }
};

extern "C" {
int set_parameter_pages(toonz_node_handle_t, int num,
                        toonz_param_page_t *params);
int set_parameter_pages_with_error(toonz_node_handle_t, int num,
                                   toonz_param_page_t *params, int *, void **);
}

template <uint32_t major, uint32_t minor>
struct interface_t<setup_interface_t, major, minor> {
  static setup_interface_t *factory() {
    setup_interface_t *t =
        base_interface_factory<setup_interface_t, major, minor>();
    t->set_parameter_pages            = set_parameter_pages;
    t->set_parameter_pages_with_error = set_parameter_pages_with_error;
    t->add_input_port                 = setup_input_port;
    return t;
  }
};

/*
template < uint32_t major, uint32_t minor >
struct interface_t < ui_page_interface_t, major, minor > {
        static ui_page_interface_t* factory()
        {
                ui_page_interface_t* t = base_interface_factory<
ui_page_interface_t, major, minor >();
                t->begin_group = begin_group;
                t->end_group = end_group;
                t->bind_param = bind_param;
                return t;
        }
};

template < uint32_t major, uint32_t minor >
struct interface_t < param_view_interface_t, major, minor > {
        static param_view_interface_t* factory()
        {
                param_view_interface_t* t = base_interface_factory<
param_view_interface_t, major, minor >();
                t->add_param_field = add_param_field;
                t->add_custom_field = add_custom_field;
                t->add_lineedit = add_lineedit;
                t->add_slider = add_slider;
                t->add_spinbox = add_spinbox;
                t->add_checkbox = add_checkbox;
                t->add_radiobutton = add_radiobutton;
                t->add_combobox = add_combobox;
                return t;
        }
};
*/

template <uint32_t major, uint32_t minor>
struct interface_t<param_interface_t, major, minor> {
  static param_interface_t *factory() {
    param_interface_t *t =
        base_interface_factory<param_interface_t, major, minor>();
    t->get_type = get_type;
    // t->hint_default_value = hint_default_value;
    // t->hint_value_range = hint_value_range;
    // t->hint_unit = hint_unit;
    // t->hint_item = hint_item;
    // t->get_value_type = get_value_type;
    t->get_value = get_value;
    // t->set_value = set_value;
    t->get_string_value   = get_string_value;
    t->get_spectrum_value = get_spectrum_value;
    return t;
  }
};

template <uint32_t major, uint32_t minor>
struct interface_t<node_interface_t, major, minor> {
  static node_interface_t *factory() {
    node_interface_t *t =
        base_interface_factory<node_interface_t, major, minor>();
    // t->add_input_port = add_input_port;
    // t->add_output_port = add_output_port;
    t->get_input_port = get_input_port;
    t->get_rect       = get_rect;
    t->set_rect       = set_rect;
    // t->add_preference = add_preference;
    // t->add_param = add_param;
    t->get_param = get_param;
    // t->create_param_view = create_param_view;
    t->set_user_data = set_user_data;
    t->get_user_data = get_user_data;
    return t;
  }
};

template <uint32_t major, uint32_t minor>
struct interface_t<toonz_tile_interface_t, major, minor> {
  static toonz_tile_interface_t *factory() {
    toonz_tile_interface_t *t =
        base_interface_factory<toonz_tile_interface_t, major, minor>();
    t->get_raw_address_unsafe = tile_interface_get_raw_address_unsafe;
    t->get_raw_stride         = tile_interface_get_raw_stride;
    t->get_element_type       = tile_interface_get_element_type;
    t->copy_rect              = tile_interface_copy_rect;
    t->create_from            = tile_interface_create_from;
    t->create                 = tile_interface_create;
    t->destroy                = tile_interface_destroy;
    t->get_rectangle          = tile_interface_get_rectangle;
    t->safen                  = tile_interface_safen;
    return t;
  }
};

template <uint32_t major, uint32_t minor>
struct interface_t<port_interface_t, major, minor> {
  static port_interface_t *factory() {
    port_interface_t *t =
        base_interface_factory<port_interface_t, major, minor>();
    t->is_connected = is_connected;
    t->get_fx       = get_fx;
    return t;
  }
};

template <uint32_t major, uint32_t minor>
struct interface_t<fxnode_interface_t, major, minor> {
  static fxnode_interface_t *factory() {
    fxnode_interface_t *t =
        base_interface_factory<fxnode_interface_t, major, minor>();
    t->get_bbox             = fxnode_get_bbox;
    t->can_handle           = fxnode_can_handle;
    t->get_input_port_count = fxnode_get_input_port_count;
    t->get_input_port       = fxnode_get_input_port;
    t->compute_to_tile      = fxnode_compute_to_tile;
    return t;
  }
};

/*
template <>
template < uint32_t major, uint32_t minor >
struct interface_t< toonz_nodal_rasterfx_interface_t >::i_< major, minor > {
        static toonz_nodal_rasterfx_interface_t* factory()
        {
                printf("toonz_nodal_rasterfx_interface_t::factory\n");
                toonz_nodal_rasterfx_interface_t* t = base_interface_factory<
toonz_nodal_rasterfx_interface_t, major, minor >();
                return t;
        }
};
*/

template <typename T, uint32_t major, uint32_t minor>
T *interface_factory() {
  // return interface_t< T >::i_< major, minor >().factory();
  return interface_t<T, major, minor>::factory();
}

static int query_interface(const UUID *uuid, void **interf) {
  typedef std::pair<const UUID *, int> uuid_dict_t;
  static const uuid_dict_t dict[] = {
      uuid_dict_t(&uuid_nodal_, 1), uuid_dict_t(&uuid_port_, 2),
      uuid_dict_t(&uuid_tile_, 3),
      // uuid_dict_t(&uuid_ui_page_, 4),
      uuid_dict_t(&uuid_fx_node_, 5),
      // uuid_dict_t(&uuid_param_view_, 6),
      uuid_dict_t(&uuid_param_, 7), uuid_dict_t(&uuid_setup_, 8),
      uuid_dict_t(&uuid_null_, 0)};

  if (!(uuid && interf)) return TOONZ_ERROR_NULL;

  try {
    const uuid_dict_t *it = &dict[0];
    while (it->first != &uuid_null_) {
      if (uuid_matches(it->first, uuid)) {
        switch (it->second) {
        case 1:
          *interf = interface_factory<toonz_node_interface_t, 1, 0>();
          break;
        case 2:
          *interf = interface_factory<toonz_port_interface_t, 1, 0>();
          break;
        case 3:
          *interf = interface_factory<toonz_tile_interface_t, 1, 0>();
          break;
        // case 4:
        //	*interf = interface_factory< toonz_ui_page_interface_t, 1, 0
        //>();
        //	break;
        case 5:
          *interf = interface_factory<toonz_fxnode_interface_t, 1, 0>();
          break;
        // case 6:
        //*interf = interface_factory< toonz_param_view_interface_t, 1, 0 >();
        // break;
        case 7:
          *interf = interface_factory<toonz_param_interface_t, 1, 0>();
          break;
        case 8:
          *interf = interface_factory<toonz_setup_interface_t, 1, 0>();
          break;
        default:
          return TOONZ_ERROR_NOT_IMPLEMENTED;
          break;
        }
      }
      it++;
    }
  } catch (const std::bad_alloc &) {
    return TOONZ_ERROR_OUT_OF_MEMORY;
  } catch (const std::exception &) {
    return TOONZ_ERROR_UNKNOWN;
  }

  return TOONZ_OK;
}

static void release_interface(void *interf) {
  if (interf) delete interf;
}

Loader::Loader() {}

void Loader::walkDirectory(const QString &path) {
  walkDirectory_(path);
  emit fixup();
}

void Loader::walkDictionary(const QString &path) {
  /* only for emitting a signal for fixup */
  printf("walkDictionary: %s [dry]\n", path.toLocal8Bit().data());
  emit fixup();
}

void Loader::walkDirectory_(const QString &path) {
  printf("walkDirectory_: %s\n", path.toLocal8Bit().data());
  QDir dir(path, QString::fromStdString("*.plugin"), QDir::Name,
           QDir::AllDirs | QDir::Files | QDir::NoDot | QDir::NoDotDot);
  auto lst = dir.entryInfoList();
  for (auto &e : lst) {
    if (e.isDir()) {
      walkDirectory_(e.filePath());
    } else if (e.isFile()) {  // file or symlink-to-file
      doLoad(e.filePath());
    }
  }
}

#if defined(_WIN32) || defined(_CYGWIN_)
static void end_library(HMODULE mod) { FreeLibrary(mod); }
#else
static void end_library(void *mod) { dlclose(mod); }
#endif

void Loader::doLoad(const QString &file) {
#if defined(_WIN32) || defined(_CYGWIN_)
  HMODULE handle = LoadLibraryA(file.toLocal8Bit().data());
  printf("doLoad handle:%p path:%s\n", handle, file.toLocal8Bit().data());
#else
  void *handle = dlopen(file.toUtf8().data(), RTLD_LOCAL);
  printf("doLoad handle:%p path:%s\n", handle, file.toUtf8().data());
#endif
  PluginInformation *pi = new PluginInformation;
  if (handle) {
    pi->library_ = library_t(handle, end_library);  // shared_ptr
                                                    /*
probe に使う plugin 情報を探す.
テーブルを export
                                                    したほうが楽だが、開発者はデバッグしにくいので関数フォームも提供する.
toonz_plugin_info で検索し、なければ toonz_plugin_probe() を呼び出す.
*/
#if defined(_WIN32) || defined(_CYGWIN_)
    auto ini = (int (*)(host_interface_t *))GetProcAddress(handle,
                                                           "toonz_plugin_init");
    auto fin = (void (*)(void))GetProcAddress(handle,
                                              "toonz_plugin_exit");  // optional
    const plugin_probe_list_t *problist =
        reinterpret_cast<const plugin_probe_list_t *>(
            GetProcAddress(handle, "toonz_plugin_info_list"));
#else
    auto ini = (int (*)(host_interface_t *))dlsym(handle, "toonz_plugin_init");
    auto fin = (void (*)(void))dlsym(handle, "toonz_plugin_exit");  // optional
    const plugin_probe_list_t *problist =
        reinterpret_cast<const plugin_probe_list_t *>(
            dlsym(handle, "toonz_plugin_info_list"));
#endif
    pi->ini_ = ini;
    pi->fin_ = fin;

    const plugin_probe_t *probinfo_begin = NULL;
    const plugin_probe_t *probinfo_end   = NULL;
    try {
      if (problist) {
        if (!is_compatible<plugin_probe_list_t, 1, 0>(*problist))
          throw std::domain_error(
              "invaid toonz_plugin_info_list: version unmatched");
        probinfo_begin = problist->begin;
        probinfo_end   = problist->end;
      }

      if (!probinfo_begin || !probinfo_end) {
        printf("use function-formed prober:toonz_plugin_probe\n");
// look at function-formed
#if defined(_WIN32) || defined(_CYGWIN_)
        void *probe = GetProcAddress(handle, "toonz_plugin_probe");
#else
        void *probe = dlsym(handle, "toonz_plugin_probe");
#endif
        if (probe) {
          printf("function-formed prober found\n");
          const plugin_probe_list_t *lst =
              (reinterpret_cast<const plugin_probe_list_t *(*)(void)>(probe))();
          if (!lst || !is_compatible<plugin_probe_list_t, 1, 0>(*lst))
            throw std::domain_error("invalid plugin list");
          plugin_probe_t *begin = lst->begin;
          plugin_probe_t *end   = lst->end;
          if (!begin || !end)
            throw std::domain_error(
                "invalid plugin information address (begin or end is null)");
          else if (begin >= end)
            throw std::domain_error(
                "invalid plugin information address (begin >= end)");
          else if (begin == end - 1)
            throw std::domain_error(
                "invalid plugin information address (information is empty)");
          probinfo_begin = begin;
          probinfo_end   = end;
        } else {
          throw std::domain_error(
              "found toonz_plugin_probe nor toonz_plugin_info");
        }
      } else {
      }
      int plugin_num = probinfo_end - probinfo_begin;
      printf("plugin count:%d begin:%p end:%p\n", plugin_num, probinfo_begin,
             probinfo_end);

      /* sanity check に失敗した場合は予期せぬアドレスを参照して toonz
         本体ごと落ちる可能性があるので
         致命的エラー扱いで早期に抜ける. */
      if (!probinfo_begin || !probinfo_end ||
          !check(probinfo_begin, probinfo_end))
        throw std::domain_error("ill-formed plugin information");

      for (const plugin_probe_t *probinfo = probinfo_begin;
           probinfo < probinfo_end; probinfo++) {
        pi->desc_                       = new PluginDescription(probinfo);
        nodal_rasterfx_handler_t *nodal = probinfo->handler;
        /* probinfo は sanity check 通過済みなのでチェック不要. handler は null
         * でないことのみが確認されている */
        if (is_compatible<nodal_rasterfx_handler_t, 1, 0>(*nodal)) {
          uint32_t c = probinfo->clss & ~(TOONZ_PLUGIN_CLASS_MODIFIER_MASK);
          uint32_t m = probinfo->clss & (TOONZ_PLUGIN_CLASS_MODIFIER_MASK);
          if (c == TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB) {
            pi->handler_ = new nodal_rasterfx_handler_t;
            if (!check_and_copy(pi->handler_, nodal))
              throw std::domain_error("ill-formed nodal interface");
          } else {
            // unknown plugin-class : gracefully end
            /* sanity check しているので来ないはずだ */
          }
        } else {
          // unknown version : gracefully end
        }

        emit load_finished(pi);

        if (pi) {
          try {
            if (pi->ini_) {
              /* interface は plugin 内部で破壊されても他に影響させないため
               * plugin instance ごとに割り当てる.  */
              host_interface_t *host  = new host_interface_t;
              host->ver.major         = 1;
              host->ver.minor         = 0;
              host->query_interface   = query_interface;
              host->release_interface = release_interface;
              int ret                 = pi->ini_(host);
              if (ret) {
                delete host;
                std::domain_error(
                    "failed initialized: error on _toonz_plugin_init");
              }
              pi->host_ = host;
              pi->decl_ = new PluginDeclaration(pi);
            } else {
              /* もっと早期にエラーを出して終了することもできるが
                 センシティブすぎると見通しが立てにくいのである程度我慢してから出す
                 */
              throw std::domain_error("not found _toonz_plugin_init");
            }
          } catch (const std::exception &e) {
            printf("Exception occured after plugin loading: %s\n", e.what());
          }

          if (pi->handler_ && pi->handler_->setup) {
            PluginSetupMessage(pi).sendBlocking();
          }

          if (probinfo + 1 < probinfo_end) {
            /* for a next plugin on the library */
            auto prev = pi->library_;
            pi        = new PluginInformation;
            /* instance に依存しない unique なリソースは引き継ぐ必要がある */
            pi->library_ = prev;
            pi->ini_     = ini;
            pi->fin_     = fin;
          }
        }
      }
    } catch (const std::exception &e) {
      printf("Exception occured while plugin loading: %s\n", e.what());
      delete pi;
      pi = NULL;
    }
  }
}

void RasterFxPluginHost::createPortsByDesc() {
  if (pi_) {
    for (auto pm : pi_->port_mapper_) {
      /* TRasterFxPort は non-copyable
       * なスマートポインタなのでポインタで引き回す */
      printf("createPortsByDesc: name:%s dir:%d type:%d\n", pm.first.c_str(),
             pm.second.input_, pm.second.type_);
      if (pm.second.input_) {
        auto p = std::make_shared<TRasterFxPort>();
        if (!addInputPort(pm.first, p)) {  // overloaded version
          printf("createPortsByDesc: failed to add: already have\n");
        }
      } else {
        auto p = new TRasterFxPort();
        /* TRasterFxPort は non-copyable
         * なスマートポインタなのでポインタで引き回す */
        if (addOutputPort(pm.first, p)) {  // overloaded version
          delete p;
          printf("createPortsByDesc: failed to add: already have\n");
        }
      }
    }
  }
}

/*
 TODO: addfxcontextmenu に移したほうがいい
 */
PluginLoadController::PluginLoadController(const std::string &basedir,
                                           QObject *listener) {
  Loader *ld = new Loader;

  ld->moveToThread(&work_entity);
  connect(&work_entity, &QThread::finished, ld, &QObject::deleteLater);
  /* AddFxContextMenu から呼ばれていたが、プラグインの検索が load_entries()
     を通じて起動時に呼ばれるようにした関係で,
     (あまりよくはないが)listner の有無によって receiver を分けるようにしている.
     listener がいる場合は従来通り context menu の構築のために
     AddFxContextMenu::fixup() に接続するが
     それ以外では plugin_dict_ への追加のため PluginLoadController::finished
     に接続する.
  */
  if (listener) {
    AddFxContextMenu *a = qobject_cast<AddFxContextMenu *>(listener);
    connect(ld, &Loader::fixup, a, &AddFxContextMenu::fixup);
    connect(this, &PluginLoadController::start, ld, &Loader::walkDictionary);
  } else {
    connect(this, &PluginLoadController::start, ld, &Loader::walkDirectory);
    connect(ld, &Loader::load_finished, this, &PluginLoadController::result);
    if (!connect(ld, &Loader::fixup, this, &PluginLoadController::finished))
      assert(false);
  }
  work_entity.start();

  QString pluginbase = (TEnv::getStuffDir() + "plugins").getQString();
  printf("plugin search directory:%s\n", pluginbase.toLocal8Bit().data());
  emit start(pluginbase);
}

void PluginLoadController::finished() {
  printf("===== PluginLoadController::finished() =====\n");
  work_entity.exit();
}

void PluginLoadController::result(PluginInformation *pi) {
  /* slot receives PluginInformation on the main thread たぶん */
  printf("PluginLoadController::result() pi:%p\n", pi);
  if (pi) {
    /* addfxcontextmenu.cpp の dict に登録する */
    plugin_dict_.insert(
        std::pair<std::string, PluginInformation *>(pi->desc_->id_, pi));
  }
}

static bool copy_rendering_setting(toonz_rendering_setting_t *dst,
                                   const TRenderSettings &src) {
  plugin::utils::copy_affine(&dst->affine, src.m_affine);
  dst->gamma                  = src.m_gamma;
  dst->time_stretch_from      = src.m_timeStretchFrom;
  dst->time_stretch_to        = src.m_timeStretchTo;
  dst->stereo_scopic_shift    = src.m_stereoscopicShift;
  dst->bpp                    = src.m_bpp;
  dst->max_tile_size          = src.m_maxTileSize;
  dst->quality                = src.m_quality;
  dst->field_prevalence       = src.m_fieldPrevalence;
  dst->stereoscopic           = src.m_stereoscopic;
  dst->is_swatch              = src.m_isSwatch;
  dst->user_cachable          = src.m_userCachable;
  dst->apply_shrink_to_viewer = src.m_applyShrinkToViewer;
  dst->context                = &src;
  dst->is_canceled            = src.m_isCanceled;
  plugin::utils::copy_rect(&dst->camera_box, src.m_cameraBox);
  return true;
}

//#include "pluginhost.moc"
