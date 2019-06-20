

// TnzCore includes
#include "tcolorfunctions.h"
#include "trandom.h"
#include "tflash.h"
#include "tcurves.h"
#include "tvectorrenderdata.h"
#include "tmathutil.h"
#include "colorfxutils.h"
#include "tpixelutils.h"

// tcg includes
#include "tcg/tcg_misc.h"

// Qt includes
#include <QCoreApplication>
#include <QStringList>

#include "strokestyles.h"

using namespace std;

#define MINTHICK 1.0

//=============================================================================

namespace {

template <class T>
class TOptimizedStrokePropT final : public TStrokeProp {
protected:
  double m_pixelSize;

  TOptimizedStrokeStyleT<T> *m_colorStyle;
  T m_data;

public:
  TOptimizedStrokePropT(const TStroke *stroke,
                        TOptimizedStrokeStyleT<T> *style);
  ~TOptimizedStrokePropT() { m_colorStyle->release(); }

  const TColorStyle *getColorStyle() const override;

  TStrokeProp *clone(const TStroke *stroke) const override;
  void draw(const TVectorRenderData &rd) override;
  void draw(TFlash &flash) override {
    getColorStyle()->drawStroke(flash, getStroke());
  }
};

//-----------------------------------------------------------------------------

template <class T>
TOptimizedStrokePropT<T>::TOptimizedStrokePropT(
    const TStroke *stroke, TOptimizedStrokeStyleT<T> *style)
    : TStrokeProp(stroke), m_colorStyle(style), m_pixelSize(0) {
  m_styleVersionNumber = style->getVersionNumber();
  m_colorStyle->addRef();
}

//-----------------------------------------------------------------------------

template <class T>
const TColorStyle *TOptimizedStrokePropT<T>::getColorStyle() const {
  return m_colorStyle;
}

//-----------------------------------------------------------------------------

template <class T>
TStrokeProp *TOptimizedStrokePropT<T>::clone(const TStroke *stroke) const {
  TOptimizedStrokePropT<T> *prop =
      new TOptimizedStrokePropT<T>(stroke, m_colorStyle);
  prop->m_strokeChanged = m_strokeChanged;
  prop->m_data          = m_data;
  return prop;
}

//-----------------------------------------------------------------------------

template <class T>
void TOptimizedStrokePropT<T>::draw(
    const TVectorRenderData
        &rd) /*assenza di const non e' una dimenticanza! Alcune sottoclassi
                devono ridefinire questo metodo e serbve che non sia const*/
{
  if (rd.m_clippingRect != TRect() && !rd.m_is3dView &&
      !convert(rd.m_aff * m_stroke->getBBox()).overlaps(rd.m_clippingRect))
    return;

  glPushMatrix();
  tglMultMatrix(rd.m_aff);

  double pixelSize = sqrt(tglGetPixelSize2());
  if (m_strokeChanged ||
      m_styleVersionNumber != m_colorStyle->getVersionNumber() ||
      !isAlmostZero(pixelSize - m_pixelSize, 1e-5)) {
    m_strokeChanged      = false;
    m_pixelSize          = pixelSize;
    m_styleVersionNumber = m_colorStyle->getVersionNumber();
    m_colorStyle->computeData(m_data, m_stroke, rd.m_cf);
  }

  m_colorStyle->drawStroke(rd.m_cf, m_data, m_stroke);

  glPopMatrix();
}
}

//=============================================================================

template <class T>
TStrokeProp *TOptimizedStrokeStyleT<T>::makeStrokeProp(const TStroke *stroke) {
  return new TOptimizedStrokePropT<T>(stroke, this);
}

//=============================================================================

inline void tglNormal(const T3DPointD &p) { glNormal3d(p.x, p.y, p.z); }

inline void tglVertex(const T3DPointD &p) { glVertex3d(p.x, p.y, p.z); }

//=============================================================================

TFurStrokeStyle::TFurStrokeStyle()
    : m_color(TPixel32::Black)
    , m_angle(120.0)
    , m_length(1.0)
    , m_cs(0.0)
    , m_sn(0.0) {
  double rad = m_angle * M_PI_180;
  m_cs       = cos(rad);
  m_sn       = sin(rad);
}

//-----------------------------------------------------------------------------

TColorStyle *TFurStrokeStyle::clone() const {
  return new TFurStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TFurStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TFurStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TFurStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < getParamCount());
  return index == 0 ? QCoreApplication::translate("TFurStrokeStyle", "Angle")
                    : QCoreApplication::translate("TFurStrokeStyle", "Size");
}

//-----------------------------------------------------------------------------

void TFurStrokeStyle::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < getParamCount());
  if (index == 0) {
    min = 0.0;
    max = 180.0;
  } else {
    min = 0.0;
    max = 1.0;
  }
}

//-----------------------------------------------------------------------------

double TFurStrokeStyle::getParamValue(TColorStyle::double_tag,
                                      int index) const {
  assert(0 <= index && index < getParamCount());
  return index == 0 ? m_angle : m_length;
}

//-----------------------------------------------------------------------------

void TFurStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < getParamCount());
  if (index == 0) {
    m_angle    = value;
    double rad = m_angle * M_PI_180;
    m_cs       = cos(rad);
    m_sn       = sin(rad);
  } else
    m_length = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TFurStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  double s  = 0.0;
  double ds = 4;
  double vs = 1;
  TRandom rnd;
  flash.setLineColor(m_color);
  vector<TSegment> segmentsArray;

  while (s <= length) {
    double w        = stroke->getParameterAtLength(s);
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD pos1    = (TPointD)pos;

    TPointD u = stroke->getSpeed(w);
    if (norm2(u) == 0.0) {
      s += 0.5;
      continue;
    }
    u         = normalize(u);
    TPointD v = rotate90(u);

    double length = m_length * pos.thick;
    vs            = -vs;

    double q = 0.01 * (rnd.getFloat() * 2 - 1);
    segmentsArray.push_back(
        TSegment(pos1, pos1 + length * ((m_cs + q) * u + (vs * m_sn) * v)));
    s += ds;
  }

  flash.drawSegments(segmentsArray, true);
}

//-----------------------------------------------------------------------------
void TFurStrokeStyle::computeData(Points &positions, const TStroke *stroke,
                                  const TColorFunction *cf) const {
  double length = stroke->getLength();

  double s  = 0.0;
  double ds = 4;
  double vs = 1;
  TRandom rnd;

  positions.clear();
  positions.reserve(tceil(length / ds) + 1);

  while (s <= length) {
    double w        = stroke->getParameterAtLength(s);
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0.0) {
      s += 0.5;
      continue;
    }
    u         = normalize(u);
    TPointD v = rotate90(u);

    double length = 0;
    if (pos.thick)
      length = m_length * pos.thick;
    else
      length = 1.0;

    vs = -vs;

    positions.push_back(pos);
    double q = 0.01 * (rnd.getFloat() * 2 - 1);
    positions.push_back(pos + length * ((m_cs + q) * u + (vs * m_sn) * v));

    s += ds;
  }
}

//-----------------------------------------------------------------------------
void TFurStrokeStyle::drawStroke(const TColorFunction *cf, Points &positions,
                                 const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  tglColor(color);

  for (UINT i = 0; i < positions.size(); i += 2) {
    glBegin(GL_LINE_STRIP);
    tglColor(color);
    tglVertex(positions[i]);
    glColor4d(1, 1, 1, 0.0);
    tglVertex(positions[i + 1]);
    glEnd();
  }
}

//=============================================================================

TChainStrokeStyle::TChainStrokeStyle(const TPixel32 &color) : m_color(color) {}

//-----------------------------------------------------------------------------

TChainStrokeStyle::TChainStrokeStyle() : m_color(TPixel32(20, 10, 0)) {}

//-----------------------------------------------------------------------------

TColorStyle *TChainStrokeStyle::clone() const {
  return new TChainStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

void TChainStrokeStyle::computeData(Points &data, const TStroke *stroke,
                                    const TColorFunction *cf) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  // spessore della catena = spessore "medio" dello stroke
  double thickness =
      0.25 *
      (stroke->getThickPoint(0).thick + stroke->getThickPoint(1.0 / 3.0).thick +
       stroke->getThickPoint(2.0 / 3.0).thick + stroke->getThickPoint(1).thick);

  if (thickness == 0) thickness = 0.1;
  double ringHeight             = thickness;
  double ringWidth              = 1.5 * ringHeight;

  // distanza fra i centri degli anelli
  double ringDistance = 2 * 1.2 * ringWidth;

  // distanza fra il centro dell'anello e il punto di attacco dell'anello
  // trasversale
  // double joinPos = 0.45 * ringWidth;

  TPointD oldPos;
  // bool firstRing = true;
  double s = 0;

  data.clear();
  data.reserve(tceil(length / ringDistance) * 2 + 2);

  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u = normalize(u);

    data.push_back(pos);
    data.push_back(u);

    // se e' il caso unisco la catena alla precedente
    s += ringDistance;
  }
}

//-----------------------------------------------------------------------------

void TChainStrokeStyle::drawStroke(const TColorFunction *cf, Points &data,
                                   const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  // double length = stroke->getLength();

  // spessore della catena = spessore "medio" dello stroke
  double thickness =
      0.25 *
      (stroke->getThickPoint(0).thick + stroke->getThickPoint(1.0 / 3.0).thick +
       stroke->getThickPoint(2.0 / 3.0).thick + stroke->getThickPoint(1).thick);

  if (thickness * thickness < 4 * tglGetPixelSize2()) {
    TCenterLineStrokeStyle *appStyle =
        new TCenterLineStrokeStyle(m_color, 0x0, thickness);
    appStyle->drawStroke(cf, stroke);
    delete appStyle;
    return;
  }

  assert(thickness);
  double ringHeight = thickness;
  double ringWidth  = 1.5 * ringHeight;

  // distanza fra i centri degli anelli
  // double ringDistance = 2 * 1.2 * ringWidth;

  // distanza fra il centro dell'anello e il punto di attacco dell'anello
  // trasversale
  double joinPos = 0.45 * ringWidth;

  // definisco la forma dell'anello della catena
  GLuint ringId;
  ringId   = glGenLists(1);
  double a = .6, b = .6;
  glNewList(ringId, GL_COMPILE);
  glPushMatrix();
  glScaled(ringWidth, ringHeight, 1);
  glBegin(GL_LINE_STRIP);
  glVertex2d(1, b);
  glVertex2d(a, 1);
  glVertex2d(-a, 1);
  glVertex2d(-1, b);
  glVertex2d(-1, -b);
  glVertex2d(-a, -1);
  glVertex2d(a, -1);
  glVertex2d(1, -b);
  glVertex2d(1, b);
  glEnd();
  glPopMatrix();
  glEndList();

  // disegno la catena

  if (cf)
    tglColor((*cf)(m_color));
  else
    tglColor(m_color);

  TPointD oldPos;
  // bool firstRing = true;
  // double s = 0;
  for (UINT i = 0; i < data.size(); i += 2) {
    TPointD pos = data[i];
    TPointD u   = data[i + 1];
    TPointD v   = rotate90(u);

    // disegno un anello della catena
    glPushMatrix();
    TAffine aff(u.x, v.x, pos.x, u.y, v.y, pos.y);
    tglMultMatrix(aff);
    glCallList(ringId);
    glPopMatrix();

    // se e' il caso unisco la catena alla precedente
    if (i != 0) {
      TPointD q = pos - u * joinPos;
      tglDrawSegment(oldPos, q);
    }

    oldPos = pos + u * joinPos;
  }

  glDeleteLists(ringId, 1);
}

//-----------------------------------------------------------------------------

void TChainStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  // spessore della catena = spessore "medio" dello stroke
  double thickness =
      0.25 *
      (stroke->getThickPoint(0).thick + stroke->getThickPoint(1.0 / 3.0).thick +
       stroke->getThickPoint(2.0 / 3.0).thick + stroke->getThickPoint(1).thick);

  if (thickness < 2) {
    TCenterLineStrokeStyle *appStyle =
        new TCenterLineStrokeStyle(m_color, 0x0, thickness);
    appStyle->drawStroke(flash, stroke);
    delete appStyle;
    return;
  }

  assert(thickness);
  double ringHeight   = thickness;
  double ringWidth    = 1.5 * ringHeight;
  double ringDistance = 2 * 1.2 * ringWidth;

  double joinPos = 0.45 * ringWidth;
  // const int ringId = 124;
  double a = .6, b = .6;

  TScale scaleM(ringWidth, ringHeight);
  vector<TPointD> chain;
  chain.push_back(scaleM * TPointD(1, b));    // 0
  chain.push_back(scaleM * TPointD(a, 1));    // 1
  chain.push_back(scaleM * TPointD(-a, 1));   // 2
  chain.push_back(scaleM * TPointD(-1, b));   // 3
  chain.push_back(scaleM * TPointD(-1, -b));  // 4
  chain.push_back(scaleM * TPointD(-a, -1));  // 5
  chain.push_back(scaleM * TPointD(a, -1));   // 6
  chain.push_back(scaleM * TPointD(1, -b));   // 7
  chain.push_back(scaleM * TPointD(1, b));    // 8

  /*
chain.push_back( TPointD(1, b));	//0
chain.push_back( TPointD( a, 1));	//1
chain.push_back( TPointD(-a, 1));	//2
chain.push_back( TPointD(-1, b));	//3
chain.push_back( TPointD(-1,-b));	//4
chain.push_back( TPointD(-a,-1));	//5
chain.push_back( TPointD( a,-1));	//6
chain.push_back( TPointD( 1,-b));	//7
chain.push_back( TPointD(1, b));	//8
*/

  vector<TSegment> chainS;
  chainS.push_back(TSegment(chain[0], chain[1]));
  chainS.push_back(TSegment(chain[1], chain[2]));
  chainS.push_back(TSegment(chain[2], chain[3]));
  chainS.push_back(TSegment(chain[3], chain[4]));
  chainS.push_back(TSegment(chain[4], chain[5]));
  chainS.push_back(TSegment(chain[5], chain[6]));
  chainS.push_back(TSegment(chain[6], chain[7]));
  chainS.push_back(TSegment(chain[7], chain[0]));

  flash.setLineColor(m_color);

  TPointD oldPos;
  bool firstRing = true;
  double s       = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u = normalize(u);
    //     TPointD v = rotate90(u);

    TTranslation translM(pos.x, pos.y);
    TRotation rotM(rad2degree(atan(u)));
    TAffine tM = translM * rotM;

    //	 With direct transformation
    vector<TSegment> lchainS;
    for (int i = 0; i < 8; i++)
      lchainS.push_back(TSegment(tM * chain[i], tM * chain[i + 1]));
    flash.drawSegments(lchainS, false);

    /*
// With TFlash transformations
flash.pushMatrix();
flash.multMatrix(translM);
flash.multMatrix(rotM);
//	 flash.multMatrix(scaleM);
flash.drawSegments(chainS,false);
//	 if (chainId==-1)
//		chainId=flash.drawSegments(chainS,false);
//	 else
//		 flash.drawShape(chainId);
flash.popMatrix();
*/

    if (!firstRing) {
      TPointD q = pos - u * joinPos;
      vector<TSegment> sv;
      sv.push_back(TSegment(oldPos, q));
      flash.drawSegments(sv, false);
    } else
      firstRing = false;

    oldPos = pos + u * joinPos;
    s += ringDistance;
  }
}

//=============================================================================

TSprayStrokeStyle::TSprayStrokeStyle()
    : m_color(TPixel32::Blue), m_blend(0.5), m_intensity(10.0), m_radius(0.3) {}

//-----------------------------------------------------------------------------

TColorStyle *TSprayStrokeStyle::clone() const {
  return new TSprayStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TSprayStrokeStyle::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TSprayStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TSprayStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 3);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TSprayStrokeStyle", "Border Fade");
    break;
  case 1:
    value = QCoreApplication::translate("TSprayStrokeStyle", "Density");
    break;
  case 2:
    value = QCoreApplication::translate("TSprayStrokeStyle", "Size");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TSprayStrokeStyle::getParamRange(int index, double &min,
                                      double &max) const {
  assert(0 <= index && index < 3);
  switch (index) {
  case 0:
    min = 0.0;
    max = 1.0;
    break;
  case 1:
    min = 0.0;
    max = 100.0;
    break;
  case 2:
    min = 0.0;
    max = 1.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TSprayStrokeStyle::getParamValue(TColorStyle::double_tag,
                                        int index) const {
  assert(0 <= index && index < 3);
  double value = 0;
  switch (index) {
  case 0:
    value = m_blend;
    break;
  case 1:
    value = m_intensity;
    break;
  case 2:
    value = m_radius;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TSprayStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);
  switch (index) {
  case 0:
    m_blend = value;
    break;
  case 1:
    m_intensity = value;
    break;
  case 2:
    m_radius = value;
    break;
  }

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TSprayStrokeStyle::drawStroke(const TColorFunction *cf,
                                   const TStroke *stroke) const {
  //  TStroke *stroke = getStroke();
  double length = stroke->getLength();
  double step   = 4;

  double blend = m_blend;          // distanza che controlla da dove il gessetto
                                   // comincia il fade out  (0, 1)
  double intensity = m_intensity;  // quanti punti vengono disegnati ad ogni
                                   // step
  double radius = m_radius;
  double decay  = 1 - blend;
  bool fill     = 0;
  TPointD pos1;
  TRandom rnd;
  TPixelD dcolor;
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color             = m_color;
  dcolor              = toPixelD(color);
  double s            = 0;
  double minthickness = MINTHICK * sqrt(tglGetPixelSize2());
  double thickness    = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u);
    TPointD shift;
    if (pos.thick < MINTHICK)
      thickness = minthickness;
    else
      thickness = pos.thick;
    for (int i = 0; i < intensity; i++) {
      double vrandnorm = (0.5 - rnd.getFloat()) * 2;
      double randomv   = vrandnorm * pos.thick;
      double randomu   = (0.5 - rnd.getFloat()) * step;
      shift            = u * randomu + v * randomv;
      pos1             = pos + shift;
      double mod       = fabs(vrandnorm);
      if (mod < decay)
        glColor4d(dcolor.r, dcolor.g, dcolor.b, rnd.getFloat() * dcolor.m);
      else
        glColor4d(dcolor.r, dcolor.g, dcolor.b,
                  rnd.getFloat() * (1 - mod) * dcolor.m);
      if (fill)
        tglDrawDisk(pos1, radius * thickness * rnd.getFloat());
      else
        tglDrawCircle(pos1, radius * thickness * rnd.getFloat());
    }
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TSprayStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  double length = stroke->getLength();
  double step   = 4;

  double blend = m_blend;          // distanza che controlla da dove il gessetto
                                   // comincia il fade out  (0, 1)
  double intensity = m_intensity;  // quanti punti vengono disegnati ad ogni
                                   // step
  double radius = m_radius;
  double decay  = 1 - blend;
  bool fill     = 0;
  TPointD pos1;
  TRandom rnd;
  TPixel32 color = m_color;
  TPixelD dcolor;
  dcolor = toPixelD(color);

  double s = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u);
    TPointD shift;
    for (int i = 0; i < intensity; i++) {
      double vrandnorm = (0.5 - rnd.getFloat()) * 2;
      double randomv   = vrandnorm * pos.thick;
      double randomu   = (0.5 - rnd.getFloat()) * step;
      shift            = u * randomu + v * randomv;
      pos1             = pos + shift;
      double mod       = fabs(vrandnorm);
      TPixelD ldcolor  = dcolor;
      ldcolor.m        = mod < decay ? rnd.getFloat() * dcolor.m
                              : rnd.getFloat() * (1 - mod) * dcolor.m;
      TPixel32 lcolor;
      lcolor = toPixel32(ldcolor);
      if (fill) {
        flash.setFillColor(lcolor);
        double r = radius * pos.thick * rnd.getFloat();
        flash.drawEllipse(pos1, r, r);
      } else {
        flash.setLineColor(lcolor);
        flash.setFillColor(TPixel32(0, 0, 0, 0));
        flash.setThickness(0.5);
        double r = radius * pos.thick * rnd.getFloat();
        flash.drawEllipse(pos1, r, r);
      }
    }
    s += step;
  }
}

