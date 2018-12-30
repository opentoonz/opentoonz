

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
#include "toonzqt/icongenerator.h"
#include <map>
#include <QRadioButton>
#include <QPushButton>
#include <QApplication>
#include "tundo.h"
#include "tools/toolutils.h"
#include "timagecache.h"
#include "tcolorstyles.h"

#include "historytypes.h"

using namespace DVGui;

namespace {

class MatchlinePair {
public:
  const TXshCell *m_cell;
  TAffine m_imgAff;
  const TXshCell *m_mcell;
  TAffine m_matchAff;

  MatchlinePair(const TXshCell &cell, const TAffine &imgAff,
                const TXshCell &mcell, const TAffine &matchAff)
      : m_cell(&cell)
      , m_imgAff(imgAff)
      , m_mcell(&mcell)
      , m_matchAff(matchAff){};
};

//-----------------------------------------------------------------------------------

void mergeRasterColumns(const std::vector<MatchlinePair> &matchingLevels) {
  if (matchingLevels.empty()) return;

  int i = 0;
  for (i = 0; i < (int)matchingLevels.size(); i++) {
    TRasterImageP img = (TRasterImageP)matchingLevels[i].m_cell->getImage(true);
    TRasterImageP match =
        (TRasterImageP)matchingLevels[i].m_mcell->getImage(false);
    if (!img || !match)
      throw TRopException("Can merge columns only on raster images!");
    // img->lock();
    TRaster32P ras      = img->getRaster();    // img->getCMapped(false);
    TRaster32P matchRas = match->getRaster();  // match->getCMapped(true);
    if (!ras || !matchRas) {
      DVGui::warning(QObject::tr(
          "The merge command is not available for greytones images."));
      return;
    }
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
      TRaster32P aux(lxout, lyout);
      TRop::resample(aux, matchRas, aff);
      matchRas = aux;
    }
    ras->lock();
    matchRas->lock();
    TRect raux = matchRas->getBounds() + offs;
    TRasterP r = ras->extract(raux);

    TRop::over(r, matchRas);

    ras->unlock();
    matchRas->unlock();

    img->setSavebox(img->getSavebox() + (matchRas->getBounds() + offs));
    // img->setSavebox(rout);
    // img->unlock();
  }
}

/*------------------------------------------------------------------------*/
/// verifica se tutta l'immagine e' gia' contenuta in un gruppo.
bool needTobeGrouped(const TVectorImageP &vimg) {
  if (vimg->getStrokeCount() <= 1) return false;

  if (!vimg->isStrokeGrouped(0)) return true;

  for (int i = 1; i < vimg->getStrokeCount(); i++) {
    if (vimg->areDifferentGroup(0, false, i, false) == 0) return true;
  }

  return false;
}

//---------------------------------------------------------------------------------------

void mergeVectorColumns(const std::vector<MatchlinePair> &matchingLevels) {
  if (matchingLevels.empty()) return;

  int i = 0;
  for (i = 0; i < (int)matchingLevels.size(); i++) {
    TVectorImageP vimg =
        (TVectorImageP)matchingLevels[i].m_cell->getImage(true);
    TVectorImageP vmatch =
        (TVectorImageP)matchingLevels[i].m_mcell->getImage(false);
    if (!vimg || !vmatch)
      throw TRopException("Cannot merge columns of different image types!");
    // img->lock();

    if (needTobeGrouped(vimg)) vimg->group(0, vimg->getStrokeCount());
    bool ungroup = false;
    if (needTobeGrouped(vmatch)) {
      ungroup = true;
      vmatch->group(0, vmatch->getStrokeCount());
    }
    TAffine aff =
        matchingLevels[i].m_imgAff.inv() * matchingLevels[i].m_matchAff;
    vimg->mergeImage(vmatch, aff);
    if (ungroup) vmatch->ungroup(0);
  }
}

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
}  // namespace
//-----------------------------------------------------------------------------

class MergeColumnsUndo final : public TUndo {
  TXshLevelP m_xl;
  int m_matchlineSessionId;
  std::map<TFrameId, QString> m_images;
  TXshSimpleLevel *m_level;

  int m_column, m_mColumn;
  TPalette *m_palette;

public:
  MergeColumnsUndo(TXshLevelP xl, int matchlineSessionId, int column,
                   TXshSimpleLevel *level,
                   const std::map<TFrameId, QString> &images, int mColumn,
                   TPalette *palette)
      : TUndo()
      , m_xl(xl)
      , m_matchlineSessionId(matchlineSessionId)
      , m_level(level)
      , m_column(column)
      , m_mColumn(mColumn)
      , m_images(images)
      , m_palette(palette->clone()) {}

  void undo() const override {
    QApplication::setOverrideCursor(Qt::WaitCursor);

    std::map<TFrameId, QString>::const_iterator it = m_images.begin();

    std::vector<TFrameId> fids;
    m_level->setPalette(m_palette->clone());
    for (; it != m_images.end(); ++it)  //, ++mit)
    {
      QString id = "MergeColumnsUndo" + QString::number(m_matchlineSessionId) +
                   "-" + QString::number(it->first.getNumber());
      TImageP img = TImageCache::instance()->get(id, false)->cloneImage();
      m_level->setFrame(it->first, img);
      fids.push_back(it->first);
    }

    if (m_xl) invalidateIcons(m_xl.getPointer(), fids);

    m_level->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    QApplication::restoreOverrideCursor();
  }

  void redo() const override {
    QApplication::setOverrideCursor(Qt::WaitCursor);

    mergeColumns(m_column, m_mColumn, true);

    QApplication::restoreOverrideCursor();
  }

  int getSize() const override { return sizeof(*this); }

