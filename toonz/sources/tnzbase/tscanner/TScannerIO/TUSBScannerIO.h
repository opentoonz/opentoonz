#pragma once

#ifndef TUSBSCANNER_IO_H
#define TUSBSCANNER_IO_H

#include "TScannerIO.h"

class TUSBScannerIOPD;

class TUSBScannerIO : public TScannerIO {
  TUSBScannerIOPD *m_data;

public:
  TUSBScannerIO();
  bool open();
  void close();
  int receive(unsigned char *buffer, int size);
  int send(unsigned char *buffer, int size);
  void trace(bool on);
  ~TUSBScannerIO();
};

#endif
