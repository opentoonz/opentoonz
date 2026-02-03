#include "rulertool.h"

#include "tools/toolhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "tools/toolutils.h"

#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tools/cursors.h"
#include "trasterimage.h"
#include "tgl.h"
#include "toonz/stage2.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/icongenerator.h"
#include "tenv.h"

#include "tconst.h"
#include "toonz/tframehandle.h"

//----------------------------------------------------------------------------------------------

RulerTool::RulerTool()
    : TTool("T_Ruler")
    , m_firstPos(TConst::nowhere)
    , m_secondPos(TConst::nowhere)
    , m_mousePos(TConst::nowhere)
    , m_dragMode(MakeNewRuler)
    , m_justClicked(false) {
  bind(TTool::AllTargets);
}

//---------------------------------------------------------

void RulerTool::setToolOptionsBox(RulerToolOptionsBox *toolOptionsBox) {
  m_toolOptionsBox.push_back(toolOptionsBox);
}

//----------------------------------------------------------------------------------------------

void RulerTool::onImageChanged() {
  /*--位置をリセット--*/
  m_firstPos  = TConst::nowhere;
  m_secondPos = TConst::nowhere;

  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++) {
    m_toolOptionsBox[i]->resetValues();
  }
}

//----------------------------------------------------------------------------------------------

void RulerTool::draw() {
  /*--- 始点が設定されていたら、描画 ---*/
  if (m_firstPos != TConst::nowhere) {
    tglColor((m_dragMode == MoveFirstPos) ? TPixel32(51, 204, 26)
                                          : TPixel32::Red);
    tglDrawCircle(m_firstPos, 4);
    tglDrawCircle(m_firstPos, 2);
    /*--- 終点が設定されていたら、その区間を描画 ---*/
    if (m_secondPos != TConst::nowhere) {
      tglColor((m_dragMode == MoveRuler) ? TPixel32(51, 204, 26)
                                         : TPixel32::Red);
      glBegin(GL_LINE_STRIP);
      tglVertex(m_firstPos);
      tglVertex(m_secondPos);
      glEnd();
      tglColor((m_dragMode == MoveSecondPos) ? TPixel32(51, 204, 26)
                                             : TPixel32::Red);
      tglDrawCircle(m_secondPos, 4);
    }
  }
}

//----------------------------------------------------------------------------------------------

void RulerTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  if (m_dragMode == MakeNewRuler) m_justClicked = true;
}

//----------------------------------------------------------------------------------------------

void RulerTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  /*-- 最初のドラッグ --*/
  if (m_justClicked && m_dragMode == MakeNewRuler) {
    m_firstPos    = m_mousePos;
    m_justClicked = false;
  }

  /*-- ドラッグモードがMakeNewRuler/MoveSecondPosのとき、secondPosを更新 --*/
  if (m_dragMode == MakeNewRuler || m_dragMode == MoveSecondPos) {
    /*-- Shiftキーが押されていたら：0,45,90度に角度固定の直線を引く --*/
    if (e.isShiftPressed())
      m_secondPos = getHVCoordinatedPos(pos, m_firstPos);
    else
      m_secondPos = pos;
  }

  /*-- ドラッグモードがMoveFirstPosのとき、firstPosを更新 --*/
  else if (m_dragMode == MoveFirstPos) {
    /*-- Shiftキーが押されていたら：0,45,90度に角度固定の直線を引く --*/
    if (e.isShiftPressed())
      m_firstPos = getHVCoordinatedPos(pos, m_secondPos);
    else
      m_firstPos = pos;
  }

  /*-- Ruler全体を移動するモード --*/
  else {
    TPointD d = pos - m_mousePos;
    m_firstPos += d;
    m_secondPos += d;
    /*-- マウス位置を更新 --*/
    m_mousePos = pos;
  }

  updateToolOption(); /*-- ToolOptionの表示を更新 --*/

  invalidate();
}

//----------------------------------------------------------------------------------------------

void RulerTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  m_justClicked = false;
  invalidate();
}

//----------------------------------------------------------------------------------------------

void RulerTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  /*-- マウス位置を更新 --*/
  m_mousePos = pos;

  /*-- マウスクリックしてたら無視 --*/
  if (e.isLeftButtonPressed()) return;

  /*--
   * マウスがRulerの端点/Rulerそのものに近い場合はドラッグモードを変更する。--*/
  if (m_firstPos != TConst::nowhere && tdistance2(pos, m_firstPos) < 16)
    m_dragMode = MoveFirstPos;
  else if (m_secondPos != TConst::nowhere && tdistance2(pos, m_secondPos) < 16)
    m_dragMode = MoveSecondPos;
  else if (isNearRuler())
    m_dragMode = MoveRuler;
  else
    m_dragMode = MakeNewRuler;

  invalidate();
}

//----------------------------------------------------------------------------------------------

void RulerTool::onActivate() {
  /*-- 位置をリセット --*/
  m_firstPos  = TConst::nowhere;
  m_secondPos = TConst::nowhere;

  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++) {
    m_toolOptionsBox[i]->resetValues();
  }
}

//----------------------------------------------------------------------------------------------

int RulerTool::getCursorId() const {
  if (m_dragMode == MakeNewRuler)
    return ToolCursor::RulerNewCursor;
  else
    return ToolCursor::RulerModifyCursor;
}

