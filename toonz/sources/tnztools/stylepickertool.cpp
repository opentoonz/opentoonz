#include "stylepickertool.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/stylepicker.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/paramfield.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/stage2.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/preferences.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/dpiscale.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcolumnfx.h"

// TnzCore includes
#include "drawutil.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tundo.h"
#include "tmsgcore.h"

// Qt includes
#include <QTimer>

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

namespace {

// Collects the columns of all the levels feeding the fx
void collectInputColumns(TFx* fx, std::set<int>& columns) {
  if (TLevelColumnFx* columnFx = dynamic_cast<TLevelColumnFx*>(fx)) {
    columns.insert(columnFx->getColumnIndex());
    return;
  }
  for (int i = 0; i < fx->getInputPortCount(); i++)
    if (TFx* inputFx = fx->getInputPort(i)->getFx())
      collectInputColumns(inputFx, columns);
}

}  // namespace

//========================================================================
// Pick Style Tool
//------------------------------------------------------------------------

StylePickerTool::StylePickerTool()
    : TTool("T_StylePicker")
    , m_currentStyleId(0)
    , m_colorType("Mode:")
    , m_replaceStyle("Replace Style", false)
    , m_passivePick("Passive Pick", false)
    , m_organizePalette("Organize Palette", false) {
  m_prop.bind(m_colorType);
  m_colorType.addValue(AREAS);
  m_colorType.addValue(LINES);
  m_colorType.addValue(ALL);
  m_colorType.setId("Mode");
  bind(TTool::CommonLevels);

  m_prop.bind(m_passivePick);
  m_passivePick.setId("PassivePick");

  m_prop.bind(m_replaceStyle);
  m_replaceStyle.setId("ReplaceStyle");

  m_prop.bind(m_organizePalette);
  m_organizePalette.setId("OrganizePalette");

  // Lets Fx Settings pick color indexes from the viewer
  StringParamField::setStylePickHandler(
      [this](TFx* fx, std::function<void(int)> onPicked) {
        startOneShot(fx, onPicked);
      });
}

void StylePickerTool::onActivate() {
  TPaletteHandle* ph = getApplication()->getCurrentPalette();
  if (ph) {
    bool ret = connect(ph, &TPaletteHandle::paletteSwitched, this,
                       &StylePickerTool::onPaletteSwitched);
    assert(ret);
  }
}

void StylePickerTool::onDeactivate() {
  QObject::disconnect(nullptr, &TPaletteHandle::paletteSwitched, this,
                      &StylePickerTool::onPaletteSwitched);
  m_oneShot = nullptr;
}

void StylePickerTool::leftButtonDown(const TPointD& pos, const TMouseEvent& e) {
  if (m_oneShot) {
    m_oneShotStyleId = pickConnectedStyleId(pos, e);
    return;
  }
  m_oldStyleId = m_currentStyleId =
      getApplication()->getCurrentLevelStyleIndex();
  pick(pos, e, false);
}

void StylePickerTool::leftButtonDrag(const TPointD& pos, const TMouseEvent& e) {
  if (m_oneShot) return;
  pick(pos, e);
}

void StylePickerTool::leftButtonUp(const TPointD& pos, const TMouseEvent& e) {
  if (!m_oneShot || m_oneShotStyleId <= 0) return;

  std::function<void(int)> onPicked = m_oneShot;
  m_oneShot                         = nullptr;
  onPicked(m_oneShotStyleId);

  // Restore the previous tool once the viewer has finished the release.
  QTimer::singleShot(0, this, [] {
    TTool::getApplication()->getCurrentTool()->unsetPseudoTool();
  });
}

void StylePickerTool::startOneShot(TFx* fx, std::function<void(int)> onPicked) {
  m_oneShotColumns.clear();
  collectInputColumns(fx, m_oneShotColumns);
  if (m_oneShotColumns.empty()) {
    DVGui::warning(tr("No levels are connected to this FX."));
    return;
  }

  TFxHandle* fxHandle = getApplication()->getCurrentFx();

  // Switching to another fx, or clicking the empty schematic area, cancels
  connect(fxHandle, &TFxHandle::fxSwitched, this,
          &StylePickerTool::cancelOneShot, Qt::UniqueConnection);

  ToolHandle* toolHandle = getApplication()->getCurrentTool();
  m_oneShot              = onPicked;
  m_oneShotStyleId       = -1;
  toolHandle->setPseudoTool(T_StylePicker);
  // The picker is not available for the current level
  if (toolHandle->getTool() != this) {
    m_oneShot = nullptr;
    toolHandle->unsetPseudoTool();
  }
}

