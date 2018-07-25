

#include <errno.h>
#include "texception.h"
#include "tscanner.h"
#include "tscannerepson.h"
#include "tscannerutil.h"
#include "tsystem.h"
#include "tconvert.h"
#include "trop.h"

#include "TScannerIO/TUSBScannerIO.h"

#include <cassert>
#include <memory>
#include <fstream>
#include <sstream>

using namespace TScannerUtil;

static void sense(bool) {}
static int scsi_maxlen() {
  assert(0);
  return 0;
}

#define BW_USES_GRAYTONES

#ifndef _WIN32
#define SWAPIT
#endif

#ifdef i386
#undef SWAPIT
#endif
/*
Commands used to drive the scanner:

cmd   spec                    Level
@     Reset                B2 B3 B4 B5 A5
Q     Sharpness                  B4 B5 A5
B     Halftoning        B1 B2 B3 B4 B5 A5
Z     Gamma                B2 B3 B4 B5 A5
L     Brightness           B2 B3 B4 B5 A5
t     Threshold
H     Zoom                 B2 B3 B4 B5 A5
R     Resolution        B1 B2 B3 B4 B5 A5
A     Set read Area     B1 B2 B3 B4 B5 A5
C     Linesequence Mode B1 B2 B3 B4 B5 A5
d     Line count                 B4 B5 A5
D     Dataformat        B1 B2 B3 B4 B5 A5
g     Scan speed                 B4 B5 A5
G     Start Scan        B1 B2 B3 B4 B5


e     Activate ADF
0x19  Load from ADF
0x0C  Unload from ADF

*/

/* NOTE: you can find these codes with "man ascii". */
static unsigned char STX = 0x02;
static unsigned char ACK = 0x06;
static unsigned char NAK = 0x15;
static unsigned char CAN = 0x18;
static unsigned char ESC = 0x1B;
static unsigned char PF  = 0x19;

/* STATUS bit in readLineData */
static unsigned char FatalError = 1 << 7;
static unsigned char NotReady   = 1 << 6;

//-----------------------------------------------------------------------------

#define log

class TScannerExpection final : public TException {
  TString m_scannerMsg;

public:
  TScannerExpection(const std::vector<std::string> &notFatal,
                    const std::string &fatal)
      : TException("Scanner Expection") {
    m_scannerMsg = ::to_wstring(fatal);
    for (int i = notFatal.size(); i; i--)
      m_scannerMsg += L"\n" + ::to_wstring(notFatal[i - 1]);
    log("Exception created: " + ::to_string(m_scannerMsg));
  }
  TString getMessage() const override { return m_scannerMsg; }
};

//-----------------------------------------------------------------------------

namespace {
/*
void log(string s)
{
  std::ofstream os("C:\\butta.txt", std::ios::app);
  os << s << std::endl;
  os.flush();
}
*/
}

//-----------------------------------------------------------------------------

TScannerEpson::TScannerEpson()
    : m_scannerIO(new TUSBScannerIO()), m_hasADF(false), m_isOpened(false) {
  log("Created");
}

//-----------------------------------------------------------------------------

void TScannerEpson::closeIO() {
  log("CloseIO.");
  if (m_scannerIO && m_isOpened) m_scannerIO->close();
  m_isOpened = false;
}

//-----------------------------------------------------------------------------

TScannerEpson::~TScannerEpson() {
  closeIO();
  log("Destroyed");
}

//-----------------------------------------------------------------------------

void TScannerEpson::selectDevice() {
  log(std::string("selectDevice; isOpened=") + (m_isOpened ? "true" : "false"));
  if (!m_scannerIO->open()) {
    log("open() failed");
    throw TException("unable to get handle to scanner");
  }
  m_isOpened = true;

  /*
char lev0, lev1;
unsigned short lowRes, hiRes, hMax,vMax;
collectInformation(&lev0, &lev1, &lowRes, &hiRes, &hMax, &vMax);
string version = toString(lev0) + "." + toString(lev1);
*/
  setName("Scanner EPSON (Internal driver)");
}

//-----------------------------------------------------------------------------

bool TScannerEpson::isDeviceAvailable() { return true; }

//-----------------------------------------------------------------------------

