#pragma once

#ifndef TONIONSKINMASKHANDLE_H
#define TONIONSKINMASKHANDLE_H

#include <QObject>
#include "toonz/onionskinmask.h"

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
class OnionSkinMask;

//=============================================================================
// TOnionSkinMaskHandle
//-----------------------------------------------------------------------------

class DVAPI TOnionSkinMaskHandle final : public QObject {
  Q_OBJECT

  OnionSkinMask m_onionSkinMask;

public:
  TOnionSkinMaskHandle();
  ~TOnionSkinMaskHandle();

  const OnionSkinMask &getOnionSkinMask() const;
  void setOnionSkinMask(const OnionSkinMask &onionSkinMask);

  void notifyOnionSkinMaskChanged() { emit onionSkinMaskChanged(); }
  void clear();

signals:
  void onionSkinMaskChanged();
  void onionSkinMaskSwitched();
};

#endif  // TONIONSKINMASKHANDLE_H
