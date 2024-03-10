

// Qt includes
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QSharedMemory>
#include <QProcess>
#include <QMutex>
#include <QSemaphore>
#include <QAtomicInt>
#include <QEventLoop>
#include <QTimer>

// System-specific includes
#if defined(_WIN32)
#include <windows.h>
#elif defined(MACOSX)
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(LINUX) || defined(FREEBSD) || defined(HAIKU)
#include <sys/time.h>
#include <unistd.h>
#endif

#include "tipc.h"

/*
PLATFORM-SPECIFIC REMINDERS:

There are few remarks to be aware when maintaining this code.
Please, be careful that:

- It seems that, on Windows, QLocalSocket::waitForBytesWritten does not return
  success unless the data is actually read on the other end. On Unix this is not
  the case, presumably because the data is written to a buffer which can be read
by
  the other process at some later point.

  Thus, *BE SURE* that every data written is received on the other side.

- On MACOSX, the default shared memory settings can be quite restrictive.
  On a standard machine, the maximum size of a shared segment is 4 MB, exactly
  the same as the TOTAL size of shared memory available.

  Whereas tipc respects the former parameter, there must be a way to circumvent
the
  latter in order to make the allocation of multiple shared segments of the
maximum
  size.
*/

//********************************************************
//    Diagnostics Stuff
//********************************************************

// #define TIPC_DEBUG

#ifdef TIPC_DEBUG
#define tipc_debug(expr) expr
#else
#define tipc_debug(expr)
#endif

#ifdef TIPC_DEBUG
#include <QTime>
#endif

//********************************************************
//    Local namespace Stuff
//********************************************************

namespace {
int shm_max = -1;
int shm_all = -1;
int shm_seg = -1;
int shm_mni = -1;
}  // namespace

//********************************************************
//    tipc Stream Implementation
//********************************************************

int tipc::Stream::readSize() {
  if (m_socket->bytesAvailable() < sizeof(TINT32)) return -1;

  TINT32 msgSize = -1;
  m_socket->peek((char *)&msgSize, sizeof(TINT32));

  return msgSize;
}

//-------------------------------------------------------------

bool tipc::Stream::messageReady() {
  TINT32 msgSize;
  return (msgSize = readSize()) >= 0 && m_socket->bytesAvailable() >= msgSize;
}

//-------------------------------------------------------------

bool tipc::Stream::readData(char *data, qint64 dataSize, int msecs) {
  tipc_debug(qDebug("tipc::Stream::readData entry"));
  qint64 r, dataRead = 0;
  char *currData = data;

  while (dataRead < dataSize) {
    if ((m_socket->bytesAvailable() == 0) &&
        !m_socket->waitForReadyRead(msecs)) {
      tipc_debug(
          qDebug("tipc::Stream::readData exit (unexpected loss of data)"));
      return false;
    }

    // Read the supplied data
    currData += r = m_socket->read(currData, dataSize - dataRead);
    dataRead += r;
  }

  tipc_debug(qDebug("tipc::Stream::readData exit"));

  return true;
}

//-------------------------------------------------------------

bool tipc::Stream::readDataNB(char *data, qint64 dataSize, int msecs,
                              QEventLoop::ProcessEventsFlag flag) {
  tipc_debug(qDebug("tipc::Stream::readDataNB entry"));
  qint64 r, dataRead = 0;
  char *currData = data;

  QEventLoop loop;
  QObject::connect(m_socket, SIGNAL(readyRead()), &loop, SLOT(quit()));
  QObject::connect(m_socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                   &loop, SLOT(quit()));

  if (msecs >= 0) QTimer::singleShot(msecs, &loop, SLOT(quit()));

  while (dataRead < dataSize) {
    if (m_socket->bytesAvailable() == 0) {
      loop.exec(flag);
      if (m_socket->bytesAvailable() == 0) {
        tipc_debug(
            qDebug("tipc::Stream::readDataNB exit (unexpected loss of data)"));
        return false;
      }
    }

    // Read the supplied data
    currData += r = m_socket->read(currData, dataSize - dataRead);
    dataRead += r;
  }

  tipc_debug(qDebug("tipc::Stream::readDataNB exit"));

  return true;
}

//-------------------------------------------------------------

