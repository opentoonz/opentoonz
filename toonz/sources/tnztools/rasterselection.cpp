

#include "tools/rasterselection.h"
#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "tpaletteutil.h"
#include "trop.h"
#include "drawutil.h"
#include "tconvert.h"
#include "timagecache.h"
#include "tpixelutils.h"
#include "toonzqt/rasterimagedata.h"
#include "toonzqt/strokesdata.h"
#include "toonzqt/selectioncommandids.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvdialog.h"
#include "toonz/stage.h"
#include "toonz/toonzimageutils.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/trasterimageutils.h"

#include <QApplication>
#include <QClipboard>

#include "timage_io.h"
#include "tropcm.h"

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

TRasterP getRaster(const TImageP image) {
  if (TToonzImageP ti = (TToonzImageP)(image)) return ti->getRaster();
  if (TRasterImageP ri = (TRasterImageP)(image)) return ri->getRaster();
  return (TRasterP)(0);
}

//-----------------------------------------------------------------------------

TRect convertWorldToRaster(const TRectD area, const TRasterP ras) {
  if (area.isEmpty()) return TRect();
  if (!ras)
    return TRect(tfloor(area.x0), tfloor(area.y0), tfloor(area.x1) - 1,
                 tfloor(area.y1) - 1);
  TRectD rect(area + ras->getCenterD());
  return TRect(tfloor(rect.x0), tfloor(rect.y0), tceil(rect.x1) - 1,
               tceil(rect.y1) - 1);
}

//-----------------------------------------------------------------------------

TRect convertWorldToRaster(const TRectD area, const TImageP image) {
  TRasterImageP ri(image);
  TToonzImageP ti(image);

  // Watch out! TToonzImage::getRaster() returns a TRasterCM32P, while
  // TRasterImage::getRaster() returns a TRasterP!
  TRasterP ras = (ri) ? ri->getRaster() : (TRasterP)ti->getRaster();
  return convertWorldToRaster(area, ras);
}

//-----------------------------------------------------------------------------

TRectD convertRasterToWorld(const TRect area, const TImageP image) {
  TToonzImageP ti(image);
  if (ti) return ToonzImageUtils::convertRasterToWorld(area, image);
  return TRasterImageUtils::convertRasterToWorld(area, image);
}

//-----------------------------------------------------------------------------

TRectD intersection(const TRectD &area, const TImageP image) {
  TToonzImageP ti(image);
  if (ti)
    return area * ToonzImageUtils::convertRasterToWorld(
                      ti->getRaster()->getBounds(), image);

  TRasterImageP ri(image);
  if (ri)
    return area * TRasterImageUtils::convertRasterToWorld(
                      ri->getRaster()->getBounds(), ri);

  return area;
}

//-----------------------------------------------------------------------------

// The stroke is in raster coordinates
template <class PIXEL1, class PIXEL2>
TRasterPT<PIXEL1> getImageFromStroke(TRasterPT<PIXEL2> ras,
                                     const TStroke &stroke) {
  TRectD regionsBoxD = stroke.getBBox();
  // E' volutamente allargato di un pixel!
  TRect regionsBox(tfloor(regionsBoxD.x0), tfloor(regionsBoxD.y0),
                   tceil(regionsBoxD.x1), tceil(regionsBoxD.y1));
  regionsBox *= ras->getBounds();
  if (regionsBox.isEmpty()) return (TRasterPT<PIXEL1>)0;
  TRasterPT<PIXEL1> buffer(regionsBox.getSize());
  buffer->clear();

  // Compute regions created by the std::vector
  TVectorImage app;
  app.addStroke(new TStroke(stroke));
  app.findRegions();
  int reg, j, k, y;
  ras->lock();
  for (reg = 0; reg < (int)app.getRegionCount(); reg++) {
    // For each region, pixels inside the region are copied in buffer!
    TRectD bBoxD = stroke.getBBox();
    TRect bBox(tfloor(bBoxD.x0), tfloor(bBoxD.y0), tceil(bBoxD.x1) - 1,
               tceil(bBoxD.y1) - 1);
    bBox *= ras->getBounds();
    for (y = bBox.y0; y <= bBox.y1; y++) {
      PIXEL2 *selectedLine = ras->pixels(y);
      int startY           = y - regionsBox.y0;
      PIXEL1 *bufferLine   = buffer->pixels(startY >= 0 ? startY : 0);
      std::vector<double> intersections;
      app.getRegion(reg)->computeScanlineIntersections(y, intersections);
      if (intersections.empty())
        app.getRegion(reg)->computeScanlineIntersections(y + 0.9,
                                                         intersections);
      for (j = 0; j < (int)intersections.size(); j += 2) {
        if (intersections[j] == intersections[j + 1]) continue;
        int from = std::max(tfloor(intersections[j]), bBox.x0);
        int to   = std::min(tceil(intersections[j + 1]), bBox.x1);
        for (k = from; k <= to; k++) {
          TRasterCM32P bufferCM(buffer);
          TRaster32P buffer32(buffer);
          TRasterCM32P rasCM(ras);
          TRaster32P ras32(ras);
          TRasterGR8P rasGR8(ras);
          if (bufferCM && rasCM) {
            TPixelCM32 *bottomPix =
                (TPixelCM32 *)bufferLine + k - regionsBox.x0;
            TPixelCM32 *topPix = (TPixelCM32 *)selectedLine + k;
            *bottomPix         = *topPix;
          } else if (buffer32 && ras32) {
            TPixel32 *bottomPix = (TPixel32 *)bufferLine + k - regionsBox.x0;
            TPixel32 *topPix    = (TPixel32 *)selectedLine + k;
            *bottomPix          = *topPix;
          } else if (buffer32 && rasGR8) {
            TPixel32 *bottomPix = (TPixel32 *)bufferLine + k - regionsBox.x0;
            TPixelGR8 *topPix   = (TPixelGR8 *)selectedLine + k;
            *bottomPix =
                TPixel32(topPix->value, topPix->value, topPix->value, 255);
          } else
            assert(0);
        }
      }
    }
  }
  ras->unlock();
  return buffer;
}

//-----------------------------------------------------------------------------