bool TScannerEpson::isDeviceSelected() { return m_isOpened; }

//-----------------------------------------------------------------------------

void TScannerEpson::updateParameters(TScannerParameters &parameters) {
  log("updateParameters()");
  char lev0, lev1;
  unsigned short lowRes, hiRes, hMax, vMax;
  collectInformation(&lev0, &lev1, &lowRes, &hiRes, &hMax, &vMax);
  log("collected info. res = " + std::to_string(lowRes) + "/" +
      std::to_string(hiRes));

  // non supportiamo black & white
  parameters.setSupportedTypes(true, true, true);

  // param.m_scanArea = TRectD(0, 0, 0, 0);
  parameters.setMaxPaperSize((25.4 * hMax) / (float)hiRes,
                             (25.4 * vMax) / (float)hiRes);
  parameters.updatePaperFormat();

  // cambio range, default, step, e supported. aggiorno, se necessario, il value
  // (n.b. non dovrebbe succedere
  // mai, perche' i range sono sempre gli stessi su tutti gli scanner
  TScanParam defaultEpsonParam(0., 255., 128., 1.);
  parameters.m_brightness.update(defaultEpsonParam);
  parameters.m_contrast.update(defaultEpsonParam);
  parameters.m_threshold.update(defaultEpsonParam);

  if (m_hasADF) {
    TScanParam defaultPaperFeederParam(0., 1., 0., 1.);
    parameters.m_paperFeeder.update(defaultPaperFeederParam);
  } else
    parameters.m_paperFeeder.m_supported = false;

  // cerco un default per il dpi. il piu' piccolo possibile nella forma 100+k50
  float defaultDpi = 100;
  while (defaultDpi < lowRes) defaultDpi += 50;

  TScanParam defaultDpiParam(lowRes, hiRes, defaultDpi,
                             hiRes > lowRes ? 1.F : 0.F);
  parameters.m_dpi.update(defaultDpiParam);

  //  parameters.m_twainVersion = "NATIVE";
  //  parameters.m_manufacturer = "EPSON";

  //  parameters.m_version = toString(lev0) + "." + toString(lev1);

  log("end updateParameters()");
}

//-----------------------------------------------------------------------------

