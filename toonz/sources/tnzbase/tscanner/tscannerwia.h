#pragma once

#ifndef SCANNER_WIA_INCLUDED
#define SCANNER_WIA_INCLUDED

#include "tscanner.h"

#include <string>

class TScannerWia final : public TScanner {
  std::wstring m_deviceId;
  bool m_busy;

public:
  TScannerWia();
  ~TScannerWia() override;

  bool isDeviceAvailable() override;
  bool isDeviceSelected() override;
  void selectDevice() override;
  void updateParameters(TScannerParameters &parameters) override;
  void acquire(const TScannerParameters &param, int paperCount) override;
};

#endif
