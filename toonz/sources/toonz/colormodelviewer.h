#pragma once

#ifndef COLORMODELVIEWER_H
#define COLORMODELVIEWER_H

#include "flipbook.h"

//=============================================================================
// ColorModelViewer
//-----------------------------------------------------------------------------

class ColorModelViewer : public FlipBook {
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
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);
  void loadImage(const TFilePath &fp);
  void resetImageViewer() {
    clearCache();
    m_levels.clear();
    m_title = "";
    m_imageViewer->setImage(getCurrentImage(0));
    m_currentRefImgPath = TFilePath();
    m_palette           = 0;
  }

  void contextMenuEvent(QContextMenuEvent *event);

  void mousePressEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
  void pick(const QPoint &p);
  void hideEvent(
      QHideEvent *e);  // to avoid calling the hideEvent of class Flipbook!
  void showEvent(QShowEvent *e);

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

signals:
  void refImageNotFound();
};

#endif  // COLORMODELVIEWER_H