void TScannerEpson::acquire(const TScannerParameters &params, int paperCount) {
  log("acquire");
  TRectD scanArea = params.getScanArea();
  if (scanArea.isEmpty())
    throw TException("Scan area is empty, select a paper size");

  /*
if ((scanArea.getSize().lx > params.m_maxPaperSize.lx) ||
(scanArea.getSize().ly > params.m_maxPaperSize.ly))
throw TException("Scan area too large, select a correct paper size");
*/

  for (int i = 0; i < paperCount; ++i) {
    log("paper " + std::to_string(i));
#ifdef _DEBUG
    m_scannerIO->trace(true);
#endif
    doSettings(params, i == 0);
    unsigned int nTimes = 0;
    unsigned int bytes_to_read;
    bool rc;
    unsigned char stx;

#ifdef _DEBUG
    m_scannerIO->trace(false);
#endif

    TRasterP ras, rasBuffer;
    unsigned char *buffer = 0;
    log("Scan command");
    rc = ESCI_command('G', false); /* 'SCAN' command */
    if (!rc) throw TException("Start Scan failed");
    log("Scan command OK");

    unsigned short offsetx = 0, offsety = 0;
    unsigned short dimlx = 0, dimly = 0;

    TRectD scanArea =
        params.isPreview() ? params.getScanArea() : params.getCropBox();
    scanArea2pix(params, offsetx, offsety, dimlx, dimly, scanArea);
    std::swap(dimlx, dimly);

    unsigned int bytes;

    switch (params.getScanType()) {
    case TScannerParameters::BW:
#ifdef BW_USES_GRAYTONES
      ras       = TRasterGR8P(dimlx, dimly);
      bytes     = dimlx * dimly;
      rasBuffer = TRasterGR8P(dimlx, dimly);
      buffer    = rasBuffer->getRawData();
      break;
      break;
#else
      ras       = TRasterGR8P(dimlx, dimly);
      bytes     = tceil(dimlx / 8) * dimly;
      rasBuffer = TRasterGR8P(tceil(dimlx / 8), dimly);
      // bytes = (dimlx + 7) % 8 * dimly;
      // rasBuffer = TRasterGR8P((dimlx + 7) % 8,dimly);
      buffer = rasBuffer->getRawData();
      break;
#endif
    case TScannerParameters::GR8:

#ifdef GRAYTONES_USES_RGB
      ras       = TRasterGR8P(dimlx, dimly);
      bytes     = dimlx * dimly * 3;
      rasBuffer = TRasterGR8P(dimlx * 3, dimly);
      buffer    = rasBuffer->getRawData();
      break;
#else
      ras       = TRasterGR8P(dimlx, dimly);
      bytes     = dimlx * dimly;
      rasBuffer = TRasterGR8P(dimlx, dimly);
      buffer    = rasBuffer->getRawData();
      break;
#endif

    case TScannerParameters::RGB24:
      ras       = TRaster32P(dimlx, dimly);
      bytes     = dimlx * dimly * 3;
      rasBuffer = TRasterGR8P(dimlx * 3, dimly);
      buffer    = rasBuffer->getRawData();
      break;

    default:
      throw TException("Unknown scanner mode");
      break;
    }

    log("sleep");
    TSystem::sleep(2000);
    log("reading data");
    bool areaEnd            = false;
    unsigned int bytes_read = 0;
    while (!areaEnd) {
      unsigned short lines, counter;
      unsigned char status    = 0;
      unsigned int retryCount = 0;
      do {
        ESCI_readLineData(stx, status, counter, lines, areaEnd);
        if (status & FatalError) {
          retryCount++;
          if (retryCount == 1) throw TException("Error scanning");
        }
      } while (status & FatalError);

      bytes_to_read = lines * counter;
      if (stx != 0x02) {
        std::stringstream os;
        os << "header corrupted (" << std::hex << stx << ")" << '\0';
        throw TException(os.str());
      }
      unsigned long reqBytes                      = bytes_to_read;
      std::unique_ptr<unsigned char[]> readBuffer = ESCI_read_data2(reqBytes);
      if (!readBuffer) throw TException("Error reading image data");

      log("readline: OK");
      /*
if (params.getScanType() == TScannerParameters::BW )
{
for (unsigned int i = 0; i< reqBytes;i++)
readBuffer[i] = ~readBuffer[i];
}
*/
      memcpy(buffer + bytes_read, readBuffer.get(), reqBytes);
      bytes_read += reqBytes;
      nTimes++;
      if (bytes_read == bytes) break; /* I've read enough bytes */
      if (!(stx & 0x20)) sendACK();
    }
    log("read: OK");
    { /*
FILE *myFile = fopen("C:\\temp\\prova.dmp", "w+");
fwrite(buffer, 1, bytes_to_read, myFile);
fclose(myFile);*/
    }
    if (params.m_paperFeeder.m_supported &&
        (params.m_paperFeeder.m_value == 1.)) {
      log("Advance paper");
      if (m_settingsMode == OLD_STYLE) ESCI_doADF(0);
      log("Advance paper: OK");
    }
    switch (params.getScanType()) {
    case TScannerParameters::BW:
#ifdef BW_USES_GRAYTONES
      copyGR8BufferToTRasterBW(buffer, dimlx, dimly, ras, true,
                               params.m_threshold.m_value);
      // copyGR8BufferToTRasterGR8(buffer, dimlx, dimly, ras, true);
      break;
#else
      copyBWBufferToTRasterGR8(buffer, dimlx, dimly, ras, true, true);
      break;
#endif
    case TScannerParameters::GR8:
#ifdef GRAYTONES_USES_RGB
      copyRGBBufferToTRasterGR8(buffer, dimlx, dimly, dimlx, ras);
      break;
#else
      copyGR8BufferToTRasterGR8(buffer, dimlx, dimly, ras, true);
      break;
#endif

    case TScannerParameters::RGB24:
      copyRGBBufferToTRaster32(buffer, dimlx, dimly, ras, true);
      break;

    default:
      throw TException("Unknown scanner mode");
      break;
    }

    if (params.getCropBox() != params.getScanArea() && !params.isPreview()) {
      TRect scanRect(
          TPoint(offsetx, offsety),
          TDimension(dimly, dimlx));  // dimLx and ly was has been swapped
      scanArea2pix(params, offsetx, offsety, dimlx, dimly,
                   params.getScanArea());
      TRasterP app = ras->create(dimly, dimlx);
      TRaster32P app32(app);
      TRasterGR8P appGR8(app);
      if (app32)
        app32->fill(TPixel32::White);
      else if (appGR8)
        appGR8->fill(TPixelGR8::White);
      app->copy(ras, TPoint(dimly - scanRect.y1 - 1, dimlx - scanRect.x1 - 1));
      ras = app;
    }
    TRasterImageP rasImg(ras);
    if (ras) {
      rasImg->setDpi(params.m_dpi.m_value, params.m_dpi.m_value);
      rasImg->setSavebox(ras->getBounds());
    }
    log("notifying");
    notifyImageDone(rasImg);
    if (!(params.m_paperFeeder.m_value == 1.) ||
        params.isPreview())  // feeder here!
    {
      if ((paperCount - i) > 1) notifyNextPaper();
    } else
      notifyAutomaticallyNextPaper();
    if (isScanningCanceled()) break;
  }

  /*Unload Paper if need*/

  if ((m_settingsMode == NEW_STYLE) && params.m_paperFeeder.m_supported &&
      (params.m_paperFeeder.m_value == 1.)) {
    if (!ESCI_command_1b('e', 1, true)) {
      std::vector<std::string> notFatal;
      throw TScannerExpection(notFatal, "Scanner error (un)loading paper");
    }
    unsigned char p = 0x0C;
    bool status     = true;
    send(&p, 1);
    status = expectACK();
  }

  log("acquire OK");
}

