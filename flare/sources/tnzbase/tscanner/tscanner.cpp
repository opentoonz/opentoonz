

#include "tscanner.h"
#include "tscannertwain.h"
#include "texception.h"
#include "tscannerepson.h"
#include "tstream.h"
#include "tconvert.h"
#include "tfilepath.h"
#include "tfilepath_io.h"
#include "tenv.h"
#include "tsystem.h"

extern "C" {
#include "../common/twain/ttwain_util.h"
}

//===================================================================
//
// TScanParam
//
//-------------------------------------------------------------------

void TScanParam::update(const TScanParam &model) {
  m_supported = model.m_supported;
  m_min       = model.m_min;
  m_max       = model.m_max;
  m_def       = model.m_def;
  m_step      = model.m_step;
  m_value     = tcrop(m_value, m_min, m_max);
}

//===================================================================
//
// TScannerParameters
//
//-------------------------------------------------------------------

const std::string BlackAndWhite = "Black & White";
const std::string Graytones     = "Graytones";
const std::string Rgbcolors     = "RGB Color";

//-------------------------------------------------------------------

TScannerParameters::TScannerParameters()
    : m_bw(false)
    , m_gray(false)
    , m_rgb(false)
    , m_scanType(None)
    , m_scanArea(TRectD())
    , m_cropBox(TRectD())
    , m_isPreview(false)
    , m_maxPaperSize(TDimensionD(0, 0))
    , m_paperOverflow(false)
    , m_brightness()
    , m_contrast()
    , m_threshold()
    , m_dpi()
    , m_paperFeeder()
    , m_twainVersion()
    , m_manufacturer()
    , m_prodFamily()
    , m_productName()
    , m_version()
    , m_reverseOrder(false)
    , m_validatedByCurrentScanner(false) {
  m_threshold.m_value  = 127;
  m_brightness.m_value = 127;
}

//-------------------------------------------------------------------------------------------------

TScannerParameters::~TScannerParameters() {}

//-------------------------------------------------------------------------------------------------

void TScannerParameters::assign(const TScannerParameters *src) { *this = *src; }

//-------------------------------------------------------------------------------------------------

void TScannerParameters::setSupportedTypes(bool bw, bool gray, bool rgb) {
  m_bw   = bw;
  m_gray = gray;
  m_rgb  = rgb;
  if (!m_bw && !m_gray && !m_rgb) {
    m_scanType = None;
  } else {
    switch (m_scanType) {
    case BW:
      if (!m_bw) m_scanType = m_gray ? GR8 : RGB24;
      break;
    case GR8:
      if (!m_gray) m_scanType = m_rgb ? RGB24 : BW;
      break;
    case RGB24:
      if (!m_rgb) m_scanType = m_gray ? GR8 : BW;
      break;
    case None:
      if (gray)
        m_scanType = GR8;
      else {
        if (rgb)
          m_scanType = RGB24;
        else if (bw)
          m_scanType = BW;
      }
      break;
    }
  }
}

//-------------------------------------------------------------------------------------------------

bool TScannerParameters::isSupported(ScanType scanType) const {
  switch (scanType) {
  case BW:
    return m_bw;
  case GR8:
    return m_gray;
  case RGB24:
    return m_rgb;
  default:
    assert(0);
    return false;
  }
}

//-------------------------------------------------------------------------------------------------

void TScannerParameters::cropScanArea() {
  m_paperOverflow = false;
  if (m_maxPaperSize.lx == 0 || m_maxPaperSize.ly == 0) {
    // probabilmente non e' ancora stato selezionato uno scanner e quindi non e'
    // definita una maxPaperSize
    return;
  }
  assert(m_maxPaperSize.lx > 0 && m_maxPaperSize.ly > 0);
  if (m_scanArea.x1 > m_maxPaperSize.lx) {
    m_paperOverflow = true;
    m_scanArea.x1   = m_maxPaperSize.lx;
  }
  if (m_scanArea.y1 > m_maxPaperSize.ly) {
    m_paperOverflow = true;
    m_scanArea.y1   = m_maxPaperSize.ly;
  }
}

//-------------------------------------------------------------------------------------------------

void TScannerParameters::setMaxPaperSize(double maxWidth, double maxHeight) {
  // assert(maxWidth>0 && maxHeight>0);
  m_maxPaperSize = TDimensionD(maxWidth, maxHeight);
  cropScanArea();
}

