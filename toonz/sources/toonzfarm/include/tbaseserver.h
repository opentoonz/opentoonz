#pragma once

#ifndef TBASESERVER_H
#define TBASESERVER_H

#include <string>
//#include "tthread.h"

//---------------------------------------------------------------------

class TBaseServer {
public:
  TBaseServer(int port);
  virtual ~TBaseServer();

  void start();
  void stop();

  virtual std::string exec(int argc, char *argv[]) = 0;

private:
  int extractArgs(char *s, char *argv[]);

  int m_port;
  int m_socketId;
  bool m_stopped;

  //  TThread::Mutex m_mutex;
};

#endif