//-----------------------------------------------------------------------------

bool TScannerEpson::isAreaSupported() { return true; }

//-----------------------------------------------------------------------------

inline unsigned char B0(const unsigned int v) { return (v & 0x000000ff); }
inline unsigned char B1(const unsigned int v) { return (v & 0x0000ff00) >> 8; }
inline unsigned char B2(const unsigned int v) { return (v & 0x00ff0000) >> 16; }
inline unsigned char B3(const unsigned int v) { return (v & 0xff000000) >> 24; }

void TScannerEpson::doSettings(const TScannerParameters &params,
                               bool firstSheet) {
  log("doSettings");
  int retryCount = 3;
  std::vector<std::string> notFatal;

  while (retryCount--) {
    if (m_settingsMode == NEW_STYLE) {
      if (firstSheet)
        if (!resetScanner()) {
          notFatal.push_back("Scanner error resetting");
          continue;
        }
    } else {
      if (!resetScanner()) {
        notFatal.push_back("Scanner error resetting");
        continue;
      }
    }

    unsigned char cmd[64];
    memset(&cmd, 0, 64);

    unsigned char brightness = 0x00;
    float bv                 = params.m_brightness.m_value;

    if (bv >= 0 && bv < 43) brightness    = 0xFD;
    if (bv >= 43 && bv < 86) brightness   = 0xFE;
    if (bv >= 86 && bv < 128) brightness  = 0xFF;
    if (bv >= 128 && bv < 171) brightness = 0x00;
    if (bv >= 171 && bv < 214) brightness = 0x01;
    if (bv >= 214 && bv < 255) brightness = 0x02;
    if (bv == 255) brightness             = 0x03;

    unsigned short dpi = (unsigned short)params.m_dpi.m_value;

    unsigned short offsetx, offsety;
    unsigned short sizelx, sizely;
    TRectD scanArea =
        params.isPreview() ? params.getScanArea() : params.getCropBox();
    scanArea2pix(params, offsetx, offsety, sizelx, sizely, scanArea);

    // the main scan resolution (ESC R)
    cmd[0] = B0(dpi);
    cmd[1] = B1(dpi);
    cmd[2] = B2(dpi);
    cmd[3] = B3(dpi);

    // the sub scan resolution (ESC R)
    cmd[4] = B0(dpi);
    cmd[5] = B1(dpi);
    cmd[6] = B2(dpi);
    cmd[7] = B3(dpi);

    // skipping length of main scan (ESC A)
    cmd[8]  = B0(offsetx);
    cmd[9]  = B1(offsetx);
    cmd[10] = B2(offsetx);
    cmd[11] = B3(offsetx);

    // skipping length of sub scan (ESC A)
    cmd[12] = B0(offsety);
    cmd[13] = B1(offsety);
    cmd[14] = B2(offsety);
    cmd[15] = B3(offsety);

    // the length of main scanning (ESC A)
    cmd[16] = B0(sizelx);
    cmd[17] = B1(sizelx);
    cmd[18] = B2(sizelx);
    cmd[19] = B3(sizelx);

    // the length of sub scanning (ESC A)
    cmd[20] = B0(sizely);
    cmd[21] = B1(sizely);
    cmd[22] = B2(sizely);
    cmd[23] = B3(sizely);

#ifdef GRAYTONES_USES_RGB
    cmd[24] = 0x13;  // scanning color (ESC C)
    cmd[25] = 0x08;  // data format (ESC D)
#else
    switch (params.getScanType()) {
    case TScannerParameters::BW:
      cmd[24] = 0x00;  // scanning BW/Gray (ESC C)
#ifdef BW_USES_GRAYTONES
      cmd[25] = 0x08;  // data format (ESC D)
#else
      cmd[25] = 0x01;  // data format (ESC D)
      break;
#endif
    case TScannerParameters::GR8:
      cmd[24] = 0x00;  // scanning BW/Gray (ESC C)
      cmd[25] = 0x08;  // data format (ESC D)
      break;

    case TScannerParameters::RGB24:
      cmd[24] = 0x13;  // scanning color (ESC C)
      cmd[25] = 0x08;  // data format (ESC D)
      break;

    default:
      throw TException("Unknown scanner mode");
      break;
    }

//  cmd[24] = (params.getScanType() == TScannerParameters::RGB24)?0x13:0x00;
//  //scanning color (ESC C)
//  cmd[25] = (params.getScanType() == TScannerParameters::RGB24)?0x08:0x08; //
//  data format (ESC D)
#endif

    if (m_settingsMode == NEW_STYLE)
      cmd[26] = 0;
    else
      cmd[26] = (params.m_paperFeeder.m_supported &&
                 (params.m_paperFeeder.m_value == 1.))
                    ? 0x01
                    : 0x00;  // option control (ESC e)
    cmd[27] = 0x00;          // scanning mode (ESC g)
    cmd[28] = 0xFF;          // the number of block line (ESC d)
    cmd[29] = 0x02;          // gamma (ESC Z)
    cmd[30] = brightness;    //  brightness (ESC L)
    cmd[31] = 0x80;          // color collection (ESC M)
    cmd[32] = 0x01;          // half toning processing (ESC B)
    cmd[33] = (unsigned char)params.m_threshold.m_value;  // threshold (ESC t)
    cmd[34] = 0x00;  // separate of area (ESC s)
    cmd[35] = 0x01;  // sharpness control (ESC Q)
    cmd[36] = 0x00;  // mirroring (ESC K)
    cmd[37] = 0x00;  // set film type (ESC N)
                     // other bytes should be set to 0x00 !

    if (m_settingsMode == NEW_STYLE)
      if (params.m_paperFeeder.m_supported &&
          (params.m_paperFeeder.m_value == 1.)) {
        unsigned char v = (params.m_paperFeeder.m_value == 1.) ? 0x01 : 0x00;
        if (!ESCI_command_1b('e', v, true))
          throw TScannerExpection(notFatal, "Scanner error (un)loading paper");
        if (v) {
          unsigned char p = 0x19;
          bool status     = true;
          send(&p, 1);
          status = expectACK();
        }
      }

    unsigned char setParamCmd[2] = {0x1C, 0x57};
    int wrote                    = send(&setParamCmd[0], 2);
    if (wrote != 2)
      throw TScannerExpection(notFatal,
                              "Error setting scanner parameters - W -");

    if (!expectACK()) {
      notFatal.push_back("Error setting scanner parameters - NAK on W -");
      continue;
    }
    wrote = send(&cmd[0], 0x40);
    if (wrote != 0x40)
      throw TScannerExpection(notFatal,
                              "Error setting scanner parameters - D -");

    if (!expectACK()) {
      notFatal.push_back("Error setting scanner parameters - NAK on D -");
      continue;
    }
    // if here, everything is ok, exit from loop
    break;
  }
  if (retryCount <= 0)
    throw TScannerExpection(
        notFatal, "Error setting scanner parameters, too many retries");
  log("doSettings:OK");
}

