

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/preferences.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "toonz/tonionskinmaskhandle.h"
#include "timagecache.h"
#include "tundo.h"
#include "tpalette.h"
#include "tcolorstyles.h"
#include "tvectorimage.h"
#include "tools/cursors.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "tenv.h"
#include "tools/stylepicker.h"
#include "toonz/fill.h"
#include "toonz/stage2.h"
#include "tstroke.h"
#include "drawutil.h"
#include "tsystem.h"
#include "tinbetween.h"
#include "tregion.h"
#include "tgl.h"
#include "trop.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/onionskinmask.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/toonzimageutils.h"
#include "toonz/levelproperties.h"
#include "toonz/strokegenerator.h"
#include "toonz/txshcell.h"
#include "toonzqt/imageutils.h"
#include "autofill.h"

#include "historytypes.h"

#include <stack>

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

#define NORMALFILL L"Normal"
#define RECTFILL L"Rectangular"
#define FREEHANDFILL L"Freehand"
#define POLYLINEFILL L"Polyline"

TEnv::IntVar MinFillDepth("InknpaintMinFillDepth", 0);
TEnv::IntVar MaxFillDepth("InknpaintMaxFillDepth", 10);
TEnv::StringVar FillType("InknpaintFillType", "Normal");
TEnv::StringVar FillColorType("InknpaintFillColorType", "Areas");
TEnv::IntVar FillSelective("InknpaintFillSelective", 0);
TEnv::IntVar FillOnion("InknpaintFillOnion", 0);
TEnv::IntVar FillSegment("InknpaintFillSegment", 0);
TEnv::IntVar FillRange("InknpaintFillRange", 0);

//-----------------------------------------------------------------------------
namespace {

inline int vectorFill(const TVectorImageP &img, const std::wstring &type,
                      const TPointD &point, int style, bool emptyOnly = false) {
  if (type == ALL || type == LINES) {
    int oldStyleId = img->fillStrokes(point, style);
    if (oldStyleId != -1) return oldStyleId;
  }

  if (type == ALL || type == AREAS) return img->fill(point, style, emptyOnly);

  return -1;
}

//=============================================================================
// VectorFillUndo
//-----------------------------------------------------------------------------

class VectorFillUndo final : public TToolUndo {
  int m_oldColorStyle;
  int m_newColorStyle;
  TPointD m_point;
  std::wstring m_type;
  int m_row;
  int m_column;

public:
  VectorFillUndo(int newColorStyle, int oldColorStyle, std::wstring fillType,
                 TPointD clickPoint, TXshSimpleLevel *sl, const TFrameId &fid)
      : TToolUndo(sl, fid)
      , m_newColorStyle(newColorStyle)
      , m_oldColorStyle(oldColorStyle)
      , m_point(clickPoint)
      , m_type(fillType) {
    TTool::Application *app = TTool::getApplication();
    if (app) {
      m_row    = app->getCurrentFrame()->getFrame();
      m_column = app->getCurrentColumn()->getColumnIndex();
    }
  }

  void undo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    app->getCurrentLevel()->setLevel(m_level.getPointer());
    TVectorImageP img = m_level->getFrame(m_frameId, true);
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentFrame()->setFrame(m_row);
      app->getCurrentColumn()->setColumnIndex(m_column);
    } else
      app->getCurrentFrame()->setFid(m_frameId);
    assert(img);
    if (!img) return;
    QMutexLocker lock(img->getMutex());

    vectorFill(img, m_type, m_point, m_oldColorStyle);

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    app->getCurrentLevel()->setLevel(m_level.getPointer());
    TVectorImageP img = m_level->getFrame(m_frameId, true);
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentFrame()->setFrame(m_row);
      app->getCurrentColumn()->setColumnIndex(m_column);
    } else
      app->getCurrentFrame()->setFid(m_frameId);
    assert(img);
    if (!img) return;
    QMutexLocker lock(img->getMutex());

    vectorFill(img, m_type, m_point, m_newColorStyle);

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void onAdd() override {}

  int getSize() const override { return sizeof(*this); }

  QString getToolName() override {
    return QString("Fill Tool : %1").arg(QString::fromStdWString(m_type));
  }
  int getHistoryType() override { return HistoryType::FillTool; }
};

//=============================================================================
// VectorRectFillUndo
//-----------------------------------------------------------------------------

class VectorRectFillUndo final : public TToolUndo {
  std::vector<TFilledRegionInf> *m_regionFillInformation;
  std::vector<std::pair<int, int>> *m_strokeFillInformation;
  TRectD m_selectionArea;
  int m_styleId;
  bool m_unpaintedOnly;
  TStroke *m_stroke;

public:
  ~VectorRectFillUndo() {
    if (m_regionFillInformation) delete m_regionFillInformation;
    if (m_strokeFillInformation) delete m_strokeFillInformation;
    if (m_stroke) delete m_stroke;
  }

  VectorRectFillUndo(std::vector<TFilledRegionInf> *regionFillInformation,
                     std::vector<std::pair<int, int>> *strokeFillInformation,
                     TRectD selectionArea, TStroke *stroke, int styleId,
                     bool unpaintedOnly, TXshSimpleLevel *sl,
                     const TFrameId &fid)
      : TToolUndo(sl, fid)
      , m_regionFillInformation(regionFillInformation)
      , m_strokeFillInformation(strokeFillInformation)
      , m_selectionArea(selectionArea)
      , m_styleId(styleId)
      , m_unpaintedOnly(unpaintedOnly)
      , m_stroke(0) {
    if (stroke) m_stroke = new TStroke(*stroke);
  }

  void undo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TVectorImageP img = m_level->getFrame(m_frameId, true);
    assert(!!img);
    if (!img) return;
    if (m_regionFillInformation) {
      for (UINT i = 0; i < m_regionFillInformation->size(); i++) {
        TRegion *reg = img->getRegion((*m_regionFillInformation)[i].m_regionId);
        if (reg) reg->setStyle((*m_regionFillInformation)[i].m_styleId);
      }
    }
    if (m_strokeFillInformation) {
      for (UINT i = 0; i < m_strokeFillInformation->size(); i++) {
        TStroke *s = img->getStroke((*m_strokeFillInformation)[i].first);
        s->setStyle((*m_strokeFillInformation)[i].second);
      }
    }

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TVectorImageP img = m_level->getFrame(m_frameId, true);
    assert(img);
    if (!img) return;

    img->selectFill(m_selectionArea, m_stroke, m_styleId, m_unpaintedOnly,
                    m_regionFillInformation != 0, m_strokeFillInformation != 0);
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void onAdd() override {}

  int getSize() const override {
    int size1 = m_regionFillInformation
                    ? m_regionFillInformation->capacity() *
                          sizeof(m_regionFillInformation)
                    : 0;
    int size2 = m_strokeFillInformation
                    ? m_strokeFillInformation->capacity() *
                          sizeof(m_strokeFillInformation)
                    : 0;
    return sizeof(*this) + size1 + size2 + 500;
  }

  QString getToolName() override { return QString("Fill Tool : "); }
  int getHistoryType() override { return HistoryType::FillTool; }
};

//=============================================================================
// RasterFillUndo
//-----------------------------------------------------------------------------

class RasterFillUndo final : public TRasterUndo {
  FillParameters m_params;
  bool m_saveboxOnly;

public:
  /*RasterFillUndo(TTileSetCM32 *tileSet, TPoint fillPoint,
                                                           int paintId, int
     fillDepth,
                                                           std::wstring
     fillType, bool isSegment,
                                                           bool selective, bool
     isShiftFill,
                                                           TXshSimpleLevel* sl,
     const TFrameId& fid)*/
  RasterFillUndo(TTileSetCM32 *tileSet, const FillParameters &params,
                 TXshSimpleLevel *sl, const TFrameId &fid, bool saveboxOnly)
      : TRasterUndo(tileSet, sl, fid, false, false, 0)
      , m_params(params)
      , m_saveboxOnly(saveboxOnly) {}

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    bool recomputeSavebox = false;
    TRasterCM32P r;
    if (m_saveboxOnly) {
      TRectD temp = image->getBBox();
      TRect ttemp = convert(temp);
      r           = image->getRaster()->extract(ttemp);
    } else
      r = image->getRaster();
    if (m_params.m_fillType == ALL || m_params.m_fillType == AREAS) {
      if (m_params.m_shiftFill) {
        FillParameters aux(m_params);
        aux.m_styleId    = (m_params.m_styleId == 0) ? 1 : 0;
        recomputeSavebox = fill(r, aux);
      }
      recomputeSavebox = fill(r, m_params);
    }
    if (m_params.m_fillType == ALL || m_params.m_fillType == LINES) {
      if (m_params.m_segment)
        inkSegment(r, m_params.m_p, m_params.m_styleId, 2.51, true);
      else
        inkFill(r, m_params.m_p, m_params.m_styleId, 2);
    }

    if (recomputeSavebox) ToolUtils::updateSaveBox();

    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override {
    return QString("Fill Tool : %1")
        .arg(QString::fromStdWString(m_params.m_fillType));
  }
  int getHistoryType() override { return HistoryType::FillTool; }
};

//=============================================================================
// RasterRectFillUndo
//-----------------------------------------------------------------------------

class RasterRectFillUndo final : public TRasterUndo {
  TRect m_fillArea;
  int m_paintId;
  std::wstring m_colorType;
  TStroke *m_s;
  bool m_onlyUnfilled;
  TPalette *m_palette;

public:
  ~RasterRectFillUndo() {
    if (m_s) delete m_s;
  }

