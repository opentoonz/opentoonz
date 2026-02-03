#pragma once

#ifndef FUNCTIONTREEMODEL_H
#define FUNCTIONTREEMODEL_H

// TnzCore includes
#include "tcommon.h"
#include "tdoubleparam.h"

// TnzQt includes
#include "treemodel.h"

// Qt includes
#include <QScrollBar>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================================

//    Forward declarations

class TStageObject;
class TFx;
class TDoubleParam;
class TXsheet;
class TParamContainer;
class TFxHandle;
class TObjectHandle;
class TXsheetHandle;

class FunctionTreeView;
class FunctionViewer;

//=================================================================

//*****************************************************************************************
//    FunctionTreeModel  declaration
//*****************************************************************************************

/*!
  \brief    The Function Editor's (tree-like) \a model, as in the
  <I>model-view</I> architecture.

  \details  This class represents the data associated to Toonz's Function Editor
  panels in a
            view-independent way. The model's purpose is that of representing
  all \a channels of the
            objects in a scene. A \a channel is here intended as an
  <I>animatable parameter</I>
            represented by a <I>single real-valued function</I>.

            Animatable objects are currently subdivided in two main types:
  <I>stage objects</I>
            (which consist roughly all the objects represented in the \a stage
  schematic view,
            including cameras, spline curves and pegbars), and \a fxs. Stage
  objects typically
            feature a uniform channels group structure, whereas each fx type
  have a different set
            of parameters (and thus channels). Recently, \a column objects can
  sport an additional
            group of channels, related to Plastic skeleton animations (see
  TnzExt library).
*/

class DVAPI FunctionTreeModel final : public TreeModel, public TParamObserver {
  Q_OBJECT

public:
  /*!
\brief    FunctionTreeModel's abstract base item class, adding the requirement
to
        return features visibility data.
*/
  class Item : public TreeModel::Item {
  public:
    Item() {}

    virtual bool isActive() const   = 0;
    virtual bool isAnimated() const = 0;
    virtual bool isIgnored() const  = 0;
  };

  //----------------------------------------------------------------------------------

  //! The model item representing a channels group.
  class ChannelGroup : public Item {
  public:
    enum ShowFilter { ShowAllChannels, ShowAnimatedChannels };

  private:
    QString m_name;
    ShowFilter m_showFilter;

  public:
    ChannelGroup(const QString &name = "");
    ~ChannelGroup();

    bool isActive() const override;
    bool isAnimated() const override;
    bool isIgnored() const override;

    virtual QString getShortName() const { return m_name; }
    virtual QString getLongName() const { return m_name; }

    virtual QString getIdName() const;

    void setShowFilter(ShowFilter showFilter);
    ShowFilter getShowFilter() const { return m_showFilter; }

    void applyShowFilter();  // call this method when a channel changes
                             // its animation status
    QVariant data(int role) const override;

    // used in FunctionTreeView::onActivated
    void setChildrenAllActive(bool active);

    void displayAnimatedChannels();
  };

  //----------------------------------------------------------------------------------

  /*!
\brief    The common class representing a \a parameter in the model.

\remark   This class's concept is different from that of a Channel, as a \a
parameter could
        be composed of <I>multiple channels</I>, e.g. like an animated \p RGBA
color, which
        has 4 channels.
*/
  class ParamWrapper {
  protected:
    TParamP m_param;      //!< The wrapped parameter.
    std::wstring m_fxId;  //!< Fx identifier for m_param's owner, if any.

  public:
    ParamWrapper(const TParamP &param, const std::wstring &fxId)
        : m_param(param), m_fxId(fxId) {}
    virtual ~ParamWrapper() {}

    const std::wstring &getFxId() const { return m_fxId; }

    TParamP getParam() const { return m_param; }
    virtual void setParam(const TParamP &param) { m_param = param; }
  };

  //----------------------------------------------------------------------------------

