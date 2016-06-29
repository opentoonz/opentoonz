#pragma once

#ifndef VERSION_CONTROL_TIMELINE_H
#define VERSION_CONTROL_TIMELINE_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include <QString>

class QListWidget;
class QListWidgetItem;
class QTemporaryFile;
class QPushButton;
class QCheckBox;
class QTimer;

//-----------------------------------------------------------------------------

class TimelineWidget final : public QWidget {
  Q_OBJECT
  QListWidget *m_listWidget;

public:
  TimelineWidget(QWidget *parent = 0);

  QListWidget *getListWidget() const { return m_listWidget; }

  int getCurrentIndex() const;
};

//-----------------------------------------------------------------------------

class SVNTimeline final : public DVGui::Dialog {
  Q_OBJECT

  TimelineWidget *m_timelineWidget;

  VersionControlThread m_thread;

  QList<SVNLog> m_log;

  QList<QListWidgetItem *> m_listWidgetitems;
  QList<QTemporaryFile *> m_tempFiles;
  QList<QTemporaryFile *> m_auxTempFiles;

  QStringList m_auxFiles;
  QStringList m_sceneResources;

  QString m_workingDir;
  QString m_fileName;

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QPushButton *m_updateButton;
  QPushButton *m_updateToRevisionButton;
  QPushButton *m_closeButton;

  QCheckBox *m_sceneContentsCheckBox;

  int m_currentAuxExportIndex;
  int m_currentExportIndex;

public:
  SVNTimeline(QWidget *parent, const QString &workingDir,
              const QString &fileName, const QStringList &auxFiles);
  ~SVNTimeline();

private:
  void exportToTemporaryFile(int index, bool isAuxFile = false);

  QIcon createIcon(const QString &fileName);

  void removeTempFiles();

protected slots:

  void onUpdateDone();
  void onLogDone(const QString &);

  void onUpdateButtonClicked();
  void onUpdateToRevisionButtonClicked();

  void onExportDone();
  void onExportAuxDone();

  void onStatusRetrieved(const QString &);

  void onError(const QString &);

  void onSelectionChanged();

  void onSceneContentsToggled(bool);

signals:
  void commandDone(const QStringList &files);
};

#endif  // VERSION_CONTROL_TIMELINE_H