//=============================================================================

TGraphicPenStrokeStyle::TGraphicPenStrokeStyle()
    : m_color(TPixel32::Black), m_intensity(10.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TGraphicPenStrokeStyle::clone() const {
  return new TGraphicPenStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TGraphicPenStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TGraphicPenStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TGraphicPenStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TGraphicPenStrokeStyle", "Density");
}

//-----------------------------------------------------------------------------

void TGraphicPenStrokeStyle::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < 1);
  min = 0.0;
  max = 10.0;
}

//-----------------------------------------------------------------------------

double TGraphicPenStrokeStyle::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < 1);
  return m_intensity;
}

//-----------------------------------------------------------------------------

void TGraphicPenStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_intensity = value;
  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TGraphicPenStrokeStyle::computeData(DrawmodePointsMatrix &data,
                                         const TStroke *stroke,
                                         const TColorFunction *cf) const {
  data.clear();
  double length = stroke->getLength();
  double step   = 10;
  TPointD pos1, pos2;
  TRandom rnd;
  double intensity = m_intensity;
  data.reserve(tceil(length / 10.0));
  double s = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u);
    TPointD shift;
    Points tmpPoints;
    tmpPoints.clear();
    GLenum drawMode;
    for (int i = 0; i < intensity; i++) {
      if (pos.thick) {
        drawMode = GL_LINES;
        tmpPoints.reserve((int)(intensity * 2 + 1));
        double randomv = (0.5 - rnd.getFloat()) * pos.thick;
        double randomu = (0.5 - rnd.getFloat()) * step;
        shift          = randomu * u + randomv * v;
        pos1           = pos + shift + v * (pos.thick);
        pos2           = pos + shift - v * (pos.thick);
        tmpPoints.push_back(pos1);
        tmpPoints.push_back(pos2);
      } else {
        drawMode = GL_POINTS;
        tmpPoints.reserve((int)(intensity + 1));
        tmpPoints.push_back((TPointD)pos);
      }
    }
    if (!tmpPoints.empty()) {
      assert(drawMode == GL_POINTS || drawMode == GL_LINES);
      data.push_back(make_pair(drawMode, tmpPoints));
    }
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TGraphicPenStrokeStyle::drawStroke(const TColorFunction *cf,
                                        DrawmodePointsMatrix &data,
                                        const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  tglColor(color);

  DrawmodePointsMatrix::iterator it1 = data.begin();
  for (; it1 != data.end(); ++it1) {
    if (it1->first == GL_LINES) {
      Points::iterator it2 = it1->second.begin();
      glBegin(GL_LINES);
      for (; it2 != it1->second.end(); ++it2) tglVertex(*it2);
      glEnd();
    } else {
      assert(it1->first == GL_POINTS);
      Points::iterator it2 = it1->second.begin();
      glBegin(GL_POINTS);
      for (; it2 != it1->second.end(); ++it2) tglVertex(*it2);
      glEnd();
    }
  }
}

//-----------------------------------------------------------------------------

void TGraphicPenStrokeStyle::drawStroke(TFlash &flash,
                                        const TStroke *stroke) const

{
  // TStroke *stroke = getStroke();
  vector<TSegment> segmentsArray;
  double length = stroke->getLength();
  double step   = 10;
  TPointD pos1, pos2;
  TRandom rnd;
  double intensity = m_intensity;
  flash.setLineColor(m_color);

  double s = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u);
    TPointD shift;
    for (int i = 0; i < intensity; i++) {
      double randomv = (0.5 - rnd.getFloat()) * pos.thick;
      double randomu = (0.5 - rnd.getFloat()) * step;
      shift          = randomu * u + randomv * v;
      pos1           = pos + shift + v * (pos.thick);
      pos2           = pos + shift - v * (pos.thick);
      segmentsArray.push_back(TSegment(pos1, pos2));
    }
    s += step;
  }

  flash.drawSegments(segmentsArray, false);
}

//=============================================================================

namespace {
double inline get_line_slope(double meter, double inmax, double linemax,
                             double outmax) {
  if (meter <= inmax)
    return meter / inmax;
  else if (meter <= inmax + linemax)
    return 1;
  else if (meter <= inmax + linemax + outmax)
    return (inmax + linemax - meter) / outmax + 1;
  else
    return 0;
}
}

//-----------------------------------------------------------------------------

TDottedLineStrokeStyle::TDottedLineStrokeStyle()
    : m_color(TPixel32::Black)
    , m_in(10.0)
    , m_line(50.0)
    , m_out(10.0)
    , m_blank(10.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TDottedLineStrokeStyle::clone() const {
  return new TDottedLineStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TDottedLineStrokeStyle::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TDottedLineStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TDottedLineStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 4);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TDottedLineStrokeStyle", "Fade In");
    break;
  case 1:
    value = QCoreApplication::translate("TDottedLineStrokeStyle", "Dash");
    break;
  case 2:
    value = QCoreApplication::translate("TDottedLineStrokeStyle", "Fade Out");
    break;
  case 3:
    value = QCoreApplication::translate("TDottedLineStrokeStyle", "Gap");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TDottedLineStrokeStyle::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < 4);
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
    min = 1.;
    max = 100.0;
    break;
  case 3:
    min = 0.0;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TDottedLineStrokeStyle::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < 4);
  double value = 0;
  switch (index) {
  case 0:
    value = m_in;
    break;
  case 1:
    value = m_line;
    break;
  case 2:
    value = m_out;
    break;
  case 3:
    value = m_blank;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TDottedLineStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    m_in = value;
    break;
  case 1:
    m_line = value;
    break;
  case 2:
    m_out = value;
    break;
  case 3:
    m_blank = value;
    break;
  }
  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TDottedLineStrokeStyle::computeData(Points &positions,
                                         const TStroke *stroke,
                                         const TColorFunction *cf) const {
  double length = stroke->getLength();

  double step     = 5.0;
  double linemax  = m_line;
  double inmax    = (m_in / 100);
  double outmax   = (m_out / 100);
  double blankmax = m_blank;
  double total    = 0;
  TRandom rnd;

  positions.clear();
  positions.reserve(tceil(length / step) + 1);

  TPointD oldPos1, oldPos2, oldPos3, oldPos4, pos1, pos2, pos3, pos4;
  // bool firstRing = true;
  double s            = 0;
  double meter        = 0;
  double center       = 0;
  double slopetmp     = 0;
  double minthickness = MINTHICK * sqrt(tglGetPixelSize2());
  double thickness    = 0;
  double line = 0, in = 0, out = 0, blank = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);

    if (pos.thick < MINTHICK)
      thickness = minthickness;
    else
      thickness = pos.thick;
    if (meter >= total) {
      meter = 0;

      line                        = linemax * (1 + rnd.getFloat()) * thickness;
      if (line > length - s) line = length - s;
      in                          = inmax * line;
      out                         = outmax * line;
      line                        = line - in - out;
      blank                       = blankmax * (1 + rnd.getFloat()) * thickness;
      if (in + out > length) {
        in   = rnd.getFloat() * (length / 2);
        out  = length - in;
        line = 0;
      }
      total = in + line + out + blank;
    } else if (meter > in + line + out + step) {
      s += step;
      meter += step;
      continue;
    }
    TPointD u = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u            = normalize(u);
    double slope = 0;
    slope        = get_line_slope(meter, in, line, out);
    slopetmp     = slope;
    TPointD v    = rotate90(u) * (thickness)*slope;
    if (pos.thick * slope < 1)
      center = 0.0;
    else
      center = 0.5;
    pos1     = pos + v;
    pos2     = pos + v * 0.5;
    pos3     = pos - v * 0.5;
    pos4     = pos - v;

    positions.push_back(pos1);
    positions.push_back(pos2);
    positions.push_back(pos3);
    positions.push_back(pos4);

    s += step;
    meter += step;
  }
}

//-----------------------------------------------------------------------------

void TDottedLineStrokeStyle::drawStroke(const TColorFunction *cf,
                                        Points &positions,
                                        const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  for (UINT i = 4; i < positions.size(); i += 4) {
    glBegin(GL_QUAD_STRIP);
    glColor4ub(color.r, color.g, color.b, 0);
    tglVertex(positions[i - 4]);
    tglVertex(positions[i]);
    glColor4ub(color.r, color.g, color.b, color.m);
    tglVertex(positions[i - 3]);
    tglVertex(positions[i + 1]);
    tglVertex(positions[i - 2]);
    tglVertex(positions[i + 2]);
    glColor4ub(color.r, color.g, color.b, 0);
    tglVertex(positions[i - 1]);
    tglVertex(positions[i + 3]);
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void TDottedLineStrokeStyle::drawStroke(TFlash &flash,
                                        const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  double step     = 5.0;
  double linemax  = m_line;
  double inmax    = m_in / 100;
  double outmax   = m_out / 100;
  double blankmax = m_blank;
  double total    = 0;
  TRandom rnd;

  TPixel32 color = m_color;
  TPixel32 color_transp(color.r, color.g, color.b, 0);

  TPointD oldPos1, oldPos2, oldPos3, oldPos4, pos1, pos2, pos3, pos4;
  bool firstRing      = true;
  double s            = 0;
  double meter        = 0;
  double center       = 0;
  double slopetmp     = 0;
  double minthickness = MINTHICK;
  double thickness    = 0;
  SFlashUtils sfu;
  flash.setThickness(0.0);
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    double line = 0, in = 0, out = 0, blank = 0;
    if (pos.thick < MINTHICK)
      thickness = minthickness;
    else
      thickness = pos.thick;

    if (meter >= total) {
      meter = 0;

      line                        = linemax * (1 + rnd.getFloat()) * thickness;
      if (line > length - s) line = length - s;
      in                          = inmax * line;
      out                         = outmax * line;
      line                        = line - in - out;
      blank                       = blankmax * (1 + rnd.getFloat()) * thickness;
      /*   --- OLD Version ---
line=linemax*(1+rnd.getFloat());
in=inmax*(1+rnd.getFloat())*pos.thick;
out=outmax*(1+rnd.getFloat())*pos.thick;
blank=blankmax*(1+rnd.getFloat())*pos.thick;
*/
      if (in + out > length) {
        in   = rnd.getFloat() * (length / 2);
        out  = length - in;
        line = 0;
      }
      total = in + line + out + blank;
    } else if (meter > in + line + out + step) {
      s += step;
      meter += step;
      continue;
    }
    TPointD u = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u            = normalize(u);
    double slope = 0;
    if (s <= length - out) {
      slope    = get_line_slope(meter, in, line, out);
      slopetmp = slope;
    } else
      slope   = (length - s) * (slopetmp / out);
    TPointD v = rotate90(u) * (pos.thick) * slope;
    if (pos.thick * slope < 1)
      center = 0.0;
    else
      center = 0.5;
    pos1     = pos + v;
    pos2     = pos + v * 0.5;
    pos3     = pos - v * 0.5;
    pos4     = pos - v;
    if (firstRing) {
      firstRing = false;
    } else {
      vector<TPointD> pv;
      pv.push_back(oldPos1);
      pv.push_back(pos1);
      pv.push_back(pos2);
      pv.push_back(oldPos2);
      sfu.drawGradedPolyline(flash, pv, color_transp, color);

      pv.clear();
      pv.push_back(oldPos2);
      pv.push_back(pos2);
      pv.push_back(pos3);
      pv.push_back(oldPos3);
      flash.setFillColor(color);
      flash.drawPolyline(pv);

      pv.clear();
      pv.push_back(oldPos3);
      pv.push_back(pos3);
      pv.push_back(pos4);
      pv.push_back(oldPos4);
      sfu.drawGradedPolyline(flash, pv, color, color_transp);
    }
    oldPos1 = pos1;
    oldPos2 = pos2;
    oldPos3 = pos3;
    oldPos4 = pos4;
    s += step;
    meter += step;
  }
}

//=============================================================================

TRopeStrokeStyle::TRopeStrokeStyle()
    : m_color(TPixel32(255, 135, 0)), m_bend(0.4) {}

//-----------------------------------------------------------------------------

TColorStyle *TRopeStrokeStyle::clone() const {
  return new TRopeStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TRopeStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TRopeStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TRopeStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);
  return QCoreApplication::translate("TRopeStrokeStyle", "Tilt");
}

//-----------------------------------------------------------------------------

void TRopeStrokeStyle::getParamRange(int index, double &min,
                                     double &max) const {
  assert(0 <= index && index < 1);
  min = -1.0;
  max = 1.0;
}

//-----------------------------------------------------------------------------

double TRopeStrokeStyle::getParamValue(TColorStyle::double_tag,
                                       int index) const {
  assert(0 <= index && index < 1);
  return m_bend;
}

//-----------------------------------------------------------------------------

void TRopeStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_bend = value;
  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TRopeStrokeStyle::computeData(Points &positions, const TStroke *stroke,
                                   const TColorFunction *cf) const {
  double length = stroke->getLength();
  // spessore della catena = spessore "medio" dello stroke
  double step = 10.0;
  double bend;
  double bump;
  double bump_max = step / 4;

  positions.clear();
  positions.reserve(tceil(length / step) + 1);

  TPointD oldPos1, oldPos2;
  bool firstRing = true;
  double s       = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u                          = normalize(u);
    bend                       = pos.thick * m_bend;
    bump                       = pos.thick * 0.3;
    if (bump >= bump_max) bump = bump_max;
    TPointD v                  = rotate90(u) * pos.thick;
    TPointD v1                 = v * 0.2;
    if (firstRing) {
      firstRing = false;
    } else {
      positions.push_back(pos + (bend + bump) * u + v - v1);
      positions.push_back(pos + (bend)*u + v);
      positions.push_back(oldPos1 + (bump)*u + v1);
      positions.push_back(oldPos1);
      positions.push_back(oldPos2);
      positions.push_back(oldPos2 + bump * u - v1);
      positions.push_back(pos + u * (-bend) - v);
      positions.push_back(pos + u * (bump - bend) - v + v1);
    }
    oldPos1 = pos + (bend + bump) * u + v - v1;
    oldPos2 = pos + u * (bump - bend) - v + v1;
    s += step;
  }
  positions.push_back(oldPos1);
  positions.push_back(oldPos2);
}

//-----------------------------------------------------------------------------

void TRopeStrokeStyle::drawStroke(const TColorFunction *cf, Points &positions,
                                  const TStroke *stroke) const {
  if (positions.size() <= 1) return;
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  //  GLuint  rope_id;

  TPixel32 blackcolor(TPixel32::Black);
  if (cf)
    blackcolor = (*(cf))(blackcolor);
  else
    blackcolor = blackcolor;

  // rope_id = glGenLists(1);

  static const int stride = sizeof(TPointD);
  glEnableClientState(GL_VERTEX_ARRAY);

  UINT i = 0;
  for (; i < positions.size() - 2; i += 8) {
    /*
glNewList(rope_id,GL_COMPILE);
      tglVertex(positions[i]);
      tglVertex(positions[i+1]);
      tglVertex(positions[i+2]);
      tglVertex(positions[i+3]);
      tglVertex(positions[i+4]);
      tglVertex(positions[i+5]);
      tglVertex(positions[i+6]);
      tglVertex(positions[i+7]);
glEndList();
*/
    tglColor(color);
    // glBegin(GL_POLYGON);

    glVertexPointer(2, GL_DOUBLE, stride, &positions[i]);
    glDrawArrays(GL_POLYGON, 0, 8);

    // glCallList(rope_id);
    // glEnd();

    tglColor(blackcolor);

    // glBegin(GL_LINE_STRIP);
    glVertexPointer(2, GL_DOUBLE, stride, &positions[i]);
    glDrawArrays(GL_LINE_STRIP, 0, 8);

    // glCallList(rope_id);
    // glEnd();
  }

  glDisableClientState(GL_VERTEX_ARRAY);

  glBegin(GL_LINE_STRIP);
  tglVertex(positions[i]);
  tglVertex(positions[i + 1]);
  glEnd();

  // glDeleteLists(rope_id,1);
}

//-----------------------------------------------------------------------------

void TRopeStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  // spessore della catena = spessore "medio" dello stroke

  double step = 10.0;
  double bend;
  double bump;
  double bump_max = step / 4;

  TPixel32 color = m_color;
  TPixel32 blackcolor(TPixel32::Black);

  TPointD oldPos1, oldPos2;
  bool firstRing = true;
  double s       = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u                          = normalize(u);
    bend                       = pos.thick * m_bend;
    bump                       = pos.thick * 0.3;
    if (bump >= bump_max) bump = bump_max;
    TPointD v                  = rotate90(u) * pos.thick;
    TPointD v1                 = v * 0.2;
    if (firstRing) {
      firstRing = false;
    } else {
      const int nbpp = 8;
      TPointD pp[nbpp];
      pp[0] = (pos + (bend + bump) * u + v - v1);
      pp[1] = (pos + (bend)*u + v);
      pp[2] = (oldPos1 + (bump)*u + v1);
      pp[3] = (oldPos1);
      pp[4] = (oldPos2);
      pp[5] = (oldPos2 + bump * u - v1);
      pp[6] = (pos + u * (-bend) - v);
      pp[7] = (pos + u * (bump - bend) - v + v1);

      vector<TPointD> pv;
      int i;
      for (i = 0; i < nbpp; i++) pv.push_back(pp[i]);

      flash.setFillColor(color);
      flash.drawPolyline(pv);

      vector<TSegment> sv;
      for (i = 0; i < (nbpp - 1); i++) sv.push_back(TSegment(pp[i], pp[i + 1]));
      flash.setThickness(1.0);
      flash.setLineColor(blackcolor);
      flash.drawSegments(sv, false);
    }
    oldPos1 = pos + (bend + bump) * u + v - v1;
    oldPos2 = pos + u * (bump - bend) - v + v1;
    s += step;
  }

  vector<TSegment> sv;
  sv.push_back(TSegment(oldPos1, oldPos2));
  flash.setLineColor(blackcolor);
  flash.drawSegments(sv, false);
}

//=============================================================================

TCrystallizeStrokeStyle::TCrystallizeStrokeStyle()
    : m_color(TPixel32(255, 150, 150, 255)), m_period(10.0), m_opacity(0.5) {}

//-----------------------------------------------------------------------------

TColorStyle *TCrystallizeStrokeStyle::clone() const {
  return new TCrystallizeStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TCrystallizeStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TCrystallizeStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TCrystallizeStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 2);
  return index == 0
             ? QCoreApplication::translate("TCrystallizeStrokeStyle", "Crease")
             : QCoreApplication::translate("TCrystallizeStrokeStyle",
                                           "Opacity");
}

