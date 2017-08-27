

#include "toonzqt/functionviewer.h"

// TnzQt includes
#include "toonzqt/functionselection.h"
#include "toonzqt/functionpanel.h"
#include "toonzqt/functiontreeviewer.h"
#include "toonzqt/functionsheet.h"
#include "toonzqt/functionsegmentviewer.h"
#include "toonzqt/functiontoolbar.h"
#include "toonzqt/swatchviewer.h"
#include "toonzqt/dvscrollwidget.h"

// TnzLib includes
#include "toonz/tstageobjecttree.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/fxdag.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tparamcontainer.h"
#include "tunit.h"

// TnzCore includes
#include "tstopwatch.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QToolBar>
#include <QAction>

using namespace DVGui;

//=============================================================================
//
// FunctionPanel
//
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
FunctionViewer::FunctionViewer(QWidget *parent, Qt::WindowFlags flags)
#else
FunctionViewer::FunctionViewer(QWidget *parent, Qt::WFlags flags)
#endif
    : QSplitter(parent)
    , m_xshHandle(0)
    , m_frameHandle(0)
    , m_objectHandle(0)
    , m_fxHandle(0)
    , m_columnHandle(0)
    , m_curve(0)
    , m_selection(new FunctionSelection())
    , m_sceneHandle(0) {
  setObjectName("FunctionEditor");

  // Prepare local timeline
  m_localFrame.setFrame(0);
  setFocusPolicy(Qt::NoFocus);

  m_treeView         = new FunctionTreeView(this);
  m_functionGraph    = new FunctionPanel(this);
  m_numericalColumns = new FunctionSheet();
  m_toolbar          = new FunctionToolbar;
  m_segmentViewer =
      new FunctionSegmentViewer(this, m_numericalColumns, m_functionGraph);
  QWidget *leftPanel  = new QWidget();
  QWidget *rightPanel = new QWidget();

  //----
  m_treeView->resize(150, m_treeView->size().height());
  m_treeView->setMinimumWidth(0);
  m_treeView->setIconSize(QSize(21, 17));

  FunctionTreeModel *ftModel =
      dynamic_cast<FunctionTreeModel *>(m_treeView->model());
  m_functionGraph->setModel(ftModel);
  m_numericalColumns->setModel(ftModel);

  m_functionGraph->setSelection(getSelection());
  m_numericalColumns->setSelection(getSelection());

  m_numericalColumns->setViewer(this);

  m_toolbar->setSelection(m_selection);
  m_toolbar->setFocusPolicy(Qt::NoFocus);

  m_toolbar->setFrameHandle(
      &m_localFrame);  // The function editor adopts an internal timeline
  m_functionGraph->setFrameHandle(
      &m_localFrame);  // synchronized among its various sub-widgets.
  m_numericalColumns->setFrameHandle(
      &m_localFrame);  // In case an external m_frameHandle is specified,
                       // it synchronizes with that, too.

  //---- layout

  m_leftLayout = new QVBoxLayout();
  m_leftLayout->setMargin(0);
  m_leftLayout->setSpacing(0);
  {
    m_leftLayout->addWidget(m_toolbar);
    if (Preferences::instance()->isShowXSheetToolbarEnabled() &&
        Preferences::instance()->isExpandFunctionHeaderEnabled()) {
      m_leftLayout->addSpacing(66);
    } else
      m_leftLayout->addSpacing(36);
    m_leftLayout->addWidget(m_numericalColumns);
  }
  leftPanel->setLayout(m_leftLayout);

  addWidget(leftPanel);

  QVBoxLayout *rightLayout = new QVBoxLayout();
  rightLayout->setMargin(0);
  rightLayout->setSpacing(5);
  {
    rightLayout->addWidget(m_segmentViewer, 0);
    rightLayout->addWidget(m_treeView, 1);
  }
  rightPanel->setLayout(rightLayout);

  addWidget(rightPanel);

  //--- set the splitter's default size
  setSizes(QList<int>() << 500 << 200);
  setStretchFactor(0, 5);
  setStretchFactor(1, 2);

  //---- signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_toolbar, SIGNAL(numericalColumnToggled()), this,
                       SLOT(toggleMode()));
  ret = ret && connect(ftModel, SIGNAL(activeChannelsChanged()),
                       m_functionGraph, SLOT(update()));
  ret = ret && connect(ftModel, SIGNAL(activeChannelsChanged()),
                       m_numericalColumns, SLOT(updateAll()));
  ret = ret && connect(ftModel, SIGNAL(curveChanged(bool)), m_treeView,
                       SLOT(update()));
  ret = ret && connect(ftModel, SIGNAL(curveChanged(bool)), m_functionGraph,
                       SLOT(update()));
  ret = ret && connect(ftModel, SIGNAL(curveChanged(bool)), m_numericalColumns,
                       SLOT(updateAll()));
  ret = ret && connect(ftModel, SIGNAL(curveSelected(TDoubleParam *)), this,
                       SLOT(onCurveSelected(TDoubleParam *)));
  ret = ret && connect(ftModel, SIGNAL(curveChanged(bool)), m_segmentViewer,
                       SLOT(onCurveChanged()));
  ret = ret && connect(ftModel, SIGNAL(curveChanged(bool)), this,
                       SLOT(onCurveChanged(bool)));
  ret = ret && connect(&m_localFrame, SIGNAL(frameSwitched()), this,
                       SLOT(onFrameSwitched()));
  ret = ret && connect(getSelection(), SIGNAL(selectionChanged()), this,
                       SLOT(onSelectionChanged()));
  ret = ret && connect(m_functionGraph, SIGNAL(keyframeSelected(double)),
                       m_toolbar, SLOT(setFrame(double)));

  ret = ret && connect(m_treeView, SIGNAL(switchCurrentObject(TStageObject *)),
                       this, SLOT(doSwitchCurrentObject(TStageObject *)));
  ret = ret && connect(m_treeView, SIGNAL(switchCurrentFx(TFx *)), this,
                       SLOT(doSwitchCurrentFx(TFx *)));

  ret = ret &&
        connect(ftModel,
                SIGNAL(currentChannelChanged(FunctionTreeModel::Channel *)),
                m_numericalColumns,
                SLOT(onCurrentChannelChanged(FunctionTreeModel::Channel *)));

  assert(ret);

  m_functionGraph->hide();
}

