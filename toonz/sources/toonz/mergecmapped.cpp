#include "tapp.h"
#include "tpalette.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshcell.h"
//#include "tw/action.h"
#include "tropcm.h"
#include "ttoonzimage.h"
#include "matchline.h"
#include "toonz/scenefx.h"
#include "toonz/dpiscale.h"
#include "toonz/txsheethandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"
#include "toonzqt/icongenerator.h"
#include <map>
#include <QRadioButton>
#include <QPushButton>
#include <QApplication>
#include "tundo.h"
#include "tools/toolutils.h"
#include "timagecache.h"
#include "tcolorstyles.h"
#include "toonz/levelproperties.h"
#include "toonz/childstack.h"
#include "toonz/toonzimageutils.h"
#include "tpaletteutil.h"

#include <algorithm>

using namespace DVGui;

namespace {

class MergeCmappedPair {
public:
  const TXshCell *m_cell;
  TAffine m_imgAff;
  const TXshCell *m_mcell;
  TAffine m_matchAff;

  MergeCmappedPair(const TXshCell &cell, const TAffine &imgAff,
                   const TXshCell &mcell, const TAffine &matchAff)
      : m_cell(&cell)
      , m_imgAff(imgAff)
      , m_mcell(&mcell)
      , m_matchAff(matchAff){};
};

void mergeCmapped(const std::vector<MergeCmappedPair> &matchingLevels) {
  if (matchingLevels.empty()) return;

  TPalette *palette = matchingLevels[0].m_cell->getImage(false)->getPalette();
  TPalette *matchPalette =
      matchingLevels[0].m_mcell->getImage(false)->getPalette();

  TPalette::Page *page;

  // upInkId -> downInkId
  std::map<int, int> usedColors;

  int i = 0;
  for (i = 0; i < (int)matchingLevels.size(); i++) {
    TToonzImageP img = (TToonzImageP)matchingLevels[i].m_cell->getImage(true);
    TToonzImageP match =
        (TToonzImageP)matchingLevels[i].m_mcell->getImage(false);
    if (!img || !match)
      throw TRopException("Can merge only cmapped raster images!");

    std::set<int> usedStyles;
    ToonzImageUtils::getUsedStyles(usedStyles, match);
    std::map<int, int> indexTable;
    mergePalette(palette, indexTable, matchPalette, usedStyles);
    ToonzImageUtils::scrambleStyles(match, indexTable);
    match->setPalette(palette);
    matchPalette = palette;

    // img->lock();
    TRasterCM32P ras      = img->getRaster();    // img->getCMapped(false);
    TRasterCM32P matchRas = match->getRaster();  // match->getCMapped(true);
    if (!ras || !matchRas)
      throw TRopException("Can merge only cmapped images!");

    TAffine aff =
        matchingLevels[i].m_imgAff.inv() * matchingLevels[i].m_matchAff;
    int mlx = matchRas->getLx();
    int mly = matchRas->getLy();
    int rlx = ras->getLx();
    int rly = ras->getLy();

    TRectD in = convert(matchRas->getBounds()) - matchRas->getCenterD();

    TRectD out = aff * in;

    TPoint offs((rlx - mlx) / 2 + tround(out.getP00().x - in.getP00().x),
                (rly - mly) / 2 + tround(out.getP00().y - in.getP00().y));

    int lxout = tround(out.getLx()) + 1 + ((offs.x < 0) ? offs.x : 0);
    int lyout = tround(out.getLy()) + 1 + ((offs.y < 0) ? offs.y : 0);

    if (lxout <= 0 || lyout <= 0 || offs.x >= rlx || offs.y >= rly) {
      // tmsg_error("no intersections between matchline and level");
      continue;
    }

    aff = aff.place((double)(in.getLx() / 2.0), (double)(in.getLy() / 2.0),
                    (out.getLx()) / 2.0 + ((offs.x < 0) ? offs.x : 0),
                    (out.getLy()) / 2.0 + ((offs.y < 0) ? offs.y : 0));

    if (offs.x < 0) offs.x = 0;
    if (offs.y < 0) offs.y = 0;

    if (lxout + offs.x > rlx || lyout + offs.y > rly) {
      // PRINTF("TAGLIO L'IMMAGINE\n");
      lxout = (lxout + offs.x > rlx) ? (rlx - offs.x) : lxout;
      lyout = (lyout + offs.y > rly) ? (rly - offs.y) : lyout;
    }

    if (!aff.isIdentity(1e-4)) {
      TRasterCM32P aux(lxout, lyout);
      TRop::resample(aux, matchRas, aff);
      matchRas = aux;
    }
    ras->lock();
    matchRas->lock();
    TRect raux = matchRas->getBounds() + offs;
    TRasterP r = ras->extract(raux);
    TRop::overlayCmapped(r, matchRas, palette, matchPalette, usedColors);
    ras->unlock();
    matchRas->unlock();

    img->setSavebox(img->getSavebox() + (matchRas->getBounds() + offs));
  }
  /*
    std::map<int, int>::iterator it = usedColors.begin();
    for (; it != usedColors.end(); ++it)
      if (it->first != it->second) break;

    if (it == usedColors.end())  // this means that the merged palette does not
                                 // differ from source palette.(all usedColors
                                 // are not new color )
      return;

    std::wstring pageName = L"merged palettes";

    for (i = 0; i < palette->getPageCount(); i++)
      if (palette->getPage(i)->getName() == pageName) {
        page = palette->getPage(i);
        break;
      }
    if (i == palette->getPageCount()) page = palette->addPage(pageName);

    it        = usedColors.begin();
    int count = 0;
    for (; it != usedColors.end(); ++it) {
      if (it->first == it->second) continue;
      while (palette->getStyleCount() <= it->second)
        palette->addStyle(TPixel32::Red);
      palette->setStyle(it->second, matchPalette->getStyle(it->first)->clone());
      page->addStyle(it->second);
    }
    if (usedColors.size() > 0) palette->setDirtyFlag(true);
  */
}

/*------------------------------------------------------------------------*/

void applyDeleteMatchline(TXshSimpleLevel *sl,
                          const std::vector<TFrameId> &fids,
                          const std::vector<int> &_inkIndexes) {
  TPalette::Page *page = 0;
  int i, j, pageIndex = 0;
  std::vector<int> inkIndexes = _inkIndexes;

  if (fids.empty()) return;

  TPalette *palette = 0;

  if (inkIndexes.empty()) {
    palette = sl->getFrame(fids[0], true)->getPalette();

    for (i = 0; i < palette->getPageCount(); i++)
      if (palette->getPage(i)->getName() == L"match lines") {
        page = palette->getPage(i);
        break;
      }

    if (!page) return;
    pageIndex = i;
  }

  for (i = 0; i < (int)fids.size(); i++) {
    // level[i]->lock();
    TToonzImageP image = sl->getFrame(fids[i], true);
    assert(image);
    TRasterCM32P ras = image->getRaster();  // level[i]->getCMapped(false);
    ras->lock();
    if (inkIndexes.empty())
      for (j = 0; j < page->getStyleCount(); j++)
        inkIndexes.push_back(page->getStyleId(j));

    TRop::eraseColors(ras, &inkIndexes, true);

    ras->unlock();
    TRect savebox;
    TRop::computeBBox(ras, savebox);
    image->setSavebox(savebox);
    // level[i]->unlock();
  }

  // if (page)
  //  {
  //  while (page->getStyleCount())
  //   page->removeStyle(0);
  // palette->erasePage(pageIndex);
  // }
}

/*------------------------------------------------------------------------*/
}  // namespace
//-----------------------------------------------------------------------------

