#pragma once

#ifndef SHORTCUTPOPUP_H
#define SHORTCUTPOPUP_H

#include <QDialog>
#include <QTreeWidget>
#include <QComboBox>
#include <QKeySequenceEdit>
#include "filebrowserpopup.h"
#include "toonzqt/dvdialog.h"

// forward declaration
class QPushButton;
class ShortcutItem;

//=============================================================================
// ShortcutViewer
// --------------
// Editor for the shortcut associated with the current action
// Displays the shortcut and allows changing it by typing directly
// the new key sequence
// To delete it, call removeShortcut()
//-----------------------------------------------------------------------------

class ShortcutViewer final : public QKeySequenceEdit {
  Q_OBJECT
  QAction *m_action;

  int m_keysPressed;

public:
  ShortcutViewer(QWidget *parent);
  ~ShortcutViewer();

protected:
  void enterEvent(QEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

public slots:
  void setAction(QAction *action);
  void removeShortcut();
  void onEditingFinished();

signals:
  void shortcutChanged();
};

//=============================================================================
// ShortcutTree
// ------------
// Displays all QActions (with their assigned shortcuts if any)
// Used to select the current QAction
//-----------------------------------------------------------------------------

class ShortcutTree final : public QTreeWidget {
  Q_OBJECT
  std::vector<ShortcutItem *> m_items;
  std::vector<QTreeWidgetItem *> m_folders;
  std::vector<QTreeWidgetItem *> m_subFolders;

public:
  ShortcutTree(QWidget *parent = 0);
  ~ShortcutTree();

  void searchItems(const QString &searchWord = QString());

protected:
  // adds a block of QActions. commandType is a
  // CommandType::MenubarCommandType
  void addFolder(const QString &title, int commandType,
                 QTreeWidgetItem *folder = 0);

public slots:
  void onCurrentItemChanged(QTreeWidgetItem *current,
                            QTreeWidgetItem *previous);
  void onShortcutChanged();
  void onItemClicked(const QModelIndex &);

signals:
  void actionSelected(QAction *);
  void searched(bool haveResult);
};

//=============================================================================
// ShortcutPopup
// -------------
// This is the popup that the user uses to modify shortcuts
//-----------------------------------------------------------------------------

class ShortcutPopup final : public DVGui::Dialog {
  Q_OBJECT
  QPushButton *m_removeBtn;
  ShortcutViewer *m_sViewer;
  ShortcutTree *m_list;
  QComboBox *m_presetChoiceCB;
  DVGui::Dialog *m_dialog;
  GenericLoadFilePopup *m_loadShortcutsPopup;
  GenericSaveFilePopup *m_saveShortcutsPopup;
  QPushButton *m_exportButton;
  QPushButton *m_deletePresetButton;
  QPushButton *m_savePresetButton;
  QPushButton *m_loadPresetButton;
  QPushButton *m_clearAllShortcutsButton;
  QLabel *m_dialogLabel;

public:
  ShortcutPopup();
  ~ShortcutPopup();

private:
  void setPresetShortcuts(TFilePath fp);
  void showDialog(QString text);
  bool showConfirmDialog();
  bool showOverwriteDialog(QString name);
  void importPreset();
  void buildPresets();
  void showEvent(QShowEvent *se) override;
  void setCurrentPresetPref(QString preset);
  void getCurrentPresetPref();

protected slots:
  void clearAllShortcuts(bool warning = true);
  void onSearchTextChanged(const QString &text);
  void onPresetChanged();
  void onExportButton(TFilePath fp = TFilePath());
  void onDeletePreset();
  void onSavePreset();
  void onLoadPreset();
};

#endif  // SHORTCUTPOPUP_H
