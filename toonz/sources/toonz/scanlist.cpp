

#include "scanlist.h"
#include "tapp.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/fullcolorpalette.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "cellselection.h"
#include "toonz/observer.h"
#include "trasterimage.h"
#include "tsystem.h"
#include "tlevel_io.h"
#include "tiio.h"
#include "tconvert.h"
#include "timagecache.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/icongenerator.h"
//---------------------------------------------------------

ScanListFrame::ScanListFrame() : m_xl(0) {}

//---------------------------------------------------------

ScanListFrame::ScanListFrame(TXshSimpleLevel *xl, const TFrameId &fid)
    : m_xl(xl), m_fid(fid) {
  if (m_xl) m_xl->addRef();
}

//---------------------------------------------------------

ScanListFrame::~ScanListFrame() {
  if (m_xl) m_xl->release();
}

//---------------------------------------------------------

ScanListFrame::ScanListFrame(const ScanListFrame &src)
    : m_xl(src.m_xl), m_fid(src.m_fid) {
  if (m_xl) m_xl->addRef();
}

//---------------------------------------------------------

ScanListFrame &ScanListFrame::operator=(const ScanListFrame &src) {
  ScanListFrame tmp(*this);
  std::swap(m_xl, tmp.m_xl);
  std::swap(m_fid, tmp.m_fid);
  return *this;
}

//---------------------------------------------------------

std::wstring ScanListFrame::getName() const {
  if (!m_xl) return L"";
  TFilePath fp = TFilePath(getLevelName()).withFrame(m_fid);
  return fp.getWideString();
}

//---------------------------------------------------------

std::wstring ScanListFrame::getLevelName() const {
  return m_xl ? m_xl->getName() : L"";
}

//---------------------------------------------------------

TFrameId ScanListFrame::getFrameId() const { return m_fid; }

//---------------------------------------------------------

void ScanListFrame::setRasterImage(const TRasterImageP &ras, bool isBW) const {
  if (!m_xl) return;
  LevelProperties *lprop = m_xl->getProperties();
  if (lprop->getImageDpi() == TPointD()) {
    double dpix = 0, dpiy = 0;
    ras->getDpi(dpix, dpiy);
    lprop->setImageDpi(TPointD(dpix, dpiy));
    lprop->setImageRes(ras->getRaster()->getSize());
    lprop->setDpiPolicy(LevelProperties::DP_ImageDpi);

    lprop->setBpp(isBW ? 1 : 8 * ras->getRaster()->getPixelSize());
  }

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath path    = m_xl->getPath();
  if (m_xl->getScannedPath() != TFilePath()) path = m_xl->getScannedPath();
  path                                            = scene->decodeFilePath(path);
  TSystem::touchParentDir(path);

  TLevelWriterP lw(path);
  TImageWriterP iw    = lw->getFrameWriter(m_fid);
  TPropertyGroup *pg  = Tiio::makeWriterProperties(path.getType());
  TEnumProperty *prop = (TEnumProperty *)(pg->getProperty("Bits Per Pixel"));
  if (prop) {
    std::wstring pixelSizeW                = prop->getValue().substr(0, 2);
    int ps                                 = ras->getRaster()->getPixelSize();
    const std::vector<std::wstring> &range = prop->getRange();
    if (isBW)
      prop->setValue(range[2]);
    else {
      for (int i = 0; i < (int)range.size(); i++) {
        int bpp = std::stoi(::to_string(range[i]).substr(0, 2));

#ifdef LINETEST
        if (bpp == ps * 8)
#else
        if ((bpp == 24 && ps == 4) || (bpp == ps * 8))
#endif
        {
          prop->setValue(range[i]);
          break;
        }
      }
    }
    iw->setProperties(pg);
  }

  iw->save(ras.getPointer());

  if (m_xl->getScannedPath() != TFilePath())
    m_xl->setFrameStatus(m_fid, TXshSimpleLevel::Scanned);
  std::string imageId = m_xl->getImageId(m_fid);
  TImageCache::instance()->remove(imageId);
  m_xl->setFrame(m_fid, ras.getPointer());

  if (!m_xl->getPalette()) {
    TPalette *p = FullColorPalette::instance()->getPalette(scene);
    m_xl->setPalette(p);
    TApp::instance()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->setPalette(p);
  }

  TXshSimpleLevel *sl = m_xl->getSimpleLevel();
  if (sl) {
    IconGenerator::instance()->invalidate(sl, m_fid);
    IconGenerator::instance()->invalidateSceneIcon();
  }

  TNotifier::instance()->notify(TXsheetChange());

  // TDrawingView::notify(level->frame(fid));
  TNotifier::instance()->notify(TDrawingChange(m_xl, m_fid));
  TNotifier::instance()->notify(TColumnHeadChange());
}

//=========================================================

ScanList::ScanList() {}

//---------------------------------------------------------

void ScanList::addFrame(const ScanListFrame &frame) {
  m_frames.push_back(frame);
}

//---------------------------------------------------------

int ScanList::getFrameCount() const { return m_frames.size(); }

//---------------------------------------------------------

const ScanListFrame &ScanList::getFrame(int index) const {
  assert(0 <= index && index < getFrameCount());
  return m_frames[index];
}

//=========================================================

namespace {

//---------------------------------------------------------

typedef std::set<std::pair<TXshSimpleLevel *, TFrameId>> Cells;
typedef std::vector<TXshSimpleLevel *> Levels;
//---------------------------------------------------------

void getCells(Cells &cells, Levels &levels, bool includeScannedFrames) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();
  TCellSelection *selection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  if (!selection) return;
  int r0, c0, r1, c1;
  selection->getSelectedCells(r0, c0, r1, c1);
  TRect rect = TRect(c0, r0, c1, r1);  //   selRect = selection->getRect();

  std::set<TXshSimpleLevel *> visitedlevel;
  for (int c = rect.x0; c <= rect.x1; c++)
    for (int r = rect.y0; r <= rect.y1; r++) {
      TXshCell cell       = xsh->getCell(r, c);
      TXshSimpleLevel *xl = cell.getSimpleLevel();
      if (xl) {
        if (xl->getType() != TZI_XSHLEVEL && xl->getType() != OVL_XSHLEVEL &&
            xl->getType() != TZP_XSHLEVEL)
          continue;
        if (!visitedlevel.count(xl)) {
          visitedlevel.insert(xl);
          levels.push_back(xl);
        }
        TFrameId fid = cell.m_frameId;
        if (includeScannedFrames || !xl->getFrame(fid, false)) {
          cells.insert(std::make_pair(xl, fid));
        }
      }
    }
}

//---------------------------------------------------------

}  // namespace

//---------------------------------------------------------

bool ScanList::areScannedFramesSelected() {
  Cells cells;
  Levels levels;
  getCells(cells, levels, true);
  Cells::iterator it;
  for (it = cells.begin(); it != cells.end(); ++it) {
    assert(it->first);
    if (it->first->getFrame(it->second, false)) return true;
  }
  return false;
}

//---------------------------------------------------------

void ScanList::update(bool includeScannedFrames) {
  m_frames.clear();
  Cells cells;
  Levels levels;
  getCells(cells, levels, includeScannedFrames);
  if (cells.empty()) return;
  Levels::iterator lit;
  for (lit = levels.begin(); lit != levels.end(); ++lit) {
    Cells::iterator it;
    for (it = cells.begin(); it != cells.end(); ++it)
      if (*lit == it->first)
        m_frames.push_back(ScanListFrame(it->first, it->second));
  }
}

//---------------------------------------------------------
