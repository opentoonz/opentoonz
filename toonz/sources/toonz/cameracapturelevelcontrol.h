#pragma once

#ifndef CAMERA_CAPTURE_LEVEL_CONTROL_H
#define CAMERA_CAPTURE_LEVEL_CONTROL_H

#include <QFrame>
#include <QVector>
#include "opencv2/opencv.hpp"

namespace DVGui {
class IntLineEdit;
class DoubleLineEdit;
}  // namespace DVGui
//=============================================================================
// CameraCaptureLevelHistogram
//-----------------------------------------------------------------------------
class CameraCaptureLevelHistogram : public QFrame {
  Q_OBJECT
public:
  enum LevelControlItem {
    None = 0,
    BlackSlider,
    WhiteSlider,
    GammaSlider,
    ThresholdSlider,
    NumItems
  };
  enum LevelControlMode { Color_GrayScale, BlackAndWhite, NumModes };

private:
  QVector<int> m_histogramData;
  LevelControlItem m_currentItem;

  int m_black, m_white, m_threshold;
  float m_gamma;
  int m_offset;  // offset between the handle position and the clicked position

  LevelControlMode m_mode;

public:
  CameraCaptureLevelHistogram(QWidget* parent = 0);

  void updateHistogram(cv::Mat& image);

  int black() { return m_black; }
  int white() { return m_white; }
  float gamma() { return m_gamma; }
  int threshold() { return m_threshold; }
  LevelControlMode mode() { return m_mode; }
  void setValues(int black, int white, float gamma) {
    m_black = black;
    m_white = white;
    m_gamma = gamma;
  }
  void setThreshold(int thres) { m_threshold = thres; }
  void setMode(LevelControlMode mode) { m_mode = mode; }

protected:
  void paintEvent(QPaintEvent* event) override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  void leaveEvent(QEvent* event) override;

signals:
  void valueChange(int itemId);
};

//-----------------------------------------------------------------------------

class CameraCaptureLevelControl : public QFrame {
  Q_OBJECT

  CameraCaptureLevelHistogram* m_histogram;
  DVGui::IntLineEdit *m_blackFld, *m_whiteFld, *m_thresholdFld;
  DVGui::DoubleLineEdit* m_gammaFld;
  cv::Mat m_lut;

  void computeLut();

public:
  CameraCaptureLevelControl(QWidget* parent = 0);

  void updateHistogram(cv::Mat& image) { m_histogram->updateHistogram(image); }
  void setMode(bool color_grayscale);

  void adjustLevel(cv::Mat& image);
  void binarize(cv::Mat& image);

protected slots:
  void onHistogramValueChanged(int itemId);
  void onFieldChanged();
};

#endif