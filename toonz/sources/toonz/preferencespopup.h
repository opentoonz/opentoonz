#pragma once

#ifndef PREFERENCESPOPUP_H
#define PREFERENCESPOPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/filefield.h"

// TnzLib includes
#include "toonz/preferences.h"

// Qt includes
#include <QComboBox>
#include <QFontComboBox>

//==============================================================

//    Forward declarations

class QLineEdit;
class QPushButton;
class QLabel;
class QGroupBox;

//==============================================================

//**********************************************************************************
//    PreferencesPopup  definition
//**********************************************************************************
typedef QPair<QString, QVariant> ComboBoxItem;

class SizeField final : public QWidget {
  Q_OBJECT
  DVGui::IntLineEdit *m_fieldX, *m_fieldY;

public:
  SizeField(QSize min, QSize max, QSize value = QSize(),
            QWidget* parent = nullptr);
  QSize getValue() const;
  void setValue(const QSize&);
signals:
  void editingFinished();
};

class PreferencesPopup;

typedef void (PreferencesPopup::*OnEditedPopupFunc)();

class PreferencesPopup final : public QDialog {
  Q_OBJECT

  QMap<QWidget*, PreferencesItemId> m_controlIdMap;
  QMap<PreferencesItemId, OnEditedPopupFunc> m_onEditedFuncMap;
  QMap<PreferencesItemId, OnEditedPopupFunc> m_preEditedFuncMap;

public:
  PreferencesPopup();

private:
  class FormatProperties;

private:
  Preferences* m_pref;
  FormatProperties* m_formatProperties;

  DVGui::CheckBox *m_projectRootDocuments, *m_projectRootDesktop,
      *m_projectRootCustom;
  QPushButton* m_editLevelFormat;
  QComboBox* m_levelFormatNames;

private:
  void rebuildFormatsList();
  QList<ComboBoxItem> buildFontStyleList() const;

  QWidget* createUI(
      PreferencesItemId id,
      const QList<ComboBoxItem>& comboItems = QList<ComboBoxItem>());
  QGridLayout* insertGroupBoxUI(PreferencesItemId id, QGridLayout* layout);
  void insertUI(PreferencesItemId id, QGridLayout* layout,
                const QList<ComboBoxItem>& comboItems = QList<ComboBoxItem>());
  void insertDualUIs(
      PreferencesItemId leftId, PreferencesItemId rightId, QGridLayout* layout,
      const QList<ComboBoxItem>& leftComboItems  = QList<ComboBoxItem>(),
      const QList<ComboBoxItem>& rightComboItems = QList<ComboBoxItem>());
  void insertFootNote(QGridLayout* layout);
  QString getUIString(PreferencesItemId id);
  QList<ComboBoxItem> getComboItemList(PreferencesItemId id) const;
  template <typename T>
  inline T getUI(PreferencesItemId id);

  QWidget* createGeneralPage();
  QWidget* createInterfacePage();
  QWidget* createVisualizationPage();
  QWidget* createLoadingPage();
  QWidget* createSavingPage();
  QWidget* createImportExportPage();
  QWidget* createDrawingPage();
  QWidget* createToolsPage();
  QWidget* createXsheetPage();
  QWidget* createAnimationPage();
  QWidget* createPreviewPage();
  QWidget* createOnionSkinPage();
  QWidget* createColorsPage();
  QWidget* createVersionControlPage();
  QWidget* createTouchTabletPage();

  //--- callbacks ---
  // General
  void onAutoSaveChanged();
  void onAutoSaveOptionsChanged();
  void onWatchFileSystemClicked();
  void onPathAliasPriorityChanged();
  // Interface
  void onStyleSheetTypeChanged();
  void onPixelsOnlyChanged();
  void onUnitChanged();
  void beforeRoomChoiceChanged();
  // Drawing
  void onDefLevelTypeChanged();
  void onUseNumpadForSwitchingStylesClicked();
  // Tools
  void onLevelBasedToolsDisplayChanged();
  // Xsheet
  void onShowKeyframesOnCellAreaChanged();
  void onShowXSheetToolbarClicked();
  // Preview
  void onBlankCountChanged();
  void onBlankColorChanged();
  // Onion Skin
  void onOnionSkinVisibilityChanged();
  void onOnionColorChanged();
  // Colors
  void onTranspCheckDataChanged();
  // Version Control
  void onSVNEnabledChanged();
  // Commonly used
  void notifySceneChanged();

private slots:
  void onChange();
  void onColorFieldChanged(const TPixel32&, bool);

  void onAutoSaveExternallyChanged();
  void onAutoSavePeriodExternallyChanged();
  void onProjectRootChanged();
  void onPixelUnitExternallySelected(bool on);
  void onInterfaceFontChanged(const QString& text);
  void onLutPathChanged();

  void onAddLevelFormat();
  void onRemoveLevelFormat();
  void onEditLevelFormat();
  void onLevelFormatEdited();
  void onImportPolicyExternallyChanged(int policy);
};

//**********************************************************************************
//    PreferencesPopup::FormatProperties  definition
//**********************************************************************************

class PreferencesPopup::FormatProperties final : public DVGui::Dialog {
  Q_OBJECT

public:
  FormatProperties(PreferencesPopup* parent);

  void setLevelFormat(const Preferences::LevelFormat& lf);
  Preferences::LevelFormat levelFormat() const;

private:
  QComboBox* m_dpiPolicy;

  DVGui::LineEdit *m_name, *m_regExp;

  DVGui::DoubleLineEdit* m_dpi;

  DVGui::IntLineEdit *m_priority, *m_subsampling, *m_antialias;

  DVGui::CheckBox *m_premultiply, *m_whiteTransp, *m_doAntialias;

private slots:

  void updateEnabledStatus();
};

#endif  // PREFERENCESPOPUP_H
