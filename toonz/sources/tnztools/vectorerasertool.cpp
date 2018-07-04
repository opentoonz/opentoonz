

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tool.h"
#include "tools/cursors.h"

// TnzQt includes
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/strokegenerator.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage2.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tmathutil.h"
#include "tundo.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "tgl.h"
#include "tinbetween.h"
#include "drawutil.h"

// Qt includes
#include <QCoreApplication>  // For Qt translation support

using namespace ToolUtils;

TEnv::DoubleVar EraseVectorSize("InknpaintEraseVectorSize", 10);
TEnv::StringVar EraseVectorType("InknpaintEraseVectorType", "Normal");
TEnv::IntVar EraseVectorSelective("InknpaintEraseVectorSelective", 0);
TEnv::IntVar EraseVectorInvert("InknpaintEraseVectorInvert", 0);
TEnv::IntVar EraseVectorRange("InknpaintEraseVectorRange", 0);

//=============================================================================
// Eraser Tool
//=============================================================================

namespace {

#define NORMAL_ERASE L"Normal"
#define RECT_ERASE L"Rectangular"
#define FREEHAND_ERASE L"Freehand"
#define POLYLINE_ERASE L"Polyline"

//-----------------------------------------------------------------------------

const double minDistance2 = 16.0;  // 4 pixel

//-----------------------------------------------------------------------------

void mapToVector(const std::map<int, VIStroke *> &theMap,
                 std::vector<int> &theVect) {
  assert(theMap.size() == theVect.size());
  std::map<int, VIStroke *>::const_iterator it = theMap.begin();
  UINT i = 0;
  for (; it != theMap.end(); ++it) {
    theVect[i++] = it->first;
  }
}

//-----------------------------------------------------------------------------

class UndoEraser final : public ToolUtils::TToolUndo {
  std::vector<TFilledRegionInf> m_oldFillInformation, m_newFillInformation;

  int m_row;
  int m_column;

public:
  std::map<int, VIStroke *> m_originalStrokes;
  std::map<int, VIStroke *> m_newStrokes;

  UndoEraser(TXshSimpleLevel *level, const TFrameId &frameId)
      : ToolUtils::TToolUndo(level, frameId) {
    TVectorImageP image = level->getFrame(m_frameId, true);
    if (!image) return;
    TTool::Application *app = TTool::getApplication();
    if (app) {
      m_row    = app->getCurrentFrame()->getFrame();
      m_column = app->getCurrentColumn()->getColumnIndex();
    }
    ImageUtils::getFillingInformationInArea(image, m_oldFillInformation,
                                            image->getBBox());
  }

  void onAdd() override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(!!image);
    if (!image) return;
    ImageUtils::getFillingInformationInArea(image, m_newFillInformation,
                                            image->getBBox());
  }

  ~UndoEraser() {
    std::map<int, VIStroke *>::const_iterator it;
    for (it = m_originalStrokes.begin(); it != m_originalStrokes.end(); ++it)
      deleteVIStroke(it->second);
    for (it = m_newStrokes.begin(); it != m_newStrokes.end(); ++it)
      deleteVIStroke(it->second);
  }

  void addOldStroke(int index, VIStroke *stroke) {
    VIStroke *s = cloneVIStroke(stroke);
    m_originalStrokes.insert(std::map<int, VIStroke *>::value_type(index, s));
  }

  void addNewStroke(int index, VIStroke *stroke) {
    VIStroke *s = cloneVIStroke(stroke);
    m_newStrokes.insert(std::map<int, VIStroke *>::value_type(index, s));
  }

  void undo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TFrameId currentFid;
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentColumn()->setColumnIndex(m_column);
      app->getCurrentFrame()->setFrame(m_row);
      currentFid = TFrameId(m_row + 1);
    } else {
      app->getCurrentFrame()->setFid(m_frameId);
      currentFid = m_frameId;
    }

    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(image);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    std::vector<int> newStrokeIndex(m_newStrokes.size());
    mapToVector(m_newStrokes, newStrokeIndex);

    image->removeStrokes(newStrokeIndex, true, false);

    std::map<int, VIStroke *>::const_iterator it = m_originalStrokes.begin();
    UINT i = 0;
    VIStroke *s;
    for (; it != m_originalStrokes.end(); ++it) {
      s = cloneVIStroke(it->second);
      image->insertStrokeAt(s, it->first);
    }

    if (image->isComputedRegionAlmostOnce())
      image->findRegions();  // in futuro togliere. Serve perche' la
                             // removeStrokes, se gli si dice
    // di non calcolare le regioni, e' piu' veloce ma poi chrash tutto

    UINT size = m_oldFillInformation.size();
    if (!size) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
      return;
    }

    TRegion *reg;
    i = 0;
    for (; i < size; i++) {
      reg = image->getRegion(m_oldFillInformation[i].m_regionId);
      assert(reg);
      if (reg) reg->setStyle(m_oldFillInformation[i].m_styleId);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TFrameId currentFid;
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentColumn()->setColumnIndex(m_column);
      app->getCurrentFrame()->setFrame(m_row);
      currentFid = TFrameId(m_row + 1);
    } else {
      app->getCurrentFrame()->setFid(m_frameId);
      currentFid = m_frameId;
    }
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(image);
    if (!image) return;

    QMutexLocker lock(image->getMutex());
    std::vector<int> oldStrokeIndex(m_originalStrokes.size());
    mapToVector(m_originalStrokes, oldStrokeIndex);

    image->removeStrokes(oldStrokeIndex, true, false);

    std::map<int, VIStroke *>::const_iterator it = m_newStrokes.begin();
    UINT i = 0;
    VIStroke *s;
    for (; it != m_newStrokes.end(); ++it) {
      s = cloneVIStroke(it->second);
      image->insertStrokeAt(s, it->first);
    }

    if (image->isComputedRegionAlmostOnce())
      image->findRegions();  // in futuro togliere. Serve perche' la
                             // removeStrokes, se gli si dice
    // di non calcolare le regioni, e' piu' veloce ma poi chrash tutto

    UINT size = m_newFillInformation.size();
    if (!size) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
      return;
    }

    TRegion *reg;
    i = 0;
    for (; i < size; i++) {
      reg = image->getRegion(m_newFillInformation[i].m_regionId);
      assert(reg);
      if (reg) reg->setStyle(m_newFillInformation[i].m_styleId);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           (m_oldFillInformation.capacity() + m_newFillInformation.capacity()) *
               sizeof(TFilledRegionInf) +
           500;
  }

  QString getToolName() override { return QString("Vector Eraser Tool"); }

  int getHistoryType() override { return HistoryType::EraserTool; }
};

}  // namespace

