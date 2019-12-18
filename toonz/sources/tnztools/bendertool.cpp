

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tstroke.h"
#include "tstrokeutil.h"
#include "tstrokedeformations.h"
#include "tmathutil.h"
#include "tools/cursors.h"
#include "drawutil.h"

#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"

using namespace ToolUtils;

//=============================================================================
namespace {

const UINT MAX_SAMPLE = 2;
const int MY_ERROR    = -1;

const double c_LengthOfBenderRegion = 10.0;

const int IS_BEGIN = 0;
const int IS_END   = 1;
const int IS_ALL   = 2;

//-----------------------------------------------------------------------------

double _extractFirst(DoublePair val) { return val.first; }

//-----------------------------------------------------------------------------

double _extractSecond(DoublePair val) { return val.second; }

//-----------------------------------------------------------------------------

bool strokeIsConnected(const TStroke &s, double toll = TConsts::epsilon) {
  bool out          = true;
  int count         = s.getChunkCount();
  long double toll2 = sq(TConsts::epsilon);
  if (count > 0) {
    const TThickQuadratic *refTQ = s.getChunk(0);

    for (int i = 1; i < count; ++i) {
      if (out && tdistance2(refTQ->getP2(), s.getChunk(i)->getP0()) > toll2)
        out = false;
    }
  }
  return out;
}

//-----------------------------------------------------------------------------

inline bool strokeIsConnected(const TStroke *s,
                              double toll = TConsts::epsilon) {
  assert(s);
  return strokeIsConnected(*s, toll);
}

//-----------------------------------------------------------------------------

/*
Extract an value of a pair.
*/
void extract(const std::vector<DoublePair> &s, ArrayOfDouble &d,
             bool isFirstOrSecond = true) {
  if (isFirstOrSecond)
    std::transform(s.begin(), s.end(), back_inserter(d), _extractFirst);
  else
    std::transform(s.begin(), s.end(), back_inserter(d), _extractSecond);
}

//-----------------------------------------------------------------------------

double retrieveInitLength(double strokeLength, int beginEndOrAll) {
  double initLength = MY_ERROR;
  switch (beginEndOrAll) {
  case IS_BEGIN:
    initLength = 0.0;
    break;
  case IS_END:
    initLength = strokeLength;
    break;
  case IS_ALL:
    initLength = strokeLength * 0.5;
    break;
  }
  return initLength;
}

//-----------------------------------------------------------------------------

inline bool isOdd(UINT val) { return val & 1 ? true : false; }

inline bool isEven(UINT val) { return !isOdd(val); }

void clearPointerMap(std::map<TStroke *, std::vector<int> *> &corners) {
  std::map<TStroke *, std::vector<int> *>::iterator it = corners.begin();
  for (; it != corners.end(); ++it) {
    delete it->second;
  }
}

//=============================================================================
// Bender Tool
//-----------------------------------------------------------------------------

class BenderTool final : public TTool {
private:
  TUndo *m_undo;
  bool m_atLeastOneIsChanged;

  std::vector<bool> m_directionIsChanged;
  std::vector<TPointD> m_accumulator;

  void increaseCP(TStroke *, int);
  bool m_active;
  int m_cursor;
  enum PNT_SELECTED { BNDR_NULL = 0, BNDR_P0 = 1, BNDR_P1 = 2 };

  struct benderStrokeInfo {
    TStroke *m_stroke;
    DoublePair m_extremes;
    int m_isBeginEndOrAll;

    benderStrokeInfo(TStroke *stroke, DoublePair &info, int isBeginEndOrAll) {
      m_stroke          = stroke;
      m_extremes        = info;
      m_isBeginEndOrAll = isBeginEndOrAll;
    }
  };
  std::vector<benderStrokeInfo> m_info;

  // contains information about stroke which have
  //  intersection with benderSegment
  std::map<TStroke *, ArrayOfStroke> m_metaStroke;
  std::map<TStroke *, std::vector<int> *> m_hitStrokeCorners;

  bool m_showTangents;

  // value selected
  int m_buttonDownCounter;

  TSegment m_benderSegment;