//-----------------------------------------------------------------------------

void TCrystallizeStrokeStyle::getParamRange(int index, double &min,
                                            double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = 1.0;
    max = 100.0;
  } else {
    min = 0.0;
    max = 1.0;
  }
}

//-----------------------------------------------------------------------------

double TCrystallizeStrokeStyle::getParamValue(TColorStyle::double_tag,
                                              int index) const {
  assert(0 <= index && index < 2);
  return index == 0 ? m_period : m_opacity;
}

//-----------------------------------------------------------------------------

void TCrystallizeStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);
  if (index == 0)
    m_period = value;
  else
    m_opacity = value;

  // updateVersionNumber(); non serve perche' i parametri vengono sfrutttati
  // direttamente nella draw
}

//-----------------------------------------------------------------------------

void TCrystallizeStrokeStyle::computeData(Points &positions,
                                          const TStroke *stroke,
                                          const TColorFunction *cf) const {
  double length = stroke->getLength();
  double step   = 10.0;
  TRandom rnd;
  positions.clear();
  positions.reserve(tceil((length + 1) / step));
  double s = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u) * (pos.thick / 2);
    positions.push_back(pos + v * (1 + rnd.getFloat()) +
                        u * 2 * rnd.getFloat());
    positions.push_back(pos - v * (1 + rnd.getFloat()) -
                        u * 2 * rnd.getFloat());
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TCrystallizeStrokeStyle::drawStroke(const TColorFunction *cf,
                                         Points &positions,
                                         const TStroke *stroke) const {
  // double length = stroke->getLength();
  double step    = 10.0;
  double period  = (101 - m_period) * step;
  double counter = 0;
  double opacity = m_opacity;
  TRandom rnd;

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  TPixelD dcolor = toPixelD(color);
  // double s = 0;

  glBegin(GL_QUAD_STRIP);
  for (int i = 0; i < (int)positions.size() / 2; i++) {
    if (counter > period) counter = 0;
    glColor4d(dcolor.r, dcolor.g, dcolor.b,
              (opacity + (counter / period) * rnd.getFloat()) * dcolor.m);
    tglVertex(positions[i * 2]);
    tglVertex(positions[i * 2 + 1]);
    counter += step;
  }
  glEnd();
  counter = 0;
  glColor4d(dcolor.r, dcolor.g, dcolor.b, dcolor.m);
  for (int j = 1; j < (int)positions.size() / 2; j++) {
    glBegin(GL_LINES);
    tglVertex(positions[(j - 1) * 2]);
    tglVertex(positions[j * 2]);
    glEnd();
    glBegin(GL_LINES);
    tglVertex(positions[(j - 1) * 2 + 1]);
    tglVertex(positions[j * 2 + 1]);
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void TCrystallizeStrokeStyle::drawStroke(TFlash &flash,
                                         const TStroke *stroke) const {
  double length  = stroke->getLength();
  double step    = 10.0;
  double period  = m_period * step;
  double counter = 0;
  double opacity = m_opacity;
  TRandom rnd;
  //  const double flashGrad=16384.0;

  TPixel32 color = m_color;

  TPixelD dcolor = toPixelD(color);
  vector<TPointD> points1;
  vector<TPointD> points2;
  double s = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u) * (pos.thick / 2);
    points1.push_back(pos + v * (1 + rnd.getFloat()) + u * 2 * rnd.getFloat());
    points2.push_back(pos - v * (1 + rnd.getFloat()) - u * 2 * rnd.getFloat());
    s += step;
  }

  // Just for the polygon grading function
  //  SRegionDrawInFlash rdf;
  SFlashUtils sfu;
  TPixelD ldcolorPrev = dcolor;
  ldcolorPrev.m       = (opacity + (0.0 / period) * rnd.getFloat()) * dcolor.m;
  flash.setThickness(0.0);
  for (int i = 0; i < (int)(points1.size() - 1); i++) {
    if (counter > period) counter = 0;
    TPixelD ldcolor               = dcolor;
    ldcolor.m = (opacity + (counter / period) * rnd.getFloat()) * dcolor.m;

    TPixel32 lcolorPrev;
    lcolorPrev = toPixel32(ldcolorPrev);
    TPixel32 lcolor;
    lcolor = toPixel32(ldcolor);

    vector<TPointD> tpv;
    tpv.push_back(points1[i]);
    tpv.push_back(points2[i]);
    tpv.push_back(points2[i + 1]);
    tpv.push_back(points1[i + 1]);

    // Solid Color version
    //	flash.setFillColor(blend(lcolorPrev,lcolor,0.5));
    //    flash.drawPolyline(tpv);
    sfu.drawGradedPolyline(flash, tpv, lcolorPrev, lcolor);

    counter += step;
    ldcolorPrev = ldcolor;
  }

  counter = 0;
  vector<TSegment> tsv1, tsv2;
  for (int j = 1; j < (int)points1.size(); j++) {
    tsv1.push_back(TSegment(points1[j - 1], points1[j]));
    tsv2.push_back(TSegment(points2[j - 1], points2[j]));
  }
  flash.setThickness(1.0);
  flash.setLineColor(color);
  flash.drawSegments(tsv1, false);
  flash.drawSegments(tsv2, false);
}

//=============================================================================

namespace {

class Stripe {
public:
  TPointD oldpos1;
  TPointD oldpos2;
  TPointD pos1;
  TPointD pos2;
  int phase;
  TPixel32 color;
  Stripe();
  void drawpolygon();
  void drawpolygon(TFlash &flash);
  void drawlines(TPixel32 blackcolor);
  void addToSegment(vector<TSegment> *sv, vector<TSegment> &scontour,
                    TPixel32 *colors);
};

Stripe::Stripe() {
  oldpos1 = TPointD(0, 0);
  oldpos2 = TPointD(0, 0);
  pos1    = TPointD(0, 0);
  pos2    = TPointD(0, 0);
  color   = TPixel32(0, 0, 0);
  phase   = 0;
}

void Stripe::drawpolygon() {
  tglColor(color);
  glBegin(GL_POLYGON);
  tglVertex(oldpos1);
  tglVertex(pos1);
  tglVertex(pos2);
  tglVertex(oldpos2);
  glEnd();
}

void Stripe::drawpolygon(TFlash &flash) {
  vector<TPointD> pv;
  pv.push_back(oldpos1);
  pv.push_back(oldpos2);
  pv.push_back(pos2);
  pv.push_back(pos1);
  flash.setThickness(0);
  flash.setFillColor(color);
  flash.drawPolyline(pv);

  // Draws the black contour
  flash.setThickness(0.5);
  flash.setLineColor(TPixel32::Black);
  vector<TSegment> sv;
  sv.push_back(TSegment(oldpos1, pos1));
  sv.push_back(TSegment(oldpos2, pos2));
  flash.drawSegments(sv, false);

  // It is better, but the flash.drawLine() is missing in my SDK
  /*	flash.setThickness(0.5);
flash.setLineColor(TPixel32::Black);
  flash.drawLine(oldpos1,pos1);
  flash.drawLine(oldpos2,pos2);
*/
}

void Stripe::addToSegment(vector<TSegment> *sv, vector<TSegment> &scontour,
                          TPixel32 *colors) {
  TPointD p0 = (oldpos1 + oldpos2) * 0.5;
  TPointD p1 = (pos1 + pos2) * 0.5;
  //	TPointD p0=oldpos1;
  //	TPointD p1=pos1;
  if (color == colors[0]) sv[0].push_back(TSegment(p0, p1));
  if (color == colors[1]) sv[1].push_back(TSegment(p0, p1));
  if (color == colors[2]) sv[2].push_back(TSegment(p0, p1));

  scontour.push_back(TSegment(oldpos1, pos1));
  scontour.push_back(TSegment(oldpos2, pos2));
}

void Stripe::drawlines(TPixel32 blackcolor) {
  tglColor(blackcolor);
  glBegin(GL_LINE_STRIP);
  tglVertex(oldpos1);
  tglVertex(pos1);
  glEnd();
  glBegin(GL_LINE_STRIP);
  tglVertex(pos2);
  tglVertex(oldpos2);
  glEnd();
}
}

//-----------------------------------------------------------------------------

TBraidStrokeStyle::TBraidStrokeStyle()
    : m_period(80.0)

{
  m_colors[0] = TPixel32::Red;
  m_colors[1] = TPixel32::Green;
  m_colors[2] = TPixel32::Blue;
}

//-----------------------------------------------------------------------------

TColorStyle *TBraidStrokeStyle::clone() const {
  return new TBraidStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TBraidStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TBraidStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TBraidStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TBraidStrokeStyle", "Twirl");
}

//-----------------------------------------------------------------------------

void TBraidStrokeStyle::getParamRange(int index, double &min,
                                      double &max) const {
  assert(0 <= index && index < 1);
  min = 1.0;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TBraidStrokeStyle::getParamValue(TColorStyle::double_tag,
                                        int index) const {
  assert(0 <= index && index < 1);
  return m_period;
}

//-----------------------------------------------------------------------------

void TBraidStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_period = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

TPixel32 TBraidStrokeStyle::getColorParamValue(int index) const {
  TPixel32 tmp;
  switch (index) {
  case (0):
    tmp = m_colors[0];
    break;
  case (1):
    tmp = m_colors[1];
    break;
  case (2):
    tmp = m_colors[2];
    break;
  }
  return tmp;
}

//-----------------------------------------------------------------------------

void TBraidStrokeStyle::setColorParamValue(int index, const TPixel32 &color) {
  switch (index) {
  case (0):
    m_colors[0] = color;
    break;
  case (1):
    m_colors[1] = color;
    break;
  case (2):
    m_colors[2] = color;
    break;
  }
}

//-----------------------------------------------------------------------------

void TBraidStrokeStyle::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 112)
    throw TException("Braid stroke style: unknown obsolete format");

  is >> m_colors[0] >> m_period;

  m_period /= 10.0;
  m_colors[0] = TPixel32::Red;
  m_colors[1] = TPixel32::Green;
  m_colors[2] = TPixel32::Blue;
}

//-----------------------------------------------------------------------------

void TBraidStrokeStyle::drawStroke(const TColorFunction *cf,
                                   const TStroke *stroke) const {
  double length                = stroke->getLength();
  const int ntick              = 81;
  const double stripethickness = 0.3;
  int period                   = (int)(101 - m_period) * 20;
  double step                  = period / (double)ntick;
  double freq                  = M_2PI / ntick;
  int swapcount                = 0;
  int count                    = 0;
  bool firstRing               = true;
  double s                     = 0;
  double swap;
  vector<Stripe> braid;
  vector<double> ssin;
  int k = 0;
  TPixel32 colors[3];
  for (k = 0; k < 3; k++) {
    if (cf)
      colors[k] = (*(cf))(m_colors[k]);
    else
      colors[k] = m_colors[k];
  }
  TPixel32 blackcolor = TPixel32::Black;
  if (cf) blackcolor  = (*(cf))(blackcolor);

  for (k = 0; k < 3; k++) {
    Stripe tmp;
    tmp.phase = (ntick * k) / 3;
    tmp.color = colors[k];
    braid.push_back(tmp);
  }

  for (int z = 0; z < ntick; z++) {
    double tmpsin = sin(z * freq);
    ssin.push_back(tmpsin);
  }
  while (s <= length) {
    count++;
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u          = normalize(u);
    TPointD v  = rotate90(u) * pos.thick;
    TPointD v1 = v * stripethickness;
    v          = v * 0.5;
    // int modper=(int)s%(int)period;
    if (firstRing) {
      firstRing = false;
      swap      = 0;
      for (int j = 0; j < (int)braid.size(); j++) {
        int tmp          = (count + braid[j].phase) % ntick;
        braid[j].oldpos1 = pos + v * ssin[tmp];
        braid[j].oldpos2 = pos + v * ssin[tmp] + v1;
      }
    } else {
      for (int i = 0; i < (int)braid.size(); i++) {
        int tmp       = (count + braid[i].phase) % ntick;
        braid[i].pos1 = pos + v * ssin[tmp];
        braid[i].pos2 = pos + v * ssin[tmp] + v1;
        braid[i].drawpolygon();
        braid[i].drawlines(blackcolor);
        braid[i].oldpos1 = pos + v * ssin[tmp];
        braid[i].oldpos2 = pos + v * ssin[tmp] + v1;
      }
    }
    s += step;
    swap += step;
    if (swap > (period / 3.0)) {
      swapcount++;
      std::swap(braid[0], braid[1 + (swapcount & 1)]);
      swap -= period / 3.0;
    }
  }
}

//-----------------------------------------------------------------------------

void TBraidStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  double length                = stroke->getLength();
  const int ntick              = 162;
  const double stripethickness = 0.3;
  int period                   = (int)(101 - m_period) * 20;
  double step                  = period / (double)ntick;
  double freq                  = M_2PI / ntick;
  int swapcount                = 0;
  int count                    = 0;
  bool firstRing               = true;
  double s                     = 0;
  double swap;
  vector<Stripe> braid;
  vector<double> ssin;
  int k = 0;
  TPixel32 colors[3];

  for (k = 0; k < 3; k++) colors[k] = m_colors[k];

  TPixel32 blackcolor = TPixel32::Black;

  for (k = 0; k < 3; k++) {
    Stripe tmp;
    tmp.phase = (ntick * k) / 3;
    tmp.color = colors[k];
    braid.push_back(tmp);
  }

  for (int z = 0; z < ntick; z++) {
    double tmpsin = sin(z * freq);
    ssin.push_back(tmpsin);
  }

  while (s <= length) {
    count++;
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u          = normalize(u);
    TPointD v  = rotate90(u) * pos.thick;
    TPointD v1 = v * stripethickness;
    v          = v * 0.5;
    // int modper=(int)s%(int)period;
    if (firstRing) {
      firstRing = false;
      swap      = 0;
      for (int j = 0; j < (int)braid.size(); j++) {
        int tmp          = (count + braid[j].phase) % ntick;
        braid[j].oldpos1 = pos + v * ssin[tmp];
        braid[j].oldpos2 = pos + v * ssin[tmp] + v1;
      }
    } else {
      for (int i = 0; i < (int)braid.size(); i++) {
        int tmp       = (count + braid[i].phase) % ntick;
        braid[i].pos1 = pos + v * ssin[tmp];
        braid[i].pos2 = pos + v * ssin[tmp] + v1;

        braid[i].drawpolygon(flash);

        braid[i].oldpos1 = pos + v * ssin[tmp];
        braid[i].oldpos2 = pos + v * ssin[tmp] + v1;
      }
    }
    s += step;
    swap += step;
    if (swap > (period / 3.0)) {
      swapcount++;
      std::swap(braid[0], braid[1 + (swapcount & 1)]);
      swap -= period / 3.0;
    }
  }
}

//=============================================================================

TSketchStrokeStyle::TSketchStrokeStyle()
    : m_color(TPixel32(100, 100, 150, 127)), m_density(0.4) {}

//-----------------------------------------------------------------------------

TColorStyle *TSketchStrokeStyle::clone() const {
  return new TSketchStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TSketchStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TSketchStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TSketchStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TSketchStrokeStyle", "Density");
}

//-----------------------------------------------------------------------------

void TSketchStrokeStyle::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 1);
  min = 0.0;
  max = 1.0;
}

//-----------------------------------------------------------------------------

double TSketchStrokeStyle::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 1);
  return m_density;
}

//-----------------------------------------------------------------------------

void TSketchStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_density = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TSketchStrokeStyle::drawStroke(const TColorFunction *cf,
                                    const TStroke *stroke) const {
  double length = stroke->getLength();
  if (length <= 0) return;

  int count = (int)(length * m_density);

  double maxDw = std::min(1.0, 20.0 / length);
  double minDw = 1.0 / length;

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  tglColor(color);

  TRandom rnd;

  for (int i = 0; i < count; i++) {
    double r    = rnd.getFloat();
    double dw   = (1 - r) * minDw + r * maxDw;
    double wmin = dw, wmax = 1 - dw;
    if (wmin >= wmax) continue;
    r        = rnd.getFloat();
    double w = (1 - r) * wmin + r * wmax;

    double w0 = w - dw;
    double w1 = w + dw;

    TThickPoint p0 = stroke->getThickPoint(w0);
    TThickPoint p1 = stroke->getThickPoint(w1);
    double d01     = tdistance(p0, p1);
    if (d01 == 0) continue;

    int count = (int)(d01);

    TPointD v0 = stroke->getSpeed(w0);
    TPointD v1 = stroke->getSpeed(w1);

    if (norm2(v0) == 0 || norm2(v1) == 0)
      continue;  // non dovrebbe succedere mai, ma....
    v0 = rotate90(normalize(v0));
    v1 = rotate90(normalize(v1));

    double delta  = 0.5 * (rnd.getFloat() - 0.5) * (p0.thick + p1.thick);
    double d      = 0.1 * d01;
    double delta0 = delta - d;
    double delta1 = delta + d;

    glBegin(GL_LINE_STRIP);
    tglVertex(p0 + v0 * delta0);
    for (int j = 1; j < count; j++) {
      double t  = j / (double)count;
      w         = (1 - t) * w0 + t * w1;
      TPointD v = rotate90(normalize(stroke->getSpeed(w)));
      assert(0 <= w && w <= 1);
      TPointD p      = stroke->getPoint(w);
      double delta_t = (1 - t) * delta0 + t * delta1;
      tglVertex(p + v * delta_t);
    }
    tglVertex(p1 + v1 * delta1);
    glEnd();
  }
  glColor4d(0, 0, 0, 1);
}

//-----------------------------------------------------------------------------

void TSketchStrokeStyle::drawStroke(TFlash &flash,
                                    const TStroke *stroke) const {
  double length = stroke->getLength();
  if (length <= 0) return;
  vector<TQuadratic> quadsArray;

  int count = (int)(length * m_density);

  double maxDw = std::min(1.0, 20.0 / length);
  double minDw = 1.0 / length;
  TPixel color(m_color.r, m_color.g, m_color.b, m_color.m);
  flash.setLineColor(color);

  TRandom rnd;

  for (int i = 0; i < count; i++) {
    double r    = rnd.getFloat();
    double dw   = (1 - r) * minDw + r * maxDw;
    double wmin = dw, wmax = 1 - dw;
    if (wmin >= wmax) continue;
    r        = rnd.getFloat();
    double w = (1 - r) * wmin + r * wmax;

    double w0 = w - dw;
    double w1 = w + dw;

    TThickPoint p0 = stroke->getThickPoint(w0);
    TThickPoint p1 = stroke->getThickPoint(w1);
    double d01     = tdistance(p0, p1);
    if (d01 == 0) continue;

    // int count = (int)(d01);

    TPointD v0 = stroke->getSpeed(w0);
    TPointD v1 = stroke->getSpeed(w1);

    if (norm2(v0) == 0 || norm2(v1) == 0)
      continue;  // non dovrebbe succedere mai, ma....
    v0 = rotate90(normalize(v0));
    v1 = rotate90(normalize(v1));

    double delta  = 0.5 * (rnd.getFloat() - 0.5) * (p0.thick + p1.thick);
    double d      = 0.1 * d01;
    double delta0 = delta - d;
    double delta1 = delta + d;

    TPointD v      = rotate90(normalize(stroke->getSpeed(0.5 * (w0 + w1))));
    TPointD p      = stroke->getPoint(0.5 * (w0 + w1));
    double delta_t = 0.5 * (delta0 + delta1);
    // quadsArray.push_back(TSegment(p0 + v0*delta0, p1 + v1*delta1));
    TPointD pp0 = p0 + v0 * delta0;
    TPointD pp2 = p1 + v1 * delta1;
    TPointD pp1 = 2 * (p + v * delta_t) -
                  0.5 * (pp0 + pp2);  // punto p1 ottenuto imponendo che la quad
                                      // passi per il punto p in t=.5
    quadsArray.push_back(TQuadratic(pp0, pp1, pp2));
  }
  flash.drawquads(quadsArray);
}

