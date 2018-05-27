

// TnzCore includes
#include "tsimplecolorstyles.h"
#include "timage_io.h"
#include "tconvert.h"
#include "tvectorimage.h"
#include "tpixelutils.h"
#include "tsystem.h"
#include "tstream.h"

// Qt includes
#include <QMutexLocker>

#include "tpalette.h"

#include <memory>
#include <sstream>

PERSIST_IDENTIFIER(TPalette, "palette")

TPersistDeclarationT<TPalette> auxPaletteDeclaration("vectorpalette");

DEFINE_CLASS_CODE(TPalette, 30)

//*************************************************************************************
//    Local namespace  stuff
//*************************************************************************************

namespace {

const std::string pointToString(const TColorStyle::PickedPosition &point) {
  if (point.frame == 0)
    return std::to_string(point.pos.x) + "," + std::to_string(point.pos.y);
  else
    return std::to_string(point.pos.x) + "," + std::to_string(point.pos.y) +
           "," + std::to_string(point.frame);
}

// splitting string with ','
const TColorStyle::PickedPosition stringToPoint(const std::string &string) {
  std::string buffer;
  std::stringstream ss(string);
  std::vector<std::string> result;
  while (std::getline(ss, buffer, ','))  // split with comma
    result.push_back(buffer);

  int x     = std::stoi(result[0]);
  int y     = std::stoi(result[1]);
  int frame = 0;
  if (result.size() == 3)  // getting the third part of string - if any.
    frame = std::stoi(result[2]);

  return {TPoint(x, y), frame};
}

}  // namespace

//===================================================================
//
// TPalette::Page
//
//-------------------------------------------------------------------

TPalette::Page::Page(std::wstring name)
    : m_name(name), m_index(-1), m_palette(0) {}

//-------------------------------------------------------------------

TColorStyle *TPalette::Page::getStyle(int index) const {
  assert(m_palette);
  if (0 <= index && index < getStyleCount())
    return m_palette->getStyle(m_styleIds[index]);
  else
    return 0;
}

//-------------------------------------------------------------------

int TPalette::Page::getStyleId(int index) const {
  assert(m_palette);
  if (0 <= index && index < getStyleCount())
    return m_styleIds[index];
  else
    return -1;
}

//-------------------------------------------------------------------

int TPalette::Page::addStyle(int styleId) {
  assert(m_palette);
  if (styleId < 0 || styleId >= m_palette->getStyleCount()) return -1;
  if (m_palette->m_styles[styleId].first != 0) return -1;
  m_palette->m_styles[styleId].first = this;
  int indexInPage                    = int(m_styleIds.size());
  m_styleIds.push_back(styleId);
  return indexInPage;
}

//-------------------------------------------------------------------

int TPalette::Page::addStyle(TColorStyle *style) {
  assert(m_palette);
  int stylesCount = int(m_palette->m_styles.size());
  int styleId;
  for (styleId = 0; styleId < stylesCount; styleId++)
    if (m_palette->m_styles[styleId].first == 0) break;
  if (styleId >= stylesCount - 1) return addStyle(m_palette->addStyle(style));
  m_palette->setStyle(styleId, style);
  return addStyle(styleId);
}

//-------------------------------------------------------------------

int TPalette::Page::addStyle(TPixel32 color) {
  return addStyle(new TSolidColorStyle(color));
}

//-------------------------------------------------------------------

void TPalette::Page::insertStyle(int indexInPage, int styleId) {
  assert(m_palette);
  if (styleId < 0 || styleId >= m_palette->getStyleCount()) return;
  if (m_palette->m_styles[styleId].first != 0) return;
  m_palette->m_styles[styleId].first = this;
  if (indexInPage < 0)
    indexInPage = 0;
  else if (indexInPage > getStyleCount())
    indexInPage = getStyleCount();
  m_styleIds.insert(m_styleIds.begin() + indexInPage, styleId);
}

//-------------------------------------------------------------------

void TPalette::Page::insertStyle(int indexInPage, TColorStyle *style) {
  assert(m_palette);
  int styleId = m_palette->addStyle(style);
  if (styleId >= 0) insertStyle(indexInPage, styleId);
}

//-------------------------------------------------------------------

void TPalette::Page::insertStyle(int indexInPage, TPixel32 color) {
  assert(m_palette);
  int styleId = m_palette->addStyle(color);
  if (styleId >= 0) insertStyle(indexInPage, styleId);
}

