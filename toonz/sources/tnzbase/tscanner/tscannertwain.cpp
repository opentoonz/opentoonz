

#include <texception.h>
#include <tscanner.h>
#include "tscannertwain.h"
#include "tscannerutil.h"

using namespace TScannerUtil;

extern "C" {
#include "../common/twain/ttwain_util.h"
#include "../common/twain/ttwain_global_def.h" /* forward declare functions */
}

/* callback used to handle TTWAIN done/error status*/

static void throwIT(const char *msg) { throw TException(msg); }

extern "C" void TTWAIN_ErrorBox(const char *msg) { throwIT(msg); }

extern "C" {
static int onDoneCB(UCHAR *buffer, TTWAIN_PIXTYPE pixelType, int lx, int ly,
                    int wrap, float xdpi, float ydpi, void *usrData) {
  TRasterP ras;
  switch (pixelType) {
  case TTWAIN_BW:
  case TTWAIN_WB: {
    try {
      TRasterGR8P ras8(lx, ly);
      copy90BWBufferToRasGR8(buffer, lx, ly, wrap, (pixelType == TTWAIN_BW),
                             ras8, 1, 0);
      ras8->xMirror();
      ras8->yMirror();
      ras = ras8;
    } catch (...) {
    }
  } break;

  case TTWAIN_RGB24: {
    try {
      TRaster32P ras32(lx, ly);
      copyRGBBufferToTRaster32(buffer, lx, ly, ras32, false);
      ras = ras32;
    } catch (...) {
    }
  } break;

  case TTWAIN_GRAY8: {
    try {
      TRasterGR8P ras8(lx, ly);
      copyGR8BufferToTRasterGR8(buffer, lx, ly, ras8, false);
      ras = ras8;
    } catch (...) {
    }
  } break;
  default:
    break;
  }

  TRasterImageP rasImg;
  if (ras) {
    rasImg = TRasterImageP(ras);
    rasImg->setDpi(xdpi, ydpi);
    // rasImg->sethPos(hpos);
    rasImg->setSavebox(ras->getBounds());
  }

  TScanner *scannerDevice = reinterpret_cast<TScanner *>(usrData);
  scannerDevice->notifyImageDone(rasImg);
  TTWAIN_FreeMemory(buffer);
  scannerDevice->decrementPaperLeftCount();
  return scannerDevice->getPaperLeftCount();
}
}

//-----------------------------------------------------------------------------

extern "C" {
static void onErrorCB(void *usrData, void *alwaysZero) {
  TScanner *scannerDevice = reinterpret_cast<TScanner *>(usrData);
  scannerDevice->notifyError();
}
}

//-----------------------------------------------------------------------------

TScannerTwain::TScannerTwain() {}

//-----------------------------------------------------------------------------

TScannerTwain::~TScannerTwain() {}

//-----------------------------------------------------------------------------

namespace {
bool deviceSelected = false;
}

//-----------------------------------------------------------------------------

void TScannerTwain::selectDevice() {
  TTWAIN_SelectImageSource(0);
  deviceSelected = true;

  if (TTWAIN_OpenDefaultSource()) {
    QString twainVersion(TTWAIN_GetTwainVersion());
    QString manufacturer(TTWAIN_GetManufacturer());
    QString productFamily(TTWAIN_GetProductFamily());
    QString productName(TTWAIN_GetProductName());
    QString version(TTWAIN_GetVersion());

    TTWAIN_CloseAll(0);

    if (manufacturer != "") {
      // se c'e' il nome della marca lo tolgo dal productFamily e productName
      productFamily.replace(manufacturer, "");
      productName.replace(manufacturer, "");
    }
    setName(manufacturer + " " + productFamily + " " + productName + " (" +
            version + ")");
  }
}

//-----------------------------------------------------------------------------

bool TScannerTwain::isDeviceSelected() { return deviceSelected; }

//-----------------------------------------------------------------------------

bool TScannerTwain::isDeviceAvailable() { return !!TTWAIN_IsAvailable(); }

//-----------------------------------------------------------------------------