  ~MergeColumnsUndo() {
    std::map<TFrameId, QString>::const_iterator it = m_images.begin();
    for (; it != m_images.end(); ++it)  //, ++mit)
    {
      QString id = "MergeColumnsUndo" + QString::number(m_matchlineSessionId) +
                   "-" + QString::number(it->first.getNumber());
      TImageCache::instance()->remove(id);
    }
  }

  QString getHistoryString() override {
    return QObject::tr("Merge Raster Levels");
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

//-----------------------------------------------------------------------------

void mergeColumns(const std::set<int> &columns) {
  QApplication::setOverrideCursor(Qt::WaitCursor);

  std::set<int>::const_iterator it = columns.begin();

  int dstColumn = *it;
  ++it;

  TUndoManager::manager()->beginBlock();

  for (; it != columns.end(); ++it) mergeColumns(dstColumn, *it, false);

  TUndoManager::manager()->endBlock();

  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------

static int MergeColumnsSessionId = 0;

void mergeColumns(int column, int mColumn, bool isRedo) {
  if (!isRedo) MergeColumnsSessionId++;
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int start, end;
  xsh->getCellRange(column, start, end);

  if (start > end) return;
  std::vector<TXshCell> cell(end - start + 1);
  std::vector<TXshCell> mCell(end - start + 1);

  xsh->getCells(start, column, cell.size(), &(cell[0]));

  xsh->getCells(start, mColumn, cell.size(), &(mCell[0]));

  TXshColumn *col  = xsh->getColumn(column);
  TXshColumn *mcol = xsh->getColumn(mColumn);

  std::vector<MatchlinePair> matchingLevels;

  std::set<TFrameId> alreadyDoneSet;

  TXshSimpleLevel *level = 0, *mLevel = 0;
  TXshLevelP xl;
  bool areRasters = false;

  std::map<TFrameId, QString> images;

  for (int i = 0; i < (int)cell.size(); i++) {
    if (cell[i].isEmpty() || mCell[i].isEmpty()) continue;
    if (!level) {
      level = cell[i].getSimpleLevel();
      xl    = cell[i].m_level;
    }

    else if (level != cell[i].getSimpleLevel()) {
      DVGui::warning(
          QObject::tr("It is not possible to perform a merging involving more "
                      "than one level per column."));
      return;
    }

    if (!mLevel)
      mLevel = mCell[i].getSimpleLevel();
    else if (mLevel != mCell[i].getSimpleLevel()) {
      DVGui::warning(
          QObject::tr("It is not possible to perform a merging involving more "
                      "than one level per column."));
      return;
    }
    TImageP img   = cell[i].getImage(true);
    TImageP match = mCell[i].getImage(false);
    TFrameId fid  = cell[i].m_frameId;
    TFrameId mFid = mCell[i].m_frameId;

    if (!img || !match) continue;

    if (alreadyDoneSet.find(fid) == alreadyDoneSet.end()) {
      TRasterImageP timg   = (TRasterImageP)img;
      TRasterImageP tmatch = (TRasterImageP)match;
      TVectorImageP vimg   = (TVectorImageP)img;
      TVectorImageP vmatch = (TVectorImageP)match;

      if (timg) {
        if (!tmatch) {
          DVGui::warning(QObject::tr(
              "Only raster levels can be merged to a raster level."));
          return;
        }
        areRasters = true;
      } else if (vimg) {
        if (!vmatch) {
          DVGui::warning(QObject::tr(
              "Only vector levels can be merged to a vector level."));
          return;
        }
      } else {
        DVGui::warning(
            QObject::tr("It is possible to merge only Toonz vector levels or "
                        "standard raster levels."));
        return;
      }

      if (!isRedo) {
        QString id = "MergeColumnsUndo" +
                     QString::number(MergeColumnsSessionId) + "-" +
                     QString::number(fid.getNumber());
        TImageCache::instance()->add(
            id, (timg) ? timg->cloneImage() : vimg->cloneImage());
        images[fid] = id;
      }
      TAffine imgAff, matchAff;
      getColumnPlacement(imgAff, xsh, start + i, column, false);
      getColumnPlacement(matchAff, xsh, start + i, mColumn, false);
      TAffine dpiAff  = getDpiAffine(level, fid);
      TAffine mdpiAff = getDpiAffine(mLevel, mFid);
      matchingLevels.push_back(MatchlinePair(cell[i], imgAff * dpiAff, mCell[i],
                                             matchAff * mdpiAff));
      alreadyDoneSet.insert(fid);
    }
  }

  if (matchingLevels.empty()) {
    DVGui::warning(
        QObject::tr("It is possible to merge only Toonz vector levels or "
                    "standard raster levels."));
    return;
  }

  ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
  TXshSimpleLevel *simpleLevel =
      sc->getLevelSet()->getLevel(column)->getSimpleLevel();

  if (!isRedo)
    TUndoManager::manager()->add(
        new MergeColumnsUndo(xl, MergeColumnsSessionId, column, level, images,
                             mColumn, level->getPalette()));

  if (areRasters) {
    mergeRasterColumns(matchingLevels);
    for (int i = 0; i < (int)cell.size(); i++)  // the saveboxes must be updated
    {
      if (cell[i].isEmpty() || mCell[i].isEmpty()) continue;

      if (!cell[i].getImage(false) || !mCell[i].getImage(false)) continue;

      ToolUtils::updateSaveBox(cell[i].getSimpleLevel(), cell[i].m_frameId);
    }
  } else
    mergeVectorColumns(matchingLevels);

  TXshLevel *sl =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet()->getLevel(
          column);
  std::vector<TFrameId> fidsss;
  sl->getFids(fidsss);
  invalidateIcons(sl, fidsss);
  sl->setDirtyFlag(true);
  level->setDirtyFlag(true);
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

}  // namespace
