#include "toonz/pathanimations.h"

#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "tstroke.h"

//-----------------------------------------------------------------------------
// StrokeId

StrokeId::StrokeId(TXsheet *xsheet, const TXshCell &cellId, TStroke *stroke)
  : m_xsheet (xsheet), m_cellId (cellId), m_stroke (stroke)
{ }

bool operator == (const StrokeId &a, const StrokeId &b) {
  return a.m_xsheet == b.m_xsheet &&
    a.m_cellId == b.m_cellId &&
    a.m_stroke == b.m_stroke;
}

bool operator < (const StrokeId &a, const StrokeId &b) {
  if (a.m_xsheet < b.m_xsheet) return true;
  if (a.m_xsheet > b.m_xsheet) return false;
  if (a.m_cellId < b.m_cellId) return true;
  if (b.m_cellId < a.m_cellId) return false;
  if (a.m_stroke < b.m_stroke) return true;
  if (a.m_stroke > b.m_stroke) return false;
  return false;
}

QString StrokeId::name() const {
  return m_stroke->name();
}

//-----------------------------------------------------------------------------
// PathAnimation

PathAnimation::PathAnimation(PathAnimations *animations, const StrokeId &strokeId)
  : m_animations(animations), m_strokeId(strokeId)
{ }

void PathAnimation::takeSnapshot() {
  updateChunks();
  // update points
}

void PathAnimation::updateChunks() {
  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < stroke->getChunkCount(); i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);

  }
}

//-----------------------------------------------------------------------------
// PathAnimations

PathAnimations::PathAnimations(TXsheet *xsheet) :
  m_xsheet(xsheet)
{ }

shared_ptr<PathAnimation> PathAnimations::stroke(const StrokeId &strokeId) const {
  map<StrokeId, shared_ptr<PathAnimation>>::const_iterator it = m_shapeAnimation.find(strokeId);
  if (it == m_shapeAnimation.end())
    return nullptr;
  return it->second;
}

shared_ptr<PathAnimation> PathAnimations::addStroke(const StrokeId &strokeId) {
  shared_ptr<PathAnimation> alreadyHas = stroke(strokeId);
  if (alreadyHas)
    return alreadyHas;
  // race condition?

  shared_ptr<PathAnimation> newOne { new PathAnimation(this, strokeId) };
  m_shapeAnimation[strokeId] = newOne;
  return newOne;
}
