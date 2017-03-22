#include "toonz/pathanimations.h"

#include "toonz/doubleparamcmd.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "tstroke.h"

//-----------------------------------------------------------------------------
// StrokeId

StrokeId::StrokeId(TXsheet *xsheet, const TXshCell &cellId, TStroke *stroke)
    : m_xsheet(xsheet), m_cellId(cellId), m_stroke(stroke) {}

bool operator==(const StrokeId &a, const StrokeId &b) {
  return a.m_xsheet == b.m_xsheet && a.m_cellId == b.m_cellId &&
         a.m_stroke == b.m_stroke;
}

bool operator<(const StrokeId &a, const StrokeId &b) {
  if (a.m_xsheet < b.m_xsheet) return true;
  if (a.m_xsheet > b.m_xsheet) return false;
  if (a.m_cellId < b.m_cellId) return true;
  if (b.m_cellId < a.m_cellId) return false;
  if (a.m_stroke < b.m_stroke) return true;
  if (a.m_stroke > b.m_stroke) return false;
  return false;
}

PathAnimations *StrokeId::pathAnimations() const {
  return m_xsheet->pathAnimations();
}

QString StrokeId::name() const { return m_stroke->name(); }

//-----------------------------------------------------------------------------
// PathAnimation

PathAnimation::PathAnimation(PathAnimations *animations,
                             const StrokeId &strokeId)
    : m_animations(animations), m_strokeId(strokeId), m_activated(false) {
  m_params = new TParamSet(name().toStdString());
}

void PathAnimation::takeSnapshot(int atFrame) {
  updateChunks();
  snapshotChunks(atFrame);
}

int PathAnimation::chunkCount() const { return m_params->getParamCount(); }

TParamSetP PathAnimation::chunkParam(int i) const {
  return m_params->getParam(i);
}

TThickPointParamP PathAnimation::pointParam(int chunk, int point) const {
  return chunkParam(chunk)->getParam(point);
}

// ensures that chunks count matches the referenced stroke
// and that each chunk has params for all of its points and thicknesses
void PathAnimation::updateChunks() {
  m_params->removeAllParam();

  map<const TThickQuadratic *, TParamSetP> chunks;

  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < stroke->getChunkCount(); i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    map<const TThickQuadratic *, TParamSetP>::iterator found =
        m_lastChunks.find(chunk);

    TParamSetP param;
    if (found != m_lastChunks.end()) {
      chunks.insert(*found);
      param = found->second;
    } else {
      param = new TParamSet();
      chunks.insert(pair<const TThickQuadratic *, TParamSetP>(chunk, param));
    }
    m_params->addParam(param, "Chunk " + std::to_string(i + 1));

    if (param->getParamCount())  // has params for lifetime of the chunk
      continue;
    param->addParam(new TThickPointParam(chunk->getThickP0()), "point0");
    param->addParam(new TThickPointParam(chunk->getThickP1()), "point1");
    param->addParam(new TThickPointParam(chunk->getThickP2()), "point2");
  }
  m_lastChunks = chunks;
}

// while activated: sets a keyframe at current frame to match current curve
// while deactivated: updates the curve to match current state
void PathAnimation::snapshotChunks(int frame) {
  TStroke *stroke = m_strokeId.stroke();

  for (int i = 0; i < stroke->getChunkCount(); i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    map<const TThickQuadratic *, TParamSetP>::iterator found =
        m_lastChunks.find(chunk);
    assert(found != m_lastChunks.end());

    TParamSetP chunkParam = found->second;
    for (int j = 0; j < 3; j++)
      if (m_activated)
        setThickPointKeyframe(chunkParam->getParam(j), frame);
      else
        setThickPointInanimate(chunkParam->getParam(j), frame);
  }
}

void PathAnimation::setThickPointKeyframe(TThickPointParamP thickPoint,
                                          int frame) {
  if (!thickPoint) return;
  setDoubleKeyframe(thickPoint->getX(), frame);
  setDoubleKeyframe(thickPoint->getY(), frame);
  setDoubleKeyframe(thickPoint->getThickness(), frame);
}

void PathAnimation::setThickPointInanimate(TThickPointParamP thickPoint,
                                           int frame) {
  if (!thickPoint) return;
  thickPoint->getX()->setDefaultValue(thickPoint->getX()->getValue(frame));
  thickPoint->getY()->setDefaultValue(thickPoint->getY()->getValue(frame));
  thickPoint->getThickness()->setDefaultValue(thickPoint->getThickness()->getValue(frame));
}

void PathAnimation::setDoubleKeyframe(TDoubleParamP &param, int frame) {
  KeyframeSetter setter(param.getPointer(), -1, false);
  setter.createKeyframe(frame);
}