//-------------------------------------------------------------------

void TPalette::Page::removeStyle(int indexInPage) {
  if (indexInPage < 0 || indexInPage >= getStyleCount()) return;
  assert(m_palette);
  int styleId = getStyleId(indexInPage);
  assert(0 <= styleId && styleId < m_palette->getStyleCount());
  assert(m_palette->m_styles[styleId].first == this);
  m_palette->m_styles[styleId].first = 0;
  m_styleIds.erase(m_styleIds.begin() + indexInPage);
}

//-------------------------------------------------------------------

int TPalette::Page::search(int styleId) const {
  std::vector<int>::const_iterator it =
      std::find(m_styleIds.begin(), m_styleIds.end(), styleId);
  if (it == m_styleIds.end())
    return -1;
  else
    return it - m_styleIds.begin();
}

//-------------------------------------------------------------------

int TPalette::Page::search(TColorStyle *style) const {
  assert(style);
  assert(m_palette);
  for (int i = 0; i < getStyleCount(); i++)
    if (m_palette->getStyle(m_styleIds[i]) == style) return i;
  return -1;
}

//===================================================================
//
// TPalette
//
//-------------------------------------------------------------------

TPalette::TPalette()
    : m_version(0)
    , m_isCleanupPalette(false)
    , m_currentFrame(-1)
    , m_dirtyFlag(false)
    , m_mutex(QMutex::Recursive)
    , m_isLocked(false)
    , m_askOverwriteFlag(false)
    , m_shortcutScopeIndex(0) {
  QString tempName(QObject::tr("colors"));
  std::wstring pageName = tempName.toStdWString();
  Page *page            = addPage(pageName);
  page->addStyle(TPixel32(255, 255, 255, 0));
  page->addStyle(TPixel32(0, 0, 0, 255));
  getStyle(0)->setName(L"color_0");
  getStyle(1)->setName(L"color_1");

  for (int i = 0; i < 10; i++) m_shortcuts['0' + i] = i;
}

//-------------------------------------------------------------------

TPalette::~TPalette() {
  std::set<TColorStyle *> table;
  int i = 0;
  for (i = 0; i < getStyleCount(); i++) {
    assert(table.find(getStyle(i)) == table.end());
    table.insert(getStyle(i));
  }
  clearPointerContainer(m_pages);
}

//-------------------------------------------------------------------

TPalette *TPalette::clone() const {
  TPalette *palette = new TPalette;
  palette->assign(this);
  return palette;
}

//-------------------------------------------------------------------

TColorStyle *TPalette::getStyle(int index) const {
  if (0 <= index && index < getStyleCount())
    return m_styles[index].second.getPointer();
  else {
    static TSolidColorStyle *ss = new TSolidColorStyle(TPixel32::Red);
    ss->addRef();
    return ss;
  }
}

//-------------------------------------------------------------------

int TPalette::getStyleInPagesCount() const {
  int styleInPagesCount = 0;
  for (int i = 0; i < getStyleCount(); i++)
    if (m_styles[i].first != 0) styleInPagesCount++;
  return styleInPagesCount;
}

//-------------------------------------------------------------------

int TPalette::getFirstUnpagedStyle() const {
  for (int i = 0; i < getStyleCount(); i++)
    if (m_styles[i].first == 0) return i;
  return -1;
}

//-------------------------------------------------------------------
/*! Adding style with new styleId. Even if there are deleted styles in the
 * palette, the new style will be appended to the end of the list.
*/
int TPalette::addStyle(TColorStyle *style) {
  // limit the number of cleanup style to 7
  if (isCleanupPalette() && getStyleInPagesCount() >= 8) return -1;

  int styleId = int(m_styles.size());
  if (styleId < 4096) {
    // checking if the style is overlapped
    int i = 0;
    for (i = 0; i < styleId; i++)
      if (getStyle(i) == style) break;
    if (i == styleId) {
      m_styles.push_back(std::make_pair((Page *)0, style));
      return styleId;
    }
  }
  delete style;
  return -1;
}

//-------------------------------------------------------------------

int TPalette::addStyle(const TPixel32 &color) {
  return addStyle(new TSolidColorStyle(color));
}

//-------------------------------------------------------------------

