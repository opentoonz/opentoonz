

#include "tvectorgl.h"
#include "tgl.h"
#include "tpalette.h"
#include "tproperty.h"
#include "tthreadmessage.h"
#include "tvectorimage.h"
#include "drawutil.h"
#include "tcurveutil.h"
#include "tstroke.h"
#include "tstrokeutil.h"
#include "tvectorrenderdata.h"
#include "tstrokedeformations.h"
#include "tmathutil.h"

// Toonz includes
#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"

// Qt includes
#include <QCoreApplication>  // For Qt translation support

using namespace ToolUtils;

//*****************************************************************************
//    PumpTool declaration
//*****************************************************************************

class PumpTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(PumpTool)

  int m_strokeStyleId, m_strokeIndex;  //!< Edited stroke indices
  TStroke *m_inStroke, *m_outStroke;   //!< Input/Output strokes
  std::vector<TStroke *>
      m_splitStrokes;  //!< Merging these, m_inStroke is reformed
  int m_stroke1Idx,
      m_stroke2Idx;  //!< Indices of deformed strokes among split ones

  TUndo *m_undo;  //!< Undo to be added upon non-trivial button up

  double m_actionW;               //!< The action center stroke parameter
  double m_actionS1, m_actionS2;  //!< Action center length in m_stroke
  double m_actionRadius;          //!< Tool action radius in curve length

  std::vector<double>
      m_splitPars;  //!< Split parameters for action localization
  std::vector<double> m_cpLenDiff1,
      m_cpLenDiff2;  //!< Distorted CPs' length distances from action center

  bool m_active;         //!< Whether a stroke is currently being edited
  bool m_enabled;        //!< Tells whether the image allows editing
  bool m_cursorEnabled;  //!< Whether the 'pump preview cursor' can be seen
  bool m_draw;           //!< Should be removed...?

  TPointD m_oldPoint, m_downPoint;  //!< Mouse positions upon editing
  TThickPoint m_cursor;             //!< Pump preview cursor data
  int m_cursorId;

  double m_errorTol;  //!< Allowed approximation error during edit

  TDoubleProperty m_toolSize;
  TIntProperty m_accuracy;
  TPropertyGroup m_prop;

public:
  PumpTool()
      : TTool("T_Pump")
      , m_active(false)
      , m_actionW(0)
      , m_strokeIndex((std::numeric_limits<UINT>::max)())
      , m_inStroke(0)
      , m_outStroke(0)
      , m_stroke1Idx(-1)
      , m_stroke2Idx(-1)
      , m_cursorEnabled(false)
      , m_cursorId(ToolCursor::PumpCursor)
      , m_actionRadius(1)
      , m_draw(false)
      , m_undo(0)
      , m_toolSize("Size:", 1, 100, 20)
      , m_accuracy("Accuracy:", 0, 100, 40)
      , m_enabled(false) {
    bind(TTool::VectorImage);

    m_splitPars.resize(2);

    m_prop.bind(m_toolSize);
    m_prop.bind(m_accuracy);
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void updateTranslation() override {
    m_toolSize.setQStringName(tr("Size:"));
    m_accuracy.setQStringName(tr("Accuracy:"));
  }

  void onEnter() override;
  void onLeave() override;

  void draw() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  bool moveCursor(const TPointD &pos);

  int getCursorId() const override { return m_cursorId; }
  void invalidateCursorArea();

  void onDeactivate() override;

private:
  double actionRadius(double strokeLength);
  void splitStroke(TStroke *s);
  TStroke *mergeStrokes(const std::vector<TStroke *> &strokes);

} PumpToolInstance;

//*****************************************************************************
//    PumpTool implementation
//*****************************************************************************

void PumpTool::onEnter() {
  m_draw = true;
  if (TTool::getApplication()->getCurrentObject()->isSpline() ||
      !(TVectorImageP)getImage(false)) {
    m_enabled  = false;
    m_cursorId = ToolCursor::CURSOR_NO;
  } else {
    m_enabled  = true;
    m_cursorId = ToolCursor::PumpCursor;
  }
}

//----------------------------------------------------------------------