QString PathAnimation::name() const { return m_strokeId.name(); }

void PathAnimation::toggleActivated() {
  m_activated = !m_activated;
  emit m_animations->xsheet()->sublayerActivatedChanged();
}

void PathAnimation::clearKeyframes() {
  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < chunkCount(); i++) {
    TParamSetP chunk = chunkParam(i);
    assert(chunk);

    TThickQuadratic *quad = const_cast<TThickQuadratic *>(stroke->getChunk(i));
    for (int j = 0; j < 3; j++) {
      TThickPointParamP pointParam = chunk->getParam(j);
      pointParam->clearKeyframes();
    }
  }
}

void PathAnimation::animate(int frame) const {
  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < chunkCount(); i++) {
    TParamSetP chunk = chunkParam(i);
    assert(chunk);

    TThickQuadratic *quad = const_cast<TThickQuadratic *>(stroke->getChunk(i));
    for (int j = 0; j < 3; j++) {
      TThickPointParamP pointParam = chunk->getParam(j);
      quad->setThickP(j, pointParam->getValue(frame));
    }
  }
  stroke->invalidate();
}

int PathAnimation::controlPointCount() const { return chunkCount() * 2 + 1; }

TThickPoint PathAnimation::getControlPointPos(int cpIndex, int frame) const {
  TThickPointParamP param = controlPoint(cpIndex);
  if (isActivated())
    return param->getValue(frame);
  else
    return param->getDefaultValue();
}
void PathAnimation::setControlPointPos(int cpIndex, int frame,
                                       const TThickPoint &pos) {
  TThickPointParamP param = controlPoint(cpIndex);
  if (isActivated())
    param->setValue(frame, pos);
  else
    param->setDefaultValue(pos);
}

TThickPointParamP PathAnimation::controlPoint(int cpIndex) const {
  assert(chunkCount());
  int cpCount = controlPointCount();
  int chCount = chunkCount();

  if (cpIndex <= 0)
    return pointParam(0, 0);
  else if (cpIndex >= controlPointCount())
    return pointParam(chunkCount() - 1, 2);
  else {
    int chunk = (cpIndex - 1) / 2;
    return pointParam(chunk, cpIndex - chunk * 2);
  }
}

//-----------------------------------------------------------------------------
// PathAnimations

PathAnimations::PathAnimations(TXsheet *xsheet) : m_xsheet(xsheet) {}

shared_ptr<PathAnimation> PathAnimations::stroke(
    const StrokeId &strokeId) const {
  map<StrokeId, shared_ptr<PathAnimation>>::const_iterator it =
      m_shapeAnimation.find(strokeId);
  if (it == m_shapeAnimation.end()) return nullptr;
  return it->second;
}

shared_ptr<PathAnimation> PathAnimations::addStroke(const StrokeId &strokeId) {
  shared_ptr<PathAnimation> alreadyHas = stroke(strokeId);
  if (alreadyHas) return alreadyHas;
  // race condition?

  shared_ptr<PathAnimation> newOne{new PathAnimation(this, strokeId)};
  m_shapeAnimation[strokeId] = newOne;
  return newOne;
}

void PathAnimations::setFrame(TVectorImage *vi, const TXshCell &cell,
                              int frame) {
  for (int i = 0; i < vi->getStrokeCount(); i++) {
    StrokeId strokeId{m_xsheet, cell, vi->getStroke(i)};

    shared_ptr<PathAnimation> animation = addStroke(strokeId);
    animation->animate(frame);
  }
}

StrokeId PathAnimations::appStrokeId(const TApplication *app, TStroke *stroke) {
  TXsheet *xsheet = app->getCurrentXsheet()->getXsheet();
  TXshLevelP level = app->getCurrentLevel()->getLevel();
  int frame = app->getCurrentFrame()->getFrame();
  int layer = app->getCurrentColumn()->getColumnIndex();
  TXshCell cell = xsheet->getCell(CellPosition(frame, layer));
  return StrokeId { xsheet, cell, stroke };
}

shared_ptr<PathAnimation> PathAnimations::appStroke(const TApplication *app, TStroke *stroke) {
  TXsheet *xsheet = app->getCurrentXsheet()->getXsheet();
  return xsheet->pathAnimations()->addStroke(appStrokeId (app, stroke));
}

void PathAnimations::appSnapshot(const TApplication *app, TStroke *stroke) {
  shared_ptr<PathAnimation> animation = appStroke(app, stroke);
  animation->takeSnapshot(app->getCurrentFrame()->getFrame());
}
