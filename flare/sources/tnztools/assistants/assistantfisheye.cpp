

// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>
#include <tools/assistants/guidelineellipse.h>

#include "assistantellipse.h"

// TnzCore includes
#include <tgl.h>

// std includes
#include <limits>


//*****************************************************************************************
//    TAssistantFisheye implementation
//*****************************************************************************************

class TAssistantFisheye final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantEllipse)
public:
  const TStringId m_idCircle;
  const TStringId m_idGrid;
  const TStringId m_idGridDepth;
  const TStringId m_idFlipGrids;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;
  TAssistantPoint &m_gridD0;
  TAssistantPoint &m_gridD1;

public:
  TAssistantFisheye(TMetaObject &object):
    TAssistant(object),
    m_idCircle("circle"),
    m_idGrid("grid"),
    m_idGridDepth("gridDepth"),
    m_idFlipGrids("flipGrids"),
    m_center( addPoint("center", TAssistantPoint::CircleCross) ),
    m_a( addPoint("a", TAssistantPoint::CircleFill, TPointD(200,   0)) ),
    m_b( addPoint("b", TAssistantPoint::Circle,     TPointD(  0, 200)) ),
    m_grid0(  addPoint("grid0",  TAssistantPoint::CircleDoubleDots, TPointD( -25,  25)) ),
    m_grid1(  addPoint("grid1",  TAssistantPoint::CircleDots,       TPointD(  25, -25)) ),
    m_gridD0( addPoint("gridD0", TAssistantPoint::CircleDoubleDots, TPointD( -70, -70)) ),
    m_gridD1( addPoint("gridD1", TAssistantPoint::CircleDots,       TPointD( -90, -90)) )
  {
    addProperty( new TBoolProperty(m_idCircle.str(), getCircle()) );
    addProperty( new TBoolProperty(m_idGrid.str(), getGrid()) );
    addProperty( new TBoolProperty(m_idGridDepth.str(), getGridDepth()) );
    addProperty( new TBoolProperty(m_idFlipGrids.str(), getFlipGrids()) );
  }

  static QString getLocalName()
    { return tr("Fish Eye"); }

  void updateTranslation() const override {
    TAssistant::updateTranslation();
    setTranslation(m_idCircle, tr("Circle"));
    setTranslation(m_idGrid, tr("Grid"));
    setTranslation(m_idGridDepth, tr("Depth Grid"));
    setTranslation(m_idFlipGrids, tr("Flip Grids"));
  }

  inline bool getCircle() const
    { return data()[m_idCircle].getBool(); }
  inline bool getGrid() const
    { return data()[m_idGrid].getBool(); }
  inline bool getGridDepth() const
    { return data()[m_idGridDepth].getBool(); }
  inline bool getFlipGrids() const
    { return data()[m_idFlipGrids].getBool(); }

  void onDataChanged(const TVariant &value) override {
    TAssistant::onDataChanged(value);
    m_grid0.visible  = m_grid1.visible  = getGrid();
    m_gridD0.visible = m_gridD1.visible = getGridDepth();
    bool prevB = m_b.visible;
    m_b.visible = !getCircle();
    if (prevB && !m_b.visible)
      fixBAndGrid(m_center.position, m_a.position, m_b.position);
  }

private:
  void fixBAndGrid(
    TPointD prevCenter,
    TPointD prevA,
    TPointD prevB )
  {
    const TPointD &center = m_center.position;
    TPointD da0 = prevA - prevCenter;
    TPointD da1 = m_a.position - center;
    double la0 = norm2(da0);
    double la1 = norm2(da1);
    if (!(la0 > TConsts::epsilon) || !(la1 > TConsts::epsilon))
      return;
    
    TPointD db = m_b.position - center;
    TPointD dp0 = TPointD(-da0.y, da0.x);
    TPointD dp1 = TPointD(-da1.y, da1.x);
    if (getCircle()) {
      m_b.position = center + (db*dp0 < 0 ? -dp1 : dp1);
    } else {
      m_b.position = db*dp0/la0*dp1 + center;
    }

    TPointD db0 = prevB - prevCenter;
    TPointD db1 = m_b.position - center;
    double lb0 = norm2(db0);
    double lb1 = norm2(db1);
    if (!(lb0 > TConsts::epsilon) || !(lb1 > TConsts::epsilon))
      return;

    TPointD dg0  = m_grid0.position  - center;
    TPointD dg1  = m_grid1.position  - center;
    TPointD dgD0 = m_gridD0.position - center;
    TPointD dgD1 = m_gridD1.position - center;
    m_grid0.position  =  dg0*da0/la0*da1 +  dg0*db0/lb0*db1 + center;
    m_grid1.position  =  dg1*da0/la0*da1 +  dg1*db0/lb0*db1 + center;
    m_gridD0.position = dgD0*da0/la0*da1 + dgD0*db0/lb0*db1 + center;
    m_gridD1.position = dgD1*da0/la0*da1 + dgD1*db0/lb0*db1 + center;
  }

  void fixGridD1() {
    TAffine em = calcEllipseMatrix();
    TAffine emi = em.inv();
    TPointD d0 = emi*m_gridD0.position;
    TPointD d1 = emi*m_gridD1.position;
    double l0 = norm2(d0);
    double l1 = norm2(d1);
    if ( l0 > TConsts::epsilon*TConsts::epsilon
      && l1 > TConsts::epsilon*TConsts::epsilon )
      m_gridD1.position = em*( d0*sqrt(l1/l0) );
  }

