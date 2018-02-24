

#include "toonz/tpalettehandle.h"
#include "tools/toolhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "tools/tool.h"
#include "tcolorstyles.h"
#include "tpalette.h"
#include "tools/toolutils.h"
#include "tstroke.h"
#include "tmathutil.h"
#include "tools/cursors.h"
#include "tproperty.h"
#include "ttoonzimage.h"
#include "drawutil.h"
#include "tvectorimage.h"
#include "tenv.h"
#include "bluredbrush.h"
#include "toonz/ttileset.h"
#include "toonz/toonzimageutils.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/dvdialog.h"
#include "toonz/trasterimageutils.h"
#include "toonz/preferences.h"
#include "historytypes.h"

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

class GeometricTool;
class MultiLinePrimitive;

TEnv::DoubleVar GeometricSize("InknpaintGeometricSize", 1);
TEnv::DoubleVar GeometricRasterSize("InknpaintGeometricRasterSize", 1);
TEnv::StringVar GeometricType("InknpaintGeometricType", "Rectangle");
TEnv::IntVar GeometricEdgeCount("InknpaintGeometricEdgeCount", 3);
TEnv::IntVar GeometricSelective("InknpaintGeometricSelective", 0);
TEnv::IntVar GeometricGroupIt("InknpaintGeometricGroupIt", 0);
TEnv::IntVar GeometricAutofill("InknpaintGeometricAutofill", 0);
TEnv::IntVar GeometricPencil("InknpaintGeometricPencil", 0);
TEnv::DoubleVar GeometricBrushHardness("InknpaintGeometricHardness", 100);
TEnv::DoubleVar GeometricOpacity("InknpaintGeometricOpacity", 100);
TEnv::IntVar GeometricCapStyle("InknpaintGeometricCapStyle", 0);
TEnv::IntVar GeometricJoinStyle("InknpaintGeometricJoinStyle", 0);
TEnv::IntVar GeometricMiterValue("InknpaintGeometricMiterValue", 4);
TEnv::IntVar GeometricSnap("InknpaintGeometricSnap", 0);
TEnv::IntVar GeometricSnapSensitivity("InknpaintGeometricSnapSensitivity", 0);

//-------------------------------------------------------------------

#define ROUNDC_WSTR L"round_cap"
#define BUTT_WSTR L"butt_cap"
#define PROJECTING_WSTR L"projecting_cap"
#define ROUNDJ_WSTR L"round_join"
#define BEVEL_WSTR L"bevel_join"
#define MITER_WSTR L"miter_join"
#define LOW_WSTR L"Low"
#define MEDIUM_WSTR L"Medium"
#define HIGH_WSTR L"High"

const double SNAPPING_LOW    = 5.0;
const double SNAPPING_MEDIUM = 25.0;
const double SNAPPING_HIGH   = 100.0;

//=============================================================================
// Utility Functions
//-----------------------------------------------------------------------------

static TPointD rectify(const TPointD &oldPos, const TPointD &pos) {
  const double h             = sqrt(2.0) / 2.0;
  const TPointD directions[] = {TPointD(1, 0),  TPointD(h, h),  TPointD(0, 1),
                                TPointD(-h, h), TPointD(-1, 0), TPointD(-h, -h),
                                TPointD(0, -1), TPointD(h, -h)};
  TPointD v        = pos - oldPos;
  int j            = 0;
  double bestValue = v * directions[j];
  for (int k = 1; k < 8; k++) {
    double value = v * directions[k];
    if (value > bestValue) {
      bestValue = value;
      j         = k;
    }
  }
  return oldPos + bestValue * directions[j];
}

//-----------------------------------------------------------------------------

static TPointD computeSpeed(TPointD p0, TPointD p1, double factor) {
  TPointD d = p1 - p0;
  return (d == TPointD()) ? TPointD() : d * (factor / norm(d));
}

//-----------------------------------------------------------------------------

static TRect drawBluredBrush(const TRasterImageP &ri, TStroke *stroke,
                             int thick, double hardness, double opacity) {
  TStroke *s       = new TStroke(*stroke);
  TPointD riCenter = ri->getRaster()->getCenterD();
  s->transform(TTranslation(riCenter));
  int styleId    = s->getStyle();
  TPalette *plt  = ri->getPalette();
  TPixel32 color = plt->getStyle(styleId)->getMainColor();

  TRectD bbox      = s->getBBox();
  TRect bboxI      = convert(bbox).enlarge(1);
  TRect rectRender = bboxI * ri->getRaster()->getBounds();
  if (rectRender.isEmpty()) return TRect();

  TRaster32P workRas(ri->getRaster()->getSize());
  TRaster32P backupRas = ri->getRaster()->clone();
  workRas->clear();
  QRadialGradient brushPad = ToolUtils::getBrushPad(thick, hardness);
  BluredBrush brush(workRas, thick, brushPad, false);

  int i, chunkCount = s->getChunkCount();
  for (i = 0; i < chunkCount; i++) {
    const TThickQuadratic *q = s->getChunk(i);
    std::vector<TThickPoint> points;
    points.push_back(q->getThickP0());
    points.push_back(q->getThickP1());
    points.push_back(q->getThickP2());
    // i punti contenuti nello stroke sembra che haano la tickness pari al
    // raggio e non al diametro del punto!
    points[0].thick *= 2;
    points[1].thick *= 2;
    points[2].thick *= 2;
    TRect chunkBox = brush.getBoundFromPoints(points);
    brush.addArc(points[0], points[1], points[2], 1, 1);
    brush.updateDrawing(ri->getRaster(), backupRas, color, chunkBox, opacity);
  }

  delete s;
  return rectRender;
}

//-----------------------------------------------------------------------------

static TRect drawBluredBrush(const TToonzImageP &ti, TStroke *stroke, int thick,
                             double hardness, bool selective) {
  TStroke *s       = new TStroke(*stroke);
  TPointD riCenter = ti->getRaster()->getCenterD();
  s->transform(TTranslation(riCenter));
  int styleId = s->getStyle();

  TRectD bbox      = s->getBBox();
  TRect bboxI      = convert(bbox).enlarge(1);
  TRect rectRender = bboxI * ti->getRaster()->getBounds();
  if (rectRender.isEmpty()) return TRect();

  TRaster32P workRas(ti->getRaster()->getSize());
  TRasterCM32P backupRas = ti->getRaster()->clone();
  workRas->clear();
  QRadialGradient brushPad = ToolUtils::getBrushPad(thick, hardness);
  BluredBrush brush(workRas, thick, brushPad, false);

  int i, chunkCount = s->getChunkCount();
  for (i = 0; i < chunkCount; i++) {
    const TThickQuadratic *q = s->getChunk(i);
    std::vector<TThickPoint> points;
    points.push_back(q->getThickP0());
    points.push_back(q->getThickP1());
    points.push_back(q->getThickP2());
    // i punti contenuti nello stroke sembra che haano la tickness pari al
    // raggio e non al diametro del punto!
    points[0].thick *= 2;
    points[1].thick *= 2;
    points[2].thick *= 2;
    TRect chunkBox = brush.getBoundFromPoints(points);
    brush.addArc(points[0], points[1], points[2], 1, 1);
    brush.updateDrawing(ti->getRaster(), backupRas, chunkBox, styleId,
                        selective);
  }

  delete s;
  return rectRender;
}

//-----------------------------------------------------------------------------

class MultilinePrimitiveUndo final : public TUndo {
  std::vector<TPointD> m_oldVertex;
  std::vector<TPointD> m_newVertex;
  MultiLinePrimitive *m_tool;

public:
  MultilinePrimitiveUndo(const std::vector<TPointD> &vertex,
                         MultiLinePrimitive *tool)
      : TUndo(), m_oldVertex(vertex), m_tool(tool), m_newVertex() {}

  ~MultilinePrimitiveUndo() {}

  void undo() const override;
  void redo() const override;
  void setNewVertex(const std::vector<TPointD> &vertex) {
    m_newVertex = vertex;
  }

  int getSize() const override { return sizeof(this); }

  QString getToolName();
  int getHistoryType() override { return HistoryType::GeometricTool; }
};

//-----------------------------------------------------------------------------

class FullColorBluredPrimitiveUndo final : public UndoFullColorPencil {
  int m_thickness;
  double m_hardness;

public:
  FullColorBluredPrimitiveUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                               TStroke *stroke, int thickness, double hardness,
                               double opacity, bool doAntialias,
                               bool createdFrame, bool createdLevel)
      : UndoFullColorPencil(level, frameId, stroke, opacity, doAntialias,
                            createdFrame, createdLevel)
      , m_thickness(thickness)
      , m_hardness(hardness) {
    TRasterP raster = getImage()->getRaster();
    TDimension d    = raster->getSize();
    m_tiles         = new TTileSetFullColor(d);
    TRect rect      = convert(stroke->getBBox()) +
                 TPoint((int)(d.lx * 0.5), (int)(d.ly * 0.5));
    m_tiles->add(raster, rect.enlarge(2));
    m_stroke = new TStroke(*stroke);
  }

  ~FullColorBluredPrimitiveUndo() {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TRasterImageP ri = getImage();
    if (!ri) return;
    drawBluredBrush(ri, m_stroke, m_thickness, m_hardness, m_opacity);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return UndoFullColorPencil::getSize() + sizeof(this);
  }
};

