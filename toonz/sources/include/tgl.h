#pragma once

#ifndef TGL_INCLUDED
#define TGL_INCLUDED

//#include "tgeometry.h"
#include "tmachine.h"

#ifdef _WIN32
#include <windows.h>
#include <cstdlib>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#ifdef MACOSX
#include <GLUT/glut.h>
#define GLUT_NO_LIB_PRAGMA
#define GLUT_NO_WARNING_DISABLE
#endif

#if defined(LINUX) || defined(FREEBSD)
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

//#include "tcurves.h"
#include "traster.h"
//#include "tfilepath.h"

class TFilePath;
class TCubic;

#undef DVAPI
#undef DVVAR

#ifdef DV_LOCAL_DEFINED
#define DVAPI
#define DVVAR
#else
#ifdef TGL_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif
#endif

#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
#define TGL_FMT GL_BGRA_EXT
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
#define TGL_FMt GL_ABGR_EXT
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
#define TGL_FMT GL_RGBA
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
#define TGL_FMT GL_BGRA
#else
@undefined chan order
#endif

#ifdef TNZ_MACHINE_CHANNEL_ORDER_MRGB
#define TGL_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#else
#define TGL_TYPE GL_UNSIGNED_BYTE
#endif

//=============================================================================
// settings for 30bit display

#define TGL_TexFmt10 GL_RGB10_A2
#define TGL_TYPE16 GL_UNSIGNED_SHORT

//=============================================================================

#ifdef _DEBUG
#define CHECK_ERRORS_BY_GL                                                     \
  {                                                                            \
    GLenum glErr = glGetError();                                               \
    assert(glErr != GL_INVALID_ENUM);                                          \
    assert(glErr != GL_INVALID_VALUE);                                         \
    assert(glErr != GL_INVALID_OPERATION);                                     \
    assert(glErr != GL_STACK_OVERFLOW);                                        \
    assert(glErr != GL_STACK_UNDERFLOW);                                       \
    assert(glErr != GL_OUT_OF_MEMORY);                                         \
  }
#else
#define CHECK_ERRORS_BY_GL ;
#endif

//=============================================================================

// forward declarations
class TStroke;
class TVectorImage;
class TVectorRenderData;

//=============================================================================

//! Extension to set a vertex
inline void tglVertex(const TPointD &p) { glVertex2d(p.x, p.y); }
inline void tglVertex(const TPoint &p) { glVertex2i(p.x, p.y); }

//! Extension to set a color
inline void tglColor(const TPixel &p) { glColor4ub(p.r, p.g, p.b, p.m); }
inline void tglColor(const TPixelD &p) { glColor4d(p.r, p.g, p.b, p.m); }

//! retrieve the square of pixel size from current GL_MODELVIEW matrix
DVAPI double tglGetPixelSize2();

//! Draw text in string s at position p.

DVAPI void tglDrawText(const TPointD &p, const std::string &s,
                       void *font = GLUT_STROKE_ROMAN);
DVAPI void tglDrawText(const TPointD &p, const std::wstring &s,
                       void *font = GLUT_STROKE_ROMAN);

//! Returns text width

DVAPI double tglGetTextWidth(const std::string &s,
                             void *font = GLUT_STROKE_ROMAN);

/*!
 Draw circle of radius r with center c.
 Remark: it is possible set number of slices
 */
DVAPI void tglDrawCircle(const TPointD &c, double r);

/*!
 Draw circle of radius r with center c.
 Remark: it is possible set number of slices
 */
DVAPI void tglDrawDisk(const TPointD &c, double r);

/*!
 Draw a segment.
 */
DVAPI void tglDrawSegment(const TPointD &p1, const TPointD &p2);

inline void tglDrawSegment(const TPoint &p1, const TPoint &p2) {
  tglDrawSegment(convert(p1), convert(p2));
}

inline void tglDrawSegment(double x0, double y0, double x1, double y1) {
  tglDrawSegment(TPointD(x0, y0), TPointD(x1, y1));
}

// inline  void  glDrawSegment(const TSegment& s){
//  glDrawSegment( s.getP0(), s.getP1() );
//}

/*!
 Draw a rect.
 */
DVAPI void tglDrawRect(const TRectD &rect);

inline void tglDrawRect(const TRect &rect) { tglDrawRect(convert(rect)); }

inline void tglDrawRect(double x0, double y0, double x1, double y1) {
  tglDrawRect(TRectD(x0, y0, x1, y1));
}

/*!
 Fill a rect.
 */
DVAPI void tglFillRect(const TRectD &rect);

inline void tglFillRect(const TRect &rect) { tglFillRect(convert(rect)); }

inline void tglFillRect(double x0, double y0, double x1, double y1) {
  tglFillRect(TRectD(x0, y0, x1, y1));
}

inline void tglMultMatrix(const TAffine &aff) {
  GLdouble m[] = {aff.a11, aff.a21, 0, 0, aff.a12, aff.a22, 0, 0,
                  0,       0,       1, 0, aff.a13, aff.a23, 0, 1};
  glMultMatrixd(m);
}

//=============================================================================

void DVAPI tglRgbOnlyColorMask();
void DVAPI tglAlphaOnlyColorMask();

void DVAPI tglEnableBlending(GLenum src = GL_SRC_ALPHA,
                             GLenum dst = GL_ONE_MINUS_SRC_ALPHA);

void DVAPI tglEnableLineSmooth(bool enable = true, double lineSize = 1.0);
void DVAPI tglEnablePointSmooth(double pointSize = 1.0);

void DVAPI tglGetColorMask(GLboolean &red, GLboolean &green, GLboolean &blue,
                           GLboolean &alpha);
void DVAPI tglMultColorMask(GLboolean red, GLboolean green, GLboolean blue,
                            GLboolean alpha);

//=============================================================================

/*!
  Draw a stroke in a GL context.
  \par stroke is element to draw
  \par rd: \sa TVectorRenderData
 */
/* spostati in tvectorgl.h

void DVAPI tglDraw(const TVectorRenderData &rd, const TStroke* stroke);
void DVAPI tglDraw(const TVectorRenderData &rd, const TVectorImage* vim);
void DVAPI tglDrawMask(const TVectorRenderData &rd, const TVectorImage* vim);
*/
//=============================================================================

/*!
  Draw a vector image in a GL context.
  \par vi vector image to draw
 */
/*
void DVAPI  drawVectorImage(
                     const TVectorImageP& vim,
                     const TRect& clippingRect,
                     const TAffine& aff,
                     const TColorFunction *cf = 0);
*/

//-----------------------------------------------------------------------------

#define NEW_DRAW_TEXT

enum GlDrawTextIndentation { Left = 0, Right = 1, Center = 2 };

// precision is the number of segments to draw the curve
void DVAPI tglDraw(const TCubic &cubic, int precision,
                   GLenum pointOrLine = GL_LINE);

void DVAPI tglDraw(const TRectD &rect, const std::vector<TRaster32P> &textures,
                   bool blending = true);

void DVAPI tglDraw(const TRectD &rect, const TRaster32P &tex,
                   bool blending = true);

void DVAPI tglBuildMipmaps(std::vector<TRaster32P> &rasters,
                           const TFilePath &filepath);

//-----------------------------------------------------------------------------

#ifdef _WIN32
typedef std::pair<HDC, HGLRC> TGlContext;
#else
typedef void *TGlContext;
#endif

DVAPI TGlContext tglGetCurrentContext();
DVAPI void tglMakeCurrent(TGlContext context);
DVAPI void tglDoneCurrent(TGlContext context);

#endif