  RasterRectFillUndo(TTileSetCM32 *tileSet, TStroke *s, TRect fillArea,
                     int paintId, TXshSimpleLevel *level,
                     std::wstring colorType, bool onlyUnfilled,
                     const TFrameId &fid, TPalette *palette)
      : TRasterUndo(tileSet, level, fid, false, false, 0)
      , m_fillArea(fillArea)
      , m_paintId(paintId)
      , m_colorType(colorType)
      , m_onlyUnfilled(onlyUnfilled)
      , m_palette(palette) {
    m_s = s ? new TStroke(*s) : 0;
  }

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    TRasterCM32P ras = image->getRaster();
    AreaFiller filler(ras);
    if (!m_s)
      filler.rectFill(m_fillArea, m_paintId, m_onlyUnfilled,
                      m_colorType != LINES, m_colorType != AREAS);
    else
      filler.strokeFill(m_s, m_paintId, m_onlyUnfilled, m_colorType != LINES,
                        m_colorType != AREAS);

    if (m_palette) {
      TRect rect   = m_fillArea;
      TRect bounds = ras->getBounds();
      if (bounds.overlaps(rect)) {
        rect *= bounds;
        const TTileSetCM32::Tile *tile =
            m_tiles->getTile(m_tiles->getTileCount() - 1);
        TRasterCM32P rbefore;
        tile->getRaster(rbefore);
        fillautoInks(ras, rect, rbefore, m_palette);
      }
    }
    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    int size =
        m_s ? m_s->getControlPointCount() * sizeof(TThickPoint) + 100 : 0;
    return sizeof(*this) + TRasterUndo::getSize() + size;
  }

  QString getToolName() override {
    return QString("Fill Tool : %1").arg(QString::fromStdWString(m_colorType));
  }
  int getHistoryType() override { return HistoryType::FillTool; }
};

//=============================================================================
// RasterRectAutoFillUndo
//-----------------------------------------------------------------------------

class RasterRectAutoFillUndo final : public TRasterUndo {
  TRect m_rectToFill;
  TFrameId m_fidToLearn;
  bool m_onlyUnfilled;

public:
  ~RasterRectAutoFillUndo() {}

  RasterRectAutoFillUndo(TTileSetCM32 *tileSet, const TRect &rectToFill,
                         TXshSimpleLevel *level, bool onlyUnfilled,
                         const TFrameId &currentFid, const TFrameId &fidToLearn)
      : TRasterUndo(tileSet, level, currentFid, false, false, 0)
      , m_rectToFill(rectToFill)
      , m_onlyUnfilled(onlyUnfilled)
      , m_fidToLearn(fidToLearn) {}

  void redo() const override {
    TToonzImageP image        = getImage();
    TToonzImageP imageToLearn = m_level->getFrame(m_fidToLearn, false);
    if (!image || !imageToLearn) return;
    rect_autofill_learn(imageToLearn, m_rectToFill.x0, m_rectToFill.y0,
                        m_rectToFill.x1, m_rectToFill.y1);

    TTileSetCM32 tileSet(image->getRaster()->getSize());
    bool recomputeBBox = rect_autofill_apply(
        image, m_rectToFill.x0, m_rectToFill.y0, m_rectToFill.x1,
        m_rectToFill.y1, m_onlyUnfilled, &tileSet);
    if (recomputeBBox) ToolUtils::updateSaveBox();
    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }
};

//=============================================================================
// RasterStrokeAutoFillUndo
//-----------------------------------------------------------------------------

class RasterStrokeAutoFillUndo final : public TRasterUndo {
  TTileSetCM32 *m_tileSet;

public:
  ~RasterStrokeAutoFillUndo() {
    if (m_tileSet) delete m_tileSet;
  }

  RasterStrokeAutoFillUndo(TTileSetCM32 *tileSet, TXshSimpleLevel *level,
                           const TFrameId &currentFid)
      : TRasterUndo(tileSet, level, currentFid, false, false, 0)
      , m_tileSet(0) {}

  void setTileSet(TTileSetCM32 *tileSet) { m_tileSet = tileSet; }

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;

    ToonzImageUtils::paste(image, m_tileSet);
    ToolUtils::updateSaveBox(m_level, m_frameId);

    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize() + m_tileSet->getMemorySize();
  }
};

//=============================================================================
// RasterRectAutoFillUndo
//-----------------------------------------------------------------------------

class VectorAutoFillUndo final : public TToolUndo {
  std::vector<TFilledRegionInf> *m_regionFillInformation;
  TRectD m_selectionArea;
  TStroke *m_selectingStroke;
  bool m_unpaintedOnly;
  TFrameId m_onionFid;
  int m_row;
  int m_column;

public:
  ~VectorAutoFillUndo() {
    if (m_regionFillInformation) delete m_regionFillInformation;
    if (m_selectingStroke) delete m_selectingStroke;
  }

  VectorAutoFillUndo(std::vector<TFilledRegionInf> *regionFillInformation,
                     TRectD selectionArea, TStroke *selectingStroke,
                     bool unpaintedOnly, TXshSimpleLevel *sl,
                     const TFrameId &fid, const TFrameId &onionFid)
      : TToolUndo(sl, fid)
      , m_regionFillInformation(regionFillInformation)
      , m_selectionArea(selectionArea)
      , m_unpaintedOnly(unpaintedOnly)
      , m_onionFid(onionFid) {
    m_selectingStroke = selectingStroke ? new TStroke(*selectingStroke) : 0;
  }

  void undo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;
    TVectorImageP img = m_level->getFrame(m_frameId, true);
    ;
    assert(!!img);
    if (!img) return;
    if (m_regionFillInformation) {
      for (UINT i = 0; i < m_regionFillInformation->size(); i++) {
        TRegion *reg = img->getRegion((*m_regionFillInformation)[i].m_regionId);
        if (reg) reg->setStyle((*m_regionFillInformation)[i].m_styleId);
      }
    }

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TVectorImageP img = m_level->getFrame(m_frameId, true);
    assert(img);
    if (!img) return;

    TVectorImageP onionImg = m_level->getFrame(m_onionFid, false);
    if (!onionImg) return;

    if (m_selectingStroke) {
      stroke_autofill_learn(onionImg, m_selectingStroke);
      stroke_autofill_apply(img, m_selectingStroke, m_unpaintedOnly);
    } else {
      rect_autofill_learn(onionImg, m_selectionArea);
      rect_autofill_apply(img, m_selectionArea, m_unpaintedOnly);
    }

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    int size =
        m_selectingStroke
            ? m_selectingStroke->getControlPointCount() * sizeof(TThickPoint) +
                  100
            : 0;
    return sizeof(*this) +
           m_regionFillInformation->capacity() *
               sizeof(m_regionFillInformation) +
           500 + size;
  }
};

//-----------------------------------------------------------------------------

void doRectAutofill(const TImageP &img, const TRectD selectingRect,
                    bool onlyUnfilled, const OnionSkinMask &osMask,
                    TXshSimpleLevel *sl, const TFrameId &currentFid) {
  TToonzImageP ti(img);
  TVectorImageP vi(img);
  if (!img || !sl) return;

  std::vector<int> rows;
  osMask.getAll(sl->guessIndex(currentFid), rows);
  if (rows.empty()) return;

  TFrameId onionFid;
  int i;
  for (i = 0; i < (int)rows.size(); i++) {
    const TFrameId &app = sl->index2fid(rows[i]);
    if (app > currentFid) break;
    onionFid = app;
  }
  if (onionFid.isEmptyFrame()) onionFid = sl->index2fid(rows[0]);
  if (onionFid.isEmptyFrame() || onionFid == currentFid || !sl->isFid(onionFid))
    return;
  if (ti) {
    TRect rect = ToonzImageUtils::convertWorldToRaster(selectingRect, ti);

    TToonzImageP onionImg(sl->getFrame(onionFid, false));
    if (!onionImg) return;
    TRect workRect = rect * ti->getRaster()->getBounds();
    if (workRect.isEmpty()) return;

    rect_autofill_learn(onionImg, workRect.x0, workRect.y0, workRect.x1,
                        workRect.y1);
    TTileSetCM32 *tileSet = new TTileSetCM32(ti->getRaster()->getSize());
    bool recomputeBBox =
        rect_autofill_apply(ti, workRect.x0, workRect.y0, workRect.x1,
                            workRect.y1, onlyUnfilled, tileSet);
    if (recomputeBBox) ToolUtils::updateSaveBox();
    if (tileSet->getTileCount() > 0)
      TUndoManager::manager()->add(new RasterRectAutoFillUndo(
          tileSet, workRect, sl, onlyUnfilled, currentFid, onionFid));
  } else if (vi) {
    TVectorImageP onionImg(sl->getFrame(onionFid, false));
    if (!onionImg) return;
    std::vector<TFilledRegionInf> *regionFillInformation =
        new std::vector<TFilledRegionInf>;
    ImageUtils::getFillingInformationInArea(vi, *regionFillInformation,
                                            selectingRect);
    onionImg->findRegions();
    vi->findRegions();
    rect_autofill_learn(onionImg, selectingRect);
    bool hasFilled = rect_autofill_apply(vi, selectingRect, onlyUnfilled);
    if (hasFilled)
      TUndoManager::manager()->add(
          new VectorAutoFillUndo(regionFillInformation, selectingRect, 0,
                                 onlyUnfilled, sl, currentFid, onionFid));
  }
}

