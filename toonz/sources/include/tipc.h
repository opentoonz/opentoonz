#pragma once

#ifndef TIPC_H
#define TIPC_H

// Qt includes
#include <QDataStream>
#include <QByteArray>
#include <QLocalSocket>
#include <QEventLoop>

// STL includes
#include <limits>

// Toonz includes
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-----------------------------------------------

//  Forward declarations

class QSharedMemory;
class QProcess;

//-----------------------------------------------

//********************************************************
//    Toonz Inter-Process Communication namespace
//********************************************************

/*! \namespace tipc
  The tipc namespace provides facilities to deal with
  inter-process communication tasks.
  The tipc inner workings make use of Qt's
  QLocalServer/QLocalSocket standard ipc framework;
  but moreover, it helps the user hiding some of the
  intricacies of ipc management.
\m \m
  The tipc model assumes there is a server process
  running to which several processes can connect to in order
  to require services through appropriate server-supported
  commands. Communication happens by sending tipc::Message
  instances through a tipc::Stream constructed on a
  QLocalSocket device connected to the server in a Qt-like
  manner.
\n\n
  Messages encapsulate an atomic communication that must be
  received in its entirety to be understood by the receiver.
  Any message sent by a client must provide a QString header
  to identify the message type, which must be recognizable
  by the server side to be appropriately parsed.
  Each message understood by the server process will be replied
  with a message to ensure the client that the parsing has
  been completed.
\n\n
  A tipc::Server instance must be used on the server side to
  process supported messages. Message parsing is responsibility
  of specific subclasses of the tipc::MessageParser base class,
  which must be implemented and added to the server.
\n \n
  Tipc standard messages include requests for temporary files
  or shared memory segments, which must be released after use.
*/

namespace tipc {

//********************************************************
//    tipc Fundamental Classes
//********************************************************

/*!
  The tipc::Message class encapsulates several data into one single byte array
  in order to make data transfer among sockets an atomic operation.
*/
class Message {
  QByteArray m_ba;
  QDataStream m_ds;

public:
  Message() : m_ba(), m_ds(&m_ba, QIODevice::ReadWrite) {}
  ~Message() {}

  const QByteArray &ba() const { return m_ba; }
  QByteArray &ba() { return m_ba; }

  const QDataStream &ds() const { return m_ds; }
  QDataStream &ds() { return m_ds; }

  template <typename Type>
  Message &operator<<(const Type &data) {
    m_ds << data;
    return *this;
  }

  template <typename Type>
  Message &operator>>(Type &data) {
    m_ds >> data;
    return *this;
  }

  Message &operator<<(const char *str) { return operator<<(QString(str)); }

  void clear() {
    m_ba.clear();
    m_ds.device()->seek(0);
  }
};

//-----------------------------------------------------------------------

/*!
  A tipc::Stream is a specialized QDataStream designed to work with a tipc-based
  QLocalSocket instance.
*/
class DVAPI Stream final : public QDataStream {
  QLocalSocket *m_socket;

public:
  Stream(QLocalSocket *socket) : QDataStream(socket), m_socket(socket) {}

  QLocalSocket *socket() { return m_socket; }

  int readSize();
  bool messageReady();

  bool readData(char *data, qint64 dataSize, int msecs = -1);
  bool readMessage(Message &msg, int msecs = -1);

  bool readDataNB(char *data, qint64 dataSize, int msecs = -1,
                  QEventLoop::ProcessEventsFlag flag = QEventLoop::AllEvents);
  bool readMessageNB(
      Message &msg, int msecs = -1,
      QEventLoop::ProcessEventsFlag flag = QEventLoop::AllEvents);

  bool flush(int msecs = -1);
};

}  // namespace tipc

//********************************************************
//    tipc Stream Operators
//********************************************************

DVAPI tipc::Stream &operator>>(tipc::Stream &stream, tipc::Message &msg);
DVAPI tipc::Stream &operator<<(tipc::Stream &stream, tipc::Message &msg);

//-----------------------------------------------------------------------

