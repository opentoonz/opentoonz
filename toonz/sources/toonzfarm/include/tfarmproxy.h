#pragma once

#ifndef TFARMPROXY_H
#define TFARMPROXY_H

#ifdef TFARMAPI
#undef TFARMAPI
#endif

#include <string>
#include <vector>

#include "texception.h"

#ifdef _WIN32
#ifdef TFARM_EXPORTS
#define TFARMAPI __declspec(dllexport)
#else
#define TFARMAPI __declspec(dllimport)
#endif
#else
#define TFARMAPI
#endif

//------------------------------------------------------------------------------

class TFARMAPI TFarmProxy {
public:
  TFarmProxy(const QString &hostName, const QString &addr, int port)
      : m_hostName(hostName), m_addr(addr), m_port(port) {}

  virtual ~TFarmProxy() {}

  QString sendToStub(const QString &data);
  static int extractArgs(const QString &s, std::vector<QString> &argv);

protected:
  QString m_hostName;
  QString m_addr;
  int m_port;
};

//------------------------------------------------------------------------------

class TFARMAPI TFarmProxyException : public TException {
public:
  TFarmProxyException(const QString &hostname, const QString &addr, int port,
                      const QString &msg)
      : TException(msg.toStdString())
      , m_hostName(hostname)
      , m_address(addr)
      , m_port(port) {}

  QString getHostName() const { return m_hostName; }

  QString getAddress() const { return m_address; }

  int getPort() const { return m_port; }

protected:
  QString m_hostName;
  QString m_address;
  int m_port;
};

//------------------------------------------------------------------------------

class CantConnectToStub final : public TFarmProxyException {
public:
  CantConnectToStub(const QString &hostname, const QString &addr, int port)
      : TFarmProxyException(hostname, addr, port, "") {}

  TString getMessage() const override;
};

#endif
