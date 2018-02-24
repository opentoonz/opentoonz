

#include "rgbpickertool.h"

#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tools/cursors.h"
#include "tcolorstyles.h"
#include "toonz/cleanupcolorstyles.h"
#include "trasterimage.h"
#include "tgl.h"
#include "tenv.h"
#include "tstroke.h"

#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/stage2.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"

#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lutcalibrator.h"

#include "tools/toolhandle.h"
#include "tools/stylepicker.h"
#include "tools/toolutils.h"
#include "tools/RGBpicker.h"

#define NORMAL_PICK L"Normal"
#define RECT_PICK L"Rectangular"
#define FREEHAND_PICK L"Freehand"
#define POLYLINE_PICK L"Polyline"

TEnv::StringVar PickVectorType("InknpaintPickVectorType", "Normal");
TEnv::IntVar PickPassive("InknpaintPickPassive", 0);

namespace RGBPicker {

//============================================================
// Pick RGB Tool
//------------------------------------------------------------

class UndoPickRGBM final : public TUndo {
  TPaletteP m_palette;
  int m_styleId;
  int m_styleParamIndex;
  TPixel32 m_oldValue, m_newValue;
  TXshSimpleLevelP m_level;
  bool m_colorAutoApplyEnabled;

public:
  UndoPickRGBM(TPalette *palette, int styleId, const TPixel32 newValue,
               const TXshSimpleLevelP &level)
      : m_palette(palette)
      , m_styleId(styleId)
      , m_newValue(newValue)
      , m_level(level)
      , m_colorAutoApplyEnabled(true) {
    PaletteController *controller =
        TTool::getApplication()->getPaletteController();
    m_colorAutoApplyEnabled = controller->isColorAutoApplyEnabled();
    m_styleParamIndex = controller->getCurrentPalette()->getStyleParamIndex();
    if (m_colorAutoApplyEnabled) {
      TColorStyle *cs = m_palette->getStyle(m_styleId);
      if (0 <= m_styleParamIndex &&
          m_styleParamIndex < cs->getColorParamCount())
        m_oldValue = cs->getColorParamValue(m_styleParamIndex);
      else
        m_oldValue = cs->getMainColor();
    } else {
      m_oldValue = controller->getColorSample();
    }
  }

  void setColor(const TPixel32 &color) const {
    PaletteController *controller =
        TTool::getApplication()->getPaletteController();
    if (m_colorAutoApplyEnabled) {
      TColorStyle *cs = m_palette->getStyle(m_styleId);
      if (0 <= m_styleParamIndex &&
          m_styleParamIndex < cs->getColorParamCount())
        cs->setColorParamValue(m_styleParamIndex, color);
      else
        cs->setMainColor(color);
      cs->invalidateIcon();
      controller->getCurrentPalette()->notifyColorStyleChanged();
      updateLevel();
    } else {
      controller->setColorSample(color);
    }
  }

  void undo() const override { setColor(m_oldValue); }

  void redo() const override { setColor(m_newValue); }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("RGB Picker (R%1, G%2, B%3)")
        .arg(QString::number((int)m_newValue.r))
        .arg(QString::number((int)m_newValue.g))
        .arg(QString::number((int)m_newValue.b));
  }

private:
  void updateLevel() const {
    std::vector<TFrameId> fids;
    if (!m_level) return;
    m_level->getFids(fids);
    unsigned int i;
    for (i = 0; i < fids.size(); i++)
      IconGenerator::instance()->invalidate(m_level.getPointer(), fids[i]);
    IconGenerator::instance()->invalidateSceneIcon();
    TTool::getApplication()->getCurrentScene()->notifySceneChanged();
  }
};

//----------------------------------------------------------------------------------------------