//=============================================================================

TBubbleStrokeStyle::TBubbleStrokeStyle()
    : m_color0(TPixel32::Red), m_color1(TPixel32::Green) {}

//-----------------------------------------------------------------------------

TColorStyle *TBubbleStrokeStyle::clone() const {
  return new TBubbleStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

void TBubbleStrokeStyle::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 114)
    throw TException("Bubble  stroke style: unknown obsolete format");

  m_color0 = TPixel32::Red;
  m_color1 = TPixel32::Green;
}

//-----------------------------------------------------------------------------

void TBubbleStrokeStyle::drawStroke(const TColorFunction *cf,
                                    const TStroke *stroke) const {
  double length = stroke->getLength();
  if (length <= 0) return;

  TRandom rnd(0);

  TPixel32 color0, color1;
  if (cf) {
    color0 = (*(cf))(m_color0);
    color1 = (*(cf))(m_color1);
  } else {
    color0 = m_color0;
    color1 = m_color1;
  }

  double minthickness = MINTHICK * sqrt(tglGetPixelSize2());
  double thickness    = 0;
  for (double s = 0; s < length; s += 5) {
    TPointD p = stroke->getPointAtLength(s);
    double w  = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    int toff        = rnd.getInt(0, 999);
    int t           = (m_currentFrame + toff) % 1000;
    TRandom rnd2(t >> 2);
    p += 2 * TPointD(-0.5 + rnd2.getFloat(), -0.5 + rnd2.getFloat());
    if (pos.thick < MINTHICK)
      thickness = minthickness;
    else
      thickness = pos.thick;
    tglColor(blend(color0, color1, rnd.getFloat()));
    double radius = (t & ((int)(thickness)));
    tglDrawCircle(p, radius);
  }
}

//-----------------------------------------------------------------------------

void TBubbleStrokeStyle::drawStroke(TFlash &flash,
                                    const TStroke *stroke) const {
  double length = stroke->getLength();
  if (length <= 0) return;

  TRandom rnd(0);
  static int count = 0;
  count++;

  TPixel32 color0 = m_color0;
  TPixel32 color1 = m_color1;

  for (double s = 0; s < length; s += 5) {
    TPointD p = stroke->getPointAtLength(s);
    double w  = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    int toff        = rnd.getInt(0, 999);
    int t           = (count + toff) % 1000;
    TRandom rnd2(t >> 2);
    p += 2 * TPointD(-0.5 + rnd2.getFloat(), -0.5 + rnd2.getFloat());
    double r = (t & ((int)(pos.thick)));
    flash.setThickness(0.5);
    flash.setLineColor(blend(color0, color1, rnd.getFloat()));
    flash.setFillColor(TPixel32(0, 0, 0, 0));
    flash.drawEllipse(p, r, r);
  }
}

//=============================================================================

TTissueStrokeStyle::TTissueStrokeStyle()
    : m_color(TPixel32::Black), m_density(3.0), m_border(1.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TTissueStrokeStyle::clone() const {
  return new TTissueStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TTissueStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TTissueStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TTissueStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 2);
  return index == 0
             ? QCoreApplication::translate("TTissueStrokeStyle", "Density")
             : QCoreApplication::translate("TTissueStrokeStyle", "Border Size");
}

//-----------------------------------------------------------------------------

void TTissueStrokeStyle::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = 2.0;
    max = 10.0;
  } else {
    min = 0.0;
    max = 1.0;
  }
}

//-----------------------------------------------------------------------------

double TTissueStrokeStyle::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 2);
  return index == 0 ? m_density : m_border;
}

//-----------------------------------------------------------------------------

void TTissueStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);
  if (index == 0)
    m_density = value;
  else
    m_border = value;

  updateVersionNumber();
}
//-----------------------------------------------------------------------------

void TTissueStrokeStyle::computeData(PointMatrix &data, const TStroke *stroke,
                                     const TColorFunction *cf) const {
  data.clear();
  double length = stroke->getLength();
  double step   = 5.0;
  double border = m_border;
  TPointD pos1, oldPos1;
  TRandom rnd;
  double increment = 0.0;
  int intensity    = (int)m_density + 2;
  vector<TPointD> points;
  vector<TPointD> oldpoints;
  double s       = 0;
  bool firstRing = true;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u);
    increment = (2 * pos.thick) / (intensity - 1);
    for (int i = 1; i < intensity - 1; i++) {
      pos1 = pos + v * (-pos.thick + i * increment);
      points.push_back(pos1);
    }
    if (firstRing) {
      firstRing = false;
    } else {
      Points tmpPoints1;
      tmpPoints1.clear();
      tmpPoints1.reserve(intensity);

      for (int i = 1; i < intensity - 1; i++) {
        pos1    = points[i - 1];
        oldPos1 = oldpoints[i - 1];
        tmpPoints1.push_back(oldPos1);
        tmpPoints1.push_back(pos1);
      }
      data.push_back(tmpPoints1);
      if (increment > 1) {
        int count = tceil(step / increment + 1);
        Points tmpPoints2;
        tmpPoints2.clear();
        tmpPoints2.reserve(count);
        double startpoint = -step - increment / 2.0;
        for (int j = 1; j < step / increment + 1; j++) {
          tmpPoints2.push_back(points[0] -
                               v * border * increment * rnd.getFloat() +
                               u * (startpoint + j * (increment)));
          tmpPoints2.push_back(points[intensity - 3] +
                               v * border * increment * rnd.getFloat() +
                               u * (startpoint + j * (increment)));
        }
        data.push_back(tmpPoints2);
      }
    }
    oldpoints = points;
    points.clear();
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TTissueStrokeStyle::drawStroke(const TColorFunction *cf, PointMatrix &data,
                                    const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  tglColor(color);

  PointMatrix::iterator it1 = data.begin();
  for (; it1 != data.end(); ++it1) {
    glBegin(GL_LINES);
    Points::iterator it2 = (*it1).begin();
    for (; it2 != (*it1).end(); ++it2) {
      tglVertex(*it2);
    }
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void TTissueStrokeStyle::drawStroke(TFlash &flash,
                                    const TStroke *stroke) const {
  double length = stroke->getLength();
  double step   = 5.0;
  double border = m_border;
  TPointD pos1, oldPos1;
  TRandom rnd;
  double increment = 0.0;
  int intensity    = (int)m_density + 2;
  vector<TPointD> points;
  vector<TPointD> oldpoints;
  TPixel32 color = m_color;

  flash.setLineColor(m_color);
  flash.setThickness(1.0);
  double s       = 0;
  bool firstRing = true;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u         = normalize(u);
    TPointD v = rotate90(u);
    increment = (2 * pos.thick) / (intensity - 1);
    for (int i = 1; i < intensity - 1; i++) {
      pos1 = pos + v * (-pos.thick + i * increment);
      points.push_back(pos1);
    }
    if (firstRing) {
      firstRing = false;
    } else {
      flash.setThickness(1.5);
      vector<TSegment> sv;
      for (int i = 1; i < intensity - 1; i++) {
        pos1    = points[i - 1];
        oldPos1 = oldpoints[i - 1];
        sv.push_back(TSegment(oldPos1, pos1));
      }
      flash.drawSegments(sv, false);

      if (increment > 1) {
        sv.clear();
        double startpoint = -step - increment / 2.0;
        for (int j = 1; j < step / increment + 1; j++) {
          TPointD p0 = points[0] - v * border * increment * rnd.getFloat() +
                       u * (startpoint + j * (increment));
          TPointD p1 = points[intensity - 3] +
                       v * border * increment * rnd.getFloat() +
                       u * (startpoint + j * (increment));
          //			vector<TSegment> sv;
          sv.push_back(TSegment(p0, p1));
        }
        flash.drawSegments(sv, false);
      }
    }
    oldpoints = points;
    points.clear();
    s += step;
  }
}

//=============================================================================

TBiColorStrokeStyle::TBiColorStrokeStyle()
    : m_color0(TPixel32::Red), m_color1(TPixel32::Black) {}

//-----------------------------------------------------------------------------

TColorStyle *TBiColorStrokeStyle::clone() const {
  return new TBiColorStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

void TBiColorStrokeStyle::loadData(TInputStreamInterface &is) {
  is >> m_color0 >> m_color1 >> m_parameter;
}

//-----------------------------------------------------------------------------

void TBiColorStrokeStyle::saveData(TOutputStreamInterface &os) const {
  os << m_color0 << m_color1 << m_parameter;
}

//-----------------------------------------------------------------------------

void TBiColorStrokeStyle::drawRegion(const TColorFunction *cf,
                                     const bool antiAliasing,
                                     TRegionOutline &boundary) const {}

//-----------------------------------------------------------------------------

void TBiColorStrokeStyle::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 115 && ids != 119)
    throw TException("Bicolor stroke style: unknown obsolete format");

  is >> m_color0 >> m_parameter;

  m_color1 = TPixel32::Black;
}

//-----------------------------------------------------------------------------

void TBiColorStrokeStyle::drawStroke(const TColorFunction *cf,
                                     TStrokeOutline *outline,
                                     const TStroke *stroke) const {
  TPixel32 color0 = m_color0;
  TPixel32 color1 = m_color1;

  if (cf) {
    color0 = (*(cf))(m_color0);
    color1 = (*(cf))(m_color1);
  } else {
    color0 = m_color0;
    color1 = m_color1;
  }

  UINT i;

  const std::vector<TOutlinePoint> &v = outline->getArray();

  if (v.empty()) return;

  // outline with antialiasing
  glBegin(GL_LINE_STRIP);
  tglColor(color0);
  for (i = 0; i < v.size(); i += 2) glVertex2dv(&v[i].x);
  glEnd();

  glBegin(GL_LINE_STRIP);
  tglColor(color1);
  for (i = 1; i < v.size(); i += 2) glVertex2dv(&v[i].x);
  glEnd();

  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < v.size(); i += 2) {
    tglColor(color0);
    glVertex2dv(&v[i].x);
    tglColor(color1);
    glVertex2dv(&v[i + 1].x);
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void TBiColorStrokeStyle::drawStroke(TFlash &flash,
                                     const TStroke *stroke) const {
  TOutlineUtil::OutlineParameter param;
  param.m_lengthStep = std::max(10.0, m_parameter);
  TStrokeOutline outline;
  TOutlineStyle::computeOutline(stroke, outline, param);
  const std::vector<TOutlinePoint> &v = outline.getArray();
  if (v.empty()) return;

  TPixel32 color0 = m_color0;
  TPixel32 color1 = m_color1;
  flash.setThickness(0.0);
  // Just for the polygon grading function
  SFlashUtils sfu;
  for (UINT i = 0; i < (v.size() - 3); i += 2) {
    vector<TPointD> plv;
    plv.push_back(TPointD(v[i].x, v[i].y));
    plv.push_back(TPointD(v[i + 2].x, v[i + 2].y));
    plv.push_back(TPointD(v[i + 3].x, v[i + 3].y));
    plv.push_back(TPointD(v[i + 1].x, v[i + 1].y));
    //	 flash.setFillColor(blend(color0,color1,0.5));
    //	 flash.drawPolyline(plv);
    // graded multipolygons
    sfu.drawGradedPolyline(flash, plv, color0, color1);
  }
}

//=============================================================================

TNormal2StrokeStyle::TNormal2StrokeStyle()
    : m_color(TPixel32::Yellow)
    , m_lightx(45.0)
    , m_lighty(200.0)
    , m_shininess(50.0)
    , m_metal(0.5)
    , m_bend(1.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TNormal2StrokeStyle::clone() const {
  return new TNormal2StrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TNormal2StrokeStyle::getParamCount() const { return 5; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TNormal2StrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TNormal2StrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 5);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TNormal2StrokeStyle", "Light X Pos");
    break;
  case 1:
    value = QCoreApplication::translate("TNormal2StrokeStyle", "Light Y Pos");
    break;
  case 2:
    value = QCoreApplication::translate("TNormal2StrokeStyle", "Shininess");
    break;
  case 3:
    value = QCoreApplication::translate("TNormal2StrokeStyle", "Plastic");
    break;
  case 4:
    value = QCoreApplication::translate("TNormal2StrokeStyle", "Bump");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TNormal2StrokeStyle::getParamRange(int index, double &min,
                                        double &max) const {
  assert(0 <= index && index < 5);
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
    min = 0.1;
    max = 128.0;
    break;
  case 3:
    min = 0.0;
    max = 1.0;
    break;
  case 4:
    min = 0.0;
    max = 1.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TNormal2StrokeStyle::getParamValue(TColorStyle::double_tag,
                                          int index) const {
  assert(0 <= index && index < 5);
  double value;
  switch (index) {
  case 0:
    value = m_lightx;
    break;
  case 1:
    value = m_lighty;
    break;
  case 2:
    value = m_shininess;
    break;
  case 3:
    value = m_metal;
    break;
  case 4:
    value = m_bend;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TNormal2StrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 5);
  switch (index) {
  case 0:
    m_lightx = value;
    break;
  case 1:
    m_lighty = value;
    break;
  case 2:
    m_shininess = value;
    break;
  case 3:
    m_metal = value;
    break;
  case 4:
    m_bend = value;
    break;
  }

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TNormal2StrokeStyle::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 121)
    throw TException("Normal  stroke style: unknown obsolete format");

  is >> m_color >> m_lightx >> m_lighty >> m_shininess >> m_metal;
  m_bend = 1.0;
}

//-----------------------------------------------------------------------------

void TNormal2StrokeStyle::drawRegion(const TColorFunction *cf,
                                     const bool antiAliasing,
                                     TRegionOutline &boundary) const {}

//-----------------------------------------------------------------------------

void TNormal2StrokeStyle::drawStroke(const TColorFunction *cf,
                                     TStrokeOutline *outline,
                                     const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  TPixelD dcolor;
  dcolor = toPixelD(color);
  UINT i;
  double bend                         = 2 * m_bend;
  const std::vector<TOutlinePoint> &v = outline->getArray();
  if (v.empty()) return;
  vector<T3DPointD> normal;

  GLfloat light_position[] = {(float)(m_lightx), (float)(m_lighty), 100.0, 0.0};
  glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_NORMALIZE);
  GLfloat mat_ambient[] = {(float)dcolor.r, (float)dcolor.g, (float)dcolor.b,
                           1.0};
  GLfloat mat_specular[] = {(float)(m_metal * (1 - dcolor.r) + dcolor.r),
                            (float)(m_metal * (1 - dcolor.g) + dcolor.g),
                            (float)(m_metal * (1 - dcolor.b) + dcolor.b), 1.0};
  GLfloat mat_shininess[] = {(float)m_shininess};

  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_ambient);

  // glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

  // outline with antialiasing
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < v.size(); i += 2) {
    T3DPointD pointa(v[i].x, v[i].y, 0);
    T3DPointD pointb(v[i + 1].x, v[i + 1].y, 0);
    T3DPointD d           = pointb - pointa;
    if (norm2(d) > 0.0) d = normalize(d);
    normal.push_back(d);
    T3DPointD pointaNormal = T3DPointD(0, 0, 1) - bend * d;
    tglNormal(pointaNormal);
    tglVertex(pointa);
  }
  glEnd();

  int normalcounter = 0;
  glBegin(GL_LINE_STRIP);
  for (i = 1; i < v.size(); i += 2) {
    T3DPointD pointa(v[i].x, v[i].y, 0);
    T3DPointD pointaNormal =
        T3DPointD(0, 0, 1) + bend * normal[normalcounter++];
    tglNormal(pointaNormal);
    tglVertex(pointa);
  }
  glEnd();

  normalcounter = 0;
  for (i = 0; i <= v.size() - 4; i += 2) {
    glBegin(GL_QUAD_STRIP);
    T3DPointD olda(v[i].x, v[i].y, 0);
    T3DPointD oldb(v[i + 1].x, v[i + 1].y, 0);
    T3DPointD oldcenter = 0.5 * (olda + oldb);
    T3DPointD oldcenterNormal(0, 0, 1);
    T3DPointD oldaNormal = T3DPointD(0, 0, 1) - bend * normal[normalcounter];
    T3DPointD oldbNormal = T3DPointD(0, 0, 1) + bend * normal[normalcounter];
    T3DPointD a(v[i + 2].x, v[i + 2].y, 0);
    T3DPointD b(v[i + 3].x, v[i + 3].y, 0);
    T3DPointD center = 0.5 * (a + b);
    T3DPointD centerNormal(0, 0, 1);
    T3DPointD aNormal = T3DPointD(0, 0, 1) - bend * normal[normalcounter++];
    T3DPointD bNormal = T3DPointD(0, 0, 1) + bend * normal[normalcounter];
    tglNormal(oldaNormal);
    tglVertex(olda);
    tglNormal(aNormal);
    tglVertex(a);
    tglNormal(oldcenterNormal);
    tglVertex(oldcenter);
    tglNormal(centerNormal);
    tglVertex(center);
    tglNormal(oldbNormal);
    tglVertex(oldb);
    tglNormal(bNormal);
    tglVertex(b);
    glEnd();
  }

  glDisable(GL_NORMALIZE);
  glDisable(GL_LIGHTING);
  glDisable(GL_LIGHT0);
}

//-----------------------------------------------------------------------------

void TNormal2StrokeStyle::drawStroke(TFlash &flash,
                                     const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  // double length = stroke->getLength();
  // double step=10.0;
  TPointD pos1, pos2, pos3, pos4, oldPos1, oldPos2, oldPos3, oldPos4;

  TOutlineUtil::OutlineParameter param;
  param.m_lengthStep = 10.0;
  TStrokeOutline outline;
  TOutlineStyle::computeOutline(stroke, outline, param);
  const std::vector<TOutlinePoint> &v = outline.getArray();

  TPixel32 color = m_color;
  TPixelD dcolor;
  dcolor = toPixelD(color);

  TPixel32 color1;
  TPixelD dcolor1(0.5 * dcolor.r, 0.5 * dcolor.g, 0.5 * dcolor.b, 1.0);
  color1 = toPixel32(dcolor1);

  dcolor = TPixelD(dcolor.r + (1.0 - dcolor.r) * m_metal,
                   dcolor.g + (1.0 - dcolor.g) * m_metal,
                   dcolor.b + (1.0 - dcolor.b) * m_metal, dcolor.m);
  color = toPixel32(dcolor);
  flash.setThickness(0.0);

  SFlashUtils sfu;
  for (int i = 0; i <= (int)(v.size() - 4); i += 2) {
    TPointD olda(v[i].x, v[i].y);
    TPointD oldb(v[i + 1].x, v[i + 1].y);
    TPointD oldcenter = 0.5 * (olda + oldb);
    TPointD a(v[i + 2].x, v[i + 2].y);
    TPointD b(v[i + 3].x, v[i + 3].y);
    TPointD center = 0.5 * (a + b);

    vector<TPointD> vpl;
    vpl.push_back(olda);
    vpl.push_back(a);
    vpl.push_back(center);
    vpl.push_back(oldcenter);
    sfu.drawGradedPolyline(flash, vpl, color1, color);

    vpl.clear();
    vpl.push_back(oldb);
    vpl.push_back(b);
    vpl.push_back(center);
    vpl.push_back(oldcenter);
    sfu.drawGradedPolyline(flash, vpl, color1, color);
  }
}