/*!
  Reads the message and returns its header.
  This function reads a complete message from the socket, waiting
  until it is completely available. The function accepts
  an inactivity timeout which can be supplied to drop the operation
  after msecs milliseconds no data has been received.
*/
bool tipc::Stream::readMessage(Message &msg, int msecs) {
  TINT32 msgSize = 0;
  if (!readData((char *)&msgSize, sizeof(TINT32), msecs)) return false;

  msg.ba().resize(msgSize);
  if (!readData(msg.ba().data(), msgSize, msecs)) return false;

  return true;
}

//-------------------------------------------------------------

/*!
  The non-blocking equivalent to readMessage(), this function
  performs event processing in a local event loop until all
  message data has been received.
*/
bool tipc::Stream::readMessageNB(Message &msg, int msecs,
                                 QEventLoop::ProcessEventsFlag flag) {
  TINT32 msgSize = 0;
  if (!readDataNB((char *)&msgSize, sizeof(TINT32), msecs, flag)) return false;

  msg.ba().resize(msgSize);
  if (!readDataNB(msg.ba().data(), msgSize, msecs, flag)) return false;

  return true;
}

//-------------------------------------------------------------

/*!
  Flushes all data written to the stream.
  This function waits until all data written on the stream
  has been successfully delivered in output.
  Returns true if the operation was successful, false if
  it timed out or an error occurred.
*/
bool tipc::Stream::flush(int msecs) {
  tipc_debug(qDebug("tipc:flush entry"));

  while (m_socket->bytesToWrite() > 0) {
    tipc_debug(qDebug() << "bytes to write:" << m_socket->bytesToWrite());
    bool ok = m_socket->flush();
    tipc_debug(qDebug() << "flush success:" << ok
                        << "bytes to write:" << m_socket->bytesToWrite());
    if (m_socket->bytesToWrite() > 0 && !m_socket->waitForBytesWritten(msecs))
      return false;
  }

  tipc_debug(qDebug() << "tipc:flush exit - bytes to write:"
                      << m_socket->bytesToWrite());
  return (m_socket->bytesToWrite() == 0);
}

//********************************************************
//    tipc Stream Operators
//********************************************************

//! \warning This operation assumes that all the message is available for read.
//! Use tipc::stream::readMessage if this cannot be ensured.
tipc::Stream &operator>>(tipc::Stream &stream, tipc::Message &msg) {
  QLocalSocket *socket = stream.socket();
  msg.clear();

  TINT32 msgSize;
  socket->read((char *)&msgSize, sizeof(TINT32));
  msg.ba().resize(msgSize);
  socket->read(msg.ba().data(), msgSize);
  return stream;
}

tipc::Stream &operator<<(tipc::Stream &stream, tipc::Message &msg) {
  QLocalSocket *socket = stream.socket();

  TINT32 size = msg.ba().size();
  socket->write((char *)&size, sizeof(TINT32));
  socket->write(msg.ba().data(), size);

  return stream;
}

//********************************************************
//    tipc Utilities
//********************************************************

/*!
  Appends the invoking process' pid to the passed srvName.
*/
QString tipc::applicationSpecificServerName(QString srvName) {
  return srvName + QString::number(QCoreApplication::applicationPid());
}

//-------------------------------------------------------------

bool tipc::startBackgroundProcess(QString cmdlineProgram,
                                  QStringList cmdlineArguments) {
#ifdef _WIN32
  QProcess *proc = new QProcess;

  proc->start(cmdlineProgram, cmdlineArguments);
  if (proc->state() == QProcess::NotRunning) {
    delete proc;
    return false;
  }

  QObject::connect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc,
                   SLOT(deleteLater()));
  QObject::connect(proc, SIGNAL(error(QProcess::ProcessError)), proc,
                   SLOT(deleteLater()));
  return true;
#else
  return QProcess::startDetached(cmdlineProgram, cmdlineArguments);
  ;
#endif
}

//-------------------------------------------------------------

