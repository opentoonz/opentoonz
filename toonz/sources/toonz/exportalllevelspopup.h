#pragma once

#ifndef EXPORTALLLEVELSPOPUP_H
#define EXPORTALLLEVELSPOPUP_H

#include "exportlevelpopup.h"

class ExportAllLevelsPopup final : public ExportLevelPopup {
  Q_OBJECT

public:
  ExportAllLevelsPopup();
  ~ExportAllLevelsPopup();
  bool execute() override;  // overrride FileBrowserPopup::execute()

private:
  bool isexport_all;  // set to false when "Export All" be dis-checked
  int level_exported = 0;
  ToonzScene *scene;
  std::map<std::wstring ,std::wstring> level_to_foldername;
  std::wstring backfoldername(std::string colname, std::wstring levelname);

  // Widgets
private:
  DVGui::CheckBox *m_exportall;
  QPushButton *m_skipButton;

private slots:
  void GetSelectedSimpLevels() override;
  void showEvent(QShowEvent *se) override;
  void updateOnSelection() override;
  void onExportAll(bool toggled);
  void skip();

  QString getLevelTypeName(int type);
};

#endif  // EXPORTALLLEVELSPOPUP_H