#pragma once

#ifndef COMMANDBARPOPUP_H
#define COMMANDBARPOPUP_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QAction>

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"

class QXmlStreamReader;
class QXmlStreamWriter;

//=============================================================================
// CommandItem
//-----------------------------------------------------------------------------

class CommandItem final : public QTreeWidgetItem {
  QAction* m_action;

public:
  explicit CommandItem(QTreeWidgetItem* parent, QAction* action);

  QAction* getAction() const { return m_action; }
};

//=============================================================================
// SeparatorItem
//-----------------------------------------------------------------------------

class SeparatorItem final : public QTreeWidgetItem {
public:
  explicit SeparatorItem(QTreeWidgetItem* parent);
};

//=============================================================================
// CommandListTree
// Shared by menubar popup and custom panel editor popup
//-----------------------------------------------------------------------------

class CommandListTree final : public QTreeWidget {
  Q_OBJECT

  QString m_dropTargetString;

  QTreeWidgetItem* addFolder(const QString& title, int commandType,
                             QTreeWidgetItem* parentFolder = nullptr);

public:
  explicit CommandListTree(const QString& dropTargetString,
                           QWidget* parent    = nullptr,
                           bool withSeparator = true);

  void searchItems(const QString& searchWord = QString());

private:
  void displayAll(QTreeWidgetItem* item);
  void hideAll(QTreeWidgetItem* item);

protected:
  void mousePressEvent(QMouseEvent* event) override;
};

//=============================================================================
// CommandBarTree
//-----------------------------------------------------------------------------

class CommandBarTree final : public QTreeWidget {
  Q_OBJECT

  void loadMenuTree(const TFilePath& fp);
  void loadMenuRecursive(QXmlStreamReader& reader, QTreeWidgetItem* parentItem);
  void saveMenuRecursive(QXmlStreamWriter& writer, QTreeWidgetItem* parentItem);

public:
  explicit CommandBarTree(TFilePath& path, QWidget* parent = nullptr);

  void saveMenuTree(TFilePath& path);

protected:
  bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data,
                    Qt::DropAction action) override;

  QStringList mimeTypes() const override;

  void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
  void removeItem();
};

//=============================================================================
// CommandBarPopup
//-----------------------------------------------------------------------------

class CommandBarPopup final : public DVGui::Dialog {
  Q_OBJECT

  CommandListTree* m_commandListTree;
  CommandBarTree* m_menuBarTree;
  TFilePath m_path;

public:
  explicit CommandBarPopup(bool isXsheetToolbar = false);

private slots:
  void onOkPressed();
  void onSearchTextChanged(const QString& text);
};

#endif  // COMMANDBARPOPUP_H
