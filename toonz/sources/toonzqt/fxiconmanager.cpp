#include "toonzqt/fxiconmanager.h"

#include <QPixmap>

FxIconPixmapManager::FxIconPixmapManager() {}

FxIconPixmapManager *FxIconPixmapManager::instance() {
  static FxIconPixmapManager _instance;
  return &_instance;
}

const QPixmap &FxIconPixmapManager::getFxIconPm(std::string type) {
  std::map<std::string, QPixmap>::iterator it;
  it = m_pms.find(type);
  if (it != m_pms.end()) return it->second;

  int i;
  for (i = 0; fxTypeInfo[i].pixmapFilename; i++)
    if (strcmp(type.c_str(), fxTypeInfo[i].fxType) == 0) {
      QString path = QString(":Resources/fxicons/") +
                     fxTypeInfo[i].pixmapFilename + ".png";
      it = m_pms.insert(std::make_pair(type, QPixmap(path))).first;
      return it->second;
    }

  static const QPixmap unidentifiedFxPixmap(
      ":Resources/fxicons/fx_unidentified.png");
  it = m_pms.insert(std::make_pair(type, unidentifiedFxPixmap)).first;
  return it->second;
}
