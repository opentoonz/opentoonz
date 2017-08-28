

#include "toonz/txshsoundcolumn.h"
#include "toonz/levelset.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshsoundlevel.h"

#include "tstream.h"
#include "toutputproperties.h"
#include "tsop.h"
#include "tconvert.h"

#include <QAudioFormat>
#include <QAudioDeviceInfo>

//=============================================================================
//  ColumnLevel
//=============================================================================

class ColumnLevel {
  TXshSoundLevelP m_soundLevel;

  /*!Offsets: in frames. Start offset is a positive number.*/
  int m_startOffset;
  /*!Offsets: in frames. End offset is a positive number(to subtract to..).*/
  int m_endOffset;

  //! Starting frame in the timeline
  int m_startFrame;

  //! frameRate
  double m_fps;

public:
  ColumnLevel(TXshSoundLevel *soundLevel = 0, int startFrame = -1,
              int startOffset = -1, int endOffset = -1, double fps = -1);
  ~ColumnLevel();
  ColumnLevel *clone() const;

  //! Overridden from TXshLevel
  TXshSoundLevel *getSoundLevel() const { return m_soundLevel.getPointer(); }
  void setSoundLevel(TXshSoundLevelP level) { m_soundLevel = level; }

  void loadData(TIStream &is);
  void saveData(TOStream &os);

  void setStartOffset(int value);
  int getStartOffset() const { return m_startOffset; }

  void setEndOffset(int value);
  int getEndOffset() const { return m_endOffset; }

  void setOffsets(int startOffset, int endOffset);

  //! Return the starting frame without offsets.
  void setStartFrame(int frame) { m_startFrame = frame; }
  int getStartFrame() const { return m_startFrame; }

  //! Return the ending frame without offsets.
  int getEndFrame() const;

  //! Return frame count without offset.
  int getFrameCount() const;

  //! Return frame count with offset.
  int getVisibleFrameCount() const;

  //! Return start frame with offset.
  int getVisibleStartFrame() const;
  //! Return last frame with offset.
  int getVisibleEndFrame() const;
  //! Updates m_startOfset and m_endOffset.
  void updateFrameRate(double newFrameRate);

  void setFrameRate(double fps) { m_fps = fps; }
};

//=============================================================================

ColumnLevel::ColumnLevel(TXshSoundLevel *soundLevel, int startFrame,
                         int startOffset, int endOffset, double fps)
    : m_soundLevel(soundLevel)
    , m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_startFrame(startFrame)
    , m_fps(fps) {}

//-----------------------------------------------------------------------------

ColumnLevel::~ColumnLevel() {}

//-----------------------------------------------------------------------------

ColumnLevel *ColumnLevel::clone() const {
  ColumnLevel *soundColumnLevel = new ColumnLevel();
  soundColumnLevel->setSoundLevel(getSoundLevel());
  soundColumnLevel->setStartFrame(m_startFrame);
  soundColumnLevel->setStartOffset(m_startOffset);
  soundColumnLevel->setEndOffset(m_endOffset);
  soundColumnLevel->setFrameRate(m_fps);
  return soundColumnLevel;
}

//-----------------------------------------------------------------------------

void ColumnLevel::loadData(TIStream &is) {
  std::string tagName;
  is.openChild(tagName);
  if (tagName == "SoundCells") {
    TPersist *p = 0;
    is >> m_startOffset >> m_endOffset >> m_startFrame >> p;
    TXshSoundLevel *xshLevel   = dynamic_cast<TXshSoundLevel *>(p);
    if (xshLevel) m_soundLevel = xshLevel;
  }
  is.closeChild();
}

//-----------------------------------------------------------------------------

void ColumnLevel::saveData(TOStream &os) {
  os.child("SoundCells") << getStartOffset() << getEndOffset()
                         << getStartFrame() << m_soundLevel.getPointer();
}

//-----------------------------------------------------------------------------

void ColumnLevel::setStartOffset(int value) {
  if (!m_soundLevel) return;
  if (value < 0 || value > getFrameCount() - getEndOffset() - 1) return;
  m_startOffset = value;
}

//-----------------------------------------------------------------------------

void ColumnLevel::setEndOffset(int value) {
  if (!m_soundLevel) return;
  if (value < 0 ||
      getStartFrame() + getFrameCount() - value <
          getStartFrame() + getStartOffset() + 1)
    return;
  m_endOffset = value;
}

//-----------------------------------------------------------------------------

void ColumnLevel::setOffsets(int startOffset, int endOffset) {
  assert(startOffset > 0 && endOffset > 0);

  if (startOffset < 0 || startOffset > getFrameCount() - endOffset - 1) return;
  m_startOffset = startOffset;
  if (endOffset < 0 ||
      getStartFrame() + getFrameCount() - endOffset <
          getStartFrame() + getStartOffset() + 1)
    return;
  m_endOffset = endOffset;
}

