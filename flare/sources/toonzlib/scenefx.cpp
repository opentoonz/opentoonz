

// TnzBase includes
#include "tfxattributes.h"
#include "tfxutil.h"
#include "tmacrofx.h"
#include "toutputproperties.h"
#include "tparamcontainer.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/fxdag.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/dpiscale.h"
#include "toonz/tcamera.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/plasticdeformerfx.h"
#include "toonz/stage.h"
#include "toonz/preferences.h"
#include "ttzpimagefx.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/txshsoundtextlevel.h"

#include "../stdfx/motionawarebasefx.h"
#include "../stdfx/textawarebasefx.h"
#include "../stdfx/globalcontrollablefx.h"

#include "toonz/scenefx.h"

#include <QList>

/*
  TODO: Some parts of the following render-tree building procedure should be
  revised. In particular,
        there is scarce support for frame-shifting fxs, whenever the frame-shift
  can be resolved
        only during rendering (as is the case for ParticlesFx).
*/

//***************************************************************************************************
//    TimeShuffleFx  definition
//***************************************************************************************************

//! TimeShuffleFx is the rendering-tree equivalent of a sub-xsheet column.
/*!
  TimeShuffleFx is a special-purpose fx which is used in render-tree building
procedures
  to simulate the effect of a sub-xsheet.
\n\n
  A rendering tree is a fully expanded tree that mixes implicit xsheet nesting
with
  the explicit fxs dag <I> for a specific frame <\I>. Since the frame the tree
is developed from
  is fixed, a sub-xsheet can be seen as a <I> frame setter <\I> fx.
*/

class TimeShuffleFx final : public TRasterFx {
  FX_DECLARATION(TimeShuffleFx)

private:
  int m_frame;                 //!< Frame this fx redirects to
  TFxTimeRegion m_timeRegion;  //!< Input (outer) valid column frame range
  TRasterFxPort m_port;        //!< Input port
  TXshCellColumn *m_cellColumn;

public:
  TimeShuffleFx()
      : TRasterFx(), m_frame(0), m_timeRegion(), m_cellColumn(nullptr) {
    addInputPort("source", m_port);

    enableComputeInFloat(true);
  }
  ~TimeShuffleFx() {}

  TFx *clone(bool recursive = true) const override {
    TimeShuffleFx *fx = dynamic_cast<TimeShuffleFx *>(TFx::clone(recursive));
    assert(fx);

    fx->setFrame(m_frame);
    fx->setTimeRegion(getTimeRegion());
    fx->setCellColumn(m_cellColumn);

    return fx;
  }

  int getFrame() const { return m_frame; }
  void setFrame(int frame) { m_frame = frame; }

  void setTimeRegion(const TFxTimeRegion &timeRegion) {
    m_timeRegion = timeRegion;
  }
  TFxTimeRegion getTimeRegion() const override { return m_timeRegion; }

  void setCellColumn(TXshCellColumn *cellColumn) { m_cellColumn = cellColumn; }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  std::string getPluginId() const override { return std::string(); }

  int getLevelFrame(int frame) const {
    if (!m_cellColumn) return m_frame;
    TXshCell cell = m_cellColumn->getCell(tfloor(frame));
    assert(!cell.isEmpty());
    return cell.m_frameId.getNumber() - 1;
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_port.isConnected()) {
      tile.getRaster()->clear();
      return;
    }

    // Exchange frame with the stored one
    TRasterFxP(m_port.getFx())->compute(tile, getLevelFrame(frame), ri);
  }

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override {
    if (!m_port.isConnected()) return false;
    return TRasterFxP(m_port.getFx())
        ->doGetBBox(getLevelFrame(frame), bbox, info);
  }

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override {
    return TRasterFx::getAlias(getLevelFrame(frame), info);
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (m_port.isConnected())
      TRasterFxP(m_port.getFx())->dryCompute(rect, getLevelFrame(frame), info);
  }

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override {
    return tileIsLinear;
  }

private:
  // not implemented
  TimeShuffleFx(const TimeShuffleFx &);
  TimeShuffleFx &operator=(const TimeShuffleFx &);
};

FX_IDENTIFIER_IS_HIDDEN(TimeShuffleFx, "timeShuffleFx")

//***************************************************************************************************
//    AffineFx  definition
//***************************************************************************************************

//! AffineFx is a specialization of TGeometryFx which implements animated or
//! stage-controlled affines
/*!
  This specific implementation of TGeometryFx is needed to deal with those
  affines which are best
  \b not resolved during the rendering-tree expansion procedure.
*/

class AffineFx final : public TGeometryFx {
  FX_DECLARATION(AffineFx)

private:
  TXsheet *m_xsheet;            //!< Xsheet owning m_stageObject
  TStageObject *m_stageObject;  //!< The stage object this AffineFx refers to
  TRasterFxPort m_input;        //!< The input port

public:
  AffineFx() : m_xsheet(0), m_stageObject(0) {
    addInputPort("source", m_input);
    setName(L"AffineFx");
  }
  AffineFx(TXsheet *xsh, TStageObject *pegbar)
      : m_xsheet(xsh), m_stageObject(pegbar) {
    addInputPort("source", m_input);
    setName(L"AffineFx");
  }
  ~AffineFx() {}

  TFx *clone(bool recursive = true) const override {
    AffineFx *fx = dynamic_cast<AffineFx *>(TFx::clone(recursive));
    assert(fx);
    fx->m_stageObject = m_stageObject;
    fx->m_xsheet      = m_xsheet;
    return fx;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  TAffine getPlacement(double frame) override {
    TAffine objAff = m_stageObject->getPlacement(frame);

    double objZ        = m_stageObject->getZ(frame);
    double objNoScaleZ = m_stageObject->getGlobalNoScaleZ();

    TStageObjectId cameraId =
        m_xsheet->getStageObjectTree()->getCurrentCameraId();
    TStageObject *camera = m_xsheet->getStageObject(cameraId);
    TAffine cameraAff    = camera->getPlacement(frame);
    double cameraZ       = camera->getZ(frame);

    TAffine aff;
    bool isVisible = TStageObject::perspective(aff, cameraAff, cameraZ, objAff,
                                               objZ, objNoScaleZ);

    if (!isVisible)
      return TAffine();  // uh oh
    else
      return aff;
  }

  TAffine getParentPlacement(double frame) override {
    return m_stageObject->getPlacement(frame);
  }

  std::string getPluginId() const override { return std::string(); }

private:
  // not implemented
  AffineFx(const AffineFx &);
  AffineFx &operator=(const AffineFx &);
};

FX_IDENTIFIER_IS_HIDDEN(AffineFx, "affineFx")

//***************************************************************************************************
//    PlacedFx  definition
//***************************************************************************************************

//! PlacedFx is the enriched form of a TRasterFx during render-tree building.
class PlacedFx {
public:
  double m_z;         //!< Z value for this fx's column
  double m_so;        //!< Same as above, for stacking order
  int m_columnIndex;  //!< This fx's column index
  bool m_isPostXsheetNode;

