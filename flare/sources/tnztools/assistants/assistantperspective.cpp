

// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>

#include "assistantline.h"
#include "assistantvanishingpoint.h"

// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TAssistantPerspective implementation
//*****************************************************************************************

class TAssistantPerspective final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantPerspective)
public:
  const TStringId m_idParallelX;
  const TStringId m_idParallelY;
  const TStringId m_idParallelZ;
  const TStringId m_idGridXY;
  const TStringId m_idGridYZ;
  const TStringId m_idGridZX;
  const TStringId m_idShowBox;

protected:
  TAssistantPoint &m_o;
  TAssistantPoint &m_x;
  TAssistantPoint &m_y;
  TAssistantPoint &m_z;
  TAssistantPoint &m_xy;
  TAssistantPoint &m_yz;
  TAssistantPoint &m_zx;
  TAssistantPoint &m_vx;
  TAssistantPoint &m_vy;
  TAssistantPoint &m_vz;
  
public:
  TAssistantPerspective(TMetaObject &object):
    TAssistant(object),
    m_idParallelX("parallelX"),
    m_idParallelY("parallelY"),
    m_idParallelZ("parallelZ"),
    m_idGridXY("gridXY"),
    m_idGridYZ("gridYZ"),
    m_idGridZX("gridZX"),
    m_idShowBox("showBox"),
    m_o    ( addPoint("o",     TAssistantPoint::CircleCross)                          ),
    m_x    ( addPoint("x",     TAssistantPoint::CircleFill,       TPointD(  50,  0 )) ),
    m_y    ( addPoint("y",     TAssistantPoint::CircleFill,       TPointD(   0, 50 )) ),
    m_z    ( addPoint("z",     TAssistantPoint::CircleFill,       TPointD(  25, 25 )) ),
    m_xy   ( addPoint("xy",    TAssistantPoint::Circle,           TPointD(  50, 50 )) ),
    m_yz   ( addPoint("yz",    TAssistantPoint::Circle,           TPointD(  25, 25 )) ),
    m_zx   ( addPoint("zx",    TAssistantPoint::Circle,           TPointD(  75, 25 )) ),
    m_vx   ( addPoint("vx",    TAssistantPoint::Circle)                               ),
    m_vy   ( addPoint("vy",    TAssistantPoint::Circle)                               ),
    m_vz   ( addPoint("vz",    TAssistantPoint::Circle)                               )
  {
    addProperty( new TBoolProperty(m_idParallelX.str(), getParallelX()) );
    addProperty( new TBoolProperty(m_idParallelY.str(), getParallelY()) );
    addProperty( new TBoolProperty(m_idParallelZ.str(), getParallelZ()) );
    addProperty( new TBoolProperty(m_idGridXY.str(), getGridXY()) );
    addProperty( new TBoolProperty(m_idGridYZ.str(), getGridYZ()) );
    addProperty( new TBoolProperty(m_idGridZX.str(), getGridZX()) );
    addProperty( new TBoolProperty(m_idShowBox.str(), getShowBox()) );
  }


  static QString getLocalName()
    { return tr("Perspective"); }


  void updateTranslation() const override {
    TAssistant::updateTranslation();
    setTranslation(m_idParallelX, tr("Parallel X"));
    setTranslation(m_idParallelY, tr("Parallel Y"));
    setTranslation(m_idParallelZ, tr("Parallel Z"));
    setTranslation(m_idGridXY, tr("Grid XY"));
    setTranslation(m_idGridYZ, tr("Grid YZ"));
    setTranslation(m_idGridZX, tr("Grid ZX"));
    setTranslation(m_idShowBox, tr("Show Box"));
  }


  inline bool getParallelX() const
    { return data()[m_idParallelX].getBool(); }
  inline bool getParallelY() const
    { return data()[m_idParallelY].getBool(); }
  inline bool getParallelZ() const
    { return data()[m_idParallelZ].getBool(); }
  inline bool getGridXY() const
    { return data()[m_idGridXY].getBool(); }
  inline bool getGridYZ() const
    { return data()[m_idGridYZ].getBool(); }
  inline bool getGridZX() const
    { return data()[m_idGridZX].getBool(); }
  inline bool getShowBox() const
    { return data()[m_idShowBox].getBool(); }

  void onDataChanged(const TVariant &value) override {
    TAssistant::onDataChanged(value);
    m_xy.visible = !getParallelX() || !getParallelY();
    m_yz.visible = !getParallelY() || !getParallelZ();
    m_zx.visible = !getParallelZ() || !getParallelX();
    fixPoints();
  }


