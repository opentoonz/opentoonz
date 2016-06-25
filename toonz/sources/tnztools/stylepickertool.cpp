

#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/stylepicker.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"

#include "drawutil.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tundo.h"

#include "toonz/tpalettehandle.h"

#include "toonz/stage2.h"
#include "tproperty.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"
#include "tools/toolhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/preferences.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/dpiscale.h"
#include "toonz/palettecontroller.h"

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

namespace {

//========================================================================
// Pick Style Tool
//------------------------------------------------------------------------

class StylePickerTool : public TTool {
  int m_oldStyleId, m_currentStyleId;

  TEnumProperty m_colorType;
  TPropertyGroup m_prop;

  TBoolProperty m_passivePick;

public:
  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  StylePickerTool()
      : TTool("T_StylePicker")
      , m_currentStyleId(0)
      , m_colorType("Mode:")
      , m_passivePick("Passive Pick", false) {
    m_prop.bind(m_colorType);
    m_colorType.addValue(AREAS);
    m_colorType.addValue(LINES);
    m_colorType.addValue(ALL);
    m_colorType.setId("Mode");
    bind(TTool::CommonLevels);

    m_prop.bind(m_passivePick);
    m_passivePick.setId("PassivePick");
  }

  ToolType getToolType() const override { return TTool::LevelReadTool; }

  void draw() override {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_oldStyleId = m_currentStyleId =
        getApplication()->getCurrentLevelStyleIndex();
    pick(pos, e);
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    pick(pos, e);
  }

  void pick(const TPointD &pos, const TMouseEvent &e) {
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
      int superPickedColumnId = getViewer()->posToColumnIndex(
          e.m_pos, getPixelSize() * getPixelSize(), false);

      if (superPickedColumnId >= 0 /*-- 何かColumnに当たった場合 --*/
          &&
          getApplication()->getCurrentColumn()->getColumnIndex() !=
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
        TXshSimpleLevel *picked_level = pickedCell.getSimpleLevel();
        if ((picked_ti || picked_vi) && picked_level) {
          TPointD tmpMousePosition =
              getColumnMatrix(superPickedColumnId).inv() *
              getViewer()->winToWorld(e.m_pos);

          TPointD tmpDpiScale =
              getCurrentDpiScale(picked_level, getCurrentFid());

          tmpMousePosition.x /= tmpDpiScale.x;
          tmpMousePosition.y /= tmpDpiScale.y;

          StylePicker superPicker(pickedImage);
          int picked_subsampling =
              picked_level->getImageSubsampling(pickedCell.getFrameId());
          int superPicked_StyleId = superPicker.pickStyleId(
              TScale(1.0 / picked_subsampling) * tmpMousePosition +
                  TPointD(-0.5, -0.5),
              getPixelSize() * getPixelSize(), modeValue);
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
            getApplication()->setCurrentLevelStyleIndex(superPicked_StyleId);
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
    TXshSimpleLevel *level =
        getApplication()->getCurrentLevel()->getSimpleLevel();
    if ((!ti && !vi) || !level) return;

    /*-- 画面外をpickしても拾えないようにする --*/
    if (!m_viewer->getGeometry().contains(pos)) return;

    int subsampling = level->getImageSubsampling(getCurrentFid());
    StylePicker picker(image);
    int styleId = picker.pickStyleId(
        TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5),
        getPixelSize() * getPixelSize(), modeValue);

    if (styleId < 0) return;

    if (modeValue == 1)  // LINES
    {
      /*-- pickLineモードのとき、取得Styleが0の場合はカレントStyleを変えない。
       * --*/
      if (styleId == 0) return;
      /*--
       * pickLineモードのとき、PurePaintの部分をクリックしてもカレントStyleを変えない
       * --*/
      if (ti &&
          picker.pickTone(TScale(1.0 / subsampling) * pos +
                          TPointD(-0.5, -0.5)) == 255)
        return;
    }

    /*--- Styleを選択している場合は選択を解除する ---*/
    TSelection *selection =
        TTool::getApplication()->getCurrentSelection()->getSelection();
    if (selection) {
      TStyleSelection *styleSelection =
          dynamic_cast<TStyleSelection *>(selection);
      if (styleSelection) styleSelection->selectNone();
    }

    getApplication()->setCurrentLevelStyleIndex(styleId);
  }

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_passivePick.getValue()) return;
    /*--- PassiveにStyleを拾う機能 ---*/
    PaletteController *controller =
        TTool::getApplication()->getPaletteController();

    TImageP image    = getImage(false);
    TToonzImageP ti  = image;
    TVectorImageP vi = image;
    TXshSimpleLevel *level =
        getApplication()->getCurrentLevel()->getSimpleLevel();
    if ((!ti && !vi) || !level || !m_viewer->getGeometry().contains(pos)) {
      controller->notifyStylePassivePicked(-1, -1, -1);
      return;
    }

    int subsampling = level->getImageSubsampling(getCurrentFid());
    StylePicker picker(image);
    TPointD pickPos(TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5));
    int inkStyleId =
        picker.pickStyleId(pickPos, getPixelSize() * getPixelSize(), 1);
    int paintStyleId =
        picker.pickStyleId(pickPos, getPixelSize() * getPixelSize(), 0);
    int tone = picker.pickTone(pickPos);
    controller->notifyStylePassivePicked(inkStyleId, paintStyleId, tone);
  }

  void onActivate() override {}

  int getCursorId() const override {
    bool isBlackBG = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg;

    if (m_colorType.getValue() == LINES)
      return (isBlackBG) ? ToolCursor::PickerCursorWhiteLine
                         : ToolCursor::PickerCursorLine;
    else if (m_colorType.getValue() == AREAS)
      return (isBlackBG) ? ToolCursor::PickerCursorWhiteArea
                         : ToolCursor::PickerCursorArea;
    else  // line&areas
      return (isBlackBG) ? ToolCursor::PickerCursorWhite
                         : ToolCursor::PickerCursor;
  }

} stylePickerTool;
}
