#pragma once

#ifndef TSCANNER_EPSON_INCLUDED
#define TSCANNER_EPSON_INCLUDED

#include "tscanner.h"
#include "TScannerIO/TScannerIO.h"

#include <memory>

/*
PLEASE DO NOT REMOVE unreferenced methods... they are useful for debugging  :)
max
*/

class TScannerEpson final : public TScanner {
  enum SettingsMode { OLD_STYLE, NEW_STYLE };

  TScannerIO *m_scannerIO;
  bool m_hasADF;
  bool m_isOpened;
  SettingsMode m_settingsMode;

public:
  TScannerEpson();
  ~TScannerEpson();

  void selectDevice() override;
  bool isDeviceAvailable() override;
  bool isDeviceSelected() override;
  void updateParameters(TScannerParameters &param) override;  // vedi TScanner
  void acquire(const TScannerParameters &params, int paperCount) override;
  bool isAreaSupported();
  void closeIO();

private:
  void doSettings(const TScannerParameters &params, bool isFirstSheet);
  void collectInformation(char *lev0, char *lev1, unsigned short *lowRes,
                          unsigned short *hiRes, unsigned short *hMax,
                          unsigned short *vMax);
  bool resetScanner();
  void reportError(std::string errMsg);  // debug only

  char *ESCI_inquiry(char cmd); /* returns 0 if failed */
  bool ESCI_command(char cmd, bool checkACK);
  bool ESCI_command_1b(char cmd, unsigned char p0, bool checkACK);
  bool ESCI_command_2b(char cmd, unsigned char p0, unsigned char p1,
                       bool checkACK);
  bool ESCI_command_2w(char cmd, unsigned short p0, unsigned short p1,
                       bool checkACK);
  bool ESCI_command_4w(char cmd, unsigned short p0, unsigned short p1,
                       unsigned short p2, unsigned short p3, bool checkACK);

  std::unique_ptr<unsigned char[]> ESCI_read_data2(unsigned long &size);
  void ESCI_readLineData(unsigned char &stx, unsigned char &status,
                         unsigned short &counter, unsigned short &lines,
                         bool &areaEnd);
  void ESCI_readLineData2(unsigned char &stx, unsigned char &status,
                          unsigned short &counter);

  int receive(unsigned char *buffer, int size);
  int send(unsigned char *buffer, int size);

  bool ESCI_doADF(bool on);
  int sendACK();
  bool expectACK();
  void scanArea2pix(const TScannerParameters &params, unsigned short &offsetx,
                    unsigned short &offsety, unsigned short &dimlx,
                    unsigned short &dimly, const TRectD &scanArea);
};

#endif