  //! The model item representing a channel (i.e. a real-valued function).
  class DVAPI Channel final : public ParamWrapper,
                              public Item,
                              public TParamObserver {
    FunctionTreeModel *m_model;  //!< (\p not \p owned) Reference to the model
    ChannelGroup
        *m_group;  //!< (\p not \p owned) Reference to the enclosing group

    std::string m_paramNamePref;

    bool m_isActive;  //!< Whether the channels is active, ie visible
                      //!< as a curve and numeric column
  public:
    Channel(FunctionTreeModel *model, TDoubleParam *param,
            std::string paramNamePrefix = "", std::wstring fxId = L"");
    ~Channel();

    TDoubleParam *getParam() const {
      return (TDoubleParam *)m_param.getPointer();
    }
    void setParam(const TParamP &param) override;

    QString getShortName() const;
    QString getLongName() const;

    // in order to show the expression name in the tooltip
    QString getExprRefName() const;

    ChannelGroup *getChannelGroup() const { return m_group; }
    void setChannelGroup(ChannelGroup *group) { m_group = group; }

    QVariant data(int role) const override;

    bool isActive() const override { return m_isActive; }
    void setIsActive(bool active);

    bool isAnimated() const override;
    bool isIgnored() const override;

    bool isCurrent() const;
    void setIsCurrent(bool current);

    bool isHidden() const;  // the channel is hidden if it is filtered out
                            // by its channelgroup
    void onChange(const TParamChange &) override;

    void *getInternalPointer() const override;
  };

private:
  ChannelGroup
      *m_stageObjects,  //!< Predefined group for stage object channels.
      *m_fxs;           //!< Predefined group for fx parameters.

  std::vector<Channel *> m_activeChannels;

  Channel *m_currentChannel;  //!< (\p not \p owned) Current channel.
  TStageObject
      *m_currentStageObject;  //!< (\p not \p owned) Current stage object.
  TFx *m_currentFx;           //!< (\p not \p owned) Current fx.

  bool m_paramsChanged;

  TFxHandle *m_fxHandle;
  TObjectHandle *m_objectHandle;

public:
  FunctionTreeModel(
      FunctionTreeView *parent = 0);  // BUT! Should be view-independent! :o
  ~FunctionTreeModel();

  Channel *getCurrentChannel() const { return m_currentChannel; }

  Channel *getActiveChannel(int index) const;
  int getActiveChannelCount() const { return m_activeChannels.size(); }

  int getColumnIndexByCurve(TDoubleParam *param) const;

  double getValue(Channel *channel, double frame) const;
  int getClosestKeyframe(Channel *channel,
                         double frame) const;  // -1 if not found
  Channel *getClosestChannel(double frame, double value) const;

  void refreshActiveChannels();
  void refreshData(TXsheet *xsh);  // call this method when the stageObject/Fx
                                   // structure has been modified
  void resetAll();

  void applyShowFilters();

  void setCurrentStageObject(TStageObject *obj) { m_currentStageObject = obj; }
  TStageObject *getCurrentStageObject() const { return m_currentStageObject; }

  void setCurrentFx(TFx *fx);
  TFx *getCurrentFx() const { return m_currentFx; }

  void addParameter(
      TParam *parameter,
      const TFilePath
          &folder);  //!< See function FunctionViewer::addParameter().

  TFxHandle *getFxHandle() { return m_fxHandle; }
  void setFxHandle(TFxHandle *fxHandle) { m_fxHandle = fxHandle; }

  TObjectHandle *getObjectHandle() { return m_objectHandle; }
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

signals:

  void activeChannelsChanged();
  void curveSelected(TDoubleParam *);
  void curveChanged(bool isDragging);
  void currentChannelChanged(FunctionTreeModel::Channel *);

private:
  void addParameter(ChannelGroup *group, const std::string &prefixString,
                    const std::wstring &fxId, TParam *param);

  //! remove channel from m_activeChannels and m_currentChannel
  void onChannelDestroyed(Channel *channel);

