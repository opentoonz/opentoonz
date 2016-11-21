#pragma once

#ifndef LOCATORPOPUP_H
#define LOCATORPOPUP_H

#include "tgeometry.h"
#include <QDialog>

class SceneViewer;

//=============================================================================
// LoactorPopup
//-----------------------------------------------------------------------------

class LocatorPopup : public QDialog {
  Q_OBJECT
  SceneViewer* m_viewer;
  bool m_initialZoom;

public:
  LocatorPopup(QWidget* parent = 0);
  SceneViewer* viewer() { return m_viewer; }

  void onChangeViewAff(const TPointD& curPos);

protected:
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:
  void changeWindowTitle();
};

#endif