void setCurrentColor(const TPixel32 &color) {
  PaletteController *controller =
      TTool::getApplication()->getPaletteController();
  TPaletteHandle *ph = controller->getCurrentPalette();

  TColorStyle *cs = ph->getStyle();
  if (!cs) return;

  if (controller->isColorAutoApplyEnabled()) {
    TCleanupStyle *ccs = dynamic_cast<TCleanupStyle *>(cs);
    if (ccs) ccs->setCanUpdate(true);

    int index = ph->getStyleParamIndex();
    if (0 <= index && index < cs->getColorParamCount())
      cs->setColorParamValue(index, color);
    else
      cs->setMainColor(color);

    cs->invalidateIcon();
    ph->notifyColorStyleChanged();

    // per le palette animate
    int styleIndex    = ph->getStyleIndex();
    TPalette *palette = ph->getPalette();
    if (palette && palette->isKeyframe(styleIndex, palette->getFrame()))
      palette->setKeyframe(styleIndex, palette->getFrame());

    if (ccs) ccs->setCanUpdate(false);
  } else
    controller->setColorSample(color);
}

//----------------------------------------------------------------------------------------------

void setCurrentColorWithUndo(const TPixel32 &color) {
  TTool::Application *app = TTool::getApplication();
  TPaletteHandle *ph      = app->getPaletteController()->getCurrentPalette();
  int styleId             = ph->getStyleIndex();
  TPalette *palette       = ph->getPalette();
  TXshSimpleLevel *level  = app->getCurrentLevel()->getSimpleLevel();
  if (palette)
    TUndoManager::manager()->add(
        new UndoPickRGBM(palette, styleId, color, level));

  setCurrentColor(color);

  if (level) {
    std::vector<TFrameId> fids;
    level->getFids(fids);
    invalidateIcons(level, fids);
  }
}

}  // namespace RGBPicker

using namespace RGBPicker;

//----------------------------------------------------------------------------------------------

RGBPickerTool::RGBPickerTool()
    : TTool("T_RGBPicker")
    , m_currentStyleId(0)
    , m_pickType("Type:")
    , m_drawingTrack()
    , m_workingTrack()
    , m_firstDrawingPos()
    , m_firstWorkingPos()
    , m_mousePosition()
    , m_thick(0.5)
    , m_stroke(0)
    , m_firstStroke(0)
    , m_makePick(false)
    , m_firstTime(true)
    , m_passivePick("Passive Pick", false)
    , m_toolOptionsBox(0)
    , m_mousePixelPosition() {
  bind(TTool::CommonLevels);
  m_prop.bind(m_pickType);
  m_pickType.addValue(NORMAL_PICK);
  m_pickType.addValue(RECT_PICK);
  m_pickType.addValue(FREEHAND_PICK);
  m_pickType.addValue(POLYLINE_PICK);
  m_pickType.setId("Type");

  m_prop.bind(m_passivePick);
  m_passivePick.setId("PassivePick");
}

//---------------------------------------------------------

void RGBPickerTool::setToolOptionsBox(RGBPickerToolOptionsBox *toolOptionsBox) {
  m_toolOptionsBox.push_back(toolOptionsBox);
}

//---------------------------------------------------------

void RGBPickerTool::updateTranslation() {
  m_pickType.setQStringName(tr("Type:"));
  m_passivePick.setQStringName(tr("Passive Pick"));
}

//---------------------------------------------------------

// Used to notify and set the currentColor outside the draw() methods:
// using special style there was a conflict between the draw() methods of the
// tool
// and the genaration of the icon inside the style editor (makeIcon()) which use
// another glContext

void RGBPickerTool::onImageChanged() {
  if (m_currentStyleId != 0 && m_makePick &&
      (m_pickType.getValue() == POLYLINE_PICK ||
       m_pickType.getValue() == RECT_PICK)) {
    TTool::Application *app = TTool::getApplication();
    TPaletteHandle *ph      = app->getPaletteController()->getCurrentPalette();
    int styleId             = ph->getStyleIndex();
    TPalette *palette       = ph->getPalette();
    TXshSimpleLevel *level  = app->getCurrentLevel()->getSimpleLevel();
    if (palette)
      TUndoManager::manager()->add(
          new UndoPickRGBM(palette, styleId, m_currentValue, level));
    setCurrentColor(m_currentValue);
    if (level) {
      std::vector<TFrameId> fids;
      level->getFids(fids);
      invalidateIcons(level, fids);
    }
  }
  m_makePick = false;
}