  TFxP m_fx;      //!< The referenced fx
  TAffine m_aff;  //!<

  TFxPort *m_leftXsheetPort;

public:
  PlacedFx()
      : m_z(0)
      , m_so(0)
      , m_columnIndex(-1)
      , m_fx(0)
      , m_aff()
      , m_leftXsheetPort(0)
      , m_isPostXsheetNode(false) {}
  explicit PlacedFx(const TFxP &fx)
      : m_z(0)
      , m_so(0)
      , m_columnIndex(-1)
      , m_fx(fx)
      , m_aff()
      , m_leftXsheetPort(0)
      , m_isPostXsheetNode(false) {}

  bool operator<(const PlacedFx &pf) const {
    return (m_z < pf.m_z)     ? true
           : (m_z > pf.m_z)   ? false
           : (m_so < pf.m_so) ? true
           : (m_so > pf.m_so) ? false
                              : (m_columnIndex < pf.m_columnIndex);
  }

  TFxP makeFx() {
    return (!m_fx)                ? TFxP()
           : (m_aff == TAffine()) ? m_fx
                                  : TFxUtil::makeAffine(m_fx, m_aff);
  }
};

//***************************************************************************************************
//    Local namespace
//***************************************************************************************************

namespace {

TFxP timeShuffle(TFxP fx, int frame, TFxTimeRegion timeRegion,
                 TXshCellColumn *cellColumn) {
  TimeShuffleFx *timeShuffle = new TimeShuffleFx();

  timeShuffle->setFrame(frame);
  timeShuffle->setTimeRegion(timeRegion);
  timeShuffle->setCellColumn(cellColumn);
  if (!timeShuffle->connect("source", fx.getPointer()))
    assert(!"Could not connect ports!");

  return timeShuffle;
};

}  // namespace

//***************************************************************************************************
//    Column-related functions
//***************************************************************************************************

bool getColumnPlacement(TAffine &aff, TXsheet *xsh, double row, int col,
                        bool isPreview) {
  if (col < 0) return false;
  TStageObject *pegbar = xsh->getStageObject(TStageObjectId::ColumnId(col));
  TAffine objAff       = pegbar->getPlacement(row);
  double objZ          = pegbar->getZ(row);
  double noScaleZ      = pegbar->getGlobalNoScaleZ();

  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera = xsh->getStageObject(cameraId);
  TAffine cameraAff    = camera->getPlacement(row);
  double cameraZ       = camera->getZ(row);

  bool isVisible = TStageObject::perspective(aff, cameraAff, cameraZ, objAff,
                                             objZ, noScaleZ);

  return isVisible;
}

//-------------------------------------------------------------------

static bool getColumnPlacement(PlacedFx &pf, TXsheet *xsh, double row, int col,
                               bool isPreview) {
  if (col < 0) return false;
  TStageObject *pegbar = xsh->getStageObject(TStageObjectId::ColumnId(col));
  TAffine objAff       = pegbar->getPlacement(row);
  pf.m_z               = pegbar->getZ(row);
  pf.m_so              = pegbar->getSO(row);

  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera = xsh->getStageObject(cameraId);
  TAffine cameraAff    = camera->getPlacement(row);
  double cameraZ       = camera->getZ(row);

  bool isVisible =
      TStageObject::perspective(pf.m_aff, cameraAff, cameraZ, objAff, pf.m_z,
                                pegbar->getGlobalNoScaleZ());

  return isVisible;
}

//-------------------------------------------------------------------
/*-- Obtain the position of the Object --*/
static bool getStageObjectPlacement(TAffine &aff, TXsheet *xsh, double row,
                                    TStageObjectId &id, bool isPreview) {
  TStageObject *pegbar = xsh->getStageObjectTree()->getStageObject(id, false);
  if (!pegbar) return false;

  TAffine objAff  = pegbar->getPlacement(row);
  double objZ     = pegbar->getZ(row);
  double noScaleZ = pegbar->getGlobalNoScaleZ();

  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera = xsh->getStageObject(cameraId);
  TAffine cameraAff    = camera->getPlacement(row);
  double cameraZ       = camera->getZ(row);

  bool isVisible = TStageObject::perspective(aff, cameraAff, cameraZ, objAff,
                                             objZ, noScaleZ);

  return isVisible;
}

/*-- Get StageObjectId from type and index --*/
namespace {
TStageObjectId getMotionObjectId(MotionObjectType type, int index) {
  switch (type) {
  case OBJTYPE_OWN:
    return TStageObjectId::NoneId;
    break;
  case OBJTYPE_COLUMN:
    if (index == 0) return TStageObjectId::NoneId;
    return TStageObjectId::ColumnId(index - 1);
    break;
  case OBJTYPE_PEGBAR:
    if (index == 0) return TStageObjectId::NoneId;
    return TStageObjectId::PegbarId(index - 1);
    break;
  case OBJTYPE_TABLE:
    return TStageObjectId::TableId;
    break;
  case OBJTYPE_CAMERA:
    if (index == 0) return TStageObjectId::NoneId;
    return TStageObjectId::CameraId(index - 1);
    break;
  }

  return TStageObjectId::NoneId;
}
};  // namespace

//-------------------------------------------------------------------

static TPointD getColumnSpeed(TXsheet *xsh, double row, int col,
                              bool isPreview) {
  TAffine aff;
  TPointD a, b;
  const double h = 0.001;
  getColumnPlacement(aff, xsh, row + h, col, isPreview);

  /*-- Reacts to columns and camera movement --*/
  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera = xsh->getStageObject(cameraId);
  TAffine cameraAff    = camera->getPlacement(row + h);
  a                    = aff * TPointD(-cameraAff.a13, -cameraAff.a23);

  aff = TAffine();
  getColumnPlacement(aff, xsh, row - h, col, isPreview);

  cameraAff = camera->getPlacement(row - h);
  b         = aff * TPointD(-cameraAff.a13, -cameraAff.a23);

  return (b - a) * (0.5 / h);
}

//-------------------------------------------------------------------
/*-- Obtain the trajectory of an object by the difference from the reference
point objectId: The reference object for the move. NoneId for itself.
--*/
static QList<TPointD> getColumnMotionPoints(TXsheet *xsh, double row, int col,
                                            TStageObjectId &objectId,
                                            bool isPreview, double shutterStart,
                                            double shutterEnd,
                                            int traceResolution) {
  /*-- Returns an empty list if the previous and next frames are both zero. --*/
  if (shutterStart == 0.0 && shutterEnd == 0.0) return QList<TPointD>();

  /*-- 現在のカメラを得る --*/
  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera = xsh->getStageObject(cameraId);
  TAffine dpiAff       = getDpiAffine(camera->getCamera());

  /*-- 基準点の位置を得る --*/
  TAffine aff;

  /*-- objectIdが有効なものかどうかチェック --*/
  bool useOwnMotion = false;
  if (objectId == TStageObjectId::NoneId ||
      !xsh->getStageObjectTree()->getStageObject(objectId, false)) {
    getColumnPlacement(aff, xsh, row, col, isPreview);
    useOwnMotion = true;
  } else {
    getStageObjectPlacement(aff, xsh, row, objectId, isPreview);
  }

  TAffine cameraAff = camera->getPlacement(row);
  TPointD basePos =
      dpiAff.inv() * aff * TPointD(-cameraAff.a13, -cameraAff.a23);

  /*-- 結果を収めるリスト --*/
  QList<TPointD> points;
  /*-- 軌跡点間のフレーム間隔 --*/
  double dFrame = (shutterStart + shutterEnd) / (double)traceResolution;
  /*-- 各点の位置を、基準点との差分で格納していく --*/
  for (int i = 0; i <= traceResolution; i++) {
    /*-- 基準位置とのフレーム差 --*/
    double frameOffset = -shutterStart + dFrame * (double)i;
    /*-- 基準位置とのフレーム差が無ければ、基準点に一致するので差分は０を入れる
     * --*/
    if (frameOffset == 0.0) {
      points.append(TPointD(0.0, 0.0));
      continue;
    }

    double targetFrame = row + frameOffset;
    // Proper position cannot be obtained for frame = -1.0
    if (targetFrame == -1.0) targetFrame = -0.9999;

    /*-- 自分自身の動きを使うか、別オブジェクトの動きを使うか --*/
    if (useOwnMotion)
      getColumnPlacement(aff, xsh, targetFrame, col, isPreview);
    else
      getStageObjectPlacement(aff, xsh, targetFrame, objectId, isPreview);

    TAffine cameraAff = camera->getPlacement(targetFrame);
    TPointD tmpPos =
        dpiAff.inv() * aff * TPointD(-cameraAff.a13, -cameraAff.a23);

    /*-- 基準位置との差を記録 --*/
    points.append(tmpPos - basePos);
  }
  return points;
}

//-------------------------------------------------------------------
/*--
    フレーム前後のアフィン変換を得る
        objectId: 移動の参考にするオブジェクト。自分自身の場合はNoneId
--*/
static void getColumnMotionAffines(TAffine &aff_Before, TAffine &aff_After,
                                   TXsheet *xsh, double row, int col,
                                   TStageObjectId &objectId, bool isPreview,
                                   double shutterLength) {
  /*-- 現在のカメラを得る --*/
  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera = xsh->getStageObject(cameraId);
  TAffine dpiAff       = getDpiAffine(camera->getCamera());

  /*-- objectIdが有効なものかどうかチェック --*/
  bool useOwnMotion = false;
  if (objectId == TStageObjectId::NoneId ||
      !xsh->getStageObjectTree()->getStageObject(objectId, false)) {
    useOwnMotion = true;
  }

  /*-- 結果を収めるリスト --*/
  TAffine retAff[2];
  TAffine aff;

  /*-- 各点の位置を、基準点との差分で格納していく --*/
  for (int i = 0; i < 2; i++) {
    /*-- 基準位置とのフレーム差 --*/
    double frameOffset = (i == 0) ? -shutterLength : shutterLength;
    double targetFrame = row + frameOffset;
    // Proper position cannot be obtained for frame = -1.0
    if (targetFrame == -1.0) targetFrame = -0.9999;

    /*-- 自分自身の動きを使うか、別オブジェクトの動きを使うか --*/
    if (useOwnMotion)
      getColumnPlacement(aff, xsh, targetFrame, col, isPreview);
    else
      getStageObjectPlacement(aff, xsh, targetFrame, objectId, isPreview);

    TAffine cameraAff = camera->getPlacement(targetFrame);
    retAff[i]         = dpiAff.inv() * aff * cameraAff.inv();
  }

  aff_Before = retAff[0];
  aff_After  = retAff[1];
}

namespace {

QString getNoteText(TXsheet *xsh, double row, int col, int noteColumnIndex,
                    bool neighbor) {
  int colIndex;
  if (neighbor)
    colIndex = col - 1;
  else
    colIndex = noteColumnIndex;

  TXshColumn *column = xsh->getColumn(colIndex);
  if (!column || !column->getSoundTextColumn()) return QString();

  TXshCell cell = xsh->getCell(row, colIndex);
  if (cell.isEmpty() || !cell.getSoundTextLevel()) return QString();

  return cell.getSoundTextLevel()->getFrameText(cell.m_frameId.getNumber() - 1);
}
};  // namespace

//***************************************************************************************************
//    FxBuilder  definition
//***************************************************************************************************

class FxBuilder {
public:
  ToonzScene *m_scene;
  TXsheet *m_xsh;
  TAffine m_cameraAff;
  double m_cameraZ;
  double m_frame;
  int m_whichLevels;
  bool m_isPreview;
  bool m_expandXSheet;