//=============================================================================
//  EraserTool class declaration
//-----------------------------------------------------------------------------

class EraserTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(EraserTool)

public:
  EraserTool();
  ~EraserTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void draw() override;

  void startErase(
      TVectorImageP vi,
      const TPointD &pos);  //, const TImageLocation &imageLocation);
  void erase(TVectorImageP vi, const TPointD &pos);
  void erase(TVectorImageP vi,
             TRectD &rect);  //, const TImageLocation &imageLocation);

  void stopErase(TVectorImageP vi);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  bool onPropertyChanged(std::string propertyName) override;
  void onEnter() override;
  void onLeave() override;
  void onActivate() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getCursorId() const override { return ToolCursor::EraserCursor; }
  void onImageChanged() override;

  /*-- ドラッグ中にツールが切り替わった場合、Eraseの終了処理を行う --*/
  void onDeactivate() override;

private:
  TPropertyGroup m_prop;

  TEnumProperty m_eraseType;
  TDoubleProperty m_toolSize;
  TBoolProperty m_selective;
  TBoolProperty m_invertOption;
  TBoolProperty m_multi;

  double m_pointSize, m_distance2;

  TPointD m_mousePos,  //!< Current mouse position.
      m_oldMousePos,   //!< Previous mouse position.
      m_brushPos,      //!< Position the brush will be painted at.
      m_firstPos;      //!< Either The first point inserted either in m_track or
                       //! m_polyline
  //!  (depending on selected erase mode).
  UndoEraser *m_undo;
  std::vector<int> m_indexes;

  TRectD m_selectingRect, m_firstRect;

  TFrameId m_firstFrameId, m_veryFirstFrameId;

  TXshSimpleLevelP m_level;
  std::pair<int, int> m_currCell;

  StrokeGenerator m_track;  //!< Lazo selection generator.

  std::vector<TPointD> m_polyline;  //!< Polyline points.

  TStroke *m_stroke;  //!< Stores the stroke generated by m_track.
  TStroke
      *m_firstStroke;  //!< Stores the first stroke in the "frame range" case.
  TImageP m_activeImage = NULL;  // needed if an frame is changed mid-erase
  double m_thick;

  bool m_firstTime, m_active, m_firstFrameSelected;

private:
  void resetMulti();

  void updateTranslation() override;

  // Metodi per disegnare la linea della modalita' Freehand
  void startFreehand(const TPointD &pos);
  void freehandDrag(const TPointD &pos);
  void closeFreehand(const TPointD &pos);

  // Metodi per disegnare la linea della modalita' Polyline
  void addPointPolyline(const TPointD &pos);
  void closePolyline(const TPointD &pos);

  void eraseRegion(const TVectorImageP vi, TStroke *stroke);

  void multiEraseRect(TFrameId firstFrameId, TFrameId lastFrameId,
                      TRectD firstRect, TRectD lastRect, bool invert);
  void doMultiErase(TFrameId &firstFrameId, TFrameId &lastFrameId,
                    const TStroke *firstStroke, const TStroke *lastStroke);
  void doErase(double t, const TXshSimpleLevelP &sl, const TFrameId &fid,
               const TVectorImageP &firstImage, const TVectorImageP &lastImage);
  void multiEreserRegion(TStroke *stroke, const TMouseEvent &e);

} eraserTool;

//=============================================================================
//  EraserTool implemention
//-----------------------------------------------------------------------------

EraserTool::EraserTool()
    : TTool("T_Eraser")
    , m_eraseType("Type:")             // "W_ToolOptions_Erasetype"
    , m_toolSize("Size:", 1, 100, 10)  // "W_ToolOptions_EraserToolSize"
    , m_selective("Selective", false)  // "W_ToolOptions_Selective"
    , m_invertOption("Invert", false)  // "W_ToolOptions_Invert"
    , m_multi("Frame Range", false)    // "W_ToolOptions_FrameRange"
    , m_pointSize(-1)
    , m_undo(0)
    , m_currCell(-1, -1)
    , m_stroke(0)
    , m_thick(5)
    , m_active(false)
    , m_firstTime(true) {
  bind(TTool::VectorImage);

  m_prop.bind(m_toolSize);
  m_prop.bind(m_eraseType);
  m_eraseType.addValue(NORMAL_ERASE);
  m_eraseType.addValue(RECT_ERASE);
  m_eraseType.addValue(FREEHAND_ERASE);
  m_eraseType.addValue(POLYLINE_ERASE);
  m_prop.bind(m_selective);
  m_prop.bind(m_invertOption);
  m_prop.bind(m_multi);

  m_selective.setId("Selective");
  m_invertOption.setId("Invert");
  m_multi.setId("FrameRange");
  m_eraseType.setId("Type");
}