class DeleteMatchlineUndo final : public TUndo {
public:
  TXshLevel *m_xl;
  TXshSimpleLevel *m_sl;
  std::vector<TFrameId> m_fids;
  std::vector<int> m_indexes;
  TPaletteP m_matchlinePalette;

  DeleteMatchlineUndo(
      TXshLevel *xl, TXshSimpleLevel *sl, const std::vector<TFrameId> &fids,
      const std::vector<int> &indexes)  //, TPalette*matchPalette)
      : TUndo(),
        m_xl(xl),
        m_sl(sl),
        m_fids(fids),
        m_indexes(indexes)
  //, m_matchlinePalette(matchPalette->clone())
  {
    // assert(matchPalette);
    int i;
    for (i = 0; i < fids.size(); i++) {
      QString id = "DeleteMatchlineUndo" + QString::number((uintptr_t) this) +
                   "-" + QString::number(i);
      TToonzImageP image = sl->getFrame(fids[i], false);
      assert(image);
      TImageCache::instance()->add(id, image->clone());
    }
  }

  void undo() const override {
    int i;
    // TPalette *palette = m_matchlinePalette->clone();
    // m_sl->setPalette(palette);
    for (i = 0; i < m_fids.size(); i++) {
      QString id = "DeleteMatchlineUndo" + QString::number((uintptr_t) this) +
                   "-" + QString::number(i);
      TImageP img = TImageCache::instance()->get(id, false)->cloneImage();

      m_sl->setFrame(m_fids[i], img);
      ToolUtils::updateSaveBox(m_sl, m_fids[i]);
    }
    // TApp::instance()->getPaletteController()->getCurrentLevelPalette()->setPalette(palette);

    if (m_xl) invalidateIcons(m_xl, m_fids);
    m_sl->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int i;

    // for (i=0; i<m_fids.size(); i++)
    //  images.push_back(m_sl->getFrame(m_fids[i], true));

    applyDeleteMatchline(m_sl, m_fids, m_indexes);
    for (i = 0; i < m_fids.size(); i++) {
      ToolUtils::updateSaveBox(m_sl, m_fids[i]);
    }
    if (m_xl) invalidateIcons(m_xl, m_fids);
    m_sl->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  ~DeleteMatchlineUndo() {
    int i;
    for (i = 0; i < m_fids.size(); i++)
      TImageCache::instance()->remove("DeleteMatchlineUndo" +
                                      QString::number((uintptr_t) this) + "-" +
                                      QString::number(i));
  }
};

//-----------------------------------------------------------------------------
/*
namespace {

class DeleteLevelUndo final : public TUndo {
  TXshLevelP m_xl;
public:
  DeleteLevelUndo(TXshLevel *xl) : m_xl(xl) {}

  void undo() const {
                ToonzScene *scene =
TApp::instance()->getCurrentScene()->getScene();
    scene->getLevelSet()->insertLevel(m_xl.getPointer());
                TApp::instance()->getCurrentScene()->notifyCastChange();
  }
  void redo() const {
                ToonzScene *scene =
TApp::instance()->getCurrentScene()->getScene();
    scene->getLevelSet()->removeLevel(m_xl.getPointer());
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }

  int getSize() const {
    return sizeof *this + 100;
  }
};

} //namespace
*/

static bool removeLevel(TXshLevel *level) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  // if(scene->getChildStack()->getTopXsheet()->isLevelUsed(level))
  //	DVGui::error(QObject::tr("It is not possible to delete the used level
  //%1.").arg(QString::fromStdWString(level->getName())));//"E_CantDeleteUsedLevel_%1"
  // else
  {
    // TUndoManager *um = TUndoManager::manager();
    // um->add(new DeleteLevelUndo(level));
    scene->getLevelSet()->removeLevel(level, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }
  return true;
}

class MergeCmappedUndo final : public TUndo {
  TXshLevel *m_xl;
  int m_mergeCmappedSessionId;
  std::map<TFrameId, QString> m_images;
  TXshSimpleLevel *m_level;
  TPalette *m_palette;
  int m_column, m_mColumn;
  std::wstring m_fullpath;

public:
  MergeCmappedUndo(TXshLevel *xl, int mergeCmappedSessionId, int column,
                   TXshSimpleLevel *level,
                   const std::map<TFrameId, QString> &images, int mColumn,
                   TPalette *palette)
      : TUndo()
      , m_xl(xl)
      , m_mergeCmappedSessionId(mergeCmappedSessionId)
      , m_palette(palette->clone())
      , m_level(level)
      , m_column(column)
      , m_mColumn(mColumn)
      , m_images(images)

