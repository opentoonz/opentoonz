#pragma once

#ifndef T_THUMBNAILVIEWE_INCLUDED
#define T_THUMBNAILVIEWE_INCLUDED

#include "tw/scrollview.h"

// forward declaration
class Thumbnail;
class TTextField;
class TFilePath;

class ThumbnailViewer : public TScrollView {
  vector<Thumbnail *> m_items;
  int m_selectedItemIndex;

  const TDimension m_itemSize;
  const TDimension m_itemSpace;
  const TDimension m_margins;

  const TRect m_playButtonBox;
  const TRect m_iconBox;
  const TRect m_textBox;

  TPoint m_oldPos;
  bool m_dragDropArmed;

  bool m_flag;

  bool m_playing, m_loading;
  bool m_timerActive;

  class NameField;
  NameField *m_nameField;

public:
  ThumbnailViewer(TWidget *parent, string name = "thumbnailViewer");
  ~ThumbnailViewer();

  void addItem(Thumbnail *item);

  //! removes the item from the ThumbnailViewer. It doesn't destroy the item
  void removeItem(Thumbnail *item);

  TDimension getIconSize() const { return m_iconBox.getSize(); }

  int getItemCount() const;
  Thumbnail *getItem(int index) const;

  //! find the max number of thumbnails which fit the current widget width
  int getColumnCount() const;

  //! find the bottom-left corner coordinate of the index-th thumbnail
  //! (icon+controls)
  TPoint getItemPos(int index);

  // ! find the region covered by the index-th thumbnail (icon+controls)
  TRect getItemBounds(int index);

  // ! coordinate --> thumbnail index; returns -1 if not found
  int findItem(const TPoint &pos);

  void configureNotify(const TDimension &d);

  //! draw the index-th thumbnail current frame
  void drawFrame(int index);

  //! draw the index-th thumbnail
  void drawItem(int index);

  //! draw the empty space around and between thumbnails
  void drawBackground();

  void repaint();

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void leftButtonDoubleClick(const TMouseEvent &e);

  void onTimer(int);
  void startPlaying();
  void stopPlaying();

  void select(int);
  int getSelectedItemIndex() { return m_selectedItemIndex; }

  void clearItems();
  void loadDirectory(const TFilePath &dirPath, const vector<string> &fileTypes);

  void updateContentSize();

  void middleButtonDown(const TMouseEvent &e) {}
  void middleButtonDrag(const TMouseEvent &e) {}
  void middleButtonUp(const TMouseEvent &e) {}

  void scrollPage(int y);

  virtual void onDoubleClick(int index) {}
  virtual void onSelect(int index) {}
};

#endif
