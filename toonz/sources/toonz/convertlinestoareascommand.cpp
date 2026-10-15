#include "convertlinestoareascommand.h"

#include "tapp.h"
#include "menubarcommandids.h"
#include "tools/toolutils.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/ttilesaver.h"
#include "toonz/ttileset.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/styleselection.h"
#include "toonzqt/tselectionhandle.h"
#include "ttoonzimage.h"

#include <QTimer>

#include <bitset>
#include <memory>

namespace {

using StyleMask = std::bitset<4096>;

enum class ConversionDirection { LineToArea, AreaToLine };

// Conversion can leave paint underneath solid ink to prevent antialiasing
// gaps (see Naa2TlvConverter::makeTlv). Test coverage in either direction.
bool convertPixel(TPixelCM32 &pixel, const StyleMask &styles,
                  ConversionDirection direction) {
  const bool toArea     = direction == ConversionDirection::LineToArea;
  const int source      = toArea ? pixel.getInk() : pixel.getPaint();
  const int destination = toArea ? pixel.getPaint() : pixel.getInk();
  const int maxTone     = TPixelCM32::getMaxTone();
  int coverage          = toArea ? maxTone - pixel.getTone() : pixel.getTone();
  if (source == 0 || !styles[source] || coverage == 0) return false;

  if (coverage == maxTone || destination == source) {
    // Hidden destination data contributes nothing. Matching style IDs combine
    // to full coverage while retaining the style's own opacity.
    coverage = maxTone;
  } else if (destination != 0) {
    // Two different contributing styles cannot share one destination slot.
    return false;
  }

  // Retain fractional coverage against transparency, including antialiased
  // edges.
  pixel = toArea ? TPixelCM32(0, source, coverage)
                 : TPixelCM32(source, 0, maxTone - coverage);
  return true;
}

void convertLineArea(const TRasterCM32P &ras, const StyleMask &styles,
                     ConversionDirection direction,
                     TTileSaverCM32 *saver = nullptr) {
  ras->lock();
  for (int y = 0; y < ras->getLy(); ++y) {
    TPixelCM32 *row = ras->pixels(y);
    for (int x = 0; x < ras->getLx(); ++x) {
      TPixelCM32 converted = row[x];
      if (!convertPixel(converted, styles, direction)) continue;
      if (saver) saver->save(TPoint(x, y));
      row[x] = converted;
    }
  }
  ras->unlock();
}

bool getTarget(TXshCell &cell, StyleMask &styles) {
  TApp *app = TApp::instance();
  if (app->getCurrentFrame()->isPlaying()) return false;

  cell                = TTool::getImageCell();
  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (!sl || sl->getType() != TZP_XSHLEVEL || sl->isReadOnly() ||
      !sl->isFid(cell.getFrameId()) || sl->isFrameReadOnly(cell.getFrameId()))
    return false;

  if (!app->getCurrentFrame()->isEditingLevel()) {
    TXsheet *xsheet = app->getCurrentXsheet()->getXsheet();
    TXshColumn *column =
        xsheet->getColumn(app->getCurrentColumn()->getColumnIndex());
    if (!column || column->isLocked()) return false;
  }

  TPaletteHandle *handle = app->getCurrentPalette();
  TPalette *palette      = handle->getPalette();
  if (!palette || palette != sl->getPalette() || palette->isCleanupPalette())
    return false;

  const auto addStyle = [&](int id) {
    if (id > 0 && id < palette->getStyleCount() &&
        id <= TPixelCM32::getMaxInk())
      styles.set(id);
  };
  TStyleSelection *selection = dynamic_cast<TStyleSelection *>(
      app->getCurrentSelection()->getSelection());
  if (selection && !selection->isEmpty()) {
    if (!selection->getPaletteHandle() || selection->getPalette() != palette)
      return false;
    const int pageIndex = selection->getPageIndex();
    if (pageIndex < 0 || pageIndex >= palette->getPageCount()) return false;
    TPalette::Page *page = palette->getPage(pageIndex);
    for (int index : selection->getIndicesInPage()) {
      if (index >= 0 && index < page->getStyleCount())
        addStyle(page->getStyleId(index));
    }
  } else {
    // A shortcut used from the viewer acts on the current drawing style.
    addStyle(handle->getStyleIndex());
  }
  return styles.any();
}

class ConvertLineAreaUndo final : public ToolUtils::TRasterUndo {
  StyleMask m_styles;
  ConversionDirection m_direction;

public:
  ConvertLineAreaUndo(TTileSetCM32 *tiles, TXshSimpleLevel *sl,
                      const TFrameId &fid, const StyleMask &styles,
                      ConversionDirection direction)
      : TRasterUndo(tiles, sl, fid, false, false, nullptr, false)
      , m_styles(styles)
      , m_direction(direction) {}