private:
  void fixAxisPoint(
    TPointD &a,
    const TAssistantPoint &v,
    const TPointD &oldV ) const
  {
    const TPointD &o = m_o.position;
    if (!v.visible)
      { a = o + v.position; return; }
    
    TPointD dv = v.position - o;
    TPointD oldDv = oldV - o;
    double l = norm2(oldDv);
    double ln = norm2(dv);
    if (!(l > TConsts::epsilon) || !(ln > TConsts::epsilon))
      return;
    
    double d = (a - o)*oldDv;
    a = o + dv*(d/l);
  }
  

  inline void fixAxisPoint(
    TAssistantPoint &a,
    const TAssistantPoint &v,
    const TPointD &oldV ) const
      { fixAxisPoint(a.position, v, oldV); }

  
  void fixVanishingPoint(
    TAssistantPoint &v,
    const TPointD &a,
    const TPointD &oldA ) const
  {
    const TPointD &o = m_o.position;
    TPointD da = a - o;
    if (!v.visible)
      { v.position = da; return; }
    
    TPointD oldDa = oldA - o;
    double l = norm2(oldDa);
    double ln = norm2(da);
    if (!(l > TConsts::epsilon) || !(ln > TConsts::epsilon))
      return;
    
    double d = (v.position - o)*oldDa;
    v.position = o + da*(d/l);
  }
  

  inline void fixVanishingPoint(
    TAssistantPoint &v,
    const TAssistantPoint &a,
    const TPointD &oldA ) const
      { fixVanishingPoint(v, a.position, oldA); }


  void fixVanishingPoint(
    TAssistantPoint &v,
    const TPointD &a0,
    const TPointD &a1,
    const TPointD &b0,
    const TPointD &b1 ) const
  {
    TPointD da = a1 - a0;
    TPointD db = b1 - b0;
    const TPointD ab = b0 - a0;
    double k = db.x*da.y - db.y*da.x;

    if ( (&v == &m_vx && getParallelX())
      || (&v == &m_vy && getParallelY())
      || (&v == &m_vz && getParallelZ()) )
        k = 0;
    
    if (fabs(k) > TConsts::epsilon) {
      double lb = (da.x*ab.y - da.y*ab.x)/k;
      v.position.x = lb*db.x + b0.x;
      v.position.y = lb*db.y + b0.y;
      v.visible = true;
    } else {
      v.position = da;
      v.visible = false;
    }
  }


  inline void fixVanishingPoint(
    TAssistantPoint &v,
    const TAssistantPoint &a0,
    const TAssistantPoint &a1,
    const TAssistantPoint &b0,
    const TAssistantPoint &b1 ) const
      { fixVanishingPoint(v, a0.position, a1.position, b0.position, b1.position); }

  
  static bool lineCross(
    TPointD &p,
    const TPointD &a,
    const TPointD &b,
    const TPointD &da,
    const TPointD &db )
  {
    double d = da.x*db.y - da.y*db.x;
    if (!(fabs(d) > TConsts::epsilon))
      return false;
    d = 1/d;
    p = TPointD(
      (a.y*db.x + b.x*db.y)*da.x - (a.x*da.y + b.y*da.x)*db.x,
      (a.y*da.x + b.x*da.y)*db.y - (a.x*db.y + b.y*db.x)*da.y )*d;
    return true;
  }
  

  void fixSidePoint(
    TPointD &p,
    const TPointD &a, // pass 'a' and 'b' by copy
    const TPointD &b, 
    const TAssistantPoint &va,
    const TAssistantPoint &vb ) const
  {
    
    TPointD da = va.visible ? va.position - a : va.position;
    TPointD db = vb.visible ? vb.position - b : vb.position;
    lineCross(p, a, b, da, db);
  }


  inline void fixSidePoint(
    TAssistantPoint &p,
    const TAssistantPoint &a,
    const TAssistantPoint &b,
    const TAssistantPoint &va,
    const TAssistantPoint &vb ) const
  { fixSidePoint(p.position, a.position, b.position, va, vb); }


  void fixSidePoints() {
    fixSidePoint(m_xy, m_x, m_y, m_vy, m_vx);
    fixSidePoint(m_yz, m_y, m_z, m_vz, m_vy);
    fixSidePoint(m_zx, m_z, m_x, m_vx, m_vz);
  }

  
  void addGuideline(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    const TAssistantPoint &v,
    TGuidelineList &outGuidelines ) const
  {
    if (v.visible) {
      TPointD p = toTool * v.position;
      if (tdistance2(p, position) > 4*TConsts::epsilon*TConsts::epsilon)
        outGuidelines.push_back(
          new TGuidelineRay(
            getEnabled(),
            getMagnetism(),
            color, 
            p,
            position ));
    } else {
      TPointD d = toTool.transformDirection(v.position);
      if (norm2(d) > 4*TConsts::epsilon*TConsts::epsilon)
        outGuidelines.push_back(
          new TGuidelineInfiniteLine(
            getEnabled(),
            getMagnetism(),
            color, 
            position,
            position + d ));
    }
  }
      
