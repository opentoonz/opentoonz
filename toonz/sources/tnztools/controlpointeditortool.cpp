#include "controlpointeditortool.h"

#include "tundo.h"
#include "tthreadmessage.h"
#include "tvectorimage.h"
#include "drawutil.h"
#include "tenv.h"

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"
#include "tools/toolhandle.h"

#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/strokegenerator.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/stage2.h"
#include "toonz/tstageobject.h"

#include <QKeyEvent>

using namespace ToolUtils;

TEnv::StringVar CPSelectionType("ControlPointEditorToolSelectionType",
                                "Rectangular");
TEnv::IntVar AutoSelectDrawing("ControlPointEditorToolAutoSelectDrawing", 1);
TEnv::IntVar Snap("ControlPointEditorToolSnap", 0);
TEnv::IntVar SnapSensitivity("ControlPointEditorToolSnapSensitivity", 0);

//-----------------------------------------------------------------------------

#define LOW_WSTR L"Low"
#define MEDIUM_WSTR L"Medium"
#define HIGH_WSTR L"High"

#define RECTANGULAR_WSTR L"Rectangular"
#define FREEHAND_WSTR L"Freehand"

const double SNAPPING_LOW    = 5.0;
const double SNAPPING_MEDIUM = 25.0;
const double SNAPPING_HIGH   = 100.0;