  // in the makePF() methods m_particleDescendentCount>0 iff the TFx* is an
  // ancestor
  // (at least) of a particle Fx
  int m_particleDescendentCount;

  // fxid and pointer to the correspondent blend fx for the global
  // control. Boolean is the flag indicating that the makePF is just called
  // from the blend fx. If the flag is true then just compute the
  // global controlled Fx without inserting the same blend fx in order to
  // prevent infinite loop.
  QMap<std::wstring, QPair<TFxP, bool>> m_globalControlledFx;

public:
  FxBuilder(ToonzScene *scene, TXsheet *xsh, double frame, int whichLevels,
            bool isPreview = false, bool expandXSheet = true);

  TFxP buildFx();
  TFxP buildFx(const TFxP &root, BSFX_Transforms_Enum transforms);

  PlacedFx makePF(TLevelColumnFx *fx);
  PlacedFx makePF(TPaletteColumnFx *fx);
  PlacedFx makePF(TZeraryColumnFx *fx);
  PlacedFx makePF(TXsheetFx *fx);
  PlacedFx makePFfromUnaryFx(TFx *fx);
  PlacedFx makePFfromGenericFx(TFx *fx);
  PlacedFx makePF(TFx *fx);

  TFxP getFxWithColumnMovements(const PlacedFx &pf);