//-----------------------------------------------------------------------------

EraserTool::~EraserTool() {
  if (m_stroke) delete m_stroke;

  if (m_firstStroke) delete m_firstStroke;
}

//-----------------------------------------------------------------------------

void EraserTool::updateTranslation() {
  m_toolSize.setQStringName(tr("Size:"));
  m_selective.setQStringName(tr("Selective"));
  m_invertOption.setQStringName(tr("Invert"));
  m_multi.setQStringName(tr("Frame Range"));
  m_eraseType.setQStringName(tr("Type:"));
  m_eraseType.setItemUIName(NORMAL_ERASE, tr("Normal"));
  m_eraseType.setItemUIName(RECT_ERASE, tr("Rectangular"));
  m_eraseType.setItemUIName(FREEHAND_ERASE, tr("Freehand"));
  m_eraseType.setItemUIName(POLYLINE_ERASE, tr("Polyline"));
}

//-----------------------------------------------------------------------------

void EraserTool::draw() {
  if (m_pointSize <= 0) return;

  double pixelSize2 = getPixelSize() * getPixelSize();
  m_thick           = pixelSize2 / 2.0;

  TImageP image(getImage(false));
  TVectorImageP vi = image;
  if (vi) {
    bool blackBg = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg;
    if (m_eraseType.getValue() == RECT_ERASE) {
      TPixel color = blackBg ? TPixel32::White : TPixel32::Red;
      if (m_multi.getValue() && m_firstFrameSelected)
        drawRect(m_firstRect, color, 0x3F33, true);

      if (m_active || (m_multi.getValue() && !m_firstFrameSelected))
        drawRect(m_selectingRect, color, 0xFFFF, true);
    }
    if (m_eraseType.getValue() == NORMAL_ERASE) {
      // If toggled off, don't draw brush outline
      if (!Preferences::instance()->isCursorOutlineEnabled()) return;

      tglColor(TPixel32(255, 0, 255));
      tglDrawCircle(m_brushPos, m_pointSize);
    }
    if ((m_eraseType.getValue() == FREEHAND_ERASE ||
         m_eraseType.getValue() == POLYLINE_ERASE) &&
        m_multi.getValue() && m_firstStroke) {
      TPixel color = blackBg ? TPixel32::White : TPixel32::Red;
      tglColor(color);
      glPushAttrib(GL_ALL_ATTRIB_BITS);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
      if (m_firstFrameSelected) {
        glLineStipple(1, 0x3F33);
        glEnable(GL_LINE_STIPPLE);
      }
      drawStrokeCenterline(*m_firstStroke, 1);
      glPopAttrib();
    }
    if (m_eraseType.getValue() == POLYLINE_ERASE && !m_polyline.empty()) {
      TPixel color = blackBg ? TPixel32::White : TPixel32::Black;
      tglColor(color);
      tglDrawCircle(m_polyline[0], 2);
      glBegin(GL_LINE_STRIP);
      for (UINT i = 0; i < m_polyline.size(); i++) tglVertex(m_polyline[i]);
      tglVertex(m_mousePos);
      glEnd();
    } else if (m_eraseType.getValue() == FREEHAND_ERASE && !m_track.isEmpty()) {
      TPixel color = blackBg ? TPixel32::White : TPixel32::Black;
      tglColor(color);
      glPushMatrix();
      m_track.drawAllFragments();
      glPopMatrix();
    }
  }
}

//-----------------------------------------------------------------------------

void EraserTool::resetMulti() {
  m_firstFrameSelected = false;
  m_firstRect.empty();

  m_selectingRect.empty();
  TTool::Application *application = TTool::getApplication();
  if (!application) return;

  m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
  m_level = application->getCurrentLevel()->getLevel()
                ? application->getCurrentLevel()->getLevel()->getSimpleLevel()
                : 0;

  if (m_firstStroke) {
    delete m_firstStroke;
    m_firstStroke = 0;
  }
}

//-----------------------------------------------------------------------------

void EraserTool::startErase(
    TVectorImageP vi,
    const TPointD &pos)  //, const TImageLocation &imageLocation)
{
  UINT size = vi->getStrokeCount();
  m_indexes.resize(size);
  for (UINT i = 0; i < size; i++) m_indexes[i] = i;

  if (m_undo) delete m_undo;
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo        = new UndoEraser(level, getCurrentFid());
  m_oldMousePos = pos;
  m_distance2   = minDistance2 * getPixelSize() * getPixelSize();
  erase(vi, pos);
}

//-----------------------------------------------------------------------------

