

#include "tgl.h"
#include "tvectorgl.h"
#include "tregion.h"
#include "tregionprop.h"
#include "tstrokeutil.h"
#include "tvectorrenderdata.h"
#include "tvectorimage.h"
#include "tpalette.h"
#include "tcolorfunctions.h"
#include "tsimplecolorstyles.h"
#include "tthreadmessage.h"
#include "tstrokeprop.h"

#include "tconvert.h"
#include "tcurves.h"
#include "tstrokeoutline.h"
#include <QTime>

#ifndef _WIN32
#define CALLBACK
#endif

#ifndef checkErrorsByGL
#define checkErrorsByGL                                                        \
  {                                                                            \
    GLenum err = glGetError();                                                 \
    assert(err != GL_INVALID_ENUM);                                            \
    assert(err != GL_INVALID_VALUE);                                           \
    assert(err != GL_INVALID_OPERATION);                                       \
    assert(err != GL_STACK_OVERFLOW);                                          \
    assert(err != GL_STACK_UNDERFLOW);                                         \
    assert(err != GL_OUT_OF_MEMORY);                                           \
    assert(err == GL_NO_ERROR);                                                \
  }
#endif

#undef checkErrorsByGL
#define checkErrorsByGL

using namespace std;

//-----------------------------------------------------------------------------
/*
DV_EXPORT_API void mylog(std::string s)
{
  static TThread::Mutex mutex;
  QMutexLocker sl(mutex);
  std::ofstream os("C:\\gmt\\buttami\\bu.txt", std::ios::app);

  LARGE_INTEGER ticksPerSecond;
  LARGE_INTEGER tick;
  static LARGE_INTEGER firstTick;
  static bool firstTime = true;
  long dt = 0;

  QueryPerformanceFrequency(&ticksPerSecond);
  QueryPerformanceCounter(&tick);
  if(firstTime) {firstTick = tick;firstTime=false;}
  else
  {
    dt =
(long)(1000000*(tick.QuadPart-firstTick.QuadPart)/ticksPerSecond.QuadPart);
  }
  os << dt << ":" << s << std::endl;
}
*/

//=============================================================================
#ifdef _DEBUG

