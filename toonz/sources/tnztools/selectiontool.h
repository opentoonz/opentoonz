#pragma once

#ifndef SELECTIONTOOL_INCLUDED
#define SELECTIONTOOL_INCLUDED

#include "tproperty.h"
#include "toonzqt/selection.h"
#include "tools/toolutils.h"
#include "toonz/strokegenerator.h"

// For Qt translation support
#include <QCoreApplication>
#include <QSet>

class SelectionTool;

//=============================================================================
// Constants / Defines
//-----------------------------------------------------------------------------

enum SelectionType {
  RECT_SELECTION_IDX,
  FREEHAND_SELECTION_IDX,
  POLYLINE_SELECTION_IDX,
};

#define RECT_SELECTION L"Rectangular"
#define FREEHAND_SELECTION L"Freehand"
#define POLYLINE_SELECTION L"Polyline"

//=============================================================================
// FreeDeformer
//-----------------------------------------------------------------------------

class FreeDeformer {
protected:
  TPointD m_originalP00;
  TPointD m_originalP11;

  std::vector<TPointD> m_newPoints;

public:
  FreeDeformer() {}
  virtual ~FreeDeformer() {}

  /*! Set \b index point to \b p, with index from 0 to 3. */
  virtual void setPoint(int index, const TPointD &p) = 0;
  /*! Helper function. */
  virtual void setPoints(const TPointD &p0, const TPointD &p1,
                         const TPointD &p2, const TPointD &p3) = 0;
  virtual void deformImage() = 0;
};

//=============================================================================
// DragSelectionTool
//-----------------------------------------------------------------------------
namespace DragSelectionTool {
//-----------------------------------------------------------------------------

//=============================================================================
// FourPoints
//-----------------------------------------------------------------------------

class FourPoints {
  TPointD m_p00, m_p01, m_p10, m_p11;

public:
  FourPoints(TPointD p00, TPointD p01, TPointD p10, TPointD p11)
      : m_p00(p00), m_p01(p01), m_p10(p10), m_p11(p11) {}
  FourPoints()
      : m_p00(TPointD())
      , m_p01(TPointD())
      , m_p10(TPointD())
      , m_p11(TPointD()) {}

  void setP00(TPointD p) { m_p00 = p; }
  void setP01(TPointD p) { m_p01 = p; }
  void setP10(TPointD p) { m_p10 = p; }
  void setP11(TPointD p) { m_p11 = p; }

  TPointD getP00() const { return m_p00; }
  TPointD getP01() const { return m_p01; }
  TPointD getP10() const { return m_p10; }
  TPointD getP11() const { return m_p11; }

  /*! Order four point from BottomLeft to TopRight. */
  FourPoints orderedPoints() const;
  TPointD getBottomLeft() const { return orderedPoints().getP00(); }
  TPointD getBottomRight() const { return orderedPoints().getP10(); }
  TPointD getTopRight() const { return orderedPoints().getP11(); }
  TPointD getTopLeft() const { return orderedPoints().getP01(); }

  /*! Point = index: P00 =  0; P10 = 1; P11 = 2; P01 = 3; P0M = 4; P1M = 5; PM1
   * = 6; P0M = 7. */
  TPointD getPoint(int index) const;
  /*! Point = index: P00 =  0; P10 = 1; P11 = 2; P01 = 3. */
  void setPoint(int index, const TPointD &p);

  FourPoints enlarge(double d);
  bool isEmpty();
  void empty();
  bool contains(TPointD p);
  TRectD getBox() const;
  FourPoints &operator=(const TRectD &r);
  bool operator==(const FourPoints &p) const;
  FourPoints operator*(const TAffine &aff) const;
};

//=============================================================================
void drawFourPoints(const FourPoints &rect, const TPixel32 &color,
                    unsigned short stipple, bool doContrast);
//-----------------------------------------------------------------------------

//=============================================================================
// DeformValues
//-----------------------------------------------------------------------------

struct DeformValues {
  double m_rotationAngle, m_maxSelectionThickness;
  TPointD m_scaleValue, m_moveValue;
  bool m_isSelectionModified;

  DeformValues(double rotationAngle = 0, double maxSelectionThickness = 0,
               TPointD scaleValue = TPointD(1, 1),
               TPointD moveValue = TPointD(), bool isSelectionModified = false)
      : m_rotationAngle(rotationAngle)
      , m_maxSelectionThickness(maxSelectionThickness)
      , m_scaleValue(scaleValue)
      , m_moveValue(moveValue)
      , m_isSelectionModified(isSelectionModified) {}

  void reset() {
    m_rotationAngle         = 0;
    m_maxSelectionThickness = 0;
    m_scaleValue            = TPointD(1, 1);
    m_moveValue             = TPointD();
    m_isSelectionModified   = false;
  }
};

//=============================================================================
// DragTool
//-----------------------------------------------------------------------------

class DragTool {
protected:
  SelectionTool *m_tool;

public:
  DragTool(SelectionTool *tool) : m_tool(tool) {}
  virtual ~DragTool() {}

