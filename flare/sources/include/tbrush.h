#pragma once

//-----------------------------------------------------------------------------
//  tbrush.h: class to define an abstract brush
//-----------------------------------------------------------------------------
#ifndef TBRUSH_H
#define TBRUSH_H

#ifdef PER_VECCHIO_ELLIPTIC_BRUSH

#include "tcommon.h"

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TStroke;
class TStrokeOutline;

//---------------------------------------------------------------------------------------

class DVAPI TBrush {
public:
  class OutlineParameter {
  public:
    /*double m_pixelSize;*/
    double m_lengthStep;  //  max lengthStep (sulla centerline) per la
                          //  linearizzazione dell'outline

    OutlineParameter(/*double pixelSize,*/ double lengthStep = 0)
        /*: m_pixelSize(pixelSize)*/
        : m_lengthStep(lengthStep) {}
  };

  TBrush() {}
  virtual ~TBrush() {}

  virtual void makeOutline(const TStroke &stroke, TStrokeOutline &outline,
                           const OutlineParameter &param) = 0;

  virtual void draw()     = 0;
  virtual TBrush *clone() = 0;
};
#endif  // PER_VECCHIO_ELLIPTIC_BRUSH

#endif  // TBRUSH_H