  //! called when channel status (active/current) has been modified
  void emitDataChanged(Channel *channel) {
    QModelIndex index = channel->createIndex();
    emit dataChanged(index, index);
    emit activeChannelsChanged();
  }

  void emitCurveSelected(TDoubleParam *curve) { emit curveSelected(curve); }

  void emitCurrentChannelChanged(FunctionTreeModel::Channel *channel) {
    emit currentChannelChanged(channel);
  }

  void addChannels(TFx *fx, ChannelGroup *fxItem, TParamContainer *params);

  // Observers notification functions

  void onChange(
      const TParamChange &) override;   // Multiple param notifications ...
  void onParamChange(bool isDragging);  // ... that get compressed into one

  // Update functions

  void refreshStageObjects(TXsheet *xsh);
  void refreshFxs(TXsheet *xsh);
  void refreshPlasticDeformations();
  void addActiveChannels(TreeModel::Item *item);

public:
  ChannelGroup *getStageObjectChannel(int index) const;
  ChannelGroup *getFxChannel(int index) const;
  int getStageObjectsChannelCount() const {
    return m_stageObjects->getChildCount();
  }
  int getFxsChannelCount() const { return m_fxs->getChildCount(); }
};

//=============================================================================

class DVAPI FxChannelGroup final : public FunctionTreeModel::ChannelGroup {
public:
  TFx *m_fx;

public:
  FxChannelGroup(TFx *fx);
  ~FxChannelGroup();

  QString getShortName() const override;
  QString getLongName() const override;

  QString getIdName() const override;

  void *getInternalPointer() const override {
    return static_cast<void *>(m_fx);
  }
  TFx *getFx() const { return m_fx; }
  QVariant data(int role) const override;

  void refresh() override;
};

//=============================================================================

class DVAPI StageObjectChannelGroup final
    : public FunctionTreeModel::ChannelGroup {
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

//*****************************************************************************************
//    FunctionTreeView  declaration
//*****************************************************************************************

//! TreeView with stage object and fx channels. controls channel visibility and
//! current channel
class FunctionTreeView final : public TreeView {
  Q_OBJECT

  TFilePath m_scenePath;
  FunctionTreeModel::Channel *m_clickedItem;

  FunctionTreeModel::Channel *m_draggingChannel;
  QPoint m_dragStartPosition;
  FunctionViewer *m_viewer;
  //---

  // set color by using style sheet
  QColor m_textColor;  // text color (black)
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)

  TXsheetHandle *m_xshHandle;

public:
  FunctionTreeView(FunctionViewer *parent);

  void setCurrentScenePath(TFilePath scenePath) { m_scenePath = scenePath; }

  void openContextMenu(TreeModel::Item *item, const QPoint &globalPos) override;

  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }
  FunctionViewer *getViewer() { return m_viewer; }

  void setXsheetHandle(TXsheetHandle *xshHandle) { m_xshHandle = xshHandle; }
  TXsheetHandle *getXsheetHandle() { return m_xshHandle; }

protected:
  void onClick(TreeModel::Item *item, const QPoint &itemPos,
               QMouseEvent *e) override;

  void onMidClick(TreeModel::Item *item, const QPoint &itemPos,
                  QMouseEvent *e) override;

  void onDrag(TreeModel::Item *item, const QPoint &itemPos,
              QMouseEvent *e) override;
  void onRelease() override;

  void openContextMenu(FunctionTreeModel::Channel *channel,
                       const QPoint &globalPos);
  void openContextMenu(FunctionTreeModel::ChannelGroup *group,
                       const QPoint &globalPos);

public slots:

  void onActivated(const QModelIndex &index);
  void updateAll();

  // show all the animated channels when the scene switched
  void displayAnimatedChannels();

signals:

  void switchCurrentObject(TStageObject *obj);
  void switchCurrentFx(TFx *fx);
};

#endif  // FUNCTIONTREEMODEL_H
