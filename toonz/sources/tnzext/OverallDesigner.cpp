#include "ext/OverallDesigner.h"
//#include "ext/StrokeParametricDeformer.h"
#include "ext/StrokeDeformation.h"
#include "ext/SmoothDeformation.h"
#include "ext/CornerDeformation.h"
#include "ext/StraightCornerDeformation.h"
#include "ext/Selector.h"
#include "ext/ContextStatus.h"

#include "tcurves.h"

#include <tstroke.h>
#include <tgl.h>
#include <tgeometry.h>
#include <drawutil.h>
#include <sstream>

/*
#ifdef  _DEBUG
#undef  _DEBUG
#define RESTORE_DEBUG
#endif
//*/

//-----------------------------------------------------------------------------

namespace {
void extglDrawText(const TPointD &p, const std::string &s,
                   void *character = GLUT_BITMAP_TIMES_ROMAN_10) {
  glPushMatrix();
  glTranslated(p.x, p.y, 0);
  if (character <= GLUT_STROKE_MONO_ROMAN) {
    double factor = 0.07;
    glScaled(factor, factor, factor);
    for (int i = 0; i < (int)s.size(); i++)
      glutStrokeCharacter(character, s[i]);
  } else
    for (int i = 0; i < (int)s.size(); i++)
      glutBitmapCharacter(character, s[i]);

  glPopMatrix();
}

//---------------------------------------------------------------------------

void drawSign(const TPointD &p, const TPointD &v, double size) {
  if (size == 0.0) return;
  size = fabs(size);

  TPointD v90  = rotate90(v);
  TPointD v270 = rotate270(v);

  glBegin(GL_LINES);
  glVertex2d(p.x, p.y);
  v90 = normalize(v90);
  v90 = v90 * size;
  v90 = p + v90;
  glVertex2d(v90.x, v90.y);
  glEnd();
  glBegin(GL_LINES);
  glVertex2d(p.x, p.y);
  v270 = normalize(v270);
  v270 = v270 * size;
  v270 = p + v270;
  glVertex2d(v270.x, v270.y);
  glEnd();
}

//---------------------------------------------------------------------------

void drawCross(const TPointD &p, double size) {
  TPointD v1 = TPointD(1, 1);
  TPointD v2 = TPointD(1, -1);

  drawSign(p, v1, size);
  drawSign(p, v2, size);
}

//---------------------------------------------------------------------------

/**
   * Try to verify if it is possible to call
   * some method of this stroke.
   */
bool isValid(const TStroke *s) {
  if (!s) return false;
  const int cpCount = s->getControlPointCount();
  int i;
  for (i               = 0; i < cpCount; ++i) s->getControlPoint(i);
  const int chunkCount = s->getChunkCount();
  for (i = 0; i < chunkCount; ++i) s->getChunk(i);
  return true;
}

//---------------------------------------------------------------------------

void showCP(const TStroke *s, double pixelSize) {
  if (!s) return;

  // show control points
  const int countCP = s->getControlPointCount();
  TThickPoint p1;

  for (int i = 0; i < countCP; ++i) {
    p1 = s->getControlPoint(i);
    if (i & 1)
      glColor3d(1.0, 0.0, 0.0);
    else
      glColor3d(1.0, 1.0, 0.0);

    drawCross(convert(p1), p1.thick * pixelSize);
    tglDrawCircle(convert(p1), p1.thick * pixelSize);
  }
}

//---------------------------------------------------------------------------

void drawStrokeCenterLine(const TStroke *stroke, double pixelSize,
                          const ToonzExt::Interval &vals) {
  double from = vals.first, to = vals.second;

  if (!stroke || pixelSize < 0.0) return;

  from = std::max(std::min(from, 1.0), 0.0);
  to   = std::max(std::min(to, 1.0), 0.0);

  if (from < to) {
    drawStrokeCenterline(*stroke, pixelSize, from, to);
  } else {
    drawStrokeCenterline(*stroke, pixelSize, from, 1.0);
    drawStrokeCenterline(*stroke, pixelSize, 0.0, to);
  }
  tglDrawDisk(stroke->getPoint(from), 5.0 * pixelSize);
  tglDrawDisk(stroke->getPoint(to), 5.0 * pixelSize);

#ifdef _DEBUG
  glColor3d(1.0, 0.0, 0.0);
  tglDrawDisk(stroke->getPoint(0.0), 9.0 * pixelSize);
#endif
}

//---------------------------------------------------------------------------

void showCorners(const TStroke *s, int cornerSize, double pixelSize) {
  if (!s) return;

  const TPointD offset(20, 20);

  // show corners
  std::vector<double> corners;

  ToonzExt::cornersDetector(s, cornerSize, corners);
  if (corners.empty()) return;

  int i, size = corners.size();

  glColor3d(0.8, 1.0, 0.3);
  // find interval with corner like extremes
  for (i = 0; i < size; ++i) {
    double tmp   = corners[i];
    TPointD pnt  = s->getPoint(tmp);
    double large = 5.0;
    drawCross(pnt, large * pixelSize);

    glColor3d(0.7, 0.7, 0.7);
    std::ostringstream oss;
    oss << "[" << tmp << "]";
    extglDrawText(TPointD(pnt.x, pnt.y) + offset, oss.str());
  }

  corners.clear();

  ToonzExt::straightCornersDetector(s, corners);
  if (corners.empty()) return;

  size = corners.size();
  glColor3d(0.8, 1.0, 0.3);
  // find interval with corner like extremes
  for (i = 0; i < size; ++i) {
    double tmp   = corners[i];
    TPointD pnt  = s->getPoint(tmp);
    double large = 5.0;
    drawCross(pnt, large * pixelSize);

    glColor3d(0.7, 0.2, 0.2);
    std::ostringstream oss;
    oss << "[" << tmp << "]";
    extglDrawText(TPointD(pnt.x, pnt.y) - offset, oss.str());
  }
}
}