void RGBPickerTool::draw() {
  double pixelSize2 = getPixelSize() * getPixelSize();
  m_thick           = sqrt(pixelSize2) / 2.0;
  if (m_makePick) {
    if (m_currentStyleId != 0) {
      // Il pick in modalita' polyline e rectangular deve essere fatto soltanto
      // dopo aver cancellato il
      //"disegno" della polyline altrimenti alcuni pixels neri delle spezzate
      // che la
      // compongono vengono presi in considerazione nel calcolo del "colore
      // medio"
      if (m_pickType.getValue() == POLYLINE_PICK && m_drawingPolyline.empty())
        doPolylineFreehandPick();
      else if (m_pickType.getValue() == RECT_PICK && m_drawingRect.isEmpty())
        pickRect();
      else if (m_pickType.getValue() == NORMAL_PICK)
        pick();
      else if (m_pickType.getValue() == FREEHAND_PICK && m_stroke)
        doPolylineFreehandPick();
    }
    return;
  }
  if (m_passivePick.getValue() == true) {
    passivePick();
  }
  if (m_pickType.getValue() == RECT_PICK && !m_makePick) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Red;
    ToolUtils::drawRect(m_drawingRect, color, 0x3F33, true);
  } else if (m_pickType.getValue() == POLYLINE_PICK &&
             !m_drawingPolyline.empty()) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    tglColor(color);
    tglDrawCircle(m_drawingPolyline[0], 2);
    glBegin(GL_LINE_STRIP);
    for (UINT i = 0; i < m_drawingPolyline.size(); i++)
      tglVertex(m_drawingPolyline[i]);
    tglVertex(m_mousePosition);
    glEnd();
  } else if (m_pickType.getValue() == FREEHAND_PICK &&
             !m_drawingTrack.isEmpty()) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    tglColor(color);
    m_drawingTrack.drawAllFragments();
  }
}

//---------------------------------------------------------

void RGBPickerTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app   = TTool::getApplication();
  TPaletteHandle *pltHandle = app->getPaletteController()->getCurrentPalette();
  m_currentStyleId          = pltHandle->getStyleIndex();
  if (m_currentStyleId == 0) return;

  TColorStyle *colorStyle = pltHandle->getStyle();

  if (colorStyle) m_oldValue = colorStyle->getMainColor();

  if (m_pickType.getValue() == RECT_PICK) {
    m_selectingRect.x0 = e.m_pos.x;
    m_selectingRect.y0 = e.m_pos.y;
    m_selectingRect.x1 = e.m_pos.x;
    m_selectingRect.y1 = e.m_pos.y;
    m_drawingRect.x0   = pos.x;
    m_drawingRect.y0   = pos.y;
    m_drawingRect.x1   = pos.x;
    m_drawingRect.y1   = pos.y;
    invalidate();
    return;
  } else if (m_pickType.getValue() == FREEHAND_PICK) {
    startFreehand(pos, e.m_pos);
    return;
  } else if (m_pickType.getValue() == POLYLINE_PICK) {
    addPointPolyline(pos, e.m_pos);
    return;
  } else {  // NORMAL_PICK
    m_mousePixelPosition = e.m_pos;
    m_makePick           = true;
    invalidate();
  }
}

//---------------------------------------------------------

void RGBPickerTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (m_currentStyleId == 0) return;
  if (m_pickType.getValue() == RECT_PICK) {
    m_selectingRect.x1 = e.m_pos.x;
    m_selectingRect.y1 = e.m_pos.y;
    m_drawingRect.x1   = pos.x;
    m_drawingRect.y1   = pos.y;
    invalidate();
    return;
  } else if (m_pickType.getValue() == FREEHAND_PICK) {
    freehandDrag(pos, e.m_pos);
    invalidate();
  }
}

//---------------------------------------------------------

void RGBPickerTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (m_currentStyleId == 0) return;
  if (m_pickType.getValue() == RECT_PICK) {
    m_makePick = true;
    m_drawingRect.empty();
  }
  if (m_pickType.getValue() == FREEHAND_PICK) {
    closeFreehand();
    if (m_currentStyleId != 0) m_makePick = true;
  }
  invalidate();
}

//---------------------------------------------------------

void RGBPickerTool::leftButtonDoubleClick(const TPointD &pos,
                                          const TMouseEvent &e) {
  if (m_currentStyleId == 0) return;
  if (m_pickType.getValue() == POLYLINE_PICK) {
    closePolyline(pos, e.m_pos);
    std::vector<TThickPoint> strokePoints;
    for (UINT i = 0; i < m_workingPolyline.size() - 1; i++) {
      strokePoints.push_back(TThickPoint(m_workingPolyline[i], 1));
      strokePoints.push_back(TThickPoint(
          0.5 * (m_workingPolyline[i] + m_workingPolyline[i + 1]), 1));
    }
    strokePoints.push_back(TThickPoint(m_workingPolyline.back(), 1));
    m_drawingPolyline.clear();
    m_workingPolyline.clear();
    m_stroke   = new TStroke(strokePoints);
    m_makePick = true;
    invalidate();
  }
}

//---------------------------------------------------------

void RGBPickerTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  /*--- Pick color passively and display in the tool option bar ---*/
  if (m_passivePick.getValue() == true) {
    m_mousePixelPosition = e.m_pos;
    invalidate();
  }
  if (m_pickType.getValue() == POLYLINE_PICK && !m_drawingPolyline.empty()) {
    m_mousePosition = pos;
    invalidate();
  }
}

//---------------------------------------------------------

void RGBPickerTool::passivePick() {
  TImageP image = TImageP(getImage(false));
  if (!image) return;
  TRectD area = TRectD(m_mousePixelPosition.x, m_mousePixelPosition.y,
                       m_mousePixelPosition.x, m_mousePixelPosition.y);

  StylePicker picker(image);

  if (LutCalibrator::instance()->isValid()) m_viewer->bindFBO();

  TPixel32 pix = picker.pickColor(area);

  if (LutCalibrator::instance()->isValid()) m_viewer->releaseFBO();

  QColor col((int)pix.r, (int)pix.g, (int)pix.b);

  PaletteController *controller =
      TTool::getApplication()->getPaletteController();
  controller->notifyColorPassivePicked(col);
}

//---------------------------------------------------------

void RGBPickerTool::pick() {
  TImageP image = TImageP(getImage(false));

  TTool::Application *app = TTool::getApplication();
  TPaletteHandle *ph      = app->getPaletteController()->getCurrentPalette();
  int styleId             = ph->getStyleIndex();
  TPalette *palette       = ph->getPalette();
  if (!palette) return;

  TRectD area = TRectD(m_mousePixelPosition.x - 1, m_mousePixelPosition.y - 1,
                       m_mousePixelPosition.x + 1, m_mousePixelPosition.y + 1);
  StylePicker picker(image, palette);

  if (LutCalibrator::instance()->isValid()) m_viewer->bindFBO();

  m_currentValue = picker.pickColor(area);

  if (LutCalibrator::instance()->isValid()) m_viewer->releaseFBO();

  TXshSimpleLevel *level = app->getCurrentLevel()->getSimpleLevel();
  UndoPickRGBM *cmd = new UndoPickRGBM(palette, styleId, m_currentValue, level);
  TUndoManager::manager()->add(cmd);
  cmd->redo();

  m_makePick = false;
  /*
setCurrentColor(m_currentValue);
if(level)
{
vector<TFrameId> fids;
level->getFids(fids);
invalidateIcons(level,fids);
}
*/
}
//---------------------------------------------------------