  {
    m_fullpath = m_xl->getPath().getWideString();
  }

  void undo() const override {
    std::map<TFrameId, QString>::const_iterator it = m_images.begin();
    TPalette *palette = m_palette->clone();
    m_level->setPalette(palette);
    std::vector<TFrameId> fids;
    for (; it != m_images.end(); ++it)  //, ++mit)
    {
      QString id = "MergeCmappedUndo" +
                   QString::number(m_mergeCmappedSessionId) + "-" +
                   QString::number(it->first.getNumber());
      TImageP img = TImageCache::instance()->get(id, false)->cloneImage();
      img->setPalette(palette);
      m_level->setFrame(it->first, img);
      fids.push_back(it->first);
    }

    removeLevel(m_xl);

    TApp::instance()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->setPalette(palette);
    m_level->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    mergeCmapped(m_column, m_mColumn, QString::fromStdWString(m_fullpath),
                 true);
  }

  int getSize() const override { return sizeof(*this); }

  ~MergeCmappedUndo() {
    std::map<TFrameId, QString>::const_iterator it = m_images.begin();
    for (; it != m_images.end(); ++it)  //, ++mit)
    {
      QString id = "MergeCmappedUndo" + QString::number((uintptr_t) this) +
                   "-" + QString::number(it->first.getNumber());
      TImageCache::instance()->remove(id);
    }
    delete m_palette;
  }
};

//-----------------------------------------------------------------------------

static int LastMatchlineIndex = -1;

class MergedPair {
public:
  TFrameId m_f0, m_f1;
  TAffine m_aff;
  MergedPair(const TFrameId &f0, const TFrameId &f1, const TAffine &aff)
      : m_f0(f0), m_f1(f1), m_aff(aff) {}

