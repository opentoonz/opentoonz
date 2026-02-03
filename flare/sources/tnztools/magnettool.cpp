

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"

#include "tstroke.h"
#include "tstrokeutil.h"
#include "tstrokedeformations.h"
#include "tmathutil.h"
#include "tproperty.h"
#include "drawutil.h"
#include "tcurveutil.h"

#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"

using namespace ToolUtils;

// For Qt translation support
#include <QCoreApplication>

//=============================================================================

namespace {

/*
 Controlla se il primo punto della prima sotto_stroke
 e' interno al cerchio, nel caso la stroke successiva
 deve essere quella esterna e cosi' via, basta allora
 fissare un contatore in modo da prendere le stroke
 alternativamente (interne/esterne).
*/
void selectStrokeToMove(const ArrayOfStroke &v, const TPointD &center,
                        double radius, ArrayOfStroke &toMove) {
  if (v.empty()) return;

  UINT counter;
  if (tdistance2(v.front()->getPoint(0), center) < sq(radius))
    counter = 0;
  else
    counter = 1;
  do {
    toMove.push_back(v[counter]);
    counter += 2;
  } while (counter < v.size());
}

//=============================================================================
/*
inline  double computeStep(const TQuadratic& quad, double invOfPixelSize)
{
  double step = std::numeric_limits<double>::max();

  TPointD A = quad.getP0() - 2.0*quad.getP1() + quad.getP2(); // 2*A is the
acceleration of the curve
  double  A_len = norm(A);
  if (A_len > 0)  step = sqrt(2 * invOfPixelSize / A_len);

  return  step;
}
*/
//-----------------------------------------------------------------------------

void drawQuadratic(const TQuadratic &quad, double pixelSize) {
  double m_min_step_at_normal_size = computeStep(quad, pixelSize);

  // It draws the curve as a linear piecewise approximation

  double invSqrtScale = 1.0;
  // First of all, it computes the control circles of the curve in screen
  // coordinates
  TPointD scP0 = quad.getP0();
  TPointD scP1 = quad.getP1();
  TPointD scP2 = quad.getP2();

  TPointD A = scP0 - 2 * scP1 + scP2;
  TPointD B = scP0 - scP1;

  double h;
  h         = invSqrtScale * m_min_step_at_normal_size;
  double h2 = h * h;

  TPointD P = scP0, D2 = 2 * h2 * A, D1 = A * h2 - 2 * B * h;

  if (h < 0 || isAlmostZero(h)) return;

  // It draws the whole curve, using forward differencing
  glBegin(GL_LINE_STRIP);  // The curve starts from scP0
  glVertex2d(scP0.x, scP0.y);

  for (double t = h; t < 1; t = t + h) {
    P  = P + D1;
    D1 = D1 + D2;
    glVertex2d(P.x, P.y);
  }

  glVertex2d(scP2.x, scP2.y);  // The curve ends in scP2
  glEnd();
}

}  // namespace

//=============================================================================
// MagnetTool
//-----------------------------------------------------------------------------

class MagnetTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(MagnetTool)

  bool m_active;

  TPointD m_startingPos;

  TPointD m_oldPos, m_pointAtMouseDown, m_pointAtMove;

  DoublePair m_extremes;
  int m_cursorId;

  TUndo *m_undo;

  typedef struct {
    TStroke *m_parent;
    ArrayOfStroke m_splitted;
    ArrayOfStroke m_splittedToMove;
  } strokeCollection;

  std::vector<strokeCollection> m_strokeToModify;
  // vector<StrokeInfo>  m_info;
  std::vector<TStroke *> m_strokeHit, m_oldStrokesArray;

  std::vector<int> m_changedStrokes;

  std::vector<std::vector<int> *> m_hitStrokeCorners, m_strokeToModifyCorners;

  TDoubleProperty m_toolSize;
  TPropertyGroup m_prop;