//-----------------------------------------------------------------------------

ToonzExt::OverallDesigner::OverallDesigner(int x, int y) : x_(x), y_(y) {
  pixelSize_ = sqrt(this->getPixelSize2());
  scale_     = pixelSize_ != 0.0 ? pixelSize_ : 1.0;
}

//-----------------------------------------------------------------------------

ToonzExt::OverallDesigner::~OverallDesigner() {}

//-----------------------------------------------------------------------------

void ToonzExt::OverallDesigner::draw(ToonzExt::SmoothDeformation *sd) {
#ifdef _DEBUG
  glPushMatrix();
  {
    this->setPosition();
    TCubic c;
    c.setP0(TPointD(0.0, 0.0));
    c.setP1(TPointD(4.0, 12.0));
    c.setP2(TPointD(12.0, 12.0));
    c.setP3(TPointD(16.0, 0.0));
    glColor3d(1.0, 0.0, 1.0);
    tglDraw(c, 100, GL_LINE);
  }
  glPopMatrix();
#endif
}

//-----------------------------------------------------------------------------

void ToonzExt::OverallDesigner::draw(ToonzExt::CornerDeformation *sd) {
#ifdef _DEBUG
  glPushMatrix();
  {
    this->setPosition();
    TCubic c;
    c.setP0(TPointD(0.0, 0.0));
    c.setP1(TPointD(0.0, 12.0));
    c.setP2(TPointD(6.0, 12.0));
    c.setP3(TPointD(12.0, 12.0));
    glColor3d(1.0, 0.0, 1.0);
    tglDraw(c, 100, GL_LINE);
    c.setP0(TPointD(12.0, 12.0));
    c.setP1(TPointD(6.0, 8.0));
    c.setP2(TPointD(6.0, 4.0));
    c.setP3(TPointD(12.0, 0.0));
    tglDraw(c, 100, GL_LINE);
  }
  glPopMatrix();
#endif
}

//-----------------------------------------------------------------------------

void ToonzExt::OverallDesigner::draw(ToonzExt::StraightCornerDeformation *sd) {
#ifdef _DEBUG
  glPushMatrix();
  {
    this->setPosition();
    glColor3d(0.0, 1.0, 1.0);
    glBegin(GL_LINE_STRIP);
    tglVertex(TPointD(0.0, 0.0));
    tglVertex(TPointD(8.0, 12.0));
    tglVertex(TPointD(16.0, 0.0));
    glEnd();
  }
  glPopMatrix();
#endif
}

//-----------------------------------------------------------------------------