template <class PIXEL1, class PIXEL2>
TRasterPT<PIXEL1> getImageFromSelection(TRasterPT<PIXEL2> &ras,
                                        RasterSelection &selection) {
  if (selection.isEmpty()) return (TRasterPT<PIXEL1>)0;
  TRectD wSelectionBound = selection.getSelectionBbox();
  TRect rSelectionBound  = convertWorldToRaster(wSelectionBound, ras);
  rSelectionBound *= ras->getBounds();
  TRasterPT<PIXEL1> selectedRaster(rSelectionBound.getSize());
  selectedRaster->clear();
  std::vector<TStroke> strokes = selection.getStrokes();
  TPoint startPosition         = rSelectionBound.getP00();
  unsigned int i;
  for (i = 0; i < strokes.size(); i++) {
    TStroke stroke = strokes[i];
    stroke.transform(TTranslation(ras->getCenterD()));
    TRasterPT<PIXEL1> app = getImageFromStroke<PIXEL1, PIXEL2>(ras, stroke);
    if (!app) continue;
    TRectD strokeRectD = stroke.getBBox();
    TRect strokeRect(tfloor(strokeRectD.x0), tfloor(strokeRectD.y0),
                     tceil(strokeRectD.x1) - 1, tceil(strokeRectD.y1) - 1);
    TPoint offset((strokeRect * rSelectionBound).getP00() -
                  rSelectionBound.getP00());
    TPoint startP = rSelectionBound.getP00() + offset;
    startPosition = TPoint(std::min(startPosition.x, startP.x),
                           std::min(startPosition.y, startP.y));
    TRop::over(selectedRaster, app, offset);
  }

  selection.setStartPosition(startPosition);
  return selectedRaster;
}

//-----------------------------------------------------------------------------

TRasterP getImageFromSelection(const TImageP &image,
                               RasterSelection &selection) {
  if (TToonzImageP toonzImage = (TToonzImageP)image) {
    TRasterPT<TPixelCM32> ras = toonzImage->getRaster();
    return getImageFromSelection<TPixelCM32, TPixelCM32>(ras, selection);
  }
  if (TRasterImageP rasterImage = (TRasterImageP)image) {
    TRasterP ras = rasterImage->getRaster();
    if (TRaster32P ras32 = (TRaster32P)ras)
      return getImageFromSelection<TPixel32, TPixel32>(ras32, selection);
    if (TRasterGR8P rasGR8 = (TRasterGR8P)ras)
      return getImageFromSelection<TPixel32, TPixelGR8>(rasGR8, selection);
  }
  return (TRasterP)0;
}

//-----------------------------------------------------------------------------

template <typename PIXEL>
void deleteSelectionWithoutUndo(TRasterPT<PIXEL> &ras,
                                const std::vector<TStroke> &strokes,
                                PIXEL emptyValue) {
  if (!ras) return;
  unsigned int i;
  for (i = 0; i < strokes.size(); i++) {
    TStroke s = strokes[i];
    s.transform(TTranslation(ras->getCenterD()));
    TRectD strokeRectD = s.getBBox();
    // E' volutamente allargato di un pixel!
    TRect strokeRect(tfloor(strokeRectD.x0), tfloor(strokeRectD.y0),
                     tceil(strokeRectD.x1), tceil(strokeRectD.y1));
    if (!strokeRect.overlaps(ras->getBounds())) continue;

    // Compute regions created by the std::vector
    TVectorImage app;
    app.addStroke(new TStroke(s));
    app.findRegions();
    int reg, j, k, y;
    ras->lock();
    TRect rasRect(ras->getBounds());
    for (reg = 0; reg < (int)app.getRegionCount(); reg++) {
      // For each region, pixels inside the region are erased!
      TRectD bBoxD = app.getRegion(reg)->getBBox();
      TRect bBox(tfloor(bBoxD.x0), tfloor(bBoxD.y0), tceil(bBoxD.x1) - 1,
                 tceil(bBoxD.y1) - 1);
      bBox *= rasRect;
      for (y = bBox.y0; y <= bBox.y1; y++) {
        PIXEL *selectedLine = ras->pixels(y);
        int startY          = y - strokeRect.y0;
        std::vector<double> intersections;
        app.getRegion(reg)->computeScanlineIntersections(y, intersections);
        if (intersections.empty())
          app.getRegion(reg)->computeScanlineIntersections(y + 0.9,
                                                           intersections);
        for (j = 0; j < (int)intersections.size(); j += 2) {
          if (intersections[j] == intersections[j + 1]) continue;
          int from = std::max(tfloor(intersections[j]), bBox.x0);
          int to   = std::min(tceil(intersections[j + 1]), bBox.x1);
          for (k = from; k <= to; k++) *(selectedLine + k) = emptyValue;
        }
      }
    }
    ras->unlock();
  }
}

//-----------------------------------------------------------------------------

void deleteSelectionWithoutUndo(const TImageP &image,
                                const std::vector<TStroke> &strokes) {
  if (TToonzImageP toonzImage = (TToonzImageP)image) {
    TRasterPT<TPixelCM32> ras = toonzImage->getRaster();
    deleteSelectionWithoutUndo<TPixelCM32>(ras, strokes, TPixelCM32());
  }
  if (TRasterImageP rasterImage = (TRasterImageP)image) {
    TRasterP ras = rasterImage->getRaster();
    if (TRaster32P ras32 = (TRaster32P)ras)
      deleteSelectionWithoutUndo<TPixel32>(ras32, strokes,
                                           TPixel32::Transparent);
    if (TRasterGR8P rasGR8 = (TRasterGR8P)ras)
      deleteSelectionWithoutUndo<TPixelGR8>(rasGR8, strokes, TPixelGR8::White);
  }
}

//-----------------------------------------------------------------------------