//=============================================================================

namespace {
double get_inout_intensityslope(double in, double out, double t) {
  if (out < in) out = in;
  if (t < in)
    return t / in;
  else if (t > out)
    return (t - 1) / (out - 1);
  else
    return 1;
}
}

//-----------------------------------------------------------------------------

TChalkStrokeStyle2::TChalkStrokeStyle2()
    : m_color(TPixel32::Black)
    , m_blend(1.0)
    , m_intensity(50.0)
    , m_in(0.25)
    , m_out(0.25)
    , m_noise(0.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TChalkStrokeStyle2::clone() const {
  return new TChalkStrokeStyle2(*this);
}

//-----------------------------------------------------------------------------

int TChalkStrokeStyle2::getParamCount() const { return 5; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TChalkStrokeStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TChalkStrokeStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 5);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TChalkStrokeStyle2", "Border Fade");
    break;
  case 1:
    value = QCoreApplication::translate("TChalkStrokeStyle2", "Density");
    break;
  case 2:
    value = QCoreApplication::translate("TChalkStrokeStyle2", "Fade In");
    break;
  case 3:
    value = QCoreApplication::translate("TChalkStrokeStyle2", "Fade Out");
    break;
  case 4:
    value = QCoreApplication::translate("TChalkStrokeStyle2", "Noise");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TChalkStrokeStyle2::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 5);
  switch (index) {
  case 0:
    min = 0.0;
    max = 1.0;
    break;
  case 1:
    min = 0.0;
    max = 100.0;
    break;
  case 2:
    min = 0.0;
    max = 1.0;
    break;
  case 3:
    min = 0.0;
    max = 1.0;
    break;
  case 4:
    min = 0.0;
    max = 1.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TChalkStrokeStyle2::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 5);
  double value = 0;
  switch (index) {
  case 0:
    value = m_blend;
    break;
  case 1:
    value = m_intensity;
    break;
  case 2:
    value = m_in;
    break;
  case 3:
    value = m_out;
    break;
  case 4:
    value = m_noise;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TChalkStrokeStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 5);
  switch (index) {
  case 0:
    m_blend = value;
    break;
  case 1:
    m_intensity = value;
    break;
  case 2:
    m_in = value;
    break;
  case 3:
    m_out = value;
    break;
  case 4:
    m_noise = value;
    break;
  }

  // updateVersionNumber(); non serve perche' i parametri vengono sfrutttati
  // direttamente nella draw
}

//-----------------------------------------------------------------------------

void TChalkStrokeStyle2::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 105)
    throw TException("chalk stroke style: unknown obsolete format");
  m_in = 0.0, m_out = 0.0, m_noise = 0.0;

  is >> m_color >> m_blend >> m_intensity;
  m_blend = 1 - m_blend;
}

//-----------------------------------------------------------------------------

void TChalkStrokeStyle2::computeData(Doubles &data, const TStroke *stroke,
                                     const TColorFunction *cf) const {
  double length = stroke->getLength();
  double step   = 4;
  double s      = 0;

  data.clear();
  data.reserve(tceil(length / step) * 6 + 6);

  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u = normalize(u);

    data.push_back(pos.x);
    data.push_back(pos.y);
    data.push_back(pos.thick);
    data.push_back(u.x);
    data.push_back(u.y);
    data.push_back(s / length);

    s += step;
  }
}

//-----------------------------------------------------------------------------

void TChalkStrokeStyle2::drawStroke(const TColorFunction *cf, Doubles &data,
                                    const TStroke *stroke) const {
  double step = 4;

  double blend = m_blend;  // distanza che controlla da dove il gessetto
                           // comincia il fade out  (0, 1)
  double intensitymax =
      m_intensity;  // quanti punti vengono disegnati ad ogni step

  TRandom rnd;
  TRandom rnd_noise;
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  TPixelD dcolor;
  dcolor             = toPixelD(color);
  double mattedcolor = 0.5 * dcolor.m;

  double noise      = 0;
  double noiseslope = 0;
  double tmpnoise   = 0;

  GLuint chalkId;
  chalkId = glGenLists(1);
  glNewList(chalkId, GL_COMPILE);
  glBegin(GL_QUADS);
  glVertex2d(1, 1);
  glVertex2d(-1, 1);
  glVertex2d(-1, -1);
  glVertex2d(1, -1);
  glEnd();
  glEndList();

  for (UINT i = 0; i < data.size(); i += 6) {
    TThickPoint pos;
    pos.x     = data[i];
    pos.y     = data[i + 1];
    pos.thick = data[i + 2];
    TPointD u;
    u.x       = data[i + 3];
    u.y       = data[i + 4];
    TPointD v = rotate90(u);

    TPointD shift;
    double intslope    = get_inout_intensityslope(m_in, 1 - m_out, data[i + 5]);
    double transpslope = (intslope / blend) * dcolor.m;
    if (m_noise) {
      if (tmpnoise <= 0) {
        noise    = (100 / m_noise) * rnd_noise.getFloat();
        tmpnoise = noise;
      }
      noiseslope = get_inout_intensityslope(0.5, 0.5, tmpnoise / noise);
      tmpnoise -= step;
    } else
      noiseslope = 1;

    for (int i = 0; i < intensitymax * intslope * noiseslope; i++) {
      double vrandnorm = rnd.getFloat(-1.0, 1.0);
      double randomv   = vrandnorm * pos.thick * noiseslope;
      double randomu   = (0.5 - rnd.getFloat()) * step;
      shift            = pos + u * randomu + v * randomv;
      double mod       = fabs(vrandnorm);
      if (mod > 1 - blend)
        glColor4d(dcolor.r, dcolor.g, dcolor.b,
                  (double)rnd.getFloat() * ((1 - mod) * transpslope));
      else
        glColor4d(dcolor.r, dcolor.g, dcolor.b, mattedcolor);
      glPushMatrix();
      glTranslated(shift.x, shift.y, 0.0);
      glCallList(chalkId);
      glPopMatrix();
    }
  }

  glDeleteLists(chalkId, 1);
}

//=============================================================================

TBlendStrokeStyle2::TBlendStrokeStyle2()
    : m_color(TPixel32::Red), m_blend(1.0), m_in(0.25), m_out(0.25) {}

//-----------------------------------------------------------------------------

TColorStyle *TBlendStrokeStyle2::clone() const {
  return new TBlendStrokeStyle2(*this);
}

//-----------------------------------------------------------------------------

int TBlendStrokeStyle2::getParamCount() const { return 3; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TBlendStrokeStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TBlendStrokeStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 3);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TBlendStrokeStyle2", "Border Fade");
    break;
  case 1:
    value = QCoreApplication::translate("TBlendStrokeStyle2", "Fade In");
    break;
  case 2:
    value = QCoreApplication::translate("TBlendStrokeStyle2", "Fade Out");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TBlendStrokeStyle2::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 3);
  min = 0.0;
  max = 1.0;
}

//-----------------------------------------------------------------------------

double TBlendStrokeStyle2::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 3);
  double value = 0;
  switch (index) {
  case 0:
    value = m_blend;
    break;
  case 1:
    value = m_in;
    break;
  case 2:
    value = m_out;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TBlendStrokeStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 3);
  switch (index) {
  case 0:
    m_blend = value;
    break;
  case 1:
    m_in = value;
    break;
  case 2:
    m_out = value;
    break;
  }

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TBlendStrokeStyle2::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 110)
    throw TException("Blend  stroke style: unknown obsolete format");
  m_in = 0.0, m_out = 0.0;
  is >> m_color >> m_blend;
  m_blend = 1 - m_blend;
}

//-----------------------------------------------------------------------------

void TBlendStrokeStyle2::computeData(PointsAndDoubles &data,
                                     const TStroke *stroke,
                                     const TColorFunction *cf) const {
  data.clear();
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();
  double step   = 10.0;
  TPointD pos1, pos2, pos3, pos4, oldPos1, oldPos2, oldPos3, oldPos4;
  double oldintslope;
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  TPixelD dcolor;
  dcolor           = toPixelD(color);
  bool firstRing   = true;
  double s         = 0;
  double maxfactor = 2 * m_blend / step;  // max definisce il numero di
                                          // intervalli in cui la regione viene
                                          // divisa
  // per evitare il problema del blend poco efficiente sui triangoli
  double minthickness = MINTHICK * sqrt(tglGetPixelSize2());
  double thickness    = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u = normalize(u);
    if (pos.thick < MINTHICK)
      thickness = minthickness;
    else
      thickness = pos.thick;
    TPointD v   = rotate90(u) * thickness;

    TPointD v1 = v * (1 - m_blend);
    pos1       = pos + v;
    pos2       = pos + v1;
    pos3       = pos - v1;
    pos4       = pos - v;
    double intslope =
        get_inout_intensityslope(m_in, 1 - m_out, s / length) * dcolor.m;
    if (firstRing) {
      firstRing = false;
    } else {
      int max       = (int)(maxfactor * thickness);
      double invmax = 1.0 / max;
      data.push_back(make_pair(oldPos1, 0.0));
      data.push_back(make_pair(pos1, 0.0));
      int i;
      for (i = 1; i < max; i++) {
        data.push_back(make_pair(i * (oldPos2 - oldPos1) * invmax + oldPos1,
                                 (i * oldintslope) * invmax));
        data.push_back(make_pair(i * (pos2 - pos1) * invmax + pos1,
                                 (i * intslope) * invmax));
      }

      data.push_back(make_pair(oldPos2, oldintslope));
      data.push_back(make_pair(pos2, intslope));
      data.push_back(make_pair(oldPos3, oldintslope));
      data.push_back(make_pair(pos3, intslope));

      for (i = 0; i < max; i++) {
        data.push_back(make_pair(i * (oldPos4 - oldPos3) * invmax + oldPos3,
                                 (oldintslope * invmax) * (max - i)));
        data.push_back(make_pair(i * (pos4 - pos3) * invmax + pos3,
                                 (intslope * invmax) * (max - i)));
      }

      data.push_back(make_pair(oldPos4, 0.0));
      data.push_back(make_pair(pos4, 0.0));
    }
    oldPos1     = pos1;
    oldPos2     = pos2;
    oldPos3     = pos3;
    oldPos4     = pos4;
    oldintslope = intslope;
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TBlendStrokeStyle2::drawStroke(const TColorFunction *cf,
                                    PointsAndDoubles &data,
                                    const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  TPixelD dcolor;
  dcolor = toPixelD(color);

  PointsAndDoubles::iterator it = data.begin();
  glBegin(GL_QUAD_STRIP);
  for (; it != data.end(); ++it) {
    glColor4d(dcolor.r, dcolor.g, dcolor.b, it->second);
    tglVertex(it->first);
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void TBlendStrokeStyle2::drawStroke(TFlash &flash,
                                    const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();
  double step   = 10.0;
  TPointD pos1, pos2, pos3, pos4, oldPos1, oldPos2, oldPos3, oldPos4;
  double oldintslope;
  TPixel32 color = m_color;

  double lblend = m_blend;
  // For the Flash version, to simplify the grading.
  lblend = 1.0;

  TPixelD dcolor;
  dcolor         = toPixelD(color);
  bool firstRing = true;
  double s       = 0;
  // double maxfactor=2*lblend/step; //max definisce il numero di intervalli in
  // cui la regione viene divisa
  // per evitare il problema del blend poco efficiente sui triangoli

  vector<TPointD> vp1, vp2;
  vector<TPixelD> vdc1, vdc2;

  flash.setThickness(0);
  SFlashUtils sfu;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u          = normalize(u);
    TPointD v  = rotate90(u) * pos.thick;
    TPointD v1 = v * (1 - lblend);
    pos1       = pos + v;
    pos2       = pos + v1;
    pos3       = pos - v1;
    pos4       = pos - v;
    double intslope =
        get_inout_intensityslope(m_in, 1 - m_out, s / length) * dcolor.m;
    if (firstRing) {
      firstRing = false;
    } else {
      vp1.clear();
      vp2.clear();
      vdc1.clear();
      vdc2.clear();

      // The Flash version has been simplified. Only one direction grading!

      vdc1.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, 0));
      vp1.push_back(oldPos1);
      vdc2.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, 0));
      vp2.push_back(pos1);
      vdc1.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, oldintslope));
      vp1.push_back(oldPos2);
      vdc2.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, intslope));
      vp2.push_back(pos2);
      vdc1.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, oldintslope));
      vp1.push_back(oldPos3);
      vdc2.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, intslope));
      vp2.push_back(pos3);
      vdc1.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, 0));
      vp1.push_back(oldPos4);
      vdc2.push_back(TPixelD(dcolor.r, dcolor.g, dcolor.b, 0));
      vp2.push_back(pos4);

      vector<TPointD> vpl;
      vpl.push_back(vp1[0]);
      vpl.push_back(vp1[3]);
      vpl.push_back(vp2[3]);
      vpl.push_back(vp2[0]);

      TPixel32 col[4];
      col[0] = toPixel32(vdc1[1]);
      col[1] = toPixel32(vdc1[2]);
      col[2] = toPixel32(vdc2[2]);
      col[3] = toPixel32(vdc2[1]);

      sfu.drawGradedPolyline(flash, vpl, blend(col[0], col[1], 0.5),
                             blend(col[2], col[3], 0.5));
    }
    oldPos1     = pos1;
    oldPos2     = pos2;
    oldPos3     = pos3;
    oldPos4     = pos4;
    oldintslope = intslope;
    s += step;
  }
}

//=============================================================================

TTwirlStrokeStyle::TTwirlStrokeStyle()
    : m_color(TPixel32::Green), m_period(30.0), m_blend(0.5) {}

//-----------------------------------------------------------------------------

TColorStyle *TTwirlStrokeStyle::clone() const {
  return new TTwirlStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TTwirlStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TTwirlStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TTwirlStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 2);
  return index == 0 ? QCoreApplication::translate("TTwirlStrokeStyle", "Twirl")
                    : QCoreApplication::translate("TTwirlStrokeStyle", "Shade");
}

//-----------------------------------------------------------------------------

void TTwirlStrokeStyle::getParamRange(int index, double &min,
                                      double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = 1.0;
    max = 100.0;
  } else {
    min = 0.0;
    max = 1.0;
  }
}

//-----------------------------------------------------------------------------

double TTwirlStrokeStyle::getParamValue(TColorStyle::double_tag,
                                        int index) const {
  assert(0 <= index && index < 2);
  return index == 0 ? m_period : m_blend;
}

//-----------------------------------------------------------------------------

void TTwirlStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);
  if (index == 0)
    m_period = value;
  else
    m_blend = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TTwirlStrokeStyle::computeData(Doubles &data, const TStroke *stroke,
                                    const TColorFunction *cf) const {
  double length   = stroke->getLength();
  double step     = 5.0;
  double period   = 10 * (102 - m_period);
  double hperiod  = period / 2;
  double blendval = 0;
  TRandom rnd;

  data.clear();
  data.reserve(tceil(length / step) + 1);

  double s = 0;
  TPointD app;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u            = normalize(u);
    TPointD v    = rotate90(u) * (pos.thick);
    double shift = sin((M_PI / hperiod) * s);

    app = pos + v * shift;
    data.push_back(app.x);
    data.push_back(app.y);
    app = pos - v * shift;
    data.push_back(app.x);
    data.push_back(app.y);

    blendval = get_inout_intensityslope(
        m_blend, 1.0 - m_blend, (s - ((int)(s / hperiod) * hperiod)) / hperiod);
    data.push_back(blendval);
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TTwirlStrokeStyle::drawStroke(const TColorFunction *cf, Doubles &data,
                                   const TStroke *stroke) const {
  TPixel32 blackcolor = TPixel32::Black;

  TPixel32 color;
  if (cf) {
    color      = (*(cf))(m_color);
    blackcolor = (*(cf))(blackcolor);
  } else {
    color = m_color;
  }
  blackcolor.m = m_color.m;

  TPointD app;
  UINT i = 0;

  glBegin(GL_QUAD_STRIP);
  for (; i < data.size(); i += 5) {
    tglColor(blend(blackcolor, color, data[i + 4]));
    app.x = data[i];
    app.y = data[i + 1];
    tglVertex(app);
    app.x = data[i + 2];
    app.y = data[i + 3];
    tglVertex(app);
  }
  glEnd();

  for (i = 5; i < data.size(); i += 5) {
    tglColor(blend(color, blackcolor, data[i + 4]));
    glBegin(GL_LINES);
    app.x = data[i - 5];
    app.y = data[i - 4];
    tglVertex(app);
    app.x = data[i];
    app.y = data[i + 1];
    tglVertex(app);
    glEnd();
    glBegin(GL_LINES);
    app.x = data[i - 3];
    app.y = data[i - 2];
    tglVertex(app);
    app.x = data[i + 2];
    app.y = data[i + 3];
    tglVertex(app);
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void TTwirlStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  double length   = stroke->getLength();
  double step     = 5.0;
  double period   = 10 * (102 - m_period);
  double hperiod  = period / 2;
  double blendval = 0;
  TRandom rnd;
  TPixel32 blackcolor = TPixel32::Black;
  TPixel32 color      = m_color;
  blackcolor.m        = m_color.m;

  vector<TPointD> points1;
  vector<TPointD> points2;
  vector<double> vblend;
  double s = 0;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u            = normalize(u);
    TPointD v    = rotate90(u) * (pos.thick);
    double shift = sin((M_PI / hperiod) * s);
    points1.push_back(pos + v * shift);
    points2.push_back(pos - v * shift);
    blendval = get_inout_intensityslope(
        m_blend, 1.0 - m_blend, (s - ((int)(s / hperiod) * hperiod)) / hperiod);
    vblend.push_back(blendval);
    s += step;
  }

  SFlashUtils sfu;
  for (int i = 1; i < (int)points1.size(); i++) {
    vector<TPointD> vp;
    vp.push_back(points1[i - 1]);
    vp.push_back(points2[i - 1]);
    vp.push_back(points2[i]);
    vp.push_back(points1[i]);
    flash.setThickness(0.0);
    sfu.drawGradedPolyline(flash, vp, blend(blackcolor, color, vblend[i - 1]),
                           blend(blackcolor, color, vblend[i]));
    //	flash.setFillColor(blend(blackcolor, color , vblend[i-1]));
    //	flash.drawPolyline(vp);

    vector<TSegment> sv;
    sv.push_back(TSegment(points1[i - 1], points1[i]));
    sv.push_back(TSegment(points2[i - 1], points2[i]));
    flash.setThickness(1.0);
    flash.setLineColor(blend(color, blackcolor, vblend[i - 1]));
    flash.drawSegments(sv, false);
  }
}

//=============================================================================

TSawToothStrokeStyle::TSawToothStrokeStyle(TPixel32 color, double parameter)
    : m_color(color), m_parameter(parameter) {}

//-----------------------------------------------------------------------------

TColorStyle *TSawToothStrokeStyle::clone() const {
  return new TSawToothStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

void TSawToothStrokeStyle::computeOutline(
    const TStroke *stroke, TStrokeOutline &outline,
    TOutlineUtil::OutlineParameter param) const {
  param.m_lengthStep = m_parameter;
  TOutlineStyle::computeOutline(stroke, outline, param);
}

//-----------------------------------------------------------------------------

void TSawToothStrokeStyle::drawStroke(TFlash &flash,
                                      const TStroke *stroke) const {
  TOutlineUtil::OutlineParameter param;
  param.m_lengthStep = std::max(20.0, m_parameter);
  TStrokeOutline outline;
  TOutlineStyle::computeOutline(stroke, outline, param);
  const std::vector<TOutlinePoint> &v = outline.getArray();
  if (v.empty()) return;

  TPixel32 color = m_color;
  flash.setThickness(0.0);
  flash.setFillColor(color);

  if (v.empty()) return;
  double old[2];
  int counter = 0;
  for (UINT i = 0; i < v.size() - 2; i += 2) {
    if (0 != v[i].stepCount) {
      if (counter) {
        vector<TPointD> plv;
        //        flash.setFillColor(color);

        plv.push_back(TPointD(old[0], old[1]));
        plv.push_back(TPointD(v[i].x, v[i].y));
        plv.push_back(TPointD(v[i + 1].x, v[i + 1].y));
        flash.drawPolyline(plv);
      }
      old[0] = v[i].x;
      old[1] = v[i].y;
      counter++;
    }
  }
}

//-----------------------------------------------------------------------------

void TSawToothStrokeStyle::drawStroke(const TColorFunction *cf,
                                      TStrokeOutline *outline,
                                      const TStroke *stroke) const {
  UINT i;
  int counter = 0;
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  tglColor(color);
  const std::vector<TOutlinePoint> &v = outline->getArray();
  if (v.empty()) return;
  double old[2];
  // outline with antialiasing
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < v.size() - 2; i += 2) {
    if (0 != v[i].stepCount) {
      if (counter) {
        glVertex2dv(old);
        glVertex2dv(&v[i].x);
        glVertex2dv(&v[i + 1].x);
        glVertex2dv(old);
      }
      old[0] = v[i].x;
      old[1] = v[i].y;
      counter++;
    }
  }
  glEnd();
  counter = 0;
  glBegin(GL_TRIANGLES);
  for (i = 0; i < v.size() - 2; i += 2) {
    if (0 != v[i].stepCount) {
      if (counter) {
        glVertex2dv(old);
        glVertex2dv(&v[i].x);
        glVertex2dv(&v[i + 1].x);
      }
      old[0] = v[i].x;
      old[1] = v[i].y;
      counter++;
    }
  }
  glEnd();
}

//-----------------------------------------------------------------------------

int TSawToothStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TSawToothStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TSawToothStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TSawToothStrokeStyle", "Distance");
}

