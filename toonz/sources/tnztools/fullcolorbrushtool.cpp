

#include "fullcolorbrushtool.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "tools/tooloptions.h"
#include "tools/replicator.h"

#include "mypainttoonzbrush.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/strokegenerator.h"
#include "toonz/tstageobject.h"
#include "toonz/palettecontroller.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tgl.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "tenv.h"
#include "tpalette.h"
#include "trop.h"
#include "tstream.h"
#include "tstroke.h"
#include "timagecache.h"
#include "tpixelutils.h"

// Qt includes
#include <QCoreApplication>  // Qt translation support

//----------------------------------------------------------------------------------

TEnv::IntVar FullcolorBrushMinSize("FullcolorBrushMinSize", 1);
TEnv::IntVar FullcolorBrushMaxSize("FullcolorBrushMaxSize", 5);
TEnv::IntVar FullcolorPressureSensitivity("FullcolorPressureSensitivity", 1);
TEnv::DoubleVar FullcolorBrushHardness("FullcolorBrushHardness", 100);
TEnv::DoubleVar FullcolorMinOpacity("FullcolorMinOpacity", 100);
TEnv::DoubleVar FullcolorMaxOpacity("FullcolorMaxOpacity", 100);
TEnv::DoubleVar FullcolorModifierSize("FullcolorModifierSize", 0);
TEnv::DoubleVar FullcolorModifierOpacity("FullcolorModifierOpacity", 100);
TEnv::IntVar FullcolorModifierEraser("FullcolorModifierEraser", 0);
TEnv::IntVar FullcolorModifierLockAlpha("FullcolorModifierLockAlpha", 0);
TEnv::IntVar FullcolorAssistants("FullcolorAssistants", 1);
TEnv::StringVar FullcolorBrushPreset("FullcolorBrushPreset", "<custom>");

//----------------------------------------------------------------------------------

#define CUSTOM_WSTR L"<custom>"

//----------------------------------------------------------------------------------

namespace {

class FullColorBrushUndo final : public ToolUtils::TFullColorRasterUndo {
  TPoint m_offset;
  QString m_id;

public:
  FullColorBrushUndo(TTileSetFullColor *tileSet, TXshSimpleLevel *level,
                     const TFrameId &frameId, bool isFrameCreated,
                     const TRasterP &ras, const TPoint &offset)
      : ToolUtils::TFullColorRasterUndo(tileSet, level, frameId, isFrameCreated,
                                        false, 0)
      , m_offset(offset) {
    static int counter = 0;

    m_id = QString("FullColorBrushUndo") + QString::number(counter++);
    TImageCache::instance()->add(m_id.toStdString(), TRasterImageP(ras));
  }

  ~FullColorBrushUndo() { TImageCache::instance()->remove(m_id); }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TRasterImageP image = getImage();
    TRasterP ras        = image->getRaster();