  bool addPlasticDeformerFx(PlacedFx &pf);
};

//===================================================================

FxBuilder::FxBuilder(ToonzScene *scene, TXsheet *xsh, double frame,
                     int whichLevels, bool isPreview, bool expandXSheet)
    : m_scene(scene)
    , m_xsh(xsh)
    , m_frame(frame)
    , m_whichLevels(whichLevels)
    , m_isPreview(isPreview)
    , m_expandXSheet(expandXSheet)
    , m_particleDescendentCount(0) {
  TStageObjectId cameraId;
  if (m_isPreview)
    cameraId = m_xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = m_xsh->getStageObjectTree()->getCurrentCameraId();

  TStageObject *camera = m_xsh->getStageObject(cameraId);
  m_cameraAff          = camera->getPlacement(m_frame);
  m_cameraZ            = camera->getZ(m_frame);
}

//-------------------------------------------------------------------

TFxP FxBuilder::buildFx() {
  TFx *outputFx = m_xsh->getFxDag()->getOutputFx(0);
  if (!outputFx || outputFx->getInputPortCount() != 1 ||
      outputFx->getInputPort(0)->getFx() == 0)
    return TFxP();

  outputFx->setName(L"OutputFx");

  assert(m_particleDescendentCount == 0);
  PlacedFx pf = makePF(outputFx->getInputPort(0)->getFx());
  assert(m_particleDescendentCount == 0);

  TAffine cameraFullAff = m_cameraAff * TScale((1000 + m_cameraZ) / 1000);
  return TFxUtil::makeAffine(pf.makeFx(), cameraFullAff.inv());
}

//-------------------------------------------------------------------

TFxP FxBuilder::buildFx(const TFxP &root, BSFX_Transforms_Enum transforms) {
  assert(m_particleDescendentCount == 0);
  PlacedFx pf = makePF(root.getPointer());
  assert(m_particleDescendentCount == 0);

  TFxP fx = (transforms & BSFX_COLUMN_TR) ? pf.makeFx() : pf.m_fx;
  if (transforms & BSFX_CAMERA_TR) {
    TAffine cameraFullAff = m_cameraAff * TScale((1000 + m_cameraZ) / 1000);
    fx                    = TFxUtil::makeAffine(fx, cameraFullAff.inv());
  }

  return fx;
}

//-------------------------------------------------------------------

TFxP FxBuilder::getFxWithColumnMovements(const PlacedFx &pf) {
  TFxP fx = pf.m_fx;
  if (!fx) return fx;

  if (pf.m_columnIndex == -1) return pf.m_fx;

  TStageObjectId id    = TStageObjectId::ColumnId(pf.m_columnIndex);
  TStageObject *pegbar = m_xsh->getStageObject(id);

  AffineFx *affFx = new AffineFx(m_xsh, pegbar);
  affFx->getInputPort(0)->setFx(fx.getPointer());

  return affFx;
}

//-------------------------------------------------------------------

bool FxBuilder::addPlasticDeformerFx(PlacedFx &pf) {
  TStageObject *obj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(pf.m_columnIndex));
  TStageObjectId parentId(obj->getParent());

  if (parentId.isColumn() && obj->getParentHandle()[0] != 'H') {
    const SkDP &sd =
        m_xsh->getStageObject(parentId)->getPlasticSkeletonDeformation();

    const TXshCell &parentCell = m_xsh->getCell(m_frame, parentId.getIndex());
    TXshSimpleLevel *parentSl  = parentCell.getSimpleLevel();

    if (sd && parentSl && (parentSl->getType() == MESH_XSHLEVEL)) {
      // Plastic Deformer case - add the corresponding fx,
      // absorb the dpi and local column placement affines

      PlasticDeformerFx *plasticFx = new PlasticDeformerFx;
      plasticFx->m_xsh             = m_xsh;
      plasticFx->m_col             = parentId.getIndex();
      plasticFx->m_texPlacement    = obj->getLocalPlacement(m_frame);

      if (!plasticFx->connect("source", pf.m_fx.getPointer()))
        assert(!"Could not connect ports!");

      pf.m_fx  = plasticFx;
      pf.m_aff = pf.m_aff * plasticFx->m_texPlacement.inv();

      return true;
    }
  }

  return false;
}

//-------------------------------------------------------------------

PlacedFx FxBuilder::makePF(TFx *fx) {
  if (!fx) return PlacedFx();

  if (TLevelColumnFx *lcfx = dynamic_cast<TLevelColumnFx *>(fx))
    return makePF(lcfx);
  else if (TPaletteColumnFx *pcfx = dynamic_cast<TPaletteColumnFx *>(fx))
    return makePF(pcfx);
  else if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx))
    return makePF(zcfx);
  else if (TXsheetFx *xsfx = dynamic_cast<TXsheetFx *>(fx))
    return makePF(xsfx);
  else if (fx->getInputPortCount() == 1)
    return makePFfromUnaryFx(fx);
  else
    return makePFfromGenericFx(fx);
}

//-------------------------------------------------------------------

PlacedFx FxBuilder::makePF(TXsheetFx *fx) {
  if (!m_expandXSheet)  // Xsheet expansion is typically blocked for render-tree
                        // building of post-xsheet fxs only.
  {
    PlacedFx ret(fx);
    ret.m_isPostXsheetNode = true;
    return ret;
  }

  // Expand the render-tree from terminal fxs
  TFxSet *fxs = m_xsh->getFxDag()->getTerminalFxs();
  int m       = fxs->getFxCount();
  if (m == 0) {
    PlacedFx ret;
    ret.m_isPostXsheetNode = true;
    return ret;
  }

  std::vector<PlacedFx> pfs(m);
  int i;
  for (i = 0; i < m; i++) {
    // Expand each terminal fx
    TFx *fx = fxs->getFx(i);
    assert(fx);
    pfs[i] = makePF(fx);  // Builds the sub-render-trees here
  }

  /*--
   * Xsheetに複数ノードが繋がっていた場合、PlacedFxの条件に従ってOverノードの付く順番を決める
   * --*/
  std::sort(pfs.begin(),
            pfs.end());  // Sort each terminal depending on Z/SO/Column index

  // Compose them in a cascade of overs (or affines 'leftXsheetPort' cases)
  TFxP currentFx =
      pfs[0].makeFx();  // Adds an NaAffineFx if pf.m_aff is not the identity
  for (i = 1; i < m; i++) {
    TFxP fx = pfs[i].makeFx();  // See above
    if (pfs[i].m_leftXsheetPort) {
      // LeftXsheetPort cases happen for those fxs like Add, Multiply, etc that
      // declare an xsheet-like input port.
      // That is, all terminal fxs below ours are attached COMPOSED to enter the
      // fx's leftXsheet input port.

      TFxP inputFx = currentFx;
      inputFx      = TFxUtil::makeAffine(inputFx, pfs[i].m_aff.inv());
      pfs[i].m_leftXsheetPort->setFx(inputFx.getPointer());
      currentFx = fx;
    } else {
      if (Preferences::instance()
              ->isShowRasterImagesDarkenBlendedInViewerEnabled())
        currentFx = TFxUtil::makeDarken(currentFx, fx);
      else
        currentFx = TFxUtil::makeOver(currentFx, fx);  // Common over case
    }
  }

  PlacedFx ret(currentFx);
  ret.m_isPostXsheetNode = true;
  return ret;
}

//-------------------------------------------------------------------

