

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"

#include "tstrokeutil.h"
#include "tmathutil.h"
#include "tgl.h"
#include "tstroke.h"

#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"

#ifdef _DEBUG
#include "tcolorstyles.h"
#include "tsimplecolorstyles.h"
#include "tstrokeoutline.h"
#include "tpalette.h"
#endif

using namespace ToolUtils;

namespace {

const int MY_ERROR = -1;

//=============================================================================
// Iron
//-----------------------------------------------------------------------------

// index must be between 0 and controlPointCount-1
inline bool isIncluded(int index, int left, int rigth) {
  if (left < rigth) {
    return left <= index && index <= rigth;
  } else {
    return left <= index || index <= rigth;
  }
}

//=============================================================================
// IronTool
//-----------------------------------------------------------------------------

class IronTool final : public TTool {
  TStroke *m_strokeRef, *m_oldStroke;
  TUndo *m_undo;

  DoublePair m_range;

  bool m_draw;
  bool m_dragged;

  TThickPoint m_cursor;
  int m_selectedStroke;
  TPointD m_beginPoint;

  int m_cpIndexMin, m_cpIndexMax;

  bool m_active;
  int m_cursorId;

public:
  IronTool()
      : TTool("T_Iron")
      , m_strokeRef(0)
      , m_draw(false)
      , m_active(false)
      , m_dragged(false)
      , m_undo(0)
      , m_cpIndexMin(-1)
      , m_cpIndexMax(-1)
      , m_oldStroke(0)
      , m_cursorId(ToolCursor::IronCursor) {
    bind(TTool::Vectors);
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void draw() override {
    if (m_draw && (TVectorImageP)getImage(false)) {
      glColor3d(1, 0, 1);
      if (m_cursor.thick > 0) tglDrawCircle(m_cursor, m_cursor.thick);
      tglDrawCircle(m_cursor, m_cursor.thick + 4 * getPixelSize());
    }
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_active) return;
    assert(m_undo == 0);
    m_active = false;

    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) return;

    // select nearest stroke and finds its parameter
    double dist;
    UINT stroke;

    if (!vi->getNearestStroke(pos, m_range.first, stroke, dist)) {
      m_strokeRef      = 0;
      m_selectedStroke = MY_ERROR;
      m_draw           = false;
    } else {
      m_draw           = true;
      m_active         = true;
      m_strokeRef      = vi->getStroke(stroke);
      m_selectedStroke = stroke;
      m_beginPoint     = m_strokeRef->getPoint(m_range.first);
      m_oldStroke      = new TStroke(*vi->getStroke(stroke));

      m_range.second = m_range.first;

      if (TTool::getApplication()->getCurrentObject()->isSpline())
        m_undo = new UndoPath(
            getXsheet()->getStageObject(getObjectId())->getSpline());
      else {
        TXshSimpleLevel *sl =
            TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
        assert(sl);
        TFrameId id = getCurrentFid();
        m_undo      = new UndoModifyStrokeAndPaint(sl, id, stroke);
      }
    }

    if (m_strokeRef) m_cpIndexMin = m_strokeRef->getControlPointCount();
    m_cpIndexMax                  = -1;

    invalidate();
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    TVectorImageP vi(getImage(true));
    if (!m_active || !vi || !m_strokeRef) {
      delete m_undo;
      m_undo = 0;
      return;
    }

    m_range.second = m_strokeRef->getW(pos);
    m_cursor       = m_strokeRef->getThickPoint(m_range.second);

    double v0        = std::min(m_range.first, m_range.second);
    double v1        = std::max(m_range.first, m_range.second);
    const double eps = 0.005;

    if (v1 - v0 < eps && !(m_strokeRef->isSelfLoop() && 1 - (v1 - v0) < eps)) {
      invalidate();
      return;
    }

    TPointD point2 = m_strokeRef->getPoint(m_range.second);
    double tdist2  = tdistance2(m_beginPoint, point2);

    double pixelSize = getPixelSize();
    if (tdist2 < 100 * pixelSize * pixelSize) {
      invalidate();
      return;
    }

    double draggedStrokeLen = m_strokeRef->getLength(v0, v1);

    double totalStrokeLen = m_strokeRef->getLength();

    bool direction = !m_strokeRef->isSelfLoop() ||
                     draggedStrokeLen < totalStrokeLen - draggedStrokeLen;