//-----------------------------------------------------------------------------

void TScannerParameters::setPaperFormat(std::string paperFormat) {
  TPaperFormatManager *formatManager = TPaperFormatManager::instance();
  assert(formatManager->isValidFormat(paperFormat));
  if (!formatManager->isValidFormat(paperFormat))
    paperFormat = formatManager->getDefaultFormat();
  m_paperFormat = paperFormat;
  TDimensionD d = TPaperFormatManager::instance()->getSize(paperFormat);
  m_scanArea    = TRectD(TPointD(0, 0), d);
  cropScanArea();
  if (m_cropBox == TRectD()) m_cropBox = m_scanArea;
}

//-----------------------------------------------------------------------------

void TScannerParameters::updatePaperFormat() {
  if (m_paperFormat == "")
    m_paperFormat = TPaperFormatManager::instance()->getDefaultFormat();
  setPaperFormat(m_paperFormat);
}

//-----------------------------------------------------------------------------

void TScannerParameters::setScanType(ScanType scanType) {
  assert(scanType == None || scanType == BW || scanType == GR8 ||
         scanType == RGB24);
  m_scanType = scanType;
}

//-----------------------------------------------------------------------------

void TScannerParameters::adaptToCurrentScanner() {
  try {
    if (TScanner::instance()->isDeviceSelected()) {
      TScanner::instance()->updateParameters(*this);
      m_validatedByCurrentScanner = true;
    }
  } catch (TException &) {
    // TMessage::error("%1", e.getMessage());
  }
}

//-------------------------------------------------------------------------------------------------

void TScannerParameters::saveData(TOStream &os) const {
  std::map<std::string, std::string> attr;
  attr["fmt"] = m_paperFormat;
  os.openCloseChild("paper", attr);

  if (m_paperFeeder.m_value == 1.0) {
    attr.clear();
    os.openCloseChild("autoFeeder", attr);
  }

  if (m_reverseOrder) {
    attr.clear();
    os.openCloseChild("reverseOrder", attr);
  }

  if (m_scanType != None) {
    std::string scanTypeString = Rgbcolors;
    switch (m_scanType) {
    case BW:
      scanTypeString = BlackAndWhite;
      break;
    case GR8:
      scanTypeString = Graytones;
      break;
    case RGB24:
      scanTypeString = Rgbcolors;
      break;
    default:
      break;
    }
    attr.clear();
    attr["value"] = scanTypeString;
    os.openCloseChild("mode", attr);
  }

  if (m_dpi.m_supported) {
    attr.clear();
    attr["value"] = std::to_string(m_dpi.m_value);
    os.openCloseChild("dpi", attr);
  }

  if (m_brightness.m_supported) {
    attr.clear();
    attr["value"] = std::to_string(m_brightness.m_value);
    os.openCloseChild("brightness", attr);
  }

  if (m_contrast.m_supported) {
    attr.clear();
    attr["value"] = std::to_string(m_contrast.m_value);
    os.openCloseChild("contrast", attr);
  }

  if (m_threshold.m_supported) {
    attr.clear();
    attr["value"] = std::to_string(m_threshold.m_value);
    os.openCloseChild("threshold", attr);
  }
}

//-------------------------------------------------------------------------------------------------

void TScannerParameters::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "dpi") {
      std::string s                  = is.getTagAttribute("value");
      if (isDouble(s)) m_dpi.m_value = std::stof(s);
    } else if (tagName == "brightness") {
      std::string s                         = is.getTagAttribute("value");
      if (isDouble(s)) m_brightness.m_value = std::stof(s);
    } else if (tagName == "threshold") {
      std::string s                        = is.getTagAttribute("value");
      if (isDouble(s)) m_threshold.m_value = std::stof(s);
    } else if (tagName == "contrast") {
      std::string s                       = is.getTagAttribute("value");
      if (isDouble(s)) m_contrast.m_value = std::stof(s);
    } else if (tagName == "autoFeeder") {
      m_paperFeeder.m_value = 1.0;
    } else if (tagName == "reverseOrder") {
      m_reverseOrder = true;
    } else if (tagName == "mode") {
      std::string scanTypeString = is.getTagAttribute("value");
      m_scanType                 = None;
      if (scanTypeString == BlackAndWhite)
        m_scanType = BW;
      else if (scanTypeString == Graytones)
        m_scanType = GR8;
      else if (scanTypeString == Rgbcolors)
        m_scanType = RGB24;
    } else if (tagName == "paper") {
      std::string paperFormat = is.getTagAttribute("fmt");
      if (paperFormat != "") setPaperFormat(paperFormat);
    }
  }
  m_validatedByCurrentScanner = false;
}

