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
    : m_animations(animations), m_strokeId(strokeId), m_activated(false) {
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
  for (int i = 0; i < vi->getStrokeCount(); i++) {
    StrokeId strokeId{m_xsheet, cell, vi->getStroke(i)};

    shared_ptr<PathAnimation> animation = addStroke(strokeId);
    animation->animate(frame);
  }
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

shared_ptr<PathAnimation> PathAnimations::appStroke(const TApplication *app,
                                                    TStroke *stroke) {
  return appAnimations(app)->addStroke(appStrokeId(app, stroke));
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
                if (vi) {
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
              if (tagName == "chunk") {
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
            os.openChild("chunk", attr);
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
