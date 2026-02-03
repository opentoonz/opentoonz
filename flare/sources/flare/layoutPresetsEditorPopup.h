#pragma once
#ifndef LayoutPresetDIALOG_H
#define LayoutPresetDIALOG_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include <QList>
#include <QStringList>
#include <QVariant>
#include <QGroupBox>
#include <QtGlobal>
#include <algorithm>
#include "viewerdraw.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;

namespace DVGui {
class FileField;
class DoubleLineEdit;
class ColorField;
}  // namespace DVGui

using namespace ViewerDraw;

class LayoutPresetsEditorPopup : public DVGui::Dialog {
  Q_OBJECT

private:
  static LayoutPresetsEditorPopup* s_instance;
  explicit LayoutPresetsEditorPopup(QWidget* parent = nullptr);

public:
  static LayoutPresetsEditorPopup* instance(QWidget* parent = nullptr);
  void showAndPosition(const QPoint& globalPos);

protected:
  void accept() override;
  void reject() override;
  void closeEvent(QCloseEvent* e) override;

private:
  TFilePath m_iniFilePath;
  QList<LayoutPreset> m_presets;

  QString m_oldLayoutName;
  bool m_previewActive = false;
  bool m_needSaveIni   = false;

  // UI
  QListWidget* m_presetList;
  QGroupBox* m_editorGroup;
  QLineEdit* m_nameEdit;
  DVGui::FileField* m_layoutField;
  DVGui::DoubleLineEdit* m_offsetXEdit;
  DVGui::DoubleLineEdit* m_offsetYEdit;
  QListWidget* m_areaList;

  QGroupBox* m_areaEditorGroup;
  DVGui::DoubleLineEdit* m_widthEdit;
  DVGui::DoubleLineEdit* m_heightEdit;
  DVGui::ColorField* m_colorField;

  QPushButton* m_addAreaBtn;
  QPushButton* m_removeAreaBtn;

  void loadIniFile();
  void saveIniFile();
  void updatePresetList();
  void updateAreaList();
  void updateEditorWidgets();

  QString getCurrentLayoutPresetName() const;
  void setSceneLayoutPresetName(const QString& name);
  void triggerPreview();

private slots:
  void onPresetSelected();
  void onAreaSelected();

  void onAddPreset();
  void onRemovePreset();

  void onMovePresetUp();
  void onMovePresetDown();

  void onAddArea();
  void onRemoveArea();

  void onNameChanged(const QString& text);
  void onLayoutPathChanged();

  void onOffsetXChanged();
  void onOffsetYChanged();

  // Width/Height/RGB changed
  void onAreaParamChanged();
};

#endif  // LayoutPresetDIALOG_H