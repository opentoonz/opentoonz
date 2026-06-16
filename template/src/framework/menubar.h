#pragma once

#include <QTabBar>
#include <QToolBar>
#include <QMenuBar>
#include <QFrame>
#include <QStackedWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QMap>
#include <QContextMenuEvent>

class RoomTabWidget : public QTabBar {
  Q_OBJECT

  int m_clickedTabIndex;
  int m_tabToDeleteIndex;
  int m_renameTabIndex;
  QLineEdit *m_renameTextField;
  bool m_isLocked;

public:
  RoomTabWidget(QWidget *parent);
  ~RoomTabWidget();

  bool isLocked() { return m_isLocked; }

public slots:
  void setIsLocked(bool lock);

protected:
  void swapIndex(int firstIndex, int secondIndex);

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void updateTabName();
  void addNewTab();
  void deleteTab();

signals:
  void indexSwapped(int firstIndex, int secondIndex);
  void insertNewTabRoom();
  void deleteTabRoom(int index);
  void renameTabRoom(int index, const QString name);
};

//-----------------------------------------------------------------------------

class StackedMenuBar final : public QStackedWidget {
  Q_OBJECT

public:
  StackedMenuBar(QWidget *parent);
  void createMenuBarForRoom(const QString &roomName);

private:
  QMenuBar *buildDefaultMenuBar();
};

//-----------------------------------------------------------------------------

class TopBar final : public QToolBar {
  Q_OBJECT

  QFrame *m_containerFrame;
  RoomTabWidget *m_roomTabBar;
  StackedMenuBar *m_stackedMenuBar;
  QCheckBox *m_lockRoomCB;

public:
  TopBar(QWidget *parent);

  QTabBar *getRoomTabWidget() const { return m_roomTabBar; }
  StackedMenuBar *getStackedMenuBar() const { return m_stackedMenuBar; }

protected:
  void contextMenuEvent(QContextMenuEvent *event) override { event->accept(); }
};