//===================================================================
//
// TScanner
//
//-------------------------------------------------------------------

namespace {
TScanner *instanceTwain = 0;
TScanner *instanceEpson = 0;
}

//-------------------------------------------------------------------

bool TScanner::m_isTwain = true;

//-------------------------------------------------------------------

namespace {
class Cleaner {
public:
  bool m_activated;
  Cleaner() : m_activated(false) {}
  ~Cleaner() {
    TScanner *scannerToDestroy        = 0;
    if (m_activated) scannerToDestroy = TScanner::instance();
    delete scannerToDestroy;
  }
};
Cleaner MyCleaner;
};

//-------------------------------------------------------------------

// #define DUMMYSCAN
#ifdef DUMMYSCAN
class TScannerDummy final : public TScanner {
public:
  TScannerDummy() {}
  ~TScannerDummy() {}
  void selectDevice() {}
  bool isDeviceAvailable() { return true; }
  void updateParameters(TScannerParameters &parameters) {
    parameters.setSupportedTypes(false, true, true);
    parameters.setMaxPaperSize(1000., 1000.);
    parameters.enablePaperFeeder(true);
    parameters.m_brightness.update(TScanParam(0, 255, 128, 1));
    parameters.m_contrast.update(TScanParam(0, 255, 128, 1));
    parameters.m_threshold.update(TScanParam(0, 255, 128, 1));
    parameters.m_dpi.update(TScanParam(60, 1200, 100, 1));

    setName("DummyScanner");
  }
  bool isTwain() const { return true; }
  void acquire(const TScannerParameters &param, int paperCount) { return; }
  bool isAreaSupported() { return true; }
};
#endif

//-----------------------------------------------------------------------------

TScanner *TScanner::instance() {
  MyCleaner.m_activated = true;
#ifdef DUMMYSCAN
  static TScannerDummy dummy = TScannerDummy();
  return &dummy;
#else
  if (m_isTwain) {
    if (instanceEpson) {
      TScannerEpson *se = (TScannerEpson *)instanceEpson;
      se->closeIO();
      // delete m_instanceEpson; //e' singletone, perche' buttarlo? (vinz)
      // m_instanceEpson=0;
    }
    if (!instanceTwain) instanceTwain = new TScannerTwain();
  } else if (!m_isTwain) {
    if (instanceTwain) {
      // delete m_instanceTwain;  //e' singletone, perche' buttarlo? (vinz)
      // m_instanceTwain=0;
      TTWAIN_CloseAll(0);
    }
    if (!instanceEpson) instanceEpson = new TScannerEpson();
  }

  return (m_isTwain ? instanceTwain : instanceEpson);
#endif
}

//-----------------------------------------------------------------------------
TScanner::TScanner() : m_paperLeft(0) {}

//-----------------------------------------------------------------------------

TScanner::~TScanner() {
  if (instanceEpson) {
    TScannerEpson *se = (TScannerEpson *)instanceEpson;
    se->closeIO();
  }
}

//-----------------------------------------------------------------------------

void TScanner::addListener(TScannerListener *lst) { m_listeners.insert(lst); }

//-----------------------------------------------------------------------------

void TScanner::removeListener(TScannerListener *lst) { m_listeners.erase(lst); }

//-----------------------------------------------------------------------------

void TScanner::notifyImageDone(const TRasterImageP &img) {
  std::set<TScannerListener *>::iterator it = m_listeners.begin();
  for (; it != m_listeners.end(); ++it) {
    (*it)->onImage(img);
  }
}

//-----------------------------------------------------------------------------

void TScanner::notifyNextPaper() {
  std::set<TScannerListener *>::iterator it = m_listeners.begin();
  for (; it != m_listeners.end(); ++it) {
    (*it)->onNextPaper();
  }
}

//-----------------------------------------------------------------------------

void TScanner::notifyAutomaticallyNextPaper() {
  std::set<TScannerListener *>::iterator it = m_listeners.begin();
  for (; it != m_listeners.end(); ++it) {
    (*it)->onAutomaticallyNextPaper();
  }
}

//-----------------------------------------------------------------------------