/*!
  Invokes the passed command line to run a slave server.
  A slave server is hereby intended as a 'child' server process which
  automatically destroys itself in case the calling application
  crashes.
  This process \b MUST support one server, running in the \b MAIN \b THREAD,
  whose name is <srvName>_main.
  This function waits until the main server is up and ready to
  listen for incoming connections - no timeout accepted.

  \warning Please, observe that a correct slave server name should be
  ensured to be unique to the system.
*/
bool tipc::startSlaveServer(QString srvName, QString cmdlineProgram,
                            QStringList cmdlineArguments) {
  if (!tipc::startBackgroundProcess(cmdlineProgram, cmdlineArguments))
    return false;

  QString mainSrvName(srvName + "_main");

  // Establish a dummy socket connection to provide a mean for the process
  // to tell whether the calling process exited unexpectedly.
  QLocalSocket *dummySock = new QLocalSocket;
  dummySock->connectToServer(mainSrvName);

  // Wait up to msecs until the socket is connecting. Wait a small amount of
  // time
  // until the server is up and listening to connection (there is no other way
  // to tell).
  while (dummySock->state() == QLocalSocket::UnconnectedState) {
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10 << 10);  // 10.24 msecs
#endif

    dummySock->connectToServer(mainSrvName);
  }

  dummySock->waitForConnected(-1);

  tipc::Stream stream(dummySock);
  tipc::Message msg;

  // Supply the 'quit if this socket connection fails' command
  // This command ensure termination of the child process in case of some errors
  // or ending of the program
  stream << (msg << QString("$quit_on_error"));
  if (tipc::readMessage(stream, msg, 3000) == QString()) {
    std::cout << "tipc::startSlaveServer - tipc::readMessage TIMEOUT"
              << std::endl;
    return false;
  }

  // The server should die if dummyDock is destroyed. This should happen when
  // the *MAIN* thread
  // in *this process* exits. So, if this is not the main thread, we must move
  // the socket there.
  if (QCoreApplication::instance() &&
      QThread::currentThread() != QCoreApplication::instance()->thread())
    dummySock->moveToThread(QCoreApplication::instance()->thread());

  // If a connection error takes place, release the dummy socket.
  // Please, observe that this QObject::connect is invoked *AFTER* the
  // connection trials above...
  QObject::connect(dummySock, SIGNAL(error(QLocalSocket::LocalSocketError)),
                   dummySock, SLOT(deleteLater()));

  return true;
}

//-------------------------------------------------------------

/*!
  Connects the passed socket to the server with name <srvName> + <threadName>.
  Awaits for the connection up to msecs milliseconds before returning false.
  If no server was found, a new slave server is started by invoking
  the supplied command line and connection is re-attempted.
  Returns true on success, false otherwise.

  \warning Please, observe that a correct slave server name should be
  ensured to be unique to the parent process.
*/
bool tipc::startSlaveConnection(QLocalSocket *socket, QString srvName,
                                int msecs, QString cmdlineProgram,
                                QStringList cmdlineArguments,
                                QString threadName) {
  QElapsedTimer time;
  time.start();

  if (msecs == -1) msecs = (std::numeric_limits<int>::max)();

  QString fullSrvName(srvName + threadName);
  socket->connectToServer(fullSrvName);

  // If the socket is not connecting, the server lookup table returned that the
  // no server with
  // the passed name exists. This means that a server must be created.
  if (socket->state() == QLocalSocket::UnconnectedState &&
      !cmdlineProgram.isEmpty()) {
    // Completely serialize the server start
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    // Retry connection - this is required due to the mutex
    socket->connectToServer(fullSrvName);
    if (socket->state() != QLocalSocket::UnconnectedState) goto connecting;

    // Invoke the supplied command line to start the server
    if (!tipc::startSlaveServer(srvName, cmdlineProgram, cmdlineArguments))
      return false;

    // Reconnect to the server
    socket->connectToServer(fullSrvName);
    if (socket->state() == QLocalSocket::UnconnectedState) return false;
  }

connecting:

  // Now, the server is connecting or already connected. Wait until the socket
  // is connected.
  socket->waitForConnected(msecs - time.elapsed());
  if (socket->state() != QLocalSocket::ConnectedState) return false;

  return true;
}

//-------------------------------------------------------------

/*!
  Waits and reads the next message from stream.
  This function is mainly a useful macro that encapsulates
  the following steps in one call:

  \li Flush the write buffer (output messages)
  \li Wait until an input message is completely readable
  \li Read the message from stream
  \li Read the first string from the message and return it

  This function returns an empty QString if the message could not be
  entirely retrieved from the stream.
*/
QString tipc::readMessage(Stream &stream, Message &msg, int msecs) {
  msg.clear();
  stream.flush();
  if (!stream.readMessage(msg, msecs)) return QString();

  QString res;
  msg >> res;
  return res;
}

//-------------------------------------------------------------