//----------------------------------------------------------------------------------------------
/*-- ToolOptionの表示を更新 --*/
void RulerTool::updateToolOption() {
  TTool::Application *app    = TTool::getApplication();
  TFrameHandle *currentFrame = app->getCurrentFrame();

  double x, y, w, h, a, l;

  /*--
   * Level編集モードのとき、そのLevelのDPIに合わせてInchの値を得る。Pixelの値も出力する。
   * --*/
  if (currentFrame->isEditingLevel()) {
    TXshLevelHandle *currentLevel = app->getCurrentLevel();
    TXshLevel *xl                 = currentLevel->getLevel();
    if (xl) {
      TXshSimpleLevel *sl = xl->getSimpleLevel();
      if (sl) {
        int subsampling = sl->getImageSubsampling(getCurrentFid());

        TPointD dpiScale = getViewer()->getDpiScale();
        TPointD pp1 =
            TPointD(m_firstPos.x / dpiScale.x, m_firstPos.y / dpiScale.y);
        TPointD pp2 =
            TPointD(m_secondPos.x / dpiScale.x, m_secondPos.y / dpiScale.y);

        TPointD p1 = TScale(1.0 / subsampling) * pp1 + TPointD(-0.5, -0.5);
        TPointD p2 = TScale(1.0 / subsampling) * pp2 + TPointD(-0.5, -0.5);

        TImageP image = getImage(false);

        TPoint pix1, pix2;

        TToonzImageP ti  = image;
        TRasterImageP ri = image;

        if (ti || ri) {
          if (ti) {
            TDimension size = ti->getSize();
            pix1            = TPoint(tround(0.5 * size.lx + p1.x),
                          tround(0.5 * size.ly + p1.y));
            pix2 = TPoint(tround(0.5 * size.lx + p2.x),
                          tround(0.5 * size.ly + p2.y));
          } else if (ri) {
            TDimension size = ri->getRaster()->getSize();
            pix1            = TPoint(tround(0.5 * size.lx + p1.x),
                          tround(0.5 * size.ly + p1.y));
            pix2 = TPoint(tround(0.5 * size.lx + p2.x),
                          tround(0.5 * size.ly + p2.y));
          }
          int xPix, yPix, wPix, hPix;
          TPointD dpi = sl->getDpi(getCurrentFid());
          xPix        = pix1.x;
          yPix        = pix1.y;
          wPix        = pix2.x - pix1.x;
          hPix        = pix2.y - pix1.y;

          x = (double)xPix / dpi.x;
          y = (double)yPix / dpi.y;
          w = (double)wPix / dpi.x;
          h = (double)hPix / dpi.y;
          a = atan2(h, w) * 180.0 / 3.14159264;
          l = sqrt(w * w + h * h);

          for (int i = 0; i < (int)m_toolOptionsBox.size(); i++) {
            m_toolOptionsBox[i]->updateValues(true, x, y, w, h, a, l, xPix,
                                              yPix, wPix, hPix);
          }
          return;
        }
      }
    }
  }

  /*-- シーン編集モードのとき、Stage::inchで割った値がInch値となる --*/
  x = m_firstPos.x / Stage::inch;
  y = m_firstPos.y / Stage::inch;
  w = (m_secondPos.x - m_firstPos.x) / Stage::inch;
  h = (m_secondPos.y - m_firstPos.y) / Stage::inch;
  a = atan2(h, w) * 180.0 / 3.14159264;
  l = sqrt(w * w + h * h);

  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++) {
    m_toolOptionsBox[i]->updateValues(false, x, y, w, h, a, l);
  }
}

//----------------------------------------------------------------------------------------------
/*! 現在のマウス位置がRulerに十分近ければTrueを返す
*/
bool RulerTool::isNearRuler() {
  double a, b, c;

  TPointD vec = m_secondPos - m_firstPos;

  a = -vec.y;
  b = vec.x;
  c = -a * m_firstPos.x - b * m_firstPos.y;

  double k  = a * m_mousePos.x + b * m_mousePos.y + c;
  double d2 = k * k / (a * a + b * b);

  /*-- 距離が4より遠ければfalse --*/
  if (d2 > 16) return false;

  /*--
   * 垂線の足がRuler上にあるかを判断。Rulerを対角線とするRectにマウス位置が収まるかどうか
   * --*/
  TRectD rect = TRectD(m_firstPos, m_secondPos).enlarge(4);
  return rect.contains(m_mousePos);
}

//----------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
/*! 基準点に対し、マウス位置を0,45,90度にフィットさせた位置を返す
        斜め方向では、X/Yの値のうち絶対値の小さいほうに合わせる。
*/
//----------------------------------------------------------------------------------------------
TPointD RulerTool::getHVCoordinatedPos(TPointD p, TPointD centerPos) {
  TPointD vec = p - centerPos;
  double degree =
      (vec.x == 0.0) ? 90.0 : atan(vec.y / vec.x) * 180.0 / 3.1415926536;

  TPointD outPoint;

  if (degree <= -67.5) /*--垂直--*/
  {
    outPoint.x = centerPos.x;
    outPoint.y = p.y;
  } else if (degree < -22.5) /*--右斜め下--*/
  {
    if (std::abs(vec.x) > std::abs(vec.y))
      outPoint = centerPos + TPointD(-vec.y, vec.y);
    else
      outPoint = centerPos + TPointD(vec.x, -vec.x);
  } else if (degree <= 22.5) /*--水平--*/
  {
    outPoint.x = p.x;
    outPoint.y = centerPos.y;
  } else if (degree < 67.5) /*--右斜め上--*/
  {
    if (std::abs(vec.x) > std::abs(vec.y))
      outPoint = centerPos + TPointD(vec.y, vec.y);
    else
      outPoint = centerPos + TPointD(vec.x, vec.x);
  } else /*--再び垂直--*/
  {
    outPoint.x = centerPos.x;
    outPoint.y = p.y;
  }

  return outPoint;
}

//----------------------------------------------------------------------------------------------
RulerTool RulerTool;