void EraserTool::erase(TVectorImageP vi, const TPointD &pos) {
  std::vector<int>::iterator it = m_indexes.begin();
  m_distance2 += tdistance2(m_oldMousePos, pos);

  if (m_distance2 < minDistance2 * getPixelSize() * getPixelSize()) return;

  m_distance2   = 0;
  m_oldMousePos = pos;

  // quadrato circoscritto alla circonferenza
  TRectD circumscribedSquare(pos.x - m_pointSize, pos.y - m_pointSize,
                             pos.x + m_pointSize, pos.y + m_pointSize);
  if (!circumscribedSquare.overlaps(vi->getBBox())) {
    invalidate();
    return;
  }

  std::vector<double> intersections;
  std::vector<DoublePair> sortedWRanges;

  std::vector<TStroke *> splitStrokes;
  double rectEdge_2 = m_pointSize * M_SQRT1_2;

  // quadrato iscritto nella circonferenza
  TRectD enrolledSquare(pos.x - rectEdge_2, pos.y - rectEdge_2,
                        pos.x + rectEdge_2, pos.y + rectEdge_2);

  UINT i            = 0;
  double pointSize2 = sq(m_pointSize);

  std::vector<int> oneStrokeIndex(1);
  int index = TTool::getApplication()->getCurrentLevelStyleIndex();
  QMutexLocker lock(vi->getMutex());
  while (i < vi->getStrokeCount()) {
    assert(it != m_indexes.end());

    TStroke *oldStroke = vi->getStroke(i);
    if (!vi->inCurrentGroup(i) ||
        (m_selective.getValue() && oldStroke->getStyle() != index)) {
      i++;
      it++;
      continue;
    }

    TRectD strokeBBox = oldStroke->getBBox();

    if (!circumscribedSquare.overlaps(strokeBBox)) {  // stroke all'esterno del
                                                      // quadrato circoscritto
                                                      // alla circonferenxa
      i++;
      it++;
      continue;
    }

    if (enrolledSquare.contains(strokeBBox)) {  // stroke tutta contenuta nel
                                                // quadrato iscritto nella
                                                // circonferenza
      if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));
      oneStrokeIndex[0] = i;
      vi->removeStrokes(oneStrokeIndex, true, true);

      it = m_indexes.erase(it);
      continue;
    }

    splitStrokes.clear();
    intersections.clear();
    /*int intersNum=*/intersect(*oldStroke, pos, m_pointSize, intersections);

    /*
#ifdef _DEBUG
if(intersections.size()==2 && intersections[0]==intersections[1])
{
intersections.clear();
intersect( *oldStroke, pos, m_pointSize, intersections );
}
#endif
*/

    if (intersections.empty()) {
      // BASTEREBBE CONTROLLARE UN SOLO PUNTO PERCHE' SE LA
      // STROKE NON INTERSECA IL CERCHIO E CONTENTIENE ALMENO
      // UN PUNTO, LA CONTIENE TUTTA. MA SICCOME NON MI FIDO
      // DELLA INTERSECT, NE CONTROLLO UN PAIO E AVVANTAGGIO (CON L' AND)
      // IL NON CONTENIMENTO, DATO CHE E' MEGLIO CANCELLARE UNA COSA IN
      // MENO, CHE UNA IN PIU'
      if (tdistance2(oldStroke->getPoint(0), pos) < pointSize2 &&
          tdistance2(oldStroke->getPoint(1), pos) <
              pointSize2) {  // stroke tutta contenuta nella circonferenxa
        if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));
        oneStrokeIndex[0] = i;
        vi->removeStrokes(oneStrokeIndex, true, true);
        it = m_indexes.erase(it);
      } else {  // non colpita
        i++;
        it++;
      }
      continue;
    }

    //------------------------ almeno un'intersezione
    //---------------------------------------------------------

    if (intersections.size() == 1) {
      if (oldStroke->isSelfLoop()) {  // una sola intersezione di una stroke
                                      // chiusa con un cerchio dovrebbe accadere
        // solo in caso di sfioramento, quindi faccio finta di nulla
        i++;
        it++;
        continue;
      }

      if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));

      double w0            = intersections[0];
      TThickPoint hitPoint = oldStroke->getThickPoint(w0);
      int chunck;
      double t;
      oldStroke->getChunkAndT(w0, chunck, t);

      int chunckIndex;
      double w1;
      if (tdistance2(oldStroke->getPoint(0), pos) < pointSize2) {
        chunckIndex = oldStroke->getChunkCount() - 1;
        w1          = 1;
      } else {
        chunckIndex = 0;
        w1          = 0;
      }

      UINT cI;
      std::vector<TThickPoint> points;
      if (w1 == 0) {
        for (cI = chunckIndex; cI < (UINT)chunck; cI++) {
          points.push_back(oldStroke->getChunk(cI)->getThickP0());
          points.push_back(oldStroke->getChunk(cI)->getThickP1());
        }

        TThickQuadratic t1, t2;
        oldStroke->getChunk(chunck)->split(t, t1, t2);
        points.push_back(t1.getThickP0());
        points.push_back(t1.getThickP1());
        points.push_back(hitPoint);
      } else {
        TThickQuadratic t1, t2;
        oldStroke->getChunk(chunck)->split(t, t1, t2);
        points.push_back(hitPoint);
        points.push_back(t2.getThickP1());
        points.push_back(t2.getThickP2());

        for (cI = chunck + 1; cI <= (UINT)chunckIndex; cI++) {
          points.push_back(oldStroke->getChunk(cI)->getThickP1());
          points.push_back(oldStroke->getChunk(cI)->getThickP2());
        }
      }

      oldStroke->reshape(&(points[0]), points.size());

      vi->notifyChangedStrokes(i);  // per adesso cosi', pero' e' lento

      *it = -1;
      i++;
      it++;
      continue;
    }

    //---------- piu'
    // intersezioni--------------------------------------------------------

    if (intersections.size() & 1 &&
        oldStroke->isSelfLoop()) {  // non dovrebbe mai accadere
      assert(0);
      i++;
      it++;
      continue;
    }

    if (intersections.size() == 2 &&
        intersections[0] == intersections[1]) {  // solo sfiorata
      i++;
      it++;
      continue;
    }

    UINT oldStrokeSize = vi->getStrokeCount();

    if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));

    sortedWRanges.clear();

    if (tdistance2(vi->getStroke(i)->getPoint(0), pos) > pointSize2) {
      if (intersections[0] == 0.0)
        intersections.erase(intersections.begin());
      else
        intersections.insert(intersections.begin(), 0.0);
    }

    if (intersections[0] != 1.0) intersections.push_back(1.0);

    sortedWRanges.reserve(intersections.size() / 2);

    for (UINT j = 0; j < intersections.size() - 1; j += 2)
      sortedWRanges.push_back(
          std::make_pair(intersections[j], intersections[j + 1]));

