

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tfunctorinvoker.h"

// TnzBase incudes
#include "tunit.h"
#include "tparamcontainer.h"
#include "tparamset.h"
#include "tmacrofx.h"
#include "tparamchange.h"

// TnzExt includes
#include "ext/plasticskeleton.h"

// TnzLib includes
#include "toonz/tstageobjecttree.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/fxdag.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tfxhandle.h"
#include "toonz/tobjecthandle.h"

// TnzQt includes
#include "toonzqt/functionviewer.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/plasticvertexselection.h"
#include "tw/stringtable.h"

// Qt includes
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMouseEvent>
#include <QMetaObject>
#include <QColor>
#include <QApplication>  // for drag&drop
#include <QDrag>
#include <QMimeData>

#include "toonzqt/functiontreeviewer.h"

//*************************************************************************************
//    ChannelGroup specialization  definition
//*************************************************************************************

namespace {

class ParamChannelGroup final : public FunctionTreeModel::ParamWrapper,
                                public FunctionTreeModel::ChannelGroup {
public:
  ParamChannelGroup(TParam *param, const std::wstring &fxId,
                    std::string &paramName);

  void refresh() override;
  void *getInternalPointer() const override;
};

//=============================================================================

class StageObjectChannelGroup final : public FunctionTreeModel::ChannelGroup {
public:
  TStageObject *m_stageObject;  //!< (not owned) Referenced stage object
  FunctionTreeModel::ChannelGroup
      *m_plasticGroup;  //!< (not owned) Eventual plastic channels group

public:
  StageObjectChannelGroup(TStageObject *pegbar);
  ~StageObjectChannelGroup();

  QString getShortName() const override;
  QString getLongName() const override;

  QString getIdName() const override;

  void *getInternalPointer() const override {
    return static_cast<void *>(m_stageObject);
  }

  TStageObject *getStageObject() const { return m_stageObject; }
  QVariant data(int role) const override;
};

//=============================================================================

class SkVDChannelGroup final : public FunctionTreeModel::ChannelGroup {
public:
  StageObjectChannelGroup *m_stageObjectGroup;  //!< Parent stage object group
  const QString *m_vxName;                      //!< The associated vertex name

public:
  SkVDChannelGroup(const QString *vxName, StageObjectChannelGroup *stageGroup)
      : ChannelGroup(*vxName)
      , m_stageObjectGroup(stageGroup)
      , m_vxName(vxName) {}

  QString getShortName() const override {
    return m_stageObjectGroup->getShortName();
  }
  QString getLongName() const override { return *m_vxName; }

  void *getInternalPointer() const override { return (void *)m_vxName; }

  static inline bool compareStr(const TreeModel::Item *item,
                                const QString &str) {
    const QString &thisStr =
        static_cast<const SkVDChannelGroup *>(item)->getLongName();
    return (QString::localeAwareCompare(thisStr, str) < 0);
  }

  QVariant data(int role) const override;
};

}  // namespace

