#pragma once

#ifndef VECTORGUIDEDDRAWINGPANE_H
#define VECTORGUIDEDDRAWINGPANE_H

#include "toonzqt/dvdialog.h"

#include <QToolBar>

class QComboBox;
class QCheckBox;
class QPushButton;

//=============================================================================
// VectorGuidedDrawingPan
//-----------------------------------------------------------------------------

class VectorGuidedDrawingPane final : public QFrame {
  Q_OBJECT

  QComboBox *m_guidedTypeCB, *m_interpolationTypeCB;
  QCheckBox *m_autoInbetween;
  QPushButton *m_selectPrevGuideBtn, *m_selectNextGuideBtn,
      *m_selectBothGuideBtn, *m_resetGuidesBtn, *m_tweenSelectedGuidesBtn,
      *m_tweenToSelectedStrokeBtn, *m_SelectAndTweenBtn,
      *m_FlipNextDirectionBtn, *m_FlipPrevDirectionBtn;

public:
#if QT_VERSION >= 0x050500
  VectorGuidedDrawingPane(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  VectorGuidedDrawingPane(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#endif
  ~VectorGuidedDrawingPane(){};

  void updateStatus();
protected slots:
  void onGuidedTypeChanged();
  void onAutoInbetweenChanged();
  void onInterpolationTypeChanged();
  void onPreferenceChanged(const QString &);
};

#endif  // VECTORGUIDEDDRAWINGPANE_H
