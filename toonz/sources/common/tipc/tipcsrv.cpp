

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

//#define TIPC_DEBUG

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
  m_socket->QObject::disconnect(SIGNAL(readyRead()));

  // Auto-delete this
  delete this;
}

//*******************************************************************************
//    Server implementation
//*******************************************************************************

tipc::Server::Server() : m_lock(false) {
  connect(this, SIGNAL(newConnection()), this, SLOT(onNewConnection()));

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
  QHash<QString, MessageParser *>::iterator it;
  for (it = m_parsers.begin(); it != m_parsers.end(); ++it) delete it.value();
}

//-----------------------------------------------------------------------

void tipc::Server::addParser(MessageParser *parser) {
  m_parsers.insert(parser->header(), parser);
}

//-----------------------------------------------------------------------

void tipc::Server::removeParser(QString header) {
  MessageParser *parser = m_parsers.take(header);
  if (parser) delete parser;
}

//-----------------------------------------------------------------------

void tipc::Server::onNewConnection() {
  tipc_debug(qDebug("new connection"));

  // Accept the connection
  QLocalSocket *socket = nextPendingConnection();

  // Allocate a controller for the socket
  SocketController *controller = new SocketController;
  controller->m_server         = this;
  controller->m_socket         = socket;

  // Connect the controller to the socket's signals
  connect(socket, SIGNAL(readyRead()), controller, SLOT(onReadyRead()));
  connect(socket, SIGNAL(disconnected()), controller, SLOT(onDisconnected()));
  connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
  connect(socket, SIGNAL(error(QLocalSocket::LocalSocketError)), this,
          SLOT(onError(QLocalSocket::LocalSocketError)));
}

//-----------------------------------------------------------------------

void tipc::Server::onError(QLocalSocket::LocalSocketError error) {
  tipc_debug(qDebug() << "Server error #" << error << ": " << errorString());
}

//-----------------------------------------------------------------------

void tipc::Server::dispatchSocket(QLocalSocket *socket) {
  // The lock is established when a message is currently being processed.
  // Returning if the lock is set avoids having recursive message processing;
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

    tipc_debug(qDebug() << header << endl);

    QHash<QString, MessageParser *>::iterator it = m_parsers.find(header);
    if (it == m_parsers.end()) {
      tipc_debug(qDebug() << "Error: Unrecognized command" << endl);
      continue;
    }

    m_lock = true;

    MessageParser *parser = it.value();
    parser->m_socket      = socket;
    parser->m_stream      = &stream;
    parser->operator()(msg);

    m_lock = false;

    // The Message has been read and processed. Send the reply.
    if (msg.ba().size() > 0) stream << msg;
  }
}
