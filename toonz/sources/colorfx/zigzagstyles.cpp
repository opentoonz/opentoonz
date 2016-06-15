

/*test*/
#include "zigzagstyles.h"
//#include "tstrokeoutline.h"
#include "trandom.h"
//#include "tgl.h"

#ifdef POICIPENSO

TZigzagStrokeStyle::TZigzagStrokeStyle(const TPixel32 &color)
    : m_color(color), m_density(0.5) {}

//-----------------------------------------------------------------------------

TZigzagStrokeStyle::TZigzagStrokeStyle()
    : m_color(TPixel32(0, 0, 0, 255)), m_density(0.5) {}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::draw(double pixelSize, const TColorFunction *) {
  // Distance between sampling points
  const double minDist = 0.5;
  const double maxDist = 6.0;
  // Rotation angle of tangent
  const int minRotAngle = 0;
  const int maxRotAngle = 10;
  // e.g minimum translation length is the half of the thickness
  const double minTranslLength = 0.7;

  TStroke *stroke = getStroke();
  if (!stroke) return;
  double length = stroke->getLength();
  if (length <= 0) return;

  int count =
      (int)(length / (minDist + (maxDist - minDist) * (1.0 - m_density)) + 1);
  double dist = length / count;

  TRandom rnd;
  //  glEnable(GL_BLEND);
  tglColor(m_color);
  glBegin(GL_LINE_STRIP);
  int first = 1;
  for (double s = 0; s < (length + dist); s += dist, first = -first) {
    double w        = stroke->getParameterAtLength(s);
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD u       = stroke->getSpeed(w);
    if (norm2(u) < TConsts::epsilon) continue;
    u         = normalize(u);
    int angle = rnd.getInt(minRotAngle, maxRotAngle);
    TRotation rotM(angle);
    u                      = rotM * u;
    double maxTranslLength = angle == 90 ? 1.0 : 2.0;
    if (angle > 30 && angle < 90) {
      double rta      = 1.0 / tan(degree2rad(angle));
      maxTranslLength = sqrt(sq(rta) + 1.0);
    }
    double r = (minTranslLength +
                (maxTranslLength - minTranslLength) * rnd.getDouble()) *
               pos.thick * first;
    glVertex(pos + r * u);
  }
  glEnd();
  //  glDisable(GL_BLEND);
  //  glColor4d(0,0,0,1);
}

//-----------------------------------------------------------------------------

void TZigzagStrokeStyle::changeParameter(double delta) {
  m_density = tcrop(m_density + delta, 0.0, 1.0);
}

//-----------------------------------------------------------------------------

TStrokeStyle *TZigzagStrokeStyle::clone() const {
  TColorStyle *cs = new TZigzagStrokeStyle(*this);
  cs->assignNames(this);
  return cs;
}

//=============================================================================

TImageBasedZigzagStrokeStyle::TImageBasedZigzagStrokeStyle(
    const TPixel32 &color)
    : m_color(color)
    , m_textScale(1.0)
    , m_texture(0)

{}

//-----------------------------------------------------------------------------

TImageBasedZigzagStrokeStyle::TImageBasedZigzagStrokeStyle()
    : m_color(TPixel32(0, 0, 0, 255)), m_textScale(1.0), m_texture(0) {}

//-----------------------------------------------------------------------------

TImageBasedZigzagStrokeStyle::TImageBasedZigzagStrokeStyle(
    const TRaster32P texture)
    : m_color(TPixel32(0, 0, 0, 255)), m_textScale(1.0), m_texture(texture) {}

//-----------------------------------------------------------------------------

inline int TImageBasedZigzagStrokeStyle::realTextCoord(const int a,
                                                       const int l) const {
  return a == 0 ? 0 : (a > 0 ? a % l : l - 1 - ((-a) % l));
}

//-----------------------------------------------------------------------------

void TImageBasedZigzagStrokeStyle::draw(double pixelSize,
                                        const TColorFunction *) {
  // Distance between sampling points
  const double dist = 1.0;
  // Scaling of texture
  double recScale =
      fabs(m_textScale) < TConsts::epsilon ? 1.0 : 1.0 / m_textScale;

  TStroke *stroke = getStroke();
  if (!stroke) return;
  double length = stroke->getLength();
  if (length <= 0) return;

  TRandom rnd;
  //  glEnable(GL_BLEND);
  glColor(m_color);
  glBegin(GL_LINE_STRIP);
  TPointD textC   = m_texture->getCenterD();
  TRectD strokeBB = stroke->getBBox();
  TPointD strokeC((strokeBB.x0 + strokeBB.x1) / 2,
                  (strokeBB.y0 + strokeBB.y1) / 2);
  for (double s = 0; s < (length + dist); s += dist) {
    double w        = stroke->getParameterAtLength(s);
    TThickPoint pos = stroke->getThickPoint(w);
    TPointD textPos((pos.x - strokeC.x) * recScale + textC.x,
                    (pos.y - strokeC.y) * recScale + textC.y);
    TPointD u = stroke->getSpeed(w);
    if (norm2(u) == 0) continue;
    u = normalize(u);

    TPoint iTextPos(tround(textPos.x), tround(textPos.y));
    iTextPos.x     = realTextCoord(iTextPos.x, m_texture->getLx());
    iTextPos.y     = realTextCoord(iTextPos.y, m_texture->getLy());
    TPixel32 color = m_texture->pixels(iTextPos.y)[iTextPos.x];
    double ch, cl, cs;
    RGB2HLS(color.r / 255.0, color.g / 255.0, color.b / 255.0, &ch, &cl, &cs);
    TRotation rotM(tround(ch));
    u        = rotM * u;
    double r = cl * pos.thick;
    glVertex(pos + r * u);
  }
  glEnd();
  //  glDisable(GL_BLEND);
  //  glColor4d(0,0,0,1);
}

//-----------------------------------------------------------------------------

void TImageBasedZigzagStrokeStyle::changeParameter(double delta) {
  m_textScale = tcrop(m_textScale + delta, 0.1, 2.5);
}

//-----------------------------------------------------------------------------

TStrokeStyle *TImageBasedZigzagStrokeStyle::clone() const {
  TColorStyle *cs = new TImageBasedZigzagStrokeStyle(*this);
  cs->assignNames(this);
  return cs;
}

#endif
