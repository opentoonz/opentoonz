#pragma once

#ifndef HISTOGRAMPOPUP_H
#define HISTOGRAMPOPUP_H

#include "toonzqt/dvdialog.h"
#include "timage.h"
#include "tpixel.h"

class ComboHistogram;

//=============================================================================
// HistogramPopup
//-----------------------------------------------------------------------------

class HistogramPopup : public QDialog {
  Q_OBJECT

protected:
  ComboHistogram *m_histogram;

public:
  HistogramPopup(QString title = "");

  void setTitle(QString title);

  void setImage(TImageP image);

  void updateInfo(const TPixel32 &pix, const TPointD &imagePos);
  void updateAverageColor(const TPixel32 &pix);
};

//=============================================================================
// ViewerHistogramPopup
//-----------------------------------------------------------------------------

class ViewerHistogramPopup : public HistogramPopup {
  Q_OBJECT

public:
  ViewerHistogramPopup();

protected:
  void showEvent(QShowEvent *e);
  void hideEvent(QHideEvent *e);

protected slots:
  void setCurrentRaster();
};

#endif  // HISTOGRAMSPOPUP_H