bool checkQuadraticDistance(TStroke *stroke, bool checkThickness) {
  UINT i, qCount = stroke->getChunkCount();
  const TThickQuadratic *q;
  TThickPoint p1, p2, p3;

  // se i punti coincidono e' una stroke puntiforme ed e' ammessa
  if (qCount == 1) return true;

  for (i = 0; i != qCount; i++) {
    q  = stroke->getChunk(i);
    p1 = q->getThickP0();
    p2 = q->getThickP1();
    p3 = q->getThickP2();

    if (areAlmostEqual(p1.x, p2.x) && areAlmostEqual(p2.x, p3.x) &&
        areAlmostEqual(p1.y, p2.y) && areAlmostEqual(p2.y, p3.y) &&
        (!checkThickness || (areAlmostEqual(p1.thick, p2.thick) &&
                             areAlmostEqual(p2.thick, p3.thick))))
      return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

void drawControlPoints(const TVectorRenderData &rd, TStroke *stroke,
                       double pixelSize, bool allPoints = true) {
  int i;
  TPointD p;
  glPushMatrix();

  tglMultMatrix(rd.m_aff);

  glPointSize(2.0);
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
  glPopMatrix();
}

#endif

//-----------------------------------------------------------------------------

void drawArrows(TStroke *stroke, bool onlyFirstPoint) {
  double length = stroke->getLength(0.0, 1.0);
  int points    = length / 20;
  if (points < 2) points += 1;
  double currentPosition = 0.0;

  TPointD prePoint, point, postPoint;
  glColor3d(1.0, 0.0, 0.0);
  for (int i = 0; i <= points; i++) {
    currentPosition = i / (double)points;
    point           = stroke->getPointAtLength(length * currentPosition);
    prePoint =
        (i == 0) ? point
                 : stroke->getPointAtLength(length * (currentPosition - 0.02));
    postPoint =
        (i == points)
            ? point
            : stroke->getPointAtLength(length * (currentPosition + 0.02));

    if (prePoint == postPoint) continue;

    double radian =
        std::atan2(postPoint.y - prePoint.y, postPoint.x - prePoint.x);
    double degree = radian * 180.0 / 3.14159265;

    glPushMatrix();
    glTranslated(point.x, point.y, 0);
    glRotated(degree, 0, 0, 1);
    glBegin(GL_LINES);
    glVertex2d(0, 0);
    glVertex2d(-3, -3);
    glVertex2d(0, 0);
    glVertex2d(-3, 3);
    glEnd();
    glPopMatrix();

    if (onlyFirstPoint) break;
    // make the arrow blue from the second one
    glColor3d(0.0, 0.0, 1.0);
  }
}

//-----------------------------------------------------------------------------
// Used for Guided Drawing
void drawFirstControlPoint(const TVectorRenderData &rd, TStroke *stroke) {
  TPointD p          = stroke->getPoint(0.0);
  double length      = stroke->getLength(0.0, 1.0);
  int msecs          = QTime::currentTime().msec();
  double modifier    = (msecs / 100) * 0.1;
  TPointD startPoint = stroke->getPointAtLength(length * modifier);
  TPointD endPoint   = stroke->getPointAtLength(length * 0.10);
  double j           = 0.025;

  glPushMatrix();
  tglMultMatrix(rd.m_aff);
  if (!rd.m_animatedGuidedDrawing) glLineWidth(2.0f);
  glColor3d(0.0, 1.0, 0.0);
  if (!rd.m_animatedGuidedDrawing) {
    drawArrows(stroke, false);
  }

  if (rd.m_animatedGuidedDrawing) {
    drawArrows(stroke, true);
    glColor3d(0.0, 1.0, 0.0);
    j = 0.025 + modifier;
    glBegin(GL_LINES);
    // draw the first animated section
    for (int i = 0; i < 8; i++) {
      endPoint = stroke->getPointAtLength(length * j);
      glVertex2d(startPoint.x, startPoint.y);
      glVertex2d(endPoint.x, endPoint.y);
      startPoint = endPoint;
      j += 0.025;
      if (j > 1) {
        j -= 1;
        startPoint = stroke->getPointAtLength(length * j);
      }
    }

    modifier = modifier + 0.5;
    if (modifier >= 1) modifier -= 1;
    startPoint = stroke->getPointAtLength(length * modifier);
    j          = 0.025 + modifier;

    // draw another animated section
    for (int i = 0; i < 8; i++) {
      endPoint = stroke->getPointAtLength(length * j);
      glVertex2d(startPoint.x, startPoint.y);
      glVertex2d(endPoint.x, endPoint.y);
      startPoint = endPoint;
      j += 0.025;
      if (j > 1) {
        j -= 1;
        startPoint = stroke->getPointAtLength(length * j);
      }
    }
    glEnd();
  }

  glLineWidth(1.0f);
  glPopMatrix();
}

//=============================================================================

/*TPixel TransparencyCheckBlackBgInk = TPixel(255,255,255);
TPixel TransparencyCheckWhiteBgInk = TPixel(0,0,0);
TPixel TransparencyCheckPaint = TPixel(127,127,127);*/
static int Index = 0;
void tglDraw(const TVectorRenderData &rd, TRegion *r, bool pushAttribs) {
  checkErrorsByGL;
  assert(r);
  checkErrorsByGL;
  if (!r) return;
  bool alphaChannel = rd.m_alphaChannel;
  checkErrorsByGL;

  int j          = 0;
  bool visible   = false;
  int colorCount = 0;

  TColorStyleP style;
  if (rd.m_paintCheckEnabled && r->getStyle() == rd.m_colorCheckIndex) {
    static TSolidColorStyle *redColor = new TSolidColorStyle();
    redColor->addRef();
    redColor->setMainColor(TPixel::Red);
    style = redColor;
  } else if (rd.m_tcheckEnabled) {
    static TSolidColorStyle *color = new TSolidColorStyle();
    color->addRef();
    color->setMainColor(rd.m_tCheckPaint);
    style = color;
  } else
    style = rd.m_palette->getStyle(r->getStyle());

  colorCount = style->getColorParamCount();
  if (colorCount == 0) {  // for example texture
    visible = true;
  } else {
    visible = false;
    for (j = 0; j < colorCount && !visible; j++) {
      TPixel32 color            = style->getColorParamValue(j);
      if (rd.m_cf) color        = (*(rd.m_cf))(color);
      if (color.m != 0) visible = true;
    }
  }
  if (visible) {
    TRegionProp *prop = r->getProp(/*rd.m_palette*/);
    /// questo codice satva dentro tregion::getprop/////
    int styleId = r->getStyle();
    if (styleId) {
      // TColorStyle * style = rd.m_palette->getStyle(styleId);
      if (!style->isRegionStyle() || style->isEnabled() == false) {
        prop = 0;
      } else {
        // Warning: The same remark of stroke props holds here.
        if (!prop || style.getPointer() != prop->getColorStyle()) {
          r->setProp(style->makeRegionProp(r));
          prop = r->getProp();
        }
      }
    }

    ////// draw
    if (prop) {
      if (pushAttribs) glPushAttrib(GL_ALL_ATTRIB_BITS);

      tglEnableLineSmooth(true);
//#define DRAW_EDGE_NUMBERS
#ifdef DRAW_EDGE_NUMBERS
      glPushMatrix();
      tglMultMatrix(rd.m_aff);
      switch (Index % 7) {
      case 0:
        tglColor(TPixel::Red);
        break;
      case 1:
        tglColor(TPixel::Green);
        break;
      case 2:
        tglColor(TPixel::Blue);
        break;
      case 3:
        tglColor(TPixel::Cyan);
        break;
      case 4:
        tglColor(TPixel::Magenta);
        break;
      case 5:
        tglColor(TPixel::Yellow);
        break;
      case 6:
        tglColor(TPixel::Black);
        break;
      default:
        tglColor(TPixel::Red);
        break;
      }

      Index++;
      if (rIndex == 2) {
        double y = r->getEdge(0)
                       ->m_s
                       ->getThickPoint(
                           (r->getEdge(0)->m_w0 + r->getEdge(0)->m_w1) / 2.0)
                       .y;
        tglDrawSegment(TPointD(-1000, y), TPointD(1000, y));
      }

      for (int i = 0; i < (int)r->getEdgeCount(); i++) {
        TEdge *e  = r->getEdge(i);
        TPointD p = e->m_s->getPoint(0.8 * e->m_w0 + 0.2 * e->m_w1);
        if (i == 0)
          tglDrawText(p,
                      (QString::number(rIndex) + QString("-0")).toStdString());
        else
          tglDrawText(p, QString::number(i).toStdString());
        if (e->m_index == 3) {
          tglColor(TPixel::Black);
          TStroke *s = e->m_s;
          drawPoint(s->getChunk(0)->getP0(), .3);
          tglColor(TPixel::Red);
          tglDrawText(s->getChunk(0)->getP0(),
                      QString::number(0).toStdString());
          for (int ii = 0; ii < s->getChunkCount(); ii++) {
            drawPoint(s->getChunk(ii)->getP2(), .3);
            if (ii < s->getChunkCount() - 1) {
              tglColor(TPixel::Red);
              tglDrawText(s->getChunk(ii)->getP2(),
                          QString::number(ii + 1).toStdString());
            }
          }
        }
      }
      glPopMatrix();
#endif

      if (alphaChannel) {
        GLboolean red, green, blue, alpha;
        tglGetColorMask(red, green, blue, alpha);

        // Draw RGB channels
        tglEnableBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(red, green, blue, GL_FALSE);
        prop->draw(rd);

        // Draw Matte channel
        tglEnableBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, alpha);
        prop->draw(rd);

        glColorMask(red, green, blue, alpha);
      } else {
        // pezza: in render, le aree fillate dei custom styles sparivano.
        if (!rd.m_isOfflineRender || !rd.m_isImagePattern)
          tglRgbOnlyColorMask();  // RGB components only

        prop->draw(rd);
      }

      if (pushAttribs) glPopAttrib();
    }
  }

  for (UINT i = 0; i < r->getSubregionCount(); i++)
    tglDraw(rd, r->getSubregion(i), pushAttribs);
  checkErrorsByGL;
}

//-----------------------------------------------------------------------------

void tglDrawMask(const TVectorRenderData &rd1, const TVectorImage *vim) {
  UINT i;
  assert(vim);
  if (!vim) return;
  TVectorRenderData rd(rd1);

  glPushAttrib(GL_ALL_ATTRIB_BITS);

  if (!rd.m_palette) {
    TPalette *vPalette = vim->getPalette();
    assert(vPalette);
    rd.m_palette = vPalette;
  }
  for (i = 0; i < vim->getRegionCount(); i++)
    tglDraw(rd, vim->getRegion(i), false);

  glPopAttrib();
}

//-----------------------------------------------------------------------------

namespace {
bool isOThick(const TStroke *s) {
  int i;
  for (i = 0; i < s->getControlPointCount(); i++)
    if (s->getControlPoint(i).thick != 0) return false;
  return true;
}
}

void tglDraw(const TVectorRenderData &rd, const TStroke *s, bool pushAttribs) {
  assert(s);
  if (!s) return;

  TStrokeProp *prop  = 0;
  bool pushedAttribs = false;

  try {
    TColorStyleP style;
    TStroke *stroke = const_cast<TStroke *>(s);
    if (rd.m_inkCheckEnabled && s->getStyle() == rd.m_colorCheckIndex) {
      static TSolidColorStyle *redColor = new TSolidColorStyle();
      redColor->addRef();
      redColor->setMainColor(TPixel::Red);
      style = redColor;
    } else if (rd.m_tcheckEnabled) {
      static TSolidColorStyle *color = new TSolidColorStyle();
      color->addRef();
      color->setMainColor(rd.m_tCheckInk);
      style = color;
    } else
      style = rd.m_palette->getStyle(stroke->getStyle());

    if (!rd.m_show0ThickStrokes && isOThick(s) &&
        dynamic_cast<TSolidColorStyle *>(
            style.getPointer())  // This is probably to exclude
                                 // TCenterlineStrokeStyle-like styles
        && !rd.m_tcheckEnabled)  // I wonder why this?
      return;

    // const TStroke& stroke = *s;  //serve???

    assert(rd.m_palette);

    prop = s->getProp(/*rd.m_palette*/);
    /////questo codice stava dentro tstroke::getprop/////////
    if (prop) prop->getMutex()->lock();

    if (!style->isStrokeStyle() || style->isEnabled() == false) {
      if (prop) prop->getMutex()->unlock();

      prop = 0;
    } else {
      // Warning: the following pointers check is conceptually wrong - we
      // keep it because the props maintain SMART POINTER-like reference to
      // the associated style. This prevents the style from being destroyed
      // while still referenced by the prop.
      if (!prop || style.getPointer() != prop->getColorStyle()) {
        if (prop) prop->getMutex()->unlock();

        stroke->setProp(style->makeStrokeProp(stroke));
        prop = stroke->getProp();
        if (prop) prop->getMutex()->lock();
      }
    }

    //--------- draw ------------
    if (!prop) return;

    if (pushAttribs) glPushAttrib(GL_ALL_ATTRIB_BITS), pushedAttribs = true;

    bool alphaChannel = rd.m_alphaChannel, antialias = rd.m_antiAliasing;
    TVectorImagePatternStrokeProp *aux =
        dynamic_cast<TVectorImagePatternStrokeProp *>(prop);
    if (aux)  // gli image pattern vettoriali tornano in questa funzione....non
              // facendo il corpo dell'else'si evita di disegnarli due volte!
      prop->draw(rd);
    else {
      if (antialias)
        tglEnableLineSmooth(true);
      else
        tglEnableLineSmooth(false);

      if (alphaChannel) {
        GLboolean red, green, blue, alpha;
        tglGetColorMask(red, green, blue, alpha);

        // Draw RGB channels
        tglEnableBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(red, green, blue, GL_FALSE);
        prop->draw(rd);

        // Draw Matte channel
        tglEnableBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, alpha);
        prop->draw(rd);

        glColorMask(red, green, blue, alpha);
      } else {
        tglEnableBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        prop->draw(rd);
      }
    }

    if (pushAttribs) glPopAttrib(), pushedAttribs = false;

    prop->getMutex()->unlock();
    //---------------------
  } catch (...) {
    if (prop) prop->getMutex()->unlock();
    if (pushedAttribs) glPopAttrib();
  }
}