//-----------------------------------------------------------------------------

void doStrokeAutofill(const TImageP &img, TStroke *selectingStroke,
                      bool onlyUnfilled, const OnionSkinMask &osMask,
                      TXshSimpleLevel *sl, const TFrameId &currentFid) {
  TToonzImageP ti(img);
  TVectorImageP vi(img);
  if (!img || !sl) return;

  std::vector<int> rows;
  osMask.getAll(sl->guessIndex(currentFid), rows);
  if (rows.empty()) return;

  TFrameId onionFid;
  int i;
  for (i = 0; i < (int)rows.size(); i++) {
    const TFrameId &app = sl->index2fid(rows[i]);
    if (app > currentFid) break;
    onionFid = app;
  }
  if (onionFid.isEmptyFrame()) onionFid = sl->index2fid(rows[0]);
  if (onionFid.isEmptyFrame() || onionFid == currentFid || !sl->isFid(onionFid))
    return;
  if (ti) {
    TToonzImageP onionImg(sl->getFrame(onionFid, false));
    if (!onionImg) return;

    TRasterCM32P ras = onionImg->getRaster();
    TPointD center   = ras->getCenterD();

    TPoint pos;
    TRaster32P image =
        convertStrokeToImage(selectingStroke, ras->getBounds(), pos);
    TRect bbox = (image->getBounds() + pos).enlarge(2);
    pos        = bbox.getP00();

    TRasterCM32P onionAppRas = ras->extract(bbox)->clone();
    TRasterCM32P tiAppRas    = ti->getRaster()->extract(bbox)->clone();

    TRect workRect = onionAppRas->getBounds().enlarge(-1);
    TToonzImageP onionApp(onionAppRas, workRect);
    TToonzImageP tiApp(tiAppRas, workRect);

    ToonzImageUtils::eraseImage(onionApp, image, TPoint(2, 2), true, true, true,
                                false, 1);
    ToonzImageUtils::eraseImage(tiApp, image, TPoint(2, 2), true, true, true,
                                false, 1);

    rect_autofill_learn(onionApp, workRect.x0, workRect.y0, workRect.x1,
                        workRect.y1);
    TTileSetCM32 *tileSet = new TTileSetCM32(ti->getRaster()->getSize());
    bool recomputeBBox =
        rect_autofill_apply(tiApp, workRect.x0, workRect.y0, workRect.x1,
                            workRect.y1, onlyUnfilled, tileSet);

    delete tileSet;
    tileSet = new TTileSetCM32(ti->getRaster()->getSize());
    tileSet->add(ti->getRaster(), bbox);
    RasterStrokeAutoFillUndo *undo =
        new RasterStrokeAutoFillUndo(tileSet, sl, currentFid);
    TRop::over(ti->getRaster(), tiAppRas, pos);
    TTileSetCM32 *newTileSet = new TTileSetCM32(ti->getRaster()->getSize());
    newTileSet->add(ti->getRaster(), bbox);
    undo->setTileSet(newTileSet);
    TUndoManager::manager()->add(undo);

  } else if (vi) {
    TVectorImageP onionImg(sl->getFrame(onionFid, false));
    if (!onionImg) return;
    std::vector<TFilledRegionInf> *regionFillInformation =
        new std::vector<TFilledRegionInf>;
    ImageUtils::getFillingInformationInArea(vi, *regionFillInformation,
                                            selectingStroke->getBBox());
    onionImg->findRegions();
    vi->findRegions();
    stroke_autofill_learn(onionImg, selectingStroke);
    bool hasFilled = stroke_autofill_apply(vi, selectingStroke, onlyUnfilled);
    if (hasFilled)
      TUndoManager::manager()->add(new VectorAutoFillUndo(
          regionFillInformation, TRectD(), selectingStroke, onlyUnfilled, sl,
          currentFid, onionFid));
  }
}

//=============================================================================
// fillRectWithUndo
//-----------------------------------------------------------------------------

bool inline hasAutoInks(const TPalette *plt) {
  for (int i = 0; i < plt->getStyleCount(); i++)
    if (plt->getStyle(i)->getFlags() != 0) return true;
  return false;
}

//-----------------------------------------------------------------------------

void fillAreaWithUndo(const TImageP &img, const TRectD &area, TStroke *stroke,
                      bool onlyUnfilled, std::wstring colorType,
                      TXshSimpleLevel *sl, const TFrameId &fid, int cs,
                      bool autopaintLines) {
  TRectD selArea = stroke ? stroke->getBBox() : area;

  if (TToonzImageP ti = img) {
    // allargo di 1 la savebox, perche cosi' il rectfill di tutta l'immagine fa
    // una sola fillata
    TRect enlargedSavebox =
        ti->getSavebox().enlarge(1) * TRect(TPoint(0, 0), ti->getSize());
    TRect rasterFillArea =
        ToonzImageUtils::convertWorldToRaster(selArea, ti) * enlargedSavebox;
    if (rasterFillArea.isEmpty()) return;

    TRasterCM32P ras = ti->getRaster();
    /*-- tileSetでFill範囲のRectをUndoに格納しておく --*/
    TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
    tileSet->add(ras, rasterFillArea);
    AreaFiller filler(ti->getRaster());
    if (!stroke)
      filler.rectFill(rasterFillArea, cs, onlyUnfilled, colorType != LINES,
                      colorType != AREAS);
    else
      filler.strokeFill(stroke, cs, onlyUnfilled, colorType != LINES,
                        colorType != AREAS);

    TPalette *plt = ti->getPalette();

    // !autopaintLines will temporary disable autopaint line feature
    if ((plt && !hasAutoInks(plt)) || !autopaintLines) plt = 0;

    std::set<int> autoInks;
    autoInks.insert(3);
    autoInks.insert(4);
    autoInks.insert(5);

    if (plt) {
      TRect rect   = rasterFillArea;
      TRect bounds = ras->getBounds();
      if (bounds.overlaps(rect)) {
        rect *= bounds;
        const TTileSetCM32::Tile *tile =
            tileSet->getTile(tileSet->getTileCount() - 1);
        TRasterCM32P rbefore;
        tile->getRaster(rbefore);
        fillautoInks(ras, rect, rbefore, plt);
      }
    }
    ToolUtils::updateSaveBox(sl, fid);

    TUndoManager::manager()->add(
        new RasterRectFillUndo(tileSet, stroke, rasterFillArea, cs, sl,
                               colorType, onlyUnfilled, fid, plt));
  } else if (TVectorImageP vi = img) {
    TPalette *palette = vi->getPalette();
    assert(palette);
    const TColorStyle *style = palette->getStyle(cs);
    // if( !style->isRegionStyle() )
    // return;

    vi->findRegions();

    std::vector<TFilledRegionInf> *regionFillInformation = 0;
    std::vector<std::pair<int, int>> *strokeFillInformation = 0;
    if (colorType != LINES) {
      regionFillInformation = new std::vector<TFilledRegionInf>;
      ImageUtils::getFillingInformationInArea(vi, *regionFillInformation,
                                              selArea);
    }
    if (colorType != AREAS) {
      strokeFillInformation = new std::vector<std::pair<int, int>>;
      ImageUtils::getStrokeStyleInformationInArea(vi, *strokeFillInformation,
                                                  selArea);
    }

    VectorRectFillUndo *fUndo =
        new VectorRectFillUndo(regionFillInformation, strokeFillInformation,
                               selArea, stroke, cs, onlyUnfilled, sl, fid);

    QMutexLocker lock(vi->getMutex());
    if (vi->selectFill(area, stroke, cs, onlyUnfilled, colorType != LINES,
                       colorType != AREAS))
      TUndoManager::manager()->add(fUndo);
    else
      delete fUndo;
  }
}

//=============================================================================
// doFill
//-----------------------------------------------------------------------------