//-----------------------------------------------------------------------------
/*-- Hardness<100 のときの GeometricToolのUndo --*/
class CMBluredPrimitiveUndo final : public UndoRasterPencil {
  int m_thickness;
  double m_hardness;
  bool m_selective;

public:
  CMBluredPrimitiveUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                        TStroke *stroke, int thickness, double hardness,
                        bool selective, bool doAntialias, bool createdFrame,
                        bool createdLevel, std::string primitiveName)
      : UndoRasterPencil(level, frameId, stroke, selective, false, doAntialias,
                         createdFrame, createdLevel, primitiveName)
      , m_thickness(thickness)
      , m_hardness(hardness)
      , m_selective(selective) {
    TRasterCM32P raster = getImage()->getRaster();
    TDimension d        = raster->getSize();
    m_tiles             = new TTileSetCM32(d);
    TRect rect          = convert(stroke->getBBox()) +
                 TPoint((int)(d.lx * 0.5), (int)(d.ly * 0.5));
    m_tiles->add(raster, rect.enlarge(2));
    m_stroke = new TStroke(*stroke);
  }

  ~CMBluredPrimitiveUndo() {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TToonzImageP ti = getImage();
    if (!ti) return;
    drawBluredBrush(ti, m_stroke, m_thickness, m_hardness, m_selective);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return UndoRasterPencil::getSize() + sizeof(this);
  }
};

//-----------------------------------------------------------------------------

class PrimitiveParam {
  Q_DECLARE_TR_FUNCTIONS(PrimitiveParam)

public:
  TDoubleProperty m_toolSize;
  TIntProperty m_rasterToolSize;
  TDoubleProperty m_opacity;
  TDoubleProperty m_hardness;
  TEnumProperty m_type;
  TIntProperty m_edgeCount;
  TBoolProperty m_autogroup;
  TBoolProperty m_autofill;
  TBoolProperty m_selective;
  TBoolProperty m_pencil;
  TEnumProperty m_capStyle;
  TEnumProperty m_joinStyle;
  TIntProperty m_miterJoinLimit;
  TBoolProperty m_snap;
  TEnumProperty m_snapSensitivity;
  TPropertyGroup m_prop[2];

  int m_targetType;

  // for snapping
  int m_strokeIndex1;
  double m_w1, m_pixelSize, m_currThickness, m_minDistance2;
  bool m_foundSnap = false;
  TPointD m_snapPoint;

  PrimitiveParam(int targetType)
      : m_type("Shape:")  // "W_ToolOptions_ShapeType")
      , m_toolSize("Size:", 0, 100,
                   1)  // "W_ToolOptions_ShapeThickness", 0,30,1)
      , m_rasterToolSize("Size:", 1, 100, 1)
      , m_opacity("Opacity:", 0, 100, 100)
      , m_hardness("Hardness:", 0, 100, 100)
      , m_edgeCount("Polygon Sides:", 3, 15, 3)
      , m_autogroup("Auto Group", false)
      , m_autofill("Auto Fill", false)
      , m_selective("Selective", false)
      , m_pencil("Pencil Mode", false)
      , m_capStyle("Cap")
      , m_joinStyle("Join")
      , m_miterJoinLimit("Miter:", 0, 100, 4)
      , m_snap("Snap", false)
      , m_snapSensitivity("Sensitivity:")
      , m_targetType(targetType) {
    if (targetType & TTool::Vectors) m_prop[0].bind(m_toolSize);
    if (targetType & TTool::ToonzImage || targetType & TTool::RasterImage) {
      m_prop[0].bind(m_rasterToolSize);
      m_prop[0].bind(m_hardness);
    }
    if (targetType & TTool::RasterImage) m_prop[0].bind(m_opacity);
    m_prop[0].bind(m_type);

    m_prop[0].bind(m_edgeCount);
    m_snapSensitivity.addValue(LOW_WSTR);
    m_snapSensitivity.addValue(MEDIUM_WSTR);
    m_snapSensitivity.addValue(HIGH_WSTR);
    if (targetType & TTool::Vectors) {
      m_prop[0].bind(m_autogroup);
      m_prop[0].bind(m_autofill);
      m_prop[0].bind(m_snap);
      m_snap.setId("Snap");
      m_prop[0].bind(m_snapSensitivity);
      m_snapSensitivity.setId("SnapSensitivity");
    }
    if (targetType & TTool::ToonzImage) {
      m_prop[0].bind(m_selective);
      m_prop[0].bind(m_pencil);
      m_pencil.setId("PencilMode");
    }

    m_capStyle.addValue(BUTT_WSTR);
    m_capStyle.addValue(ROUNDC_WSTR);
    m_capStyle.addValue(PROJECTING_WSTR);
    m_capStyle.setId("Cap");

    m_joinStyle.addValue(MITER_WSTR);
    m_joinStyle.addValue(ROUNDJ_WSTR);
    m_joinStyle.addValue(BEVEL_WSTR);
    m_joinStyle.setId("Join");

    m_miterJoinLimit.setId("Miter");

    m_prop[1].bind(m_capStyle);
    m_prop[1].bind(m_joinStyle);
    m_prop[1].bind(m_miterJoinLimit);

    m_selective.setId("Selective");
    m_autogroup.setId("AutoGroup");
    m_autofill.setId("Autofill");
    m_type.setId("GeometricShape");
    m_edgeCount.setId("GeometricEdge");
  }

  void updateTranslation() {
    m_type.setQStringName(tr("Shape:"));
    m_toolSize.setQStringName(tr("Size:"));
    m_rasterToolSize.setQStringName(tr("Thickness:"));
    m_opacity.setQStringName(tr("Opacity:"));
    m_hardness.setQStringName(tr("Hardness:"));
    m_edgeCount.setQStringName(tr("Polygon Sides:"));
    m_autogroup.setQStringName(tr("Auto Group"));
    m_autofill.setQStringName(tr("Auto Fill"));
    m_selective.setQStringName(tr("Selective"));
    m_pencil.setQStringName(tr("Pencil Mode"));
    m_capStyle.setQStringName(tr("Cap"));
    m_joinStyle.setQStringName(tr("Join"));
    m_miterJoinLimit.setQStringName(tr("Miter:"));
    m_snap.setQStringName(tr("Snap"));
    m_snapSensitivity.setQStringName(tr(""));
  }
};

//=============================================================================
// Abstract Class Primitive
//-----------------------------------------------------------------------------

class Primitive {
protected:
  bool m_isEditing, m_rasterTool, m_isPrompting;
  GeometricTool *m_tool;
  PrimitiveParam *m_param;

public:
  Primitive(PrimitiveParam *param, GeometricTool *tool, bool rasterTool)
      : m_param(param)
      , m_tool(tool)
      , m_isEditing(false)
      , m_rasterTool(rasterTool)
      , m_isPrompting(false) {}

  double getThickness() const {
    if (m_rasterTool)
      return m_param->m_rasterToolSize.getValue() * 0.5;
    else
      return m_param->m_toolSize.getValue() * 0.5;
  }

  void setIsPrompting(bool value) { m_isPrompting = value; }

  virtual std::string getName() const = 0;

  virtual ~Primitive() {}

  virtual void leftButtonDown(const TPointD &p, const TMouseEvent &e){};
  virtual void leftButtonDrag(const TPointD &p, const TMouseEvent &e){};
  virtual void leftButtonUp(const TPointD &p, const TMouseEvent &e){};
  virtual void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e){};
  virtual void rightButtonDown(const TPointD &p, const TMouseEvent &e){};
  virtual void mouseMove(const TPointD &p, const TMouseEvent &e){};
  virtual bool keyDown(QKeyEvent *event) { return false; }
  virtual void onEnter(){};
  virtual void draw(){};
  virtual void onActivate(){};
  virtual void onDeactivate(){};
  virtual void onImageChanged(){};
  TPointD calculateSnap(TPointD pos);
  void drawSnap();
  TPointD getSnap(TPointD pos);
  void resetSnap();
  TPointD checkGuideSnapping(TPointD pos);

  virtual TStroke *makeStroke() const = 0;
};

//-----------------------------------------------------------------------------

void Primitive::resetSnap() {
  m_param->m_strokeIndex1 = -1;
  m_param->m_w1           = -1;
  m_param->m_foundSnap    = false;
}

//-----------------------------------------------------------------------------

