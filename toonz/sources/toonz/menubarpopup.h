#pragma once

#ifndef MENUBARPOPUP_H
#define MENUBARPOPUP_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QAction>

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"

class Room;
class QXmlStreamReader;
class QXmlStreamWriter;
class CommandListTree;

//=============================================================================
// MenuBarTree
//-----------------------------------------------------------------------------

class MenuBarTree final : public QTreeWidget {
  Q_OBJECT

  TFilePath m_path;

  void loadMenuTree(const TFilePath& fp);
  void loadMenuRecursive(QXmlStreamReader& reader, QTreeWidgetItem* parentItem);
  void saveMenuRecursive(QXmlStreamWriter& writer, QTreeWidgetItem* parentItem);

public:
  explicit MenuBarTree(TFilePath& path, QWidget* parent = nullptr);

  // Prohibit copying and moving
  MenuBarTree(const MenuBarTree&)            = delete;
  MenuBarTree& operator=(const MenuBarTree&) = delete;
  MenuBarTree(MenuBarTree&&)                 = delete;
  MenuBarTree& operator=(MenuBarTree&&)      = delete;

  void saveMenuTree();

protected:
  bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data,
                    Qt::DropAction action) override;
  QStringList mimeTypes() const override;
  void contextMenuEvent(QContextMenuEvent* event) override;

protected slots:
  void insertMenu();
  void removeItem();
  void onItemChanged(QTreeWidgetItem*, int);
};

//=============================================================================
// MenuBarPopup
//-----------------------------------------------------------------------------

class MenuBarPopup final : public DVGui::Dialog {
  Q_OBJECT

  CommandListTree* m_commandListTree = nullptr;
  MenuBarTree* m_menuBarTree         = nullptr;
  bool m_searchBusy                  = false;  // prevent reentrant search

public:
  explicit MenuBarPopup(Room* room);

  // Prohibit copying and moving
  MenuBarPopup(const MenuBarPopup&)            = delete;
  MenuBarPopup& operator=(const MenuBarPopup&) = delete;
  MenuBarPopup(MenuBarPopup&&)                 = delete;
  MenuBarPopup& operator=(MenuBarPopup&&)      = delete;

protected slots:
  void onOkPressed();
  void onSearchTextChanged(const QString& text);
};

#endif