//-----------------------------------------------------------------------------

FunctionViewer::~FunctionViewer() {
  delete m_selection;
  m_toolbar->setFrameHandle(0);
}

//-----------------------------------------------------------------------------

void FunctionViewer::showEvent(QShowEvent *) {
  refreshModel();

  // Connect handles
  bool ret = true;

  if (m_xshHandle) {
    ret = connect(m_xshHandle, SIGNAL(xsheetChanged()), this,
                  SLOT(refreshModel())) &&
          ret;
    ret = connect(m_xshHandle, SIGNAL(xsheetSwitched()), this,
                  SLOT(rebuildModel())) &&
          ret;
  }

  if (m_frameHandle)
    ret = connect(m_frameHandle, SIGNAL(frameSwitched()), this,
                  SLOT(propagateExternalSetFrame())) &&
          ret;

  if (m_objectHandle) {
    ret = connect(m_objectHandle, SIGNAL(objectSwitched()), this,
                  SLOT(onStageObjectSwitched())) &&
          ret;
    ret = connect(m_objectHandle, SIGNAL(objectChanged(bool)), this,
                  SLOT(onStageObjectChanged(bool))) &&
          ret;
  }

  if (m_fxHandle)
    ret =
        connect(m_fxHandle, SIGNAL(fxSwitched()), this, SLOT(onFxSwitched())) &&
        ret;

  // display animated channels when the scene is switched
  if (m_sceneHandle)
    ret = connect(m_sceneHandle, SIGNAL(sceneSwitched()), m_treeView,
                  SLOT(displayAnimatedChannels())) &&
          ret;

  assert(ret);

  m_treeView->setExpanded(m_treeView->model()->index(0, 0), true);
  m_treeView->setExpanded(m_treeView->model()->index(1, 0), true);

  FunctionTreeModel *ftm =
      static_cast<FunctionTreeModel *>(m_treeView->model());

  if (m_objectHandle) {
    assert(m_xshHandle);
    TXsheet *xsh = m_xshHandle->getXsheet();

    const TStageObjectId &objId = m_objectHandle->getObjectId();
    ftm->setCurrentStageObject((objId == TStageObjectId::NoneId)
                                   ? (TStageObject *)0
                                   : xsh->getStageObject(objId));
  }

  if (m_fxHandle) ftm->setCurrentFx(m_fxHandle->getFx());
}

