#pragma once

#ifndef TFRAMEHANDLE_H
#define TFRAMEHANDLE_H

#include <QObject>
#include <QTime>
#include "tfilepath.h"
#include "toonz/txshsoundcolumn.h"

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TXshSoundColumn;
class TXsheet;

//=============================================================================
// TFrameHandle
//-----------------------------------------------------------------------------

class DVAPI TFrameHandle final : public QObject {
  Q_OBJECT

public:
  enum FrameType { SceneFrame, LevelFrame };

private:
  int m_frame;     // valid if m_frameType == SceneFrame
  TFrameId m_fid;  // valid if m_frameType == LevelFrame

  int m_sceneFrameSize, m_frame0, m_frame1;

  std::vector<TFrameId> m_fids;

  int m_timerId;
  int m_previewFrameRate;
  FrameType m_frameType;
  bool m_looping, m_isPlaying;

  // audio scrub
  TXshSoundColumn *m_audioColumn;
  TXsheet *m_xsheet;
  std::pair<int, int> m_scrubRange;
  double m_fps;
  QTime m_clock;

  // void startPlaying(bool looping);
  // void stopPlaying();
  void setTimer(int frameRate);

  bool scrub(int r0, int r1, double framePerSecond);

public:
  TFrameHandle();
  ~TFrameHandle();

  void setCurrentFrame(int index);

  // if m_frameType == SceneFrame
  int getFrame() const;
  void setFrame(int frame);  // => m_frameType = SceneFrame

  // if m_frameType == LevelFrame
  TFrameId getFid() const;
  void setFid(const TFrameId &id);  // => m_frameType = LevelFrame

  bool isPlaying() const { return m_isPlaying; }
  bool isScrubbing() const { return m_scrubRange.first <= m_scrubRange.second; }

  FrameType getFrameType() const { return m_frameType; }
  // void setFrameType(FrameType frameType);

  void scrubColumn(int r0, int r1, TXshSoundColumn *audioColumn,
                   double framePerSecond = 25);
  void scrubXsheet(int r0, int r1, TXsheet *xsh, double framePerSecond = 25);
  void stopScrubbing();

  bool isEditingLevel() const { return getFrameType() == LevelFrame; }
  bool isEditingScene() const { return getFrameType() == SceneFrame; }

  // i  metodi seguenti funzionano sia con LevelFrame sia con SceneFrame
  int getMaxFrameIndex() const;
  int getFrameIndex() const;
  QString getFrameIndexName(int index) const;
  void setFrameIndex(int index);
  void setFrameIndexByName(const QString &str);

  void setSceneFrameSize(int frameSize) { m_sceneFrameSize = frameSize; }
  void setFrameRange(int f0, int f1) {
    m_frame0 = f0;
    m_frame1 = f1;
  }
  void setFrameIds(const std::vector<TFrameId> &fids) { m_fids = fids; }
  int getStartFrame() { return m_frame0; }
  int getEndFrame() { return m_frame1; }

public slots:

  void nextFrame(TFrameId = 0);
  void prevFrame();
  void firstFrame();
  void lastFrame();
  void setPlaying(bool isPlaying);

signals:
  void frameSwitched();
  void scrubStarted();
  void scrubStopped();
  void frameTypeChanged();
  void isPlayingStatusChanged();

protected:
  void timerEvent(QTimerEvent *event) override;
};

#endif  // TFRAMEHANDLE_H
