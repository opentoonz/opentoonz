

#include "tconvert.h"
#include "tfarmproxy.h"
#include "ttcpip.h"
#include <QStringList>

//------------------------------------------------------------------------------

QString TFarmProxy::sendToStub(const QString &data) {
  TTcpIpClient client;

  int sock;
  int ret = client.connect(m_hostName, m_addr, m_port, sock);
  if (ret != OK) {
    throw CantConnectToStub(m_hostName, m_addr, m_port);
  }

  QString reply;
  ret = client.send(sock, data, reply);
  if (ret != OK) {
    client.disconnect(sock);
    throw CantConnectToStub(m_hostName, m_addr, m_port);
  }

  client.disconnect(sock);
  return reply;
}

//------------------------------------------------------------------------------

int TFarmProxy::extractArgs(const QString &s, std::vector<QString> &argv) {
  argv.clear();
  if (s == "") return 0;

  QStringList sl = s.split(',');
  int i;
  for (i = 0; i < sl.size(); i++) argv.push_back(sl.at(i));

  return argv.size();
}

//==============================================================================

TString CantConnectToStub::getMessage() const {
  std::string msg("Unable to connect to ");
  msg += m_hostName.toStdString();
  msg += " on port ";
  msg += std::to_string(m_port);

  return ::to_wstring(msg);
}
