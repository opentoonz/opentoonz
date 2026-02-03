#include "versioncontrolxmlwriter.h"
#include <QFile>
#include <tenv.h>
#include <tsystem.h>
#include <QBuffer>
#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <filebrowserpopup.h>

SVNConfigWriter::SVNConfigWriter() { 
    m_permission_path = (TEnv::getConfigDir() + "permissions.xml").getQString();
  m_versioncontrol_path =
      (TEnv::getConfigDir() + "versioncontrol.xml").getQString();
  m_userName = TSystem::getUserName();
}

bool SVNConfigWriter::writeSvnUser(QString name, QString passwd,bool remove) {
  if (name.isEmpty()) return false;
  if (!remove)
      if(passwd.isEmpty()) 
          return false;

  QFile file(m_permission_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QXmlStreamReader r(&file);
  QByteArray newXmlContent;
  QBuffer buffer(&newXmlContent);
  buffer.open(QIODevice::WriteOnly);

  QXmlStreamWriter w(&buffer);
  w.setAutoFormatting(true);
  bool userExists = false;
  QString currentUserName;

  while (!r.atEnd()) {
    r.readNext();

    if (r.isStartElement()) {
      QString elementName = r.name().toString();

      if (elementName == "user") {
        currentUserName = r.attributes().value("name").toString();
        if (r.attributes().value("name") == m_userName) userExists = true;
      }

      if (userExists && elementName == "svn" &&
          r.attributes().value("name").toString() == name) {
        r.skipCurrentElement();
        continue;
      }

      w.writeStartElement(elementName);

      for (const QXmlStreamAttribute& attr : r.attributes()) {
        w.writeAttribute(attr.name().toString(),
                                   attr.value().toString());
      }
    } else if (r.isEndElement()) {
      QString elementName = r.name().toString();
      if (!userExists && elementName == "users") {
        w.writeStartElement("user");
        w.writeAttribute("name", m_userName);
        
        w.writeEmptyElement("svn");
        w.writeAttribute("name", name);
        w.writeAttribute("password", passwd);
        
        w.writeEndElement();//user
      }
      if (userExists && elementName == "user" &&
          currentUserName == m_userName && !remove) {
        w.writeEmptyElement("svn");
        w.writeAttribute("name", name);
        w.writeAttribute("password", passwd);
      }
      w.writeEndElement();
    } else if (r.isCharacters() && !r.isWhitespace()) {
      w.writeCharacters(r.text().toString());
    }
  }

  w.writeEndDocument();
  file.close();

  if (r.hasError()) {
    return false;
  }

  QFile outFile(m_permission_path);
  if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  outFile.write(newXmlContent);
  outFile.close();

  return true;
}

bool SVNConfigWriter::writeRepository(QString name, QString repoPath,
                                    QString localPath,bool remove) {
  if (name.isEmpty()) return false;
  if (!remove)
      if(repoPath.isEmpty() || localPath.isEmpty())
          return false;

  QFile file(m_versioncontrol_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QXmlStreamReader r(&file);
  QByteArray newXmlContent;
  QBuffer buffer(&newXmlContent);
  buffer.open(QIODevice::WriteOnly);

  QXmlStreamWriter w(&buffer);
  w.setAutoFormatting(true);
  w.writeStartDocument();
  bool foundRepo = false;

  while (!r.atEnd()) {
    r.readNext();

    if (r.isStartElement()) {
      QString elementName = r.name().toString();
      if (elementName == "repository") {
        r.readNextStartElement();
        elementName = r.name().toString();
        if (elementName == "name") {
          QString repoName = r.readElementText();
          if (repoName == name) {
            if (remove) {
              while (!(r.isEndElement() && r.name().toString() == "repository"))
                r.skipCurrentElement();
              continue;
            }
            w.writeStartElement("repository");

            w.writeTextElement("name", name);
            w.writeTextElement("localPath", localPath);
            w.writeTextElement("repoPath", repoPath);

            while (!(r.isEndElement() && r.name().toString() == "repository"))
              r.skipCurrentElement();

            w.writeEndElement();  // repository
            foundRepo = true;
            continue;
          } else {
            w.writeStartElement("repository");
            w.writeTextElement("name", repoName);
            continue;
          }
        }
      }
      
      if (!remove && !foundRepo && elementName == "svnPath") {
        w.writeStartElement("repository");

        w.writeTextElement("name", name);
        w.writeTextElement("localPath", localPath);
        w.writeTextElement("repoPath", repoPath);

        w.writeEndElement();  // repository
      }

      w.writeStartElement(elementName);
    } else if (r.isCharacters() && !r.isWhitespace()) {
      w.writeCharacters(r.text().toString());
    } else if (r.isEndElement()) {
      w.writeEndElement();
    } 
  }

  w.writeEndDocument();
  file.close();

  if (r.hasError()) {
    return false;
  }

  QFile outFile(m_versioncontrol_path);
  if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  outFile.write(newXmlContent);
  outFile.close();

  return true;
}

QString SVNConfigWriter::writeSvnUser(QString name) {
  QDialog m_configUser;
  m_configUser.setWindowTitle("Edit User");
  QLineEdit* usernameEdit   = new QLineEdit(&m_configUser);
  if (!name.isEmpty()) {
    usernameEdit->setText(name);
    usernameEdit->setReadOnly(true);
  }
  QLineEdit* passwordEdit   = new QLineEdit(&m_configUser);
  QPushButton* userOkButton = new QPushButton("OK");

  // Layout
  QVBoxLayout* layout = new QVBoxLayout(&m_configUser);
  layout->addWidget(new QLabel("User Name:"));
  layout->addWidget(usernameEdit);
  layout->addWidget(new QLabel("Password:"));
  layout->addWidget(passwordEdit);
  layout->addWidget(userOkButton);
  m_configUser.setLayout(layout);

  QObject::connect(userOkButton, &QPushButton::clicked, &m_configUser,
                   &QDialog::accept);
  connect(&m_configUser, &QDialog::accepted, this,
          [this, usernameEdit, passwordEdit]() {
            writeSvnUser(usernameEdit->text(), passwordEdit->text());
          });
  m_configUser.exec();
  return usernameEdit->text();
}

QString SVNConfigWriter::writeRepository(QString name) {
  QDialog m_configRepo;
  m_configRepo.setWindowTitle("Edit Repository");
  m_configRepo.setMinimumWidth(400);
  QLineEdit* repoNameEdit      = new QLineEdit(&m_configRepo);
  if (!name.isEmpty()) {
    repoNameEdit->setText(name);
    repoNameEdit->setReadOnly(true);
  }
  QLineEdit* repoEdit          = new QLineEdit(&m_configRepo);
  QLineEdit* repoLocalPathEdit = new QLineEdit(&m_configRepo);
  QPushButton* repoOkButton    = new QPushButton("OK");
  QPushButton* openBrowserButton    = new QPushButton("...");
  FolderSelector* folderSelector = new FolderSelector();

  QVBoxLayout* repoLayout = new QVBoxLayout(&m_configRepo);
  QHBoxLayout* hLay        = new QHBoxLayout(&m_configRepo);
  repoLayout->addWidget(new QLabel("Repository Name:"));
  repoLayout->addWidget(repoNameEdit);
  repoLayout->addWidget(new QLabel("Repository Path:"));
  repoLayout->addWidget(repoEdit);
  repoLayout->addWidget(new QLabel("Local Path:"));
  hLay->addWidget(repoLocalPathEdit);
  hLay->addWidget(openBrowserButton);
  repoLayout->addLayout(hLay);
  repoLayout->addWidget(repoOkButton);
  m_configRepo.setLayout(repoLayout);

  // Connection
  QObject::connect(repoOkButton, &QPushButton::clicked, &m_configRepo,
                   &QDialog::accept);
  connect(&m_configRepo, &QDialog::accepted, this,
          [this, repoNameEdit, repoEdit, repoLocalPathEdit]() {
          writeRepository(repoNameEdit->text(), repoEdit->text(),
              repoLocalPathEdit->text());
          });
  connect(openBrowserButton, &QPushButton::clicked, folderSelector,
          &FolderSelector::show);
  connect(folderSelector, &QDialog::accepted, this,
          [repoLocalPathEdit, folderSelector]() {
            repoLocalPathEdit->setText(folderSelector->getFolderPath());
          });
  m_configRepo.exec();
  return repoNameEdit->text();
}