//-----------------------------------------------------------------------------

void TSawToothStrokeStyle::getParamRange(int index, double &min,
                                         double &max) const {
  assert(0 <= index && index < 1);
  min = 0.1;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TSawToothStrokeStyle::getParamValue(TColorStyle::double_tag,
                                           int index) const {
  assert(0 <= index && index < 1);
  return m_parameter;
}

//-----------------------------------------------------------------------------

void TSawToothStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_parameter = value;
  updateVersionNumber();  // questo si chiama per ogno cambiamento di parametro
                          // per cui di deve ricalcolare l'outline
}

//=============================================================================

TMultiLineStrokeStyle2::TMultiLineStrokeStyle2()
    : m_color0(TPixel32(0, 255, 0))
    , m_color1(TPixel32(0, 0, 0))
    , m_intensity(0.2)
    , m_length(20.0)
    , m_thick(0.3)
    , m_noise(0.5) {}

//-----------------------------------------------------------------------------

TColorStyle *TMultiLineStrokeStyle2::clone() const {
  return new TMultiLineStrokeStyle2(*this);
}

//-----------------------------------------------------------------------------

namespace {

typedef struct {
  TPointD u, v;
  TThickPoint p;
} myLineData;
}

//-----------------------------------------------------------------------------

int TMultiLineStrokeStyle2::getParamCount() const { return 4; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TMultiLineStrokeStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TMultiLineStrokeStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 4);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TMultiLineStrokeStyle2", "Density");
    break;
  case 1:
    value = QCoreApplication::translate("TMultiLineStrokeStyle2", "Size");
    break;
  case 2:
    value = QCoreApplication::translate("TMultiLineStrokeStyle2", "Thickness");
    break;
  case 3:
    value = QCoreApplication::translate("TMultiLineStrokeStyle2", "Noise");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TMultiLineStrokeStyle2::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    min = 0.0;
    max = 1.0;
    break;
  case 1:
    min = 0.0;
    max = 100.0;
    break;
  case 2:
    min = 0.0;
    max = 1.0;
    break;
  case 3:
    min = 0.0;
    max = 1.0;
    break;
  }
}
//-----------------------------------------------------------------------------

double TMultiLineStrokeStyle2::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < 4);
  double value = 0;
  switch (index) {
  case 0:
    value = m_intensity;
    break;
  case 1:
    value = m_length;
    break;
  case 2:
    value = m_thick;
    break;
  case 3:
    value = m_noise;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TMultiLineStrokeStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    m_intensity = value;
    break;
  case 1:
    m_length = value;
    break;
  case 2:
    m_thick = value;
    break;
  case 3:
    m_noise = value;
    break;
  }

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TMultiLineStrokeStyle2::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 118 && ids != 128)
    throw TException("Multi Line stroke style: unknown obsolete format");
  if (ids == 118) {
    m_length = 20.0, m_thick = 0.3, m_noise = 0.0;
    is >> m_color0 >> m_intensity;
    m_color1 = TPixel32::Black;
  } else {
    is >> m_color0 >> m_intensity >> m_length >> m_thick >> m_noise;
    m_color1 = TPixel32::Black;
  }
}

//-----------------------------------------------------------------------------

void TMultiLineStrokeStyle2::computeData(BlendAndPoints &data,
                                         const TStroke *stroke,
                                         const TColorFunction *cf) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();
  double step   = 4.0;
  int maxlength = (int)m_length;
  double factor = 0;
  TRandom rnd;

  vector<myLineData> LineData;
  myLineData Data;
  double s = 0;

  double minthickness = MINTHICK * sqrt(tglGetPixelSize2());
  double thickness    = 0;
  double strokethick  = m_thick;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    Data.p = stroke->getThickPoint(w);
    Data.u = stroke->getSpeed(w);
    if (norm2(Data.u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    Data.u = normalize(Data.u);
    if (Data.p.thick < MINTHICK)
      thickness = minthickness;
    else
      thickness = Data.p.thick;
    Data.v      = rotate90(Data.u) * thickness;

    LineData.push_back(Data);
    s += step;
  }

  BlendAndPoint appData;

  data.clear();
  data.reserve(LineData.size());

  for (int i = 0; i < m_intensity * LineData.size(); i++) {
    appData.points.clear();
    int start = rnd.getInt(0, LineData.size());
    int end   = start + maxlength + rnd.getInt(0, maxlength);
    if (end > (int)LineData.size()) end = LineData.size();
    double halfcount                    = (end - start) / 2.0;
    double vshift                       = (0.5 - rnd.getFloat());

    appData.blend = rnd.getFloat();

    for (int j = 0; j < (end - start); j++) {
      if (j < halfcount)
        factor = j / halfcount;
      else
        factor = 1 - (j - halfcount) / halfcount;
      appData.points.push_back(
          LineData[j + start].p +
          LineData[j + start].v *
              (vshift -
               strokethick * factor * (1 - m_noise * (1 - rnd.getFloat()))));
      appData.points.push_back(
          LineData[j + start].p +
          LineData[j + start].v *
              (vshift +
               strokethick * factor * (1 - m_noise * (1 - rnd.getFloat()))));
    }
    data.push_back(appData);
  }
}

//-----------------------------------------------------------------------------

void TMultiLineStrokeStyle2::drawStroke(const TColorFunction *cf,
                                        BlendAndPoints &data,
                                        const TStroke *stroke) const {
  TPixel32 color0, color1;
  if (cf) {
    color0 = (*(cf))(m_color0);
    color1 = (*(cf))(m_color1);
  } else {
    color0 = m_color0;
    color1 = m_color1;
  }

  glEnable(GL_POLYGON_SMOOTH);

  for (UINT i = 0; i < data.size(); i++) {
    tglColor(blend(color0, color1, data[i].blend));

    glBegin(GL_QUAD_STRIP);
    for (UINT j = 0; j < data[i].points.size(); j++)
      tglVertex(data[i].points[j]);

    glEnd();
  }
  glDisable(GL_POLYGON_SMOOTH);
}

//-----------------------------------------------------------------------------

void TMultiLineStrokeStyle2::drawStroke(TFlash &flash,
                                        const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();
  double step   = 4.0;
  int maxlength = (int)m_length;
  double factor = 0;
  TRandom rnd;
  TPixel32 color0, color1;
  color0 = m_color0;
  color1 = m_color1;

  vector<myLineData> LineData;
  myLineData Data;
  double s           = 0;
  double strokethick = m_thick;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < 0) {
      s += 0.1;
      continue;
    }  // per tamponare il baco della getParameterAtLength()
    Data.p = stroke->getThickPoint(w);
    Data.u = stroke->getSpeed(w);
    if (norm2(Data.u) == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    Data.u = normalize(Data.u);
    Data.v = rotate90(Data.u) * Data.p.thick;
    LineData.push_back(Data);
    s += step;
  }

  for (int i = 0; i < m_intensity * LineData.size(); i++) {
    int start = rnd.getInt(0, LineData.size());
    int end   = start + maxlength + rnd.getInt(0, maxlength);
    if (end > (int)LineData.size()) end = LineData.size();
    double halfcount                    = (end - start) / 2.0;
    double vshift                       = (0.5 - rnd.getFloat());
    flash.setThickness(0.0);
    flash.setFillColor(blend(color0, color1, rnd.getFloat()));
    vector<TSegment> sv;
    int j;
    for (j = 0; j < (end - start); j++) {
      if (j < halfcount)
        factor = j / halfcount;
      else
        factor   = 1 - (j - halfcount) / halfcount;
      float rand = rnd.getFloat();
      TPointD p0 =
          (LineData[j + start].p +
           LineData[j + start].v *
               (vshift - strokethick * factor * (1 - m_noise * (1 - rand))));
      TPointD p1 =
          (LineData[j + start].p +
           LineData[j + start].v *
               (vshift + strokethick * factor * (1 - m_noise * (1 - rand))));
      sv.push_back(TSegment(p0, p1));
    }
    for (j = 0; j < ((int)sv.size() - 1); j++) {
      vector<TPointD> pv;
      pv.push_back(sv[j].getP0());
      pv.push_back(sv[j].getP1());
      pv.push_back(sv[j + 1].getP1());
      pv.push_back(sv[j + 1].getP0());
      flash.drawPolyline(pv);
    }
  }
}

//=============================================================================

TZigzagStrokeStyle::TZigzagStrokeStyle()
    : m_color(TPixel32(0, 0, 0))
    , m_minDist(0.5)
    , m_maxDist(6.0)
    , m_minAngle(30.0)
    , m_maxAngle(60.0)
    , m_thickness(1.0) {}

//-----------------------------------------------------------------------------

TColorStyle *TZigzagStrokeStyle::clone() const {
  return new TZigzagStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TZigzagStrokeStyle::getParamCount() const { return 5; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TZigzagStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TZigzagStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 5);
  QString value;
  switch (index) {
  case 0:
    value = QCoreApplication::translate("TZigzagStrokeStyle", "Min Distance");
    break;
  case 1:
    value = QCoreApplication::translate("TZigzagStrokeStyle", "Max Distance");
    break;
  case 2:
    value = QCoreApplication::translate("TZigzagStrokeStyle", "Min Angle");
    break;
  case 3:
    value = QCoreApplication::translate("TZigzagStrokeStyle", "Max Angle");
    break;
  case 4:
    value = QCoreApplication::translate("TZigzagStrokeStyle", "Thickness");
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 5);
  switch (index) {
  case 0:
    min = 0.5;
    max = 50.0;
    break;
  case 1:
    min = 0.5;
    max = 50.0;
    break;
  case 2:
    min = -90.0;
    max = 90.0;
    break;
  case 3:
    min = -90.0;
    max = 90.0;
    break;
  case 4:
    min = 0.0;
    max = 3.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double TZigzagStrokeStyle::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 5);
  double value = 0;
  switch (index) {
  case 0:
    value = m_minDist;
    break;
  case 1:
    value = m_maxDist;
    break;
  case 2:
    value = m_minAngle;
    break;
  case 3:
    value = m_maxAngle;
    break;
  case 4:
    value = m_thickness;
    break;
  }
  return value;
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 5);
  switch (index) {
  case 0:
    m_minDist = value;
    break;
  case 1:
    m_maxDist = value;
    break;
  case 2:
    m_minAngle = value;
    break;
  case 3:
    m_maxAngle = value;
    break;
  case 4:
    m_thickness = value;
    break;
  }
  updateVersionNumber();
}

//-----------------------------------------------------------------------------

bool TZigzagStrokeStyle::getZigZagPosition(const TStroke *stroke, TRandom &rnd,
                                           const double s, const int first,
                                           const double minTranslLength,
                                           TThickPoint &pos,
                                           TThickPoint &pos1) const {
  double w  = stroke->getParameterAtLength(s);
  pos       = stroke->getThickPoint(w);
  TPointD u = stroke->getSpeed(w);
  if (norm2(u) < TConsts::epsilon) return false;
  u          = normalize(u);
  TPointD uu = u;
  double angle =
      m_minAngle + (m_maxAngle - m_minAngle) * (double)rnd.getUInt(101) * 0.01;
  TRotation rotM(angle);
  u                      = rotM * u;
  double maxTranslLength = angle == 90 ? 1.0 : 2.0;
  if (angle > 30 && angle < 90) {
    double rta      = 1.0 / tan(degree2rad(angle));
    maxTranslLength = sqrt(sq(rta) + 1.0);
  }
  double r =
      (minTranslLength + (maxTranslLength - minTranslLength) * rnd.getFloat()) *
      pos.thick * first;
  pos  = pos + r * u;
  pos1 = pos + uu * m_thickness;
  return true;
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::setRealMinMax() const {
  TZigzagStrokeStyle *ncpthis = const_cast<TZigzagStrokeStyle *>(this);
  double minDist              = std::min(m_minDist, m_maxDist);
  double maxDist              = std::max(m_minDist, m_maxDist);
  double minAngle             = std::min(m_minAngle, m_maxAngle);
  double maxAngle             = std::max(m_minAngle, m_maxAngle);
  ncpthis->m_minDist          = minDist;
  ncpthis->m_maxDist          = maxDist;
  ncpthis->m_minAngle         = minAngle;
  ncpthis->m_maxAngle         = maxAngle;
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::computeData(Points &points, const TStroke *stroke,
                                     const TColorFunction *cf) const {
  assert(glGetError() == GL_NO_ERROR);
  assert(!!stroke);
  double length = stroke->getLength();
  if (length <= 0) return;

  setRealMinMax();
  // e.g minimum translation length is the half of the thickness
  const double minTranslLength = 0.7;
  // const bool isTransparent=m_color.m<255;

  int first = 1;
  TThickPoint pos;
  TThickPoint pos1;
  TRandom rnd;

  points.clear();
  points.reserve(tceil(length / m_minDist) * 2 + 2);

  for (double s = 0.0; s <= length; first = -first) {
    if (getZigZagPosition(stroke, rnd, s, first, minTranslLength, pos, pos1)) {
      // TRectD rec(pos.x,pos.y,pos1.x,pos1.y);
      points.push_back(pos);
      points.push_back(pos1);
    }
    s += m_minDist + (m_maxDist - m_minDist) * (double)rnd.getUInt(101) * 0.01;
  }
  if (getZigZagPosition(stroke, rnd, length - TConsts::epsilon, first,
                        minTranslLength, pos, pos1)) {
    points.push_back(pos);
    points.push_back(pos1);
  }
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::drawStroke(const TColorFunction *cf, Points &points,
                                    const TStroke *stroke) const {
  if (points.size() <= 1) return;
  TPixel32 color;

  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  tglColor(m_color);

  glEnableClientState(GL_VERTEX_ARRAY);

  glVertexPointer(2, GL_DOUBLE, sizeof(TPointD), &points[0]);
  glDrawArrays(GL_QUAD_STRIP, 0, points.size());
  /*
glBegin(GL_QUAD_STRIP);
for(UINT i=0; i<rects.size(); i++)
{
tglVertex(rects[i].getP00());
tglVertex(rects[i].getP11());
}
glEnd();
*/

  glVertexPointer(2, GL_DOUBLE, sizeof(TPointD) * 2, &points[0]);
  glDrawArrays(GL_LINE_STRIP, 0, points.size() / 2);

  glVertexPointer(2, GL_DOUBLE, sizeof(TPointD) * 2, &points[1]);
  glDrawArrays(GL_LINE_STRIP, 0, points.size() / 2);

  glVertexPointer(2, GL_DOUBLE, sizeof(TPointD), &points[0]);
  glDrawArrays(GL_LINES, 0, points.size());

  glDisableClientState(GL_VERTEX_ARRAY);
  // drawBLines(rects);
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::drawStroke(TFlash &flash,
                                    const TStroke *stroke) const {
  if (!stroke) return;
  double length = stroke->getLength();
  if (length <= 0) return;

  setRealMinMax();
  // e.g minimum translation length is the half of the thickness
  const double minTranslLength = 0.7;

  int first = 1;
  TThickPoint pos;
  TThickPoint pos1;
  TRandom rnd;
  RectVector rects;

  for (double s = 0.0; s <= length; first = -first) {
    if (getZigZagPosition(stroke, rnd, s, first, minTranslLength, pos, pos1)) {
      TRectD rec(pos.x, pos.y, pos1.x, pos1.y);
      rects.push_back(rec);
    }
    s += m_minDist + (m_maxDist - m_minDist) * (double)rnd.getUInt(101) * 0.01;
  }
  if (getZigZagPosition(stroke, rnd, length - TConsts::epsilon, first,
                        minTranslLength, pos, pos1)) {
    TRectD rec(pos.x, pos.y, pos1.x, pos1.y);
    rects.push_back(rec);
  }

  flash.setLineColor(m_color);
  vector<TSegment> segmentsArray;

  flash.setThickness(m_thickness);
  RectVector::const_iterator rvi = rects.begin();
  for (; rvi != (rects.end() - 1); rvi++) {
    RectVector::const_iterator rvii = rvi + 1;
    TPointD p0((rvi->x0 + rvi->x1) / 2.0, (rvi->y0 + rvi->y1) / 2.0);
    TPointD p1((rvii->x0 + rvii->x1) / 2.0, (rvii->y0 + rvii->y1) / 2.0);
    segmentsArray.push_back(TSegment(p0, p1));
  }

  flash.drawSegments(segmentsArray, false);
}

//=============================================================================

TSinStrokeStyle::TSinStrokeStyle()
    : m_color(TPixel32::Black), m_frequency(10.0), m_thick(0.4) {}

//-----------------------------------------------------------------------------

TColorStyle *TSinStrokeStyle::clone() const {
  return new TSinStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int TSinStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TSinStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TSinStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 2);
  return index == 0
             ? QCoreApplication::translate("TSinStrokeStyle", "Frequency")
             : QCoreApplication::translate("TZigzTSinStrokeStyleagStrokeStyle",
                                           "Thickness");
}

//-----------------------------------------------------------------------------

void TSinStrokeStyle::getParamRange(int index, double &min, double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = 1.0;
    max = 20.0;
  } else {
    min = 0.0;
    max = 1.0;
  }
}

//-----------------------------------------------------------------------------

double TSinStrokeStyle::getParamValue(TColorStyle::double_tag,
                                      int index) const {
  assert(0 <= index && index < 2);
  return index == 0 ? m_frequency : m_thick;
}

//-----------------------------------------------------------------------------

void TSinStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);
  if (index == 0)
    m_frequency = value;
  else
    m_thick = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TSinStrokeStyle::computeData(Points &positions, const TStroke *stroke,
                                  const TColorFunction *cf) const {
  double length = stroke->getLength();
  double step   = 5.0;

  positions.clear();
  positions.reserve(tceil((length + 1) / step));

  double frequency = m_frequency / 100;
  ;
  double s = 0;
  // bool firstRing = true;
  double thick = 1 - m_thick;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u               = normalize(u);
    TPointD v       = rotate90(u);
    double sinvalue = sin(frequency * s);
    positions.push_back(pos + v * pos.thick * sinvalue);
    positions.push_back(pos + v * thick * pos.thick * sinvalue);
    s += step;
  }
}

