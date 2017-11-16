

// TnzCore includes
#include "tpalette.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "texception.h"
#include "tvectorrenderdata.h"
#include "tconvert.h"
#include "tofflinegl.h"
#include "tpixelutils.h"
#include "tflash.h"
#include "tcolorstyles.h"

//*****************************************************************************
//    Macros
//*****************************************************************************

#ifndef checkErrorsByGL
#define checkErrorsByGL                                                        \
  {                                                                            \
    GLenum err = glGetError();                                                 \
    assert(err != GL_INVALID_ENUM);                                            \
    assert(err != GL_INVALID_VALUE);                                           \
    assert(err != GL_INVALID_OPERATION);                                       \
    assert(err != GL_STACK_OVERFLOW);                                          \
    assert(err != GL_STACK_UNDERFLOW);                                         \
    assert(err != GL_OUT_OF_MEMORY);                                           \
    assert(err == GL_NO_ERROR);                                                \
  }
#endif

#undef checkErrorsByGL
#define checkErrorsByGL

//*****************************************************************************
//    TColorStyle  implementation
//*****************************************************************************

int TColorStyle::m_currentFrame = 0;

//-------------------------------------------------------------------

TColorStyle::TColorStyle()
    : m_name(L"color")
    , m_globalName(L"")
    , m_originalName(L"")
    , m_versionNumber(0)
    , m_flags(0)
    , m_enabled(true)
    , m_icon(0)
    , m_validIcon(false)
    , m_isEditedFromOriginal(false)
    , m_pickedPosition() {}

//-------------------------------------------------------------------

TColorStyle::~TColorStyle() {}

//-------------------------------------------------------------------

TColorStyle::TColorStyle(const TColorStyle &other)
    : m_name(other.m_name)
    , m_globalName(other.m_globalName)
    , m_originalName(other.m_originalName)
    , m_versionNumber(other.m_versionNumber)
    , m_flags(other.m_flags)
    , m_enabled(other.m_enabled)
    , m_validIcon(false)
    , m_isEditedFromOriginal(other.m_isEditedFromOriginal)
    , m_pickedPosition(other.m_pickedPosition) {}

//-------------------------------------------------------------------

TColorStyle &TColorStyle::operator=(const TColorStyle &other) {
  m_name                 = other.m_name;
  m_globalName           = other.m_globalName;
  m_originalName         = other.m_originalName;
  m_versionNumber        = other.m_versionNumber;
  m_flags                = other.m_flags;
  m_enabled              = other.m_enabled;
  m_validIcon            = false;
  m_isEditedFromOriginal = other.m_isEditedFromOriginal;
  m_pickedPosition       = other.m_pickedPosition;

  return *this;
}

//-------------------------------------------------------------------

bool TColorStyle::operator==(const TColorStyle &cs) const {
  if (getTagId() != cs.getTagId()) return false;

  if (getMainColor() != cs.getMainColor()) return false;

  int paramCount = getParamCount();
  if (paramCount != cs.getParamCount()) return false;

  int colorParamCount = getColorParamCount();
  if (colorParamCount != cs.getColorParamCount()) return false;

  if (m_name != cs.getName()) return false;
  if (m_originalName != cs.getOriginalName()) return false;
  if (m_globalName != cs.getGlobalName()) return false;
  if (m_isEditedFromOriginal != cs.getIsEditedFlag()) return false;
  if (m_pickedPosition != cs.getPickedPosition()) return false;
  if (m_flags != cs.getFlags()) return false;

  for (int p = 0; p < colorParamCount; ++p)
    if (getColorParamValue(p) != cs.getColorParamValue(p)) return false;

  for (int p = 0; p < paramCount; ++p) {
    switch (getParamType(p)) {
    case BOOL:
      if (getParamValue(bool_tag(), p) != cs.getParamValue(bool_tag(), p))
        return false;
      break;
    case INT:
    case ENUM:
      if (getParamValue(int_tag(), p) != cs.getParamValue(int_tag(), p))
        return false;
      break;
    case DOUBLE:
      if (getParamValue(double_tag(), p) != cs.getParamValue(double_tag(), p))
        return false;
      break;
    case FILEPATH:
      if (getParamValue(TFilePath_tag(), p) !=
          cs.getParamValue(TFilePath_tag(), p))
        return false;
      break;
    default:
      assert(false);
    }
  }

  return true;
}