void TPalette::setStyle(int styleId, TColorStyle *style) {
  std::unique_ptr<TColorStyle> styleOwner(style);

  int styleCount = getStyleCount();

  if (0 <= styleId && styleId < styleCount) {
    // Find out if the supplied style is already in the palette
    // with a different style id. In that case, bail out as a noop.
    for (int i = 0; i < styleCount; ++i)
      if (style == getStyle(i)) return;

    // Substitution can take place
    if (typeid(*m_styles[styleId].second.getPointer()) != typeid(*style))
      m_styleAnimationTable.erase(styleId);

    m_styles[styleId].second = styleOwner.release();
  }
}

//-------------------------------------------------------------------

void TPalette::setStyle(int styleId, const TPixelRGBM32 &color) {
  setStyle(styleId, new TSolidColorStyle(color));
}

//-------------------------------------------------------------------

int TPalette::getPageCount() const { return int(m_pages.size()); }

//-------------------------------------------------------------------

TPalette::Page *TPalette::getPage(int pageIndex) {
  if (0 <= pageIndex && pageIndex < getPageCount()) {
    Page *page = m_pages[pageIndex];
    assert(page->getIndex() == pageIndex);
    assert(page->m_palette == this);
    return page;
  } else
    return 0;
}

//-------------------------------------------------------------------

const TPalette::Page *TPalette::getPage(int pageIndex) const {
  if (0 <= pageIndex && pageIndex < getPageCount()) {
    Page *page = m_pages[pageIndex];
    assert(page->getIndex() == pageIndex);
    assert(page->m_palette == this);
    return page;
  } else
    return 0;
}

//-------------------------------------------------------------------

TPalette::Page *TPalette::addPage(std::wstring name) {
  Page *page      = new Page(name);
  page->m_index   = getPageCount();
  page->m_palette = this;
  m_pages.push_back(page);
  return page;
}

//-------------------------------------------------------------------

void TPalette::erasePage(int index) {
  Page *page = getPage(index);
  if (!page) return;
  m_pages.erase(m_pages.begin() + index);
  int i;
  for (i = 0; i < getPageCount(); i++) m_pages[i]->m_index = i;
  for (i                                = 0; i < page->getStyleCount(); i++)
    m_styles[page->getStyleId(i)].first = 0;
  page->m_palette                       = 0;
  delete page;
}

//-------------------------------------------------------------------

void TPalette::movePage(Page *page, int dstPageIndex) {
  assert(page);
  assert(page->m_palette == this);
  dstPageIndex = tcrop(dstPageIndex, 0, getPageCount() - 1);
  if (dstPageIndex == page->getIndex()) return;
  m_pages.erase(m_pages.begin() + page->getIndex());
  m_pages.insert(m_pages.begin() + dstPageIndex, page);
  for (int i = 0; i < getPageCount(); i++) m_pages[i]->m_index = i;
  assert(page->getIndex() == dstPageIndex);
}

//-------------------------------------------------------------------

TPalette::Page *TPalette::getStylePage(int styleId) const {
  if (0 <= styleId && styleId < getStyleCount())
    return m_styles[styleId].first;
  else
    return 0;
}

//-------------------------------------------------------------------

int TPalette::getClosestStyle(const TPixel32 &color) const {
  struct locals {
    static inline int getDistance2(const TPixel32 &a, const TPixel32 &b) {
      return (a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) +
             (a.b - b.b) * (a.b - b.b) + (a.m - b.m) * (a.m - b.m);
    }
  };  // locals

  if (color == TPixel32::Transparent) return 0;
  int bestIndex    = -1;
  int bestDistance = 255 * 255 * 4 + 1;
  for (int i = 1; i < (int)m_styles.size(); i++) {
    // if(i==FirstUserStyle+2) continue;
    TSolidColorStyle *scs =
        dynamic_cast<TSolidColorStyle *>(m_styles[i].second.getPointer());
    if (scs) {
      int d = locals::getDistance2(scs->getMainColor(), color);
      if (d < bestDistance) {
        bestIndex    = i;
        bestDistance = d;
      }
    }
  }
  return bestIndex;
}

//-------------------------------------------------------------------

bool TPalette::getFxRects(const TRect &rect, TRect &rectIn, TRect &rectOut) {
  int i;
  bool ret = false;
  int borderIn, borderOut, fullBorderIn = 0, fullBorderOut = 0;

  for (i = 0; i < (int)m_styles.size(); i++)
    if (m_styles[i].second->isRasterStyle()) {
      m_styles[i].second->getRasterStyleFx()->getEnlargement(borderIn,
                                                             borderOut);
      fullBorderIn  = std::max(fullBorderIn, borderIn);
      fullBorderOut = std::max(fullBorderOut, borderOut);
      ret           = true;
    }

  rectIn  = rect.enlarge(fullBorderIn);
  rectOut = rect.enlarge(fullBorderOut);
  return ret;
}