void pasteFloatingSelectionWithoutUndo(const TImageP &image,
                                       const TRasterP &floatingSelection,
                                       const TAffine &transformation,
                                       const TRectD &wSelectionBound,
                                       bool noAntialiasing) {
  TRasterImageP ri = (TRasterImageP)image;
  TToonzImageP ti  = (TToonzImageP)image;

  TRasterP targetRaster = (ri) ? ri->getRaster() : (TRasterP)ti->getRaster();
  if (!targetRaster || !floatingSelection) return;

  TRect rSelectionBound = convertWorldToRaster(wSelectionBound, targetRaster);
  TRop::over(targetRaster, floatingSelection, rSelectionBound.getP00(),
             transformation,
             noAntialiasing ? TRop::ClosestPixel : TRop::Triangle);
}

//=============================================================================
// UndoDeleteSelection
//-----------------------------------------------------------------------------

class UndoDeleteSelection final : public TUndo {
  static int m_id;
  TXshSimpleLevelP m_level;
  TFrameId m_frameId;
  std::string m_erasedImageId;
  TPoint m_erasePoint;
  std::vector<TStroke> m_strokes;
  TTool *m_tool;

public:
  UndoDeleteSelection(RasterSelection *selection, TXshSimpleLevel *level)
      : TUndo()
      , m_level(level)
      , m_frameId(selection->getFrameId())
      , m_strokes(selection->getOriginalStrokes()) {
    TImageP image   = m_level->getFrame(m_frameId, true);
    m_erasedImageId = "UndoDeleteSelection" + std::to_string(m_id++);
    TRasterP ras    = getRaster(image);
    TRasterP erasedRas;
    if (!selection->isFloating())
      erasedRas = TRasterP(getImageFromSelection(image, *selection));
    else
      erasedRas = TRasterP(selection->getOriginalFloatingSelection());
    TImageP erasedImage;
    if (TRasterCM32P toonzRas = (TRasterCM32P)(erasedRas))
      erasedImage = TToonzImageP(toonzRas, toonzRas->getBounds());
    else if (TRaster32P fullColorRas = (TRaster32P)(erasedRas))
      erasedImage = TRasterImageP(fullColorRas);
    TImageCache::instance()->add(m_erasedImageId, erasedImage, false);
    m_erasePoint = selection->getStartPosition();
    m_tool       = TTool::getApplication()->getCurrentTool()->getTool();
  }

  ~UndoDeleteSelection() {
    if (TImageCache::instance()->isCached(m_erasedImageId))
      TImageCache::instance()->remove(m_erasedImageId);
  }

  void undo() const override {
    TImageP image = m_level->getFrame(m_frameId, true);
    if (!image) return;
    TRasterP ras = getRaster(image);
    if (!ras) return;
    TImageP erasedImage = TImageCache::instance()->get(m_erasedImageId, false);
    if (!erasedImage) return;
    TRasterP erasedRaster = getRaster(erasedImage);
    TRop::over(ras, erasedRaster, m_erasePoint);

    ToolUtils::updateSaveBox(m_level, m_frameId);

    if (!m_tool) return;
    m_tool->notifyImageChanged(m_frameId);
    m_tool->invalidate();
  }

  void redo() const override {
    TImageP image       = m_level->getFrame(m_frameId, true);
    TImageP erasedImage = TImageCache::instance()->get(m_erasedImageId, false);
    if (!erasedImage) return;
    deleteSelectionWithoutUndo(image, m_strokes);

    ToolUtils::updateSaveBox(m_level, m_frameId);
    if (!m_tool) return;
    m_tool->notifyImageChanged(m_frameId);
    m_tool->invalidate();
  }

  int getSize() const override { return sizeof(*this); }
};

int UndoDeleteSelection::m_id = 0;

//=============================================================================
// UndoPasteSelection
//-----------------------------------------------------------------------------

class UndoPasteSelection final : public TUndo {
  RasterSelection *m_currentSelection, m_newSelection;

public:
  UndoPasteSelection(RasterSelection *currentSelection)
      : TUndo()
      , m_currentSelection(currentSelection)
      , m_newSelection(*currentSelection) {}

  ~UndoPasteSelection() {}

  void undo() const override {
    m_currentSelection->setFloatingSeletion(TRasterP());
    m_currentSelection->selectNone();
    m_currentSelection->notify();
  }

  void redo() const override {
    *m_currentSelection = m_newSelection;
    m_currentSelection->notify();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Paste"); }
};

//=============================================================================
// UndoPasteFloatingSelection
//-----------------------------------------------------------------------------

class UndoPasteFloatingSelection final : public TUndo {
  static int m_id;

  TXshCell m_imageCell;  //!< Level/frame pair to the pasted-to image
                         //!< (seemingly cached as m_imageId)
  TPaletteP m_oldPalette, m_newPalette;
  std::string m_imageId, m_floatingImageId, m_undoImageId, m_oldFloatingImageId;
  std::vector<TStroke> m_strokes;
  TRectD m_selectionRect;
  TAffine m_transformation;
  TPoint m_startPos;
  bool m_isPastedSelection;
  bool m_noAntialiasing;