void StylePickerTool::pick(const TPointD& pos, const TMouseEvent& e,
                           bool isDragging) {
  int columnIndex;
  int styleId = pickStyleId(pos, e, columnIndex);
  if (styleId < 0) return;

  // MultiLayerStylePicker: move to the level and column the style was found in
  if (columnIndex >= 0) {
    int frame     = getApplication()->getCurrentFrame()->getFrame();
    TXshCell cell = getApplication()->getCurrentXsheet()->getXsheet()->getCell(
        frame, columnIndex);
    getApplication()->getCurrentLevel()->setLevel(cell.getSimpleLevel());
    getApplication()->getCurrentColumn()->setColumnIndex(columnIndex);
    if (getApplication()->getCurrentSelection()->getSelection())
      getApplication()->getCurrentSelection()->getSelection()->selectNone();
    getApplication()->setCurrentLevelStyleIndex(styleId, !isDragging);
    return;
  }

  /*--- Styleを選択している場合は選択を解除する ---*/
  TSelection* selection =
      TTool::getApplication()->getCurrentSelection()->getSelection();
  if (selection) {
    TStyleSelection* styleSelection = dynamic_cast<TStyleSelection*>(selection);
    if (styleSelection) styleSelection->selectNone();
  }

  // When clicking and switching between studio palette and level palette, the
  // signal broadcastColorStyleSwitched is not emitted if the picked style is
  // previously selected one.
  // Therefore here I set the "forceEmit" flag to true in order to emit the
  // signal whenever the picking with mouse press.
  getApplication()->setCurrentLevelStyleIndex(styleId, !isDragging);
}

int StylePickerTool::pickStyleId(const TPointD& pos, const TMouseEvent& e,
                                 int& columnIndex) {
  columnIndex = -1;
  // Area = 0, Line = 1, All = 2
  int modeValue = m_colorType.getIndex();

  //------------------------------------
  // MultiLayerStylePicker
  /*---
                  PickしたStyleId = 0、かつ
                  Preference で MultiLayerStylePickerが有効、かつ
                  Scene編集モード、かつ
                  下のカラムから拾った色がTransparentでない場合、
                  → カレントLevelを移動する。
  ---*/
  if (Preferences::instance()->isMultiLayerStylePickerEnabled() &&
      getApplication()->getCurrentFrame()->isEditingScene()) {
    int superPickedColumnId =
        getViewer()->posToColumnIndex(e.m_pos, 10.0, false);

    if (superPickedColumnId >= 0 /*-- 何かColumnに当たった場合 --*/
        && getApplication()->getCurrentColumn()->getColumnIndex() !=
               superPickedColumnId) /*-- かつ、Current Columnでない場合 --*/
    {
      /*-- そのColumnからPickを試みる --*/
      int superPicked_StyleId = pickFromColumn(superPickedColumnId, e);
      /*-- 何かStyleが拾えて、Transparentでない場合 --*/
      if (superPicked_StyleId > 0) {
        columnIndex = superPickedColumnId;
        return superPicked_StyleId;
      }
    }
  }
  /*-- MultiLayerStylePicker ここまで --*/
  //------------------------------------
  TImageP image    = getImage(false);
  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  TXshSimpleLevel* level =
      getApplication()->getCurrentLevel()->getSimpleLevel();
  if ((!ti && !vi) || !level) return -1;

  /*-- 画面外をpickしても拾えないようにする --*/
  if (!m_viewer->getGeometry().contains(pos)) return -1;

  TAffine aff     = getViewer()->getViewMatrix() * getCurrentColumnMatrix();
  double scale2   = aff.det();
  int subsampling = level->getImageSubsampling(getCurrentFid());
  StylePicker picker(getViewer()->viewerWidget(), image);
  int styleId =
      picker.pickStyleId(TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5),
                         10.0, scale2, modeValue);

  if (styleId < 0) return -1;

  if (modeValue == 1)  // LINES
  {
    /*-- pickLineモードのとき、取得Styleが0の場合はカレントStyleを変えない。
     * --*/
    if (styleId == 0) return -1;
    /*--
     * pickLineモードのとき、PurePaintの部分をクリックしてもカレントStyleを変えない
     * --*/
    if (ti && picker.pickTone(TScale(1.0 / subsampling) * pos +
                              TPointD(-0.5, -0.5)) == 255)
      return -1;
  }
  return styleId;
}

