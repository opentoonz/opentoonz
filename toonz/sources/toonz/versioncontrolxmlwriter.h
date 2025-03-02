#pragma once

#include <QXmlStreamWriter>
#include <QString>
#include <QDialog>
#include <filebrowserpopup.h>
#include <filebrowser.h>

class SVNConfigWriter : public QObject{
  Q_OBJECT

  QString m_permission_path;
  QString m_versioncontrol_path;
  QString m_userName;

  class FolderSelector : public FileBrowserPopup {
  public:
    FolderSelector() : FileBrowserPopup(QString("Select a Folder")) {
      setOkText(tr("Select"));
      setFileMode(true);
    }
    bool execute() override {
      FileBrowserPopup::hide();
      return true;
    }
    void show() { 
        FileBrowserPopup::setModal(true);
        FileBrowserPopup::show();
        FileBrowserPopup::raise();
    };
    QString getFolderPath() { return m_nameField->text();}
  };

public:
  SVNConfigWriter();
  QString writeSvnUser(QString name);
  QString writeRepository(QString name);
  bool writeSvnUser(QString name, QString passwd,bool remove = false);
  bool writeRepository(QString name, QString repoPath, QString localPath,
                     bool remove = false);

};