  TTool *m_tool;
  TFrameId m_frameId;

public:
  UndoPasteFloatingSelection(RasterSelection *currentSelection,
                             TPalette *oldPalette, bool noAntialiasing)
      : TUndo()
      , m_imageCell(currentSelection->getCurrentImageCell())
      , m_oldPalette(oldPalette ? oldPalette->clone() : 0)
      , m_strokes(currentSelection->getOriginalStrokes())
      , m_selectionRect(currentSelection->getSelectionBbox())
      , m_transformation(currentSelection->getTransformation())
      , m_isPastedSelection(currentSelection->isPastedSelection())
      , m_noAntialiasing(noAntialiasing)
      , m_undoImageId("")
      , m_frameId(currentSelection->getFrameId()) {
    TImageP image(currentSelection->getCurrentImage());
    if (!image) return;

    m_imageId = "UndoPasteImage_" + std::to_string(m_id);
    TImageCache::instance()->add(m_imageId, image, false);

    m_floatingImageId =
        "UndoPasteFloatingSelection_floating_" + std::to_string(m_id);
    TRasterP floatingRas = currentSelection->getFloatingSelection();
    TImageP floatingImage;
    if (TRasterCM32P toonzRas = (TRasterCM32P)(floatingRas))
      floatingImage = TToonzImageP(toonzRas, toonzRas->getBounds());
    else if (TRaster32P fullColorRas = (TRaster32P)(floatingRas))
      floatingImage = TRasterImageP(fullColorRas);
    else if (TRasterGR8P grRas = (TRasterGR8P)(floatingRas))
      floatingImage = TRasterImageP(grRas);
    TImageCache::instance()->add(m_floatingImageId, floatingImage, false);

    m_oldFloatingImageId =
        "UndoPasteFloatingSelection_oldFloating_" + std::to_string(m_id);
    TRasterP oldFloatingRas = currentSelection->getOriginalFloatingSelection();
    TImageP olfFloatingImage;
    if (TRasterCM32P toonzRas = (TRasterCM32P)(oldFloatingRas))
      olfFloatingImage = TToonzImageP(toonzRas, toonzRas->getBounds());
    else if (TRaster32P fullColorRas = (TRaster32P)(oldFloatingRas))
      olfFloatingImage = TRasterImageP(fullColorRas);
    else if (TRasterGR8P grRas = (TRasterGR8P)(oldFloatingRas))
      olfFloatingImage = TRasterImageP(grRas);
    TImageCache::instance()->add(m_oldFloatingImageId, olfFloatingImage, false);

    TPaletteP imgPalette = image->getPalette();
    m_newPalette         = imgPalette ? imgPalette->clone() : 0;
    TRasterP rasImage    = getRaster(image);
    TRectD wRect         = m_selectionRect.enlarge(2);
    TRect rRect          = convertWorldToRaster(wRect, image);
    rRect *= rasImage->getBounds();
    if (!rRect.isEmpty()) {
      m_undoImageId = "UndoPasteFloatingSelection_undo" + std::to_string(m_id);
      TRasterP undoRas = rasImage->extract(rRect)->clone();
      TImageP undoImage;
      if (TRasterCM32P toonzRas = (TRasterCM32P)(undoRas))
        undoImage = TToonzImageP(toonzRas, toonzRas->getBounds());
      else if (TRaster32P fullColorRas = (TRaster32P)(undoRas))
        undoImage = TRasterImageP(fullColorRas);
      else if (TRasterGR8P grRas = (TRasterGR8P)(undoRas))
        undoImage = TRasterImageP(grRas);
      TImageCache::instance()->add(m_undoImageId, undoImage, false);
    }
    m_startPos = currentSelection->getStartPosition();
    m_id++;
    m_tool = TTool::getApplication()->getCurrentTool()->getTool();
  }

  ~UndoPasteFloatingSelection() {
    if (TImageCache::instance()->isCached(m_imageId))
      TImageCache::instance()->remove(m_imageId);
    if (TImageCache::instance()->isCached(m_floatingImageId))
      TImageCache::instance()->remove(m_floatingImageId);
    if (TImageCache::instance()->isCached(m_undoImageId))
      TImageCache::instance()->remove(m_undoImageId);
  }

  void undo() const override {
    TImageP image = TImageCache::instance()->get(m_imageId, false);
    if (!image) return;
    TRasterP rasImage = getRaster(image);
    TRectD wRect      = m_selectionRect.enlarge(2);
    TRect rRect       = convertWorldToRaster(wRect, image);
    rRect *= rasImage->getBounds();
    if (!m_undoImageId.empty()) {
      TImageP undoImage = TImageCache::instance()->get(m_undoImageId, false);
      if (!undoImage) return;
      rasImage->copy(getRaster(undoImage), rRect.getP00());
    }

    TXshSimpleLevelP sl(m_imageCell.getSimpleLevel());
    const TFrameId &fid = m_imageCell.m_frameId;

    if (!m_isPastedSelection) {
      TImageP floatingImage =
          TImageCache::instance()->get(m_oldFloatingImageId, false);
      if (!floatingImage) return;
      TRasterP floatingRaster = getRaster(floatingImage);
      TRop::over(rasImage, floatingRaster, m_startPos);
    }

    ToolUtils::updateSaveBox(sl, fid);

    if (m_oldPalette) image->getPalette()->assign(m_oldPalette->clone());
    TTool::Application *app = TTool::getApplication();
    app->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();
    if (!m_tool) return;
    m_tool->notifyImageChanged(m_frameId);
    m_tool->invalidate();
  }

