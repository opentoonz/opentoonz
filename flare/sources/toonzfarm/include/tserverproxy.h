#pragma once

#ifndef TSERVERPROXY_H
#define TSERVERPROXY_H

#include <string>

//-----------------------------------------------------------------------------

class TServerProxy {
public:
  TServerProxy(const std::string hostName, int portId);
  ~TServerProxy();

  bool testConnection() const;
  int exec(int argc, char *argv[], std::string &reply);

private:
  std::string m_hostName;
  int m_portId;
};

#endif