//-----------------------------------------------------------------------------

int ColumnLevel::getEndFrame() const {
  if (!m_soundLevel) return -1;
  return getStartFrame() + getFrameCount() - 1;
}

//-----------------------------------------------------------------------------

int ColumnLevel::getFrameCount() const {
  if (!m_soundLevel) return -1;
  return m_soundLevel->getFrameCount();
}

//-----------------------------------------------------------------------------

int ColumnLevel::getVisibleFrameCount() const {
  if (!m_soundLevel) return -1;
  return getFrameCount() - getStartOffset() - getEndOffset();
}

//-----------------------------------------------------------------------------

int ColumnLevel::getVisibleStartFrame() const {
  if (!m_soundLevel) return -1;
  return getStartFrame() + getStartOffset();
}

//-----------------------------------------------------------------------------

int ColumnLevel::getVisibleEndFrame() const {
  if (!m_soundLevel) return -1;
  return getVisibleStartFrame() + getVisibleFrameCount() - 1;
}

//-----------------------------------------------------------------------------

void ColumnLevel::updateFrameRate(double newFrameRate) {
  if (m_fps == newFrameRate) return;
  double fact   = (double)newFrameRate / m_fps;
  m_startFrame  = tround(m_startFrame * fact);
  m_startOffset = tround(m_startOffset * fact);
  m_endOffset   = tround(m_endOffset * fact);
  m_fps         = newFrameRate;
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

bool lessThan(const ColumnLevel *s1, const ColumnLevel *s2) {
  return s1->getVisibleStartFrame() < s2->getVisibleStartFrame();
}

//-----------------------------------------------------------------------------
}
//=============================================================================

TXshSoundColumn::TXshSoundColumn()
    : m_player(0)
    , m_volume(0.4)
    , m_currentPlaySoundTrack(0)
    , m_isOldVersion(false) {
  m_timer.setInterval(500);
  m_timer.setSingleShot(true);
  m_timer.stop();

  connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimerOut()));
}

//-----------------------------------------------------------------------------