//-------------------------------------------------------------------

QString TColorStyle::getParamNames(int index) const {
  assert(false);
  return QString("");
}

//-------------------------------------------------------------------

void TColorStyle::updateVersionNumber() { ++m_versionNumber; }

//-------------------------------------------------------------------

const TRaster32P &TColorStyle::getIcon(const TDimension &d) {
  checkErrorsByGL;
  if (!m_validIcon || !m_icon || m_icon->getSize() != d) {
    checkErrorsByGL;
    makeIcon(d);
    checkErrorsByGL;
    m_validIcon = true;
  }
  checkErrorsByGL;

  if (!m_icon) {
    checkErrorsByGL;
    TRaster32P icon(d);
    checkErrorsByGL;
    icon->fill(TPixel32::Black);
    checkErrorsByGL;
    int lx = icon->getLx();
    checkErrorsByGL;
    int ly = icon->getLy();
    checkErrorsByGL;
    for (int y = 0; y < ly; y++) {
      checkErrorsByGL;
      int x = ((lx - 1 - 10) * y / ly);
      checkErrorsByGL;
      icon->extractT(x, y, x + 5, y)->fill(TPixel32::Red);
      checkErrorsByGL;
    }
    checkErrorsByGL;
    m_icon = icon;
    checkErrorsByGL;
  }
  return m_icon;
}

//-------------------------------------------------------------------

