#pragma once

#ifndef TTCPIP_H
#define TTCPIP_H

#include <memory>

#include "tcommon.h"

#include <QString>
#include <QThread>

//---------------------------------------------------------------------

#ifdef TFARMAPI
#undef TFARMAPI
#endif

#ifdef WIN32
#ifdef TFARM_EXPORTS
#define TFARMAPI __declspec(dllexport)
#else
#define TFARMAPI __declspec(dllimport)
#endif
#else
#define TFARMAPI
#endif

//---------------------------------------------------------------------

class TTcpIpServerImp;

class TFARMAPI TTcpIpServer : public QThread {
  int m_exitCode;

public:
  TTcpIpServer(int port);
  virtual ~TTcpIpServer();

  int getPort() const;

  void run();
  int shutdown();

  virtual void onReceive(int socket, const QString &data) = 0;

  void sendReply(int socket, const QString &reply);

  int getExitCode() const;

private:
  std::shared_ptr<TTcpIpServerImp> m_imp;
};

//---------------------------------------------------------------------

enum {
  OK,
  STARTUP_FAILED,
  HOST_UNKNOWN,
  SOCKET_CREATION_FAILED,
  CONNECTION_FAILED,
  CONNECTION_REFUSED,
  CONNECTION_TIMEDOUT,
  SEND_FAILED,
  RECEIVE_FAILED
};

class TFARMAPI TTcpIpClient {
public:
  TTcpIpClient();
  ~TTcpIpClient();

  int connect(const QString &name, const QString &addr, int port, int &sock);
  int disconnect(int sock);

  int send(int sock, const QString &data);
  int send(int sock, const QString &data, QString &reply);
};

#endif