//-----------------------------------------------------------------------------

void FunctionViewer::hideEvent(QHideEvent *) {
  if (m_xshHandle) m_xshHandle->disconnect(this);
  if (m_frameHandle) m_frameHandle->disconnect(this);
  if (m_objectHandle) m_objectHandle->disconnect(this);
  if (m_fxHandle) m_fxHandle->disconnect(this);
  if (m_sceneHandle) m_sceneHandle->disconnect(this);

  if (m_functionGraph->isVisible()) m_functionGraph->hide();
}

//-----------------------------------------------------------------------------

void FunctionViewer::openContextMenu(TreeModel::Item *item,
                                     const QPoint &globalPos) {
  m_treeView->openContextMenu(item, globalPos);
}

//-----------------------------------------------------------------------------

void FunctionViewer::rebuildModel() {
  getSelection()->selectNone();
  onCurveSelected(0);
  m_functionGraph->getModel()->resetAll();
  refreshModel();
  m_treeView->displayAnimatedChannels();
}

//-----------------------------------------------------------------------------

void FunctionViewer::refreshModel() {
  TXsheet *xsh = m_xshHandle ? m_xshHandle->getXsheet() : 0;

  m_functionGraph->getModel()->refreshData(xsh);

  if (xsh) {
    int rowCount = xsh->getFrameCount();
    m_numericalColumns->setRowCount(rowCount);
    m_numericalColumns->updateAll();

    ToonzScene *scene = xsh->getScene();
    if (!scene)  // This seems wrong. It should rather be
      return;    // asserted - though I'm not touching it now...

    TFilePath scenePath = scene->getScenePath().getParentDir();
    if (scene->isUntitled())
      scenePath =
          TProjectManager::instance()->getCurrentProject()->getScenesPath();

    m_treeView->setCurrentScenePath(scenePath);

    int distance, offset;
    scene->getProperties()->getMarkers(distance, offset);
    m_numericalColumns->setMarkRow(distance, offset);
  }

  m_treeView->updateAll();
  m_toolbar->setCurve(0);
}

//-----------------------------------------------------------------------------

void FunctionViewer::setXsheetHandle(TXsheetHandle *xshHandle) {
  if (xshHandle == m_xshHandle) return;

  if (m_xshHandle) m_xshHandle->disconnect(this);

  m_xshHandle = xshHandle;
  m_segmentViewer->setXsheetHandle(xshHandle);

  if (m_xshHandle && isVisible()) {
    TXsheet *xsh = m_xshHandle->getXsheet();
    m_functionGraph->getModel()->refreshData(xsh);

    bool ret = connect(m_xshHandle, SIGNAL(xsheetChanged()), this,
                       SLOT(refreshModel()));
    assert(ret);
  }
}

//-----------------------------------------------------------------------------

void FunctionViewer::setFrameHandle(TFrameHandle *frameHandle) {
  if (m_frameHandle == frameHandle) return;

  if (m_frameHandle) m_frameHandle->disconnect(this);

  m_frameHandle = frameHandle;

  if (m_frameHandle && isVisible()) {
    bool ret = connect(m_frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(propagateExternalSetFrame()));
    assert(ret);
  }
}

//-----------------------------------------------------------------------------

void FunctionViewer::setObjectHandle(TObjectHandle *objectHandle) {
  if (objectHandle == m_objectHandle) return;

  if (m_objectHandle) m_objectHandle->disconnect(this);

  m_objectHandle = objectHandle;

  if (m_objectHandle && isVisible()) {
    m_treeView->updateAll();

    bool ret = true;
    ret      = connect(m_objectHandle, SIGNAL(objectSwitched()), this,
                  SLOT(onStageObjectSwitched())) &&
          ret;
    ret = connect(m_objectHandle, SIGNAL(objectChanged(bool)), this,
                  SLOT(onStageObjectChanged(bool))) &&
          ret;

    assert(ret);
  }

  FunctionTreeModel *ftModel =
      static_cast<FunctionTreeModel *>(m_treeView->model());
  if (ftModel) ftModel->setObjectHandle(objectHandle);
}
//-----------------------------------------------------------------------------