void ToonzExt::OverallDesigner::draw(ToonzExt::StrokeDeformation *sd) {
  if (sd) {
    const TStroke *s;

    // glColor3d(1.0,0.0,1.0);
    s = sd->getCopiedStroke();
    if (s) {
      const ContextStatus *status = sd->getStatus();
      double w = 0.0, pixelSize = 1.0;

      if (status) {
        w         = status->w_;
        pixelSize = status->pixelSize_ < 0 ? 1.0 : status->pixelSize_;
      }
#ifdef _DEBUG
      drawCross(s->getPoint(0), 2 * pixelSize);

      tglDrawCircle(s->getPoint(w), 8 * pixelSize);
      drawCross(s->getPoint(w), 8 * pixelSize);
#endif
      ToonzExt::Interval ex = sd->getExtremes();
      drawStrokeCenterLine(s, pixelSize_, ex);
      if (status) {
#ifdef _DEBUG
        glColor3d(0, 0, 0);
        showCorners(s, status->cornerSize_, pixelSize);

        glColor3d(0, 0, 0);
        TPointD offset = normalize(TPointD(1.0, 1.0)) * 20.0;
        std::ostringstream oss;
        oss << "(" << this->x_ << "," << this->y_ << ")\n{" << w << ",{"
            << sd->getExtremes().first << "," << sd->getExtremes().second
            << "}}";
        extglDrawText(TPointD(x_, y_) + offset, oss.str());

        glColor3d(0.5, 1.0, 0.5);
        showCP(s, pixelSize);
#endif
      }
    }

    /*
glColor3d(1.0,1.0,0.0);
s = sd->getStroke();
if(s)
{
drawCross( s->getPoint(0),
     4);

drawStrokeCenterLine(s,
               pixelSize_,
               ToonzExt::Interval(0,1));
}
*/

    s = sd->getTransformedStroke();

    glColor3d(1.0, 0.0, 0.0);
    if (s) {
#ifdef _DEBUG
      isValid(s);
#endif
      drawStrokeCenterline(*s, pixelSize_);
    }

#ifdef _DEBUG
    {
      const TStroke *c = sd->getCopiedStroke(), *s = sd->getStroke();
      if (c && s) {
        // glColor3d(1,1,0);
        // tglDrawDisk(s->getPoint(0.0),
        //            5*pixelSize_);

        int count =
            std::min(c->getControlPointCount(), s->getControlPointCount());
        for (int i = 0; i < count; ++i) {
          TThickPoint ccp = c->getControlPoint(i);
          TThickPoint scp = s->getControlPoint(i);
        }
      }
    }
#endif
  }
}

//-----------------------------------------------------------------------------

void ToonzExt::OverallDesigner::setPosition() {
  TPointD offset(1, -1);
  offset = normalize(offset) * (20.0 * scale_);
  glTranslated(x_ + offset.x, y_ + offset.y, 0.0);
  glScalef(scale_, scale_, scale_);
}

//-----------------------------------------------------------------------------

void ToonzExt::OverallDesigner::draw(ToonzExt::Selector *selector) {
  if (!selector) return;

  const TStroke *ref = selector->getStroke();

  if (!ref) return;

  double length_at_w         = ref->getLength(selector->getW()),
         emi_selector_length = selector->getLength() * 0.5,
         stroke_length       = ref->getLength();

  ToonzExt::Interval interval;

  if (ref->isSelfLoop()) {
    interval.first = length_at_w - emi_selector_length;
    if (interval.first < 0.0) interval.first = stroke_length + interval.first;

    interval.first = ref->getParameterAtLength(interval.first);

    interval.second = length_at_w + emi_selector_length;
    if (interval.second > stroke_length)
      interval.second = interval.second - stroke_length;

    interval.second = ref->getParameterAtLength(interval.second);
  } else {
    interval.first = ref->getParameterAtLength(
        std::max(0.0, length_at_w - emi_selector_length));
    interval.second = ref->getParameterAtLength(
        std::min(stroke_length, length_at_w + emi_selector_length));
  }

  float prev_line_width = 1.0;

  glGetFloatv(GL_LINE_WIDTH, &prev_line_width);

  glLineWidth(2.0);
  drawStrokeCenterLine(ref, pixelSize_, interval);
  glLineWidth(prev_line_width);
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
/*
#ifdef  RESTORE_DEBUG
#undef  RESTORE_DEBUG
#define _DEBUG  1
#endif
//*/