TPointD Primitive::calculateSnap(TPointD pos) {
  m_param->m_foundSnap = false;
  if (Preferences::instance()->getVectorSnappingTarget() == 1) return pos;
  TVectorImageP vi(TTool::getImage(false));
  TPointD snapPoint = pos;
  if (vi && m_param->m_snap.getValue()) {
    double minDistance2     = m_param->m_minDistance2;
    m_param->m_strokeIndex1 = -1;

    int i, strokeNumber = vi->getStrokeCount();

    TStroke *stroke;
    double distance2, outW;

    for (i = 0; i < strokeNumber; i++) {
      stroke = vi->getStroke(i);
      if (stroke->getNearestW(pos, outW, distance2) &&
          distance2 < minDistance2) {
        minDistance2            = distance2;
        m_param->m_strokeIndex1 = i;
        if (areAlmostEqual(outW, 0.0, 1e-3))
          m_param->m_w1 = 0.0;
        else if (areAlmostEqual(outW, 1.0, 1e-3))
          m_param->m_w1 = 1.0;
        else
          m_param->m_w1      = outW;
        TThickPoint point1   = stroke->getPoint(m_param->m_w1);
        snapPoint            = TPointD(point1.x, point1.y);
        m_param->m_foundSnap = true;
        m_param->m_snapPoint = snapPoint;
      }
    }
  }
  return snapPoint;
}

//-----------------------------------------------------------------------------

TPointD Primitive::getSnap(TPointD pos) {
  if (m_param->m_foundSnap)
    return m_param->m_snapPoint;
  else
    return pos;
}

// Primitive::drawSnap and Primitive::checkGuideSnapping are below the
// Geometric Tool definition since they use the m_tool property of Primitive
// but it isn't defined yet up here.

//=============================================================================
// Rectangle Primitive Class Declaration
//-----------------------------------------------------------------------------

class RectanglePrimitive final : public Primitive {
  TRectD m_selectingRect;
  TPointD m_pos;
  TPointD m_startPoint;
  TPixel32 m_color;

public:
  RectanglePrimitive(PrimitiveParam *param, GeometricTool *tool,
                     bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Rectangle";
  }  // W_ToolOptions_ShapeRect"; }

  TStroke *makeStroke() const override;
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &realPos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// Circle Primitive Class Declaration
//-----------------------------------------------------------------------------

class CirclePrimitive final : public Primitive {
  TPointD m_centre;
  TPointD m_pos;
  double m_radius;
  TPixel32 m_color;

public:
  CirclePrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Circle";
  }  // W_ToolOptions_ShapeCircle";}

  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  TStroke *makeStroke() const override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// MultiLine Primitive Class Declaration
//-----------------------------------------------------------------------------

const double joinDistance = 5.0;

class MultiLinePrimitive : public Primitive {
protected:
  std::vector<TPointD> m_vertex;
  TPointD m_mousePosition;
  TPixel32 m_color;
  bool m_closed, m_isSingleLine;
  bool m_speedMoved;        //!< True after moveSpeed(), false after addVertex()
  bool m_beforeSpeedMoved;  //!< Old value of m_speedMoved
  bool m_ctrlDown;          //!< Whether ctrl is hold down
  MultilinePrimitiveUndo *m_undo;

public:
  MultiLinePrimitive(PrimitiveParam *param, GeometricTool *tool,
                     bool reasterTool)
      : Primitive(param, tool, reasterTool)
      , m_closed(false)
      , m_isSingleLine(false)
      , m_speedMoved(false)
      , m_beforeSpeedMoved(false)
      , m_ctrlDown(false) {}

  std::string getName() const override {
    return "Polyline";
  }  // W_ToolOptions_ShapePolyline";}

