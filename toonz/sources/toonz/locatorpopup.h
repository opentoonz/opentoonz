#pragma once

#ifndef LOCATORPOPUP_H
#define LOCATORPOPUP_H

#include "tgeometry.h"
#include "toonzqt/dvdialog.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class SceneViewer;

//=============================================================================
// LoactorPopup
//-----------------------------------------------------------------------------

class LocatorPopup : public QFrame {
  Q_OBJECT
  SceneViewer* m_viewer;
  bool m_initialZoom;

public:
  LocatorPopup(QWidget* parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~LocatorPopup() {}

  SceneViewer* viewer() { return m_viewer; }

  void onChangeViewAff(const TPointD& curPos);

protected:
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:
  void changeWindowTitle();
};

#endif