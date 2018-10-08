

#include "frameheadgadget.h"

#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include "toonzqt/gutil.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "onionskinmaskgui.h"

#include "filmstrip.h"
#include "toonz/tframehandle.h"

#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"

#include <QPainter>
#include <QEvent>
#include <QMouseEvent>

#include <QAction>

namespace {

void enableOnionSkin(bool enable = true) {
  TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
  OnionSkinMask osm          = osmh->getOnionSkinMask();

  osm.enable(enable);
  osmh->setOnionSkinMask(osm);
  osmh->notifyOnionSkinMaskChanged();
}

bool isOnionSkinEnabled() {
  TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
  OnionSkinMask osm          = osmh->getOnionSkinMask();
  return osm.isEnabled() && !osm.isEmpty();
}

}  // namespace

FrameHeadGadget::FrameHeadGadget()
    : m_action(None)
    , m_highlightedFosFrame(-1)
    , m_buttonPressCellIndex(-1)
    , m_highlightedMosFrame(-1) {}

FrameHeadGadget::~FrameHeadGadget() {}

void FrameHeadGadget::draw(QPainter &p, const QColor &lightColor,
                           const QColor &darkColor) {
  // drawPlayingHead(p, lightColor, darkColor);
  if (CommandManager::instance()->getAction(MI_ShiftTrace)->isChecked())
    drawShiftTraceMarker(p);
  else if (Preferences::instance()->isOnionSkinEnabled())
    drawOnionSkinSelection(p, lightColor, darkColor);
}

void FrameHeadGadget::drawPlayingHead(QPainter &p, const QColor &lightColor,
                                      const QColor &darkColor) {
  int currentFrame = getCurrentFrame();
  int yy           = index2y(currentFrame);

  std::vector<QPointF> pts;
  int xa = 12;  // m_a;
  int xx = xa;  // m_a
  pts.push_back(QPointF(xx, yy));
  pts.push_back(QPointF(xx + 10, yy));
  pts.push_back(QPointF(xx + 13, yy + 1));
  pts.push_back(QPointF(xx + 17, yy + 5));
  pts.push_back(QPointF(xx + 13, yy + 9));
  pts.push_back(QPointF(xx + 10, yy + 10));
  pts.push_back(QPointF(xx, yy + 10));
  drawPolygon(p, pts, true, lightColor);
  p.setPen(darkColor);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) p.drawPoint(xx + 3 + i * 2, yy + 2 + j * 2);

  p.fillRect(QRect(xx - 7, yy + 1, 7, 9), QBrush(lightColor));
  p.setPen(darkColor);
  p.drawLine(xx - 9, yy + 5, xx - 8, yy + 5);

  p.setPen(Qt::black);
  p.drawLine(xx - 7, yy, xx - 1, yy);
  p.drawLine(xx - 7, yy + 10, xx - 1, yy + 10);

  p.setPen(Qt::black);
  xx = xa - 6;
  p.drawPoint(xx - 4, yy + 5);
  p.drawRect(QRect(xx - 6, yy, 4, 4));
  p.drawRect(QRect(xx - 6, yy + 6, 4, 4));

  p.fillRect(QRect(xx - 5, yy + 1, 3, 3), QBrush(lightColor));
  p.fillRect(QRect(xx - 5, yy + 7, 3, 3), QBrush(lightColor));
}

