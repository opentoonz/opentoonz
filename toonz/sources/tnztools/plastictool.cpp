

#include "plastictool.h"

// TnzTools includes
#include "tw/keycodes.h"  // Obsolete by now... still currently used though
#include "tooloptionscontrols.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvmimedata.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/selectioncommandids.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tstageobject.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshsimplelevel.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticdeformerstorage.h"

// TnzCore includes
#include "tgl.h"
#include "tundo.h"
#include "tfunctorinvoker.h"

// Qt includes
#include <QApplication>
#include <QString>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QClipboard>

// tcg includes
#include "tcg/tcg_macros.h"
#include "tcg/tcg_point_ops.h"
#include "tcg/tcg_list.h"
#include "tcg/tcg_function_types.h"
#include "tcg/tcg_iterator_ops.h"

//****************************************************************************************
//    Local namespace
//****************************************************************************************

namespace {

static const double l_dmax = (std::numeric_limits<double>::max)();

}  // namespace

//****************************************************************************************
//    PlasticToolLocals namespace
//****************************************************************************************

using namespace PlasticToolLocals;

namespace PlasticToolLocals {

PlasticTool l_plasticTool;
bool l_suspendParamsObservation = false;

//------------------------------------------------------------------------

TPointD projection(const PlasticSkeleton &skeleton, int e, const TPointD &pos) {
  const PlasticSkeleton::edge_type &ed = skeleton.edge(e);

  const TPointD &p0 = skeleton.vertex(ed.vertex(0)).P();
  const TPointD &p1 = skeleton.vertex(ed.vertex(1)).P();

  return tcg::point_ops::projection(pos, p0, tcg::point_ops::direction(p0, p1));
}

//------------------------------------------------------------------------

double frame() {
  return TTool::getApplication()->getCurrentFrame()->getFrame();
}

//------------------------------------------------------------------------

int row() { return int(frame()) + 1; }

//------------------------------------------------------------------------

int column() {
  return TTool::getApplication()->getCurrentColumn()->getColumnIndex();
}

//------------------------------------------------------------------------

void setCell(int row, int col) {
  TTool::Application *app = TTool::getApplication();
  app->getCurrentFrame()->setCurrentFrame(row);
  app->getCurrentColumn()->setColumnIndex(col);
}

//------------------------------------------------------------------------

TXshColumn *xshColumn() {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  return xsh->getColumn(column());
}

//------------------------------------------------------------------------

TStageObject *stageObject() {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  return xsh->getStageObject(TStageObjectId::ColumnId(column()));
}

//------------------------------------------------------------------------

const TXshCell &xshCell() {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  return xsh->getCell(frame(), column());
}

//------------------------------------------------------------------------

int skeletonId() {
  TStageObject *obj                      = stageObject();
  const PlasticSkeletonDeformationP &def = obj->getPlasticSkeletonDeformation();

  return def ? def->skeletonId(obj->paramsTime(frame()))
             : 1;  // 1 (not -1) is intended.
}  // Means '' (empty string)

//------------------------------------------------------------------------

double sdFrame() { return stageObject()->paramsTime(frame()); }

//------------------------------------------------------------------------

void setKeyframe(TDoubleParamP &param, double frame) {
  if (!param->isKeyframe(frame)) {
    KeyframeSetter setter(param.getPointer(), -1,
                          false);  // Not placing undos through this setter
    setter.createKeyframe(frame);
  }
}

//------------------------------------------------------------------------

void setKeyframe(SkVD *vd, double frame) {
  // vd->setKeyframe(frame);                          // Nope. In fact...

  // Keyframe set is performed with a special tool that is NOT AVAILABLE in
  // TnzExt
  // (thus, not available to m_sd). It deals with specifying the correct
  // interpolation
  // type (by user preference, which is TnzLib stuff), etc...

  // Traverse vd's parameters. In case they don't have a keyframe at current
  // frame, add one.
  for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
    setKeyframe(vd->m_params[p], frame);
}

//------------------------------------------------------------------------

void setKeyframe(const PlasticSkeletonDeformationP &sd, double frame) {
  // NOTE: The skeleton ids parameter is NOT affected

  SkD::vd_iterator vdt, vdEnd;
  sd->vertexDeformations(vdt, vdEnd);

  for (; vdt != vdEnd; ++vdt) setKeyframe((*vdt).second, frame);
}

//------------------------------------------------------------------------

void invalidateXsheet() {
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  stageObject()->updateKeyframes();

  l_plasticTool.storeDeformation();
  l_plasticTool.invalidate();
}

}  // namespace PlasticToolLocals

//****************************************************************************************
//    Mime  definitions
//****************************************************************************************

struct PlasticSkeletonPMime final : public DvMimeData {
  PlasticSkeletonP m_skeleton;

public:
  PlasticSkeletonPMime(const PlasticSkeletonP &skeleton)
      : m_skeleton(skeleton) {}

  DvMimeData *clone() const override {
    return new PlasticSkeletonPMime(m_skeleton);
  }

  void releaseData() override { m_skeleton = PlasticSkeletonP(); }
};

struct SkDPMime final : public DvMimeData {
  SkDP m_sd;

public:
  SkDPMime(const SkDP &sd) : m_sd(sd) {}

  DvMimeData *clone() const override { return new SkDPMime(m_sd); }

  void releaseData() override { m_sd = SkDP(); }
};

//****************************************************************************************
//    Undo  definitions
//****************************************************************************************

// NOTE: Some of the following UNDOs have complex and dynamic contents, and I
// don't want to
//       trace their size thoroughly. So, I'll follow this guideline: given the
//       standard
//       100 MB undos pool, how many undos of one specific type I'd want the
//       pool to be able
//       to store?

namespace {

class SetVertexNameUndo final : public TUndo {
  int m_row, m_col;  //!< Xsheet coordinates
  int m_v;           //!< Changed vertex

public:
  mutable QString m_oldName, m_newName;  //!< Vertex names
  mutable SkVD
      m_oldVd;  //!< Old Vertex deformation (SHARE-OWNED, rather than CLONED)

public:
  SetVertexNameUndo(int v, const QString &newName)
      : m_row(::row()), m_col(::column()), m_v(v), m_newName(newName) {
    const PlasticSkeletonP &skeleton = l_plasticTool.skeleton();
    const PlasticSkeletonVertex &vx  = skeleton->vertex(v);

    m_oldName = vx.name();
  }

  int getSize() const override {
    return sizeof(*this);
  }  // sizeof this is roughly ok

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    // Store the vertex deformation before it's released (possibly destroyed)
    {
      const SkDP &sd = l_plasticTool.deformation();
      TCG_ASSERT(sd, return );

      const SkVD *vd = sd->vertexDeformation(m_oldName);
      TCG_ASSERT(vd, return );

      m_oldVd = *vd;
    }

    if (m_v >= 0) l_plasticTool.setSkeletonSelection(m_v);

    l_plasticTool.setVertexName(m_newName);

    ::invalidateXsheet();
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const SkDP &sd = l_plasticTool.deformation();
    TCG_ASSERT(sd, return );

    if (m_v >= 0) l_plasticTool.setSkeletonSelection(m_v);

    l_plasticTool.setVertexName(m_oldName);

    // Restore the vertex deformation.
    SkVD *vd = sd->vertexDeformation(m_oldName);
    assert(vd);

    *vd = m_oldVd;

    ::invalidateXsheet();
  }
};

//========================================================================

class PasteDeformationUndo final : public TUndo {
  int m_col;              //!< Affected column
  SkDP m_oldSd, m_newSd;  //!< The skeleton deformations

public:
  PasteDeformationUndo(const SkDP &newSd)
      : m_col(column())
      , m_oldSd(stageObject()->getPlasticSkeletonDeformation())
      , m_newSd(newSd) {}

  int getSize() const override { return 1 << 20; }

  void redo() const override {
    TTool::getApplication()->getCurrentColumn()->setColumnIndex(m_col);
    stageObject()->setPlasticSkeletonDeformation(m_newSd);
    ::invalidateXsheet();
  }

  void undo() const override {
    TTool::getApplication()->getCurrentColumn()->setColumnIndex(m_col);
    stageObject()->setPlasticSkeletonDeformation(m_oldSd);
    ::invalidateXsheet();
  }
};

}  // namespace

//****************************************************************************************
//    PlasticTool::TemporaryActivation  implementation
//****************************************************************************************

PlasticTool::TemporaryActivation::TemporaryActivation(int row, int col)
    : m_activate(!l_plasticTool.isActive()) {
  if (m_activate) l_plasticTool.onActivate();

  ::setCell(row, col);
}

//------------------------------------------------------------------------