//===================================================================
//
// I/O
//
//-------------------------------------------------------------------

namespace {

class StyleWriter final : public TOutputStreamInterface {
  TOStream &m_os;
  int m_index;

public:
  static TFilePath m_rootDir;
  StyleWriter(TOStream &os, int index) : m_os(os), m_index(index) {}
  static void setRootDir(const TFilePath &fp) { m_rootDir = fp; }

  TOutputStreamInterface &operator<<(double x) override {
    m_os << x;
    return *this;
  };
  TOutputStreamInterface &operator<<(int x) override {
    m_os << x;
    return *this;
  };
  TOutputStreamInterface &operator<<(std::string x) override {
    m_os << x;
    return *this;
  };
  TOutputStreamInterface &operator<<(UCHAR x) override {
    m_os << (int)x;
    return *this;
  };
  TOutputStreamInterface &operator<<(USHORT x) override {
    m_os << (int)x;
    return *this;
  };
  TOutputStreamInterface &operator<<(const TPixel32 &x) override {
    m_os << x;
    return *this;
  };
  TOutputStreamInterface &operator<<(const TRaster32P &ras) override {
    assert(m_rootDir != TFilePath());

    std::string name = "texture_" + std::to_string(m_index);
    m_os << name;
    TFilePath filename = ((m_rootDir + "textures") + name).withType("bmp");
    if (!TFileStatus(m_rootDir + "textures").doesExist()) {
      try {
        TSystem::mkDir(m_rootDir + "textures");
      } catch (...) {
      }
    }

    TImageWriter::save(filename, ras);
    return *this;
  };
};

//-------------------------------------------------------------------

class StyleReader final : public TInputStreamInterface {
  TIStream &m_is;           //!< Wrapped input stream.
  VersionNumber m_version;  //!< Palette version number (overrides m_is's one).

public:
  static TFilePath m_rootDir;

public:
  StyleReader(TIStream &is, const VersionNumber &version)
      : m_is(is), m_version(version) {}

  static void setRootDir(const TFilePath &fp) { m_rootDir = fp; }

  TInputStreamInterface &operator>>(double &x) override {
    m_is >> x;
    return *this;
  }

  TInputStreamInterface &operator>>(int &x) override {
    m_is >> x;
    return *this;
  }

  TInputStreamInterface &operator>>(std::string &x) override {
    m_is >> x;
    return *this;
  }

  TInputStreamInterface &operator>>(UCHAR &x) override {
    int v;
    m_is >> v;
    x = v;
    return *this;
  }

  TInputStreamInterface &operator>>(USHORT &x) override {
    int v;
    m_is >> v;
    x = v;
    return *this;
  }

  TInputStreamInterface &operator>>(TRaster32P &x) override {
    assert(m_rootDir != TFilePath());
    std::string name;
    m_is >> name;
    TFilePath filename = ((m_rootDir + "textures") + name).withType("bmp");
    TRasterP ras;
    if (TImageReader::load(filename, ras)) {
      x = ras;
    }
    return *this;
  }

  TInputStreamInterface &operator>>(TPixel32 &x) override {
    m_is >> x;
    return *this;
  }

  /*!
\details  Explicitly ovverrides the stream's version, returning m_version.
        This is necessary since palettes have their \a own version number,
        which is \a not the TIStream's file one.
*/
  VersionNumber versionNumber() const override {
    return m_version;
  }  //!< Returns the palette's version number.
};

TFilePath StyleWriter::m_rootDir = TFilePath();
TFilePath StyleReader::m_rootDir = TFilePath();

}  // namespace

//===================================================================

void TPalette::setRootDir(const TFilePath &fp) {
  StyleWriter::setRootDir(fp);
  StyleReader::setRootDir(fp);
}

//-------------------------------------------------------------------