//! Creates and returns a PlacedFx for a TLevelColumnFx.
/*
  Fxs under a ParticlesFx node seem to have special treatment - that is,
  empty column cells are still attached to a not-empty PlacedFx.

  This must be a remnant of old Toonz code, that should no longer remain here -
  in fact, well, you can only extract an empty render from an empty column!
  So why bother?
*/
PlacedFx FxBuilder::makePF(TLevelColumnFx *lcfx) {
  assert(m_scene);
  assert(lcfx);
  assert(lcfx->getColumn());
  if (!lcfx || !lcfx->getColumn() || lcfx->getColumn()->isEmpty())
    return PlacedFx();

  if (!lcfx->getColumn()->isPreviewVisible())  // This is the 'eye' icon
                                               // property in the column header
                                               // interface
    return PlacedFx();  // that disables rendering of this particular column

  // Retrieve the corresponding xsheet cell to build up
  /*-- 現在のフレームのセルを取得 --*/
  TXshCell cell  = lcfx->getColumn()->getCell(tfloor(m_frame));
  int levelFrame = cell.m_frameId.getNumber() - 1;

  /*--  ParticlesFxに繋がっておらず、空セルの場合は 中身無しを返す --*/
  // -> even if the cell is empty, pass the affine infotmation of the column to
  // the subsequent nodes
  // if (m_particleDescendentCount == 0 && cell.isEmpty()) return PlacedFx();

  if (m_whichLevels == TOutputProperties::AnimatedOnly) {
    // In case only 'animated levels' are selected to be rendered, exclude all
    // 'backgrounds' - that is,
    // fullcolor levels...

    // Still, I wonder if this is still used in Toonz. I don't remember seeing
    // it anywhere :\ ?

    TXshLevel *xl = cell.m_level.getPointer();
    // if the cell is empty, use a level of the first occupied cell instead.
    if (!xl) {
      int r0, r1;
      if (lcfx->getColumn()->getRange(r0, r1) > 0)
        xl = lcfx->getColumn()->getCell(r0).m_level.getPointer();
    }

    /*-- ParticleFxのTextureポートに繋がっていない場合 --*/
    if (m_particleDescendentCount == 0) {
      if (!xl ||
          (xl->getType() != PLI_XSHLEVEL && xl->getType() != TZP_XSHLEVEL &&
           xl->getType() != CHILD_XSHLEVEL))
        return PlacedFx();
    }
    /*-- ParticleFxのTextureポートに繋がっている場合 --*/
    else {
      if (xl && xl->getType() != PLI_XSHLEVEL &&
          xl->getType() != TZP_XSHLEVEL && xl->getType() != CHILD_XSHLEVEL)
        return PlacedFx();
    }
  }

  // Build a PlacedFx for the column - start with the standard version for
  // common (image) levels
  PlacedFx pf;
  pf.m_columnIndex = lcfx->getColumn()->getIndex();
  // Build column placement
  bool columnVisible =
      getColumnPlacement(pf, m_xsh, m_frame, pf.m_columnIndex, m_isPreview);

  // if the cell is empty, only inherits its placement
  if ((m_particleDescendentCount == 0 && cell.isEmpty())) return pf;

  pf.m_fx = lcfx;

  /*-- subXsheetのとき、その中身もBuildFxを実行 --*/
  if (!cell.isEmpty() && cell.m_level->getChildLevel()) {
    // Treat the sub-xsheet case - build the sub-render-tree and reassign stuff
    // to pf
    TXsheet *xsh = cell.m_level->getChildLevel()->getXsheet();

    // Build the sub-render-tree
    FxBuilder builder(m_scene, xsh, levelFrame, m_whichLevels, m_isPreview);

    // Then, add the TimeShuffleFx
    pf.m_fx = timeShuffle(builder.buildFx(), levelFrame, lcfx->getTimeRegion(),
                          lcfx->getColumn());
    pf.m_fx->setIdentifier(lcfx->getIdentifier());
    pf.m_fx->getAttributes()->passiveCacheDataIdx() =
        lcfx->getAttributes()->passiveCacheDataIdx();

    // If the level should sustain a Plastic deformation, add the corresponding
    // fx
    addPlasticDeformerFx(pf);
  }

  if (columnVisible) {
    // Column is visible, alright
    TXshSimpleLevel *sl = cell.isEmpty() ? 0 : cell.m_level->getSimpleLevel();
    if (sl) {
      // If the level should sustain a Plastic deformation, add the
      // corresponding fx
      if (!addPlasticDeformerFx(pf)) {
        // Common (image) level case - add an NaAffineFx to compensate for the
        // image's dpi
        TAffine dpiAff = ::getDpiAffine(
            sl, cell.m_frameId, true);  // true stands for 'force full-sampling'
        pf.m_fx = TFxUtil::makeAffine(pf.m_fx, dpiAff);
        if (pf.m_fx) pf.m_fx->setName(L"LevelColumn AffineFx");
      }
    } else {
      // Okay, weird code ensues. This is what happens on non-common image
      // cases, which should be:

      //  1. Sub-Xsheet cases - and it really shouldn't
      //  2. Empty cell cases - with m_particles_blabla > 0; and again I don't
      //  get why on earth this should happen...

      // Please, note that (1) is a bug, although it happens when inserting a
      // common level and a sub-xsh
      // level in the same column...

      // when a cell not exists, there is no way to keep the dpi of the image!
      // in this case it is kept the dpi of the first cell not empty in the
      // column!
      /*--
       * 空セルのとき、Dpiアフィン変換には、その素材が入っている一番上のセルのものを使う
       * --*/
      TXshLevelColumn *column = lcfx->getColumn();

      int r0, r1;
      if (column->getRange(r0, r1) > 0)
        sl = column->getCell(r0).m_level->getSimpleLevel();
      if (sl) {
        TAffine dpiAff =
            ::getDpiAffine(sl, column->getCell(r0).m_frameId, true);
        pf.m_fx = TFxUtil::makeAffine(pf.m_fx, dpiAff);
      }
    }

    // Apply column's color filter and semi-transparency for rendering
    TXshLevelColumn *column = lcfx->getColumn();
    if (m_scene->getProperties()->isColumnColorFilterOnRenderEnabled() &&
        (column->getColorFilterId() != 0 ||  // None
         (column->isCamstandVisible() && column->getOpacity() != 255))) {
      TPixel32 colorScale = m_scene->getProperties()->getColorFilterColor(
          column->getColorFilterId());
      if (colorScale != TPixel::Black || column->getOpacity() != 255) {
        colorScale.m = (typename TPixel32::Channel)((int)colorScale.m *
                                                    (int)column->getOpacity() /
                                                    TPixel32::maxChannelValue);
        pf.m_fx      = TFxUtil::makeColumnColorFilter(pf.m_fx, colorScale);
      }
    }

    return pf;
  } else
    return PlacedFx();
}

//-------------------------------------------------------------------

PlacedFx FxBuilder::makePF(TPaletteColumnFx *pcfx) {
  assert(pcfx);
  assert(pcfx->getColumn());
  if (!pcfx->getColumn()->isPreviewVisible()) return PlacedFx();

  TXshCell cell = pcfx->getColumn()->getCell(tfloor(m_frame));
  if (cell.isEmpty()) return PlacedFx();

  PlacedFx pf;
  pf.m_columnIndex = pcfx->getColumn()->getIndex();
  pf.m_fx          = pcfx;

  return pf;
}

//-------------------------------------------------------------------

