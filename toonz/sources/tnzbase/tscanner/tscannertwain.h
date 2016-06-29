#pragma once

#ifndef SCANNER_TWAIN_INCLUDED
#define SCANNER_TWAIN_INCLUDED

#include "tscanner.h"

class TScannerTwain final : public TScanner {
public:
  TScannerTwain();
  ~TScannerTwain();

  bool isDeviceAvailable() override;
  bool isDeviceSelected() override;
  void selectDevice() override;

  void updateParameters(TScannerParameters &) override;  // vedi TScanner
  void acquire(const TScannerParameters &param, int paperCount) override;
  bool isAreaSupported();
};

#endif
