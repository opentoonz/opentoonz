
#include <streambuf>

#include <QStandardPaths>

#include "tfilepath_io.h"
#include "timage_io.h"
#include "trop.h"
#include "tsystem.h"
#include "tvectorimage.h"
#include "tpixelutils.h"
#include "toonz/toonzscene.h"

#include "tstrokeprop.h"
#include "tvectorrenderdata.h"
#include "tgl.h"
#include "tmathutil.h"

#include "toonz/mypaintbrushstyle.h"

#include <QDebug>

#include <sstream>

//=============================================================================
MyPaintBrushStrokeProp::MyPaintBrushStrokeProp(const TStroke *stroke,
                                               TMyPaintBrushStyle *style)
    : TStrokeProp(stroke)
    , m_colorStyle(style)
    , m_outline()
    , m_outlinePixelSize(0) {
  m_styleVersionNumber = m_colorStyle->getVersionNumber();
  m_altStyle.setMainColor(style->getMainColor());
}

//-----------------------------------------------------------------------------

TStrokeProp *MyPaintBrushStrokeProp::clone(const TStroke *stroke) const {
  MyPaintBrushStrokeProp *prop =
      new MyPaintBrushStrokeProp(stroke, m_colorStyle);
  prop->m_strokeChanged    = m_strokeChanged;
  prop->m_outline          = m_outline;
  prop->m_outlinePixelSize = m_outlinePixelSize;
  return prop;
}

//-----------------------------------------------------------------------------

const TColorStyle *MyPaintBrushStrokeProp::getColorStyle() const {
  return m_colorStyle;
}

//-----------------------------------------------------------------------------

void MyPaintBrushStrokeProp::draw(const TVectorRenderData &rd) {
  if (rd.m_clippingRect != TRect() && !rd.m_is3dView &&
      !convert(rd.m_aff * m_stroke->getBBox()).overlaps(rd.m_clippingRect))
    return;

  glPushMatrix();
  tglMultMatrix(rd.m_aff);

  double pixelSize = sqrt(tglGetPixelSize2());

  if (m_stroke->isCenterLine()) {
    TCenterLineStrokeStyle *appStyle =
        new TCenterLineStrokeStyle(m_colorStyle->getAverageColor(), 0, 0);
    appStyle->drawStroke(rd.m_cf, m_stroke);
    delete appStyle;
  } else {
    if (!isAlmostZero(pixelSize - m_outlinePixelSize, 1e-5) ||
        m_strokeChanged ||
        m_styleVersionNumber != m_colorStyle->getVersionNumber()) {
      m_strokeChanged    = false;
      m_outlinePixelSize = pixelSize;
      TOutlineUtil::OutlineParameter param;

      m_outline.getArray().clear();
      m_altStyle.computeOutline(m_stroke, m_outline, param);
      m_styleVersionNumber = m_colorStyle->getVersionNumber();
    }

    if (rd.m_antiAliasing)
      m_altStyle.drawStroke(rd.m_cf, &m_outline, m_stroke);
    else
      m_altStyle.drawAliasedStroke(rd.m_cf, &m_outline, m_stroke);
  }

  glPopMatrix();
}

//*************************************************************************************
//    TMyPaintBrushStyle  implementation
//*************************************************************************************

TMyPaintBrushStyle::TMyPaintBrushStyle() {}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::TMyPaintBrushStyle(const TFilePath &path) {
  loadBrush(path);
}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::TMyPaintBrushStyle(const TMyPaintBrushStyle &other)
    : TColorStyle(other)
    , m_path(other.m_path)
    , m_fullpath(other.m_fullpath)
    , m_brushOriginal(other.m_brushOriginal)
    , m_brushModified(other.m_brushModified)
    , m_preview(other.m_preview)
    , m_color(other.m_color)
    , m_baseValues(other.m_baseValues) {}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::~TMyPaintBrushStyle() {}

//-----------------------------------------------------------------------------