//-----------------------------------------------------------------------------

void TScannerEpson::collectInformation(char *lev0, char *lev1,
                                       unsigned short *lowRes,
                                       unsigned short *hiRes,
                                       unsigned short *hMax,
                                       unsigned short *vMax) {
  log("collectInformation");
  unsigned char stx;
  int pos = 0;
  unsigned short counter;
  unsigned char status;

  /*
if (!resetScanner())
throw TException("Scanner error resetting");
*/
  if (!ESCI_command('I', false))
    throw TException("Unable to get scanner info. Is it off ?");

  unsigned long s = 4;  // 4 bytes cfr Identity Data Block on ESCI Manual!!!
  std::unique_ptr<unsigned char[]> buffer2 = ESCI_read_data2(s);
  if (!buffer2 || (s != 4)) throw TException("Error reading scanner info");

  memcpy(&stx, buffer2.get(), 1);
  memcpy(&counter, &(buffer2[2]), 2);

#ifdef SWAPIT
  counter = swapUshort(counter);
#endif

#ifdef _DEBUG
  memcpy(&status, &(buffer2[1]), 1);
  std::stringstream os;
  os << "stx = " << stx << " status = " << status << " counter=" << counter
     << '\n'
     << '\0';
#endif

  s                                       = counter;
  std::unique_ptr<unsigned char[]> buffer = ESCI_read_data2(s);
  int len                                 = strlen((const char *)buffer.get());

  /*printf("Level %c%c", buffer[0], buffer[1]);*/
  if (len > 1) {
    *lev0 = buffer[0];
    *lev1 = buffer[1];
  }
  pos = 2;
  /* buffer[pos] contains 'R' */
  if (len < 3 || buffer[pos] != 'R') {
    *lev0   = '0';
    *lev1   = '0';
    *lowRes = 0;
    *hiRes  = 0;
    *vMax   = 0;
    *hMax   = 0;
    throw TException("unable to get information from scanner");
  }

  *lowRes = (buffer[pos + 2] * 256) + buffer[pos + 1];
  *hiRes  = *lowRes;

  while (buffer[pos] == 'R') {
    *hiRes = (buffer[pos + 2] * 256) + buffer[pos + 1];
    /*  printf("Resolution %c  %d", buffer[pos], *hiRes);*/
    pos += 3;
  }

  if (buffer[pos] != 'A') {
    *lev0   = '0';
    *lev1   = '0';
    *lowRes = 0;
    *hiRes  = 0;
    *vMax   = 0;
    *hMax   = 0;
    throw TException("unable to get information from scanner");
  }

  *hMax = (buffer[pos + 2] * 256) + buffer[pos + 1];
  *vMax = (buffer[pos + 4] * 256) + buffer[pos + 3];

  ESCI_command('f', false);

  ESCI_readLineData2(stx, status, counter);
  if (status & FatalError)
    throw TException("Fatal error reading information from scanner");

  s      = counter;
  buffer = ESCI_read_data2(s);
  // name buffer+1A
  const char *name = (const char *)(buffer.get() + 0x1A);
  if (strncmp(name, "Perfection1640", strlen("Perfection1640"))) {
    m_settingsMode = NEW_STYLE;
  } else {
    m_settingsMode = OLD_STYLE;
  }
#if 0
scsi_new_d("ESCI_extended_status");
scsi_len(42);
/* main status*/
scsi_b00(0, "push_button_supported", 0);
scsi_b11(0, "warming_up", 0);
scsi_b33(0, "adf_load_sequence", 0);  /* 1 from first sheet; 0 from last or not supp */
scsi_b44(0, "both_sides_on_adf", 0);
scsi_b55(0, "adf_installed_main_status", 0);
scsi_b66(0, "NFlatbed", 0);   /*0 if scanner is flatbed else 1 */
scsi_b77(0, "system_error", 0);
/* adf status */
scsi_b77(1, "adf_installed",0);
/*... some other info.., refer to manual if needed*/
/**/
#endif
  m_hasADF = !!(buffer[1] & 0x80);
  log("collectInformation:OK");
}