void FrameHeadGadget::drawOnionSkinSelection(QPainter &p,
                                             const QColor &lightColor,
                                             const QColor &darkColor) {
  int currentRow = getCurrentFrame();
  int xa         = 12;

  if (m_highlightedFosFrame >= 0 && m_highlightedFosFrame != currentRow) {
    int y = index2y(m_highlightedFosFrame) + 3;
    QRect rect(xa - 6, y + 1, 4, 4);
    p.setPen(Qt::blue);  // grey150);
    p.drawRect(rect);
    p.fillRect(rect.adjusted(1, 1, 0, 0), QBrush(lightColor));
  }

  OnionSkinMask osMask =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  if (!osMask.isEnabled()) return;

  int i;
  for (i = 0; i < osMask.getFosCount(); i++) {
    int fos = osMask.getFos(i);
    if (fos == currentRow) continue;
    int y = index2y(fos) + 3;
    QRect rect(xa - 6, y + 1, 4, 4);
    p.setPen(Qt::black);
    p.drawRect(rect);
    p.fillRect(rect.adjusted(1, 1, 0, 0), QBrush(darkColor));
  }

  int lastY;
  int xc       = xa - 10;
  int mosCount = osMask.getMosCount();
  for (i = 0; i < mosCount; i++) {
    int mos = osMask.getMos(i);
    int y   = 0 + index2y(currentRow + mos) + 3;
    QRect rect(xa - 12, y + 1, 4, 4);
    p.setPen(Qt::black);
    p.drawRect(rect);
    p.fillRect(rect.adjusted(1, 1, 0, 0), QBrush(lightColor));
    if (i > 0 || mos > 0) {
      int ya = y;
      int yb;
      if (i == 0 || mos > 0 && osMask.getMos(i - 1) < 0) {
        ya = index2y(currentRow) + 10;
        yb = index2y(currentRow + mos) + 4;
      } else
        yb = lastY + 5;
      p.setPen(Qt::black);
      p.drawLine(xc, ya, xc, yb);
    }
    lastY = y;
    if (mos < 0 && (i == mosCount - 1 || osMask.getMos(i + 1) > 0)) {
      int ya = index2y(currentRow);
      int yb = lastY + 5;
      p.setPen(Qt::black);
      p.drawLine(xc, ya, xc, yb);
    }
  }
}

bool FrameHeadGadget::eventFilter(QObject *obj, QEvent *e) {
  QWidget *viewer = dynamic_cast<QWidget *>(obj);

  if (e->type() == QEvent::MouseButtonPress) {
    m_action = None;

    // click
    if (m_highlightedFosFrame >= 0) {
      m_highlightedFosFrame = -1;
      viewer->update();
    }

    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    int frame               = y2index(mouseEvent->pos().y());
    m_buttonPressCellIndex  = frame;
    int currentFrame        = getCurrentFrame();
    int x                   = mouseEvent->pos().x();
    int y                   = mouseEvent->pos().y() - index2y(frame);

    if (x > 24 || x > 12 && frame != currentFrame) return false;
    if (y < 12) {
      bool isMosArea = x < 6;
      // click nel quadratino bianco.
      if (isMosArea) {
        // Mos
        bool on  = !isMos(frame);
        m_action = on ? ActivateMos : DeactivateMos;
        if (frame != currentFrame) setMos(frame, on);
      } else if (frame == currentFrame)
        m_action = MoveHead;
      else {
        // Fos
        bool on  = !isFos(frame);
        m_action = on ? ActivateFos : DeactivateFos;
        setFos(frame, on);
      }
    } else {
      // fuori dal quadratino bianco: muovo il frame corrente
      m_action = MoveHead;
      setCurrentFrame(frame);
    }
    viewer->update();
  } else if (e->type() == QEvent::MouseButtonRelease) {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    int frame               = y2index(mouseEvent->pos().y());
    int currentFrame        = getCurrentFrame();
    int x                   = mouseEvent->pos().x();
    int y                   = mouseEvent->pos().y() - index2y(frame);
    if (x > 24 || x > 12 && frame != currentFrame) return false;
    if (mouseEvent->button() == Qt::RightButton) return false;
    if (x < 12 && y < 12 && frame == currentFrame &&
        m_buttonPressCellIndex == frame) {
      enableOnionSkin(!isOnionSkinEnabled());
      viewer->update();
    }
  } else if (e->type() == QEvent::MouseMove) {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    int frame               = y2index(mouseEvent->pos().y());
    int x                   = mouseEvent->pos().x();
    int y                   = mouseEvent->pos().y() - index2y(frame);
    if (mouseEvent->buttons() == 0) {
      // mouse move
      int highlightedFosFrame = (6 <= x && x <= 12 && y < 12) ? frame : -1;
      if (highlightedFosFrame != m_highlightedFosFrame) {
        m_highlightedFosFrame = highlightedFosFrame;
        viewer->update();
      }
      if (getCurrentFrame() == frame)
        viewer->setToolTip(tr("Current Frame"));
      else if (x < 7) {
        viewer->setToolTip("");
      } else if (x < 13)
        viewer->setToolTip(tr("Fixed Onion Skin Toggle"));
      else
        viewer->setToolTip(tr(""));
    } else if (mouseEvent->buttons() & Qt::LeftButton) {
      // drag
      switch (m_action) {
      case MoveHead:
        setCurrentFrame(frame);
        break;
      case ActivateMos:
        setMos(frame, true);
        break;
      case DeactivateMos:
        setMos(frame, false);
        break;
      case ActivateFos:
        setFos(frame, true);
        break;
      case DeactivateFos:
        setFos(frame, false);
        break;
      default:
        return false;
      }
      viewer->update();
    } else if (mouseEvent->buttons() & Qt::MidButton)
      return false;
  } else
    return false;
  return true;
}