//------------------------------------------------------------------------------------

static void tglDoDraw(const TVectorRenderData &rd, TRegion *r) {
  bool visible   = false;
  int colorCount = 0;
  if (!r) return;

  TColorStyleP style = rd.m_palette->getStyle(r->getStyle());
  colorCount         = style->getColorParamCount();
  if (colorCount == 0)  // for example texture
    visible = true;
  else {
    visible = false;
    for (int j = 0; j < colorCount && !visible; j++) {
      TPixel32 color            = style->getColorParamValue(j);
      if (rd.m_cf) color        = (*(rd.m_cf))(color);
      if (color.m != 0) visible = true;
    }
  }
  if (visible)
    tglDraw(rd, r, false);
  else
    for (UINT j = 0; j < r->getSubregionCount(); j++)
      tglDraw(rd, r->getSubregion(j), false);
}

//------------------------------------------------------------------------------------

static bool tglDoDraw(const TVectorRenderData &rd, const TStroke *s) {
  bool visible   = false;
  int colorCount = 0;

  const TPalette *palette = rd.m_palette;

  int styleId = s->getStyle();
  // assert(0<=styleId && styleId<stylesCount);
  TColorStyleP style = palette->getStyle(styleId);
  assert(style);
  colorCount = style->getColorParamCount();
  if (colorCount == 0)
    visible = true;
  else {
    visible = false;
    for (int j = 0; j < style->getColorParamCount() && !visible; j++) {
      TPixel32 color            = style->getColorParamValue(j);
      if (rd.m_cf) color        = (*(rd.m_cf))(color);
      if (color.m != 0) visible = true;
    }
  }

  bool ret = false;

  if (visible) {
    // Change stroke color to blue if guided drawing
    if (rd.m_showGuidedDrawing && rd.m_highLightNow) {
      TVectorRenderData *newRd = new TVectorRenderData(
          rd, rd.m_aff, rd.m_clippingRect, rd.m_palette, rd.m_guidedCf);
      tglDraw(*newRd, s, false);
      delete newRd;
      TStroke *new_s = (TStroke *)s;
      drawFirstControlPoint(rd, new_s);
      ret = rd.m_animatedGuidedDrawing;
    } else {
      tglDraw(rd, s, false);
    }
  }
#ifdef _DEBUG
// drawControlPoints(rd, vim->getStroke(i), sqrt(tglGetPixelSize2()), true);
// assert(checkQuadraticDistance(vim->getStroke(i),true));
#endif
  return ret;
}

