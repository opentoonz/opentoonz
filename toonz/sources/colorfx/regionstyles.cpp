

#include "regionstyles.h"
#include "tgl.h"
#include "tcolorfunctions.h"
#include "trandom.h"
#include "colorfxutils.h"
#include "tflash.h"
#include "tregion.h"
#include "tcurves.h"
#include "tmathutil.h"
#include "tstencilcontrol.h"

//***************************************************************************
//    MovingModifier  implementation
//***************************************************************************

TOutlineStyle::RegionOutlineModifier *MovingModifier::clone() const {
  return new MovingModifier(*this);
}

//------------------------------------------------------------

void MovingModifier::modify(TRegionOutline &outline) const {
  TRegionOutline::Boundary::iterator regIt    = outline.m_exterior.begin();
  TRegionOutline::Boundary::iterator regItEnd = outline.m_exterior.end();

  TRegionOutline::PointVector::iterator pIt;
  TRegionOutline::PointVector::iterator pItEnd;

  for (; regIt != regItEnd; ++regIt) {
    pIt    = regIt->begin();
    pItEnd = regIt->end();

    for (; pIt != pItEnd; ++pIt) {
      pIt->x += m_move.x;
      pIt->y += m_move.y;
    }
  }

  regIt    = outline.m_interior.begin();
  regItEnd = outline.m_interior.end();
  for (; regIt != regItEnd; ++regIt) {
    pIt    = regIt->begin();
    pItEnd = regIt->end();

    for (; pIt != pItEnd; ++pIt) {
      pIt->x += m_move.x;
      pIt->y += m_move.y;
    }
  }
}

//***************************************************************************
//    MovingSolidColor  implementation
//***************************************************************************

MovingSolidColor::MovingSolidColor(const TPixel32 &color, const TPointD &move)
    : TSolidColorStyle(color) {
  m_regionOutlineModifier = new MovingModifier(move);
}

//------------------------------------------------------------

TColorStyle *MovingSolidColor::clone() const {
  return new MovingSolidColor(*this);
}

//------------------------------------------------------------

void MovingSolidColor::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  delete m_regionOutlineModifier;
  MovingModifier *mov = new MovingModifier(TPointD());
  mov->loadData(is);
  m_regionOutlineModifier = mov;
}

//------------------------------------------------------------

void MovingSolidColor::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);

  assert(m_regionOutlineModifier);
  ((MovingModifier *)m_regionOutlineModifier)->saveData(os);
}

//------------------------------------------------------------

int MovingSolidColor::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType MovingSolidColor::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString MovingSolidColor::getParamNames(int index) const {
  assert(0 <= index && index < 2);

  return index == 0
             ? QCoreApplication::translate("MovingSolidColor", "Horiz Offset")
             : QCoreApplication::translate("MovingSolidColor", "Vert Offset");
}

//-----------------------------------------------------------------------------

void MovingSolidColor::getParamRange(int index, double &min,
                                     double &max) const {
  assert(0 <= index && index < 2);
  min = -100.0;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double MovingSolidColor::getParamValue(TColorStyle::double_tag,
                                       int index) const {
  assert(0 <= index && index < 2);

  if (index)
    return ((MovingModifier *)m_regionOutlineModifier)->getMovePoint().y;
  else
    return ((MovingModifier *)m_regionOutlineModifier)->getMovePoint().x;
}

//-----------------------------------------------------------------------------

void MovingSolidColor::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);

  TPointD oldMove = ((MovingModifier *)m_regionOutlineModifier)->getMovePoint();

  if (!index) {
    if (oldMove.x != value) {
      delete m_regionOutlineModifier;
      oldMove.x               = value;
      m_regionOutlineModifier = new MovingModifier(oldMove);
      updateVersionNumber();
    }
  } else {
    if (oldMove.y != value) {
      delete m_regionOutlineModifier;
      oldMove.y               = value;
      m_regionOutlineModifier = new MovingModifier(oldMove);
      updateVersionNumber();
    }
  }
}

//------------------------------------------------------------

void MovingSolidColor::drawRegion(const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &boundary) const {
  TSolidColorStyle::drawRegion(cf, true, boundary);
}

//------------------------------------------------------------

void MovingSolidColor::drawRegion(TFlash &flash, const TRegion *r) const {
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();
  m_regionOutlineModifier->modify(rdf.m_ro);
  flash.setFillColor(getMainColor());
  rdf.drawRegionOutline(flash, false);
}

//***************************************************************************
//    ShadowStyle  implementation
//***************************************************************************

ShadowStyle::ShadowStyle(const TPixel32 &bgColor, const TPixel32 &shadowColor,
                         const TPointD &shadowDirection, double len,
                         double density)
    : TSolidColorStyle(bgColor)
    , m_shadowColor(shadowColor)
    , m_shadowDirection(normalize(shadowDirection))
    , m_len(len)
    , m_density(density) {}

//------------------------------------------------------------

TColorStyle *ShadowStyle::clone() const { return new ShadowStyle(*this); }

//------------------------------------------------------------

void ShadowStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_shadowDirection.x >> m_shadowDirection.y;
  is >> m_density;
  is >> m_shadowColor;
  is >> m_len;
}

//------------------------------------------------------------

void ShadowStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_shadowDirection.x << m_shadowDirection.y;
  os << m_density;
  os << m_shadowColor;
  os << m_len;
}

//------------------------------------------------------------

int ShadowStyle::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType ShadowStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString ShadowStyle::getParamNames(int index) const {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    return QCoreApplication::translate("ShadowStyle", "Angle");
  case 1:
    return QCoreApplication::translate("ShadowStyle", "Density");
  case 2:
    return QCoreApplication::translate("ShadowStyle", "Length");

  default:
    return QString();
  }

  assert(0);
  return QString();
}

//-----------------------------------------------------------------------------

void ShadowStyle::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    min = 0.0;
    max = 360.0;
    break;
  case 1:
    min = 0.0;
    max = 1.0;
    break;
  case 2:
    min = 0.0;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double ShadowStyle::getParamValue(TColorStyle::double_tag, int index) const {
  assert(0 <= index && index < 3);

  double degree;

  switch (index) {
  case 0:
    degree                              = asin(m_shadowDirection.y);
    if (m_shadowDirection.x < 0) degree = M_PI - degree;
    if (degree < 0) degree += M_2PI;
    return degree * M_180_PI;

  case 1:
    return m_density;

  case 2:
    return m_len;
  }

  assert(0);
  return 0.0;
}

//-----------------------------------------------------------------------------

void ShadowStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);
  double degree;

  switch (index) {
  case 0:
    degree              = value * M_PI_180;
    m_shadowDirection.x = cos(degree);
    m_shadowDirection.y = sin(degree);
    break;

  case 1:
    m_density = value;
    break;

  case 2:
    m_len = value;
    break;
  }
}

//------------------------------------------------------------

void ShadowStyle::drawPolyline(const TColorFunction *cf,
                               std::vector<T3DPointD> &polyline,
                               TPointD shadowDirection) const {
  int i;
  int stepNumber;
  double distance;

  TPointD v1, v2, diff, midPoint, ratio;
  double len;

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_shadowColor);
  else
    color = m_shadowColor;

  tglColor(color);

  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //// <-- tglEnableBlending();

  TRegionOutline::PointVector::iterator it;
  TRegionOutline::PointVector::iterator it_b = polyline.begin();
  TRegionOutline::PointVector::iterator it_e = polyline.end();

  v1.x = polyline.back().x;
  v1.y = polyline.back().y;

  for (it = it_b; it != it_e; ++it) {
    v2.x = it->x;
    v2.y = it->y;
    if (v1 == v2) continue;

    diff = normalize(rotate90(v2 - v1));
    len  = diff * shadowDirection;

    if (len > 0) {
      distance = tdistance(v1, v2) * m_density;

      ratio      = (v2 - v1) * (1.0 / distance);
      midPoint   = v1;
      stepNumber = (int)distance;

      for (i = 0; i < stepNumber; i++) {
        glBegin(GL_LINE_STRIP);

        tglColor(TPixel32(color.r, color.g, color.b, 0));
        tglVertex(midPoint);

        tglColor(color);
        tglVertex(midPoint + (shadowDirection * len * m_len * 0.5));

        tglColor(TPixel32(color.r, color.g, color.b, 0));
        tglVertex(midPoint + (shadowDirection * len * m_len));

        midPoint += ratio;

        glEnd();
      }
    }

    v1 = v2;
  }
  // tglColor(TPixel32::White);
}

//------------------------------------------------------------

void ShadowStyle::drawRegion(const TColorFunction *cf, const bool antiAliasing,
                             TRegionOutline &regionOutline) const {
  TStencilControl *stenc = TStencilControl::instance();

  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  ////stenc->beginMask();
  /*
glBegin(GL_QUADS);
glVertex2d (regionOutline.m_bbox.getP00().x, regionOutline.m_bbox.getP00().y);
glVertex2d (regionOutline.m_bbox.getP01().x, regionOutline.m_bbox.getP01().y);
glVertex2d (regionOutline.m_bbox.getP11().x, regionOutline.m_bbox.getP11().y);
glVertex2d (regionOutline.m_bbox.getP10().x, regionOutline.m_bbox.getP10().y);
glEnd();
*/
  ////stenc->endMask();
  ////stenc->enableMask(TStencilControl::SHOW_INSIDE);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();
    appStyle.drawRegion(0, false, regionOutline);
  } else {
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, regionOutline);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b =
      regionOutline.m_exterior.begin();
  TRegionOutline::Boundary::iterator regions_it_e =
      regionOutline.m_exterior.end();
  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it)
    drawPolyline(cf, *regions_it, m_shadowDirection);

  stenc->enableMask(TStencilControl::SHOW_OUTSIDE);

  regions_it_b = regionOutline.m_interior.begin();
  regions_it_e = regionOutline.m_interior.end();

  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it)
    drawPolyline(cf, *regions_it, -m_shadowDirection);

  // tglColor(TPixel32::White);

  stenc->disableMask();
}

//------------------------------------------------------------

/*
int ShadowStyle::drawPolyline(TFlash& flash, std::vector<T3DPointD> &polyline,
                                                           TPointD
shadowDirection, const bool isDraw) const
{
  int i;
  int stepNumber;
  double distance;

  TPointD v1,v2,diff,midPoint,ratio;
  double len;


  TRegionOutline::PointVector::iterator it;
  TRegionOutline::PointVector::iterator it_b = polyline.begin();
  TRegionOutline::PointVector::iterator it_e = polyline.end();


  std::vector<TSegment> segmentArray;

  v1.x = polyline.back().x;
  v1.y = polyline.back().y;

  for(it = it_b; it!= it_e; ++it)
  {
    v2.x = it->x;
    v2.y = it->y;
    if (v1==v2)
      continue;


    diff = normalize(rotate90(v2-v1));
    len=diff*shadowDirection;

    if(len>0)
    {
      distance = tdistance(v1,v2)*m_density;

      ratio= (v2-v1)*(1.0/distance);
      midPoint=v1;
      stepNumber= (int)distance;

      for(i=0; i<stepNumber;i++ )
      {
                  std::vector<TSegment> sa;

                  TPointD p0=midPoint;
                  TPointD p1=midPoint+(shadowDirection*len*m_len*0.5);
                  TPointD p2=midPoint+(shadowDirection*len*m_len);

                  segmentArray.push_back(TSegment(p1,p0));
                  segmentArray.push_back(TSegment(p1,p2));

          midPoint += ratio;
      }

    }

    v1=v2;
  }


  if ( isDraw && segmentArray.size()>0 ) {
        flash.setLineColor(m_shadowColor);
        flash.drawSegments(segmentArray, true);
  }

  if ( segmentArray.size()>0 )
         return 1;
  return 0;
}

void ShadowStyle::drawRegion( TFlash& flash, const TRegion* r) const
{
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b =
rdf.m_ro.m_exterior->begin();
  TRegionOutline::Boundary::iterator regions_it_e = rdf.m_ro.m_exterior->end();


// In the GL version the shadow lines are not croped into the filled region.
// This is the reason why I don't calculate the number of shadow lines.
//  int nbDraw=0;
//  for( regions_it = regions_it_b ; regions_it!= regions_it_e; ++regions_it)
//	  nbDraw+=drawPolyline(flash,*regions_it, m_shadowDirection,false);

//  regions_it_b = rdf.m_ro.m_interior->begin();
//  regions_it_e = rdf.m_ro.m_interior->end();
//  for( regions_it = regions_it_b ; regions_it!= regions_it_e; ++regions_it)
//     nbDraw+=drawPolyline(flash,*regions_it,-m_shadowDirection,false);


// Only the bbox rectangle is croped.
  flash.drawRegion(*r,1);
  flash.setFillColor(getMainColor());
  flash.drawRectangle(rdf.m_ro.m_bbox);

  regions_it_b = rdf.m_ro.m_exterior->begin();
  regions_it_e = rdf.m_ro.m_exterior->end();
  for( regions_it = regions_it_b ; regions_it!= regions_it_e; ++regions_it)
          drawPolyline(flash,*regions_it, m_shadowDirection);

  regions_it_b = rdf.m_ro.m_interior->begin();
  regions_it_e = rdf.m_ro.m_interior->end();
  for( regions_it = regions_it_b ; regions_it!= regions_it_e; ++regions_it)
     drawPolyline(flash,*regions_it,-m_shadowDirection);


}
*/

//------------------------------------------------------------

TPixel32 ShadowStyle::getColorParamValue(int index) const {
  return index == 0 ? m_shadowColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void ShadowStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_shadowColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void ShadowStyle::makeIcon(const TDimension &d) {
  double oldVal = getParamValue(TColorStyle::double_tag(), 1);
  setParamValue(1, oldVal * 0.25);
  TColorStyle::makeIcon(d);
  setParamValue(1, oldVal);
}

//***************************************************************************
//    ShadowStyle2  implementation
//***************************************************************************

ShadowStyle2::ShadowStyle2(const TPixel32 &bgColor, const TPixel32 &shadowColor,
                           const TPointD &shadowDirection, double shadowLength)
    : TSolidColorStyle(bgColor)
    , m_shadowColor(shadowColor)
    , m_shadowLength(shadowLength)
    , m_shadowDirection(normalize(shadowDirection)) {}

//------------------------------------------------------------

TColorStyle *ShadowStyle2::clone() const { return new ShadowStyle2(*this); }

//------------------------------------------------------------

void ShadowStyle2::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_shadowDirection.x >> m_shadowDirection.y;
  is >> m_shadowLength;
  is >> m_shadowColor;
}

//------------------------------------------------------------

void ShadowStyle2::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_shadowDirection.x << m_shadowDirection.y;
  os << m_shadowLength;
  os << m_shadowColor;
}

//------------------------------------------------------------

int ShadowStyle2::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType ShadowStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString ShadowStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 2);

  return index == 0 ? QCoreApplication::translate("ShadowStyle2", "Angle")
                    : QCoreApplication::translate("ShadowStyle2", "Size");
}

//-----------------------------------------------------------------------------

void ShadowStyle2::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = 0.0;
    max = 360.0;
  } else {
    min = 0.0;
    max = 500.0;
  }
}

//-----------------------------------------------------------------------------

double ShadowStyle2::getParamValue(TColorStyle::double_tag, int index) const {
  assert(0 <= index && index < 2);

  if (index == 1) return m_shadowLength;

  double degree                       = asin(m_shadowDirection.y);
  if (m_shadowDirection.x < 0) degree = M_PI - degree;

  if (degree < 0) degree += M_2PI;

  return degree * M_180_PI;
}

//-----------------------------------------------------------------------------

static int nbDiffVerts(const std::vector<TPointD> &pv) {
  std::vector<TPointD> lpv;
  if (pv.size() == 0) return 0;
  lpv.push_back(pv[0]);
  for (int i = 1; i < (int)pv.size(); i++) {
    bool isDiff = true;
    for (int j = 0; j < (int)lpv.size() && isDiff; j++)
      isDiff = lpv[j] == pv[i] ? false : isDiff;
    if (isDiff) {
      lpv.push_back(pv[i]);
    }
  }
  return lpv.size();
}

void ShadowStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);

  if (index == 1) {
    m_shadowLength = value;
  } else {
    double degree       = value * M_PI_180;
    m_shadowDirection.x = cos(degree);
    m_shadowDirection.y = sin(degree);
  }
}

//------------------------------------------------------------

namespace {

void drawShadowLine(TPixel32 shadowColor, TPixel32 color, TPointD v1,
                    TPointD v2, TPointD diff1, TPointD diff2) {
  v1    = v1 + diff1;
  v2    = v2 + diff2;
  diff1 = -diff1;
  diff2 = -diff2;

  double r1, r2;
  double t = 0.0;

  glBegin(GL_QUAD_STRIP);

  for (; t <= 1; t += 0.1) {
    r1 = t * t * t;
    r2 = 1 - r1;

    TPixel32 c((int)(color.r * r2 + shadowColor.r * r1),
               (int)(color.g * r2 + shadowColor.g * r1),
               (int)(color.b * r2 + shadowColor.b * r1),
               (int)(color.m * r2 + shadowColor.m * r1));
    tglColor(c);
    tglVertex(v1 + t * diff1);
    tglVertex(v2 + t * diff2);
  }

  glEnd();
}

int drawShadowLine(TFlash &flash, TPixel32 shadowColor, TPixel32 color,
                   TPointD v1, TPointD v2, TPointD diff1, TPointD diff2,
                   const bool isDraw = true) {
  int nbDraw = 0;

  v1    = v1 + diff1;
  v2    = v2 + diff2;
  diff1 = -diff1;
  diff2 = -diff2;

  TPointD vv1, vv2, ovv1, ovv2;
  TPixel32 oc;
  double r1, r2;
  double t     = 0.0;
  bool isFirst = true;
  flash.setThickness(0.0);
  SFlashUtils sfu;
  for (; t <= 1; t += 0.1) {
    if (isFirst) {
      r1 = t * t * t;
      r2 = 1 - r1;
      oc = TPixel32((int)(color.r * r2 + shadowColor.r * r1),
                    (int)(color.g * r2 + shadowColor.g * r1),
                    (int)(color.b * r2 + shadowColor.b * r1),
                    (int)(color.m * r2 + shadowColor.m * r1));
      ovv1    = v1 + t * diff1;
      ovv2    = v2 + t * diff2;
      isFirst = false;
    } else {
      r1 = t * t * t;
      r2 = 1 - r1;
      TPixel32 c((int)(color.r * r2 + shadowColor.r * r1),
                 (int)(color.g * r2 + shadowColor.g * r1),
                 (int)(color.b * r2 + shadowColor.b * r1),
                 (int)(color.m * r2 + shadowColor.m * r1));
      vv1 = (v1 + t * diff1);
      vv2 = (v2 + t * diff2);

      std::vector<TPointD> pv;
      pv.push_back(ovv1);
      pv.push_back(ovv2);
      pv.push_back(vv2);
      pv.push_back(vv1);

      int nbDV = nbDiffVerts(pv);
      if (nbDV >= 3 && nbDV <= 4) nbDraw++;

      if (isDraw) sfu.drawGradedPolyline(flash, pv, oc, c);

      oc   = c;
      ovv1 = vv1;
      ovv2 = vv2;
    }
  }
  return nbDraw;
}
}

//------------------------------------------------------------

void ShadowStyle2::drawPolyline(const TColorFunction *cf,
                                const std::vector<T3DPointD> &polyline,
                                TPointD shadowDirection) const {
  if (polyline.empty()) return;
  TPointD v0, v1, diff;
  double len1, len2;

  TPixel32 color, shadowColor;

  if (cf)
    color = (*(cf))(TSolidColorStyle::getMainColor());
  else
    color = TSolidColorStyle::getMainColor();

  if (cf)
    shadowColor = (*(cf))(m_shadowColor);
  else
    shadowColor = m_shadowColor;

  tglColor(shadowColor);

  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //// <-- tglEnableBlending();
  TRegionOutline::PointVector::const_iterator it;
  TRegionOutline::PointVector::const_iterator it_b = polyline.begin();
  TRegionOutline::PointVector::const_iterator it_e = polyline.end();

  int size = polyline.size();
  std::vector<double> lens(size);
  v0.x      = polyline.back().x;
  v0.y      = polyline.back().y;
  int count = 0;
  for (it = it_b; it != it_e; ++it) {
    v1.x = it->x;
    v1.y = it->y;
    if (v1 != v0) {
      diff               = normalize(rotate90(v1 - v0));
      len1               = diff * shadowDirection;
      if (len1 < 0) len1 = 0;
      lens[count++]      = len1;
    } else
      lens[count++] = 0;

    v0 = v1;
  }

  double firstVal = lens.front();
  for (count = 0; count != size - 1; count++) {
    lens[count] = (lens[count] + lens[count + 1]) * 0.5;
  }
  lens[size - 1] = (lens[size - 1] + firstVal) * 0.5;

  for (count = 0; count != size - 1; count++) {
    v0.x = polyline[count].x;
    v0.y = polyline[count].y;
    v1.x = polyline[count + 1].x;
    v1.y = polyline[count + 1].y;
    len1 = lens[count];
    len2 = lens[count + 1];

    if (v0 != v1 && len1 >= 0 && len2 >= 0 && (len1 + len2) > 0)
      drawShadowLine(shadowColor, color, v0, v1,
                     shadowDirection * len1 * m_shadowLength,
                     shadowDirection * len2 * m_shadowLength);
  }
  v0.x = polyline[count].x;
  v0.y = polyline[count].y;
  v1.x = polyline.front().x;
  v1.y = polyline.front().y;
  len1 = lens[count];
  len2 = lens[0];
  if (v0 != v1 && len1 >= 0 && len2 >= 0 && (len1 + len2) > 0)
    drawShadowLine(shadowColor, color, v0, v1,
                   shadowDirection * len1 * m_shadowLength,
                   shadowDirection * len2 * m_shadowLength);

  // tglColor(TPixel32::White);
}

//------------------------------------------------------------

TPixel32 ShadowStyle2::getColorParamValue(int index) const {
  return index == 0 ? m_shadowColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void ShadowStyle2::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_shadowColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void ShadowStyle2::drawRegion(const TColorFunction *cf, const bool antiAliasing,
                              TRegionOutline &boundary) const {
  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();

  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b = boundary.m_exterior.begin();
  TRegionOutline::Boundary::iterator regions_it_e = boundary.m_exterior.end();

  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it)
    drawPolyline(cf, *regions_it, m_shadowDirection);

  // tglColor(TPixel32::White);

  stenc->disableMask();
}

//------------------------------------------------------------

int ShadowStyle2::drawPolyline(TFlash &flash, std::vector<T3DPointD> &polyline,
                               TPointD shadowDirection,
                               const bool isDraw) const {
  int nbDraw = 0;

  TPointD v0, v1, diff;
  double len1, len2;
  TPixel32 color, shadowColor;
  color       = TSolidColorStyle::getMainColor();
  shadowColor = m_shadowColor;

  TRegionOutline::PointVector::iterator it;
  TRegionOutline::PointVector::iterator it_b = polyline.begin();
  TRegionOutline::PointVector::iterator it_e = polyline.end();

  int size = polyline.size();
  std::vector<double> lens(size);
  v0.x      = polyline.back().x;
  v0.y      = polyline.back().y;
  int count = 0;
  for (it = it_b; it != it_e; ++it) {
    v1.x = it->x;
    v1.y = it->y;
    if (v1 != v0) {
      diff               = normalize(rotate90(v1 - v0));
      len1               = diff * shadowDirection;
      if (len1 < 0) len1 = 0;
      lens[count++]      = len1;
    } else
      lens[count++] = 0;

    v0 = v1;
  }

  double firstVal = lens.front();
  for (count = 0; count != size - 1; count++) {
    lens[count] = (lens[count] + lens[count + 1]) * 0.5;
  }
  lens[size - 1] = (lens[size - 1] + firstVal) * 0.5;

  for (count = 0; count != size - 1; count++) {
    v0.x = polyline[count].x;
    v0.y = polyline[count].y;
    v1.x = polyline[count + 1].x;
    v1.y = polyline[count + 1].y;
    len1 = lens[count];
    len2 = lens[count + 1];

    if (v0 != v1 && len1 >= 0 && len2 >= 0 && (len1 + len2) > 0)
      nbDraw += drawShadowLine(flash, shadowColor, color, v0, v1,
                               shadowDirection * len1 * m_shadowLength,
                               shadowDirection * len2 * m_shadowLength, isDraw);
  }
  v0.x = polyline[count].x;
  v0.y = polyline[count].y;
  v1.x = polyline.front().x;
  v1.y = polyline.front().y;
  len1 = lens[count];
  len2 = lens[0];
  if (v0 != v1 && len1 >= 0 && len2 >= 0 && (len1 + len2) > 0)
    nbDraw += drawShadowLine(flash, shadowColor, color, v0, v1,
                             shadowDirection * len1 * m_shadowLength,
                             shadowDirection * len2 * m_shadowLength, isDraw);

  return nbDraw;
}

//------------------------------------------------------------

void ShadowStyle2::drawRegion(TFlash &flash, const TRegion *r) const {
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b = rdf.m_ro.m_exterior.begin();
  TRegionOutline::Boundary::iterator regions_it_e = rdf.m_ro.m_exterior.end();

  int nbDraw = 0;
  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it)
    nbDraw += drawPolyline(flash, *regions_it, m_shadowDirection, false);

  flash.drawRegion(*r, nbDraw + 1);
  flash.setFillColor(getMainColor());
  flash.drawRectangle(rdf.m_ro.m_bbox);

  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it)
    drawPolyline(flash, *regions_it, m_shadowDirection);
}

//***************************************************************************
//    RubberModifier  implementation
//***************************************************************************

TOutlineStyle::RegionOutlineModifier *RubberModifier::clone() const {
  return new RubberModifier(*this);
}

//------------------------------------------------------------

void RubberModifier::modify(TRegionOutline &outline) const {
  double deformSize = 40.0 + (100 - m_deform) * 0.60;

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b = outline.m_exterior.begin();
  TRegionOutline::Boundary::iterator regions_it_e = outline.m_exterior.end();

  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it) {
    RubberDeform rd(&regions_it[0]);
    rd.deform(deformSize);
  }

  regions_it_b = outline.m_interior.begin();
  regions_it_e = outline.m_interior.end();

  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it) {
    RubberDeform rd(&regions_it[0]);
    rd.deform(deformSize);
  }
}

//***************************************************************************
//    TRubberFillStyle  implementation
//***************************************************************************

TRubberFillStyle::TRubberFillStyle(const TPixel32 &color, double deform)
    : TSolidColorStyle(color) {
  m_regionOutlineModifier = new RubberModifier(deform);
}

//------------------------------------------------------------

TColorStyle *TRubberFillStyle::clone() const {
  return new TRubberFillStyle(*this);
}

//------------------------------------------------------------

void TRubberFillStyle::makeIcon(const TDimension &d) {
  // Saves the values of member variables and sets the right icon values

  RubberModifier *prm = (RubberModifier *)(m_regionOutlineModifier);
  double LDeform      = prm->getDeform();
  prm->setDeform(LDeform);

  TColorStyle::makeIcon(d);

  // Loads the original values
  prm->setDeform(LDeform);
}

//------------------------------------------------------------

int TRubberFillStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TRubberFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TRubberFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TRubberFillStyle", "Intensity");
}

//-----------------------------------------------------------------------------

void TRubberFillStyle::getParamRange(int index, double &min,
                                     double &max) const {
  assert(0 <= index && index < 1);
  min = 0.;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TRubberFillStyle::getParamValue(TColorStyle::double_tag,
                                       int index) const {
  assert(0 <= index && index < 1);
  return ((RubberModifier *)m_regionOutlineModifier)->getDeform();
}

//-----------------------------------------------------------------------------

void TRubberFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);

  double oldDeform = ((RubberModifier *)m_regionOutlineModifier)->getDeform();

  if (oldDeform != value) {
    delete m_regionOutlineModifier;
    m_regionOutlineModifier = new RubberModifier(value);
    updateVersionNumber();
  }
}

//------------------------------------------------------------

void TRubberFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  delete m_regionOutlineModifier;
  RubberModifier *rub = new RubberModifier(0.0);
  rub->loadData(is);
  m_regionOutlineModifier = rub;
}

//------------------------------------------------------------

void TRubberFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);

  assert(m_regionOutlineModifier);
  ((RubberModifier *)m_regionOutlineModifier)->saveData(os);
}

//------------------------------------------------------------

void TRubberFillStyle::drawRegion(const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &boundary) const {
  TSolidColorStyle::drawRegion(cf, true, boundary);
}

void TRubberFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();
  m_regionOutlineModifier->modify(rdf.m_ro);
  flash.setFillColor(getMainColor());
  rdf.drawRegionOutline(flash);
  //  If Valentina prefers the angled version use this
  //	rdf.drawRegionOutline(flash,false);
}

//***************************************************************************
//    TPointShadowFillStyle  implementation
//***************************************************************************

TPointShadowFillStyle::TPointShadowFillStyle(const TPixel32 &bgColor,
                                             const TPixel32 &shadowColor,
                                             const TPointD &shadowDirection,
                                             double density, double shadowSize,
                                             double pointSize)
    : TSolidColorStyle(bgColor)
    , m_shadowColor(shadowColor)
    , m_shadowDirection(normalize(shadowDirection))
    , m_shadowSize(shadowSize)
    , m_density(density)
    , m_pointSize(pointSize) {}

//-----------------------------------------------------------------------------

TColorStyle *TPointShadowFillStyle::clone() const {
  return new TPointShadowFillStyle(*this);
}

//-----------------------------------------------------------------------------

int TPointShadowFillStyle::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TPointShadowFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TPointShadowFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 4);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TPointShadowFillStyle", "Angle");
    break;
  case 1:
    value = QCoreApplication::translate("TPointShadowFillStyle", "Density");
    break;
  case 2:
    value = QCoreApplication::translate("TPointShadowFillStyle", "Size");
    break;
  case 3:
    value = QCoreApplication::translate("TPointShadowFillStyle", "Point Size");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TPointShadowFillStyle::getParamRange(int index, double &min,
                                          double &max) const {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    min = 0.0;
    max = 360.0;
    break;
  case 1:
    min = 0.0;
    max = 1.0;
    break;
  case 2:
    min = 0.0;
    max = 100.0;
    break;
  case 3:
    min = 0.01;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TPointShadowFillStyle::getParamValue(TColorStyle::double_tag,
                                            int index) const {
  assert(0 <= index && index < 4);

  double degree = 0.0;

  switch (index) {
  case 0:
    degree                              = asin(m_shadowDirection.y);
    if (m_shadowDirection.x < 0) degree = M_PI - degree;
    if (degree < 0) degree += M_2PI;
    return degree * M_180_PI;

  case 1:
    return m_density;

  case 2:
    return m_shadowSize;

  case 3:
    return m_pointSize;
  }

  // never
  return 0;
}

//-----------------------------------------------------------------------------

void TPointShadowFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);

  double degree = 0.0;

  switch (index) {
  case 0:
    degree              = value * M_PI_180;
    m_shadowDirection.x = cos(degree);
    m_shadowDirection.y = sin(degree);

    break;

  case 1:
    m_density = value;
    break;

  case 2:
    m_shadowSize = value;
    break;

  case 3:
    m_pointSize = value;
    break;
  }
}

//------------------------------------------------------------
void TPointShadowFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_shadowDirection.x >> m_shadowDirection.y;
  is >> m_density;
  is >> m_shadowSize;
  is >> m_pointSize;
  is >> m_shadowColor;
}

//------------------------------------------------------------

void TPointShadowFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_shadowDirection.x << m_shadowDirection.y;
  os << m_density;
  os << m_shadowSize;
  os << m_pointSize;
  os << m_shadowColor;
}

//------------------------------------------------------------

