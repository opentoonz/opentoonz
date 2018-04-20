#pragma once

#ifndef PATH_ANIMATIONS_INCLUDED
#define PATH_ANIMATIONS_INCLUDED

#include "tcommon.h"
#include "tcurves.h"
#include "tdoubleparam.h"
#include "tparamset.h"
#include "tfilepath.h"
#include "tvectorimage.h"
#include "toonz/txshcell.h"
#include "toonz/tapplication.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

using std::map;
using std::pair;
using std::shared_ptr;
using std::set;
using std::vector;

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

class PathAnimations;

class DVAPI StrokeId final {
  TXsheet *m_xsheet;
  TXshCell m_cellId;
  TStroke *m_stroke;

public:
  StrokeId(TXsheet *xsheet, const TXshCell &cell, TStroke *stroke);

  friend bool operator==(const StrokeId &a, const StrokeId &b);
  friend bool operator<(const StrokeId &a, const StrokeId &b);

  PathAnimations *pathAnimations() const;
  TStroke *stroke() const { return m_stroke; }
  TXshCell cell() const { return m_cellId; }

  QString name() const;
};

//! Makes a snapshot of one kind or another
class Snapshotter {
public:
  virtual void set(TThickPointParamP param, const TThickPoint &p) = 0;
  virtual ~Snapshotter() {}
};

//! Keyframe data for a particular TStroke
//! The stroke is represented by a TParamSet
//! Its children are chunks; each chunk has a TParamSet
//! Each chunk has 3 control points: p0, p1, p2; each corresponds a
//! TThickPointParam
class DVAPI PathAnimation final {
  PathAnimations *m_animations;
  StrokeId m_strokeId;
  bool m_activated;
  bool m_highlighted;
  map<const TThickQuadratic *, TParamSetP> m_lastChunks;
  TParamSetP m_params;  //! chunk nodes
public:
  PathAnimation(PathAnimations *animations, const StrokeId &strokeId);
  ~PathAnimation() {}

  QString name() const;

  void takeSnapshot(int atFrame);
  //! updates chunk count and array
  void updateChunks();
  //! add chunk to collection, only if not present yet
  void addChunk(const TThickQuadratic *chunk);
  bool hasChunk(const TThickQuadratic *chunk);

  void setParams(TThickQuadratic *chunk, TParamSetP newParams);
  TParamSetP getParams(TThickQuadratic *chunk);

  //! remember position of specified chunk
  void snapshotChunk(const TThickQuadratic *chunk, int frame);

  bool isActivated() const { return m_activated; }
  void toggleActivated();

  bool isHighlighted() const { return m_highlighted; }
  void setHighlight(bool lit) { m_highlighted = lit; }

  set<double> getKeyframes() const;
  void clearKeyframes();

  int chunkCount() const;
  TParamSetP chunkParam(int i) const;

  TThickPointParamP pointParam(int chunk, int point) const;

  void animate(int frame) const;

  // private:
  //! updates chunk animation info
  void snapshotCurrentChunks(int atFrame);
  //! remember position of specified thick point
  void snapshotThickPoint(TThickPointParamP param, const TThickPoint &point,
                          int frame);
};

//!----------------------------------------------------------------------
//! Storage for all strokes animation keyframes
class DVAPI PathAnimations final {
  TXsheet *m_xsheet;
  map<StrokeId, shared_ptr<PathAnimation>> m_shapeAnimation;

public:
  PathAnimations(TXsheet *xsheet);
  ~PathAnimations() {}

  TXsheet *xsheet() const { return m_xsheet; }

  shared_ptr<PathAnimation> stroke(const StrokeId &strokeId) const;
  shared_ptr<PathAnimation> addStroke(const StrokeId &strokeId);
  void removeStroke(const TStroke *stroke);

  static void syncChunkCount(const TApplication *app, TStroke *lStroke,
                             TStroke *rStroke, bool switched = false);
  static void copyAnimation(const TApplication *app, TStroke *oStroke,
                            TStroke *cStroke);
  static void changeChunkDirection(const TApplication *app, TStroke *stroke);

  //! update all animatied strokes to match specified frame
  void setFrame(TVectorImage *vi, const TXshCell &cell, int frame);

  static PathAnimations *appAnimations(const TApplication *app);
  static StrokeId appStrokeId(const TApplication *app, TStroke *stroke);
  static shared_ptr<PathAnimation> appStroke(const TApplication *app,
                                             TStroke *stroke);
  static void appSnapshot(const TApplication *app, TStroke *stroke);
  static void appClearAndSnapshot(const TApplication *app, TStroke *stroke);

  map<StrokeId, shared_ptr<PathAnimation>> shapeAnimations() {
    return m_shapeAnimation;
  }
  bool hasActivatedAnimations();
  bool hasActivatedAnimations(const TXshCell cell);

  void loadData(TIStream &is);
  void saveData(TOStream &os, int occupiedColumnCount);
};

#endif