#pragma once

#ifndef COLORMODELVIEWER_H
#define COLORMODELVIEWER_H

#include "flipbook.h"

//=============================================================================
// ColorModelViewer
//-----------------------------------------------------------------------------

class ColorModelViewer final : public FlipBook {
  Q_OBJECT

  /*-- ツールのタイプを手元に持っておき、取得の手間を省く --*/
  int m_mode;
  /*-- ColorModelのファイルパスを覚えておいて、UseCurrentFrame間の移動に対応
   * --*/
  TFilePath m_currentRefImgPath;

public:
  ColorModelViewer(QWidget *parent = 0);
  ~ColorModelViewer();

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void loadImage(const TFilePath &fp);
  void resetImageViewer() {
    clearCache();
    m_levels.clear();
    m_title = "";
    m_imageViewer->setImage(getCurrentImage(0));
    m_currentRefImgPath = TFilePath();
    m_palette           = 0;
  }

  void contextMenuEvent(QContextMenuEvent *event) override;

  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void pick(const QPoint &p);
  void hideEvent(QHideEvent *e)
      override;  // to avoid calling the hideEvent of class Flipbook!
  void showEvent(QShowEvent *e) override;

  /*-
   * UseCurrentFrameのLevelに移動してきたときに、改めてCurrentFrameを格納しなおす
   * -*/
  void reloadCurrentFrame();

protected slots:
  void showCurrentImage();

  void loadCurrentFrame();
  void removeColorModel();

  void onRefImageNotFound();
  void updateViewer();

  /*-
   * ツールのTypeに合わせてPickのタイプも変える。それにあわせカーソルも切り替える
   * -*/
  void changePickType();

  void repickFromColorModel();

signals:
  void refImageNotFound();
};

#endif  // COLORMODELVIEWER_H