public:
  void onFixPoints() override {
    fixVanishingPoint(m_vx, m_o, m_x, m_y, m_xy);
    fixVanishingPoint(m_vy, m_o, m_y, m_x, m_xy);
    fixVanishingPoint(m_vz, m_o, m_z, m_x, m_zx);
    fixSidePoints();
  }

  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    if (!point.visible)
      return;

    TPointD old = point.position;
    point.position = position;
    
    if (&point == &m_o) {
      TPointD d = m_o.position - old;
      m_x.position  += d;
      m_y.position  += d;
      m_z.position  += d;
      m_xy.position += d;
      m_yz.position += d;
      m_zx.position += d;
      if (m_vx.visible) m_vx.position += d;
      if (m_vy.visible) m_vy.position += d;
      if (m_vz.visible) m_vz.position += d;
    } else
    if (&point == &m_x) {
      fixVanishingPoint(m_vx, m_x, old);
      fixSidePoints();
    } else
    if (&point == &m_y) {
      fixVanishingPoint(m_vy, m_y, old);
      fixSidePoints();
    } else
    if (&point == &m_z) {
      fixVanishingPoint(m_vz, m_z, old);
      fixSidePoints();
    } else
    if (&point == &m_xy) {
      fixVanishingPoint(m_vx, m_o, m_x, m_y, m_xy);
      fixVanishingPoint(m_vy, m_o, m_y, m_x, m_xy);
      fixSidePoints();
    } else
    if (&point == &m_yz) {
      fixVanishingPoint(m_vy, m_o, m_y, m_z, m_yz);
      fixVanishingPoint(m_vz, m_o, m_z, m_y, m_yz);
      fixSidePoints();
    } else
    if (&point == &m_zx) {
      fixVanishingPoint(m_vz, m_o, m_z, m_x, m_zx);
      fixVanishingPoint(m_vx, m_o, m_x, m_z, m_zx);
      fixSidePoints();
    } else
    if (&point == &m_vx) {
      fixAxisPoint(m_x, m_vx, old);
      fixSidePoints();
    } else
    if (&point == &m_vy) {
      fixAxisPoint(m_y, m_vy, old);
      fixSidePoints();
    } else
    if (&point == &m_vz) {
      fixAxisPoint(m_z, m_vz, old);
      fixSidePoints();
    }
  }

  void getGuidelines(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    TGuidelineList &outGuidelines ) const override
  {
    addGuideline(position, toTool, color, m_vx, outGuidelines);
    addGuideline(position, toTool, color, m_vy, outGuidelines);
    addGuideline(position, toTool, color, m_vz, outGuidelines);
  }

  void drawGrid(
    const TAssistantPoint &vx,
    const TAssistantPoint &vy,
    const TPointD &y ) const
  {
    double alpha = getDrawingGridAlpha();
    const TPointD &o = m_o.position;
    
    if (!vx.visible && !vy.visible) {
      TAssistantLine::drawGrid(
        o, o + vx.position, o, y, false, false, false, alpha );
      return;
    }
    
    if (!vx.visible) {
      TAssistantLine::drawGrid(
        vy.position, vy.position + vx.position, o, y, false, false, true, alpha );
      return;
    }
    
    TPointD p = y;
    if (vy.visible) {
      const TPointD &a = vx.position;
      const TPointD &b = o;
      const TPointD da = y - a;
      const TPointD db = vy.position - vx.position;
      lineCross(p, a, b, da, db);
    }
    
    TAssistantVanishingPoint::drawPerspectiveGrid(vx.position, o, p, alpha);
  }
  
  void drawVanishingPoint(const TAssistantPoint &v, double pixelSize, double alpha) const {
    if (!v.visible)
      return;
    const TPointD &p = v.position;
    TPointD dx(20.0*pixelSize, 0.0);
    TPointD dy(0.0, 10.0*pixelSize);
    drawSegment(p-dx-dy, p+dx+dy, pixelSize, alpha);
    drawSegment(p-dx+dy, p+dx-dy, pixelSize, alpha);
  }

  void drawBox(double alpha) const {
    double pixelSize = sqrt(tglGetPixelSize2());
    TPointD xyz;
    fixSidePoint(xyz, m_xy.position, m_zx.position, m_vz, m_vy);
    drawSegment(xyz, m_xy.position, pixelSize, alpha);
    drawSegment(xyz, m_yz.position, pixelSize, alpha);
    drawSegment(xyz, m_zx.position, pixelSize, alpha);
    drawSegment(m_xy.position, m_x.position, pixelSize, alpha);
    drawSegment(m_xy.position, m_y.position, pixelSize, alpha);
    drawSegment(m_yz.position, m_y.position, pixelSize, alpha);
    drawSegment(m_yz.position, m_z.position, pixelSize, alpha);
    drawSegment(m_zx.position, m_z.position, pixelSize, alpha);
    drawSegment(m_zx.position, m_x.position, pixelSize, alpha);
    drawSegment(m_o.position, m_x.position, pixelSize, alpha);
    drawSegment(m_o.position, m_y.position, pixelSize, alpha);
    drawSegment(m_o.position, m_z.position, pixelSize, alpha);
  }
  
  void draw(TToolViewer*, bool enabled) const override {
    double pixelSize = sqrt(tglGetPixelSize2());
    double alpha = getDrawingAlpha(enabled);
    drawVanishingPoint(m_vx, pixelSize, alpha);
    drawVanishingPoint(m_vy, pixelSize, alpha);
    drawVanishingPoint(m_vz, pixelSize, alpha);
    if (getGridXY()) {
      drawGrid(m_vx, m_vy, m_y.position);
      drawGrid(m_vy, m_vx, m_x.position);
    }
    if (getGridYZ()) {
      drawGrid(m_vy, m_vz, m_z.position);
      drawGrid(m_vz, m_vy, m_y.position);
    }
    if (getGridZX()) {
      drawGrid(m_vz, m_vx, m_x.position);
      drawGrid(m_vx, m_vz, m_z.position);
    }
    if (getShowBox())
      drawBox(alpha);
  }

  void drawEdit(TToolViewer *viewer) const override {
    if (!getShowBox()) drawBox(getDrawingAlpha(false));
    TAssistant::drawEdit(viewer);
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantPerspective> assistantPerspective("assistantPerspective");
