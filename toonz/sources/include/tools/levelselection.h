#pragma once

#ifndef LEVELSELECTION_H
#define LEVELSELECTION_H

// TnzQt includes
#include "toonzqt/selection.h"

// TnzCore includes
#include "tcommon.h"

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//  Forward declarations

class TVectorImage;

//====================================================

//*******************************************************************************
//    LevelSelection  declaration
//*******************************************************************************

/*!
  \brief    Selection type for level-based multi-frame selections targetting
            primitives described by predefined filtering functions.
*/

class DVAPI LevelSelection : public TSelection {
public:
  enum FramesMode     //!  Possible frames selection modes.
  { FRAMES_NONE,      //!< No frame is selected.
    FRAMES_CURRENT,   //!< Selects the (context-defined) current frame.
    FRAMES_SELECTED,  //!< Selects the frames specified in \p
                      //! TTool::getSelectedFrames().
    FRAMES_ALL,       //!< Selects the whole level.
  };

  enum Filter          //!  Possible selection filters.
  { EMPTY,             //!< Selection is empty - everyting is filtered out.
    WHOLE,             //!< Selects everything - nothing is filtered out.
    SELECTED_STYLES,   //!< Acts only entities with selected palette styles.
    BOUNDARY_STROKES,  //!< Acts only on boundary strokes (applies only to
                       //! vector images).
  };

  typedef std::set<int>
      styles_container;  //!< Container of style indexes used together with the
                         //!  \p SELECTED_STYLES filter.
public:
  LevelSelection();  //!< Constructs an empty level selection.

  /*! \remark   The selection is hereby considered empty \a only if it either
     has
          FramesMode FRAMES_NONE or Filter EMPTY. It is user's responsibility
          to check whether nontrivial selections are empty or not. */

  bool isEmpty() const override;  //!< Returns whether the selection is empty.
  void selectNone()
      override;  //!< Resets the selection to empty. This is functionally
                 //!  equivalent to <TT>operator=(LevelSelection());</TT>
  FramesMode framesMode() const { return m_framesMode; }
  FramesMode &framesMode()  //!  Returns current frames selection mode.
  {
    return m_framesMode;
  }

  Filter filter() const { return m_filter; }
  Filter &filter()  //!  Returns current styles selection mode.
  {
    return m_filter;
  }

  const styles_container &styles() const { return m_styles; }
  styles_container &styles() {
    return m_styles;
  }  //!< Returns selected palette styles used with the
     //!  \p SELECTED_STYLES filter.
private:
  FramesMode m_framesMode;  //!< Selected level frames.
  Filter m_filter;          //!< Selection filter.

  std::set<int> m_styles;  //!< Selected palette styles used with the \p
                           //! SELECTED_STYLES filter.
};

//*******************************************************************************
//    Related standalone functions
//*******************************************************************************

/*!
  \brief    Calculates the strokes on the boundary of a vector image.
  \details  Boundary strokes are those with <I>a side entirely adjacent</I>
            to a region with style index \p 0.

  \return   The indexes of boundary strokes in the input image.
*/

DVAPI std::vector<int> getBoundaryStrokes(TVectorImage &vi);

//------------------------------------------------------------------------

/*!
  \brief    Applies a selection filter to a given vector image.
  \return   The stroke indexes included in the filtered selection.

  \remark   In case the filter is LevelSelection::BOUNDARY_STROKES, the
  resulting
            selection is composed of all strokes which are <I>even partially</I>
            adjacent to a region with style index \p 0.

  \sa       Function getBoundaryStrokes().
*/

DVAPI std::vector<int> getSelectedStrokes(
    TVectorImage &vi,  //!< Vector image whose strokes will be selected.
    const LevelSelection
        &levelSelection  //!< Selection filter for the specified image.
    );

#endif  // LEVELSELECTION_H