/*!
  The non-blocking equivalent to tipc::readMessage.
*/
QString tipc::readMessageNB(Stream &stream, Message &msg, int msecs,
                            QEventLoop::ProcessEventsFlag flag) {
  msg.clear();
  if (!stream.readMessageNB(msg, msecs, flag)) return QString();

  QString res;
  msg >> res;
  return res;
}

//-------------------------------------------------------------

/*!
  Returns an inter-process unique id string; the returned
  id should be used to create QSharedMemory objects.
*/
QString tipc::uniqueId() {
  static QAtomicInt count;
  count.ref();
  return QString::number(QCoreApplication::applicationPid()) + "_" +
         QString::number((int)count);
}

//-------------------------------------------------------------

//! Returns the maximum size of a shared memory segment allowed by the system
int tipc::shm_maxSegmentSize() {
  if (shm_max < 0) {
#ifdef MACOSX
    // Retrieve it by invoking sysctl
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmmax", &val, &valSize, NULL, 0);
    shm_max = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
    // Windows case: no such limit
    // Observe that QSharedMemory accepts only an int size - so the num_lim is
    // against int.
    shm_max = (std::numeric_limits<int>::max)();
#endif
  }

  return shm_max;
}

//-------------------------------------------------------------

//! Returns the maximum number of shared segments allowed by the system
int tipc::shm_maxSegmentCount() {
  if (shm_seg < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmseg", &val, &valSize, NULL, 0);
    shm_seg = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
    // Windows case: no such limit - again, using limit against max due to Qt
    shm_seg = (std::numeric_limits<int>::max)();
#endif
  }

  return shm_seg;
}

//-------------------------------------------------------------

int tipc::shm_maxSharedPages() {
  if (shm_all < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmall", &val, &valSize, NULL, 0);
    shm_all = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
    shm_all = (std::numeric_limits<int>::max)();
#endif
  }

  return shm_all;
}

//-------------------------------------------------------------

int tipc::shm_maxSharedCount() {
  if (shm_mni < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmmni", &val, &valSize, NULL, 0);
    shm_mni = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
    shm_mni = (std::numeric_limits<int>::max)();
#endif
  }

  return shm_mni;
}

//-------------------------------------------------------------

/*!
  Attempts to set the shared memory parameters to the system.
  This is only working on MAC's SystemV shm, it's a no-op on Win.
  This function will fail anyway if the process is not owned by an
  admin.
*/
void tipc::shm_set(int shmmax, int shmseg, int shmall, int shmmni) {
  tipc_debug(qDebug("shmmax: %i, shmseg: %i, shmall: %i, shmmni: %i", shmmax,
                    shmseg, shmall, shmmni));
#ifdef MACOSX
  TINT64 val;
  int err;
  if (shmmax > 0) {
    val = shmmax;
    err = sysctlbyname("kern.sysv.shmmax", NULL, NULL, &val, sizeof(TINT64));
    if (!err) shm_max = shmmax;
  }
  if (shmseg > 0) {
    val = shmseg;
    err = sysctlbyname("kern.sysv.shmseg", NULL, NULL, &val, sizeof(TINT64));
    if (!err) shm_seg = shmseg;
  }
  if (shmall > 0) {
    val = shmall;
    err = sysctlbyname("kern.sysv.shmall", NULL, NULL, &val, sizeof(TINT64));
    if (!err) shm_all = shmall;
  }
  if (shmmni > 0) {
    val = shmmni;
    err = sysctlbyname("kern.sysv.shmmni", NULL, NULL, &val, sizeof(TINT64));
    if (!err) shm_mni = shmmni;
  }
#endif
}

//-------------------------------------------------------------

/*!
  Creates a shared memory segment for passed QSharedMemory.

  This function attempts creation of a shared memory segment
  in the form of Qt's QSharedMemory, with the following \b UNIX-specific
  distinctions:

  <LI> If the segment size is beyond that supported by the system,
  the function can be set to either fail or return a segment with
  the maximum supported size. <\LI>

  <LI> Unlike QSharedMemory::create, this function attempts to
  reclaim an already existing memory id before creating a new one. <\LI>
*/
int tipc::create(QSharedMemory &shmem, int size, bool strictSize) {
  bool ok, retried = false;

  if (!strictSize) size = std::min(size, (int)shm_maxSegmentSize());

  tipc_debug(qDebug() << "shMem create: size =" << size);

retry:

  ok = shmem.create(size);
  if (!ok) {
    tipc_debug(qDebug() << "Error: Shared Segment could not be created: #"
                        << shmem.errorString());

    // Unix-specific error recovery follows. See Qt's docs about it.

    // Try to recover error #AlreadyExists - supposedly, the server crashed in a
    // previous instance.
    // As shared memory segments that happen to go this way are owned by the
    // server process with 1
    // reference count, detaching it now may solve the issue.
    if (shmem.error() == QSharedMemory::AlreadyExists && !retried) {
      retried = true;  // We're trying this only once... for now it works.
      shmem.attach();
      shmem.detach();
      goto retry;
    }

    return -1;
  }

  return size;
}