//-----------------------------------------------------------------------------

void TSinStrokeStyle::drawStroke(const TColorFunction *cf,
                                 std::vector<TPointD> &positions,
                                 const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  tglColor(color);

  glBegin(GL_QUAD_STRIP);
  int i = 0;
  for (; i < (int)positions.size(); i += 2) {
    tglVertex(positions[i]);
    tglVertex(positions[i + 1]);
  }
  glEnd();
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < (int)positions.size(); i += 2) {
    tglVertex(positions[i]);
  }
  glEnd();
  glBegin(GL_LINE_STRIP);
  for (i = 1; i < (int)positions.size() - 1; i += 2) {
    tglVertex(positions[i]);
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void TSinStrokeStyle::drawStroke(TFlash &flash, const TStroke *stroke) const {
  double length = stroke->getLength();
  double step   = 5.0;

  double frequency = m_frequency / 100;
  ;
  vector<TPointD> points;

  double s = 0;
  // bool firstRing = true;
  double thick = 1 - m_thick;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    // if(w<0) {s+=0.1; continue;} // per tamponare il baco della
    // getParameterAtLength()
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    double normu    = norm2(u);
    if (normu == 0) {
      s += 0.1;
      continue;
    }  // non dovrebbe succedere mai, ma per prudenza....
    u               = normalize(u);
    TPointD v       = rotate90(u);
    double sinvalue = sin(frequency * s);
    points.push_back(pos + v * pos.thick * sinvalue);
    points.push_back(pos + v * thick * pos.thick * sinvalue);
    s += step;
  }

  // Polyline version
  flash.setThickness(0.0);
  flash.setFillColor(m_color);
  for (int i = 0; i < ((int)points.size() - 2); i += 2) {
    vector<TPointD> plv;
    plv.push_back(points[i]);
    plv.push_back(points[i + 1]);
    plv.push_back(points[i + 3]);
    plv.push_back(points[i + 2]);
    flash.drawPolyline(plv);
  }

  // Quadratic version
  /*  flash.setThickness(m_thick);
vector<TPointD> pp;
for( int i=0; i<(int)(points.size()-1); i+=2 )
    pp.push_back(TPointD((points[i]+points[i+1])*0.5));

if ( pp.size()<=2 ) {
    if ( pp.size()==2 ) {
       vector<TSegment> sv;
       sv.push_back(TSegment(pp[0],pp[1]));
           flash.setLineColor(m_color);
           flash.drawSegments(sv,false);
    }
    return;
}

vector<TQuadratic> qv;
qv.push_back(TQuadratic(pp[i],pp[i]*0.75+pp[i+1]*0.25,(pp[i]+pp[i+1])*0.5));
for( i=1; i<(int)(pp.size()-1); i++ ) {
   TPointD p0=((pp[i-1]+pp[i])*0.5);
   TPointD p1=pp[i];
   TPointD p2=((pp[i]+pp[i+1])*0.5);
   qv.push_back(TQuadratic(p0,p1,p2));
}
int n=pp.size()-1;
qv.push_back(TQuadratic((pp[n-1]+pp[n])*0.5,pp[n-1]*0.25+pp[n]*0.75,pp[n]));

flash.setLineColor(m_color);
flash.setThickness(m_thick);
flash.drawquads(qv);
*/
}

//=============================================================================

TFriezeStrokeStyle2::TFriezeStrokeStyle2()
    : m_color(TPixel32::Black), m_parameter(0.7), m_thick(0.3) {}

//-----------------------------------------------------------------------------

TColorStyle *TFriezeStrokeStyle2::clone() const {
  return new TFriezeStrokeStyle2(*this);
}

//-----------------------------------------------------------------------------

int TFriezeStrokeStyle2::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TFriezeStrokeStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TFriezeStrokeStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 2);
  if (index == 0)
    return QCoreApplication::translate("TFriezeStrokeStyle2", "Twirl");
  else
    return QCoreApplication::translate("TFriezeStrokeStyle2", "Thickness");
}

//-----------------------------------------------------------------------------

void TFriezeStrokeStyle2::getParamRange(int index, double &min,
                                        double &max) const {
  assert(0 <= index && index < 2);
  if (index == 0) {
    min = -1.0;
    max = 1.0;
  } else {
    min = 0.0;
    max = 1.0;
  }
}

//-----------------------------------------------------------------------------

double TFriezeStrokeStyle2::getParamValue(TColorStyle::double_tag,
                                          int index) const {
  assert(0 <= index && index < 2);
  if (index == 0)
    return m_parameter;
  else
    return m_thick;
}

//-----------------------------------------------------------------------------

void TFriezeStrokeStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 2);
  if (index == 0)
    m_parameter = value;
  else
    m_thick = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

void TFriezeStrokeStyle2::loadData(int ids, TInputStreamInterface &is) {
  if (ids != 102)
    throw TException("Frieze  stroke style: unknown obsolete format");
  m_thick = 0.0;
  is >> m_color >> m_parameter;
}

//-----------------------------------------------------------------------------

void TFriezeStrokeStyle2::computeData(Points &positions, const TStroke *stroke,
                                      const TColorFunction *cf) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  double ds = 0.5;

  positions.clear();
  positions.reserve(tceil((length + 1) / ds));

  double s     = 0.01;
  double lastS = 0;
  double phi   = 0;
  double lastW = 0;
  double thick = 1 - m_thick;
  vector<TPointD> points;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < lastW) {
      s += 0.1;
      continue;
    }
    lastW           = w;
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = normalize(stroke->getSpeed(w));
    TPointD v       = rotate90(u);

    double thickness = pos.thick;  // 5; //(1-t)*40 + t * 10;

    if (thickness > 0) {
      double omega = M_PI / thickness;

      double q        = 0.5 * (1 - cos(phi));
      double theta    = M_PI_2 - M_PI * m_parameter * q;
      double r        = thickness * sin(phi);
      double r1       = r * thick;
      double costheta = cos(theta);
      double sintheta = sin(theta);
      positions.push_back(pos + u * (r * costheta) + v * (r * sintheta));
      positions.push_back(pos + u * (r1 * costheta) + v * (r1 * sintheta));
      phi += (s - lastS) * omega;
      lastS = s;
    } else {
      positions.push_back(pos);
      positions.push_back(pos);
    }

    s += ds;
  }
}

//-----------------------------------------------------------------------------

void TFriezeStrokeStyle2::drawStroke(const TColorFunction *cf,
                                     Points &positions,
                                     const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;
  tglColor(color);

  glBegin(GL_QUAD_STRIP);
  int i = 0;
  for (; i < (int)positions.size(); i += 2) {
    tglVertex(positions[i]);
    tglVertex(positions[i + 1]);
  }
  glEnd();
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < (int)positions.size(); i += 2) {
    tglVertex(positions[i]);
  }
  glEnd();
  glBegin(GL_LINE_STRIP);
  for (i = 1; i < (int)positions.size() - 1; i += 2) {
    tglVertex(positions[i]);
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void TFriezeStrokeStyle2::drawStroke(TFlash &flash,
                                     const TStroke *stroke) const {
  // TStroke *stroke = getStroke();
  double length = stroke->getLength();

  double s     = 0.01;
  double lastS = 0;
  double phi   = 0;
  double lastW = 0;
  double thick = 1 - m_thick;
  vector<TPointD> points;
  while (s <= length) {
    double w = stroke->getParameterAtLength(s);
    if (w < lastW) {
      s += 0.1;
      continue;
    }
    lastW           = w;
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = normalize(stroke->getSpeed(w));
    TPointD v       = rotate90(u);

    double thickness = pos.thick;  // 5; //(1-t)*40 + t * 10;

    if (thickness > 0) {
      double omega = M_PI / thickness;

      double q        = 0.5 * (1 - cos(phi));
      double theta    = M_PI_2 - M_PI * m_parameter * q;
      double r        = thickness * sin(phi);
      double r1       = r * thick;
      double costheta = cos(theta);
      double sintheta = sin(theta);
      points.push_back(pos + u * (r * costheta) + v * (r * sintheta));
      points.push_back(pos + u * (r1 * costheta) + v * (r1 * sintheta));
      phi += (s - lastS) * omega;
      lastS = s;
    } else {
      points.push_back(pos);
      points.push_back(pos);
    }

    double ds = 0.5;
    s += ds;
  }

  // Polyline version
  flash.setThickness(0.0);
  flash.setFillColor(m_color);
  for (int i = 0; i < ((int)points.size() - 2); i += 2) {
    vector<TPointD> plv;
    plv.push_back(points[i]);
    plv.push_back(points[i + 1]);
    plv.push_back(points[i + 3]);
    plv.push_back(points[i + 2]);
    flash.drawPolyline(plv);
  }

  /*
vector<TPointD> pp;
for( int i=0; i<((int)points.size()-1); i+=2 )
    pp.push_back(TPointD((points[i]+points[i+1])*0.5));

// Quadratic version

if ( pp.size()<=2 ) {
    if ( pp.size()==2 ) {
       vector<TSegment> sv;
       sv.push_back(TSegment(pp[0],pp[1]));
           flash.setLineColor(m_color);
           flash.drawSegments(sv,false);
    }
    return;
}

vector<TQuadratic> qv;
qv.push_back(TQuadratic(pp[i],pp[i]*0.75+pp[i+1]*0.25,(pp[i]+pp[i+1])*0.5));
for( i=1; i<(int)(pp.size()-1); i++ ) {
   TPointD p0=((pp[i-1]+pp[i])*0.5);
   TPointD p1=pp[i];
   TPointD p2=((pp[i]+pp[i+1])*0.5);
   qv.push_back(TQuadratic(p0,p1,p2));
}
int n=pp.size()-1;
qv.push_back(TQuadratic((pp[n-1]+pp[n])*0.5,pp[n-1]*0.25+pp[n]*0.75,pp[n]));

flash.setLineColor(m_color);
flash.setThickness(m_thick);
flash.drawquads(qv);

*/
  // Segment version
  /*
flash.setThickness(m_thick);
flash.setLineColor(m_color);
  for( i=0; i<(int)(pp.size()-1); i++ ) {
  vector<TSegment> sv;
          sv.push_back(TSegment(pp[i],pp[i+1]));
  flash.drawSegments(sv,false);
  }
*/
}

//=============================================================================

TDualColorStrokeStyle2::TDualColorStrokeStyle2(TPixel32 color0, TPixel32 color1,
                                               double parameter)
    : m_color0(color0), m_color1(color1), m_parameter(parameter) {}

//-----------------------------------------------------------------------------

TColorStyle *TDualColorStrokeStyle2::clone() const {
  return new TDualColorStrokeStyle2(*this);
}

//-----------------------------------------------------------------------------

void TDualColorStrokeStyle2::computeOutline(
    const TStroke *stroke, TStrokeOutline &outline,
    TOutlineUtil::OutlineParameter param) const {
  param.m_lengthStep = m_parameter;
  TOutlineStyle::computeOutline(stroke, outline, param);
}

//-----------------------------------------------------------------------------

void TDualColorStrokeStyle2::drawStroke(const TColorFunction *cf,
                                        TStrokeOutline *outline,
                                        const TStroke *stroke) const {
  UINT i;
  const std::vector<TOutlinePoint> &v = outline->getArray();
  TPixel32 colorv[2];

  if (cf) {
    colorv[0] = (*(cf))(m_color0);
    colorv[1] = (*(cf))(m_color1);
  } else {
    colorv[0] = m_color0;
    colorv[1] = m_color1;
  }
  int colorindex = 0;
  if (v.empty()) return;
  glBegin(GL_LINE_STRIP);
  tglColor(colorv[0]);
  for (i = 0; i < v.size(); i += 2) {
    glVertex2dv(&v[i].x);
    if (0 != v[i].stepCount) {
      colorindex++;
      tglColor(colorv[colorindex & 1]);
      glVertex2dv(&v[i].x);
    }
  }
  glEnd();

  colorindex = 0;
  glBegin(GL_LINE_STRIP);
  tglColor(colorv[0]);
  for (i = 1; i < v.size(); i += 2) {
    glVertex2dv(&v[i].x);
    if (0 != v[i].stepCount) {
      colorindex++;
      tglColor(colorv[colorindex & 1]);
      glVertex2dv(&v[i].x);
    }
  }
  glEnd();

  colorindex = 0;
  glBegin(GL_QUAD_STRIP);
  tglColor(colorv[0]);
  for (i = 0; i < v.size(); i += 2) {
    glVertex2dv(&v[i].x);
    glVertex2dv(&v[i + 1].x);
    if (0 != v[i].stepCount) {
      colorindex++;
      tglColor(colorv[colorindex & 1]);
      glVertex2dv(&v[i].x);
      glVertex2dv(&v[i + 1].x);
    }
  }
  glEnd();

  // antialias delle linee normali
  tglColor(colorv[0]);
  for (i = 0; i < v.size(); i += 2) {
    if (0 != v[i].stepCount) {
      glBegin(GL_LINES);
      glVertex2dv(&v[i].x);
      glVertex2dv(&v[i + 1].x);
      glEnd();
    }
  }
}

//-----------------------------------------------------------------------------

void TDualColorStrokeStyle2::drawStroke(TFlash &flash,
                                        const TStroke *stroke) const {
  TOutlineUtil::OutlineParameter param;
  param.m_lengthStep = m_parameter;
  TStrokeOutline outline;
  TOutlineStyle::computeOutline(stroke, outline, param);
  const std::vector<TOutlinePoint> &v = outline.getArray();
  if (v.empty()) return;

  TPixel32 colorv[2] = {m_color0, m_color1};
  int colorindex     = 0;
  flash.setThickness(0.0);
  flash.setFillColor(TPixel32(0, 0, 0, 255));
  for (UINT i = 0; i < (v.size() - 2); i += 2) {
    vector<TPointD> tpv;
    tpv.push_back(TPointD(v[i].x, v[i].y));
    tpv.push_back(TPointD(v[i + 1].x, v[i + 1].y));
    tpv.push_back(TPointD(v[i + 3].x, v[i + 3].y));
    tpv.push_back(TPointD(v[i + 2].x, v[i + 2].y));

    if (0 != v[i].stepCount) {
      colorindex++;
      flash.setFillColor(colorv[colorindex & 1]);
    }
    flash.drawPolyline(tpv);

    /*  --- testing ---
vector<TSegment> s;
s.push_back(TSegment(tpv[0],tpv[1]));
s.push_back(TSegment(tpv[1],tpv[2]));
s.push_back(TSegment(tpv[2],tpv[3]));
s.push_back(TSegment(tpv[3],tpv[0]));
flash.drawSegments(s,false);
*/
  }
}

//-----------------------------------------------------------------------------

int TDualColorStrokeStyle2::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TDualColorStrokeStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TDualColorStrokeStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TDualColorStrokeStyle2", "Distance");
}

//-----------------------------------------------------------------------------

void TDualColorStrokeStyle2::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < 1);
  min = 1.0;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TDualColorStrokeStyle2::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < 1);
  return m_parameter;
}

//-----------------------------------------------------------------------------

void TDualColorStrokeStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_parameter = value;
  updateVersionNumber();  // questo si chiama per ogno cambiamento di parametro
                          // per cui di deve ricalcolare l'outline
}

//=============================================================================

TLongBlendStrokeStyle2::TLongBlendStrokeStyle2(TPixel32 color0, TPixel32 color1,
                                               double parameter)
    : m_color0(color0), m_color1(color1), m_parameter(parameter) {}

//-----------------------------------------------------------------------------

TColorStyle *TLongBlendStrokeStyle2::clone() const {
  return new TLongBlendStrokeStyle2(*this);
}

//-----------------------------------------------------------------------------

void TLongBlendStrokeStyle2::computeOutline(
    const TStroke *stroke, TStrokeOutline &outline,
    TOutlineUtil::OutlineParameter param) const {
  param.m_lengthStep = m_parameter;
  TOutlineStyle::computeOutline(stroke, outline, param);
}

//-----------------------------------------------------------------------------

void TLongBlendStrokeStyle2::drawStroke(const TColorFunction *cf,
                                        TStrokeOutline *outline,
                                        const TStroke *stroke) const {
  TPixel32 color0, color1;
  if (cf) {
    color0 = (*(cf))(m_color0);
    color1 = (*(cf))(m_color1);
  } else {
    color0 = m_color0;
    color1 = m_color1;
  }
  UINT i;

  const std::vector<TOutlinePoint> &v = outline->getArray();
  if (v.empty()) return;

  // outline with antialiasing
  glBegin(GL_LINE_STRIP);
  int mystepCount = 0;

  double totallength = stroke->getLength();
  double ntick       = totallength / m_parameter + 1;
  tglColor(color0);
  for (i = 0; i < v.size(); i += 2) {
    if (0 != v[i].stepCount) {
      tglColor(blend(color0, color1, (double)mystepCount / ntick));
      mystepCount++;
    }
    glVertex2dv(&v[i].x);
  }
  glEnd();

  glBegin(GL_LINE_STRIP);
  mystepCount = 0;
  tglColor(color0);
  for (i = 1; i < v.size(); i += 2) {
    if (0 != v[i].stepCount) {
      tglColor(blend(color0, color1, (double)mystepCount / ntick));
      mystepCount++;
    }
    glVertex2dv(&v[i].x);
  }
  glEnd();

  glBegin(GL_QUAD_STRIP);
  mystepCount = 0;
  tglColor(color0);
  for (i = 0; i < v.size(); i += 2) {
    if (0 != v[i].stepCount) {
      tglColor(blend(color0, color1, (double)mystepCount / ntick));
      mystepCount++;
    }
    glVertex2dv(&v[i].x);
    glVertex2dv(&v[i + 1].x);
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void TLongBlendStrokeStyle2::drawStroke(TFlash &flash,
                                        const TStroke *stroke) const {
  TPixel32 color0, color1;
  color0 = m_color0;
  color1 = m_color1;

  UINT i;
  TOutlineUtil::OutlineParameter param;
  double lParameter =
      m_parameter >= 20.0 || m_parameter < 0.0 ? 10.0 : m_parameter;
  param.m_lengthStep = lParameter;
  TStrokeOutline outline;
  TOutlineStyle::computeOutline(stroke, outline, param);
  const std::vector<TOutlinePoint> &v = outline.getArray();
  if (v.empty()) return;

  flash.setThickness(0.0);

  int mystepCount    = 0;
  double totallength = stroke->getLength();
  double ntick       = totallength / lParameter + 1;
  SFlashUtils sfu;
  TPixel32 col0, col1;
  col0 = col1 = color0;
  for (i = 2; i < v.size(); i += 2) {
    if (0 != v[i].stepCount) {
      col1 = blend(color0, color1, (double)mystepCount / ntick);
      mystepCount++;
    }
    vector<TPointD> plv;
    plv.push_back(TPointD(v[i - 2].x, v[i - 2].y));
    plv.push_back(TPointD(v[i - 1].x, v[i - 1].y));
    plv.push_back(TPointD(v[i + 1].x, v[i + 1].y));
    plv.push_back(TPointD(v[i].x, v[i].y));
    sfu.drawGradedPolyline(flash, plv, col0, col1);
    col0 = col1;
  }
}

//-----------------------------------------------------------------------------

int TLongBlendStrokeStyle2::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TLongBlendStrokeStyle2::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TLongBlendStrokeStyle2::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  return QCoreApplication::translate("TLongBlendStrokeStyle2",
                                     "Distance");  // W_Watercolor_Distance
}

//-----------------------------------------------------------------------------

void TLongBlendStrokeStyle2::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < 1);
  min = 0.1;
  max = 100.0;
}