void PumpTool::draw() {
  if (!m_draw || !m_enabled) return;

  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return;

  QMutexLocker lock(vi->getMutex());

  TPalette *palette = vi->getPalette();
  assert(palette);
  if (m_active) {
    // Editing with the tool
    assert(m_outStroke);

    TRectD bboxD(m_outStroke->getBBox());
    TRect bbox(tfloor(bboxD.x0), tfloor(bboxD.y0), tceil(bboxD.x1) - 1,
               tceil(bboxD.y1) - 1);

    tglDraw(TVectorRenderData(TAffine(), bbox, palette, 0, true), m_outStroke);
  } else {
    // Hovering

    double w, dist;
    UINT index;

    if (m_cursorEnabled) {
      // Draw cursor
      glColor3d(1.0, 0.0, 1.0);
      if (m_cursor.thick > 0) tglDrawCircle(m_cursor, m_cursor.thick);
      tglDrawCircle(m_cursor, m_cursor.thick + 4 * getPixelSize());
    }

    if (vi->getNearestStroke(m_cursor, w, index, dist, true)) {
      TStroke *stroke  = vi->getStroke(index);
      double totalLen  = stroke->getLength();
      double actionLen = actionRadius(totalLen);

      tglColor(TPixel32::Red);

      if (totalLen < actionLen ||
          (stroke->isSelfLoop() && totalLen < actionLen + actionLen))
        drawStrokeCenterline(*stroke, getPixelSize());
      else {
        int i, chunckIndex1, chunckIndex2;
        double t, t1, t2, w1, w2;

        double len = stroke->getLength(w);

        double len1 = len - actionLen;
        if (len1 < 0)
          if (stroke->isSelfLoop())
            len1 += totalLen;
          else
            len1 = 0;

        double len2 = len + actionLen;
        if (len2 > totalLen)
          if (stroke->isSelfLoop())
            len2 -= totalLen;
          else
            len2 = totalLen;

        w1 = stroke->getParameterAtLength(len1);
        w2 = stroke->getParameterAtLength(len2);

        int chunkCount = stroke->getChunkCount();

        stroke->getChunkAndT(w1, chunckIndex1, t1);
        stroke->getChunkAndT(w2, chunckIndex2, t2);
        double step;

        const TThickQuadratic *q = 0;

        glBegin(GL_LINE_STRIP);

        q    = stroke->getChunk(chunckIndex1);
        step = computeStep(*q, getPixelSize());

        if (chunckIndex1 == chunckIndex2 && t1 < t2) {
          for (t = t1; t < t2; t += step) tglVertex(q->getPoint(t));

          tglVertex(stroke->getPoint(w2));
          glEnd();
          return;
        }

        for (t = t1; t < 1; t += step) tglVertex(q->getPoint(t));

        for (i = chunckIndex1 + 1; i != chunckIndex2; i++) {
          if (i == chunkCount) i = 0;

          if (i == chunckIndex2) break;

          q    = stroke->getChunk(i);
          step = computeStep(*q, getPixelSize());
          for (t = 0; t < 1; t += step) tglVertex(q->getPoint(t));
        }

        q    = stroke->getChunk(chunckIndex2);
        step = computeStep(*q, getPixelSize());
        for (t = 0; t < t2; t += step) tglVertex(q->getPoint(t));

        tglVertex(stroke->getPoint(w2));

        glEnd();
      }
    }
  }
}

//----------------------------------------------------------------------

void PumpTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  if (m_active || !m_enabled) return;

  assert(m_undo == 0);
  m_active = false;

  TVectorImageP vi(getImage(true));
  if (!vi) return;

  QMutexLocker lock(vi->getMutex());

  // set current point and init parameters
  m_oldPoint  = pos;
  m_downPoint = pos;

  m_inStroke = m_outStroke = 0;
  m_stroke1Idx = m_stroke2Idx = -1;
  m_splitPars[0] = m_splitPars[1] = -2;
  m_actionW                       = 0;

  m_errorTol = (1.0 - 0.01 * m_accuracy.getValue()) * getPixelSize();

  double dist2 = 0.0;
  int cpCount;
  int i;
  UINT index;

  if (vi->getNearestStroke(pos, m_actionW, index, dist2)) {
    // A stroke near the pressed point was found - modify it
    m_active      = true;
    m_strokeIndex = index;

    m_inStroke  = vi->getStroke(m_strokeIndex);
    m_outStroke = new TStroke(*m_inStroke);

    double totalLength = m_inStroke->getLength();
    TXshSimpleLevel *sl =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    assert(sl);
    TFrameId id = getCurrentFid();

    // Allocate the modification undo - will be assigned to the undo manager on
    // mouse release
    m_undo = new UndoModifyStrokeAndPaint(sl, id, m_strokeIndex);

    // Set the stroke's style to 'none'. This is needed to make the original
    // stroke transparent,
    // while the deformed one is shown at its place.
    m_strokeStyleId = m_inStroke->getStyle();
    m_inStroke->setStyle(0);

    if (totalLength <= 0.0) {
      // Single point case
      cpCount = m_inStroke->getControlPointCount();
      m_cpLenDiff1.resize(cpCount);

      for (i = 0; i < cpCount; i++) m_cpLenDiff1[i] = 0.0;

      m_splitStrokes.resize(1);
      m_splitStrokes[0] = new TStroke(*m_inStroke);

      m_stroke1Idx = 0;
    } else
      // Common strokes - split the stroke according to deformation requirements
      splitStroke(m_inStroke);
  }

  invalidate();
}

