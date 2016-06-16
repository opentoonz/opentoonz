

// Toonz includes
#include "traster.h"
#include "timage_io.h"

// Qt includes
#include <QIODevice>
#include <QSharedMemory>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

// tipc includes
#include "tipc.h"

#include "tipcmsg.h"

//---------------------------------------------------------------------

//  Local stuff

namespace {
QHash<QString, QSharedMemory *> sharedMemories;
QHash<QString, QString> temporaryFiles;
}

//---------------------------------------------------------------------

namespace tipc {

//*******************************************************************************
//    Shared Memory Request
//*******************************************************************************

template <>
QString DefaultMessageParser<SHMEM_REQUEST>::header() const {
  return QString("$shmem_request");
}

//------------------------------------------------------------------

template <>
void DefaultMessageParser<SHMEM_REQUEST>::operator()(Message &msg) {
  int size;
  QString id;
  msg >> id >> size >> clr;

  QSharedMemory *mem = new QSharedMemory(id);

  bool ok = (tipc::create(*mem, size) > 0);
  if (!ok) {
    msg << QString("err");
    delete mem;
    return;
  }

  sharedMemories.insert(id, mem);
  msg << QString("ok");
}

//*******************************************************************************
//    Shared Memory Release
//*******************************************************************************

template <>
QString DefaultMessageParser<SHMEM_RELEASE>::header() const {
  return QString("$shmem_release");
}

//------------------------------------------------------------------

template <>
void DefaultMessageParser<SHMEM_RELEASE>::operator()(Message &msg) {
  QString id;
  msg >> id >> clr;
  QSharedMemory *mem = sharedMemories.take(id);
  if (mem) delete mem;
  msg << QString("ok");
}

//*******************************************************************************
//    Temporary File Request
//*******************************************************************************

template <>
QString DefaultMessageParser<TMPFILE_REQUEST>::header() const {
  return QString("$tmpfile_request");
}

//------------------------------------------------------------------

template <>
void DefaultMessageParser<TMPFILE_REQUEST>::operator()(Message &msg) {
  QString id;
  msg >> id >> clr;

  // Build a temporary file with passed id group.
  // The created QTemporaryFile CANNOT be stored directly, as it internally
  // keeps the file open until the object is destroyed. Instead, we store its
  // filePath and manually remove it upon release.

  QTemporaryFile tmp(QDir::temp().filePath(id));
  tmp.setAutoRemove(false);
  if (!tmp.open()) {
    msg << QString("err");
    return;
  }

  temporaryFiles.insert(id, tmp.fileName());
  msg << QString("ok") << tmp.fileName();
}

//*******************************************************************************
//    Temporary File Release
//*******************************************************************************

template <>
QString DefaultMessageParser<TMPFILE_RELEASE>::header() const {
  return QString("$tmpfile_release");
}

//------------------------------------------------------------------

template <>
void DefaultMessageParser<TMPFILE_RELEASE>::operator()(Message &msg) {
  QString id;
  msg >> id >> clr;

  QString tmpPath = temporaryFiles.take(id);
  if (!tmpPath.isEmpty()) {
    QFile file(tmpPath);
    file.remove();
  }

  msg << QString("ok");
}

//*******************************************************************************
//    Quit On Error
//*******************************************************************************

template <>
QString DefaultMessageParser<QUIT_ON_ERROR>::header() const {
  return QString("$quit_on_error");
}

//------------------------------------------------------------------

template <>
void DefaultMessageParser<QUIT_ON_ERROR>::operator()(Message &msg) {
  QObject::connect(socket(), SIGNAL(error(QLocalSocket::LocalSocketError)),
                   QCoreApplication::instance(), SLOT(quit()));
  // In Qt 5.5 originating process's termination emits 'disconnected' instead of
  // 'error'
  QObject::connect(socket(), SIGNAL(disconnected()),
                   QCoreApplication::instance(), SLOT(quit()));

  msg << clr << QString("ok");
}

//*******************************************************************************
//    Explicit template instantiation for all basic datatypes
//*******************************************************************************

//-------------------------- Default Messages ---------------------------

template class DefaultMessageParser<SHMEM_REQUEST>;
template class DefaultMessageParser<SHMEM_RELEASE>;
template class DefaultMessageParser<TMPFILE_REQUEST>;
template class DefaultMessageParser<TMPFILE_RELEASE>;
template class DefaultMessageParser<QUIT_ON_ERROR>;

}  // namespace tipc
