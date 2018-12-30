

#include "toutputproperties.h"

// TnzLib includes
#include "toonz/boardsettings.h"

// TnzBase includes
#include "trasterfx.h"

// TnzCore includes
#include "tiio.h"
#include "tproperty.h"

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

inline void deleteValue(const std::pair<std::string, TPropertyGroup *> &p) {
  delete p.second;
}

}  // namespace

//**********************************************************************************
//    TOutputProperties  implementation
//**********************************************************************************

TOutputProperties::TOutputProperties()
    : m_path(TFilePath("+outputs") + ".tif")
    , m_renderSettings()
    , m_frameRate(24)
    , m_from(0)
    , m_to(-1)
    , m_offset(0)
    , m_step(1)
    , m_whichLevels(false)
    , m_multimediaRendering(0)
    , m_maxTileSizeIndex(0)
    , m_threadIndex(2)
    , m_subcameraPreview(false)
    , m_boardSettings(new BoardSettings()) {
  m_renderSettings = new TRenderSettings();
}

//-------------------------------------------------------------------

TOutputProperties::TOutputProperties(const TOutputProperties &src)
    : m_path(src.m_path)
    , m_formatProperties(src.m_formatProperties)
    , m_renderSettings(new TRenderSettings(*src.m_renderSettings))
    , m_frameRate(src.m_frameRate)
    , m_from(src.m_from)
    , m_to(src.m_to)
    , m_whichLevels(src.m_whichLevels)
    , m_offset(src.m_offset)
    , m_step(src.m_step)
    , m_multimediaRendering(src.m_multimediaRendering)
    , m_maxTileSizeIndex(src.m_maxTileSizeIndex)
    , m_threadIndex(src.m_threadIndex)
    , m_subcameraPreview(src.m_subcameraPreview)
    , m_boardSettings(new BoardSettings(*src.m_boardSettings)) {
  std::map<std::string, TPropertyGroup *>::iterator ft,
      fEnd = m_formatProperties.end();
  for (ft = m_formatProperties.begin(); ft != fEnd; ++ft) {
    if (ft->second) ft->second = ft->second->clone();
  }
}

//-------------------------------------------------------------------

TOutputProperties::~TOutputProperties() {
  delete m_renderSettings;

  std::for_each(m_formatProperties.begin(), m_formatProperties.end(),
                ::deleteValue);
}

//-------------------------------------------------------------------

TOutputProperties &TOutputProperties::operator=(const TOutputProperties &src) {
  m_path        = src.m_path;
  m_from        = src.m_from;
  m_to          = src.m_to;
  m_frameRate   = src.m_frameRate;
  m_whichLevels = src.m_whichLevels;
  m_offset      = src.m_offset;
  m_step        = src.m_step;

  m_multimediaRendering = src.m_multimediaRendering;
  m_maxTileSizeIndex    = src.m_maxTileSizeIndex;
  m_threadIndex         = src.m_threadIndex;
  m_subcameraPreview    = src.m_subcameraPreview;

  delete m_renderSettings;
  m_renderSettings = new TRenderSettings(*src.m_renderSettings);

  std::for_each(m_formatProperties.begin(), m_formatProperties.end(),
                ::deleteValue);

  std::map<std::string, TPropertyGroup *>::const_iterator sft,
      sfEnd = src.m_formatProperties.end();
  for (sft = src.m_formatProperties.begin(); sft != sfEnd; ++sft)
    m_formatProperties[sft->first] = sft->second->clone();

  delete m_boardSettings;
  m_boardSettings = new BoardSettings(*src.m_boardSettings);

  return *this;
}

//-------------------------------------------------------------------

TFilePath TOutputProperties::getPath() const { return m_path; }

//-------------------------------------------------------------------

void TOutputProperties::setPath(const TFilePath &fp) { m_path = fp; }

//-------------------------------------------------------------------

void TOutputProperties::setOffset(int off) { m_offset = off; }

//-----------------------------------------------------------------------------

bool TOutputProperties::getRange(int &r0, int &r1, int &step) const {
  step = m_step;
  if (m_from > m_to) {
    r0 = 0;
    r1 = -1;
    return false;
  } else {
    r0 = m_from;
    r1 = m_to;
    return true;
  }
}

//-----------------------------------------------------------------------------

void TOutputProperties::setRange(int r0, int r1, int step) {
  assert(0 <= r0 && r0 <= r1 || r0 == 0 && r1 == -1);
  m_from = r0;
  m_to   = r1;
  m_step = step;
}

//-----------------------------------------------------------------------------

void TOutputProperties::setFrameRate(double fps) { m_frameRate = fps; }

//-------------------------------------------------------------------

TPropertyGroup *TOutputProperties::getFileFormatProperties(std::string ext) {
  std::map<std::string, TPropertyGroup *>::const_iterator it;
  it = m_formatProperties.find(ext);
  if (it == m_formatProperties.end()) {
    TPropertyGroup *ret     = Tiio::makeWriterProperties(ext);
    m_formatProperties[ext] = ret;
    return ret;
  } else
    return it->second;
}

//-------------------------------------------------------------------

void TOutputProperties::getFileFormatPropertiesExtensions(
    std::vector<std::string> &v) const {
  v.reserve(m_formatProperties.size());
  std::map<std::string, TPropertyGroup *>::const_iterator it;
  for (it = m_formatProperties.begin(); it != m_formatProperties.end(); ++it)
    v.push_back(it->first);
}

//-------------------------------------------------------------------

void TOutputProperties::setRenderSettings(
    const TRenderSettings &renderSettings) {
  assert(renderSettings.m_bpp == 32 || renderSettings.m_bpp == 64);
  assert(renderSettings.m_gamma > 0);
  assert(renderSettings.m_quality == TRenderSettings::StandardResampleQuality ||
         renderSettings.m_quality == TRenderSettings::ImprovedResampleQuality ||
         renderSettings.m_quality == TRenderSettings::HighResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Triangle_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Mitchell_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Cubic5_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Cubic75_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Cubic1_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Hann2_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Hann3_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Hamming2_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Hamming3_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Lanczos2_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Lanczos3_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Gauss_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::ClosestPixel_FilterResampleQuality ||
         renderSettings.m_quality ==
             TRenderSettings::Bilinear_FilterResampleQuality);

  *m_renderSettings = renderSettings;
}
