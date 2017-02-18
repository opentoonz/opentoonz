#include "toonz/pathanimations.h"

#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"

//-----------------------------------------------------------------------------
// StrokeId

StrokeId::StrokeId(TXsheet *xsheet, TXshColumn *column, const TFrameId &frameId)
  : m_xsheet (xsheet), m_column (column), m_frameId (frameId)
{ }

bool operator == (const StrokeId &a, const StrokeId &b) {
  return a.m_xsheet == b.m_xsheet &&
    a.m_column == b.m_column &&
    a.m_frameId == b.m_frameId;
}

bool operator < (const StrokeId &a, const StrokeId &b) {
  return a.m_xsheet < b.m_xsheet ||
    (a.m_xsheet == b.m_xsheet && a.m_column < b.m_column ||
    (a.m_column == b.m_column && a.m_frameId < b.m_frameId));
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

  TParamSetP newOne = new TParamSet("name");
  m_shapeAnimation.insert(std::make_pair(strokeId, newOne));
  return newOne;
}