//-----------------------------------------------------------------------------

bool TScannerEpson::resetScanner() {
  log("resetScanner");
  bool ret = ESCI_command('@', true);
  log(std::string("resetScanner: ") + (ret ? "OK" : "FAILED"));
  return ret;
}

//-----------------------------------------------------------------------------

int TScannerEpson::receive(unsigned char *buffer, int size) {
  return m_scannerIO->receive(buffer, size);
}

int TScannerEpson::send(unsigned char *buffer, int size) {
  return m_scannerIO->send(buffer, size);
}

int TScannerEpson::sendACK() { return send(&ACK, 1); }

bool TScannerEpson::expectACK() {
  log("expectACK");
  unsigned char ack = NAK;
  int nb            = receive(&ack, 1);

#ifdef _DEBUG
  if (ack != ACK) {
    std::stringstream os;
    os << "ack fails ret = 0x" << std::hex << (int)ack << '\n' << '\0';
    TSystem::outputDebug(os.str());
  }
#endif
  log(std::string("expectACK: ") + (ack == ACK ? "ACK" : "FAILED"));

  return (ack == ACK);
}

char *TScannerEpson::ESCI_inquiry(char cmd) /* returns 0 if failed */
{
  assert(0);
  return 0;
}

bool TScannerEpson::ESCI_command(char cmd, bool checkACK) {
  unsigned char p[2];
  p[0] = ESC;
  p[1] = cmd;
  bool status;

  int count = send(&(p[0]), 2);
  status    = count == 2;

  if (checkACK) status = expectACK();

  return status;
}