void TColorStyle::makeIcon(const TDimension &d) {
  checkErrorsByGL;
  TColorStyle *style = this->clone();
  checkErrorsByGL;

  TPaletteP tmpPalette = new TPalette();
  checkErrorsByGL;
  int id = tmpPalette->addStyle(style);
  checkErrorsByGL;

  int contextLx = pow(2.0, tceil(log((double)d.lx) / log(2.0)));
  int contextLy = pow(2.0, tceil(log((double)d.ly) / log(2.0)));
  TDimension dim(contextLx, contextLy);

  TOfflineGL *glContext = TOfflineGL::getStock(dim);

  checkErrorsByGL;
  glContext->clear(TPixel32::White);
  checkErrorsByGL;

  TVectorImageP img = new TVectorImage;
  checkErrorsByGL;
  img->setPalette(tmpPalette.getPointer());
  checkErrorsByGL;

  std::vector<TThickPoint> points(3);

  if (isRegionStyle() && !isStrokeStyle()) {
    points[0]        = TThickPoint(-55, -50, 1);
    points[1]        = TThickPoint(0, -60, 1);
    points[2]        = TThickPoint(55, -50, 1);
    TStroke *stroke1 = new TStroke(points);

    img->addStroke(stroke1);

    points[0]        = TThickPoint(50, -55, 1);
    points[1]        = TThickPoint(60, 0, 1);
    points[2]        = TThickPoint(50, 55, 1);
    TStroke *stroke2 = new TStroke(points);
    img->addStroke(stroke2);

    points[0]        = TThickPoint(55, 50, 1);
    points[1]        = TThickPoint(0, 60, 1);
    points[2]        = TThickPoint(-55, 50, 1);
    TStroke *stroke3 = new TStroke(points);
    img->addStroke(stroke3);

    points[0]        = TThickPoint(-50, 55, 1);
    points[1]        = TThickPoint(-60, 0, 1);
    points[2]        = TThickPoint(-50, -55, 1);
    TStroke *stroke4 = new TStroke(points);
    img->addStroke(stroke4);

    img->fill(TPointD(0, 0), id);
  } else if (isStrokeStyle() && !isRegionStyle()) {
    double rasX05 = d.lx * 0.5;
    double rasY05 = d.ly * 0.5;

    points[0]        = TThickPoint(-rasX05, -rasY05, 7);
    points[1]        = TThickPoint(0, -rasY05, 9);
    points[2]        = TThickPoint(rasX05, rasY05, 12);
    TStroke *stroke1 = new TStroke(points);

    stroke1->setStyle(id);

    img->addStroke(stroke1);
    points.clear();
  } else if (!isRasterStyle()) {
    assert(isStrokeStyle() && isRegionStyle());

    points[0]        = TThickPoint(-60, -30, 0.5);
    points[1]        = TThickPoint(0, -30, 0.5);
    points[2]        = TThickPoint(60, -30, 0.5);
    TStroke *stroke1 = new TStroke(points);
    stroke1->setStyle(id);
    img->addStroke(stroke1);

    points[0]        = TThickPoint(60, -30, 0.5);
    points[1]        = TThickPoint(60, 0, 0.5);
    points[2]        = TThickPoint(60, 30, 0.5);
    TStroke *stroke2 = new TStroke(points);
    stroke2->setStyle(id);
    img->addStroke(stroke2);

    points[0]        = TThickPoint(60, 30, 0.5);
    points[1]        = TThickPoint(0, 30, 0.5);
    points[2]        = TThickPoint(-60, 30, 0.5);
    TStroke *stroke3 = new TStroke(points);
    stroke3->setStyle(id);
    img->addStroke(stroke3);

    points[0]        = TThickPoint(-60, 30, 0.5);
    points[1]        = TThickPoint(-60, 0, 0.5);
    points[2]        = TThickPoint(-60, -30, 0.5);
    TStroke *stroke4 = new TStroke(points);
    stroke4->setStyle(id);
    img->addStroke(stroke4);

    img->fill(TPointD(0, 0), id);
  }

  TRectD bbox = img->getBBox();
  checkErrorsByGL;

  bbox = bbox.enlarge(TDimensionD(-10, -10));
  checkErrorsByGL;

  double scx = 0.9 * d.lx / bbox.getLx();
  double scy = 0.9 * d.ly / bbox.getLy();
  double sc  = std::min(scx, scy);
  double dx  = (d.lx - bbox.getLx() * sc) * 0.5;
  double dy  = (d.ly - bbox.getLy() * sc) * 0.5;
  TAffine aff =
      TScale(scx, scy) * TTranslation(-bbox.getP00() + TPointD(dx, dy));

  checkErrorsByGL;
  if (isRegionStyle() && !isStrokeStyle()) aff = aff * TTranslation(-10, -10);

  checkErrorsByGL;
  const TVectorRenderData rd(aff, TRect(), tmpPalette.getPointer(), 0, true);
  checkErrorsByGL;
  glContext->draw(img, rd);
  checkErrorsByGL;

  TRect rect(d);
  if (!m_icon || m_icon->getSize() != d) {
    checkErrorsByGL;
    m_icon = glContext->getRaster()->extract(rect)->clone();
  } else {
    checkErrorsByGL;
    m_icon->copy(glContext->getRaster()->extract(rect));
  }
}

//-------------------------------------------------------------------

void TColorStyle::assignNames(const TColorStyle *src) {
  m_name                 = src->getName();
  m_globalName           = src->getGlobalName();
  m_originalName         = src->getOriginalName();
  m_isEditedFromOriginal = src->getIsEditedFlag();
}

//-------------------------------------------------------------------

void TColorStyle::assignBlend(const TColorStyle &a, const TColorStyle &b,
                              double t) {
  // Blend colors
  {
    int col, colCount = getColorParamCount();
    assert(a.getColorParamCount() == colCount &&
           b.getColorParamCount() == colCount);

    for (col = 0; col != colCount; ++col)
      setColorParamValue(
          col, blend(a.getColorParamValue(col), b.getColorParamValue(col), t));
  }

  // Blend parameters
  {
    int par, parCount = getParamCount();
    assert(a.getParamCount() == parCount && b.getParamCount() == parCount);

    for (par = 0; par != parCount; ++par) {
      switch (getParamType(par)) {
      case DOUBLE:
        setParamValue(par, (1 - t) * a.getParamValue(double_tag(), par) +
                               t * b.getParamValue(double_tag(), par));
        break;
      default:
        break;
      }
    }
  }

  invalidateIcon();
}

