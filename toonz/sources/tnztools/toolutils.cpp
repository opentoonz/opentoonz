

#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "toonzqt/imageutils.h"
#include "trop.h"
#include "tools/tool.h"
#include "tstroke.h"
#include "timageinfo.h"
#include "timagecache.h"
#include "tgl.h"

#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/imagemanager.h"
#include "toonz/ttileset.h"
#include "toonz/toonzimageutils.h"
#include "toonz/levelproperties.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobject.h"
#include "toonz/trasterimageutils.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"
#include "toonz/palettecontroller.h"

#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/selection.h"
#include "toonzqt/gutil.h"

#include "tools/strokeselection.h"

#include <QPainter>
#include <QGLWidget>  // for QGLWidget::convertToGLFormat
#include <QFont>
#include <QFontMetrics>

//****************************************************************************************
//    Local namespace
//****************************************************************************************

namespace {

//! Riempie il vettore \b theVect con gli indici degli stroke contenuti nel
//! mapping \b theMap.
void mapToVector(const std::map<int, VIStroke *> &theMap,
                 std::vector<int> &theVect) {
  assert(theMap.size() == theVect.size());
  std::map<int, VIStroke *>::const_iterator it = theMap.begin();
  UINT i = 0;
  for (; it != theMap.end(); ++it) {
    theVect[i++] = it->first;
  }
}

//------------------------------------------------------------

void updateSaveBox(const TToonzImageP &ti) {
  if (ti) {
    assert(ti->getRaster());            // Image should have a raster
    assert(ti->getSubsampling() == 1);  // Image should not be subsampled -
                                        // modified images must be the ORIGINAL
                                        // ones

    const TRect &savebox = ti->getSavebox();
    {
      TRect newSaveBox;
      TRop::computeBBox(ti->getRaster(), newSaveBox);  // This iterates the
                                                       // WHOLE raster to find
                                                       // its new savebox!

      if (!Preferences::instance()->isMinimizeSaveboxAfterEditing())
        newSaveBox +=
            savebox;  // If not minimizing the savebox, it cannot be shrunk.

      ti->setSavebox(newSaveBox);
    }
  }
}

}  // namespace

//****************************************************************************************
//    ToolUtils namespace
//****************************************************************************************

void ToolUtils::updateSaveBox(const TXshSimpleLevelP &sl, const TFrameId &fid) {
  // TODO: Savebox updates should not happen on mouse updates. This is,
  // unfortunately, what currently happens.
  sl->setDirtyFlag(true);

  TImageP img = sl->getFrame(fid, true);  // The image will be modified (it
                                          // should already have been, though)
  if (!img) return;
  // Observe that the returned image will forcedly have subsampling 1
  ::updateSaveBox(img);

  TImageInfo *info = sl->getFrameInfo(fid, true);
  ImageBuilder::setImageInfo(*info, img.getPointer());
}

//------------------------------------------------------------

void ToolUtils::updateSaveBox(const TXshSimpleLevelP &sl, const TFrameId &fid,
                              TImageP img) {
  sl->setFrame(fid, img);
  ToolUtils::updateSaveBox(sl, fid);
}

//------------------------------------------------------------

void ToolUtils::updateSaveBox() {
  TTool::Application *application = TTool::getApplication();
  if (!application) return;

  TXshLevel *xl = application->getCurrentLevel()->getLevel();
  if (!xl) return;

  TXshSimpleLevel *sl = xl->getSimpleLevel();
  if (!sl || sl->getType() != TZP_XSHLEVEL) return;

  TFrameId fid = getFrameId();
  ToolUtils::updateSaveBox(sl, fid);
}

//-----------------------------------------------------------------------------
//! Return the right value in both case: LevelFrame and SceneFrame.
TFrameId ToolUtils::getFrameId() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return TFrameId();

  TFrameHandle *frameHandle = app->getCurrentFrame();
  if (frameHandle->isEditingScene()) {
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (!xsh) return 0;
    int row = frameHandle->getFrame();
    int col = app->getCurrentColumn()->getColumnIndex();
    if (col < 0) return 0;
    TXshCell cell = xsh->getCell(row, col);
    return cell.getFrameId();
  } else
    return frameHandle->getFid();
}

//------------------------------------------------------------

void ToolUtils::drawRect(const TRectD &rect, const TPixel32 &color,
                         unsigned short stipple, bool doContrast) {
  GLint src, dst;
  bool isEnabled;
  tglColor(color);
  if (doContrast) {
    if (color == TPixel32::Black) tglColor(TPixel32(90, 90, 90));
    isEnabled = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC, &src);
    glGetIntegerv(GL_BLEND_DST, &dst);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
  }

  if (stipple != 0xffff) {
    glLineStipple(1, stipple);
    glEnable(GL_LINE_STIPPLE);
  }

  glBegin(GL_LINE_STRIP);
  tglVertex(rect.getP00());
  tglVertex(rect.getP01());
  tglVertex(rect.getP11());
  tglVertex(rect.getP10());
  tglVertex(rect.getP00());
  glEnd();
  glDisable(GL_LINE_STIPPLE);
  if (doContrast) {
    if (!isEnabled) glDisable(GL_BLEND);
    glBlendFunc(src, dst);
  }
}

//-----------------------------------------------------------------------------

void ToolUtils::fillRect(const TRectD &rect, const TPixel32 &color) {
  tglColor(color);
  glBegin(GL_QUADS);
  tglVertex(rect.getP00());
  tglVertex(rect.getP01());
  tglVertex(rect.getP11());
  tglVertex(rect.getP10());
  tglVertex(rect.getP00());
  glEnd();
}

//-----------------------------------------------------------------------------

void ToolUtils::drawPoint(const TPointD &q, double pixelSize) {
  double size = pixelSize * 2.0;
  glBegin(GL_QUADS);
  glVertex2d(q.x - size, q.y - size);
  glVertex2d(q.x - size, q.y + size);
  glVertex2d(q.x + size, q.y + size);
  glVertex2d(q.x + size, q.y - size);
  glEnd();
}

//-----------------------------------------------------------------------------

void ToolUtils::drawCross(const TPointD &q, double pixelSize) {
  double size = pixelSize;
  glBegin(GL_LINES);
  glVertex2d(q.x - size, q.y);
  glVertex2d(q.x + size, q.y);
  glEnd();
  glBegin(GL_LINES);
  glVertex2d(q.x, q.y - size);
  glVertex2d(q.x, q.y + size);
  glEnd();
}

//-----------------------------------------------------------------------------

void ToolUtils::drawArrow(const TSegment &s, double pixelSize) {
  TPointD v, vn;

  double length = s.getLength() * pixelSize;

  if (length == 0) return;

  v  = normalize(s.getSpeed());
  vn = v;

  TPointD p1 = s.getP0() + v * length;
  glBegin(GL_LINES);
  tglVertex(s.getP0());
  tglVertex(p1);
  glEnd();

  v = v * length * 0.7;

  vn = vn * length * 0.2;

  TPointD p;

  glBegin(GL_TRIANGLES);
  p = s.getP0() + v + rotate90(vn);
  tglVertex(p);
  tglVertex(p1);
  p = s.getP0() + v + rotate270(vn);
  tglVertex(p);
  glEnd();
}