void doFill(const TImageP &img, const TPointD &pos, FillParameters &params,
            bool isShiftFill, TXshSimpleLevel *sl, const TFrameId &fid,
            bool autopaintLines) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (TToonzImageP ti = TToonzImageP(img)) {
    TPoint offs(0, 0);
    TRasterCM32P ras = ti->getRaster();

    if (Preferences::instance()->getFillOnlySavebox()) {
      TRectD bbox = ti->getBBox();
      TRect ibbox = convert(bbox);
      offs        = ibbox.getP00();
      ras         = ti->getRaster()->extract(ibbox);
    }

    bool recomputeSavebox = false;
    TPalette *plt         = ti->getPalette();

    if (!ras.getPointer() || ras->isEmpty()) return;

    ras->lock();

    TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
    TTileSaverCM32 tileSaver(ras, tileSet);
    TDimension imageSize = ti->getSize();
    TPointD p(imageSize.lx % 2 ? 0.0 : 0.5, imageSize.ly % 2 ? 0.0 : 0.5);

    /*-- params.m_p = convert(pos-p)では、マイナス座標でずれが生じる --*/
    TPointD tmp_p = pos - p;
    params.m_p = TPoint((int)floor(tmp_p.x + 0.5), (int)floor(tmp_p.y + 0.5));

    params.m_p += ti->getRaster()->getCenter();
    params.m_p -= offs;
    params.m_shiftFill = isShiftFill;

    TRect rasRect(ras->getSize());
    if (!rasRect.contains(params.m_p)) {
      ras->unlock();
      return;
    }

    // !autoPaintLines will temporary disable autopaint line feature
    if (plt && hasAutoInks(plt) && autopaintLines) params.m_palette = plt;

    if (params.m_fillType == ALL || params.m_fillType == AREAS) {
      if (isShiftFill) {
        FillParameters aux(params);
        aux.m_styleId    = (params.m_styleId == 0) ? 1 : 0;
        recomputeSavebox = fill(ras, aux, &tileSaver);
      }
      recomputeSavebox = fill(ras, params, &tileSaver);
    }
    if (params.m_fillType == ALL || params.m_fillType == LINES) {
      if (params.m_segment)
        inkSegment(ras, params.m_p, params.m_styleId, 2.51, true, &tileSaver);
      else if (!params.m_segment)
        inkFill(ras, params.m_p, params.m_styleId, 2, &tileSaver);
    }

    if (tileSaver.getTileSet()->getTileCount() != 0) {
      static int count = 0;
      TSystem::outputDebug("FILL" + std::to_string(count++) + "\n");
      if (offs != TPoint())
        for (int i = 0; i < tileSet->getTileCount(); i++) {
          TTileSet::Tile *t = tileSet->editTile(i);
          t->m_rasterBounds = t->m_rasterBounds + offs;
        }
      TUndoManager::manager()->add(
          new RasterFillUndo(tileSet, params, sl, fid,
                             Preferences::instance()->getFillOnlySavebox()));
    }

    // al posto di updateFrame:

    TXshLevel *xl = app->getCurrentLevel()->getLevel();
    if (!xl) return;

    TXshSimpleLevel *sl = xl->getSimpleLevel();
    sl->getProperties()->setDirtyFlag(true);
    if (recomputeSavebox &&
        Preferences::instance()->isMinimizeSaveboxAfterEditing())
      ToolUtils::updateSaveBox(sl, fid);

    ras->unlock();
  } else if (TVectorImageP vi = TImageP(img)) {
    int oldStyleId;
    QMutexLocker lock(vi->getMutex());
    /*if(params.m_fillType==ALL || params.m_fillType==AREAS)
vi->computeRegion(pos, params.m_styleId);*/

    if ((oldStyleId = vectorFill(vi, params.m_fillType, pos, params.m_styleId,
                                 params.m_emptyOnly)) != -1)
      TUndoManager::manager()->add(new VectorFillUndo(
          params.m_styleId, oldStyleId, params.m_fillType, pos, sl, fid));
  }

  TTool *t = app->getCurrentTool()->getTool();
  if (t) t->notifyImageChanged();
}

//=============================================================================
// SequencePainter
// da spostare in toolutils?
//-----------------------------------------------------------------------------

class SequencePainter {
public:
  virtual void process(TImageP img /*, TImageLocation &imgloc*/, double t,
                       TXshSimpleLevel *sl, const TFrameId &fid) = 0;
  void processSequence(TXshSimpleLevel *sl, TFrameId firstFid,
                       TFrameId lastFid);
  virtual ~SequencePainter() {}
};

//-----------------------------------------------------------------------------

void SequencePainter::processSequence(TXshSimpleLevel *sl, TFrameId firstFid,
                                      TFrameId lastFid) {
  if (!sl) return;

  bool backward = false;
  if (firstFid > lastFid) {
    tswap(firstFid, lastFid);
    backward = true;
  }
  assert(firstFid <= lastFid);
  std::vector<TFrameId> allFids;
  sl->getFids(allFids);

  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFid) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFid) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TImageP img = sl->getFrame(fid, true);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    process(img, backward ? 1 - t : t, sl, fid);
    // Setto il fid come corrente per notificare il cambiamento dell'immagine
    TTool::Application *app = TTool::getApplication();
    if (app) {
      if (app->getCurrentFrame()->isEditingScene())
        app->getCurrentFrame()->setFrame(fid.getNumber());
      else
        app->getCurrentFrame()->setFid(fid);
      TTool *tool = app->getCurrentTool()->getTool();
      if (tool) tool->notifyImageChanged(fid);
    }
  }
  TUndoManager::manager()->endBlock();
}

//=============================================================================
// MultiAreaFiller : SequencePainter
//-----------------------------------------------------------------------------

class MultiAreaFiller final : public SequencePainter {
  TRectD m_firstRect, m_lastRect;
  bool m_unfilledOnly;
  std::wstring m_colorType;
  TVectorImageP m_firstImage, m_lastImage;
  int m_styleIndex;
  bool m_autopaintLines;

public:
  MultiAreaFiller(const TRectD &firstRect, const TRectD &lastRect,
                  bool unfilledOnly, std::wstring colorType, int styleIndex,
                  bool autopaintLines)
      : m_firstRect(firstRect)
      , m_lastRect(lastRect)
      , m_unfilledOnly(unfilledOnly)
      , m_colorType(colorType)
      , m_firstImage()
      , m_lastImage()
      , m_styleIndex(styleIndex)
      , m_autopaintLines(autopaintLines) {}

  ~MultiAreaFiller() {
    if (m_firstImage) {
      m_firstImage->removeStroke(0);
      m_lastImage->removeStroke(0);
    }
  }

  MultiAreaFiller(TStroke *&firstStroke, TStroke *&lastStroke,
                  bool unfilledOnly, std::wstring colorType, int styleIndex,
                  bool autopaintLines)
      : m_firstRect()
      , m_lastRect()
      , m_unfilledOnly(unfilledOnly)
      , m_colorType(colorType)
      , m_styleIndex(styleIndex)
      , m_autopaintLines(autopaintLines) {
    m_firstImage = new TVectorImage();
    m_lastImage  = new TVectorImage();
    m_firstImage->addStroke(firstStroke);
    m_lastImage->addStroke(lastStroke);
  }

  void process(TImageP img, double t, TXshSimpleLevel *sl,
               const TFrameId &fid) override {
    if (!m_firstImage) {
      TPointD p0 = m_firstRect.getP00() * (1 - t) + m_lastRect.getP00() * t;
      TPointD p1 = m_firstRect.getP11() * (1 - t) + m_lastRect.getP11() * t;
      TRectD rect(p0.x, p0.y, p1.x, p1.y);
      fillAreaWithUndo(img, rect, 0, m_unfilledOnly, m_colorType, sl, fid,
                       m_styleIndex, m_autopaintLines);
    } else {
      if (t == 0)
        fillAreaWithUndo(img, TRectD(), m_firstImage->getStroke(0),
                         m_unfilledOnly, m_colorType, sl, fid, m_styleIndex,
                         m_autopaintLines);
      else if (t == 1)
        fillAreaWithUndo(img, TRectD(), m_lastImage->getStroke(0),
                         m_unfilledOnly, m_colorType, sl, fid, m_styleIndex,
                         m_autopaintLines);
      else
      // if(t>1)
      {
        assert(t > 0 && t < 1);
        assert(m_firstImage->getStrokeCount() == 1);
        assert(m_lastImage->getStrokeCount() == 1);

        TVectorImageP vi = TInbetween(m_firstImage, m_lastImage).tween(t);

        assert(vi->getStrokeCount() == 1);

        fillAreaWithUndo(img, TRectD(), vi->getStroke(0) /*, imgloc*/,
                         m_unfilledOnly, m_colorType, sl, fid, m_styleIndex,
                         m_autopaintLines);
      }
    }
  }
};

//=============================================================================
// MultiFiller : SequencePainter
//-----------------------------------------------------------------------------

class MultiFiller final : public SequencePainter {
  TPointD m_firstPoint, m_lastPoint;
  FillParameters m_params;
  bool m_autopaintLines;

public:
  MultiFiller(const TPointD &firstPoint, const TPointD &lastPoint,
              const FillParameters &params, bool autopaintLines)
      : m_firstPoint(firstPoint)
      , m_lastPoint(lastPoint)
      , m_params(params)
      , m_autopaintLines(autopaintLines) {}
  void process(TImageP img, double t, TXshSimpleLevel *sl,
               const TFrameId &fid) override {
    TPointD p = m_firstPoint * (1 - t) + m_lastPoint * t;
    doFill(img, p, m_params, false, sl, fid, m_autopaintLines);
  }
};

//=============================================================================
/*
                                        if(e.isShiftPressed())
            {
                                                m_firstPoint = pos;
                                                m_firstFrameId =
TApplication::instance()->getCurrentFrameId();
                                                }
                                        else
                                          {
                                                m_firstClick = false;
                                                TApplication::instance()->setCurrentFrame(m_veryFirstFrameId);
                                                }
                                        TNotifier::instance()->notify(TLevelChange());
          TNotifier::instance()->notify(TStageChange());
          }
}
*/

//=============================================================================
// AreaFillTool
//-----------------------------------------------------------------------------

void drawPolyline(const std::vector<TPointD> &points) {
  if (points.empty()) return;

  tglDrawCircle(points[0], 2);

  for (UINT i = 0; i < points.size() - 1; i++)
    tglDrawSegment(points[i], points[i + 1]);
}

//-----------------------------------------------------------------------------

class AreaFillTool {
public:
  enum Type { RECT, FREEHAND, POLYLINE };

private:
  bool m_frameRange;
  bool m_onlyUnfilled;
  Type m_type;

  bool m_selecting;
  TRectD m_selectingRect;