TPixel32 TPointShadowFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_shadowColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------
void TPointShadowFillStyle::setColorParamValue(int index,
                                               const TPixel32 &color) {
  if (index == 0)
    m_shadowColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

double TPointShadowFillStyle::triangleArea(const TPointD &a, const TPointD &b,
                                           const TPointD &c) const {
  double ab = tdistance(a, b);
  double ac = tdistance(a, c);
  double bc = tdistance(b, c);
  double s  = (ab + bc + ac) / 2.0;
  return sqrt(s * (s - ab) * (s - ac) * (s - bc));
}

//------------------------------------------------------------

void TPointShadowFillStyle::shadowOnEdge_parallel(const TPointD &p0,
                                                  const TPointD &p1,
                                                  const TPointD &p2,
                                                  TRandom &rnd) const {
  if (p0 == p1 || p1 == p2) return;

  TPointD diff = normalize(rotate90(p1 - p0));
  double len1  = diff * m_shadowDirection;

  diff        = normalize(rotate90(p2 - p1));
  double len2 = diff * m_shadowDirection;

  if (len1 >= 0 && len2 >= 0 && (len1 + len2) > 0) {
    TPointD la = p1 + m_shadowDirection * len1 * m_shadowSize;
    TPointD lb = p2 + m_shadowDirection * len2 * m_shadowSize;
    double t   = triangleArea(p1, p2, lb) + triangleArea(p2, lb, la);
    int nb     = (int)(m_density * t);
    for (int i = 0; i < nb; i++) {
      double q  = rnd.getUInt(1001) / 1000.0;
      double r  = rnd.getUInt(1001) / 1000.0;
      r         = r * r;
      TPointD u = p1 + (p2 - p1) * q;
      u         = u +
          r * (len1 * (1.0 - q) + len2 * q) * m_shadowDirection * m_shadowSize;
      tglColor(TPixel32(m_shadowColor.r, m_shadowColor.g, m_shadowColor.b,
                        (int)((1.0 - r) * (double)m_shadowColor.m)));
      tglVertex(u);
    }
  }
}

//------------------------------------------------------------

int TPointShadowFillStyle::shadowOnEdge_parallel(
    TFlash &flash, const TPointD &p0, const TPointD &p1, const TPointD &p2,
    TRandom &rnd, const double radius, const bool isDraw) const {
  int nbDraw = 0;

  if (p0 == p1 || p1 == p2) return 0;

  TPointD diff = normalize(rotate90(p1 - p0));
  double len1  = diff * m_shadowDirection;
  len1         = std::max(0.0, len1);

  diff        = normalize(rotate90(p2 - p1));
  double len2 = diff * m_shadowDirection;
  len2        = std::max(0.0, len2);

  if ((len1 + len2) > 0) {
    TPointD la = p1 + m_shadowDirection * len1 * m_shadowSize;
    TPointD lb = p2 + m_shadowDirection * len2 * m_shadowSize;
    double t   = triangleArea(p1, p2, lb) + triangleArea(p2, lb, la);
    int nb     = (int)(m_density * t);
    for (int i = 0; i < nb; i++) {
      double q  = rnd.getUInt(1001) / 1000.0;
      double r  = rnd.getUInt(1001) / 1000.0;
      r         = r * r;
      TPointD u = p1 + (p2 - p1) * q;
      u         = u +
          r * (len1 * (1.0 - q) + len2 * q) * m_shadowDirection * m_shadowSize;
      nbDraw++;
      if (isDraw) {
        flash.setFillColor(TPixel32(m_shadowColor.r, m_shadowColor.g,
                                    m_shadowColor.b, (int)((1.0 - r) * 255)));
        flash.drawEllipse(u, radius, radius);
        // flash.drawDot(u,radius);
      }
    }
  }
  return nbDraw;
}

//------------------------------------------------------------

void TPointShadowFillStyle::deleteSameVerts(
    TRegionOutline::Boundary::iterator &rit, std::vector<T3DPointD> &pv) const {
  pv.clear();
  if (rit->size() <= 0) return;
  TRegionOutline::PointVector::iterator it_beg = rit->begin();
  TRegionOutline::PointVector::iterator it_end = rit->end();
  TRegionOutline::PointVector::iterator it     = it_beg;
  pv.push_back(*it);
  it++;
  for (; it != it_end; it++) {
    if (tdistance(*it, pv.back()) > TConsts::epsilon) {
      pv.push_back(*it);
    }
  }

  if (pv.size() > 2) {
    if (tdistance(*(pv.begin()), pv.back()) <= TConsts::epsilon) pv.pop_back();
  }
}

//------------------------------------------------------------

void TPointShadowFillStyle::drawRegion(const TColorFunction *cf,
                                       const bool antiAliasing,
                                       TRegionOutline &boundary) const {
  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  GLfloat pointSizeSave;
  glGetFloatv(GL_POINT_SIZE, &pointSizeSave);
  GLfloat sizes[2];
  glGetFloatv(GL_POINT_SIZE_RANGE, sizes);
  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // glEnable(GL_POINT_SMOOTH);
  // glPointSize((float)(sizes[0]+(sizes[1]-sizes[0])*m_pointSize*0.01));
  tglEnablePointSmooth(
      (float)(sizes[0] + (sizes[1] - sizes[0]) * m_pointSize * 0.01));

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b = boundary.m_exterior.begin();
  TRegionOutline::Boundary::iterator regions_it_e = boundary.m_exterior.end();

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_shadowColor);
  else
    color = m_shadowColor;

  TRandom rnd;

  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it) {
    std::vector<T3DPointD> pv;
    deleteSameVerts(regions_it, pv);
    if (pv.size() < 3) continue;
    std::vector<T3DPointD>::iterator it_beg  = pv.begin();
    std::vector<T3DPointD>::iterator it_end  = pv.end();
    std::vector<T3DPointD>::iterator it_last = it_end - 1;
    std::vector<T3DPointD>::iterator it0, it1, it2;
    glBegin(GL_POINTS);
    for (it1 = it_beg; it1 != it_end; it1++) {
      it0 = it1 == it_beg ? it_last : it1 - 1;
      it2 = it1 == it_last ? it_beg : it1 + 1;

      shadowOnEdge_parallel(TPointD(it0->x, it0->y), TPointD(it1->x, it1->y),
                            TPointD(it2->x, it2->y), rnd);
    }
    glEnd();
  }

  glPointSize(pointSizeSave);
  stenc->disableMask();
}

//------------------------------------------------------------

void TPointShadowFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();

  TRegionOutline::Boundary::iterator regions_it;
  TRegionOutline::Boundary::iterator regions_it_b = rdf.m_ro.m_exterior.begin();
  TRegionOutline::Boundary::iterator regions_it_e = rdf.m_ro.m_exterior.end();

  TPixel32 color = m_shadowColor;
  TRandom rnd;
  rnd.reset();

  double sizes[2] = {0.15, 10.0};
  double radius   = (sizes[0] + (sizes[1] - sizes[0]) * m_pointSize * 0.01);

  int nbDraw = 0;
  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it) {
    std::vector<T3DPointD> pv;
    deleteSameVerts(regions_it, pv);
    if (pv.size() < 3) continue;
    std::vector<T3DPointD>::iterator it_beg  = pv.begin();
    std::vector<T3DPointD>::iterator it_end  = pv.end();
    std::vector<T3DPointD>::iterator it_last = it_end - 1;
    std::vector<T3DPointD>::iterator it0, it1, it2;
    for (it1 = it_beg; it1 != it_end; it1++) {
      it0 = it1 == it_beg ? it_last : it1 - 1;
      it2 = it1 == it_last ? it_beg : it1 + 1;
      nbDraw += shadowOnEdge_parallel(
          flash, TPointD(it0->x, it0->y), TPointD(it1->x, it1->y),
          TPointD(it2->x, it2->y), rnd, radius, false);
    }
  }

  rnd.reset();
  flash.drawRegion(*r, nbDraw + 1);  // +1 bbox
  flash.setFillColor(getMainColor());
  flash.drawRectangle(rdf.m_ro.m_bbox);

  flash.setThickness(0.0);
  for (regions_it = regions_it_b; regions_it != regions_it_e; ++regions_it) {
    std::vector<T3DPointD> pv;
    deleteSameVerts(regions_it, pv);
    if (pv.size() < 3) continue;
    std::vector<T3DPointD>::iterator it_beg  = pv.begin();
    std::vector<T3DPointD>::iterator it_end  = pv.end();
    std::vector<T3DPointD>::iterator it_last = it_end - 1;
    std::vector<T3DPointD>::iterator it0, it1, it2;
    for (it1 = it_beg; it1 != it_end; it1++) {
      it0 = it1 == it_beg ? it_last : it1 - 1;
      it2 = it1 == it_last ? it_beg : it1 + 1;
      shadowOnEdge_parallel(flash, TPointD(it0->x, it0->y),
                            TPointD(it1->x, it1->y), TPointD(it2->x, it2->y),
                            rnd, radius, true);
    }
  }
}

//***************************************************************************
//    TDottedFillStyle  implementation
//***************************************************************************

TDottedFillStyle::TDottedFillStyle(const TPixel32 &bgColor,
                                   const TPixel32 &pointColor,
                                   const double dotSize, const double dotDist,
                                   const bool isShifted)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_dotSize(dotSize)
    , m_dotDist(dotDist)
    , m_isShifted(isShifted) {}

//------------------------------------------------------------

TDottedFillStyle::TDottedFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32(0, 0, 200))
    , m_pointColor(color)
    , m_dotSize(3.0)
    , m_dotDist(15.0)
    , m_isShifted(true) {}

//------------------------------------------------------------

TColorStyle *TDottedFillStyle::clone() const {
  return new TDottedFillStyle(*this);
}

//------------------------------------------------------------

int TDottedFillStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TDottedFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TDottedFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 2);

  return index == 0
             ? QCoreApplication::translate("TDottedFillStyle", "Dot Size")
             : QCoreApplication::translate("TDottedFillStyle", "Dot Distance");
}

//-----------------------------------------------------------------------------

void TDottedFillStyle::getParamRange(int index, double &min,
                                     double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = 0.001;
    max = 30.0;
  } else {
    min = 2.0;
    max = 100.0;
  }
}

//-----------------------------------------------------------------------------

double TDottedFillStyle::getParamValue(TColorStyle::double_tag,
                                       int index) const {
  assert(0 <= index && index < 2);

  if (!index)
    return m_dotSize;
  else
    return m_dotDist;
}

//-----------------------------------------------------------------------------

void TDottedFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);

  if (!index) {
    m_dotSize = value;
  } else {
    m_dotDist = value;
  }
}

//------------------------------------------------------------

void TDottedFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_dotSize;
  is >> m_dotDist;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TDottedFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_dotSize;
  os << m_dotDist;
  os << m_pointColor;
}

//------------------------------------------------------------

TPixel32 TDottedFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void TDottedFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TDottedFillStyle::drawRegion(const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &boundary) const {
  double LDotDist = std::max(m_dotDist, 0.1);
  // double LDotSize=m_dotSize;
  bool LIsShifted          = m_isShifted;
  const bool isTransparent = m_pointColor.m < 255;

  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();

  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  if (isTransparent) {
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //// <-- tglEnableBlending();
  }

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_pointColor);
  else
    color = m_pointColor;

  tglColor(color);

  int i = 0;
  for (double y = boundary.m_bbox.y0; y <= boundary.m_bbox.y1;
       y += LDotDist, ++i) {
    double x = LIsShifted && (i % 2) == 1 ? boundary.m_bbox.x0 + LDotDist / 2.0
                                          : boundary.m_bbox.x0;
    for (; x <= boundary.m_bbox.x1; x += LDotDist)
      tglDrawDisk(TPointD(x, y), m_dotSize);
    //			tglDrawCircle(TPointD(x,y),m_dotSize);
  }

  if (isTransparent) {
    // tglColor(TPixel32::White);
    // glDisable(GL_BLEND);
  }

  stenc->disableMask();
}

//------------------------------------------------------------

int TDottedFillStyle::nbClip(const double LDotDist, const bool LIsShifted,
                             const TRectD &bbox) const {
  int nbClipLayers = 1;  // the bbox rectangle
  int i            = 0;
  for (double y = bbox.y0; y <= bbox.y1; y += LDotDist, ++i) {
    double x = LIsShifted && (i % 2) == 1 ? bbox.x0 + LDotDist / 2.0 : bbox.x0;
    for (; x <= bbox.x1; x += LDotDist) nbClipLayers++;
  }
  return nbClipLayers;
}

//------------------------------------------------------------

void TDottedFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  double LDotDist = std::max(m_dotDist, 0.1);
  double LDotSize = m_dotSize;
  bool LIsShifted = m_isShifted;
  TRectD bbox(r->getBBox());

  flash.setFillColor(TPixel::Black);
  flash.drawRegion(*r, true);
  int nClip = nbClip(LDotDist, LIsShifted, bbox);

  flash.drawRegion(*r, nClip);

  flash.setFillColor(getMainColor());
  flash.drawRectangle(bbox);
  flash.setFillColor(m_pointColor);

  int i = 0;
  for (double y = bbox.y0; y <= bbox.y1; y += LDotDist, ++i) {
    double x = LIsShifted && (i % 2) == 1 ? bbox.x0 + LDotDist / 2.0 : bbox.x0;
    for (; x <= bbox.x1; x += LDotDist)
      flash.drawEllipse(TPointD(x, y), LDotSize, LDotSize);
  }
}

//***************************************************************************
//    TCheckedFillStyle  implementation
//***************************************************************************

TCheckedFillStyle::TCheckedFillStyle(const TPixel32 &bgColor,
                                     const TPixel32 &pointColor,
                                     const double HDist, const double HAngle,
                                     const double VDist, const double VAngle,
                                     const double Thickness)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_HDist(HDist)
    , m_HAngle(HAngle)
    , m_VDist(VDist)
    , m_VAngle(VAngle)
    , m_Thickness(Thickness) {}

//------------------------------------------------------------

TCheckedFillStyle::TCheckedFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32::Transparent)
    , m_pointColor(color)
    , m_HDist(15.0)
    , m_HAngle(0.0)
    , m_VDist(15.0)
    , m_VAngle(0.0)
    , m_Thickness(6.0) {}

//------------------------------------------------------------

TColorStyle *TCheckedFillStyle::clone() const {
  return new TCheckedFillStyle(*this);
}

//------------------------------------------------------------