void FunctionViewer::setFxHandle(TFxHandle *fxHandle) {
  if (fxHandle == m_fxHandle) return;

  if (m_fxHandle) m_fxHandle->disconnect(this);

  m_fxHandle = fxHandle;

  if (isVisible()) {
    m_treeView->updateAll();

    bool ret =
        connect(m_fxHandle, SIGNAL(fxSwitched()), this, SLOT(onFxSwitched()));
    assert(ret);
  }

  FunctionTreeModel *ftModel =
      static_cast<FunctionTreeModel *>(m_treeView->model());
  if (ftModel) ftModel->setFxHandle(fxHandle);
}

//-----------------------------------------------------------------------------

void FunctionViewer::setColumnHandle(TColumnHandle *columnHandle) {
  if (columnHandle == m_columnHandle) return;

  m_columnHandle = columnHandle;

  if (isVisible()) m_treeView->updateAll();
}

//-----------------------------------------------------------------------------

void FunctionViewer::onFrameSwitched() {
  int frame = m_localFrame.getFrame();
  m_segmentViewer->setSegmentByFrame(m_curve, frame);

  if (m_frameHandle) m_frameHandle->setFrame(frame);
}

//-----------------------------------------------------------------------------

void FunctionViewer::toggleMode() {
  if (isHidden()) return;

  if (m_functionGraph->isVisible())
    m_functionGraph->hide();
  else
    m_functionGraph->show();
}

//-----------------------------------------------------------------------------

