#include "toonz/pathanimations.h"

#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "tstroke.h"

//-----------------------------------------------------------------------------
// StrokeId

StrokeId::StrokeId(TXsheet *xsheet, TXshColumn *column, const TFrameId &frameId, TStroke *stroke)
  : m_xsheet (xsheet), m_column (column), m_frameId (frameId), m_stroke (stroke)
{ }

bool operator == (const StrokeId &a, const StrokeId &b) {
  return a.m_xsheet == b.m_xsheet &&
    a.m_column == b.m_column &&
    a.m_frameId == b.m_frameId &&
    a.m_stroke == b.m_stroke;
}

bool operator < (const StrokeId &a, const StrokeId &b) {
  if (a.m_xsheet < b.m_xsheet) return true;
  if (a.m_xsheet > b.m_xsheet) return false;
  if (a.m_column < b.m_column) return true;
  if (a.m_column > b.m_column) return false;
  if (a.m_frameId < b.m_frameId) return true;
  if (a.m_frameId > b.m_frameId) return false;
  if (a.m_stroke < b.m_stroke) return true;
  if (a.m_stroke > b.m_stroke) return false;
  return false;
}

QString StrokeId::name() const {
  return m_stroke->name();
}

//-----------------------------------------------------------------------------
// PathAnimations

PathAnimations::PathAnimations(TXsheet *xsheet) :
  m_xsheet(xsheet)
{ }

TParamSetP PathAnimations::stroke(const StrokeId &strokeId) const {
  map<StrokeId, TParamSetP>::const_iterator it = m_shapeAnimation.find(strokeId);
  if (it == m_shapeAnimation.end())
    return nullptr;
  return it->second;
}

TParamSetP PathAnimations::addStroke(const StrokeId &strokeId) {
  TParamSetP alreadyHas = stroke(strokeId);
  if (alreadyHas)
    return alreadyHas;
  // race condition?

  TParamSetP newOne = new TParamSet(strokeId.name().toStdString());
  m_shapeAnimation.insert(std::make_pair(strokeId, newOne));
  return newOne;
}