//-----------------------------------------------------------------------------

void ToolUtils::drawSquare(const TPointD &pos, double r,
                           const TPixel32 &color) {
  TRectD rect(pos - TPointD(r, r), pos + TPointD(r, r));
  tglColor(color);
  glBegin(GL_LINE_STRIP);
  tglVertex(rect.getP00());
  tglVertex(rect.getP01());
  tglVertex(rect.getP11());
  tglVertex(rect.getP10());
  tglVertex(rect.getP00());
  glEnd();
}

//-----------------------------------------------------------------------------

void ToolUtils::drawRectWhitArrow(const TPointD &pos, double r) {
  if (TTool::getApplication()->getCurrentObject()->isSpline()) return;
  TRectD rect(pos - TPointD(14 * r, 2 * r), pos + TPointD(14 * r, 2 * r));
  tglColor(TPixel32::Black);
  glBegin(GL_POLYGON);
  tglVertex(rect.getP00());
  tglVertex(rect.getP10());
  tglVertex(rect.getP11());
  tglVertex(rect.getP01());
  glEnd();

  double par = 5 * r;

  TPointD p01 = 0.5 * (rect.getP00() + rect.getP10());
  TPointD p02 = 0.5 * (rect.getP01() + rect.getP11());
  TPointD p11 = TPointD(p01.x, p01.y - par);
  TPointD p12 = TPointD(p02.x, p02.y + par);
  TPointD p;

  tglColor(TPixel32(130, 130, 130));

  glBegin(GL_TRIANGLES);
  p = p11 + rotate90(TPointD(0, par));
  tglVertex(p);
  tglVertex(p01);
  p = p11 + rotate270(TPointD(0, par));
  tglVertex(p);
  glEnd();

  glBegin(GL_TRIANGLES);
  p = p12 + rotate90(TPointD(0, -par));
  tglVertex(p);
  tglVertex(p02);
  p = p12 + rotate270(TPointD(0, -par));
  tglVertex(p);
  glEnd();
}

//-----------------------------------------------------------------------------

QRadialGradient ToolUtils::getBrushPad(int size, double hardness) {
  hardness        = tcrop(hardness, 0.0, 0.97);
  double halfSize = size * 0.5;
  double x        = halfSize * hardness;
  TQuadratic q(TPointD(x, 1.0), TPointD((halfSize + x) * 0.5, 0.0),
               TPointD(halfSize, 0.0));
  QRadialGradient rd(QPointF(halfSize, halfSize), halfSize,
                     QPointF(halfSize, halfSize));
  rd.setColorAt(0, QColor(0, 0, 0));

  double t;
  double offset = halfSize - x;
  assert(offset > 0);
  for (t = 0; t <= 1; t += 1.0 / offset) {
    TPointD p = q.getPoint(t);
    int value = 255 * p.y;
    rd.setColorAt(p.x / halfSize, QColor(0, 0, 0, value));
  }
  return rd;
}

//-----------------------------------------------------------------------------

QList<TRect> ToolUtils::splitRect(const TRect &first, const TRect &second) {
  TRect intersection = first * second;
  QList<TRect> rects;
  if (intersection.isEmpty()) {
    rects.append(first);
    return rects;
  }

  TRect rect;
  if (first.x0 < intersection.x0) {
    rect = TRect(first.getP00(), TPoint(intersection.x0 - 1, first.y1));
    rects.append(rect);
  }
  if (intersection.x1 < first.x1) {
    rect = TRect(TPoint(intersection.x1 + 1, first.y0), first.getP11());
    rects.append(rect);
  }
  if (intersection.y1 < first.y1) {
    rect =
        TRect(intersection.x0, intersection.y1 + 1, intersection.x1, first.y1);
    rects.append(rect);
  }
  if (first.y0 < intersection.y0) {
    rect =
        TRect(intersection.x0, first.y0, intersection.x1, intersection.y0 - 1);
    rects.append(rect);
  }
  return rects;
}

//-----------------------------------------------------------------------------

TRaster32P ToolUtils::convertStrokeToImage(TStroke *stroke,
                                           const TRect &imageBounds,
                                           TPoint &pos) {
  int count = stroke->getControlPointCount();
  if (count == 0) return TRaster32P();
  TPointD imgCenter = TPointD((imageBounds.x0 + imageBounds.x1) * 0.5,
                              (imageBounds.y0 + imageBounds.y1) * 0.5);

  TStroke s(*stroke);

  // check self looped stroke
  TThickPoint first = s.getControlPoint(0);
  TThickPoint back  = s.getControlPoint(count - 1);
  if (first != back) {
    TPointD mid = (first + back) * 0.5;
    s.setControlPoint(count, mid);
    s.setControlPoint(count + 1, first);
  }

  // check bounds intersection
  s.transform(TTranslation(imgCenter));
  TRectD bbox = s.getBBox();
  TRect rect(tfloor(bbox.x0), tfloor(bbox.y0), tfloor(bbox.x1),
             tfloor(bbox.y1));
  pos = rect.getP00();
  pos = TPoint(pos.x > 0 ? pos.x : 0, pos.y > 0 ? pos.y : 0);
  rect *= imageBounds;
  if (rect.isEmpty()) return TRaster32P();

  // creates the image
  QImage img(rect.getLx(), rect.getLy(), QImage::Format_ARGB32);
  img.fill(Qt::transparent);
  QColor color = Qt::black;
  QPainter p(&img);
  p.setPen(QPen(color, 1, Qt::SolidLine));
  p.setBrush(color);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPainterPath path = strokeToPainterPath(&s);
  QRectF pathRect   = path.boundingRect();
  p.translate(-toQPoint(pos));
  p.drawPath(path);
  return rasterFromQImage(img, true, false);
}

//-----------------------------------------------------------------------------

TStroke *ToolUtils::merge(const ArrayOfStroke &a) {
  std::vector<TThickPoint> v;

  TStroke *ref      = 0;
  int controlPoints = 0;

  for (UINT i = 0; i < a.size(); ++i) {
    ref = a[i];
    assert(ref);
    if (!ref) continue;

    controlPoints = ref->getControlPointCount() - 1;

    for (int j = 0; j < controlPoints; ++j)
      v.push_back(ref->getControlPoint(j));
  }

  if (controlPoints > 0) v.push_back(ref->getControlPoint(controlPoints));

  TStroke *out = new TStroke(v);

  return out;
}

//================================================================================================
//
// TToolUndo
//
//================================================================================================