TColorStyle *TMyPaintBrushStyle::clone(std::string brushIdName) const {
  TMyPaintBrushStyle *style = new TMyPaintBrushStyle(*this);
  style->loadBrush(TFilePath(getBrushIdNameParam(brushIdName)));
  return style;
}

//-----------------------------------------------------------------------------

TColorStyle &TMyPaintBrushStyle::copy(const TColorStyle &other) {
  const TMyPaintBrushStyle *otherBrushStyle =
      dynamic_cast<const TMyPaintBrushStyle *>(&other);
  if (otherBrushStyle) {
    m_path          = otherBrushStyle->m_path;
    m_fullpath      = otherBrushStyle->m_fullpath;
    m_brushOriginal = otherBrushStyle->m_brushOriginal;
    m_brushModified = otherBrushStyle->m_brushModified;
    m_preview       = otherBrushStyle->m_preview;
    m_baseValues    = otherBrushStyle->m_baseValues;
  }
  assignBlend(other, other, 0.0);
  return *this;
}

//-----------------------------------------------------------------------------

QString TMyPaintBrushStyle::getDescription() const {
  return "MyPaintBrushStyle";
}

//-----------------------------------------------------------------------------

std::string TMyPaintBrushStyle::getBrushIdName() const {
  std::wstring ws = m_path.getWideString();
  const std::string s(ws.begin(), ws.end());
  return "MyPaintBrushStyle:" + s;
}

//-----------------------------------------------------------------------------

std::string TMyPaintBrushStyle::getBrushType() { return "myb"; }

//-----------------------------------------------------------------------------

TFilePathSet TMyPaintBrushStyle::getBrushesDirs() {
  TFilePathSet paths;
  paths.push_back(m_libraryDir + "mypaint brushes");
  QStringList genericLocations =
      QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for (QStringList::iterator i = genericLocations.begin();
       i != genericLocations.end(); ++i)
    paths.push_back(TFilePath(*i) + "mypaint" + "brushes");
  return paths;
}

//-----------------------------------------------------------------------------

TFilePath TMyPaintBrushStyle::decodePath(const TFilePath &path) const {
  if (path.isAbsolute()) return path;

  if (m_currentScene) {
    TFilePath p = m_currentScene->decodeFilePath(path);
    TFileStatus fs(p);
    if (fs.doesExist() && !fs.isDirectory()) return p;
  }

  TFilePathSet paths = getBrushesDirs();
  for (TFilePathSet::iterator i = paths.begin(); i != paths.end(); ++i) {
    TFilePath p = *i + path;
    TFileStatus fs(p);
    if (fs.doesExist() && !fs.isDirectory()) return p;
  }

  return path;
}