int TCheckedFillStyle::getParamCount() const { return 5; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TCheckedFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TCheckedFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 5);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TCheckedFillStyle", "Horiz Dist");
    break;
  case 1:
    value = QCoreApplication::translate("TCheckedFillStyle", "Horiz Angle");
    break;
  case 2:
    value = QCoreApplication::translate("TCheckedFillStyle", "Vert Dist");
    break;
  case 3:
    value = QCoreApplication::translate("TCheckedFillStyle", "Vert Angle");
    break;
  case 4:
    value = QCoreApplication::translate("TCheckedFillStyle", "Thickness");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TCheckedFillStyle::getParamRange(int index, double &min,
                                      double &max) const {
  assert(0 <= index && index < 5);
  switch (index) {
  case 0:
    min = 1.0;
    max = 100.0;
    break;
  case 1:
    min = -45.0;
    max = 45.0;
    break;
  case 2:
    min = 1.0;
    max = 100.0;
    break;
  case 3:
    min = -45.0;
    max = 45.0;
    break;
  case 4:
    min = 0.5;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TCheckedFillStyle::getParamValue(TColorStyle::double_tag,
                                        int index) const {
  assert(0 <= index && index < 5);

  switch (index) {
  case 0:
    return m_HDist;
  case 1:
    return m_HAngle;
  case 2:
    return m_VDist;
  case 3:
    return m_VAngle;
  case 4:
    return m_Thickness;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void TCheckedFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 5);

  switch (index) {
  case 0:
    m_HDist = value;
    break;
  case 1:
    m_HAngle = value;
    break;
  case 2:
    m_VDist = value;
    break;
  case 3:
    m_VAngle = value;
    break;
  case 4:
    m_Thickness = value;
    break;
  }
}

//------------------------------------------------------------

void TCheckedFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_HDist;
  is >> m_HAngle;
  is >> m_VDist;
  is >> m_VAngle;
  is >> m_Thickness;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TCheckedFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_HDist;
  os << m_HAngle;
  os << m_VDist;
  os << m_VAngle;
  os << m_Thickness;
  os << m_pointColor;
}

//------------------------------------------------------------

void TCheckedFillStyle::getHThickline(const TPointD &lc, const double lx,
                                      TPointD &p0, TPointD &p1, TPointD &p2,
                                      TPointD &p3) const {
  double l = m_Thickness / cos(degree2rad(m_HAngle));
  l *= 0.5;
  p0       = TPointD(lc.x, lc.y - l);
  p1       = TPointD(lc.x, lc.y + l);
  double y = lc.y + lx * tan(degree2rad(m_HAngle));
  p2       = TPointD(lc.x + lx, y + l);
  p3       = TPointD(lc.x + lx, y - l);
}

//------------------------------------------------------------

void TCheckedFillStyle::getVThickline(const TPointD &lc, const double ly,
                                      TPointD &p0, TPointD &p1, TPointD &p2,
                                      TPointD &p3) const {
  double l = m_Thickness / cos(degree2rad(-m_VAngle));
  l *= 0.5;
  p0       = TPointD(lc.x - l, lc.y);
  p1       = TPointD(lc.x + l, lc.y);
  double x = lc.x + ly * tan(degree2rad(-m_VAngle));
  p2       = TPointD(x + l, lc.y + ly);
  p3       = TPointD(x - l, lc.y + ly);
}

//------------------------------------------------------------

TPixel32 TCheckedFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void TCheckedFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TCheckedFillStyle::drawRegion(const TColorFunction *cf,
                                   const bool antiAliasing,
                                   TRegionOutline &boundary) const {
  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  const bool isTransparent = m_pointColor.m < 255;
  if (isTransparent) {
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //// <-- tglEnableBlending();
  }

  glBegin(GL_QUADS);
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_pointColor);
  else
    color = m_pointColor;

  tglColor(color);

  // Horizontal Lines
  double lx   = boundary.m_bbox.x1 - boundary.m_bbox.x0;
  double ly   = boundary.m_bbox.y1 - boundary.m_bbox.y0;
  double beg  = boundary.m_bbox.y0;
  double end  = boundary.m_bbox.y1;
  beg         = m_HAngle <= 0 ? beg : beg - lx * tan(degree2rad(m_HAngle));
  end         = m_HAngle >= 0 ? end : end - lx * tan(degree2rad(m_HAngle));
  double dist = m_HDist / cos(degree2rad(m_HAngle));
  for (double y = beg; y <= end; y += dist) {
    TPointD p0, p1, p2, p3;
    getHThickline(TPointD(boundary.m_bbox.x0, y), lx, p0, p1, p2, p3);
    tglVertex(p0);
    tglVertex(p1);
    tglVertex(p2);
    tglVertex(p3);
  }

  // Vertical Lines
  beg  = boundary.m_bbox.x0;
  end  = boundary.m_bbox.x1;
  beg  = (-m_VAngle) <= 0 ? beg : beg - ly * tan(degree2rad(-m_VAngle));
  end  = (-m_VAngle) >= 0 ? end : end - ly * tan(degree2rad(-m_VAngle));
  dist = m_VDist / cos(degree2rad(-m_VAngle));
  for (double x = beg; x <= end; x += dist) {
    TPointD p0, p1, p2, p3;
    getVThickline(TPointD(x, boundary.m_bbox.y0), ly, p0, p1, p2, p3);
    tglVertex(p0);
    tglVertex(p1);
    tglVertex(p2);
    tglVertex(p3);
  }

  glEnd();

  if (isTransparent) {
    // tglColor(TPixel32::White);
    // glDisable(GL_BLEND);
  }

  stenc->disableMask();
}

//------------------------------------------------------------

int TCheckedFillStyle::nbClip(const TRectD &bbox) const {
  int nbClip = 1;  // the bbox rectangle

  double lx   = bbox.x1 - bbox.x0;
  double ly   = bbox.y1 - bbox.y0;
  double beg  = bbox.y0;
  double end  = bbox.y1;
  beg         = m_HAngle <= 0 ? beg : beg - lx * tan(degree2rad(m_HAngle));
  end         = m_HAngle >= 0 ? end : end - lx * tan(degree2rad(m_HAngle));
  double dist = m_HDist / cos(degree2rad(m_HAngle));
  for (double y = beg; y <= end; y += dist) nbClip++;

  // Vertical lines
  beg  = bbox.x0;
  end  = bbox.x1;
  beg  = (-m_VAngle) <= 0 ? beg : beg - ly * tan(degree2rad(-m_VAngle));
  end  = (-m_VAngle) >= 0 ? end : end - ly * tan(degree2rad(-m_VAngle));
  dist = m_VDist / cos(degree2rad(-m_VAngle));
  for (double x = beg; x <= end; x += dist) nbClip++;
  return nbClip;
}

//------------------------------------------------------------

void TCheckedFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());

  //	flash.drawRegion(*r,true);
  flash.drawRegion(*r, nbClip(bbox));

  flash.setFillColor(getMainColor());
  flash.drawRectangle(bbox);

  flash.setFillColor(m_pointColor);
  // Horizontal Lines
  double lx   = bbox.x1 - bbox.x0;
  double ly   = bbox.y1 - bbox.y0;
  double beg  = bbox.y0;
  double end  = bbox.y1;
  beg         = m_HAngle <= 0 ? beg : beg - lx * tan(degree2rad(m_HAngle));
  end         = m_HAngle >= 0 ? end : end - lx * tan(degree2rad(m_HAngle));
  double dist = m_HDist / cos(degree2rad(m_HAngle));
  for (double y = beg; y <= end; y += dist) {
    TPointD p0, p1, p2, p3;
    getHThickline(TPointD(bbox.x0, y), lx, p0, p1, p2, p3);
    std::vector<TPointD> v;
    v.push_back(p0);
    v.push_back(p1);
    v.push_back(p2);
    v.push_back(p3);
    flash.drawPolyline(v);
  }

  // Vertical lines
  beg  = bbox.x0;
  end  = bbox.x1;
  beg  = (-m_VAngle) <= 0 ? beg : beg - ly * tan(degree2rad(-m_VAngle));
  end  = (-m_VAngle) >= 0 ? end : end - ly * tan(degree2rad(-m_VAngle));
  dist = m_VDist / cos(degree2rad(-m_VAngle));
  for (double x = beg; x <= end; x += dist) {
    TPointD p0, p1, p2, p3;
    getVThickline(TPointD(x, bbox.y0), ly, p0, p1, p2, p3);
    std::vector<TPointD> v;
    v.push_back(p0);
    v.push_back(p1);
    v.push_back(p2);
    v.push_back(p3);
    flash.drawPolyline(v);
  }
}

//***************************************************************************
//    ArtisticModifier  implementation
//***************************************************************************

void ArtisticModifier::modify(TRegionOutline &outline) const {
  TRegionOutline::Boundary::iterator regIt    = outline.m_exterior.begin();
  TRegionOutline::Boundary::iterator regItEnd = outline.m_exterior.end();

  TRegionOutline::PointVector::iterator pIt;
  TRegionOutline::PointVector::iterator pItEnd;
  TRandom rnd;
  double counter    = 0;
  double maxcounter = 0;
  for (; regIt != regItEnd; ++regIt) {
    pIt    = regIt->begin();
    pItEnd = regIt->end();

    for (; pIt != pItEnd; ++pIt) {
      if (counter >= maxcounter) {
        double tmp = (201 - m_period) * (rnd.getFloat() + 1);
        maxcounter = tmp * tmp;
        counter    = 0;
      }
      if (pIt != regIt->begin()) {
        double distance = (pIt->x - (pIt - 1)->x) * (pIt->x - (pIt - 1)->x) +
                          (pIt->y - (pIt - 1)->y) * (pIt->y - (pIt - 1)->y);
        counter += distance;
      }
      double wave          = 1;
      if (maxcounter) wave = sin(M_2PI * counter / maxcounter);

      pIt->x += m_move.x * wave;
      pIt->y += m_move.y * wave;
    }
  }

  regIt    = outline.m_interior.begin();
  regItEnd = outline.m_interior.end();
  for (; regIt != regItEnd; ++regIt) {
    pIt    = regIt->begin();
    pItEnd = regIt->end();

    for (; pIt != pItEnd; ++pIt) {
      pIt->x += (0.5 - rnd.getFloat()) * m_move.x;
      pIt->y += (0.5 - rnd.getFloat()) * m_move.y;
    }
  }
}

//------------------------------------------------------------

TOutlineStyle::RegionOutlineModifier *ArtisticModifier::clone() const {
  return new ArtisticModifier(*this);
}

//***************************************************************************
//    ArtisticSolidColor  implementation
//***************************************************************************

ArtisticSolidColor::ArtisticSolidColor(const TPixel32 &color,
                                       const TPointD &move, double period)
    : TSolidColorStyle(color) {
  m_regionOutlineModifier = new ArtisticModifier(move, period);
}

//------------------------------------------------------------

TColorStyle *ArtisticSolidColor::clone() const {
  return new ArtisticSolidColor(*this);
}

//------------------------------------------------------------

void ArtisticSolidColor::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  delete m_regionOutlineModifier;
  ArtisticModifier *mov = new ArtisticModifier(TPointD(), double());
  mov->loadData(is);
  m_regionOutlineModifier = mov;
}

//------------------------------------------------------------

void ArtisticSolidColor::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);

  assert(m_regionOutlineModifier);
  ((ArtisticModifier *)m_regionOutlineModifier)->saveData(os);
}

//------------------------------------------------------------

int ArtisticSolidColor::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType ArtisticSolidColor::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString ArtisticSolidColor::getParamNames(int index) const {
  assert(0 <= index && index < 3);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("ArtisticSolidColor", "Horiz Offset");
    break;
  case 1:
    value = QCoreApplication::translate("ArtisticSolidColor", "Vert Offset");
    break;
  case 2:
    value = QCoreApplication::translate("ArtisticSolidColor", "Noise");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void ArtisticSolidColor::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 3);
  switch (index) {
  case 0:
    min = 0.0;
    max = 20.0;
    break;
  case 1:
    min = 0.0;
    max = 20.0;
    break;
  case 2:
    min = 0.0;
    max = 200.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double ArtisticSolidColor::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 3);
  double value;
  switch (index) {
  case 0:
    value = ((ArtisticModifier *)m_regionOutlineModifier)->getMovePoint().x;
    break;
  case 1:
    value = ((ArtisticModifier *)m_regionOutlineModifier)->getMovePoint().y;
    break;
  case 2:
    value = ((ArtisticModifier *)m_regionOutlineModifier)->getPeriod();
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void ArtisticSolidColor::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);

  TPointD oldMove =
      ((ArtisticModifier *)m_regionOutlineModifier)->getMovePoint();
  double oldPeriod = ((ArtisticModifier *)m_regionOutlineModifier)->getPeriod();

  switch (index) {
  case 0:
    if (oldMove.x != value) {
      delete m_regionOutlineModifier;
      oldMove.x               = value;
      m_regionOutlineModifier = new ArtisticModifier(oldMove, oldPeriod);
      updateVersionNumber();
    }
    break;
  case 1:
    if (oldMove.y != value) {
      delete m_regionOutlineModifier;
      oldMove.y               = value;
      m_regionOutlineModifier = new ArtisticModifier(oldMove, oldPeriod);
      updateVersionNumber();
    }
    break;
  case 2:
    if (oldPeriod != value) {
      delete m_regionOutlineModifier;
      oldPeriod               = value;
      m_regionOutlineModifier = new ArtisticModifier(oldMove, oldPeriod);
      updateVersionNumber();
    }
    break;
  }
}

//------------------------------------------------------------

void ArtisticSolidColor::drawRegion(const TColorFunction *cf,
                                    const bool antiAliasing,
                                    TRegionOutline &boundary) const {
  TSolidColorStyle::drawRegion(cf, true, boundary);
}

//------------------------------------------------------------

void ArtisticSolidColor::drawRegion(TFlash &flash, const TRegion *r) const {
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();
  m_regionOutlineModifier->modify(rdf.m_ro);
  flash.setFillColor(getMainColor());
  rdf.drawRegionOutline(flash, false);
}

//***************************************************************************
//    TChalkFillStyle  implementation
//***************************************************************************

TChalkFillStyle::TChalkFillStyle(const TPixel32 &color0, const TPixel32 &color1,
                                 const double density, const double size)
    : TSolidColorStyle(color1)
    , m_color0(color0)
    , m_density(density)
    , m_size(size) {}

//------------------------------------------------------------

TChalkFillStyle::TChalkFillStyle(const TPixel32 &color0, const TPixel32 &color1)
    : TSolidColorStyle(color0)
    , m_color0(color1)
    , m_density(25.0)
    , m_size(1.0) {}

//------------------------------------------------------------

TColorStyle *TChalkFillStyle::clone() const {
  return new TChalkFillStyle(*this);
}

//------------------------------------------------------------

int TChalkFillStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TChalkFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TChalkFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 2);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TChalkFillStyle", "Density");
    break;
  case 1:
    value = QCoreApplication::translate("TChalkFillStyle", "Dot Size");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TChalkFillStyle::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 1133)
    throw TException("Chalk Fill style: unknown obsolete format");
  TSolidColorStyle::loadData(is);
  is >> m_color0 >> m_density >> m_size;
  m_density                      = m_density / 1000;
  if (m_density > 100) m_density = 100;
}

//-----------------------------------------------------------------------------

void TChalkFillStyle::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < 2);
  switch (index) {
  case 0:
    min = 0;
    max = 100.0;
    break;
  case 1:
    min = 0.0;
    max = 10.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TChalkFillStyle::getParamValue(TColorStyle::double_tag,
                                      int index) const {
  assert(0 <= index && index < 2);
  double value;
  switch (index) {
  case 0:
    value = m_density;
    break;
  case 1:
    value = m_size;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TChalkFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);

  switch (index) {
  case 0:
    m_density = value;
    break;
  case 1:
    m_size = value;
    break;
  }
}

//------------------------------------------------------------

void TChalkFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_color0;
  is >> m_density;
  is >> m_size;
}

//------------------------------------------------------------
void TChalkFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_color0;
  os << m_density;
  os << m_size;
}
//------------------------------------------------------------
TPixel32 TChalkFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_color0 : TSolidColorStyle::getMainColor();
}
//------------------------------------------------------------
void TChalkFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_color0 = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TChalkFillStyle::drawRegion(const TColorFunction *cf,
                                 const bool antiAliasing,
                                 TRegionOutline &boundary) const {
  // const bool isTransparent=m_color0.m<255;

  // TRegionOutline::Boundary& exter=*(boundary.m_exterior);
  // TRegionOutline::Boundary& inter=*(boundary.m_interior);

  TPixel32 color0;
  if (cf)
    color0 = (*(cf))(m_color0);
  else
    color0 = m_color0;

  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  int chalkId = glGenLists(1);
  glNewList(chalkId, GL_COMPILE);
  glBegin(GL_QUADS);
  glVertex2d(m_size, m_size);
  glVertex2d(-m_size, m_size);
  glVertex2d(-m_size, -m_size);
  glVertex2d(m_size, -m_size);
  glEnd();
  glEndList();
  TRandom rnd;

  double lx = boundary.m_bbox.x1 - boundary.m_bbox.x0;
  double ly = boundary.m_bbox.y1 - boundary.m_bbox.y0;

  // cioe' imposta una densita' tale, per cui in una regione che ha bbox 200x200
  // inserisce esattamente m_density punti
  int pointNumber = (int)(m_density * ((lx * ly) * 0.02));

  for (int i = 0; i < pointNumber; i++) {
    TPixel32 tmpcolor = color0;
    double shiftx     = boundary.m_bbox.x0 + rnd.getFloat() * lx;
    double shifty     = boundary.m_bbox.y0 + rnd.getFloat() * ly;
    tmpcolor.m        = (UCHAR)(tmpcolor.m * rnd.getFloat());
    tglColor(tmpcolor);
    glPushMatrix();
    glTranslated(shiftx, shifty, 0.0);
    glCallList(chalkId);
    glPopMatrix();
  }

  // glEnd(); e questo che era???

  glDeleteLists(chalkId, 1);

  stenc->disableMask();
}