  void addVertex(const TPointD &pos);
  void moveSpeed(const TPointD &delta);
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  bool keyDown(QKeyEvent *event) override;
  TStroke *makeStroke() const override;
  void endLine();
  void onActivate() override;
  void onDeactivate() override {
    m_vertex.clear();
    m_speedMoved       = false;
    m_beforeSpeedMoved = false;
  }
  void onEnter() override;
  void onImageChanged() override;
  void setVertexes(const std::vector<TPointD> &vertex) { m_vertex = vertex; };
  void setSpeedMoved(bool speedMoved) { m_speedMoved = speedMoved; };
};

//-----------------------------------------------------------------------------

void MultilinePrimitiveUndo::undo() const {
  m_tool->setVertexes(m_oldVertex);
  int count       = m_oldVertex.size();
  bool speedMoved = (count != 0 && count % 4 != 1);
  m_tool->setSpeedMoved(speedMoved);
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//-----------------------------------------------------------------------------

void MultilinePrimitiveUndo::redo() const {
  m_tool->setVertexes(m_newVertex);
  int count       = m_newVertex.size();
  bool speedMoved = (count != 0 && count % 4 != 1);
  m_tool->setSpeedMoved(speedMoved);
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//----------------------------------------------------------------------------

QString MultilinePrimitiveUndo::getToolName() {
  return QString("Geometric Tool %1")
      .arg(QString::fromStdString(m_tool->getName()));
}

//=============================================================================
// Line Primitive Class Declaration
//-----------------------------------------------------------------------------

class LinePrimitive final : public MultiLinePrimitive {
public:
  LinePrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : MultiLinePrimitive(param, tool, reasterTool) {
    m_isSingleLine = true;
  }

  std::string getName() const override {
    return "Line";
  }  // W_ToolOptions_ShapePolyline";}

  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override {}
};

//=============================================================================
// Ellipse Primitive Class Declaration
//-----------------------------------------------------------------------------

class EllipsePrimitive final : public Primitive {
  TPointD m_pos;
  TRectD m_selectingRect;
  TPointD m_startPoint;
  TPixel32 m_color;

public:
  EllipsePrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Ellipse";
  }  // W_ToolOptions_ShapeEllipse";}

  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &realPos, const TMouseEvent &e) override;
  TStroke *makeStroke() const override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// Arc Primitive Class Declaration
//-----------------------------------------------------------------------------

class ArcPrimitive final : public Primitive {
  TStroke *m_stroke;
  TPointD m_startPoint, m_endPoint, m_centralPoint;
  int m_clickNumber;
  TPixel32 m_color;

public:
  ArcPrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool), m_stroke(0), m_clickNumber(0) {}

  ~ArcPrimitive() {
    if (m_stroke) delete m_stroke;
  }

  std::string getName() const override {
    return "Arc";
  }  // _ToolOptions_ShapeArc";}

  TStroke *makeStroke() const override;
  void draw() override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// Polygon Primitive Class Declaration
//-----------------------------------------------------------------------------

class PolygonPrimitive final : public Primitive {
  TPointD m_startPoint, m_centre;
  double m_radius;
  TPixel32 m_color;

public:
  PolygonPrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Polygon";
  }  // W_ToolOptions_ShapePolygon";}

  TStroke *makeStroke() const override;
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// Geometric Tool
//-----------------------------------------------------------------------------

class GeometricTool final : public TTool {
protected:
  Primitive *m_primitive;
  std::map<std::wstring, Primitive *> m_primitiveTable;
  PrimitiveParam m_param;
  std::wstring m_typeCode;
  bool m_active;
  bool m_firstTime;

public:
  GeometricTool(int targetType)
      : TTool("T_Geometric")
      , m_primitive(0)
      , m_param(targetType)
      , m_active(false)
      , m_firstTime(true) {
    bind(targetType);
    if ((targetType & TTool::RasterImage) || (targetType & TTool::ToonzImage)) {
      addPrimitive(new RectanglePrimitive(&m_param, this, true));
      addPrimitive(new CirclePrimitive(&m_param, this, true));
      addPrimitive(new EllipsePrimitive(&m_param, this, true));
      addPrimitive(new LinePrimitive(&m_param, this, true));
      addPrimitive(new MultiLinePrimitive(&m_param, this, true));
      addPrimitive(new ArcPrimitive(&m_param, this, true));
      addPrimitive(new PolygonPrimitive(&m_param, this, true));
    } else  // targetType == 1
    {
      // vector
      addPrimitive(m_primitive = new RectanglePrimitive(&m_param, this, false));
      addPrimitive(new CirclePrimitive(&m_param, this, false));
      addPrimitive(new EllipsePrimitive(&m_param, this, false));
      addPrimitive(new LinePrimitive(&m_param, this, false));
      addPrimitive(new MultiLinePrimitive(&m_param, this, false));
      addPrimitive(new ArcPrimitive(&m_param, this, false));
      addPrimitive(new PolygonPrimitive(&m_param, this, false));
    }
  }

  ~GeometricTool() {
    std::map<std::wstring, Primitive *>::iterator it;
    for (it = m_primitiveTable.begin(); it != m_primitiveTable.end(); ++it)
      delete it->second;
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override { m_param.updateTranslation(); }

  void addPrimitive(Primitive *p) {
    // TODO: aggiungere il controllo per evitare nomi ripetuti
    std::wstring name = ::to_wstring(p->getName());
    // wstring name = TStringTable::translate(p->getName());

    m_primitiveTable[name] = p;
    m_param.m_type.addValue(name);
  }

  void changeType(std::wstring name) {
    std::map<std::wstring, Primitive *>::iterator it =
        m_primitiveTable.find(name);
    if (it != m_primitiveTable.end()) m_primitive = it->second;
  }

  void leftButtonDown(const TPointD &p, const TMouseEvent &e) override {
    /* m_active = getApplication()->getCurrentObject()->isSpline() ||
   (bool) getImage(true);*/

    m_active = touchImage();  // NEEDS to be done even if(m_active), due
    if (!m_active)            // to the HORRIBLE m_frameCreated / m_levelCreated
      return;                 // mechanism. touchImage() is the ONLY function
    // resetting them to false...                       >_<
    if (m_primitive) m_primitive->leftButtonDown(p, e);
    invalidate();
  }
  void leftButtonDrag(const TPointD &p, const TMouseEvent &e) override {
    if (!m_active) return;
    if (m_primitive) m_primitive->leftButtonDrag(p, e);
    invalidate();
  }
  void leftButtonUp(const TPointD &p, const TMouseEvent &e) override {
    if (!m_active) return;
    if (m_primitive) m_primitive->leftButtonUp(p, e);
    invalidate();
  }
  void leftButtonDoubleClick(const TPointD &p, const TMouseEvent &e) override {
    if (!m_active) return;
    if (m_primitive) m_primitive->leftButtonDoubleClick(p, e);
    invalidate();
  }

  bool keyDown(QKeyEvent *event) override {
    return m_primitive->keyDown(event);
  }

  void onImageChanged() override {
    if (m_primitive) m_primitive->onImageChanged();
    invalidate();
  }

  void rightButtonDown(const TPointD &p, const TMouseEvent &e) override {
    if (m_primitive) m_primitive->rightButtonDown(p, e);
    invalidate();
  }

  void mouseMove(const TPointD &p, const TMouseEvent &e) override {
    if (m_primitive) m_primitive->mouseMove(p, e);
  }

  void onActivate() override {
    if (m_firstTime) {
      m_param.m_toolSize.setValue(GeometricSize);
      m_param.m_rasterToolSize.setValue(GeometricRasterSize);
      m_param.m_opacity.setValue(GeometricOpacity);
      m_param.m_hardness.setValue(GeometricBrushHardness);
      m_param.m_selective.setValue(GeometricSelective ? 1 : 0);
      m_param.m_autogroup.setValue(GeometricGroupIt ? 1 : 0);
      m_param.m_autofill.setValue(GeometricAutofill ? 1 : 0);
      std::wstring typeCode = ::to_wstring(GeometricType.getValue());
      m_param.m_type.setValue(typeCode);
      GeometricType = ::to_string(typeCode);
      m_typeCode    = typeCode;
      changeType(typeCode);
      m_param.m_edgeCount.setValue(GeometricEdgeCount);
      m_param.m_pencil.setValue(GeometricPencil ? 1 : 0);
      m_param.m_capStyle.setIndex(GeometricCapStyle);
      m_param.m_joinStyle.setIndex(GeometricJoinStyle);
      m_param.m_miterJoinLimit.setValue(GeometricMiterValue);
      m_firstTime = false;
      m_param.m_snap.setValue(GeometricSnap);
      m_param.m_snapSensitivity.setIndex(GeometricSnapSensitivity);
      switch (GeometricSnapSensitivity) {
      case 0:
        m_param.m_minDistance2 = SNAPPING_LOW;
        break;
      case 1:
        m_param.m_minDistance2 = SNAPPING_MEDIUM;
        break;
      case 2:
        m_param.m_minDistance2 = SNAPPING_HIGH;
        break;
      }
    }
    m_primitive->resetSnap();
    /*--
       ショートカットでいきなりスタート（＝onEnterを通らない場合）のとき、
            LineToolが反応しないことがある対策 --*/
    m_active = (getImage(false) != 0 ||
                Preferences::instance()->isAutoCreateEnabled());

    if (m_primitive) m_primitive->onActivate();
  }

  void onDeactivate() override {
    if (m_primitive) m_primitive->onDeactivate();
  }

  void onEnter() override {
    m_active = getImage(false) != 0;
    if (m_active && m_primitive) m_primitive->onEnter();
  }

  void draw() override {
    if (m_primitive) m_primitive->draw();
  }

  int getCursorId() const override { return ToolCursor::PenCursor; }

  int getColorClass() const { return 1; }

  TPropertyGroup *getProperties(int idx) override {
    return &m_param.m_prop[idx];
  }

  bool onPropertyChanged(std::string propertyName) override {
    /*---	変更されたPropertyごとに処理を分ける。
            注意：m_toolSizeとm_rasterToolSizeは同じName(="Size:")なので、
            扱っている画像がラスタかどうかで区別する ---*/
    if (propertyName == m_param.m_toolSize.getName()) {
      TImageP img = getImage(false);
      TToonzImageP ri(img); /*-- ラスタかどうかの判定 --*/
      if (ri)
        GeometricRasterSize = m_param.m_rasterToolSize.getValue();
      else
        GeometricSize = m_param.m_toolSize.getValue();
    } else if (propertyName == m_param.m_type.getName()) {
      std::wstring typeCode = m_param.m_type.getValue();
      GeometricType         = ::to_string(typeCode);
      if (typeCode != m_typeCode) {
        m_typeCode = typeCode;
        changeType(typeCode);
      }
    } else if (propertyName == m_param.m_edgeCount.getName())
      GeometricEdgeCount = m_param.m_edgeCount.getValue();
    else if (propertyName == m_param.m_autogroup.getName()) {
      if (!m_param.m_autogroup.getValue()) {
        m_param.m_autofill.setValue(false);
        // this is ugly: it's needed to refresh the GUI of the toolbar after
        // having set to false the autofill...
        TTool::getApplication()->getCurrentTool()->setTool(
            "");  // necessary, otherwise next setTool is ignored...
        TTool::getApplication()->getCurrentTool()->setTool(
            QString::fromStdString(getName()));
      }
      GeometricGroupIt = m_param.m_autogroup.getValue();
    } else if (propertyName == m_param.m_autofill.getName()) {
      if (m_param.m_autofill.getValue()) {
        m_param.m_autogroup.setValue(true);
        // this is ugly: it's needed to refresh the GUI of the toolbar after
        // having set to false the autofill...
        TTool::getApplication()->getCurrentTool()->setTool(
            "");  // necessary, otherwise next setTool is ignored...
        TTool::getApplication()->getCurrentTool()->setTool(
            QString::fromStdString(getName()));
      }
      GeometricGroupIt = m_param.m_autofill.getValue();
    } else if (propertyName == m_param.m_selective.getName())
      GeometricSelective = m_param.m_selective.getValue();
    else if (propertyName == m_param.m_pencil.getName())
      GeometricPencil = m_param.m_pencil.getValue();
    else if (propertyName == m_param.m_hardness.getName())
      GeometricBrushHardness = m_param.m_hardness.getValue();
    else if (propertyName == m_param.m_opacity.getName())
      GeometricOpacity = m_param.m_opacity.getValue();
    else if (propertyName == m_param.m_capStyle.getName())
      GeometricCapStyle = m_param.m_capStyle.getIndex();
    else if (propertyName == m_param.m_joinStyle.getName())
      GeometricJoinStyle = m_param.m_joinStyle.getIndex();
    else if (propertyName == m_param.m_miterJoinLimit.getName())
      GeometricMiterValue = m_param.m_miterJoinLimit.getValue();
    else if (propertyName == m_param.m_snap.getName())
      GeometricSnap = m_param.m_snap.getValue();
    else if (propertyName == m_param.m_snapSensitivity.getName()) {
      GeometricSnapSensitivity = m_param.m_snapSensitivity.getIndex();
      switch (GeometricSnapSensitivity) {
      case 0:
        m_param.m_minDistance2 = SNAPPING_LOW;
        break;
      case 1:
        m_param.m_minDistance2 = SNAPPING_MEDIUM;
        break;
      case 2:
        m_param.m_minDistance2 = SNAPPING_HIGH;
        break;
      }
    }

    return false;
  }

  void addStroke() {
    if (!m_primitive) return;
    TStroke *stroke = m_primitive->makeStroke();
    if (!stroke) return;

    TStroke::OutlineOptions &options = stroke->outlineOptions();
    options.m_capStyle               = m_param.m_capStyle.getIndex();
    options.m_joinStyle              = m_param.m_joinStyle.getIndex();
    options.m_miterUpper             = m_param.m_miterJoinLimit.getValue();

    TImage *image = getImage(true);
    TToonzImageP ti(image);
    TVectorImageP vi(image);
    TRasterImageP ri(image);
    TXshSimpleLevel *sl =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TFrameId id = getCurrentFid();
    /*-- ToonzImageの場合 --*/
    if (ti) {
      int styleId    = TTool::getApplication()->getCurrentLevelStyleIndex();
      bool selective = m_param.m_selective.getValue();

      bool filled = false;

      stroke->setStyle(styleId);
      double hardness = m_param.m_hardness.getValue() * 0.01;
      TRect savebox;
      if (hardness == 1 || m_param.m_pencil.getValue()) {
        TUndoManager::manager()->add(new UndoRasterPencil(
            sl, id, stroke, selective, filled, !m_param.m_pencil.getValue(),
            m_isFrameCreated, m_isLevelCreated, m_primitive->getName()));
        savebox = ToonzImageUtils::addInkStroke(ti, stroke, styleId, selective,
                                                filled, TConsts::infiniteRectD,
                                                !m_param.m_pencil.getValue());
      } else {
        int thickness = m_param.m_rasterToolSize.getValue();
        TUndoManager::manager()->add(new CMBluredPrimitiveUndo(
            sl, id, stroke, thickness, hardness, selective, false,
            m_isFrameCreated, m_isLevelCreated, m_primitive->getName()));
        savebox = drawBluredBrush(ti, stroke, thickness, hardness, selective);
      }
      ToolUtils::updateSaveBox();
      delete stroke;
    }
    /*-- VectorImageの場合 --*/
    else if (vi) {
      if (TTool::getApplication()->getCurrentObject()->isSpline()) {
        if (!ToolUtils::isJustCreatedSpline(vi.getPointer())) {
          m_primitive->setIsPrompting(true);
          QString question("Are you sure you want to replace the motion path?");
          int ret =
              DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"), 0);
          m_primitive->setIsPrompting(false);
          if (ret == 2 || ret == 0) return;
        }
        TUndo *undo = new UndoPath(
            getXsheet()->getStageObject(getObjectId())->getSpline());
        while (vi->getStrokeCount() > 0) vi->deleteStroke(0);
        vi->addStroke(stroke, false);
        notifyImageChanged();
        TUndoManager::manager()->add(undo);
      } else {
        int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
        if (styleId >= 0) stroke->setStyle(styleId);
        QMutexLocker lock(vi->getMutex());
        std::vector<TFilledRegionInf> *fillInformation =
            new std::vector<TFilledRegionInf>;
        ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                         stroke->getBBox());
        vi->addStroke(stroke);
        TUndoManager::manager()->add(new UndoPencil(
            vi->getStroke(vi->getStrokeCount() - 1), fillInformation, sl, id,
            m_isFrameCreated, m_isLevelCreated, m_param.m_autogroup.getValue(),
            m_param.m_autofill.getValue()));
      }
      if (m_param.m_autogroup.getValue() && stroke->isSelfLoop()) {
        int index = vi->getStrokeCount() - 1;
        vi->group(index, 1);
        if (m_param.m_autofill.getValue()) {
          // to avoid filling other strokes, I enter into the new stroke group
          int currentGroup = vi->exitGroup();
          vi->enterGroup(index);
          vi->selectFill(stroke->getBBox().enlarge(1, 1), 0, stroke->getStyle(),
                         false, true, false);
          if (currentGroup != -1)
            vi->enterGroup(currentGroup);
          else
            vi->exitGroup();
        }
      }
    }
    /*-- RasterImageの場合 --*/
    else if (ri) {
      int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
      stroke->setStyle(styleId);
      double opacity  = m_param.m_opacity.getValue() * 0.01;
      double hardness = m_param.m_hardness.getValue() * 0.01;
      TRect savebox;
      if (hardness == 1) {
        TUndoManager::manager()->add(new UndoFullColorPencil(
            sl, id, stroke, opacity, true, m_isFrameCreated, m_isLevelCreated));
        savebox = TRasterImageUtils::addStroke(ri, stroke, TRectD(), opacity);
      } else {
        int thickness = m_param.m_rasterToolSize.getValue();
        TUndoManager::manager()->add(new FullColorBluredPrimitiveUndo(
            sl, id, stroke, thickness, hardness, opacity, true,
            m_isFrameCreated, m_isLevelCreated));
        savebox = drawBluredBrush(ri, stroke, thickness, hardness, opacity);
      }
      ToolUtils::updateSaveBox();
      delete stroke;
    }
    notifyImageChanged();
    m_active = false;
  }
};

GeometricTool GeometricVectorTool(TTool::Vectors | TTool::EmptyTarget);
GeometricTool GeometricRasterTool(TTool::ToonzImage | TTool::EmptyTarget);
GeometricTool GeometricRasterFullColorTool(TTool::RasterImage |
                                           TTool::EmptyTarget);

//-------------------------------------------------------------------------------------------------------------

void Primitive::drawSnap() {
  // snapping
  if ((m_param->m_targetType & TTool::Vectors) && m_param->m_snap.getValue()) {
    m_param->m_pixelSize = m_tool->getPixelSize();
    double thick         = 6.0 * m_param->m_pixelSize;
    if (m_param->m_foundSnap) {
      tglColor(TPixelD(0.1, 0.9, 0.1));
      tglDrawCircle(m_param->m_snapPoint, thick);
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

TPointD Primitive::checkGuideSnapping(TPointD pos) {
  if (Preferences::instance()->getVectorSnappingTarget() == 0) {
    if (m_param->m_foundSnap)
      return m_param->m_snapPoint;
    else
      return pos;
  }
  if ((m_param->m_targetType & TTool::Vectors) && m_param->m_snap.getValue()) {
    int vGuideCount = 0, hGuideCount = 0;
    double guideDistance  = sqrt(m_param->m_minDistance2);
    TTool::Viewer *viewer = m_tool->getViewer();
    if (viewer) {
      vGuideCount = viewer->getVGuideCount();
      hGuideCount = viewer->getHGuideCount();
    }
    double distanceToVGuide = -1.0, distanceToHGuide = -1.0;
    double vGuide, hGuide;
    bool useGuides = false;
    if (vGuideCount) {
      for (int j = 0; j < vGuideCount; j++) {
        double guide        = viewer->getVGuide(j);
        double tempDistance = abs(guide - pos.y);
        if (tempDistance < guideDistance &&
            (distanceToVGuide < 0 || tempDistance < distanceToVGuide)) {
          distanceToVGuide = tempDistance;
          vGuide           = guide;
          useGuides        = true;
        }
      }
    }
    if (hGuideCount) {
      for (int j = 0; j < hGuideCount; j++) {
        double guide        = viewer->getHGuide(j);
        double tempDistance = abs(guide - pos.x);
        if (tempDistance < guideDistance &&
            (distanceToHGuide < 0 || tempDistance < distanceToHGuide)) {
          distanceToHGuide = tempDistance;
          hGuide           = guide;
          useGuides        = true;
        }
      }
    }
    if (useGuides && m_param->m_foundSnap) {
      double currYDistance = abs(m_param->m_snapPoint.y - pos.y);
      double currXDistance = abs(m_param->m_snapPoint.x - pos.x);
      double hypotenuse =
          sqrt(pow(currYDistance, 2.0) + pow(currXDistance, 2.0));
      if ((distanceToVGuide >= 0 && distanceToVGuide < hypotenuse) ||
          (distanceToHGuide >= 0 && distanceToHGuide < hypotenuse))
        useGuides = true;
      else
        useGuides = false;
    }
    if (useGuides) {
      assert(distanceToHGuide >= 0 || distanceToVGuide >= 0);
      if (distanceToHGuide < 0 ||
          (distanceToVGuide <= distanceToHGuide && distanceToVGuide >= 0)) {
        m_param->m_snapPoint.y = vGuide;
        m_param->m_snapPoint.x = pos.x;

      } else {
        m_param->m_snapPoint.y = pos.y;
        m_param->m_snapPoint.x = hGuide;
      }
      m_param->m_foundSnap = true;
    }
    if (m_param->m_foundSnap)
      return m_param->m_snapPoint;
    else
      return pos;
  } else
    return pos;
}

//=============================================================================
// Rectangle Primitive Class Implementation
//-----------------------------------------------------------------------------

void RectanglePrimitive::draw() {
  drawSnap();
  if (m_isEditing || m_isPrompting ||
      areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1)) {
    tglColor(m_isEditing ? m_color : TPixel32::Green);
    glBegin(GL_LINE_LOOP);
    tglVertex(m_selectingRect.getP00());
    tglVertex(m_selectingRect.getP01());
    tglVertex(m_selectingRect.getP11());
    tglVertex(m_selectingRect.getP10());
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    // app->getCurrentTool()->getTool()->touchImage();
    const TColorStyle *style = app->getCurrentLevelStyle();
    m_isEditing              = style != 0 && style->isStrokeStyle();
    m_color = (style) ? style->getAverageColor() : TPixel32::Black;
  }

  if (!m_isEditing) return;
  TPointD newPos = getSnap(pos);
  if (m_param->m_pencil.getValue() &&
      (m_param->m_targetType & TTool::ToonzImage ||
       m_param->m_targetType & TTool::RasterImage)) {
    if (m_param->m_rasterToolSize.getValue() % 2 != 0)
      m_startPoint = TPointD((int)pos.x, (int)pos.y);
    else
      m_startPoint = TPointD((int)pos.x + 0.5, (int)pos.y + 0.5);
  } else
    m_startPoint     = newPos;
  m_selectingRect.x0 = m_startPoint.x;
  m_selectingRect.y0 = m_startPoint.y;
  m_selectingRect.x1 = m_startPoint.x;
  m_selectingRect.y1 = m_startPoint.y;
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::leftButtonDrag(const TPointD &realPos,
                                        const TMouseEvent &e) {
  if (!m_isEditing) return;

  TPointD pos;
  if (e.isShiftPressed()) {
    double distance = tdistance(realPos, m_startPoint) * M_SQRT1_2;
    pos.x           = (realPos.x > m_startPoint.x) ? m_startPoint.x + distance
                                         : m_startPoint.x - distance;
    pos.y = (realPos.y > m_startPoint.y) ? m_startPoint.y + distance
                                         : m_startPoint.y - distance;
  } else {
    pos = calculateSnap(realPos);
    pos = checkGuideSnapping(realPos);
  }

  if (m_param->m_pencil.getValue() &&
      (m_param->m_targetType & TTool::ToonzImage ||
       m_param->m_targetType & TTool::RasterImage)) {
    if (m_param->m_rasterToolSize.getValue() % 2 != 0)
      pos = TPointD((int)pos.x, (int)pos.y);
    else
      pos = TPointD((int)pos.x + 0.5, (int)pos.y + 0.5);
  }

  m_selectingRect.x1 = pos.x;
  m_selectingRect.y1 = pos.y;
  if (!e.isAltPressed()) {
    m_selectingRect.x0 = m_startPoint.x;
    m_selectingRect.y0 = m_startPoint.y;
  } else {
    m_selectingRect.x0 = m_startPoint.x + m_startPoint.x - pos.x;
    m_selectingRect.y0 = m_startPoint.y + m_startPoint.y - pos.y;
  }
}

//-----------------------------------------------------------------------------

TStroke *RectanglePrimitive::makeStroke() const {
  if (areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1))
    return 0;

  TRectD selArea;
  selArea.x0 = std::min(m_selectingRect.x0, m_selectingRect.x1);
  selArea.y0 = std::min(m_selectingRect.y0, m_selectingRect.y1);
  selArea.x1 = std::max(m_selectingRect.x0, m_selectingRect.x1);
  selArea.y1 = std::max(m_selectingRect.y0, m_selectingRect.y1);

  double thick = getThickness();

  TStroke *stroke = 0;
  if (m_param->m_targetType & TTool::Vectors) {
    std::vector<TThickPoint> points(17);

    points[0] = TThickPoint(selArea.x1, selArea.y1, thick);
    points[1] = TThickPoint(selArea.x1, selArea.y1, thick) + TPointD(-0.01, 0);

    points[3] = TThickPoint(selArea.x0, selArea.y1, thick) + TPointD(0.01, 0);
    points[4] = TThickPoint(selArea.x0, selArea.y1, thick);
    points[5] = TThickPoint(selArea.x0, selArea.y1, thick) + TPointD(0, -0.01);

    points[7] = TThickPoint(selArea.x0, selArea.y0, thick) + TPointD(0, 0.01);
    points[8] = TThickPoint(selArea.x0, selArea.y0, thick);
    points[9] = TThickPoint(selArea.x0, selArea.y0, thick) + TPointD(0.01, 0);

    points[11] = TThickPoint(selArea.x1, selArea.y0, thick) + TPointD(-0.01, 0);
    points[12] = TThickPoint(selArea.x1, selArea.y0, thick);
    points[13] = TThickPoint(selArea.x1, selArea.y0, thick) + TPointD(0, 0.01);

    points[15] = points[0] + TPointD(0, -0.01);
    points[16] = points[0];

    points[2]  = 0.5 * (points[1] + points[3]);
    points[6]  = 0.5 * (points[5] + points[7]);
    points[10] = 0.5 * (points[9] + points[11]);
    points[14] = 0.5 * (points[13] + points[15]);

    stroke = new TStroke(points);
  } else if (m_param->m_targetType & TTool::ToonzImage ||
             m_param->m_targetType & TTool::RasterImage) {
    std::vector<TThickPoint> points(9);
    double middleX = (selArea.x0 + selArea.x1) * 0.5;
    double middleY = (selArea.y0 + selArea.y1) * 0.5;

    points[0] = TThickPoint(selArea.x1, selArea.y1, thick);
    points[1] = TThickPoint(middleX, selArea.y1, thick);
    points[2] = TThickPoint(selArea.x0, selArea.y1, thick);
    points[3] = TThickPoint(selArea.x0, middleY, thick);
    points[4] = TThickPoint(selArea.x0, selArea.y0, thick);
    points[5] = TThickPoint(middleX, selArea.y0, thick);
    points[6] = TThickPoint(selArea.x1, selArea.y0, thick);
    points[7] = TThickPoint(selArea.x1, middleY, thick);
    points[8] = points[0];
    stroke    = new TStroke(points);
  }
  stroke->setSelfLoop();
  return stroke;
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;
  m_tool->addStroke();
  resetSnap();
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos);
  newPos         = checkGuideSnapping(pos);
  m_pos          = newPos;
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// Circle Primitive Class Implementation
//-----------------------------------------------------------------------------

void CirclePrimitive::draw() {
  drawSnap();
  if (m_isEditing || m_isPrompting) {
    tglColor(m_isEditing ? m_color : TPixel32::Green);
    tglDrawCircle(m_centre, m_radius);
  }
}

//-----------------------------------------------------------------------------

void CirclePrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  m_pos    = getSnap(pos);
  m_centre = m_pos;

  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;
}

//-----------------------------------------------------------------------------

void CirclePrimitive::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_isEditing) return;

  m_pos    = pos;
  m_pos    = calculateSnap(pos);
  m_pos    = checkGuideSnapping(pos);
  m_radius = tdistance(m_centre, m_pos);
}

//-----------------------------------------------------------------------------

TStroke *CirclePrimitive::makeStroke() const {
  return makeEllipticStroke(getThickness(), m_centre, m_radius, m_radius);
}

//-----------------------------------------------------------------------------

void CirclePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;
  if (isAlmostZero(m_radius)) return;

