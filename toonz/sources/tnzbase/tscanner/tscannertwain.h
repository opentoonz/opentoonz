#pragma once

#ifndef SCANNER_TWAIN_INCLUDED
#define SCANNER_TWAIN_INCLUDED

#include "tscanner.h"

class TScannerTwain : public TScanner {
public:
  TScannerTwain();
  ~TScannerTwain();

  bool isDeviceAvailable();
  bool isDeviceSelected();
  void selectDevice();

  void updateParameters(TScannerParameters &);  // vedi TScanner
  void acquire(const TScannerParameters &param, int paperCount);
  bool isAreaSupported();
};

#endif
