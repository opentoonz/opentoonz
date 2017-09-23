#pragma once

#ifndef TESTCUSTOMTAB_H
#define TESTCUSTOMTAB_H

#include "tfilepath.h"
#include "toonzqt/menubarcommand.h"

#if QT_VERSION >= 0x050000
#include <QtWidgets/QMainWindow>
#else
#include <QtGui/QMainWindow>
#endif
#include <map>
#include <QAction>
#include <QString>

#include "../toonzqt/tdockwindows.h"

class QStackedWidget;
class TPanel;
class UpdateChecker;
class ComboViewerPanel;
class TopBar;
//-----------------------------------------------------------------------------

class Room final : public TMainWindow {
  Q_OBJECT

  TFilePath m_path;
  QString m_name;

  /*--
   * Room毎にComboViewerの初期状態をLoadするため、MainWindowからComboViewerにアクセスできるようにする。
   * --*/
  ComboViewerPanel *m_centralViewer;

public:
#if QT_VERSION >= 0x050500
  Room(QWidget *parent = 0, Qt::WindowFlags flags = 0)
#else
  Room(QWidget *parent = 0, Qt::WFlags flags = 0)
#endif
      : TMainWindow(parent, flags), m_centralViewer(0) {
  }

  ~Room() {}

  TFilePath getPath() const { return m_path; }
  void setPath(TFilePath path) { m_path = path; }

  QString getName() const { return m_name; }
  void setName(QString name) { m_name = name; }

  ComboViewerPanel *getCentralViewerPanel() const { return m_centralViewer; }
  void setCentralViewerPanel(ComboViewerPanel *cvp) {
    if (!m_centralViewer) m_centralViewer = cvp;
  }

  void save();
  void load(const TFilePath &fp);
};

//-----------------------------------------------------------------------------

class MainWindow final : public QMainWindow {
  Q_OBJECT

  bool m_saveSettingsOnQuit;
  bool m_startupPopupShown = false;
  int m_oldRoomIndex;
  QString m_currentRoomsChoice;
  UpdateChecker *m_updateChecker;

  TopBar *m_topBar;

  QActionGroup *m_toolsActionGroup;

  QStackedWidget *m_stackedWidget;

  /*-- show layout name in the title bar --*/
  QString m_layoutName;

public:
#if QT_VERSION >= 0x050500
  MainWindow(const QString &argumentLayoutFileName, QWidget *parent = 0,
             Qt::WindowFlags flags = 0);
#else
  MainWindow(const QString &argumentLayoutFileName, QWidget *parent = 0,
             Qt::WFlags flags = 0);
#endif
  ~MainWindow();

  void startupFloatingPanels();

  void onQuit();
  void onUndo();
  void onRedo();
  void onNewScene();
  void onLoadScene();
  void onLoadSubScene();
  void resetRoomsLayout();
  void maximizePanel();
  void fullScreenWindow();
  void autofillToggle();
  void onUpgradeTabPro();
  void onAbout();
  void checkForUpdates();
  int getRoomCount() const;
  Room *getRoom(int index) const;
  Room *getRoomByName(QString &roomName);

  Room *getCurrentRoom() const;
  void refreshWriteSettings();
  /*-- FillAreas,FillLinesに直接切り替えるコマンド --*/
  void toggleFillAreas();
  void toggleFillLines();
  /*-- StylepickerAreas,StylepickerLinesに直接切り替えるコマンド --*/
  void togglePickStyleAreas();
  void togglePickStyleLines();
  void onNewVectorLevelButtonPressed();
  void onNewToonzRasterLevelButtonPressed();
  void onNewRasterLevelButtonPressed();

  QString getLayoutName() { return m_layoutName; }

protected:
  void showEvent(QShowEvent *) override;
  void closeEvent(QCloseEvent *) override;
  void readSettings(const QString &layoutFileName);
  void writeSettings();

private:
  /*!Must be call before readSettings().*/
  void defineActions();
  /*
  Room *createPaintRoom();
  Room *createAnimationRoom();
  Room *createBrowserRoom();
  Room *createPltEditRoom();
  Room *createFarmRoom();
  */
  Room *createCleanupRoom();
  Room *createPltEditRoom();
  Room *createInknPaintRoom();
  Room *createXsheetRoom();
  Room *createBatchesRoom();
  Room *createBrowserRoom();