//----------------------------------------------------------------------

void PumpTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_active || !m_enabled) return;

  TVectorImageP vi(getImage(true));
  if (!vi || !m_outStroke) return;

  QMutexLocker lock(vi->getMutex());

  // Revert current deformation, recovering the one from button press
  delete m_outStroke;

  // Retrieve cursor's vertical displacement
  TPointD delta = TPointD(0, (pos - m_downPoint).y);
  int deltaSign = tsign(delta.y);
  if (deltaSign == 0) {
    // Use a copy of the original stroke
    m_outStroke = new TStroke(*m_inStroke);
    m_outStroke->setStyle(m_strokeStyleId);
    invalidate();
    return;
  }

  // Build deformation upon the original stroke pieces
  TStroke *stroke1 = 0, *stroke2 = 0;

  stroke1 = new TStroke(*m_splitStrokes[m_stroke1Idx]);

  // Deform stroke1
  TStrokeThicknessDeformation deformer(stroke1, delta, m_actionS1,
                                       m_actionRadius, deltaSign);
  modifyThickness(*stroke1, deformer, m_cpLenDiff1, deltaSign < 0);

  if (m_stroke2Idx >= 0) {
    // Deform stroke2
    stroke2 = new TStroke(*m_splitStrokes[m_stroke2Idx]);

    TStrokeThicknessDeformation deformer2(stroke2, delta, m_actionS2,
                                          m_actionRadius, deltaSign);
    modifyThickness(*stroke2, deformer2, m_cpLenDiff2, deltaSign < 0);
  }

  // Apply deformation
  std::vector<TStroke *> splitStrokesCopy(m_splitStrokes);
  splitStrokesCopy[m_stroke1Idx]              = stroke1;
  if (stroke2) splitStrokesCopy[m_stroke2Idx] = stroke2;

  m_outStroke = mergeStrokes(splitStrokesCopy);

  delete stroke1;
  delete stroke2;

  invalidate();
}

//----------------------------------------------------------------------

void PumpTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  TVectorImageP vi;

  if (!m_active || !m_enabled) goto cleanup;

  vi = TVectorImageP(getImage(true));
  if (!vi) goto cleanup;

  {
    m_active = false;

    QMutexLocker lock(vi->getMutex());

    // Reset cursor data
    double t;
    UINT index;
    double dist2;
    if (vi->getNearestStroke(pos, t, index, dist2)) {
      TStroke *nearestStroke      = vi->getStroke(index);
      if (nearestStroke) m_cursor = nearestStroke->getThickPoint(t);
    }

    if (m_outStroke &&
        !areAlmostEqual(m_downPoint, pos, PickRadius * getPixelSize())) {
      // Accept action

      // Clone input stroke - it is someway needed by the stroke change
      // notifier... I wonder why...
      TStroke *oldStroke = new TStroke(*m_inStroke);

      m_outStroke->swap(*m_inStroke);

      m_inStroke->invalidate();

      delete m_outStroke;
      m_outStroke = 0;

      assert(m_undo);
      TUndoManager::manager()->add(m_undo);
      m_undo = 0;

      vi->notifyChangedStrokes(m_strokeIndex, oldStroke);
      notifyImageChanged();

      delete oldStroke;
    }
  }

cleanup:

  if (m_inStroke)
    m_inStroke->setStyle(
        m_strokeStyleId);  // Make the image stroke visible again

  m_strokeIndex = m_strokeStyleId = -1;

  clearPointerContainer(m_splitStrokes);

  delete m_outStroke;
  m_inStroke = m_outStroke = 0;

  delete m_undo;
  m_undo = 0;

  invalidate();
}

//----------------------------------------------------------------------

void PumpTool::invalidateCursorArea() {
  double r = m_cursor.thick + 6;
  TPointD d(r, r);
  invalidate(TRectD(m_cursor - d, m_cursor + d));
}

//----------------------------------------------------------------------

void PumpTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  if (m_active || !m_enabled) return;

  // Cursor preview updates on 3-pixel steps
  if (tdistance2(pos, m_oldPoint) < 9.0 * sq(getPixelSize())) return;

  if (!m_draw) m_draw = true;

  m_oldPoint = pos;

  if (moveCursor(pos)) {
    m_cursorEnabled = true;
    invalidate();
  } else
    m_cursorEnabled = false;

  invalidate();
}

//----------------------------------------------------------------------

bool PumpTool::moveCursor(const TPointD &pos) {
  TVectorImageP vi(getImage(false));
  if (vi) {
    double t;
    UINT index;
    double dist2;
    if (vi->getNearestStroke(pos, t, index, dist2)) {
      TStroke *stroke = vi->getStroke(index);
      if (stroke) {
        m_cursor = stroke->getThickPoint(t);
        return true;
      }
    }
  }

  return false;
}

//----------------------------------------------------------------------

void PumpTool::onDeactivate() {
  m_draw = false;
  if (m_active) {
    m_active = false;
    TVectorImageP vi(getImage(true));
    assert(!!vi && m_outStroke);
    if (!vi || !m_outStroke) return;

    clearPointerContainer(m_splitStrokes);
    if (m_splitPars[0] == -1) {
      delete m_outStroke;
      m_outStroke = 0;
    }

    // restore previous style
    assert(m_strokeIndex >= 0);
    if (m_strokeIndex >= 0) {
      TStroke *stroke = vi->getStroke(m_strokeIndex);
      stroke->setStyle(m_strokeStyleId);
    }

    assert(m_undo);
    delete m_undo;
    m_undo = 0;

    invalidate();

    m_strokeIndex = -1;
    m_outStroke   = 0;
  }
}

//----------------------------------------------------------------------

void PumpTool::onLeave() {
  if (!m_active) m_draw = false;
}

//*****************************************************************************
//    PumpTool privates
//*****************************************************************************

double PumpTool::actionRadius(double strokeLength) {
  double toolSize         = m_toolSize.getValue();
  double toolPercent      = toolSize * 0.01;
  double interpolationVal = pow(toolPercent, 5);
  double indipendentValue = 7.0 * toolSize;

  double actionRadius = (indipendentValue) * (1.0 - interpolationVal) +
                        (strokeLength * toolPercent) * interpolationVal;

  return std::max(actionRadius, indipendentValue);
}

//----------------------------------------------------------------------