  TPointD m_prevPoint;

  double m_rotationVersus;

  //!
  ArrayOfStroke m_strokesToRotate;
  //!
  ArrayOfStroke m_strokesToBend;

  std::vector<int> m_changedStrokes;

  bool m_enableDragSelection;

  void findCurves(TVectorImageP &);
  void findVersus(const TPointD &p);
  double computeRotationVersus(const TPointD &, const TPointD &);

public:
  void initBenderAction(TVectorImageP &, const TPointD &);
  // BenderTool(TVectorImageP  vimage, GLTestWidget* ref );
  BenderTool();

  virtual ~BenderTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void draw() override;
  void leftButtonDown(const TPointD &, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &, const TMouseEvent &) override;
  void leftButtonUp(const TPointD &, const TMouseEvent &) override;
  void onEnter() override;

  void onActivate() override {
    m_buttonDownCounter = 1;
    m_prevPoint         = TConsts::napd;
    m_benderSegment.setP0(TConsts::napd);
    m_benderSegment.setP1(TConsts::napd);
    m_metaStroke.clear();
    clearPointerMap(m_hitStrokeCorners);
    m_hitStrokeCorners.clear();
  }

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return m_cursor;
  }
} BenderTool;

//-----------------------------------------------------------------------------

BenderTool::BenderTool()
    : TTool("T_Bender")
    , m_showTangents(false)
    , m_buttonDownCounter(1)
    , m_rotationVersus(0.0)
    , m_enableDragSelection(false)
    , m_undo(0)
    , m_cursor(ToolCursor::BenderCursor) {
  bind(TTool::Vectors);
  m_prevPoint = TConsts::napd;
  m_benderSegment.setP0(TConsts::napd);
  m_benderSegment.setP1(TConsts::napd);
}

//-----------------------------------------------------------------------------

BenderTool::~BenderTool() {}

//-----------------------------------------------------------------------------

void BenderTool::onEnter() {
  if ((TVectorImageP)getImage(false))
    m_cursor = ToolCursor::BenderCursor;
  else
    m_cursor = ToolCursor::CURSOR_NO;
}

//-----------------------------------------------------------------------------

void BenderTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  m_active         = false;
  TVectorImageP vi = TImageP(getImage(true));
  if (!vi) return;
  QMutexLocker lock(vi->getMutex());

  m_active = true;

  std::vector<TStroke *> oldStrokesArray(m_changedStrokes.size());

  int i;
  for (i               = 0; i < (int)m_changedStrokes.size(); i++)
    oldStrokesArray[i] = new TStroke(*(vi->getStroke(m_changedStrokes[i])));

  if (3 == m_buttonDownCounter) {
    m_rotationVersus = 0.0;

    // hide bender tool
    m_prevPoint = TConsts::napd;
    m_benderSegment.setP0(TConsts::napd);
    m_benderSegment.setP1(TConsts::napd);

    typedef std::map<TStroke *, ArrayOfStroke>::iterator itAATS;
    // UINT count=0;

    for (itAATS it1 = m_metaStroke.begin(); it1 != m_metaStroke.end(); ++it1) {
      UINT i;
      // copy vector and...
      ArrayOfStroke &refA = it1->second;
      TStroke *s          = it1->first;

      TStroke *out = merge(refA);

      out->reduceControlPoints(getPixelSize(), *(m_hitStrokeCorners[s]));

      if (it1->first->isSelfLoop()) {
        int cpCount      = out->getControlPointCount();
        TThickPoint p1   = out->getControlPoint(0);
        TThickPoint p2   = out->getControlPoint(cpCount - 1);
        TThickPoint midP = (p1 + p2) * 0.5;
        out->setControlPoint(0, midP);
        out->setControlPoint(cpCount - 1, midP);
        out->setSelfLoop(true);
      }

      it1->first->swap(*out);
      it1->first->setStyle(out->getStyle());
      it1->first->outlineOptions() = out->outlineOptions();
      it1->first->invalidate();

      for (i = 0; i < refA.size(); ++i) delete refA[i];

      delete out;
    }

    m_metaStroke.clear();
    clearPointerMap(m_hitStrokeCorners);
    m_hitStrokeCorners.clear();

    m_buttonDownCounter = 1;

    UINT lastCpIndex, i, size = m_changedStrokes.size();
    TStroke *stroke;
    TThickPoint p1, p2, middleP;

    double loopError = 0.5 * getPixelSize();

    for (i = 0; i < size; i++) {
      stroke = vi->getStroke(m_changedStrokes[i]);
      if (m_directionIsChanged[i]) stroke->changeDirection();

      lastCpIndex = stroke->getControlPointCount() - 1;
      p1          = stroke->getControlPoint(0);
      p2          = stroke->getControlPoint(lastCpIndex);
      if (isAlmostZero(p1.x - p2.x, loopError) &&
          isAlmostZero(p1.y - p2.y, loopError)) {
        middleP = (p1 + p2) * 0.5;
        stroke->setControlPoint(0, middleP);
        stroke->setControlPoint(lastCpIndex, middleP);
      } else {
        stroke->setSelfLoop(false);
      }
    }

    vi->notifyChangedStrokes(m_changedStrokes, oldStrokesArray);
    notifyImageChanged();

    if (m_undo) TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }
  clearPointerContainer(oldStrokesArray);
  m_active = false;
  invalidate();
}

