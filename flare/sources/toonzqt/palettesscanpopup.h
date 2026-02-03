#pragma once

#ifndef PALETTESCANPOPUP_H
#define PALETTESCANPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"

#include "toonz/studiopalette.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// PalettesScanPopup
//-----------------------------------------------------------------------------

class DVAPI PalettesScanPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::FileField *m_field;
  QLabel *m_label;

  TFilePath m_folderPath;
  int m_timerId;

  struct Directory {
    TFilePath m_path;
    TFilePathSet m_files;
    TFilePathSet::iterator m_it;
  };
  std::vector<Directory *> m_stack;

public:
  PalettesScanPopup();

  void setCurrentFolder(TFilePath path);
  TFilePath getCurrentFolder();

protected slots:
  void onOkBtnClicked();

protected:
  void setLabel(const TFilePath &fp);
  void timerEvent(QTimerEvent *event) override;
  void push(const TFilePath &fp);
  void push(const TFilePathSet &fs);
  void pop();
  bool step();
  void clearStack();
  void onPlt(const TFilePath &fp);
};

#endif  // PALETTESCANPOPUP_H