PlasticTool::TemporaryActivation::~TemporaryActivation() {
  if (m_activate) l_plasticTool.onDeactivate();
}

//****************************************************************************************
//    PlasticToolOptionsBox::SkelIdComboBox  definition
//****************************************************************************************

class PlasticToolOptionsBox::SkelIdsComboBox final : public QComboBox {
public:
  SkelIdsComboBox(QWidget *parent = 0) : QComboBox(parent) {
    updateSkeletonsList();
  }

  void updateSkeletonsList();
  void updateCurrentSkeleton();
};

//------------------------------------------------------------------------

void PlasticToolOptionsBox::SkelIdsComboBox::updateSkeletonsList() {
  clear();

  const SkDP &sd = l_plasticTool.deformation();
  if (!sd) return;

  QStringList skeletonsList;

  SkD::skelId_iterator st, sEnd;
  sd->skeletonIds(st, sEnd);

  for (; st != sEnd; ++st) skeletonsList.push_back(QString::number(*st));

  QComboBox::insertItems(0, skeletonsList);

  updateCurrentSkeleton();
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::SkelIdsComboBox::updateCurrentSkeleton() {
  setCurrentIndex(findText(QString::number(::skeletonId())));
}

//****************************************************************************************
//    PlasticToolOptionsBox  implementation
//****************************************************************************************

PlasticToolOptionsBox::PlasticToolOptionsBox(QWidget *parent, TTool *tool,
                                             TPaletteHandle *pltHandle,
                                             ToolHandle *toolHandle)
    : GenericToolOptionsBox(parent, tool, pltHandle, PlasticTool::MODES_COUNT,
                            toolHandle)
    , m_tool(tool)
    , m_subToolbars(new GenericToolOptionsBox *[PlasticTool::MODES_COUNT])
//, m_subToolbarActions(new QAction*[PlasticTool::MODES_COUNT])
{
  struct locals {
    static inline QWidget *newSpace(QWidget *parent = 0) {
      QWidget *space = new QWidget(parent);
      space->setFixedWidth(TOOL_OPTIONS_LEFT_MARGIN);

      return space;
    }
  };

  // Create Mesh button
  QPushButton *meshifyButton = new QPushButton(tr("Create Mesh"));
  // Add skeleton id-related widgets
  QLabel *skelIdLabel = new QLabel(tr("Skeleton:"));
  m_skelIdComboBox    = new SkelIdsComboBox;
  m_addSkelButton     = new QPushButton("+");  // Connected in the show event
  m_removeSkelButton  = new QPushButton("-");  // Connected in the show event
  // Add sub-options for each mode group
  for (int m         = 0; m != PlasticTool::MODES_COUNT; ++m)
    m_subToolbars[m] = new GenericToolOptionsBox(0, tool, pltHandle, m);

  meshifyButton->setFixedHeight(20);
  QAction *meshifyAction =
      CommandManager::instance()->getAction("A_ToolOption_Meshify");
  meshifyButton->addAction(meshifyAction);

  skelIdLabel->setFixedHeight(20);
  m_skelIdComboBox->setFixedWidth(50);
  m_addSkelButton->setFixedSize(20, 20);
  m_removeSkelButton->setFixedSize(20, 20);
  for (int m = 0; m != PlasticTool::MODES_COUNT; ++m)
    m_subToolbars[m]->setContentsMargins(0, 0, 0, 0);

  /*- Layout -*/
  // Add created widgets to the toolbar (in reverse order since we're inserting
  // at 0)
  m_layout->insertWidget(0, m_removeSkelButton);
  m_layout->insertWidget(0, m_addSkelButton);
  m_layout->insertWidget(0, m_skelIdComboBox);
  m_layout->insertWidget(0, skelIdLabel);
  m_layout->insertWidget(0, locals::newSpace(this));
  m_layout->insertWidget(0, meshifyButton);
  m_layout->insertWidget(0, locals::newSpace(this));

  for (int m = 0; m != PlasticTool::MODES_COUNT; ++m)
    m_layout->insertWidget(m_layout->count() - 1, m_subToolbars[m], 1);

  bool ret = true;
  ret      = ret && connect(meshifyButton, SIGNAL(clicked()), meshifyAction,
                       SLOT(trigger()));
  assert(ret);

  // Add Animation mode fields corresponding to vertex properties
  GenericToolOptionsBox *animateOptionsBox =
      m_subToolbars[PlasticTool::ANIMATE_IDX];

  // Adjust some specific controls first
  {
    ToolOptionTextField *minAngleField = static_cast<ToolOptionTextField *>(
        animateOptionsBox->control("minAngle"));
    assert(minAngleField);

    minAngleField->setFixedWidth(40);

    ToolOptionTextField *maxAngleField = static_cast<ToolOptionTextField *>(
        animateOptionsBox->control("maxAngle"));
    assert(maxAngleField);

    maxAngleField->setFixedWidth(40);
  }

  // Distance
  ToolOptionParamRelayField *distanceField = new ToolOptionParamRelayField(
      &l_plasticTool, &l_plasticTool.m_distanceRelay);
  distanceField->setGlobalKey(&l_plasticTool.m_globalKey,
                              &l_plasticTool.m_relayGroup);

  ClickableLabel *distanceLabel = new ClickableLabel(tr("Distance"));
  distanceLabel->setFixedHeight(20);

  // Angle
  ToolOptionParamRelayField *angleField = new ToolOptionParamRelayField(
      &l_plasticTool, &l_plasticTool.m_angleRelay);
  angleField->setGlobalKey(&l_plasticTool.m_globalKey,
                           &l_plasticTool.m_relayGroup);

  ClickableLabel *angleLabel = new ClickableLabel(tr("Angle"));
  angleLabel->setFixedHeight(20);

  // SO
  ToolOptionParamRelayField *soField =
      new ToolOptionParamRelayField(&l_plasticTool, &l_plasticTool.m_soRelay);
  soField->setGlobalKey(&l_plasticTool.m_globalKey,
                        &l_plasticTool.m_relayGroup);

  ClickableLabel *soLabel = new ClickableLabel(tr("SO"));
  soLabel->setFixedHeight(20);

  QHBoxLayout *animateLayout = animateOptionsBox->hLayout();
  animateLayout->insertWidget(0, soField);
  animateLayout->insertWidget(0, soLabel);
  animateLayout->insertWidget(0, angleField);
  animateLayout->insertWidget(0, angleLabel);
  animateLayout->insertWidget(0, distanceField);
  animateLayout->insertWidget(0, distanceLabel);

  ret = ret && connect(distanceLabel, SIGNAL(onMousePress(QMouseEvent *)),
                       distanceField, SLOT(receiveMousePress(QMouseEvent *)));
  ret = ret && connect(distanceLabel, SIGNAL(onMouseMove(QMouseEvent *)),
                       distanceField, SLOT(receiveMouseMove(QMouseEvent *)));
  ret = ret && connect(distanceLabel, SIGNAL(onMouseRelease(QMouseEvent *)),
                       distanceField, SLOT(receiveMouseRelease(QMouseEvent *)));
  ret = ret && connect(angleLabel, SIGNAL(onMousePress(QMouseEvent *)),
                       angleField, SLOT(receiveMousePress(QMouseEvent *)));
  ret = ret && connect(angleLabel, SIGNAL(onMouseMove(QMouseEvent *)),
                       angleField, SLOT(receiveMouseMove(QMouseEvent *)));
  ret = ret && connect(angleLabel, SIGNAL(onMouseRelease(QMouseEvent *)),
                       angleField, SLOT(receiveMouseRelease(QMouseEvent *)));
  ret = ret && connect(soLabel, SIGNAL(onMousePress(QMouseEvent *)), soField,
                       SLOT(receiveMousePress(QMouseEvent *)));
  ret = ret && connect(soLabel, SIGNAL(onMouseMove(QMouseEvent *)), soField,
                       SLOT(receiveMouseMove(QMouseEvent *)));
  ret = ret && connect(soLabel, SIGNAL(onMouseRelease(QMouseEvent *)), soField,
                       SLOT(receiveMouseRelease(QMouseEvent *)));
  assert(ret);

  onPropertyChanged();
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::showEvent(QShowEvent *se) {
  bool ret = true;

  ret = ret && connect(&l_plasticTool, SIGNAL(skelIdsListChanged()),
                       SLOT(onSkelIdsListChanged()));
  ret = ret && connect(&l_plasticTool, SIGNAL(skelIdChanged()),
                       SLOT(onSkelIdChanged()));
  ret = ret && connect(m_skelIdComboBox, SIGNAL(activated(int)),
                       SLOT(onSkelIdEdited()));
  ret = ret &&
        connect(m_addSkelButton, SIGNAL(released()), SLOT(onAddSkeleton()));
  ret = ret && connect(m_removeSkelButton, SIGNAL(released()),
                       SLOT(onRemoveSkeleton()));

  assert(ret);

  m_skelIdComboBox->updateSkeletonsList();
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::hideEvent(QHideEvent *he) {
  disconnect(&l_plasticTool, 0, this, 0);
  disconnect(m_skelIdComboBox, 0, this, 0);
  disconnect(m_addSkelButton, 0, this, 0);
  disconnect(m_removeSkelButton, 0, this, 0);
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::onPropertyChanged() {
  // Fetch current mode index
  TPropertyGroup *pGroup = m_tool->getProperties(PlasticTool::MODES_COUNT);
  assert(pGroup);

  TEnumProperty *prop =
      dynamic_cast<TEnumProperty *>(pGroup->getProperty("mode"));
  assert(prop);

  int mode = prop->getIndex();

  // Show the specified mode options, hide all the others
  for (int m = 0; m != PlasticTool::MODES_COUNT; ++m)
    m_subToolbars[m]->setVisible(m == mode);
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::onSkelIdsListChanged() {
  m_skelIdComboBox->updateSkeletonsList();
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::onSkelIdChanged() {
  m_skelIdComboBox->updateCurrentSkeleton();
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::onSkelIdEdited() {
  int skelId = m_skelIdComboBox->currentText().toInt();
  if (skelId == ::skeletonId()) return;

  if (!l_plasticTool.deformation()) return;

  l_plasticTool.editSkelId_undo(skelId);
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::onAddSkeleton() {
  if (l_plasticTool.isEnabled())
    l_plasticTool.addSkeleton_undo(new PlasticSkeleton);
}

//------------------------------------------------------------------------

void PlasticToolOptionsBox::onRemoveSkeleton() {
  if (l_plasticTool.isEnabled() && l_plasticTool.deformation())
    l_plasticTool.removeSkeleton_withKeyframes_undo(::skeletonId());
}

//****************************************************************************************
//    PlasticTool  implementation
//****************************************************************************************

PlasticTool::PlasticTool()
    : TTool(T_Plastic)
    , m_skelId(-(std::numeric_limits<int>::max)())
    , m_propGroup(new TPropertyGroup[MODES_COUNT + 1])
    , m_mode("mode")
    , m_vertexName("vertexName", L"")
    , m_interpolate("interpolate", false)
    , m_snapToMesh("snapToMesh", false)
    , m_thickness("Thickness", 1, 100, 5)
    , m_rigidValue("rigidValue")
    , m_globalKey("globalKeyframe", true)
    , m_keepDistance("keepDistance", true)
    , m_minAngle("minAngle", L"")
    , m_maxAngle("maxAngle", L"")
    , m_distanceRelay("distanceRelay")
    , m_angleRelay("angleRelay")
    , m_soRelay("soRelay")
    , m_skelIdRelay("skelIdRelay")
    , m_pressedPos(TConsts::napd)
    , m_dragged(false)
    , m_svHigh(-1)
    , m_seHigh(-1)
    , m_mvHigh(-1)
    , m_meHigh(-1)
    , m_rigidityPainter(createRigidityPainter())
    , m_showSkeletonOS(true)
    , m_recompileOnMouseRelease(false) {
  // And now, a little trick about tool binding
  bind(TTool::AllImages);   // Attach the tool to all types :)
  bind(TTool::MeshLevels);  // But disable it for all but meshes :0

  // This little trick is needed to associate the tool to common levels (the
  // toolbar must appear), in
  // order to make them meshable.

  // Bind properties to the appropriate property group (needed by the automatic
  // toolbar builder)
  m_propGroup[MODES_COUNT].bind(m_mode);
  m_propGroup[MODES_COUNT].bind(m_vertexName);

  m_propGroup[RIGIDITY_IDX].bind(m_thickness);
  m_propGroup[RIGIDITY_IDX].bind(m_rigidValue);

  m_propGroup[BUILD_IDX].bind(m_interpolate);
  m_propGroup[BUILD_IDX].bind(m_snapToMesh);

  m_propGroup[ANIMATE_IDX].bind(m_globalKey);
  m_propGroup[ANIMATE_IDX].bind(m_keepDistance);
  m_propGroup[ANIMATE_IDX].bind(m_minAngle);
  m_propGroup[ANIMATE_IDX].bind(m_maxAngle);

  m_relayGroup.bind(m_distanceRelay);
  m_relayGroup.bind(m_angleRelay);
  m_relayGroup.bind(m_soRelay);

  m_mode.setId("SkeletonMode");
  m_vertexName.setId("VertexName");
  m_interpolate.setId("Interpolate");
  m_snapToMesh.setId("SnapToMesh");
  m_thickness.setId("Thickness");
  m_rigidValue.setId("RigidValue");
  m_globalKey.setId("GlobalKey");
  m_keepDistance.setId("KeepDistance");
  m_minAngle.setId("MinAngle");
  m_maxAngle.setId("MaxAngle");
  m_distanceRelay.setId("DistanceRelay");
  m_angleRelay.setId("AngleRelay");
  m_soRelay.setId("SoRelay");
  m_skelIdRelay.setId("SkelIdRelay");

  // Attach to selections
  m_svSel.setView(this);
  m_mvSel.setView(this), m_meSel.setView(this);
}

//------------------------------------------------------------------------

PlasticTool::~PlasticTool() {
  if (m_sd) m_sd->removeObserver(this);
}

//------------------------------------------------------------------------

TTool::ToolType PlasticTool::getToolType() const {
  switch (m_mode.getIndex()) {
  case MESH_IDX:
  case RIGIDITY_IDX:
    return TTool::LevelWriteTool;

  case BUILD_IDX:
  case ANIMATE_IDX:
    return TTool::ColumnTool;
  }

  assert(false);
  return TTool::GenericTool;
}

//------------------------------------------------------------------------

void PlasticTool::updateTranslation() {
  m_mode.setQStringName(tr("Mode:"));
  m_mode.deleteAllValues();
  m_mode.addValue(tr("Edit Mesh").toStdWString());
  m_mode.addValue(tr("Paint Rigid").toStdWString());
  m_mode.addValue(tr("Build Skeleton").toStdWString());
  m_mode.addValue(tr("Animate").toStdWString());
  m_mode.setIndex(BUILD_IDX);

  m_vertexName.setQStringName(tr("Vertex Name:"));
  m_interpolate.setQStringName(tr("Allow Stretching"));
  m_snapToMesh.setQStringName(tr("Snap To Mesh"));
  m_thickness.setQStringName(tr("Thickness"));

  m_rigidValue.setQStringName("");
  m_rigidValue.deleteAllValues();
  m_rigidValue.addValue(tr("Rigid").toStdWString());
  m_rigidValue.addValue(tr("Flex").toStdWString());

  m_globalKey.setQStringName(tr("Global Key"));
  m_keepDistance.setQStringName(tr("Keep Distance"));
  m_minAngle.setQStringName(tr("Angle Bounds"));
  m_maxAngle.setQStringName("");
}

//-------------------------------------------------------------------------------------------------------

ToolOptionsBox *PlasticTool::createOptionsBox() {
  // Create the options box
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = m_application->getCurrentTool();
  PlasticToolOptionsBox *optionsBox =
      new PlasticToolOptionsBox(0, this, currPalette, currTool);

  // Connect it to receive m_mode notifications
  m_mode.addListener(optionsBox);

  return optionsBox;
}

//------------------------------------------------------------------------

PlasticSkeletonP PlasticTool::skeleton() const {
  return m_sd ? m_sd->skeleton(::sdFrame()) : PlasticSkeletonP();
}

//------------------------------------------------------------------------

PlasticSkeleton &PlasticTool::deformedSkeleton() {
  typedef tcg::function<void (PlasticTool::*)(PlasticSkeleton &),
                        &PlasticTool::updateDeformedSkeleton>
      Func;

  return m_deformedSkeleton(tcg::bind1st(Func(), *this));
}

//------------------------------------------------------------------------

void PlasticTool::touchSkeleton() {
  touchDeformation();

  int skelId = ::skeletonId();
  if (!m_sd->skeleton(skelId)) {
    m_sd->attach(skelId, new PlasticSkeleton);
    emit skelIdsListChanged();
  }
}

//------------------------------------------------------------------------

void PlasticTool::touchDeformation() {
  if (m_sd) return;

  // Store a new deformation in the column's stage object
  stageObject()->setPlasticSkeletonDeformation(new PlasticSkeletonDeformation);
  storeDeformation();  // Builds the deformed skeleton too
}

//------------------------------------------------------------------------

void PlasticTool::storeDeformation() {
  const SkDP &sd = stageObject()->getPlasticSkeletonDeformation();
  if (m_sd != sd) {
    clearSkeletonSelections();

    if (m_sd) {
      m_sd->removeObserver(this);
      m_skelIdRelay.setParam(TDoubleParamP());
    }

    // Store the deformation, retrieving it from current column stage object
    m_sd = sd;

    if (m_sd) {
      m_sd->addObserver(this);
      m_skelIdRelay.setParam(m_sd->skeletonIdsParam());
    }

    m_skelIdRelay.notifyListeners();
  }

  storeSkeletonId();
  if (m_mode.getIndex() == ANIMATE_IDX)
    m_deformedSkeleton.invalidate();  // Store the deformed skeleton too

  emit skelIdsListChanged();
}

//------------------------------------------------------------------------

void PlasticTool::storeSkeletonId() {
  int skelId = m_sd ? m_sd->skeletonIdsParam()->getValue(::sdFrame())
                    : -(std::numeric_limits<int>::max)();
  if (m_skelId != skelId) {
    m_skelId = skelId;
    clearSkeletonSelections();

    emit skelIdChanged();
  }
}

//------------------------------------------------------------------------

void PlasticTool::updateDeformedSkeleton(PlasticSkeleton &deformedSkeleton) {
  if (m_sd)
    m_sd->storeDeformedSkeleton(::skeletonId(), ::sdFrame(), deformedSkeleton);
  else
    deformedSkeleton.clear();
}

//------------------------------------------------------------------------

void PlasticTool::onFrameSwitched() {
  storeSkeletonId();
  storeMeshImage();

  switch (m_mode.getIndex()) {
  case ANIMATE_IDX:
    m_deformedSkeleton.invalidate();
  }

  // Update the relays' current frame
  double frame = ::sdFrame();

  m_distanceRelay.frame() = frame;
  m_angleRelay.frame()    = frame;
  m_soRelay.frame()       = frame;
  m_skelIdRelay.frame()   = frame;

  m_distanceRelay.notifyListeners();
  m_angleRelay.notifyListeners();
  m_soRelay.notifyListeners();
  m_skelIdRelay.notifyListeners();
}

//------------------------------------------------------------------------

void PlasticTool::onColumnSwitched() {
  switch (m_mode.getIndex()) {
  case MESH_IDX:
  case BUILD_IDX:
  case RIGIDITY_IDX:
    m_pvs.m_showOriginalColumn = xshColumn();
  }

  storeDeformation();
  storeMeshImage();
}

//------------------------------------------------------------------------

void PlasticTool::onXsheetChanged() {
  onColumnSwitched();
  TTool::updateEnabled();  // Current cell may no longer be a mesh one (or
                           // viceversa),
}  // so tool enabled status must be updated.

//------------------------------------------------------------------------

void PlasticTool::onChange() {
  // Since parameters are typically coupled, we could pass multiple, consecutive
  // times in
  // this notification function. We have to employ counter-measures to prevent
  // multiple
  // calls from affecting performance.

  static bool refresh = false;  // Accessible from locals since static

  struct locals {
    struct RefreshFunctor final : public TFunctorInvoker::BaseFunctor {
      void operator()() override {
        refresh = false;
        l_plasticTool.storeSkeletonId();  // Calls ::sdFrame()

        // This is needed to repaint the xsheet (not automatic otherwise)
        TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(
            false);
      }
    };
  };  // locals

  // Using invalidate/update and delayed invocation to prevent multiple calls to
  // ::sdFrame()
  m_deformedSkeleton.invalidate();

  if (!refresh) {
    refresh = true;
    QMetaObject::invokeMethod(TFunctorInvoker::instance(), "invoke",
                              Qt::QueuedConnection,
                              Q_ARG(void *, new locals::RefreshFunctor));
  }

  // Passing through Qt's event system to compress repaints in a single one
  TTool::Viewer *viewer = getViewer();
  if (viewer)                 // This goes through paintEvent(),
    viewer->invalidateAll();  // \a unlike TTool::invalidate()
}

//------------------------------------------------------------------------

void PlasticTool::onChange(const TParamChange &pc) {
  if (l_suspendParamsObservation) return;

  onChange();
}

//------------------------------------------------------------------------

void PlasticTool::onSetViewer() {
  Viewer *viewer = getViewer();
  if (viewer) {
    PlasticVisualSettings &pvs =
        viewer->visualSettings().m_plasticVisualSettings;
    pvs = m_pvs;

    // Force options if needed
    if (m_mode.getIndex() == RIGIDITY_IDX) pvs.m_drawRigidity = true;
  }
}

//------------------------------------------------------------------------

void PlasticTool::onActivate() {
  bool ret;
  ret = connect(TTool::m_application->getCurrentFrame(),
                SIGNAL(frameSwitched()), this, SLOT(onFrameSwitched())),
  assert(ret);
  ret = connect(TTool::m_application->getCurrentColumn(),
                SIGNAL(columnIndexSwitched()), this, SLOT(onColumnSwitched())),
  assert(ret);
  ret = connect(TTool::m_application->getCurrentXsheet(),
                SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged())),
  assert(ret);
  ret = connect(TTool::m_application->getCurrentXsheet(),
                SIGNAL(xsheetSwitched()), this, SLOT(onXsheetChanged())),
  assert(ret);

  onSetViewer();
  onColumnSwitched();
  onFrameSwitched();

  setActive(true);
}

//------------------------------------------------------------------------

void PlasticTool::onDeactivate() {
  setActive(false);

  bool ret;
  ret = disconnect(TTool::m_application->getCurrentFrame(),
                   SIGNAL(frameSwitched()), this, SLOT(onFrameSwitched())),
  assert(ret);
  ret =
      disconnect(TTool::m_application->getCurrentColumn(),
                 SIGNAL(columnIndexSwitched()), this, SLOT(onColumnSwitched())),
  assert(ret);
  ret = disconnect(TTool::m_application->getCurrentXsheet(),
                   SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged())),
  assert(ret);
  ret = disconnect(TTool::m_application->getCurrentXsheet(),
                   SIGNAL(xsheetSwitched()), this, SLOT(onXsheetChanged())),
  assert(ret);

  Viewer *viewer = getViewer();
  if (viewer)
    viewer->visualSettings().m_plasticVisualSettings = PlasticVisualSettings();

  m_sd = PlasticSkeletonDeformationP();
}

//------------------------------------------------------------------------

void PlasticTool::onEnter() {}

//------------------------------------------------------------------------

void PlasticTool::onLeave() {
  // Clear visualization vars
  m_pos    = TConsts::napd;
  m_svHigh = m_seHigh = -1;
  m_mvHigh = m_meHigh = MeshIndex();
}

//------------------------------------------------------------------------

void PlasticTool::onSelectionChanged() {
  SkVD *vd = 0;
  if (m_sd && m_svSel.hasSingleObject()) {
    int skelId = ::skeletonId();

    const PlasticSkeleton::vertex_type &vx =
        m_sd->skeleton(skelId)->vertex(m_svSel);

    m_vertexName.setValue(vx.name().toStdWString());
    m_interpolate.setValue(vx.m_interpolate);

    m_minAngle.setValue((vx.m_minAngle == -l_dmax)
                            ? L""
                            : QString::number(vx.m_minAngle).toStdWString());
    m_maxAngle.setValue((vx.m_maxAngle == l_dmax)
                            ? L""
                            : QString::number(vx.m_maxAngle).toStdWString());

    vd = m_sd->vertexDeformation(skelId, m_svSel);
  } else {
    m_vertexName.setValue(L"");
    m_interpolate.setValue(false);

    m_minAngle.setValue(L"");
    m_maxAngle.setValue(L"");
  }

  // Attach or detach relays depending on selected vertex's parameters
  m_soRelay.setParam(vd ? vd->m_params[SkVD::SO] : TDoubleParamP());

  if (vd && m_svSel.hasSingleObject() && m_svSel.objects().front() > 0) {
    m_distanceRelay.setParam(vd->m_params[SkVD::DISTANCE]);
    m_angleRelay.setParam(vd->m_params[SkVD::ANGLE]);
  } else {
    m_distanceRelay.setParam(TDoubleParamP());
    m_angleRelay.setParam(TDoubleParamP());
  }

  m_vertexName.notifyListeners();
  m_interpolate.notifyListeners();
  m_minAngle.notifyListeners();
  m_maxAngle.notifyListeners();

  m_distanceRelay.notifyListeners();
  m_angleRelay.notifyListeners();
  m_soRelay.notifyListeners();
}

//------------------------------------------------------------------------

void PlasticTool::enableCommands() {
  if (TSelection::getCurrent() == &m_svSel)
    m_svSel.enableCommand(this, MI_Clear,
                          &PlasticTool::deleteSelectedVertex_undo);
  else if (TSelection::getCurrent() == &m_meSel) {
    m_meSel.enableCommand(this, MI_Clear, &PlasticTool::collapseEdge_mesh_undo);
    m_meSel.enableCommand(this, MI_Insert, &PlasticTool::splitEdge_mesh_undo);
  }
}

//------------------------------------------------------------------------

void PlasticTool::setSkeletonSelection(const PlasticVertexSelection &vSel) {
  if (vSel.isEmpty()) {
    m_svSel.selectNone();
    m_svSel.makeNotCurrent();

    return;
  }

  assert(m_sd);

  m_svSel.skeletonId() = m_skelId;
  m_svSel.setObjects(vSel.objects());

  m_svSel.notifyView();
  m_svSel.makeCurrent();

  // Okay, the following is cheap - we have to update the Function Editor
  // (specifically)
  // since current vertex is shown in a special color. We know that the same
  // happens for
  // the current stage object, so... we'll attach there.
  TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(
      false);  // Carry on, you've seen nothing ;)
}

//------------------------------------------------------------------------

void PlasticTool::toggleSkeletonSelection(
    const PlasticVertexSelection &addition) {
  const std::vector<int> &storedIdxs = m_svSel.objects();
  const std::vector<int> &addedIdxs  = addition.objects();

  // Build new selection
  std::vector<int> selectedIdxs;

  if (m_svSel.contains(addition)) {
    std::set_difference(storedIdxs.begin(), storedIdxs.end(), addedIdxs.begin(),
                        addedIdxs.end(), std::back_inserter(selectedIdxs));
  } else {
    std::set_union(storedIdxs.begin(), storedIdxs.end(), addedIdxs.begin(),
                   addedIdxs.end(), std::back_inserter(selectedIdxs));
  }

  setSkeletonSelection(selectedIdxs);
}

//------------------------------------------------------------------------

void PlasticTool::clearSkeletonSelections() {
  m_svHigh = m_seHigh = -1;

  m_svSel.selectNone();
  m_svSel.makeNotCurrent();
}

//------------------------------------------------------------------------

PlasticVertexSelection PlasticTool::branchSelection(int vIdx) const {
  struct locals {
    static void addBranch(const PlasticSkeleton &skeleton, int v,
                          std::vector<int> &branch) {
      branch.push_back(v);

      const PlasticSkeletonVertex &vx = skeleton.vertex(v);

      PlasticSkeletonVertex::edges_const_iterator et, eEnd = vx.edgesEnd();
      for (et = vx.edgesBegin(); et != eEnd; ++et) {
        int child = skeleton.edge(*et).vertex(1);

        if (v != child)  // The edge to parent is in the list
          addBranch(skeleton, child,
                    branch);  // I wonder if it's ensured to be always at begin?
      }
    }
  };

  assert(skeleton());

  std::vector<int> selectedIdxs;
  locals::addBranch(*skeleton(), vIdx, selectedIdxs);

  return selectedIdxs;
}

//------------------------------------------------------------------------

void PlasticTool::copySkeleton() {
  if (!m_sd) return;

  const PlasticSkeletonP &skel = m_sd->skeleton(::skeletonId());
  if (!skel) return;

  // Copy a CLONE of currently addressed skeleton in the app clipboard
  QMimeData *data = new PlasticSkeletonPMime(new PlasticSkeleton(*skel));
  QApplication::clipboard()->setMimeData(data, QClipboard::Clipboard);
}

//------------------------------------------------------------------------

void PlasticTool::pasteSkeleton_undo() {
  const PlasticSkeletonPMime *data = dynamic_cast<const PlasticSkeletonPMime *>(
      QApplication::clipboard()->mimeData());
  if (!data) return;

  PlasticSkeletonP newSkeleton(new PlasticSkeleton(*data->m_skeleton));

  touchDeformation();
  assert(m_sd);

  int skelId                      = ::skeletonId();
  const PlasticSkeletonP &oldSkel = m_sd->skeleton(skelId);

  if (oldSkel && !oldSkel->empty()) {
    // In case there exists a not-empty skeleton, add a NEW skeleton
    addSkeleton_undo(newSkeleton);
  } else {
    TUndoManager *manager = TUndoManager::manager();
    manager->beginBlock();
    {
      removeSkeleton_undo(skelId);  // No problem - it's eventually empty
      addSkeleton_undo(skelId, newSkeleton.getPointer());
    }
    manager->endBlock();
  }
}

//------------------------------------------------------------------------

void PlasticTool::copyDeformation() {
  if (!m_sd) return;

  // Copy a reference to currently addressed skeleton in the app clipboard
  QMimeData *data = new SkDPMime(m_sd);
  QApplication::clipboard()->setMimeData(data, QClipboard::Clipboard);
}

//------------------------------------------------------------------------

void PlasticTool::pasteDeformation_undo() {
  const SkDPMime *data =
      dynamic_cast<const SkDPMime *>(QApplication::clipboard()->mimeData());
  if (!data) return;

  // Given a skeleton, attempt to assign it to the current stage object
  TStageObject *obj = ::stageObject();
  assert(obj);

  const PlasticSkeletonDeformationP &oldSd =
      obj->getPlasticSkeletonDeformation();
  if (oldSd) {
    // A skeleton already exists. Ask the user if it has to be replaced.
    bool replace = DVGui::MsgBox(tr("A group of skeletons already exists for "
                                    "current column. Replacing it will also "
                                    "substitute any existing vertex "
                                    "animation.\n\nDo you want to continue?"),
                                 tr("Ok"), tr("Cancel")) == 1;

    if (!replace) return;
  }

  // Clone the whole skeleton deformation (skeleton itself included)
  SkDP newSd(new PlasticSkeletonDeformation(*data->m_sd));

  // Insert the undo and perform the op
  TUndoManager::manager()->add(new PasteDeformationUndo(newSd));

  obj->setPlasticSkeletonDeformation(newSd);
  ::invalidateXsheet();
}

//------------------------------------------------------------------------

void PlasticTool::setKey() {
  assert(m_svSel.hasSingleObject());

  SkVD *vd     = m_sd->vertexDeformation(::skeletonId(), m_svSel);
  double frame = ::frame();

  if (vd->isFullKeyframe(frame))
    vd->deleteKeyframe(frame);
  else
    ::setKeyframe(vd, frame);
}

//------------------------------------------------------------------------

void PlasticTool::setGlobalKey() {
  struct locals {
    inline static bool isFullKeyframe(const SkDP &sd, double frame) {
      SkD::vd_iterator vdt, vdEnd;
      sd->vertexDeformations(vdt, vdEnd);

      for (; vdt != vdEnd; ++vdt)
        if (!(*vdt).second->isFullKeyframe(frame)) return false;

      return true;
    }
  };

  double frame = ::frame();

  if (locals::isFullKeyframe(m_sd, frame))
    m_sd->deleteKeyframe(frame);
  else
    ::setKeyframe(m_sd, frame);
}

//------------------------------------------------------------------------

void PlasticTool::setRestKey() {
  assert(m_svSel.hasSingleObject());

  SkVD *vd     = m_sd->vertexDeformation(::skeletonId(), m_svSel);
  double frame = ::frame();

  for (int p = 0; p != SkVD::PARAMS_COUNT; ++p)
    vd->m_params[p]->setValue(frame, vd->m_params[p]->getDefaultValue());
}

//------------------------------------------------------------------------

void PlasticTool::setGlobalRestKey() {
  double frame = ::frame();

  SkD::vd_iterator vdt, vdEnd;
  m_sd->vertexDeformations(vdt, vdEnd);

  for (; vdt != vdEnd; ++vdt) {
    SkVD *vd = (*vdt).second;

    for (int p = 0; p != SkVD::PARAMS_COUNT; ++p)
      vd->m_params[p]->setValue(frame, vd->m_params[p]->getDefaultValue());
  }
}

//------------------------------------------------------------------------

void PlasticTool::setKey_undo() { keyFunc_undo(&PlasticTool::setKey); }

//------------------------------------------------------------------------

void PlasticTool::setGlobalKey_undo() {
  keyFunc_undo(&PlasticTool::setGlobalKey);
}

//------------------------------------------------------------------------

void PlasticTool::setRestKey_undo() { keyFunc_undo(&PlasticTool::setRestKey); }

//------------------------------------------------------------------------

void PlasticTool::setGlobalRestKey_undo() {
  keyFunc_undo(&PlasticTool::setGlobalRestKey);
}

//------------------------------------------------------------------------

void PlasticTool::setVertexName(QString &name) {
  const PlasticSkeletonP &skeleton = this->skeleton();
  assert(skeleton && m_svSel.hasSingleObject() && m_svSel > 0);

  // Update the selected vertex's name
  while (!m_sd->skeleton(::skeletonId())->setVertexName(m_svSel, name))
    name += "_";

  m_vertexName.setValue(name.toStdWString());
  m_vertexName.notifyListeners();  // NOTE: This should NOT invoke this function
                                   // recursively

  // Re-store the deformed skeleton. This is necessary since any follow-up
  // vertex
  // manipulation must refer the correct vd name.
  m_deformedSkeleton.invalidate();

  PlasticDeformerStorage::instance()->invalidateSkeleton(
      m_sd.getPointer(), ::skeletonId(), PlasticDeformerStorage::NONE);
}

//------------------------------------------------------------------------

void PlasticTool::mouseMove(const TPointD &pos, const TMouseEvent &me) {
  // Discriminate mode
  switch (m_mode.getIndex()) {
  case MESH_IDX:
    mouseMove_mesh(pos, me);
    break;
  case BUILD_IDX:
    mouseMove_build(pos, me);
    break;
  case RIGIDITY_IDX:
    mouseMove_rigidity(pos, me);
    break;
  case ANIMATE_IDX:
    mouseMove_animate(pos, me);
    break;
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown(const TPointD &pos, const TMouseEvent &me) {
  switch (m_mode.getIndex()) {
  case MESH_IDX:
    leftButtonDown_mesh(pos, me);
    break;
  case BUILD_IDX:
    leftButtonDown_build(pos, me);
    break;
  case RIGIDITY_IDX:
    leftButtonDown_rigidity(pos, me);
    break;
  case ANIMATE_IDX:
    leftButtonDown_animate(pos, me);
    break;
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &me) {
  // Track dragging status
  m_dragged = true;

  switch (m_mode.getIndex()) {
  case MESH_IDX:
    leftButtonDrag_mesh(pos, me);
    break;
  case BUILD_IDX:
    leftButtonDrag_build(pos, me);
    break;
  case RIGIDITY_IDX:
    leftButtonDrag_rigidity(pos, me);
    break;
  case ANIMATE_IDX:
    leftButtonDrag_animate(pos, me);
    break;
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonUp(const TPointD &pos, const TMouseEvent &me) {
  switch (m_mode.getIndex()) {
  case MESH_IDX:
    leftButtonUp_mesh(pos, me);
    break;
  case BUILD_IDX:
    leftButtonUp_build(pos, me);
    break;
  case RIGIDITY_IDX:
    leftButtonUp_rigidity(pos, me);
    break;
  case ANIMATE_IDX:
    leftButtonUp_animate(pos, me);
    break;
  }

  m_pressedPos = TConsts::napd;
  m_pressedVxsPos.clear();
  m_dragged = false;
}

//------------------------------------------------------------------------

void PlasticTool::addContextMenuItems(QMenu *menu) {
  bool ret = true;

  // Add global actions
  if (m_sd && m_sd->skeleton(::skeletonId())) {
    QAction *copySkeleton = menu->addAction(tr("Copy Skeleton"));
    ret = ret && connect(copySkeleton, SIGNAL(triggered()), &l_plasticTool,
                         SLOT(copySkeleton()));
  }

  if (dynamic_cast<const PlasticSkeletonPMime *>(
          QApplication::clipboard()->mimeData())) {
    QAction *pasteSkeleton = menu->addAction(tr("Paste Skeleton"));
    ret = ret && connect(pasteSkeleton, SIGNAL(triggered()), &l_plasticTool,
                         SLOT(pasteSkeleton_undo()));
  }

  menu->addSeparator();  // Separate actions type

  // Add editing actions
  switch (m_mode.getIndex()) {
  case MESH_IDX:
    addContextMenuActions_mesh(menu);
    break;
  case BUILD_IDX:
    addContextMenuActions_build(menu);
    break;
  case RIGIDITY_IDX:
    addContextMenuActions_rigidity(menu);
    break;
  case ANIMATE_IDX:
    addContextMenuActions_animate(menu);
    break;
  }

  // Add view actions
  QAction *showMesh = menu->addAction(tr("Show Mesh"));
  showMesh->setCheckable(true);
  showMesh->setChecked(m_pvs.m_drawMeshesWireframe);

  ret = ret && connect(showMesh, SIGNAL(triggered(bool)), &l_plasticTool,
                       SLOT(onShowMeshToggled(bool)));

  QAction *showRigidity = menu->addAction(tr("Show Rigidity"));
  showRigidity->setCheckable(true);
  showRigidity->setChecked(m_pvs.m_drawRigidity);

  ret = ret && connect(showRigidity, SIGNAL(triggered(bool)), &l_plasticTool,
                       SLOT(onShowRigidityToggled(bool)));

  QAction *showSO = menu->addAction(tr("Show SO"));
  showSO->setCheckable(true);
  showSO->setChecked(m_pvs.m_drawSO);

  ret = ret && connect(showSO, SIGNAL(triggered(bool)), &l_plasticTool,
                       SLOT(onShowSOToggled(bool)));

  QAction *showSkeletonOS = menu->addAction(tr("Show Skeleton Onion Skin"));
  showSkeletonOS->setCheckable(true);
  showSkeletonOS->setChecked(m_showSkeletonOS);

  ret = ret && connect(showSkeletonOS, SIGNAL(triggered(bool)), &l_plasticTool,
                       SLOT(onShowSkelOSToggled(bool)));

  assert(ret);

  menu->addSeparator();  // Separate from common view options
}

//------------------------------------------------------------------------

void PlasticTool::reset() {
  // NOTE: This is an inherited virtual. Please leave it even if it's empty.
}

//------------------------------------------------------------------------

bool PlasticTool::onPropertyChanged(std::string propertyName) {
  struct locals {
    static bool alreadyContainsVertexName(const PlasticSkeleton &skel,
                                          const QString &name) {
      tcg::list<PlasticSkeletonVertex>::const_iterator vt,
          vEnd(skel.vertices().end());
      for (vt = skel.vertices().begin(); vt != vEnd; ++vt)
        if (vt->name() == name) return true;
      return false;
    }

    static int vdCount(const SkDP &sd, const QString &name) {
      SkD::vx_iterator vxBegin, vxEnd;
      sd->vdSkeletonVertices(name, vxBegin, vxEnd);

      return std::distance(vxBegin, vxEnd);
    }
  };  // locals

  if (propertyName == "mode") {
    switch (m_mode.getIndex()) {
    case MESH_IDX:
    case BUILD_IDX:
    case RIGIDITY_IDX:
      m_pvs.m_showOriginalColumn = xshColumn();
      break;
    case ANIMATE_IDX:
      m_pvs.m_showOriginalColumn = 0;

      if (m_svSel.objects().size() > 1) setSkeletonSelection(-1);

      storeDeformation();  // Rebuild deformed skeleton
      break;
    }

    m_mode.notifyListeners();  // You thought that was automatic, eh? BTW, this
                               // means
    // we're requesting toolbars to update options visibility

    onSetViewer();  // Store m_pvs in the viewer's visual settings
    invalidate();
  } else if (propertyName == "vertexName") {
    if (m_sd && m_svSel >= 0) {
      // Update the selected vertex's name
      QString newName(QString::fromStdWString(m_vertexName.getValue()));

      const PlasticSkeletonP skeleton = this->skeleton();
      assert(skeleton);

      const QString &oldName = skeleton->vertex(m_svSel).name();

      bool doRename = true;
      if (oldName != newName &&
          !locals::alreadyContainsVertexName(*skeleton, newName) &&
          m_sd->vertexDeformation(newName) &&
          locals::vdCount(m_sd, oldName) == 1)
        doRename =
            (DVGui::MsgBox(
                 tr("The previous vertex name will be discarded, and all "
                    "associated keys will be lost.\n\nDo you want to proceed?"),
                 QObject::tr("Ok"), QObject::tr("Cancel")) == 1);

      if (doRename) {
        TUndo *undo = new SetVertexNameUndo(m_svSel, newName);
        TUndoManager::manager()->add(undo);

        undo->redo();
      } else {
        m_vertexName.setValue(oldName.toStdWString());
        m_vertexName.notifyListeners();
      }
    }
  } else if (propertyName == "interpolate") {
    if (m_sd && m_svSel >= 0) {
      // Set interpolation property to the associated skeleton vertex
      int skelId = ::skeletonId();

      m_sd->skeleton(skelId)->vertex(m_svSel).m_interpolate =
          m_interpolate.getValue();

      m_interpolate.notifyListeners();  // NOTE: This should NOT invoke this
                                        // function recursively

      PlasticDeformerStorage::instance()->invalidateSkeleton(
          m_sd.getPointer(), skelId, PlasticDeformerStorage::ALL);
    }
  } else if (propertyName == "minAngle") {
    if (m_sd && m_svSel >= 0) {
      // Set maxAngle property to the associated skeleton vertex
      int skelId = ::skeletonId();

      bool ok;
      double value =
          QString::fromStdWString(m_minAngle.getValue()).toDouble(&ok);

      if (!ok) value = -l_dmax, m_minAngle.setValue(L"");

      m_sd->skeleton(skelId)->vertex(m_svSel).m_minAngle = value;
      if (m_mode.getIndex() == ANIMATE_IDX)
        deformedSkeleton().vertex(m_svSel).m_minAngle = value;

      m_minAngle.notifyListeners();  // NOTE: This should NOT invoke this
                                     // function recursively
    }
  } else if (propertyName == "maxAngle") {
    if (m_sd && m_svSel >= 0) {
      // Set maxAngle property to the associated skeleton vertex
      int skelId = ::skeletonId();

      bool ok;
      double value =
          QString::fromStdWString(m_maxAngle.getValue()).toDouble(&ok);

      if (!ok) value = l_dmax, m_maxAngle.setValue(L"");

      m_sd->skeleton(skelId)->vertex(m_svSel).m_maxAngle = value;
      if (m_mode.getIndex() == ANIMATE_IDX)
        deformedSkeleton().vertex(m_svSel).m_maxAngle = value;

      m_maxAngle.notifyListeners();  // NOTE: This should NOT invoke this
                                     // function recursively
    }
  }

  return true;
}

//------------------------------------------------------------------------

void PlasticTool::onShowMeshToggled(bool on) {
  m_pvs.m_drawMeshesWireframe = on;
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::onShowSOToggled(bool on) {
  m_pvs.m_drawSO = on;
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::onShowRigidityToggled(bool on) {
  m_pvs.m_drawRigidity = on;
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::onShowSkelOSToggled(bool on) {
  m_showSkeletonOS = on;
  invalidate();
}

//****************************************************************************************
//    Drawing functions
//****************************************************************************************

namespace PlasticToolLocals {

void drawSquare(const TPointD &pos, double radius) {
  glBegin(GL_LINE_LOOP);
  glVertex2d(pos.x - radius, pos.y - radius);
  glVertex2d(pos.x + radius, pos.y - radius);
  glVertex2d(pos.x + radius, pos.y + radius);
  glVertex2d(pos.x - radius, pos.y + radius);
  glEnd();
}

//------------------------------------------------------------------------

void drawFullSquare(const TPointD &pos, double radius) {
  glBegin(GL_QUADS);
  glVertex2d(pos.x - radius, pos.y - radius);
  glVertex2d(pos.x + radius, pos.y - radius);
  glVertex2d(pos.x + radius, pos.y + radius);
  glVertex2d(pos.x - radius, pos.y + radius);
  glEnd();
}

//------------------------------------------------------------------------

static void drawFilledSquare(const TPointD &pos, double radius) {
  glBegin(GL_QUADS);
  glVertex2d(pos.x - radius, pos.y - radius);
  glVertex2d(pos.x + radius, pos.y - radius);
  glVertex2d(pos.x + radius, pos.y + radius);
  glVertex2d(pos.x - radius, pos.y + radius);
  glEnd();
}

//------------------------------------------------------------------------

static void drawHandle(const TPointD &pos, double radius,
                       const TPixel32 &color) {
  glColor4ub(0, 0, 0, color.m);  // Black border
  glLineWidth(4.0f);
  drawSquare(pos, radius);

  glColor4ub(color.r, color.g, color.b, color.m);
  glLineWidth(2.0f);
  drawSquare(pos, radius);
}

//------------------------------------------------------------------------

static void drawFilledHandle(const TPointD &pos, double radius,
                             double pixelSize, const TPixel32 &color) {
  glColor4ub(0, 0, 0, color.m);
  drawFilledSquare(pos, radius + pixelSize);

  glColor4ub(color.r, color.g, color.b, color.m);
  drawFilledSquare(pos, radius);
}

//------------------------------------------------------------------------

static void drawText(const TPointD &pos, const QString &text,
                     double fontScale) {
  // Get the world-to-window affine
  double matrix[16];

  glGetDoublev(GL_MODELVIEW_MATRIX, matrix);
  TAffine worldToWindowAff(matrix[0], matrix[4], matrix[12], matrix[1],
                           matrix[5], matrix[13]);

  // Push the window reference
  glPushMatrix();
  glLoadIdentity();
  glScaled(fontScale, fontScale, 1.0);

  tglDrawText(TScale(1.0 / fontScale) * worldToWindowAff * pos,
              text.toStdWString());

  // Bottom-left fixed text version

  // double origin = 10.0 / fontScale
  // tglDrawText(TPointD(origin, origin), text.toStdWString());

  glPopMatrix();
}

}  // namespace PlasticToolLocals

//========================================================================

void PlasticTool::drawHighlights(const SkDP &sd,
                                 const PlasticSkeleton *skeleton,
                                 double pixelSize) {
  glColor3f(1.0f, 0.0f, 0.0f);  // Red
  glLineWidth(1.0f);

  // Vertex highlights
  if (m_svHigh >= 0) {
    double handleRadius = HIGHLIGHTED_HANDLE_SIZE * pixelSize;

    const PlasticSkeleton::vertex_type &vx = skeleton->vertex(m_svHigh);

    int hookNumber = sd->hookNumber(vx.name());
    assert(hookNumber >= 0);

    {
      glPushAttrib(GL_LINE_BIT);

      glEnable(GL_LINE_STIPPLE);
      glLineStipple(1, 0xCCCC);

      drawSquare(vx.P(), handleRadius);

      glPopAttrib();
    }

    drawText(vx.P() + TPointD(2.0 * handleRadius, 2.0 * handleRadius),
             QString("(%1) ").arg(hookNumber) + vx.name(), 1.7);
  } else if (m_seHigh >= 0) {
    // Draw a handle at the projection of current mouse position towards the
    // highlighted edge
    double handleRadius = HANDLE_SIZE * pixelSize;
    drawSquare(projection(*skeleton, m_seHigh, m_pos), handleRadius);
  }
}

//------------------------------------------------------------------------

void PlasticTool::drawSelections(const SkDP &sd,
                                 const PlasticSkeleton &skeleton,
                                 double pixelSize) {
  glColor3f(1.0f, 0.0f, 0.0f);  // Red
  glLineWidth(1.0f);

  double handleRadius = SELECTED_HANDLE_SIZE * pixelSize;

  if (!m_svSel.isEmpty()) {
    typedef PlasticVertexSelection::objects_container objects_container;
    const objects_container &vIdxs = m_svSel.objects();

    // Draw a handle square for each selected vertex
    objects_container::const_iterator vst, vsEnd = vIdxs.end();
    for (vst = vIdxs.begin(); vst != vsEnd; ++vst)
      drawSquare(skeleton.vertex(*vst).P(), handleRadius);

    // Draw vertex descriptions (only in the single selection case - to avoid
    // text pollution)
    if (vIdxs.size() == 1) {
      const PlasticSkeleton::vertex_type &vx = skeleton.vertex(m_svSel);

      int hookNumber = sd->hookNumber(vx.name());
      assert(hookNumber >= 0);

      drawText(vx.P() + TPointD(2.0 * handleRadius, 2.0 * handleRadius),
               QString("(%1) ").arg(hookNumber) + vx.name(), 1.7);
    }
  }
}

//------------------------------------------------------------------------

void PlasticTool::drawSkeleton(const PlasticSkeleton &skel, double pixelSize,
                               UCHAR alpha) {
  struct locals {
    inline static void drawLine(const TPointD &p0, const TPointD &p1) {
      glVertex2d(p0.x, p0.y);
      glVertex2d(p1.x, p1.y);
    }
  };  // locals

  const tcg::list<PlasticSkeleton::vertex_type> &vertices = skel.vertices();
  if (vertices.size() > 0) {
    // Draw edges
    {
      const tcg::list<PlasticSkeleton::edge_type> &edges = skel.edges();

      tcg::list<PlasticSkeleton::edge_type>::const_iterator et,
          eEnd(edges.end());

      glColor4ub(0, 0, 0, alpha);
      glLineWidth(4.0f);  // Black border

      glBegin(GL_LINES);
      {
        for (et = edges.begin(); et != eEnd; ++et)
          locals::drawLine(skel.vertex(et->vertex(0)).P(),
                           skel.vertex(et->vertex(1)).P());
      }
      glEnd();

      glColor4ub(250, 184, 70, alpha);
      glLineWidth(2.0f);  // Yellow/Orange-ish line center

      glBegin(GL_LINES);
      {
        for (et = edges.begin(); et != eEnd; ++et)
          locals::drawLine(skel.vertex(et->vertex(0)).P(),
                           skel.vertex(et->vertex(1)).P());
      }
      glEnd();
    }

    // Draw vertices
    {
      const TPixel32 magenta(255, 0, 255, alpha);
      const TPixel32 yellow(255, 255, 0, alpha);

      double handleRadius  = HANDLE_SIZE * pixelSize;
      float intHandleThick = 2.0f, extHandleThick = 4.0f;

      // Draw root
      drawFilledHandle(vertices.begin()->P(), handleRadius, pixelSize, magenta);

      // Draw remaining vertices
      tcg::list<PlasticSkeleton::vertex_type>::const_iterator vt(
          vertices.begin()),
          vEnd(vertices.end());
      if (vt != vEnd) {
        for (vt = ++vertices.begin(); vt != vEnd; ++vt)
          drawHandle(vt->P(), handleRadius,
                     vt->m_interpolate ? magenta : yellow);
      }
    }
  }
}

//------------------------------------------------------------------------

void PlasticTool::drawOnionSkinSkeletons_build(double pixelSize) {
  if (!(m_showSkeletonOS && m_sd)) return;

  const OnionSkinMask &os =
      TTool::getApplication()->getCurrentOnionSkin()->getOnionSkinMask();

  std::vector<int> osRows;
  int currentRow = ::row();

  os.getAll(currentRow, osRows);

  TStageObject *obj = ::stageObject();

  // Sieve osRows' associated skeleton ids first
  std::map<int, UCHAR> skelAlphas;

  int r, rCount = int(osRows.size());
  for (r = 0; r != rCount; ++r) {
    assert(osRows[r] != currentRow);

    double sdFrame = obj->paramsTime(double(osRows[r] - 1));
    int skelId     = m_sd->skeletonId(sdFrame);

    UCHAR &skelAlpha = skelAlphas[skelId];

    UCHAR alpha =
        255 -
        UCHAR(255.0 * OnionSkinMask::getOnionSkinFade(osRows[r] - currentRow));
    skelAlpha = std::max(skelAlpha, alpha);
  }

  std::map<int, UCHAR>::iterator st, sEnd(skelAlphas.end());
  for (st = skelAlphas.begin(); st != sEnd; ++st) {
    const PlasticSkeletonP &skel = m_sd->skeleton(st->first);
    drawSkeleton(*skel, pixelSize, st->second);
  }
}

//------------------------------------------------------------------------

void PlasticTool::drawOnionSkinSkeletons_animate(double pixelSize) {
  if (!(m_showSkeletonOS && m_sd)) return;

  const OnionSkinMask &os =
      TTool::getApplication()->getCurrentOnionSkin()->getOnionSkinMask();

  std::vector<int> osRows;
  int currentRow = ::row();

  os.getAll(currentRow, osRows);

  TStageObject *obj = ::stageObject();

  int r, rCount = int(osRows.size());
  for (r = 0; r != rCount; ++r) {
    assert(osRows[r] != currentRow);

    double sdFrame = obj->paramsTime(double(osRows[r] - 1));

    PlasticSkeleton skel;
    m_sd->storeDeformedSkeleton(m_sd->skeletonId(sdFrame), sdFrame, skel);

    UCHAR alpha =
        255 -
        255.0 * OnionSkinMask::getOnionSkinFade(abs(osRows[r] - currentRow));
    drawSkeleton(skel, pixelSize, alpha);
  }
}

//------------------------------------------------------------------------

void PlasticTool::drawAngleLimits(const SkDP &sd, int skelId, int v,
                                  double pixelSize) {
  struct {
    PlasticTool *m_this;

    void drawAnnulusArc(const TPointD &center, double angleStart,
                        double angleEnd, double radiusA, double radiusB,
                        double pixelSize) {
      double angleDelta = acos(1.0 - pixelSize / std::max(radiusA, radiusB)) *
                          ((angleStart <= angleEnd) ? 1.0 : -1.0);

      int a, aCount = tcg::numeric_ops::grow(
                 fabs((angleEnd - angleStart) / angleDelta));

      glBegin(GL_QUAD_STRIP);
      {
        for (a = 0; a != aCount; ++a) {
          double angle = angleStart + a * angleDelta;
          TPointD direction(cos(angle), sin(angle));

          tglVertex(center + radiusA * direction);
          tglVertex(center + radiusB * direction);
        }

        TPointD direction(cos(angleEnd), sin(angleEnd));

        tglVertex(center + radiusA * direction);
        tglVertex(center + radiusB * direction);
      }
      glEnd();
    }

    void drawLimit(const SkDP &sd, int skelId, int v, double angleLimit,
                   double pixelSize) {
      const PlasticSkeleton &skel    = *sd->skeleton(skelId);
      const PlasticSkeleton &defSkel = m_this->deformedSkeleton();

      const PlasticSkeletonVertex &vx    = skel.vertex(v);
      const PlasticSkeletonVertex &defVx = defSkel.vertex(v);

      int vParent = vx.parent();

      const PlasticSkeletonVertex &vxParent    = skel.vertex(vParent);
      const PlasticSkeletonVertex &defVxParent = defSkel.vertex(vParent);

      // Build directions
      int vGrandParent = vxParent.parent();

      TPointD dirFromParent(vx.P() - vxParent.P()), dirFromGrandParent(1, 0),
          dirFromDeformedGrandParent(1, 0);

      if (vGrandParent >= 0) {
        const PlasticSkeletonVertex &vxGrandParent = skel.vertex(vGrandParent),
                                    &defVxGrandParent =
                                        defSkel.vertex(vGrandParent);

        dirFromGrandParent         = vxParent.P() - vxGrandParent.P();
        dirFromDeformedGrandParent = defVxParent.P() - defVxGrandParent.P();
      }

      // Retrieve angular data
      double angleShift =
          sd->vertexDeformation(skelId, v)->m_params[SkVD::ANGLE]->getValue(
              ::frame());
      double defaultAngleValue =
          tcg::point_ops::angle(dirFromGrandParent, dirFromParent) * M_180_PI;

      // Convert to radians
      double currentBranchAngle_rad =
          tcg::point_ops::rad(dirFromDeformedGrandParent);

      double currentAngle_rad =
          currentBranchAngle_rad + (angleShift + defaultAngleValue) * M_PI_180;
      double limitDirection_rad =
          currentBranchAngle_rad + (angleLimit + defaultAngleValue) * M_PI_180;

      glColor4ub(0, 0, 255, 128);

      // Draw limit lines
      if (angleShift - 180.0 <= angleLimit &&
          angleLimit <= angleShift + 180.0) {
        TPointD limitDirection(cos(limitDirection_rad),
                               sin(limitDirection_rad));

        glBegin(GL_LINES);
        {
          tglVertex(defVxParent.P());
          tglVertex(defVxParent.P() + 1e4 * limitDirection);
        }
        glEnd();
      }

      // Draw limit annulus arc
      angleLimit = tcrop(angleLimit, angleShift - 180.0, angleShift + 180.0);
      limitDirection_rad =
          currentBranchAngle_rad + (angleLimit + defaultAngleValue) * M_PI_180;

      double radius = tcg::point_ops::dist(defVx.P(), defVxParent.P()) * 0.25;

      drawAnnulusArc(defVxParent.P(), limitDirection_rad, currentAngle_rad,
                     radius - 5.0 * pixelSize, radius + 5.0 * pixelSize,
                     pixelSize);
    }
  } locals = {this};

  // Dismiss no-ops
  const PlasticSkeletonP &skel = sd->skeleton(skelId);

  if (!skel || v < 0) return;

  // Ensure we're editing a vertex with an existing parent
  if (m_dragged) {
    const PlasticSkeletonVertex &vx = skel->vertex(v);

    int vParent = vx.parent();
    if (vParent >= 0) {
      // Draw angular limits
      if (vx.m_minAngle != -l_dmax)
        locals.drawLimit(sd, skelId, v, vx.m_minAngle, pixelSize);

      if (vx.m_maxAngle != l_dmax)
        locals.drawLimit(sd, skelId, v, vx.m_maxAngle, pixelSize);
    }
  }
}

//------------------------------------------------------------------------

void PlasticTool::draw() {
  glPushAttrib(GL_LINE_BIT | GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT);

  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);

  switch (m_mode.getIndex()) {
  case MESH_IDX:
    draw_mesh();
    break;
  case BUILD_IDX:
    draw_build();
    break;
  case RIGIDITY_IDX:
    draw_rigidity();
    break;
  case ANIMATE_IDX:
    draw_animate();
    break;
  };

  glPopAttrib();
}