  SelectionTool *getTool() const { return m_tool; }

  virtual void transform(TAffine aff, double angle){};
  virtual void transform(TAffine aff){};
  virtual TPointD transform(int index, TPointD newPos) { return TPointD(); };
  virtual void addTransformUndo(){};

  virtual void leftButtonDown(const TPointD &pos, const TMouseEvent &) = 0;
  virtual void leftButtonDrag(const TPointD &pos, const TMouseEvent &) = 0;
  virtual void leftButtonUp(const TPointD &pos, const TMouseEvent &)   = 0;
  virtual void draw() = 0;
};

//=============================================================================
// DeformTool
//-----------------------------------------------------------------------------

class DeformTool : public DragTool {
protected:
  TPointD m_curPos;
  bool m_isDragging;
  TPointD m_startScaleValue;
  TPointD m_startPos;

public:
  DeformTool(SelectionTool *tool);

  virtual void applyTransform(FourPoints bbox) = 0;
  virtual void applyTransform(TAffine aff){};

  void addTransformUndo() override = 0;

  int getSymmetricPointIndex(int index) const;
  /*! Return before point \b index between possible point index
   * {0,4,1,5,2,6,3,7}, include middle point. */
  int getBeforePointIndex(int index) const;
  /*! Return next point \b index between possible point index {0,4,1,5,2,6,3,7},
   * include middle point. */
  int getNextPointIndex(int index) const;
  /*! Return before vertex \b index between possible point vertex index
   * {0,1,2,3}*/
  int getBeforeVertexIndex(int index) const;
  /*! Return next vertex \b index between possible point vertex index
   * {0,1,2,3}*/
  int getNextVertexIndex(int index) const;

  TPointD getStartPos() const { return m_startPos; }
  void setStartPos(const TPointD &pos) { m_startPos = pos; }
  TPointD getCurPos() const { return m_curPos; }
  void setCurPos(const TPointD &pos) { m_curPos = pos; }
  bool isDragging() const { return m_isDragging; }
  TPointD getStartScaleValue() const { return m_startScaleValue; }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override = 0;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void draw() override = 0;
};

//=============================================================================
// Rotation
//-----------------------------------------------------------------------------

class Rotation {
  double m_curAng, m_dstAng;
  DeformTool *m_deformTool;

public:
  Rotation(DeformTool *deformTool);
  TPointD getStartCenter() const;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
  void draw();
};

//=============================================================================
// FreeDeform
//-----------------------------------------------------------------------------

class FreeDeform {
  DeformTool *m_deformTool;

public:
  FreeDeform(DeformTool *deformTool);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
};

//=============================================================================
// MoveSelection
//-----------------------------------------------------------------------------

class MoveSelection {
  DeformTool *m_deformTool;
  TPointD m_lastDelta, m_firstPos;

public:
  MoveSelection(DeformTool *deformTool);
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
};

//=============================================================================
// Scale
//-----------------------------------------------------------------------------

class Scale {
  TPointD m_startCenter;
  bool m_isShiftPressed;
  bool m_isAltPressed;
  bool m_scaleInCenter;
  std::vector<FourPoints> m_startBboxs;

  DeformTool *m_deformTool;

public:
  enum Type { GLOBAL = 0, HORIZONTAL = 1, VERTICAL = 2 };
  int m_type;
  Scale(DeformTool *deformTool, int type);

  /*! Return intersection between straight line in \b point0, \b point1 and
straight line for
\b p parallel to straight line in \b point2, \b point3. */
  TPointD getIntersectionPoint(const TPointD &point0, const TPointD &point1,
                               const TPointD &point2, const TPointD &point3,
                               const TPointD &p) const;
  /*! Scale \b index point of \b bbox in \b pos and return scaled bbox. */
  FourPoints bboxScale(int index, const FourPoints &oldBbox,
                       const TPointD &pos);
  /*! Compute new scale value take care of new position of \b movedIndex point
   * in \b bbox. */
  TPointD computeScaleValue(int movedIndex, const FourPoints newBbox);
  /*! Return \b index point scaled in \b center of \b scaleValue. */
  TPointD getScaledPoint(int index, const FourPoints &oldBbox,
                         const TPointD scaleValue, const TPointD center);
  /*! Compute new center after scale of \b bbox \b index point. */
  TPointD getNewCenter(int index, const FourPoints bbox,
                       const TPointD scaleValue);
  /*! Scale \b bbox \b index point in pos and if \b m_scaleInCenter is true
scale in \b center \b bbox symmetric point;
compute scaleValue. */
  FourPoints bboxScaleInCenter(int index, const FourPoints &oldBbox,
                               const TPointD newPos, TPointD &scaleValue,
                               const TPointD center, bool recomputeScaleValue);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);

