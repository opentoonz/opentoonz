

#include "tbaseserver.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#endif

//#include "tcommon.h"

#include <string>
#include <vector>
using namespace std;

//-----------------------------------------------------------------------------

TBaseServer::TBaseServer(int port)
    : m_port(port), m_socketId(-1), m_stopped(false) {}

//-----------------------------------------------------------------------------

TBaseServer::~TBaseServer() {
  if (m_socketId != -1) stop();
}

//-----------------------------------------------------------------------------

void TBaseServer::start() {
#ifdef WIN32
  // Windows Socket startup
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(1, 1);
  int irc                = WSAStartup(wVersionRequested, &wsaData);
  if (irc != 0) throw("Windows Socket Startup failed");
#else
  int c;
  FILE *fp;
  register int ns;
#endif

  if ((m_socketId = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef WIN32
    char szError[1024];
    wsprintf(szError, TEXT("Allocating socket failed. Error: %d"),
             WSAGetLastError());
    throw(szError);
#else
    throw("Allocating socket failed.");
#endif
  }

  struct sockaddr_in myname;
  myname.sin_family      = AF_INET;
  myname.sin_port        = htons(m_port);
  myname.sin_addr.s_addr = INADDR_ANY;

  if (bind(m_socketId, (sockaddr *)&myname, sizeof myname) < 0) {
    throw("Unable to bind to connection");
  }

#ifdef TRACE
  cout << "Listening ..." << endl;
#endif

  // length of the queue of pending connections
  int maxPendingConnections = 5;

  if (listen(m_socketId, maxPendingConnections) < 0) {
    throw("Unable to start listening");
  }

  while (true) {
    register int clientSocket;
    if ((clientSocket = accept(m_socketId, 0, 0)) < 0) {
      if (m_stopped)
        return;
      else
        throw("Error accepting from client");
    }

    printf("ho accettato\n");

    char msgFromClient[1024];
    memset(msgFromClient, 0, sizeof(msgFromClient));

#ifdef WIN32

    // Receive data from the client.
    int iReturn = recv(clientSocket, msgFromClient, sizeof(msgFromClient), 0);

    // Verify that data was received. If yes, use it.
    if (iReturn == SOCKET_ERROR) {
      char szError[1024];
      wsprintf(szError,
               TEXT("No data is received, receive failed.") TEXT(" Error: %d"),
               WSAGetLastError());

      throw(szError);
    }

#else
    fp = fdopen(clientSocket, "r");

    printf("Partito!\n");

    int i = 0;
    while ((int)(c = fgetc(fp)) != EOF) {
      putchar(c);
      msgFromClient[i] = c;
    }
#endif

    printf("msg finito\n");

    char *argv[128];
    int argc = extractArgs(msgFromClient, argv);

    string reply = exec(argc, argv);
    reply += "\n";

    for (int i = 0; i < argc; ++i) delete[] argv[i];

    // Send the reply string from the server to the client.
    if (send(clientSocket, reply.c_str(), reply.length() + 1, 0) < 0) {
      char szError[1024];

#ifdef WIN32
      wsprintf(szError, TEXT("Sending data to the client failed. Error: %d"),
               WSAGetLastError());
#endif

      throw(szError);
    }

#ifndef WIN32
    close(clientSocket);
#endif
  }
}

//-----------------------------------------------------------------------------

void TBaseServer::stop() {
  //  TThread::ScopedLock sl(m_mutex);

  if (!m_stopped) {
    m_stopped = true;

#ifdef WIN32
    if (closesocket(m_socketId) != 0) {
      char szError[1024];
      wsprintf(szError, TEXT(" Error: %d"), WSAGetLastError());

      throw(szError);
    }
#else
    close(m_socketId);
#endif

    m_socketId = -1;
  }
}

//-----------------------------------------------------------------------------

int TBaseServer::extractArgs(char *s, char *argv[]) {
  string ss(s);

  string::size_type pos1 = 0;
  string::size_type pos2 = ss.find(",");

  vector<string> args;
  while (pos2 != string::npos) {
    string arg = ss.substr(pos1, pos2 - pos1);
    args.push_back(arg);

    pos1 = pos2 + 1;
    pos2 = ss.find(",", pos1);
  }

  string arg = ss.substr(pos1, ss.length() - pos1);
  args.push_back(arg);

  vector<string>::iterator it = args.begin();
  for (int i = 0; it != args.end(); ++it, ++i) {
    string arg = *it;
    argv[i]    = new char[arg.size() + 1];
    strcpy(argv[i], arg.c_str());
  }

  return args.size();
}