    TRasterImageP srcImg =
        TImageCache::instance()->get(m_id.toStdString(), false);
    ras->copy(srcImg->getRaster(), m_offset);

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + ToolUtils::TFullColorRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Raster Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

}  // namespace

//************************************************************************
//    FullColor Brush Tool implementation
//************************************************************************

FullColorBrushTool::FullColorBrushTool(std::string name)
    : TTool(name)
    , m_thickness("Size", 1, 1000, 1, 5, false)
    , m_pressure("Pressure", true)
    , m_opacity("Opacity", 0, 100, 100, 100, true)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_modifierSize("ModifierSize", -3, 3, 0, true)
    , m_modifierOpacity("ModifierOpacity", 0, 100, 100, true)
    , m_modifierEraser("ModifierEraser", false)
    , m_modifierLockAlpha("Lock Alpha", false)
    , m_assistants("Assistants", true)
    , m_preset("Preset:")
    , m_enabledPressure(false)
    , m_minCursorThick(0)
    , m_maxCursorThick(0)
    , m_tileSet(0)
    , m_tileSaver(0)
    , m_notifier(0)
    , m_presetsLoaded(false)
    , m_firstTime(true)
    , m_started(false) {
  bind(TTool::RasterImage | TTool::EmptyTarget);
  m_thickness.setNonLinearSlider();

  m_prop.bind(m_thickness);
  m_prop.bind(m_hardness);
  m_prop.bind(m_opacity);
  m_prop.bind(m_modifierSize);
  m_prop.bind(m_modifierOpacity);
  m_prop.bind(m_modifierEraser);
  m_prop.bind(m_modifierLockAlpha);
  m_prop.bind(m_pressure);
  m_prop.bind(m_assistants);
  m_prop.bind(m_preset);

  m_inputmanager.setHandler(this);
#ifndef NDEBUG
  m_modifierTest = new TModifierTest();
#endif
  m_modifierLine         = new TModifierLine();
  m_modifierTangents     = new TModifierTangents();
  m_modifierAssistants   = new TModifierAssistants();
  m_modifierSegmentation = new TModifierSegmentation();

  m_inputmanager.addModifier(
      TInputModifierP(m_modifierAssistants.getPointer()));

  m_thickness.setNonLinearSlider();
  m_preset.setId("BrushPreset");
  m_modifierEraser.setId("RasterEraser");
  m_modifierLockAlpha.setId("LockAlpha");
  m_pressure.setId("PressureSensitivity");
}

//-------------------------------------------------------------------------------------------------------

unsigned int FullColorBrushTool::getToolHints() const {
  unsigned int h = TTool::getToolHints() & ~HintAssistantsAll;
  if (m_assistants.getValue()) {
    h |= HintReplicators;
    h |= HintReplicatorsPoints;
    h |= HintReplicatorsEnabled;
  }
  return h;
}

//---------------------------------------------------------------------------------------------------

ToolOptionsBox *FullColorBrushTool::createOptionsBox() {
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
  return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onCanvasSizeChanged() {
  onDeactivate();
  setWorkAndBackupImages();
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onColorStyleChanged() {
  getApplication()->getCurrentTool()->notifyToolChanged();
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateTranslation() {
  m_thickness.setQStringName(tr("Size"));
  m_pressure.setQStringName(tr("Pressure"));
  m_opacity.setQStringName(tr("Opacity"));
  m_hardness.setQStringName(tr("Hardness:"));
  m_preset.setQStringName(tr("Preset:"));
  m_modifierSize.setQStringName(tr("Size"));
  m_modifierOpacity.setQStringName(tr("Opacity"));
  m_modifierEraser.setQStringName(tr("Eraser"));
  m_modifierLockAlpha.setQStringName(tr("Lock Alpha"));
  m_assistants.setQStringName(tr("Assistants"));
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onActivate() {
  if (!m_notifier) m_notifier = new FullColorBrushToolNotifier(this);
  m_notifier->onActivate();

  updateCurrentStyle();

  if (m_firstTime) {
    m_firstTime = false;

    std::wstring wpreset =
        QString::fromStdString(FullcolorBrushPreset.getValue()).toStdWString();
    if (wpreset != CUSTOM_WSTR) {
      initPresets();
      if (!m_preset.isValue(wpreset)) wpreset = CUSTOM_WSTR;
      m_preset.setValue(wpreset);
      FullcolorBrushPreset = m_preset.getValueAsString();
      loadPreset();
    } else
      loadLastBrush();
  }

  setWorkAndBackupImages();
  onColorStyleChanged();
  updateModifiers();
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::onDeactivate() {
  if (m_notifier) m_notifier->onDeactivate();

  m_inputmanager.finishTracks();
  m_workRaster = TRaster32P();
  m_backUpRas  = TRasterP();
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateWorkAndBackupRasters(const TRect &rect) {
  if (rect.isEmpty()) return;

  TRasterImageP ri = TImageP(getImage(false, 1));
  if (!ri) return;

  TRasterP ras = ri->getRaster();

  const int denominator = 8;
  TRect enlargedRect    = rect + m_lastRect;
  int dx                = (enlargedRect.getLx() - 1) / denominator + 1;
  int dy                = (enlargedRect.getLy() - 1) / denominator + 1;

  if (m_lastRect.isEmpty()) {
    enlargedRect.x0 -= dx;
    enlargedRect.y0 -= dy;
    enlargedRect.x1 += dx;
    enlargedRect.y1 += dy;

    TRect _rect = enlargedRect * ras->getBounds();
    if (_rect.isEmpty()) return;

    m_workRaster->extract(_rect)->copy(ras->extract(_rect));
    m_backUpRas->extract(_rect)->copy(ras->extract(_rect));
  } else {
    if (enlargedRect.x0 < m_lastRect.x0) enlargedRect.x0 -= dx;
    if (enlargedRect.y0 < m_lastRect.y0) enlargedRect.y0 -= dy;
    if (enlargedRect.x1 > m_lastRect.x1) enlargedRect.x1 += dx;
    if (enlargedRect.y1 > m_lastRect.y1) enlargedRect.y1 += dy;

    TRect _rect = enlargedRect * ras->getBounds();
    if (_rect.isEmpty()) return;

    TRect _lastRect    = m_lastRect * ras->getBounds();
    QList<TRect> rects = ToolUtils::splitRect(_rect, _lastRect);
    for (int i = 0; i < rects.size(); i++) {
      m_workRaster->extract(rects[i])->copy(ras->extract(rects[i]));
      m_backUpRas->extract(rects[i])->copy(ras->extract(rects[i]));
    }
  }

  m_lastRect = enlargedRect;
}

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::askRead(const TRect &rect) { return askWrite(rect); }

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::askWrite(const TRect &rect) {
  if (rect.isEmpty()) return true;
  m_strokeRect += rect;
  m_strokeSegmentRect += rect;
  updateWorkAndBackupRasters(rect);
  m_tileSaver->save(rect);
  return true;
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateModifiers() {
  m_modifierAssistants->magnetism = m_assistants.getValue() ? 1 : 0;
  m_inputmanager.drawPreview      = false; //!m_modifierAssistants->drawOnly;

  m_modifierReplicate.clear();
  if (m_assistants.getValue())
    TReplicator::scanReplicators(this, nullptr, &m_modifierReplicate, false, true, false, false, nullptr);
  
  m_inputmanager.clearModifiers();
  m_inputmanager.addModifier(TInputModifierP(m_modifierTangents.getPointer()));
  m_inputmanager.addModifier(
      TInputModifierP(m_modifierAssistants.getPointer()));
#ifndef NDEBUG
  m_inputmanager.addModifier(TInputModifierP(m_modifierTest.getPointer()));
#endif
  m_inputmanager.addModifiers(m_modifierReplicate);
  m_inputmanager.addModifier(
      TInputModifierP(m_modifierSegmentation.getPointer()));
}

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::preLeftButtonDown() {
  updateModifiers();
  touchImage();

  if (m_isFrameCreated) {
    setWorkAndBackupImages();
    // When the xsheet frame is selected, whole viewer will be updated from
    // SceneViewer::onXsheetChanged() on adding a new frame.
    // We need to take care of a case when the level frame is selected.
    if (m_application->getCurrentFrame()->isEditingLevel()) invalidate();
  }

  return true;
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::handleMouseEvent(MouseEventType type,
                                          const TPointD &pos,
                                          const TMouseEvent &e) {
  TTimerTicks t = TToolTimer::ticks();
  bool alt      = e.getModifiersMask() & TMouseEvent::ALT_KEY;
  bool shift    = e.getModifiersMask() & TMouseEvent::SHIFT_KEY;
  bool control  = e.getModifiersMask() & TMouseEvent::CTRL_KEY;

  if (shift && type == ME_DOWN && e.button() == Qt::LeftButton && !m_started) {
    m_modifierAssistants->magnetism = 0;
    m_inputmanager.clearModifiers();
    m_inputmanager.addModifier(TInputModifierP(m_modifierLine.getPointer()));
    m_inputmanager.addModifier(
        TInputModifierP(m_modifierAssistants.getPointer()));
    m_inputmanager.addModifiers(m_modifierReplicate);
    m_inputmanager.addModifier(
        TInputModifierP(m_modifierSegmentation.getPointer()));
    m_inputmanager.drawPreview = true;
  }

  if (alt != m_inputmanager.state.isKeyPressed(TKey::alt))
    m_inputmanager.keyEvent(alt, TKey::alt, t, nullptr);
  if (shift != m_inputmanager.state.isKeyPressed(TKey::shift))
    m_inputmanager.keyEvent(shift, TKey::shift, t, nullptr);
  if (control != m_inputmanager.state.isKeyPressed(TKey::control))
    m_inputmanager.keyEvent(control, TKey::control, t, nullptr);

  if (type == ME_MOVE) {
    THoverList hovers(1, pos);
    m_inputmanager.hoverEvent(hovers);
  } else {
    bool   isMyPaint   = getApplication()->getCurrentLevelStyle()->getTagId() == 4001;
    int    deviceId    = e.isTablet() ? 1 : 0;
    double defPressure = isMyPaint ? 0.5 : 1.0;
    bool   hasPressure = e.isTablet();
    double pressure    = hasPressure ? e.m_pressure : defPressure;
    bool   final       = type == ME_UP;
    m_inputmanager.trackEvent(
      deviceId, 0, pos, pressure, TPointD(), hasPressure, false, final, t);
    m_inputmanager.processTracks();
  }
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &e) {
  handleMouseEvent(ME_DOWN, pos, e);
}
void FullColorBrushTool::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  handleMouseEvent(ME_DRAG, pos, e);
}
void FullColorBrushTool::leftButtonUp(const TPointD &pos,
                                      const TMouseEvent &e) {
  handleMouseEvent(ME_UP, pos, e);
}
void FullColorBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  handleMouseEvent(ME_MOVE, pos, e);
}

//---------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::inputMouseMove(const TPointD &position,
                                        const TInputState &state) {
  struct Locals {
    FullColorBrushTool *m_this;

    void notify(TProperty &prop) {
      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addMinMax(TIntPairProperty &prop, double add) {
      const TIntPairProperty::Range &range = prop.getRange();

      TIntPairProperty::Value value = prop.getValue();
      value.second =
          tcrop<double>(value.second + add, range.first, range.second);
      value.first = tcrop<double>(value.first + add, range.first, range.second);
      prop.setValue(value);

      notify(prop);
    }

    void addMinMaxSeparate(TIntPairProperty &prop, double min, double max) {
      if (min == 0.0 && max == 0.0) return;
      const TIntPairProperty::Range &range = prop.getRange();

      TIntPairProperty::Value value = prop.getValue();
      value.first += min;
      value.second += max;
      if (value.first > value.second) value.first = value.second;
      value.first = tcrop<double>(value.first, range.first, range.second);

      value.second = tcrop<double>(value.second, range.first, range.second);
      prop.setValue(value);

      notify(prop);
    }

    void add(TDoubleProperty &prop, double x) {
      if (x == 0.0) return;

      const TDoubleProperty::Range &range = prop.getRange();
      double value =
          tcrop<double>(prop.getValue() + x, range.first, range.second);
      prop.setValue(value);

      notify(prop);
    }
  } locals = {this};

  if (state.isKeyPressed(TKey::control) && state.isKeyPressed(TKey::alt)) {
    const TPointD &diff = position - m_mousePos;
    if (getBrushStyle()) {
      locals.add(m_modifierSize, 0.01 * diff.x);
    } else {
      locals.addMinMaxSeparate(m_thickness, int(diff.x / 2), int(diff.y / 2));
    }
  } else {
    m_brushPos = position;
  }

  m_mousePos = position;
  invalidate();
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::inputSetBusy(bool busy) {
  if (m_started == busy) return;
  if (busy) {
    // begin paint
    TRasterImageP ri = (TRasterImageP)getImage(true);
    if (!ri) ri = (TRasterImageP)touchImage();
    if (!ri) return;
    TRasterP ras = ri->getRaster();

    if (!(m_workRaster && m_backUpRas)) setWorkAndBackupImages();
    m_workRaster->lock();
    m_tileSet   = new TTileSetFullColor(ras->getSize());
    m_tileSaver = new TTileSaverFullColor(ras, m_tileSet);

    // update color here since the current style might be switched
    // with numpad shortcut keys
    updateCurrentStyle();
  } else {
    // end paint
    if (TRasterImageP ri = (TRasterImageP)getImage(true)) {
      TRasterP ras = ri->getRaster();

      m_lastRect.empty();
      m_workRaster->unlock();

      if (m_tileSet->getTileCount() > 0) {
        delete m_tileSaver;
        TTool::Application *app   = TTool::getApplication();
        TXshLevel *level          = app->getCurrentLevel()->getLevel();
        TXshSimpleLevelP simLevel = level->getSimpleLevel();
        TFrameId frameId          = getCurrentFid();
        TRasterP subras           = ras->extract(m_strokeRect)->clone();
        TUndoManager::manager()->add(new FullColorBrushUndo(
            m_tileSet, simLevel.getPointer(), frameId, m_isFrameCreated, subras,
            m_strokeRect.getP00()));
      }

      notifyImageChanged();
      m_strokeRect.empty();
    }
  }
  m_started = busy;
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::inputPaintTrackPoint(const TTrackPoint &point,
                                              const TTrack &track,
                                              bool firstTrack,
                                              bool preview)
{
  // get raster
  if (!m_started || !getViewer() || preview)
    return;
  
  TRasterImageP ri = (TRasterImageP)getImage(true);
  if (!ri) return;
  TRasterP ras      = ri->getRaster();
  TPointD rasCenter = ras->getCenterD();

  // init brush
  TrackHandler *handler;
  if (track.size() == track.pointsAdded && !track.handler && m_workRaster) {
    mypaint::Brush mypaintBrush;
    applyToonzBrushSettings(mypaintBrush);
    handler = new TrackHandler(m_workRaster, *this, mypaintBrush);
    handler->brush.beginStroke();
    track.handler = handler;
  }
  handler = dynamic_cast<TrackHandler *>(track.handler.getPointer());
  if (!handler) return;

  bool   isMyPaint   = getApplication()->getCurrentLevelStyle()->getTagId() == 4001;
  double defPressure = isMyPaint ? 0.5 : 1.0;
  double pressure    = m_enabledPressure ? point.pressure : defPressure;
  
  // paint stroke
  m_strokeSegmentRect.empty();
  handler->brush.strokeTo(point.position + rasCenter,
                          pressure, point.tilt,
                          point.time - track.previous().time);
  if (track.pointsAdded == 1 && track.finished()) handler->brush.endStroke();

  // update affected area
  TRect updateRect = m_strokeSegmentRect * ras->getBounds();
  if (!updateRect.isEmpty())
    ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));
  TRectD invalidateRect = convert(m_strokeSegmentRect) - rasCenter;
  if (firstTrack) {
    TPointD thickOffset(m_maxCursorThick * 0.5, m_maxCursorThick * 0.5);
    invalidateRect +=
        TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
    invalidateRect +=
        TRectD(point.position - thickOffset, point.position + thickOffset);
    m_brushPos = m_mousePos = point.position;
  }
  invalidate(invalidateRect.enlarge(2.0));
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::inputInvalidateRect(const TRectD &bounds) {
  invalidate(bounds);
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::draw() {
  if (TRasterImageP ri = TRasterImageP(getImage(false))) {
    // If toggled off, don't draw brush outline
    if (!Preferences::instance()->isCursorOutlineEnabled()) return;

    TRasterP ras = ri->getRaster();

    double alpha       = 1.0;
    double alphaRadius = 3.0;
    double pixelSize   = sqrt(tglGetPixelSize2());

    // circles with lesser radius looks more bold
    // to avoid these effect we'll reduce alpha for small radiuses
    double minX     = m_minCursorThick / (alphaRadius * pixelSize);
    double maxX     = m_maxCursorThick / (alphaRadius * pixelSize);
    double minAlpha = alpha * (1.0 - 1.0 / (1.0 + minX));
    double maxAlpha = alpha * (1.0 - 1.0 / (1.0 + maxX));

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    tglEnableBlending();
    tglEnableLineSmooth(true, 0.5);

    if (m_minCursorThick < m_maxCursorThick - pixelSize) {
      glColor4d(1.0, 1.0, 1.0, minAlpha);
      tglDrawCircle(m_brushPos, (m_minCursorThick + 1) * 0.5 - pixelSize);
      glColor4d(0.0, 0.0, 0.0, minAlpha);
      tglDrawCircle(m_brushPos, (m_minCursorThick + 1) * 0.5);
    }

    glColor4d(1.0, 1.0, 1.0, maxAlpha);
    tglDrawCircle(m_brushPos, (m_maxCursorThick + 1) * 0.5 - pixelSize);
    glColor4d(0.0, 0.0, 0.0, maxAlpha);
    tglDrawCircle(m_brushPos, (m_maxCursorThick + 1) * 0.5);

    glPopAttrib();
  }
  m_inputmanager.draw();
}

//--------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onEnter() { updateCurrentStyle(); }

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onLeave() {
  m_minCursorThick = 0;
  m_maxCursorThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *FullColorBrushTool::getProperties(int targetType) {
  if (!m_presetsLoaded) initPresets();
  return &m_prop;
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onImageChanged() { setWorkAndBackupImages(); }

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::setWorkAndBackupImages() {
  TRasterImageP ri = (TRasterImageP)getImage(false, 1);
  if (!ri) return;

  TRasterP ras   = ri->getRaster();
  TDimension dim = ras->getSize();

  if (!m_workRaster || m_workRaster->getLx() != dim.lx ||
      m_workRaster->getLy() != dim.ly)
    m_workRaster = TRaster32P(dim);

  if (!m_backUpRas || m_backUpRas->getLx() != dim.lx ||
      m_backUpRas->getLy() != dim.ly ||
      m_backUpRas->getPixelSize() != ras->getPixelSize())
    m_backUpRas = ras->create(dim.lx, dim.ly);

  m_strokeRect.empty();
  m_lastRect.empty();
}

//------------------------------------------------------------------

bool FullColorBrushTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  updateCurrentStyle();

  if (propertyName == "Preset:") {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last saved brush settings
      loadLastBrush();

    FullcolorBrushPreset = m_preset.getValueAsString();
    m_propertyUpdating   = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  }

  FullcolorBrushMinSize        = m_thickness.getValue().first;
  FullcolorBrushMaxSize        = m_thickness.getValue().second;
  FullcolorPressureSensitivity = m_pressure.getValue();
  FullcolorBrushHardness       = m_hardness.getValue();
  FullcolorMinOpacity          = m_opacity.getValue().first;
  FullcolorMaxOpacity          = m_opacity.getValue().second;
  FullcolorModifierSize        = m_modifierSize.getValue();
  FullcolorModifierOpacity     = m_modifierOpacity.getValue();
  FullcolorModifierEraser      = m_modifierEraser.getValue() ? 1 : 0;
  FullcolorModifierLockAlpha   = m_modifierLockAlpha.getValue() ? 1 : 0;
  FullcolorAssistants          = m_assistants.getValue() ? 1 : 0;

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    FullcolorBrushPreset = m_preset.getValueAsString();
    m_propertyUpdating   = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  return true;
}

//------------------------------------------------------------------

void FullColorBrushTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(TEnv::getConfigDir() + "brush_raster.txt");
  }

  // Rebuild the presets property entries
  const std::set<BrushData> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));

  std::set<BrushData>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it) m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::loadPreset() {
  const std::set<BrushData> &presets = m_presetsManager.presets();
  std::set<BrushData>::const_iterator it;

  it = presets.find(BrushData(m_preset.getValue()));
  if (it == presets.end()) return;

  const BrushData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    m_thickness.setValue(TIntPairProperty::Value(std::max((int)preset.m_min, 1),
                                                 (int)preset.m_max));
    m_hardness.setValue(preset.m_hardness, true);
    m_opacity.setValue(
        TDoublePairProperty::Value(preset.m_opacityMin, preset.m_opacityMax));
    m_pressure.setValue(preset.m_pressure);
    m_modifierSize.setValue(preset.m_modifierSize);
    m_modifierOpacity.setValue(preset.m_modifierOpacity);
    m_modifierEraser.setValue(preset.m_modifierEraser);
    m_modifierLockAlpha.setValue(preset.m_modifierLockAlpha);
    m_assistants.setValue(preset.m_assistants);
  } catch (...) {
  }
}

//------------------------------------------------------------------

void FullColorBrushTool::addPreset(QString name) {
  // Build the preset
  BrushData preset(name.toStdWString());

  preset.m_min               = m_thickness.getValue().first;
  preset.m_max               = m_thickness.getValue().second;
  preset.m_hardness          = m_hardness.getValue();
  preset.m_opacityMin        = m_opacity.getValue().first;
  preset.m_opacityMax        = m_opacity.getValue().second;
  preset.m_pressure          = m_pressure.getValue();
  preset.m_modifierSize      = m_modifierSize.getValue();
  preset.m_modifierOpacity   = m_modifierOpacity.getValue();
  preset.m_modifierEraser    = m_modifierEraser.getValue();
  preset.m_modifierLockAlpha = m_modifierLockAlpha.getValue();
  preset.m_assistants        = m_assistants.getValue();

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
  FullcolorBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void FullColorBrushTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  FullcolorBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void FullColorBrushTool::loadLastBrush() {
  m_thickness.setValue(
      TIntPairProperty::Value(FullcolorBrushMinSize, FullcolorBrushMaxSize));
  m_pressure.setValue(FullcolorPressureSensitivity ? 1 : 0);
  m_opacity.setValue(
      TDoublePairProperty::Value(FullcolorMinOpacity, FullcolorMaxOpacity));
  m_hardness.setValue(FullcolorBrushHardness);
  m_modifierSize.setValue(FullcolorModifierSize);
  m_modifierOpacity.setValue(FullcolorModifierOpacity);
  m_modifierEraser.setValue(FullcolorModifierEraser ? true : false);
  m_modifierLockAlpha.setValue(FullcolorModifierLockAlpha ? true : false);
  m_assistants.setValue(FullcolorAssistants ? true : false);
}

//------------------------------------------------------------------

void FullColorBrushTool::updateCurrentStyle() {
  m_currentColor = TPixel32::Black;
  if (TTool::Application *app = getApplication()) {
    if (app->getCurrentObject()->isSpline()) {
      m_currentColor = TPixel32::Red;
    } else if (TPalette *plt = app->getCurrentPalette()->getPalette()) {
      int style               = app->getCurrentLevelStyleIndex();
      TColorStyle *colorStyle = plt->getStyle(style);
      m_currentColor          = colorStyle->getMainColor();
    }
  }

  int prevMinCursorThick = m_minCursorThick;
  int prevMaxCursorThick = m_maxCursorThick;

  m_enabledPressure = m_pressure.getValue();
  if (TMyPaintBrushStyle *brushStyle = getBrushStyle()) {
    double radiusLog = brushStyle->getBrush().getBaseValue(
                           MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC) +
                       m_modifierSize.getValue() * log(2.0);
    double radius    = exp(radiusLog);
    m_minCursorThick = m_maxCursorThick = (int)round(2.0 * radius);
  } else {
    m_minCursorThick = std::max(m_thickness.getValue().first, 1);
    m_maxCursorThick =
        std::max(m_thickness.getValue().second, m_minCursorThick);
    if (!m_enabledPressure) m_minCursorThick = m_maxCursorThick;
  }

  // if this function is called from onEnter(), the clipping rect will not be
  // set in order to update whole viewer.
  if (prevMinCursorThick == 0 && prevMaxCursorThick == 0) return;

  if (m_minCursorThick != prevMinCursorThick ||
      m_maxCursorThick != prevMaxCursorThick) {
    TRectD rect(
        m_brushPos - TPointD(m_maxCursorThick + 2, m_maxCursorThick + 2),
        m_brushPos + TPointD(m_maxCursorThick + 2, m_maxCursorThick + 2));
    invalidate(rect);
  }
}

//------------------------------------------------------------------

TMyPaintBrushStyle *FullColorBrushTool::getBrushStyle() {
  if (TTool::Application *app = getApplication())
    return dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
  return 0;
}

//------------------------------------------------------------------

void FullColorBrushTool::applyClassicToonzBrushSettings(
    mypaint::Brush &mypaintBrush) {
  const double precision       = 1e-5;
  const double hardnessOpacity = 0.1;

  double minThickness = 0.5 * m_thickness.getValue().first;
  double maxThickness = 0.5 * m_thickness.getValue().second;
  double minOpacity   = 0.01 * m_opacity.getValue().first;
  double maxOpacity   = 0.01 * m_opacity.getValue().second;
  double hardness     = 0.01 * m_hardness.getValue();

  TPixelD color = PixelConverter<TPixelD>::from(m_currentColor);
  double colorH = 0.0;
  double colorS = 0.0;
  double colorV = 0.0;
  RGB2HSV(color.r, color.g, color.b, &colorH, &colorS, &colorV);

  // avoid log(0)
  if (minThickness < precision) minThickness = precision;
  if (maxThickness < precision) maxThickness = precision;

  // tune hardness opacity for better visual softness
  hardness *= hardness;
  double opacityAmplifier = 1.0 - hardnessOpacity + hardness * hardnessOpacity;
  minOpacity *= opacityAmplifier;
  maxOpacity *= opacityAmplifier;

  // reset
  mypaintBrush.fromDefaults();
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, 1.0);
  mypaintBrush.setMappingN(MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY,
                           MYPAINT_BRUSH_INPUT_PRESSURE, 0);

  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_HARDNESS,
                            0.5 * hardness + 0.5);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_H, colorH / 360.0);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_S, colorS);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_V, colorV);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS,
                            5.0 + hardness * 10.0);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 0.0);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_DABS_PER_SECOND, 0.0);

  // thickness may be dynamic
  if (minThickness + precision >= maxThickness) {
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                              log(maxThickness));
    mypaintBrush.setMappingN(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                             MYPAINT_BRUSH_INPUT_PRESSURE, 0);
  } else {
    double minThicknessLog  = log(minThickness);
    double maxThicknessLog  = log(maxThickness);
    double baseThicknessLog = 0.5 * (minThicknessLog + maxThicknessLog);
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                              baseThicknessLog);
    mypaintBrush.setMappingN(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                             MYPAINT_BRUSH_INPUT_PRESSURE, 2);
    mypaintBrush.setMappingPoint(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                                 MYPAINT_BRUSH_INPUT_PRESSURE, 0, 0.0,
                                 minThicknessLog - baseThicknessLog);
    mypaintBrush.setMappingPoint(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                                 MYPAINT_BRUSH_INPUT_PRESSURE, 1, 1.0,
                                 maxThicknessLog - baseThicknessLog);
  }

  // opacity may be dynamic
  if (minOpacity + precision >= maxOpacity) {
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE, maxOpacity);
    mypaintBrush.setMappingN(MYPAINT_BRUSH_SETTING_OPAQUE,
                             MYPAINT_BRUSH_INPUT_PRESSURE, 0);
  } else {
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE, minOpacity);
    mypaintBrush.setMappingN(MYPAINT_BRUSH_SETTING_OPAQUE,
                             MYPAINT_BRUSH_INPUT_PRESSURE, 2);
    mypaintBrush.setMappingPoint(MYPAINT_BRUSH_SETTING_OPAQUE,
                                 MYPAINT_BRUSH_INPUT_PRESSURE, 0, 0.0, 0.0);
    mypaintBrush.setMappingPoint(MYPAINT_BRUSH_SETTING_OPAQUE,
                                 MYPAINT_BRUSH_INPUT_PRESSURE, 1, 1.0,
                                 maxOpacity - minOpacity);
  }

  if (m_modifierLockAlpha.getValue()) {
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_LOCK_ALPHA, 1.0);
  }
}