/*
  Edited strokes are split near the corresponding editing position, in order
  to localize stroke manipulation.
  Only the localized part of the stroke will receive CP increase and thickness
  tuning needed for the tool action.
*/
void PumpTool::splitStroke(TStroke *s) {
  assert(m_splitStrokes.empty());

  TStroke *stroke1 = 0, *stroke2 = 0;

  // Build the action radius
  double totalLength = s->getLength();
  m_actionRadius     = actionRadius(totalLength);

  // Get the length at selected point and build the split (length) positions
  m_actionS1      = s->getLength(m_actionW);
  double startLen = m_actionS1 - m_actionRadius;
  double endLen   = m_actionS1 + m_actionRadius;

  // Now, perform splitting
  int i, cpCount;

  if ((startLen <= 0 && endLen >= totalLength) ||
      (s->isSelfLoop() && totalLength < (m_actionRadius + m_actionRadius))) {
    // The whole stroke is included in the action - no split
    m_splitStrokes.resize(1);

    m_splitPars[0] = -1;

    m_splitStrokes[0] = new TStroke(*s);

    m_stroke1Idx = 0;
    stroke1      = m_splitStrokes[m_stroke1Idx];

    TStrokeThicknessDeformation deformer(s, m_actionS1, m_actionRadius);
    increaseControlPoints(*stroke1, deformer, getPixelSize());
  } else {
    if (!s->isSelfLoop() || (startLen >= 0.0 && endLen <= totalLength)) {
      // Regular split positions, in the [0.0, totalLength] range.
      // Split points at extremities are dealt.

      m_splitPars[0] = s->getParameterAtLength(
          std::max(startLen, 0.0));  // Crop in the open case
      m_splitPars[1] = s->getParameterAtLength(std::min(endLen, totalLength));

      if (m_splitPars[0] ==
          0.0)  // the "&& m_splitPars[0] == totalLength" was dealt outside
      {
        m_splitStrokes.resize(2);
        m_splitStrokes[0] = new TStroke;
        m_splitStrokes[1] = new TStroke;

        s->split(m_splitPars[1], *(m_splitStrokes[0]), *(m_splitStrokes[1]));

        m_stroke1Idx = 0;
      } else {
        if (m_splitPars[1] == 1.0) {
          m_splitStrokes.resize(2);
          m_splitStrokes[0] = new TStroke;
          m_splitStrokes[1] = new TStroke;

          s->split(m_splitPars[0], *(m_splitStrokes[0]), *(m_splitStrokes[1]));
        } else
          ::splitStroke(*s, m_splitPars, m_splitStrokes);

        m_stroke1Idx = 1;

        // Update the edit point to refer to the central stroke piece
        m_actionS1 -= m_splitStrokes[0]->getLength();
      }

      stroke1 = m_splitStrokes[m_stroke1Idx];

      // Apply deformation to the middle piece
      TStrokeThicknessDeformation deformer(stroke1, m_actionS1, m_actionRadius);
      increaseControlPoints(*stroke1, deformer, getPixelSize());

      m_actionS2 = 0;
    } else {
      // Circular 'overflow' case - (exactly) one split point is outside the
      // regular scope.

      // Since the action diameter is < totalLength, these cases are mutually
      // exclusive.
      if (startLen < 0)
        startLen += totalLength;
      else {
        endLen -= totalLength;
        m_actionS1 -= totalLength;
      }

      // The deformation must be applied in two distinct strokes, since its
      // action interval crosses the junction point

      m_splitPars[0] = s->getParameterAtLength(endLen);
      m_splitPars[1] = s->getParameterAtLength(startLen);

      ::splitStroke(*s, m_splitPars, m_splitStrokes);
      assert(m_splitStrokes.size() >= 3);

      m_stroke1Idx = 0;
      m_stroke2Idx = 2;

      stroke1 = m_splitStrokes[m_stroke1Idx];
      stroke2 = m_splitStrokes[m_stroke2Idx];

      m_actionS2 = m_actionS1 + stroke2->getLength();

      TStrokeThicknessDeformation deformer(stroke1, m_actionS1, m_actionRadius);
      increaseControlPoints(*stroke1, deformer, getPixelSize());
      TStrokeThicknessDeformation deformer2(stroke2, m_actionS2,
                                            m_actionRadius);
      increaseControlPoints(*stroke2, deformer2, getPixelSize());

      cpCount = stroke2->getControlPointCount();
      m_cpLenDiff2.resize(cpCount);

      for (i            = 0; i < cpCount; ++i)
        m_cpLenDiff2[i] = stroke2->getLengthAtControlPoint(i) - m_actionS2;
    }
  }

  cpCount = stroke1->getControlPointCount();
  m_cpLenDiff1.resize(cpCount);

  double diff;
  for (i = 0; i < cpCount; i++) {
    diff            = stroke1->getLengthAtControlPoint(i) - m_actionS1;
    m_cpLenDiff1[i] = (s->isSelfLoop() && stroke2 && totalLength - diff < diff)
                          ? totalLength - diff
                          : diff;
  }
}

//----------------------------------------------------------------------

/*
  A split stroke must be reassembled before it is output.
  In particular, it must be ensured that the merge does not add additional CPS
  at split points, leaving the output seamless.
*/
TStroke *PumpTool::mergeStrokes(const std::vector<TStroke *> &strokes) {
  assert(strokes.size() > 0);

  TStroke *mergedStroke;
  if (strokes.size() > 1) {
    if (m_errorTol > 0.0) {
      strokes[m_stroke1Idx]->reduceControlPoints(m_errorTol);
      if (m_stroke2Idx >= 0)
        strokes[m_stroke2Idx]->reduceControlPoints(m_errorTol);
    }

    // Merge split strokes
    mergedStroke = merge(strokes);
    // mergedStroke->reduceControlPoints(0.4*getPixelSize());    //Originally on
    // the whole result...

    if (m_inStroke->isSelfLoop()) {
      int cpCount = mergedStroke->getControlPointCount();

      TThickPoint p1   = mergedStroke->getControlPoint(0);
      TThickPoint p2   = mergedStroke->getControlPoint(cpCount - 1);
      TThickPoint midP = 0.5 * (p1 + p2);

      mergedStroke->setControlPoint(0, midP);
      mergedStroke->setControlPoint(cpCount - 1, midP);
      mergedStroke->setSelfLoop(true);
    }

    mergedStroke->outlineOptions() = strokes[0]->outlineOptions();
  } else {
    mergedStroke = new TStroke(*strokes[0]);
    if (m_errorTol > 0.0) mergedStroke->reduceControlPoints(m_errorTol);
  }

  mergedStroke->setStyle(m_strokeStyleId);
  mergedStroke->invalidate();

  return mergedStroke;
}
