#pragma once

#ifndef CONTROLPOINT_SELECTION_INCLUDED
#define CONTROLPOINT_SELECTION_INCLUDED

#include "toonzqt/selection.h"
#include "tools/tool.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tcurves.h"

#include <memory>
#include <set>

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

    ControlPoint(int i, const TThickPoint& speedIn, const TThickPoint& speedOut,
                 bool isCusp = true)
        : m_pointIndex(i)
        , m_speedIn(speedIn)
        , m_speedOut(speedOut)
        , m_isCusp(isCusp) {}

    ControlPoint() = default;
  };

  QList<ControlPoint> m_controlPoints;
  TVectorImageP m_vi;
  int m_strokeIndex = -1;

  // Adjust chunk parity in the stroke
  void adjustChunkParity();

  // Reset m_controlPoints based on current stroke
  void resetControlPoints();

  // Get dependent point between two control points
  TThickPoint getPureDependentPoint(int index) const;

  // Get all points that depend on the control point at index
  void getDependentPoints(
      int index, std::vector<std::pair<int, TThickPoint>>& points) const;

  // Update all points in the stroke
  void updatePoints();

  // Update points dependent on the specified control point
  void updateDependentPoint(int index);

  // Get next control point index (with wrap-around for self-loop strokes)
  int nextIndex(int index) const;

  // Get previous control point index (with wrap-around for self-loop strokes)
  int prevIndex(int index) const;

  // Move speed-in point
  void moveSpeedIn(int index, const TPointD& delta, double minDistance);

  // Move speed-out point
  void moveSpeedOut(int index, const TPointD& delta, double minDistance);

  // Move a single control point without updating dependents
  void moveSingleControlPoint(int index, const TPointD& delta);

public:
  enum PointType { CONTROL_POINT, SPEED_IN, SPEED_OUT, SEGMENT, NONE };

  ControlPointEditorStroke() = default;

  // Allow copy constructor and assignment operator for compatibility
  ControlPointEditorStroke(const ControlPointEditorStroke& other);
  ControlPointEditorStroke& operator=(const ControlPointEditorStroke& other);

  // Create a deep copy of this stroke editor
  ControlPointEditorStroke* clone() const;

  /*! Modify stroke: between two linear or cusp points there must be an even
     number of chunks. WARNING: This function may add control points to the
     stroke. */
  bool setStroke(const TVectorImageP& vi, int strokeIndex);

  // Get the underlying TStroke
  TStroke* getStroke() const {
    return m_vi ? m_vi->getStroke(m_strokeIndex) : nullptr;
  }

  // Stroke index accessors
  void setStrokeIndex(int strokeIndex) { m_strokeIndex = strokeIndex; }
  int getStrokeIndex() const { return m_strokeIndex; }

  // Control point count
  int getControlPointCount() const { return m_controlPoints.size(); }

  // Get control point data
  TThickPoint getControlPoint(int index) const;

  /*! Convert from index point in ControlPointEditorStroke to index point in
   * TStroke. */
  int getIndexPointInStroke(int index) const;

  // Get speed points
  TThickPoint getSpeedInPoint(int index) const;
  TThickPoint getSpeedOutPoint(int index) const;

  // Cusp point management
  bool isCusp(int index) const;
  void setCusp(int index, bool isCusp, bool setSpeedIn);

  // Linear speed point management
  bool isSpeedInLinear(int index) const;
  bool isSpeedOutLinear(int index) const;
  void setLinearSpeedIn(int index, bool linear = true,
                        bool updatePoints = true);
  void setLinearSpeedOut(int index, bool linear = true,
                         bool updatePoints = true);

  /*! If isLinear==true, set the speedIn and speedOut values of the index point
     to "0"; otherwise, set them to default values. Returns true if speedIn or
     speedOut is modified. If updatePoints is true, update dependent points
     afterwards. */
  bool setLinear(int index, bool isLinear, bool updatePoints = true);
  bool setControlPointsLinear(const std::set<int>& points, bool isLinear);

  // Control point manipulation
  void moveControlPoint(int index, const TPointD& delta);
  int addControlPoint(const TPointD& pos);
  void deleteControlPoint(int index);

  // Speed point manipulation
  void moveSpeed(int index, const TPointD& delta, bool isIn,
                 double minDistance);

  // Segment manipulation
  void moveSegment(int beforeIndex, int nextIndex, const TPointD& delta,
                   const TPointD& pos);

  /*! Returns NONE if pos is far from the stroke.
      Otherwise returns the type of point at the given position. */
  PointType getPointTypeAt(const TPointD& pos, double& distance2,
                           int& index) const;

  // Check if stroke is self-looping
  bool isSelfLoop() const {
    TStroke* stroke = getStroke();
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
  int m_strokeIndex                                    = -1;
  ControlPointEditorStroke* m_controlPointEditorStroke = nullptr;

public:
  ControlPointSelection() = default;

  // QObject doesn't allow copying, so we delete copy operations
  ControlPointSelection(const ControlPointSelection&)            = delete;
  ControlPointSelection& operator=(const ControlPointSelection&) = delete;

  // Set the control point editor stroke to work with
  void setControlPointEditorStroke(
      ControlPointEditorStroke* controlPointEditorStroke) {
    m_controlPointEditorStroke = controlPointEditorStroke;
  }

  // Selection interface implementation
  bool isEmpty() const override { return m_selectedPoints.empty(); }
  void selectNone() override { m_selectedPoints.clear(); }

  // Selection management
  bool isSelected(int index) const;
  void select(int index);
  void unselect(int index);

  // Delete selected control points
  void deleteControlPoints();

  // Context menu management
  void addMenuItems(QMenu* menu);

  // Command enabling
  void enableCommands() override;

private slots:
  void setLinear();
  void setUnlinear();
};

#endif  // CONTROLPOINT_SELECTION_INCLUDED