void TPalette::saveData(TOStream &os) {
  os.child("version") << 71 << 0;  // Inserting the version tag at this level.
  // This is necessary to support the tpl format
  if (m_refImgPath !=
      TFilePath())  // since it performs *untagged* stream output
    os.child("refImgPath")
        << m_refImgPath;  // (the palette is streamed directly).

  os.openChild("styles");
  {
    for (int i = 0; i < getStyleCount(); ++i) {
      TColorStyleP style = m_styles[i].second;
      if (style->getPickedPosition().pos == TPoint())
        os.openChild("style");
      else {
        std::map<std::string, std::string> attr;
        attr["pickedpos"] = pointToString(style->getPickedPosition());
        os.openChild("style", attr);
      }
      {
        StyleWriter w(os, i);
        style->save(w);
      }
      os.closeChild();
    }
  }
  os.closeChild();

  os.openChild("stylepages");
  {
    for (int i = 0; i < getPageCount(); ++i) {
      Page *page = getPage(i);
      os.openChild("page");
      {
        os.child("name") << page->getName();

        os.openChild("indices");
        {
          int m = page->getStyleCount();

          for (int j = 0; j < m; ++j) os << page->getStyleId(j);
        }
        os.closeChild();
      }
      os.closeChild();
    }
  }
  os.closeChild();

  if (isAnimated()) {
    os.openChild("animation");
    {
      StyleAnimationTable::iterator sat, saEnd = m_styleAnimationTable.end();
      for (sat = m_styleAnimationTable.begin(); sat != saEnd; ++sat) {
        int styleId               = sat->first;
        StyleAnimation &animation = sat->second;

        std::map<std::string, std::string> attributes;
        attributes["id"] = std::to_string(styleId);

        os.openChild("style", attributes);
        {
          StyleAnimation::iterator kt, kEnd = animation.end();
          for (kt = animation.begin(); kt != kEnd; ++kt) {
            int frame = kt->first;

            TColorStyle *cs = kt->second.getPointer();
            assert(cs);

            attributes.clear();
            attributes["frame"] = std::to_string(frame);

            /*os.openChild("keycolor", attributes);                       // Up to Toonz 7.0, animations saved
              os << cs->getMainColor();                                   // the main color only
            os.closeChild();*/  //

            os.openChild("keyframe", attributes);
            {
              StyleWriter w(os, sat->first);
              kt->second->save(w);
            }
            os.closeChild();
          }
        }
        os.closeChild();
      }
    }
    os.closeChild();
  }

  // salvo gli shortcuts solo se sono non standard
  int i;
  for (i = 0; i < 10; ++i)
    if (getShortcutValue('0' + i) != i) break;

  if (i < 10) {
    os.openChild("shortcuts");
    {
      for (i = 0; i < 10; i++) os << getShortcutValue('0' + i);
    }
    os.closeChild();
  }
  if (isLocked()) {
    os.openChild("lock");
    os << 1;
    os.closeChild();
  }
}

//-------------------------------------------------------------------

void TPalette::loadData(TIStream &is) {
  m_styles.clear();
  clearPointerContainer(m_pages);

  VersionNumber version = is.getVersion();

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "version") {
      is >> version.first >> version.second;
      if (version > VersionNumber(71, 0))
        throw TException("palette, unsupported version number");
    } else if (tagName == "styles") {
      while (!is.eos())  // I think while(is.openChild(tagName))
      {                  // would be better. However, I don't trust
        if (!is.openChild(tagName) ||
            tagName !=
                "style")  // TIStream's implementation very much. Keeping it
          throw TException(
              "palette, expected tag <style>");  // like this for now.
        {
          StyleReader r(is, version);
          TColorStyle *cs = TColorStyle::load(r);

          std::string pickedPosStr;
          if (is.getTagParam("pickedpos", pickedPosStr))
            cs->setPickedPosition(stringToPoint(pickedPosStr));

          addStyle(cs);
        }

        is.closeChild();
      }
    } else if (tagName == "stylepages") {
      while (!is.eos()) {
        if (!is.openChild(tagName) || tagName != "page")
          throw TException("palette, expected tag <stylepage>");
        {
          std::wstring pageName;

          if (!is.openChild(tagName) || tagName != "name")
            throw TException("palette, expected tag <name>");
          { is >> pageName; }
          is.closeChild();

          Page *page = addPage(pageName);

          if (!is.openChild(tagName) || tagName != "indices")
            throw TException("palette, expected tag <indices>");
          {
            while (!is.eos()) {
              int index;
              is >> index;
              page->addStyle(index);
            }
          }
          is.closeChild();
        }
        is.closeChild();
      }
    } else if (tagName == "refImgPath")
      is >> m_refImgPath;
    else if (tagName == "animation") {
      while (!is.eos()) {
        if (!is.openChild(tagName) || tagName != "style")
          throw TException("palette, expected tag <style>");
        {
          int styleId = 0;
          if (!is.getTagParam("id", styleId))
            throw TException("palette, missing id attribute in tag <style>");

          StyleAnimation animation;

          TColorStyle *style = getStyle(styleId);
          assert(style);

          while (is.matchTag(tagName)) {
            TColorStyle *cs = 0;
            int frame       = 0;

            if (tagName == "keycolor") {
              if (!is.getTagParam("frame", frame))
                throw TException(
                    "palette, missing frame attribute in tag <keycolor>");

              TPixel32 color;
              is >> color;

              cs = style->clone();
              cs->setMainColor(color);
            } else if (tagName == "keyframe") {
              if (!is.getTagParam("frame", frame))
                throw TException(
                    "palette, missing frame attribute in tag <keyframe>");

              StyleReader r(is, version);
              cs = TColorStyle::load(r);
            } else
              throw TException("palette, expected <keyframe> tag");

            animation[frame] = cs;
            is.closeChild();
          }

          m_styleAnimationTable[styleId] = animation;
        }
        is.closeChild();
      }
    } else if (tagName == "stylepages") {
      int key = '0';
      while (!is.eos()) {
        int styleId = 0;
        is >> styleId;

        if (key <= '9') setShortcutValue(key, styleId);
      }
    } else if (tagName == "shortcuts") {
      for (int i = 0; i < 10; ++i) {
        int v;
        is >> v;

        setShortcutValue('0' + i, v);
      }
    } else if (tagName == "lock") {
      int lockValue;
      is >> lockValue;
      m_isLocked = (bool)lockValue;
    } else
      throw TException("palette, unknown tag: " + tagName);

    is.closeChild();
  }
}