//-----------------------------------------------------------------------------

void BenderTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  if (!m_active) return;
  TVectorImageP vi = TImageP(getImage(true));
  if (!vi) return;
  QMutexLocker lock(vi->getMutex());

  double pixelSize = getPixelSize();
  if (tdistance2(pos, m_prevPoint) < 9.0 * pixelSize * pixelSize) return;

  if (m_buttonDownCounter < 3) return;
  if (m_enableDragSelection) {
    m_accumulator.push_back(pos);

    if (m_accumulator.size() < 3) return;

    TPointD middlePnt;
    accumulate(m_accumulator.begin(), m_accumulator.end(), middlePnt);
    static const double inv_of_3 = 1.0 / 2.0;

    middlePnt = inv_of_3 * middlePnt;

    m_accumulator.clear();

    initBenderAction(vi, pos + middlePnt);
    m_enableDragSelection = false;
    m_buttonDownCounter   = 3;
  }

  TPointD p = pos;

  TPointD vc(p - m_benderSegment.getP0()),        // current vector
      vp(m_prevPoint - m_benderSegment.getP0());  // previous vector

  TPointD s2v = m_benderSegment.getSpeed();

  // some check to jump invalid case
  double norm2BenderSeg    = norm2(s2v);
  double norm2CurrentVect  = norm2(vc);
  double norm2PreviousVect = norm2(vp);

  // invalid segments
  if (0.0 == norm2BenderSeg || 0.0 == norm2PreviousVect ||
      0.0 == norm2CurrentVect)
    return;

  // invalid rotation versus
  if (tsign(cross(s2v, vc)) != m_rotationVersus) return;

  // compute delta of rotation angle
  double diff = asin(cross(normalize(vp), normalize(vc)));

  // make a rototranslation matrix
  TRotation rot(m_benderSegment.getP0(), rad2degree(diff));

  // rotate references
  for (itAS its = m_strokesToRotate.begin(); its != m_strokesToRotate.end();
       ++its)
    (*its)->transform(rot);

  // deform strokes
  for (UINT i = 0; i < m_info.size(); ++i) {
    TStroke &ref = *m_info[i].m_stroke;

    // DoublePair  &extr   = m_info[i].m_extremes;
    double strokeLength = ref.getLength();
    double initLength   = retrieveInitLength(
        strokeLength, m_info[i].m_isBeginEndOrAll);  // ? 0.0 : strokeLength;

    if (MY_ERROR == initLength) return;

    int innerOrOuter = m_info[i].m_isBeginEndOrAll == IS_ALL
                           ? TStrokeBenderDeformation::OUTER
                           : TStrokeBenderDeformation::INNER;

    TStrokeBenderDeformation def(&ref, m_benderSegment.getP0(), diff,
                                 initLength, innerOrOuter, strokeLength);

    modifyControlPoints(ref, def);
  }

  // fix previous point
  m_prevPoint = p;
  invalidate();
}

