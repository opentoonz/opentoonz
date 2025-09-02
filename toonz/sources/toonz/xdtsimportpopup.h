#pragma once
#ifndef XDTSIMPORTPOPUP_H
#define XDTSIMPORTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"

#include <QMap>
namespace DVGui {
class FileField;
}
class ToonzScene;
class QComboBox;

class XDTSImportPopup : public DVGui::Dialog {
  Q_OBJECT
  QMap<QString, DVGui::FileField*> m_fields;
  QStringList m_pathSuggestedLevels;
  ToonzScene* m_scene;

  QComboBox *m_tick1Combo, *m_tick2Combo, *m_keyFrameCombo,
      *m_referenceFrameCombo;

  bool m_isUext;  // whether if the loading xdts is unofficial extension (UEXT)
                  // version

  void updateSuggestions(const QString samplePath);

  // Fallback Search
  void updateSuggestions(const TFilePath &path);

public:
  XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                  TFilePath scenePath, bool isUextVersion);
  QString getLevelPath(QString levelName);
  void getMarkerIds(int& tick1Id, int& tick2Id, int& keyFrameId,
                    int& referenceFrameId);
protected slots:
  void onPathChanged();
};

#endif