//-------------------------------------------------------------

/*!
  Writes data through a shared memory segment medium.
*/
bool tipc::writeShMemBuffer(Stream &stream, Message &msg, int bufSize,
                            ShMemWriter *dataWriter) {
  tipc_debug(QTime time; time.start());
  tipc_debug(qDebug("tipc::writeShMemBuffer entry"));

  static QSemaphore sem(tipc::shm_maxSegmentCount());
  sem.acquire(1);

  {
    // Create a shared memory segment, possibly of passed size
    QSharedMemory shmem(tipc::uniqueId());
    bool ok = (tipc::create(shmem, bufSize) > 0);
    if (!ok) goto err;

    // Communicate the shared memory id and bufSize to the reader
    msg << QString("shm") << shmem.key() << bufSize;

    // Fill in data until all the buffer has been sent
    int chunkData, remainingData = bufSize;
    while (remainingData > 0) {
      // Write to the shared memory segment
      tipc_debug(QTime xchTime; xchTime.start());
      shmem.lock();
      remainingData -= chunkData = dataWriter->write(
          (char *)shmem.data(), std::min(shmem.size(), remainingData));
      shmem.unlock();
      tipc_debug(qDebug() << "exchange time:" << xchTime.elapsed());

      stream << (msg << QString("chk") << chunkData);

      if (tipc::readMessage(stream, msg) != "ok") goto err;

      msg.clear();
    }
  }

  sem.release(1);
  tipc_debug(qDebug("tipc::writeShMemBuffer exit"));
  tipc_debug(qDebug() << "tipc::writeShMemBuffer time:" << time.elapsed());
  return true;

err:

  tipc_debug(qDebug("tipc::writeShMemBuffer exit (error)"));

  msg.clear();
  sem.release(1);
  return false;
}

//-------------------------------------------------------------

/*!
  Reads data through a shared memory segment medium.
*/
bool tipc::readShMemBuffer(Stream &stream, Message &msg,
                           ShMemReader *dataReader) {
  tipc_debug(QTime time; time.start(););
  tipc_debug(qDebug("tipc::readShMemBuffer entry"));

  // Read the id from stream
  QString res(tipc::readMessage(stream, msg));
  if (res != "shm") {
    tipc_debug(qDebug("tipc::readShMemBuffer exit (res != \"shm\")"));
    return false;
  }

  // Read message and reply
  QString id, chkStr;
  int bufSize;
  msg >> id >> bufSize >> chkStr;

  // Data is ready to be read - attach to the shared memory segment.
  QSharedMemory shmem(id);
  shmem.attach();
  if (!shmem.isAttached()) {
    tipc_debug(qDebug("tipc::readShMemBuffer exit (shmem not attached)"));
    return false;
  }

  // Start reading from it
  int chunkData, remainingData = bufSize;
  while (true) {
    msg >> chunkData;

    tipc_debug(QTime xchTime; xchTime.start());
    shmem.lock();
    remainingData -= dataReader->read((const char *)shmem.data(), chunkData);
    shmem.unlock();
    tipc_debug(qDebug() << "exchange time:" << xchTime.elapsed());

    // Data was read. Inform the writer
    stream << (msg << clr << QString("ok"));
    stream.flush();

    if (remainingData <= 0) break;

    // Wait for more chunks
    if (tipc::readMessage(stream, msg) != "chk") {
      tipc_debug(
          qDebug("tipc::readShMemBuffer exit (unexpected chunk absence)"));
      return false;
    }
  }

  shmem.detach();
  tipc_debug(qDebug("tipc::readShMemBuffer exit"));
  tipc_debug(qDebug() << "tipc::readShMemBuffer time:" << time.elapsed());
  return true;
}