//-----------------------------------------------------------------------------
namespace {

/*! Restituisce i parametri riferiti allo stroke della curva che si vuole
   muovere.
    I parametri dipendono da come sono i punti in \b beforeIndex, \b nextIndex
   (cuspidi, lineari).
    Puo' restituire due range nei casi in cui lo stroke e' selfLoop e la curva
   e' a cavallo del punto di chiusura.
*/
void getSegmentParameter(ControlPointEditorStroke *cpEditor, int beforeIndex,
                         int nextIndex, double &w0, double &w1, double &q0,
                         double &q1) {
  TStroke *stroke = cpEditor->getStroke();
  if (!stroke) return;
  q0 = q1 = w0 = w1 = -1;
  int cpCount       = cpEditor->getControlPointCount();
  // Il punto di controllo precedente non e' lincato
  if (cpEditor->isSpeedOutLinear(beforeIndex) ||
      cpEditor->isSpeedInLinear(beforeIndex) || cpEditor->isCusp(beforeIndex)) {
    if (cpEditor->isSelfLoop() && beforeIndex == 0 &&
        nextIndex == cpCount - 1)  // Nel caso selfLoop si invertono i valori
      w1 = 1;
    else
      w0 = stroke->getParameterAtControlPoint(
          cpEditor->getIndexPointInStroke(beforeIndex));
  } else  // Punto di controllo precedente lincato
  {
    if (!cpEditor->isSelfLoop() || beforeIndex != 0)
      w0 = stroke->getParameterAtControlPoint(
          cpEditor->getIndexPointInStroke(beforeIndex) - 4);
    else {
      if (nextIndex == 1)  // Primo chunk
      {
        w0 = 0;
        q0 = stroke->getParameterAtControlPoint(
            cpEditor->getIndexPointInStroke(cpCount - 1));
        q1 = 1;
      } else if (nextIndex == cpCount - 1)  // Ultimo chunk
      {
        w1 = 1;
        q0 = 0;
        q1 = stroke->getParameterAtControlPoint(
            cpEditor->getIndexPointInStroke(1));
      } else {
        assert(0);
      }  // Non dovrebbe mai accadere
    }
  }
  // Il punto di controllo successivo non e' lincato
  if (cpEditor->isSpeedInLinear(nextIndex) ||
      cpEditor->isSpeedOutLinear(nextIndex) || cpEditor->isCusp(nextIndex)) {
    if (cpEditor->isSelfLoop() && beforeIndex == 0 &&
        nextIndex == cpCount - 1)  // Nel caso selfLoop si invertono i valori
      w0 = stroke->getParameterAtControlPoint(
          cpEditor->getIndexPointInStroke(nextIndex));
    else
      w1 = stroke->getParameterAtControlPoint(
          cpEditor->getIndexPointInStroke(nextIndex));
  } else  // Punto di controllo successivo lincato
  {
    if (!cpEditor->isSelfLoop() || nextIndex != cpCount - 1 || beforeIndex != 0)
      w1 = stroke->getParameterAtControlPoint(
          cpEditor->getIndexPointInStroke(nextIndex) + 4);
    else if (nextIndex == cpCount - 1)  // Ultimo chunk
      w0 = stroke->getParameterAtControlPoint(
          cpEditor->getIndexPointInStroke(nextIndex) - 4);
    else {
      assert(0);
    }  // Non dovrebbe mai accadere, i vari casi dovrebbero essere gestiti sopra
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TPointD ControlPointEditorTool::calculateSnap(TPointD pos) {
  m_foundSnap = false;
  TVectorImageP vi(TTool::getImage(false));
  TPointD snapPoint = pos;
  if (vi && m_snap.getValue()) {
    double minDistance = m_snapMinDistance;

    int i, strokeNumber = vi->getStrokeCount();

    TStroke *selfStroke = m_controlPointEditorStroke.getStroke();
    TStroke *stroke;
    double distance, outW, w;

    for (i = 0; i < strokeNumber; i++) {
      stroke = vi->getStroke(i);
      if (stroke != selfStroke) {
        if (stroke->getNearestW(pos, outW, distance) &&
            distance < minDistance) {
          minDistance = distance;
          if (areAlmostEqual(outW, 0.0, 1e-3))
            w = 0.0;
          else if (areAlmostEqual(outW, 1.0, 1e-3))
            w = 1.0;
          else
            w = outW;
          TThickPoint point = stroke->getPoint(w);
          snapPoint         = TPointD(point.x, point.y);
          m_foundSnap       = true;
          m_snapPoint       = snapPoint;
        }
      }
    }
  }
  return snapPoint;
}

void ControlPointEditorTool::drawSnap() {
  double thick = 6.0;
  if (m_foundSnap) {
    tglColor(TPixelD(0.1, 0.9, 0.1));
    tglDrawCircle(m_snapPoint, thick);
  }
}

TPointD ControlPointEditorTool::getSnap(TPointD pos) {
  if (m_foundSnap)
    return m_snapPoint;
  else
    return pos;
}

void ControlPointEditorTool::resetSnap() { m_foundSnap = false; }

//=============================================================================
// Spline Editor Tool
//-----------------------------------------------------------------------------

ControlPointEditorTool::ControlPointEditorTool()
    : TTool("T_ControlPointEditor")
    , m_draw(false)
    , m_lastPointSelected(-1)
    , m_isImageChanged(false)
    , m_selectingRect(TRectD())
    , m_autoSelectDrawing("Auto Select Drawing", true)
    , m_snap("snap", false)
    , m_snapSensitivity("Sensitivity:")
    , m_selectType("Type:")
    , m_action(NONE)
    , m_cursorType(NORMAL)
    , m_undo(0)
    , m_isMenuViewed(false)
    , m_moveControlPointEditorStroke()
    , m_moveSegmentLimitation() {
  bind(TTool::Vectors);
  m_prop.bind(m_selectType);
  m_prop.bind(m_autoSelectDrawing);
  m_prop.bind(m_snap);
  m_prop.bind(m_snapSensitivity);
  m_selection.setControlPointEditorStroke(&m_controlPointEditorStroke);

  m_selectType.addValue(RECTANGULAR_WSTR);
  m_selectType.addValue(FREEHAND_WSTR);
  m_selectType.setId("Type");

  m_autoSelectDrawing.setId("AutoSelectDrawing");
  m_snap.setId("Snap");
  m_snapSensitivity.addValue(LOW_WSTR);
  m_snapSensitivity.addValue(MEDIUM_WSTR);
  m_snapSensitivity.addValue(HIGH_WSTR);
  m_snapSensitivity.setId("SnapSensitivity");
}

//-----------------------------------------------------------------------------

void ControlPointEditorTool::updateTranslation() {
  m_autoSelectDrawing.setQStringName(tr("Auto Select Drawing"));
  m_snap.setQStringName(tr("Snap"));
  m_selectType.setQStringName(tr("Type:"));
  m_selectType.setItemUIName(RECTANGULAR_WSTR, tr("Rectangular"));
  m_selectType.setItemUIName(FREEHAND_WSTR, tr("Freehand"));

  m_snapSensitivity.setQStringName(tr(""));
  m_snapSensitivity.setItemUIName(LOW_WSTR, tr("Low"));
  m_snapSensitivity.setItemUIName(MEDIUM_WSTR, tr("Med"));
  m_snapSensitivity.setItemUIName(HIGH_WSTR, tr("High"));
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::initUndo() {
  if (TTool::getApplication()->getCurrentObject()->isSpline()) {
    m_undo =
        new UndoPath(getXsheet()->getStageObject(getObjectId())->getSpline());
    return;
  }
  TVectorImageP vi(getImage(false));
  if (!vi) return;
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  UndoControlPointEditor *undo =
      new UndoControlPointEditor(level, getCurrentFid());
  int index = m_controlPointEditorStroke.getStrokeIndex();
  if (index > -1) undo->addOldStroke(index, vi->getVIStroke(index));
  m_undo = undo;
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::getNearestStrokeColumnIndexes(
    std::vector<int> &indexes, TPointD pos) {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  int currentFrame        = app->getCurrentFrame()->getFrameIndex();
  std::vector<int> newIndexes;
  TAffine aff = getMatrix();
  int i       = 0;
  for (i = 0; i < (int)indexes.size(); i++) {
    if (xsh->getColumn(i)->isLocked()) continue;
    int index        = indexes[i];
    TVectorImageP vi = xsh->getCell(currentFrame, index).getImage(false);
    if (!vi) continue;
    double dist2, t = 0;
    UINT strokeIndex = -1;
    TPointD p        = getColumnMatrix(index).inv() * getMatrix() * pos;
    if (vi->getNearestStroke(p, t, strokeIndex, dist2) &&
        dist2 < 25 * getPixelSize() * getPixelSize())
      newIndexes.push_back(index);
  }
  indexes.clear();
  indexes = newIndexes;
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::drawMovingSegment() {
  int beforeIndex = m_moveSegmentLimitation.first;
  int nextIndex   = m_moveSegmentLimitation.second;
  if (m_action != SEGMENT_MOVEMENT || beforeIndex == -1 || nextIndex == -1 ||
      !m_moveControlPointEditorStroke.getStroke())
    return;
  tglColor(TPixel::Green);
  double w0, w1;
  double q0, q1;
  getSegmentParameter(&m_moveControlPointEditorStroke, beforeIndex, nextIndex,
                      w0, w1, q0, q1);
  if (w0 != -1 && w1 != -1)  // Dovrebbero essere sempre diversi...
    drawStrokeCenterline(*m_moveControlPointEditorStroke.getStroke(),
                         getPixelSize(), w0, w1);
  if (q0 != -1 && q1 != -1)
    drawStrokeCenterline(*m_moveControlPointEditorStroke.getStroke(),
                         getPixelSize(), q0, q1);
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::drawControlPoint() {
  TPixel color1         = TPixel(79, 128, 255);
  TPixel color2         = TPixel::White;
  TPixel color_handle   = TPixel(96, 64, 201);
  int controlPointCount = m_controlPointEditorStroke.getControlPointCount();

  double pix    = getPixelSize() * 2.0f;
  double pix1_5 = 1.5 * pix, pix2 = pix + pix, pix2_5 = pix1_5 + pix,
         pix3 = pix2 + pix, pix3_5 = pix2_5 + pix, pix4 = pix3 + pix;

  double maxDist2 = sq(5.0 * pix);
  double dist2    = 0;
  int pointIndex;
  ControlPointEditorStroke::PointType pointType =
      m_controlPointEditorStroke.getPointTypeAt(m_pos, maxDist2, pointIndex);
  int i;
  for (i = 0; i < controlPointCount; i++) {
    TThickPoint point = m_controlPointEditorStroke.getControlPoint(i);
    TPointD pa        = m_controlPointEditorStroke.getSpeedInPoint(i);
    TPointD pb        = m_controlPointEditorStroke.getSpeedOutPoint(i);
    tglColor(color_handle);
    tglDrawSegment(pa, point);
    if (i == pointIndex && pointType == ControlPointEditorStroke::SPEED_IN)
      tglFillRect(pa.x - pix2_5, pa.y - pix2_5, pa.x + pix2_5, pa.y + pix2_5);
    else
      tglFillRect(pa.x - pix1_5, pa.y - pix1_5, pa.x + pix1_5, pa.y + pix1_5);

    tglDrawSegment(pb, point);
    if (i == pointIndex && pointType == ControlPointEditorStroke::SPEED_OUT)
      tglFillRect(pb.x - pix2_5, pb.y - pix2_5, pb.x + pix2_5, pb.y + pix2_5);
    else
      tglFillRect(pb.x - pix1_5, pb.y - pix1_5, pb.x + pix1_5, pb.y + pix1_5);

    tglColor(color1);

    if (i == pointIndex &&
        pointType == ControlPointEditorStroke::CONTROL_POINT) {
      tglFillRect(point.x - pix3_5, point.y - pix3_5, point.x + pix3_5,
                  point.y + pix3_5);
      if (!m_selection.isSelected(i)) {
        tglColor(color2);
        tglFillRect(point.x - pix2_5, point.y - pix2_5, point.x + pix2_5,
                    point.y + pix2_5);
      }
    } else {
      tglFillRect(point.x - pix2, point.y - pix2, point.x + pix2,
                  point.y + pix2);
      if (!m_selection.isSelected(i)) {
        tglColor(color2);
        tglFillRect(point.x - pix, point.y - pix, point.x + pix, point.y + pix);
      }
    }
  }
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::draw() {
  TVectorImageP vi(getImage(false));
  if (!m_draw) return;

  int currentStroke = m_controlPointEditorStroke.getStrokeIndex();
  if (!vi || currentStroke == -1 ||
      m_controlPointEditorStroke.getControlPointCount() == 0 ||
      vi->getStrokeCount() == 0 || (int)vi->getStrokeCount() <= currentStroke) {
    m_controlPointEditorStroke.setStroke((TVectorImage *)0, -1);
    return;
  }

  TPixel color1, color2;
  if (m_action == RECT_SELECTION)  // Disegna il rettangolo per la selezione
  {
    color1 = TPixel32::
        Black;  // TransparencyCheck::instance()->isEnabled()?TPixel32::White:TPixel32::Black;
    drawRect(m_selectingRect, color1, 0x3F33, true);
  } else if (m_action == FREEHAND_SELECTION && !m_track.isEmpty()) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    tglColor(color);
    m_track.drawAllFragments();
  }

  if (m_controlPointEditorStroke.getControlPointCount() <= 0) return;

  color1          = TPixel(79, 128, 255);
  color2          = TPixel::White;
  TStroke *stroke = m_controlPointEditorStroke.getStroke();
  tglColor(color1);
  drawStrokeCenterline(*stroke, getPixelSize());

  drawControlPoint();

  drawMovingSegment();

  drawSnap();
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::mouseMove(const TPointD &pos,
                                       const TMouseEvent &e) {
  // Scelgo il cursore in base alla distanza del mouse dalla curva e dai punti
  // di controllo
  TVectorImageP vi(getImage(false));
  if (!vi) {
    m_controlPointEditorStroke.setStroke((TVectorImage *)0, -1);
    m_cursorType = NO_ACTIVE;
    return;
  } else
    m_cursorType = NORMAL;

  m_pos = pos;

  if (!m_draw || m_controlPointEditorStroke.getStrokeIndex() == -1) return;
  if (e.isAltPressed())
    m_cursorType = EDIT_SPEED;
  else {
    double maxDist  = 5 * getPixelSize();
    double maxDist2 = maxDist * maxDist;
    int pointIndexCP;
    ControlPointEditorStroke::PointType pointType =
        m_controlPointEditorStroke.getPointTypeAt(pos, maxDist2, pointIndexCP);
    if (pointType == ControlPointEditorStroke::SEGMENT && e.isCtrlPressed())
      m_cursorType = ADD;
    // else if(pointType == ControlPointEditorStroke::SEGMENT)
    // m_cursorType=EDIT_SEGMENT;
    else
      m_cursorType = NORMAL;
  }
  invalidate();
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::leftButtonDown(const TPointD &pos,
                                            const TMouseEvent &e) {
  if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
    getViewer()->doPickGuideStroke(pos);
    return;
  }

  m_pos           = pos;
  double pix      = getPixelSize() * 2.0f;
  double maxDist  = 5 * pix;
  double maxDist2 = maxDist * maxDist;
  double dist2    = 0;
  int pointIndex;
  ControlPointEditorStroke::PointType pointType =
      m_controlPointEditorStroke.getPointTypeAt(pos, maxDist2, pointIndex);

  if (pointType == ControlPointEditorStroke::NONE) {
    // ho cliccato lontano dalla curva corrente
    TTool::Application *app = TTool::getApplication();
    if (m_autoSelectDrawing.getValue()) {
      // Non sono in nessun gadget
      std::vector<int> columnIndexes;
      getViewer()->posToColumnIndexes(e.m_pos, columnIndexes, 5.0, false);
      getNearestStrokeColumnIndexes(columnIndexes, pos);
      if (!columnIndexes.empty()) {
        int currentColumnIndex = app->getCurrentColumn()->getColumnIndex();
        int columnIndex;
        if (columnIndexes.size() == 1)
          columnIndex = columnIndexes[0];
        else {
          ToolUtils::ColumChooserMenu *menu = new ToolUtils::ColumChooserMenu(
              app->getCurrentXsheet()->getXsheet(), columnIndexes);
          m_isMenuViewed = true;
          columnIndex    = menu->execute();
        }
        TXshColumn *column =
            app->getCurrentXsheet()->getXsheet()->getColumn(columnIndex);
        if (columnIndex >= 0 && columnIndex != currentColumnIndex && column &&
            !column->isLocked()) {
          TAffine aff = getMatrix();
          app->getCurrentColumn()->setColumnIndex(columnIndex);
          updateMatrix();
          currentColumnIndex = columnIndex;
          m_pos              = getMatrix().inv() * aff * pos;
        }
      }
    }

    TVectorImageP vi = getImage(false);
    if (!vi) return;
    double dist2, t = 0;
    UINT index = -1;
    if (vi->getNearestStroke(m_pos, t, index, dist2) &&
        dist2 < 25 * getPixelSize() * getPixelSize()) {
      // ho cliccato vicino alla curva index-esima
      assert(0 <= index && index < vi->getStrokeCount());
      m_controlPointEditorStroke.setStroke(vi, index);
      m_action = NONE;
      m_selection.makeCurrent();
    } else {
      if (m_selectType.getValue() == RECTANGULAR_WSTR) {
        m_action        = RECT_SELECTION;
        m_selectingRect = TRectD(m_pos.x, m_pos.y, m_pos.x + 1, m_pos.y + 1);
        if (m_selectingRect.x0 > m_selectingRect.x1)
          std::swap(m_selectingRect.x1, m_selectingRect.x0);
        if (m_selectingRect.y0 > m_selectingRect.y1)
          std::swap(m_selectingRect.y1, m_selectingRect.y0);

      } else if (m_selectType.getValue() == FREEHAND_WSTR) {
        m_action = FREEHAND_SELECTION;
        startFreehand(pos);
      }
    }
    m_selection.selectNone();
    return;
  }
  TVectorImageP vi = getImage(true);
  if (!vi) return;

  if (pointType == ControlPointEditorStroke::SPEED_IN ||
      pointType == ControlPointEditorStroke::SPEED_OUT) {
    bool isIn = pointType == ControlPointEditorStroke::SPEED_IN;
    m_selection.selectNone();
    m_selection.select(pointIndex);
    m_action = isIn ? IN_SPEED_MOVEMENT : OUT_SPEED_MOVEMENT;
    if (e.isAltPressed()) {
      initUndo();
      if (m_controlPointEditorStroke.isCusp(pointIndex))
        linkSpeedInOut(pointIndex);
      else
        unlinkSpeedInOut(pointIndex);
      TUndoManager::manager()->add(m_undo);
      m_undo = 0;
    }
    m_selection.makeCurrent();
  } else if (pointType == ControlPointEditorStroke::CONTROL_POINT) {
    if (e.isAltPressed()) {
      m_action = NONE;
      m_selection.selectNone();
      m_selection.select(pointIndex);
      initUndo();
      bool isSpeedIn  = m_controlPointEditorStroke.isSpeedInLinear(pointIndex);
      bool isSpeedOut = m_controlPointEditorStroke.isSpeedOutLinear(pointIndex);
      m_controlPointEditorStroke.setLinear(pointIndex,
                                           !isSpeedIn && !isSpeedOut);
      TUndoManager::manager()->add(m_undo);
      m_undo = 0;
      return;
    }
    if (e.isCtrlPressed()) {
      if (m_selection.isSelected(pointIndex))
        m_selection.unselect(pointIndex);
      else
        m_selection.select(pointIndex);
    } else if (m_selection.isEmpty() || !m_selection.isSelected(pointIndex)) {
      m_selection.selectNone();
      m_selection.select(pointIndex);
    }
    m_lastPointSelected = pointIndex;
    m_action            = CP_MOVEMENT;
    m_selection.makeCurrent();
  } else if (pointType == ControlPointEditorStroke::SEGMENT &&
             !e.isAltPressed()) {
    m_selection.selectNone();
    if (e.isCtrlPressed()) {
      // Aggiungo un punto
      initUndo();
      pointIndex = m_controlPointEditorStroke.addControlPoint(pos);
      m_selection.select(pointIndex);
      m_action = CP_MOVEMENT;
      TUndoManager::manager()->add(m_undo);
      m_lastPointSelected = -1;
      notifyImageChanged();
    } else {
      // Inizio a muovere la curva
      int precPointIndex, nextPointIndex;
      precPointIndex = pointIndex;
      nextPointIndex =
          (m_controlPointEditorStroke.isSelfLoop() &&
           precPointIndex ==
               m_controlPointEditorStroke.getControlPointCount() - 1)
              ? 0
              : precPointIndex + 1;
      if (precPointIndex > -1 && nextPointIndex > -1) {
        if (precPointIndex > nextPointIndex)
          std::swap(precPointIndex, nextPointIndex);
        m_moveSegmentLimitation.first  = precPointIndex;
        m_moveSegmentLimitation.second = nextPointIndex;
      }
      m_moveControlPointEditorStroke = *m_controlPointEditorStroke.clone();
      // Se e' premuto shift setto a cusp i due punti di controllo che
      // delimitano il segmento.
      if (e.isShiftPressed()) {
        if (!m_controlPointEditorStroke.isCusp(precPointIndex)) {
          m_controlPointEditorStroke.setCusp(precPointIndex, true, false);
          m_moveControlPointEditorStroke.setCusp(precPointIndex, true, false);
        }
        if (!m_controlPointEditorStroke.isCusp(nextPointIndex)) {
          m_controlPointEditorStroke.setCusp(nextPointIndex, true, true);
          m_moveControlPointEditorStroke.setCusp(nextPointIndex, true, true);
        }
      }
      m_action = SEGMENT_MOVEMENT;
    }
    m_selection.makeCurrent();
  }

  int currentStroke = m_controlPointEditorStroke.getStrokeIndex();
  if (currentStroke != -1) initUndo();
  invalidate();
  m_isImageChanged = false;
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::rightButtonDown(const TPointD &pos,
                                             const TMouseEvent &) {
  TVectorImageP vi = getImage(true);
  if (!vi) return;
  double maxDist  = 5 * getPixelSize();
  double maxDist2 = maxDist * maxDist;
  double dist2    = 0;
  int pointIndex;
  ControlPointEditorStroke::PointType pointType =
      m_controlPointEditorStroke.getPointTypeAt(pos, maxDist2, pointIndex);
  if (pointType != ControlPointEditorStroke::CONTROL_POINT) return;
  m_selection.select(pointIndex);
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::moveControlPoints(const TPointD &delta) {
  int i;
  int cpCount = m_controlPointEditorStroke.getControlPointCount();
  for (i = 0; i < cpCount; i++)
    if (m_selection.isSelected(i))
      m_controlPointEditorStroke.moveControlPoint(i, delta);
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::moveSpeed(const TPointD &delta, bool isIn) {
  int i;
  for (i = 0; i < m_controlPointEditorStroke.getControlPointCount(); i++)
    if (m_selection.isSelected(i))
      m_controlPointEditorStroke.moveSpeed(i, delta, isIn, 4 * getPixelSize());
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::moveSegment(const TPointD &delta, bool dragging,
                                         bool isShiftPressed) {
  int beforeIndex = m_moveSegmentLimitation.first;
  int nextIndex   = m_moveSegmentLimitation.second;
  // Se e' premuto shift setto a cusp i due punti di controllo che delimitano il
  // segmento.
  if (isShiftPressed) {
    if (!m_controlPointEditorStroke.isCusp(beforeIndex)) {
      if (dragging)
        m_moveControlPointEditorStroke.setCusp(beforeIndex, true, false);
      else
        m_controlPointEditorStroke.setCusp(beforeIndex, true, false);
    }
    if (!m_controlPointEditorStroke.isCusp(nextIndex)) {
      if (dragging)
        m_moveControlPointEditorStroke.setCusp(nextIndex, true, true);
      else
        m_controlPointEditorStroke.setCusp(nextIndex, true, true);
    }
  }
  if (dragging)
    m_moveControlPointEditorStroke.moveSegment(beforeIndex, nextIndex, delta,
                                               m_pos);
  else
    m_controlPointEditorStroke.moveSegment(beforeIndex, nextIndex, delta,
                                           m_pos);
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::leftButtonDrag(const TPointD &pos,
                                            const TMouseEvent &e) {
  TVectorImageP vi(getImage(true));
  int currentStroke = m_controlPointEditorStroke.getStrokeIndex();
  if (!vi || currentStroke == -1 || m_action == NONE) return;
  QMutexLocker lock(vi->getMutex());

  TPointD delta = pos - m_pos;

  if (m_action == CP_MOVEMENT) {
    if (!m_selection.isSelected(m_lastPointSelected) && e.isCtrlPressed())
      m_selection.select(m_lastPointSelected);  // Controllo che non venga
                                                // deselezionata l'ultima
                                                // selezione nel movimento

    if (m_lastPointSelected >= 0) {
      TThickPoint cp;
      TPointD controlPoint;
      TPointD newPos;

      cp = m_controlPointEditorStroke.getControlPoint(m_lastPointSelected);
      controlPoint = TPointD(cp.x, cp.y);
      newPos       = calculateSnap(pos);
      delta        = newPos - m_pos + (m_pos - controlPoint);
    }

    m_pos = pos;

    moveControlPoints(delta);
    m_isImageChanged = true;
  }
  if (m_action == SEGMENT_MOVEMENT) {
    m_moveControlPointEditorStroke = *m_controlPointEditorStroke.clone();
    moveSegment(delta, true, e.isShiftPressed());
    m_isImageChanged = true;
  }
  if (m_action == OUT_SPEED_MOVEMENT || m_action == IN_SPEED_MOVEMENT) {
    m_pos = pos;
    moveSpeed(delta, m_action == IN_SPEED_MOVEMENT);
    m_isImageChanged = true;
  }

  if (m_action == RECT_SELECTION) {
    int cpCount        = m_controlPointEditorStroke.getControlPointCount();
    m_selectingRect.x0 = m_pos.x;
    m_selectingRect.y0 = m_pos.y;
    m_selectingRect.x1 = pos.x;
    m_selectingRect.y1 = pos.y;
    if (m_selectingRect.x0 > m_selectingRect.x1)
      std::swap(m_selectingRect.x1, m_selectingRect.x0);
    if (m_selectingRect.y0 > m_selectingRect.y1)
      std::swap(m_selectingRect.y1, m_selectingRect.y0);
    int i;
    m_selection.selectNone();
    for (i = 0; i < cpCount; i++)
      if (m_selectingRect.contains(
              m_controlPointEditorStroke.getControlPoint(i)))
        m_selection.select(i);
  } else if (m_action == FREEHAND_SELECTION) {
    freehandDrag(pos);
  }

  invalidate();
}

//---------------------------------------------------------------------------
void ControlPointEditorTool::selectRegion(TStroke *stroke) {
  int cpCount = m_controlPointEditorStroke.getControlPointCount();

  TVectorImage img;
  img.addStroke(stroke);
  img.findRegions();
  for (int rI = 0; rI < (int)img.getRegionCount(); rI++) {
    TRegion *region = img.getRegion(rI);
    for (int i = 0; i < cpCount; i++) {
      if (region->contains(m_controlPointEditorStroke.getControlPoint(i))) {
        m_selection.select(i);
      }
    }
  }
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::leftButtonUp(const TPointD &realPos,
                                          const TMouseEvent &e) {
  TVectorImageP vi(getImage(true));
  int currentStroke = m_controlPointEditorStroke.getStrokeIndex();
  if (!vi || currentStroke == -1) return;
  QMutexLocker lock(vi->getMutex());

  TPointD pos;
  pos = getSnap(realPos);
  resetSnap();

  if (m_action == SEGMENT_MOVEMENT) {
    m_moveControlPointEditorStroke.setStroke((TVectorImage *)0, -1);
    TPointD delta = pos - m_pos;
    moveSegment(delta, false, e.isShiftPressed());
  }

  if (m_action == RECT_SELECTION || m_action == FREEHAND_SELECTION) {
    if (m_action == FREEHAND_SELECTION) {
      closeFreehand(pos);
      selectRegion(m_stroke);
      m_track.clear();
    }

    if (m_selection.isEmpty()) {
      // Non ho selezionato nulla
      if (!TTool::getApplication()
               ->getCurrentObject()
               ->isSpline())  // se non e' una spline deseleziono
        m_controlPointEditorStroke.setStroke((TVectorImage *)0, -1);
      m_action         = NONE;
      m_isImageChanged = false;
    } else {
      m_action = CP_MOVEMENT;
      m_selection.makeCurrent();
      m_isImageChanged = false;
    }
  }

  if (m_action == NONE || !m_isImageChanged) {
    m_undo = 0;
    invalidate();
    return;
  }

  notifyImageChanged();
  invalidate();

  // Registro l'UNDO
  if (m_undo) {
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::addContextMenuItems(QMenu *menu) {
  m_isMenuViewed = true;
  m_selection.addMenuItems(menu);
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::linkSpeedInOut(int index) {
  if ((index == 0 ||
       index == m_controlPointEditorStroke.getControlPointCount() - 1) &&
      !m_controlPointEditorStroke.isSelfLoop())
    return;
  if (m_action == IN_SPEED_MOVEMENT || m_action == CP_MOVEMENT)
    m_controlPointEditorStroke.setCusp(index, false, true);
  if (m_action == OUT_SPEED_MOVEMENT)
    m_controlPointEditorStroke.setCusp(index, false, false);
  invalidate();
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::unlinkSpeedInOut(int pointIndex) {
  m_controlPointEditorStroke.setCusp(pointIndex, true, true);
}

//---------------------------------------------------------------------------

bool ControlPointEditorTool::keyDown(QKeyEvent *event) {
  TVectorImageP vi(getImage(true));
  if (!vi || (vi && m_selection.isEmpty())) return false;

  TPointD delta;
  switch (event->key()) {
  case Qt::Key_Up:
    delta.y = 1;
    break;
  case Qt::Key_Down:
    delta.y = -1;
    break;
  case Qt::Key_Left:
    delta.x = -1;
    break;
  case Qt::Key_Right:
    delta.x = 1;
    break;
  default:
    return false;
    break;
  }
  // Inizializzo l'UNDO
  initUndo();

  moveControlPoints(delta);

  invalidate();
  // Registro l'UNDO
  TUndoManager::manager()->add(m_undo);

  return true;
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::onEnter() {
  TVectorImageP vi(getImage(false));
  int currentStroke = m_controlPointEditorStroke.getStrokeIndex();
  if (m_isMenuViewed) {
    m_isMenuViewed = false;
    return;
  }
  /*m_draw=true;
  if(currentStroke==-1 || !vi)
          return;

  m_controlPointEditorStroke.setStroke((TVectorImage*)0, -1);

if(TTool::getApplication()->getCurrentObject()->isSpline())
          m_controlPointEditorStroke.setStroke(vi, 0);*/
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::onLeave() {
  if (m_isMenuViewed) return;
  // m_draw=false;
}

//-----------------------------------------------------------------------------

bool ControlPointEditorTool::onPropertyChanged(std::string propertyName) {
  CPSelectionType   = ::to_string(m_selectType.getValue());
  AutoSelectDrawing = (int)(m_autoSelectDrawing.getValue());
  Snap              = (int)(m_snap.getValue());
  SnapSensitivity   = (int)(m_snapSensitivity.getIndex());
  switch (SnapSensitivity) {
  case 0:
    m_snapMinDistance = SNAPPING_LOW;
    break;
  case 1:
    m_snapMinDistance = SNAPPING_MEDIUM;
    break;
  case 2:
    m_snapMinDistance = SNAPPING_HIGH;
    break;
  }
  return true;
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::onActivate() {
  // TODO: getApplication()->editImageOrSpline();
  m_selectType.setValue(::to_wstring(CPSelectionType.getValue()));
  m_autoSelectDrawing.setValue(AutoSelectDrawing ? 1 : 0);
  m_snap.setValue(Snap ? 1 : 0);
  m_snapSensitivity.setIndex(SnapSensitivity);
  switch (SnapSensitivity) {
  case 0:
    m_snapMinDistance = SNAPPING_LOW;
    break;
  case 1:
    m_snapMinDistance = SNAPPING_MEDIUM;
    break;
  case 2:
    m_snapMinDistance = SNAPPING_HIGH;
    break;
  }
  m_controlPointEditorStroke.setStroke((TVectorImage *)0, -1);
  m_draw = true;
  resetSnap();
}

//---------------------------------------------------------------------------

void ControlPointEditorTool::onDeactivate() { m_draw = false; }

//---------------------------------------------------------------------------

void ControlPointEditorTool::onImageChanged() {
  TVectorImageP vi(getImage(false));
  if (!vi) return;

  int currentStroke = m_controlPointEditorStroke.getStrokeIndex();
  if (!vi || currentStroke == -1 ||
      m_controlPointEditorStroke.getControlPointCount() == 0 ||
      vi->getStrokeCount() == 0 || (int)vi->getStrokeCount() <= currentStroke) {
    m_controlPointEditorStroke.setStroke((TVectorImage *)0, -1);
    return;
  } else {
    if (m_controlPointEditorStroke.setStroke(vi, currentStroke))
      m_selection.selectNone();
  }
}

//---------------------------------------------------------------------------

int ControlPointEditorTool::getCursorId() const {
  if (m_viewer && m_viewer->getGuidedStrokePickerMode())
    return m_viewer->getGuidedStrokePickerCursor();

  switch (m_cursorType) {
  case NORMAL:
    return ToolCursor::SplineEditorCursor;
  case ADD:
    return ToolCursor::SplineEditorCursorAdd;
  case EDIT_SPEED:
    return ToolCursor::SplineEditorCursorSelect;
  case EDIT_SEGMENT:
    return ToolCursor::PinchCursor;
  case NO_ACTIVE:
    return ToolCursor::CURSOR_NO;
  default:
    return ToolCursor::SplineEditorCursor;
  }
}

//-----------------------------------------------------------------------------

// returns true if the pressed key is recognized and processed in the tool
// instead of triggering the shortcut command.
bool ControlPointEditorTool::isEventAcceptable(QEvent *e) {
  if (!isEnabled()) return false;
  TVectorImageP vi(getImage(false));
  if (!vi || (vi && m_selection.isEmpty())) return false;
  // arrow keys will be used for moving the selected points
  QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
  // shift + arrow will not be recognized for now
  if (keyEvent->modifiers() & Qt::ShiftModifier) return false;
  int key = keyEvent->key();
  return (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left ||
          key == Qt::Key_Right);
}

//=============================================================================

// TTool *getSplineEditorTool() {return &controlPointEditorTool;}

//=============================================================================

void ControlPointEditorTool::startFreehand(const TPointD &pos) {
  m_track.clear();
  m_firstPos       = pos;
  double pixelSize = getPixelSize();
  m_track.add(TThickPoint(pos, 0), pixelSize * pixelSize);
}

//-----------------------------------------------------------------------------

void ControlPointEditorTool::freehandDrag(const TPointD &pos) {
  double pixelSize = getPixelSize();
  m_track.add(TThickPoint(pos, 0), pixelSize * pixelSize);
}

//-----------------------------------------------------------------------------
void ControlPointEditorTool::closeFreehand(const TPointD &pos) {
  if (m_track.isEmpty()) return;
  double pixelSize = getPixelSize();
  m_track.add(TThickPoint(m_firstPos, 0), pixelSize * pixelSize);
  m_track.filterPoints();
  double error = (30.0 / 11) * pixelSize;
  m_stroke     = m_track.makeStroke(error);
  m_stroke->setStyle(1);
}

ControlPointEditorTool controlPointEditorTool;