void FullColorBrushTool::applyToonzBrushSettings(mypaint::Brush &mypaintBrush) {
  TMyPaintBrushStyle *mypaintStyle = getBrushStyle();

  if (mypaintStyle) {
    const double precision = 1e-5;

    double modifierSize    = m_modifierSize.getValue() * log(2.0);
    double modifierOpacity = 0.01 * m_modifierOpacity.getValue();
    bool modifierEraser    = m_modifierEraser.getValue();
    bool modifierLockAlpha = m_modifierLockAlpha.getValue();

    TPixelD color = PixelConverter<TPixelD>::from(m_currentColor);
    double colorH = 0.0;
    double colorS = 0.0;
    double colorV = 0.0;
    RGB2HSV(color.r, color.g, color.b, &colorH, &colorS, &colorV);

    mypaintBrush.fromBrush(mypaintStyle->getBrush());

    float baseSize =
        mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
    float baseOpacity = mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE);

    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                              baseSize + modifierSize);
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE,
                              baseOpacity * modifierOpacity);
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_H, colorH / 360.0);
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_S, colorS);
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_V, colorV);

    if (modifierEraser) {
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_ERASER, 1.0);
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_LOCK_ALPHA, 0.0);
    } else if (modifierLockAlpha) {
      // lock-alpha already disables eraser
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_LOCK_ALPHA, 1.0);
    }
  } else {
    applyClassicToonzBrushSettings(mypaintBrush);
  }
}

