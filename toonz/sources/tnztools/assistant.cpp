
#include <tools/assistant.h>

#include <tools/tool.h>

#include <toonz/tapplication.h>
#include <toonz/txsheet.h>
#include <toonz/txsheethandle.h>
#include <toonz/txshlevelhandle.h>
#include <toonz/tframehandle.h>
#include <toonz/tobjecthandle.h>
#include <toonz/dpiscale.h>
#include <toonz/toonzscene.h>
#include <toonz/sceneproperties.h>

#include <tgl.h>
#include <tproperty.h>
#include <tpixelutils.h>

#include <limits>
#include <cassert>



#ifdef MACOSX
const double TAssistantBase::lineWidthScale = 1.5;
#else
const double TAssistantBase::lineWidthScale = 1.0;
#endif


unsigned int TAssistantBase::drawFlags = 0;
TPixelD TAssistantBase::colorBase  (0, 0, 0, 1);
TPixelD TAssistantBase::colorError (1, 0, 0, 1);
TPixelD TAssistantBase::colorEdit  (0, 0, 0, 1);
TPixelD TAssistantBase::colorSelect(0, 0, 1, 1);


//************************************************************************
//    TGuideline implementation
//************************************************************************

void
TGuideline::drawSegment(
  const TPointD &p0,
  const TPointD &p1,
  double pixelSize,
  bool active,
  bool enabled ) const
{
  double alpha = !this->enabled || !enabled ? 0.125
               : active                     ? 0.5
               :                              0.25;
  TPixelD cl = color;
  cl.m *= alpha;
  TPixelD clBack = TAssistantBase::makeContrastColor(cl);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();
  tglEnableLineSmooth(true, 1.0 * TAssistant::lineWidthScale);
  TPointD d = p1 - p0;
  double k = norm2(d);
  if (k > TConsts::epsilon*TConsts::epsilon) {
    k = 0.5*pixelSize*TAssistant::lineWidthScale/sqrt(k);
    d = TPointD(-k*d.y, k*d.x);
    tglColor(clBack);
    tglDrawSegment(p0 - d, p1 - d);
    tglColor(cl);
    tglDrawSegment(p0 + d, p1 + d);
  }
  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

double
TGuideline::calcTrackWeight(const TTrack &track, const TAffine &toScreen, bool &outLongEnough) const {
  outLongEnough = false;
  if (!enabled || track.size() < 2)
    return std::numeric_limits<double>::infinity();

  const double snapLenght = 20.0;
  const double snapScale = 1.0;
  const double maxLength = 2.0*snapLenght*snapScale;

  double sumWeight = 0.0;
  double sumLength = 0.0;
  double sumDeviation = 0.0;

  TPointD prev = toScreen*track[0].position;
  for(int i = 1; i < track.size(); ++i) {
    const TTrackPoint &tp = track[i];
    TTrackPoint mid = TTrack::interpolationLinear(track[i-1], track[i], 0.5);
    TPointD p = toScreen*tp.position;
    double length = tdistance(p, prev);
    sumLength += length;

    double midStepLength = sumLength - 0.5*length;
    if (midStepLength > TConsts::epsilon) {
      double weight = length*logNormalDistribuitionUnscaled(midStepLength, snapLenght, snapScale);
      sumWeight += weight;

      double deviation = tdistance(
        toScreen*mid.position,
        toScreen*nearestPoint(mid.position) );
      sumDeviation += weight*deviation;
    }
    prev = p;

    if (sumLength >= maxLength)
      { outLongEnough = i < track.fixedSize(); break; }
  }
  return sumWeight > TConsts::epsilon
       ? sumDeviation/sumWeight
       : std::numeric_limits<double>::infinity();
}

//---------------------------------------------------------------------------------------------------

TGuidelineP
TGuideline::findBest(const TGuidelineList &guidelines, const TTrack &track, const TAffine &toScreen, bool &outLongEnough) {
  outLongEnough = true;
  double bestWeight = 0.0;
  TGuidelineP best;
  for(TGuidelineList::const_iterator i = guidelines.begin(); i != guidelines.end(); ++i) {
    double weight = (*i)->calcTrackWeight(track, toScreen, outLongEnough);
    if (!best || weight < bestWeight)
      { bestWeight = weight; best = *i; }
  }
  return best;
}


//************************************************************************
//    TAssistantPoint implementation
//************************************************************************

TAssistantPoint::TAssistantPoint(const TStringId &name, const TPointD &defPosition):
  name(name),
  defPosition(defPosition),
  type(Circle),
  position(defPosition),
  radius(10.0),
  visible(true),
  selected() { }


//************************************************************************
//    TAssistantType implementation
//************************************************************************

TMetaObjectHandler*
TAssistantType::createHandler(TMetaObject &obj) const
  { return createAssistant(obj); }


//************************************************************************
//    TAssistantBase implementation
//************************************************************************

TAssistantBase::TAssistantBase(TMetaObject &object):
  TMetaObjectHandler(object),
  m_idEnabled("enabled"),
  m_idPoints("points"),
  m_idX("x"),
  m_idY("y"),
  m_idZ("z"),
  m_idW("w"),
  m_basePoint()
{
  addProperty( new TBoolProperty(m_idEnabled.str(), getEnabled()) );
}

//---------------------------------------------------------------------------------------------------

TAssistantPoint&
TAssistantBase::addPoint(
  const TStringId &name,
  TAssistantPoint::Type type,
  const TPointD &defPosition,
  bool visible,
  double radius )
{
  assert(!m_points.count(name));
  TAssistantPoint &p = m_points.insert(
    TAssistantPointMap::value_type(name, TAssistantPoint(name, defPosition)) ).first->second;
  m_pointsOrder.push_back(&p);
  p.type     = type;
  p.radius   = radius;
  p.visible  = visible;
  if (!m_basePoint) m_basePoint = &p;
  return p;
}

//---------------------------------------------------------------------------------------------------

TAssistantPoint&
TAssistantBase::addPoint(
  const TStringId &name,
  TAssistantPoint::Type type,
  const TPointD &defPosition,
  bool visible )
    { return addPoint(name, type, defPosition, visible, 10.0); }

//---------------------------------------------------------------------------------------------------

const TAssistantPoint&
TAssistantBase::getBasePoint() const
  { assert(m_basePoint); return *m_basePoint; }

//---------------------------------------------------------------------------------------------------

TIntProperty*
TAssistantBase::createSpinProperty(const TStringId &id, int def, int min, int max, bool hasMax) {
  if (!hasMax && max < def) max = def;
  assert(min <= def && def <= max);
  TIntProperty *property = new TIntProperty(id.str(), min, max, def, hasMax);
  property->setSpinner();
  return property;
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::addProperty(TProperty *p)
  { m_properties.add(p); }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::setTranslation(const TStringId &name, const QString &localName) const
  { m_properties.getProperty(name)->setQStringName( localName ); }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::updateTranslation() const
  { setTranslation(m_idEnabled, tr("Enabled")); }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onSetDefaults() {
  setEnabled(true);
  for(TAssistantPointMap::iterator i = m_points.begin(); i != m_points.end(); ++i)
    i->second.position = i->second.defPosition;
  fixPoints();
  fixData();
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::fixPoints()
  { onFixPoints(); }

//---------------------------------------------------------------------------------------------------

bool
TAssistantBase::move(const TPointD &position) {
  TPointD d = position - getBasePoint().position;
  if (d != TPointD()) {
    for(TAssistantPointMap::iterator i = m_points.begin(); i != m_points.end(); ++i)
      i->second.position += d;
    fixPoints();
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------------------------

bool
TAssistantBase::movePoint(const TStringId &name, const TPointD &position) {
  TAssistantPointMap::iterator i = m_points.find(name);
  if (i != m_points.end() && i->second.position != position) {
    onMovePoint(i->second, position);
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::setPointSelection(const TStringId &name, bool selected)  const {
  if (const TAssistantPoint *p = findPoint(name))
    p->selected = selected;
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::setAllPointsSelection(bool selected) const {
  for(TAssistantPointMap::const_iterator i = points().begin(); i != points().end(); ++i)
    i->second.selected = selected;
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onDataChanged(const TVariant &value) {
  const TVariant& pointsData = data()[m_idPoints];
  TVariantPathEntry entry;

  if (&value == &data() || &value == &pointsData)
    onAllDataChanged();
  else
  if (pointsData.getChildPathEntry(value, entry) && entry.isField()) {
    const TVariant& pointData = pointsData[entry];
    TPointD position = TPointD(
      pointData[m_idX].getDouble(),
      pointData[m_idY].getDouble() );
    movePoint(entry.field(), position);
  } else
  if (data().getChildPathEntry(value, entry) && entry.isField()) {
    onDataFieldChanged(entry.field(), data()[entry.field()]);
  }
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onDataFieldChanged(const TStringId &name, const TVariant &value)
  { updateProperty(name, value); }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onAllDataChanged() {
  const TVariant& pointsData = data()[m_idPoints];
  for(TAssistantPointMap::iterator i = m_points.begin(); i != m_points.end(); ++i) {
    const TVariant& pointData = pointsData[i->first];
    i->second.position = TPointD(
      pointData[m_idX].getDouble(),
      pointData[m_idY].getDouble() );
  }
  fixPoints();
  updateProperties();
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onFixPoints()
  { }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onMovePoint(TAssistantPoint &point, const TPointD &position)
  { point.position = position; }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onFixData() {
  TVariant& pointsData = data()[m_idPoints];
  for(TAssistantPointMap::const_iterator i = points().begin(); i != points().end(); ++i) {
    TVariant& pointData = pointsData[i->first];
    pointData[m_idX].setDouble( i->second.position.x );
    pointData[m_idY].setDouble( i->second.position.y );
  }
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::updateProperties() {
  const TVariantMap &map = data().getMap();
  for(TVariantMap::const_iterator i = map.begin(); i != map.end(); ++i)
    if (i->first != m_idPoints)
      updateProperty(i->first, i->second);
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::updateProperty(const TStringId &name, const TVariant &value) {
  TProperty *property = m_properties.getProperty(name);
  if (!property)
    return;

  if (TBoolProperty *boolProperty = dynamic_cast<TBoolProperty*>(property)) {
    boolProperty->setValue( value.getBool() );
  } else
  if (TDoubleProperty *doubleProperty = dynamic_cast<TDoubleProperty*>(property)) {
    doubleProperty->setValue( value.getDouble() );
  } else
  if (TIntProperty *intProperty = dynamic_cast<TIntProperty*>(property)) {
    intProperty->setValue( (int)value.getDouble() );
  } else
  if (TStringProperty *stringProperty = dynamic_cast<TStringProperty*>(property)) {
    stringProperty->setValue( to_wstring(value.getString()) );
  } else
  if (TEnumProperty *enumProperty = dynamic_cast<TEnumProperty*>(property)) {
    enumProperty->setValue( to_wstring(value.getString()) );
  }
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::onPropertyChanged(const TStringId &name) {
  TProperty *property = m_properties.getProperty(name);
  if (!property)
    return;

  if (name == m_idPoints)
    return;

  if (TBoolProperty *boolProperty = dynamic_cast<TBoolProperty*>(property)) {
    data()[name].setBool( boolProperty->getValue() );
  } else
  if (TDoubleProperty *doubleProperty = dynamic_cast<TDoubleProperty*>(property)) {
    data()[name].setDouble( doubleProperty->getValue() );
  } else
  if (TIntProperty *intProperty = dynamic_cast<TIntProperty*>(property)) {
    data()[name].setDouble( (double)intProperty->getValue() );
  } else
  if (TStringProperty *stringProperty = dynamic_cast<TStringProperty*>(property)) {
    data()[name].setString( to_string(stringProperty->getValue()) );
  } else
  if (TEnumProperty *enumProperty = dynamic_cast<TEnumProperty*>(property)) {
    data()[name].setString( to_string(enumProperty->getValue()) );
  }
}

//---------------------------------------------------------------------------------------------------

double
TAssistantBase::getDrawingAlpha(bool enabled) const
  { return enabled && this->getEnabled() ? 0.5 : 0.25; }

//---------------------------------------------------------------------------------------------------

double
TAssistantBase::getDrawingGridAlpha() const
  { return 0.2; }

//---------------------------------------------------------------------------------------------------

TPixelD
TAssistantBase::makeContrastColor(const TPixelD &color) {
  return color.r + color.g + color.b < 1.5
       ? TPixelD(1, 1, 1, color.m)
       : TPixelD(0, 0, 0, color.m);
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawSegment(const TPointD &p0, const TPointD &p1, double pixelSize, double alpha0, double alpha1) {
  TPixelD color0 = (drawFlags & DRAW_ERROR) ? colorError : colorBase;
  TPixelD color1 = color0;
  color0.m *= alpha0;
  color1.m *= alpha1;
  TPixelD colorBack0 = makeContrastColor(color0);
  TPixelD colorBack1 = makeContrastColor(color1);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();
  tglEnableLineSmooth(true, 1.0 * lineWidthScale);
  TPointD d = p1 - p0;
  double k = norm2(d);
  if (k > TConsts::epsilon*TConsts::epsilon) {
    k = 0.5*pixelSize*lineWidthScale/sqrt(k);
    d = TPointD(-k*d.y, k*d.x);
    glBegin(GL_LINES);
    tglColor(colorBack0); tglVertex(p0 - d);
    tglColor(colorBack1); tglVertex(p1 - d);
    tglColor(color0); tglVertex(p0 + d);
    tglColor(color1); tglVertex(p1 + d);
    glEnd();
  }
  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawMark(const TPointD &p, const TPointD &normal, double pixelSize, double alpha) {
  TPointD d = normal*5*pixelSize;
  drawSegment(p - d,p + d, pixelSize, alpha);
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawDot(const TPointD &p, const TPixelD &color) {
  TPixelD colorBack = makeContrastColor(color);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();

  tglColor(colorBack);
  tglEnablePointSmooth(6.0);
  glBegin(GL_POINTS);
  glVertex2d(p.x, p.y);
  glEnd();

  tglColor(color);
  tglEnablePointSmooth(3.0);
  glBegin(GL_POINTS);
  glVertex2d(p.x, p.y);
  glEnd();

  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawDot(const TPointD &p, double alpha) {
  TPixelD color = (drawFlags & DRAW_ERROR) ? colorError : colorBase;
  color.m *= alpha;
  drawDot(p, color);
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawPoint(const TAssistantPoint &point, double pixelSize) {
  if (!point.visible) return;

  double radius = point.radius;
  double crossSize = 1.2*radius;
  double width = point.selected ? 2.0 : 1.5;
  
  double alpha = 0.5;
  TPixelD colorStroke = point.selected ? colorSelect : colorEdit;
  colorStroke.m *= alpha;
  TPixelD colorFill = colorStroke;
  colorFill.m *= 0.5;
  TPixelD colorBack = makeContrastColor(colorStroke);

  glPushAttrib(GL_ALL_ATTRIB_BITS);

  // fill
  tglEnableBlending();
  if (point.type == TAssistantPoint::CircleFill) {
    tglColor(colorFill);
    tglDrawDisk(point.position, radius*pixelSize);
  }

  TPointD crossA(pixelSize*crossSize, 0.0);
  TPointD crossB(0.0, pixelSize*crossSize);
  TPointD gridDx(pixelSize*radius, 0.0);
  TPointD gridDy(0.0, pixelSize*radius);

  bool cross = point.type == TAssistantPoint::CircleCross
            || point.type == TAssistantPoint::CircleDiagonalCross;
  if (point.type == TAssistantPoint::CircleDiagonalCross)
    { crossA.y = crossB.y; crossB.x = crossA.x; }
  
  // back line
  tglEnableLineSmooth(true, 2.0*width*lineWidthScale);
  tglColor(colorBack);
  if (cross) {
    tglDrawSegment(point.position - crossA, point.position + crossA);
    tglDrawSegment(point.position - crossB, point.position + crossB);
  }
  tglDrawCircle(point.position, radius*pixelSize);

  // front line
  glLineWidth(width * lineWidthScale);
  tglColor(colorStroke);
  if (cross) {
    tglDrawSegment(point.position - crossA, point.position + crossA);
    tglDrawSegment(point.position - crossB, point.position + crossB);
  }
  tglDrawCircle(point.position, radius*pixelSize);

  // dots
  switch(point.type) {
  case TAssistantPoint::CircleDoubleDots:
    drawDot(point.position - gridDx*0.5, colorStroke);
    drawDot(point.position + gridDx*0.5, colorStroke);
    drawDot(point.position - gridDy*0.5, colorStroke);
    drawDot(point.position + gridDy*0.5, colorStroke);
    //no break
  case TAssistantPoint::CircleDots:
    drawDot(point.position - gridDx, colorStroke);
    drawDot(point.position + gridDx, colorStroke);
    drawDot(point.position - gridDy, colorStroke);
    drawDot(point.position + gridDy, colorStroke);
    //no break
  case TAssistantPoint::Circle:
    drawDot(point.position, colorStroke);
    break;
  default:
    break;
  }

  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawIndex(const TPointD &p, int index, bool selected, double pixelSize) {
  static const int segments[7][4] = {
    { 0, 2, 1, 2 },   // A
    { 1, 1, 1, 2 },   // B    + A +
    { 1, 0, 1, 1 },   // C    F   B
    { 0, 0, 1, 0 },   // D    + G +
    { 0, 0, 0, 1 },   // E    E   C
    { 0, 1, 0, 2 },   // F    + D +
    { 0, 1, 1, 1 } }; // G
    
  static const int glyphs[][7] = {
  // A B C D E F G
    {1,1,1,1,1,1,0},   // 0
    {0,1,1,0,0,0,0},   // 1
    {1,1,0,1,1,0,1},   // 2
    {1,1,1,1,0,0,1},   // 3
    {0,1,1,0,0,1,1},   // 4
    {1,0,1,1,0,1,1},   // 5
    {1,0,1,1,1,1,1},   // 6
    {1,1,1,0,0,1,0},   // 7
    {1,1,1,1,1,1,1},   // 8
    {1,1,1,1,0,1,1} }; // 9
  
  if (index < 0) index = 0;
  
  int len = 0;
  int digits[16] = {};
  for(int i = index; i; i /= 10)
    digits[len++] = i%10;
  if (!len) len = 1;
  
  double w = 5, h = 5, d = 0.5, dx = w+2;
  double alpha = 0.5;
  TPixelD color = selected ? colorSelect : colorEdit;
  color.m *= alpha;
  TPixelD colorBack = makeContrastColor(color);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();
  tglEnableLineSmooth(true, 1.0 * lineWidthScale);
  double k = 0.5*pixelSize*lineWidthScale;
  
  double y = p.y;
  for(int i = 0; i < len; ++i) {
    double x = p.x + dx*(len-i-1);
    const int *g = glyphs[digits[i]];
    for(int i = 0; i < 7; ++i) {
      if (!g[i]) continue;
      const int *s = segments[i];
      if (s[0] == s[2]) {
        // vertical
        tglColor(colorBack);
        tglDrawSegment(
          TPointD(x + s[0]*w + k, y + s[1]*h + d),
          TPointD(x + s[2]*w + k, y + s[3]*h - d) );
        tglColor(color);
        tglDrawSegment(
          TPointD(x + s[0]*w - k, y + s[1]*h + d),
          TPointD(x + s[2]*w - k, y + s[3]*h - d) );
      } else {
        // horisontal
        tglColor(colorBack);
        tglDrawSegment(
          TPointD(x + s[0]*w + d, y + s[1]*h + k),
          TPointD(x + s[2]*w - d, y + s[3]*h + k) );
        tglColor(color);
        tglDrawSegment(
          TPointD(x + s[0]*w + d, y + s[1]*h - k),
          TPointD(x + s[2]*w - d, y + s[3]*h - k) );
      }
    }
  }
  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::draw(TToolViewer*, bool) const
  { }

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawEdit(TToolViewer *viewer) const {
  // paint all points
  draw(viewer);
  double pixelSize = sqrt(tglGetPixelSize2());
  for(TAssistantPointMap::const_iterator i = points().begin(); i != points().end(); ++i)
    drawPoint(i->second, pixelSize);
}

//---------------------------------------------------------------------------------------------------

void
TAssistantBase::drawEdit(TToolViewer *viewer, int index) const {
  drawEdit(viewer);
  drawIndex(
    getBasePoint().position + TPointD(8, 8),
    index, getBasePoint().selected, sqrt(tglGetPixelSize2()) );
}


//************************************************************************
//    TAssistant implementation
//************************************************************************

TAssistant::TAssistant(TMetaObject &object):
  TAssistantBase(object),
  m_idMagnetism("magnetism")
  { addProperty( new TDoubleProperty(m_idMagnetism.str(), 0.0, 1.0, getMagnetism()) ); }

//---------------------------------------------------------------------------------------------------

void
TAssistant::updateTranslation() const {
  TAssistantBase::updateTranslation();
  setTranslation(m_idMagnetism, tr("Magnetism"));
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onSetDefaults() {
  setMagnetism(1.0);
  TAssistantBase::onSetDefaults();
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::onFixData() {
  TAssistantBase::onFixData();
  setMagnetism( std::max(0.0, std::min(1.0, getMagnetism())) );
}

//---------------------------------------------------------------------------------------------------

void
TAssistant::getGuidelines(const TPointD&, const TAffine&, const TPixelD&, TGuidelineList&) const
  { }

//---------------------------------------------------------------------------------------------------

bool
TAssistant::calcPerspectiveStep(
  double minStep,
  double minX,
  double maxX,
  double x0,
  double x1,
  double x2,
  double &outW,
  double &outMinI,
  double &outMaxI )
{
  outW = outMinI = outMaxI = 0;
  if (!(minStep > TConsts::epsilon)) return false;
  if (!(minX + TConsts::epsilon < maxX)) return false;
  
  minX -= x0;
  maxX -= x0;
  x1   -= x0;
  x2   -= x0;

  // check and fix input
  if (!(fabs(x1) > TConsts::epsilon)) return false;
  if (!(fabs(x2) > TConsts::epsilon)) return false;
  if (!(fabs(x1 - x2) > TConsts::epsilon)) return false;
  if ((x1 < 0) != (x2 < 0)) x2 = -x2;
  if (fabs(x2) < fabs(x1)) std::swap(x1, x2);

  // check if bounds is behind the horizon
  if (x1 < 0 && !(minX < -TConsts::epsilon)) return false;
  if (x1 > 0 && !(maxX >  TConsts::epsilon)) return false;
  
  // calc W and initial bounds
  double w = 1/x2 - 1/x1;
  double d = sqrt(fabs(w/minStep));
  if (x1 < 0) d = -d;
  double i0 = (d - 1)/w;
  double i1 = -1/w;
  
  // min/max bounds
  if (x1 < 0) {
    if (maxX < -TConsts::epsilon)
      i0 = std::max( i0, (1 - maxX)/(maxX*w) );
    i1 = std::min( i1, (1 - minX)/(minX*w) );
  } else {
    if (minX > TConsts::epsilon)
      i0 = std::max( i0, (1 - minX)/(minX*w) );
    i1 = std::min( i1, (1 - maxX)/(maxX*w) );
  }

  // i must be iterable (i < i + 1)
  const double bound = 1e10;
  i0 = i0 > -bound ? (i0 < bound ? i0 : bound) : -bound;
  i1 = i1 > -bound ? (i1 < bound ? i1 : bound) : -bound;
  
  // tune beginning of grid to place it axact to grid points
  // must be: i0 + integer == ig
  double ig = (1 - x1)/(x1*w);
  double frac = ig - floor(ig);
  i0 = ceil(i0 - frac + TConsts::epsilon) + frac;
  i1 -= TConsts::epsilon;
  
  // restrict count
  if (i1 - i0 > 10000) return false;
  if (!(i0 < i1)) return false;
  
  outW = w;
  outMinI = i0;
  outMaxI = i1;
  return true;
}


bool
TAssistant::scanAssistants(
  TTool *tool,
  const TPointD *positions,
  int positionsCount,
  TGuidelineList *outGuidelines,
  bool draw,
  bool enabledOnly,
  bool markEnabled,
  bool drawGuidelines,
  TImage *skipImage )
{
  TGuidelineList guidelines;
  if (drawGuidelines && !outGuidelines)
    outGuidelines = &guidelines;

  bool found = false;
  bool findGuidelines = (positions && positionsCount > 0 && outGuidelines);
  if (!findGuidelines) drawGuidelines = false;
  bool doSomething = findGuidelines || draw;
  
  if (tool)
  if (TToolViewer *viewer = tool->getViewer())
  if (TApplication *application = tool->getApplication())
  if (TXshLevelHandle *levelHandle = application->getCurrentLevel())
  if (TXshLevel *level = levelHandle->getLevel())
  if (TXshSimpleLevel *simpleLevel = level->getSimpleLevel())
  if (TFrameHandle *frameHandle = application->getCurrentFrame())
  if (TXsheetHandle *XsheetHandle = application->getCurrentXsheet())
  if (TXsheet *Xsheet = XsheetHandle->getXsheet())
  {
    ToonzScene *scene = Xsheet->getScene();
    TSceneProperties *props = scene ? scene->getProperties() : nullptr;
    TPointD dpiScale = getCurrentDpiScale(simpleLevel, tool->getCurrentFid());
    int frame = frameHandle->getFrame();
    int count = Xsheet->getColumnCount();
    TAffine worldToTrack;
    if ( tool->getToolType() & TTool::LevelTool
      && !application->getCurrentObject()->isSpline() )
    {
      worldToTrack.a11 /= dpiScale.x;
      worldToTrack.a22 /= dpiScale.y;
    }

    for(int i = 0; i < count; ++i) {
      if (TXshColumn *column = Xsheet->getColumn(i))
      if (column->isCamstandVisible())
      if (column->isPreviewVisible())
      if (TImageP image = Xsheet->getCell(frame, i).getImage(false))
      if (image != skipImage)
      if (image->getType() == TImage::META)
      if (TMetaImage *metaImage = dynamic_cast<TMetaImage*>(image.getPointer()))
      {
        TAffine imageToTrack = worldToTrack * tool->getColumnMatrix(i);
        
        TPixelD origColor = TAssistantBase::colorBase;
        TPixelD color = props && column->getColorFilterId()
                      ? toPixelD(props->getColorFilterColor( column->getColorFilterId() ))
                      : origColor;
        color.m *= column->getOpacity()/255.0;
        
        if (draw) {
          TAssistantBase::colorBase = color;
          glPushMatrix();
          tglMultMatrix(imageToTrack);
        }

        TMetaImage::Reader reader(*metaImage);
        for(TMetaObjectListCW::iterator i = reader->begin(); i != reader->end(); ++i) {
          if (*i)
          if (const TAssistant *assistant = (*i)->getHandler<TAssistant>())
          if (!enabledOnly || assistant->getEnabled())
          {
            found = true;
            if (!doSomething) return true;
            if (findGuidelines)
              for(int i = 0; i < positionsCount; ++i)
                assistant->getGuidelines(positions[i], imageToTrack, color, *outGuidelines);
            if (draw) assistant->draw(viewer, assistant->getEnabled() && markEnabled);
          }
        } // for each meta object
        
        if (draw) {
          glPopMatrix();
          TAssistantBase::colorBase = origColor;
        }
      }
    } // for each replication
  }
  
  if (drawGuidelines)
    for(TGuidelineList::const_iterator i = outGuidelines->begin(); i != outGuidelines->end(); ++i)
      (*i)->draw();
  
  return found;
}
