#include "toonz/pathanimations.h"

#include "toonz/doubleparamcmd.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobject.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "tstroke.h"

namespace {
TFrameId qstringToFrameId(QString str) {
  if (str.isEmpty() || str == "-1")
    return TFrameId::EMPTY_FRAME;
  else if (str == "-" || str == "-2")
    return TFrameId::NO_FRAME;
  TFrameId fid;
  int s = 0;
  QString number;
  char letter(0);
  for (s = 0; s < str.size(); s++) {
    QChar c = str.at(s);
    if (c.isNumber()) number.append(c);
#if QT_VERSION >= 0x050500
    else
      letter = c.toLatin1();
#else
    else
      letter = c.toAscii();
#endif
  }
  return TFrameId(number.toInt(), letter);
}
}
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
    : m_animations(animations)
    , m_strokeId(strokeId)
    , m_activated(false)
    , m_highlighted(false) {
  m_params = new TParamSet(name().toStdString());
}

void PathAnimation::takeSnapshot(int atFrame) {
  updateChunks();
  snapshotCurrentChunks(atFrame);
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

void PathAnimation::addChunk(const TThickQuadratic *chunk) {
  map<const TThickQuadratic *, TParamSetP>::iterator found =
      m_lastChunks.find(chunk);
  if (found != m_lastChunks.end())  // already present
    return;
  TParamSetP param = new TParamSet();
  m_lastChunks.insert(pair<const TThickQuadratic *, TParamSetP>(chunk, param));

  m_params->addParam(param, "Chunk ?");  // reindex later
  param->addParam(new TThickPointParam(chunk->getThickP0()), "point0");
  param->addParam(new TThickPointParam(chunk->getThickP1()), "point1");
  param->addParam(new TThickPointParam(chunk->getThickP2()), "point2");
}

bool PathAnimation::hasChunk(const TThickQuadratic *chunk) {
  map<const TThickQuadratic *, TParamSetP>::iterator found =
      m_lastChunks.find(chunk);

  return (found != m_lastChunks.end());
}

void PathAnimation::setParams(TThickQuadratic *chunk, TParamSetP newParams) {
  map<const TThickQuadratic *, TParamSetP>::iterator found =
      m_lastChunks.find(chunk);

  if (found == m_lastChunks.end()) return;

  m_params->removeParam(found->second);
  found->second = newParams;
  m_params->addParam(newParams, "Chunk ?");
}

TParamSetP PathAnimation::getParams(TThickQuadratic *chunk) {
  map<const TThickQuadratic *, TParamSetP>::iterator found =
      m_lastChunks.find(chunk);

  if (found == m_lastChunks.end()) return 0;

  return found->second;
}

// while activated: sets a keyframe at current frame to match current curve
// while deactivated: updates the curve to match current state
void PathAnimation::snapshotCurrentChunks(int frame) {
  TStroke *stroke = m_strokeId.stroke();

  for (int i = 0; i < stroke->getChunkCount(); i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    snapshotChunk(chunk, frame);
  }
}

void PathAnimation::snapshotChunk(const TThickQuadratic *chunk, int frame) {
  map<const TThickQuadratic *, TParamSetP>::iterator found =
      m_lastChunks.find(chunk);
  assert(found != m_lastChunks.end());

  TParamSetP chunkParam = found->second;
  for (int j = 0; j < 3; j++)
    snapshotThickPoint(chunkParam->getParam(j), chunk->getThickP(j), frame);
}

void PathAnimation::snapshotThickPoint(TThickPointParamP param,
                                       const TThickPoint &point, int frame) {
  if (!param) return;
  if (m_activated)
    param->setValue(frame, point);
  else
    param->setDefaultValue(point);
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

set<double> PathAnimation::getKeyframes() const {
  set<double> result;
  if (!m_lastChunks.size()) return result;

  TParamSetP anyParam = m_lastChunks.begin()->second;
  anyParam->getKeyframes(result);
  return result;
}

void PathAnimation::animate(int frame) const {
  TStroke *stroke = m_strokeId.stroke();
  for (int i = 0; i < chunkCount(); i++) {
    TParamSetP chunk = chunkParam(i);
    assert(chunk);

    TThickQuadratic *quad = const_cast<TThickQuadratic *>(stroke->getChunk(i));
    if (!quad) continue;
    for (int j = 0; j < 3; j++) {
      TThickPointParamP pointParam = chunk->getParam(j);
      quad->setThickP(j, pointParam->getValue(frame));
    }
  }
  stroke->invalidate();
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

void PathAnimations::removeStroke(const TStroke *stroke) {
  // remove all mentions of the stroke
  for (map<StrokeId, shared_ptr<PathAnimation>>::iterator it =
           m_shapeAnimation.begin();
       it != m_shapeAnimation.end();)
    if (it->first.stroke() == stroke)
      m_shapeAnimation.erase(it++);
    else
      it++;
}

void PathAnimations::setFrame(TVectorImage *vi, const TXshCell &cell,
                              int frame) {
  std::vector<int> indexArray;
  std::vector<TStroke *> strokeArray;
  for (int i = 0; i < vi->getStrokeCount(); i++) {
    StrokeId strokeId{m_xsheet, cell, vi->getStroke(i)};

    shared_ptr<PathAnimation> animation = addStroke(strokeId);
    if (animation->isActivated()) {
      animation->animate(frame);
      indexArray.push_back(i);
      strokeArray.push_back(vi->getStroke(i));
    }
  }
  if (indexArray.size()) vi->notifyChangedStrokes(indexArray, strokeArray);
}

PathAnimations *PathAnimations::appAnimations(const TApplication *app) {
  TXsheet *xsheet = app->getCurrentXsheet()->getXsheet();
  return xsheet->pathAnimations();
}

StrokeId PathAnimations::appStrokeId(const TApplication *app, TStroke *stroke) {
  TXsheet *xsheet  = app->getCurrentXsheet()->getXsheet();
  TXshLevelP level = app->getCurrentLevel()->getLevel();
  int frame        = app->getCurrentFrame()->getFrame();
  int layer        = app->getCurrentColumn()->getColumnIndex();
  TXshCell cell    = xsheet->getCell(CellPosition(frame, layer));
  return StrokeId{xsheet, cell, stroke};
}
StrokeId PathAnimations::appStrokeId(TXsheet *xsheet, int frame, int layer,
                                     TStroke *stroke) {
  TXshCell cell = xsheet->getCell(CellPosition(frame, layer));
  return StrokeId{xsheet, cell, stroke};
}

shared_ptr<PathAnimation> PathAnimations::appStroke(const TApplication *app,
                                                    TStroke *stroke) {
  return appAnimations(app)->addStroke(appStrokeId(app, stroke));
}

shared_ptr<PathAnimation> PathAnimations::appStroke(const TApplication *app,
                                                    TXsheet *xsheet, int frame,
                                                    int layer,
                                                    TStroke *stroke) {
  return appAnimations(app)->addStroke(
      appStrokeId(xsheet, frame, layer, stroke));
}
void PathAnimations::appSnapshot(const TApplication *app, TStroke *stroke) {
  shared_ptr<PathAnimation> animation = appStroke(app, stroke);
  animation->takeSnapshot(app->getCurrentFrame()->getFrame());
}

void PathAnimations::appClearAndSnapshot(const TApplication *app,
                                         TStroke *stroke) {
  shared_ptr<PathAnimation> animation = appStroke(app, stroke);
  animation->clearKeyframes();
  animation->takeSnapshot(app->getCurrentFrame()->getFrame());
}

bool PathAnimations::hasActivatedAnimations() {
  for (map<StrokeId, shared_ptr<PathAnimation>>::iterator it =
           m_shapeAnimation.begin();
       it != m_shapeAnimation.end(); it++) {
    shared_ptr<PathAnimation> animation = it->second;
    if (animation->isActivated()) return true;
  }

  return false;
}
bool PathAnimations::hasActivatedAnimations(const TXshCell cell) {
  for (map<StrokeId, shared_ptr<PathAnimation>>::iterator it =
           m_shapeAnimation.begin();
       it != m_shapeAnimation.end(); it++) {
    StrokeId animationKey = it->first;
    if (animationKey.cell() != cell) continue;
    shared_ptr<PathAnimation> animation = it->second;
    if (animation->isActivated()) return true;
  }
  return false;
}

void PathAnimations::syncChunkCount(const TApplication *app, TStroke *lStroke,
                                    TStroke *rStroke, bool switched) {
  int lChunkCount = lStroke->getChunkCount();
  int rChunkCount = rStroke->getChunkCount();

  // No difference, do nothing
  if (lChunkCount == rChunkCount) return;

  // Let's call this function with the strokes switched
  if (lChunkCount < rChunkCount) {
    syncChunkCount(app, rStroke, lStroke, true);
    return;
  }

  // Left stroke is the master and we want to add to the right stroke.

  shared_ptr<PathAnimation> lAnimation = appStroke(app, lStroke);
  shared_ptr<PathAnimation> rAnimation = appStroke(app, rStroke);

  // Let's build a list of the right strokes at the different keyframes
  std::vector<std::pair<TStroke *, int>> strokeList;

  // Add what the stroke looks like at each keyframe
  set<double> keyframes = rAnimation->getKeyframes();

  TStroke *tmpStroke;

  if (!switched && !keyframes.size()) {
    tmpStroke = new TStroke(*rStroke);
    std::pair<TStroke *, int> strokeAtkeyframe(tmpStroke, -1);
    strokeList.push_back(strokeAtkeyframe);
    keyframes.clear();
  }

  set<double>::iterator itf;
  for (itf = keyframes.begin(); itf != keyframes.end(); itf++) {
    tmpStroke = new TStroke(*rStroke);

    // Take the original stroke and adjust control points per keyframe
    // information
    for (int i = 0; i < tmpStroke->getChunkCount(); i++) {
      TParamSetP chunk = rAnimation->chunkParam(i);
      if (!chunk) continue;
      TThickQuadratic *quad = (TThickQuadratic *)tmpStroke->getChunk(i);
      if (!quad) continue;
      for (int j = 0; j < 3; j++) {
        TThickPointParamP pointParam = chunk->getParam(j);
        quad->setThickP(j, pointParam->getValue(*itf));
      }
    }
    tmpStroke->invalidate();

    std::pair<TStroke *, int> strokeAtkeyframe(tmpStroke, int(*itf));
    strokeList.push_back(strokeAtkeyframe);
  }

  // Go through each stroke in the list and add missing chunks
  double lMaxDist = lStroke->getLength();
  double lDist, lDistPct;
  for (int i = 0; i < strokeList.size(); i++) {
    std::vector<double> newChunkPositions;
    double rMaxDist = strokeList[i].first->getLength();
    double rDist, rDistPct;
    int lIdx = 0, rIdx = 0;

    // Determine where the new chunks need to go on the right stroke
    // relative to where chunks appear on the left stroke
    rChunkCount   = strokeList[i].first->getChunkCount();
    int insertCnt = 0;
    for (rIdx = 0; rIdx < (rChunkCount + 1); rIdx++) {
      const TThickQuadratic *rChunk = rStroke->getChunk(rIdx);

      lDist          = lStroke->getLength(lIdx, 0.0);
      lDistPct       = (lDist / lMaxDist) * 100.0;
      rDist          = strokeList[i].first->getLength(rIdx, 0.0);
      rDistPct       = (rDist / rMaxDist) * 100.0;
      double pctDiff = rDistPct - lDistPct;

      if (int(pctDiff) <= 3 && int(rDistPct) != 100) {
        if (lIdx < lChunkCount) lIdx++;
        continue;
      }

      while (insertCnt < (lChunkCount - rChunkCount) && lIdx < lChunkCount &&
             (int(pctDiff) > 3 || int(rDistPct) == 100)) {
        insertCnt++;
        newChunkPositions.push_back((rMaxDist * (lDistPct / 100.0)));

        lIdx++;
        if (lIdx == lChunkCount) break;

        lDist    = lStroke->getLength(lIdx, 0.0);
        lDistPct = (lDist / lMaxDist) * 100.0;
        pctDiff  = rDistPct - lDistPct;
      }
      if (lIdx < lChunkCount) lIdx++;
    }

    // Add the missing chunks to the right stroke now
    for (int c = 0; c < newChunkPositions.size(); c++) {
      strokeList[i].first->insertControlPointsAtLength(newChunkPositions[c]);
      if (!i) rStroke->insertControlPointsAtLength(newChunkPositions[c]);
    }
    strokeList[i].first->invalidate();
    if (!i) rStroke->invalidate();
    rChunkCount = strokeList[i].first->getChunkCount();

    // Rebuild right chunk params
    for (int r = 0; r < rChunkCount; r++) {
      const TThickQuadratic *rChunk  = rStroke->getChunk(r);
      const TThickQuadratic *slChunk = strokeList[i].first->getChunk(r);

      if (!rAnimation->hasChunk(rChunk)) rAnimation->addChunk(rChunk);

      TParamSetP rParams = rAnimation->getParams((TThickQuadratic *)rChunk);
      TParamSetP nParams = new TParamSet();
      nParams->addParam(new TThickPointParam(slChunk->getThickP0()), "point0");
      nParams->addParam(new TThickPointParam(slChunk->getThickP1()), "point1");
      nParams->addParam(new TThickPointParam(slChunk->getThickP2()), "point2");

      TThickPointParamP param0 = nParams->getParam(0);
      TThickPointParamP param1 = nParams->getParam(1);
      TThickPointParamP param2 = nParams->getParam(2);

      if (strokeList[i].second >= 0) {
        param0->setValue(strokeList[i].second, slChunk->getThickP0());
        param1->setValue(strokeList[i].second, slChunk->getThickP1());
        param2->setValue(strokeList[i].second, slChunk->getThickP2());
      }

      for (int p = 0; p < 3; p++) {
        TThickPointParamP rParam = rParams->getParam(p);
        TThickPointParamP nParam = nParams->getParam(p);
        if (!rParam) continue;

        if (!i) rParam->setDefaultValue(nParam->getDefaultValue());
        if (strokeList[i].second >= 0)
          rParam->setValue(strokeList[i].second,
                           nParam->getValue(strokeList[i].second));
      }

      rAnimation->setParams((TThickQuadratic *)rChunk, rParams);
    }

    delete strokeList[i].first;
  }
}

void PathAnimations::copyAnimation(const TApplication *app, TStroke *oStroke,
                                   TStroke *cStroke) {
  shared_ptr<PathAnimation> oAnimation = appStroke(app, oStroke);
  shared_ptr<PathAnimation> cAnimation = appStroke(app, cStroke);

  if (!oAnimation->isActivated()) return;

  oAnimation->updateChunks();
  cAnimation->updateChunks();

  if (!cAnimation->isActivated()) cAnimation->toggleActivated();

  int oChunkCount = oStroke->getChunkCount();
  int cChunkCount = cStroke->getChunkCount();

  if (oChunkCount != cChunkCount) {
    syncChunkCount(app, oStroke, cStroke);

    oChunkCount = oStroke->getChunkCount();
    cChunkCount = cStroke->getChunkCount();
  }

  if (oChunkCount != cChunkCount) return;

  cAnimation->clearKeyframes();

  for (int i = 0; i < oChunkCount; i++) {
    const TThickQuadratic *oChunk = oStroke->getChunk(i);
    const TThickQuadratic *cChunk = cStroke->getChunk(i);
    if (!oChunk) continue;

    if (!cChunk) cAnimation->addChunk(cChunk);

    TParamSetP oParams = oAnimation->getParams((TThickQuadratic *)oChunk);
    cAnimation->setParams((TThickQuadratic *)cChunk, oParams);
  }
}

void PathAnimations::changeChunkDirection(const TApplication *app,
                                          TStroke *stroke) {
  shared_ptr<PathAnimation> animation = appStroke(app, stroke);

  if (!animation->isActivated()) return;

  int chunkCount = stroke->getChunkCount();

  std::vector<TParamSetP> paramSet;

  for (int i = 0; i < chunkCount; i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    TParamSetP params = animation->getParams((TThickQuadratic *)chunk);
    paramSet.push_back(params);
  }

  std::reverse(paramSet.begin(), paramSet.end());

  for (int i = 0; i < chunkCount; i++) {
    const TThickQuadratic *chunk = stroke->getChunk(i);
    animation->setParams((TThickQuadratic *)chunk, paramSet[i]);
  }

  animation->updateChunks();
}

void PathAnimations::loadData(TIStream &is) {
  if (!m_xsheet) return;

  std::string tagName;

  while (is.matchTag(tagName)) {
    if (tagName == "pathanimation") {
      std::string levelName, strokeName;
      int strokeID, strokeIdx;
      char frameLetter;
      ToonzScene *scene   = m_xsheet->getScene();
      TLevelSet *levelSet = scene->getLevelSet();
      TFrameId frameId;
      TXshLevel *level;
      std::shared_ptr<PathAnimation> animation;
      QString frameStr;

      while (is.matchTag(tagName)) {
        if (tagName == "frame") {
          TPersist *p = 0;
          is >> p >> frameStr;
          level   = dynamic_cast<TXshLevel *>(p);
          frameId = qstringToFrameId(frameStr);
        } else if (tagName == "stroke") {
          strokeID   = stoi(is.getTagAttribute("id"));
          strokeIdx  = stoi(is.getTagAttribute("idx")) - 1;
          strokeName = is.getTagAttribute("name");
          if (level && !frameId.isEmptyFrame()) {
            TXshSimpleLevel *simpleLevel = level->getSimpleLevel();
            if (simpleLevel) {
              simpleLevel->setScene(scene);
              if (!simpleLevel->getFrameCount()) {
                // Try and load level now because we need the actual stroke
                // information
                try {
                  simpleLevel->load();
                } catch (...) {
                }
              }
              TImageP image = simpleLevel->getFrame(frameId, false);
              if (image) {
                TXshCell cell = TXshCell(level, frameId);
                emit ImageManager::instance()->updatedFrame(cell);
                TVectorImageP vi = image.getPointer();
                if (vi && strokeIdx < vi->getStrokeCount()) {
                  vi->getStroke(strokeIdx)->setId(strokeID);
                  if (strokeName.length())
                    vi->getStroke(strokeIdx)->setName(
                        QString::fromStdString(strokeName));
                  StrokeId strokeId(m_xsheet, cell, vi->getStroke(strokeIdx));
                  animation = addStroke(strokeId);
                  animation->updateChunks();
                }
              }
            }
          }
          if (!is.isBeginEndTag()) {
            int chunkId;
            while (is.matchTag(tagName)) {
              if (tagName == "handle") {
                if (!animation->isActivated()) animation->toggleActivated();
                chunkId          = stoi(is.getTagAttribute("id")) - 1;
                TParamSetP chunk = animation->chunkParam(chunkId);
                while (is.matchTag(tagName)) {
                  if (tagName == "p0") {
                    chunk->getParam(0)->loadData(is);
                  } else if (tagName == "p1") {
                    chunk->getParam(1)->loadData(is);
                  } else if (tagName == "p2") {
                    chunk->getParam(2)->loadData(is);
                  } else
                    throw TException("Chunk. unexpeced tag: " + tagName);
                  is.matchEndTag();
                }
              } else
                throw TException("Stroke. unexpeced tag: " + tagName);
              is.matchEndTag();
            }
          }
        } else
          throw TException("Animation. unexpeced tag: " + tagName);
        if (!is.isBeginEndTag()) is.matchEndTag();
      }
    } else
      throw TException("PathAnimation. unexpeced tag: " + tagName);
    is.matchEndTag();
  }
}

void PathAnimations::saveData(TOStream &os, int occupiedColumnCount) {
  if (!m_xsheet) return;

  int strokeIdx = 0;
  TXshLevelP prevLevel;
  TFrameId prevFrame;
  bool animationOpen = false;
  bool levelChanged  = false;
  std::map<std::string, std::string> attr;

  for (int x = 0; x < m_xsheet->getColumnCount(); x++) {
    TXshColumn *column = m_xsheet->getColumn(x);
    if (!column) continue;
    TXshLevelColumn *levelColumn = column->getLevelColumn();
    if (!levelColumn || levelColumn->isEmpty()) continue;

    vector<TXshCell> cells;
    int f0, f1;
    levelColumn->getRange(f0, f1);
    for (int f = f0; f <= f1; f++) {
      const TXshCell &cell = levelColumn->getCell(f);
      if (std::find(cells.begin(), cells.end(), cell) != cells.end()) continue;
      cells.push_back(cell);

      TFrameId frame         = cell.getFrameId();
      TXshSimpleLevel *level = cell.getSimpleLevel();
      if (!level) continue;
      TImageP image = level->getFrame(frame, false);
      if (!image) continue;
      TVectorImageP vectorImage = {
          dynamic_cast<TVectorImage *>(image.getPointer())};
      if (!vectorImage) continue;

      for (int i = 0; i < vectorImage->getStrokeCount(); i++) {
        StrokeId strokeKey{m_xsheet, cell, vectorImage->getStroke(i)};

        map<StrokeId, shared_ptr<PathAnimation>>::iterator animation =
            m_shapeAnimation.find(strokeKey);
        if (animation == m_shapeAnimation.end()) continue;
        shared_ptr<PathAnimation> shapeAnimation = animation->second;
        TStroke *stroke                          = strokeKey.stroke();
        TXshLevelP level                         = cell.m_level;

        if (!level || level != prevLevel || frame != prevFrame) {
          prevLevel    = level;
          prevFrame    = frame;
          levelChanged = true;
          strokeIdx    = 0;
        }

        strokeIdx++;

        if (levelChanged) {
          levelChanged = false;

          // Processing a new cell, start a new animation
          if (animationOpen) os.closeChild();  // animation

          os.openChild("pathanimation");
          animationOpen = true;

          os.child("frame") << cell.m_level.getPointer() << frame.expand();
        }

        attr.clear();
        attr["id"]   = std::to_string(stroke->getId());
        attr["name"] = stroke->name().toStdString();
        attr["idx"]  = std::to_string(strokeIdx);
        if (shapeAnimation->isActivated()) {
          os.openChild("stroke", attr);
          for (int n = 0; n < shapeAnimation->chunkCount(); n++) {
            attr.clear();
            attr["id"] = std::to_string(n + 1);
            os.openChild("handle", attr);
            TParamSetP chunk = shapeAnimation->chunkParam(n);

            for (int p = 0; p < 3; p++) {
              TThickPointParamP point = chunk->getParam(p);
              os.openChild("p" + std::to_string(p));
              os.child("x") << *point->getX();
              os.child("y") << *point->getY();
              os.child("thickness") << *point->getThickness();
              os.closeChild();  // p#
            }
            os.closeChild();  // chunk
          }
          os.closeChild();  // stroke
        } else
          os.openCloseChild("stroke", attr);
      }
    }
  }

  if (animationOpen) os.closeChild();  // animation
}