  void notify() const {
    m_level->touchFrame(m_frameId);
    notifyImageChanged();
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void undo() const override {
    if (!getImage()) return;
    TRasterUndo::undo();
    notify();
  }

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image || !image->getRaster()) return;
    convertLineArea(image->getRaster(), m_styles, m_direction);
    notify();
  }

  int getSize() const override {
    return TRasterUndo::getSize() + sizeof(m_styles) + sizeof(m_direction);
  }

  QString getToolName() override {
    return m_direction == ConversionDirection::LineToArea
               ? QObject::tr("Convert Lines to Areas")
               : QObject::tr("Convert Areas to Lines");
  }
};

class ConvertLineAreaCommand final : public MenuItemHandler {
  ConversionDirection m_direction;

public:
  ConvertLineAreaCommand(CommandId id, ConversionDirection direction)
      : MenuItemHandler(id), m_direction(direction) {}

  void execute() override {
    TXshCell cell;
    StyleMask styles;
    if (!getTarget(cell, styles)) return;

    TToonzImageP image = cell.getImage(true);
    if (!image || !image->getRaster()) return;
    TRasterCM32P ras = image->getRaster();
    auto tiles       = std::make_unique<TTileSetCM32>(ras->getSize());
    TTileSaverCM32 saver(ras, tiles.get());
    convertLineArea(ras, styles, m_direction, &saver);
    if (tiles->getTileCount() == 0) return;

    auto undo = std::make_unique<ConvertLineAreaUndo>(
        tiles.release(), cell.getSimpleLevel(), cell.getFrameId(), styles,
        m_direction);
    undo->notify();
    TUndoManager::manager()->add(undo.release());
  }
};

ConvertLineAreaCommand convertLinesToAreasCommand(
    MI_ConvertLinesToAreas, ConversionDirection::LineToArea);
ConvertLineAreaCommand convertAreasToLinesCommand(
    MI_ConvertAreasToLines, ConversionDirection::AreaToLine);

}  // namespace

void initConvertLineAreaCommands(QAction *action, QAction *reverseAction) {
  TApp *app = TApp::instance();
  action->setToolTip(QObject::tr(
      "Convert selected styles' lines (ink) to areas (paint) in the current "
      "Toonz Raster drawing, preserving antialiasing and transparency. Mixed "
      "edge pixels with a different area style are left unchanged."));
  reverseAction->setToolTip(QObject::tr(
      "Convert selected styles' areas (paint) to lines (ink) in the current "
      "Toonz Raster drawing, preserving antialiasing and transparency. Mixed "
      "edge pixels with a different line style are left unchanged."));
  // Palette clicks update the style and selection in separate steps. Refresh
  // after both changes so Ctrl/Shift selections are evaluated together.
  const auto update = []() {
    TXshCell cell;
    StyleMask styles;
    const bool enabled = getTarget(cell, styles);
    CommandManager::instance()->enable(MI_ConvertLinesToAreas, enabled);
    CommandManager::instance()->enable(MI_ConvertAreasToLines, enabled);
  };
  const auto scheduleUpdate = [action, update]() {
    QTimer::singleShot(0, action, update);
  };
  QObject::connect(app->getCurrentSelection(),
                   &TSelectionHandle::selectionSwitched, action,
                   scheduleUpdate);
  QObject::connect(app->getCurrentSelection(),
                   &TSelectionHandle::selectionChanged, action, scheduleUpdate);
  QObject::connect(app->getCurrentLevel(), &TXshLevelHandle::xshLevelSwitched,
                   action, scheduleUpdate);
  QObject::connect(app->getCurrentFrame(), &TFrameHandle::frameSwitched, action,
                   scheduleUpdate);
  QObject::connect(app->getCurrentFrame(),
                   &TFrameHandle::isPlayingStatusChanged, action,
                   scheduleUpdate);
  QObject::connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
                   action, scheduleUpdate);
  QObject::connect(app->getCurrentPalette(), &TPaletteHandle::paletteSwitched,
                   action, scheduleUpdate);
  QObject::connect(app->getCurrentPalette(),
                   &TPaletteHandle::colorStyleSwitched, action, scheduleUpdate);
  QObject::connect(app->getCurrentLevel(), &TXshLevelHandle::xshLevelChanged,
                   action, scheduleUpdate);
  QObject::connect(app->getCurrentColumn(), &TColumnHandle::columnIndexSwitched,
                   action, scheduleUpdate);
  QObject::connect(app->getCurrentFrame(), &TFrameHandle::frameTypeChanged,
                   action, scheduleUpdate);
  QObject::connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
                   action, scheduleUpdate);
  update();
}
