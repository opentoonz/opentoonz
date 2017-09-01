
#include <streambuf>

#include <QStandardPaths>

#include "tfilepath_io.h"
#include "timage_io.h"
#include "trop.h"
#include "tsystem.h"
#include "tvectorimage.h"
#include "tpixelutils.h"
#include "toonz/toonzscene.h"

#include "toonz/mypaintbrushstyle.h"


//*************************************************************************************
//    TMyPaintBrushStyle  implementation
//*************************************************************************************

TMyPaintBrushStyle::TMyPaintBrushStyle()
  { }

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::TMyPaintBrushStyle(const TFilePath &path) {
  loadBrush(path);
}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::TMyPaintBrushStyle(const TMyPaintBrushStyle &other):
  TColorStyle(other),
  m_path          (other.m_path),
  m_fullpath      (other.m_fullpath),
  m_brushOriginal (other.m_brushOriginal),
  m_brushModified (other.m_brushModified),
  m_preview       (other.m_preview),
  m_color         (other.m_color),
  m_baseValues    (other.m_baseValues)
  { }


//-----------------------------------------------------------------------------

TMyPaintBrushStyle::~TMyPaintBrushStyle()
  { }

//-----------------------------------------------------------------------------

TColorStyle& TMyPaintBrushStyle::copy(const TColorStyle &other) {
  const TMyPaintBrushStyle *otherBrushStyle =
    dynamic_cast<const TMyPaintBrushStyle*>(&other);
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

QString TMyPaintBrushStyle::getDescription() const
  { return "MyPaintBrushStyle"; }

//-----------------------------------------------------------------------------

std::string TMyPaintBrushStyle::getBrushType()
  { return "myb"; }

//-----------------------------------------------------------------------------

TFilePathSet TMyPaintBrushStyle::getBrushesDirs() {
  TFilePathSet paths;
  paths.push_back(m_libraryDir + "mypaint brushes");
  QStringList genericLocations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for(QStringList::iterator i = genericLocations.begin(); i != genericLocations.end(); ++i)
    paths.push_back(TFilePath(*i) + "mypaint" + "brushes");
  return paths;
}

//-----------------------------------------------------------------------------

TFilePath TMyPaintBrushStyle::decodePath(const TFilePath &path) const {
  if (path.isAbsolute())
    return path;

  if (m_currentScene) {
    TFilePath p = m_currentScene->decodeFilePath(path);
    TFileStatus fs(p);
    if (fs.doesExist() && !fs.isDirectory())
      return p;
  }

  TFilePathSet paths = getBrushesDirs();
  for(TFilePathSet::iterator i = paths.begin(); i != paths.end(); ++i) {
    TFilePath p = *i + path;
    TFileStatus fs(p);
    if (fs.doesExist() && !fs.isDirectory())
      return p;
  }

  return path;
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::loadBrush(const TFilePath &path) {
  m_path = path;
  m_fullpath = decodePath(path);
  m_brushOriginal.fromDefaults();

  Tifstream is(m_fullpath);
  if (is) {
    std::string str;
    str.assign( std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>() );
    m_brushOriginal.fromString(str);
  }

  m_brushModified = m_brushOriginal;
  std::map<MyPaintBrushSetting, float> baseValuesCopy;
  baseValuesCopy.swap(m_baseValues);
  for(std::map<MyPaintBrushSetting, float>::const_iterator i = baseValuesCopy.begin(); i != baseValuesCopy.end(); ++i)
    setBaseValue(i->first, i->second);

  TFilePath preview_path = m_fullpath.getParentDir() + (m_fullpath.getWideName() + L"_prev.png");
  TImageReader::load(preview_path, m_preview);

  invalidateIcon();
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setBaseValue(MyPaintBrushSetting id, bool enable, float value) {
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
  for(int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i)
    setBaseValueEnabled((MyPaintBrushSetting)i, false);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::makeIcon(const TDimension &d) {
  TFilePath path = m_fullpath.getParentDir() + (m_fullpath.getWideName() + L"_prev.png");
  if (!m_preview) {
    m_icon = TRaster32P(d);
    m_icon->fill(TPixel32::Red);
  } else
  if (m_preview->getSize() == d) {
    m_icon = m_preview;
  } else {
    m_icon = TRaster32P(d);
    double sx = (double)d.lx/(double)m_preview->getLx();
    double sy = (double)d.ly/(double)m_preview->getLy();
    TRop::resample(m_icon, m_preview, TScale(sx, sy));
  }

  // paint color marker
  if (d.lx > 0 && d.ly > 0) {
    int size = std::min( 1 + std::min(d.lx, d.ly)*2/3,
                         1 + std::max(d.lx, d.ly)/2 );
    TPixel32 color = getMainColor();
    for(int y = 0; y < size; ++y) {
      TPixel32 *p = m_icon->pixels(d.ly - y - 1);
      TPixel32 *endp = p + size - y - 1;
      for( ;p != endp; ++p)
        *p = color;
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
  for(int i = 0; i < baseSettingsCount; ++i) {
    std::string key;
    double value = 0.0;
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
  for(std::map<MyPaintBrushSetting, float>::const_iterator i = m_baseValues.begin(); i != m_baseValues.end(); ++i) {
    os << mypaint::Setting::byId(i->first).key;
    os << (double)i->second;
  }
}

//-----------------------------------------------------------------------------

int TMyPaintBrushStyle::getParamCount() const
  { return MYPAINT_BRUSH_SETTINGS_COUNT; }

//-----------------------------------------------------------------------------

QString TMyPaintBrushStyle::getParamNames(int index) const
  { return QString::fromUtf8(mypaint::Setting::byId((MyPaintBrushSetting)index).name.c_str()); }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TMyPaintBrushStyle::getParamType(int index) const
  { return DOUBLE; }

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::hasParamDefault(int index) const
  { return true; }

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setParamDefault(int index)
  { setBaseValueEnabled((MyPaintBrushSetting)index, false); }

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::isParamDefault(int index) const
  { return getBaseValueEnabled((MyPaintBrushSetting)index); }

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::getParamRange(int index, double &min, double &max) const {
  const mypaint::Setting &setting = mypaint::Setting::byId((MyPaintBrushSetting)index);
  min = setting.min;
  max = setting.max;
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setParamValue(int index, double value)
  { setBaseValue((MyPaintBrushSetting)index, value); }

//-----------------------------------------------------------------------------

double TMyPaintBrushStyle::getParamValue(double_tag, int index) const
  { return getBaseValue((MyPaintBrushSetting)index); }

//-----------------------------------------------------------------------------

namespace {
  TColorStyle::Declaration mypaintBrushStyle(new TMyPaintBrushStyle());
}
