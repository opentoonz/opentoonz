#pragma once

#ifndef TIPC_SERVER_PRIVATE_H
#define TIPC_SERVER_PRIVATE_H

#include <QObject>
#include <QLocalSocket>

namespace tipc {

//------------------------------------------------------------------

//  Forward declarations

class Server;

//------------------------------------------------------------------

class SocketController final : public QObject {
  Q_OBJECT

public:
  Server *m_server;
  QLocalSocket *m_socket;

public Q_SLOTS:

  void onReadyRead();
  void onDisconnected();
};

}  // namespace tipc

#endif  // TIPC_SERVER_PRIVATE_H