  std::vector<FourPoints> getStartBboxs() const { return m_startBboxs; }
  TPointD getStartCenter() const { return m_startCenter; }
  bool scaleInCenter() const { return m_scaleInCenter; }
};
};

//=============================================================================
// Utility
//-----------------------------------------------------------------------------

DragSelectionTool::DragTool *createNewMoveSelectionTool(SelectionTool *st);
DragSelectionTool::DragTool *createNewRotationTool(SelectionTool *st);
DragSelectionTool::DragTool *createNewFreeDeformTool(SelectionTool *st);
DragSelectionTool::DragTool *createNewScaleTool(SelectionTool *st, int type);

//=============================================================================
// SelectionTool
//-----------------------------------------------------------------------------

class SelectionTool : public TTool, public TSelection::View {
  Q_DECLARE_TR_FUNCTIONS(SelectionTool)

protected:
  bool m_firstTime;
  DragSelectionTool::DragTool *m_dragTool;

  StrokeGenerator m_track;
  std::vector<TPointD> m_polyline;
  TPointD m_mousePosition;
  TStroke *m_stroke;

  // To modify selection
  TPointD m_curPos;
  TPointD m_firstPos;

  bool m_selecting;
  bool m_justSelected;
  bool m_shiftPressed;

  enum {
    Outside,
    Inside,
    DEFORM,
    ROTATION,
    MOVE_CENTER,
    SCALE,
    SCALE_X,
    SCALE_Y,
    GLOBAL_THICKNESS,
    ADD_SELECTION
  } m_what;  // RV
  enum {
    P00 = 0,
    P10 = 1,
    P11 = 2,
    P01 = 3,
    PM0 = 4,
    P1M = 5,
    PM1 = 6,
    P0M = 7,
    NONE
  } m_selectedPoint;  // RV

  int m_cursorId;

  bool m_leftButtonMousePressed;

  DragSelectionTool::FourPoints m_selectingRect;

  std::vector<DragSelectionTool::FourPoints> m_bboxs;
  std::vector<TPointD> m_centers;
  std::vector<FreeDeformer *> m_freeDeformers;

  TEnumProperty m_strokeSelectionType;
  TPropertyGroup m_prop;

  virtual void updateAction(TPointD pos, const TMouseEvent &e);

  virtual void modifySelectionOnClick(TImageP image, const TPointD &pos,
                                      const TMouseEvent &e) = 0;

  virtual void doOnActivate()   = 0;
  virtual void doOnDeactivate() = 0;

  // Metodi per disegnare la linea della selezione Freehand
  void startFreehand(const TPointD &pos);
  void freehandDrag(const TPointD &pos);
  void closeFreehand(const TPointD &pos);

  // Metodi per disegnare la linea della selezione Polyline
  void addPointPolyline(const TPointD &pos);
  void closePolyline(const TPointD &pos);

  void updateTranslation() override;

  void drawPolylineSelection();
  void drawRectSelection(const TImage *image);
  void drawFreehandSelection();
  void drawCommandHandle(const TImage *image);

public:
  DragSelectionTool::DeformValues m_deformValues;

  SelectionTool(int targetType);
  ~SelectionTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  TPointD getCenter(int index = 0) const;
  void setCenter(const TPointD &center, int index = 0);

  int getBBoxsCount() const;
  DragSelectionTool::FourPoints getBBox(int index = 0) const;
  virtual void setBBox(const DragSelectionTool::FourPoints &points,
                       int index = 0);

  FreeDeformer *getFreeDeformer(int index = 0) const;
  virtual void setNewFreeDeformer()       = 0;
  void clearDeformers();

  int getSelectedPoint() const { return m_selectedPoint; }

  virtual bool isConstantThickness() const { return true; }
  virtual bool isLevelType() const { return false; }
  virtual bool isSelectedFramesType() const { return false; }
  virtual bool isSameStyleType() const { return false; }
  virtual bool isModifiableSelectionType() const {
    return !(isLevelType() || isSelectedFramesType());
  }
  virtual bool isFloating() const { return false; }

  virtual QSet<int> getSelectedStyles() const { return QSet<int>(); }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override = 0;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override   = 0;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &,
                             const TMouseEvent &e) override = 0;
  bool keyDown(QKeyEvent *) override;

  int getCursorId() const override;

  void draw() override = 0;

  TSelection *getSelection() override = 0;
  virtual bool isSelectionEmpty()     = 0;

  virtual void computeBBox() = 0;

  void onActivate() override;
  void onDeactivate() override;
  void onImageChanged() override = 0;
  void onSelectionChanged() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  bool onPropertyChanged(std::string propertyName) override;

  // returns true if the pressed key is recognized and processed.
  bool isEventAcceptable(QEvent *e) override;
};

#endif  // SELECTIONTOOL_INCLUDED
