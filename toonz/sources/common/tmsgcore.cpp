

#include "tmsgcore.h"
#include <qlocalserver.h>
#include <qlocalsocket.h>
#include <QStringList>
#include <QCoreApplication>

#include <QTcpServer>
#include <QTcpSocket>

TMsgCore *TMsgCore::instance() {
  static TMsgCore *theInstance  = 0;
  if (!theInstance) theInstance = new TMsgCore();
  return theInstance;
}

//----------------------------------

TMsgCore::TMsgCore() : m_tcpServer(0), m_clientSocket(0) {}

//----------------------------------

void TMsgCore::OnNewConnection()  // server side
{
  QTcpSocket *socket;
  if (m_tcpServer) socket = m_tcpServer->nextPendingConnection();
  assert(socket);

  bool ret = connect(socket, SIGNAL(readyRead()), SLOT(OnReadyRead()));
  ret = ret && connect(socket, SIGNAL(disconnected()), SLOT(OnDisconnected()));
  assert(ret);
  m_sockets.insert(socket);
}

//----------------------------------------------------------

#define TMSG_PORT 10545

bool TMsgCore::openConnection()  // server side
{
  // QHostAddress host = (remoteConnection) ?
  // QHostAddress::AnyQHostAddress::LocalHost : ;

  if (m_tcpServer != 0 && m_tcpServer->serverAddress() == QHostAddress::Any)
    return true;
  if (m_tcpServer != 0) delete m_tcpServer;

  m_tcpServer = new QTcpServer();
  bool ret =
      connect(m_tcpServer, SIGNAL(newConnection()), SLOT(OnNewConnection()));
  // bool listen =
  // m_tcpServer->listen(QString("tmsg")+QString::number(QCoreApplication::applicationPid()));
  bool listen = m_tcpServer->listen(
      QHostAddress::Any,
      TMSG_PORT);  // QString("tmsg")+QString::number(QCoreApplication::applicationPid()));
  if (!listen) {
    QString err = m_tcpServer->errorString();
  }

  assert(ret && listen);
  return true;
}

//---------------------------

QString TMsgCore::getConnectionName()  // server side
{
  assert(m_tcpServer);
  QString serverName = "pippo";  // m_tcpServer->serverName();
  return serverName;
}

//-----------------------------------------------------------------------

void TMsgCore::OnDisconnected()  // server side
{
  std::set<QTcpSocket *>::iterator it = m_sockets.begin();
  while (it != m_sockets.end()) {
    if ((*it)->state() != QTcpSocket::ConnectedState)
      m_sockets.erase(it++);
    else
      it++;
  }

  // if (m_socketIn)
  // delete m_socketIn;
  // m_socketIn = 0;
}

//------------------------------------------------------------------

void TMsgCore::OnReadyRead()  // server side
{
  std::set<QTcpSocket *>::iterator it = m_sockets.begin();
  for (; it != m_sockets.end(); it++)  // devo scorrerli tutti perche' non so da
                                       // quale socket viene il messaggio...
  {
    if ((*it)->state() == QTcpSocket::ConnectedState &&
        (*it)->bytesAvailable() > 0)
      break;
  }
  if (it != m_sockets.end()) {
    readFromSocket(*it);
    OnReadyRead();
  }
}

//-------------------------------------------------

void TMsgCore::readFromSocket(QTcpSocket *socket)  // server side
{
  static char data[1024];
  static QString prevMessage;
  QString str;
  int byteread;

  while ((byteread = socket->read(data, 1023)) != 0) {
    data[byteread] = '\0';
    str += QString(data);
  }
  QString message = prevMessage + str;
  prevMessage     = QString();
  if (message.isEmpty()) return;

  int lastEnd   = message.lastIndexOf("#END");
  int lastbegin = message.lastIndexOf("#TMSG");
  if (lastbegin == -1 && lastEnd == -1) {
    prevMessage = message;
    return;
  } else if (lastbegin != -1 && lastEnd != -1 && lastEnd < lastbegin) {
    prevMessage = message.right(message.size() - lastbegin);
    message.chop(lastbegin);
  }

  QStringList messages = message.split("#TMSG", QString::SkipEmptyParts);

  for (int i = 0; i < messages.size(); i++) {
    QString str = messages.at(i).simplified();
    str.chop(4);  // rimuovo i "#END" alla fine
    if (str.startsWith("ERROR"))
      DVGui::error(str.right(str.size() - 5));
    else if (str.startsWith("WARNING"))
      DVGui::warning(str.right(str.size() - 7));
    else if (str.startsWith("INFO"))
      DVGui::info(str.right(str.size() - 4));
    else
      assert(false);
  }
}

//------------------------------------------------------

TMsgCore::~TMsgCore() {
  if (m_tcpServer == 0 && m_clientSocket != 0)  // client side
  {
    // m_socketIn->waitForBytesWritten (-1);
    //   m_clientSocket->disconnectFromServer();
    // m_clientSocket->waitForDisconnected();
    delete m_clientSocket;
    m_clientSocket = 0;
  }
}

//----------------------------------

bool TMsgCore::send(DVGui::MsgType type, const QString &message)  // client side
{
  if (receivers(SIGNAL(sendMessage(int, const QString &))) == 0) {
    if (m_clientSocket == 0 ||
        m_clientSocket->state() != QTcpSocket::ConnectedState)
      return false;

    QString socketMessage =
        (type == DVGui::CRITICAL
             ? "#TMSG ERROR "
             : (type == DVGui::WARNING ? "#TMSG WARNING " : "#TMSG INFO ")) +
        message + " #END\n";

#if QT_VERSION >= 0x050000
    m_clientSocket->write(socketMessage.toLatin1());
#else
    m_clientSocket->write(socketMessage.toAscii());
#endif
    m_clientSocket->flush();
    // m_clientSocket->waitForBytesWritten (1000);
  } else
    Q_EMIT sendMessage(type, message);

  return true;
}

//-------------------------------------------------------------------

void TMsgCore::connectTo(const QString &address)  // client side
{
  m_clientSocket = new QTcpSocket();
  m_clientSocket->connectToHost(
      address == "" ? QHostAddress::LocalHost : QHostAddress(address),
      TMSG_PORT);
  // m_clientSocket->connectToServer (connectionName, QIODevice::ReadWrite |
  // QIODevice::Text);
  bool ret = m_clientSocket->waitForConnected(1000);
  if (!ret) {
    // std::cout << "cannot connect to "<< address.toStdString() << std::endl;
    // std::cout << "error "<< m_clientSocket->errorString().toStdString() <<
    // std::endl;
    int err = m_clientSocket->error();
  }
}

//-----------------------------------------------------------------------

void DVGui::MsgBox(MsgType type, const QString &text) {
  TMsgCore::instance()->send(type, text);
}

//-----------------------------------------------------------------------

void DVGui::error(const QString &msg) { DVGui::MsgBox(DVGui::CRITICAL, msg); }

//-----------------------------------------------------------------------------

void DVGui::warning(const QString &msg) { DVGui::MsgBox(DVGui::WARNING, msg); }

//-----------------------------------------------------------------------------

void DVGui::info(const QString &msg) { DVGui::MsgBox(DVGui::INFORMATION, msg); }