bool TScannerEpson::ESCI_command_1b(char cmd, unsigned char p0, bool checkACK) {
  bool status = false;
  if (ESCI_command(cmd, checkACK)) {
    unsigned char p = p0;
    status          = true;
    send(&p, 1);
    if (checkACK) status = expectACK();
  }

  return status;
}

bool TScannerEpson::ESCI_command_2b(char cmd, unsigned char p0,
                                    unsigned char p1, bool checkACK) {
  bool status = false;
  if (ESCI_command(cmd, checkACK)) {
    status = true;
    unsigned char p[2];
    p[0]        = p0;
    p[1]        = p1;
    int timeout = 30000;
    send(&(p[0]), 2);
    if (checkACK) status = expectACK();
  }

  return status;
}

bool TScannerEpson::ESCI_command_2w(char cmd, unsigned short p0,
                                    unsigned short p1, bool checkACK) {
  bool status = false;
  if (ESCI_command(cmd, checkACK)) {
    status = true;
    unsigned short p[2];
    p[0]          = p0;
    p[1]          = p1;
    const int len = 1;
    int timeout   = 30000;
    send((unsigned char *)(&(p[0])), 4);
    if (checkACK) status = expectACK();
  }

  return status;
}

bool TScannerEpson::ESCI_command_4w(char cmd, unsigned short p0,
                                    unsigned short p1, unsigned short p2,
                                    unsigned short p3, bool checkACK) {
  bool status = false;
  if (ESCI_command(cmd, checkACK)) {
    status = true;
    unsigned char p[8];
    p[0] = (unsigned char)p0;
    p[1] = (unsigned char)(p0 >> 8);
    p[2] = (unsigned char)p1;
    p[3] = (unsigned char)(p1 >> 8);
    p[4] = (unsigned char)p2;
    p[5] = (unsigned char)(p2 >> 8);
    p[6] = (unsigned char)(p3);
    p[7] = (unsigned char)(p3 >> 8);

    send((unsigned char *)&(p[0]), 8);

    if (checkACK) status = expectACK();
  }

  return status;
}

