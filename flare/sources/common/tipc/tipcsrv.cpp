

#include "assert.h"

// Qt includes
#include <QDataStream>
#include <QDebug>

#include "tipc.h"
#include "tipcmsg.h"

#include "tipcsrvP.h"
#include "tipcsrv.h"

//*******************************************************************************
//    Diagnostics stuff
//*******************************************************************************

// #define TIPC_DEBUG

#ifdef TIPC_DEBUG
#define tipc_debug(expr) expr
#else
#define tipc_debug(expr)
#endif

#ifdef TIPC_DEBUG
#include <QTime>
#endif

//*******************************************************************************
//    tipc::SocketListener implementation
//*******************************************************************************

void tipc::SocketController::onReadyRead() {
  // Deliver the message to the server for interpretation.
  m_server->dispatchSocket(m_socket);
}

//-----------------------------------------------------------------------

void tipc::SocketController::onDisconnected() {
  // Automatically deleted by parent socket; no deleteLater() needed
}

//*******************************************************************************
//    Server implementation
//*******************************************************************************

tipc::Server::Server() : m_lock(false) {
  connect(this, &QLocalServer::newConnection, this, &Server::onNewConnection);

  // Add default parsers
  addParser(new DefaultMessageParser<SHMEM_REQUEST>);
  addParser(new DefaultMessageParser<SHMEM_RELEASE>);
  addParser(new DefaultMessageParser<TMPFILE_REQUEST>);
  addParser(new DefaultMessageParser<TMPFILE_RELEASE>);
  addParser(new DefaultMessageParser<QUIT_ON_ERROR>);
}

//-----------------------------------------------------------------------

tipc::Server::~Server() {
  // Release parsers
  for (auto it = m_parsers.begin(); it != m_parsers.end(); ++it) {
    delete it.value();
  }
  m_parsers.clear();
}

//-----------------------------------------------------------------------

void tipc::Server::addParser(MessageParser *parser) {
  m_parsers.insert(parser->header(), parser);
}

//-----------------------------------------------------------------------

void tipc::Server::removeParser(QString header) {
  MessageParser *parser = m_parsers.take(header);
  delete parser;  // delete is safe even if parser is nullptr
}

//-----------------------------------------------------------------------

void tipc::Server::onNewConnection() {
  tipc_debug(qDebug("new connection"));

  // Accept the connection
  QLocalSocket *socket = nextPendingConnection();
  if (!socket) return;

  // Controller is parented to the socket and deleted automatically with it
  SocketController *controller =
      new SocketController(socket);  // Pass socket as parent
  controller->m_server = this;
  controller->m_socket = socket;

  connect(socket, &QLocalSocket::readyRead, controller,
          &SocketController::onReadyRead);
  connect(socket, &QLocalSocket::disconnected, controller,
          &SocketController::onDisconnected);
  // Socket will self-delete when disconnected
  connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
  connect(socket, &QLocalSocket::errorOccurred, this, &Server::onError);
}

//-----------------------------------------------------------------------

void tipc::Server::onError(QLocalSocket::LocalSocketError error) {
  tipc_debug(qDebug() << "Server error #" << error << ": " << errorString());
}

//-----------------------------------------------------------------------

void tipc::Server::dispatchSocket(QLocalSocket *socket) {
  // The lock is established when a message is currently being processed.
  // Returning if the lock is set avoids recursive message processing;
  // which is possible if a parser expects further message packets.
  if (m_lock) return;

  tipc::Stream stream(socket);
  QString header;

  while (socket->bytesAvailable() > 0) {
    if (!stream.messageReady()) return;

    Message msg;
    stream >> msg;
    msg >> header;
    assert(!header.isEmpty());

    tipc_debug(qDebug() << header);

    auto it = m_parsers.find(header);
    if (it == m_parsers.end()) {
      tipc_debug(qDebug() << "Error: Unrecognized command");
      continue;
    }

    m_lock = true;

    MessageParser *parser = it.value();
    parser->m_socket      = socket;
    parser->m_stream      = &stream;
    (*parser)(msg);

    m_lock = false;

    // The Message has been read and processed. Send the reply.
    if (!msg.ba().isEmpty()) stream << msg;
  }
}