PlacedFx FxBuilder::makePF(TZeraryColumnFx *zcfx) {
  assert(zcfx);
  assert(zcfx->getColumn());

  if (!zcfx->getColumn()->isPreviewVisible())  // ...
    return PlacedFx();

  if (!zcfx->getAttributes()->isEnabled())  // ...
    return PlacedFx();

  TFx *fx = zcfx->getZeraryFx();
  if (!fx || !fx->getAttributes()->isEnabled())  // ... Perhaps these shouldn't
                                                 // be tested altogether? Only 1
                                                 // truly works !
    return PlacedFx();

  TXshCell cell = zcfx->getColumn()->getCell(tfloor(m_frame));

  // Build
  PlacedFx pf;
  pf.m_columnIndex = zcfx->getColumn()->getIndex();

  // if the cell is empty, only inherits its placement
  if (cell.isEmpty()) {
    // Add the column placement NaAffineFx
    if (!getColumnPlacement(pf, m_xsh, m_frame, pf.m_columnIndex, m_isPreview))
      return PlacedFx();
    return pf;
  }

  // set m_fx only when the current cell is not empty
  pf.m_fx =
      fx->clone(false);  // Detach the fx with a clone. Why? It's typically done
                         // to build fx connections in the render-tree freely.
                         // Here, it's used just for particles, I guess...

  // Deal with input sub-trees
  for (int i = 0; i < fx->getInputPortCount(); ++i) {
    // Note that only particles should end up here, currently
    if (TFxP inputFx = fx->getInputPort(i)->getFx()) {
      PlacedFx inputPF;

      // if the effect is a particle fx, it is necessary to consider also empty
      // cells
      // this causes a connection with the effect and a level also with empty
      // cells.
      if (fx->getFxType() == "STD_particlesFx" ||
          fx->getFxType() == "STD_iwa_TiledParticlesFx" ||
          fx->getFxType() == "STD_tiledParticlesFx") {
        m_particleDescendentCount++;
        inputPF = makePF(inputFx.getPointer());
        m_particleDescendentCount--;
      } else
        inputPF = makePF(inputFx.getPointer());

      inputFx = getFxWithColumnMovements(inputPF);
      if (!inputFx) continue;

      inputFx = TFxUtil::makeAffine(inputFx, pf.m_aff.inv());
      if (!pf.m_fx->connect(pf.m_fx->getInputPortName(i), inputFx.getPointer()))
        assert(!"Could not connect ports!");
    }
  }

  if (pf.m_fx->getFxType() == "STD_iwa_TextFx") {
    TextAwareBaseFx *textFx =
        dynamic_cast<TextAwareBaseFx *>(pf.m_fx.getPointer());
    if (textFx && textFx->getSourceType() != TextAwareBaseFx::INPUT_TEXT) {
      int noteColumnIndex = textFx->getNoteColumnIndex();
      bool getNeighbor =
          (textFx->getSourceType() == TextAwareBaseFx::NEARBY_COLUMN);
      textFx->setNoteLevelStr(getNoteText(m_xsh, m_frame, pf.m_columnIndex,
                                          noteColumnIndex, getNeighbor));
    }
  }
  if (pf.m_fx->getAttributes()->isSpeedAware()) {
    MotionAwareAffineFx *maafx =
        dynamic_cast<MotionAwareAffineFx *>(pf.m_fx.getPointer());
    if (maafx) {
      double shutterLength    = maafx->getShutterLength()->getValue(m_frame);
      MotionObjectType type   = maafx->getMotionObjectType();
      int index               = maafx->getMotionObjectIndex()->getValue();
      TStageObjectId objectId = getMotionObjectId(type, index);
      TAffine aff_Before, aff_After;
      getColumnMotionAffines(aff_Before, aff_After, m_xsh, m_frame,
                             pf.m_columnIndex, objectId, m_isPreview,
                             shutterLength);
      pf.m_fx->getAttributes()->setMotionAffines(aff_Before, aff_After);
    }
  }

  // Add the column placement NaAffineFx
  if (getColumnPlacement(pf, m_xsh, m_frame, pf.m_columnIndex, m_isPreview))
    return pf;
  else
    return PlacedFx();
}

//-------------------------------------------------------------------

PlacedFx FxBuilder::makePFfromUnaryFx(TFx *fx) {
  assert(!dynamic_cast<TLevelColumnFx *>(fx));
  assert(!dynamic_cast<TZeraryColumnFx *>(fx));
  assert(fx->getInputPortCount() == 1);

  TFx *inputFx = fx->getInputPort(0)->getFx();
  if (!inputFx) return PlacedFx();

  // global controllable fx
  if (fx->getAttributes()->hasGlobalControl()) {
    if (!m_globalControlledFx.contains(fx->getFxId())) {
      GlobalControllableFx *gcFx = dynamic_cast<GlobalControllableFx *>(fx);
      double val                 = gcFx->getGrobalControlValue(m_frame);
      if (val < 1.0) {
        // insert cross dissolve fx and mix with the input fx
        TFxP blendFx = TFx::create("blendFx");
        blendFx->connect("Source1", fx);
        blendFx->connect("Source2", inputFx);
        // set the global intensity value to the cross dissolve fx
        dynamic_cast<TDoubleParam *>(blendFx->getParams()->getParam("value"))
            ->setDefaultValue(val * 100.0);
        m_globalControlledFx.insert(fx->getFxId(), {blendFx, true});
        return makePF(blendFx.getPointer());
      }
    } else if (m_globalControlledFx.value(fx->getFxId()).second)
      m_globalControlledFx[fx->getFxId()].second = false;
    else {
      m_globalControlledFx[fx->getFxId()].second = true;
      return makePF(
          m_globalControlledFx.value(fx->getFxId()).first.getPointer());
    }
  }

  PlacedFx pf = makePF(inputFx);  // Build sub-render-tree
  if (pf.m_columnIndex < 0 && !pf.m_isPostXsheetNode) return PlacedFx();
  // inherit the column placement even if the current cell is empty
  if (!pf.m_fx) return pf;

  if (fx->getAttributes()->isEnabled()) {
    // Fx is enabled, so insert it in the render-tree

    // Clone this fx necessary
    if (pf.m_fx.getPointer() != inputFx ||  // As in an earlier makePF, clone
                                            // whenever input connections have
                                            // changed
        fx->getAttributes()->isSpeedAware() ||  // In the 'speedAware' case,
                                                // we'll alter the fx's
                                                // attributes (see below)
        dynamic_cast<TMacroFx *>(fx))  // As for macros... I'm not sure. Not
                                       // even who wrote this *understood*
    // why - it just solved a bug  X( . Investigate!
    {
      fx = fx->clone(false);
      if (!fx->connect(fx->getInputPortName(0), pf.m_fx.getPointer()))
        assert(!"Could not connect ports!");
    }

    pf.m_fx = fx;

    if (fx->getAttributes()->isSpeedAware()) {
      /*-- スピードでなく、軌跡を取得する場合 --*/
      MotionAwareBaseFx *mabfx = dynamic_cast<MotionAwareBaseFx *>(fx);
      if (mabfx) {
        double shutterStart = mabfx->getShutterStart()->getValue(m_frame);
        double shutterEnd   = mabfx->getShutterEnd()->getValue(m_frame);
        int traceResolution = mabfx->getTraceResolution()->getValue();
        /*-- 移動の参考にするオブジェクトの取得。自分自身の場合はNoneId --*/
        MotionObjectType type   = mabfx->getMotionObjectType();
        int index               = mabfx->getMotionObjectIndex()->getValue();
        TStageObjectId objectId = getMotionObjectId(type, index);
        fx->getAttributes()->setMotionPoints(getColumnMotionPoints(
            m_xsh, m_frame, pf.m_columnIndex, objectId, m_isPreview,
            shutterStart, shutterEnd, traceResolution));
      } else {
        TPointD speed =
            getColumnSpeed(m_xsh, m_frame, pf.m_columnIndex, m_isPreview);
        fx->getAttributes()->setSpeed(speed);
      }
    }
  }

  return pf;
}