  QAction *createAction(const char *id, const QString &name,
                        const QString &defaultShortcut,
                        CommandType type = MenuFileCommandType);
  QAction *createRightClickMenuAction(const char *id, const QString &name,
                                      const QString &defaultShortcut);
  QAction *createMenuFileAction(const char *id, const QString &name,
                                const QString &defaultShortcut);
  QAction *createMenuEditAction(const char *id, const QString &name,
                                const QString &defaultShortcut);
  QAction *createMenuScanCleanupAction(const char *id, const QString &name,
                                       const QString &defaultShortcut);
  QAction *createMenuLevelAction(const char *id, const QString &name,
                                 const QString &defaultShortcut);
  QAction *createMenuXsheetAction(const char *id, const QString &name,
                                  const QString &defaultShortcut);
  QAction *createMenuCellsAction(const char *id, const QString &name,
                                 const QString &defaultShortcut);
  QAction *createMenuViewAction(const char *id, const QString &name,
                                const QString &defaultShortcut);
  QAction *createMenuWindowsAction(const char *id, const QString &name,
                                   const QString &defaultShortcut);

  QAction *createPlaybackAction(const char *id, const QString &name,
                                const QString &defaultShortcut);
  QAction *createRGBAAction(const char *id, const QString &name,
                            const QString &defaultShortcut);
  QAction *createFillAction(const char *id, const QString &name,
                            const QString &defaultShortcut);
  QAction *createMenuAction(const char *id, const QString &name,
                            QList<QString> list);
  QAction *createToggle(const char *id, const QString &name,
                        const QString &defaultShortcut, bool startStatus,
                        CommandType type);
  QAction *createToolAction(const char *id, const char *iconName,
                            const QString &name,
                            const QString &defaultShortcut);
  QAction *createViewerAction(const char *id, const QString &name,
                              const QString &defaultShortcut);
  QAction *createMiscAction(const char *id, const QString &name,
                            const char *defaultShortcut);
  QAction *createToolOptionsAction(const char *id, const QString &name,
                                   const QString &defaultShortcut);

protected slots:
  void onCurrentRoomChanged(int newRoomIndex);
  void onIndexSwapped(int firstIndex, int secondIndex);
  void insertNewRoom();
  void deleteRoom(int index);
  void renameRoom(int index, const QString name);
  void onMenuCheckboxChanged();

  // make InkCheck and Ink#1Check exclusive.
  void onInkCheckTriggered(bool on);
  void onInk1CheckTriggered(bool on);

  void onUpdateCheckerDone(bool);

public slots:
  /*--- タイトルにシーン名を入れる ---*/
  void changeWindowTitle();
  /*--- FlipモジュールでタイトルバーにロードしたLevel名を表示 ---*/
  /*--- Cleanupモジュールでタイトルバーに進捗を表示 ---*/
  void changeWindowTitle(QString &);

signals:
  void exit(bool &);
};

class RecentFiles {
  friend class StartupPopup;
  QList<QString> m_recentScenes;
  QList<QString> m_recentLevels;
  QList<QString> m_recentFlipbookImages;

  RecentFiles();

public:
  enum FileType { Scene, Level, Flip, None };

  static RecentFiles *instance();
  ~RecentFiles();

  void addFilePath(QString path, FileType fileType);
  void moveFilePath(int fromIndex, int toIndex, FileType fileType);
  void removeFilePath(int fromIndex, FileType fileType);
  QString getFilePath(int index, FileType fileType) const;
  void clearRecentFilesList(FileType fileType);
  void loadRecentFiles();
  void saveRecentFiles();

protected:
  void refreshRecentFilesMenu(FileType fileType);
  QList<QString> getFilesNameList(FileType fileType);
};

#endif  // TESTCUSTOMTAB_H