//-----------------------------------------------------------------------------

double TLongBlendStrokeStyle2::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < 1);
  return m_parameter;
}

//-----------------------------------------------------------------------------

void TLongBlendStrokeStyle2::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);
  m_parameter = value;
  updateVersionNumber();  // questo si chiama per ogno cambiamento di parametro
                          // per cui di deve ricalcolare l'outline
}

//=============================================================================

#ifdef _DEBUG

OutlineViewerStyle::OutlineViewerStyle(TPixel32 color, double parameter0,
                                       double parameter1, double parameter2,
                                       double parameter3)
    : TSolidColorStyle(color)
    , m_boolPar(false)
    , m_intPar(1)
    , m_enumPar(2)
    , m_pathPar("testPath") {
  m_parameter[0] = parameter0;
  m_parameter[1] = parameter1;
  m_parameter[2] = parameter2;
  m_parameter[3] = parameter3;
}

//-----------------------------------------------------------------------------

TColorStyle *OutlineViewerStyle::clone() const {
  return new OutlineViewerStyle(*this);
}

//-----------------------------------------------------------------------------

int OutlineViewerStyle::getParamCount() const { return 8; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType OutlineViewerStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  static const TColorStyle::ParamType types[8] = {
      TColorStyle::DOUBLE, TColorStyle::DOUBLE,  TColorStyle::DOUBLE,
      TColorStyle::DOUBLE, TColorStyle::BOOL,    TColorStyle::INT,
      TColorStyle::ENUM,   TColorStyle::FILEPATH};

  return types[index];
}

//-----------------------------------------------------------------------------

QString OutlineViewerStyle::getParamNames(int index) const {
  assert(0 <= index && index < 8);

  switch (index) {
  case 0:
    return QCoreApplication::translate("OutlineViewerStyle", "Control Point");
  case 1:
    return QCoreApplication::translate("OutlineViewerStyle", "Center Line");
  case 2:
    return QCoreApplication::translate("OutlineViewerStyle", "Outline Mode");
  case 3:
    return QCoreApplication::translate("OutlineViewerStyle", "Distance");
  case 4:
    return "Bool";
  case 5:
    return "Int";
  case 6:
    return "Enum";
  case 7:
    return "Path";
  }

  assert(0);
  return QCoreApplication::translate("OutlineViewerStyle", "distance");
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::getParamRange(int index, double &min,
                                       double &max) const {
  assert(0 <= index && index < 4);
  switch (index) {
  case 0:
    min = 0.0;
    max = 2.99;
    break;
  case 1:
    min = 0.0;
    max = 1.99;
    break;
  case 2:
    min = 0.0;
    max = 3.99;
    break;
  case 3:
    min = 3.0;
    max = 100.0;
    break;
  }
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::getParamRange(int index, int &min, int &max) const {
  assert(5 <= index && index < 7);
  switch (index) {
  case 5:
    min = 0, max = 10;
    break;
  case 6:
    min = 0, max = 4;
    break;
  }
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::getParamRange(int index,
                                       QStringList &enumItems) const {
  enumItems << "Prova 1"
            << "Prova 2"
            << "Prova 3"
            << "Prova 4";
}

//-----------------------------------------------------------------------------

bool OutlineViewerStyle::getParamValue(TColorStyle::bool_tag, int index) const {
  assert(index == 4);
  return m_boolPar;
}

//-----------------------------------------------------------------------------

int OutlineViewerStyle::getParamValue(TColorStyle::int_tag, int index) const {
  assert(5 <= index && index < 7);
  return (index == 5) ? m_intPar : m_enumPar;
}

//-----------------------------------------------------------------------------

double OutlineViewerStyle::getParamValue(TColorStyle::double_tag,
                                         int index) const {
  assert(0 <= index && index < 4);
  return m_parameter[index];
}

//-----------------------------------------------------------------------------

TFilePath OutlineViewerStyle::getParamValue(TColorStyle::TFilePath_tag,
                                            int index) const {
  assert(index == 7);
  return m_pathPar;
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::setParamValue(int index, bool value) {
  assert(index == 4);
  m_boolPar = value;
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::setParamValue(int index, int value) {
  assert(5 <= index && index < 7);
  (index == 5) ? m_intPar = value : m_enumPar = value;
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 4);

  if (index >= 2 && (int)m_parameter[index] != (int)value)  // cambia l'outline
  {
    updateVersionNumber();
  }

  m_parameter[index] = value;
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::setParamValue(int index, const TFilePath &value) {
  assert(index == 7);
  m_pathPar = value;
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::loadData(TInputStreamInterface &is) {
  TPixel32 color;
  is >> color;
  TSolidColorStyle::setMainColor(color);
  is >> m_parameter[0];
  is >> m_parameter[1];
  is >> m_parameter[2];
  is >> m_parameter[3];

  int boolPar;
  is >> boolPar;
  m_boolPar = boolPar;
  is >> m_intPar;
  is >> m_enumPar;
  std::string str;
  is >> str;
  m_pathPar = TFilePath(::to_wstring(str));
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::saveData(TOutputStreamInterface &os) const {
  os << TSolidColorStyle::getMainColor();
  os << m_parameter[0];
  os << m_parameter[1];
  os << m_parameter[2];
  os << m_parameter[3];

  os << int(m_boolPar);
  os << m_intPar;
  os << m_enumPar;
  os << ::to_string(m_pathPar.getWideString());
}

//-----------------------------------------------------------------------------

void OutlineViewerStyle::computeOutline(
    const TStroke *stroke, TStrokeOutline &outline,
    TOutlineUtil::OutlineParameter param) const {
  if (m_parameter[2] >= 1.0) {
    param.m_lengthStep = (m_parameter[2] >= 3.0) ? m_parameter[3] : 0;

    TOutlineStyle::computeOutline(stroke, outline, param);
  }
}

//-----------------------------------------------------------------------------
namespace {

void drawOutline(TStrokeOutline *outline, bool cut) {
  const std::vector<TOutlinePoint> &v = outline->getArray();

  if (v.empty()) return;

  UINT i;
  // outline with antialiasing
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < v.size(); i += 2) glVertex2dv(&v[i].x);
  glEnd();

  glBegin(GL_LINE_STRIP);
  for (i = 1; i < v.size(); i += 2) glVertex2dv(&v[i].x);
  glEnd();

  if (cut) {
    static const int stride = sizeof(TOutlinePoint);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_DOUBLE, stride, &v[0]);
    glDrawArrays(GL_LINES, 0, v.size());
    glDisableClientState(GL_VERTEX_ARRAY);
  }
}

void drawControlPoints(const TStroke *stroke, bool allPoints) {
  int i;
  TPointD p;
  glPointSize(4.0);
  glBegin(GL_POINTS);

  if (allPoints) {
    int n = stroke->getControlPointCount();
    for (i = 0; i < n; ++i) {
      p = stroke->getControlPoint(i);
      glColor3d((i + 1) & 1, i & 1, 0.0);
      glVertex2d(p.x, p.y);
    }
  } else {
    int n = stroke->getChunkCount();
    for (i = 0; i < n; ++i) {
      const TThickQuadratic *chunk = stroke->getChunk(i);
      p                            = chunk->getP0();
      glColor3d(1.0, 0.0, 0.0);
      glVertex2d(p.x, p.y);
    }
    const TThickQuadratic *chunk = stroke->getChunk(n - 1);
    glColor3d(1.0, 0.0, 0.0);
    p = chunk->getP2();
    glVertex2d(p.x, p.y);
  }

  glEnd();
}

void drawCenterline(const TStroke *stroke) {
  glBegin(GL_LINE_STRIP);
  int n = stroke->getChunkCount();
  int i = 0;
  for (i = 0; i < n; ++i) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    double length                = chunk->getLength(0, 1);
    int maxCount  = std::max(tceil(length / (5 * sqrt(tglGetPixelSize2()))), 1);
    double deltaT = 1.0 / maxCount;
    double t      = 0;
    for (t = 0; t < 1 + (deltaT / 2); t += deltaT) {
      TPointD point = chunk->getPoint(t);
      glVertex2d(point.x, point.y);
    }
  }
  glEnd();
  return;
}
}
//------------------------------------------------------------

void OutlineViewerStyle::drawStroke(const TColorFunction *cf,
                                    TStrokeOutline *outline,
                                    const TStroke *stroke) const {
  TPixel32 color;
  if (cf)
    color = (*(cf))(getMainColor());
  else
    color = getMainColor();

  tglColor(color);

  if (m_parameter[1] >= 1.0) drawCenterline(stroke);

  if (m_parameter[2] >= 1.0) drawOutline(outline, m_parameter[2] >= 2.0);

  if (m_parameter[0] >= 1.0) drawControlPoints(stroke, m_parameter[0] >= 2.0);
}

#endif

//=============================================================================

TMatrioskaStrokeProp::TMatrioskaStrokeProp(const TStroke *stroke,
                                           TMatrioskaStrokeStyle *style)
    : TStrokeProp(stroke)
    , m_colorStyle(style)
    , m_outline()
    , m_outlinePixelSize(0) {
  m_styleVersionNumber = m_colorStyle->getVersionNumber();
}

//-----------------------------------------------------------------------------

TStrokeProp *TMatrioskaStrokeProp::clone(const TStroke *stroke) const {
  TMatrioskaStrokeProp *prop = new TMatrioskaStrokeProp(stroke, m_colorStyle);
  prop->m_strokeChanged      = m_strokeChanged;
  prop->m_outline            = m_outline;
  prop->m_outlinePixelSize   = m_outlinePixelSize;
  return prop;
}

//-----------------------------------------------------------------------------

const TColorStyle *TMatrioskaStrokeProp::getColorStyle() const {
  return m_colorStyle;
}

//-----------------------------------------------------------------------------

namespace {
void recomputeStrokes(const TStroke *stroke, vector<TStroke *> &strokes,
                      int strokeNumber) {
  clearPointerContainer(strokes);

  strokes.resize(strokeNumber);

  int nCP = stroke->getControlPointCount();

  double reduction;

  for (int strokeId = 0; strokeId != strokeNumber; strokeId++) {
    strokes[strokeId] = new TStroke(*stroke);

    reduction = ((double)strokeId + 0.5) / (double)(strokeNumber + 0.5);

    for (int i = 0; i < nCP; i++) {
      TThickPoint tp = strokes[strokeId]->getControlPoint(i);
      tp.thick *= reduction;
      strokes[strokeId]->setControlPoint(i, tp);
    }
  }
}

void recomputeOutlines(const TStroke *stroke, vector<TStroke *> &strokes,
                       vector<TStrokeOutline> &outlines,
                       const TSolidColorStyle *style) {
  TOutlineUtil::OutlineParameter param;
  int strokeNumber = strokes.size();
  outlines.resize(strokeNumber + 1);
  int strokeId;
  for (strokeId = 0; strokeId != strokeNumber; strokeId++) {
    outlines[strokeId].getArray().clear();
    style->computeOutline(strokes[strokeId], outlines[strokeId], param);
  }
  outlines[strokeId].getArray().clear();
  style->computeOutline(stroke, outlines[strokeId], param);
}
};

void TMatrioskaStrokeProp::draw(const TVectorRenderData &rd) {
  if (rd.m_clippingRect != TRect() && !rd.m_is3dView &&
      !convert(rd.m_aff * m_stroke->getBBox()).overlaps(rd.m_clippingRect))
    return;

  int strokeId;
  glPushMatrix();
  tglMultMatrix(rd.m_aff);

  double pixelSize = sqrt(tglGetPixelSize2());
  int strokeNumber =
      (int)(m_colorStyle->getParamValue(TColorStyle::double_tag(), 0)) - 1;

  if (m_strokeChanged || (UINT)strokeNumber != m_appStrokes.size()) {
    m_strokeChanged    = false;
    m_outlinePixelSize = pixelSize;
    recomputeStrokes(m_stroke, m_appStrokes, strokeNumber);
    recomputeOutlines(m_stroke, m_appStrokes, m_outline, m_colorStyle);
  } else if (!isAlmostZero(pixelSize - m_outlinePixelSize, 1e-5)
             //      || m_styleVersionNumber != m_colorStyle->getVersionNumber()
             ) {
    m_strokeChanged    = false;
    m_outlinePixelSize = pixelSize;
    recomputeOutlines(m_stroke, m_appStrokes, m_outline, m_colorStyle);
  }

  m_colorStyle->drawStroke(rd.m_cf, &m_outline[m_appStrokes.size()], m_stroke);

  TSolidColorStyle appStyle(m_colorStyle->getColorParamValue(1));

  //  if(m_colorStyle->isAlternate())
  //  {

  for (strokeId = strokeNumber - 1; strokeId >= 0; strokeId--) {
    if ((m_appStrokes.size() - strokeId) & 1)
      appStyle.drawStroke(rd.m_cf, &m_outline[strokeId],
                          m_appStrokes[strokeId]);
    else
      m_colorStyle->drawStroke(rd.m_cf, &m_outline[strokeId],
                               m_appStrokes[strokeId]);
  }

  //  }
  //  else
  //  {
  //    TPixel32 color0=m_colorStyle->getColorParamValue(0);
  //    TPixel32 color1=m_colorStyle->getColorParamValue(1);

  //    for(strokeId=strokeNumber-1; strokeId>=0;strokeId--)
  //    {
  //      double r2=(double)strokeId/(double)(strokeNumber);
  //      double r1=1-r2;
  //      TPixel32  color((int)(color0.r*r2+color1.r*r1),
  //                    (int)(color0.g*r2+color1.g*r1),
  //                    (int)(color0.b*r2+color1.b*r1),
  //                    (int)(color0.m*r2+color1.m*r1)
  //                    );

  //      appStyle.setMainColor(color);
  //      appStyle.drawStroke(rd.m_cf,
  //      &m_outline[strokeId],m_appStrokes[strokeId]);
  //    }

  //  }

  glPopMatrix();
}

//------------------------------------------------------------------------------------------

void TMatrioskaStrokeProp::draw(TFlash &flash) {
  int strokeId,
      strokeNumber =
          (int)(m_colorStyle->getParamValue(TColorStyle::double_tag(), 0)) - 1;
  if ((UINT)strokeNumber != m_appStrokes.size()) {
    recomputeStrokes(m_stroke, m_appStrokes, strokeNumber);
  }

  m_colorStyle->TOutlineStyle::drawStroke(flash, m_stroke);

  TSolidColorStyle appStyle(m_colorStyle->getColorParamValue(1));

  for (strokeId = strokeNumber - 1; strokeId >= 0; strokeId--) {
    if ((m_appStrokes.size() - strokeId) & 1)
      appStyle.TOutlineStyle::drawStroke(flash, m_appStrokes[strokeId]);
    else
      m_colorStyle->TOutlineStyle::drawStroke(flash, m_appStrokes[strokeId]);
  }
}

//-----------------------------------------------------------------------------

TMatrioskaStrokeProp::~TMatrioskaStrokeProp() {
  clearPointerContainer(m_appStrokes);
}

//=============================================================================

TMatrioskaStrokeStyle::TMatrioskaStrokeStyle(TPixel32 color1, TPixel32 color2,
                                             double parameter, bool alternate)
    : TSolidColorStyle(color1), m_color2(color2), m_parameter(parameter) {}

//-----------------------------------------------------------------------------

TColorStyle *TMatrioskaStrokeStyle::clone() const {
  return new TMatrioskaStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

TStrokeProp *TMatrioskaStrokeStyle::makeStrokeProp(const TStroke *stroke) {
  return new TMatrioskaStrokeProp(stroke, this);
}

//-----------------------------------------------------------------------------

int TMatrioskaStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TMatrioskaStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TMatrioskaStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < 1);

  //  if(index)
  //    return "alter/gradual";
  //  else

  return QCoreApplication::translate("TMatrioskaStrokeStyle", "Stripes");
}

//-----------------------------------------------------------------------------

void TMatrioskaStrokeStyle::getParamRange(int index, double &min,
                                          double &max) const {
  assert(0 <= index && index < 1);

  //  if(index)
  //  {
  //    min = 0;
  //    max = 1;
  //  }
  //  else
  //  {

  min = 1.0;
  max = 10.0;
  //}
}

//-----------------------------------------------------------------------------

double TMatrioskaStrokeStyle::getParamValue(TColorStyle::double_tag,
                                            int index) const {
  assert(0 <= index && index < 1);

  //  if(index)
  //    return (m_alternate)? 0 : 1;
  //  else

  return m_parameter;
}

//-----------------------------------------------------------------------------

void TMatrioskaStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < 1);

  //  if(index)
  //    m_alternate = value<0.5;
  //  else

  m_parameter = value;

  updateVersionNumber();
}

//-----------------------------------------------------------------------------

TPixel32 TMatrioskaStrokeStyle::getColorParamValue(int index) const {
  assert(0 <= index && index < 2);
  TPixel32 tmp;
  switch (index) {
  case (0):
    tmp = TSolidColorStyle::getMainColor();
    break;
  case (1):
    tmp = m_color2;
    break;
  }
  return tmp;
}

//-----------------------------------------------------------------------------

void TMatrioskaStrokeStyle::setColorParamValue(int index,
                                               const TPixel32 &color) {
  assert(0 <= index && index < 2);
  switch (index) {
  case (0):
    TSolidColorStyle::setMainColor(color);
    break;
  case (1):
    m_color2 = color;
    break;
  }
}

//-----------------------------------------------------------------------------
void TMatrioskaStrokeStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);
  is >> m_parameter;
  is >> m_color2;
}

//------------------------------------------------------------

void TMatrioskaStrokeStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_parameter;
  os << m_color2;
}
