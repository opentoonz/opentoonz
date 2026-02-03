#pragma once

#ifndef BINARIZEPOPUP_H
#define BINARIZEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include "traster.h"

class QSlider;
class ImageViewer;
class TSelection;
class QProgressDialog;

namespace DVGui {
class IntField;
class CheckBox;
}

//=============================================================================
// BinarizePopup
//-----------------------------------------------------------------------------

class BinarizePopup final : public DVGui::Dialog {
  Q_OBJECT

protected:
  // DVGui::IntField* m_brightnessField;
  // DVGui::IntField* m_contrastField;
  DVGui::CheckBox *m_previewChk, *m_alphaChk;
  QPushButton *m_okBtn;
  TRaster32P m_inRas, m_outRas;

  class Swatch;
  Swatch *m_viewer;

public:
  BinarizePopup();
  void setSample(const TRasterP &ras);

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

private:
  typedef std::vector<std::pair<TXshSimpleLevel *, TFrameId>> Frames;
  Frames m_frames;
  int m_frameIndex;
  QProgressBar *m_progressBar;
  int m_timerId;

  int getSelectedFrames();
  void computePreview();

protected slots:
  void onPreviewCheckboxChanged(int);
  void onAlphaCheckboxChanged(int);
  void onLevelSwitched(TXshLevel *oldLevel) { fetchSample(); }
  void fetchSample();
  void apply();
};

#endif  // BINARIZEPOPUP_H