//===================================================================
//
// color style global list
//
//===================================================================

namespace {

class ColorStyleList {  // singleton
  ColorStyleList() {}

  struct Item {
    TColorStyle *m_style;
    bool m_isObsolete;
    //    Item() : m_style(0), m_isObsolete(false) { assert(0); }
    Item(TColorStyle *style, bool obsolete = false)
        : m_style(style), m_isObsolete(obsolete) {}
  };

  typedef std::map<int, Item> Table;
  Table m_table;

public:
  static ColorStyleList *instance() {
    static ColorStyleList *_instance = 0;
    if (!_instance) _instance        = new ColorStyleList();
    return _instance;
  }

  int getStyleCount() { return int(m_table.size()); }

  void declare(TColorStyle *style) {
    int id = style->getTagId();
    if (m_table.find(id) != m_table.end()) {
      throw TException("Duplicate color style declaration. id = " +
                       std::to_string(id));
    }
    m_table.insert(std::make_pair(id, Item(style)));
    std::vector<int> ids;
    style->getObsoleteTagIds(ids);
    for (std::vector<int>::iterator it = ids.begin(); it != ids.end(); ++it) {
      if (m_table.find(*it) != m_table.end()) {
        throw TException(
            "Duplicate color style declaration for obsolete style. id = " +
            std::to_string(*it));
      }
      m_table.insert(std::make_pair(*it, Item(style->clone(), true)));
    }
  }

  TColorStyle *create(int id, bool &isObsolete) {
    Table::iterator it = m_table.find(id);
    if (it == m_table.end())
      throw TException("Unknown color style id; id = " + std::to_string(id));

    isObsolete = it->second.m_isObsolete;

    return it->second.m_style->clone();
  }

  void getAllTags(std::vector<int> &tags) {
    tags.clear();
    tags.reserve(m_table.size());
    for (Table::iterator it = m_table.begin(); it != m_table.end(); ++it)
      if (!it->second.m_isObsolete) tags.push_back(it->first);
  }

  ~ColorStyleList() {
    Table::iterator it = m_table.begin();
    for (; it != m_table.end(); ++it) {
      delete it->second.m_style;
    }
  }

private:
  // not implemented
  ColorStyleList(const ColorStyleList &);
  ColorStyleList &operator=(const ColorStyleList &);
};

//-----------------------------------------------------------------------------

}  // namespace

//===================================================================

void TColorStyle::declare(TColorStyle *style) {
  ColorStyleList::instance()->declare(style);
}

//===================================================================

static double computeAverageThickness(const TStroke *s, double &minThickness,
                                      double &maxThickness) {
  int count = s->getControlPointCount();

  minThickness    = 1000;
  maxThickness    = -1;
  double resThick = 0;

  for (int i = 0; i < s->getControlPointCount(); i++) {
    double thick = s->getControlPoint(i).thick;
    if (i >= 2 && i < s->getControlPointCount() - 2) resThick += thick;

    if (thick < minThickness) minThickness = thick;
    if (thick > maxThickness) maxThickness = thick;
  }

  if (count < 6) return s->getControlPoint(count / 2 + 1).thick;
  return resThick / (s->getControlPointCount() - 4);
}