#ifdef _DEBUG

    for (UINT kkk = 0; kkk < sortedWRanges.size() - 1; kkk++) {
      assert(sortedWRanges[kkk].first < sortedWRanges[kkk].second);
      assert(sortedWRanges[kkk].second <= sortedWRanges[kkk + 1].first);
    }
    assert(sortedWRanges.back().first < sortedWRanges.back().second);

#endif

    vi->splitStroke(i, sortedWRanges);

    UINT addedStroke = vi->getStrokeCount() -
                       (oldStrokeSize - 1);  //-1 perche' e' quella tolta

    i += addedStroke;

    *it = -1;
    m_indexes.insert(it, addedStroke - 1, -1);
    it = m_indexes.begin() + i;
  }

  invalidate();
}

//-----------------------------------------------------------------------------

void EraserTool::erase(TVectorImageP vi, TRectD &rect) {
  if (rect.x0 > rect.x1) tswap(rect.x1, rect.x0);
  if (rect.y0 > rect.y1) tswap(rect.y1, rect.y0);
  int i     = 0;
  int index = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::vector<int> eraseStrokes;

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo = new UndoEraser(level, getCurrentFid());
  for (i = 0; i < (int)vi->getStrokeCount(); i++) {
    if (!vi->inCurrentGroup(i)) continue;
    TStroke *stroke = vi->getStroke(i);
    if (!m_invertOption.getValue()) {
      if ((!m_selective.getValue() || stroke->getStyle() == index) &&
          rect.contains(stroke->getBBox())) {
        m_undo->addOldStroke(i, vi->getVIStroke(i));
        eraseStrokes.push_back(i);
      }
    } else {
      if ((!m_selective.getValue() || stroke->getStyle() == index) &&
          !rect.contains(stroke->getBBox())) {
        m_undo->addOldStroke(i, vi->getVIStroke(i));
        eraseStrokes.push_back(i);
      }
    }
  }
  for (i = (int)eraseStrokes.size() - 1; i >= 0; i--)
    vi->deleteStroke(eraseStrokes[i]);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
  invalidate();
}

//-----------------------------------------------------------------------------

void EraserTool::stopErase(TVectorImageP vi) {
  assert(m_undo != 0);

  UINT size = m_indexes.size();

  assert(size == vi->getStrokeCount());
  UINT i = 0;
  for (; i < size; i++) {
    if (m_indexes[i] == -1) m_undo->addNewStroke(i, vi->getVIStroke(i));
  }
  TUndoManager::manager()->add(m_undo);
  m_undo   = 0;
  m_active = false;
  invalidate();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

void EraserTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;

  m_active = true;

  TImageP image(getImage(true));
  m_activeImage = image;
  if (m_eraseType.getValue() == NORMAL_ERASE) {
    if (TVectorImageP vi = image) startErase(vi, pos /*,imageLocation*/);
  } else if (m_eraseType.getValue() == RECT_ERASE) {
    m_selectingRect.x0 = pos.x;
    m_selectingRect.y0 = pos.y;
    m_selectingRect.x1 = pos.x + 1;
    m_selectingRect.y1 = pos.y + 1;
    invalidate();
  } else if (m_eraseType.getValue() == FREEHAND_ERASE) {
    startFreehand(pos);
  } else if (m_eraseType.getValue() == POLYLINE_ERASE) {
    addPointPolyline(pos);
  }
}

//-----------------------------------------------------------------------------

void EraserTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;

  if (!m_active) return;

  TImageP image(getImage(true));
  if (m_eraseType.getValue() == RECT_ERASE) {
    m_selectingRect.x1 = pos.x;
    m_selectingRect.y1 = pos.y;
    invalidate();
    return;
  } else if (m_eraseType.getValue() == NORMAL_ERASE) {
    if (TVectorImageP vi = image) erase(vi, pos);
  } else if (m_eraseType.getValue() == FREEHAND_ERASE) {
    freehandDrag(pos);
  }
}

//-----------------------------------------------------------------------------

