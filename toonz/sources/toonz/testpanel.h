#pragma once

#ifndef TESTPANEL_H
#define TESTPANEL_H

#include "pane.h"
#include "tpixel.h"

//=============================================================================
// TestPanel

class TestPanel final : public TPanel {
  Q_OBJECT

public:
#if QT_VERSION >= 0x050500
  TestPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  TestPanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~TestPanel();

public slots:
  void onDoubleValuesChanged(bool isDragging);
  void onDoubleValueChanged(bool isDragging);
  void onIntValueChanged(bool isDragging);
  void onColorValueChanged(const TPixel32 &, bool isDragging);
};

#endif  // TESTPANEL_H
