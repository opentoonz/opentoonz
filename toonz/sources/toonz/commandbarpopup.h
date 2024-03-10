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
  CommandItem(QTreeWidgetItem* parent, QAction* action);
  QAction* getAction() const { return m_action; }
};

//=============================================================================
// SeparatorItem
//-----------------------------------------------------------------------------

class SeparatorItem final : public QTreeWidgetItem {
public:
  SeparatorItem(QTreeWidgetItem* parent);
};

//=============================================================================
// CommandListTree
// shared by menubar popup and cutom panel editor popup
//-----------------------------------------------------------------------------

class CommandListTree final : public QTreeWidget {
  Q_OBJECT

  QString m_dropTargetString;

  QTreeWidgetItem* addFolder(const QString& title, int commandType,
                             QTreeWidgetItem* parentFolder = 0);

public:
  CommandListTree(const QString& dropTargetString, QWidget* parent = 0,
                  bool withSeparator = true);

  void searchItems(const QString& searchWord = QString());

private:
  void displayAll(QTreeWidgetItem* item);
  void hideAll(QTreeWidgetItem* item);

protected:
  void mousePressEvent(QMouseEvent*) override;
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
  CommandBarTree(TFilePath& path, QWidget* parent = 0);
  void saveMenuTree(TFilePath& path);

protected:
  bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data,
                    Qt::DropAction action) override;
  QStringList mimeTypes() const override;
  void contextMenuEvent(QContextMenuEvent* event) override;
protected slots:
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
  CommandBarPopup(bool isXsheetToolbar = false);
protected slots:
  void onOkPressed();
  void onSearchTextChanged(const QString& text);
};

#endif