public:
  MagnetTool()
      : TTool("T_Magnet")
      , m_active(false)
      , m_oldStrokesArray()
      , m_toolSize("Size:", 10, 1000, 20)  // W_ToolOptions_MagnetTool
  {
    bind(TTool::Vectors);

    m_toolSize.setNonLinearSlider();

    m_prop.bind(m_toolSize);
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override { m_toolSize.setQStringName(tr("Size:")); }

  void onEnter() override {
    if ((TVectorImageP)getImage(false))
      m_cursorId = ToolCursor::MagnetCursor;
    else
      m_cursorId = ToolCursor::CURSOR_NO;
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    TPointD p(pos);

    if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
      getViewer()->doPickGuideStroke(p);
      return;
    }

    m_oldPos = pos;

    m_pointAtMouseDown = p;
    m_startingPos      = p;
    m_active           = false;
    TVectorImageP vi   = TImageP(getImage(true));
    if (!vi) return;
    QMutexLocker lock(vi->getMutex());
    m_active = true;

    m_pointAtMove = m_pointAtMouseDown = p;
    m_strokeHit.clear();
    m_changedStrokes.clear();
    m_strokeToModify.clear();

    std::vector<TStroke *> strokeUndo;

    TStroke *ref;

    m_hitStrokeCorners.clear();
    m_strokeToModifyCorners.clear();

    double pointSize = m_toolSize.getValue();

    UINT i = 0;
    for (; i < vi->getStrokeCount(); ++i) {
      if (!vi->inCurrentGroup(i)) continue;

      TStroke *stroke = vi->getStroke(i);
      ref             = stroke;
      //  calcola le intersezioni
      std::vector<double> intersections;
      intersect(*ref, p, pointSize, intersections);

      if (intersections.empty()) {
        if (increaseControlPoints(*ref,
                                  TStrokePointDeformation(p, pointSize))) {
          m_changedStrokes.push_back(i);
          m_strokeHit.push_back(ref);

          std::vector<int> *corners = new std::vector<int>;
          corners->push_back(0);
          detectCorners(ref, 20, *corners);
          corners->push_back(ref->getChunkCount());
          m_hitStrokeCorners.push_back(corners);

          ref->disableComputeOfCaches();

          strokeUndo.push_back(ref);
        }
      } else {
        strokeUndo.push_back(ref);

        MagnetTool::strokeCollection sc;

        sc.m_parent = ref;

        splitStroke(*sc.m_parent, intersections, sc.m_splitted);

        selectStrokeToMove(sc.m_splitted, p, pointSize, sc.m_splittedToMove);
        for (UINT ii = 0; ii < sc.m_splittedToMove.size(); ++ii) {
          TStroke *temp = sc.m_splittedToMove[ii];
          bool test     = increaseControlPoints(
              *temp, TStrokePointDeformation(p, pointSize));
          assert(test);

          std::vector<int> *corners = new std::vector<int>;
          corners->push_back(0);
          detectCorners(temp, 20, *corners);
          corners->push_back(temp->getChunkCount());
          m_strokeToModifyCorners.push_back(corners);
        }

        m_strokeToModify.push_back(sc);
        m_changedStrokes.push_back(i);
      }
    }

    m_oldStrokesArray.resize(m_changedStrokes.size());
    for (i = 0; i < m_changedStrokes.size(); i++)
      m_oldStrokesArray[i] = new TStroke(*(vi->getStroke(m_changedStrokes[i])));

    if (!strokeUndo.empty()) {
      if (TTool::getApplication()->getCurrentObject()->isSpline())
        m_undo = new UndoPath(
            getXsheet()->getStageObject(getObjectId())->getSpline());
      else {
        TXshSimpleLevel *sl =
            TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
        assert(sl);
        TFrameId id = getCurrentFid();
        m_undo      = new UndoModifyListStroke(sl, id, strokeUndo);
      }
    }

    invalidate();
    // vi->validateRegionEdges(ref, true);
  };

  void leftButtonDrag(const TPointD &p, const TMouseEvent &) override {
    if (!m_active) return;

    //      double dx = p.x - m_pointAtMouseDown.x;
    double pixelSize = getPixelSize();
    if (tdistance2(p, m_oldPos) < 9.0 * pixelSize * pixelSize) return;

    m_oldPos           = p;
    m_pointAtMouseDown = p;

    // double sc = exp(0.001 * (double)dx);
    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) return;
    QMutexLocker lock(vi->getMutex());
    TPointD offset = p - m_pointAtMove;

    /*
if( tdistance2(m_pointAtMouseDown, p ) > sq(m_pointSize * 0.5) ) // reincremento
{
leftButtonUp(p);
lefrightButtonDown(p);
}
*/
    double pointSize = m_toolSize.getValue();

    UINT i, j;

    for (i = 0; i < m_strokeHit.size(); ++i)
      modifyControlPoints(
          *m_strokeHit[i],
          TStrokePointDeformation(offset, m_pointAtMouseDown, pointSize * 0.7));

    for (i = 0; i < m_strokeToModify.size(); ++i)
      for (j = 0; j < m_strokeToModify[i].m_splittedToMove.size(); ++j) {
        TStroke *temp = m_strokeToModify[i].m_splittedToMove[j];
        modifyControlPoints(*temp,
                            TStrokePointDeformation(offset, m_pointAtMouseDown,
                                                    pointSize * 0.7));
      }

    m_pointAtMove = p;

    invalidate();
  };

  void mouseMove(const TPointD &p, const TMouseEvent &e) override {
    m_pointAtMove    = p;
    double pixelSize = getPixelSize();
    if (tdistance2(p, m_oldPos) < 9.0 * pixelSize * pixelSize) return;

    m_oldPos = p;
    invalidate();
  }

  void leftButtonUp(const TPointD &, const TMouseEvent &) override {
    if (!m_active) return;

    m_active           = false;
    m_pointAtMouseDown = m_pointAtMove = TConsts::napd;

    TStroke *ref;

    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) return;
    QMutexLocker lock(vi->getMutex());
    UINT i, j;

    for (i = 0; i < m_strokeHit.size(); ++i) {
      ref = m_strokeHit[i];
      ref->enableComputeOfCaches();
      ref->reduceControlPoints(getPixelSize() * ReduceControlPointCorrection,
                               *(m_hitStrokeCorners[i]));
      // vi->validateRegionEdges(ref, false);
    }
    clearPointerContainer(m_hitStrokeCorners);
    m_hitStrokeCorners.clear();

    UINT count = 0;
    for (i = 0; i < m_strokeToModify.size(); ++i) {
      // recupero la stroke collection
      MagnetTool::strokeCollection &sc = m_strokeToModify[i];

      for (j = 0; j < sc.m_splittedToMove.size(); ++j) {
        ref = sc.m_splittedToMove[j];
        ref->enableComputeOfCaches();
        ref->reduceControlPoints(getPixelSize() * ReduceControlPointCorrection,
                                 *(m_strokeToModifyCorners[count++]));
      }

      // ricostruisco una stroke con quella data
      ref = merge(sc.m_splitted);

      if (sc.m_parent->isSelfLoop()) {
        int cpCount      = ref->getControlPointCount();
        TThickPoint p1   = ref->getControlPoint(0);
        TThickPoint p2   = ref->getControlPoint(cpCount - 1);
        TThickPoint midP = (p1 + p2) * 0.5;
        ref->setControlPoint(0, midP);
        ref->setControlPoint(cpCount - 1, midP);
        ref->setSelfLoop(true);
      }

      sc.m_parent->swapGeometry(*ref);

      delete ref;  // elimino la curva temporanea
      clearPointerContainer(
          sc.m_splitted);           // pulisco le stroke trovate con lo split
      sc.m_splittedToMove.clear();  // pulisco il contenitore ( le stroke
                                    // che erano contenute qua sono state
                                    // eliminate nella clearPointer....
    }
    clearPointerContainer(m_strokeToModifyCorners);
    m_strokeToModifyCorners.clear();

    for (i = 0; i < vi->getStrokeCount(); ++i) {
      ref = vi->getStroke(i);
      ref->invalidate();
    }

    vi->notifyChangedStrokes(m_changedStrokes, m_oldStrokesArray);
    notifyImageChanged();
    if (m_undo) TUndoManager::manager()->add(m_undo);
    m_undo = 0;
    clearPointerContainer(m_oldStrokesArray);
    m_oldStrokesArray.clear();
    invalidate();
  };

  void draw() override {
    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) return;

    // TAffine viewMatrix = getViewer()->getViewMatrix();
    // glPushMatrix();
    // tglMultMatrix(viewMatrix);

    double pointSize = m_toolSize.getValue();

    tglColor(TPixel32::Red);
    tglDrawCircle(m_pointAtMove, pointSize);

    if (!m_active) {
      // glPopMatrix();
      return;
    }

    TPointD delta = m_pointAtMouseDown - m_startingPos;

    //    glPushMatrix();
    // tglMultMatrix(m_referenceMatrix);

    if (!m_strokeHit.empty())
      for (UINT i = 0; i < m_strokeHit.size(); ++i)
        drawStrokeCenterline(*m_strokeHit[i], getPixelSize());

    tglColor(TPixel32::Red);
    UINT i, j;

    for (i = 0; i < m_strokeToModify.size(); ++i)
      for (j = 0; j < m_strokeToModify[i].m_splittedToMove.size(); ++j) {
        TStroke *temp = m_strokeToModify[i].m_splittedToMove[j];
        drawStrokeCenterline(*temp, getPixelSize());
      }

    //    glPopMatrix();
    // glPopMatrix();
  }

  void onActivate() override {
    //        getApplication()->editImageOrSpline();
  }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return m_cursorId;
  }

  bool onPropertyChanged(std::string propertyName) override {
    if (propertyName == m_toolSize.getName()) {
      invalidate();
    }

    return true;
  }

} magnetTool;

// TTool *getMagnetTool() {return &magnetTool;}
