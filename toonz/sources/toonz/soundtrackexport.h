#pragma once

#ifndef SOUNDTRACKEXPORT_H
#define SOUNDTRACKEXPORT_H

#include "toonzqt/dvdialog.h"
#include "toonz/sceneproperties.h"
#include "tsound.h"

// forward declaration
class ToonzScene;

//=============================================================================
// SoundtrackExport
//-----------------------------------------------------------------------------

class SoundtrackExport : public QWidget {
  Q_OBJECT

public:
  SoundtrackExport();
  bool hasSoundTrack();
  void makeSoundtrack(int r0, int r1, double fps);
  void saveSoundtrack();

protected:
  bool m_hasSoundtrack;
  TSoundTrackP m_st;
  TSoundTrack *m_sound;
  ToonzScene *m_scene;
  TFilePath m_fp;
};

#endif  // SOUNDTRACKEXPORT_H
