#include "stylepickertool.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/stylepicker.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"

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

// TnzCore includes
#include "drawutil.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tundo.h"
#include "tmsgcore.h"

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

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
}

void StylePickerTool::leftButtonDown(const TPointD& pos, const TMouseEvent& e) {
  m_oldStyleId = m_currentStyleId =
      getApplication()->getCurrentLevelStyleIndex();
  pick(pos, e, false);
}

void StylePickerTool::leftButtonDrag(const TPointD& pos, const TMouseEvent& e) {
  pick(pos, e);
}

void StylePickerTool::pick(const TPointD& pos, const TMouseEvent& e,
                           bool isDragging) {
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
    double pickRange = 10.0;
    int superPickedColumnId =
        getViewer()->posToColumnIndex(e.m_pos, pickRange, false);

    if (superPickedColumnId >= 0 /*-- 何かColumnに当たった場合 --*/
        && getApplication()->getCurrentColumn()->getColumnIndex() !=
               superPickedColumnId) /*-- かつ、Current Columnでない場合 --*/
    {
      /*-- そのColumnからPickを試みる --*/
      int currentFrame = getApplication()->getCurrentFrame()->getFrame();
      TXshCell pickedCell =
          getApplication()->getCurrentXsheet()->getXsheet()->getCell(
              currentFrame, superPickedColumnId);
      TImageP pickedImage           = pickedCell.getImage(false).getPointer();
      TToonzImageP picked_ti        = pickedImage;
      TVectorImageP picked_vi       = pickedImage;
      TXshSimpleLevel* picked_level = pickedCell.getSimpleLevel();
      if ((picked_ti || picked_vi) && picked_level) {
        TPointD tmpMousePosition = getColumnMatrix(superPickedColumnId).inv() *
                                   getViewer()->winToWorld(e.m_pos);

        TPointD tmpDpiScale = getCurrentDpiScale(picked_level, getCurrentFid());

        tmpMousePosition.x /= tmpDpiScale.x;
        tmpMousePosition.y /= tmpDpiScale.y;

        TAffine aff =
            getViewer()->getViewMatrix() * getColumnMatrix(superPickedColumnId);
        double scale2 = aff.det();
        StylePicker superPicker(getViewer()->viewerWidget(), pickedImage);
        int picked_subsampling =
            picked_level->getImageSubsampling(pickedCell.getFrameId());
        int superPicked_StyleId = superPicker.pickStyleId(
            TScale(1.0 / picked_subsampling) * tmpMousePosition +
                TPointD(-0.5, -0.5),
            pickRange, scale2, modeValue);
        /*-- 何かStyleが拾えて、Transparentでない場合 --*/
        if (superPicked_StyleId > 0) {
          /*-- Levelの移動 --*/
          getApplication()->getCurrentLevel()->setLevel(picked_level);
          /*-- Columnの移動 --*/
          getApplication()->getCurrentColumn()->setColumnIndex(
              superPickedColumnId);
          /*-- 選択の解除 --*/
          if (getApplication()->getCurrentSelection()->getSelection())
            getApplication()
                ->getCurrentSelection()
                ->getSelection()
                ->selectNone();
          /*-- StyleIdの移動 --*/
          getApplication()->setCurrentLevelStyleIndex(superPicked_StyleId,
                                                      !isDragging);
          return;
        }
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
  if ((!ti && !vi) || !level) return;

  /*-- 画面外をpickしても拾えないようにする --*/
  if (!m_viewer->getGeometry().contains(pos)) return;

  TAffine aff     = getViewer()->getViewMatrix() * getCurrentColumnMatrix();
  double scale2   = aff.det();
  int subsampling = level->getImageSubsampling(getCurrentFid());
  StylePicker picker(getViewer()->viewerWidget(), image);
  int styleId =
      picker.pickStyleId(TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5),
                         10.0, scale2, modeValue);

  if (styleId < 0) return;

  if (modeValue == 1)  // LINES
  {
    /*-- pickLineモードのとき、取得Styleが0の場合はカレントStyleを変えない。
     * --*/
    if (styleId == 0) return;
    /*--
     * pickLineモードのとき、PurePaintの部分をクリックしてもカレントStyleを変えない
     * --*/
    if (ti && picker.pickTone(TScale(1.0 / subsampling) * pos +
                              TPointD(-0.5, -0.5)) == 255)
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
    return true;
  }
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