//-----------------------------------------------------------------------------

void BenderTool::leftButtonDown(const TPointD &p, const TMouseEvent &) {
  if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
    getViewer()->doPickGuideStroke(p);
    return;
  }

  m_active         = false;
  TVectorImageP vi = TImageP(getImage(true));
  if (!vi) return;
  QMutexLocker lock(vi->getMutex());
  m_active = true;

  switch (m_buttonDownCounter) {
  case 1:
    findCurves(vi);
    m_strokesToRotate.clear();

    // le curve puntate sono state eliminate dalla clear precedente
    m_info.clear();
    m_benderSegment.setP0(p);
    m_benderSegment.setP1(p);
    break;

  case 2:  // second buttonDown
    m_prevPoint = p;
    m_benderSegment.setP1(p);
    m_enableDragSelection = true;
    break;
    /*
case 3: // third buttonDown
// initBenderAction( vi, p );
break;
*/
  }

  ++m_buttonDownCounter;
  invalidate();
  // vi->validateRegionEdges(vi->getStroke( m_strokeIndex ), true);
}

//-----------------------------------------------------------------------------

void BenderTool::findVersus(const TPointD &p) {
  TPointD v1 = m_benderSegment.getSpeed(), v2 = p - m_benderSegment.getP0();

  // fix rotation versus
  m_rotationVersus = tsign(cross(v1, v2));

  if (isAlmostZero(m_rotationVersus)) m_rotationVersus = 1.0;
}

//-----------------------------------------------------------------------------

inline void one_minus_x(double &x) { x = 1.0 - x; }

//-----------------------------------------------------------------------------

