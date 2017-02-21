#pragma once

#ifndef PATH_ANIMATIONS_INCLUDED
#define PATH_ANIMATIONS_INCLUDED

#include "tcommon.h"
#include "tparamset.h"
#include "tfilepath.h"
#include "toonz/txshcell.h"

#include <map>

using std::map;

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TXsheet;
class TXshCell;
class TStroke;

class DVAPI StrokeId final {
  TXsheet *m_xsheet;
  TXshCell m_cellId;
  TStroke *m_stroke;

public:
  StrokeId(TXsheet *xsheet, const TXshCell &cell, TStroke *stroke);

  friend bool operator == (const StrokeId &a, const StrokeId &b);
  friend bool operator < (const StrokeId &a, const StrokeId &b);

  QString name() const;
};

// storage class for keyframes for all parameters animating
// a particular TStroke
class DVAPI PathAnimations final {
  TXsheet *m_xsheet;
  map<StrokeId, TParamSetP> m_shapeAnimation;

public:
  PathAnimations(TXsheet *xsheet);
  ~PathAnimations() { }

  TParamSetP stroke(const StrokeId &strokeId) const;
  TParamSetP addStroke(const StrokeId &strokeId);
};

#endif