void EraserTool::multiEraseRect(TFrameId firstFrameId, TFrameId lastFrameId,
                                TRectD firstRect, TRectD lastRect,
                                bool invert) {
  int r0 = firstFrameId.getNumber();
  int r1 = lastFrameId.getNumber();

  if (r0 > r1) {
    tswap(r0, r1);
    tswap(firstFrameId, lastFrameId);
    tswap(firstRect, lastRect);
  }
  if ((r1 - r0) < 1) return;

  std::vector<TFrameId> allFids;
  m_level->getFids(allFids);
  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFrameId) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFrameId) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFrameId <= fid && fid <= lastFrameId);
    TVectorImageP img = (TVectorImageP)m_level->getFrame(fid, true);
    assert(img);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    TRectD rect = interpolateRect(firstRect, lastRect, t);
    // m_level->setFrame(fid, img); //necessario: se la getFrame ha scompattato
    // una img compressa, senza setFrame le modifiche sulla img fatte qui
    // andrebbero perse.

    // Setto fid come corrente per notificare il cambiamento dell'immagine
    TTool::Application *app = TTool::getApplication();
    if (app) {
      if (app->getCurrentFrame()->isEditingScene())
        app->getCurrentFrame()->setFrame(fid.getNumber() - 1);
      else
        app->getCurrentFrame()->setFid(fid);
    }

    erase(img, rect);

    notifyImageChanged();
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void EraserTool::onImageChanged() {
  if (m_active) {
    stopErase(m_activeImage);
  }
  if (!m_multi.getValue()) return;
  TTool::Application *application = TTool::getApplication();
  if (!application) return;
  TXshSimpleLevel *xshl = 0;
  if (application->getCurrentLevel()->getLevel())
    xshl = application->getCurrentLevel()->getLevel()->getSimpleLevel();

  if (!xshl || m_level.getPointer() != xshl ||
      (m_eraseType.getValue() == RECT_ERASE && m_selectingRect.isEmpty()) ||
      ((m_eraseType.getValue() == FREEHAND_ERASE ||
        m_eraseType.getValue() == POLYLINE_ERASE) &&
       !m_firstStroke))
    resetMulti();
  else if (m_firstFrameId == getCurrentFid())
    m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e torno
                                   // all'immagine iniziale, torno allo stato
                                   // iniziale
  else {                           // cambio stato.
    m_firstFrameSelected = true;
    if (m_eraseType.getValue() == RECT_ERASE) {
      assert(!m_selectingRect.isEmpty());
      m_firstRect = m_selectingRect;
    }
  }
}

//-----------------------------------------------------------------------------

void EraserTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (!m_active) return;
  m_active = false;
  TImageP image(getImage(true));
  TVectorImageP vi = image;

  TTool::Application *application = TTool::getApplication();
  if (!vi || !application) return;
  if (m_eraseType.getValue() == NORMAL_ERASE)
    stopErase(vi);
  else if (m_eraseType.getValue() == RECT_ERASE) {
    if (m_selectingRect.x0 > m_selectingRect.x1)
      tswap(m_selectingRect.x1, m_selectingRect.x0);
    if (m_selectingRect.y0 > m_selectingRect.y1)
      tswap(m_selectingRect.y1, m_selectingRect.y0);

    if (m_multi.getValue()) {
      if (m_firstFrameSelected) {
        multiEraseRect(m_firstFrameId, getCurrentFid(), m_firstRect,
                       m_selectingRect, m_invertOption.getValue());
        if (e.isShiftPressed()) {
          m_firstRect    = m_selectingRect;
          m_firstFrameId = getCurrentFid();
        } else {
          if (application->getCurrentFrame()->isEditingScene()) {
            application->getCurrentColumn()->setColumnIndex(m_currCell.first);
            application->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            application->getCurrentFrame()->setFid(m_veryFirstFrameId);
          resetMulti();
        }
        invalidate();  // invalidate(m_selectingRect.enlarge(2));
      } else {
        if (application->getCurrentFrame()->isEditingScene())
          m_currCell = std::pair<int, int>(
              application->getCurrentColumn()->getColumnIndex(),
              application->getCurrentFrame()->getFrame());
      }
      return;
    } else {
      erase(vi, m_selectingRect);
      notifyImageChanged();
      m_selectingRect.empty();
    }
  } else if (m_eraseType.getValue() == FREEHAND_ERASE) {
    closeFreehand(pos);
    if (m_multi.getValue()) {
      multiEreserRegion(m_stroke, e);
      invalidate();
    } else {
      eraseRegion(vi, m_stroke);
      invalidate();
      notifyImageChanged();
    }
    m_track.clear();
  }
}

//-----------------------------------------------------------------------------

//! Viene chiusa la polyline e si da il via alla cancellazione.
/*!Viene creato lo \b stroke rappresentante la polyline disegnata.
  Se e' selzionato il metodo di cancellazione "frame range" viene richiamato il
  metodo \b multiEreserRegion, altrimenti
  viene richiamato il metodo \b eraseRegion*/
void EraserTool::leftButtonDoubleClick(const TPointD &pos,
                                       const TMouseEvent &e) {
  TVectorImageP vi = getImage(true);
  if (m_eraseType.getValue() == POLYLINE_ERASE) {
    closePolyline(pos);

    std::vector<TThickPoint> strokePoints;
    for (UINT i = 0; i < m_polyline.size() - 1; i++) {
      strokePoints.push_back(TThickPoint(m_polyline[i], 1));
      strokePoints.push_back(
          TThickPoint(0.5 * (m_polyline[i] + m_polyline[i + 1]), 1));
    }
    strokePoints.push_back(TThickPoint(m_polyline.back(), 1));
    m_polyline.clear();
    TStroke *stroke = new TStroke(strokePoints);
    assert(stroke->getPoint(0) == stroke->getPoint(1));
    if (m_multi.getValue())
      multiEreserRegion(stroke, e);
    else {
      eraseRegion(vi, stroke);
      m_active = false;
      notifyImageChanged();
    }
    invalidate();
  }
}

//-----------------------------------------------------------------------------

void EraserTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  struct Locals {
    EraserTool *m_this;

    void setValue(TDoubleProperty &prop, double value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addValue(TDoubleProperty &prop, double add) {
      const TDoubleProperty::Range &range = prop.getRange();
      setValue(prop, tcrop(prop.getValue() + add, range.first, range.second));
    }

  } locals = {this};

  switch (e.getModifiersMask()) {
  case TMouseEvent::ALT_KEY: {
    // User wants to alter the maximum brush size
    const TPointD &diff = pos - m_mousePos;
    double add          = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

    locals.addValue(m_toolSize, add);
    break;
  }

  default:
    m_brushPos = pos;
    break;
  }

  m_oldMousePos = m_mousePos = pos;
  invalidate();
}