//------------------------------------------------------------------------------------

namespace {

void doDraw(const TVectorImage *vim, const TVectorRenderData &_rd,
            bool drawEnteredGroup, TStroke **guidedStroke = 0) {
  static TOnionFader *fade = new TOnionFader(TPixel::White, 0.5);

  TVectorRenderData rd(_rd);

  if (!rd.m_palette) {
    TPalette *vPalette = vim->getPalette();
    rd.m_palette       = vPalette;
    if (!vPalette) return;
  }

  if (!drawEnteredGroup && !rd.m_isIcon && vim->isInsideGroup() > 0)
    rd.m_cf = fade;

  TVectorRenderData rdRegions = rd;

  /*if (rd.m_drawRegions && rd.m_isImagePattern)//gli image pattern hanno
bisogno dell'antialiasig per le linee, ma sulle aree ci sarebbero un sacco di
assert
rdRegions.m_alphaChannel = rdRegions.m_antiAliasing = false;*/
  UINT strokeIndex = 0;
  Index            = 0;

  while (strokeIndex <
         vim->getStrokeCount())  // ogni ciclo di while disegna un gruppo
  {
    int currStrokeIndex = strokeIndex;
    if (!rd.m_isIcon && vim->isInsideGroup() > 0 &&
        ((drawEnteredGroup && !vim->isEnteredGroupStroke(strokeIndex)) ||
         !drawEnteredGroup && vim->isEnteredGroupStroke(strokeIndex))) {
      while (strokeIndex < vim->getStrokeCount() &&
             vim->sameGroup(strokeIndex, currStrokeIndex))
        strokeIndex++;
      continue;
    }

    if (rd.m_drawRegions)
      for (UINT regionIndex = 0; regionIndex < vim->getRegionCount();
           regionIndex++)
        if (vim->sameGroupStrokeAndRegion(currStrokeIndex, regionIndex))
          tglDoDraw(rdRegions, vim->getRegion(regionIndex));
    while (strokeIndex < vim->getStrokeCount() &&
           vim->sameGroup(strokeIndex, currStrokeIndex)) {
      if (rd.m_indexToHighlight != strokeIndex) {
        rd.m_highLightNow = false;
      } else {
        rd.m_highLightNow = true;
      }
#if DISEGNO_OUTLINE == 1
      CurrStrokeIndex = strokeIndex;
      CurrVimg        = vim;
#endif
      bool isGuided = tglDoDraw(rd, vim->getStroke(strokeIndex));
      if (isGuided && guidedStroke) *guidedStroke = vim->getStroke(strokeIndex);
      strokeIndex++;
    }
  }
}
}

//------------------------------------------------------------------------------------

void tglDraw(const TVectorRenderData &rd, const TVectorImage *vim,
             TStroke **guidedStroke) {
  assert(vim);
  if (!vim) return;

  QMutexLocker sl(vim->getMutex());

  checkErrorsByGL;
  checkErrorsByGL;

  glPushAttrib(GL_ALL_ATTRIB_BITS);

  // if(!rd.m_palette) rd.m_palette = vim->getPalette();
  // mylog("tglDraw start; mutex=" + toString((unsigned long)&vim->getMutex()));

  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0);

  doDraw(vim, rd, false, guidedStroke);
  if (!rd.m_isIcon && vim->isInsideGroup() > 0)
    doDraw(vim, rd, true, guidedStroke);

  glDisable(GL_ALPHA_TEST);

  glPopAttrib();

#ifdef _DEBUG
  vim->drawAutocloses(rd);
#endif
  checkErrorsByGL;
  // mylog("tglDraw stop");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