//------------------------------------------------------------

void TChalkFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TPixel32 bgColor = TSolidColorStyle::getMainColor();

  double minDensity;
  double maxDensity;

  getParamRange(0, minDensity, maxDensity);

  double r1 = (m_density - minDensity) / (maxDensity - minDensity);
  double r2 = 1.0 - r1;

  TPixel32 color((int)(bgColor.r * r2 + m_color0.r * r1),
                 (int)(bgColor.g * r2 + m_color0.g * r1),
                 (int)(bgColor.b * r2 + m_color0.b * r1),
                 (int)(bgColor.m * r2 + m_color0.m * r1));

  flash.setFillColor(color);
  flash.drawRegion(*r);

  /*
  SFlashUtils rdf(r);
  rdf.computeRegionOutline();

TRandom rnd;

const bool isTransparent=m_color0.m<255;

  TRegionOutline::Boundary& exter=*(rdf.m_ro.m_exterior);
  TRegionOutline::Boundary& inter=*(rdf.m_ro.m_interior);

TPixel32 color0=m_color0;

  double lx=rdf.m_ro.m_bbox.x1-rdf.m_ro.m_bbox.x0;
  double ly=rdf.m_ro.m_bbox.y1-rdf.m_ro.m_bbox.y0;

// cioe' imposta una densita' tale, per cui in una regione che ha bbox 200x200
// inserisce esattamente m_density punti
int pointNumber= (int)(m_density*((lx*ly)*0.000025));

flash.drawRegion(*r,pointNumber+1); // -1 i don't know why

flash.setFillColor(getMainColor());
flash.drawRectangle(TRectD(TPointD(rdf.m_ro.m_bbox.x0,rdf.m_ro.m_bbox.y0),
                           TPointD(rdf.m_ro.m_bbox.x1,rdf.m_ro.m_bbox.y1)));

flash.setThickness(0.0);
for( int i=0;i< pointNumber; i++ ) {
 TPixel32 tmpcolor=color0;
 double shiftx=rdf.m_ro.m_bbox.x0+rnd.getFloat()*lx;
 double shifty=rdf.m_ro.m_bbox.y0+rnd.getFloat()*ly;
 tmpcolor.m=(UCHAR)(tmpcolor.m*rnd.getFloat());
 flash.setFillColor(tmpcolor);
 flash.pushMatrix();
     TTranslation tM(shiftx, shifty);
 flash.multMatrix(tM);
 flash.drawRectangle(TRectD(TPointD(-1,-1),TPointD(1,1)));
 flash.popMatrix();
  }
*/
}

//***************************************************************************
//    TChessFillStyle  implementation
//***************************************************************************

TChessFillStyle::TChessFillStyle(const TPixel32 &bgColor,
                                 const TPixel32 &pointColor, const double HDist,
                                 const double VDist, const double Angle)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_HDist(HDist)
    , m_VDist(VDist)
    , m_Angle(Angle) {}

//------------------------------------------------------------

TChessFillStyle::TChessFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32::White)
    , m_pointColor(color)
    , m_HDist(10.0)
    , m_VDist(10.0)
    , m_Angle(0.0) {}

//------------------------------------------------------------

TColorStyle *TChessFillStyle::clone() const {
  return new TChessFillStyle(*this);
}

//------------------------------------------------------------

int TChessFillStyle::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TChessFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TChessFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 3);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TChessFillStyle", "Horiz Size");
    break;
  case 1:
    value = QCoreApplication::translate("TChessFillStyle", "Vert Size");
    break;
  case 2:
    value = QCoreApplication::translate("TChessFillStyle", "Angle");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TChessFillStyle::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < 3);
  switch (index) {
  case 0:
    min = 1.0;
    max = 100.0;
    break;
  case 1:
    min = 1.0;
    max = 100.0;
    break;
  case 2:
    min = -45.0;
    max = 45.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TChessFillStyle::getParamValue(TColorStyle::double_tag,
                                      int index) const {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    return m_HDist;
  case 1:
    return m_VDist;
  case 2:
    return m_Angle;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void TChessFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    m_HDist = value;
    break;
  case 1:
    m_VDist = value;
    break;
  case 2:
    m_Angle = value;
    break;
  }
}

//------------------------------------------------------------

void TChessFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_HDist;
  is >> m_VDist;
  is >> m_Angle;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TChessFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_HDist;
  os << m_VDist;
  os << m_Angle;
  os << m_pointColor;
}

//------------------------------------------------------------

TPixel32 TChessFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void TChessFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TChessFillStyle::makeGrid(TRectD &bbox, TRotation &rotM,
                               std::vector<TPointD> &grid, int &nbClip) const {
  double lx = bbox.x1 - bbox.x0;
  double ly = bbox.y1 - bbox.y0;
  TPointD center =
      TPointD((bbox.x1 + bbox.x0) * 0.5, (bbox.y1 + bbox.y0) * 0.5);
  double l  = (lx + ly) / 1.3;
  double l2 = l / 2;

  bool isFirst = true;
  for (double y = -l2; y < (l2 + m_VDist); y += m_VDist) {
    double x = isFirst ? -l2 : -l2 + m_HDist;
    isFirst  = !isFirst;
    for (; x < (l2 + m_HDist); x += 2 * m_HDist) {
      grid.push_back(rotM * TPointD(x, y) + center);
      nbClip++;
    }
  }
}

//------------------------------------------------------------

void TChessFillStyle::drawRegion(const TColorFunction *cf,
                                 const bool antiAliasing,
                                 TRegionOutline &boundary) const {
  //	const bool isTransparent=m_pointColor.m<255;

  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();

  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_pointColor);
  else
    color = m_pointColor;

  tglColor(color);

  TPointD vert[4];
  vert[0].x = -0.5;
  vert[0].y = 0.5;
  vert[1].x = -0.5;
  vert[1].y = -0.5;
  vert[2].x = 0.5;
  vert[2].y = -0.5;
  vert[3].x = 0.5;
  vert[3].y = 0.5;

  TRotation rotM(m_Angle);
  TScale scaleM(m_HDist, m_VDist);
  for (int i = 0; i < 4; i++) vert[i] = rotM * scaleM * vert[i];

  int chessId = glGenLists(1);
  glNewList(chessId, GL_COMPILE);
  glBegin(GL_QUADS);
  glVertex2d(vert[0].x, vert[0].y);
  glVertex2d(vert[1].x, vert[1].y);
  glVertex2d(vert[2].x, vert[2].y);
  glVertex2d(vert[3].x, vert[3].y);
  glEnd();
  glEndList();

  int nbClip = 1;
  std::vector<TPointD> grid;
  makeGrid(boundary.m_bbox, rotM, grid, nbClip);

  std::vector<TPointD>::const_iterator it  = grid.begin();
  std::vector<TPointD>::const_iterator ite = grid.end();
  for (; it != ite; it++) {
    glPushMatrix();
    glTranslated(it->x, it->y, 0.0);
    glCallList(chessId);
    glPopMatrix();
  }

  stenc->disableMask();
  glDeleteLists(chessId, 1);
}

//------------------------------------------------------------

void TChessFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());

  TPointD vert[4];
  vert[0].x = -0.5;
  vert[0].y = 0.5;
  vert[1].x = -0.5;
  vert[1].y = -0.5;
  vert[2].x = 0.5;
  vert[2].y = -0.5;
  vert[3].x = 0.5;
  vert[3].y = 0.5;

  TRotation rotM(m_Angle);
  TScale scaleM(m_HDist, m_VDist);
  for (int i = 0; i < 4; i++) vert[i] = rotM * scaleM * vert[i];

  int nbClip = 1;  // just for the getMainColor() rectangle
  std::vector<TPointD> grid;
  makeGrid(bbox, rotM, grid, nbClip);

  //	flash.drawRegion(*r,true);
  flash.drawRegion(*r, nbClip);

  flash.setFillColor(getMainColor());
  flash.drawRectangle(bbox);

  flash.setFillColor(m_pointColor);

  std::vector<TPointD>::const_iterator it  = grid.begin();
  std::vector<TPointD>::const_iterator ite = grid.end();
  for (; it != ite; it++) {
    TTranslation trM(it->x, it->y);
    std::vector<TPointD> lvert;
    lvert.push_back(trM * vert[0]);
    lvert.push_back(trM * vert[1]);
    lvert.push_back(trM * vert[2]);
    lvert.push_back(trM * vert[3]);
    flash.drawPolyline(lvert);
  }
}

//***************************************************************************
//    TStripeFillStyle  implementation
//***************************************************************************

TStripeFillStyle::TStripeFillStyle(const TPixel32 &bgColor,
                                   const TPixel32 &pointColor,
                                   const double Dist, const double Angle,
                                   const double Thickness)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_Dist(Dist)
    , m_Angle(Angle)
    , m_Thickness(Thickness) {}

//------------------------------------------------------------

TStripeFillStyle::TStripeFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32::Transparent)
    , m_pointColor(color)
    , m_Dist(15.0)
    , m_Angle(0.0)
    , m_Thickness(6.0) {}

//------------------------------------------------------------

TColorStyle *TStripeFillStyle::clone() const {
  return new TStripeFillStyle(*this);
}

//------------------------------------------------------------

int TStripeFillStyle::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TStripeFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TStripeFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 3);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TStripeFillStyle", "Distance");
    break;
  case 1:
    value = QCoreApplication::translate("TStripeFillStyle", "Angle");
    break;
  case 2:
    value = QCoreApplication::translate("TStripeFillStyle", "Thickness");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TStripeFillStyle::getParamRange(int index, double &min,
                                     double &max) const {
  assert(0 <= index && index < 3);
  switch (index) {
  case 0:
    min = 1.0;
    max = 100.0;
    break;
  case 1:
    min = -90.0;
    max = 90.0;
    break;
  case 2:
    min = 0.5;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TStripeFillStyle::getParamValue(TColorStyle::double_tag,
                                       int index) const {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    return m_Dist;
  case 1:
    return m_Angle;
  case 2:
    return m_Thickness;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void TStripeFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    m_Dist = value;
    break;
  case 1:
    m_Angle = value;
    break;
  case 2:
    m_Thickness = value;
    break;
  }
}

//------------------------------------------------------------

void TStripeFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_Dist;
  is >> m_Angle;
  is >> m_Thickness;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TStripeFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_Dist;
  os << m_Angle;
  os << m_Thickness;
  os << m_pointColor;
}

void TStripeFillStyle::getThickline(const TPointD &lc, const double lx,
                                    TPointD &p0, TPointD &p1, TPointD &p2,
                                    TPointD &p3) const {
  double l = m_Thickness / cos(degree2rad(m_Angle));
  l *= 0.5;
  p0       = TPointD(lc.x, lc.y - l);
  p1       = TPointD(lc.x, lc.y + l);
  double y = lc.y + lx * tan(degree2rad(m_Angle));
  p2       = TPointD(lc.x + lx, y + l);
  p3       = TPointD(lc.x + lx, y - l);
}

//------------------------------------------------------------

TPixel32 TStripeFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------
void TStripeFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

inline void trim(TPointD &p0, TPointD &p1, double y0, double y1) {
  if (p0.y < y0) {
    // Trim the first extreme of the segment at y0
    double t = (y0 - p0.y) / (p1.y - p0.y);
    p0.x     = p0.x + t * (p1.x - p0.x);
    p0.y     = y0;
  } else if (p0.y > y1) {
    // The same, at y1
    double t = (y1 - p0.y) / (p1.y - p0.y);
    p0.x     = p0.x + t * (p1.x - p0.x);
    p0.y     = y1;
  }

  // Same for p1
  if (p1.y < y0) {
    double t = (y0 - p1.y) / (p0.y - p1.y);
    p1.x     = p1.x + t * (p0.x - p1.x);
    p1.y     = y0;
  } else if (p1.y > y1) {
    double t = (y1 - p1.y) / (p0.y - p1.y);
    p1.x     = p1.x + t * (p0.x - p1.x);
    p1.y     = y1;
  }
}

//------------------------------------------------------------

void TStripeFillStyle::drawRegion(const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &boundary) const {
  const bool isTransparent = m_pointColor.m < 255;

  TStencilControl *stenc = TStencilControl::instance();

  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();
    appStyle.drawRegion(0, false, boundary);
  } else {
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();

  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  if (isTransparent) {
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glEnable(GL_BLEND);
    //// <-- tglEnableBlending();
  }

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_pointColor);
  else
    color = m_pointColor;

  tglColor(color);

  // Horizontal Lines
  if (fabs(m_Angle) != 90) {
    double lx = boundary.m_bbox.x1 - boundary.m_bbox.x0;
    // double ly=boundary.m_bbox.y1-boundary.m_bbox.y0;
    double beg  = boundary.m_bbox.y0;
    double end  = boundary.m_bbox.y1;
    beg         = m_Angle <= 0 ? beg : beg - lx * tan(degree2rad(m_Angle));
    end         = m_Angle >= 0 ? end : end - lx * tan(degree2rad(m_Angle));
    double dist = m_Dist / cos(degree2rad(m_Angle));

    double y;

    TStencilControl *stenc2 = TStencilControl::instance();
    stenc2->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);

    glBegin(GL_QUADS);
    for (y = beg; y <= end; y += dist) {
      TPointD p0, p1, p2, p3;
      getThickline(TPointD(boundary.m_bbox.x0, y), lx, p0, p1, p2, p3);
      tglVertex(p0);
      tglVertex(p1);
      tglVertex(p2);
      tglVertex(p3);
    }
    glEnd();
    stenc2->endMask();

    stenc2->enableMask(TStencilControl::SHOW_OUTSIDE);

    if (m_Angle != 0)  // ANTIALIASING
    {
      // glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
      // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      // glEnable(GL_BLEND);
      // glEnable(GL_LINE_SMOOTH);

      tglEnableLineSmooth();

      // NOTE: Trimming the fat lines is necessary outside the (-60, 60) angles
      // interval
      // seemingly due to a bug in MAC-Leopard's openGL implementation...

      glBegin(GL_LINES);
      for (y = beg; y <= end; y += dist) {
        TPointD p0, p1, p2, p3;
        getThickline(TPointD(boundary.m_bbox.x0, y), lx, p0, p1, p2, p3);
        trim(p1, p2, boundary.m_bbox.y0, boundary.m_bbox.y1);
        tglVertex(p1);
        tglVertex(p2);

        trim(p0, p3, boundary.m_bbox.y0, boundary.m_bbox.y1);
        tglVertex(p0);
        tglVertex(p3);
      }
      glEnd();
    }
    stenc2->disableMask();

  } else {
    double beg = boundary.m_bbox.x0;
    double end = boundary.m_bbox.x1;
    double y0  = boundary.m_bbox.y0;
    double y1  = boundary.m_bbox.y1;

    glBegin(GL_QUADS);
    for (double x = beg; x <= end; x += m_Dist) {
      TPointD p0(x, y0);
      TPointD p1(x + m_Thickness, y0);
      TPointD p2(x, y1);
      TPointD p3(x + m_Thickness, y1);
      tglVertex(p0);
      tglVertex(p1);
      tglVertex(p3);
      tglVertex(p2);
    }
    glEnd();
  }

  // tglColor(TPixel32::White);

  stenc->disableMask();
}

//------------------------------------------------------------

int TStripeFillStyle::nbClip(const TRectD &bbox) const {
  int nbClip = 1;  // the bbox rectangle

  if (fabs(m_Angle) != 90) {
    double lx = bbox.x1 - bbox.x0;
    // double ly=bbox.y1-bbox.y0;
    double beg  = bbox.y0;
    double end  = bbox.y1;
    beg         = m_Angle <= 0 ? beg : beg - lx * tan(degree2rad(m_Angle));
    end         = m_Angle >= 0 ? end : end - lx * tan(degree2rad(m_Angle));
    double dist = m_Dist / cos(degree2rad(m_Angle));
    for (double y = beg; y <= end; y += dist) nbClip++;
  } else {
    double beg = bbox.x0;
    double end = bbox.x1;
    // double y0=bbox.y0;
    // double y1=bbox.y1;
    for (double x = beg; x <= end; x += m_Dist) nbClip++;
  }

  return nbClip;
}

