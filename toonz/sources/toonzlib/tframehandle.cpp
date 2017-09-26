

#include "toonz/tframehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"

#include "toonz/txsheethandle.h"

//=============================================================================

namespace {

/* int getCurrentSceneFrameCount()
  {
    return 100; //
  TApp::instance()->getCurrentScene()->getScene()->getFrameCount();
  }*/

/*void getCurrentScenePlayRange(int &r0, int &r1, int &step)
  {
    /*
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    scene->getProperties()->getPreviewProperties()->getRange(r0, r1, step);
    if(r0>r1)
    {
      r0 = 0;
      r1 = scene->getFrameCount()-1;
    }
    */
/*   r0 = 0;
    r1 = getCurrentSceneFrameCount()-1;
  }*/

bool getCurrentLevelFids(std::vector<TFrameId> &fids) {
  /*
TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
if(!sl) return false;
sl->getFids(fids);
*/
  fids.push_back(TFrameId(1));
  return !fids.empty();
}

TFrameId getLastLevelFid() { return TFrameId(1); }

}  // namespace

//=============================================================================
// TFrameHandle
//-----------------------------------------------------------------------------

TFrameHandle::TFrameHandle()
    : m_frame(-1)
    , m_fid()
    , m_timerId(0)
    , m_previewFrameRate(25)
    , m_frameType(SceneFrame)
    , m_looping(false)
    , m_isPlaying(false)
    , m_scrubRange(0, -1)
    , m_audioColumn(0)
    , m_xsheet(0)
    , m_fps(0)
    , m_frame0(-1)
    , m_frame1(-1) {}

//-----------------------------------------------------------------------------

TFrameHandle::~TFrameHandle() {}

//-----------------------------------------------------------------------------

int TFrameHandle::getFrame() const { return m_frame; }

//-----------------------------------------------------------------------------

void TFrameHandle::setCurrentFrame(int frame) {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    // if(!getCurrentLevelFids(fids)) return;

    if (m_fids.size() <= 0) return;
    if (frame - 1 >= (int)m_fids.size())
      setFid(m_fids.back());
    else
      setFid(m_fids[frame - 1]);
  } else
    setFrame(frame - 1);
}

//-----------------------------------------------------------------------------

void TFrameHandle::setFrame(int frame) {
  if (m_frame == frame && m_frameType == SceneFrame) return;
  m_frame = frame;
  if (m_frameType != SceneFrame) {
    m_frameType = SceneFrame;
    emit frameTypeChanged();
  }
  emit frameSwitched();
}

//-----------------------------------------------------------------------------

TFrameId TFrameHandle::getFid() const { return m_fid; }

//-----------------------------------------------------------------------------

void TFrameHandle::setFid(const TFrameId &fid) {
  if (m_fid == fid && m_frameType == LevelFrame) return;
  m_fid = fid;
  if (m_frameType != LevelFrame) {
    m_frameType = LevelFrame;
    emit frameTypeChanged();
  }
  emit frameSwitched();
}

//-----------------------------------------------------------------------------

void TFrameHandle::nextFrame() {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    // if(!getCurrentLevelFids(fids)) return;
    if (m_fids.size() <= 0) return;
    std::vector<TFrameId>::iterator it;
    it = std::upper_bound(m_fids.begin(), m_fids.end(), m_fid);
    if (it == m_fids.end()) {
      // non c'e' nessun frame del livello oltre m_fid. Non vado oltre al primo
      // frame dopo l'ultimo.
      // TXshSimpleLevel *sl =
      // TApp::instance()->getCurrentLevel()->getSimpleLevel();
      TFrameId fid = m_fids.back();  // sl->index2fid(sl->getFrameCount());
      setFid(fid);
    } else
      setFid(*it);
  } else {
    setFrame(m_frame + 1);
  }
}

//-----------------------------------------------------------------------------

void TFrameHandle::prevFrame() {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    if (m_fids.size() <= 0) return;
    std::vector<TFrameId>::iterator it;
    it = std::lower_bound(m_fids.begin(), m_fids.end(), m_fid);
    // tornando indietro non vado prima del primo frame del livello
    if (it != m_fids.end() && it != m_fids.begin()) {
      --it;
      setFid(*it);
    } else {
      // se sono dopo l'ultimo, vado all'ultimo
      if (!m_fids.empty() && m_fid > m_fids.back()) setFid(m_fids.back());
    }
  } else {
    if (m_frame > 0) setFrame(m_frame - 1);
  }
}

//-----------------------------------------------------------------------------

void TFrameHandle::firstFrame() {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    if (m_fids.size() <= 0) return;
    setFid(m_fids.front());
  } else {
    /*int r0,r1,step;
getCurrentScenePlayRange(r0,r1,step);*/
    setFrame(m_frame0);
  }
}

//-----------------------------------------------------------------------------

void TFrameHandle::lastFrame() {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    if (m_fids.size() <= 0) return;
    setFid(m_fids.back());
  } else {
    /*int r0,r1,step;
getCurrentScenePlayRange(r0,r1,step);*/
    if (m_frame1 > 0) setFrame(m_frame1);
  }
}