    if (!direction) {
      draggedStrokeLen = totalStrokeLen - draggedStrokeLen;
      std::swap(v0, v1);
    }

    // se la lunghezza della parte di stroke tra i due punti di inizio e fine
    // drag
    // e' piu' del quadruplo della distanza tra i due punti, allora non si fa
    // nulla perche'
    // molto probabilmente si sta facendo drag tra due punti vicini di una curva
    // piu' lunga
    if ((draggedStrokeLen * draggedStrokeLen) > 16.0 * tdist2) {
      m_beginPoint = point2;
      return;
    }

    int i;
    int maxCP = m_strokeRef->getControlPointCount() - 1;

    // iCP0 is the control point index before v0 (or just on v0)
    int iCP0 = (int)(maxCP * v0);
    int iCP1 = m_strokeRef->getControlPointIndexAfterParameter(v1);

    if (iCP0 > maxCP) {
      if (!direction)
        iCP0 = maxCP;
      else {
        invalidate();
        return;
      }
    }

    if (iCP1 > maxCP) iCP1 = maxCP;

    if (direction && (iCP1 - iCP0) < 2) {
      invalidate();
      return;
    }

    if (!direction && maxCP + 1 - (iCP1 - iCP0) < 3) {
      invalidate();
      return;
    }

    //*********************************************** point of no return
    //**************************
    m_dragged    = true;
    m_beginPoint = point2;

    if (!m_strokeRef->isSelfLoop()) {
      if (m_cpIndexMin > iCP0) m_cpIndexMin = iCP0;

      if (m_cpIndexMax < iCP1) m_cpIndexMax = iCP1;
    } else {
      if (m_cpIndexMin == m_strokeRef->getControlPointCount() &&
          m_cpIndexMax == -1) {
        m_cpIndexMin = iCP0;
        m_cpIndexMax = iCP1;
      } else {
        if (isIncluded(iCP0, m_cpIndexMin, m_cpIndexMax)) {
          if (!isIncluded(iCP1, m_cpIndexMin, m_cpIndexMax)) {
            m_cpIndexMax = iCP1;
          }
        } else if (isIncluded(iCP1, m_cpIndexMin, m_cpIndexMax)) {
          assert(!isIncluded(iCP0, m_cpIndexMin, m_cpIndexMax));
          m_cpIndexMin = iCP0;
        } else {
          int sgn             = tsign(m_range.second - m_range.first);
          if (!direction) sgn = -sgn;

          switch (sgn) {
          case 1:
            m_cpIndexMax = iCP1;
            break;
          case -1:
            m_cpIndexMin = iCP0;
            break;
          case 0:
            assert(0);
          default:
            assert(0);
          }
        }
      }
    }

    m_range.first = m_range.second;

    assert(m_cpIndexMin != m_cpIndexMax);

    v0 = m_strokeRef->getParameterAtControlPoint(iCP0);
    v1 = m_strokeRef->getParameterAtControlPoint(iCP1);

    // double smoothFactor =
    // getApplication()->getVectorToolsParameters().getToolSize();
    // smoothFactor *= 0.01;
    // smoothFactor = (smoothFactor*smoothFactor*smoothFactor)*0.7;

    const double smoothFactor = 0.08;

    double wDistance = (direction) ? v1 - v0 : 1 - (v0 - v1);

    double smoothFP = smoothFactor / wDistance;

    TPointD pf0 = m_strokeRef->getControlPoint(iCP0) * smoothFP;
    TPointD pf1 = m_strokeRef->getControlPoint(iCP1) * smoothFP;

    TPointD appDPoint;
    TThickPoint appThickPoint;

    double oppSmoothFactor = 1.0 - smoothFactor;
    double vp, v1vp, vpv0;

    i = (iCP0 == maxCP || (iCP0 == 0 && !direction)) ? 1 : iCP0 + 1;

