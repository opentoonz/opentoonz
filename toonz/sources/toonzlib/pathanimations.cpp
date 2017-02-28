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
{
  m_params = new TParamSet(name().toStdString());
}

void PathAnimation::takeSnapshot() {
  updateChunks();
  snapshotChunks();
}

int PathAnimation::chunkCount() const {
  return m_strokeId.stroke()->getChunkCount();
}

// ensures that chunks count matches the referenced stroke
// and that each chunk has params for all of its points and thicknesses
void PathAnimation::updateChunks() {
  m_params->removeAllParam();

  map<const TThickQuadratic *, TParamSetP> chunks;

  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < stroke->getChunkCount(); i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    map<const TThickQuadratic *, TParamSetP>::iterator found = m_lastChunks.find(chunk);
    
    TParamSetP param;
    if (found != m_lastChunks.end()) {
      chunks.insert(*found);
      param = found->second;
    }
    else {
      param = new TParamSet();
      chunks.insert(pair<const TThickQuadratic *, TParamSetP>(chunk, param));
    }
    m_params->addParam(param, "Chunk " + std::to_string(i + 1));

    if (param->getParamCount()) // has params for lifetime of the chunk
      continue;
    param->addParam(new TThickPointParam(chunk->getThickP0()), "point0");
    param->addParam(new TThickPointParam(chunk->getThickP1()), "point1");
    param->addParam(new TThickPointParam(chunk->getThickP2()), "point2");
  }
  m_lastChunks = chunks;
}

// sets a keyframe for the points and thicknesses
void PathAnimation::snapshotChunks() {
  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < stroke->getChunkCount(); i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    map<const TThickQuadratic *, TParamSetP>::iterator found = m_lastChunks.find(chunk);
    TParamSetP param = found->second;

    // set key frame in some way
  }
}

QString PathAnimation::name() const {
  return m_strokeId.name();
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
