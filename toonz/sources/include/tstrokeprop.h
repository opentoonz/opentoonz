#pragma once

#ifndef TSTROKEPROP_H
#define TSTROKEPROP_H

#include "tstrokeoutline.h"
#include "tstroke.h"
#include "tsimplecolorstyles.h"
#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declarations
class TVectorRenderData;
// class TStroke;
class TColorStyle;
class TSimpleStrokeStyle;
class TFlash;
class TRasterImagePatternStrokeStyle;
class TVectorImagePatternStrokeStyle;
class TCenterLineStrokeStyle;
//=============================================================================

//=============================================================================

class DVAPI TSimpleStrokeProp : public TStrokeProp {
protected:
  TSimpleStrokeStyle *m_colorStyle;

public:
  TSimpleStrokeProp(const TStroke *stroke, TSimpleStrokeStyle *style);
  ~TSimpleStrokeProp();

  const TColorStyle *getColorStyle() const;

  TStrokeProp *clone(const TStroke *stroke) const;
  void draw(const TVectorRenderData &rd);
  void draw(TFlash &flash);
};

//=============================================================================

class DVAPI TRasterImagePatternStrokeProp : public TStrokeProp {
protected:
  TRasterImagePatternStrokeStyle *m_colorStyle;
  std::vector<TAffine> m_transformations;

public:
  TRasterImagePatternStrokeProp(const TStroke *stroke,
                                TRasterImagePatternStrokeStyle *style);
  ~TRasterImagePatternStrokeProp();

  const TColorStyle *getColorStyle() const;

  TStrokeProp *clone(const TStroke *stroke) const;
  void draw(const TVectorRenderData &rd);
  void draw(TFlash &flash);
};

//=============================================================================

class DVAPI TVectorImagePatternStrokeProp : public TStrokeProp {
protected:
  TVectorImagePatternStrokeStyle *m_colorStyle;
  std::vector<TAffine> m_transformations;

public:
  TVectorImagePatternStrokeProp(const TStroke *stroke,
                                TVectorImagePatternStrokeStyle *style);
  ~TVectorImagePatternStrokeProp();

  const TColorStyle *getColorStyle() const;

  TStrokeProp *clone(const TStroke *stroke) const;
  void draw(const TVectorRenderData &rd);
  void draw(TFlash &flash);
};

//=============================================================================

class DVAPI OutlineStrokeProp : public TStrokeProp {
protected:
  TOutlineStyleP m_colorStyle;
  TStrokeOutline m_outline;
  double m_outlinePixelSize;

public:
  OutlineStrokeProp(const TStroke *stroke, TOutlineStyleP style);

  const TColorStyle *getColorStyle() const;

  TStrokeProp *clone(const TStroke *stroke) const;
  void draw(const TVectorRenderData &rd);
  void draw(TFlash &flash);
};

//=============================================================================

#endif