    for (; i != iCP1;) {
      vp = m_strokeRef->getParameterAtControlPoint(i);

      appThickPoint = m_strokeRef->getControlPoint(i);
      appDPoint     = appThickPoint;

      v1vp = v1 - vp;
      if (v1vp < 0) {
        if (!direction)
          v1vp += 1;
        else
          assert(0);
      }

      vpv0 = vp - v0;
      if (vpv0 < 0) {
        if (!direction)
          vpv0 += 1;
        else
          assert(0);
      }

      assert(isAlmostZero(v1vp + vpv0 - wDistance));

      assert(isAlmostZero((smoothFP * v1vp + smoothFP * vpv0) +
                          oppSmoothFactor - 1.0));

      m_strokeRef->setControlPoint(
          i, TThickPoint(pf0 * v1vp + pf1 * vpv0 + appDPoint * oppSmoothFactor,
                         appThickPoint.thick));
      // this is like
      //
      // averageP = ( p0*(v1-vp)/(v1-v0) + p1*(vp-v0)/(v1-v0));
      // newCP(i) = oldCP(i)*(1-smoothFactor) + averageP*smoothFactor
      //
      // but faster

      i++;
      if (i == maxCP + 1) {
        m_strokeRef->setControlPoint(0, m_strokeRef->getControlPoint(maxCP));
        i = 1;
      }
    }

    m_strokeRef->invalidate();
    invalidate();
    notifyImageChanged();
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {
    if (!m_active) return;

    m_active = false;

    invalidate();

    TVectorImageP vi(getImage(true));
    if (!vi || !m_strokeRef || !m_dragged) {
      delete m_undo;
      m_undo = 0;
      return;
    }
    QMutexLocker lock(vi->getMutex());

    // TStroke *oldStroke = new TStroke(*(vi->getStroke(m_selectedStroke)));

    //------------------------------------------------------------------------------

    UINT cpCount = m_strokeRef->getControlPointCount();

    m_cpIndexMin &= ~1;  // nearest even value less or equal to m_cpIndexMin

    if (m_cpIndexMax & 1)  // odd
      m_cpIndexMax++;

    int i      = 0;
    UINT count = 0;

    if ((m_cpIndexMin == 0 && (UINT)m_cpIndexMax == cpCount - 1) ||
        (m_cpIndexMin == m_cpIndexMax && m_strokeRef->isSelfLoop())) {
      /*
e' meglio rilevarli i corners.
Tanto se hai appiattito abbastanza, se ne vanno da soli,
senza lasciare ingrati compiti alla reuceControlPoints
Altrimenti non si fa altro che aumentarli i punti di controllo
*/
      std::vector<int> corners;
      corners.push_back(0);
      detectCorners(m_strokeRef, 45, corners);
      corners.push_back(m_strokeRef->getChunkCount());
      m_strokeRef->reduceControlPoints(2.0 * getPixelSize(), corners);
    } else {
      if (m_cpIndexMin < m_cpIndexMax) {
        std::vector<TThickPoint> hitPoints(m_cpIndexMax - m_cpIndexMin + 1);
        count = 0;
        for (i = m_cpIndexMin; i <= m_cpIndexMax; i++) {
          hitPoints[count++] = m_strokeRef->getControlPoint(i);
        }
        TStroke *newStroke = new TStroke(hitPoints);

        std::vector<int> corners;
        corners.push_back(0);
        detectCorners(newStroke, 45, corners);
        corners.push_back(newStroke->getChunkCount());
        newStroke->reduceControlPoints(2.0 * getPixelSize(), corners);

        hitPoints.resize(m_cpIndexMin + newStroke->getControlPointCount() +
                         cpCount - 1 - m_cpIndexMax);

        count = 0;

        for (i = 0; i < m_cpIndexMin; i++) {
          hitPoints[count++] = m_strokeRef->getControlPoint(i);
        }

        for (i = 0; i < newStroke->getControlPointCount(); i++) {
          hitPoints[count++] = newStroke->getControlPoint(i);
        }

        for (i = m_cpIndexMax + 1; i < (int)cpCount; i++) {
          hitPoints[count++] = m_strokeRef->getControlPoint(i);
        }

        m_strokeRef->reshape(&hitPoints[0], hitPoints.size());
        delete newStroke;
      } else {
        assert(m_cpIndexMin != m_cpIndexMax);

        std::vector<TThickPoint> hitPoints(cpCount - m_cpIndexMin);
        count = 0;
        for (i = m_cpIndexMin; i < (int)cpCount; i++) {
          hitPoints[count++] = m_strokeRef->getControlPoint(i);
        }
        TStroke *newStroke1 = new TStroke(hitPoints);

        std::vector<int> corners;
        corners.push_back(0);
        detectCorners(newStroke1, 45, corners);
        corners.push_back(newStroke1->getChunkCount());
        newStroke1->reduceControlPoints(2.0 * getPixelSize(), corners);

        hitPoints.resize(m_cpIndexMax + 1);
        count = 0;
        for (i = 0; i <= m_cpIndexMax; i++) {
          hitPoints[count++] = m_strokeRef->getControlPoint(i);
        }
        TStroke *newStroke2 = new TStroke(hitPoints);

        corners.clear();
        corners.push_back(0);
        detectCorners(newStroke2, 45, corners);
        corners.push_back(newStroke2->getChunkCount());
        newStroke2->reduceControlPoints(2.0 * getPixelSize(), corners);

        hitPoints.resize(newStroke1->getControlPointCount() +
                         newStroke2->getControlPointCount() + m_cpIndexMin -
                         m_cpIndexMax - 1);

        count = 0;
        for (i = 0; i < newStroke2->getControlPointCount(); i++) {
          hitPoints[count++] = newStroke2->getControlPoint(i);
        }
        for (i = m_cpIndexMax + 1; i < m_cpIndexMin; i++) {
          hitPoints[count++] = m_strokeRef->getControlPoint(i);
        }
        for (i = 0; i < newStroke1->getControlPointCount(); i++) {
          hitPoints[count++] = newStroke1->getControlPoint(i);
        }

        m_strokeRef->reshape(&hitPoints[0], hitPoints.size());
        delete newStroke1;
        delete newStroke2;
      }
    }

    invalidate();

    m_dragged = false;
    assert(m_undo);

    assert(m_oldStroke);
    vi->notifyChangedStrokes(m_selectedStroke, m_oldStroke);

    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
    delete m_oldStroke;
  }