void BenderTool::findCurves(TVectorImageP &vi) {
  ArrayOfStroke strokeToModify;
  m_changedStrokes.clear();
  m_directionIsChanged.clear();

  UINT j;

  for (UINT i = 0; i < vi->getStrokeCount(); ++i)  // for all stroke in image
  {
    if (!vi->inCurrentGroup(i)) continue;

    TStroke *s = vi->getStroke(i);  // a useful reference

    std::vector<DoublePair> pair_intersection;  // informations about extremes

    // if there is interesection between stroke and bender tool

    if (intersect(*s, m_benderSegment, pair_intersection)) {
      if (s->isSelfLoop()) {
        // make the semgnet longer
        // such as the points are just a littleBit
        // outside the stroke bbox
        const double littleBit = 0.1;

        TRectD bbox = s->getBBox();

        TPointD bboxP0 = bbox.getP00();
        TPointD bboxP1 = bbox.getP11();
        double bboxX0  = std::min(bboxP0.x, bboxP1.x);
        double bboxX1  = std::max(bboxP0.x, bboxP1.x);
        double bboxY0  = std::min(bboxP0.y, bboxP1.y);
        double bboxY1  = std::max(bboxP0.y, bboxP1.y);

        TSegment segment;
        TPointD pp0 = m_benderSegment.getP0();
        TPointD pp1 = m_benderSegment.getP1();

        TPointD newP0;
        TPointD newP1;

        if (bbox.contains(pp1)) {
          double x = (pp0.x < pp1.x) ? bboxX1 : bboxX0;
          double y = (pp0.y < pp1.y) ? bboxY1 : bboxY0;

          double t;
          if (pp1.x == pp0.x && pp1.y == pp0.y) {
            assert(!"segmento del bender puntiforme");
            return;
          }

          if (pp1.x == pp0.x)
            t = (y - pp0.y) / (pp1.y - pp0.y);
          else if (pp1.y == pp0.y)
            t = (x - pp0.x) / (pp1.x - pp0.x);
          else
            t = std::min((x - pp0.x) / (pp1.x - pp0.x),
                         (y - pp0.y) / (pp1.y - pp0.y));

          newP1 = (t > 1) ? m_benderSegment.getPoint(t + littleBit) : pp1;
        } else
          newP1 = pp1;

        if (bbox.contains(pp0)) {
          double x = (pp0.x > pp1.x) ? bboxX1 : bboxX0;
          double y = (pp0.y > pp1.y) ? bboxY1 : bboxY0;

          double t;
          if (pp1.x == pp0.x && pp1.y == pp0.y) {
            assert(!"segmento del bender puntiforme");
            return;
          }

          if (pp1.x == pp0.x)
            t = (y - pp0.y) / (pp1.y - pp0.y);
          else if (pp1.y == pp0.y)
            t = (x - pp0.x) / (pp1.x - pp0.x);
          else
            t = std::max((x - pp0.x) / (pp1.x - pp0.x),
                         (y - pp0.y) / (pp1.y - pp0.y));

          newP0 = (t < 0) ? m_benderSegment.getPoint(t - littleBit) : pp0;
        } else
          newP0 = pp0;

        segment = TSegment(newP0, newP1);

        pair_intersection.clear();
        intersect(*s, segment, pair_intersection);
        assert(isEven(pair_intersection.size()));
        assert(!pair_intersection.empty());
        if (pair_intersection.empty())  // non dovrebbe accadere
          continue;
      }
      //-----------------------------------------------------------------------------

      strokeToModify.push_back(s);
      m_changedStrokes.push_back(i);

      //-----------------------------------------------------------------------------

      m_metaStroke[s] = ArrayOfStroke();

      ArrayOfStroke &tempAS = m_metaStroke[s];

      // extract information about intersection parameter
      ArrayOfDouble intersection;
      extract(pair_intersection, intersection);

      // now add stroke to rotate in m_info struct
      TPointD v = s->getSpeed(intersection[0]);
      TPointD normalToBenderSeg;

      normalToBenderSeg =
          m_rotationVersus * rotate90(m_benderSegment.getSpeed());

      m_atLeastOneIsChanged = false;
      // m_directionIsChanged = false;
      if (tsign(v * normalToBenderSeg) < 0) {
        m_atLeastOneIsChanged = true;
        m_directionIsChanged.push_back(true);
        std::for_each(intersection.begin(), intersection.end(), one_minus_x);
        std::reverse(intersection.begin(), intersection.end());
        s->changeDirection();
      } else
        m_directionIsChanged.push_back(false);

      splitStroke(*s, intersection, tempAS);

      // number of curves is number of intersection plus one
      UINT numberOfCurves = intersection.size() + 1;

      // and begin increase of control point
      if (isEven(intersection.size()) &&
          m_atLeastOneIsChanged)  // if solution are even
      {
        for (j = 0; j < numberOfCurves; ++j) {
          if (isOdd(j))
            increaseCP(tempAS[j], IS_ALL);
          else
            m_strokesToRotate.push_back(tempAS[j]);
        }
      } else {
        increaseCP(tempAS[0], IS_END);
        for (j = 1; j < numberOfCurves - 1; ++j) {
          if (isEven(j))
            increaseCP(tempAS[j], IS_ALL);
          else
            m_strokesToRotate.push_back(tempAS[j]);
        }

        if (isOdd(numberOfCurves))
          increaseCP(tempAS.back(), IS_BEGIN);
        else
          m_strokesToRotate.push_back(tempAS.back());
      }

      TStroke *tempForCorners   = merge(tempAS);
      std::vector<int> *corners = new std::vector<int>;
      corners->push_back(0);
      detectCorners(tempForCorners, 20, *corners);
      corners->push_back(tempForCorners->getChunkCount());
      m_hitStrokeCorners[s] = corners;
      delete tempForCorners;
    }
  }

  if (!strokeToModify.empty()) {
    UINT i, size = strokeToModify.size();
    for (i = 0; i < size; i++)
      if (m_directionIsChanged[i]) strokeToModify[i]->changeDirection();

    if (TTool::getApplication()->getCurrentObject()->isSpline())
      m_undo =
          new UndoPath(getXsheet()->getStageObject(getObjectId())->getSpline());
    else {
      TXshSimpleLevel *sl =
          TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
      assert(sl);
      TFrameId id = getCurrentFid();
      m_undo      = new UndoModifyListStroke(sl, id, strokeToModify);
    }

    for (i = 0; i < size; i++)
      if (m_directionIsChanged[i]) strokeToModify[i]->changeDirection();
  }
}