  TRectD m_firstRect;
  bool m_firstFrameSelected;
  TXshSimpleLevelP m_level;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  TTool *m_parent;
  std::wstring m_colorType;
  std::pair<int, int> m_currCell;
  StrokeGenerator m_track;
  std::vector<TPointD> m_polyline;
  bool m_isPath;
  bool m_active;
  bool m_enabled;
  double m_thick;
  TPointD m_firstPos;
  TStroke *m_firstStroke;
  TPointD m_mousePosition;
  bool m_onion;
  bool m_isLeftButtonPressed;
  bool m_autopaintLines;

public:
  AreaFillTool(TTool *parent)
      : m_frameRange(false)
      , m_onlyUnfilled(false)
      , m_selecting(false)
      , m_selectingRect(TRectD())
      , m_firstRect(TRectD())
      , m_firstFrameSelected(false)
      , m_level(0)
      , m_parent(parent)
      , m_colorType(AREAS)
      , m_currCell(-1, -1)
      , m_type(RECT)
      , m_isPath(false)
      , m_enabled(false)
      , m_active(false)
      , m_firstStroke(0)
      , m_thick(0.5)
      , m_mousePosition()
      , m_onion(false)
      , m_isLeftButtonPressed(false)
      , m_autopaintLines(true) {}

  void draw() {
    m_thick = m_parent->getPixelSize() / 2.0;

    TPixel color = TPixel32::Red;
    if (m_type == RECT) {
      if (m_frameRange && m_firstFrameSelected)
        drawRect(m_firstRect, color, 0x3F33, true);
      if (m_selecting || (m_frameRange && !m_firstFrameSelected))
        drawRect(m_selectingRect, color, 0xFFFF, true);
    } else if ((m_type == FREEHAND || m_type == POLYLINE) && m_frameRange) {
      tglColor(color);
      if (m_firstStroke) drawStrokeCenterline(*m_firstStroke, 1);
    }

    if (m_type == POLYLINE && !m_polyline.empty()) {
      glPushMatrix();
      tglColor(TPixel::Red);
      tglDrawCircle(m_polyline[0], 2);
      glBegin(GL_LINE_STRIP);
      for (UINT i = 0; i < m_polyline.size(); i++) tglVertex(m_polyline[i]);
      tglVertex(m_mousePosition);
      glEnd();
      glPopMatrix();
    } else if (m_type == FREEHAND && !m_track.isEmpty()) {
      tglColor(TPixel::Red);
      glPushMatrix();
      m_track.drawAllFragments();
      glPopMatrix();
    }
  }

  void resetMulti() {
    m_firstFrameSelected = false;
    m_firstRect.empty();
    m_selectingRect.empty();
    TTool::Application *app = TTool::getApplication();
    TXshLevel *xl           = app->getCurrentLevel()->getLevel();
    m_level                 = xl ? xl->getSimpleLevel() : 0;
    m_firstFrameId = m_veryFirstFrameId = m_parent->getCurrentFid();
    if (m_firstStroke) {
      delete m_firstStroke;
      m_firstStroke = 0;
    }
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &, TImage *img) {
    TVectorImageP vi = TImageP(img);
    TToonzImageP ti  = TToonzImageP(img);

    if (!vi && !ti) {
      m_selecting = false;
      return;
    }

    m_selecting = true;
    if (m_type == RECT) {
      m_selectingRect.x0 = pos.x;
      m_selectingRect.y0 = pos.y;
      m_selectingRect.x1 = pos.x + 1;
      m_selectingRect.y1 = pos.y + 1;
    } else if (m_type == FREEHAND || m_type == POLYLINE) {
      int col  = TTool::getApplication()->getCurrentColumn()->getColumnIndex();
      m_isPath = TTool::getApplication()
                     ->getCurrentObject()
                     ->isSpline();  // getApplication()->isEditingSpline();
      m_enabled = col >= 0 || m_isPath;

      if (!m_enabled) return;
      m_active = true;

      m_track.clear();
      m_firstPos        = pos;
      double pixelSize2 = m_parent->getPixelSize() * m_parent->getPixelSize();
      m_track.add(TThickPoint(pos, m_thick), pixelSize2);
      if (m_type == POLYLINE) {
        if (m_polyline.empty() || m_polyline.back() != pos)
          m_polyline.push_back(pos);
        m_mousePosition = pos;
      } else
        m_track.add(TThickPoint(pos, m_thick), pixelSize2);

      if (m_type == POLYLINE) {
        if (m_polyline.empty() || m_polyline.back() != pos)
          m_polyline.push_back(pos);
      }
    }
    m_isLeftButtonPressed = true;
  }

  /*-- PolyLineFillを閉じる時に呼ばれる --*/
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) {
    TStroke *stroke;

    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    if (m_polyline.size() <= 1) {
      resetMulti();
      return;
    }

    if (m_polyline.back() != pos) m_polyline.push_back(pos);
    if (m_polyline.back() != m_polyline.front())
      m_polyline.push_back(m_polyline.front());
    std::vector<TThickPoint> strokePoints;
    for (UINT i = 0; i < m_polyline.size() - 1; i++) {
      strokePoints.push_back(TThickPoint(m_polyline[i], 1));
      strokePoints.push_back(
          TThickPoint(0.5 * (m_polyline[i] + m_polyline[i + 1]), 1));
    }
    strokePoints.push_back(TThickPoint(m_polyline.back(), 1));
    m_polyline.clear();
    stroke = new TStroke(strokePoints);
    assert(stroke->getPoint(0) == stroke->getPoint(1));

    //    if (m_type==POLYLINE)
    //      m_polyline.push_back(pos);
    // drawPolyline(m_polyline);
    int styleIndex = app->getCurrentLevelStyleIndex();
    if (m_frameRange)  // stroke multi
    {
      if (m_firstFrameSelected) {
        MultiAreaFiller filler(m_firstStroke, stroke, m_onlyUnfilled,
                               m_colorType, styleIndex, m_autopaintLines);
        filler.processSequence(m_level.getPointer(), m_firstFrameId,
                               m_parent->getCurrentFid());
        m_parent->invalidate(m_selectingRect.enlarge(2));
        if (e.isShiftPressed()) {
          m_firstStroke  = stroke;
          m_firstFrameId = m_parent->getCurrentFid();
        } else {
          if (app->getCurrentFrame()->isEditingScene()) {
            app->getCurrentColumn()->setColumnIndex(m_currCell.first);
            app->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            app->getCurrentFrame()->setFid(m_veryFirstFrameId);
          resetMulti();
        }
      } else  // primo frame
      {
        m_firstStroke = stroke;
        // if (app->getCurrentFrame()->isEditingScene())
        m_currCell =
            std::pair<int, int>(app->getCurrentColumn()->getColumnIndex(),
                                app->getCurrentFrame()->getFrame());
      }
    } else {
      if (m_onion) {
        OnionSkinMask osMask = app->getCurrentOnionSkin()->getOnionSkinMask();
        doStrokeAutofill(m_parent->getImage(true), stroke, m_onlyUnfilled,
                         osMask, m_level.getPointer(),
                         m_parent->getCurrentFid());
      } else
        fillAreaWithUndo(m_parent->getImage(true), TRectD(), stroke,
                         m_onlyUnfilled, m_colorType, m_level.getPointer(),
                         m_parent->getCurrentFid(), styleIndex,
                         m_autopaintLines);
      TTool *t = app->getCurrentTool()->getTool();
      if (t) t->notifyImageChanged();
    }
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
    if (m_type == RECT) {
      m_selectingRect.x1 = pos.x;
      m_selectingRect.y1 = pos.y;
      m_parent->invalidate();
    } else if (m_type == FREEHAND) {
      if (!m_enabled || !m_active) return;

#if defined(MACOSX)
// m_parent->m_viewer->enableRedraw(false);
#endif
      double pixelSize2 = m_parent->getPixelSize() * m_parent->getPixelSize();
      m_track.add(TThickPoint(pos, m_thick), pixelSize2);
      m_parent->invalidate();
    }
  }

  void mouseMove(const TPointD &pos, const TMouseEvent &e) {
    if (m_type != POLYLINE || m_polyline.empty()) return;
    if (!m_enabled || !m_active) return;
    m_mousePosition = pos;
    m_parent->invalidate();
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
    if (!m_isLeftButtonPressed) return;
    m_isLeftButtonPressed = false;

    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TXshLevel *xl = app->getCurrentLevel()->getLevel();
    m_level       = xl ? xl->getSimpleLevel() : 0;

    int styleIndex = app->getCurrentLevelStyleIndex();
    m_selecting    = false;
    if (m_type == RECT) {
      if (m_selectingRect.x0 > m_selectingRect.x1)
        tswap(m_selectingRect.x0, m_selectingRect.x1);
      if (m_selectingRect.y0 > m_selectingRect.y1)
        tswap(m_selectingRect.y0, m_selectingRect.y1);

      if (m_frameRange) {
        if (m_firstFrameSelected) {
          MultiAreaFiller filler(m_firstRect, m_selectingRect, m_onlyUnfilled,
                                 m_colorType, styleIndex, m_autopaintLines);
          filler.processSequence(m_level.getPointer(), m_firstFrameId,
                                 m_parent->getCurrentFid());
          m_parent->invalidate(m_selectingRect.enlarge(2));
          if (e.isShiftPressed()) {
            m_firstRect    = m_selectingRect;
            m_firstFrameId = m_parent->getCurrentFid();
          } else {
            if (app->getCurrentFrame()->isEditingScene()) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            resetMulti();
          }
        } else {
          // if (app->getCurrentFrame()->isEditingScene())
          m_currCell =
              std::pair<int, int>(app->getCurrentColumn()->getColumnIndex(),
                                  app->getCurrentFrame()->getFrame());
        }
      } else {
        if (m_onion) {
          OnionSkinMask osMask = app->getCurrentOnionSkin()->getOnionSkinMask();
          doRectAutofill(m_parent->getImage(true), m_selectingRect,
                         m_onlyUnfilled, osMask, m_level.getPointer(),
                         m_parent->getCurrentFid());
        } else
          fillAreaWithUndo(m_parent->getImage(true), m_selectingRect, 0,
                           m_onlyUnfilled, m_colorType, m_level.getPointer(),
                           m_parent->getCurrentFid(), styleIndex,
                           m_autopaintLines);
        m_parent->invalidate();
        m_selectingRect.empty();
        TTool *t = app->getCurrentTool()->getTool();
        if (t) t->notifyImageChanged();
      }
    } else if (m_type == FREEHAND) {
#if defined(MACOSX)
// m_parent->m_viewer->enableRedraw(true);
#endif

      bool isValid = m_enabled && m_active;
      m_enabled = m_active = false;
      if (!isValid || m_track.isEmpty()) return;
      double pixelSize2 = m_parent->getPixelSize() * m_parent->getPixelSize();
      m_track.add(TThickPoint(m_firstPos, m_thick), pixelSize2);
      m_track.filterPoints();
      double error    = (m_isPath ? 20.0 : 30.0 / 11) * sqrt(pixelSize2);
      TStroke *stroke = m_track.makeStroke(error);

      stroke->setStyle(1);
      m_track.clear();

      if (m_frameRange)  // stroke multi
      {
        if (m_firstFrameSelected) {
          MultiAreaFiller filler(m_firstStroke, stroke, m_onlyUnfilled,
                                 m_colorType, styleIndex, m_autopaintLines);
          filler.processSequence(m_level.getPointer(), m_firstFrameId,
                                 m_parent->getCurrentFid());
          m_parent->invalidate(m_selectingRect.enlarge(2));
          if (e.isShiftPressed()) {
            m_firstStroke  = stroke;
            m_firstFrameId = m_parent->getCurrentFid();
          } else {
            if (app->getCurrentFrame()->isEditingScene()) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            resetMulti();
          }
        } else  // primo frame
        {
          m_firstStroke = stroke;
          // if (app->getCurrentFrame()->isEditingScene())
          m_currCell =
              std::pair<int, int>(app->getCurrentColumn()->getColumnIndex(),
                                  app->getCurrentFrame()->getFrame());
        }

      } else  // stroke non multi
      {
        if (!m_parent->getImage(true)) return;
        if (m_onion) {
          OnionSkinMask osMask = app->getCurrentOnionSkin()->getOnionSkinMask();
          doStrokeAutofill(m_parent->getImage(true), stroke, m_onlyUnfilled,
                           osMask, m_level.getPointer(),
                           m_parent->getCurrentFid());
        } else
          fillAreaWithUndo(
              m_parent->getImage(true), TRectD(), stroke /*, imageLocation*/,
              m_onlyUnfilled, m_colorType, m_level.getPointer(),
              m_parent->getCurrentFid(), styleIndex, m_autopaintLines);
        delete stroke;
        TTool *t = app->getCurrentTool()->getTool();
        if (t) t->notifyImageChanged();
        m_parent->invalidate();
      }
    }
  }

  void onImageChanged() {
    if (!m_frameRange) return;
    TTool::Application *app = TTool::getApplication();
    if (!app) return;
    TXshLevel *xshl = app->getCurrentLevel()->getLevel();

    if (!xshl || m_level.getPointer() != xshl ||
        (m_selectingRect.isEmpty() && !m_firstStroke))
      resetMulti();
    else if (m_firstFrameId == m_parent->getCurrentFid())
      m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e
                                     // torno all'immagine iniziale, torno allo
                                     // stato iniziale
    else {                           // cambio stato.
      m_firstFrameSelected = true;
      if (m_type != FREEHAND && m_type != POLYLINE) {
        assert(!m_selectingRect.isEmpty());
        m_firstRect = m_selectingRect;
      }
    }
  }

  /*--Normal以外のTypeが選択された場合に呼ばれる--*/
  bool onPropertyChanged(bool multi, bool onlyUnfilled, bool onion, Type type,
                         std::wstring colorType, bool autopaintLines) {
    m_frameRange     = multi;
    m_onlyUnfilled   = onlyUnfilled;
    m_colorType      = colorType;
    m_type           = type;
    m_onion          = onion;
    m_autopaintLines = autopaintLines;

    if (m_frameRange) resetMulti();

    /*--動作中にプロパティが変わったら、現在の動作を無効にする--*/
    if (m_isLeftButtonPressed) m_isLeftButtonPressed = false;

    if (m_type == POLYLINE && !m_polyline.empty()) m_polyline.clear();

    return true;
  }

  void onActivate() {
    // getApplication()->editImage();

    if (m_frameRange) resetMulti();

    if (TVectorImageP vi = TImageP(m_parent->getImage(false)))
      vi->findRegions();
  }

  void onEnter() {
    // getApplication()->editImage();
  }
};

}  // namespace

