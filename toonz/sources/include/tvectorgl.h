#pragma once

#ifndef TVECTORGL_INCLUDED
#define TVECTORGL_INCLUDED

#ifdef _WIN32
#include <windows.h>
#include <cstdlib>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#ifdef MACOSX
#include <GLUT/glut.h>
#endif

//#include "tgeometry.h"
#include "tcommon.h"

#undef DVAPI
#undef DVVAR

#ifdef DV_LOCAL_DEFINED
#define DVAPI
#define DVVAR
#else
#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif
#endif

//=============================================================================
// forward declarations
class TStroke;
class TRegion;
class TVectorImage;
class TVectorRenderData;

//=============================================================================

/*!
  Draw a stroke in a GL context.
  \par stroke is element to draw
  \par rd: \sa TVectorRenderData
 */
void DVAPI tglDraw(const TVectorRenderData &rd, const TVectorImage *vim,
                   TStroke **guidedStroke = (TStroke **)nullptr);
void DVAPI tglDrawMask(const TVectorRenderData &rd, const TVectorImage *vim);
void DVAPI tglDraw(const TVectorRenderData &rd, const TStroke *stroke,
                   bool pushAttribs = true);
void DVAPI tglDraw(const TVectorRenderData &rd, TRegion *r,
                   bool pushAttribs = true);

//=============================================================================

#endif
