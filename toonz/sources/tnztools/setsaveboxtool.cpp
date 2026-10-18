

#include "setsaveboxtool.h"
#include "tgl.h"
#include "tundo.h"
#include "trop.h"
#include "toonz/toonzimageutils.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/ttileset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"

using namespace ToolUtils;

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class SetSaveboxUndo final : public TRasterUndo {
  TRect m_modifiedSavebox;
  TRect m_originalSavebox;

public:
  SetSaveboxUndo(TTileSetCM32 *tileSet, const TRect &modifiedSavebox,
                 const TRect &originalSavebox, TXshSimpleLevel *level,
                 const TFrameId &frameId)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_modifiedSavebox(modifiedSavebox)
      , m_originalSavebox(originalSavebox) {}

  void redo() const override {
    TToonzImageP ti = getImage();
    if (!ti) return;
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    ti->getRaster()->clearOutside(m_modifiedSavebox);
    ti->setSavebox(m_modifiedSavebox);
    notifyImageChanged();
  }

  void undo() const override {
    TRasterUndo::undo();
    TToonzImageP ti = getImage();
    if (!ti) return;
    ti->setSavebox(m_originalSavebox);
  }

  int getSize() const override {
    return TRasterUndo::getSize() + sizeof(this) + 100;
  }

  ~SetSaveboxUndo() {}

  QString getHistoryString() override {
    return QObject::tr("Set Save Box : (X%1,Y%2,W%3,H%4)->(X%5,Y%6,W%7,H%8)")
        .arg(QString::number(m_originalSavebox.x0))
        .arg(QString::number(m_originalSavebox.y0))
        .arg(QString::number(m_originalSavebox.getLx()))
        .arg(QString::number(m_originalSavebox.getLy()))
        .arg(QString::number(m_modifiedSavebox.x0))
        .arg(QString::number(m_modifiedSavebox.y0))
        .arg(QString::number(m_modifiedSavebox.getLx()))
        .arg(QString::number(m_modifiedSavebox.getLy()));
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// SetSaveboxTool
//-----------------------------------------------------------------------------

SetSaveboxTool::SetSaveboxTool(TTool *tool)
    : m_tool(tool), m_pos(), m_modifiedRect() {}

//-----------------------------------------------------------------------------

TToonzImage *SetSaveboxTool::getImage() {
  TImageP image = m_tool->getImage(true);
  if (!image) return 0;
  TToonzImageP ti = TToonzImageP(image);
  if (!ti) return 0;
  return ti.getPointer();
}

//-----------------------------------------------------------------------------

int SetSaveboxTool::getDragType(const TPointD &pos) {
  TToonzImageP image = getImage();
  if (!image) return eNone;
  TRectD bbox =
      ToonzImageUtils::convertRasterToWorld(convert(image->getBBox()), image);

  int ret = 0;
  int dx  = std::min(fabs(bbox.x0 - pos.x), fabs(bbox.x1 - pos.x));
  int dy  = std::min(fabs(bbox.y0 - pos.y), fabs(bbox.y1 - pos.y));

  double maxDist = 5 * m_tool->getPixelSize();
  if (dx > maxDist && dy > maxDist)
    return (bbox.contains(pos)) ? eMoveRect : eNone;

  if (dx <= maxDist && pos.y >= bbox.y0 - maxDist &&
      pos.y <= bbox.y1 + maxDist) {
    if (areAlmostEqual(dx, fabs(bbox.x0 - pos.x), maxDist))
      ret = eMoveLeft;
    else if (areAlmostEqual(dx, fabs(bbox.x1 - pos.x), maxDist))
      ret = eMoveRight;
  } else if (dy <= maxDist && pos.x >= bbox.x0 - maxDist &&
             pos.x <= bbox.x1 + maxDist) {
    if (areAlmostEqual(dy, fabs(bbox.y0 - pos.y), maxDist))
      return eMoveDown;
    else
      return eMoveUp;
  } else
    return eNone;

  if (dy > maxDist) return ret;

  return (areAlmostEqual(dy, fabs(bbox.y0 - pos.y), maxDist)) ? ret | eMoveDown
                                                              : ret | eMoveUp;
}

//-----------------------------------------------------------------------------

int SetSaveboxTool::getCursorId(const TPointD &pos) {
  int dragType = getDragType(pos);
  switch (dragType) {
  case eMoveRect:
    return ToolCursor::MoveCursor;
  case eMoveLeft:
  case eMoveRight:
    return ToolCursor::ScaleHCursor;
  case eMoveDown:
  case eMoveUp:
    return ToolCursor::ScaleVCursor;
  case eMoveLeft | eMoveUp:
  case eMoveRight | eMoveDown:
    return ToolCursor::ScaleInvCursor;
  case eMoveLeft | eMoveDown:
  case eMoveRight | eMoveUp:
    return ToolCursor::ScaleCursor;
  default:
    return ToolCursor::StrokeSelectCursor;
  }
  return ToolCursor::StrokeSelectCursor;
}

//-----------------------------------------------------------------------------

void SetSaveboxTool::leftButtonDown(const TPointD &pos) {
  TToonzImageP image = getImage();
  if (!image) return;
  m_modifiedRect =
      ToonzImageUtils::convertRasterToWorld(convert(image->getBBox()), image);
  m_pos          = pos;
  m_movementType = (SetSaveboxTool::Moviment)getDragType(pos);
}

//-----------------------------------------------------------------------------

void SetSaveboxTool::leftButtonDrag(const TPointD &pos) {
  TToonzImageP image = getImage();
  if (!image) return;

  if (m_movementType == eNone) return;
  if (m_movementType == eMoveRect)
    m_modifiedRect = m_modifiedRect + (pos - m_pos);
  else {
    if (m_movementType & eMoveLeft) m_modifiedRect.x0 += pos.x - m_pos.x;
    if (m_movementType & eMoveRight) m_modifiedRect.x1 += pos.x - m_pos.x;
    if (m_movementType & eMoveDown) m_modifiedRect.y0 += pos.y - m_pos.y;
    if (m_movementType & eMoveUp) m_modifiedRect.y1 += pos.y - m_pos.y;
  }
  m_pos = pos;
}

//-----------------------------------------------------------------------------

void SetSaveboxTool::leftButtonUp(const TPointD &pos) {
  TToonzImageP image = getImage();
  if (!image) return;
  m_pos                  = TPointD();
  TRectD originalSavebox = image->getBBox();
  TRect savebox = ToonzImageUtils::convertWorldToRaster(m_modifiedRect, image);
  if (savebox.isEmpty()) {
    m_modifiedRect = TRectD();
    return;
  }
  TRasterCM32P ras      = image->getRaster();
  TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
  tileSet->add(ras, image->getSavebox());
  /*-- 以下を有効にすると、SaveBoxの外の絵のデータが失われる --*/
  // ras->clearOutside(savebox);
  // if(Preferences::instance()->isMinimizeSaveboxAfterEditing())
  //	TRop::computeBBox(ras, savebox);
  image->setSavebox(savebox);

  TXshSimpleLevel *level =
      m_tool->getApplication()->getCurrentLevel()->getSimpleLevel();
  TUndoManager::manager()->add(
      new SetSaveboxUndo(tileSet, savebox, convert(originalSavebox), level,
                         m_tool->getCurrentFid()));
  m_modifiedRect = TRectD();
  m_tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void SetSaveboxTool::draw() {
  TToonzImageP image = getImage();
  if (!image) return;

  TRectD bbox;
  if (m_modifiedRect == TRectD())
    bbox =
        ToonzImageUtils::convertRasterToWorld(convert(image->getBBox()), image);
  else
    bbox = m_modifiedRect;

  bbox *= image->getSubsampling();
  drawRect(bbox, TPixel32(210, 210, 210), 0xF0F0, true);

  double pixelSize = m_tool->getPixelSize();
  TPointD p00      = bbox.getP00();
  TPointD p11      = bbox.getP11();
  TPointD p01      = bbox.getP01();
  TPointD p10      = bbox.getP10();
  TPointD p11_01   = (bbox.getP11() + bbox.getP01()) * 0.5;
  TPointD p11_10   = (bbox.getP11() + bbox.getP10()) * 0.5;
  TPointD p00_01   = (bbox.getP00() + bbox.getP01()) * 0.5;
  TPointD p00_10   = (bbox.getP00() + bbox.getP10()) * 0.5;
  TPixel32 frameColor(210, 210, 210);
  TPixel32 shadowColor(0, 0, 0);
  auto drawHandle = [&](const TPointD &point) {
    drawSquare(point + TPointD(-pixelSize, pixelSize), pixelSize * 4,
               frameColor);
    drawSquare(point, pixelSize * 4, shadowColor);
  };

  drawHandle(p00);
  drawHandle(p11);
  drawHandle(p01);
  drawHandle(p10);
  drawHandle(p11_01);
  drawHandle(p11_10);
  drawHandle(p00_01);
  drawHandle(p00_10);
}
