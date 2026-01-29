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
  // Constructor with optional parent
  explicit SocketController(QObject *parent = nullptr)
      : QObject(parent), m_server(nullptr), m_socket(nullptr) {}

  Server *m_server;
  QLocalSocket *m_socket;

public Q_SLOTS:
  void onReadyRead();
  void onDisconnected();
};

}  // namespace tipc

#endif  // TIPC_SERVER_PRIVATE_H