//----------------------------------------------------------------------

bool EraserTool::onPropertyChanged(std::string propertyName) {
  EraseVectorType      = ::to_string(m_eraseType.getValue());
  EraseVectorSize      = m_toolSize.getValue();
  EraseVectorSelective = m_selective.getValue();
  EraseVectorInvert    = m_invertOption.getValue();
  EraseVectorRange     = m_multi.getValue();

  double x = m_toolSize.getValue();

  double minRange = 1;
  double maxRange = 100;

  double minSize = 2;
  double maxSize = 100;

  m_pointSize =
      ((x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize) *
      0.5;
  invalidate();

  return true;
}

//-----------------------------------------------------------------------------

void EraserTool::onEnter() {
  if (m_firstTime) {
    m_toolSize.setValue(EraseVectorSize);
    m_eraseType.setValue(::to_wstring(EraseVectorType.getValue()));
    m_selective.setValue(EraseVectorSelective ? 1 : 0);
    m_invertOption.setValue(EraseVectorInvert ? 1 : 0);
    m_multi.setValue(EraseVectorRange ? 1 : 0);
    m_firstTime = false;
  }

  double x = m_toolSize.getValue();

  double minRange = 1;
  double maxRange = 100;

  double minSize = 2;
  double maxSize = 100;

  m_pointSize =
      ((x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize) *
      0.5;

  //  getApplication()->editImage();
}

//-----------------------------------------------------------------------------

void EraserTool::onLeave() {
  draw();
  m_pointSize = -1;
}

//-----------------------------------------------------------------------------

void EraserTool::onActivate() {
  resetMulti();
  m_polyline.clear();
  onEnter();
}

//-----------------------------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del lazzo.
//! Viene inizializzato \b m_firstPos
void EraserTool::startFreehand(const TPointD &pos) {
  m_track.clear();
  m_firstPos = pos;
  m_track.add(TThickPoint(pos, m_thick), getPixelSize() * getPixelSize());
}

//-----------------------------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del lazzo.
void EraserTool::freehandDrag(const TPointD &pos) {
#if defined(MACOSX)
//		m_viewer->enableRedraw(false);
#endif
  m_track.add(TThickPoint(pos, m_thick), getPixelSize() * getPixelSize());
  invalidate(m_track.getModifiedRegion());
}

//-----------------------------------------------------------------------------

//! Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene creato
//! lo stroke rappresentante il lazzo.
void EraserTool::closeFreehand(const TPointD &pos) {
#if defined(MACOSX)
//		m_viewer->enableRedraw(true);
#endif
  if (m_track.isEmpty()) return;
  m_track.add(TThickPoint(m_firstPos, m_thick),
              getPixelSize() * getPixelSize());
  m_track.filterPoints();
  double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
  m_stroke     = m_track.makeStroke(error);
  m_stroke->setStyle(1);
}

//-----------------------------------------------------------------------------

//! Viene aggiunto un punto al vettore m_polyline.
void EraserTool::addPointPolyline(const TPointD &pos) {
  m_firstPos = pos;
  m_polyline.push_back(pos);
}

//-----------------------------------------------------------------------------

//! Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b
//! m_polyline.front() alla fine del vettore)
void EraserTool::closePolyline(const TPointD &pos) {
  if (m_polyline.size() <= 1) return;
  if (m_polyline.back() != pos) m_polyline.push_back(pos);
  if (m_polyline.back() != m_polyline.front())
    m_polyline.push_back(m_polyline.front());
  invalidate();
}

//-----------------------------------------------------------------------------

//! Cancella stroke presenti in \b vi e contenuti nella regione delimitata da \b
//! stroke.
/*!Vengono cercati gli stroke da cancellare facendo i dovuti controlli sui
   parametri \b m_invertOption e \b m_selective.
        Se uno stroke deve essere cancellato viene inserito in \b eraseStrokes.
        Gli stroke vengono cancellati tutti alla fine.*/
void EraserTool::eraseRegion(
    const TVectorImageP vi,
    TStroke *stroke)  //, const TImageLocation &imageLocation)
{
  if (!vi || !stroke) return;
  TVectorImage eraseImg;
  TStroke *eraseStroke = new TStroke(*stroke);
  eraseImg.addStroke(eraseStroke);
  eraseImg.findRegions();
  int strokeIndex, regionIndex, colorStyle;
  colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::vector<int> eraseStrokes;

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo = new UndoEraser(level, getCurrentFid());

  if (!m_invertOption.getValue()) {
    for (strokeIndex = 0; strokeIndex < (int)vi->getStrokeCount();
         strokeIndex++) {
      if (!vi->inCurrentGroup(strokeIndex)) continue;
      TStroke *currentStroke = vi->getStroke(strokeIndex);
      for (regionIndex = 0; regionIndex < (int)eraseImg.getRegionCount();
           regionIndex++) {
        TRegion *region = eraseImg.getRegion(regionIndex);
        if ((!m_selective.getValue() ||
             (m_selective.getValue() &&
              currentStroke->getStyle() == colorStyle)) &&
            region->contains(*currentStroke, true)) {
          eraseStrokes.push_back(strokeIndex);
          m_undo->addOldStroke(strokeIndex, vi->getVIStroke(strokeIndex));
        }
      }
    }
  } else {
    for (strokeIndex = 0; strokeIndex < (int)vi->getStrokeCount();
         strokeIndex++) {
      TStroke *currentStroke = vi->getStroke(strokeIndex);
      bool eraseIt           = false;
      for (regionIndex = 0; regionIndex < (int)eraseImg.getRegionCount();
           regionIndex++) {
        TRegion *region = eraseImg.getRegion(regionIndex);
        if (!m_selective.getValue() ||
            (m_selective.getValue() && currentStroke->getStyle() == colorStyle))
          eraseIt = true;
        if (region->contains(*currentStroke, true)) {
          eraseIt = false;
          break;
        }
      }
      if (eraseIt) {
        m_undo->addOldStroke(strokeIndex, vi->getVIStroke(strokeIndex));
        eraseStrokes.push_back(strokeIndex);
      }
    }
  }
  int i;
  for (i = (int)eraseStrokes.size() - 1; i >= 0; i--)
    vi->deleteStroke(eraseStrokes[i]);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
}

//-----------------------------------------------------------------------------

//! Viene richiamata la doErase sui frame compresi tra \b firstFrameId e \b
//! lastFrameId.
/*!Viene caricato il vettore \b fids con i TFrameId delle immagini sulle quali
 * si deve effettuare una cancellazione.*/
void EraserTool::doMultiErase(TFrameId &firstFrameId, TFrameId &lastFrameId,
                              const TStroke *firstStroke,
                              const TStroke *lastStroke) {
  TXshSimpleLevel *sl =
      TTool::getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel();
  TStroke *first           = new TStroke();
  TStroke *last            = new TStroke();
  *first                   = *firstStroke;
  *last                    = *lastStroke;
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();
  firstImage->addStroke(first);
  lastImage->addStroke(last);

  bool backward = false;
  if (firstFrameId > lastFrameId) {
    tswap(firstFrameId, lastFrameId);
    backward = true;
  }
  assert(firstFrameId <= lastFrameId);

  std::vector<TFrameId> allFids;
  sl->getFids(allFids);
  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFrameId) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFrameId) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFrameId <= fid && fid <= lastFrameId);
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    // Setto il fid come corrente per notificare il cambiamento dell'immagine
    TTool::Application *app = TTool::getApplication();
    if (app) {
      if (app->getCurrentFrame()->isEditingScene())
        app->getCurrentFrame()->setFrame(fid.getNumber() - 1);
      else
        app->getCurrentFrame()->setFid(fid);
    }
    doErase(backward ? 1 - t : t, sl, fid, firstImage, lastImage);
    notifyImageChanged();
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