// TODO!! renderla un update
void TScannerTwain::updateParameters(TScannerParameters &param) {
  int rc = TTWAIN_OpenDefaultSource();
  if (!rc) return;

#ifdef MACOSX
// TTWAIN_DumpCapabilities(printf);
#endif
  if (isAreaSupported()) {
    float w, h;
    TTWAIN_GetPhysicalWidthWoAdf(&w);
    TTWAIN_GetPhysicalHeightWoAdf(&h);
    double width_mm  = w * 25.4;
    double height_mm = h * 25.4;

    param.setMaxPaperSize(width_mm, height_mm);
    // sembra sbagliato: bisognerebbe comunque tenere conto del paperFormat
    // selezionato
    //    param.m_scanArea = TRectD(0,0, width_mm,height_mm);
  }
  TScanParam defaultTwainParam(0., 255., 128., 1.);
  if (TTWAIN_IsCapBrightnessSupported()) {
    m_brightness.m_supported = true;
    TTWAIN_GetBrightness(&m_brightness.m_min, &m_brightness.m_max,
                         &m_brightness.m_step, &m_brightness.m_def);
  } else {
    // m_brightness.m_def = m_brightness.m_max = m_brightness.m_min =
    // m_brightness.m_step = 0.;
    m_brightness.update(defaultTwainParam);
    m_brightness.m_supported = false;
  }
  // m_brightness.m_value = m_brightness.m_def;
  param.m_brightness.update(m_brightness);

  if (TTWAIN_IsCapContrastSupported()) {
    m_contrast.m_supported = true;
    TTWAIN_GetContrast(&m_contrast.m_min, &m_contrast.m_max, &m_contrast.m_step,
                       &m_contrast.m_def);
  } else {
    // m_contrast.m_def = m_contrast.m_max = m_contrast.m_min =
    // m_contrast.m_step = 0.;
    m_contrast.update(defaultTwainParam);
    m_contrast.m_supported = false;
  }
  // m_contrast.m_value = m_contrast.m_def;
  param.m_contrast.update(m_contrast);

  if (TTWAIN_IsCapThresholdSupported()) {
    m_threshold.m_supported = true;
    TTWAIN_GetThreshold(&m_threshold.m_min, &m_threshold.m_max,
                        &m_threshold.m_step, &m_threshold.m_def);
  } else {
    // m_threshold.m_def = m_threshold.m_max = m_threshold.m_min =
    // m_threshold.m_step = 0.;
    m_threshold.update(defaultTwainParam);
    m_threshold.m_supported = false;
  }

  // m_threshold.m_value = m_threshold.m_def;
  param.m_threshold.update(m_threshold);

  if (TTWAIN_IsCapResolutionSupported()) {
    m_dpi.m_supported = true;
    TTWAIN_GetResolution(&m_dpi.m_min, &m_dpi.m_max, &m_dpi.m_step,
                         &m_dpi.m_def);
    param.m_dpi.update(m_dpi);
  } else {
    // m_dpi.m_def = m_dpi.m_max = m_dpi.m_min = m_dpi.m_step = 0.;
    param.m_dpi.m_supported = false;
  }
  // m_dpi.m_value = m_dpi.m_def;

  /*
param.m_twainVersion = string(TTWAIN_GetTwainVersion());
param.m_manufacturer = string(TTWAIN_GetManufacturer ());
param.m_prodFamily = string(TTWAIN_GetProductFamily());
param.m_productName = string(TTWAIN_GetProductName  ());
param.m_version = string(TTWAIN_GetVersion());
*/

  bool bw = !!TTWAIN_SupportsPixelType(TTWAIN_BW) ||
            !!TTWAIN_SupportsPixelType(TTWAIN_WB);
  bool gray = !!TTWAIN_SupportsPixelType(TTWAIN_GRAY8);
  bool rgb  = !!TTWAIN_SupportsPixelType(TTWAIN_RGB24);

  param.setSupportedTypes(bw, gray, rgb);

  TTWAIN_CloseAll(0);
}

//-----------------------------------------------------------------------------