ToolUtils::TToolUndo::TToolUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                                bool createdFrame, bool createdLevel,
                                const TPaletteP &oldPalette)
    : TUndo()
    , m_level(level)
    , m_frameId(frameId)
    , m_oldPalette(oldPalette)
    , m_col(-2)
    , m_row(-1)
    , m_isEditingLevel(false)
    , m_createdFrame(createdFrame)
    , m_createdLevel(createdLevel)
    , m_imageId("") {
  m_animationSheetEnabled = Preferences::instance()->isAnimationSheetEnabled();
  TTool::Application *app = TTool::getApplication();
  m_isEditingLevel        = app->getCurrentFrame()->isEditingLevel();
  if (!m_isEditingLevel) {
    m_col = app->getCurrentColumn()->getColumnIndex();
    m_row = app->getCurrentFrame()->getFrameIndex();
    if (m_animationSheetEnabled) {
      m_cellsData = TTool::m_cellsData;
    }
  }
  if (createdFrame) {
    m_imageId = "TToolUndo" + std::to_string(m_idCount++);
    TImageCache::instance()->add(m_imageId, level->getFrame(frameId, false),
                                 false);
  }
}

//------------------------------------------------------------------------------------------

ToolUtils::TToolUndo::~TToolUndo() {
  TImageCache::instance()->remove(m_imageId);
}

//-----------------------------------------------------------------------------

void ToolUtils::TToolUndo::insertLevelAndFrameIfNeeded() const {
  TTool::Application *app = TTool::getApplication();
  if (m_createdLevel) {
    TLevelSet *levelSet = app->getCurrentScene()->getScene()->getLevelSet();
    if (levelSet) {
      levelSet->insertLevel(m_level.getPointer());
      app->getCurrentScene()->notifyCastChange();
    }
  }
  if (m_createdFrame) {
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    TImageP img  = TImageCache::instance()->get(m_imageId, false);
    m_level->setFrame(m_frameId, img);
    if (!m_isEditingLevel) {
      if (m_animationSheetEnabled) {
        int m = m_cellsData.size() / 3;
        for (int i = 0; i < m; i++) {
          int r0   = m_cellsData[i * 3];
          int r1   = m_cellsData[i * 3 + 1];
          int type = m_cellsData[i * 3 + 2];
          TXshCell cell;
          if (type == 1)
            cell = xsh->getCell(r0 - 1, m_col);
          else
            cell = TXshCell(m_level.getPointer(), m_frameId);
          for (int r = r0; r <= r1; r++) xsh->setCell(r, m_col, cell);
        }
      } else {
        TXshCell cell(m_level.getPointer(), m_frameId);
        xsh->setCell(m_row, m_col, cell);
      }
    }
    app->getCurrentLevel()->notifyLevelChange();
  }
}

//-----------------------------------------------------------------------------

void ToolUtils::TToolUndo::removeLevelAndFrameIfNeeded() const {
  TTool::Application *app = TTool::getApplication();
  if (m_createdFrame) {
    m_level->eraseFrame(m_frameId);
    if (!m_isEditingLevel) {
      TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
      if (m_animationSheetEnabled) {
        int m = m_cellsData.size() / 3;
        for (int i = 0; i < m; i++) {
          int r0   = m_cellsData[i * 3];
          int r1   = m_cellsData[i * 3 + 1];
          int type = m_cellsData[i * 3 + 2];
          TXshCell cell;
          if (type == 0) cell = xsh->getCell(r0 - 1, m_col);
          for (int r = r0; r <= r1; r++) xsh->setCell(r, m_col, cell);
        }
      } else {
        xsh->clearCells(m_row, m_col);
      }
    }
    if (m_createdLevel) {
      // butta il livello
      TLevelSet *levelSet = app->getCurrentScene()->getScene()->getLevelSet();
      if (levelSet) {
        levelSet->removeLevel(m_level.getPointer());
        app->getCurrentScene()->notifyCastChange();
      }
    }
    app->getCurrentLevel()->notifyLevelChange();
  }
  if (m_oldPalette.getPointer()) {
    m_level->getPalette()->assign(m_oldPalette->clone());
    app->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();
  }
}

//------------------------------------------------------------------------------------------

void ToolUtils::TToolUndo::notifyImageChanged() const {
  TTool::Application *app = TTool::getApplication();

  TXshSimpleLevel *currentSl = 0;
  TFrameId currentFrameId;
  if (app->getCurrentFrame()->isEditingLevel()) {
    TXshLevel *xl = app->getCurrentLevel()->getLevel();
    if (!xl) return;
    currentSl      = xl->getSimpleLevel();
    currentFrameId = app->getCurrentFrame()->getFid();
  } else {
    int row = app->getCurrentFrame()->getFrame();
    int col = app->getCurrentColumn()->getColumnIndex();
    if (col < 0) return;
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (!xsh) return;
    TXshCell cell  = xsh->getCell(row, col);
    currentSl      = cell.getSimpleLevel();
    currentFrameId = cell.getFrameId();
  }
  if (currentSl == m_level.getPointer() && currentFrameId == m_frameId) {
    TTool *tool = app->getCurrentTool()->getTool();
    if (tool) tool->onImageChanged();
  }

  IconGenerator::instance()->invalidate(m_level.getPointer(), m_frameId);
  IconGenerator::instance()->invalidateSceneIcon();

  if (m_level && m_level->getType() == PLI_XSHLEVEL) {
    std::string id = m_level->getImageId(m_frameId) + "_rasterized";
    ImageManager::instance()->invalidate(id);
  }
}

int ToolUtils::TToolUndo::m_idCount = 0;

//================================================================================================

//================================================================================================

ToolUtils::TRasterUndo::TRasterUndo(TTileSetCM32 *tiles, TXshSimpleLevel *level,
                                    const TFrameId &frameId, bool createdFrame,
                                    bool createdLevel,
                                    const TPaletteP &oldPalette)
    : TToolUndo(level, frameId, createdFrame, createdLevel, oldPalette)
    , m_tiles(tiles) {}

//------------------------------------------------------------------------------------------

ToolUtils::TRasterUndo::~TRasterUndo() {
  if (m_tiles) delete m_tiles;
}

//------------------------------------------------------------------------------------------

TToonzImageP ToolUtils::TRasterUndo::getImage() const {
  if (m_level->isFid(m_frameId))
    return (TToonzImageP)m_level->getFrame(m_frameId, true);
  return 0;
}

//------------------------------------------------------------------------------------------

int ToolUtils::TRasterUndo::getSize() const {
  int size = sizeof(*this);
  size += sizeof(*(m_level.getPointer()));
  size += sizeof(*(m_oldPalette.getPointer()));
  return m_tiles ? m_tiles->getMemorySize() + size : size;
}

//------------------------------------------------------------------------------------------

void ToolUtils::TRasterUndo::undo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (m_tiles && m_tiles->getTileCount() > 0) {
    TToonzImageP image = getImage();
    if (!image) return;

    ToonzImageUtils::paste(image, m_tiles);
    ToolUtils::updateSaveBox(m_level, m_frameId);
  }

  removeLevelAndFrameIfNeeded();

  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//================================================================================================

//================================================================================================