int StylePickerTool::pickFromColumn(int columnIndex, const TMouseEvent& e) {
  int frame     = getApplication()->getCurrentFrame()->getFrame();
  TXshCell cell = getApplication()->getCurrentXsheet()->getXsheet()->getCell(
      frame, columnIndex);
  TImageP image          = cell.getImage(false).getPointer();
  TXshSimpleLevel* level = cell.getSimpleLevel();
  if ((!TToonzImageP(image) && !TVectorImageP(image)) || !level) return -1;

  TPointD pos =
      getColumnMatrix(columnIndex).inv() * getViewer()->winToWorld(e.m_pos);
  TPointD dpiScale = getCurrentDpiScale(level, getCurrentFid());
  pos.x /= dpiScale.x;
  pos.y /= dpiScale.y;

  TAffine aff     = getViewer()->getViewMatrix() * getColumnMatrix(columnIndex);
  int subsampling = level->getImageSubsampling(cell.getFrameId());
  TPointD pickPos = TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5);
  int modeValue   = m_colorType.getIndex();
  StylePicker picker(getViewer()->viewerWidget(), image);
  int styleId = picker.pickStyleId(pickPos, 10.0, aff.det(), modeValue);

  // Lines mode: the ink of a pure paint pixel is not visible
  if (modeValue == 1 && TToonzImageP(image) && picker.pickTone(pickPos) == 255)
    return -1;
  return styleId;
}

// Picks from the top-most column under the cursor that feeds the fx
int StylePickerTool::pickConnectedStyleId(const TPointD& pos,
                                          const TMouseEvent& e) {
  if (getApplication()->getCurrentFrame()->isEditingScene()) {
    std::vector<int> columns;  // bottom to top
    getViewer()->posToColumnIndexes(e.m_pos, columns, 10.0, false);
    for (auto it = columns.rbegin(); it != columns.rend(); ++it) {
      if (!m_oneShotColumns.count(*it)) continue;
      int styleId = pickFromColumn(*it, e);
      if (styleId > 0) return styleId;
    }
    return -1;
  }

  // Level editing: only the current level, if it feeds the fx
  int currentColumn = getApplication()->getCurrentColumn()->getColumnIndex();
  if (!m_oneShotColumns.count(currentColumn)) return -1;
  int columnIndex;
  return pickStyleId(pos, e, columnIndex);
}

void StylePickerTool::cancelOneShot() {
  if (!m_oneShot) return;
  m_oneShot = nullptr;
  getApplication()->getCurrentTool()->unsetPseudoTool();
}

QString StylePickerTool::updateEnabled(int rowIndex, int columnIndex) {
  // The one-shot pick reads from the connected columns, not the current one
  if (m_oneShot) return (enable(true), QString());
  return TTool::updateEnabled(rowIndex, columnIndex);
}

void StylePickerTool::mouseMove(const TPointD& pos, const TMouseEvent& e) {
  if (!m_passivePick.getValue()) return;
  /*--- PassiveにStyleを拾う機能 ---*/
  PaletteController* controller =
      TTool::getApplication()->getPaletteController();

  TImageP image    = getImage(false);
  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  TXshSimpleLevel* level =
      getApplication()->getCurrentLevel()->getSimpleLevel();
  if ((!ti && !vi) || !level || !m_viewer->getGeometry().contains(pos)) {
    controller->notifyStylePassivePicked(-1, -1, -1);
    return;
  }

  TAffine aff     = getViewer()->getViewMatrix() * getCurrentColumnMatrix();
  double scale2   = aff.det();
  int subsampling = level->getImageSubsampling(getCurrentFid());
  StylePicker picker(getViewer()->viewerWidget(), image);
  TPointD pickPos(TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5));
  int inkStyleId   = picker.pickStyleId(pickPos, 10.0, scale2, 1);
  int paintStyleId = picker.pickStyleId(pickPos, 10.0, scale2, 0);
  int tone         = picker.pickTone(pickPos);
  controller->notifyStylePassivePicked(inkStyleId, paintStyleId, tone);
}