static void setupParameters(const TScannerParameters &params,
                            bool isAreaSupported) {
  if (isAreaSupported) {
    TRectD scanArea = params.getScanArea();
    float L         = (float)(scanArea.getP00().x / 25.4);
    float T         = (float)(scanArea.getP00().y / 25.4);
    float R         = (float)(scanArea.getP11().x / 25.4);
    float B         = (float)(scanArea.getP11().y / 25.4);
    TTWAIN_SetImageLayout(L, T, R, B);
  }

  TTWAIN_PIXTYPE pt;
  switch (params.getScanType()) {
  case TScannerParameters::BW:
    pt = TTWAIN_BW;
    break;
  case TScannerParameters::GR8:
    pt = TTWAIN_GRAY8;
    break;
  case TScannerParameters::RGB24:
    pt = TTWAIN_RGB24;
    break;
  default:
    /*      assert(0);*/
    pt = TTWAIN_RGB24;
    break;
  }

  TTWAIN_SetPixelType(pt);
  if (params.m_dpi.m_supported) TTWAIN_SetResolution(params.m_dpi.m_value);

  if (params.m_brightness.m_supported)
    TTWAIN_SetBrightness(params.m_brightness.m_value);

  if (params.m_contrast.m_supported)
    TTWAIN_SetContrast(params.m_contrast.m_value);

  if (params.m_threshold.m_supported)
    TTWAIN_SetThreshold(params.m_threshold.m_value);
}

//-----------------------------------------------------------------------------
static void openAndSetupTwain() {
  int rc = TTWAIN_OpenDefaultSource();
  if (rc) {
    TTWAIN_SetTwainUsage(TTWAIN_MODE_UNLEASHED);
    if (TTWAIN_IsCapDeviceOnLineSupported()) {
      int status = TTWAIN_IsDeviceOnLine();
      if (status == 0) {
      }
    }
  }
  TTWAIN_SetUIStatus(TRUE);
}

void TScannerTwain::acquire(const TScannerParameters &params, int paperCount) {
  openAndSetupTwain();

  // int feeder = TTWAIN_IsFeederLoaded();
  int feeder  = params.isPaperFeederEnabled();
  m_paperLeft = paperCount;
  int rc;

#ifdef MACOSX
  // pezza solo HP su MACOSX per gestire la scansione di piu' frame
  bool ishp = !!(strcasestr(TTWAIN_GetProductName(), "hp"));
#else
  bool ishp = false;
#endif
  if (ishp) {
    for (int i = 0; i < paperCount; ++i) {
      printf("scanning %d/%d\n", i + 1, paperCount);
      if (i == 0) {
        setupParameters(params, isAreaSupported());
        TTWAIN_SetOnDoneCallback(onDoneCB, this);
        TTWAIN_SetOnErrorCallback(onErrorCB, this);
      } else
        openAndSetupTwain();

      rc = TTWAIN_SetXferMech(TTWAIN_TRANSFERMODE_MEMORY, 0, 0, 0, 0, 1);
      int acq_rc = TTWAIN_AcquireMemory(0);

      if (acq_rc == 0) break;  // error, or the user has closed the TWAIN UI
      if (!feeder && ((paperCount - i) > 1)) notifyNextPaper();
      TTWAIN_CloseAll(0);
    }
  } else {
    for (int i = 0; i < (feeder ? 1 : paperCount); ++i) {
      printf("scanning %d/%d\n", i + 1, paperCount);
      setupParameters(params, isAreaSupported());

      rc = TTWAIN_SetXferMech(TTWAIN_TRANSFERMODE_MEMORY, 0, 0, 0, 0,
                              paperCount);

      TTWAIN_SetOnDoneCallback(onDoneCB, this);
      TTWAIN_SetOnErrorCallback(onErrorCB, this);
      int acq_rc = TTWAIN_AcquireMemory(0);

      if (acq_rc == 0) break;  // error, or the user has closed the TWAIN UI
      if (!feeder && ((paperCount - i) > 1)) notifyNextPaper();
    }
    TTWAIN_CloseAll(0);
  }
}

//-----------------------------------------------------------------------------

bool TScannerTwain::isAreaSupported() {
  /*
TTWAIN_IsCapPhysicalWidthSupported    (void);
TTWAIN_IsCapPhysicalHeightSupported   (void);
*/
  return !!TTWAIN_IsCapImageLayoutSupported();
}
