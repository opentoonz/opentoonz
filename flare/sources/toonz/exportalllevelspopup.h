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
  bool m_isExportAll;  // set to false when "Export All" is unchecked
  std::map<std::wstring ,std::wstring> level_to_foldername;
  std::wstring backFolderName(std::string colname, std::wstring levelname);
  bool isAllLevelsExported();

  // Widgets
private:
  DVGui::CheckBox *m_exportAll;
  QPushButton *m_skipButton;

private slots:
  void collectSelectedSimpleLevels() override;
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *he) override;
  void updateOnSelection() override;
  void onExportAll(bool toggled);
  void skip();

  QString getLevelTypeName(int type);
};

#endif  // EXPORTALLLEVELSPOPUP_H