void TScanner::notifyError() {
  std::set<TScannerListener *>::iterator it = m_listeners.begin();
  for (; it != m_listeners.end(); ++it) {
    (*it)->onError();
  }
}

//-----------------------------------------------------------------------------
/*! If one listener is set to cancel return true. */
bool TScanner::isScanningCanceled() {
  std::set<TScannerListener *>::iterator it = m_listeners.begin();
  for (; it != m_listeners.end(); ++it) {
    if ((*it)->isCanceled()) return true;
  }
  return false;
}

//===================================================================
//
// TPaperFormatManager
//
//-------------------------------------------------------------------

namespace {
const std::pair<std::string, TDimensionD> defaultPaperFormat(
    "A4 paper", TDimensionD(210.00, 297.00));
}

//-----------------------------------------------------------------------------

TPaperFormatManager::TPaperFormatManager() {
  readPaperFormats();
  // se non c'e' aggiungo il formato di default. In questo modo e' sempre
  // definito
  if (!isValidFormat(defaultPaperFormat.first))
    m_formats[defaultPaperFormat.first] = Format(defaultPaperFormat.second);
}

//-----------------------------------------------------------------------------

TPaperFormatManager *TPaperFormatManager::instance() {
  static TPaperFormatManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void TPaperFormatManager::getFormats(std::vector<std::string> &names) const {
  for (FormatTable::const_iterator it = m_formats.begin();
       it != m_formats.end(); ++it)
    names.push_back(it->first);
}

//-----------------------------------------------------------------------------

TDimensionD TPaperFormatManager::getSize(std::string name) const {
  FormatTable::const_iterator it = m_formats.find(name);
  if (it == m_formats.end())
    return TDimensionD(0., 0.);
  else
    return it->second.m_size;
}

//-----------------------------------------------------------------------------

bool TPaperFormatManager::isValidFormat(std::string name) const {
  FormatTable::const_iterator it = m_formats.find(name);
  return it != m_formats.end();
}

//-----------------------------------------------------------------------------

std::string TPaperFormatManager::getDefaultFormat() const {
  return defaultPaperFormat.first;
}

//-----------------------------------------------------------------------------

void TPaperFormatManager::readPaperFormat(const TFilePath &path) {
  if (path.getType() != "pap") return;
  Tifstream is(path);
  std::string name;
  TDimensionD size(0, 0);
  while (is) {
    char buffer[1024];
    is.getline(buffer, sizeof buffer);

    // i e' il carattere successivo alla fine della linea
    unsigned int i = 0;
    for (i = 0; i < sizeof buffer && buffer[i]; i++) {
    }
    if (i > 0 && buffer[i - 1] == '\n') i--;
    while (i > 0 && buffer[i - 1] == ' ') i--;
    unsigned int j = 0;
    unsigned int k = 0;
    // j e' il carattere successivo alla fine del primo token
    for (j = 0; j < i && buffer[j] != ' '; j++) {
    }

    // k e' l'inizio del secondo token (se c'e', altrimenti == i)
    for (k = j; k < i && buffer[k] == ' '; k++) {
    }

    std::string value;
    if (k < i) value = std::string(buffer + k, i - k);

    if (buffer[0] == '#') {
      if (k < i && name == "") name = value;
    } else if (std::string(buffer).find("WIDTH") == 0) {
      if (isDouble(value)) size.lx = std::stod(value);
    } else if (std::string(buffer).find("LENGTH") == 0) {
      if (isDouble(value)) size.ly = std::stod(value);
    }
  }
  if (name == "" || size.lx == 0 || size.ly == 0) {
    // TMessage::error("Error reading paper format file : %1",path);
  } else
    m_formats[name] = Format(size);
}

//-----------------------------------------------------------------------------

void TPaperFormatManager::readPaperFormats() {
  TFilePathSet fps;
  TFilePath papDir = TEnv::getConfigDir() + "pap";
  if (!TFileStatus(papDir).isDirectory()) {
    // TMessage::error("E_CanNotReadDirectory_%1", papDir);
    return;
  }

  try {
    fps = TSystem::readDirectory(papDir);
  } catch (TException &) {
    // TMessage::error("E_CanNotReadDirectory_%1", papDir);
    return;
  }

  TFilePathSet::const_iterator it = fps.begin();
  for (; it != fps.end(); ++it) readPaperFormat(*it);
}