//-------------------------------------------------------------------

PlacedFx FxBuilder::makePFfromGenericFx(TFx *fx) {
  assert(!dynamic_cast<TLevelColumnFx *>(fx));
  assert(!dynamic_cast<TZeraryColumnFx *>(fx));

  PlacedFx pf;

  if (!fx->getAttributes()->isEnabled()) {
    if (fx->getInputPortCount() == 0) return PlacedFx();

    TFxP inputFx = fx->getInputPort(fx->getPreferredInputPort())->getFx();
    if (inputFx) return makePF(inputFx.getPointer());

    return pf;
  }

  // global controllable fx
  if (fx->getAttributes()->hasGlobalControl()) {
    if (!m_globalControlledFx.contains(fx->getFxId())) {
      GlobalControllableFx *gcFx = dynamic_cast<GlobalControllableFx *>(fx);
      double val                 = gcFx->getGrobalControlValue(m_frame);
      if (val < 1.0) {
        TFxP inputFx = fx->getInputPort(fx->getPreferredInputPort())->getFx();
        if (!inputFx) return pf;
        // insert cross dissolve fx and mix with the input fx
        TFxP blendFx = TFx::create("blendFx");
        blendFx->connect("Source1", fx);
        blendFx->connect("Source2", inputFx.getPointer());
        m_globalControlledFx.insert(fx->getFxId(), {blendFx, true});
        // set the global intensity value to the cross dissolve fx
        dynamic_cast<TDoubleParam *>(blendFx->getParams()->getParam("value"))
            ->setDefaultValue(val * 100.0);
        return makePF(blendFx.getPointer());
      }
    } else if (m_globalControlledFx.value(fx->getFxId()).second)
      m_globalControlledFx[fx->getFxId()].second = false;
    else {
      m_globalControlledFx[fx->getFxId()].second = true;
      return makePF(
          m_globalControlledFx.value(fx->getFxId()).first.getPointer());
    }
  }

  // Multi-input fxs are always cloned - since at least one of its input ports
  // will have an NaAffineFx
  // injected just before its actual input fx.
  pf.m_fx = fx->clone(false);

  bool firstInput = true;

  int m = fx->getInputPortCount();
  for (int i = 0; i < m; ++i) {
    if (TFxP inputFx = fx->getInputPort(i)->getFx()) {
      PlacedFx inputPF;
      if (fx->getFxType() == "STD_iwa_FlowPaintBrushFx") {
        m_particleDescendentCount++;
        inputPF = makePF(inputFx.getPointer());
        m_particleDescendentCount--;
      } else
        inputPF = makePF(inputFx.getPointer());
      inputFx = inputPF.m_fx;
      // check the column index instead of inputFx
      // so that the firstly-found input column always inherits
      // its placement even if the current cell is empty.
      if (inputPF.m_columnIndex < 0 && !inputPF.m_isPostXsheetNode) continue;

      if (firstInput) {
        firstInput = false;

        // The first found input PlacedFx carries its placement infos up
        pf.m_aff              = inputPF.m_aff;
        pf.m_columnIndex      = inputPF.m_columnIndex;
        pf.m_z                = inputPF.m_z;
        pf.m_so               = inputPF.m_so;
        pf.m_isPostXsheetNode = inputPF.m_isPostXsheetNode;

        /*-- 軌跡を取得するBinaryFxの場合 --*/
        if (pf.m_fx->getAttributes()->isSpeedAware()) {
          MotionAwareBaseFx *mabfx =
              dynamic_cast<MotionAwareBaseFx *>(pf.m_fx.getPointer());
          if (mabfx) {
            double shutterStart = mabfx->getShutterStart()->getValue(m_frame);
            double shutterEnd   = mabfx->getShutterEnd()->getValue(m_frame);
            int traceResolution = mabfx->getTraceResolution()->getValue();
            /*-- 移動の参考にするオブジェクトの取得。自分自身の場合はNoneId --*/
            MotionObjectType type   = mabfx->getMotionObjectType();
            int index               = mabfx->getMotionObjectIndex()->getValue();
            TStageObjectId objectId = getMotionObjectId(type, index);
            pf.m_fx->getAttributes()->setMotionPoints(getColumnMotionPoints(
                m_xsh, m_frame, pf.m_columnIndex, objectId, m_isPreview,
                shutterStart, shutterEnd, traceResolution));
          }
        }

        if (!inputFx) continue;

      } else if (!inputFx)
        continue;
      else {
        // The follow-ups traduce their PlacedFx::m_aff into an NaAffineFx,
        // instead
        inputFx = getFxWithColumnMovements(inputPF);
        inputFx = TFxUtil::makeAffine(inputFx, pf.m_aff.inv());
      }

      if (!pf.m_fx->connect(pf.m_fx->getInputPortName(i), inputFx.getPointer()))
        assert(!"Could not connect ports!");
    }
  }

  // The xsheet-like input port is activated and brought upwards whenever it is
  // both
  // specified by the fx, and there is no input fx attached to it.
  if (pf.m_fx->getXsheetPort() && pf.m_fx->getXsheetPort()->getFx() == 0)
    pf.m_leftXsheetPort = pf.m_fx->getXsheetPort();

  return pf;
}

//***************************************************************************************************
//    Exported  Render-Tree building  functions
//***************************************************************************************************

TFxP buildSceneFx(ToonzScene *scene, TXsheet *xsh, double row, int whichLevels,
                  int shrink, bool isPreview) {
  FxBuilder builder(scene, xsh, row, whichLevels, isPreview);
  TFxP fx = builder.buildFx();
  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *cameraPegbar = xsh->getStageObject(cameraId);
  assert(cameraPegbar);
  TCamera *camera = cameraPegbar->getCamera();
  assert(camera);

  TAffine aff = getDpiAffine(camera).inv();
  if (shrink > 1) {
    double fac = 0.5 * (1.0 / shrink - 1.0);
    aff = TTranslation(fac * camera->getRes().lx, fac * camera->getRes().ly) *
          TScale(1.0 / shrink) * aff;
  }

  fx = TFxUtil::makeAffine(fx, aff);
  if (fx) fx->setName(L"CameraDPI and Shrink NAffineFx");

  fx = TFxUtil::makeOver(
      TFxUtil::makeColorCard(scene->getProperties()->getBgColor()), fx);
  return fx;
}

