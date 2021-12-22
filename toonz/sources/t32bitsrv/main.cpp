

#if (!(defined(x64) || defined(__LP64__) || defined(LINUX) || defined(FREEBSD)))

// Toonz includes
#include "tiio_std.h"
#include "tnzimage.h"

// Qt includes
#include <QCoreApplication>
#include <QThread>

// tipc includes
#include "tipcmsg.h"
#include "tipcsrv.h"

// Specific Parsers includes
#include "t32movmsg.h"
#include "t323gpmsg.h"
#include "t32fontmsg.h"

//************************************************************************
//    Server Thread
//************************************************************************

class ServerThread : public QThread {
  QString m_srvName;

public:
  ServerThread(const QString &srvName) : m_srvName(srvName) {}

  void run() override {
    // Start a local server receiving connections on the specified key
    tipc::Server server;
    mov_io::addParsers(&server);
    _3gp_io::addParsers(&server);
#ifdef MACOSX
    font_io::addParsers(&server);
#endif

    // Start listening on supplied key
    bool ok = server.listen(m_srvName);

    exec();
  }
};

//************************************************************************
//    Main server implementation
//************************************************************************

int main(int argc, char *argv[]) {
  if (argc < 2)  // The server key name must be passed
    return -1;

  QCoreApplication a(argc, argv);

  Tiio::defineStd();
  initImageIo();

  QString srvName(QString::fromUtf8(argv[1]));
  QString mainSrvName(srvName + "_main");

  QLocalServer::removeServer(srvName);
  QLocalServer::removeServer(mainSrvName);

  // Start a separate thread to host most of the event processing
  ServerThread *srvThread = new ServerThread(srvName);
  srvThread->start();

  // Start a server on the main thread too - this one to host
  // commands that need to be explicitly performed on the main thread
  tipc::Server server;
  mov_io::addParsers(&server);
  _3gp_io::addParsers(&server);
#ifdef MACOSX
  font_io::addParsers(&server);
#endif

  // Start listening on supplied key
  bool ok = server.listen(srvName + "_main");

  a.exec();
}

#else

int main(int argc, char *argv[]) { return 0; }

#endif  // !x64 && !__LP64__
