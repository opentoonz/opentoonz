#pragma once

#ifndef SPECTRUMFIELD_H
#define SPECTRUMFIELD_H

#include <QWidget>
#include "toonzqt/colorfield.h"
#include "tpixel.h"
#include "tspectrum.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration

//=============================================================================

namespace DVGui {

//=============================================================================
// SpectrumBar
//-----------------------------------------------------------------------------

class DVAPI SpectrumBar final : public QWidget {
  Q_OBJECT

  int m_x0;
  int m_currentKeyIndex;

  QPixmap m_chessBg;

  TSpectrum m_spectrum;

public:
  SpectrumBar(QWidget *parent = 0, TPixel32 color = TPixel32(0, 0, 0, 255));

  ~SpectrumBar();

  int getCurrentKeyIndex() { return m_currentKeyIndex; }
  void setCurrentKeyIndex(int index);

  int getCurrentPos();
  TPixel32 getCurrentColor();

  TSpectrum &getSpectrum() { return m_spectrum; }
  void setSpectrum(TSpectrum &spectrum) {
    m_spectrum = spectrum;
    /*-- Undoの場合、Spectrumの差し替えによってIndexがあふれてしまうことがある
     * --*/
    if (m_currentKeyIndex >= m_spectrum.getKeyCount())
      setCurrentKeyIndex(getMaxPosKeyIndex());
    update();
  }

public slots:
  void setCurrentPos(int pos, bool isDragging);
  void setCurrentColor(const TPixel32 &color);
  void addKeyAt(int pos);

signals:
  void currentPosChanged(bool isDragging);
  void currentKeyChanged();
  void currentKeyAdded(int);
  void currentKeyRemoved(int);

protected:
  double posToSpectrumValue(int pos);
  int spectrumValueToPos(double spectrumValue);

  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;

  void mouseReleaseEvent(QMouseEvent *e) override;

  int getMaxPosKeyIndex();
  int getMinPosKeyIndex();
  int getNearPosKeyIndex(int pos);
};

//=============================================================================
// SpectrumField
//-----------------------------------------------------------------------------

class DVAPI SpectrumField final : public QWidget {
  Q_OBJECT

  int m_margin;
  int m_spacing;

  ColorField *m_colorField;
  SpectrumBar *m_spectrumbar;

public:
  SpectrumField(QWidget *parent = 0, TPixel32 color = TPixel32(0, 0, 0, 255));

  ~SpectrumField();

  TSpectrum &getSpectrum() { return m_spectrumbar->getSpectrum(); }
  void setSpectrum(TSpectrum &spectrum) {
    m_spectrumbar->setSpectrum(spectrum);
    m_colorField->setColor(m_spectrumbar->getCurrentColor());
  }

  int getCurrentKeyIndex() { return m_spectrumbar->getCurrentKeyIndex(); }
  void setCurrentKeyIndex(int index) {
    m_spectrumbar->setCurrentKeyIndex(index);
    m_colorField->setColor(m_spectrumbar->getCurrentColor());
  }

protected slots:
  void onCurrentPosChanged(bool isDragging);
  void onCurrentKeyChanged();
  void onColorChanged(const TPixel32 &color, bool isDragging);

protected:
  void paintEvent(QPaintEvent *e) override;

signals:
  void keyColorChanged(bool);
  void keyPositionChanged(bool);
  void keyAdded(int);
  void keyRemoved(int);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // SPECTRUMFIELD_H