//-----------------------------------------------------------------------------

void TFrameHandle::setPlaying(bool isPlaying) {
  if (m_isPlaying == isPlaying) return;
  m_isPlaying = isPlaying;
  emit isPlayingStatusChanged();
}

//-----------------------------------------------------------------------------
#ifdef LEVO

void TFrameHandle::startPlaying(bool looping) {
  if (m_previewFrameRate == 0) return;
  if (isScrubbing()) stopScrubbing();
  setTimer(m_previewFrameRate);
  m_looping = looping;
  emit playStarted();
}

//-----------------------------------------------------------------------------

void TFrameHandle::stopPlaying() {
  if (m_timerId != 0) killTimer(m_timerId);
  m_timerId = 0;
  m_looping = false;
  emit playStopped();
}

//-----------------------------------------------------------------------------
#endif

void TFrameHandle::setTimer(int frameRate) {
  m_previewFrameRate = frameRate;
  if (m_timerId != 0) killTimer(m_timerId);
  int interval = troundp(1000.0 / double(m_previewFrameRate));
  m_timerId    = startTimer(interval);
}

//-----------------------------------------------------------------------------

void TFrameHandle::timerEvent(QTimerEvent *event) {
  assert(isScrubbing());

  int elapsedTime = m_clock.elapsed();
  int frame       = m_scrubRange.first + elapsedTime * m_fps / 1000;
  int lastFrame   = m_scrubRange.second;
  if (frame >= lastFrame) {
    if (m_frame != lastFrame) setFrame(lastFrame);
    stopScrubbing();
  } else
    setFrame(frame);
}

//-----------------------------------------------------------------------------

int TFrameHandle::getMaxFrameIndex() const {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    if (m_fids.size() <= 0)
      return -1;
    else
      return m_fids.size() - 1;
  } else
    // return getCurrentSceneFrameCount()-1;
    return m_sceneFrameSize - 1;
}

//-----------------------------------------------------------------------------

int TFrameHandle::getFrameIndex() const {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    if (m_fids.size() > 0) {
      std::vector<TFrameId>::const_iterator it =
          std::find(m_fids.begin(), m_fids.end(), m_fid);
      if (it != m_fids.end())
        return std::distance(m_fids.begin(), it);
      else {
        if (m_fid > m_fids.back())
          return m_fids.size();
        else
          return -1;
      }
    } else
      return -1;
  } else
    return m_frame;
}

//-----------------------------------------------------------------------------

QString TFrameHandle::getFrameIndexName(int index) const {
  if (m_frameType == LevelFrame) {
    if (m_fid.getNumber() <= 0)
      return "";
    else {
      return QString::number(m_fid.getNumber());
    }
  } else
    return QString::number(m_frame + 1);
}

void TFrameHandle::setFrameIndex(int index) {
  if (m_frameType == LevelFrame) {
    // std::vector<TFrameId> fids;
    if (m_fids.size() > 0 && 0 <= index && index < (int)m_fids.size())
      setFid(m_fids[index]);
  } else
    setFrame(index);
}

void TFrameHandle::setFrameIndexByName(const QString &str) {
  int num = str.toInt();
  if (m_frameType == LevelFrame) {
    setFid(TFrameId(num));
  } else
    setFrame(num - 1);
}

void TFrameHandle::scrubColumn(int r0, int r1, TXshSoundColumn *audioColumn,
                               double framePerSecond) {
  m_audioColumn = audioColumn;
  if (!scrub(r0, r1, framePerSecond)) m_audioColumn = 0;
}

void TFrameHandle::scrubXsheet(int r0, int r1, TXsheet *xsheet,
                               double framePerSecond) {
  m_xsheet = xsheet;
  if (!scrub(r0, r1, framePerSecond)) m_xsheet = 0;
}

bool TFrameHandle::scrub(int r0, int r1, double framePerSecond) {
  if (isPlaying() || isScrubbing()) return false;
  bool onlyOneFrame = (r0 == r1);

  if (!isScrubbing() || !onlyOneFrame) emit scrubStarted();
  if (!onlyOneFrame) {
    m_fps        = framePerSecond;
    m_scrubRange = std::make_pair(r0, r1);
  }
  setFrame(r0);

  if (m_audioColumn)
    m_audioColumn->scrub(r0, r1);
  else if (m_xsheet) {
    int i;
    for (i = r0; i <= r1; i++) m_xsheet->scrub(i, true);
  }

  if (onlyOneFrame) return false;

  m_clock.start();
  m_timerId = startTimer(40);
  return true;
}

void TFrameHandle::stopScrubbing() {
  if (!isScrubbing()) return;
  if (m_timerId > 0) killTimer(m_timerId);
  m_timerId                        = 0;
  m_scrubRange                     = std::make_pair(0, -1);
  if (m_audioColumn) m_audioColumn = 0;
  if (m_xsheet) m_xsheet           = 0;
  m_fps                            = 0;
  emit scrubStopped();
}
