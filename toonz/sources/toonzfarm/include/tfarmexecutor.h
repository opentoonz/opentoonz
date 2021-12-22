#pragma once

#ifndef TFARMEXECUTOR_H
#define TFARMEXECUTOR_H

#include "ttcpip.h"

#include <string>

//==============================================================================

#ifdef TFARMAPI
#undef TFARMAPI
#endif

#ifdef _WIN32
#ifdef TFARM_EXPORTS
#define TFARMAPI __declspec(dllexport)
#else
#define TFARMAPI __declspec(dllimport)
#endif
#else
#define TFARMAPI
#endif

//==============================================================================

class TFARMAPI TFarmExecutor : public TTcpIpServer {
public:
  TFarmExecutor(int port);
  virtual ~TFarmExecutor() {}

  // TTcpIpServer overrides
  void onReceive(int socket, const QString &data) override;

protected:
  virtual QString execute(const std::vector<QString> &argv) = 0;

private:
  TTcpIpServer *m_tcpipServer;
};

#endif