//-----------------------------------------------------------------------------

void BenderTool::increaseCP(TStroke *tmpStroke, int beginEndOrAll) {
  DoublePair extremes;

  double strokeLength = tmpStroke->getLength();

  double initLength = retrieveInitLength(strokeLength, beginEndOrAll);

  if (MY_ERROR == initLength) return;

  TStrokeBenderDeformation deformer(tmpStroke, initLength, strokeLength * 0.5);

  increaseControlPoints(*tmpStroke, deformer, getPixelSize());

  tmpStroke->disableComputeOfCaches();

  m_info.push_back(benderStrokeInfo(tmpStroke, extremes, beginEndOrAll));
}

//-----------------------------------------------------------------------------

void BenderTool::draw() {
  // TAffine viewMatrix = getViewer()->getViewMatrix();
  // glPushMatrix();
  // tglMultMatrix(viewMatrix);

  double pixelSize = getPixelSize();

  typedef std::map<TStroke *, ArrayOfStroke>::const_iterator mapTACit;
  for (mapTACit cit1 = m_metaStroke.begin(); cit1 != m_metaStroke.end();
       ++cit1) {
    const ArrayOfStroke &tmp = (*cit1).second;
    tglColor(TPixel32::Red);
    for (citAS cit2 = tmp.begin(); cit2 != tmp.end(); ++cit2)
      drawStrokeCenterline(**cit2, pixelSize);
    // drawStroke(**cit2, TRect(0,0,1000,1000), TAffine() );
  }

  double length = m_benderSegment.getLength();

  // pnt to draw rotation vector
  TPointD pnt;

  if (length != 0.0) {
    TPointD v  = m_prevPoint - m_benderSegment.getP0();
    double tmp = norm2(v);
    if (tmp != 0.0) {
      pnt = v * (length / sqrt(tmp));
      pnt += m_benderSegment.getP0();
    }
  } else
    pnt = m_prevPoint;

  // rotation vector
  if (m_buttonDownCounter == 3) {
    tglColor(TPixel::Black);
    tglDrawSegment(m_benderSegment.getP0(), pnt);
    drawPoint(pnt, pixelSize);
  }

  // bender vector
  tglColor(TPixel::Red);
  tglDrawSegment(m_benderSegment.getP0(), m_benderSegment.getP1());
  drawPoint(m_benderSegment.getP0(), pixelSize);
  drawPoint(m_benderSegment.getP1(), pixelSize);

  // point where is mouse pointer
  drawPoint(m_prevPoint, pixelSize);

  // arrow in up direction
  TPointD vDir   = m_benderSegment.getSpeed();
  double length2 = norm2(vDir);
  if (length2 == 0.0) {
    // glPopMatrix();
    return;
  }
  TPointD vUp = 15.0 * normalize(rotate90(vDir));

  tglColor(TPixel::Magenta);
  TPointD middlePnt = 0.5 * (m_benderSegment.getP0() + m_benderSegment.getP1());
  drawArrow(TSegment(middlePnt, m_rotationVersus * vUp + middlePnt), pixelSize);

  // glPopMatrix();
}

//-----------------------------------------------------------------------------

void BenderTool::initBenderAction(TVectorImageP &vi, const TPointD &p) {
  // reset counter
  // m_buttonDownCounter = 0;

  // Versus of bender segment depends from
  //  point selected to do rotation
  // P0 is always center of rotation
  if (tdistance2(p, m_benderSegment.getP0()) <
      tdistance2(p, m_benderSegment.getP1())) {
    TPointD tmpPnt = m_benderSegment.getP1();

    m_benderSegment.setP1(m_benderSegment.getP0());
    m_benderSegment.setP0(tmpPnt);
  }

  // fix information about versus
  findVersus(p);

  // find curves to bender and init data structures
  findCurves(vi);

  // now it's possible set first position of rotation
  // in the same position of extremes
  m_benderSegment.setP1(p);
  m_prevPoint = p;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

// TTool *getBenderTool() {return &BenderTool;}
