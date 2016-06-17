#pragma once

#ifndef TSCANNER_IO_H
#define TSCANNER_IO_H

class TScannerIO {
public:
  TScannerIO() {}
  virtual bool open()  = 0;
  virtual void close() = 0;
  virtual int receive(unsigned char *buffer, int size) = 0;
  virtual int send(unsigned char *buffer, int size)    = 0;
  virtual void trace(bool on) = 0;
  virtual ~TScannerIO() {}
};
#endif