//=============================================================================
//
// ChannelGroup
//
//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup::ChannelGroup(const QString &name)
    : m_name(name), m_showFilter(ShowAllChannels) {}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup::~ChannelGroup() {}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::ChannelGroup::isActive() const {
  // Analyze children. If one is active, this is active too.
  int c, childCount = getChildCount();
  for (c = 0; c != childCount; ++c)
    if (static_cast<Item *>(getChild(c))->isActive()) return true;

  return false;
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::ChannelGroup::isAnimated() const {
  // Same for the animated feature, this is animate if any of its children is.
  int c, childCount = getChildCount();
  for (c = 0; c != childCount; ++c)
    if (static_cast<Item *>(getChild(c))->isAnimated()) return true;

  return false;
}

//-----------------------------------------------------------------------------

QVariant FunctionTreeModel::ChannelGroup::data(int role) const {
  if (role == Qt::DisplayRole)
    return getLongName();
  else if (role == Qt::DecorationRole) {
    bool animated = isAnimated();
    bool active   = isActive();

    if (active) {
      static QIcon folderAnimOpen(":Resources/folderanim_open.svg");
      static QIcon folderAnimClose(":Resources/folderanim_close.svg");
      static QIcon folderOpen(":Resources/folder_open.svg");
      static QIcon folderClose(":Resources/folder_close.svg");

      return animated ? isOpen() ? folderAnimOpen : folderAnimClose
                      : isOpen() ? folderOpen : folderClose;
    } else {
      static QIcon folderAnimOpen(":Resources/folderanim_open_off.svg");
      static QIcon folderAnimClose(":Resources/folderanim_close_off.svg");
      static QIcon folderOpen(":Resources/folder_open_off.svg");
      static QIcon folderClose(":Resources/folder_close_off.svg");

      return animated ? isOpen() ? folderAnimOpen : folderAnimClose
                      : isOpen() ? folderOpen : folderClose;
    }
  } else
    return Item::data(role);
}

//-----------------------------------------------------------------------------

//! \todo     This is \a not recursive - I guess it should be...?
void FunctionTreeModel::ChannelGroup::applyShowFilter() {
  int i, itemCount = getChildCount();
  for (i = 0; i < itemCount; i++) {
    FunctionTreeModel::Channel *channel =
        dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
    /*--- ChannelGroupの内部も同じフィルタで更新する ---*/
    if (!channel) {
      FunctionTreeModel::ChannelGroup *channelGroup =
          dynamic_cast<FunctionTreeModel::ChannelGroup *>(getChild(i));
      if (!channelGroup) continue;

      channelGroup->setShowFilter(m_showFilter);
      continue;
    }

    bool showItem = (m_showFilter == ShowAllChannels) ||
                    channel->getParam()->hasKeyframes();

    QModelIndex modelIndex = createIndex();
    getModel()->setRowHidden(i, modelIndex, !showItem);

    if (!showItem) channel->setIsActive(false);
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::setShowFilter(ShowFilter showFilter) {
  m_showFilter = showFilter;
  applyShowFilter();
}

//-----------------------------------------------------------------------------

QString FunctionTreeModel::ChannelGroup::getIdName() const {
  QString tmpName = QString(m_name);
  tmpName.remove(QChar(' '), Qt::CaseInsensitive);
  tmpName = tmpName.toLower();

  FunctionTreeModel::ChannelGroup *parentGroup =
      dynamic_cast<FunctionTreeModel::ChannelGroup *>(getParent());
  if (parentGroup) {
    return parentGroup->getIdName() + QString(".") + tmpName;
  }
  return tmpName;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::setChildrenAllActive(bool active) {
  for (int i = 0; i < getChildCount(); i++) {
    // for Channel
    FunctionTreeModel::Channel *channel =
        dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
    if (channel) {
      channel->setIsActive(active);
      continue;
    }
    // for ChannelGroup
    else {
      FunctionTreeModel::ChannelGroup *channelGroup =
          dynamic_cast<FunctionTreeModel::ChannelGroup *>(getChild(i));
      if (channelGroup) {
        channelGroup->setChildrenAllActive(active);
        continue;
      }
    }
  }
}

//=============================================================================
//
// StageObjectChannelGroup
//
//-----------------------------------------------------------------------------

StageObjectChannelGroup::StageObjectChannelGroup(TStageObject *stageObject)
    : m_stageObject(stageObject), m_plasticGroup() {
  m_stageObject->addRef();
}

//-----------------------------------------------------------------------------

StageObjectChannelGroup::~StageObjectChannelGroup() {
  m_stageObject->release();
}

//-----------------------------------------------------------------------------

QVariant StageObjectChannelGroup::data(int role) const {
  if (role == Qt::DisplayRole) {
    std::string name = m_stageObject->getName();
    std::string id   = m_stageObject->getId().toString();

    return (name == id) ? QString::fromStdString(name)
                        : QString::fromStdString(id + " (" + name + ")");

  } else if (role == Qt::ForegroundRole) {
    FunctionTreeModel *model = dynamic_cast<FunctionTreeModel *>(getModel());
    if (!model)
#if QT_VERSION >= 0x050000
      return QColor(Qt::black);
#else
      return Qt::black;
#endif
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(model->getView());
    if (!view || !model->getCurrentStageObject())
#if QT_VERSION >= 0x050000
      return QColor(Qt::black);
#else
      return Qt::black;
#endif
    TStageObjectId currentId = model->getCurrentStageObject()->getId();
    return m_stageObject->getId() == currentId ? view->getCurrentTextColor()
                                               : view->getTextColor();
  } else
    return ChannelGroup::data(role);
}

//-----------------------------------------------------------------------------

QString StageObjectChannelGroup::getShortName() const {
  return QString::fromStdString(m_stageObject->getName());
}

//-----------------------------------------------------------------------------

QString StageObjectChannelGroup::getLongName() const {
  return QString::fromStdString(m_stageObject->getFullName());
}

//-----------------------------------------------------------------------------

QString StageObjectChannelGroup::getIdName() const {
  return QString::fromStdString(m_stageObject->getId().toString()).toLower();
}

//=============================================================================
//
// FxChannelGroup
//
//-----------------------------------------------------------------------------

FxChannelGroup::FxChannelGroup(TFx *fx) : m_fx(fx) {
  if (m_fx) m_fx->addRef();
}

//-----------------------------------------------------------------------------

FxChannelGroup::~FxChannelGroup() {
  if (m_fx) m_fx->release();
  m_fx = 0;
}

//-----------------------------------------------------------------------------

QString FxChannelGroup::getShortName() const {
  return QString::fromStdWString(m_fx->getFxId());
}

//-----------------------------------------------------------------------------

QString FxChannelGroup::getLongName() const {
  return QString::fromStdWString(m_fx->getFxId());
}

//-----------------------------------------------------------------------------

QVariant FxChannelGroup::data(int role) const {
  if (role == Qt::DecorationRole) {
    bool isAnimated                 = false;
    TParamContainer *paramContainer = m_fx->getParams();
    int i;
    for (i = 0; i < paramContainer->getParamCount(); i++) {
      if (!paramContainer->getParam(i)->hasKeyframes()) continue;
      isAnimated = true;
      break;
    }
    bool isOneChildActive = false;
    for (i = 0; i < getChildCount(); i++) {
      FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
      if (!channel || !channel->isActive()) continue;
      isOneChildActive = true;
      break;
    }
    if (isOneChildActive) {
      static QIcon folderAnimOpen(":Resources/folderanim_open.svg");
      static QIcon folderAnimClose(":Resources/folderanim_close.svg");
      static QIcon folderOpen(":Resources/folder_open.svg");
      static QIcon folderClose(":Resources/folder_close.svg");

      return isAnimated ? isOpen() ? folderAnimOpen : folderAnimClose
                        : isOpen() ? folderOpen : folderClose;
    } else {
      static QIcon folderAnimOpen(":Resources/folderanim_open_off.svg");
      static QIcon folderAnimClose(":Resources/folderanim_close_off.svg");
      static QIcon folderOpen(":Resources/folder_open_off.svg");
      static QIcon folderClose(":Resources/folder_close_off.svg");

      return isAnimated ? isOpen() ? folderAnimOpen : folderAnimClose
                        : isOpen() ? folderOpen : folderClose;
    }
  } else if (role == Qt::DisplayRole) {
    std::wstring name = m_fx->getName();
    std::wstring id   = m_fx->getFxId();
    if (name == id)
      return QString::fromStdWString(name);
    else
      return QString::fromStdWString(id + L" (" + name + L")");
  } else if (role == Qt::ForegroundRole) {
    FunctionTreeModel *model = dynamic_cast<FunctionTreeModel *>(getModel());
    if (!model)
#if QT_VERSION >= 0x050000
      return QColor(Qt::black);
#else
      return Qt::black;
#endif
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(model->getView());
    if (!view)
#if QT_VERSION >= 0x050000
      return QColor(Qt::black);
#else
      return Qt::black;
#endif
    TFx *currentFx = model->getCurrentFx();
    return m_fx == currentFx ? view->getCurrentTextColor()
                             : view->getTextColor();
  } else
    return Item::data(role);
}

//-----------------------------------------------------------------------------

QString FxChannelGroup::getIdName() const {
  return QString::fromStdWString(m_fx->getFxId()).toLower();
}

//-----------------------------------------------------------------------------

void FxChannelGroup::refresh() {
  TMacroFx *macroFx = dynamic_cast<TMacroFx *>(m_fx);

  int i, childrenCount = getChildCount();
  for (i = 0; i < childrenCount; ++i) {
    FunctionTreeModel::ParamWrapper *wrap =
        dynamic_cast<FunctionTreeModel::ParamWrapper *>(getChild(i));
    assert(wrap);

    TParam *param = 0;
    {
      TParamContainer *paramContainer = 0;
      if (macroFx) {
        const std::wstring &fxId = wrap->getFxId();
        TFx *subFx               = macroFx->getFxById(fxId);
        if (!subFx) continue;

        paramContainer = subFx->getParams();
      } else
        paramContainer = m_fx->getParams();

      param = paramContainer->getParam(wrap->getParam()->getName());
    }

    assert(param);
    wrap->setParam(param);

    ParamChannelGroup *paramGroup = dynamic_cast<ParamChannelGroup *>(wrap);
    if (paramGroup) paramGroup->refresh();
  }
}

//=============================================================================
//
// ParamChannelGroup
//
//-----------------------------------------------------------------------------

ParamChannelGroup::ParamChannelGroup(TParam *param, const std::wstring &fxId,
                                     std::string &paramName)
    : ParamWrapper(param, fxId)
    , ChannelGroup(
          param->hasUILabel()
              ? QString::fromStdString(param->getUILabel())
              : QString::fromStdWString(TStringTable::translate(paramName))) {}

//-----------------------------------------------------------------------------

void *ParamChannelGroup::getInternalPointer() const {
  return this->m_param.getPointer();
}

//-----------------------------------------------------------------------------

void ParamChannelGroup::refresh() {
  TParamSet *paramSet = dynamic_cast<TParamSet *>(m_param.getPointer());
  if (!paramSet) return;

  int c, childrenCount = getChildCount();
  for (c = 0; c < childrenCount; ++c) {
    FunctionTreeModel::ParamWrapper *wrap =
        dynamic_cast<FunctionTreeModel::ParamWrapper *>(getChild(c));
    assert(wrap);

    TParamP currentParam = wrap->getParam();
    assert(currentParam);

    int p = paramSet->getParamIdx(wrap->getParam()->getName());
    assert(p < paramSet->getParamCount());

    TParamP param = paramSet->getParam(p);
    wrap->setParam(param);
  }
}

//=============================================================================
//
// SkVDChannelGroup
//
//-----------------------------------------------------------------------------

QVariant SkVDChannelGroup::data(int role) const {
  if (role == Qt::ForegroundRole) {
    // Check whether current selection is a PlasticVertex one - in case, paint
    // it red
    // if this group refers to current vertex

    if (PlasticVertexSelection *vxSel =
            dynamic_cast<PlasticVertexSelection *>(TSelection::getCurrent()))
      if (TStageObject *obj = static_cast<FunctionTreeModel *>(getModel())
                                  ->getCurrentStageObject())
        if (obj == m_stageObjectGroup->m_stageObject)
          if (const SkDP &sd = obj->getPlasticSkeletonDeformation()) {
            int vIdx = *vxSel;

            if (vIdx >= 0 &&
                sd->skeleton(vxSel->skeletonId())->vertex(vIdx).name() ==
                    getLongName())
#if QT_VERSION >= 0x050000
              return QColor(Qt::red);
#else
              return Qt::red;
#endif
          }

#if QT_VERSION >= 0x050000
    return QColor(Qt::black);
#else
    return Qt::black;
#endif
  } else
    return ChannelGroup::data(role);
}

//=============================================================================
//
// Channel
//
//-----------------------------------------------------------------------------

FunctionTreeModel::Channel::Channel(FunctionTreeModel *model,
                                    TDoubleParam *param,
                                    std::string paramNamePref,
                                    std::wstring fxId)
    : ParamWrapper(param, fxId)
    , m_model(model)
    , m_group(0)
    , m_isActive(false)
    , m_paramNamePref(paramNamePref) {}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel::~Channel() {
  m_model->onChannelDestroyed(this);
  if (m_isActive) getParam()->removeObserver(this);
}

//-----------------------------------------------------------------------------

void *FunctionTreeModel::Channel::getInternalPointer() const {
  return this->m_param.getPointer();
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isAnimated() const {
  return m_param->hasKeyframes();
}

//-----------------------------------------------------------------------------

QVariant FunctionTreeModel::Channel::data(int role) const {
  if (role == Qt::DecorationRole) {
    static QIcon paramAnimOn(":Resources/paramanim_on.svg");
    static QIcon paramAnimOff(":Resources/paramanim_off.svg");
    static QIcon paramOn(":Resources/param_on.svg");
    static QIcon paramOff(":Resources/param_off.svg");

    return m_param->hasKeyframes() ? isActive() ? paramAnimOn : paramAnimOff
                                   : isActive() ? paramOn : paramOff;
  } else if (role == Qt::DisplayRole) {
    if (m_param->hasUILabel()) {
      return QString::fromStdString(m_param->getUILabel());
    }
    std::string name            = m_paramNamePref + m_param->getName();
    std::wstring translatedName = TStringTable::translate(name);
    if (m_fxId.size() > 0)
      return QString::fromStdWString(translatedName + L" (" + m_fxId + L")");
    return QString::fromStdWString(translatedName);
  } else if (role == Qt::ForegroundRole) {
    // 130221 iwasawa
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(m_model->m_view);
    if (!view)
#if QT_VERSION >= 0x050000
      return QColor(Qt::black);
#else
      return Qt::black;
#endif
    return (isCurrent()) ? view->getCurrentTextColor() : view->getTextColor();
  } else
    return TreeModel::Item::data(role);
}

//-----------------------------------------------------------------------------

QString FunctionTreeModel::Channel::getShortName() const {
  if (m_param->hasUILabel()) {
    return QString::fromStdString(m_param->getUILabel());
  }
  std::string name            = m_paramNamePref + m_param->getName();
  std::wstring translatedName = TStringTable::translate(name);
  return QString::fromStdWString(translatedName);
}

//-----------------------------------------------------------------------------

QString FunctionTreeModel::Channel::getLongName() const {
  QString name                = getShortName();
  if (getChannelGroup()) name = getChannelGroup()->getLongName() + " " + name;
  return name;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::setParam(const TParamP &param) {
  if (param.getPointer() == m_param.getPointer()) return;

  TParamP oldParam = m_param;
  m_param          = param;

  if (m_isActive) {
    oldParam->removeObserver(this);
    param->addObserver(this);
  }
}

//-----------------------------------------------------------------------------
/*! in order to show the expression name in the tooltip
*/
QString FunctionTreeModel::Channel::getExprRefName() const {
  QString tmpName = QString(QString::fromStdWString(
      TStringTable::translate(m_paramNamePref + m_param->getName())));
  /*--- stage
   * objectパラメータの場合、TableにあわせてtmpNameを代表的なExpression名にする---*/
  StageObjectChannelGroup *stageGroup =
      dynamic_cast<StageObjectChannelGroup *>(m_group);
  if (stageGroup) {
    if (tmpName == "N/S")
      tmpName = "ns";
    else if (tmpName == "E/W")
      tmpName = "ew";
    else if (tmpName == "Z")
      tmpName = "z";
    else if (tmpName == "Rotation")
      tmpName = "rot";
    else if (tmpName == "Scale H")
      tmpName = "sx";
    else if (tmpName == "Scale V")
      tmpName = "sy";
    else if (tmpName == "Shear H")
      tmpName = "shx";
    else if (tmpName == "Shear V")
      tmpName = "shy";
    else if (tmpName == "posPath")
      tmpName = "path";
    else if (tmpName == "Scale")
      tmpName = "sc";
    else
      tmpName = tmpName.toLower();

    return stageGroup->getIdName() + QString(".") + tmpName;
  }

  /*--- Fxパラメータの場合 ---*/
  tmpName.remove(QChar(' '), Qt::CaseInsensitive);
  tmpName.remove(QChar('/'));
  tmpName = tmpName.toLower();

  FunctionTreeModel::ChannelGroup *parentGroup =
      dynamic_cast<FunctionTreeModel::ChannelGroup *>(getParent());
  if (parentGroup) {
    return QString("fx.") + parentGroup->getIdName() + QString(".") + tmpName;
  } else
    return "";
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::setIsActive(bool active) {
  if (active == m_isActive) return;

  m_isActive = active;
  m_model->refreshActiveChannels();
  if (m_isActive) {
    getParam()->addObserver(this);
    /*--- これが最初にVisibleにしたChannelの場合 ---*/
    if (!m_model->m_currentChannel) {
      setIsCurrent(true);
      m_model->emitCurveSelected(getParam());
    }
  } else {
    getParam()->removeObserver(this);
    if (isCurrent()) {
      setIsCurrent(false);
      m_model->emitCurveSelected(0);
    }
  }

  m_model->emitDataChanged(this);
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isCurrent() const {
  return m_model->m_currentChannel == this;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::setIsCurrent(bool current) {
  Channel *oldCurrent = m_model->m_currentChannel;
  if (current) {
    // this channel must become the current
    if (oldCurrent == this) return;  // already it is: nothing to do
    m_model->m_currentChannel = this;

    // change the current fx if the FxChannelGroup is clicked
    FxChannelGroup *fxGroup = dynamic_cast<FxChannelGroup *>(m_group);
    if (fxGroup && m_model->getFxHandle()) {
      m_model->getFxHandle()->setFx(fxGroup->getFx());
    }
    // or, change the current object if the stageObjectChannelGroup is clicked
    else {
      StageObjectChannelGroup *stageObjectGroup =
          dynamic_cast<StageObjectChannelGroup *>(m_group);
      if (stageObjectGroup && m_model->getObjectHandle()) {
        m_model->getObjectHandle()->setObjectId(
            stageObjectGroup->getStageObject()->getId());
      }
    }

    // the current channel must be active
    if (!m_isActive) {
      m_isActive = true;
      m_model->refreshActiveChannels();
      getParam()->addObserver(this);
    }
    // refresh the old current (if !=0) and the new one
    if (oldCurrent) m_model->emitDataChanged(oldCurrent);
    m_model->emitDataChanged(this);
    m_model->emitCurveSelected(getParam());
    // scroll the column to ensure visible the current channel
    m_model->emitCurrentChannelChanged(this);
  } else {
    // this channel is not the current anymore
    if (oldCurrent != this) return;  // it was not: nothing to do
    m_model->m_currentChannel = 0;
    // refresh the channel
    m_model->emitDataChanged(this);
  }
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isHidden() const {
  return getChannelGroup()->getShowFilter() ==
             ChannelGroup::ShowAnimatedChannels &&
         !getParam()->hasKeyframes();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::onChange(const TParamChange &ch) {
  m_model->onChange(ch);
}

//=============================================================================
//
// FunctionTreeModel
//
//-----------------------------------------------------------------------------

FunctionTreeModel::FunctionTreeModel(FunctionTreeView *parent)
    : TreeModel(parent)
    , m_currentChannel(0)
    , m_stageObjects(0)
    , m_fxs(0)
    , m_currentStageObject(0)
    , m_currentFx(0)
    , m_paramsChanged(false) {}

//-----------------------------------------------------------------------------

FunctionTreeModel::~FunctionTreeModel() {
  // I must delete items here (not in TreeModel::~TreeModel()).
  // Channel::~Channel() refers to the model, which should be alive.
  setRootItem(0);
  if (m_currentFx) m_currentFx->release();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshData(TXsheet *xsh) {
  m_activeChannels.clear();
  Channel *currentChannel = m_currentChannel;

  beginRefresh();
  {
    if (!getRootItem()) {
      setRootItem(new ChannelGroup("Root"));

      if (xsh) {
        getRootItem()->appendChild(m_stageObjects = new ChannelGroup("Stage"));
        getRootItem()->appendChild(m_fxs = new ChannelGroup("FX"));

        assert(getRootItem()->getChildCount() == 2);
        assert(getRootItem()->getChild(0) == m_stageObjects);
        assert(getRootItem()->getChild(1) == m_fxs);
      }
    }

    if (xsh) {
      refreshStageObjects(xsh);
      refreshFxs(xsh);
    }

    refreshActiveChannels();
  }
  endRefresh();

  if (m_currentChannel != currentChannel) emit curveSelected(0);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshStageObjects(TXsheet *xsh) {
  static const int channelIds[TStageObject::T_ChannelCount] = {
      TStageObject::T_X,      TStageObject::T_Y,      TStageObject::T_Z,
      TStageObject::T_SO,     TStageObject::T_Path,   TStageObject::T_Angle,
      TStageObject::T_ScaleX, TStageObject::T_ScaleY, TStageObject::T_Scale,
      TStageObject::T_ShearX, TStageObject::T_ShearY};  // Explicitly ordered
                                                        // channels

  // Retrieve all (not-empty) root stage objects, and add them in the tree model
  QList<TreeModel::Item *> newItems;
  TStageObjectTree *ptree = xsh->getStageObjectTree();

  int i, iCount = ptree->getStageObjectCount();
  for (i = 0; i < iCount; ++i) {
    TStageObject *pegbar = ptree->getStageObject(i);
    TStageObjectId id    = pegbar->getId();
    if (id.isColumn() && xsh->isColumnEmpty(id.getIndex())) continue;

    newItems.push_back(new StageObjectChannelGroup(pegbar));
  }

  /*--- newItemsの中で、これまでのChildrenに無いものだけ
  m_stageObjectsの子に追加。既に有るものはnewChildrenから除外---*/
  m_stageObjects->setChildren(newItems);

  // Add channels to the NEW stage entries (see the above call to setChildren())
  iCount = newItems.size();
  for (i = 0; i < iCount; ++i) {
    StageObjectChannelGroup *pegbarItem =
        dynamic_cast<StageObjectChannelGroup *>(newItems[i]);

    TStageObject *stageObject = pegbarItem->getStageObject();

    // Add the standard stage object channels
    int j, jCount = TStageObject::T_ChannelCount;
    // making each channel of pegbar
    for (j = 0; j < jCount; ++j) {
      TDoubleParam *param =
          stageObject->getParam((TStageObject::Channel)channelIds[j]);
      Channel *channel = new Channel(this, param);

      pegbarItem->appendChild(channel);
      channel->setChannelGroup(pegbarItem);
    }

    pegbarItem->applyShowFilter();
  }

  // As plastic deformations are stored in stage objects, refresh them if
  // necessary
  refreshPlasticDeformations();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshFxs(TXsheet *xsh) {
  std::vector<TFx *> fxs;
  xsh->getFxDag()->getFxs(fxs);
  for (int i = 0; i < xsh->getColumnCount(); i++) {
    TXshZeraryFxColumn *zc =
        dynamic_cast<TXshZeraryFxColumn *>(xsh->getColumn(i));
    if (!zc) continue;
    fxs.push_back(zc->getZeraryColumnFx()->getZeraryFx());
  }

  // sort items by fxId
  for (int j = 1; j < (int)fxs.size(); j++) {
    int index = j;
    while (index > 0 &&
           QString::localeAwareCompare(
               QString::fromStdWString(fxs[index - 1]->getFxId()),
               QString::fromStdWString(fxs[index]->getFxId())) > 0) {
      std::swap(fxs[index - 1], fxs[index]);
      index = index - 1;
    }
  }

  QList<TreeModel::Item *> newItems;
  int i;
  for (i = 0; i < (int)fxs.size(); i++) {
    TFx *fx = fxs[i];
    if (!fx) continue;
    TParamContainer *params = fx->getParams();
    bool hasChannel         = false;
    int j;
    for (j = 0; j < params->getParamCount(); j++)
      if (0 != dynamic_cast<TDoubleParam *>(params->getParam(j)) ||
          0 != dynamic_cast<TPointParam *>(params->getParam(j)) ||
          0 != dynamic_cast<TRangeParam *>(params->getParam(j)) ||
          0 != dynamic_cast<TPixelParam *>(params->getParam(j))) {
        hasChannel = true;
        break;
      }
    if (hasChannel) newItems.push_back(new FxChannelGroup(fxs[i]));
  }
  m_fxs->setChildren(newItems);
  // Add channels to new fxs (only for those actually added: see setChildren())

  for (i = 0; i < (int)newItems.size(); i++) {
    TreeModel::Item *item  = newItems[i];
    FxChannelGroup *fxItem = dynamic_cast<FxChannelGroup *>(item);
    assert(fxItem);
    if (!fxItem) continue;
    TFx *fx = fxItem->getFx();
    assert(fx);
    if (!fx) continue;
    TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx);
    if (macroFx) {
      const std::vector<TFxP> &macroFxs = macroFx->getFxs();
      int j;
      for (j = 0; j < (int)macroFxs.size(); j++) {
        TParamContainer *params = macroFxs[j]->getParams();
        addChannels(macroFxs[j].getPointer(), fxItem, params);
      }
    } else {
      TParamContainer *params = fx->getParams();
      addChannels(fx, fxItem, params);
    }
    fxItem->applyShowFilter();
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshPlasticDeformations() {
  // Refresh ALL stage object items (including OLD ones)
  int i, iCount = m_stageObjects->getChildCount();
  for (i = 0; i < iCount; ++i) {
    // Add the eventual Plastic channels group
    StageObjectChannelGroup *stageItem =
        static_cast<StageObjectChannelGroup *>(m_stageObjects->getChild(i));

    TStageObject *stageObject = stageItem->getStageObject();

    const PlasticSkeletonDeformationP &sd =
        stageObject->getPlasticSkeletonDeformation();
    FunctionTreeModel::ChannelGroup *&plasticGroup = stageItem->m_plasticGroup;

    if (sd || plasticGroup) {
      if (!plasticGroup) {
        // Add a group
        plasticGroup = new ChannelGroup("Plastic Skeleton");
        stageItem->appendChild(plasticGroup);
      }

      // Prepare each vertex deformation
      QList<TreeModel::Item *> plasticItems;

      if (sd) {
        SkD::vd_iterator vdt, vdEnd;
        sd->vertexDeformations(vdt, vdEnd);

        for (; vdt != vdEnd; ++vdt) {
          const QString *str = (*vdt).first;

          QList<TreeModel::Item *>::iterator it =
              std::lower_bound(plasticItems.begin(), plasticItems.end(), *str,
                               SkVDChannelGroup::compareStr);

          plasticItems.insert(it, new SkVDChannelGroup(str, stageItem));
        }

        // Add the channel corresponding to the skeleton id
        {
          Channel *skelIdsChannel =
              new Channel(this, sd->skeletonIdsParam().getPointer());

          plasticItems.insert(plasticItems.begin(), skelIdsChannel);
          skelIdsChannel->setChannelGroup(plasticGroup);
        }
      }

      if (plasticItems.empty()) plasticGroup->setIsOpen(false);

      // Add the vertex deformations group (this needs to be done BEFORE adding
      // the actual
      // channels, seemingly due to a limitation in the TreeModel
      // implementation)
      plasticGroup->setChildren(plasticItems);

      int pi,
          piCount =
              plasticItems.size();  // NOTE: plasticItems now stores only PART
      for (pi = 0; pi != piCount;
           ++pi)  // of the specified items - those considered
      {           // 'new' by internal pointer comparison...
        SkVDChannelGroup *vxGroup =
            dynamic_cast<SkVDChannelGroup *>(plasticItems[pi]);
        if (!vxGroup) continue;

        SkVD *skvd =
            sd->vertexDeformation(vxGroup->ChannelGroup::getShortName());

        for (int k = 0; k < SkVD::PARAMS_COUNT; ++k) {
          // Add channel in the vertex deformation
          Channel *channel = new Channel(this, skvd->m_params[k].getPointer());
          channel->setChannelGroup(vxGroup);

          vxGroup->appendChild(channel);
        }

        vxGroup->applyShowFilter();
      }

      plasticGroup->applyShowFilter();
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addParameter(ChannelGroup *group,
                                     const std::string &prefixString,
                                     const std::wstring &fxId, TParam *param) {
  if (TDoubleParam *dp = dynamic_cast<TDoubleParam *>(param)) {
    Channel *channel = new Channel(this, dp, prefixString, fxId);

    group->appendChild(channel);
    channel->setChannelGroup(group);
  } else if (dynamic_cast<TPointParam *>(param) ||
             dynamic_cast<TRangeParam *>(param) ||
             dynamic_cast<TPixelParam *>(param)) {
    TParamSet *paramSet = dynamic_cast<TParamSet *>(param);
    assert(paramSet);

    std::string paramName = prefixString + param->getName();

    ChannelGroup *paramChannel = new ParamChannelGroup(param, fxId, paramName);
    group->appendChild(paramChannel);

    TPixelParam *pixParam = dynamic_cast<TPixelParam *>(param);

    int p, paramCount = paramSet->getParamCount();
    for (p = 0; p != paramCount; ++p) {
      TDoubleParam *dp =
          dynamic_cast<TDoubleParam *>(paramSet->getParam(p).getPointer());
      if (!dp ||
          (pixParam && !pixParam->isMatteEnabled() && p == paramCount - 1))
        continue;

      Channel *channel = new Channel(this, dp, "", fxId);

      paramChannel->appendChild(channel);
      channel->setChannelGroup(group);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addChannels(TFx *fx, ChannelGroup *groupItem,
                                    TParamContainer *params) {
  FxChannelGroup *fxItem = static_cast<FxChannelGroup *>(groupItem);

  std::wstring fxId = L"";
  TMacroFx *macro   = dynamic_cast<TMacroFx *>(fxItem->getFx());
  if (macro) fxId   = fx->getFxId();

  const std::string &paramNamePref = fx->getFxType() + ".";

  int p, pCount = params->getParamCount();
  for (p = 0; p != pCount; ++p)
    addParameter(fxItem, paramNamePref, fxId, params->getParam(p));
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addActiveChannels(TreeModel::Item *item) {
  assert(item);

  if (Channel *channel = dynamic_cast<Channel *>(item)) {
    if (channel->isActive()) m_activeChannels.push_back(channel);
  } else
    for (int i = 0; i < item->getChildCount(); i++)
      addActiveChannels(item->getChild(i));
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshActiveChannels() {
  m_activeChannels.clear();

  if (m_stageObjects) addActiveChannels(m_stageObjects);

  if (m_fxs) addActiveChannels(m_fxs);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::onChannelDestroyed(Channel *channel) {
  if (channel == m_currentChannel) m_currentChannel = 0;
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel *FunctionTreeModel::getActiveChannel(
    int index) const {
  if (index < 0 || index >= (int)m_activeChannels.size())
    return 0;
  else
    return m_activeChannels[index];
}

//-----------------------------------------------------------------------------

int FunctionTreeModel::getColumnIndexByCurve(TDoubleParam *param) const {
  for (int i = 0; i < (int)m_activeChannels.size(); i++) {
    if (m_activeChannels[i]->getParam() == param) return i;
  }
  return -1;
}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup *FunctionTreeModel::getStageObjectChannel(
    int index) const {
  if (index < 0 || index >= (int)m_stageObjects->getChildCount())
    return 0;
  else
    return dynamic_cast<FunctionTreeModel::ChannelGroup *>(
        m_stageObjects->getChild(index));
}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup *FunctionTreeModel::getFxChannel(
    int index) const {
  if (index < 0 || index >= (int)m_fxs->getChildCount())
    return 0;
  else
    return dynamic_cast<FunctionTreeModel::ChannelGroup *>(
        m_fxs->getChild(index));
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::onChange(const TParamChange &tpc) {
  if (!m_paramsChanged) {
    m_paramsChanged = true;

    struct Func final : public TFunctorInvoker::BaseFunctor {
      FunctionTreeModel *m_obj;
      // Use a copy of 'TParamChange' since callers declare
      // and free this value on the stack,
      // so we can't ensure its valid later on when the notifier executes.
      const TParamChange m_tpc;

      Func(FunctionTreeModel *obj, const TParamChange *tpc)
          : m_obj(obj), m_tpc(*tpc) {}
      void operator()() override { m_obj->onParamChange(m_tpc.m_dragging); }
    };

    QMetaObject::invokeMethod(TFunctorInvoker::instance(), "invoke",
                              Qt::QueuedConnection,
                              Q_ARG(void *, new Func(this, &tpc)));
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::onParamChange(bool isDragging) {
  m_paramsChanged = false;

  emit curveChanged(isDragging);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::resetAll() {
#if QT_VERSION >= 0x050000
  beginResetModel();
#endif
  m_activeChannels.clear();

  TreeModel::Item *root_item = getRootItem();
  setRootItem_NoFree(NULL);

  m_stageObjects = 0;
  m_fxs          = 0;
#if QT_VERSION < 0x050000
  reset();
#endif

  beginRefresh();
  refreshActiveChannels();
  endRefresh();

  // postpone until after refresh,
  // since its members are used for reference.
  delete root_item;

  m_currentChannel = 0;

#if QT_VERSION >= 0x050000
  endResetModel();
#endif
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setCurrentFx(TFx *fx) {
  TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx);
  if (zcfx) fx          = zcfx->getZeraryFx();
  if (fx != m_currentFx) {
    if (fx) fx->addRef();
    if (m_currentFx) m_currentFx->release();
    m_currentFx = fx;
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::applyShowFilters() {
  // WARNING: This is implemented BAD - notice that the get*() functions below
  //          DO NOT ACTUALLY RETURN CHANNELS, but rather the child
  //          ChannelGROUPS!
  //
  //          This means that these show filters are presumably applied only to
  //          the FIRST LEVEL OF PARAMETERS...!

  if (m_stageObjects) {
    int so, soCount = m_stageObjects->getChildCount();
    for (so = 0; so != soCount; ++so)
      getStageObjectChannel(so)->applyShowFilter();
  }

  if (m_fxs) {
    int fx, fxCount = m_fxs->getChildCount();
    for (fx = 0; fx != fxCount; ++fx) getFxChannel(fx)->applyShowFilter();
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addParameter(TParam *parameter,
                                     const TFilePath &folder) {
  struct locals {
    static void locateExistingRoot(ChannelGroup *&root, TFilePath &fp) {
      std::wstring firstName;
      TFilePath tempFp;

      while (!fp.isEmpty()) {
        // Get the path's first name
        fp.split(firstName, tempFp);

        // Search a matching channel group in root's children
        int c, cCount = root->getChildCount();
        for (c = 0; c != cCount; ++c) {
          if (ChannelGroup *group =
                  dynamic_cast<ChannelGroup *>(root->getChild(c))) {
            if (group->getShortName().toStdWString() == firstName) {
              root = group, fp = tempFp;
              break;
            }
          }
        }

        if (c == cCount) break;
      }
    }

    static void addFolders(ChannelGroup *&group, TFilePath &fp) {
      std::wstring firstName;
      TFilePath tempFp;

      while (!fp.isEmpty()) {
        fp.split(firstName, tempFp);

        ChannelGroup *newGroup =
            new ChannelGroup(QString::fromStdWString(firstName));
        group->appendChild(newGroup);

        group = newGroup, fp = tempFp;
      }
    }
  };  // locals

  // Search for the furthest existing channel group chain leading to our folder
  ChannelGroup *group = static_cast<ChannelGroup *>(getRootItem());
  assert(group);

  TFilePath path = folder;
  locals::locateExistingRoot(group, path);

  // If the chain interrupts prematurely, create new groups up to the required
  // folder
  if (!path.isEmpty()) locals::addFolders(group, path);

  assert(path.isEmpty());

  // Add the parameter to the last group
  addParameter(group, "", L"", parameter);
}

//=============================================================================
//
// FunctionTreeView
//
//-----------------------------------------------------------------------------

FunctionTreeView::FunctionTreeView(FunctionViewer *parent)
    : TreeView(parent), m_scenePath(), m_clickedItem(0), m_draggingChannel(0) {
  assert(parent);

  setModel(new FunctionTreeModel(this));

  setObjectName("FunctionEditorTree");
  setSelectionMode(QAbstractItemView::NoSelection);

  connect(this, SIGNAL(pressed(const QModelIndex &)), this,
          SLOT(onActivated(const QModelIndex &)));

  setFocusPolicy(Qt::NoFocus);
  setIndentation(14);
  setAlternatingRowColors(true);
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onActivated(const QModelIndex &index) {
  enum {
    NO_CHANNELS       = 0x0,
    ACTIVE_CHANNELS   = 0x1,
    INACTIVE_CHANNELS = 0x2,
    HAS_CHANNELS      = ACTIVE_CHANNELS | INACTIVE_CHANNELS
  };

  if (!index.isValid()) return;

  FunctionTreeModel *ftModel = (FunctionTreeModel *)model();
  if (!ftModel) return;

  std::vector<FunctionTreeModel::Channel *> childChannels;
  std::vector<FunctionTreeModel::ChannelGroup *> channelGroups;

  // Scan for already active children - to decide whether to activate or
  // deactivate them
  int activeFlag = NO_CHANNELS;

  TreeModel::Item *item =
      static_cast<TreeModel::Item *>(index.internalPointer());
  if (item) {
    int c, cCount = item->getChildCount();
    for (c = 0; c != cCount; ++c) {
      FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(item->getChild(c));

      if (!channel) {
        FunctionTreeModel::ChannelGroup *channelGroup =
            dynamic_cast<FunctionTreeModel::ChannelGroup *>(item->getChild(c));
        if (!channelGroup) continue;
        channelGroups.push_back(channelGroup);
        continue;
      }
      if (channel->isHidden()) continue;

      childChannels.push_back(channel);

      activeFlag |= (channel->isActive() ? ACTIVE_CHANNELS : INACTIVE_CHANNELS);
    }
  }

  // Open the item (ie show children) if it was closed AND not all its children
  // were active
  bool someInactiveChannels = (activeFlag != ACTIVE_CHANNELS);

  if (someInactiveChannels && !isExpanded(index)) {
    setExpanded(index, true);
    ftModel->onExpanded(index);
  }

  if (item) {
    if (!childChannels.empty()) {
      // Activate child channels if there is some inactive channel - deactivate
      // otherwise
      int c, cCount = childChannels.size();
      for (c = 0; c != cCount; ++c)
        childChannels[c]->setIsActive(someInactiveChannels);

      for (int i = 0; i < (int)channelGroups.size(); i++)
        channelGroups[i]->setChildrenAllActive(someInactiveChannels);

      update();
    } else {
      // There was no child channel. Try to activate children groups.
      int c, cCount = item->getChildCount();
      for (c = 0; c != cCount; ++c)
        onActivated(item->getChild(c)->createIndex());
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onClick(TreeModel::Item *item, const QPoint &itemPos,
                               QMouseEvent *e) {
  m_draggingChannel = 0;
  FunctionTreeModel::Channel *channel =
      dynamic_cast<FunctionTreeModel::Channel *>(item);
  FxChannelGroup *fxChannelGroup = dynamic_cast<FxChannelGroup *>(item);
  StageObjectChannelGroup *stageObjectChannelGroup =
      dynamic_cast<StageObjectChannelGroup *>(item);

  m_clickedItem = channel;

  if (channel) {
    fxChannelGroup = dynamic_cast<FxChannelGroup *>(channel->getParent());
    stageObjectChannelGroup =
        dynamic_cast<StageObjectChannelGroup *>(channel->getParent());

    int x = itemPos.x();
    if (x >= 20)
      channel->setIsCurrent(true);
    else if (0 <= x && x < 20) {
      channel->setIsActive(
          (e->button() == Qt::RightButton) ? true : !channel->isActive());
      update();
    }
  }

  if (fxChannelGroup) {
    TFx *fx = fxChannelGroup->getFx();
    emit switchCurrentFx(fx);
  }

  if (stageObjectChannelGroup) {
    TStageObject *obj = stageObjectChannelGroup->getStageObject();
    emit switchCurrentObject(obj);
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onMidClick(TreeModel::Item *item, const QPoint &itemPos,
                                  QMouseEvent *e) {
  FunctionTreeModel::Channel *channel =
      dynamic_cast<FunctionTreeModel::Channel *>(item);
  if (channel && e->button() == Qt::MidButton) {
    m_draggingChannel   = channel;
    m_dragStartPosition = e->pos();
  } else
    m_draggingChannel = 0;
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onDrag(TreeModel::Item *item, const QPoint &itemPos,
                              QMouseEvent *e) {
  // middle drag of the channel item can retrieve expression name
  if ((e->buttons() & Qt::MidButton) && m_draggingChannel &&
      (e->pos() - m_dragStartPosition).manhattanLength() >=
          QApplication::startDragDistance()) {
    QDrag *drag         = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setText(m_draggingChannel->getExprRefName());
    drag->setMimeData(mimeData);
    static const QPixmap cursorPixmap(":Resources/dragcursor_exp_text.png");
    drag->setDragCursor(cursorPixmap, Qt::MoveAction);
    Qt::DropAction dropAction = drag->exec();
    return;
  }

  FunctionTreeModel::Channel *channel =
      dynamic_cast<FunctionTreeModel::Channel *>(item);
  if (!channel || !m_clickedItem) return;

  // i0: item under the current cursor position
  // i1: clicked item
  QModelIndex i0 = channel->createIndex(), i1 = m_clickedItem->createIndex();
  if (!i0.isValid() || !i1.isValid() || i0.parent() != i1.parent()) return;

  if (i0.row() > i1.row()) tswap(i0, i1);

  FunctionTreeModel *md = static_cast<FunctionTreeModel *>(model());

  bool active = m_clickedItem->isActive();

  for (int row = i0.row(); row <= i1.row(); ++row) {
    if (isRowHidden(row, i0.parent())) continue;

    QModelIndex index = md->index(row, 0, i0.parent());

    TreeModel::Item *chItem =
        static_cast<TreeModel::Item *>(index.internalPointer());
    FunctionTreeModel::Channel *ch =
        dynamic_cast<FunctionTreeModel::Channel *>(chItem);

    if (ch && ch->isActive() != active) {
      ch->setIsActive(active);
      update();
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onRelease() { m_clickedItem = 0; }

//-----------------------------------------------------------------------------

void FunctionTreeView::openContextMenu(TreeModel::Item *item,
                                       const QPoint &globalPos) {
  if (FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(item))
    openContextMenu(channel, globalPos);
  else if (FunctionTreeModel::ChannelGroup *group =
               dynamic_cast<FunctionTreeModel::ChannelGroup *>(item))
    openContextMenu(group, globalPos);
}

//-----------------------------------------------------------------------------

void FunctionTreeView::openContextMenu(FunctionTreeModel::Channel *channel,
                                       const QPoint &globalPos) {
  assert(channel);

  QWidget *pw = dynamic_cast<QWidget *>(parentWidget());
  if (!pw) return;

  FunctionViewer *fv = dynamic_cast<FunctionViewer *>(pw->parentWidget());

  if (!fv) {
    assert(fv);
    return;
  }

  QMenu menu;

  QAction saveCurveAction(tr("Save Curve"), 0);
  QAction loadCurveAction(tr("Load Curve"), 0);
  QAction exportDataAction(tr("Export Data"), 0);
  menu.addAction(&saveCurveAction);
  menu.addAction(&loadCurveAction);
  menu.addAction(&exportDataAction);

  QAction *action = menu.exec(globalPos);

  TDoubleParam *curve = channel->getParam();

  if (action == &saveCurveAction)
    fv->emitIoCurve((int)FunctionViewer::eSaveCurve, curve, "");
  else if (action == &loadCurveAction)
    fv->emitIoCurve((int)FunctionViewer::eLoadCurve, curve, "");
  else if (action == &exportDataAction)
    fv->emitIoCurve((int)FunctionViewer::eExportCurve, curve,
                    channel->getLongName().toStdString());
}

//-----------------------------------------------------------------------------

void FunctionTreeView::openContextMenu(FunctionTreeModel::ChannelGroup *group,
                                       const QPoint &globalPos) {
  assert(group);

  QMenu menu;

  QAction showAnimateOnly(tr("Show Animated Only"), 0);
  QAction showAll(tr("Show All"), 0);
  menu.addAction(&showAnimateOnly);
  menu.addAction(&showAll);

  // execute menu
  QAction *action = menu.exec(globalPos);

  if (action != &showAll && action != &showAnimateOnly) return;

  FunctionTreeModel::ChannelGroup::ShowFilter showFilter =
      (action == &showAll)
          ? FunctionTreeModel::ChannelGroup::ShowAllChannels
          : FunctionTreeModel::ChannelGroup::ShowAnimatedChannels;

  expand(group->createIndex());
  group->setShowFilter(showFilter);
}

//-----------------------------------------------------------------------------

void FunctionTreeView::updateAll() {
  FunctionTreeModel *functionTreeModel =
      dynamic_cast<FunctionTreeModel *>(model());
  assert(functionTreeModel);

  functionTreeModel->applyShowFilters();
  update();
}

//-----------------------------------------------------------------------------
/*! show all the animated channels when the scene switched
*/
void FunctionTreeView::displayAnimatedChannels() {
  FunctionTreeModel *functionTreeModel =
      dynamic_cast<FunctionTreeModel *>(model());
  assert(functionTreeModel);
  int i;
  for (i = 0; i < functionTreeModel->getStageObjectsChannelCount(); i++)
    functionTreeModel->getStageObjectChannel(i)->displayAnimatedChannels();
  for (i = 0; i < functionTreeModel->getFxsChannelCount(); i++)
    functionTreeModel->getFxChannel(i)->displayAnimatedChannels();
  update();
}

//-----------------------------------------------------------------------------
/*! show all the animated channels when the scene switched
*/
void FunctionTreeModel::ChannelGroup::displayAnimatedChannels() {
  int itemCount = getChildCount();
  int i;

  for (i = 0; i < itemCount; i++) {
    FunctionTreeModel::Channel *channel =
        dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
    if (!channel) {
      FunctionTreeModel::ChannelGroup *channelGroup =
          dynamic_cast<FunctionTreeModel::ChannelGroup *>(getChild(i));
      if (!channelGroup) continue;

      channelGroup->displayAnimatedChannels();
      continue;
    }
    channel->setIsActive(channel->getParam()->hasKeyframes());
  }
}
