#pragma once

#ifndef TXSHEETHANDLE_H
#define TXSHEETHANDLE_H

#include <QObject>

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

// forward declaration
class TXsheet;

//=============================================================================
// TXsheetHandle
//-----------------------------------------------------------------------------

class DVAPI TXsheetHandle final : public QObject {
  Q_OBJECT

  TXsheet *m_xsheet;

public:
  TXsheetHandle();
  ~TXsheetHandle();

  TXsheet *getXsheet() const;
  void setXsheet(TXsheet *xsheet);
  void notifyXsheetChanged() { emit xsheetChanged(); }
  void notifyXsheetSwitched() { emit xsheetSwitched(); }
  void notifyXsheetSoundChanged() { emit xsheetSoundChanged(); }
  void changeXsheetCamera(int index) { emit xsheetCameraChange(index); }

signals:
  void xsheetSwitched();
  void xsheetChanged();
  void xsheetSoundChanged();
  void xsheetCameraChange(int);
};

#endif  // TXSHEETHANDLE_H