//------------------------------------------------------------

void TStripeFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());

  //	flash.drawRegion(*r,true);
  flash.drawRegion(*r, nbClip(bbox));  // -1 i don't know why

  flash.setFillColor(getMainColor());
  flash.drawRectangle(bbox);

  flash.setFillColor(m_pointColor);
  // Horizontal Lines
  if (fabs(m_Angle) != 90) {
    double lx = bbox.x1 - bbox.x0;
    // double ly=bbox.y1-bbox.y0;
    double beg  = bbox.y0;
    double end  = bbox.y1;
    beg         = m_Angle <= 0 ? beg : beg - lx * tan(degree2rad(m_Angle));
    end         = m_Angle >= 0 ? end : end - lx * tan(degree2rad(m_Angle));
    double dist = m_Dist / cos(degree2rad(m_Angle));
    for (double y = beg; y <= end; y += dist) {
      TPointD p0, p1, p2, p3;
      getThickline(TPointD(bbox.x0, y), lx, p0, p1, p2, p3);
      std::vector<TPointD> v;
      v.push_back(p0);
      v.push_back(p1);
      v.push_back(p2);
      v.push_back(p3);
      flash.drawPolyline(v);
    }
  } else {
    double beg = bbox.x0;
    double end = bbox.x1;
    double y0  = bbox.y0;
    double y1  = bbox.y1;
    for (double x = beg; x <= end; x += m_Dist) {
      TPointD p0(x, y0);
      TPointD p1(x + m_Thickness, y0);
      TPointD p2(x, y1);
      TPointD p3(x + m_Thickness, y1);
      std::vector<TPointD> v;
      v.push_back(p0);
      v.push_back(p1);
      v.push_back(p3);
      v.push_back(p2);
      flash.drawPolyline(v);
    }
  }
}

//------------------------------------------------------------

void TStripeFillStyle::makeIcon(const TDimension &d) {
  // Saves the values of member variables and sets the right icon values
  double LDist      = m_Dist;
  double LAngle     = m_Angle;
  double LThickness = m_Thickness;

  m_Dist *= 1.33;
  m_Thickness *= 1.66;

  TColorStyle::makeIcon(d);

  m_Dist      = LDist;
  m_Angle     = LAngle;
  m_Thickness = LThickness;
}

//***************************************************************************
//    TLinGradFillStyle  implementation
//***************************************************************************

TLinGradFillStyle::TLinGradFillStyle(const TPixel32 &bgColor,
                                     const TPixel32 &pointColor,
                                     const double Angle, const double XPos,
                                     const double YPos, const double Size)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_Angle(Angle)
    , m_XPos(XPos)
    , m_YPos(YPos)
    , m_Size(Size) {}

//-----------------------------------------------------------------------------

TLinGradFillStyle::TLinGradFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32::White)
    , m_pointColor(color)
    , m_Angle(0.0)
    , m_XPos(0.0)
    , m_YPos(0.0)
    , m_Size(100.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TLinGradFillStyle::clone() const {
  return new TLinGradFillStyle(*this);
}

//------------------------------------------------------------

int TLinGradFillStyle::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TLinGradFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TLinGradFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 4);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TLinGradFillStyle", "Angle");
    break;
  case 1:
    value = QCoreApplication::translate("TLinGradFillStyle", "X Position");
    break;
  case 2:
    value = QCoreApplication::translate("TLinGradFillStyle", "Y Position");
    break;
  case 3:
    value = QCoreApplication::translate("TLinGradFillStyle", "Smoothness");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TLinGradFillStyle::getParamRange(int index, double &min,
                                      double &max) const {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    min = -180.0;
    max = 180.0;
    break;
  case 1:
    min = -100.0;
    max = 100.0;
    break;
  case 2:
    min = -100.0;
    max = 100.0;
    break;
  case 3:
    min = 1.0;
    max = 500.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TLinGradFillStyle::getParamValue(TColorStyle::double_tag,
                                        int index) const {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    return m_Angle;
  case 1:
    return m_XPos;
  case 2:
    return m_YPos;
  case 3:
    return m_Size;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void TLinGradFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    m_Angle = value;
    break;
  case 1:
    m_XPos = value;
    break;
  case 2:
    m_YPos = value;
    break;
  case 3:
    m_Size = value;
    break;
  }
}

//------------------------------------------------------------

void TLinGradFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_Angle;
  is >> m_XPos;
  is >> m_YPos;
  is >> m_Size;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TLinGradFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_Angle;
  os << m_XPos;
  os << m_YPos;
  os << m_Size;
  os << m_pointColor;
}

//------------------------------------------------------------

TPixel32 TLinGradFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void TLinGradFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TLinGradFillStyle::getRects(const TRectD &bbox, std::vector<TPointD> &r0,
                                 std::vector<TPointD> &r1,
                                 std::vector<TPointD> &r2) const {
  r0.clear();
  r1.clear();
  r2.clear();

  TPointD p0, p1, p2, p3;
  double lx = bbox.x1 - bbox.x0;
  double ly = bbox.y1 - bbox.y0;
  TPointD center((bbox.x1 + bbox.x0) / 2.0, (bbox.y1 + bbox.y0) / 2.0);
  center = center + TPointD(m_XPos * 0.01 * lx * 0.5, m_YPos * 0.01 * ly * 0.5);
  double l = tdistance(TPointD(bbox.x0, bbox.y0), TPointD(bbox.x1, bbox.y1));

  r0.push_back(TPointD(-m_Size - l, l));
  r0.push_back(TPointD(-m_Size - l, -l));
  r0.push_back(TPointD(-m_Size, -l));
  r0.push_back(TPointD(-m_Size, l));

  r1.push_back(TPointD(-m_Size, l));
  r1.push_back(TPointD(-m_Size, -l));
  r1.push_back(TPointD(m_Size, -l));
  r1.push_back(TPointD(m_Size, l));

  r2.push_back(TPointD(m_Size, l));
  r2.push_back(TPointD(m_Size, -l));
  r2.push_back(TPointD(m_Size + l, -l));
  r2.push_back(TPointD(m_Size + l, l));

  TRotation rotM(m_Angle);
  TTranslation traM(center);
  TAffine M(traM * rotM);

  for (int i = 0; i < 4; i++) {
    r0[i] = M * r0[i];
    r1[i] = M * r1[i];
    r2[i] = M * r2[i];
  }
}

//------------------------------------------------------------

void TLinGradFillStyle::drawRegion(const TColorFunction *cf,
                                   const bool antiAliasing,
                                   TRegionOutline &boundary) const {
  // only to create stencil mask
  TStencilControl *stenc = TStencilControl::instance();
  TSolidColorStyle appStyle(TPixel32::White);
  stenc->beginMask();  // does not draw on screen
  appStyle.drawRegion(0, false, boundary);
  stenc->endMask();

  // compute colors
  TPixel32 color1, color2;
  if (cf) {
    color1 = (*(cf))(TSolidColorStyle::getMainColor());
    color2 = (*(cf))(m_pointColor);
  } else {
    color1 = TSolidColorStyle::getMainColor();
    color2 = m_pointColor;
  }

  // compute points
  TRectD bbox(boundary.m_bbox);
  std::vector<TPointD> r0, r1, r2;
  getRects(bbox, r0, r1, r2);
  assert(r0.size() == 4);
  assert(r1.size() == 4);
  assert(r2.size() == 4);

  // draw

  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  glBegin(GL_QUADS);

  tglColor(color2);
  int i = 0;
  for (; i < 4; tglVertex(r0[i++]))
    ;
  tglVertex(r1[0]);
  tglVertex(r1[1]);

  tglColor(color1);
  tglVertex(r1[2]);
  tglVertex(r1[3]);
  for (i = 0; i < 4; tglVertex(r2[i++]))
    ;

  glEnd();

  stenc->disableMask();
}

//------------------------------------------------------------

// It is the new version, which uses XPos, YPos, Smooth parameters.
// There is a gap between the flat and graded regions. This is the reason,
// why the old version (without XPos, YPos, Smooth parameters) is used.
void TLinGradFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());
  std::vector<TPointD> rect;

  TPointD center((bbox.x1 + bbox.x0) / 2.0, (bbox.y1 + bbox.y0) / 2.0);
  center = center + TPointD(m_XPos * 0.01 * (bbox.x1 - bbox.x0) * 0.5,
                            m_YPos * 0.01 * (bbox.y1 - bbox.y0) * 0.5);
  double l = tdistance(TPointD(bbox.x0, bbox.y0), TPointD(bbox.x1, bbox.y1));

  TAffine M(TTranslation(center) * TRotation(m_Angle));

  rect.push_back(M * TPointD(-m_Size, l));
  rect.push_back(M * TPointD(-m_Size, -l));
  rect.push_back(M * TPointD(m_Size, -l));
  rect.push_back(M * TPointD(m_Size, l));

  flash.setThickness(0.0);

  SFlashUtils sfu;
  sfu.drawGradedRegion(flash, rect, m_pointColor, getMainColor(), *r);
}

/*
// --- Old version ---
void TLinGradFillStyle::drawRegion(TFlash& flash, const TRegion* r) const
{
  flash.drawRegion(*r,1);
  TRectD bbox(r->getBBox());
  TPointD p0,p1,p2,p3;
  p0=TPointD(bbox.x0,bbox.y0);
  p1=TPointD(bbox.x0,bbox.y1);
  p2=TPointD(bbox.x1,bbox.y0);
  p3=TPointD(bbox.x1,bbox.y1);
  std::vector<TPointD> pv;
  if ( fabs(m_Angle)!=90 ) {
                double tga=tan(degree2rad(fabs(m_Angle)));
                double lx=bbox.x1-bbox.x0;
                double ly=bbox.y1-bbox.y0;
                double ax=lx/(tga*tga+1);
                double bx=lx-ax;
                double mx=ax*tga;
                double rlylx=ly/lx;
                double ay=ax*rlylx;
                double by=bx*rlylx;
                double my=mx*rlylx;
                if ( m_Angle<=0.0) {
                        p0=p0+TPointD(-my,by);
                        p1=p1+TPointD(bx,mx);
                        p2=p2+TPointD(-bx,-mx);
                        p3=p3+TPointD(my,-by);
                } else {
                        p0=p0+TPointD(bx,-mx);
                        p1=p1+TPointD(-my,-by);
                        p2=p2+TPointD(my,by);
                        p3=p3+TPointD(-bx,mx);
                }
                pv.push_back(p0);
                pv.push_back(p1);
                pv.push_back(p3);
                pv.push_back(p2);
  } else {
                if ( m_Angle==-90 ) {
                        pv.push_back(p1);
                        pv.push_back(p3);
                        pv.push_back(p2);
                        pv.push_back(p0);
                } else {
                        pv.push_back(p0);
                        pv.push_back(p2);
                        pv.push_back(p3);
                        pv.push_back(p1);
                }
  }
  SFlashUtils sfu;
  sfu.drawGradedPolyline(flash,pv,m_pointColor,getMainColor());
}
*/

//***************************************************************************
//    TRadGradFillStyle  implementation
//***************************************************************************

TRadGradFillStyle::TRadGradFillStyle(const TPixel32 &bgColor,
                                     const TPixel32 &pointColor,
                                     const double XPos, const double YPos,
                                     const double Radius, const double Smooth)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_XPos(XPos)
    , m_YPos(YPos)
    , m_Radius(Radius)
    , m_Smooth(Smooth) {}

//-----------------------------------------------------------------------------

TRadGradFillStyle::TRadGradFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32::White)
    , m_pointColor(color)
    , m_XPos(0.0)
    , m_YPos(0.0)
    , m_Radius(50.0)
    , m_Smooth(50) {}

//-----------------------------------------------------------------------------

TColorStyle *TRadGradFillStyle::clone() const {
  return new TRadGradFillStyle(*this);
}

//------------------------------------------------------------

int TRadGradFillStyle::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TRadGradFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TRadGradFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 4);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TRadGradFillStyle", "X Position");
    break;
  case 1:
    value = QCoreApplication::translate("TRadGradFillStyle", "Y Position");
    break;
  case 2:
    value = QCoreApplication::translate("TRadGradFillStyle", "Radius");
    break;
  case 3:
    value = QCoreApplication::translate("TRadGradFillStyle", "Smoothness");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TRadGradFillStyle::getParamRange(int index, double &min,
                                      double &max) const {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    min = -100.0;
    max = 100.0;
    break;
  case 1:
    min = -100.0;
    max = 100.0;
    break;
  case 2:
    min = 0.01;
    max = 100.0;
    break;
  case 3:
    min = 0.01;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TRadGradFillStyle::getParamValue(TColorStyle::double_tag,
                                        int index) const {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    return m_XPos;
  case 1:
    return m_YPos;
  case 2:
    return m_Radius;
  case 3:
    return m_Smooth;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void TRadGradFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    m_XPos = value;
    break;
  case 1:
    m_YPos = value;
    break;
  case 2:
    m_Radius = value;
    break;
  case 3:
    m_Smooth = value;
    break;
  }
}

//------------------------------------------------------------

void TRadGradFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_XPos;
  is >> m_YPos;
  is >> m_Radius;
  is >> m_Smooth;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TRadGradFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_XPos;
  os << m_YPos;
  os << m_Radius;
  os << m_Smooth;
  os << m_pointColor;
}

//------------------------------------------------------------

TPixel32 TRadGradFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void TRadGradFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TRadGradFillStyle::drawRegion(const TColorFunction *cf,
                                   const bool antiAliasing,
                                   TRegionOutline &boundary) const {
  TStencilControl *stenc = TStencilControl::instance();
  // only to create stencil mask
  TSolidColorStyle appStyle(TPixel32::White);
  stenc->beginMask();  // does not draw on screen
  appStyle.drawRegion(0, false, boundary);
  stenc->endMask();

  TPixel32 backgroundColor, color;
  if (cf) {
    backgroundColor = (*(cf))(TSolidColorStyle::getMainColor());
    color           = (*(cf))(m_pointColor);
  } else {
    backgroundColor = TSolidColorStyle::getMainColor();
    color           = m_pointColor;
  }

  TRectD bbox(boundary.m_bbox);
  double lx = bbox.x1 - bbox.x0;
  double ly = bbox.y1 - bbox.y0;
  double r2 = std::max(lx, ly) * 5.0;
  double r1 = 0.5 * std::max(lx, ly) * m_Radius * 0.01;
  double r0 = r1 * (100.0 - m_Smooth) * 0.01;

  TPointD center((bbox.x1 + bbox.x0) / 2.0, (bbox.y1 + bbox.y0) / 2.0);
  center = center + TPointD(m_XPos * 0.01 * lx * 0.5, m_YPos * 0.01 * ly * 0.5);

  const double dAngle = 5.0;
  std::vector<TPointD> sincos;
  for (double angle = 0.0; angle <= 360.0; angle += dAngle)
    sincos.push_back(TPointD(sin(degree2rad(angle)), cos(degree2rad(angle))));

  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  glBegin(GL_TRIANGLE_FAN);
  tglColor(color);
  tglVertex(center);
  int i = 0;
  for (; i < (int)sincos.size(); i++)
    tglVertex(center + TPointD(r0 * sincos[i].x, r0 * sincos[i].y));
  glEnd();

  if (fabs(r0 - r1) > TConsts::epsilon) {
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < (int)sincos.size(); i++) {
      tglColor(color);
      tglVertex(center + TPointD(r0 * sincos[i].x, r0 * sincos[i].y));
      tglColor(backgroundColor);
      tglVertex(center + TPointD(r1 * sincos[i].x, r1 * sincos[i].y));
    }
    glEnd();
  }

  tglColor(backgroundColor);
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < (int)sincos.size(); i++) {
    tglVertex(center + TPointD(r1 * sincos[i].x, r1 * sincos[i].y));
    tglVertex(center + TPointD(r2 * sincos[i].x, r2 * sincos[i].y));
  }
  glEnd();

  stenc->disableMask();
}

