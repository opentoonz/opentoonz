#pragma once

#ifndef ADJUST_THICKNESS_POPUP_H
#define ADJUST_THICKNESS_POPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"

// TnzCore includes
#include "tvectorimage.h"

// STD includes
#include <vector>
#include <set>

//=====================================================

//    Forward declarations

class TSelection;

namespace DVGui {
class MeasuredDoubleField;
}

class QPushButton;
class QComboBox;

//=====================================================

//*****************************************************************************
//    Adjust Thickness Popup  declaration
//*****************************************************************************

class AdjustThicknessPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  AdjustThicknessPopup();

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

public:
  class Swatch;

  struct FrameData  //!  Xsheet frame data used as processing input.
  {
    TXshSimpleLevelP m_sl;  //!< Referenced level.
    int m_frameIdx;         //!< Index of the referenced level frame index.

  public:
    FrameData();
    FrameData(const TXshSimpleLevelP &sl, int frameIdx);

    static FrameData getCurrent();

    bool operator==(const FrameData &other) const;
    bool operator!=(const FrameData &other) const { return !operator==(other); }

    TVectorImageP image() const;
  };

  struct SelectionData  //!  Selection-related data used as processing input.
  {
    enum ContentType  //!  Type of content in the selection.
    { NONE,           //!< Empty / invalid selection.
      IMAGE,          //!< Selection of entire images, each frame.
      STYLES,      //!< Selection of strokes with specific styles, each frame.
      BOUNDARIES,  //!< Selection of boundary strokes.
      STROKES,     //!< Selection of a specific set of strokes, on a
                   //!  <I>single image</I>.
    };

    enum FramesType     //!  Selection type for frames in the level.
    { ALL_FRAMES,       //!< All frames in the level.
      SELECTED_FRAMES,  //!< Frames selected in SelectionData::m_frameIdxs.
    };

  public:
    /*! \remark  Value STROKES restricts m_framesType to SELECTED_FRAMES, and
     m_frameIdxs to a sigle element.                                    */
    ContentType m_contentType;  //!< Content type of the selection.
    FramesType m_framesType;    //!< Frames type of the selection.

    // Level Range / Image  data

    TXshSimpleLevelP m_sl;      //!< Source level to be processed.
    std::set<int> m_frameIdxs;  //!< Level frame indexes to be processed.

    // Additional selected data

    /*! \details    Type values for which m_idxs has a meaning are:
          \li  STROKES yields selected stroke indexes
          \li  STYLES  yields selected style indexes                    */
    std::vector<int>
        m_idxs;  //!< Selection of \a sorted indexes whose meaning depends
                 //!  on m_type.
  public:
    SelectionData();
    SelectionData(const TSelection *s);
    SelectionData(const FrameData &fd);

    void getRange(int &startIdx, int &endIdx) const;

    operator bool() const;
  };

private:
  // Widgets

  QComboBox *m_thicknessMode;  //!< Thickness mode (scale thickness, add
                               //! thickness, constant).
  DVGui::MeasuredDoubleField *m_fromScale,  //!< Starting thickness scale.
      *m_fromDisplacement,  //!< Starting thickness displacement.
      *m_toScale,           //!< Ending thickness scale.
      *m_toDisplacement;    //!< Ending thickness displacement.

  Swatch *m_viewer;  //!< Swatch viewer widget.

  QPushButton *m_okBtn;  //!< Ok button.

  // Cached processing data

  SelectionData m_selectionData;  //!< Current selection-related data.
  FrameData m_currentFrameData;   //!< Current frame-related data.

  FrameData m_previewedFrameData;  //!< Currently previewed frame data.
  bool m_validPreview;             //!< Preview validity flag.

private:
  void getTransformParameters(
      double (&fromTransform)[2],
      double (
          &toTransform)[2]);   //!< Retrieves transform parameters at the end of
                               //!  the selected frames range.
  void updateSelectionData();  //!< Rebuilds m_selectionData.
  void updateCurrentFrameData();  //!< Rebuilds m_currentFrameData.
  void updateSelectionGui();  //!< Updates the popup's GUI according to current
                              //!  level selection.
  void
  schedulePreviewUpdate();  //!< Delayed, coalesced updatePreview() invoker.

private slots:

  void onXsheetChanged();     //!< Reactions to xsheet changes.
  void onSelectionChanged();  //!< Reactions to current selection changes.
  void onFrameChanged();      //!< Reactions to current frame changes.
  void onModeChanged();       //!< Reactions to m_thicknessMode changes.
  void onParamsChanged();     //!< Reactions to processing parameters changes.

  void updatePreview();  //!< Updates shown preview image.
  void apply();          //!< Starts processing.
};

#endif  // ADJUST_THICKNESS_POPUP_H