  m_tool->addStroke();
  m_radius = 0;
}

//-----------------------------------------------------------------------------

void CirclePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_pos = calculateSnap(pos);
  m_pos = checkGuideSnapping(pos);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void CirclePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// MultiLine Primitive Class Implementation
//-----------------------------------------------------------------------------

void MultiLinePrimitive::addVertex(const TPointD &pos) {
  int count = m_vertex.size();
  // Inserisco il primo punto
  if (count == 0) {
    m_vertex.push_back(pos);
    return;
  }

  TPointD &vertex = m_vertex[count - 1];

  // Caso particolare in cui inizio una curva e la chiudo subito cliccando sul
  // punto di pertenza
  if (count == 1 && pos == vertex) {
    m_vertex.push_back(pos);
    m_vertex.push_back(pos);
    m_vertex.push_back(pos);
    return;
  }

  // Calcolo lo speedOut
  TPointD speedOutPoint;
  if (!m_speedMoved)  // Se non e' stato mosso lo speedOut devo calcolarlo e
                      // inserirlo
  {
    speedOutPoint = vertex + computeSpeed(vertex, pos, 0.01);
    m_vertex.push_back(speedOutPoint);
  } else {
    if (m_ctrlDown)
      vertex =
          m_vertex[count - 2] + computeSpeed(m_vertex[count - 2], pos, 0.01);
    speedOutPoint = vertex;
  }

  // Calcolo lo speedIn
  TPointD speedInPoint = pos + computeSpeed(pos, speedOutPoint, 0.01);
  // Calcolo il "punto di mezzo" e lo inserisco
  TPointD middlePoint = 0.5 * (speedInPoint + speedOutPoint);

  // Inserisco il "punto di mezzo"
  m_vertex.push_back(middlePoint);
  // Inserisco lo speedIn
  m_vertex.push_back(speedInPoint);
  // Inserisco il nuovo punto
  m_vertex.push_back(pos);
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::moveSpeed(const TPointD &delta) {
  int count = m_vertex.size();
  assert(count > 0);
  TPointD lastPoint        = m_vertex[count - 1];
  TPointD newSpeedOutPoint = lastPoint - delta;
  if (m_speedMoved)
    m_vertex[count - 1] = newSpeedOutPoint;
  else {
    m_vertex.push_back(newSpeedOutPoint);
    ++count;
  }
  if (count < 5) {
    assert(count == 2);
    return;
  }

  TPointD vertex = m_vertex[count - 2];

  TPointD v(0, 0);
  if (newSpeedOutPoint != vertex) v = normalize(newSpeedOutPoint - vertex);

  double speedOut         = tdistance(newSpeedOutPoint, vertex);
  TPointD newSpeedInPoint = vertex - TPointD(speedOut * v.x, speedOut * v.y);

  m_vertex[count - 3] = newSpeedInPoint;
  if (tdistance(m_vertex[count - 5], m_vertex[count - 6]) <= 0.02)
    // see ControlPointEditorStroke::isSpeedOutLinear() from
    // controlpointselection.cpp
    m_vertex[count - 5] =
        m_vertex[count - 6] +
        computeSpeed(m_vertex[count - 6], m_vertex[count - 3], 0.01);
  m_vertex[count - 4] = 0.5 * (m_vertex[count - 3] + m_vertex[count - 5]);
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::draw() {
  UINT size = m_vertex.size();

  drawSnap();

  if ((m_isEditing || m_isPrompting) && size > 0) {
    tglColor(m_isEditing ? m_color : TPixel32::Green);
    std::vector<TPointD> points;
    points.assign(m_vertex.begin(), m_vertex.end());
    int count = points.size();
    if (count % 4 == 1) {
      // No speedOut
      points.push_back(points[count - 1]);
      count++;
    } else if (m_ctrlDown)
      points[count - 1] = points[count - 2];

    points.push_back(0.5 * (m_mousePosition + points[count - 1]));
    points.push_back(m_mousePosition);
    points.push_back(m_mousePosition);

    double pixelSize = m_tool->getPixelSize();

    TStroke *stroke = new TStroke(points);
    drawStrokeCenterline(*stroke, pixelSize);
    delete stroke;

    if (m_vertex.size() > 1) {
      tglColor(TPixel(79, 128, 255));
      int index = (count < 5) ? count - 1 : count - 5;
      // Disegno lo speedOut precedente (che e' quello corrente solo nel caso in
      // cui count < 5)
      TPointD p0 = m_vertex[index];
      TPointD p1 = m_vertex[index - 1];
      if (tdistance(p0, p1) > 0.1) {
        tglDrawSegment(p0, p1);
        tglDrawDisk(p0, 2 * pixelSize);
        tglDrawDisk(p1, 4 * pixelSize);
      }
      // Disegno lo speedIn/Out corrente nel caso in cui count > 5
      if (m_speedMoved && count > 5) {
        TPointD p0 = m_vertex[count - 1];
        TPointD p1 = m_vertex[count - 2];
        TPointD p2 = m_vertex[count - 3];
        tglDrawSegment(p0, p2);
        tglDrawDisk(p0, 2 * pixelSize);
        tglDrawDisk(p1, 4 * pixelSize);
        tglDrawDisk(p2, 2 * pixelSize);
      }
    }

    if (m_closed)
      tglColor(TPixel32((m_color.r + 127) % 255, m_color.g,
                        (m_color.b + 127) % 255, m_color.m));
    else
      tglColor(m_color);
    tglDrawCircle(m_vertex[0], joinDistance * pixelSize);
  }
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  m_undo = new MultilinePrimitiveUndo(m_vertex, this);
  TUndoManager::manager()->add(m_undo);
  m_mousePosition = pos;

  TPointD newPos;
  newPos = getSnap(pos);

  // Se clicco nell'ultimo vertice chiudo la linea.
  TPointD _pos       = pos;
  if (m_closed) _pos = m_vertex.front();

  if (e.isShiftPressed() && !m_vertex.empty())
    addVertex(rectify(m_vertex.back(), _pos));
  else
    addVertex(newPos);
  m_undo->setNewVertex(m_vertex);

  m_beforeSpeedMoved = m_speedMoved;
  m_speedMoved       = false;
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  if (m_vertex.size() == 0 || m_isSingleLine) return;
  if (m_speedMoved ||
      tdistance2(m_vertex[m_vertex.size() - 1], pos) >
          sq(7.0 * m_tool->getPixelSize())) {
    moveSpeed(m_mousePosition - pos);
    m_speedMoved = true;
    m_undo->setNewVertex(m_vertex);
    m_mousePosition = pos;
  }
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonDoubleClick(const TPointD &,
                                               const TMouseEvent &e) {
  endLine();
  resetSnap();
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (m_closed) endLine();
  resetSnap();
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_ctrlDown = e.isCtrlPressed();
  TPointD newPos;
  newPos = calculateSnap(pos);
  newPos = checkGuideSnapping(pos);

  if (m_isEditing) {
    if (e.isShiftPressed() && !m_vertex.empty())
      m_mousePosition = rectify(m_vertex.back(), newPos);
    else
      m_mousePosition = newPos;

    double dist = joinDistance * joinDistance;

    if (!m_vertex.empty() &&
        (tdistance2(pos, m_vertex.front()) < dist * m_tool->getPixelSize())) {
      m_closed        = true;
      m_mousePosition = m_vertex.front();
    } else
      m_closed = false;

  } else
    m_mousePosition = newPos;
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

bool MultiLinePrimitive::keyDown(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    endLine();
    return true;
  }

  if (event->key() != Qt::Key_Escape || !m_isEditing) return false;

  UINT size = m_vertex.size();
  if (size <= 1) return 0;

  if (!m_isSingleLine) TUndoManager::manager()->popUndo((size - 1) / 4 + 1);

  m_isEditing        = false;
  m_speedMoved       = false;
  m_beforeSpeedMoved = false;
  m_closed           = false;

  m_vertex.clear();
  return true;
}

//-----------------------------------------------------------------------------

TStroke *MultiLinePrimitive::makeStroke() const {
  double thick = getThickness();

  /*---
   * Pencilの場合は、線幅を減らす。Thickness1の線を1ピクセルにするため。（thick
   * = 0 になる）---*/
  if (m_param->m_pencil.getValue()) thick -= 1.0;

  UINT size = m_vertex.size();
  if (size <= 1) return 0;

  if (!m_isSingleLine) TUndoManager::manager()->popUndo((size - 1) / 4 + 1);

  TStroke *stroke = 0;
  std::vector<TThickPoint> points;
  int i;
  for (i = 0; i < (int)size; i++) {
    TPointD vertex = m_vertex[i];
    points.push_back(TThickPoint(vertex, thick));
  }
  stroke = new TStroke(points);
  if (m_closed) stroke->setSelfLoop();

  return stroke;
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::endLine() {
  if (!m_isEditing) return;

  m_isEditing        = false;
  m_speedMoved       = false;
  m_beforeSpeedMoved = false;

  if (!m_isSingleLine && !m_vertex.empty() &&
      m_vertex.size() % 4 != 1 /* && !m_rasterTool*/) {
    m_vertex.erase(--m_vertex.end());
    assert(m_vertex.size() == 3 || m_vertex.size() % 4 == 1);
  }

  m_tool->addStroke();

  if (m_closed) m_closed = false;

  m_vertex.clear();
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::onActivate() {
  m_isEditing = false;
  m_closed    = false;
  m_vertex.clear();
  m_speedMoved       = false;
  m_beforeSpeedMoved = false;
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::onImageChanged() { onActivate(); }

//=============================================================================
// Line Primitive Class Implementation
//-----------------------------------------------------------------------------

void LinePrimitive::draw() {
  UINT size = m_vertex.size();

  drawSnap();

  tglColor(TPixel32::Red);

  if (m_isEditing || m_isPrompting) {
    glBegin(GL_LINE_STRIP);
    tglVertex(m_vertex[0]);

    tglVertex(m_mousePosition);
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void LinePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos);
  newPos         = checkGuideSnapping(pos);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void LinePrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  TPointD newPos = getSnap(pos);

  m_mousePosition = newPos;

  TPointD _pos = newPos;

  if (m_param->m_pencil.getValue() &&
      (m_param->m_targetType & TTool::ToonzImage ||
       m_param->m_targetType & TTool::RasterImage)) {
    if (m_param->m_rasterToolSize.getValue() % 2 != 0)
      _pos = TPointD((int)newPos.x, (int)newPos.y);
    else
      _pos = TPointD((int)newPos.x + 0.5, (int)newPos.y + 0.5);
  }

  if (m_vertex.size() == 0)
    addVertex(_pos);
  else {
    if (e.isShiftPressed() && !m_vertex.empty())
      addVertex(rectify(m_vertex.back(), pos));
    else
      addVertex(_pos);
    endLine();
  }
}

//-----------------------------------------------------------------------------

void LinePrimitive::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_isEditing) return;
  TPointD newPos = calculateSnap(pos);
  newPos         = checkGuideSnapping(pos);

  m_mousePosition = newPos;
}
//-----------------------------------------------------------------------------

void LinePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  // snapping
  TPointD newPos = getSnap(pos);

  m_mousePosition = newPos;
  if (e.isShiftPressed() && !m_vertex.empty())
    m_vertex.push_back(rectify(m_vertex.back(), pos));
  else
    m_vertex.push_back(newPos);

  endLine();

  resetSnap();
}
//-----------------------------------------------------------------------------

//=============================================================================
// Ellipse Primitive Class Implementation
//-----------------------------------------------------------------------------

void EllipsePrimitive::draw() {
  drawSnap();
  if (m_isEditing || m_isPrompting ||
      areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1)) {
    tglColor(m_isEditing ? m_color : TPixel32::Green);
    TPointD centre = TPointD((m_selectingRect.x0 + m_selectingRect.x1) * 0.5,
                             (m_selectingRect.y0 + m_selectingRect.y1) * 0.5);

    glPushMatrix();
    tglMultMatrix(TScale(centre, m_selectingRect.x1 - m_selectingRect.x0,
                         m_selectingRect.y1 - m_selectingRect.y0));
    tglDrawCircle(centre, 0.5);

    glPopMatrix();
    drawRect(m_selectingRect, m_color, 0x5555, true);
  }
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;
  TPointD newPos     = getSnap(pos);
  m_startPoint       = newPos;
  m_selectingRect.x0 = newPos.x;
  m_selectingRect.y0 = newPos.y;
  m_selectingRect.x1 = newPos.x;
  m_selectingRect.y1 = newPos.y;

  if (app->getCurrentObject()->isSpline()) {
    m_isEditing = true;
    m_color     = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::leftButtonDrag(const TPointD &realPos,
                                      const TMouseEvent &e) {
  if (!m_isEditing) return;

  TPointD pos;
  if (e.isShiftPressed()) {
    double distance = tdistance(realPos, m_startPoint) * M_SQRT1_2;
    pos.x           = (realPos.x > m_startPoint.x) ? m_startPoint.x + distance
                                         : m_startPoint.x - distance;
    pos.y = (realPos.y > m_startPoint.y) ? m_startPoint.y + distance
                                         : m_startPoint.y - distance;
  } else {
    pos = calculateSnap(realPos);
    pos = checkGuideSnapping(realPos);
  }
  m_pos = pos;

  m_selectingRect.x1 = pos.x;
  m_selectingRect.y1 = pos.y;
  if (!e.isAltPressed()) {
    m_selectingRect.x0 = m_startPoint.x;
    m_selectingRect.y0 = m_startPoint.y;
  } else {
    m_selectingRect.x0 = m_startPoint.x + m_startPoint.x - pos.x;
    m_selectingRect.y0 = m_startPoint.y + m_startPoint.y - pos.y;
  }
}

//-----------------------------------------------------------------------------

TStroke *EllipsePrimitive::makeStroke() const {
  if (areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1))
    return 0;

  return makeEllipticStroke(
      getThickness(), TPointD(0.5 * (m_selectingRect.x0 + m_selectingRect.x1),
                              0.5 * (m_selectingRect.y0 + m_selectingRect.y1)),
      fabs(0.5 * (m_selectingRect.x1 - m_selectingRect.x0)),
      fabs(0.5 * (m_selectingRect.y1 - m_selectingRect.y0)));
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;

  m_tool->addStroke();
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_pos = calculateSnap(pos);
  m_pos = checkGuideSnapping(pos);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// Arc Primitive Class Implementation
//-----------------------------------------------------------------------------

void ArcPrimitive::draw() {
  drawSnap();

  switch (m_clickNumber) {
  case 1:
    tglColor(m_color);
    tglDrawSegment(m_startPoint, m_endPoint);
    break;
  case 2:
    assert(m_stroke);
    if (m_stroke) {
      tglColor(m_isPrompting ? TPixel32::Green : m_color);
      if (!m_isPrompting) {
        glLineStipple(1, 0x5555);
        glEnable(GL_LINE_STIPPLE);
        glBegin(GL_LINE_STRIP);
        tglVertex(m_stroke->getControlPoint(0));
        tglVertex(m_centralPoint);
        tglVertex(m_stroke->getControlPoint(8));
        glEnd();
        glDisable(GL_LINE_STIPPLE);
      }

      drawStrokeCenterline(*m_stroke, sqrt(tglGetPixelSize2()));
    }
    break;
  };
}

//-----------------------------------------------------------------------------

TStroke *ArcPrimitive::makeStroke() const { return new TStroke(*m_stroke); }

//-----------------------------------------------------------------------------

void ArcPrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  TPointD newPos = getSnap(pos);

  std::vector<TThickPoint> points(9);
  double thick = getThickness();

  switch (m_clickNumber) {
  case 0:
    if (app->getCurrentObject()->isSpline()) {
      m_isEditing = true;
      m_color     = TPixel32::Red;
    } else {
      const TColorStyle *style = app->getCurrentLevelStyle();
      if (style) {
        m_isEditing = style->isStrokeStyle();
        m_color     = style->getAverageColor();
      } else {
        m_isEditing = false;
        m_color     = TPixel32::Black;
      }
    }

    if (!m_isEditing) return;

    m_endPoint = m_startPoint = newPos;
    m_clickNumber++;
    break;

  case 1:
    points[0] = TThickPoint(m_startPoint, thick);
    points[8] = TThickPoint(newPos, thick);
    points[4] = TThickPoint(0.5 * (points[0] + points[8]), thick);
    points[2] = TThickPoint(0.5 * (points[0] + points[4]), thick);
    points[6] = TThickPoint(0.5 * (points[4] + points[8]), thick);

    points[1] = TThickPoint(0.5 * (points[0] + points[2]), thick);
    points[3] = TThickPoint(0.5 * (points[2] + points[4]), thick);
    points[5] = TThickPoint(0.5 * (points[4] + points[6]), thick);
    points[7] = TThickPoint(0.5 * (points[6] + points[8]), thick);
    if (m_stroke) delete m_stroke;
    m_stroke = new TStroke(points);
    m_clickNumber++;
    break;

  case 2:
    m_tool->addStroke();
    m_stroke      = 0;
    m_clickNumber = 0;
    break;
  }
  resetSnap();
}

//-----------------------------------------------------------------------------

void ArcPrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos);
  newPos         = checkGuideSnapping(pos);

  switch (m_clickNumber) {
  case 1:
    if (e.isShiftPressed())
      m_endPoint = rectify(m_startPoint, pos);
    else
      m_endPoint = newPos;
    break;
  case 2:
    m_centralPoint = TThickPoint(newPos, getThickness());
    TThickQuadratic q(m_stroke->getControlPoint(0), m_centralPoint,
                      m_stroke->getControlPoint(8));
    TThickQuadratic q0, q1, q00, q01, q10, q11;

    q.split(0.5, q0, q1);
    q0.split(0.5, q00, q01);
    q1.split(0.5, q10, q11);

    assert(q00.getP2() == q01.getP0());
    assert(q01.getP2() == q10.getP0());
    assert(q10.getP2() == q11.getP0());

    m_stroke->setControlPoint(0, q00.getP0());
    m_stroke->setControlPoint(1, q00.getP1());
    m_stroke->setControlPoint(2, q00.getP2());
    m_stroke->setControlPoint(3, q01.getP1());
    m_stroke->setControlPoint(4, q01.getP2());
    m_stroke->setControlPoint(5, q10.getP1());
    m_stroke->setControlPoint(6, q10.getP2());
    m_stroke->setControlPoint(7, q11.getP1());
    m_stroke->setControlPoint(8, q11.getP2());
    break;
  }
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void ArcPrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// Polygon Primitive Class Declaration
//-----------------------------------------------------------------------------

void PolygonPrimitive::draw() {
  drawSnap();
  if (!m_isEditing && !m_isPrompting) return;
  tglColor(m_isEditing ? m_color : TPixel32::Green);

  int edgeCount = m_param->m_edgeCount.getValue();
  if (edgeCount == 0) return;

  double angleDiff = M_2PI / edgeCount;
  double angle     = (3 * M_PI + angleDiff) * 0.5;

  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < edgeCount; i++) {
    tglVertex(m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius));
    angle += angleDiff;
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_isEditing = true;
    m_color     = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  m_centre = getSnap(pos);
  m_radius = 0;
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::leftButtonDrag(const TPointD &pos,
                                      const TMouseEvent &e) {
  if (!m_isEditing) return;
  TPointD newPos = calculateSnap(pos);
  newPos         = checkGuideSnapping(pos);
  m_radius       = tdistance(m_centre, newPos);
}

//-----------------------------------------------------------------------------

TStroke *PolygonPrimitive::makeStroke() const {
  double thick = getThickness();

  int edgeCount = m_param->m_edgeCount.getValue();
  if (edgeCount == 0) return 0;

  double angleDiff = M_2PI / (double)edgeCount;
  double angle     = (3 * M_PI + angleDiff) * 0.5;

  TStroke *stroke = 0;
  if (m_param->m_targetType & TTool::Vectors) {
    std::vector<TThickPoint> points(4 * edgeCount + 1);
    int i;
    // Posiziono gli angoli
    for (i = 0; i <= (int)points.size(); i += 4) {
      points[i] = TThickPoint(
          m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius),
          thick);
      angle += angleDiff;
    }
    // posiziono i punti medi e i punti per gestire la linearita'
    for (i = 0; i < (int)points.size() - 1; i += 4) {
      TPointD vertex           = convert(points[i]);
      TPointD nextVertex       = convert(points[i + 4]);
      TPointD speed            = computeSpeed(vertex, nextVertex, 0.01);
      TPointD speedOutPoint    = vertex + speed;
      TPointD speedInNextPoint = nextVertex - speed;
      TPointD middlePoint      = 0.5 * (speedOutPoint + speedInNextPoint);
      points[i + 1]            = TThickPoint(speedOutPoint, thick);
      points[i + 2]            = TThickPoint(middlePoint, thick);
      points[i + 3]            = TThickPoint(speedInNextPoint, thick);
    }
    stroke = new TStroke(points);
  } else if (m_param->m_targetType & TTool::ToonzImage ||
             m_param->m_targetType & TTool::RasterImage) {
    std::vector<TThickPoint> points(edgeCount + edgeCount + 1);
    points[0] = TThickPoint(
        m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius),
        thick);
    for (int i = 1; i <= edgeCount; i++) {
      angle += angleDiff;
      points[i + i] = TThickPoint(
          m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius),
          thick);
      points[i + i - 1] = (points[i + i - 2] + points[i + i]) * 0.5;
    }
    stroke = new TStroke(points);
  }
  stroke->setSelfLoop();
  return stroke;
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;

  m_tool->addStroke();
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos);
  newPos         = checkGuideSnapping(pos);
  m_tool->invalidate();
}