  bool operator<(const MergedPair &p) const {
    if (m_f0 != p.m_f0) return m_f0 < p.m_f0;
    if (m_f1 != p.m_f1) return m_f1 < p.m_f1;
    if (m_aff.a11 != p.m_aff.a11) return m_aff.a11 < p.m_aff.a11;
    if (m_aff.a12 != p.m_aff.a12) return m_aff.a12 < p.m_aff.a12;
    if (m_aff.a13 != p.m_aff.a13) return m_aff.a13 < p.m_aff.a13;
    if (m_aff.a21 != p.m_aff.a21) return m_aff.a21 < p.m_aff.a21;
    if (m_aff.a22 != p.m_aff.a22) return m_aff.a22 < p.m_aff.a22;
    if (m_aff.a23 != p.m_aff.a23) return m_aff.a23 < p.m_aff.a23;
    return false;
  }
};

//--------------------------------------------------------------------

void mergeCmapped(int column, int mColumn, const QString &fullpath,
                  bool isRedo) {
  static int MergeCmappedSessionId = 0;
  MergeCmappedSessionId++;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int start, end;
  int mStart, mEnd;
  xsh->getCellRange(column, start, end);
  xsh->getCellRange(mColumn, mStart, mEnd);

  if (start > end) return;
  std::vector<TXshCell> cell(std::max(end, mEnd) - std::min(start, mStart) + 1);
  std::vector<TXshCell> mCell(cell.size());

  xsh->getCells(std::min(start, mStart), column, cell.size(), &(cell[0]));

  if (mColumn != -1)
    xsh->getCells(std::min(start, mStart), mColumn, cell.size(), &(mCell[0]));

  TXshColumn *col  = xsh->getColumn(column);
  TXshColumn *mcol = xsh->getColumn(mColumn);

  std::vector<MergeCmappedPair> matchingLevels;

  std::map<MergedPair, TFrameId> computedMergedMap;

  TXshSimpleLevel *level = 0, *mLevel = 0;
  TXshLevel *xl;

  std::map<TFrameId, QString> images;
  double dpix = 0, dpiy = 0;
  for (int i = 0; i < (int)cell.size(); i++) {
    if (!cell[i].isEmpty() && dpix == 0)
      ((TToonzImageP)(cell[i].getImage(false)))->getDpi(dpix, dpiy);

    if (!level) {
      level = cell[i].getSimpleLevel();
      xl    = cell[i].m_level.getPointer();
    }
    if (!mLevel) mLevel = mCell[i].getSimpleLevel();
  }

  if (!level || !mLevel) return;

  TFilePath fp(fullpath.toStdString());

  TXshLevel *txl = level->getScene()->createNewLevel(
      level->getType(), fp.getWideName(), level->getResolution());
  TXshSimpleLevel *newLevel = txl->getSimpleLevel();
  newLevel->setPath(fp);
  newLevel->setPalette(level->getPalette());
  newLevel->clonePropertiesFrom(level);
  //  newLevel->setPath(fp);

  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

  int count = 0;
  for (int i = 0; i < (int)cell.size(); i++) {
    if (cell[i].isEmpty() && mCell[i].isEmpty()) continue;

    TAffine imgAff, matchAff;

    getColumnPlacement(imgAff, xsh, std::min(start, mStart) + i, column, false);
    getColumnPlacement(matchAff, xsh, std::min(start, mStart) + i, mColumn,
                       false);

    // std::map<TFrameId, TFrameId>::iterator it;
    MergedPair mp(cell[i].isEmpty() ? TFrameId() : cell[i].getFrameId(),
                  mCell[i].isEmpty() ? TFrameId() : mCell[i].getFrameId(),
                  imgAff.inv() * matchAff);

    std::map<MergedPair, TFrameId>::iterator computedMergedIt =
        computedMergedMap.find(mp);

    if (computedMergedIt != computedMergedMap.end()) {
      TXshCell newCell(newLevel, computedMergedIt->second);
      xsh->setCell(i, column, newCell);
      cell[i] = newCell;
      continue;
    }

    TFrameId newFid(++count);  // level->getLastFid().getNumber()+1);
    TDimension dim = level->getResolution();
    TToonzImageP newImage;
    if (cell[i].isEmpty()) {
      newImage =
          TToonzImageP(TRasterCM32P(dim), TRect(0, 0, dim.lx - 1, dim.ly - 1));
      newImage->setDpi(dpix, dpiy);
    } else
      newImage = (TToonzImageP)(cell[i].getImage(false)->cloneImage());

    newImage->setPalette(level->getPalette());

    newLevel->setFrame(newFid, newImage);
    TXshCell newCell(newLevel, newFid);
    xsh->setCell(i, column, newCell);
    computedMergedMap[mp] = newCell.getFrameId();

    cell[i] = newCell;

    TImageP img   = cell[i].getImage(true);
    TImageP match = mCell[i].getImage(true);
    TFrameId fid  = cell[i].m_frameId;
    TFrameId mFid = mCell[i].m_frameId;

    if (!img || !match) continue;

    TToonzImageP timg   = (TToonzImageP)img;
    TToonzImageP tmatch = (TToonzImageP)match;

    QString id = "MergeCmappedUndo" + QString::number(MergeCmappedSessionId) +
                 "-" + QString::number(fid.getNumber());
    TImageCache::instance()->add(id, timg->clone());
    images[fid] = id;

    TAffine dpiAff  = getDpiAffine(level, fid);
    TAffine mdpiAff = getDpiAffine(mLevel, mFid);
    matchingLevels.push_back(MergeCmappedPair(cell[i], imgAff * dpiAff,
                                              mCell[i], matchAff * mdpiAff));
  }

  if (!isRedo) {
    TPalette *plt = level->getPalette();

    TPaletteHandle *pltHandle = new TPaletteHandle();
    pltHandle->setPalette(plt);
    int styleCount = plt->getStyleCount();

    TUndoManager::manager()->add(new MergeCmappedUndo(
        txl, MergeCmappedSessionId, column, level, images, mColumn, plt));
  }

  removeLevel(xl);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  mergeCmapped(matchingLevels);
  QApplication::restoreOverrideCursor();

  for (int i = 0; i < (int)cell.size(); i++)  // the saveboxes must be updated
  {
    if (cell[i].isEmpty() || mCell[i].isEmpty()) continue;

    if (!cell[i].getImage(false) || !mCell[i].getImage(false)) continue;

    TXshSimpleLevel *sl = cell[i].getSimpleLevel();
    const TFrameId &fid = cell[i].m_frameId;

    ToolUtils::updateSaveBox(sl, fid);
    IconGenerator::instance()->invalidate(sl, fid);
    sl->setDirtyFlag(true);
  }

  newLevel->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

namespace {

const TXshCell *findCell(int column, const TFrameId &fid) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int i;
  for (i = 0; i < xsh->getColumn(column)->getMaxFrame(); i++)
    if (xsh->getCell(i, column).getFrameId() == fid)
      return &(xsh->getCell(i, column));
  return 0;
}

bool contains(const std::vector<TFrameId> &v, const TFrameId &val) {
  int i;
  for (i = 0; i < (int)v.size(); i++)
    if (v[i] == val) return true;
  return false;
}

//-----------------------------------------------------------------------------

QString indexes2string(const std::set<TFrameId> fids) {
  if (fids.empty()) return "";

  QString str;

  std::set<TFrameId>::const_iterator it = fids.begin();

  str = QString::number(it->getNumber());

  while (it != fids.end()) {
    std::set<TFrameId>::const_iterator it1 = it;
    it1++;

    int lastVal = it->getNumber();
    while (it1 != fids.end() && it1->getNumber() == lastVal + 1) {
      lastVal = it1->getNumber();
      it1++;
    }

    if (lastVal != it->getNumber()) str += "-" + QString::number(lastVal);
    if (it1 == fids.end()) return str;

    str += ", " + QString::number(it1->getNumber());

    it = it1;
  }

  return str;
}

//-----------------------------------------------------------------------------

std::vector<int> string2Indexes(const QString &values) {
  std::vector<int> ret;
  int i, j;
  bool ok;
  QStringList vals = values.split(',', QString::SkipEmptyParts);
  for (i = 0; i < vals.size(); i++) {
    if (vals.at(i).contains('-')) {
      QStringList vals1 = vals.at(i).split('-', QString::SkipEmptyParts);
      if (vals1.size() != 2) return std::vector<int>();
      int from = vals1.at(0).toInt(&ok);
      if (!ok) return std::vector<int>();
      int to = vals1.at(1).toInt(&ok);
      if (!ok) return std::vector<int>();

      for (j = std::min(from, to); j <= std::max(from, to); j++)
        ret.push_back(j);
    } else {
      int val = vals.at(i).toInt(&ok);
      if (!ok) return std::vector<int>();
      ret.push_back(val);
    }
  }
  std::sort(ret.begin(), ret.end());
  return ret;
}

}  // namespace

//-----------------------------------------------------------------------------
