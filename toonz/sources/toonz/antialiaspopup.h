#pragma once

#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include "traster.h"

class QSlider;
class ImageViewer;
class TSelection;

namespace DVGui {
class IntField;
}

//=============================================================================
// AntialiasPopup
//-----------------------------------------------------------------------------

class AntialiasPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::IntField *m_thresholdField;
  DVGui::IntField *m_softnessField;
  QPushButton *m_okBtn;
  TRasterP m_startRas;

private:
  class Swatch;
  Swatch *m_viewer;

public:
  AntialiasPopup();

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

protected slots:

  void setCurrentSampleRaster();

public slots:

  void apply();
  void onValuesChanged(bool dragging);
};
