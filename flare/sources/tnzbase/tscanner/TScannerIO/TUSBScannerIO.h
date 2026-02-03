#pragma once

#ifndef TUSBSCANNER_IO_H
#define TUSBSCANNER_IO_H

#include "TScannerIO.h"

class TUSBScannerIOPD;

class TUSBScannerIO final : public TScannerIO {
  TUSBScannerIOPD *m_data;

public:
  TUSBScannerIO();
  bool open() override;
  void close() override;
  int receive(unsigned char *buffer, int size) override;
  int send(unsigned char *buffer, int size) override;
  void trace(bool on) override;
  ~TUSBScannerIO();
};

#endif