std::unique_ptr<unsigned char[]> TScannerEpson::ESCI_read_data2(
    unsigned long &size) {
  std::unique_ptr<unsigned char[]> buffer(new unsigned char[size]);
  memset(buffer.get(), 0, size);
  unsigned long bytesToRead = size;
  size                      = receive(buffer.get(), bytesToRead);
  return buffer;
}

bool TScannerEpson::ESCI_doADF(bool on) {
  // check ref esci documentation page 3-64
  unsigned char eject = 0x0c;
  int rc              = send(&eject, 1);
  bool status1        = expectACK();
  return status1;

  return 1;
  if (!ESCI_command_1b('e', 0x01, true)) {
    if (on)
      throw TException("Scanner error loading paper");
    else
      throw TException("Scanner error unloading paper");
  }

  unsigned char p = on ? 0x19 : 0x0C;

  send(&p, 1);
  bool status = expectACK();
  return status;
}

void TScannerEpson::scanArea2pix(const TScannerParameters &params,
                                 unsigned short &offsetx,
                                 unsigned short &offsety,
                                 unsigned short &sizelx, unsigned short &sizely,
                                 const TRectD &scanArea) {
  const double f = 25.4;
  offsetx        = (unsigned short)((scanArea.x0 * params.m_dpi.m_value) / f);
  offsety        = (unsigned short)((scanArea.y0 * params.m_dpi.m_value) / f);
  sizelx =
      (unsigned short)(((scanArea.x1 - scanArea.x0) * params.m_dpi.m_value) /
                       f);
  sizelx = (sizelx >> 3) << 3;  // questo deve essere multiplo di 8
  sizely =
      (unsigned short)(((scanArea.y1 - scanArea.y0) * params.m_dpi.m_value) /
                       f);
}

void TScannerEpson::ESCI_readLineData(unsigned char &stx, unsigned char &status,
                                      unsigned short &counter,
                                      unsigned short &lines, bool &areaEnd) {
  unsigned long s                         = 6;
  std::unique_ptr<unsigned char[]> buffer = ESCI_read_data2(s);
  if (!buffer) throw TException("Error reading scanner info");
  /* PACKET DATA LEN = 6
  type offs  descr
  byte  0    STX
  b77   1    fatal_error
  b66   1    not_ready
  b55   1    area_end
  b44   1    option_unit
  b33   1    col_attrib_bit_3
  b22   1    col_attrib_bit_2
  b11   1    extended_commands
  drow  2,   counter
  drow  4    lines
  */
  bool fatalError = !!(buffer[1] & 0x80);
  bool notReady   = !!(buffer[1] & 0x40);
  areaEnd         = !!(buffer[1] & 0x20);

  memcpy(&stx, buffer.get(), 1);
  memcpy(&counter, &(buffer[2]), 2);

#ifdef SWAPIT
  counter = swapUshort(counter);
#endif
  memcpy(&lines, &(buffer[4]), 2);
#ifdef SWAPIT
  lines = swapUshort(lines);
#endif

  status = buffer[1];

#ifdef _DEBUG
  std::stringstream os;

  os << "fatal=" << fatalError;
  os << " notReady=" << notReady;
  os << " areaEnd=" << areaEnd;

  os << " stx=" << stx;
  os << " counter=" << counter;
  os << " lines=" << lines;
  os << '\n' << '\0';

  TSystem::outputDebug(os.str());
#endif
}

void TScannerEpson::ESCI_readLineData2(unsigned char &stx,
                                       unsigned char &status,
                                       unsigned short &counter) {
  unsigned long s                         = 4;
  std::unique_ptr<unsigned char[]> buffer = ESCI_read_data2(s);
  if (!buffer) throw TException("Error reading scanner info");
  bool fatalError = !!(buffer[1] & 0x80);
  bool notReady   = !!(buffer[1] & 0x40);

  memcpy(&stx, buffer.get(), 1);
  memcpy(&counter, &(buffer[2]), 2);
#ifdef SWAPIT
  counter = swapUshort(counter);
#endif

  status = buffer[1];

#ifdef _DEBUG
  std::stringstream os;

  os << "fatal=" << fatalError;
  os << " notReady=" << notReady;

  os << " stx=" << stx;
  os << " counter=" << counter;
  os << '\n' << '\0';

  TSystem::outputDebug(os.str());
#endif
}