public:
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    TPointD prevCenter = m_center.position;
    TPointD prevA = m_a.position;
    TPointD prevB = m_b.position;
    point.position = position;
    if (&point == &m_center) {
      TPointD d = m_center.position - prevCenter;
      m_a.position += d;
      m_b.position += d;
      m_grid0.position += d;
      m_grid1.position += d;
      m_gridD0.position += d;
      m_gridD1.position += d;
    } else
    if (&point == &m_a || &point == &m_b) {
      fixBAndGrid(prevCenter, prevA, prevB);
    } else
    if (&point == &m_gridD0 || &point == &m_gridD1) {
      fixGridD1();
    }
  }

  TAffine calcEllipseMatrix() const {
    TPointD da = m_a.position - m_center.position;
    TPointD db = m_b.position - m_center.position;
    double r1 = norm(da);
    if (r1 <= TConsts::epsilon) return TAffine::zero();
    double r2 = fabs( (rotate90(da)*db)*(1.0/r1) );
    if (r2 <= TConsts::epsilon) return TAffine::zero();
    return TAffine::translation(m_center.position)
         * TAffine::rotation(atan(da))
         * TAffine::scale(r1, r2);
  }

  void getGuidelines(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    TGuidelineList &outGuidelines ) const override
  {
    TAffine matrix = calcEllipseMatrix();
    if (matrix.isZero()) return;

    matrix = toTool*matrix;
    TAffine matrixInv = matrix.inv();

    TPointD p = matrixInv*position;
    double l = norm(p);
    
    if (l > TConsts::epsilon) {
      // radius
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineLine(
          getEnabled(),
          getMagnetism(),
          color,
          matrix*TPointD(0, 0),
          matrix*(p/l) )));
    }
    
    if (!(l < 1.0 - TConsts::epsilon)) {
      // bound ellipse
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineEllipse(
          getEnabled(),
          getMagnetism(),
          color,
          matrix )));
    } else {
      // ellipse scaled by X
      double kx = fabs(p.x/sqrt(1.0 - p.y*p.y));
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineEllipse(
          getEnabled(),
          getMagnetism(),
          color,
          matrix * TAffine::scale(kx, 1.0) )));

      // ellipse scaled by Y
      double ky = fabs(p.y/sqrt(1.0 - p.x*p.x));
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineEllipse(
          getEnabled(),
          getMagnetism(),
          color,
          matrix * TAffine::scale(1.0, ky) )));
    }
  }

public:
  void draw(TToolViewer*, bool enabled) const override {
    TAffine ellipseMatrix = calcEllipseMatrix();
    if (ellipseMatrix.isZero()) return;
    
    TAffine ellipseMatrix2 = ellipseMatrix;
    std::swap(ellipseMatrix.a11, ellipseMatrix.a12);
    std::swap(ellipseMatrix.a21, ellipseMatrix.a22);

    // common data about viewport
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
    TAffine matrix = (projection*modelview).get2d();
    TAffine matrixInv = matrix.inv();

    double pixelSize = sqrt(tglGetPixelSize2());
    const double crossSize = 0.1;
    double alpha = getDrawingAlpha(enabled);
    double gridAlpha = getDrawingGridAlpha();
    
    drawSegment( ellipseMatrix*TPointD(-crossSize, 0.0),
                 ellipseMatrix*TPointD( crossSize, 0.0), pixelSize, alpha);
    drawSegment( ellipseMatrix*TPointD(0.0, -crossSize),
                 ellipseMatrix*TPointD(0.0,  crossSize), pixelSize, alpha);
    TAssistantEllipse::drawEllipse(ellipseMatrix, matrixInv, pixelSize, alpha);

    const TPointD *bound = getGrid() && getGridDepth() ? &m_gridD0.position : nullptr;
    const TPointD *boundA = nullptr;
    const TPointD *boundB = bound;
    if (getFlipGrids()) std::swap(boundA, boundB);
    
    if (getGrid()) {
      TAssistantEllipse::drawParallelGrid(
        ellipseMatrix, m_grid0.position, m_grid1.position,
        boundA, boundB, true, false, false, gridAlpha );
      TAssistantEllipse::drawParallelGrid(
        ellipseMatrix2, m_grid0.position, m_grid1.position,
        boundA, boundB, true, false, false, gridAlpha );
    }

    if (getGridDepth()) {
      TAssistantEllipse::drawParallelGrid(
        ellipseMatrix, m_gridD0.position, m_gridD1.position,
        boundB, boundA, false, true, false, gridAlpha );
      TAssistantEllipse::drawParallelGrid(
        ellipseMatrix2, m_gridD0.position, m_gridD1.position,
        boundB, boundA, false, true, false, gridAlpha );
    }
    
    if (bound) {
      TPointD b = ellipseMatrix.inv()*(*bound);
      double r = norm2(b);
      if (r < 1 - TConsts::epsilon) {
        double bx = b.x/sqrt(1 - b.y*b.y);
        double by = b.y/sqrt(1 - b.x*b.x);
        
        TAngleRangeSet ranges;
        ranges.add( TAngleRangeSet::fromDouble(-M_PI/2), TAngleRangeSet::fromDouble(M_PI/2) );
        TAssistantEllipse::drawEllipseRanges(
          ranges,
          ellipseMatrix*TAffine::scale(bx, 1),
          matrixInv,
          pixelSize,
          alpha );
        
        ranges.clear();
        ranges.add( TAngleRangeSet::fromDouble(0.0), TAngleRangeSet::fromDouble(M_PI) );
        TAssistantEllipse::drawEllipseRanges(
          ranges,
          ellipseMatrix*TAffine::scale(1, by),
          matrixInv,
          pixelSize,
          alpha );
        
        TPointD bb = bound == boundA ? TPointD(0, 0) : b/sqrt(r);
        drawSegment( ellipseMatrix*b, ellipseMatrix*bb, pixelSize, alpha );
      }
    }
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantFisheye> assistantFisheye("assistantFisheye");