bool FrameHeadGadget::isFos(int frame) const {
  OnionSkinMask osMask =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  return osMask.isFos(frame);
}

bool FrameHeadGadget::isMos(int frame) const {
  OnionSkinMask osMask =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  return osMask.isMos(frame - getCurrentFrame());
}

void FrameHeadGadget::setFos(int frame, bool on) {
  TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
  OnionSkinMask osMask       = osmh->getOnionSkinMask();
  osMask.setFos(frame, on);
  if (on && !osMask.isEnabled()) osMask.enable(true);
  osmh->setOnionSkinMask(osMask);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

void FrameHeadGadget::setMos(int frame, bool on) {
  if (frame == getCurrentFrame()) return;
  TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
  OnionSkinMask osMask       = osmh->getOnionSkinMask();
  osMask.setMos(frame - getCurrentFrame(), on);
  if (on && !osMask.isEnabled()) osMask.enable(true);
  osmh->setOnionSkinMask(osMask);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

FilmstripFrameHeadGadget::FilmstripFrameHeadGadget(FilmstripFrames *filmstrip)
    : m_filmstrip(filmstrip)
    , m_dy(m_filmstrip->getIconSize().height() + fs_frameSpacing +
           fs_iconMarginTop + fs_iconMarginBottom)
    , m_highlightedghostFrame(-1) {}

int FilmstripFrameHeadGadget::getY() const { return 50; }

int FilmstripFrameHeadGadget::index2y(int index) const { return index * m_dy; }

int FilmstripFrameHeadGadget::y2index(int y) const { return y / m_dy; }

void FilmstripFrameHeadGadget::setCurrentFrame(int index) const {
  TXshSimpleLevel *level = m_filmstrip->getLevel();
  if (!level) return;
  TFrameId fid = level->index2fid(index);
  if (fid >= TFrameId(1)) TApp::instance()->getCurrentFrame()->setFid(fid);
}

int FilmstripFrameHeadGadget::getCurrentFrame() const {
  TXshSimpleLevel *level = m_filmstrip->getLevel();
  if (!level) return 0;
  return level->guessIndex(TApp::instance()->getCurrentFrame()->getFid());
}

//-----------------------------------------------------------------------------

void FilmstripFrameHeadGadget::drawOnionSkinSelection(QPainter &p,
                                                      const QColor &lightColor,
                                                      const QColor &darkColor) {
  int currentRow = getCurrentFrame();

  TPixel frontPixel, backPixel;
  bool inksOnly;
  Preferences::instance()->getOnionData(frontPixel, backPixel, inksOnly);
  QColor frontColor((int)frontPixel.r, (int)frontPixel.g, (int)frontPixel.b,
                    128);
  QColor backColor((int)backPixel.r, (int)backPixel.g, (int)backPixel.b, 128);

  OnionSkinMask osMask =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  int mosCount = osMask.getMosCount();

  int i;
  // OnionSkinの円の左上座標のｙ値
  int onionDotYPos = m_dy / 2 - 5;

  p.setPen(Qt::red);
  //--- OnionSkinが有効なら実線、無効なら点線
  if (!osMask.isEnabled()) {
    QPen currentPen = p.pen();
    currentPen.setStyle(Qt::DashLine);
    currentPen.setColor(QColor(128, 128, 128, 255));
    p.setPen(currentPen);
  }
  //カレントフレームにOnionSkinを伸ばすハンドルを描画
  {
    int angle180 = 16 * 180;
    QRectF handleRect(0, 0, 30, 30);

    //上向きのハンドル：1フレーム目のときは描画しない
    if (currentRow > 0) {
      int y0 = index2y(currentRow) - 15;
      p.setBrush(QBrush(backColor));
      p.drawChord(handleRect.translated(0, y0), 0, angle180);
    }

    //下向きのハンドル：最終フレームの時は描画しない
    std::vector<TFrameId> fids;
    m_filmstrip->getLevel()->getFids(fids);
    int frameCount = (int)fids.size();
    if (currentRow < frameCount - 1) {
      int y1 = index2y(currentRow + 1) - 15;
      p.setBrush(QBrush(frontColor));
      p.drawChord(handleRect.translated(0, y1), angle180, angle180);
    }

    //--- 動く Onion Skinの描画 その1
    //先に線を描く
    //まず OnionSkinの最大／最小値を取得
    int minMos = 0;
    int maxMos = 0;
    for (i = 0; i < mosCount; i++) {
      int mos                  = osMask.getMos(i);
      if (minMos > mos) minMos = mos;
      if (maxMos < mos) maxMos = mos;
    }
    p.setBrush(Qt::NoBrush);
    // min/maxが更新されていたら、線を描く
    if (minMos < 0)  //上方向に伸ばす線
    {
      int y0 = index2y(currentRow + minMos) + onionDotYPos + 10;  // 10は●の直径
      int y1 = index2y(currentRow) - 15;
      p.drawLine(15, y0, 15, y1);
    }
    if (maxMos > 0)  //下方向に伸ばす線
    {
      int y0 = index2y(currentRow + 1) + 15;
      int y1 = index2y(currentRow + maxMos) + onionDotYPos;
      p.drawLine(15, y0, 15, y1);
    }
  }

  //--- Fix Onion Skinの描画
  for (i = 0; i < osMask.getFosCount(); i++) {
    int fos = osMask.getFos(i);
    // if(fos == currentRow) continue;
    int y = index2y(fos) + onionDotYPos;
    p.setPen(Qt::red);
    // OnionSkinがDisableなら中空にする
    p.setBrush(osMask.isEnabled() ? QBrush(QColor(0, 255, 255, 128))
                                  : Qt::NoBrush);
    p.drawEllipse(0, y, 10, 10);
  }
  //---

  //--- 動く Onion Skinの描画 その2

  //続いて、各OnionSkinの●を描く
  p.setPen(Qt::red);
  for (i = 0; i < mosCount; i++) {
    // mosはOnionSkinの描かれるフレームのオフセット値
    int mos = osMask.getMos(i);
    // 100312 iwasawa ハイライトする場合は後で描くのでスキップする
    if (currentRow + mos == m_highlightedMosFrame) continue;

    int y = index2y(currentRow + mos) + onionDotYPos;

    p.setBrush(mos < 0 ? backColor : frontColor);

    p.drawEllipse(10, y, 10, 10);
  }

  // Fix Onion Skin ハイライトの描画
  // FixOnionSkinがマウスが乗ってハイライトされていて、
  //かつ 現在のフレームでないときに描画
  if (m_highlightedFosFrame >= 0 && m_highlightedFosFrame != currentRow) {
    p.setPen(QColor(255, 128, 0));
    p.setBrush(QBrush(QColor(255, 255, 0, 200)));
    p.drawEllipse(0, index2y(m_highlightedFosFrame) + onionDotYPos, 10, 10);
  }

  //通常のOnion Skin ハイライトの描画
  if (m_highlightedMosFrame >= 0 && m_highlightedMosFrame != currentRow) {
    p.setPen(QColor(255, 128, 0));
    p.setBrush(QBrush(QColor(255, 255, 0, 200)));
    p.drawEllipse(10, index2y(m_highlightedMosFrame) + onionDotYPos, 10, 10);
  }
}

//-----------------------------------------------------------------------------

void FilmstripFrameHeadGadget::drawShiftTraceMarker(QPainter &p) {
  int currentRow = getCurrentFrame();

  TPixel frontPixel, backPixel;
  bool inksOnly;
  Preferences::instance()->getOnionData(frontPixel, backPixel, inksOnly);
  QColor frontColor((int)frontPixel.r, (int)frontPixel.g, (int)frontPixel.b);
  QColor backColor((int)backPixel.r, (int)backPixel.g, (int)backPixel.b);

  OnionSkinMask osMask =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();

  // draw lines to ghost frames
  int prevOffset    = osMask.getShiftTraceGhostFrameOffset(0);
  int forwardOffset = osMask.getShiftTraceGhostFrameOffset(1);

  const int shiftTraceDotSize    = 12;
  const int shiftTraceDotXOffset = 3;
  int dotYPos                    = (m_dy - shiftTraceDotSize) / 2;

  if (currentRow > 0 && prevOffset < 0)  // previous ghost
  {
    p.setPen(backColor);
    int y0 = index2y(currentRow + prevOffset) + dotYPos + shiftTraceDotSize;
    int y1 = index2y(currentRow);
    p.drawLine(shiftTraceDotXOffset + shiftTraceDotSize / 2, y0,
               shiftTraceDotXOffset + shiftTraceDotSize / 2, y1);
  }
  if (forwardOffset > 0)  // forward ghost
  {
    p.setPen(frontColor);
    int y0 = index2y(currentRow + 1);
    int y1 = index2y(currentRow + forwardOffset) + dotYPos + shiftTraceDotSize;
    p.drawLine(shiftTraceDotXOffset + shiftTraceDotSize / 2, y0,
               shiftTraceDotXOffset + shiftTraceDotSize / 2, y1);
  }
  // draw dots
  std::vector<int> offsVec      = {prevOffset, 0, forwardOffset};
  std::vector<QColor> colorsVec = {backColor, QColor(0, 162, 232), frontColor};
  QFont currentFont             = p.font();
  QFont tmpFont                 = p.font();
  tmpFont.setPointSize(7);
  p.setFont(tmpFont);

  for (int i = 0; i < 3; i++) {
    if (i != 1 && offsVec[i] == 0) continue;
    p.setPen(colorsVec[i]);
    p.setBrush(Qt::gray);
    int topPos = index2y(currentRow + offsVec[i]) + dotYPos;
    QRect dotRect(shiftTraceDotXOffset, topPos, shiftTraceDotSize,
                  shiftTraceDotSize);
    p.drawRect(dotRect);

    // draw shortcut numbers
    p.setPen(Qt::black);
    p.drawText(dotRect, Qt::AlignCenter, QString::number(i + 1));
  }
  p.setFont(currentFont);

  // highlight on mouse over
  if (m_highlightedghostFrame >= 0) {
    p.setPen(QColor(255, 255, 0));
    p.setBrush(QColor(255, 255, 0, 180));
    int topPos = index2y(m_highlightedghostFrame) + dotYPos;
    QRect dotRect(shiftTraceDotXOffset, topPos, shiftTraceDotSize,
                  shiftTraceDotSize);
    p.drawRect(dotRect);
  }
}

//-----------------------------------------------------------------------------

bool FilmstripFrameHeadGadget::eventFilter(QObject *obj, QEvent *e) {
  // shift & trace case
  if (CommandManager::instance()->getAction(MI_ShiftTrace)->isChecked())
    return shiftTraceEventFilter(obj, e);

  if (!Preferences::instance()->isOnionSkinEnabled()) return false;

  QWidget *viewer = dynamic_cast<QWidget *>(obj);

  if (e->type() == QEvent::MouseButtonPress) {
    //方針：上下タブをクリック→ドラッグでオニオンスキンの増減
    //　　　上下タブをダブルクリックでオニオンスキンの表示／非表示
    //		オニオンスキンの●エリア
    //をクリックで各フレームのON／OFF切り替え→ドラッグで伸ばせる
    //		Fixedオニオンスキンの●エリアをクリックで
    // Fixedオニオンスキンの位置の指定

    //-----
    //それぞれのパーツの位置Rectの指定。各フレームの右上座標からのオフセットも含む。
    // OnionSkinの円の左上座標のｙ値
    int onionDotYPos = m_dy / 2 - 5;
    // OnionSkinの●のRect
    QRect onionDotRect(10, onionDotYPos, 10, 10);
    // FixedOnionSkinの●のRect
    QRect fixedOnionDotRect(0, onionDotYPos, 10, 10);
    //上方向のOnionSkinタブのRect
    QRect backOnionTabRect(0, m_dy - 15, 30, 15);
    //下方向のOnionSkinタブのRect
    QRect frontOnionTabRect(0, 0, 30, 15);
    //-----

    //----- ハイライト表示、actionのリセット
    m_action = None;

    // click
    if (m_highlightedFosFrame >= 0) {
      m_highlightedFosFrame = -1;
      viewer->update();
    }
    if (m_highlightedMosFrame >= 0) {
      m_highlightedMosFrame = -1;
      viewer->update();
    }

    //----- 以下、クリック位置に応じてアクションを変えていく
    //クリックされたフレームを取得
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    int frame               = y2index(mouseEvent->pos().y());
    m_buttonPressCellIndex  = frame;
    int currentFrame        = getCurrentFrame();

    //各フレーム左上からの位置
    QPoint clickedPos = mouseEvent->pos() + QPoint(0, -index2y(frame));

    //カレントフレームの場合、無視
    if (frame == currentFrame) return false;
    //カレントフレームの上下でタブをクリックした場合
    else if ((frame == currentFrame - 1 &&
              backOnionTabRect.contains(clickedPos)) ||
             (frame == currentFrame + 1 &&
              frontOnionTabRect.contains(clickedPos))) {
      //ドラッグに備える
      m_action = ActivateMos;
    }
    //カレントフレーム以外の場合
    else {
      //通常のOnionSkinの場合
      if (onionDotRect.contains(clickedPos)) {
        // 既にオニオンスキンなら、オフにする
        bool on = !isMos(frame);
        //アクションが決まる
        m_action = on ? ActivateMos : DeactivateMos;
        //カレントフレームでなければ、オニオンスキンを切り替え
        setMos(frame, on);
      }
      // FixedOnionSkinの場合
      else if (fixedOnionDotRect.contains(clickedPos)) {
        // Fos
        bool on  = !isFos(frame);
        m_action = on ? ActivateFos : DeactivateFos;
        setFos(frame, on);
      } else
        return false;
    }
    viewer->update();

  }
  //---- 上下タブのダブルクリックで表示/非表示の切り替え
  else if (e->type() == QEvent::MouseButtonDblClick) {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    if (mouseEvent->buttons() & Qt::LeftButton) {
      int frame = y2index(mouseEvent->pos().y());
      //各フレーム左上からの位置
      QPoint clickedPos = mouseEvent->pos() + QPoint(0, -index2y(frame));
      //カレントフレーム
      int currentFrame = getCurrentFrame();
      //上方向のOnionSkinタブのRect
      QRect backOnionTabRect(0, m_dy - 15, 30, 15);
      //下方向のOnionSkinタブのRect
      QRect frontOnionTabRect(0, 0, 30, 15);
      if ((currentFrame - 1 == frame &&
           backOnionTabRect.contains(clickedPos)) ||
          (currentFrame + 1 == frame &&
           frontOnionTabRect.contains(clickedPos))) {
        enableOnionSkin(!isOnionSkinEnabled());
        viewer->update();
      }
    } else
      return false;
  }
  //----
  else if (e->type() == QEvent::MouseMove) {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    int frame               = y2index(mouseEvent->pos().y());
    //各フレーム左上からの位置
    QPoint clickedPos = mouseEvent->pos() + QPoint(0, -index2y(frame));
    //カレントフレーム
    int currentFrame = getCurrentFrame();
    //マウスボタンが押されていない場合
    if (mouseEvent->buttons() == 0) {
      //-----
      //それぞれのパーツの位置Rectの指定。各フレームの右上座標からのオフセットも含む。
      // OnionSkinの円の左上座標のｙ値
      int onionDotYPos = m_dy / 2 - 5;
      // OnionSkinの●のRect
      QRect onionDotRect(10, onionDotYPos, 10, 10);
      // FixedOnionSkinの●のRect
      QRect fixedOnionDotRect(0, onionDotYPos, 10, 10);
      //上方向のOnionSkinタブのRect
      QRect backOnionTabRect(0, m_dy - 15, 30, 15);
      //下方向のOnionSkinタブのRect
      QRect frontOnionTabRect(0, 0, 30, 15);
      //-----

      //----- Fixed Onion Skin の ハイライト
      int highlightedFrame;
      if (currentFrame != frame && fixedOnionDotRect.contains(clickedPos))
        highlightedFrame = frame;
      else
        highlightedFrame = -1;
      if (highlightedFrame != m_highlightedFosFrame) {
        m_highlightedFosFrame = highlightedFrame;
        viewer->update();
      }
      //-----
      //----- 通常の Onion Skin の ハイライト
      if (currentFrame != frame && onionDotRect.contains(clickedPos))
        highlightedFrame = frame;
      else
        highlightedFrame = -1;
      if (highlightedFrame != m_highlightedMosFrame) {
        m_highlightedMosFrame = highlightedFrame;
        viewer->update();
      }
      //-----
      //----- ツールチップの表示
      if (currentFrame == frame) {
        viewer->setToolTip(tr(""));
        return false;
      }
      // Fixed Onion Skin
      else if (fixedOnionDotRect.contains(clickedPos))
        viewer->setToolTip(tr("Click to Toggle Fixed Onion Skin"));
      //通常の Onion Skin
      else if (onionDotRect.contains(clickedPos))
        viewer->setToolTip(tr("Click / Drag to Toggle Onion Skin"));
      //カレントフレームの上下タブ
      else if ((currentFrame - 1 == frame &&
                backOnionTabRect.contains(clickedPos)) ||
               (currentFrame + 1 == frame &&
                frontOnionTabRect.contains(clickedPos)))
        viewer->setToolTip(
            tr("Drag to Extend Onion Skin, Double Click to Toggle All"));
      else {
        viewer->setToolTip(tr(""));
        return false;
      }
    }

    //左ボタンドラッグの場合
    else if (mouseEvent->buttons() & Qt::LeftButton) {
      //指定されたアクションに従い、ドラッグされたフレームも同じアクションを行う
      // drag
      switch (m_action) {
      case MoveHead:
        setCurrentFrame(frame);
        break;
      case ActivateMos:
        setMos(frame, true);
        break;
      case DeactivateMos:
        setMos(frame, false);
        break;
      case ActivateFos:
        setFos(frame, true);
        break;
      case DeactivateFos:
        setFos(frame, false);
        break;
      default:
        return false;
      }
      viewer->update();
    } else if (mouseEvent->buttons() & Qt::MidButton)
      return false;
  } else
    return false;
  return true;
}

//-----------------------------------------------------------------------------

bool FilmstripFrameHeadGadget::shiftTraceEventFilter(QObject *obj, QEvent *e) {
  QWidget *viewer = dynamic_cast<QWidget *>(obj);

  if (e->type() != QEvent::MouseButtonPress && e->type() != QEvent::MouseMove)
    return false;

  const int shiftTraceDotSize    = 12;
  const int shiftTraceDotXOffset = 3;
  int dotYPos                    = (m_dy - shiftTraceDotSize) / 2;
  QRect dotRect(shiftTraceDotXOffset, dotYPos, shiftTraceDotSize,
                shiftTraceDotSize);

  // reset highlight
  if (m_highlightedghostFrame >= 0) {
    m_highlightedghostFrame = -1;
    viewer->update();
  }

  QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
  int frame               = y2index(mouseEvent->pos().y());
  // position from top-left of the frame
  QPoint mousePos  = mouseEvent->pos() + QPoint(0, -index2y(frame));
  int currentFrame = getCurrentFrame();

  if (mousePos.x() < dotRect.left() || mousePos.x() > dotRect.right())
    return false;

  if (e->type() == QEvent::MouseButtonPress) {
    if (frame == currentFrame) {
      if (dotRect.contains(mousePos))
        OnioniSkinMaskGUI::resetShiftTraceFrameOffset();
      else
        return false;
    } else {
      OnionSkinMask osMask =
          TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
      int prevOffset    = osMask.getShiftTraceGhostFrameOffset(0);
      int forwardOffset = osMask.getShiftTraceGhostFrameOffset(1);
      // Hide previous ghost
      if (frame == currentFrame + prevOffset)
        osMask.setShiftTraceGhostFrameOffset(0, 0);
      // Hide forward ghost
      else if (frame == currentFrame + forwardOffset)
        osMask.setShiftTraceGhostFrameOffset(1, 0);
      // Move previous ghost
      else if (frame < currentFrame)
        osMask.setShiftTraceGhostFrameOffset(0, frame - currentFrame);
      // Move forward ghost
      else
        osMask.setShiftTraceGhostFrameOffset(1, frame - currentFrame);
      TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osMask);
    }
    TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
    return true;
  }
  //----
  else if (e->type() == QEvent::MouseMove) {
    if (frame == currentFrame) {
      if (dotRect.contains(mousePos)) {
        m_highlightedghostFrame = frame;
        viewer->setToolTip(
            tr("Click to Reset Shift & Trace Markers to Neighbor Frames\nHold "
               "F2 Key on the Viewer to Show This Frame Only"));
      } else {
        viewer->setToolTip(tr(""));
        return false;
      }
    } else {
      m_highlightedghostFrame = frame;
      OnionSkinMask osMask =
          TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
      int prevOffset    = osMask.getShiftTraceGhostFrameOffset(0);
      int forwardOffset = osMask.getShiftTraceGhostFrameOffset(1);
      // Hide ghost
      if (frame == currentFrame + prevOffset)
        viewer->setToolTip(
            tr("Click to Hide This Frame from Shift & Trace\nHold F1 Key on "
               "the Viewer to Show This Frame Only"));
      else if (frame == currentFrame + forwardOffset)
        viewer->setToolTip(
            tr("Click to Hide This Frame from Shift & Trace\nHold F3 Key on "
               "the Viewer to Show This Frame Only"));
      // Move ghost
      else
        viewer->setToolTip(tr("Click to Move Shift & Trace Marker"));
    }
  }
  return true;
}
//-----------------------------------------------------------------------------

class OnionSkinToggle final : public MenuItemHandler {
public:
  OnionSkinToggle() : MenuItemHandler(MI_OnionSkin) {}
  void execute() override {
    QAction *action = CommandManager::instance()->getAction(MI_OnionSkin);
    if (!action) return;
    bool checked = action->isChecked();
    enableOnionSkin(checked);
  }
} onionSkinToggle;