//------------------------------------------------------------

void TRadGradFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());
  double lx = bbox.x1 - bbox.x0;
  double ly = bbox.y1 - bbox.y0;
  double r1 = 0.5 * std::max(lx, ly) * m_Radius * 0.01;
  if (m_Smooth < 50) r1 *= (0.3 * ((100 - m_Smooth) / 50.0) + 0.7);
  TPointD center((bbox.x1 + bbox.x0) / 2.0, (bbox.y1 + bbox.y0) / 2.0);
  center = center + TPointD(m_XPos * 0.01 * lx * 0.5, m_YPos * 0.01 * ly * 0.5);

  flash.setThickness(0.0);
  TPixel32 mc(getMainColor());
  flash.setGradientFill(false, m_pointColor, mc, m_Smooth);
  const double flashGrad = 16384.0;  // size of gradient square
  TTranslation tM(center.x, center.y);
  TScale sM(2.0 * r1 / (flashGrad), 2.0 * r1 / (flashGrad));
  flash.setFillStyleMatrix(tM * sM);
  flash.drawRegion(*r);
}

//***************************************************************************
//    TCircleStripeFillStyle  implementation
//***************************************************************************

TCircleStripeFillStyle::TCircleStripeFillStyle(
    const TPixel32 &bgColor, const TPixel32 &pointColor, const double XPos,
    const double YPos, const double Dist, const double Thickness)
    : TSolidColorStyle(bgColor)
    , m_pointColor(pointColor)
    , m_XPos(XPos)
    , m_YPos(YPos)
    , m_Dist(Dist)
    , m_Thickness(Thickness) {}

//------------------------------------------------------------

TCircleStripeFillStyle::TCircleStripeFillStyle(const TPixel32 &color)
    : TSolidColorStyle(TPixel32::Transparent)
    , m_pointColor(color)
    , m_XPos(0.0)
    , m_YPos(0.0)
    , m_Dist(15.0)
    , m_Thickness(3.0) {}

//------------------------------------------------------------

TColorStyle *TCircleStripeFillStyle::clone() const {
  return new TCircleStripeFillStyle(*this);
}

//------------------------------------------------------------

int TCircleStripeFillStyle::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TCircleStripeFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TCircleStripeFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 4);

  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TCircleStripeFillStyle", "X Position");
    break;
  case 1:
    value = QCoreApplication::translate("TCircleStripeFillStyle", "Y Position");
    break;
  case 2:
    value = QCoreApplication::translate("TCircleStripeFillStyle", "Distance");
    break;
  case 3:
    value = QCoreApplication::translate("TCircleStripeFillStyle", "Thickness");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TCircleStripeFillStyle::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    min = -200.0;
    max = 200.0;
    break;
  case 1:
    min = -200.0;
    max = 200.0;
    break;
  case 2:
    min = 0.5;
    max = 100.0;
    break;
  case 3:
    min = 0.5;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TCircleStripeFillStyle::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    return m_XPos;
  case 1:
    return m_YPos;
  case 2:
    return m_Dist;
  case 3:
    return m_Thickness;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void TCircleStripeFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    m_XPos = value;
    break;
  case 1:
    m_YPos = value;
    break;
  case 2:
    m_Dist = value;
    break;
  case 3:
    m_Thickness = value;
    break;
  }
}

//------------------------------------------------------------

void TCircleStripeFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_XPos;
  is >> m_YPos;
  is >> m_Dist;
  is >> m_Thickness;
  is >> m_pointColor;
}

//------------------------------------------------------------

void TCircleStripeFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_XPos;
  os << m_YPos;
  os << m_Dist;
  os << m_Thickness;
  os << m_pointColor;
}

//------------------------------------------------------------

TPixel32 TCircleStripeFillStyle::getColorParamValue(int index) const {
  return index == 0 ? m_pointColor : TSolidColorStyle::getMainColor();
}

//------------------------------------------------------------

void TCircleStripeFillStyle::setColorParamValue(int index,
                                                const TPixel32 &color) {
  if (index == 0)
    m_pointColor = color;
  else {
    TSolidColorStyle::setMainColor(color);
  }
}

//------------------------------------------------------------

void TCircleStripeFillStyle::getCircleStripeQuads(
    const TPointD &center, const double r1, const double r2,
    std::vector<TPointD> &pv) const {
  pv.clear();
  const double dAng = 10.0;
  for (double ang = 0.0; ang <= 360; ang += dAng) {
    pv.push_back(center +
                 TPointD(r1 * cos(degree2rad(ang)), r1 * sin(degree2rad(ang))));
    pv.push_back(center +
                 TPointD(r2 * cos(degree2rad(ang)), r2 * sin(degree2rad(ang))));
  }
}

//------------------------------------------------------------

void TCircleStripeFillStyle::drawCircleStripe(const TPointD &center,
                                              const double r1, const double r2,
                                              const TPixel32 &col) const {
  std::vector<TPointD> pv;
  getCircleStripeQuads(center, r1, r2, pv);

  TStencilControl *stencil = TStencilControl::instance();
  stencil->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);

  glBegin(GL_QUAD_STRIP);
  tglColor(col);
  int i = 0;
  for (; i < (int)pv.size(); i++) tglVertex(pv[i]);
  glEnd();

  stencil->endMask();
  stencil->enableMask(TStencilControl::SHOW_OUTSIDE);

  // glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // glEnable(GL_BLEND);
  // glEnable(GL_LINE_SMOOTH);
  tglEnableLineSmooth();

  // Just for the antialiasing
  glBegin(GL_LINE_STRIP);
  tglColor(col);
  for (i = 0; i < (int)pv.size(); i += 2) tglVertex(pv[i]);
  glEnd();

  glBegin(GL_LINE_STRIP);
  tglColor(col);
  for (i = 1; i < (int)pv.size(); i += 2) tglVertex(pv[i]);
  glEnd();

  stencil->disableMask();
}

//------------------------------------------------------------

void TCircleStripeFillStyle::drawRegion(const TColorFunction *cf,
                                        const bool antiAliasing,
                                        TRegionOutline &boundary) const {
  const bool isTransparent = m_pointColor.m < 255;

  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(m_pointColor);

  TPixel32 foregroundColor;
  if (cf)
    foregroundColor = (*(cf))(m_pointColor);
  else
    foregroundColor = m_pointColor;

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  if (isTransparent) {
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //// <-- tglEnableBlending();
  }

  TRectD bbox = boundary.m_bbox;
  double lx   = bbox.x1 - bbox.x0;
  double ly   = bbox.y1 - bbox.y0;
  TPointD center((bbox.x1 + bbox.x0) * 0.5, (bbox.y1 + bbox.y0) * 0.5);
  center.x = center.x + m_XPos * 0.01 * 0.5 * lx;
  center.y = center.y + m_YPos * 0.01 * 0.5 * ly;

  double maxDist = 0.0;
  maxDist = std::max(tdistance(center, TPointD(bbox.x0, bbox.y0)), maxDist);
  maxDist = std::max(tdistance(center, TPointD(bbox.x0, bbox.y1)), maxDist);
  maxDist = std::max(tdistance(center, TPointD(bbox.x1, bbox.y0)), maxDist);
  maxDist = std::max(tdistance(center, TPointD(bbox.x1, bbox.y1)), maxDist);

  double halfThick = m_Thickness * 0.5;
  for (double d = 0; d <= maxDist; d += m_Dist)
    drawCircleStripe(center, d - halfThick, d + halfThick, foregroundColor);

  if (isTransparent) {
    // tglColor(TPixel32::White);
    // glDisable(GL_BLEND);
  }

  stenc->disableMask();
}

//------------------------------------------------------------

void TCircleStripeFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());

  double lx = bbox.x1 - bbox.x0;
  double ly = bbox.y1 - bbox.y0;
  TPointD center((bbox.x1 + bbox.x0) * 0.5, (bbox.y1 + bbox.y0) * 0.5);
  center.x = center.x + m_XPos * 0.01 * 0.5 * lx;
  center.y = center.y + m_YPos * 0.01 * 0.5 * ly;

  double maxDist = 0.0;
  maxDist = std::max(tdistance(center, TPointD(bbox.x0, bbox.y0)), maxDist);
  maxDist = std::max(tdistance(center, TPointD(bbox.x0, bbox.y1)), maxDist);
  maxDist = std::max(tdistance(center, TPointD(bbox.x1, bbox.y0)), maxDist);
  maxDist = std::max(tdistance(center, TPointD(bbox.x1, bbox.y1)), maxDist);

  int nbClip = 2;
  double d   = m_Dist;
  for (; d <= maxDist; d += m_Dist) nbClip++;
  flash.setFillColor(TPixel::Black);
  flash.drawRegion(*r, nbClip);

  flash.setFillColor(getMainColor());
  flash.drawRectangle(bbox);

  flash.setFillColor(m_pointColor);
  flash.setLineColor(m_pointColor);
  flash.setThickness(0.0);
  d = m_Thickness / 2.0;
  flash.drawEllipse(center, d, d);

  flash.setFillColor(TPixel32(0, 0, 0, 0));
  flash.setLineColor(m_pointColor);
  flash.setThickness(m_Thickness / 2.0);
  for (d = m_Dist; d <= maxDist; d += m_Dist) flash.drawEllipse(center, d, d);
}

//***************************************************************************
//    TMosaicFillStyle  implementation
//***************************************************************************

TMosaicFillStyle::TMosaicFillStyle(const TPixel32 &bgColor,
                                   const TPixel32 pointColor[4],
                                   const double size, const double deform,
                                   const double minThickness,
                                   const double maxThickness)
    : TSolidColorStyle(bgColor)
    , m_size(size)
    , m_deform(deform)
    , m_minThickness(minThickness)
    , m_maxThickness(maxThickness) {
  for (int i = 0; i < 4; i++) m_pointColor[i] = pointColor[i];
}

//------------------------------------------------------------

TMosaicFillStyle::TMosaicFillStyle(const TPixel32 bgColor)
    : TSolidColorStyle(bgColor)
    , m_size(25.0)
    , m_deform(70.0)
    , m_minThickness(20)
    , m_maxThickness(40) {
  m_pointColor[0] = TPixel32::Blue;
  m_pointColor[1] = TPixel32::Green;
  m_pointColor[2] = TPixel32::Yellow;
  m_pointColor[3] = TPixel32::Cyan;
}

//------------------------------------------------------------

TColorStyle *TMosaicFillStyle::clone() const {
  return new TMosaicFillStyle(*this);
}

//------------------------------------------------------------

int TMosaicFillStyle::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TMosaicFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TMosaicFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 4);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TMosaicFillStyle", "Size");
    break;
  case 1:
    value = QCoreApplication::translate("TMosaicFillStyle", "Distortion");
    break;
  case 2:
    value = QCoreApplication::translate("TMosaicFillStyle", "Min Thick");
    break;
  case 3:
    value = QCoreApplication::translate("TMosaicFillStyle", "Max Thick");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TMosaicFillStyle::getParamRange(int index, double &min,
                                     double &max) const {
  assert(0 <= index && index < 4);
  min = (index == 0) ? 2 : 0.001;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TMosaicFillStyle::getParamValue(TColorStyle::double_tag,
                                       int index) const {
  assert(0 <= index && index < 4);

  double value;
  switch (index) {
  case 0:
    value = m_size;
    break;
  case 1:
    value = m_deform;
    break;
  case 2:
    value = m_minThickness;
    break;
  case 3:
    value = m_maxThickness;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TMosaicFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);

  switch (index) {
  case 0:
    m_size = value;
    break;
  case 1:
    m_deform = value;
    break;
  case 2:
    m_minThickness = value;
    break;
  case 3:
    m_maxThickness = value;
    break;
  }
}

//------------------------------------------------------------

void TMosaicFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_size;
  is >> m_deform;
  is >> m_minThickness;
  is >> m_maxThickness;
  is >> m_pointColor[0];
  is >> m_pointColor[1];
  is >> m_pointColor[2];
  is >> m_pointColor[3];
}

//------------------------------------------------------------

void TMosaicFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_size;
  os << m_deform;
  os << m_minThickness;
  os << m_maxThickness;
  os << m_pointColor[0];
  os << m_pointColor[1];
  os << m_pointColor[2];
  os << m_pointColor[3];
}

//------------------------------------------------------------

TPixel32 TMosaicFillStyle::getColorParamValue(int index) const {
  TPixel32 tmp;
  if (index == 0)
    tmp = TSolidColorStyle::getMainColor();
  else if (index >= 1 && index <= 4)
    tmp = m_pointColor[index - 1];
  else
    assert(!"bad color index");

  return tmp;
}

//------------------------------------------------------------
void TMosaicFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    TSolidColorStyle::setMainColor(color);
  else if (index >= 1 && index <= 4)
    m_pointColor[index - 1] = color;
  else
    assert(!"bad color index");
}

//------------------------------------------------------------

void TMosaicFillStyle::preaprePos(const TRectD &box, std::vector<TPointD> &v,
                                  int &lX, int &lY, TRandom &rand) const {
  double dist = 5.0 + (60.0 - 5.0) * tcrop(m_size, 0.0, 100.0) * 0.01;
  lY = lX   = 0;
  double ld = 0.4 * tcrop(m_deform, 0.0, 100.0) * 0.01;
  for (double y = box.y0 - dist; y <= (box.y1 + dist); y += dist, lY++) {
    lX = 0;
    for (double x = box.x0 - dist; x <= (box.x1 + dist); x += dist, lX++) {
      double dx = (rand.getInt(0, 2001) * 0.001 - 1.0) * ld * dist;
      double dy = (rand.getInt(0, 2001) * 0.001 - 1.0) * ld * dist;
      TPointD pos(x + dx, y + dy);
      v.push_back(pos);
    }
  }
}

//------------------------------------------------------------

bool TMosaicFillStyle::getQuad(const int ix, const int iy, const int lX,
                               const int lY, std::vector<TPointD> &v,
                               TPointD *pquad, TRandom &rand) const {
  if (ix < 0 || iy < 0 || ix >= (lX - 1) || iy >= (lY - 1)) return false;

  double dmin = tcrop(m_minThickness, 0.0, 100.0) * 0.01;
  double dmax = tcrop(m_maxThickness, 0.0, 100.0) * 0.01;

  TPointD &p1 = v[iy * lX + ix];
  TPointD &p2 = v[iy * lX + ix + 1];
  TPointD &p3 = v[(iy + 1) * lX + ix + 1];
  TPointD &p4 = v[(iy + 1) * lX + ix];

  double q1 = 0.5 * (dmin + (dmax - dmin) * rand.getInt(0, 101) * 0.01);
  double q2 = 0.5 * (dmin + (dmax - dmin) * rand.getInt(0, 101) * 0.01);
  double q3 = 0.5 * (dmin + (dmax - dmin) * rand.getInt(0, 101) * 0.01);
  double q4 = 0.5 * (dmin + (dmax - dmin) * rand.getInt(0, 101) * 0.01);

  pquad[0] = (1.0 - q1) * p1 + q1 * p3;
  pquad[1] = (1.0 - q2) * p2 + q2 * p4;
  pquad[2] = (1.0 - q3) * p3 + q3 * p1;
  pquad[3] = (1.0 - q4) * p4 + q4 * p2;

  return true;
}

//------------------------------------------------------------

