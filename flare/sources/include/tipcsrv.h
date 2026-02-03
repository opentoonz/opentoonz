#pragma once

#ifndef TIPC_SERVER_H
#define TIPC_SERVER_H

// Toonz includes
#include "tcommon.h"

// STL includes
#include <map>

// Qt includes
#include <QString>
#include <QHash>
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace tipc {

class MessageParser;

//*******************************************************************************
//    Server declaration
//*******************************************************************************

/*!
  The tipc::Server class is the base server class for inter-process
  communication
  in Toonz-related applications.

  A tipc::Server is a specialized QLocalServer which stores
  header/message-callback
  associations to perform message parsing.
*/

class DVAPI Server final : public QLocalServer {
  Q_OBJECT

  QHash<QString, MessageParser *> m_parsers;
  bool m_lock;

public:
  Server();
  ~Server();

  void addParser(MessageParser *parser);
  void removeParser(QString header);

  //! Generic dispatcher function for socket messages.
  //! Acceptable socket messages are composed of a header and a body part.
  //! The header part, containing an explanation of the message's body, is
  //! the first line of the message, and is expected to be at max 1024 chars
  //! long.
  //! Depending on the header content, the rest of the message is read in
  //! specialized message handler functions.
  void dispatchSocket(QLocalSocket *socket);

public Q_SLOTS:

  //! Receives a client connection to the server and prepares a socket for the
  //! connection.
  void onNewConnection();
  void onError(QLocalSocket::LocalSocketError);
};

}  // namespace tipc

#endif  // TIPC_SERVER_H
