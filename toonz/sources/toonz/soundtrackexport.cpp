
#include "soundtrackexport.h"

#include "menubarcommandids.h"
#include "tapp.h"
#include "tsop.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"

// TnzCore includes
#include "tsystem.h"
#include "toutputproperties.h"
#include "filebrowserpopup.h"
#include "tsound_io.h"

//-----------------------------------------------------------------------------

SoundtrackExport::SoundtrackExport() {
  m_scene = TApp::instance()->getCurrentScene()->getScene();
}

//-----------------------------------------------------------------------------

bool SoundtrackExport::hasSoundTrack() {
  TXsheetHandle *xsheetHandle    = TApp::instance()->getCurrentXsheet();
  TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
  m_sound                        = xsheetHandle->getXsheet()->makeSound(prop);
  if (m_sound == NULL) {
    m_hasSoundtrack = false;
    return false;
  } else {
    m_hasSoundtrack = true;
    return true;
  }
}

void SoundtrackExport::makeSoundtrack(int r0, int r1, double fps) {
  TCG_ASSERT(r0 <= r1, return );
  long m_whiteSample;
  double samplePerFrame = m_sound->getSampleRate() / fps;

  // Extract the useful part of scene soundtrack
  TSoundTrackP snd1 = m_sound->extract((TINT32)(r0 * samplePerFrame),
                                       (TINT32)(r1 * samplePerFrame));

  assert(!m_st);
  if (!m_st) {
    m_whiteSample = 0;
    // First, add white sound before the 'from' instant
    m_st          = TSoundTrack::create(snd1->getFormat(), m_whiteSample);
    m_whiteSample = 0;
  }

  // Then, add the rest
  TINT32 fromSample = m_st->getSampleCount();
  TINT32 numSample =
      std::max(TINT32((r1 - r0 + 1) * samplePerFrame), snd1->getSampleCount());

  m_st = TSop::insertBlank(m_st, fromSample, numSample + m_whiteSample);
  m_st->copy(snd1, TINT32(fromSample + m_whiteSample));

  m_whiteSample = 0;
}

void SoundtrackExport::saveSoundtrack() {
  if (!m_st) {
    GenericSaveFilePopup *m_saveShortcutsPopup =
        new GenericSaveFilePopup("Export Soundtrack");
    m_saveShortcutsPopup->addFilterType("wav");
    TFilePath fp = m_saveShortcutsPopup->getPath();
    if (fp == TFilePath()) return;

    TSceneProperties *sprop = m_scene->getProperties();

    int step;
    int from, to;
    sprop->getOutputProperties()->getRange(from, to, step);
    if (to < 0) {
      TXsheet *xs = m_scene->getXsheet();

      // NOTE: Use of numeric_limits::min is justified since the type is
      // *INTERGRAL*.
      from = (std::numeric_limits<int>::max)(),
      to   = (std::numeric_limits<int>::min)();

      for (int k = 0; k < xs->getColumnCount(); ++k) {
        int r0, r1;
        xs->getCellRange(k, r0, r1);

        from = std::min(from, r0), to = std::max(to, r1);
      }
    }
    makeSoundtrack(
        from, to,
        m_scene->getProperties()->getOutputProperties()->getFrameRate());
    TSoundTrackWriter::save(fp, m_st);
  }
}

//-----------------------------------------------------------------------------

class ExportSoundtrackCommandHandler final : public MenuItemHandler {
public:
  ExportSoundtrackCommandHandler() : MenuItemHandler(MI_SoundTrack) {}
  void execute() override {
    SoundtrackExport soundSettings;
    if (soundSettings.hasSoundTrack())
      soundSettings.saveSoundtrack();
    else {
      DVGui::warning("There is no sound to be exported.");
    }
  }
} ExportSoundtrackCommandHandler;