  void redo() const override {
    TImageP image = TImageCache::instance()->get(m_imageId, false);
    TImageP floatingImage =
        TImageCache::instance()->get(m_floatingImageId, false);
    if (!floatingImage || !image) return;
    TRasterP floatingRas = getRaster(floatingImage);

    TXshSimpleLevelP sl(m_imageCell.getSimpleLevel());
    const TFrameId &fid = m_imageCell.m_frameId;

    if (!m_isPastedSelection) deleteSelectionWithoutUndo(image, m_strokes);

    TRasterP ras = getRaster(image);
    pasteFloatingSelectionWithoutUndo(image, floatingRas, m_transformation,
                                      m_selectionRect, m_noAntialiasing);

    ToolUtils::updateSaveBox(sl, fid);

    if (m_newPalette) image->getPalette()->assign(m_newPalette->clone());
    TTool::Application *app = TTool::getApplication();
    app->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();
    if (!m_tool) return;
    m_tool->notifyImageChanged(m_frameId);
    m_tool->invalidate();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Paste"); }
};

int UndoPasteFloatingSelection::m_id = 0;

//=============================================================================
// Next methods are used to compute intersection between selected stroke and
// image box
//-----------------------------------------------------------------------------
/*! Help function.*/
TSegment getSegmentByIndex(TRectD rect, int index) {
  if (index == 0) return TSegment(rect.getP00(), rect.getP01());
  if (index == 1) return TSegment(rect.getP01(), rect.getP11());
  if (index == 2) return TSegment(rect.getP11(), rect.getP10());
  if (index == 3) return TSegment(rect.getP10(), rect.getP00());
  return TSegment();
}

//-----------------------------------------------------------------------------
/*! Help function. precPoint -> point */
bool isClockwise(TRectD bbox, int segmentIndex, TThickPoint precPoint,
                 TThickPoint point) {
  if (segmentIndex == 0) return precPoint.y > point.y;
  if (segmentIndex == 1) return precPoint.x > point.x;
  if (segmentIndex == 2) return precPoint.y < point.y;
  if (segmentIndex == 3) return precPoint.x < point.x;
  return true;
}

//-----------------------------------------------------------------------------
/*! Help function.*/
void addPointToVector(TThickPoint point, std::vector<TThickPoint> &points,
                      bool insertMiddlePoint) {
  if (insertMiddlePoint)
    points.push_back((points[points.size() - 1] + point) * 0.5);
  points.push_back(point);
}

//-----------------------------------------------------------------------------
/*! Return intersection between \b bbox and \b chuck;
    set segmentIndex to index of segment that contains intersection. */
TThickPoint getIntersectionPoint(TRectD bbox, const TThickQuadratic *chunk,
                                 int &segmentIndex,
                                 bool secondChunkIntersection) {
  TStroke stroke;
  std::vector<TThickPoint> points;
  points.push_back(chunk->getThickP0());
  points.push_back(chunk->getThickP1());
  points.push_back(chunk->getThickP2());
  stroke.reshape(&points[0], points.size());

  std::vector<DoublePair> intersectionInfo;

  std::vector<DoublePair> intersections;
  TSegment segment0(bbox.getP00(), bbox.getP01());
  int count0 = intersect(stroke, segment0, intersections);
  if (count0 > 0) {
    DoublePair pair;
    pair.first  = intersections[0].first;
    pair.second = 0;
    intersectionInfo.push_back(pair);
    intersections.clear();
  }
  TSegment segment1(bbox.getP01(), bbox.getP11());
  int count1 = intersect(stroke, segment1, intersections);
  if (count1 > 0) {
    DoublePair pair;
    pair.first  = intersections[0].first;
    pair.second = 1;
    intersectionInfo.push_back(pair);
    intersections.clear();
  }
  TSegment segment2(bbox.getP11(), bbox.getP10());
  int count2 = intersect(stroke, segment2, intersections);
  if (count2 > 0) {
    DoublePair pair;
    pair.first  = intersections[0].first;
    pair.second = 2;
    intersectionInfo.push_back(pair);
    intersections.clear();
  }
  TSegment segment3(bbox.getP10(), bbox.getP00());
  int count3 = intersect(stroke, segment3, intersections);
  if (count3 > 0) {
    DoublePair pair;
    pair.first  = intersections[0].first;
    pair.second = 3;
    intersectionInfo.push_back(pair);
    intersections.clear();
  }
  int infoSize = intersectionInfo.size();
  assert(infoSize <= 2);
  if (infoSize == 1) {
    segmentIndex = intersectionInfo[0].second;
    return stroke.getPoint(intersectionInfo[0].first);
  } else if (infoSize == 2) {
    double firstT  = intersectionInfo[0].first;
    double secondT = intersectionInfo[1].first;
    if (!secondChunkIntersection) {
      if (firstT < secondT) {
        segmentIndex = intersectionInfo[0].second;
        return stroke.getPoint(intersectionInfo[0].first);
      } else {
        segmentIndex = intersectionInfo[1].second;
        return stroke.getPoint(intersectionInfo[1].first);
      }
    } else {
      if (firstT > secondT) {
        segmentIndex = intersectionInfo[0].second;
        return stroke.getPoint(intersectionInfo[0].first);
      } else {
        segmentIndex = intersectionInfo[1].second;
        return stroke.getPoint(intersectionInfo[1].first);
      }
    }
  }
  segmentIndex = -1;
  return TThickPoint();
}

//-----------------------------------------------------------------------------
/*! Insert \b bbox corners in \b points if \b bbox corners are contained in \b
 * outPoints stroke. */
void insertBoxCorners(TRectD bbox, std::vector<TThickPoint> &points,
                      std::vector<TThickPoint> outPoints,
                      int currentSegmentIndex, int precSegmentIndex) {
  if (outPoints[0] != outPoints[(int)outPoints.size() - 1])
    addPointToVector(outPoints[0], outPoints, true);
  assert((int)outPoints.size() % 2 == 1);
  TStroke *outPointsStroke = new TStroke();
  outPointsStroke->reshape(&(outPoints[0]), outPoints.size());
  TVectorImageP vi(new TVectorImage());
  vi->addStroke(outPointsStroke);
  vi->findRegions();
  assert((int)vi->getRegionCount() > 0);
  bool sameIndex = (precSegmentIndex == currentSegmentIndex);
  if (currentSegmentIndex == -1) return;
  int j;
  for (j = sameIndex ? 1 : 0; j < 2; j++) {
    bool clockwise = j;
    if (sameIndex)
      clockwise = isClockwise(bbox, currentSegmentIndex,
                              outPoints[outPoints.size() - 2],
                              outPoints[outPoints.size() - 1]);
    int segmentIndex = precSegmentIndex;
    if (sameIndex)
      segmentIndex =
          clockwise ? currentSegmentIndex - 1 : currentSegmentIndex + 1;
    if (segmentIndex < 0) segmentIndex = 3;
    if (segmentIndex > 3) segmentIndex = 0;
    while (segmentIndex != currentSegmentIndex) {
      if (sameIndex)  // controllo anche il segmento di partenza.
      {
        segmentIndex = currentSegmentIndex;
        sameIndex    = false;
      }
      TSegment s         = getSegmentByIndex(bbox, segmentIndex);
      TThickPoint corner = clockwise ? s.getP0() : s.getP1();
      int i;
      for (i = 0; i < (int)vi->getRegionCount(); i++)
        if (vi->getRegion(i)->contains(corner)) {
          if ((int)points.size() % 2 == 1)
            points.push_back((points[points.size() - 1] + corner) * 0.5);
          points.push_back(corner);
        }
      segmentIndex = clockwise ? segmentIndex - 1 : segmentIndex + 1;
      if (segmentIndex < 0) segmentIndex = 3;
      if (segmentIndex > 3) segmentIndex = 0;
    }
  }
}

//-----------------------------------------------------------------------------

TStroke getStrokeByRect(TRectD r) {
  TStroke stroke;
  if (r.isEmpty()) return stroke;
  std::vector<TThickPoint> points;
  points.push_back(r.getP00());
  points.push_back((r.getP00() + r.getP01()) * 0.5);
  points.push_back(r.getP01());
  points.push_back((r.getP01() + r.getP11()) * 0.5);
  points.push_back(r.getP11());
  points.push_back((r.getP11() + r.getP10()) * 0.5);
  points.push_back(r.getP10());
  points.push_back((r.getP10() + r.getP00()) * 0.5);
  points.push_back(r.getP00());
  stroke.reshape(&(points[0]), points.size());
  stroke.setSelfLoop(true);
  return stroke;
}

//-----------------------------------------------------------------------------

TStroke getIntersectedStroke(TStroke &stroke, TRectD bbox) {
  int cpCount = stroke.getControlPointCount();
  if (cpCount == 0) return stroke;
  // isFirstTime, startSegmentIndex e startOutPoints sono usati nel il caso in
  // cui lo stroke inizia fuori dalla bbox.
  bool isFirstTime = true;
  std::vector<TThickPoint> points, outPoints, startOutPoints;
  TThickPoint precPoint    = stroke.getControlPoint(0);
  bool isPrecPointInternal = bbox.contains(precPoint);
  if (isPrecPointInternal)
    points.push_back(precPoint);
  else
    outPoints.push_back(precPoint);
  int i;
  int precSegmentIndex, currentSegmentIndex, startSegmentIndex,
      precChunkIndex = -1;
  for (i = 1; i < stroke.getControlPointCount(); i++) {
    TThickPoint point    = stroke.getControlPoint(i);
    bool isPointInternal = bbox.contains(point);
    if (isPointInternal && isPrecPointInternal)
      addPointToVector(point, points, (int)points.size() % 2 != i % 2);
    if (!isPointInternal && !isPrecPointInternal)
      addPointToVector(
          point, outPoints,
          (int)outPoints.size() > 0 && (int)outPoints.size() % 2 != i % 2);
    if (isPointInternal != isPrecPointInternal) {
      // Devo trovare l'intersezione
      int chunkIndex = (i % 2 == 0) ? (i * 0.5) - 1 : i * 0.5;
      TThickPoint p  = getIntersectionPoint(bbox, stroke.getChunk(chunkIndex),
                                           currentSegmentIndex,
                                           chunkIndex == precChunkIndex);
      precChunkIndex = chunkIndex;
      addPointToVector(p, outPoints, (int)outPoints.size() % 2 == 1);
      if (!isPrecPointInternal && points.size() > 0 && outPoints.size() > 0) {
        insertBoxCorners(bbox, points, outPoints, currentSegmentIndex,
                         precSegmentIndex);
        outPoints.clear();
      } else if (outPoints.size() > 0 && isFirstTime) {
        startSegmentIndex = currentSegmentIndex;
        startOutPoints    = outPoints;
        outPoints.clear();
      }
      isFirstTime      = false;
      precSegmentIndex = currentSegmentIndex;
      addPointToVector(p, points, (int)points.size() % 2 == 1);
      addPointToVector(p, outPoints, (int)outPoints.size() % 2 == 1);
    }
    if (isPointInternal && !isPrecPointInternal)
      addPointToVector(point, points, (int)points.size() % 2 != i % 2);
    if (!isPointInternal && isPrecPointInternal)
      addPointToVector(
          point, outPoints,
          (int)outPoints.size() > 0 && (int)outPoints.size() % 2 != i % 2);
    isPrecPointInternal = isPointInternal;
  }
  // Caso in cui lo stroke aveva il primo punto fuori dalla bbox
  if (!isPrecPointInternal && points.size() > 0 && outPoints.size() > 0) {
    int t;
    for (t = 0; t < (int)outPoints.size(); t++)
      addPointToVector(outPoints[t], startOutPoints,
                       (int)startOutPoints.size() % 2 != 2);
    insertBoxCorners(bbox, points, startOutPoints, startSegmentIndex,
                     currentSegmentIndex);
    outPoints.clear();
  }

  // Caso particolare in cui lo stroke non ha intersezione con la bbox
  if (points.size() == 0) {  // Lo stroke e' completamente contenuto nella bbox
    if (bbox.contains(precPoint))
      return stroke;
    else
      return getStrokeByRect(bbox);
  }
  if (points[0] != points[(int)points.size() - 1])
    addPointToVector(points[0], points, true);

  assert((int)points.size() % 2 == 1);
  TStroke intersectedStroke;
  intersectedStroke.reshape(&(points[0]), points.size());
  intersectedStroke.setSelfLoop(true);
  return intersectedStroke;
}

}  // namespace

//=============================================================================
// RasterSelection
//-----------------------------------------------------------------------------

RasterSelection::RasterSelection()
    : TSelection()
    , m_currentImage()
    , m_oldPalette(0)
    , m_selectionBbox()
    , m_affine()
    , m_startPosition()
    , m_floatingSelection()
    , m_originalfloatingSelection()
    , m_fid()
    , m_transformationCount(0)
    , m_isPastedSelection(false)
    , m_noAntialiasing(false) {
  m_strokes.clear();
  m_originalStrokes.clear();
}

//-----------------------------------------------------------------------------

RasterSelection::RasterSelection(const RasterSelection &src)
    : TSelection()
    , m_currentImage(src.m_currentImage)
    , m_oldPalette(src.m_oldPalette)
    , m_selectionBbox(src.m_selectionBbox)
    , m_strokes(src.m_strokes)
    , m_originalStrokes(src.m_originalStrokes)
    , m_affine(src.m_affine)
    , m_startPosition(src.m_startPosition)
    , m_fid(src.m_fid)
    , m_transformationCount(src.m_transformationCount)
    , m_isPastedSelection(src.m_isPastedSelection)
    , m_noAntialiasing(src.m_noAntialiasing)

{
  setView(src.getView());
  if (src.isFloating()) {
    m_floatingSelection = src.m_floatingSelection->clone();
    if (src.m_originalfloatingSelection)
      m_originalfloatingSelection = src.m_originalfloatingSelection->clone();
    assert(isFloating());
  }
}

//-----------------------------------------------------------------------------

//! Returns the clone of this selection
TSelection *RasterSelection::clone() const {
  RasterSelection *rs = new RasterSelection(*this);
  return rs;
}

//-----------------------------------------------------------------------------

//! Notify to the viewer that the selection is changed.
void RasterSelection::notify() {
  RasterSelection *selection = dynamic_cast<RasterSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
}

//-----------------------------------------------------------------------------

//! Empty the selection.
//! If the selection is floating, the floating image is pasted using the current
//! tranformation.
void RasterSelection::selectNone() {
  if (isFloating()) {
    pasteFloatingSelection();
    notify();
    return;
  }
  m_selectionBbox = TRectD();
  m_strokes.clear();
  m_originalStrokes.clear();
  m_affine                    = TAffine();
  m_startPosition             = TPoint();
  m_floatingSelection         = TRasterP();
  m_originalfloatingSelection = TRasterP();
  m_transformationCount       = 0;
  m_isPastedSelection         = false;
  m_oldPalette                = 0;
  notify();
}

//-----------------------------------------------------------------------------

void RasterSelection::select(TStroke &stroke) {
  TRect box                 = getRaster(m_currentImage)->getBounds();
  TRectD rasterBbox         = convertRasterToWorld(box, m_currentImage);
  TStroke intersectedStroke = getIntersectedStroke(stroke, rasterBbox);
  if ((int)intersectedStroke.getControlPointCount() == 0) return;
  m_strokes.push_back(intersectedStroke);
  m_originalStrokes.push_back(intersectedStroke);
  notify();
}

//-----------------------------------------------------------------------------

void RasterSelection::select(const TRectD &rect) {
  assert(!!m_currentImage);
  TRectD r  = rect;
  TRect box = getRaster(m_currentImage)->getBounds();
  r *= convertRasterToWorld(box, m_currentImage);
  if (!r.isEmpty()) {
    TStroke stroke = getStrokeByRect(r);
    if ((int)stroke.getControlPointCount() == 0) return;
    m_strokes.push_back(stroke);
    m_originalStrokes.push_back(stroke);
  }
  notify();
}

//-----------------------------------------------------------------------------

void RasterSelection::selectAll() {
  if (!m_currentImage) return;
  selectNone();
  TRectD wRect = convertRasterToWorld(getRaster(m_currentImage)->getBounds(),
                                      m_currentImage);
  select(wRect);
}

//-----------------------------------------------------------------------------

bool RasterSelection::isEmpty() const {
  return getStrokesBound(m_strokes).isEmpty();
}

//-----------------------------------------------------------------------------

void RasterSelection::enableCommands() {
  enableCommand(this, MI_Clear, &RasterSelection::deleteSelection);
  enableCommand(this, MI_Cut, &RasterSelection::cutSelection);
  enableCommand(this, MI_Copy, &RasterSelection::copySelection);
  enableCommand(this, MI_Paste, &RasterSelection::pasteSelection);
  enableCommand(this, MI_SelectAll, &RasterSelection::selectAll);
}

//-----------------------------------------------------------------------------

bool RasterSelection::isFloating() const { return m_floatingSelection; }

//-----------------------------------------------------------------------------

void RasterSelection::transform(const TAffine &affine) {
  m_affine = affine * m_affine;
}

//-----------------------------------------------------------------------------

void RasterSelection::makeFloating() {
  if (isEmpty()) return;
  if (!m_currentImage) return;
  m_floatingSelection         = getImageFromSelection(m_currentImage, *this);
  m_originalfloatingSelection = m_floatingSelection->clone();
  deleteSelectionWithoutUndo(m_currentImage, m_strokes);

  ToolUtils::updateSaveBox();
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  tool->notifyImageChanged(m_fid);
}

//-----------------------------------------------------------------------------

void RasterSelection::pasteFloatingSelection() {
  if (!isFloating()) return;

  assert(m_transformationCount != -1 && m_transformationCount != -2);

  if (m_isPastedSelection)
    TUndoManager::manager()->popUndo(m_transformationCount + 1);
  else
    TUndoManager::manager()->popUndo(m_transformationCount);

  if (m_transformationCount > 0 || m_isPastedSelection)
    TUndoManager::manager()->add(new UndoPasteFloatingSelection(
        this, m_oldPalette.getPointer(), m_noAntialiasing));
  else if (m_transformationCount == 0)
    TUndoManager::manager()->popUndo(-1, true);

  TRectD wRect = getSelectionBbox();
  pasteFloatingSelectionWithoutUndo(m_currentImage, m_floatingSelection,
                                    m_affine, wRect, m_noAntialiasing);

  ToolUtils::updateSaveBox(m_currentImageCell.getSimpleLevel(),
                           m_currentImageCell.getFrameId());

  setFloatingSeletion(TRasterP());
  selectNone();

  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  tool->notifyImageChanged(m_fid);
}

//-----------------------------------------------------------------------------

void RasterSelection::deleteSelection() {
  if (!m_currentImage) return;
  TTool::Application *app = TTool::getApplication();
  TXshSimpleLevel *level  = app->getCurrentLevel()->getSimpleLevel();
  // we have to remove all undo transformation and the undo for the makeFloating
  // operation!
  if (isFloating()) {
    assert(m_transformationCount != -1 && m_transformationCount != -2);
    if (m_isPastedSelection)
      TUndoManager::manager()->popUndo(m_transformationCount + 1);
    else
      TUndoManager::manager()->popUndo(m_transformationCount);
  }
  if (!isPastedSelection() && !isEmpty())
    TUndoManager::manager()->add(new UndoDeleteSelection(this, level));

  if (!isFloating())
    deleteSelectionWithoutUndo(m_currentImage, m_strokes);
  else if (m_oldPalette)
    m_currentImage->getPalette()->assign(m_oldPalette.getPointer());
  m_floatingSelection         = TRasterP();
  m_originalfloatingSelection = TRasterP();

  ToolUtils::updateSaveBox();
  selectNone();
  app->getPaletteController()->getCurrentLevelPalette()->notifyPaletteChanged();
  TTool *tool = app->getCurrentTool()->getTool();
  tool->notifyImageChanged(m_fid);
}

//-----------------------------------------------------------------------------

void RasterSelection::copySelection() {
  if (isEmpty() || !m_currentImage) return;
  TRasterP ras;
  if (!isFloating())
    ras = getImageFromSelection(m_currentImage, *this);
  else
    ras = m_floatingSelection;

  double dpix, dpiy;
  std::vector<TRectD> rect;
  if (TToonzImageP ti = (TToonzImageP)(m_currentImage)) {
    ToonzImageData *data = new ToonzImageData();
    ti->getDpi(dpix, dpiy);
    data->setData(ras, ti->getPalette(), dpix, dpiy, ti->getSize(), rect,
                  m_strokes, m_originalStrokes, m_affine);
    QApplication::clipboard()->setMimeData(cloneData(data));
  } else if (TRasterImageP ri = (TRasterImageP)(m_currentImage)) {
    FullColorImageData *data = new FullColorImageData();
    ri->getDpi(dpix, dpiy);
    data->setData(ras, ri->getPalette(), dpix, dpiy, ri->getRaster()->getSize(),
                  rect, m_strokes, m_originalStrokes, m_affine);
    QApplication::clipboard()->setMimeData(cloneData(data));
  }
}

//-----------------------------------------------------------------------------

void RasterSelection::cutSelection() {
  copySelection();
  deleteSelection();
}

//-----------------------------------------------------------------------------

void RasterSelection::pasteSelection(const RasterImageData *riData) {
  std::vector<TRectD> rect;
  double currentDpiX, currentDpiY;
  double dpiX, dpiY;
  const ToonzImageData *toonzImageData =
      dynamic_cast<const ToonzImageData *>(riData);
  const FullColorImageData *fullColorData =
      dynamic_cast<const FullColorImageData *>(riData);
  if (TToonzImageP ti = (TToonzImageP)m_currentImage) {
    ti->getDpi(currentDpiX, currentDpiY);
    TRasterP cmRas;
    if (fullColorData) {
      DVGui::error(QObject::tr(
          "The copied selection cannot be pasted in the current drawing."));
      return;
    }
    riData->getData(cmRas, dpiX, dpiY, rect, m_strokes, m_originalStrokes,
                    m_affine, m_currentImage->getPalette());
    if (!cmRas) return;
    m_floatingSelection = cmRas;
  } else if (TRasterImageP ri = (TRasterImageP)m_currentImage) {
    ri->getDpi(currentDpiX, currentDpiY);
    TRasterP ras;
    riData->getData(ras, dpiX, dpiY, rect, m_strokes, m_originalStrokes,
                    m_affine, ri->getPalette());
    if (!ras) return;
    if (TRasterCM32P rasCM = ras) {
      TDimension dim = rasCM->getSize();
      TRaster32P app = TRaster32P(dim.lx, dim.ly);
      TRop::convert(app, rasCM, ri->getPalette());
      ras = app;
    }
    m_floatingSelection = ras;
  }
  if (m_floatingSelection)
    m_originalfloatingSelection = m_floatingSelection->clone();
  TScale sc;
  if (dpiX != 0 && dpiY != 0 && currentDpiX != 0 && currentDpiY != 0)
    sc     = TScale(currentDpiX / dpiX, currentDpiY / dpiY);
  m_affine = m_affine * sc;
}

//-----------------------------------------------------------------------------

void RasterSelection::pasteSelection() {
  TTool::Application *app = TTool::getApplication();
  TTool *tool             = app->getCurrentTool()->getTool();
  TImageP image           = tool->getImage(true);
  m_currentImage          = image;
  m_fid                   = tool->getCurrentFid();

  QClipboard *clipboard = QApplication::clipboard();
  const RasterImageData *riData =
      dynamic_cast<const RasterImageData *>(clipboard->mimeData());
  const StrokesData *stData =
      dynamic_cast<const StrokesData *>(clipboard->mimeData());
  if (!riData && !stData) return;
  if (isFloating()) pasteFloatingSelection();
  selectNone();
  m_isPastedSelection = true;
  m_oldPalette        = m_currentImage->getPalette()->clone();
  if (stData) {
    if (TToonzImageP ti = m_currentImage)
      riData = stData->toToonzImageData(ti);
    else {
      TRasterImageP ri = m_currentImage;
      assert(ri);
      double dpix, dpiy;
      ri->getDpi(dpix, dpiy);
      if (dpix == 0 || dpiy == 0) {
        TPointD dpi =
            tool->getXsheet()->getScene()->getCurrentCamera()->getDpi();
        dpix = dpi.x;
        dpiy = dpi.y;
        ri->setDpi(dpix, dpiy);
      }
      riData = stData->toFullColorImageData(ri);
    }
  }

  if (!riData) return;
  pasteSelection(riData);

  app->getPaletteController()->getCurrentLevelPalette()->notifyPaletteChanged();
  notify();
  TUndoManager::manager()->add(new UndoPasteSelection(this));
}

//-----------------------------------------------------------------------------

bool RasterSelection::isTransformed() { return !m_affine.isIdentity(); }

//-----------------------------------------------------------------------------

TRectD RasterSelection::getStrokesBound(std::vector<TStroke> strokes) const {
  int i;
  TRectD box = TRectD();
  for (i = 0; i < (int)strokes.size(); i++) box += strokes[i].getBBox();
  return box;
}

//-----------------------------------------------------------------------------

TRectD RasterSelection::getSelectionBound() const {
  if (m_strokes.size() == 0) return TRectD();
  TRectD selectionBox            = getStrokesBound(m_strokes);
  if (isFloating()) selectionBox = m_affine * selectionBox;
  return selectionBox;
}

//-----------------------------------------------------------------------------

TRectD RasterSelection::getOriginalSelectionBound() const {
  if (m_originalStrokes.size() == 0) return TRectD();
  return getStrokesBound(m_originalStrokes);
}

//-----------------------------------------------------------------------------

TRectD RasterSelection::getSelectionBbox() const {
  TRectD rect            = m_selectionBbox;
  if (isFloating()) rect = m_affine * m_selectionBbox;
  return rect;
}

//-----------------------------------------------------------------------------

void RasterSelection::setSelectionBbox(const TRectD &rect) {
  m_selectionBbox = rect;
}