inline tipc::Message &operator<<(tipc::Message &msg,
                                 tipc::Message &(*func)(tipc::Message &)) {
  return func(msg);
}

inline tipc::Message &operator>>(tipc::Message &msg,
                                 tipc::Message &(*func)(tipc::Message &)) {
  return func(msg);
}

//********************************************************
//    tipc Stream Manipulator Functions
//********************************************************

namespace tipc {

inline Message &clr(Message &msg) {
  msg.clear();
  return msg;
}

inline Message &reset(Message &msg) {
  msg.ds().device()->seek(0);
  return msg;
}

}  // namespace tipc

//********************************************************
//    tipc Utility Functions
//********************************************************

namespace tipc {

//---------------------- Connection-message utilities ----------------------

DVAPI bool startBackgroundProcess(QString cmdline);

DVAPI QString applicationSpecificServerName(QString srvName);
DVAPI bool startSlaveConnection(QLocalSocket *socket, QString srvName,
                                int msecs = -1, QString cmdline = QString(),
                                QString threadName = QString());
DVAPI bool startSlaveServer(QString srvName, QString cmdline);

DVAPI QString readMessage(Stream &stream, Message &msg, int msecs = -1);
DVAPI QString
readMessageNB(Stream &stream, Message &msg, int msecs = -1,
              QEventLoop::ProcessEventsFlag flag = QEventLoop::AllEvents);

//---------------------- Shared Memory utilities ----------------------

DVAPI int shm_maxSegmentSize();
DVAPI int shm_maxSegmentCount();
DVAPI int shm_maxSharedPages();
DVAPI int shm_maxSharedCount();

DVAPI void shm_set(int maxSegmentSize, int maxSegmentCount, int maxSharedSize,
                   int maxSharedCount);

DVAPI QString uniqueId();
DVAPI int create(QSharedMemory &shmem, int size, bool strictSize = false);

class ShMemReader {
public:
  virtual int read(const char *srcBuf, int len) = 0;
};
class ShMemWriter {
public:
  virtual int write(char *dstBuf, int len) = 0;
};

DVAPI bool readShMemBuffer(Stream &stream, Message &msg,
                           ShMemReader *dataReader);
DVAPI bool writeShMemBuffer(Stream &stream, Message &msg, int bufSize,
                            ShMemWriter *dataWriter);

}  // namespace tipc

//********************************************************
//    STL compatibility Stream operators
//********************************************************

template <typename T>
QDataStream &operator<<(QDataStream &ds, const std::vector<T> &vec) {
  unsigned int i, size = vec.size();

  ds << size;
  for (i = 0; i < size; ++i) ds << vec[i];

  return ds;
}

template <typename T>
QDataStream &operator>>(QDataStream &ds, T &vec) {
  return ds >> vec;
}

template <typename T>
QDataStream &operator>>(QDataStream &ds, std::vector<T> &vec) {
  unsigned int i, size;
  T val;

  ds >> size;
  vec.reserve(size);
  for (i = 0; i < size; ++i) {
    ds >> val;
    vec.push_back(val);
    if (ds.atEnd()) break;
  }

  return ds;
}

inline QDataStream &operator<<(QDataStream &ds, const std::string &str) {
  ds << QString::fromStdString(str);
  return ds;
}
inline QDataStream &operator>>(QDataStream &ds, std::string &str) {
  QString qstr;
  ds >> qstr;
  str = qstr.toStdString();
  return ds;
}

inline QDataStream &operator<<(QDataStream &ds, const std::wstring &str) {
  ds << QString::fromStdWString(str);
  return ds;
}
inline QDataStream &operator>>(QDataStream &ds, std::wstring &str) {
  QString qstr;
  ds >> qstr;
  str = qstr.toStdWString();
  return ds;
}

inline QDataStream &operator<<(QDataStream &ds, const wchar_t &ch) {
  ds.writeRawData((const char *)&ch, sizeof(wchar_t));
  return ds;
}
inline QDataStream &operator>>(QDataStream &ds, wchar_t &ch) {
  ds.readRawData((char *)&ch, sizeof(wchar_t));
  return ds;
}

#endif  // TIPC_H
