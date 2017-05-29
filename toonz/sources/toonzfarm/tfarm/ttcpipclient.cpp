

#include "ttcpip.h"
#include "tconvert.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <errno.h> /* obligatory includes */
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifndef _WIN32
#define SOCKET_ERROR -1
#endif

//------------------------------------------------------------------------------

TTcpIpClient::TTcpIpClient() {
#ifdef _WIN32
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(1, 1);
  int irc                = WSAStartup(wVersionRequested, &wsaData);
#endif
}

//------------------------------------------------------------------------------

TTcpIpClient::~TTcpIpClient() {
#ifdef _WIN32
  WSACleanup();
#endif
}

//------------------------------------------------------------------------------

int TTcpIpClient::connect(const QString &hostName, const QString &addrStr,
                          int port, int &sock) {
/*
  if (!addrStr.empty())
  {
    unsigned long ipAddr = inet_addr(addrStr.c_str());
  }
*/
#if QT_VERSION >= 0x050500
  struct hostent *he = gethostbyname(hostName.toUtf8());
#else
  struct hostent *he = gethostbyname(hostName.toAscii());
#endif
  if (!he) {
#ifdef _WIN32
    int err = WSAGetLastError();
#else
#endif
    return HOST_UNKNOWN;
  }

  int socket_id = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset((char *)&addr, 0, sizeof addr);
  addr.sin_family = he->h_addrtype;
  addr.sin_port   = htons(port);
  memcpy((char *)&(addr.sin_addr), he->h_addr, he->h_length);

  int rcConnect = ::connect(socket_id, (struct sockaddr *)&(addr), sizeof addr);
  if (rcConnect == SOCKET_ERROR) {
    sock = -1;

#ifdef _WIN32
    int err = WSAGetLastError();
    switch (err) {
    case WSAECONNREFUSED:
      err = CONNECTION_REFUSED;
      break;
    case WSAETIMEDOUT:
      err = CONNECTION_TIMEDOUT;
      break;
    default:
      err = CONNECTION_FAILED;
      break;
    }

    closesocket(socket_id);
    return err;
#else
    close(socket_id);
    return CONNECTION_FAILED;
#endif
  }

  sock = socket_id;
  return OK;
}

/*
int TTcpIpClient::connect(const std::string &hostName, const std::string
&addrStr, int port, int &sock)
{
  struct hostent *he = gethostbyname (hostName.c_str());
  if (!he)
  {
#ifdef _WIN32
    int err = WSAGetLastError();
#else
#endif
    return HOST_UNKNOWN;
  }

  int socket_id = socket (AF_INET, SOCK_STREAM,0);

  struct sockaddr_in addr;
  memset((char*)&addr, 0, sizeof addr);
  addr.sin_family = he->h_addrtype;
  addr.sin_port = htons(port);
  memcpy((char *)&(addr.sin_addr), he->h_addr, he->h_length);

  int rcConnect = ::connect (socket_id, (struct sockaddr *)&(addr), sizeof
addr);
  if (rcConnect == SOCKET_ERROR)
  {
    sock = -1;
#ifdef _WIN32
    int err = WSAGetLastError();
    switch (err)
    {
     case WSAECONNREFUSED:
        return CONNECTION_REFUSED;
     case WSAETIMEDOUT:
        return CONNECTION_TIMEDOUT;
     default:
        return CONNECTION_FAILED;
    }
#else
    return CONNECTION_FAILED;
#endif
  }

  sock = socket_id;
  return OK;
}
*/

//------------------------------------------------------------------------------

int TTcpIpClient::disconnect(int sock) {
#ifdef _WIN32
  closesocket(sock);
#else
  close(sock);
#endif

  return OK;
}

//------------------------------------------------------------------------------

int TTcpIpClient::send(int sock, const QString &data) {
  std::string dataUtf8 = data.toStdString();

  QString header("#$#THS01.00");
  header += QString::number((int)dataUtf8.size());
  header += "#$#THE";

  std::string packet = header.toStdString() + dataUtf8;

  //  string packet = data;;

  int nLeft = packet.size();
  int idx   = 0;
  while (nLeft > 0) {
#ifdef _WIN32
    int ret = ::send(sock, packet.c_str() + idx, nLeft, 0);
#else
    int ret = write(sock, packet.c_str() + idx, nLeft);
#endif

    if (ret == SOCKET_ERROR) {
// Error
#ifdef _WIN32
      int err = WSAGetLastError();
#else
#endif
      return SEND_FAILED;
    }
    nLeft -= ret;
    idx += ret;
  }

  shutdown(sock, 1);
  return OK;
}

