

#include "tserverproxy.h"
#include <iostream.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <assert.h>

using namespace std;

//=============================================================================

enum {
  OK,

  WSASTARTUP_FAILED,
  HOST_UNKNOWN,
  SOCKET_CREATION_FAILED,
  CONNECTION_FAILED,
  SEND_FAILED,
  RECEIVE_FAILED
};

//=============================================================================

int askServer(string hostName, int portId, string question, string &answer) {
  struct addrinfo *ai = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;

  int suc = getaddrinfo(hostName.c_str(), NULL, &hints, &ai);
  if (suc != 0) return HOST_UNKNOWN;

  int socket_id = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in *ai_addr;
  ai_addr = (struct sockaddr_in *)ai->ai_addr;

  struct sockaddr_in addr;
  memset((char *)&addr, 0, sizeof addr);
  addr.sin_family = ai_addr->sin_family;
  addr.sin_port   = htons(port);
  addr.sin_addr = ai_addr->sin_addr;

  int rcConnect = connect(socket_id, (struct sockaddr *)&(addr), sizeof addr);
  if (rcConnect == SOCKET_ERROR) return CONNECTION_FAILED;

  string message = question;

  int rcSend = send(socket_id, message.c_str(), message.size(), 0);
  if (rcSend == SOCKET_ERROR) return SEND_FAILED;

  char retBuffer[1024];
  memset(&retBuffer, 0, 1024);
  int rcRecv = recv(socket_id, retBuffer, sizeof(retBuffer) - 1, 0);
  if (rcRecv == SOCKET_ERROR) return RECEIVE_FAILED;

  retBuffer[rcRecv] = '\0';

  closesocket(socket_id);

  answer = retBuffer;
  return OK;
}

//=============================================================================

TServerProxy::TServerProxy(const string hostName, int portId)
    : m_hostName(hostName), m_portId(portId) {
#ifdef WIN32
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(1, 1);
  int irc                = WSAStartup(wVersionRequested, &wsaData);
#endif
}

//-----------------------------------------------------------------------------

TServerProxy::~TServerProxy() {
#ifdef WIN32
  WSACleanup();
#endif
}

//-----------------------------------------------------------------------------

bool TServerProxy::testConnection() const {
  addrinfo *ai = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;

  int suc = getaddrinfo(m_hostName.c_str(), NULL, &hints, &ai);
  if (suc != 0) return false;

  int socket_id = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in *ai_addr;
  ai_addr = (struct sockaddr_in *)ai->ai_addr;

  struct sockaddr_in addr;
  memset((char *)&addr, 0, sizeof addr);
  addr.sin_family = ai_addr->sin_family;
  addr.sin_port   = htons(port);
  addr.sin_addr = ai_addr->sin_addr;

  bool ret = (SOCKET_ERROR !=
              connect(socket_id, (struct sockaddr *)&(addr), sizeof addr));
  closesocket(socket_id);
  return ret;
}

//-----------------------------------------------------------------------------

int TServerProxy::exec(int argc, char *argv[], string &reply) {
  string cmd;
  assert(argc > 0);
  if (argc > 0) {
    cmd = argv[0];
    for (int i = 1; i < argc; ++i) {
      cmd += ",";
      cmd += argv[i];
    }
  }

  return askServer(m_hostName, m_portId, cmd, reply);
}
