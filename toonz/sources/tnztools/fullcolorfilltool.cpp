#include "fullcolorfilltool.h"

#include "toonz/stage2.h"
#include "tools/cursors.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/trasterimageutils.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"
#include "toonz/txsheethandle.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "tenv.h"
#include "tpalette.h"
#include "tsystem.h"

using namespace ToolUtils;

TEnv::IntVar FullColorMinFillDepth("InknpaintFullColorMinFillDepth", 4);
TEnv::IntVar FullColorMaxFillDepth("InknpaintFullColorMaxFillDepth", 12);

namespace {

//=============================================================================
// FullColorFillUndo
//-----------------------------------------------------------------------------

class FullColorFillUndo final : public TFullColorRasterUndo {
  FillParameters m_params;
  bool m_saveboxOnly;

public:
  FullColorFillUndo(TTileSetFullColor *tileSet, const FillParameters &params,
                    TXshSimpleLevel *sl, const TFrameId &fid, bool saveboxOnly)
      : TFullColorRasterUndo(tileSet, sl, fid, false, false, 0)
      , m_params(params)
      , m_saveboxOnly(saveboxOnly) {}

  void redo() const override {
    TRasterImageP image = getImage();
    if (!image) return;
    TRaster32P r;
    if (m_saveboxOnly) {
      TRectD temp = image->getBBox();
      TRect ttemp = convert(temp);
      r           = image->getRaster()->extract(ttemp);
    } else
      r = image->getRaster();

    fullColorFill(r, m_params);

    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + TFullColorRasterUndo::getSize();
  }

  QString getToolName() override {
    return QString("Fill Tool : %1")
        .arg(QString::fromStdWString(m_params.m_fillType));
  }
  int getHistoryType() override { return HistoryType::FillTool; }
};

//=============================================================================
// doFill
//-----------------------------------------------------------------------------

void doFill(const TImageP &img, const TPointD &pos, FillParameters &params,
            bool isShiftFill, TXshSimpleLevel *sl, const TFrameId &fid) {
  TTool::Application *app = TTool::getApplication();
  if (!app || !sl) return;

  if (TRasterImageP ri = TRasterImageP(img)) {
    TPoint offs(0, 0);
    TRaster32P ras = ri->getRaster();
    // only accept 32bpp images for now
    if (!ras.getPointer() || ras->isEmpty()) return;

    ras->lock();

    TTileSetFullColor *tileSet = new TTileSetFullColor(ras->getSize());
    TTileSaverFullColor tileSaver(ras, tileSet);
    TDimension imageSize = ras->getSize();
    TPointD p(imageSize.lx % 2 ? 0.0 : 0.5, imageSize.ly % 2 ? 0.0 : 0.5);

    /*-- params.m_p = convert(pos-p)では、マイナス座標でずれが生じる --*/
    TPointD tmp_p = pos - p;
    params.m_p = TPoint((int)floor(tmp_p.x + 0.5), (int)floor(tmp_p.y + 0.5));

    params.m_p += ras->getCenter();
    params.m_p -= offs;
    params.m_shiftFill = isShiftFill;

    TRect rasRect(ras->getSize());
    if (!rasRect.contains(params.m_p)) {
      ras->unlock();
      return;
    }

    fullColorFill(ras, params, &tileSaver);

    if (tileSaver.getTileSet()->getTileCount() != 0) {
      static int count = 0;
      TSystem::outputDebug("RASTERFILL" + std::to_string(count++) + "\n");
      if (offs != TPoint())
        for (int i = 0; i < tileSet->getTileCount(); i++) {
          TTileSet::Tile *t = tileSet->editTile(i);
          t->m_rasterBounds = t->m_rasterBounds + offs;
        }
      TUndoManager::manager()->add(
          new FullColorFillUndo(tileSet, params, sl, fid,
                                Preferences::instance()->getFillOnlySavebox()));
    }

    sl->getProperties()->setDirtyFlag(true);

    ras->unlock();
  }

  TTool *t = app->getCurrentTool()->getTool();
  if (t) t->notifyImageChanged();
}
};

//=============================================================================
// FullColorFillTool
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

FullColorFillTool::FullColorFillTool()
    : TTool("T_Fill"), m_fillDepth("Fill Depth", 0, 15, 4, 12) {
  bind(TTool::RasterImage);
  m_prop.bind(m_fillDepth);
}

void FullColorFillTool::updateTranslation() {
  m_fillDepth.setQStringName(tr("Fill Depth"));
}

FillParameters FullColorFillTool::getFillParameters() const {
  FillParameters params;
  int styleId           = TTool::getApplication()->getCurrentLevelStyleIndex();
  params.m_styleId      = styleId;
  params.m_minFillDepth = (int)m_fillDepth.getValue().first;
  params.m_maxFillDepth = (int)m_fillDepth.getValue().second;

  if (m_level) params.m_palette = m_level->getPalette();
  return params;
}

void FullColorFillTool::leftButtonDown(const TPointD &pos,
                                       const TMouseEvent &e) {
  m_clickPoint  = pos;
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  m_level       = xl ? xl->getSimpleLevel() : 0;
  FillParameters params = getFillParameters();
  doFill(getImage(true), pos, params, e.isShiftPressed(), m_level.getPointer(),
         getCurrentFid());
  invalidate();
}

void FullColorFillTool::leftButtonDrag(const TPointD &pos,
                                       const TMouseEvent &e) {
  FillParameters params = getFillParameters();
  if (m_clickPoint == pos) return;
  if (!m_level || !params.m_palette) return;
  TImageP img = getImage(true);
  TPixel32 fillColor =
      params.m_palette->getStyle(params.m_styleId)->getMainColor();
  if (TRasterImageP ri = img) {
    TRaster32P ras = ri->getRaster();
    if (!ras) return;
    TPointD center = ras->getCenterD();
    TPoint ipos    = convert(pos + center);
    if (!ras->getBounds().contains(ipos)) return;
    TPixel32 pix = ras->pixels(ipos.y)[ipos.x];
    if (pix == fillColor) {
      invalidate();
      return;
    }
  } else
    return;
  doFill(img, pos, params, e.isShiftPressed(), m_level.getPointer(),
         getCurrentFid());
  invalidate();
}

bool FullColorFillTool::onPropertyChanged(std::string propertyName) {
  // Fill Depth
  if (propertyName == m_fillDepth.getName()) {
    FullColorMinFillDepth = (int)m_fillDepth.getValue().first;
    FullColorMaxFillDepth = (int)m_fillDepth.getValue().second;
  }
  return true;
}

void FullColorFillTool::onActivate() {
  static bool firstTime = true;
  if (firstTime) {
    m_fillDepth.setValue(TDoublePairProperty::Value(FullColorMinFillDepth,
                                                    FullColorMaxFillDepth));
    firstTime = false;
  }
}

int FullColorFillTool::getCursorId() const {
  int ret = ToolCursor::FillCursor;
  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    ret = ret | ToolCursor::Ex_Negate;
  return ret;
}

FullColorFillTool FullColorRasterFillTool;