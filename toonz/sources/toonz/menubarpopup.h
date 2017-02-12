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
  MenuBarTree(TFilePath& path, QWidget* parent = 0);
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
// CommandListTree
//-----------------------------------------------------------------------------

class CommandListTree final : public QTreeWidget {
  Q_OBJECT

  void addFolder(const QString& title, int commandType,
                 QTreeWidgetItem* parentFolder = 0);

public:
  CommandListTree(QWidget* parent = 0);

protected:
  void mousePressEvent(QMouseEvent*) override;
};

//=============================================================================
// MenuBarPopup
//-----------------------------------------------------------------------------

class MenuBarPopup final : public DVGui::Dialog {
  Q_OBJECT
  CommandListTree* m_commandListTree;
  MenuBarTree* m_menuBarTree;

public:
  MenuBarPopup(Room* room);
protected slots:
  void onOkPressed();
};

#endif