ToolUtils::TFullColorRasterUndo::TFullColorRasterUndo(
    TTileSetFullColor *tiles, TXshSimpleLevel *level, const TFrameId &frameId,
    bool createdFrame, bool createdLevel, const TPaletteP &oldPalette)
    : TToolUndo(level, frameId, createdFrame, createdLevel, oldPalette)
    , m_tiles(tiles) {}

//-----------------------------------------------------------------------------

ToolUtils::TFullColorRasterUndo::~TFullColorRasterUndo() { delete m_tiles; }

//-----------------------------------------------------------------------------

TRasterImageP ToolUtils::TFullColorRasterUndo::getImage() const {
  if (m_level->isFid(m_frameId))
    return (TRasterImageP)m_level->getFrame(m_frameId, true);
  return 0;
}

//-----------------------------------------------------------------------------

int ToolUtils::TFullColorRasterUndo::getSize() const {
  int size = sizeof(*this);
  size += sizeof(*(m_level.getPointer()));
  size += sizeof(*(m_oldPalette.getPointer()));
  return m_tiles ? m_tiles->getMemorySize() + size : size;
}

//-----------------------------------------------------------------------------

void ToolUtils::TFullColorRasterUndo::undo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (m_tiles && m_tiles->getTileCount() > 0) {
    TRasterImageP image = getImage();
    if (!image) return;
    std::vector<TRect> rects = paste(image, m_tiles);
    int i;
    TRect resRect = rects[0];
    for (i = 1; i < (int)rects.size(); i++) resRect += rects[i];
  }

  removeLevelAndFrameIfNeeded();

  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

std::vector<TRect> ToolUtils::TFullColorRasterUndo::paste(
    const TRasterImageP &ti, const TTileSetFullColor *tileSet) const {
  std::vector<TRect> rects;
  TRasterP raster = ti->getRaster();
  for (int i = 0; i < tileSet->getTileCount(); i++) {
    const TTileSetFullColor::Tile *tile = tileSet->getTile(i);
    TRasterP ras;
    tile->getRaster(ras);
    assert(!!ras);
    raster->copy(ras, tile->m_rasterBounds.getP00());
    rects.push_back(tile->m_rasterBounds);
  }
  return rects;
}

//================================================================================================

//================================================================================================

ToolUtils::UndoModifyStroke::UndoModifyStroke(TXshSimpleLevel *level,
                                              const TFrameId &frameId,
                                              int strokeIndex)
    : TToolUndo(level, frameId), m_strokeIndex(strokeIndex) {
  TVectorImageP image = level->getFrame(frameId, true);
  assert(image);
  TStroke *stroke = image->getStroke(m_strokeIndex);
  int n           = stroke->getControlPointCount();
  for (int i = 0; i < n; i++) m_before.push_back(stroke->getControlPoint(i));

  m_selfLoopBefore = stroke->isSelfLoop();

  TTool::Application *app = TTool::getApplication();
  m_row                   = app->getCurrentFrame()->getFrame();
  m_column                = app->getCurrentColumn()->getColumnIndex();
}

//-----------------------------------------------------------------------------

ToolUtils::UndoModifyStroke::~UndoModifyStroke() {}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyStroke::onAdd() {
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;

  TStroke *stroke = image->getStroke(m_strokeIndex);
  assert(stroke);
  int n = stroke->getControlPointCount();
  for (int i = 0; i < n; i++) m_after.push_back(stroke->getControlPoint(i));

  m_selfLoopAfter = stroke->isSelfLoop();
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyStroke::undo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentFrame()->isEditingScene()) {
    app->getCurrentColumn()->setColumnIndex(m_column);
    app->getCurrentFrame()->setFrame(m_row);
  } else
    app->getCurrentFrame()->setFid(m_frameId);

  TSelection *selection = app->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;
  QMutexLocker lock(image->getMutex());
  TStroke *stroke = 0;
  if (image->getStrokeCount() ==
      1)  // && image->getStroke(0)->getStyle()==SplineStyle)
    stroke = image->getStroke(0);
  else
    stroke = image->getStroke(m_strokeIndex);
  if (!stroke) return;
  TStroke *oldStroke = new TStroke(*stroke);
  stroke->reshape(&m_before[0], m_before.size());
  stroke->setSelfLoop(m_selfLoopBefore);
  image->notifyChangedStrokes(m_strokeIndex, oldStroke);
  notifyImageChanged();
  delete oldStroke;
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyStroke::redo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentFrame()->isEditingScene()) {
    app->getCurrentColumn()->setColumnIndex(m_column);
    app->getCurrentFrame()->setFrame(m_row);
  } else
    app->getCurrentFrame()->setFid(m_frameId);

  TSelection *selection = app->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;
  QMutexLocker lock(image->getMutex());
  TStroke *stroke = 0;
  if (image->getStrokeCount() ==
      1)  //&& image->getStroke(0)->getStyle()==SplineStyle)
    stroke = image->getStroke(0);
  else
    stroke = image->getStroke(m_strokeIndex);
  if (!stroke) return;
  TStroke *oldStroke = new TStroke(*stroke);
  stroke->reshape(&m_after[0], m_after.size());
  stroke->setSelfLoop(m_selfLoopAfter);
  image->notifyChangedStrokes(m_strokeIndex, oldStroke);
  delete oldStroke;
  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

int ToolUtils::UndoModifyStroke::getSize() const {
  return (m_before.capacity() + m_after.capacity()) * sizeof(TThickPoint) +
         sizeof(*this) + 500;
}

//-----------------------------------------------------------------------------