int StylePickerTool::getCursorId() const {
  int ret;

  if (!Preferences::instance()->isMultiLayerStylePickerEnabled()) {
    TImageP img      = getImage(false);
    TVectorImageP vi = img;
    TToonzImageP ti  = img;

    if (!vi && !ti) return ToolCursor::CURSOR_NO;
  }

  /* in case the "organize palette" option is active */
  if (m_organizePalette.getValue())
    ret = ToolCursor::PickerCursorOrganize;
  else if (m_colorType.getValue() == LINES)
    ret = ToolCursor::PickerCursorLine;
  else if (m_colorType.getValue() == AREAS)
    ret = ToolCursor::PickerCursorArea;
  else  // line&areas
    ret = ToolCursor::PickerCursor;

  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    ret = ret | ToolCursor::Ex_Negate;
  return ret;
}

TPalette* StylePickerTool::getPal() {
  TXshLevel* level = getApplication()->getCurrentLevel()->getLevel();
  if (!level) {
    DVGui::error(tr("No current level."));
    return nullptr;
  }
  if (level->getType() != PLI_XSHLEVEL && level->getType() != TZP_XSHLEVEL &&
      (m_organizePalette.getValue() ? level->getType() != PLT_XSHLEVEL
                                    : true)) {
    DVGui::error(tr("Current level has no available palette."));
    return nullptr;
  }
  TPalette* pal = nullptr;
  if (level->getType() == PLT_XSHLEVEL)
    pal = level->getPaletteLevel()->getPalette();
  else
    pal = level->getSimpleLevel()->getPalette();

  if (!pal) {
    DVGui::error(tr("No available palette."));
    return nullptr;
  }
  return pal;
}

bool StylePickerTool::onPropertyChanged(std::string propertyName) {
  if (propertyName == m_organizePalette.getName()) {
    if (m_organizePalette.getValue()) {
      if (!startOrganizePalette()) {
        m_organizePalette.setValue(false);
        getApplication()->getCurrentTool()->notifyToolChanged();
        return false;
      } else
        m_replaceStyle.setValue(false);
    } else {
      std::cout << "End Organize Palette" << std::endl;
    }
  } else if (propertyName == m_replaceStyle.getName()) {
    if (m_replaceStyle.getValue()) {
      if (!getPal()) {
        m_replaceStyle.setValue(false);
        return false;
      } else
        m_organizePalette.setValue(false);
    } else {
      TXshLevel* xl = TTool::getApplication()->getCurrentLevel()->getLevel();
      if (xl) {
        std::vector<TFrameId> fids;
        xl->getFids(fids);
        for (TFrameId fid : fids)
          IconGenerator::instance()->invalidate(xl, fid);
      }
    }
  }
  return true;
}

/* Check if the organizing operation is available */
bool StylePickerTool::startOrganizePalette() {
  /* palette should have more than one page to organize */
  TPalette* pal = getPal();
  if (!pal) return false;
  if (pal->getPageCount() < 2) {
    DVGui::error(tr("Palette to be organized must have more than one page."));
    return false;
  }

  std::cout << "Start Organize Palette" << std::endl;

  return true;
}

/*
  If the working palette is changed, then deactivate the "organize palette"
  and "replace style" toggle.
*/
void StylePickerTool::onPaletteSwitched() {
  m_organizePalette.setValue(false);
  m_replaceStyle.setValue(false);
  getApplication()->getCurrentTool()->notifyToolChanged();
  return;
}
//-------------------------------------------------------------------------------------------------------

void StylePickerTool::updateTranslation() {
  m_colorType.setQStringName(tr("Mode:"));
  m_colorType.setItemUIName(LINES, tr("Lines"));
  m_colorType.setItemUIName(AREAS, tr("Areas"));
  m_colorType.setItemUIName(ALL, tr("Lines & Areas"));
  m_passivePick.setQStringName(tr("Passive Pick"));
  m_organizePalette.setQStringName(tr("Organize Palette"));
}

//-------------------------------------------------------------------------------------------------------

StylePickerTool stylePickerTool;
