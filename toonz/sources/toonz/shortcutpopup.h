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
// E' l'editor dello shortcut associato all'azione corrente
// Visualizza lo shortcut e permette di cambiarlo digitando direttamente
// la nuova sequenza di tasti
// Per cancellarlo bisogna chiamare removeShortcut()
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
// Visualizza tutti le QAction (con gli eventuali shortcut assegnati)
// Serve per selezionare la QAction corrente
//-----------------------------------------------------------------------------

class ShortcutTree final : public QTreeWidget {
  Q_OBJECT
  std::vector<ShortcutItem *> m_items;
  std::vector<QTreeWidgetItem *> m_folders;
  std::vector<QTreeWidgetItem *> m_subFolders;
  QString m_lastSearchTerm;
  QSet<QString> m_userCollapsedDuringSearch;  // Track folders user manually closed during search

public:
  ShortcutTree(QWidget *parent = 0);
  ~ShortcutTree();

  void searchItems(const QString &searchWord = QString());
  void refreshTree(); // Rebuild the entire tree from CommandManager
  void saveExpandedState(); // Save expansion state to QSettings
  void restoreExpandedState(); // Restore expansion state from QSettings

protected:
  // aggiunge un blocco di QAction. commandType e' un
  // CommandType::MenubarCommandType
  void addFolder(const QString &title, int commandType,
                 QTreeWidgetItem *folder = 0);
  QStringList collectExpandedState() const;
  void applyExpandedState(const QStringList &expandedFolders,
                          bool useDefaultIfEmpty);

public slots:
  void onCurrentItemChanged(QTreeWidgetItem *current,
                            QTreeWidgetItem *previous);
  void onShortcutChanged();
  void onItemClicked(const QModelIndex &);
  void onItemCollapsed(QTreeWidgetItem *item);

signals:
  void actionSelected(QAction *);
  void searched(bool haveResult);
};

//=============================================================================
// ShortcutPopup
// -------------
// Questo e' il popup che l'utente utilizza per modificare gli shortcut
//-----------------------------------------------------------------------------

class ShortcutPopup final : public DVGui::Dialog {
  Q_OBJECT
  
  // Static instance pointer to allow refresh from external code
  static ShortcutPopup *s_instance;
  
  QPushButton *m_removeBtn;
  ShortcutViewer *m_sViewer;
  ShortcutTree *m_list;
  QComboBox *m_presetChoiceCB;
  QLineEdit *m_searchEdit;  // Store search field to preserve state
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
  
  // Static method to refresh the popup if it's currently open
  static void refreshIfOpen();

private:
  void setPresetShortcuts(TFilePath fp);
  void showDialog(QString text);
  bool showConfirmDialog();
  bool showOverwriteDialog(QString name);
  void importPreset();
  void buildPresets();
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *he) override;
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

#endif  //  SHORTCUTPOPUP_H