//===================================================================

/*! if the palette is copied from studio palette, this function will modify the
 * original names.
*/
void TPalette::assign(const TPalette *src, bool isFromStudioPalette) {
  if (src == this) return;
  int i;
  m_isCleanupPalette = src->isCleanupPalette();
  // for(i=0;i<getStyleCount();i++) delete getStyle(i);
  m_styles.clear();
  clearPointerContainer(m_pages);

  for (i = 0; i < src->getStyleCount(); i++) {
    TColorStyle *srcStyle = src->getStyle(i);
    TColorStyle *dstStyle = srcStyle->clone();
    dstStyle->setName(
        srcStyle->getName());  // per un baco del TColorStyle::clone()
    dstStyle->setGlobalName(
        srcStyle->getGlobalName());  // per un baco del TColorStyle::clone()

    // if the style is copied from studio palette, put its name to the original
    // name.
    // check if the style has the global name (i.e. it comes from studio
    // palette)
    if (isFromStudioPalette && srcStyle->getGlobalName() != L"") {
      // If the original style has no original name (i.e. if the style is copied
      // from the studio palette)
      if (srcStyle->getOriginalName() == L"") {
        // put the original style name to the "original name" of the pasted
        // style.
        dstStyle->setOriginalName(srcStyle->getName());
      }
    }

    int j = addStyle(dstStyle);
    assert(i == j);
  }

  for (i = 0; i < src->getPageCount(); i++) {
    const Page *srcPage = src->getPage(i);
    Page *dstPage       = addPage(srcPage->getName());
    for (int j = 0; j < srcPage->getStyleCount(); j++)
      dstPage->addStyle(srcPage->getStyleId(j));
  }
  m_refImg     = !!src->m_refImg ? src->m_refImg->cloneImage() : TImageP();
  m_refImgPath = src->m_refImgPath;

  StyleAnimationTable::iterator it;
  StyleAnimation::iterator j;
  for (it = m_styleAnimationTable.begin(); it != m_styleAnimationTable.end();
       ++it) {
    // for(j = it->second.begin(); j != it->second.end(); ++j)
    //   delete j->second;
    it->second.clear();
  }
  m_styleAnimationTable.clear();
  StyleAnimationTable::const_iterator cit;
  for (cit = src->m_styleAnimationTable.begin();
       cit != src->m_styleAnimationTable.end(); ++cit) {
    StyleAnimation animation = cit->second;
    for (j = animation.begin(); j != animation.end(); j++)
      j->second                       = j->second->clone();
    m_styleAnimationTable[cit->first] = cit->second;
  }
  m_globalName         = src->getGlobalName();
  m_shortcuts          = src->m_shortcuts;
  m_currentFrame       = src->m_currentFrame;
  m_shortcutScopeIndex = src->m_shortcutScopeIndex;
  // setDirtyFlag(true);
}