TXshSoundColumn::~TXshSoundColumn() {
  clear();
  if (m_timer.isActive()) {
    m_timer.stop();
    stop();
  }
}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshSoundColumn::getColumnType() const {
  return eSoundType;
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::canSetCell(const TXshCell &cell) const {
  return cell.isEmpty() || cell.m_level->getSoundLevel() != 0;
}

//-----------------------------------------------------------------------------

TXshColumn *TXshSoundColumn::clone() const {
  TXshSoundColumn *column = new TXshSoundColumn();
  column->setVolume(m_volume);
  column->setXsheet(getXsheet());
  int i;
  for (i = 0; i < m_levels.size(); i++)
    column->insertColumnLevel(m_levels.at(i)->clone(), i);

  return column;
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::assignLevels(const TXshSoundColumn *src) {
  // Svuoto la lista dei livelli
  clear();

  int r;
  for (r = src->getFirstRow(); r <= src->getMaxFrame(); r++) {
    int r0, r1;
    if (!src->getLevelRange(r, r0, r1)) continue;
    TXshCell cell = src->getCell(r);
    if (cell.isEmpty()) continue;
    TXshSoundLevel *soundLevel = cell.m_level->getSoundLevel();
    assert(!!soundLevel);
    int startOffset = cell.getFrameId().getNumber();
    int startFrame  = r - startOffset;
    int endOffset   = startFrame + soundLevel->getFrameCount() - r1 - 1;
    ColumnLevel *l =
        new ColumnLevel(soundLevel, startFrame, startOffset, endOffset);
    insertColumnLevel(l);
    r = r1;
  }
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::loadData(TIStream &is) {
  VersionNumber tnzVersion = is.getVersion();
  if (tnzVersion < VersionNumber(1, 17)) {
    TFilePath path;
    is >> path;
    m_isOldVersion = true;
    int offset     = 0;
    is >> offset;
    is >> m_volume;
    if (!is.eos()) {
      int status;
      is >> status;
      setStatusWord(status);
    }
    TXshSoundLevelP level = new TXshSoundLevel(path.getWideName());
    level->setPath(path);
    ColumnLevel *l = new ColumnLevel(level.getPointer(), offset, 0, 0);
    insertColumnLevel(l);
    return;
  }
  is >> m_volume;
  int levelCount = 0;
  is >> levelCount;

  int i;
  for (i = 0; i < levelCount; i++) {
    ColumnLevel *sl = new ColumnLevel();
    sl->loadData(is);
    insertColumnLevel(sl, i);
  }
  if (!is.eos()) {
    int status;
    is >> status;
    setStatusWord(status);
  }
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::saveData(TOStream &os) {
  os << m_volume;
  int levelsCount = m_levels.size();
  os << levelsCount;
  if (levelsCount == 0) return;
  int i;
  for (i = 0; i < levelsCount; i++) m_levels.at(i)->saveData(os);
  os << getStatusWord();
}

//-----------------------------------------------------------------------------

int TXshSoundColumn::getRange(int &r0, int &r1) const {
  r0 = getFirstRow();
  r1 = getMaxFrame();
  return r1 - r0 + 1;
}

//-----------------------------------------------------------------------------

int TXshSoundColumn::getRowCount() const {
  return getMaxFrame() - getFirstRow();
}

//-----------------------------------------------------------------------------

int TXshSoundColumn::getMaxFrame() const {
  int levelsCount = m_levels.size();
  if (levelsCount == 0) return -1;
  return m_levels.at(levelsCount - 1)->getVisibleEndFrame();
}

//-----------------------------------------------------------------------------

int TXshSoundColumn::getFirstRow() const {
  if (m_levels.size() == 0) return -1;
  return m_levels.at(0)->getVisibleStartFrame();
}

//-----------------------------------------------------------------------------

const TXshCell &TXshSoundColumn::getCell(int row) const {
  static TXshCell emptyCell;

  ColumnLevel *l = getColumnLevelByFrame(row);
  if (row < 0 || row < getFirstRow() || row > getMaxFrame()) {
    if (l) emptyCell.m_level = l->getSoundLevel();
    return emptyCell;
  }

  if (!l) return emptyCell;
  TXshSoundLevel *soundLevel = l->getSoundLevel();
  TXshCell *cell = new TXshCell(soundLevel, TFrameId(row - l->getStartFrame()));
  // La nuova cella aggiunge un reference al TXshSoundLevel; poiche' le celle
  // delle
  // TXshSoundColumn non sono strutture persistenti ma sono strutture dinamiche
  // (vengono ricreate ogni volta) devo occuparmi di fare il release altrimenti
  // il
  // TXshSoundLevel non viene mai buttato.
  soundLevel->release();
  return *cell;
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::isCellEmpty(int row) const {
  if (m_levels.empty()) return true;
  ColumnLevel *l = getColumnLevelByFrame(row);
  if (!l || !l->getSoundLevel()) return true;
  return false;
}

//-----------------------------------------------------------------------------
//--- debug only
void TXshSoundColumn::checkColumn() const {
#ifndef NDEBUG
  int size = m_levels.size();
  int i;
  for (i = 0; i < size; i++) {
    ColumnLevel *level = m_levels.at(i);
    assert(level);
    assert(level->getSoundLevel());
    int start1 = level->getVisibleStartFrame();
    int end1   = level->getVisibleEndFrame();
    assert(start1 <= end1);
    if (i < size - 1) {
      ColumnLevel *nextLevel = m_levels.at(i + 1);
      assert(nextLevel);
      assert(nextLevel->getSoundLevel());
      int start2 = nextLevel->getVisibleStartFrame();
      int end2   = nextLevel->getVisibleEndFrame();
      assert(start2 <= end2);
      assert(end1 < start2);
    }
  }
#endif
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::getCells(int row, int rowCount, TXshCell cells[]) {
  // le celle da settare sono [ra, rb]
  int ra = row;
  int rb = row + rowCount - 1;
  assert(ra <= rb);
  TXshCell *dstCell = cells;
  int r;
  for (r = ra; r <= rb; r++) *cells++ = getCell(r);
  checkColumn();
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::setCellInEmptyFrame(int row, const TXshCell &cell) {
  if (cell.isEmpty()) return;
  TXshSoundLevel *soundLevel = cell.getSoundLevel();
  if (!soundLevel) return;
  int startOffset = cell.getFrameId().getNumber();
  int startFrame  = row - startOffset;
  int endOffset   = startFrame + soundLevel->getFrameCount() - 1 - row;
  insertColumnLevel(
      new ColumnLevel(soundLevel, startFrame, startOffset, endOffset));
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::setCell(int row, const TXshCell &cell) {
  return setCell(row, cell, false);
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::setCell(int row, const TXshCell &cell,
                              bool updateSequence) {
  if (!canSetCell(cell)) return false;
  assert(row >= 0);
  ColumnLevel *lBefore = getColumnLevelByFrame(row - 1);
  ColumnLevel *l       = getColumnLevelByFrame(row);
  ColumnLevel *lNext   = getColumnLevelByFrame(row + 1);

  // Se non devo fare un update della sequenzialita' e la cella e' uguale a
  // quella gia' esistente ritorno.
  if (!updateSequence && l && l->getSoundLevel() == cell.getSoundLevel() &&
      row - l->getStartFrame() == cell.getFrameId().getNumber())
    return false;

  bool isBeforeLevelInSequence =
      (lBefore && lBefore->getSoundLevel() == cell.getSoundLevel() &&
       row - lBefore->getStartFrame() == cell.getFrameId().getNumber());

  bool isNextLevelInSequence =
      (lNext && lNext->getSoundLevel() == cell.getSoundLevel() &&
       row - lNext->getStartFrame() == cell.getFrameId().getNumber());

  // Modifico solo l'endOffset del BeforeLevel
  if (isBeforeLevelInSequence) {
    int oldEndOffset = lBefore->getEndOffset();
    int endOffset    = (lBefore->getVisibleEndFrame() != row) ? oldEndOffset - 1
                                                           : oldEndOffset;
    if (isNextLevelInSequence) {
      endOffset = lNext->getEndOffset();
      // Se precedente e successivo sono diversi elimino il successivo (caso in
      // cui inserisci un frame in successione su una cella vuota)
      if (lNext != lBefore) removeColumnLevel(lNext);
    }
    if (l && l != lBefore) {
      if (l->getVisibleFrameCount() == 1)
        removeColumnLevel(l);
      else
        l->setStartOffset(l->getStartOffset() + 1);
    }
    lBefore->setEndOffset(endOffset);
    checkColumn();
    return true;
  }
  // Modifico solo lo startOffset del NextLevel
  if (isNextLevelInSequence) {
    int oldStartOffset = lNext->getStartOffset();
    int startOffset    = (lNext->getVisibleStartFrame() != row)
                          ? oldStartOffset - 1
                          : oldStartOffset;
    if (l && l != lNext) {
      if (l->getVisibleFrameCount() == 1)
        removeColumnLevel(l);
      else
        l->setEndOffset(l->getEndOffset() + 1);
    }
    lNext->setStartOffset(startOffset);
    checkColumn();
    return true;
  }

  // I Frame non sono in successione quindi devo inserire la cella in frame un
  // vuoto:
  clearCells(row, 1);
  setCellInEmptyFrame(row, cell);
  checkColumn();
  return true;
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::setCells(int row, int rowCount, const TXshCell cells[]) {
  assert(row >= 0);
  // le celle da settare sono [ra, rb]
  int ra = row;
  int rb = row + rowCount - 1;
  assert(ra <= rb);
  bool ret = false;
  int r;
  for (r = ra; r <= rb; r++) {
    bool isOneCellSet     = setCell(r, cells[r - ra]);
    if (isOneCellSet) ret = isOneCellSet;
  }
  return ret;
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::insertEmptyCells(int row, int rowCount) {
  if (m_levels.isEmpty())
    return;  // se la colonna e' vuota non devo inserire celle

  int ra = row;
  int rb = row + rowCount - 1;
  assert(ra <= rb);

  // Se il livello nella riga ra inizia prima devo "spezzarlo"
  ColumnLevel *l = getColumnLevelByFrame(ra);
  if (l && l->getVisibleStartFrame() < ra) {
    int oldOffset = l->getEndOffset();
    l->setEndOffset(oldOffset + l->getVisibleEndFrame() - ra + 1);
    insertColumnLevel(new ColumnLevel(l->getSoundLevel(), l->getStartFrame(),
                                      row - l->getStartFrame(), oldOffset));
  }

  // Sposto il first frame di tutti i livelli successivi ad ra.
  int i;
  for (i = m_levels.count() - 1; i >= 0; i--) {
    ColumnLevel *cl = m_levels.at(i);
    if (cl->getVisibleStartFrame() < ra) continue;
    cl->setStartFrame(cl->getStartFrame() + rowCount);
  }
  checkColumn();
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::removeCells(int row, int rowCount, bool shift) {
  // le celle da cancellare sono [ra, rb]
  int ra = row;
  int rb = row + rowCount - 1;
  assert(ra <= rb);

  int i;
  for (i = m_levels.size() - 1; i >= 0; i--) {
    ColumnLevel *l = m_levels.at(i);
    if (!l) continue;  // Non dovrebbe accadere!
    int visibleStartFrame = l->getVisibleStartFrame();
    int visibleEndFrame   = l->getVisibleEndFrame();
    // Fuori dal range da tagliare.
    if (visibleStartFrame > rb || visibleEndFrame < ra) continue;
    int newEndOffset   = l->getEndFrame() - ra + 1;
    int newStartOffset = rb - l->getStartFrame() + 1;
    // Il livello contiene il range da cancellare: devo "spezzare" il livello in
    // due.
    if (visibleStartFrame < ra && visibleEndFrame > rb) {
      int oldEndOffset = l->getEndOffset();
      l->setEndOffset(newEndOffset);
      insertColumnLevel(new ColumnLevel(l->getSoundLevel(), l->getStartFrame(),
                                        newStartOffset, oldEndOffset));
      continue;
    }
    if (visibleStartFrame < ra) l->setEndOffset(newEndOffset);
    if (visibleEndFrame > rb) l->setStartOffset(newStartOffset);
    // Il livello e' contenuto nel range da cancellare: lo rimuovo.
    if (visibleStartFrame >= ra && visibleEndFrame <= rb) removeColumnLevel(l);
  }

  if (shift) {
    for (i = m_levels.size() - 1; i >= 0; i--) {
      ColumnLevel *l = m_levels.at(i);
      if (!l) continue;  // Non dovrebbe accadere!
      int visibleStartFrame = l->getVisibleStartFrame();
      int visibleEndFrame   = l->getVisibleEndFrame();
      if (visibleStartFrame > ra)
        l->setStartFrame(l->getStartFrame() - rowCount);
    }

    // Devo controllare se il livello precedente il taglio e quello successivo
    // sono in sequenza
    ColumnLevel *beforeL = getColumnLevelByFrame(ra - 1);
    ColumnLevel *nextL   = getColumnLevelByFrame(ra);
    if (beforeL && nextL &&
        beforeL->getSoundLevel() == nextL->getSoundLevel() &&
        beforeL->getStartFrame() == nextL->getStartFrame()) {
      beforeL->setEndOffset(nextL->getEndOffset());
      removeColumnLevel(nextL);
    }
  }

  checkColumn();
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::clearCells(int row, int rowCount) {
  if (rowCount <= 0) return;
  if (m_levels.isEmpty())
    return;  // se la colonna e' vuota non devo "sbiancare" celle
  removeCells(row, rowCount, false);
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::removeCells(int row, int rowCount) {
  removeCells(row, rowCount, true);
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::updateCells(int row, int rowCount) {
  int r;
  for (r = row; r < row + rowCount; r++) {
    TXshCell cell = getCell(r);
    setCell(row, cell, true);
  }
}

//-----------------------------------------------------------------------------

int TXshSoundColumn::modifyCellRange(int row, int delta,
                                     bool modifyStartValue) {
  ColumnLevel *l = getColumnLevelByFrame(row);
  if (!l) return -1;

  int startVisibleFrame = l->getVisibleStartFrame();
  int endVisibleFrame   = l->getVisibleEndFrame();
  // Non dovrebbe mai accadere, metto un controllo di sicurezza.
  if (startVisibleFrame != row && endVisibleFrame != row) return -1;

  int r0 = delta > 0 ? row : row + delta;
  int r1 = delta > 0 ? row + delta : row;

  int r;
  for (r = r0; r <= r1; r++) {
    ColumnLevel *oldL = getColumnLevelByFrame(r);
    if (!oldL || oldL == l) continue;
    int visibleStartFrame = oldL->getVisibleStartFrame();
    int visibleEndFrame   = oldL->getVisibleEndFrame();
    if (visibleStartFrame >= r0 && visibleEndFrame <= r1)
      removeColumnLevel(oldL);
    if (visibleStartFrame <= r0)
      oldL->setEndOffset(oldL->getEndOffset() + visibleEndFrame - r0 + 1);
    if (visibleEndFrame >= r1)
      oldL->setStartOffset(oldL->getStartOffset() + r1 - visibleStartFrame + 1);
    r = visibleEndFrame;
  }

  if (modifyStartValue) {
    // Evito che vada oltre il limite infieriore
    if (endVisibleFrame < startVisibleFrame + delta)
      delta            = endVisibleFrame - startVisibleFrame;
    int newStartOffset = l->getStartOffset() + delta;
    // Evito che vada oltre il limite superiore
    if (newStartOffset < 0) newStartOffset = 0;
    l->setStartOffset(newStartOffset);
    checkColumn();
    getXsheet()->updateFrameCount();
    return l->getVisibleStartFrame();
  }

  // Evito che vada oltre il limite superiore
  if (startVisibleFrame > endVisibleFrame + delta)
    delta = startVisibleFrame - endVisibleFrame;
  // Evito che vada oltre il limite inferiore
  int newEndOffset                   = l->getEndOffset() - delta;
  if (newEndOffset < 0) newEndOffset = 0;
  l->setEndOffset(newEndOffset);
  checkColumn();
  getXsheet()->updateFrameCount();
  return l->getVisibleEndFrame();
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::getLevelRange(int row, int &r0, int &r1) const {
  ColumnLevel *l = getColumnLevelByFrame(row);
  if (!l) {
    r0 = r1 = row;
    return false;
  }
  r0 = l->getVisibleStartFrame();
  r1 = l->getVisibleEndFrame();
  return true;
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::getLevelRangeWithoutOffset(int row, int &r0,
                                                 int &r1) const {
  ColumnLevel *l = getColumnLevelByFrame(row);
  if (!l) {
    r0 = r1 = row;
    return false;
  }
  r0 = l->getStartFrame();
  r1 = l->getEndFrame();
  return true;
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::setXsheet(TXsheet *xsheet) {
  TXshColumn::setXsheet(xsheet);
  ToonzScene *scene = xsheet->getScene();
  if (!scene || m_levels.empty()) return;

  if (m_isOldVersion) {
    assert(m_levels.size() == 1);
    scene->getLevelSet()->insertLevel(m_levels.at(0)->getSoundLevel());
    m_isOldVersion = false;
  }

  TOutputProperties *properties = scene->getProperties()->getOutputProperties();
  setFrameRate(properties->getFrameRate());
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::setFrameRate(double fps) {
  if (m_levels.empty()) return;
  int i;
  for (i = 0; i < m_levels.size(); i++) {
    ColumnLevel *column        = m_levels.at(i);
    TXshSoundLevel *soundLevel = column->getSoundLevel();
    if (soundLevel->getFrameRate() != fps) soundLevel->setFrameRate(fps);
    column->setFrameRate(fps);
  }
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::updateFrameRate(double fps) {
  if (m_levels.empty()) return;
  int i;
  for (i = 0; i < m_levels.size(); i++) {
    ColumnLevel *column        = m_levels.at(i);
    TXshSoundLevel *soundLevel = column->getSoundLevel();
    double oldFps              = soundLevel->getFrameRate();
    if (oldFps != fps) soundLevel->setFrameRate(fps);
    column->updateFrameRate(fps);
  }
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::setVolume(double value) {
  m_volume = tcrop<double>(value, 0.0, 1.0);
  if (m_player && m_player->isPlaying())
#ifdef MACOSX
    m_player->setVolume(m_volume);
#else
    stop();
#endif
}

//-----------------------------------------------------------------------------

double TXshSoundColumn::getVolume() const { return m_volume; }

//-----------------------------------------------------------------------------

void TXshSoundColumn::play(TSoundTrackP soundtrack, int s0, int s1, bool loop) {
  if (!TSoundOutputDevice::installed()) return;

  if (!m_player) m_player = new TSoundOutputDevice();

  if (m_player) {
    try {
#ifdef MACOSX
      m_player->prepareVolume(m_volume);
      TSoundTrackP mixedTrack = soundtrack;
#else
      TSoundTrackP mixedTrack = TSoundTrack::create(
          soundtrack->getFormat(), soundtrack->getSampleCount());
      mixedTrack = TSop::mix(mixedTrack, soundtrack, 1.0, m_volume);
#endif
      m_player->play(mixedTrack, s0, s1, loop);
      m_currentPlaySoundTrack = mixedTrack;
#ifndef MACOSX
      m_timer.start();
#endif
    } catch (TSoundDeviceException &) {
    }
  }
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::play(ColumnLevel *columnLevel, int currentFrame) {
  assert(columnLevel);
  TXshSoundLevel *soundLevel = columnLevel->getSoundLevel();
  int offset                 = 0;
  int startFrame             = currentFrame - columnLevel->getStartFrame();
  int endFrame = soundLevel->getFrameCount() - columnLevel->getEndOffset();
  int spf      = soundLevel->getSamplePerFrame();
  int s0       = startFrame * spf;
  int s1       = endFrame * spf;

  if (!soundLevel->getSoundTrack()) return;
  play(soundLevel->getSoundTrack(), s0, s1, false);
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::play(int currentFrame) {
  TSoundTrackP soundTrack = getOverallSoundTrack(currentFrame);

  if (!soundTrack) return;

  int spf        = m_levels.at(0)->getSoundLevel()->getSamplePerFrame();
  int startFrame = (currentFrame - getFirstRow());

  int s0 = startFrame * spf;
  int s1 = getMaxFrame() * spf;

  play(soundTrack, s0, s1, false);
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::stop() {
  if (m_player) {
    m_player->stop();
    m_player->close();
    m_player                = 0;
    m_currentPlaySoundTrack = TSoundTrackP();
  }
}

//-----------------------------------------------------------------------------

bool TXshSoundColumn::isPlaying() const {
  return m_player && m_player->isPlaying();
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::insertColumnLevel(ColumnLevel *columnLevel, int index) {
  if (index == -1) index = m_levels.size();
  m_levels.insert(index, columnLevel);
  qSort(m_levels.begin(), m_levels.end(), lessThan);
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::removeColumnLevel(ColumnLevel *columnLevel) {
  if (!columnLevel) return;
  int index = m_levels.indexOf(columnLevel);
  if (index == -1) return;
  m_levels.removeAt(index);
  delete columnLevel;
}

//-----------------------------------------------------------------------------

ColumnLevel *TXshSoundColumn::getColumnLevelByFrame(int frame) const {
  int levelCount = m_levels.size();
  for (int i = 0; i < levelCount; i++) {
    ColumnLevel *l             = m_levels.at(i);
    TXshSoundLevel *soundLevel = l->getSoundLevel();
    int startFrame             = l->getStartFrame() + l->getStartOffset();
    int frameCount =
        soundLevel->getFrameCount() - l->getStartOffset() - l->getEndOffset();
    if (startFrame <= frame && startFrame + frameCount > frame) return l;
  }
  return 0;
}

//-----------------------------------------------------------------------------

ColumnLevel *TXshSoundColumn::getColumnLevel(int index) {
  if (index < 0 || index >= m_levels.count()) return 0;
  return m_levels.at(index);
}

//-----------------------------------------------------------------------------

int TXshSoundColumn::getColumnLevelIndex(ColumnLevel *columnLevel) const {
  return m_levels.indexOf(columnLevel);
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::clear() {
  int sequenceCount = m_levels.size();
  for (int i = 0; i < sequenceCount; i++) {
    ColumnLevel *s = m_levels[i];
    delete s;
  }
  m_levels.clear();
}
//-----------------------------------------------------------------------------

void TXshSoundColumn::onTimerOut() {
#ifndef MACOSX
  if (m_player && m_player->isAllQueuedItemsPlayed()) stop();
#endif
}

//-----------------------------------------------------------------------------

void TXshSoundColumn::scrub(int fromFrame, int toFrame) {
  if (!isCamstandVisible()) return;
  TSoundTrackP soundTrack = getOverallSoundTrack(fromFrame, toFrame + 1);
  if (!soundTrack) return;

  play(soundTrack, 0, soundTrack->getSampleCount(), false);
}

//-----------------------------------------------------------------------------

TSoundTrackP TXshSoundColumn::getOverallSoundTrack(int fromFrame, int toFrame,
                                                   double fps,
                                                   TSoundTrackFormat format) {
  int levelsCount = m_levels.size();

  if (levelsCount == 0) return 0;

  if (fps == -1) fps             = m_levels[0]->getSoundLevel()->getFrameRate();
  if (fromFrame == -1) fromFrame = getFirstRow();
  if (toFrame == -1) toFrame     = getMaxFrame();

  if (format.m_sampleRate == 0) {
    // Found the best format inside the soundsequences
    int sampleRate    = 0;
    int bitsPerSample = 8;
    int channels      = 1;
    for (int i = 0; i < levelsCount; i++) {
      TXshSoundLevel *soundLevel = m_levels.at(i)->getSoundLevel();
      TSoundTrackP soundTrack    = soundLevel->getSoundTrack();
      if (soundTrack.getPointer() == 0) continue;
      TSoundTrackFormat f = soundTrack->getFormat();
      if ((int)f.m_sampleRate > sampleRate) {
        format.m_sampleRate   = f.m_sampleRate;
        sampleRate            = f.m_sampleRate;
        format.m_channelCount = f.m_channelCount;
        channels              = f.m_channelCount;
        format.m_signedSample = f.m_signedSample;
        format.m_bitPerSample = f.m_bitPerSample;
        bitsPerSample         = f.m_bitPerSample;
      }
      if (f.m_channelCount > channels) {
        format.m_channelCount = f.m_channelCount;
        channels              = f.m_channelCount;
      }
      if (f.m_bitPerSample > bitsPerSample) {
        format.m_bitPerSample = f.m_bitPerSample;
        bitsPerSample         = f.m_bitPerSample;
      }
    }
  }

  // If there is no soundSequence I use a default format
  if (levelsCount == 0 || format.m_sampleRate == 0) {
    format.m_sampleRate   = 44100;
    format.m_bitPerSample = 16;
    format.m_channelCount = 1;
    format.m_signedSample = true;
  }

// We prefer to have 22050 as a maximum sampleRate (to avoid crashes or
// another issues)
#ifndef MACOSX
  if (format.m_sampleRate >= 44100) format.m_sampleRate = 22050;
#else
  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  QList<int> ssrs = info.supportedSampleRates();
  if (!ssrs.contains(format.m_sampleRate)) format.m_sampleRate = 44100;
  QAudioFormat qFormat;
  qFormat.setSampleRate(format.m_sampleRate);
  qFormat.setSampleType(format.m_signedSample ? QAudioFormat::SignedInt
                                              : QAudioFormat::UnSignedInt);
  qFormat.setSampleSize(format.m_bitPerSample);
  qFormat.setCodec("audio/pcm");
  qFormat.setChannelCount(format.m_channelCount);
  qFormat.setByteOrder(QAudioFormat::LittleEndian);
  if (!info.isFormatSupported((qFormat))) {
    qFormat               = info.nearestFormat(qFormat);
    format.m_bitPerSample = qFormat.sampleSize();
    format.m_channelCount = qFormat.channelCount();
    format.m_sampleRate   = qFormat.sampleRate();
    if (qFormat.sampleType() == QAudioFormat::SignedInt)
      format.m_signedSample = true;
    else
      format.m_signedSample = false;
  }
#endif
  // Create the soundTrack
  double samplePerFrame = format.m_sampleRate / fps;

  // In seconds
  double duration = double(toFrame - fromFrame) / fps;

  // Seconds to Sample  (it is more accurate than using sampleCount
  double dsamp = duration * format.m_sampleRate;
  TINT32 lsamp = (TINT32)dsamp;
  if ((double)lsamp < dsamp - TConsts::epsilon) lsamp++;

  TSoundTrackP overallSoundTrack;

  try {
    overallSoundTrack = TSoundTrack::create(format, lsamp);
  } catch (TSoundDeviceException &) {
  }

  // Blank the whole track
  overallSoundTrack->blank(0, lsamp);

  if (levelsCount == 0) return overallSoundTrack;

  for (int i = 0; i < levelsCount; i++) {
    ColumnLevel *l             = m_levels.at(i);
    TXshSoundLevel *soundLevel = l->getSoundLevel();

    // Check if the soundtrack is inside the frame Range.
    int levelStartFrame = l->getStartFrame() + l->getStartOffset();
    int levelEndFrame   = levelStartFrame + soundLevel->getFrameCount() -
                        l->getStartOffset() - l->getEndOffset();
    // If the soundSequence is completely before fromFrame I will skip it
    if ((levelStartFrame < fromFrame && levelEndFrame < fromFrame)) continue;
    // If the soundSequence is after toFrame I don't check the others (that
    // comes later..)
    if ((levelStartFrame > toFrame && levelEndFrame > toFrame)) break;

    // Extract the right soundTrack (using the offsets)
    // (pay attention to convert it before)
    TSoundTrackP s = soundLevel->getSoundTrack();
    if (!s) continue;
    TSoundTrackP soundTrack = TSop::convert(s, format);

    int s0delta                              = 0;
    if (fromFrame > levelStartFrame) s0delta = fromFrame - levelStartFrame;

    int s0 = (l->getStartOffset() + s0delta) * samplePerFrame;

    int s1delta                          = 0;
    if (toFrame < levelEndFrame) s1delta = levelEndFrame - toFrame;
    int s1 = (soundLevel->getFrameCount() - l->getEndOffset() - s1delta) *
             samplePerFrame;

    if (s1 > 0 && s1 >= s0) {
      soundTrack = soundTrack->extract(s0, s1);

      // Copy the sound track
      overallSoundTrack->copy(
          soundTrack,
          int((levelStartFrame - fromFrame) *
              samplePerFrame));  // The int cast is IMPORTANT, since
    }                            // there are 2 overloads (int & double)
  }                              // with DIFFERENT SCALES. We mean the
                                 // SAMPLES-BASED one.
  return overallSoundTrack;
}

//-----------------------------------------------------------------------------

TSoundTrackP TXshSoundColumn::mixingTogether(
    const std::vector<TXshSoundColumn *> &vect, int fromFrame, int toFrame,
    double fps) {
  int offset       = 0xffff;
  long sampleCount = 0;
  std::vector<TSoundTrackP> tracks;
  TSoundTrackP mix;

  int size = vect.size(), i = 0;

  ColumnLevel *l = vect[0]->getColumnLevel(0);
  if (!l)        // May happen if the sound level is not
    return mix;  // correctly loaded from disk

  TXshSoundLevel *soundLevel = l->getSoundLevel();
  assert(soundLevel);

  if (fps == -1) fps             = soundLevel->getFrameRate();
  if (fromFrame == -1) fromFrame = 0;
  if (toFrame == -1) toFrame     = getXsheet()->getFrameCount();

  if (!soundLevel->getSoundTrack()) return mix;
  TSoundTrackFormat format = soundLevel->getSoundTrack()->getFormat();

  TXshSoundColumn *c = 0;
  int j;
  for (j = 0; j < size; ++j) {
    TXshSoundColumn *oldC = c;
    c                     = vect[j];
    if (j == 0) {
      mix = c->getOverallSoundTrack(fromFrame, toFrame, fps, format);
      TSoundTrackP mixedTrack =
          TSoundTrack::create(mix->getFormat(), mix->getSampleCount());
      mix = TSop::mix(mixedTrack, mix, 1.0, c->getVolume());
      continue;
    }
    assert(oldC);
    if (c->getVolume() == 0 || c->isEmpty()) {
      c = oldC;
      continue;
    }
    mix =
        TSop::mix(mix, c->getOverallSoundTrack(fromFrame, toFrame, fps, format),
                  1.0, c->getVolume());
  }

  // Per ora perche mov vuole solo 16 bit
  TSoundTrackFormat fmt                            = mix->getFormat();
  if (fmt.m_bitPerSample != 16) fmt.m_bitPerSample = 16;
  mix                                              = TSop::convert(mix, fmt);
  return mix;
}

PERSIST_IDENTIFIER(TXshSoundColumn, "soundColumn")