//! Viene richiamata la \b eraseRegion per il frame giusto nel caso della
//! modalita' "Frame Range".
/*!Nei casi in cui \b t e' diverso da zero e uno, viene generata una nuova \b
   TVectorImageP richiamando la \b TInbetween.
        La nuova immagine contiene lo stroke da dare alla eraseRegion. \b fid e'
   il TFrameId dell'immagine sulla quale
        bisogna effettuare la cancellazione.*/
void EraserTool::doErase(double t, const TXshSimpleLevelP &sl,
                         const TFrameId &fid, const TVectorImageP &firstImage,
                         const TVectorImageP &lastImage) {
  //	TImageLocation imageLocation(m_level->getName(),fid);
  TVectorImageP img = sl->getFrame(fid, true);
  if (t == 0)
    eraseRegion(img, firstImage->getStroke(0));  //,imageLocation);
  else if (t == 1)
    eraseRegion(img, lastImage->getStroke(0));  //,imageLocation);
  else {
    assert(firstImage->getStrokeCount() == 1);
    assert(lastImage->getStrokeCount() == 1);
    TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
    assert(vi->getStrokeCount() == 1);
    eraseRegion(img, vi->getStroke(0));  //,imageLocation);
  }
}

//-----------------------------------------------------------------------------

//! Effettua la cancellazione nella modalita' "Frame range".
/*! Se il primo frame e' gia stato selezionato richiama la \b doMultiErase;
 altrimenti viene inizializzato
 \b m_firstStroke.*/
void EraserTool::multiEreserRegion(TStroke *stroke, const TMouseEvent &e) {
  TTool::Application *application = TTool::getApplication();
  if (!application) return;

  if (m_firstFrameSelected) {
    if (m_firstStroke && stroke) {
      TFrameId tmpFrameId = getCurrentFid();
      doMultiErase(m_firstFrameId, tmpFrameId, m_firstStroke, stroke);
    }
    if (e.isShiftPressed()) {
      m_firstStroke  = new TStroke(*stroke);
      m_firstFrameId = getCurrentFid();
    } else {
      if (application->getCurrentFrame()->isEditingScene()) {
        application->getCurrentColumn()->setColumnIndex(m_currCell.first);
        application->getCurrentFrame()->setFrame(m_currCell.second);
      } else
        application->getCurrentFrame()->setFid(m_veryFirstFrameId);
      resetMulti();
    }
  } else {
    m_firstStroke = new TStroke(*stroke);
    if (application->getCurrentFrame()->isEditingScene())
      m_currCell =
          std::pair<int, int>(application->getCurrentColumn()->getColumnIndex(),
                              application->getCurrentFrame()->getFrame());
  }
}

//-----------------------------------------------------------------------------
/*! When the tool is switched during dragging, Erase end processing is performed
*/
void EraserTool::onDeactivate() {
  if (!m_active) return;

  m_active = false;

  // TODO : finishing erase procedure is only available for normal type.
  // Supporting other types is needed. 2016/1/22 shun_iwasawa
  if (m_eraseType.getValue() != NORMAL_ERASE) return;

  TImageP image(getImage(true));
  TVectorImageP vi                = image;
  TTool::Application *application = TTool::getApplication();
  if (!vi || !application) return;

  stopErase(vi);
}

// TTool *getEraserTool() {return &eraserTool;}
