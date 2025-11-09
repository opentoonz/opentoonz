#pragma once
#ifndef COLORMODELVIEWER_H
#define COLORMODELVIEWER_H
#include "flipbook.h"
//=============================================================================
// ColorModelViewer
//-----------------------------------------------------------------------------
/*! \class ColorModelViewer
    \brief Flipbook viewer specialized for displaying and interacting with
           Color Model reference images.

    Displays the reference image associated with the current level's palette.
    Supports drag & drop loading, picking colors/styles, and "Use Current Frame"
    mode. Overrides FlipBook behavior to integrate with palette and tool system.
*/
class ColorModelViewer final : public FlipBook {
  Q_OBJECT

  /*! Current pick mode:
      0 = Areas only
      1 = Lines only
      2 = Lines & Areas (default)
      Cached locally to avoid repeated tool property lookups.
  */
  int m_mode;

  /*! Stores the current reference image path.
      Used to detect transitions between normal Color Model and
      "Use Current Frame" mode, ensuring correct reload behavior.
  */
  TFilePath m_currentRefImgPath;

  /*! Button to toggle line style picking when in "Areas" mode.
      When checked and mode is AREAS, forces m_mode = 2 (Lines & Areas).
  */
  QToolButton *m_pickLineStylesBtn;

public:
  ColorModelViewer(QWidget *parent = 0);
  ~ColorModelViewer();

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

  /*! Loads a new reference image into the current palette.
      Triggers PaletteCmd::loadReferenceImage() with appropriate behavior.
  */
  void loadImage(const TFilePath &fp);

  /*! Resets viewer to empty state.
      Clears cache, levels, title, and current image.
      Resets reference path and palette pointer.
  */
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

  /*! Picks color/style at given point and applies it as current style.
      Respects current tool mode and line/area settings.
  */
  void pick(const QPoint &p);

  void hideEvent(
      QHideEvent *e) override;  // Override to skip FlipBook::hideEvent
  void showEvent(QShowEvent *e) override;

  /*! Reloads the current frame when entering a level using "Use Current Frame".
      Ensures the displayed image matches the actual frame content.
  */
  void reloadCurrentFrame();

protected slots:
  /*! Updates viewer with current palette's reference image.
      Handles both external files and "Use Current Frame" mode.
  */
  void showCurrentImage();

  /*! Sets the current frame as the Color Model reference.
      Clones the frame image and updates palette metadata.
  */
  void loadCurrentFrame();

  /*! Removes the reference image from the current palette.
      Clears viewer and resets state.
  */
  void removeColorModel();

  /*! Emitted when the expected reference image cannot be found.
      Shows user-friendly error message.
  */
  void onRefImageNotFound();

  /*! Triggers repaint of the image viewer.
      Called when level view changes (e.g., frame switch).
  */
  void updateViewer();

  /*! Updates pick mode and cursor based on current tool.
      Syncs with tool properties ("Mode:") and m_pickLineStylesBtn state.
  */
  void changePickType();

  /*! Re-applies picked positions from the Color Model to update styles.
      Useful after manual position adjustments.
  */
  void repickFromColorModel();

signals:
  /*! Emitted when the reference image path exists but file is missing.
      Allows UI to display warning.
  */
  void refImageNotFound();
};

#endif  // COLORMODELVIEWER_H