  void invalidateCursorArea() {
    double r = m_cursor.thick + 6;
    TPointD d(r, r);
    invalidate(TRectD(m_cursor - d, m_cursor + d));
  }

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override {
    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) {
      m_draw = false;
      return;
    }

    // select nearest stroke and finds its parameter
    double dist, pW;
    UINT stroke;

    if (vi->getNearestStroke(pos, pW, stroke, dist)) {
      m_draw             = true;
      TStroke *strokeRef = vi->getStroke(stroke);
      m_cursor           = strokeRef->getThickPoint(pW);
    } else {
      m_draw = false;
    }

    invalidate();
  }

  bool moveCursor(const TPointD &pos) { return false; }

  void onActivate() override {
    //      getApplication()->editImageOrSpline();
  }

  void onLeave() override { m_draw = false; }

  int getCursorId() const override { return m_cursorId; }
  void onEnter() override {
    m_draw = true;
    if ((TVectorImageP)getImage(false))
      m_cursorId = ToolCursor::IronCursor;
    else
      m_cursorId = ToolCursor::CURSOR_NO;
  }

} ironTool;

//-----------------------------------------------------------------------------

#ifdef _DEBUG

void drawPoint(const TPointD &p, double pixelSize) {
  double sizeX = pixelSize;
  double sizeY = pixelSize;
  glBegin(GL_QUADS);
  glVertex2d(p.x - sizeX, p.y - sizeY);
  glVertex2d(p.x - sizeX, p.y + sizeY);
  glVertex2d(p.x + sizeX, p.y + sizeY);
  glVertex2d(p.x + sizeX, p.y - sizeY);
  glEnd();
  return;
}

//-----------------------------------------------------------------------------

void drawControlPoints(const TStroke *&stroke, double pixelSize) {
  int n = stroke->getChunkCount();
  int i = 0;
  for (i = 0; i < n; ++i) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    TPointD p0                   = chunk->getP0();
    TPointD p1                   = chunk->getP1();
    glColor3d(1.0, 0.0, 0.0);
    drawPoint(p0, pixelSize);
    glColor3d(1.0, 1.0, 1.0);
    drawPoint(p1, pixelSize);
  }
  const TThickQuadratic *chunk = stroke->getChunk(n - 1);
  glColor3d(1.0, 0.0, 0.0);
  TPointD p2 = chunk->getP2();
  drawPoint(p2, pixelSize);
  return;
}
// end solo per debug
#endif

}  // namespace

// TTool *getIronTool() {return &ironTool;}