//=============================================================================
/*! NormalLineFillTool
        マウスドラッグで直線を延ばし、その直線がまたいだ点をFillLineするツール。
        Raster - Normal - Line Fillツール（FrameRangeなし）のとき使用可能にする
*/
class NormalLineFillTool {
  TTool *m_parent;
  TPointD m_startPosition, m_mousePosition;
  bool m_isEditing;

public:
  NormalLineFillTool(TTool *parent)
      : m_parent(parent), m_isEditing(false), m_mousePosition() {}

  /*-- FillLineツールに戻ってきたときに前の位置情報をリセットする --*/
  void init() {
    m_startPosition = TPointD();
    m_mousePosition = TPointD();
    m_isEditing     = false;
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
    m_startPosition = pos; /*-始点-*/
    m_mousePosition = pos; /*-終点-*/

    m_isEditing = true;
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
    if (!m_isEditing) return;

    m_mousePosition = pos;
    m_parent->invalidate();
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e, TImage *img,
                    FillParameters &params) {
    if (!m_isEditing) return;

    m_mousePosition = pos;

    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TXshLevel *xl       = app->getCurrentLevel()->getLevel();
    TXshSimpleLevel *sl = xl ? xl->getSimpleLevel() : 0;

    TToonzImageP ti = TImageP(m_parent->getImage(true));
    if (!ti) return;
    TRasterCM32P ras = ti->getRaster();
    if (!ras) return;
    int styleId = params.m_styleId;

    /*--- 線分上にある全ての点でdoFillを行う ---*/
    double dx = m_mousePosition.x - m_startPosition.x;
    double dy = m_mousePosition.y - m_startPosition.y;
    if (std::abs(dx) > std::abs(dy)) /*-- 横長の線分の場合 --*/
    {
      double k = dy / dx; /*-- 直線の傾き --*/
      /*--- roundでは負値のときにうまく繋がらない ---*/
      int start = std::min((int)floor(m_startPosition.x + 0.5),
                           (int)floor(m_mousePosition.x + 0.5));
      int end = std::max((int)floor(m_startPosition.x + 0.5),
                         (int)floor(m_mousePosition.x + 0.5));
      double start_x = (m_startPosition.x < m_mousePosition.x)
                           ? m_startPosition.x
                           : m_mousePosition.x;
      double start_y = (m_startPosition.x < m_mousePosition.x)
                           ? m_startPosition.y
                           : m_mousePosition.y;
      for (int x = start; x <= end; x++) {
        double ddx = (double)(x - start);
        TPointD tmpPos(start_x + ddx, ddx * k + start_y);
        TPoint ipos((int)(tmpPos.x + ras->getLx() / 2),
                    (int)(tmpPos.y + ras->getLy() / 2));
        if (!ras->getBounds().contains(ipos)) continue;
        TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
        if (pix.getInk() == styleId || pix.isPurePaint()) continue;
        doFill(img, tmpPos, params, e.isShiftPressed(), sl,
               m_parent->getCurrentFid(), true);
      }
    } else /*-- 縦長の線分の場合 --*/
    {
      double k = dx / dy; /*-- 直線の傾き --*/
      /*--- roundでは負値のときにうまく繋がらない ---*/
      int start = std::min((int)floor(m_startPosition.y + 0.5),
                           (int)floor(m_mousePosition.y + 0.5));
      int end = std::max((int)floor(m_startPosition.y + 0.5),
                         (int)floor(m_mousePosition.y + 0.5));
      double start_x = (m_startPosition.y < m_mousePosition.y)
                           ? m_startPosition.x
                           : m_mousePosition.x;
      double start_y = (m_startPosition.y < m_mousePosition.y)
                           ? m_startPosition.y
                           : m_mousePosition.y;
      for (int y = start; y <= end; y++) {
        double ddy = (double)(y - start);
        TPointD tmpPos(ddy * k + start_x, ddy + start_y);
        TPoint ipos((int)(tmpPos.x + ras->getLx() / 2),
                    (int)(tmpPos.y + ras->getLy() / 2));
        if (!ras->getBounds().contains(ipos)) continue;
        TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
        if (pix.getInk() == styleId || pix.isPurePaint()) continue;
        doFill(img, tmpPos, params, e.isShiftPressed(), sl,
               m_parent->getCurrentFid(), true);
      }
    }
    m_isEditing = false;
    m_parent->invalidate();
  }

  void draw() {
    if (m_isEditing) {
      tglColor(TPixel32::Red);
      glBegin(GL_LINE_STRIP);
      tglVertex(m_startPosition);
      tglVertex(m_mousePosition);
      glEnd();
    }
  }
};
//=============================================================================

