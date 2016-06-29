#pragma once

#ifndef CONTROLPOINT_SELECTION_INCLUDED
#define CONTROLPOINT_SELECTION_INCLUDED

#include "toonzqt/selection.h"
#include "tools/tool.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tcurves.h"

//=============================================================================
// ControlPointEditorStroke
//-----------------------------------------------------------------------------

class ControlPointEditorStroke {
private:
  class ControlPoint {
  public:
    int m_pointIndex;
    TThickPoint m_speedIn;
    TThickPoint m_speedOut;
    bool m_isCusp;

    ControlPoint(int i, TThickPoint speedIn, TThickPoint speedOut,
                 bool isCusp = true)
        : m_pointIndex(i)
        , m_speedIn(speedIn)
        , m_speedOut(speedOut)
        , m_isCusp(isCusp) {}
    ControlPoint() {}
  };

  QList<ControlPoint> m_controlPoints;
  TVectorImageP m_vi;
  int m_strokeIndex;

  void adjustChunkParity();

  /*! Reset m_controlPoints. */
  void resetControlPoints();

  TThickPoint getPureDependentPoint(int index) const;
  void getDependentPoints(
      int index, std::vector<std::pair<int, TThickPoint>> &points) const;
  void updatePoints();
  void updateDependentPoint(int index);

  inline int nextIndex(int index) const;
  inline int prevIndex(int index) const;

  void moveSpeedIn(int index, const TPointD &delta, double minDistance);
  void moveSpeedOut(int index, const TPointD &delta, double minDistance);

  void moveSingleControlPoint(int index, const TPointD &delta);

public:
  enum PointType { CONTROL_POINT, SPEED_IN, SPEED_OUT, SEGMENT, NONE };

  ControlPointEditorStroke() : m_vi() {}

  ~ControlPointEditorStroke() { m_controlPoints.clear(); }

  ControlPointEditorStroke *clone() const;

  /*! Modify stroke: between two linear or cusp point must be a pair chunk
number.
PAY ATTENTION: Can add control point in the stroke. */
  void setStroke(const TVectorImageP &vi, int strokeIndex);
  TStroke *getStroke() const {
    return m_vi ? m_vi->getStroke(m_strokeIndex) : 0;
  }

  void setStrokeIndex(int strokeIndex) { m_strokeIndex = strokeIndex; }
  int getStrokeIndex() const { return m_strokeIndex; }

  int getControlPointCount() const { return m_controlPoints.size(); }

  TThickPoint getControlPoint(int index) const;
  /*! From index point in \b ControlPointEditorStroke to index point in \b
   * TStroke. */
  int getIndexPointInStroke(int index) const;
  TThickPoint getSpeedInPoint(int index) const;
  TThickPoint getSpeedOutPoint(int index) const;

  //! Return true if index point is a cusp.
  bool isCusp(int index) const;
  //! Set \b m_isCusp of \b index point to \b isCusp.
  /*! If isCusp == false check \b setSpeedIn and modify speed. */
  void setCusp(int index, bool isCusp, bool setSpeedIn);

  bool isSpeedInLinear(int index) const;
  bool isSpeedOutLinear(int index) const;
  void setLinearSpeedIn(int index, bool linear = true,
                        bool updatePoints = true);
  void setLinearSpeedOut(int index, bool linear = true,
                         bool updatePoints = true);

  /*! If isLinear==true set to "0" the speedIn and speedOut value of index
point; otherwise set to default value.
Return true if speedIn or speedOut is modified.
If updatePoints is true update dependent point after.*/
  bool setLinear(int index, bool isLinear, bool updatePoints = true);
  bool setControlPointsLinear(std::set<int> points, bool isLinear);

  /*! Move \b index ControlPoint of \b delta. */
  void moveControlPoint(int index, const TPointD &delta);

  /*! Add a ControlPoint in \b point. Return added ControlPoint index. */
  int addControlPoint(const TPointD &pos);

  void deleteControlPoint(int index);

  /*! Move speed in \b index of \b delta. */
  void moveSpeed(int index, const TPointD &delta, bool isIn,
                 double minDistance);

  /*! Move segment between \b beforeIndex and \b nextIndex of \b delta. */
  void moveSegment(int beforeIndex, int nextIndex, const TPointD &delta,
                   const TPointD &pos);

  /*!Return NONE if \b pos is far from stroke.*/
  PointType getPointTypeAt(const TPointD &pos, double &distance2,
                           int &index) const;

  bool isSelfLoop() const {
    TStroke *stroke = getStroke();
    return stroke && stroke->isSelfLoop();
  }
};

//=============================================================================
// ControlPointSelection
//-----------------------------------------------------------------------------

class ControlPointSelection final : public QObject, public TSelection {
  Q_OBJECT

private:
  std::set<int> m_selectedPoints;
  int m_strokeIndex;
  ControlPointEditorStroke *m_controlPointEditorStroke;

public:
  ControlPointSelection() : m_controlPointEditorStroke(0), m_strokeIndex(-1) {}

  void setControlPointEditorStroke(
      ControlPointEditorStroke *controlPointEditorStroke) {
    m_controlPointEditorStroke = controlPointEditorStroke;
  }

  bool isEmpty() const override { return m_selectedPoints.empty(); }

  void selectNone() override { m_selectedPoints.clear(); }
  bool isSelected(int index) const;
  void select(int index);
  void unselect(int index);

  void deleteControlPoints();

  void addMenuItems(QMenu *menu);

  void enableCommands() override;

protected slots:
  void setLinear();
  void setUnlinear();
};

#endif  // CONTROLPOINT_SELECTION_INCLUDED
