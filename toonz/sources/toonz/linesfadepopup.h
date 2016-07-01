#pragma once

#ifndef LINESFADEPOPUP_H
#define LINESFADEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include "traster.h"

class QSlider;
class ImageViewer;
class TSelection;

namespace DVGui {
class ColorField;
class IntField;
}

//=============================================================================
// LinesFadePopup
//-----------------------------------------------------------------------------

class LinesFadePopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::ColorField *m_linesColorField;
  DVGui::IntField *m_intensity;
  QPushButton *m_okBtn;
  TRaster32P m_startRas;

private:
  class Swatch;
  Swatch *m_viewer;

public:
  LinesFadePopup();

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

protected slots:

  void setCurrentSampleRaster();

public slots:

  void apply();
  void onLinesColorChanged(const TPixel32 &, bool);
  void onIntensityChanged(bool isDragging);
};

#endif  // LINESFADEPOPUP_H