void TColorStyle::drawStroke(TFlash &flash, const TStroke *s) const {
  bool isCenterline = false;
  double minThickness, maxThickness = 0;
  std::wstring quality = flash.getLineQuality();
  double thickness     = computeAverageThickness(s, minThickness, maxThickness);
  if (minThickness == maxThickness && minThickness == 0) return;
  if (quality == TFlash::ConstantLines)
    isCenterline = true;
  else if (quality == TFlash::MixedLines &&
           (maxThickness == 0 || minThickness / maxThickness > 0.5))
    isCenterline = true;
  else if (quality == TFlash::VariableLines &&
           maxThickness - minThickness <
               0.16)  // Quando si salva il pli, si approssima al thick.
                      // L'errore di approx e' sempre 0.1568...
    isCenterline = true;
  // else	assert(false);

  flash.setFillColor(getAverageColor());
  // flash.setFillColor(TPixel::Red);

  TStroke *saux = const_cast<TStroke *>(s);
  if (isCenterline) {
    saux->setAverageThickness(thickness);
    flash.setThickness(s->getAverageThickness());
    flash.setLineColor(getAverageColor());
    flash.drawCenterline(s, false);
  } else {
    saux->setAverageThickness(0);
    if (!flash.drawOutline(saux)) {
      flash.setThickness(thickness);
      flash.setLineColor(getAverageColor());
      flash.drawCenterline(s, false);
    }
  }
}

//-----------------------------------------------------------------------------
// Format: _123 |global name
// _123 = flag; optional (*)
// |global = global name; optional
// name = color name; mandatory (??)

// note: If name starts with a digit or by '_', another '_'
// is added.
// (*): In such case, the flag is mandatory.
void TColorStyle::save(TOutputStreamInterface &os) const {
  std::wstring name = getName();
  bool numberedName =
      !name.empty() && ('0' <= name[0] && name[0] <= '9' || name[0] == '_');

  if (m_flags > 0 || (name.length() == 1 && numberedName))
    os << ("_" + QString::number(m_flags)).toStdString();
  std::wstring gname    = getGlobalName();
  std::wstring origName = getOriginalName();

  if (gname != L"") {
    os << ::to_string(L"|" + gname);

    // save the original name from studio palette
    if (origName != L"") {
      // write two "@"s if the edited flag is ON
      os << ::to_string(((m_isEditedFromOriginal) ? L"@@" : L"@") + origName);
    }
  }

  if (numberedName) name.insert(0, L"_");

  os << ::to_string(name) << getTagId();
  saveData(os);
}

//-------------------------------------------------------------------

TColorStyle *TColorStyle::load(TInputStreamInterface &is) {
  std::string name;
  std::wstring gname;
  std::wstring origName;
  bool isEdited = false;

  is >> name;
  unsigned int flags = 0;
  if (name.length() == 2 && name[0] == '_' && '0' <= name[1] &&
      name[1] <= '9') {
    flags = QString::fromStdString(name.substr(1)).toUInt();
    is >> name;
  }
  if (name.length() > 0 && name[0] == '|') {
    gname = ::to_wstring(name.substr(1));
    is >> name;

    // If the style is copied from studio palette, original name is here
    if (name.length() > 0 && name[0] == '@') {
      // if there are two "@"s, then activate the edited flag
      if (name[1] == '@') {
        isEdited = true;
        origName = ::to_wstring(name.substr(2));
      } else {
        origName = ::to_wstring(name.substr(1));
      }
      is >> name;
    }
  }
  int id = 0;
  if (!name.empty() && '0' <= name[0] && name[0] <= '9') {
    id   = std::stoi(name);
    name = "color";
  } else {
    if (!name.empty() && name[0] == '_') name.erase(name.begin());
    is >> id;
  }
  bool isObsolete    = false;
  TColorStyle *style = ColorStyleList::instance()->create(id, isObsolete);
  assert(style);
  style->setFlags(flags);
  if (isObsolete)
    style->loadData(id, is);
  else
    style->loadData(is);
  style->setName(::to_wstring(name));
  style->setGlobalName(gname);
  style->setOriginalName(origName);
  style->setIsEditedFlag(isEdited);
  return style;
}

//-------------------------------------------------------------------

TColorStyle *TColorStyle::create(int tagId) {
  bool isObsolete = false;
  return ColorStyleList::instance()->create(tagId, isObsolete);
}

//-------------------------------------------------------------------

void TColorStyle::getAllTags(std::vector<int> &tags) {
  ColorStyleList::instance()->getAllTags(tags);
}