//-----------------------------------------------------------------------------
static std::string mybToVersion3(std::string origStr) {
  std::string outStr   = "";
  std::string comment  = "";
  bool settingsStarted = false;

  std::stringstream strstream(origStr);

  outStr += "{\n";
  for (std::string line; std::getline(strstream, line);) {
    std::replace(line.begin(), line.end(), '\r', ' ');
    if (line.find("version 2") != std::string::npos)
      continue;  // Ignore version.  We'll add version back later as version 3

    if (line[0] == '#') {
      comment = comment + line;
      continue;
    }

    if (!settingsStarted) {
      settingsStarted = true;
      outStr += "    \"comment\": \"" + comment + "\",\n";
      outStr += "    \"group\": \"\",\n";
      outStr += "    \"parent_brush_name\": \"\",\n";
      outStr += "    \"settings\": {\n";
    } else
      outStr += ",\n";

    int startPos = 0;
    int pipe     = line.find("|");

    std::string settingInfo = line.substr(startPos, pipe);
    std::string setting     = settingInfo.substr(startPos, line.find(" "));
    std::string baseValue =
        settingInfo.substr(setting.length(), settingInfo.find_last_not_of(" "));
    outStr += "        \"" + setting + "\": {\n";
    outStr += "            \"base_value\": " + baseValue + ",\n";
    if (pipe == std::string::npos)
      outStr += "            \"inputs\": {}";
    else {
      outStr += "            \"inputs\": {\n";
      while (pipe != line.length()) {
        if (startPos > 0) outStr += ",\n";
        startPos = pipe + 1;
        pipe     = line.find("|", startPos);
        if (pipe == std::string::npos) pipe = line.length();
        settingInfo      = line.substr(startPos, pipe - startPos);
        int firstCharPos = settingInfo.find_first_not_of(" ");
        setting          = settingInfo.substr(firstCharPos,
                                              settingInfo.find(" ", firstCharPos) - 1);
        outStr += "                \"" + setting + "\": [\n";
        firstCharPos = settingInfo.find_first_not_of(" ", setting.length() + 1);
        baseValue    = settingInfo.substr(firstCharPos, pipe - startPos);
        int comma    = baseValue.find(", ");
        if (comma == std::string::npos) comma = baseValue.length();
        std::string value = baseValue.substr(0, comma);
        std::replace(value.begin(), value.end(), '(', '[');
        std::replace(value.begin(), value.end(), ')', ']');
        std::replace(value.begin(), value.end(), ' ', ',');
        outStr += "                    " + value;
        int prevComma;
        while (comma != baseValue.length()) {
          outStr += ",\n";
          prevComma = comma + 2;
          comma     = baseValue.find(", ", prevComma);
          if (comma == std::string::npos) comma = baseValue.length();
          value = baseValue.substr(prevComma, comma - prevComma);
          std::replace(value.begin(), value.end(), '(', '[');
          std::replace(value.begin(), value.end(), ')', ']');
          std::replace(value.begin(), value.end(), ' ', ',');
          outStr += "                    " + value;
        }
        outStr += "\n                ]";  // Ends Input setting
      }
      outStr += "\n            }";  // End Inputs
    }
    outStr += "\n        }";  // End Setting
  }
  outStr += "\n";

  if (settingsStarted) {
    outStr += "    },\n";  // End Settings
    outStr += "    \"version\": 3\n";
  }

  outStr += "}";  // Ends file
  return outStr;
}

