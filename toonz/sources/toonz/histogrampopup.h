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
  void updateInfo(const TPixel64 &pix, const TPointD &imagePos);
  void updateInfo(const TPixelF &pix, const TPointD &imagePos);
  void updateAverageColor(const TPixel32 &pix);
  void updateAverageColor(const TPixel64 &pix);
  void updateAverageColor(const TPixelF &pix);
  void setShowCompare(bool on);
  void invalidateCompHisto();
  void moveNextToWidget(QWidget *widget);
};

//=============================================================================
// ViewerHistogramPopup
//-----------------------------------------------------------------------------

class ViewerHistogramPopup final : public HistogramPopup {
  Q_OBJECT

public:
  ViewerHistogramPopup();

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

protected slots:
  void setCurrentRaster();
};

#endif  // HISTOGRAMSPOPUP_H