ToolUtils::UndoModifyStrokeAndPaint::UndoModifyStrokeAndPaint(
    TXshSimpleLevel *level, const TFrameId &frameId, int strokeIndex)
    : UndoModifyStroke(level, frameId, strokeIndex), m_fillInformation(0) {
  TVectorImageP image = level->getFrame(frameId, true);
  assert(image);
  TStroke *stroke = image->getStroke(strokeIndex);
  m_oldBBox       = stroke->getBBox();
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyStrokeAndPaint::onAdd() {
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(!!image);
  if (!image) return;

  UndoModifyStroke::onAdd();
  TStroke *stroke   = image->getStroke(m_strokeIndex);
  m_fillInformation = new std::vector<TFilledRegionInf>;
  ImageUtils::getFillingInformationOverlappingArea(
      image, *m_fillInformation, m_oldBBox, stroke->getBBox());
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyStrokeAndPaint::undo() const {
  TTool::Application *application = TTool::getApplication();
  if (!application) return;

  UndoModifyStroke::undo();
  TRegion *reg;
  UINT size = m_fillInformation->size();
  if (!size) {
    application->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
    return;
  }

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(!!image);
  if (!image) return;

  // image->validateRegions();
  image->findRegions();
  for (UINT i = 0; i < size; i++) {
    reg = image->getRegion((*m_fillInformation)[i].m_regionId);
    assert(reg);
    if (reg) reg->setStyle((*m_fillInformation)[i].m_styleId);
  }

  application->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}
//-----------------------------------------------------------------------------

ToolUtils::UndoModifyStrokeAndPaint::~UndoModifyStrokeAndPaint() {
  delete m_fillInformation;
}

//-----------------------------------------------------------------------------

int ToolUtils::UndoModifyStrokeAndPaint::getSize() const {
  int size = m_fillInformation
                 ? m_fillInformation->size() * sizeof(TFilledRegionInf)
                 : 0;
  return UndoModifyStroke::getSize() + size;
}

//-----------------------------------------------------------------------------

ToolUtils::UndoModifyListStroke::UndoModifyListStroke(
    TXshSimpleLevel *level, const TFrameId &frameId,
    const std::vector<TStroke *> &strokeVect)
    : TToolUndo(level, frameId), m_fillInformation(0) {
  UINT strokeNum      = strokeVect.size();
  TVectorImageP image = level->getFrame(frameId, true);
  assert(image);
  for (UINT i = 0; i < strokeNum; i++) {
    m_oldBBox += (strokeVect[i])->getBBox();
    int strokeIndex = image->getStrokeIndex(strokeVect[i]);
    m_strokeList.push_back(new UndoModifyStroke(level, frameId, strokeIndex));
  }

  m_beginIt = m_strokeList.begin();
  m_endIt   = m_strokeList.end();
}

//-----------------------------------------------------------------------------

ToolUtils::UndoModifyListStroke::~UndoModifyListStroke() {
  clearPointerContainer(m_strokeList);
  delete m_fillInformation;
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyListStroke::onAdd() {
  std::list<UndoModifyStroke *>::iterator it = m_beginIt;
  TRectD newBBox;

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(!!image);
  if (!image) return;

  for (; it != m_endIt; ++it) {
    TStroke *s = image->getStroke((*it)->m_strokeIndex);
    (*it)->onAdd();
  }
  m_fillInformation = new std::vector<TFilledRegionInf>;

  if (m_beginIt != m_endIt)
    ImageUtils::getFillingInformationOverlappingArea(image, *m_fillInformation,
                                                     m_oldBBox, newBBox);
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyListStroke::undo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  std::list<UndoModifyStroke *>::iterator stroke_it = m_beginIt;
  if (m_beginIt == m_endIt) return;

  for (; stroke_it != m_endIt; ++stroke_it) {
    (*stroke_it)->undo();
  }

  UINT size = m_fillInformation->size();
  if (!size) {
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentTool()->getTool()->notifyImageChanged();
    return;
  }

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(!!image);
  if (!image) return;

  image->findRegions();

  TRegion *reg;
  for (UINT i = 0; i < size; i++) {
    reg = image->getRegion((*m_fillInformation)[i].m_regionId);
    assert(reg);
    if (reg) reg->setStyle((*m_fillInformation)[i].m_styleId);
  }
  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoModifyListStroke::redo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  std::list<UndoModifyStroke *>::iterator it = m_beginIt;

  for (; it != m_endIt; ++it) {
    (*it)->redo();
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

int ToolUtils::UndoModifyListStroke::getSize() const {
  int sum = 0;

  std::list<UndoModifyStroke *>::iterator it = m_beginIt;

  for (; it != m_endIt; ++it) {
    sum += (*it)->getSize();
  }

  if (m_fillInformation)
    sum += m_fillInformation->capacity() * sizeof(TFilledRegionInf);

  return sum;
}

//=============================================================================================

ToolUtils::UndoPencil::UndoPencil(
    TStroke *stroke, std::vector<TFilledRegionInf> *fillInformation,
    TXshSimpleLevel *level, const TFrameId &frameId, bool createdFrame,
    bool createdLevel, bool autogroup, bool autofill)
    : TToolUndo(level, frameId, createdFrame, createdLevel, 0)
    , m_strokeId(stroke->getId())
    , m_fillInformation(fillInformation)
    , m_autogroup(autogroup)
    , m_autofill(autofill) {
  m_stroke = new TStroke(*stroke);
}

//-----------------------------------------------------------------------------

ToolUtils::UndoPencil::~UndoPencil() {
  delete m_fillInformation;
  delete m_stroke;
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoPencil::undo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  /*if(app->getCurrentFrame()->isEditingScene() && m_col!=-2 && m_row!=-1)
  {
          app->getCurrentColumn()->setColumnIndex(m_col);
          app->getCurrentFrame()->setFrame(m_row);
  }
  else
          app->getCurrentFrame()->setFid(m_frameId);*/

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;

  QMutexLocker sl(image->getMutex());
  VIStroke *stroke = image->getStrokeById(m_strokeId);
  if (!stroke) return;
  image->deleteStroke(stroke);
  TSelection *selection            = app->getCurrentSelection()->getSelection();
  StrokeSelection *strokeSelection = dynamic_cast<StrokeSelection *>(selection);
  if (strokeSelection) strokeSelection->selectNone();

  UINT size = m_fillInformation->size();
  TRegion *reg;
  for (UINT i = 0; i < size; i++) {
    reg = image->getRegion((*m_fillInformation)[i].m_regionId);
    assert(reg);
    if (reg) reg->setStyle((*m_fillInformation)[i].m_styleId);
  }

  removeLevelAndFrameIfNeeded();

  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ToolUtils::UndoPencil::redo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  insertLevelAndFrameIfNeeded();

  /*if(app->getCurrentFrame()->isEditingScene())
  {
          app->getCurrentColumn()->setColumnIndex(m_col);
          app->getCurrentFrame()->setFrame(m_row);
  }
  else
          app->getCurrentFrame()->setFid(m_frameId);*/

  TVectorImageP image = m_level->getFrame(m_frameId, true);
  if (!image) return;

  QMutexLocker sl(image->getMutex());
  TStroke *stroke = new TStroke(*m_stroke);
  stroke->setId(m_strokeId);
  image->addStroke(stroke);
  if (image->isComputedRegionAlmostOnce()) image->findRegions();

  if (m_autogroup && stroke->isSelfLoop()) {
    int index = image->getStrokeCount() - 1;
    image->group(index, 1);
    if (m_autofill) {
      // to avoid filling other strokes, I enter into the new stroke group
      int currentGroup = image->exitGroup();
      image->enterGroup(index);
      image->selectFill(stroke->getBBox().enlarge(1, 1), 0, stroke->getStyle(),
                        false, true, false);
      if (currentGroup != -1)
        image->enterGroup(currentGroup);
      else
        image->exitGroup();
    }
  }
  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

int ToolUtils::UndoPencil::getSize() const {
  return sizeof(*this) +
         m_fillInformation->capacity() * sizeof(TFilledRegionInf) + 500;
}

//=============================================================================================

ToolUtils::UndoRasterPencil::UndoRasterPencil(
    TXshSimpleLevel *level, const TFrameId &frameId, TStroke *stroke,
    bool selective, bool filled, bool doAntialias, bool createdFrame,
    bool createdLevel, std::string primitiveName)
    : TRasterUndo(0, level, frameId, createdFrame, createdLevel, 0)
    , m_selective(selective)
    , m_filled(filled)
    , m_doAntialias(doAntialias)
    , m_primitiveName(primitiveName) {
  TRasterCM32P raster = getImage()->getRaster();
  TDimension d        = raster->getSize();
  m_tiles             = new TTileSetCM32(d);
  TRect rect =
      convert(stroke->getBBox()) + TPoint((int)(d.lx * 0.5), (int)(d.ly * 0.5));
  m_tiles->add(raster, rect.enlarge(2));
  m_stroke = new TStroke(*stroke);
}

//-----------------------------------------------------------------------------

ToolUtils::UndoRasterPencil::~UndoRasterPencil() { delete m_stroke; }

//-----------------------------------------------------------------------------

void ToolUtils::UndoRasterPencil::redo() const {
  insertLevelAndFrameIfNeeded();
  TToonzImageP image = getImage();
  if (!image) return;

  ToonzImageUtils::addInkStroke(image, m_stroke, m_stroke->getStyle(),
                                m_selective, m_filled, TConsts::infiniteRectD,
                                m_doAntialias);
  ToolUtils::updateSaveBox();
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

int ToolUtils::UndoRasterPencil::getSize() const {
  return TRasterUndo::getSize() +
         m_stroke->getControlPointCount() * sizeof(TThickPoint) + 100 +
         sizeof(this);
}

//=============================================================================================

ToolUtils::UndoFullColorPencil::UndoFullColorPencil(
    TXshSimpleLevel *level, const TFrameId &frameId, TStroke *stroke,
    double opacity, bool doAntialias, bool createdFrame, bool createdLevel)
    : TFullColorRasterUndo(0, level, frameId, createdFrame, createdLevel, 0)
    , m_opacity(opacity)
    , m_doAntialias(doAntialias) {
  TRasterP raster = getImage()->getRaster();
  TDimension d    = raster->getSize();
  m_tiles         = new TTileSetFullColor(d);
  TRect rect =
      convert(stroke->getBBox()) + TPoint((int)(d.lx * 0.5), (int)(d.ly * 0.5));
  m_tiles->add(raster, rect.enlarge(2));
  m_stroke = new TStroke(*stroke);
}

//-----------------------------------------------------------------------------

ToolUtils::UndoFullColorPencil::~UndoFullColorPencil() { delete m_stroke; }

//-----------------------------------------------------------------------------

void ToolUtils::UndoFullColorPencil::redo() const {
  insertLevelAndFrameIfNeeded();
  TRasterImageP image = getImage();
  if (!image) return;
  TRasterImageUtils::addStroke(image, m_stroke, TRectD(), m_opacity,
                               m_doAntialias);
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

int ToolUtils::UndoFullColorPencil::getSize() const {
  return TFullColorRasterUndo::getSize() +
         m_stroke->getControlPointCount() * sizeof(TThickPoint) + 100 +
         sizeof(this);
}

//=============================================================================================
//
// undo class (path strokes). call it BEFORE and register it AFTER path change
//
ToolUtils::UndoPath::UndoPath(TStageObjectSpline *spline) : m_spline(spline) {
  assert(!!m_spline);

  const TStroke *stroke = m_spline->getStroke();
  assert(stroke);
  int n = stroke->getControlPointCount();
  for (int i = 0; i < n; i++) m_before.push_back(stroke->getControlPoint(i));
  m_selfLoopBefore = stroke->isSelfLoop();
}

ToolUtils::UndoPath::~UndoPath() {}

void ToolUtils::UndoPath::onAdd() {
  assert(!!m_spline);

  const TStroke *stroke = m_spline->getStroke();
  assert(stroke);
  int n = stroke->getControlPointCount();
  for (int i = 0; i < n; i++) m_after.push_back(stroke->getControlPoint(i));
}

void ToolUtils::UndoPath::undo() const {
  assert(!!m_spline);

  TTool::Application *app = TTool::getApplication();
  TSelection *selection   = app->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();

  TStroke *stroke = new TStroke(*m_spline->getStroke());
  stroke->reshape(&m_before[0], m_before.size());
  stroke->setSelfLoop(m_selfLoopBefore);
  m_spline->setStroke(stroke);

  if (!app->getCurrentObject()->isSpline()) return;

  TStageObjectId currentObjectId = app->getCurrentObject()->getObjectId();
  TStageObject *stageObject =
      app->getCurrentXsheet()->getXsheet()->getStageObject(currentObjectId);
  if (stageObject->getSpline()->getId() == m_spline->getId())
    app->getCurrentObject()->setSplineObject(m_spline);

  app->getCurrentTool()->getTool()->notifyImageChanged();
}

void ToolUtils::UndoPath::redo() const {
  assert(!!m_spline);

  TTool::Application *app = TTool::getApplication();
  TSelection *selection   = app->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();

  TStroke *stroke = new TStroke(*m_spline->getStroke());
  stroke->reshape(&m_after[0], m_after.size());
  stroke->setSelfLoop(m_selfLoopBefore);
  m_spline->setStroke(stroke);

  if (!app->getCurrentObject()->isSpline()) return;

  TStageObjectId currentObjectId = app->getCurrentObject()->getObjectId();
  TStageObject *stageObject =
      app->getCurrentXsheet()->getXsheet()->getStageObject(currentObjectId);
  if (stageObject->getSpline()->getId() == m_spline->getId())
    app->getCurrentObject()->setSplineObject(m_spline);

  app->getCurrentTool()->getTool()->notifyImageChanged();
}

int ToolUtils::UndoPath::getSize() const { return sizeof(*this) + 500; }

//=============================================================================================
//
// UndoControlPointEditor
//

ToolUtils::UndoControlPointEditor::UndoControlPointEditor(
    TXshSimpleLevel *level, const TFrameId &frameId)
    : TToolUndo(level, frameId), m_isStrokeDelete(false) {
  TVectorImageP image = level->getFrame(frameId, true);
  assert(image);
  if (!image) return;

  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  m_row    = app->getCurrentFrame()->getFrame();
  m_column = app->getCurrentColumn()->getColumnIndex();
}

ToolUtils::UndoControlPointEditor::~UndoControlPointEditor() {
  deleteVIStroke(m_oldStroke.second);
  if (!m_isStrokeDelete) deleteVIStroke(m_newStroke.second);
}

void ToolUtils::UndoControlPointEditor::onAdd() {
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;
  QMutexLocker lock(image->getMutex());
  if (m_isStrokeDelete) return;
  addNewStroke(m_oldStroke.first, image->getVIStroke(m_oldStroke.first));
}

void ToolUtils::UndoControlPointEditor::addOldStroke(int index, VIStroke *vs) {
  VIStroke *s = cloneVIStroke(vs);
  m_oldStroke = std::make_pair(index, s);
}

void ToolUtils::UndoControlPointEditor::addNewStroke(int index, VIStroke *vs) {
  VIStroke *s = cloneVIStroke(vs);
  m_newStroke = std::make_pair(index, s);
}

void ToolUtils::UndoControlPointEditor::undo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentFrame()->isEditingScene()) {
    app->getCurrentColumn()->setColumnIndex(m_column);
    app->getCurrentFrame()->setFrame(m_row);
  } else
    app->getCurrentFrame()->setFid(m_frameId);

  TSelection *selection = app->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;
  QMutexLocker lock(image->getMutex());

  if (!m_isStrokeDelete) image->removeStroke(m_newStroke.first, false);

  UINT i      = 0;
  VIStroke *s = cloneVIStroke(m_oldStroke.second);
  image->insertStrokeAt(s, m_oldStroke.first);

  if (image->isComputedRegionAlmostOnce())
    image->findRegions();  // in futuro togliere. Serve perche' la
                           // removeStrokes, se gli si dice
  // di non calcolare le regioni, e' piu' veloce ma poi chrash tutto

  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

void ToolUtils::UndoControlPointEditor::redo() const {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentFrame()->isEditingScene()) {
    app->getCurrentColumn()->setColumnIndex(m_column);
    app->getCurrentFrame()->setFrame(m_row);
  } else
    app->getCurrentFrame()->setFid(m_frameId);
  TSelection *selection = app->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(image);
  if (!image) return;
  QMutexLocker lock(image->getMutex());

  image->removeStroke(m_oldStroke.first, false);

  if (!m_isStrokeDelete) {
    VIStroke *s = cloneVIStroke(m_newStroke.second);
    image->insertStrokeAt(s, m_newStroke.first);
  }

  if (image->isComputedRegionAlmostOnce())
    image->findRegions();  // in futuro togliere. Serve perche' la
                           // removeStrokes, se gli si dice
  // di non calcolare le regioni, e' piu' veloce ma poi chrash tutto

  app->getCurrentXsheet()->notifyXsheetChanged();
  notifyImageChanged();
}

//=============================================================================================
//
// Menu
//

ToolUtils::DragMenu::DragMenu() : QMenu() {}

//-----------------------------------------------------------------------------

QAction *ToolUtils::DragMenu::exec(const QPoint &p, QAction *action) {
  QAction *execAct = QMenu::exec(p, action);
  if (execAct) return execAct;

  return defaultAction();
}

//---------------------------------------------------------------------------------------------

void ToolUtils::DragMenu::mouseReleaseEvent(QMouseEvent *e) {
  QMenu::mouseReleaseEvent(e);
  hide();
}

//=============================================================================================
//
// ColumChooserMenu
//

ToolUtils::ColumChooserMenu::ColumChooserMenu(
    TXsheet *xsh, const std::vector<int> &columnIndexes)
    : DragMenu() {
  int size = columnIndexes.size();
  int i;
  for (i = size - 1; i >= 0; i--) {
    int index                 = columnIndexes[i];
    TStageObjectId id         = TStageObjectId::ColumnId(index);
    TStageObject *stageObject = xsh->getStageObject(id);
    QString cmdStr = "Column " + QString::fromStdString(stageObject->getName());
    QAction *act   = new QAction(cmdStr, this);
    act->setData(index);
    addAction(act);
    if (size - 1 == i) {
      setDefaultAction(act);
      setActiveAction(act);
    }
  }
}

//---------------------------------------------------------------------------------------------

int ToolUtils::ColumChooserMenu::execute() {
  QAction *executeAct = exec(QCursor::pos());
  return (!executeAct) ? -1 : executeAct->data().toInt();
}

//---------------------------------------------------------------------------------------------

double ToolUtils::ConeSubVolume::compute(double cover) {
  double x = (10 * tcrop(cover, -1.0, 1.0)) + 10;
  assert(0 <= x && x <= 20);
  int i = tfloor(x);
  if (i == 20)
    return m_values[i];
  else
    // Interpolazione lineare.
    return (-(x - (i + 1)) * m_values[i]) - (-(x - i) * m_values[i + 1]);
}

const double ToolUtils::ConeSubVolume::m_values[] = {
    1.0,      0.99778,  0.987779,  0.967282,  0.934874,  0.889929,   0.832457,
    0.763067, 0.683002, 0.594266,  0.5,       0.405734,  0.316998,   0.236933,
    0.167543, 0.110071, 0.0651259, 0.0327182, 0.0122208, 0.00221986, 0.0};

//---------------------------------------------------------------------------------------------

void ToolUtils::drawBalloon(const TPointD &pos, std::string text,
                            const TPixel32 &color, TPoint delta,
                            double pixelSize, bool isPicking,
                            std::vector<TRectD> *otherBalloons) {
  QString qText = QString::fromStdString(text);
  QFont font("Arial");  // ,QFont::Bold);
  font.setPixelSize(13);
  QFontMetrics fm(font);
  QRect textRect = fm.boundingRect(qText);

  int baseLine = -textRect.top();
  int mrg      = 3;

  // avoid other balloons
  if (otherBalloons) {
    std::vector<TRectD> &balloons = *otherBalloons;
    int n                         = (int)balloons.size();
    TDimensionD balloonSize(pixelSize * (textRect.width() + mrg * 2),
                            pixelSize * (textRect.height() + mrg * 2));
    TRectD balloonRect;
    for (;;) {
      balloonRect = TRectD(
          pos + TPointD(delta.x * pixelSize, delta.y * pixelSize), balloonSize);
      int i = 0;
      while (i < n && !balloons[i].overlaps(balloonRect)) i++;
      if (i == n) break;
      double y = balloons[i].y0 - balloonSize.ly - 2 * mrg * pixelSize;
      delta.y  = (y - pos.y) / pixelSize;
    }
    balloons.push_back(balloonRect);
  }

  textRect.moveTo(qMax(delta.x, 10 + mrg), qMax(mrg + 2, -delta.y - baseLine));

  int y  = textRect.top() + baseLine;
  int x0 = textRect.left() - mrg;
  int x1 = textRect.right() + mrg;
  int y0 = textRect.top() - mrg;
  int y1 = textRect.bottom() + mrg;

  if (isPicking) {
    TTool::Viewer *viewer =
        TTool::getApplication()->getCurrentTool()->getTool()->getViewer();

    if (viewer->is3DView()) {
      double x0 = pos.x + textRect.left() * pixelSize,
             y0 = pos.y + delta.y * pixelSize;
      double x1 = x0 + pixelSize * textRect.width();
      double y1 = y0 + pixelSize * textRect.height();
      double d  = pixelSize * 5;
      glRectd(x0 - d, y0 - d, x1 + d, y1 + d);
    } else {
      TPoint posBalloon = viewer->worldToPos(pos);

      double d  = 5;
      double x0 = posBalloon.x + textRect.left() - d;
      double y0 = posBalloon.y + delta.y - d;
      double x1 = x0 + textRect.width() + d;
      double y1 = y0 + textRect.height() + d;

      TPoint p1(x0, y0);
      TPoint p2(x1, y0);
      TPoint p3(x0, y1);
      TPoint p4(x1, y1);

      TPointD w1(viewer->winToWorld(p1));
      TPointD w2(viewer->winToWorld(p2));
      TPointD w3(viewer->winToWorld(p3));
      TPointD w4(viewer->winToWorld(p4));

      glBegin(GL_QUADS);
      glVertex2d(w1.x, w1.y);
      glVertex2d(w2.x, w2.y);
      glVertex2d(w4.x, w4.y);
      glVertex2d(w3.x, w3.y);
      glEnd();
    }

    return;
  }

  QSize size(textRect.width() + textRect.left() + mrg,
             qMax(textRect.bottom() + mrg, y + delta.y) + 3);

  QImage label(size.width(), size.height(), QImage::Format_ARGB32);
  label.fill(Qt::transparent);
  // label.fill(qRgba(200,200,0,200));
  QPainter p(&label);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
  p.setBrush(QColor(color.r, color.g, color.b, color.m));
  p.setPen(Qt::NoPen);

  QPainterPath pp;
  pp.moveTo(x0, y - 8);
  pp.lineTo(0, y + delta.y);
  pp.lineTo(x0, y);
  /* bordi arrotondati
int arcSize = 10;
pp.arcTo(x0,y1-arcSize,arcSize,arcSize,180,90);
pp.arcTo(x1-arcSize,y1-arcSize,arcSize,arcSize,270,90);
pp.arcTo(x1-arcSize,y0,arcSize,arcSize,0,90);
pp.arcTo(x0,y0,arcSize,arcSize,90,90);
*/
  // bordi acuti
  pp.lineTo(x0, y1);
  pp.lineTo(x1, y1);
  pp.lineTo(x1, y0);
  pp.lineTo(x0, y0);

  pp.closeSubpath();

  p.drawPath(pp);

  p.setPen(Qt::black);
  p.setFont(font);
  p.drawText(textRect, Qt::AlignCenter | Qt::TextDontClip, qText);

  QImage texture = QGLWidget::convertToGLFormat(label);

  glRasterPos2f(pos.x, pos.y);
  glBitmap(0, 0, 0, 0, 0, -size.height() + (y + delta.y), NULL);  //
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDrawPixels(texture.width(), texture.height(), GL_RGBA, GL_UNSIGNED_BYTE,
               texture.bits());
  glDisable(GL_BLEND);
  glColor3d(0, 0, 0);
}

//---------------------------------------------------------------------------------------------

void ToolUtils::drawHook(const TPointD &pos, ToolUtils::HookType type,
                         bool highlighted, bool onionSkin) {
  int r = 10, d = r + r;
  QImage image(d, d, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  // image.fill(qRgba(200,200,0,200));
  QPainter painter(&image);
  // painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);

  int matte = onionSkin ? 100 : 255;

  QColor color(0, 0, 0, matte);
  if (highlighted) color = QColor(0, 175, 175, matte);

  if (type == NormalHook || type == PassHookA) {
    painter.setPen(QPen(QColor(255, 255, 255, matte), 3));
    painter.drawEllipse(5, 5, d - 10, d - 10);
    painter.setPen(color);
    painter.drawEllipse(5, 5, d - 10, d - 10);
  } else if (type == OtherLevelHook) {
    QColor color(0, 200, 200, 200);
    // painter.setPen(QPen(Qt::white,3));
    // painter.drawEllipse(5,5,d-10,d-10);
    painter.setPen(Qt::white);
    painter.setBrush(color);
    painter.drawEllipse(6, 6, d - 12, d - 12);
  }
  if (type == NormalHook || type == PassHookB) {
    painter.setPen(QPen(QColor(255, 255, 255, matte), 3));
    painter.drawLine(0, r, d, r);
    painter.drawLine(r, 0, r, d);
    painter.setPen(color);
    painter.drawLine(0, r, d, r);
    painter.drawLine(r, 0, r, d);
  }

  QImage texture = QGLWidget::convertToGLFormat(image);
  glRasterPos2f(pos.x, pos.y);
  glBitmap(0, 0, 0, 0, -r, -r, NULL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDrawPixels(texture.width(), texture.height(), GL_RGBA, GL_UNSIGNED_BYTE,
               texture.bits());
  glDisable(GL_BLEND);
}

//---------------------------------------------------------------------------------------------

bool ToolUtils::isJustCreatedSpline(TImage *image) {
  TVectorImageP vi = image;
  if (!vi) return false;
  if (vi->getStrokeCount() != 1) return false;
  TStroke *stroke = vi->getStroke(0);
  if (stroke->getControlPointCount() != 3) return false;
  TPointD p0 = stroke->getControlPoint(0);
  TPointD p1 = stroke->getControlPoint(1);
  TPointD p2 = stroke->getControlPoint(2);
  double d   = 30.0;
  return p0 == TPointD(-d, 0) && p1 == TPointD(0, 0) && p2 == TPointD(d, 0);
}

//---------------------------------------------------------------------------------------------

TRectD ToolUtils::interpolateRect(const TRectD &rect1, const TRectD &rect2,
                                  double t) {
  assert(rect1.x0 <= rect1.x1);
  assert(rect1.y0 <= rect1.y1);
  assert(rect2.x0 <= rect2.x1);
  assert(rect2.y0 <= rect2.y1);

  return TRectD(rect1.x0 + (rect2.x0 - rect1.x0) * t,
                rect1.y0 + (rect2.y0 - rect1.y0) * t,
                rect1.x1 + (rect2.x1 - rect1.x1) * t,
                rect1.y1 + (rect2.y1 - rect1.y1) * t);
}

//-----------------------------------------------------------------------------
/*
bool ToolUtils::isASubRegion(int reg, const std::vector<TRegion*> &regions)
{
        TRegion *region=regions[reg];
        for (int i=0; i<(int)regions.size(); i++)
        {
                if(i!=reg)
                        if(region->isSubRegionOf(*regions[i]))
                                return true;
        }
        return false;
}
*/
//-----------------------------------------------------------------------------

TRectD ToolUtils::getBounds(const std::vector<TThickPoint> &points,
                            double maxThickness) {
  TThickPoint p = points[0];
  double radius = maxThickness == 0 ? p.thick * 0.5 : maxThickness * 0.5;
  TRectD rect(p - TPointD(radius, radius), p + TPointD(radius, radius));
  int i;
  for (i = 1; i < (int)points.size(); i++) {
    p      = points[i];
    radius = maxThickness == 0 ? p.thick * 0.5 : maxThickness * 0.5;
    rect =
        rect + TRectD(p - TPointD(radius, radius), p + TPointD(radius, radius));
  }
  return rect;
}

//-----------------------------------------------------------------------------
/*
template <typename PIXEL>
TRasterPT<PIXEL> ToolUtils::rotate90(const TRasterPT<PIXEL> &ras, bool toRight)
{
        if(!ras)
                return 0;
        int lx=ras->getLy();
        int ly=ras->getLx();
        TRasterPT<PIXEL> workRas(lx,ly);
        for (int i=0; i<ras->getLx(); i++)
        {
                for (int j=0; j<ras->getLy(); j++)
                {
                        if(toRight)
                                workRas->pixels(ly-1-i)[j]=ras->pixels(j)[i];
                        else
                                workRas->pixels(i)[lx-1-j]=ras->pixels(j)[i];
                }
        }
        return workRas;
}*/
