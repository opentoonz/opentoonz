#pragma once

#ifndef TPALETTEHANDLE_H
#define TPALETTEHANDLE_H

// TnzCore includes
#include "tcommon.h"
#include "tpalette.h"

// Qt includes
#include <QObject>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// TPaletteHandle
//-----------------------------------------------------------------------------

class DVAPI TPaletteHandle final : public QObject {
  Q_OBJECT

  TPalette *m_palette;

  int m_styleIndex;
  int m_styleParamIndex;

public:
  TPaletteHandle();
  ~TPaletteHandle();

  TPalette *getPalette() const;

  int getStyleIndex() const;

  int getStyleParamIndex() const;

  TColorStyle *getStyle() const;

  void setPalette(TPalette *palette, int styleIndex = 1);

  void setStyleIndex(int index);

  void setStyleParamIndex(int index);

public:
  void notifyPaletteSwitched() { emit paletteSwitched(); }
  void notifyPaletteChanged();
  void notifyPaletteTitleChanged();
  void notifyColorStyleSwitched();
  // unchange the dirty flag when undo operation
  void notifyColorStyleChanged(bool onDragging   = true,
                               bool setDirtyFlag = true);

  void notifyPaletteDirtyFlagChanged() { emit paletteDirtyFlagChanged(); }
  void notifyPaletteLockChanged() { emit paletteLockChanged(); }

  void toggleAutopaint();

public:
signals:

  void paletteSwitched();
  void paletteChanged();
  void paletteTitleChanged();
  void colorStyleSwitched();
  void colorStyleChanged(bool);
  void colorStyleChangedOnMouseRelease();
  void paletteDirtyFlagChanged();
  void paletteLockChanged();

private:
  friend class PaletteController;

  bool connectBroadcasts(const QObject *receiver);
  bool disconnectBroadcasts(const QObject *receiver);

private:
signals:

  // Internal broadcaster signals to multiple palette handles.
  // Do not connect to user code.

  void broadcastPaletteChanged();
  void broadcastPaletteTitleChanged();
  void broadcastColorStyleSwitched();
  void broadcastColorStyleChanged(bool);
  void broadcastColorStyleChangedOnMouseRelease();
};

#endif  // TPALETTEHANDLE_H