void RGBPickerTool::pickRect() {
  TImageP image = TImageP(getImage(false));

  TTool::Application *app = TTool::getApplication();
  TPaletteHandle *ph      = app->getPaletteController()->getCurrentPalette();
  int styleId             = ph->getStyleIndex();
  TPalette *palette       = ph->getPalette();
  TRectD area             = m_selectingRect;
  if (!palette) return;
  if (m_selectingRect.x0 > m_selectingRect.x1) {
    area.x1 = m_selectingRect.x0;
    area.x0 = m_selectingRect.x1;
  }
  if (m_selectingRect.y0 > m_selectingRect.y1) {
    area.y1 = m_selectingRect.y0;
    area.y0 = m_selectingRect.y1;
  }
  m_selectingRect.empty();
  if (area.getLx() <= 1 || area.getLy() <= 1) return;
  StylePicker picker(image, palette);

  if (LutCalibrator::instance()->isValid()) m_viewer->bindFBO();

  m_currentValue = picker.pickColor(area);

  if (LutCalibrator::instance()->isValid()) m_viewer->releaseFBO();
}

//---------------------------------------------------------

void RGBPickerTool::pickStroke() {
  TImageP image = TImageP(getImage(false));

  TTool::Application *app = TTool::getApplication();
  TPaletteHandle *ph      = app->getPaletteController()->getCurrentPalette();
  int styleId             = ph->getStyleIndex();
  TPalette *palette       = ph->getPalette();
  if (!palette) return;

  StylePicker picker(image, palette);
  TStroke *stroke = new TStroke(*m_stroke);

  if (LutCalibrator::instance()->isValid()) m_viewer->bindFBO();

  m_currentValue = picker.pickColor(stroke);

  if (LutCalibrator::instance()->isValid()) m_viewer->releaseFBO();

  if (!(m_pickType.getValue() == POLYLINE_PICK)) {
    TXshSimpleLevel *level = app->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(
        new UndoPickRGBM(palette, styleId, m_currentValue, level));
    setCurrentColor(m_currentValue);
    if (level) {
      std::vector<TFrameId> fids;
      level->getFids(fids);
      invalidateIcons(level, fids);
    }
  }
}

//---------------------------------------------------------

bool RGBPickerTool::onPropertyChanged(std::string propertyName) {
  if (propertyName == m_pickType.getName())
    PickVectorType = ::to_string(m_pickType.getValue());
  else if (propertyName == m_passivePick.getName())
    PickPassive = m_passivePick.getValue();
  return true;
}

//---------------------------------------------------------

void RGBPickerTool::onActivate() {
  if (m_firstTime) {
    m_pickType.setValue(::to_wstring(PickVectorType.getValue()));
    m_passivePick.setValue(PickPassive ? 1 : 0);
    m_firstTime = false;
  }
}

//---------------------------------------------------------

TPropertyGroup *RGBPickerTool::getProperties(int targetType) { return &m_prop; }

//---------------------------------------------------------

int RGBPickerTool::getCursorId() const {
  int currentStyleId = getApplication()
                           ->getPaletteController()
                           ->getCurrentPalette()
                           ->getStyleIndex();
  if (currentStyleId == 0) return ToolCursor::ForbiddenCursor;
  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    return ToolCursor::PickerRGBWhite;
  else
    return ToolCursor::PickerRGB;
}

//---------------------------------------------------------

void RGBPickerTool::doPolylineFreehandPick() {
  if (m_stroke && (m_pickType.getValue() == POLYLINE_PICK ||
                   m_pickType.getValue() == FREEHAND_PICK)) {
    pickStroke();
    delete m_stroke;
    m_stroke = 0;
  }
}