void FunctionViewer::onCurveChanged(bool isDragging) {
  if (m_objectHandle) m_objectHandle->notifyObjectIdChanged(isDragging);

  // emit signal if the current channel belongs to Fx in order to update the
  // preview fx
  if (m_fxHandle) {
    FunctionTreeModel *ftModel =
        dynamic_cast<FunctionTreeModel *>(m_treeView->model());
    if (ftModel) {
      FunctionTreeModel::Channel *currChan = ftModel->getCurrentChannel();
      if (currChan) {
        //カレントチャンネルがFxChannelGroupに含まれていたらEmit
        FxChannelGroup *fxChanGroup =
            dynamic_cast<FxChannelGroup *>(currChan->getChannelGroup());
        if (fxChanGroup) m_fxHandle->notifyFxChanged();
      }
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionViewer::onCurveSelected(TDoubleParam *curve) {
  m_curve = curve;
  m_toolbar->setCurve(curve);

  QPair<TDoubleParam *, int> selectedSegment =
      getSelection()->getSelectedSegment();
  if (selectedSegment.first)
    m_segmentViewer->setSegment(selectedSegment.first, selectedSegment.second);
  else
    m_segmentViewer->setSegment(m_curve, -1);
}

//-----------------------------------------------------------------------------

void FunctionViewer::onValueFieldChanged() {}

//-----------------------------------------------------------------------------

void FunctionViewer::onXsheetChanged() {
  TXsheet *xsh = m_xshHandle->getXsheet();
  int rowCount = xsh->getFrameCount();
  m_numericalColumns->setRowCount(rowCount);
}

//-----------------------------------------------------------------------------

void FunctionViewer::onStageObjectSwitched() {
  TXsheet *xsh = m_xshHandle->getXsheet();

  const TStageObjectId &objId = m_objectHandle->getObjectId();
  TStageObject *obj           = (objId == TStageObjectId::NoneId)
                          ? (TStageObject *)0
                          : xsh->getStageObject(objId);

  static_cast<FunctionTreeModel *>(m_treeView->model())
      ->setCurrentStageObject(obj);

  m_treeView->updateAll();
  m_functionGraph->update();
}

//-----------------------------------------------------------------------------

void FunctionViewer::onStageObjectChanged(bool isDragging) {
  TXsheet *xsh = m_xshHandle->getXsheet();

  const TStageObjectId &objId = m_objectHandle->getObjectId();
  TStageObject *obj           = (objId == TStageObjectId::NoneId)
                          ? (TStageObject *)0
                          : xsh->getStageObject(objId);

  static_cast<FunctionTreeModel *>(m_treeView->model())
      ->setCurrentStageObject(obj);

  if (!isDragging) m_treeView->updateAll();

  m_functionGraph->update();
}

//-----------------------------------------------------------------------------

void FunctionViewer::onFxSwitched() {
  TFx *fx              = m_fxHandle->getFx();
  TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx);
  if (zfx) fx          = zfx->getZeraryFx();
  static_cast<FunctionTreeModel *>(m_treeView->model())->setCurrentFx(fx);
  m_treeView->updateAll();
  m_functionGraph->update();
}

//-----------------------------------------------------------------------------

void FunctionViewer::onSelectionChanged() {
  QPair<TDoubleParam *, int> selectedSegment =
      getSelection()->getSelectedSegment();
  if (selectedSegment.first) {
    if (selectedSegment.first != m_curve) {
      m_curve = selectedSegment.first;
      m_toolbar->setCurve(selectedSegment.first);
    }
    m_segmentViewer->setSegment(selectedSegment.first, selectedSegment.second);
  } else {
    m_segmentViewer->setSegment(m_curve, -1);
  }
  // update curves
  if (m_functionGraph->isVisible() && !m_functionGraph->hasFocus())
    m_functionGraph->update();
}

//-----------------------------------------------------------------------------

void FunctionViewer::doSwitchCurrentObject(TStageObject *obj) {
  TStageObjectId id = obj->getId();
  if (id.isColumn())
    m_columnHandle->setColumnIndex(id.getIndex());
  else
    m_objectHandle->setObjectId(id);
}

//-----------------------------------------------------------------------------

void FunctionViewer::doSwitchCurrentFx(TFx *fx) {
  if (!fx) {
    m_fxHandle->setFx(0);
    return;
  }
  if (fx->isZerary()) {
    TXsheet *xsh = m_xshHandle->getXsheet();
    int i, columnCount = xsh->getColumnCount();
    for (i = 0; i < columnCount; i++) {
      TXshColumn *column = xsh->getColumn(i);
      TXshZeraryFxColumn *zfx =
          dynamic_cast<TXshZeraryFxColumn *>(xsh->getColumn(i));
      if (!zfx) continue;
      if (zfx->getZeraryColumnFx()->getZeraryFx() == fx) {
        fx = zfx->getZeraryColumnFx();
        m_columnHandle->setColumnIndex(i);
        break;
      }
    }
  }
  // Forbid update of the swatch upon column switch. This would generate
  // a useless render...
  SwatchViewer::suspendRendering(true, false);

  int columnIndex = fx->getReferenceColumnIndex();
  if (columnIndex >= 0) m_columnHandle->setColumnIndex(columnIndex);

  SwatchViewer::suspendRendering(false);
  m_fxHandle->setFx(fx);
  emit editObject();
}

//-----------------------------------------------------------------------------

void FunctionViewer::propagateExternalSetFrame() {
  assert(m_frameHandle);
  m_localFrame.setFrame(m_frameHandle->getFrame());
}

//-----------------------------------------------------------------------------

void FunctionViewer::addParameter(TParam *parameter, const TFilePath &folder) {
  static_cast<FunctionTreeModel *>(m_treeView->model())
      ->addParameter(parameter, folder);
}

//-----------------------------------------------------------------------------

void FunctionViewer::setFocusColumnsOrGraph() {
  m_numericalColumns->setFocus();
}

//-----------------------------------------------------------------------------

void FunctionViewer::clearFocusColumnsAndGraph() {
  m_functionGraph->clearFocus();
  m_numericalColumns->clearFocus();
}

//-----------------------------------------------------------------------------

bool FunctionViewer::columnsOrGraphHasFocus() {
  return m_functionGraph->hasFocus() ||
         m_numericalColumns->anyWidgetHasFocus() ||
         m_toolbar->anyWidgetHasFocus() || m_segmentViewer->anyWidgetHasFocus();
}

//-----------------------------------------------------------------------------

void FunctionViewer::setSceneHandle(TSceneHandle *sceneHandle) {
  m_sceneHandle = sceneHandle;
}

//----------------------------------------------------------------------------

bool FunctionViewer::isExpressionPageActive() {
  return m_segmentViewer->isExpressionPageActive();
}
