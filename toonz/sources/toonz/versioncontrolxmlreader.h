#pragma once

#ifndef VERSION_CONTROL_XML_READER_H
#define VERSION_CONTROL_XML_READER_H

#include <QXmlStreamReader>
#include <QList>

//-----------------------------------------------------------------------------

struct SVNPartialLockInfo {
  QString m_fileName;
  QString m_userName;
  QString m_hostName;
  unsigned int m_from;
  unsigned int m_to;
};

//-----------------------------------------------------------------------------

struct SVNPartialLock {
  QString m_fileName;
  QList<SVNPartialLockInfo> m_partialLockList;
};

//-----------------------------------------------------------------------------

class SVNPartialLockReader final : public QXmlStreamReader {
  QList<SVNPartialLock> m_partialLock;
  QString m_data;

  SVNPartialLock m_currentPartialLock;

public:
  SVNPartialLockReader(const QString &xmlSVNResponse);

  QList<SVNPartialLock> getPartialLock() { return m_partialLock; }

protected:
  void readTarget();
  void readProperty();
  void readUnknownElement();

  void readPartialLock(const QString &text);
};

//-----------------------------------------------------------------------------

struct SVNRepository {
  QString m_name;
  QString m_localPath;
  QString m_repoPath;
  QString m_username;
  QString m_password;
};

//-----------------------------------------------------------------------------

class SVNConfigReader final : public QXmlStreamReader {
  QList<SVNRepository> m_repositories;

  QString m_data;
  QString m_path;

public:
  SVNConfigReader(const QString &xml);
  QList<SVNRepository> getRepositories() { return m_repositories; }
  QString getSVNPath() { return m_path; }

protected:
  void readRepository();
  void readSVNPath();
  void readUnknownElement();
};

//-----------------------------------------------------------------------------

struct SVNStatus {
  QString m_item;
  QString m_workingRevision;
  QString m_commitRevision;
  QString m_author;
  QString m_path;
  QString m_commitDate;
  QString m_repoStatus;
  // Lock management
  bool m_isLocked;
  QString m_lockOwner;
  QString m_lockHostName;
  QString m_lockComment;
  QString m_lockDate;
  // Partial Lock Management
  bool m_isPartialLocked;
  bool m_isPartialEdited;
  unsigned int m_editFrom;
  unsigned int m_editTo;
};

//-----------------------------------------------------------------------------

class SVNStatusReader final : public QXmlStreamReader {
  QList<SVNStatus> m_status;

  QString m_data;
  QString m_currentPath;

  QString m_currentCommitRevision;
  QString m_currentAuthor;
  QString m_currentDate;

  bool m_currentIsLocked;
  QString m_currentLockComment;
  QString m_currentLockOwner;
  QString m_currentLockDate;

public:
  SVNStatusReader(const QString &xmlSVNResponse);
  ~SVNStatusReader();

  QList<SVNStatus> getStatus() { return m_status; }

protected:
  void resetCurrentValues();

  void readEntry();
  void readWCStatus();
  void readRepoStatus();
  void readLock(bool statusAlreadyAdded);
  void readCommit();
  void readUnknownElement();
};

//-----------------------------------------------------------------------------

struct SVNLog {
  QString m_revision;
  QString m_author;
  QString m_date;
  QString m_msg;
};

//-----------------------------------------------------------------------------

class SVNLogReader final : public QXmlStreamReader {
  QList<SVNLog> m_log;

  QString m_data;

  QString m_currentRevision;
  QString m_currentAuthor;
  QString m_currentDate;
  QString m_currentMsg;

public:
  SVNLogReader(const QString &xmlSVNResponse);

  QList<SVNLog> getLog() { return m_log; }

protected:
  void readEntry();
  void readUnknownElement();
};

//-----------------------------------------------------------------------------

class SVNInfoReader final : public QXmlStreamReader {
  QString m_data;

  QString m_revision;
  QString m_url;

public:
  SVNInfoReader(const QString &xmlSVNResponse);

  QString getRevision() const { return m_revision; }
  QString getURL() const { return m_url; }

protected:
  void readEntry();
  void readUnknownElement();
};

//-----------------------------------------------------------------------------

struct SVNListInfo {
  QString m_kind;
  QString m_name;
};

//-----------------------------------------------------------------------------

class SVNListReader final : public QXmlStreamReader {
  QString m_data;
  QList<SVNListInfo> m_listInfo;

public:
  SVNListReader(const QString &xmlSVNResponse);

  QList<SVNListInfo> getList() { return m_listInfo; }

  QStringList getDirs();

protected:
  void readEntry();
  void readUnknownElement();
};

#endif  // VERSION_CONTROL_XML_READER_H