//---------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del lazzo.
//! Viene inizializzato \b m_firstPos
void RGBPickerTool::startFreehand(const TPointD &drawingPos,
                                  const TPointD &workingPos) {
  m_drawingTrack.clear();
  m_workingTrack.clear();
  m_firstDrawingPos = drawingPos;
  m_firstWorkingPos = workingPos;
  double pixelSize2 = getPixelSize() * getPixelSize();
  m_drawingTrack.add(TThickPoint(drawingPos, m_thick), pixelSize2);
  m_workingTrack.add(TThickPoint(workingPos, m_thick), pixelSize2);
#if defined(MACOSX)
// m_viewer->prepareForegroundDrawing();
#endif
}

//---------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del lazzo.
void RGBPickerTool::freehandDrag(const TPointD &drawingPos,
                                 const TPointD &workingPos) {
#if defined(MACOSX)
// getViewer()->enableRedraw(false);
#endif

  double pixelSize2 = getPixelSize() * getPixelSize();
  m_drawingTrack.add(TThickPoint(drawingPos, m_thick), pixelSize2);
  m_workingTrack.add(TThickPoint(workingPos, m_thick), pixelSize2);
}

//---------------------------------------------------------

//! Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene creato
//! lo stroke rappresentante il lazzo.
void RGBPickerTool::closeFreehand() {
#if defined(MACOSX)
// getViewer()->enableRedraw(true);
#endif
  if (m_drawingTrack.isEmpty() || m_workingTrack.isEmpty()) return;
  double pixelSize2 = getPixelSize() * getPixelSize();
  m_drawingTrack.add(TThickPoint(m_firstDrawingPos, m_thick), pixelSize2);
  m_workingTrack.add(TThickPoint(m_firstWorkingPos, m_thick), pixelSize2);
  m_workingTrack.filterPoints();
  double error = (30.0 / 11) * sqrt(pixelSize2);
  m_stroke     = m_workingTrack.makeStroke(error);
  m_stroke->setStyle(1);

  m_drawingTrack.clear();
  m_workingTrack.clear();
}

//---------------------------------------------------------

//! Viene aggiunto un punto al vettore m_polyline.
void RGBPickerTool::addPointPolyline(const TPointD &drawingPos,
                                     const TPointD &workingPos) {
  m_mousePosition = drawingPos;
  /*---drawingPosは中心からの座標 workingPosは画面左下からの座標---*/
  m_drawingPolyline.push_back(drawingPos);
  m_workingPolyline.push_back(workingPos);
}

//---------------------------------------------------------

//! Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b
//! m_polyline.front() alla fine del vettore)
void RGBPickerTool::closePolyline(const TPointD &drawingPos,
                                  const TPointD &workingPos) {
  if (m_drawingPolyline.size() <= 1 || m_workingPolyline.size() <= 1) return;
  if (m_drawingPolyline.back() != drawingPos)
    m_drawingPolyline.push_back(drawingPos);
  if (m_workingPolyline.back() != workingPos)
    m_workingPolyline.push_back(workingPos);
  if (m_drawingPolyline.back() != m_drawingPolyline.front())
    m_drawingPolyline.push_back(m_drawingPolyline.front());
  if (m_workingPolyline.back() != m_workingPolyline.front())
    m_workingPolyline.push_back(m_workingPolyline.front());
}

//---------------------------------------------------------
/*! Flipbook上でPassive Pickを有効にする
*/
void RGBPickerTool::showFlipPickedColor(const TPixel32 &pix) {
  if (m_passivePick.getValue()) {
    QColor col((int)pix.r, (int)pix.g, (int)pix.b);
    PaletteController *controller =
        TTool::getApplication()->getPaletteController();
    controller->notifyColorPassivePicked(col);
  }
}

RGBPickerTool RGBpicktool;

// TTool *getPickRGBMTool() {return &pickRBGMTool;}