void TMyPaintBrushStyle::loadBrush(const TFilePath &path) {
  m_path     = path;
  m_fullpath = decodePath(path);
  m_brushOriginal.fromDefaults();

  Tifstream is(m_fullpath);
  if (is) {
    std::string str;
    str.assign(std::istreambuf_iterator<char>(is),
               std::istreambuf_iterator<char>());
    if (str.find("version 2") != std::string::npos) str = mybToVersion3(str);
    m_brushOriginal.fromString(str);
  }

  m_brushModified = m_brushOriginal;
  std::map<MyPaintBrushSetting, float> baseValuesCopy;
  baseValuesCopy.swap(m_baseValues);
  for (std::map<MyPaintBrushSetting, float>::const_iterator i =
           baseValuesCopy.begin();
       i != baseValuesCopy.end(); ++i)
    setBaseValue(i->first, i->second);

  TFilePath preview_path =
      m_fullpath.getParentDir() + (m_fullpath.getWideName() + L"_prev.png");
  TImageReader::load(preview_path, m_preview);

  invalidateIcon();
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setBaseValue(MyPaintBrushSetting id, bool enable,
                                      float value) {
  float def = m_brushOriginal.getBaseValue(id);
  if (enable && fabsf(value - def) > 0.01) {
    m_baseValues[id] = value;
    m_brushModified.setBaseValue(id, value);
  } else {
    m_baseValues.erase(id);
    m_brushModified.setBaseValue(id, def);
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::resetBaseValues() {
  for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i)
    setBaseValueEnabled((MyPaintBrushSetting)i, false);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::makeIcon(const TDimension &d) {
  TFilePath path =
      m_fullpath.getParentDir() + (m_fullpath.getWideName() + L"_prev.png");
  TPointD offset(0, 0);
  if (!m_preview) {
    m_icon = TRaster32P(d);
    m_icon->fill(TPixel32::Red);
  } else if (m_preview->getSize() == d) {
    m_icon = m_preview;
  } else {
    m_icon = TRaster32P(d);
    if (d.lx != d.ly) {
      TPixel32 col = getMainColor();
      if (col.m == 255)
        m_icon->fill(col);
      else {
        TRaster32P fg(d);
        fg->fill(premultiply(col));
        TRop::checkBoard(m_icon, TPixel32::Black, TPixel32::White,
                         TDimensionD(6, 6), TPointD());
        TRop::over(m_icon, fg);
      }
    }
    double sx    = (double)d.lx / (double)m_preview->getLx();
    double sy    = (double)d.ly / (double)m_preview->getLy();
    double scale = std::min(sx, sy);
    TRaster32P resamplePreview(m_preview->getLx(), m_preview->getLy());
    TRop::resample(resamplePreview, m_preview, TScale(scale),
                   TRop::ResampleFilterType::Hamming3);
    TRop::over(m_icon, resamplePreview);
  }

  // paint color marker
  // Only show color marker when the icon size is 22x22
  if (d.lx == d.ly && d.lx <= 22) {
    int size       = std::min(1 + std::min(d.lx, d.ly) * 2 / 3,
                              1 + std::max(d.lx, d.ly) / 2);
    TPixel32 color = getMainColor();
    color.m        = 255;  // show full opac color
    for (int y = 0; y < size; ++y) {
      TPixel32 *p    = m_icon->pixels(d.ly - y - 1);
      TPixel32 *endp = p + size - y - 1;
      for (; p != endp; ++p) *p = color;
      *p = blend(*p, color, 0.5);
    }
  }
}

//------------------------------------------------------------

void TMyPaintBrushStyle::loadData(TInputStreamInterface &is) {
  std::string path;
  is >> path;
  is >> m_color;
  loadBrush(TFilePath(path));

  int baseSettingsCount = 0;
  is >> baseSettingsCount;
  for (int i = 0; i < baseSettingsCount; ++i) {
    std::string key;
    double value    = 0.0;
    int inputsCount = 0;
    is >> key;
    is >> value;
    const mypaint::Setting *setting = mypaint::Setting::findByKey(key);
    if (setting) setBaseValue(setting->id, value);
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::saveData(TOutputStreamInterface &os) const {
  std::wstring wstr = m_path.getWideString();
  std::string str;
  str.assign(wstr.begin(), wstr.end());
  os << str;
  os << m_color;

  os << (int)m_baseValues.size();
  for (std::map<MyPaintBrushSetting, float>::const_iterator i =
           m_baseValues.begin();
       i != m_baseValues.end(); ++i) {
    os << mypaint::Setting::byId(i->first).key;
    os << (double)i->second;
  }
}

//-----------------------------------------------------------------------------

int TMyPaintBrushStyle::getParamCount() const {
  return MYPAINT_BRUSH_SETTINGS_COUNT;
}

//-----------------------------------------------------------------------------

QString TMyPaintBrushStyle::getParamNames(int index) const {
  return QString::fromUtf8(
      mypaint::Setting::byId((MyPaintBrushSetting)index).name.c_str());
}

//-----------------------------------------------------------------------------

TColorStyle::ParamType TMyPaintBrushStyle::getParamType(int index) const {
  return DOUBLE;
}

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::hasParamDefault(int index) const { return true; }

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setParamDefault(int index) {
  setBaseValueEnabled((MyPaintBrushSetting)index, false);
}

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::isParamDefault(int index) const {
  return getBaseValueEnabled((MyPaintBrushSetting)index);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::getParamRange(int index, double &min,
                                       double &max) const {
  const mypaint::Setting &setting =
      mypaint::Setting::byId((MyPaintBrushSetting)index);
  min = setting.min;
  max = setting.max;
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setParamValue(int index, double value) {
  setBaseValue((MyPaintBrushSetting)index, value);
}

//-----------------------------------------------------------------------------

double TMyPaintBrushStyle::getParamValue(double_tag, int index) const {
  return getBaseValue((MyPaintBrushSetting)index);
}

//-----------------------------------------------------------------------------

namespace {
TColorStyle::Declaration mypaintBrushStyle(new TMyPaintBrushStyle());
}