void TMosaicFillStyle::drawRegion(const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &boundary) const {
  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //// <-- tglEnableBlending();

  TPixel32 color[4];
  for (int i = 0; i < 4; i++) {
    if (cf)
      color[i] = (*(cf))(m_pointColor[i]);
    else
      color[i] = m_pointColor[i];
  }
  TPixel32 currentColor;

  std::vector<TPointD> pos;
  int posLX, posLY;
  TRandom rand;
  TPointD quad[4];

  preaprePos(boundary.m_bbox, pos, posLX, posLY, rand);

  glBegin(GL_QUADS);

  /* ma serve ?
  tglColor(getMainColor());
  tglVertex(TPointD(boundary.m_bbox.x0,boundary.m_bbox.y0));
  tglVertex(TPointD(boundary.m_bbox.x0,boundary.m_bbox.y1));
  tglVertex(TPointD(boundary.m_bbox.x1,boundary.m_bbox.y1));
  tglVertex(TPointD(boundary.m_bbox.x1,boundary.m_bbox.y0));
*/

  for (int y = 0; y < (posLY - 1); y++)
    for (int x = 0; x < (posLX - 1); x++)
      if (getQuad(x, y, posLX, posLY, pos, quad, rand)) {
        currentColor = color[rand.getInt(0, 4)];
        if (currentColor.m != 0) {
          tglColor(currentColor);
          tglVertex(quad[0]);
          tglVertex(quad[1]);
          tglVertex(quad[2]);
          tglVertex(quad[3]);
        }
      }
  glEnd();

  // tglColor(TPixel32::White);

  stenc->disableMask();
}

//------------------------------------------------------------

void TMosaicFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());

  std::vector<TPointD> pos;
  int posLX, posLY;
  TRandom rand;
  TPointD quad[4];

  preaprePos(bbox, pos, posLX, posLY, rand);

  if (pos.size() <= 0) return;

  int nbClip = (posLX - 1) * (posLY - 1) + 1;
  flash.drawRegion(*r, nbClip);

  flash.setFillColor(TSolidColorStyle::getMainColor());
  flash.setThickness(0);
  flash.drawRectangle(bbox);
  for (int y = 0; y < (posLY - 1); y++)
    for (int x = 0; x < (posLX - 1); x++)
      if (getQuad(x, y, posLX, posLY, pos, quad, rand)) {
        std::vector<TPointD> lvert;
        lvert.push_back(quad[0]);
        lvert.push_back(quad[1]);
        lvert.push_back(quad[2]);
        lvert.push_back(quad[3]);
        flash.setFillColor(m_pointColor[rand.getInt(0, 4)]);
        flash.drawPolyline(lvert);
      }
}

//***************************************************************************
//    TPatchFillStyle  implementation
//***************************************************************************

TPatchFillStyle::TPatchFillStyle(const TPixel32 &bgColor,
                                 const TPixel32 pointColor[6],
                                 const double size, const double deform,
                                 const double thickness)
    : TSolidColorStyle(bgColor)
    , m_size(size)
    , m_deform(deform)
    , m_thickness(thickness) {
  for (int i = 0; i < 6; i++) m_pointColor[i] = pointColor[i];
}

//-----------------------------------------------------------------------------

TPatchFillStyle::TPatchFillStyle(const TPixel32 &bgColor)
    : TSolidColorStyle(bgColor), m_size(25.0), m_deform(50.0), m_thickness(30) {
  m_pointColor[0] = TPixel32::Red;
  m_pointColor[1] = TPixel32::Green;
  m_pointColor[2] = TPixel32::Yellow;
  m_pointColor[3] = TPixel32::Cyan;
  m_pointColor[4] = TPixel32::Magenta;
  m_pointColor[5] = TPixel32::White;
}

//-----------------------------------------------------------------------------

TColorStyle *TPatchFillStyle::clone() const {
  return new TPatchFillStyle(*this);
}

//------------------------------------------------------------

int TPatchFillStyle::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TPatchFillStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TPatchFillStyle::getParamNames(int index) const {
  assert(0 <= index && index < 3);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TPatchFillStyle", "Size");
    break;
  case 1:
    value = QCoreApplication::translate("TPatchFillStyle", "Distortion");
    break;
  case 2:
    value = QCoreApplication::translate("TPatchFillStyle", "Thickness");
    break;
  }

  return value;
}

//-----------------------------------------------------------------------------

void TPatchFillStyle::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < 3);
  min = (index == 0) ? 2 : 0.001;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TPatchFillStyle::getParamValue(TColorStyle::double_tag,
                                      int index) const {
  assert(0 <= index && index < 3);

  double value;
  switch (index) {
  case 0:
    value = m_size;
    break;
  case 1:
    value = m_deform;
    break;
  case 2:
    value = m_thickness;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TPatchFillStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);

  switch (index) {
  case 0:
    m_size = value;
    break;
  case 1:
    m_deform = value;
    break;
  case 2:
    m_thickness = value;
    break;
  }
}

//------------------------------------------------------------

void TPatchFillStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_size;
  is >> m_deform;
  is >> m_thickness;
  for (int i = 0; i < 6; i++) is >> m_pointColor[i];
}

//------------------------------------------------------------

void TPatchFillStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_size;
  os << m_deform;
  os << m_thickness;
  for (int i = 0; i < 6; i++) os << m_pointColor[i];
}

//------------------------------------------------------------
TPixel32 TPatchFillStyle::getColorParamValue(int index) const {
  TPixel32 tmp;
  if (index == 0)
    tmp = TSolidColorStyle::getMainColor();
  else if (index >= 1 && index <= 6)
    tmp = m_pointColor[index - 1];
  else
    assert(!"bad color index");

  return tmp;
}

//------------------------------------------------------------
void TPatchFillStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    TSolidColorStyle::setMainColor(color);
  else if (index >= 1 && index <= 6)
    m_pointColor[index - 1] = color;
  else
    assert(!"bad color index");
}

//------------------------------------------------------------

void TPatchFillStyle::preaprePos(const TRectD &box, std::vector<TPointD> &v,
                                 int &lX, int &lY, TRandom &rand) const {
  double q = tcrop(m_size, 0.0, 100.0) * 0.01;
  double r = 5.0 + (60.0 - 5.0) * q;
  double m = r * sqrt(3.0) / 2.0;
  lY       = 5 + (int)((box.y1 - box.y0) / (2 * m));
  int ix   = 0;
  for (double x = box.x0 - r; x <= (box.x1 + r); ix++) {
    int nb   = ix % 4;
    double y = (nb == 0 || nb == 1) ? box.y0 - 2 * m : box.y0 - m;
    for (int iy = 0; iy < lY; iy++, y += (2 * m)) v.push_back(TPointD(x, y));
    x           = (nb == 0 || nb == 2) ? x + r : x + r / 2.0;
  }
  lX = ix;

  double maxDeform = r * 0.6 * tcrop(m_deform, 0.0, 100.0) * 0.01;
  for (UINT i = 0; i < v.size(); i++) {
    v[i].x += (rand.getInt(0, 200) - 100) * 0.01 * maxDeform;
    v[i].y += (rand.getInt(0, 200) - 100) * 0.01 * maxDeform;
  }
}

//------------------------------------------------------------

bool TPatchFillStyle::getQuadLine(const TPointD &a, const TPointD &b,
                                  const double thickn, TPointD *quad) const {
  if (tdistance(a, b) < TConsts::epsilon) return false;

  TPointD ab(b - a);
  ab = normalize(ab);
  ab = rotate90(ab);
  ab = ab * thickn;

  quad[0] = a + ab;
  quad[1] = a - ab;
  quad[2] = b - ab;
  quad[3] = b + ab;

  return true;
}

//------------------------------------------------------------

void TPatchFillStyle::drawGLQuad(const TPointD *quad) const {
  glBegin(GL_QUADS);
  tglVertex(quad[0]);
  tglVertex(quad[1]);
  tglVertex(quad[2]);
  tglVertex(quad[3]);
  glEnd();
  double r = tdistance(quad[0], quad[1]) / 2.0;
  tglDrawDisk(quad[0] * 0.5 + quad[1] * 0.5, r);
  tglDrawDisk(quad[2] * 0.5 + quad[3] * 0.5, r);
}

//------------------------------------------------------------

void TPatchFillStyle::drawRegion(const TColorFunction *cf,
                                 const bool antiAliasing,
                                 TRegionOutline &boundary) const {
  TStencilControl *stenc   = TStencilControl::instance();
  TPixel32 backgroundColor = TSolidColorStyle::getMainColor();
  if (cf) backgroundColor  = (*(cf))(backgroundColor);

  if (backgroundColor.m == 0) {  // only to create stencil mask
    TSolidColorStyle appStyle(TPixel32::White);
    stenc->beginMask();  // does not draw on screen
    appStyle.drawRegion(0, false, boundary);
  } else {  // create stencil mask and draw on screen
    stenc->beginMask(TStencilControl::DRAW_ALSO_ON_SCREEN);
    TSolidColorStyle::drawRegion(cf, antiAliasing, boundary);
  }
  stenc->endMask();
  stenc->enableMask(TStencilControl::SHOW_INSIDE);

  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //// <-- tglEnableBlending();

  TPixel32 color[6];
  for (int i = 0; i < 6; i++)
    if (cf)
      color[i] = (*(cf))(m_pointColor[i]);
    else
      color[i] = m_pointColor[i];

  TPixel32 currentColor;

  std::vector<TPointD> pos;
  int posLX, posLY;
  TRandom rand;
  TPointD quad[4];

  preaprePos(boundary.m_bbox, pos, posLX, posLY, rand);
  glBegin(GL_TRIANGLES);
  int x = 2;
  for (; x < (posLX - 2); x += 2)
    for (int y = 1; y < posLY; y++) {
      TPointD q[6];
      if ((x % 4) == 2) {
        q[0] = pos[(x - 1) * posLY + y];
        q[1] = pos[(x)*posLY + y];
        q[2] = pos[(x + 1) * posLY + y];
        q[3] = pos[(x + 2) * posLY + y];
        q[4] = pos[(x + 1) * posLY + y - 1];
        q[5] = pos[(x)*posLY + y - 1];
      } else {
        q[0] = pos[(x - 1) * posLY + y - 1];
        q[1] = pos[(x)*posLY + y - 1];
        q[2] = pos[(x + 1) * posLY + y - 1];
        q[3] = pos[(x + 2) * posLY + y - 1];
        q[4] = pos[(x + 1) * posLY + y];
        q[5] = pos[(x)*posLY + y];
      }

      currentColor = color[rand.getInt(0, 6)];
      if (currentColor.m != 0) {
        tglColor(currentColor);

        tglVertex(q[0]);
        tglVertex(q[1]);
        tglVertex(q[2]);

        tglVertex(q[2]);
        tglVertex(q[3]);
        tglVertex(q[4]);

        tglVertex(q[4]);
        tglVertex(q[5]);
        tglVertex(q[0]);

        tglVertex(q[0]);
        tglVertex(q[2]);
        tglVertex(q[4]);
      }
    }
  glEnd();

  double thickn = tcrop(m_thickness, 0.0, 100.0) * 0.01 * 5.0;
  if (thickn > 0.001) tglColor(backgroundColor);
  for (x = 0; x < (posLX - 1); x++) {
    int nb = x % 4;
    for (int y = 0; y < posLY; y++) {
      if (getQuadLine(pos[x * posLY + y], pos[(x + 1) * posLY + y], thickn,
                      quad))
        drawGLQuad(quad);
      if (y > 0 && nb == 1)
        if (getQuadLine(pos[x * posLY + y], pos[(x + 1) * posLY + y - 1],
                        thickn, quad))
          drawGLQuad(quad);
      if (y < (posLY - 1) && nb == 3)
        if (getQuadLine(pos[x * posLY + y], pos[(x + 1) * posLY + y + 1],
                        thickn, quad))
          drawGLQuad(quad);
    }
  }

  // tglColor(TPixel32::White);

  stenc->disableMask();
}

//------------------------------------------------------------

int TPatchFillStyle::nbClip(const int lX, const int lY,
                            const std::vector<TPointD> &v) const {
  TPointD quad[4];
  double thickn = tcrop(m_thickness, 0.0, 100.0) * 0.01 * 5.0;
  int nbC       = 0;
  int x         = 2;
  for (; x < (lX - 2); x += 2)
    for (int y = 1; y < lY; y++) nbC += 1;
  if (thickn > 0.001)
    for (x = 0; x < (lX - 1); x++) {
      int nb = x % 4;
      for (int y = 0; y < lY; y++) {
        if (getQuadLine(v[x * lY + y], v[(x + 1) * lY + y], thickn, quad))
          nbC += 3;
        if (y > 0 && nb == 1)
          if (getQuadLine(v[x * lY + y], v[(x + 1) * lY + y - 1], thickn, quad))
            nbC += 3;
        if (y < (lY - 1) && nb == 3)
          if (getQuadLine(v[x * lY + y], v[(x + 1) * lY + y + 1], thickn, quad))
            nbC += 3;
      }
    }
  return nbC;
}

//------------------------------------------------------------

void TPatchFillStyle::drawFlashQuad(TFlash &flash, const TPointD *quad) const {
  std::vector<TPointD> lvert;
  lvert.push_back(quad[0]);
  lvert.push_back(quad[1]);
  lvert.push_back(quad[2]);
  lvert.push_back(quad[3]);
  flash.drawPolyline(lvert);

  double r = tdistance(quad[0], quad[1]) / 2.0;
  flash.drawEllipse(quad[0] * 0.5 + quad[1] * 0.5, r, r);
  flash.drawEllipse(quad[2] * 0.5 + quad[3] * 0.5, r, r);
}

//------------------------------------------------------------

void TPatchFillStyle::drawFlashTriangle(TFlash &flash, const TPointD &p1,
                                        const TPointD &p2,
                                        const TPointD &p3) const {
  std::vector<TPointD> lvert;
  lvert.push_back(p1);
  lvert.push_back(p2);
  lvert.push_back(p3);
  flash.drawPolyline(lvert);
}

//------------------------------------------------------------

void TPatchFillStyle::drawRegion(TFlash &flash, const TRegion *r) const {
  TRectD bbox(r->getBBox());

  std::vector<TPointD> pos;
  int posLX, posLY;
  TRandom rand;
  TPointD quad[4];

  preaprePos(bbox, pos, posLX, posLY, rand);
  if (pos.size() <= 0) return;
  flash.drawRegion(*r, nbClip(posLX, posLY, pos));

  flash.setThickness(0.0);
  int x;
  for (x = 2; x < (posLX - 2); x += 2)
    for (int y = 1; y < posLY; y++) {
      std::vector<TPointD> lvert;
      if ((x % 4) == 2) {
        lvert.push_back(pos[(x - 1) * posLY + y]);
        lvert.push_back(pos[(x)*posLY + y]);
        lvert.push_back(pos[(x + 1) * posLY + y]);
        lvert.push_back(pos[(x + 2) * posLY + y]);
        lvert.push_back(pos[(x + 1) * posLY + y - 1]);
        lvert.push_back(pos[(x)*posLY + y - 1]);
      } else {
        lvert.push_back(pos[(x - 1) * posLY + y - 1]);
        lvert.push_back(pos[(x)*posLY + y - 1]);
        lvert.push_back(pos[(x + 1) * posLY + y - 1]);
        lvert.push_back(pos[(x + 2) * posLY + y - 1]);
        lvert.push_back(pos[(x + 1) * posLY + y]);
        lvert.push_back(pos[(x)*posLY + y]);
      }
      flash.setFillColor(m_pointColor[rand.getInt(0, 6)]);
      flash.drawPolyline(lvert);
    }

  flash.setFillColor(TSolidColorStyle::getMainColor());
  flash.setThickness(0.0);
  double thickn = tcrop(m_thickness, 0.0, 100.0) * 0.01 * 5.0;
  if (thickn > 0.001)
    for (x = 0; x < (posLX - 1); x++) {
      int nb = x % 4;
      for (int y = 0; y < posLY; y++) {
        if (getQuadLine(pos[x * posLY + y], pos[(x + 1) * posLY + y], thickn,
                        quad))
          drawFlashQuad(flash, quad);
        if (y > 0 && nb == 1)
          if (getQuadLine(pos[x * posLY + y], pos[(x + 1) * posLY + y - 1],
                          thickn, quad))
            drawFlashQuad(flash, quad);
        if (y < (posLY - 1) && nb == 3)
          if (getQuadLine(pos[x * posLY + y], pos[(x + 1) * posLY + y + 1],
                          thickn, quad))
            drawFlashQuad(flash, quad);
      }
    }
}
