

#include "toonz/captureparameters.h"
#include "tiio.h"
#include "tstream.h"
#include "tproperty.h"

//-----------------------------------------------------------------------------
// CaptureParameters

CaptureParameters::CaptureParameters()
    : m_deviceName(L"")
    , m_resolution()
    , m_brightness(0)
    , m_contranst(0)
    , m_useWhiteImage(false)
    , m_upsideDown(false)
    , m_filePath(TFilePath("+inputs"))
    , m_increment(1)
    , m_step(1)
    , m_format("tif") {}

//-----------------------------------------------------------------------------

TPropertyGroup *CaptureParameters::getFileFormatProperties(std::string ext) {
  std::map<std::string, TPropertyGroup *>::const_iterator it;
  it = m_formatProperties.find(ext);
  if (it == m_formatProperties.end()) {
    TPropertyGroup *ret     = Tiio::makeWriterProperties(ext);
    m_formatProperties[ext] = ret;
    return ret;
  } else
    return it->second;
}

//-----------------------------------------------------------------------------

void CaptureParameters::assign(const CaptureParameters *params) {
  m_deviceName    = params->getDeviceName();
  m_resolution    = params->getResolution();
  m_brightness    = params->getBrightness();
  m_contranst     = params->getContranst();
  m_useWhiteImage = params->isUseWhiteImage();
  m_upsideDown    = params->isUpsideDown();
  m_filePath      = params->getFilePath();
  m_increment     = params->getIncrement();
  m_step          = params->getStep();
  m_format        = params->getFileFormat();

  std::map<std::string, TPropertyGroup *>::const_iterator it =
      params->m_formatProperties.begin();
  while (it != params->m_formatProperties.end()) {
    m_formatProperties[it->first] = it->second;
    ++it;
  }
}

//-----------------------------------------------------------------------------

void CaptureParameters::saveData(TOStream &os) {
  os.child("deviceName") << m_deviceName;
  os.child("reslution") << m_resolution.lx << m_resolution.ly;
  os.child("brightness") << m_brightness;
  os.child("contranst") << m_contranst;
  os.child("useWhiteImage") << (int)m_useWhiteImage;
  os.child("upsideDown") << (int)m_upsideDown;
  os.child("filePath") << m_filePath;
  // os.child("increment") << m_increment;
  // os.child("step") << m_step;
  os.child("format") << m_format;

  os.openChild("formatsProperties");
  std::vector<std::string> fileExtensions;
  getFileFormatPropertiesExtensions(fileExtensions);
  for (int i = 0; i < (int)fileExtensions.size(); i++) {
    std::string ext    = fileExtensions[i];
    TPropertyGroup *pg = getFileFormatProperties(ext);
    assert(pg);
    std::map<std::string, std::string> attr;
    attr["ext"] = ext;
    os.openChild("formatProperties", attr);
    pg->saveData(os);
    os.closeChild();
  }
  os.closeChild();
}

//-----------------------------------------------------------------------------

void CaptureParameters::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "deviceName")
      is >> m_deviceName;
    else if (tagName == "reslution")
      is >> m_resolution.lx >> m_resolution.ly;
    else if (tagName == "brightness")
      is >> m_brightness;
    else if (tagName == "contranst")
      is >> m_contranst;
    else if (tagName == "useWhiteImage") {
      int value;
      is >> value;
      m_useWhiteImage = value != 0;
    } else if (tagName == "upsideDown") {
      int value;
      is >> value;
      m_upsideDown = value != 0;
    } else if (tagName == "filePath") {
      std::wstring str;
      is >> str;
      m_filePath = TFilePath(str);
    }
    // else if(tagName == "increment")
    //  is >> m_increment;
    // else if(tagName == "step")
    //  is >> m_step;
    else if (tagName == "format")
      is >> m_format;
    else if (tagName == "formatsProperties") {
      while (is.matchTag(tagName)) {
        if (tagName == "formatProperties") {
          std::string ext    = is.getTagAttribute("ext");
          TPropertyGroup *pg = getFileFormatProperties(ext);
          if (ext == "avi") {
            TPropertyGroup appProperties;
            appProperties.loadData(is);
            assert(appProperties.getPropertyCount() == 1);
            if (pg->getPropertyCount() != 1) {
              is.closeChild();
              continue;
            }
            TEnumProperty *enumProp =
                dynamic_cast<TEnumProperty *>(pg->getProperty(0));
            TEnumProperty *enumAppProp =
                dynamic_cast<TEnumProperty *>(appProperties.getProperty(0));
            assert(enumAppProp && enumProp);
            if (enumAppProp && enumProp) {
              try {
                enumProp->setValue(enumAppProp->getValue());
              } catch (TProperty::RangeError &) {
              }
            } else
              throw TException();
          } else
            pg->loadData(is);
          is.closeChild();
        } else
          throw TException("unexpected tag: " + tagName);
      }  // end while
    } else
      throw TException("unexpected property tag: " + tagName);
    is.closeChild();
  }
}

//-------------------------------------------------------------------

void CaptureParameters::getFileFormatPropertiesExtensions(
    std::vector<std::string> &v) const {
  v.reserve(m_formatProperties.size());
  std::map<std::string, TPropertyGroup *>::const_iterator it;
  for (it = m_formatProperties.begin(); it != m_formatProperties.end(); ++it)
    v.push_back(it->first);
}