//=============================================================================
// Fill Tool
//-----------------------------------------------------------------------------

class FillTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(FillTool)

  bool m_firstTime;
  TPointD m_firstPoint, m_clickPoint;
  bool m_firstClick;
  TXshSimpleLevelP m_level;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_onionStyleId;
  TEnumProperty m_colorType;  // Line, Area
  TEnumProperty m_fillType;   // Rect, Polyline etc.
  TBoolProperty m_onion;
  TBoolProperty m_frameRange;
  TBoolProperty m_selective;
  TDoublePairProperty m_fillDepth;
  TBoolProperty m_segment;

  AreaFillTool *m_rectFill;
  NormalLineFillTool *m_normalLineFillTool;

  TPropertyGroup m_prop;
  std::pair<int, int> m_currCell;
#ifdef _DEBUG
  std::vector<TRect> m_rects;
#endif

  // For the raster fill tool, autopaint lines is optional and can be temporary
  // disabled
  TBoolProperty m_autopaintLines;

public:
  FillTool(int targetType);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  FillParameters getFillParameters() const;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;
  void resetMulti();

  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;

  void draw() override;

  int pick(const TImageP &image, const TPointD &pos);
  int pickOnionColor(const TPointD &pos);

  void onEnter() override;

  void onActivate() override;
  void onDeactivate() override;

  int getCursorId() const override;

  int getColorClass() const { return 2; }
};

//-----------------------------------------------------------------------------

FillTool::FillTool(int targetType)
    : TTool("T_Fill")
    , m_frameRange("Frame Range", false)  // W_ToolOptions_FrameRange
    , m_fillType("Type:")
    , m_selective("Selective", false)
    , m_colorType("Mode:")
    , m_onion("Onion Skin", false)
    , m_fillDepth("Fill Depth", 0, 15, 0, 15)
    , m_segment("Segment", false)
    , m_onionStyleId(0)
    , m_currCell(-1, -1)
    , m_firstTime(true)
    , m_autopaintLines("Autopaint Lines", true) {
  m_rectFill           = new AreaFillTool(this);
  m_normalLineFillTool = new NormalLineFillTool(this);

  bind(targetType);
  m_prop.bind(m_fillType);
  m_fillType.addValue(NORMALFILL);
  m_fillType.addValue(RECTFILL);
  m_fillType.addValue(FREEHANDFILL);
  m_fillType.addValue(POLYLINEFILL);

  m_prop.bind(m_colorType);
  m_colorType.addValue(LINES);
  m_colorType.addValue(AREAS);
  m_colorType.addValue(ALL);

  m_prop.bind(m_selective);
  if (targetType == TTool::ToonzImage) {
    m_prop.bind(m_fillDepth);
    m_prop.bind(m_segment);
  }
  m_prop.bind(m_onion);
  m_prop.bind(m_frameRange);

  if (targetType == TTool::ToonzImage) m_prop.bind(m_autopaintLines);

  m_selective.setId("Selective");
  m_onion.setId("OnionSkin");
  m_frameRange.setId("FrameRange");
  m_segment.setId("SegmentInk");
  m_fillType.setId("Type");
  m_colorType.setId("Mode");
  m_autopaintLines.setId("AutopaintLines");
}
//-----------------------------------------------------------------------------

int FillTool::getCursorId() const {
  int ret;
  if (m_colorType.getValue() == LINES)
    ret = ToolCursor::FillCursorL;
  else {
    ret                                      = ToolCursor::FillCursor;
    if (m_colorType.getValue() == AREAS) ret = ret | ToolCursor::Ex_Area;
    if (!m_autopaintLines.getValue())
      ret = ret | ToolCursor::Ex_Fill_NoAutopaint;
  }
  if (m_fillType.getValue() == FREEHANDFILL)
    ret = ret | ToolCursor::Ex_FreeHand;
  else if (m_fillType.getValue() == POLYLINEFILL)
    ret = ret | ToolCursor::Ex_PolyLine;
  else if (m_fillType.getValue() == RECTFILL)
    ret = ret | ToolCursor::Ex_Rectangle;

  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    ret = ret | ToolCursor::Ex_Negate;
  return ret;
}

//-----------------------------------------------------------------------------

void FillTool::updateTranslation() {
  m_frameRange.setQStringName(tr("Frame Range"));
  m_fillType.setQStringName(tr("Type:"));
  m_selective.setQStringName(tr("Selective"));
  m_colorType.setQStringName(tr("Mode:"));
  m_onion.setQStringName(tr("Onion Skin"));
  m_fillDepth.setQStringName(tr("Fill Depth"));
  m_segment.setQStringName(tr("Segment"));
  m_autopaintLines.setQStringName(tr("Autopaint Lines"));
}

//-----------------------------------------------------------------------------

FillParameters FillTool::getFillParameters() const {
  FillParameters params;
  int styleId      = TTool::getApplication()->getCurrentLevelStyleIndex();
  params.m_styleId = styleId;
  /*---紛らわしいことに、colorTypeをfillTypeに名前を変えて保存している。間違いではない。---*/
  params.m_fillType     = m_colorType.getValue();
  params.m_emptyOnly    = m_selective.getValue();
  params.m_segment      = m_segment.getValue();
  params.m_minFillDepth = (int)m_fillDepth.getValue().first;
  params.m_maxFillDepth = (int)m_fillDepth.getValue().second;
  return params;
}

//-----------------------------------------------------------------------------

void FillTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  m_clickPoint = pos;
  if (m_fillType.getValue() != NORMALFILL) {
    m_rectFill->leftButtonDown(pos, e, getImage(true));
    return;
  }
  /*--以下、NormalFillの場合--*/
  FillParameters params = getFillParameters();

  if (m_onion.getValue()) {
    m_onionStyleId = pickOnionColor(pos);
    if (m_onionStyleId > 0) app->setCurrentLevelStyleIndex(m_onionStyleId);
  } else if (m_frameRange.getValue()) {
    if (!m_firstClick) {
      // PRIMO CLICK
      // if (app->getCurrentFrame()->isEditingScene())
      m_currCell = std::pair<int, int>(getColumnIndex(), getFrame());

      m_firstClick   = true;
      m_firstPoint   = pos;
      m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
      // gmt. NON BISOGNA DISEGNARE DENTRO LE CALLBACKS!!!!
      // drawCross(m_firstPoint, 6);
      invalidate();
    } else {
      // SECONDO CLICK
      TFrameId fid = getCurrentFid();
      MultiFiller filler(m_firstPoint, pos, params,
                         m_autopaintLines.getValue());
      filler.processSequence(m_level.getPointer(), m_firstFrameId, fid);
      if (e.isShiftPressed()) {
        m_firstPoint   = pos;
        m_firstFrameId = getCurrentFid();
      } else {
        m_firstClick = false;
        if (app->getCurrentFrame()->isEditingScene()) {
          app->getCurrentColumn()->setColumnIndex(m_currCell.first);
          app->getCurrentFrame()->setFrame(m_currCell.second);
        } else
          app->getCurrentFrame()->setFid(m_veryFirstFrameId);
      }
      TTool *t = app->getCurrentTool()->getTool();
      if (t) t->notifyImageChanged();
    }

  } else {
    if (params.m_fillType == LINES && m_targetType == TTool::ToonzImage)
      m_normalLineFillTool->leftButtonDown(pos, e);
    else {
      TXshLevel *xl = app->getCurrentLevel()->getLevel();
      m_level       = xl ? xl->getSimpleLevel() : 0;
      doFill(getImage(true), pos, params, e.isShiftPressed(),
             m_level.getPointer(), getCurrentFid(),
             m_autopaintLines.getValue());
      invalidate();
    }
  }
}

//-----------------------------------------------------------------------------

void FillTool::leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) {
  if (m_fillType.getValue() != NORMALFILL) {
    m_rectFill->leftButtonDoubleClick(pos, e);
    return;
  }
}

//-----------------------------------------------------------------------------

void FillTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (m_fillType.getValue() != NORMALFILL && !m_onion.getValue() ||
      (m_colorType.getValue() == AREAS && m_onion.getValue()))
    m_rectFill->leftButtonDrag(pos, e);
  else if (!m_onion.getValue() && !m_frameRange.getValue()) {
    FillParameters params = getFillParameters();
    if (params.m_fillType == LINES && m_targetType == TTool::ToonzImage) {
      m_normalLineFillTool->leftButtonDrag(pos, e);
      return;
    }
    if (m_clickPoint == pos) return;
    TImageP img = getImage(true);
    int styleId = params.m_styleId;
    if (TVectorImageP vi = img) {
      TRegion *r = vi->getRegion(pos);
      if (r && r->getStyle() == styleId) return;
    } else if (TToonzImageP ti = img) {
      TRasterCM32P ras = ti->getRaster();
      if (!ras) return;
      TPointD center = ras->getCenterD();
      TPoint ipos    = convert(pos + center);
      if (!ras->getBounds().contains(ipos)) return;
      TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
      if (pix.getPaint() == styleId) {
        invalidate();
        return;
      }
      TSystem::outputDebug("ok. pix=" + std::to_string(pix.getTone()) + "," +
                           std::to_string(pix.getPaint()) + "\n");
    } else
      return;
    doFill(img, pos, params, e.isShiftPressed(), m_level.getPointer(),
           getCurrentFid(), m_autopaintLines.getValue());
    invalidate();
  }
}

//-----------------------------------------------------------------------------

void FillTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (m_onion.getValue()) {
    if (m_fillType.getValue() != NORMALFILL && m_colorType.getValue() == AREAS)
      m_rectFill->leftButtonUp(pos, e);
    else if (m_onionStyleId > 0) {
      FillParameters tmp = getFillParameters();
      doFill(getImage(true), pos, tmp, e.isShiftPressed(), m_level.getPointer(),
             getCurrentFid(), m_autopaintLines.getValue());
      invalidate();
    }
  } else if (m_fillType.getValue() != NORMALFILL) {
    m_rectFill->leftButtonUp(pos, e);
    return;
  }

  if (!m_frameRange.getValue()) {
    TFrameId fid = getCurrentFid();
    // notifyImageChanged();
    if (getFillParameters().m_fillType == LINES &&
        m_targetType == TTool::ToonzImage) {
      FillParameters params = getFillParameters();
      m_normalLineFillTool->leftButtonUp(pos, e, getImage(true), params);
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void FillTool::resetMulti() {
  m_firstClick   = false;
  m_firstFrameId = -1;
  m_firstPoint   = TPointD();
  TXshLevel *xl  = TTool::getApplication()->getCurrentLevel()->getLevel();
  m_level        = xl ? xl->getSimpleLevel() : 0;
}

//-----------------------------------------------------------------------------

bool FillTool::onPropertyChanged(std::string propertyName) {
  /*--- m_rectFill->onPropertyChangedを呼ぶかどうかのフラグ
                  fillType, frameRange, selective,
     colorTypeが変わったときに呼ぶ---*/
  bool rectPropChangedflag = false;

  // Areas, Lines etc.
  if (propertyName == m_colorType.getName()) {
    FillColorType       = ::to_string(m_colorType.getValue());
    rectPropChangedflag = true;

    /*--- ColorModelのCursor更新のためにSIGNALを出す ---*/
    TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    /*--- FillLineツールに戻ってきたときに前回の位置情報をリセットする ---*/
    if (FillColorType.getValue() == "Lines") m_normalLineFillTool->init();
  }
  // Rect, Polyline etc.
  else if (propertyName == m_fillType.getName()) {
    if (m_fillType.getValue() != NORMALFILL) {
      FillOnion   = (int)(m_onion.getValue());
      FillSegment = (int)(m_segment.getValue());
    }
    FillType            = ::to_string(m_fillType.getValue());
    rectPropChangedflag = true;
  }
  // Onion Skin
  else if (propertyName == m_onion.getName()) {
    if (m_onion.getValue()) FillType = ::to_string(m_fillType.getValue());
    FillOnion                        = (int)(m_onion.getValue());
  }
  // Frame Range
  else if (propertyName == m_frameRange.getName()) {
    FillRange = (int)(m_frameRange.getValue());
    resetMulti();
    rectPropChangedflag = true;
  }
  // Selective
  else if (propertyName == m_selective.getName()) {
    rectPropChangedflag = true;
  }
  // Fill Depth
  else if (propertyName == m_fillDepth.getName()) {
    MinFillDepth = (int)m_fillDepth.getValue().first;
    MaxFillDepth = (int)m_fillDepth.getValue().second;
  }
  // Segment
  else if (propertyName == m_segment.getName()) {
    if (m_segment.getValue()) FillType = ::to_string(m_fillType.getValue());
    FillSegment                        = (int)(m_segment.getValue());
  }
  // Autopaint
  else if (propertyName == m_autopaintLines.getName()) {
    rectPropChangedflag = true;
  }

  /*--- fillType, frameRange, selective, colorTypeが変わったとき ---*/
  if (rectPropChangedflag && m_fillType.getValue() != NORMALFILL) {
    AreaFillTool::Type type;
    if (m_fillType.getValue() == RECTFILL)
      type = AreaFillTool::RECT;
    else if (m_fillType.getValue() == FREEHANDFILL)
      type = AreaFillTool::FREEHAND;
    else if (m_fillType.getValue() == POLYLINEFILL)
      type = AreaFillTool::POLYLINE;
    else
      assert(false);

    m_rectFill->onPropertyChanged(
        m_frameRange.getValue(), m_selective.getValue(), m_onion.getValue(),
        type, m_colorType.getValue(), m_autopaintLines.getValue());
  }

  return true;
}

//-----------------------------------------------------------------------------

void FillTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  if (m_fillType.getValue() != NORMALFILL) m_rectFill->mouseMove(pos, e);
}

//-----------------------------------------------------------------------------

void FillTool::onImageChanged() {
  if (m_fillType.getValue() != NORMALFILL) {
    m_rectFill->onImageChanged();
    return;
  }
  if (!m_level) resetMulti();
}

//-----------------------------------------------------------------------------

void FillTool::draw() {
  if (Preferences::instance()->getFillOnlySavebox()) {
    TToonzImageP ti = (TToonzImageP)getImage(false);
    if (ti) {
      TRectD bbox =
          ToonzImageUtils::convertRasterToWorld(convert(ti->getBBox()), ti);
      drawRect(bbox.enlarge(0.5) * ti->getSubsampling(), TPixel32::Black,
               0x5555, true);
    }
  }
  if (m_fillType.getValue() != NORMALFILL) {
    m_rectFill->draw();
    return;
  }

  if (m_frameRange.getValue() && m_firstClick) {
    tglColor(TPixel::Red);
    drawCross(m_firstPoint, 6);
  } else if (!m_frameRange.getValue() &&
             getFillParameters().m_fillType == LINES &&
             m_targetType == TTool::ToonzImage)
    m_normalLineFillTool->draw();
}

//-----------------------------------------------------------------------------

int FillTool::pick(const TImageP &image, const TPointD &pos) {
  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  if (!ti && !vi) return 0;

  StylePicker picker(image);
  double pixelSize2 = getPixelSize() * getPixelSize();
  return picker.pickStyleId(pos, pixelSize2);
}

//-----------------------------------------------------------------------------

int FillTool::pickOnionColor(const TPointD &pos) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return 0;
  bool filmStripEditing = !app->getCurrentObject()->isSpline();
  OnionSkinMask osMask  = app->getCurrentOnionSkin()->getOnionSkinMask();

  TFrameId fid = getCurrentFid();

  TXshSimpleLevel *sl = m_level.getPointer();
  if (!sl) return 0;

  std::vector<int> rows;
  osMask.getAll(sl->guessIndex(fid), rows);

  int i, j;

  for (i = 0; i < (int)rows.size(); i++)
    if (sl->index2fid(rows[i]) > fid) break;

  int onionStyleId = 0;
  for (j = i - 1; j >= 0; j--) {
    TFrameId onionFid = sl->index2fid(rows[j]);
    if (onionFid != fid &&
        ((onionStyleId =
              pick(m_level->getFrame(onionFid, ImageManager::none, 1), pos)) >
         0))  // subsabling must be 1, otherwise onionfill does  not work
      break;
  }
  if (onionStyleId == 0)
    for (j = i; j < (int)rows.size(); j++) {
      TFrameId onionFid = sl->index2fid(rows[j]);
      if (onionFid != fid &&
          ((onionStyleId =
                pick(m_level->getFrame(onionFid, ImageManager::none, 1), pos)) >
           0))  // subsabling must be 1, otherwise onionfill does  not work
        break;
    }
  return onionStyleId;
}

//-----------------------------------------------------------------------------

void FillTool::onEnter() {
  // resetMulti();

  // getApplication()->editImage();
}

//-----------------------------------------------------------------------------

void FillTool::onActivate() {
  // OnionSkinMask osMask = getApplication()->getOnionSkinMask(false);
  /*
  for (int i=0; i<osMask.getMosCount(); i++)
  boh = osMask.getMos(i);
  for (i=0; i<osMask.getFosCount(); i++)
  boh = osMask.getFos(i);
  */
  if (m_firstTime) {
    m_fillDepth.setValue(
        TDoublePairProperty::Value(MinFillDepth, MaxFillDepth));
    m_fillType.setValue(::to_wstring(FillType.getValue()));
    m_colorType.setValue(::to_wstring(FillColorType.getValue()));
    //		m_onlyEmpty.setValue(FillSelective ? 1 :0);
    m_onion.setValue(FillOnion ? 1 : 0);
    m_segment.setValue(FillSegment ? 1 : 0);
    m_frameRange.setValue(FillRange ? 1 : 0);
    m_firstTime = false;

    if (m_fillType.getValue() != NORMALFILL) {
      AreaFillTool::Type type;
      if (m_fillType.getValue() == RECTFILL)
        type = AreaFillTool::RECT;
      else if (m_fillType.getValue() == FREEHANDFILL)
        type = AreaFillTool::FREEHAND;
      else if (m_fillType.getValue() == POLYLINEFILL)
        type = AreaFillTool::POLYLINE;
      else
        assert(false);

      m_rectFill->onPropertyChanged(
          m_frameRange.getValue(), m_selective.getValue(), m_onion.getValue(),
          type, m_colorType.getValue(), m_autopaintLines.getValue());
    }
  }

  if (m_fillType.getValue() != NORMALFILL) {
    m_rectFill->onActivate();
    return;
  }

  if (FillColorType.getValue() == "Lines") m_normalLineFillTool->init();

  resetMulti();

  //  getApplication()->editImage();
  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return;
  vi->findRegions();
}

//-----------------------------------------------------------------------------

void FillTool::onDeactivate() {}

//-----------------------------------------------------------------------------

FillTool FillVectorTool(TTool::VectorImage);
FillTool FiilRasterTool(TTool::ToonzImage);
