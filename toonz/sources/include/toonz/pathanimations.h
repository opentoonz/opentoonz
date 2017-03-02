#pragma once

#ifndef PATH_ANIMATIONS_INCLUDED
#define PATH_ANIMATIONS_INCLUDED

#include "tcommon.h"
#include "tcurves.h"
#include "tdoubleparam.h"
#include "tparamset.h"
#include "tfilepath.h"
#include "toonz/txshcell.h"

#include <map>
#include <memory>

using std::map;
using std::pair;
using std::shared_ptr;

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

  TStroke *stroke() const { return m_stroke; }

  QString name() const;
};

class PathAnimations;

//! Keyframe data for a particular TStroke
class DVAPI PathAnimation final {
  PathAnimations *m_animations;
  StrokeId m_strokeId;
  bool m_activated;
  map<const TThickQuadratic *, TParamSetP> m_lastChunks;
  TParamSetP m_params; //! chunk nodes
public:
  PathAnimation(PathAnimations *animations, const StrokeId &strokeId);
  ~PathAnimation() { }

  QString name() const;

  void takeSnapshot(int atFrame);
  void updateChunks();

  bool isActivated() const { return m_activated; }
  void toggleActivated() { m_activated = !m_activated; }

  int chunkCount() const;
  TParamSetP chunk(int i) const;

private:
  void snapshotChunks(int atFrame);
  void setThickPointKeyframe(TThickPointParamP thickPoint, int frame);
  void setDoubleKeyframe(TDoubleParamP &param, int frame);
};

//! Storage for all strokes animation keyframes
class DVAPI PathAnimations final {
  TXsheet *m_xsheet;
  map<StrokeId, shared_ptr<PathAnimation>> m_shapeAnimation;

public:
  PathAnimations(TXsheet *xsheet);
  ~PathAnimations() { }

  shared_ptr<PathAnimation> stroke(const StrokeId &strokeId) const;
  shared_ptr<PathAnimation> addStroke(const StrokeId &strokeId);
};

#endif