//-------------------------------------------------------------------
/*!if the palette is merged from studio palette, this function will modify the
 * original names.
*/
void TPalette::merge(const TPalette *src, bool isFromStudioPalette) {
  std::map<int, int> table;
  int i;
  for (i = 1; i < src->getStyleCount(); i++) {
    TColorStyle *srcStyle = src->getStyle(i);
    TColorStyle *dstStyle = srcStyle->clone();
    dstStyle->setName(srcStyle->getName());
    dstStyle->setGlobalName(srcStyle->getGlobalName());

    // if the style is copied from studio palette, put its name to the original
    // name.
    // check if the style has the global name (i.e. it comes from studio
    // palette)
    if (isFromStudioPalette && srcStyle->getGlobalName() != L"") {
      // If the original style has no original name (i.e. if the style is copied
      // from the studio palette)
      if (srcStyle->getOriginalName() == L"") {
        // put the original style name to the "original name" of the pasted
        // style.
        dstStyle->setOriginalName(srcStyle->getName());
      }
    }

    int j    = addStyle(dstStyle);
    table[i] = j;
  }

  int pageCount = src->getPageCount();
  for (i = 0; i < pageCount; i++) {
    const Page *srcPage   = src->getPage(i);
    std::wstring pageName = srcPage->getName();
    if (pageName == L"colors" && src->getPaletteName() != L"")
      pageName    = src->getPaletteName();
    Page *dstPage = addPage(pageName);  //;
    for (int j = 0; j < srcPage->getStyleCount(); j++) {
      int styleId = srcPage->getStyleId(j);
      if (styleId == 0) continue;
      assert(table.find(styleId) != table.end());
      dstPage->addStyle(table[styleId]);
    }
    assert(dstPage->m_palette == this);
  }
}

//-------------------------------------------------------------------

void TPalette::setIsCleanupPalette(bool on) { m_isCleanupPalette = on; }

//-------------------------------------------------------------------

void TPalette::setRefImg(const TImageP &img) {
  m_refImg = img;
  if (img) {
    assert(img->getPalette() == 0);
  }
}

//-------------------------------------------------------------------

void TPalette::setRefLevelFids(const std::vector<TFrameId> fids) {
  m_refLevelFids = fids;
}

//-------------------------------------------------------------------

std::vector<TFrameId> TPalette::getRefLevelFids() { return m_refLevelFids; }

//-------------------------------------------------------------------

void TPalette::setRefImgPath(const TFilePath &refImgPath) {
  m_refImgPath = refImgPath;
}

//===================================================================

bool TPalette::isAnimated() const { return !m_styleAnimationTable.empty(); }

//-------------------------------------------------------------------

int TPalette::getFrame() const { return m_currentFrame; }

//-------------------------------------------------------------------

void TPalette::setFrame(int frame) {
  QMutexLocker muLock(&m_mutex);

  if (m_currentFrame == frame) return;

  m_currentFrame = frame;

  StyleAnimationTable::iterator sat, saEnd = m_styleAnimationTable.end();
  for (sat = m_styleAnimationTable.begin(); sat != saEnd; ++sat) {
    StyleAnimation &animation = sat->second;
    assert(!animation.empty());

    // Retrieve the associated style to interpolate
    int styleId = sat->first;
    assert(0 <= styleId && styleId < getStyleCount());

    TColorStyle *cs = getStyle(styleId);
    assert(cs);

    // Buid the keyframes interval containing frame
    StyleAnimation::iterator j0, j1;

    j1 = animation.lower_bound(
        frame);  // j1 is the first element:  j1->first >= frame
    if (j1 == animation.begin())
      cs->copy(*j1->second);
    else {
      j0 = j1, --j0;
      assert(j0->first <= frame);

      if (j1 == animation.end())
        cs->copy(*j0->second);
      else {
        assert(frame <= j1->first);

        cs->assignBlend(*j0->second, *j1->second,
                        (frame - j0->first) / double(j1->first - j0->first));
      }
    }
  }
}

//-------------------------------------------------------------------

bool TPalette::isKeyframe(int styleId, int frame) const {
  StyleAnimationTable::const_iterator it = m_styleAnimationTable.find(styleId);
  if (it == m_styleAnimationTable.end()) return false;
  return it->second.count(frame) > 0;
}

