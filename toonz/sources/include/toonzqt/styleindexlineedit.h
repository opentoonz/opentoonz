#pragma once

#ifndef STYLEINDEXLINEEDIT_H
#define STYLEINDEXLINEEDIT_H

#include "tcommon.h"

#include "toonzqt/lineedit.h"

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
class TPaletteHandle;

//=============================================================================

namespace DVGui {

class DVAPI StyleIndexLineEdit : public LineEdit {
  Q_OBJECT

  TPaletteHandle *m_pltHandle;

public:
  StyleIndexLineEdit();
  ~StyleIndexLineEdit();

  void setPaletteHandle(TPaletteHandle *pltHandle) { m_pltHandle = pltHandle; }
  TPaletteHandle *getPaletteHandle() { return m_pltHandle; }

protected:
  void paintEvent(QPaintEvent *pe) override;
};
}  // namspace

#endif