//==========================================================================================================

FullColorBrushToolNotifier::FullColorBrushToolNotifier(FullColorBrushTool *tool)
    : m_tool(tool) {}

//------------------------------------------------------------

void FullColorBrushToolNotifier::onActivate() {
  if (TTool::Application *app = m_tool->getApplication()) {
    if (TXshLevelHandle *levelHandle = app->getCurrentLevel()) {
      bool ret = connect(levelHandle, SIGNAL(xshCanvasSizeChanged()), this,
                         SLOT(onCanvasSizeChanged()));
      assert(ret);
    }
    if (TPaletteHandle *paletteHandle = app->getCurrentPalette()) {
      bool ret = connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
                         SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(colorStyleSwitched()), this,
                           SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(paletteSwitched()), this,
                           SLOT(onColorStyleChanged()));
      assert(ret);
    }
  }
}

//------------------------------------------------------------

void FullColorBrushToolNotifier::onDeactivate() {
  if (TTool::Application *app = m_tool->getApplication()) {
    if (TXshLevelHandle *levelHandle = app->getCurrentLevel()) {
      bool ret = disconnect(levelHandle, SIGNAL(xshCanvasSizeChanged()), this,
                            SLOT(onCanvasSizeChanged()));
      assert(ret);
    }
    if (TPaletteHandle *paletteHandle = app->getCurrentPalette()) {
      bool ret = disconnect(paletteHandle, SIGNAL(colorStyleChanged(bool)),
                            this, SLOT(onColorStyleChanged()));
      ret = ret && disconnect(paletteHandle, SIGNAL(colorStyleSwitched()), this,
                              SLOT(onColorStyleChanged()));
      ret = ret && disconnect(paletteHandle, SIGNAL(paletteSwitched()), this,
                              SLOT(onColorStyleChanged()));
      assert(ret);
    }
  }
}

//==========================================================================================================

FullColorBrushTool fullColorPencil("T_Brush");