//===================================================================

TFxP buildSceneFx(ToonzScene *scene, TXsheet *xsh, double row, int shrink,
                  bool isPreview) {
  int whichLevels =
      scene->getProperties()->getOutputProperties()->getWhichLevels();
  return buildSceneFx(scene, xsh, row, whichLevels, shrink, isPreview);
}

//===================================================================

TFxP buildSceneFx(ToonzScene *scene, double row, int shrink, bool isPreview) {
  return buildSceneFx(scene, scene->getXsheet(), row, shrink, isPreview);
}

//===================================================================

TFxP buildSceneFx(ToonzScene *scene, TXsheet *xsh, double row, const TFxP &root,
                  bool isPreview) {
  int whichLevels =
      scene->getProperties()->getOutputProperties()->getWhichLevels();
  FxBuilder builder(scene, xsh, row, whichLevels, isPreview);
  return builder.buildFx(root, BSFX_NO_TR);
}

//===================================================================

TFxP buildSceneFx(ToonzScene *scene, double row, const TFxP &root,
                  bool isPreview) {
  return buildSceneFx(scene, scene->getXsheet(), row, root, isPreview);
}

//===================================================================

//! Similar to buildSceneFx(ToonzScene *scene, double row, const TFxP &root,
//! bool isPreview) method, build the sceneFx
//! adding also camera transformations. Used for Preview Fx function.
DVAPI TFxP buildPartialSceneFx(ToonzScene *scene, double row, const TFxP &root,
                               int shrink, bool isPreview) {
  int whichLevels =
      scene->getProperties()->getOutputProperties()->getWhichLevels();
  FxBuilder builder(scene, scene->getXsheet(), row, whichLevels, isPreview);
  TFxP fx = builder.buildFx(
      root, BSFX_Transforms_Enum(BSFX_CAMERA_TR | BSFX_COLUMN_TR));

  TXsheet *xsh = scene->getXsheet();
  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *cameraPegbar = xsh->getStageObject(cameraId);
  assert(cameraPegbar);
  TCamera *camera = cameraPegbar->getCamera();
  assert(camera);

  TAffine aff = getDpiAffine(camera).inv();
  if (shrink > 1) {
    double fac = 0.5 * (1.0 / shrink - 1.0);

    aff = TTranslation(fac * camera->getRes().lx, fac * camera->getRes().ly) *
          TScale(1.0 / shrink) * aff;
  }

  fx = TFxUtil::makeAffine(fx, aff);
  return fx;
}

//===================================================================

DVAPI TFxP buildPartialSceneFx(ToonzScene *scene, TXsheet *xsheet, double row,
                               const TFxP &root, int shrink, bool isPreview) {
  int whichLevels =
      scene->getProperties()->getOutputProperties()->getWhichLevels();
  FxBuilder builder(scene, xsheet, row, whichLevels, isPreview);
  TFxP fx = builder.buildFx(
      root, BSFX_Transforms_Enum(BSFX_CAMERA_TR | BSFX_COLUMN_TR));

  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsheet->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
  TStageObject *cameraPegbar = xsheet->getStageObject(cameraId);
  assert(cameraPegbar);
  TCamera *camera = cameraPegbar->getCamera();
  assert(camera);

  TAffine aff = getDpiAffine(camera).inv();
  if (shrink > 1) {
    double fac = 0.5 * (1.0 / shrink - 1.0);

    aff = TTranslation(fac * camera->getRes().lx, fac * camera->getRes().ly) *
          TScale(1.0 / shrink) * aff;
  }

  fx = TFxUtil::makeAffine(fx, aff);
  return fx;
}

//===================================================================

/*!
  Builds the post-rendering fxs tree - that is, all fxs between the xsheet node
  and
  current output node.

  This function can be used to isolate global post-processing fxs that typically
  do not
  contribute to scene compositing. When encountered, the xsheet node is \a not
  xpanded - it must be replaced manually.
*/
DVAPI TFxP buildPostSceneFx(ToonzScene *scene, double frame, int shrink,
                            bool isPreview) {
  // NOTE: Should whichLevels access output AND PREVIEW settings?
  int whichLevels =
      scene->getProperties()->getOutputProperties()->getWhichLevels();

  TXsheet *xsh = scene->getXsheet();
  if (!xsh) xsh = scene->getXsheet();

  // Do not expand the xsheet node
  FxBuilder builder(scene, xsh, frame, whichLevels, isPreview, false);

  TFxP fx = builder.buildFx();

  TStageObjectId cameraId;
  if (isPreview)
    cameraId = xsh->getStageObjectTree()->getCurrentPreviewCameraId();
  else
    cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *cameraPegbar = xsh->getStageObject(cameraId);
  assert(cameraPegbar);
  TCamera *camera = cameraPegbar->getCamera();
  assert(camera);

  TAffine aff = getDpiAffine(camera).inv();

  if (shrink > 1) {
    double fac = 0.5 * (1.0 / shrink - 1.0);
    aff = TTranslation(fac * camera->getRes().lx, fac * camera->getRes().ly) *
          TScale(1.0 / shrink) * aff;
  }

  if (!aff.isIdentity()) fx = TFxUtil::makeAffine(fx, aff);

  return fx;
}

//===================================================================

DVAPI TFxP buildSceneFx(ToonzScene *scene, double frame, TXsheet *xsh,
                        const TFxP &root, BSFX_Transforms_Enum transforms,
                        bool isPreview, int whichLevels, int shrink) {
  // NOTE: Should whichLevels access output AND PREVIEW settings?
  if (whichLevels == -1)
    whichLevels =
        scene->getProperties()->getOutputProperties()->getWhichLevels();

  if (!xsh) xsh = scene->getXsheet();

  FxBuilder builder(scene, xsh, frame, whichLevels, isPreview);

  TFxP fx = root ? builder.buildFx(root, transforms) : builder.buildFx();

  TStageObjectId cameraId =
      isPreview ? xsh->getStageObjectTree()->getCurrentPreviewCameraId()
                : xsh->getStageObjectTree()->getCurrentCameraId();

  TStageObject *cameraPegbar = xsh->getStageObject(cameraId);
  assert(cameraPegbar);

  TCamera *camera = cameraPegbar->getCamera();
  assert(camera);

  TAffine aff;
  if (transforms & BSFX_CAMERA_DPI_TR) aff = getDpiAffine(camera).inv();

  if (shrink > 1) {
    double fac = 0.5 * (1.0 / shrink - 1.0);
    aff = TTranslation(fac * camera->getRes().lx, fac * camera->getRes().ly) *
          TScale(1.0 / shrink) * aff;
  }

  if (!aff.isIdentity()) fx = TFxUtil::makeAffine(fx, aff);

  return fx;
}