//-------------------------------------------------------------------

int TPalette::getKeyframeCount(int styleId) const {
  StyleAnimationTable::const_iterator it = m_styleAnimationTable.find(styleId);
  if (it == m_styleAnimationTable.end()) return 0;
  return int(it->second.size());
}

//-------------------------------------------------------------------

int TPalette::getKeyframe(int styleId, int index) const {
  StyleAnimationTable::const_iterator it = m_styleAnimationTable.find(styleId);
  if (it == m_styleAnimationTable.end()) return -1;
  const StyleAnimation &animation = it->second;
  if (index < 0 || index >= (int)animation.size()) return -1;
  StyleAnimation::const_iterator j = animation.begin();
  std::advance(j, index);
  return j->first;
}

//-------------------------------------------------------------------

void TPalette::setKeyframe(int styleId, int frame) {
  assert(styleId >= 0 && styleId < getStyleCount());
  assert(frame >= 0);

  StyleAnimationTable::iterator sat = m_styleAnimationTable.find(styleId);

  if (sat == m_styleAnimationTable.end())
    sat =
        m_styleAnimationTable.insert(std::make_pair(styleId, StyleAnimation()))
            .first;

  assert(sat != m_styleAnimationTable.end());

  StyleAnimation &animation = sat->second;
  animation[frame]          = getStyle(styleId)->clone();
}

//-------------------------------------------------------------------

void TPalette::clearKeyframe(int styleId, int frame) {
  assert(0 <= styleId && styleId < getStyleCount());
  assert(0 <= frame);
  StyleAnimationTable::iterator it = m_styleAnimationTable.find(styleId);
  if (it == m_styleAnimationTable.end()) return;
  StyleAnimation &animation  = it->second;
  StyleAnimation::iterator j = animation.find(frame);
  if (j == animation.end()) return;
  // j->second->release();
  animation.erase(j);
  if (animation.empty()) {
    m_styleAnimationTable.erase(styleId);
  }
}

//-------------------------------------------------------------------

int TPalette::getShortcutValue(int key) const {
  assert(Qt::Key_0 <= key && key <= Qt::Key_9);

  int shortcutIndex = (key == Qt::Key_0) ? 9 : key - Qt::Key_1;
  int indexInPage   = m_shortcutScopeIndex * 10 + shortcutIndex;
  // shortcut is available only in the first page
  return getPage(0)->getStyleId(indexInPage);
}

//-------------------------------------------------------------------

int TPalette::getStyleShortcut(int styleId) const {
  assert(0 <= styleId && styleId < getStyleCount());

  Page *page = getStylePage(styleId);
  // shortcut is available only in the first page
  if (!page || page->getIndex() != 0) return -1;
  int indexInPage   = page->search(styleId);
  int shortcutIndex = indexInPage - m_shortcutScopeIndex * 10;
  if (shortcutIndex < 0 || shortcutIndex > 9) return -1;
  return (shortcutIndex == 9) ? Qt::Key_0 : Qt::Key_1 + shortcutIndex;
}

//-------------------------------------------------------------------

void TPalette::setShortcutValue(int key, int styleId) {
  assert('0' <= key && key <= '9');
  assert(styleId == -1 || 0 <= styleId && styleId < getStyleCount());
  if (styleId == -1)
    m_shortcuts.erase(key);
  else {
    std::map<int, int>::iterator it;
    for (it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it)
      if (it->second == styleId) {
        m_shortcuts.erase(it);
        break;
      }
    m_shortcuts[key] = styleId;
  }
}

//-------------------------------------------------------------------
// Returns true if there is at least one style with picked pos value
//-------------------------------------------------------------------

bool TPalette::hasPickedPosStyle() {
  for (int i = 0; i < getStyleCount(); ++i) {
    TColorStyleP style = m_styles[i].second;
    if (style->getPickedPosition().pos != TPoint()) return true;
  }
  return false;
}

//-------------------------------------------------------------------

void TPalette::nextShortcutScope(bool invert) {
  if (invert) {
    if (m_shortcutScopeIndex > 0)
      m_shortcutScopeIndex -= 1;
    else
      m_shortcutScopeIndex = getPage(0)->getStyleCount() / 10;
  } else {
    if ((m_shortcutScopeIndex + 1) * 10 < getPage(0)->getStyleCount())
      m_shortcutScopeIndex += 1;
    else
      m_shortcutScopeIndex = 0;
  }
}