//------------------------------------------------------------------------------

static int readData(int sock, QString &data) {
  int cnt = 0;
  char buff[1024];
  memset(buff, 0, sizeof(buff));

#ifdef _WIN32
  if ((cnt = recv(sock, buff, sizeof(buff), 0)) < 0) {
    int err = WSAGetLastError();
    // GESTIRE L'ERRORE SPECIFICO
    return -1;
  }
#else
  if ((cnt = read(sock, buff, sizeof(buff))) < 0) {
    printf("socket read failure %d\n", errno);
    perror("network server");
    close(sock);
    return -1;
  }
#endif

  if (cnt == 0) return 0;

  std::string aa(buff);
  int x1 = aa.find("#$#THS01.00");
  x1 += sizeof("#$#THS01.00") - 1;
  int x2 = aa.find("#$#THE");

  std::string ssize;
  for (int i = x1; i < x2; ++i) ssize.push_back(buff[i]);

  int size = std::stoi(ssize);

  data = QString(buff + x2 + sizeof("#$#THE") - 1);
  size -= data.size();

  while (size > 0) {
    memset(buff, 0, sizeof(buff));

#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff), 0)) < 0) {
      int err = WSAGetLastError();
      // GESTIRE L'ERRORE SPECIFICO
      return -1;
    }
#else
    if ((cnt = read(sock, buff, sizeof(buff))) < 0) {
      printf("socket read failure %d\n", errno);
      perror("network server");
      close(sock);
      return -1;
    }
#endif
    else if (cnt == 0) {
      break;  // break out of loop
    } else if (cnt < (int)sizeof(buff)) {
      data += QString(buff);
      // break;  // break out of loop
    } else {
      data += QString(buff);
    }

    size -= cnt;
  }

  if (data.size() < size) return -1;

  return 0;
}

#if 0

int readData(int sock, string &data)
{
  int cnt = 0;
  char buff[1024];

  do
  {
    memset (buff,0,sizeof(buff));

#ifdef _WIN32
    if (( cnt = recv(sock, buff, sizeof(buff), 0)) < 0 )
    {
      int err = WSAGetLastError();
      // GESTIRE L'ERRORE SPECIFICO
      return -1;
    }
#else
    if (( cnt = read (sock, buff, sizeof(buff))) < 0 )
    {
      printf("socket read failure %d\n", errno);
      perror("network server");
      close(sock);
      return -1;
    }
#endif
    else
    if (cnt == 0)
      break;  // break out of loop

    data += string(buff);
  }
  while (cnt != 0);  // do loop condition

  return 0;
}

#endif

//#define PRIMA

#ifdef PRIMA

int readData(int sock, string &data) {
  int cnt = 0;
  char buff[1024];

  do {
    memset(buff, 0, sizeof(buff));

#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff), 0)) < 0) {
      int err = WSAGetLastError();
      // GESTIRE L'ERRORE SPECIFICO
      return -1;
    }
#else
    if ((cnt = read(sock, buff, sizeof(buff))) < 0) {
      printf("socket read failure %d\n", errno);
      perror("network server");
      close(sock);
      return -1;
    }
#endif
    else if (cnt == 0) {
      break;  // break out of loop
    } else if (cnt < sizeof(buff)) {
      data += string(buff);
      // break;  // break out of loop
    } else {
      data += string(buff);
    }
  } while (cnt != 0);  // do loop condition

  return 0;
}

#endif

//------------------------------------------------------------------------------

int TTcpIpClient::send(int sock, const QString &data, QString &reply) {
  if (data.size() > 0) {
    int ret           = send(sock, data);
    if (ret == 0) ret = readData(sock, reply);
    return ret;
  }

  return OK;
}

/*
int TTcpIpClient::send(int sock, const std::string &data, string &reply)
{
  int ret = send(sock, data);
  if (ret == 0)
    ret = readData(sock, reply);
  return ret;
}
*/
