#pragma once

#ifndef TFFXHANDLE_H
#define TFFXHANDLE_H

#include <QObject>
#include <QString>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFx;

//=============================================================================
// TFxHandle
//-----------------------------------------------------------------------------

class DVAPI TFxHandle final : public QObject {
  Q_OBJECT

private:
  TFx *m_fx;
  // for reperoducing the last fx-creation command in the Schematic
  QString m_previousActionString;

public:
  TFxHandle();
  ~TFxHandle();

  TFx *getFx() const { return m_fx; }

  // do not switch fx settings when single-clicking the fx node in the schematic
  void setFx(TFx *fx, bool doSwitchFxSettings = true);

  void notifyFxChanged() { emit fxChanged(); }
  void notifyFxPresetSaved() { emit fxPresetSaved(); }
  void notifyFxPresetRemoved() { emit fxPresetRemoved(); }

  void onFxNodeDoubleClicked() { emit fxSettingsShouldBeSwitched(); }

  void setPreviousActionString(QString str) { m_previousActionString = str; }
  QString getPreviousActionString() { return m_previousActionString; }

public slots:
  void onColumnChanged();

signals:
  void fxSwitched();
  void fxChanged();
  void fxPresetSaved();
  void fxPresetRemoved();

  void fxSettingsShouldBeSwitched();
};

#endif  // TFRAMEHANDLE_H
