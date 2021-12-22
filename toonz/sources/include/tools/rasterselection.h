#pragma once

#ifndef RASTER_SELECTION_H
#define RASTER_SELECTION_H

// TnzCore includes
#include "tcommon.h"
#include "tpalette.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tstroke.h"
#include "tdata.h"

// TnzLib includes
#include "toonz/ttileset.h"
#include "toonz/txshcell.h"

// TnzQt includes
#include "toonzqt/selection.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TTileSetCM32;
class StrokesData;
class RasterImageData;

//=============================================================================
// RasterSelection
//! Selection of TToonzImage and TRasterImage.
//-----------------------------------------------------------------------------

class DVAPI RasterSelection final : public TSelection {
  TImageP m_currentImage;
  TXshCell m_currentImageCell;

  TPaletteP m_oldPalette;
  TRectD m_selectionBbox;
  std::vector<TStroke> m_strokes;
  std::vector<TStroke> m_originalStrokes;
  TAffine m_affine;
  TPoint m_startPosition;
  TRasterP m_floatingSelection;
  TRasterP m_originalfloatingSelection;
  TFrameId m_fid;
  int m_transformationCount;
  bool m_isPastedSelection;
  bool m_noAntialiasing;

private:
  void pasteSelection(const RasterImageData *data);

public:
  RasterSelection();
  RasterSelection(const RasterSelection &src);

  TSelection *clone() const;
  void notify();

  const TImageP &getCurrentImage() const { return m_currentImage; }
  const TXshCell &getCurrentImageCell() const { return m_currentImageCell; }
  void setCurrentImage(const TImageP &img, const TXshCell &imageCell) {
    m_currentImage = img, m_currentImageCell = imageCell;
  }

  void setStrokes(const std::vector<TStroke> &strokes) { m_strokes = strokes; }
  std::vector<TStroke> getStrokes() const { return m_strokes; }
  std::vector<TStroke> getOriginalStrokes() const { return m_originalStrokes; }

  void setTransformation(const TAffine &affine) { m_affine = affine; }
  TAffine getTransformation() const { return m_affine; }

  void setStartPosition(const TPoint &position) { m_startPosition = position; }
  TPoint getStartPosition() const { return m_startPosition; }

  void setFloatingSeletion(const TRasterP &floatingSelection) {
    m_floatingSelection = floatingSelection;
  }
  TRasterP getFloatingSelection() const { return m_floatingSelection; }
  TRasterP getOriginalFloatingSelection() const {
    return m_originalfloatingSelection;
  }

  void setFrameId(const TFrameId &fid) { m_fid = fid; }
  TFrameId getFrameId() const { return m_fid; }

  int getTransformationCount() const { return m_transformationCount; }
  void setTransformationCount(int transformationCount) {
    m_transformationCount = transformationCount;
  }

  void setNoAntialiasing(bool value) { m_noAntialiasing = value; }

  bool isPastedSelection() const { return m_isPastedSelection; }

  /*! Returns strokes bounding box.*/
  TRectD getStrokesBound(std::vector<TStroke> strokes) const;
  /*! Returns m_strokes bounding box in world coordinates.
The bounding box results transformed if the selection is transformed.*/
  TRectD getSelectionBound() const;
  /*! Returns m_originlStrokes bounding box in world coordinates.*/
  TRectD getOriginalSelectionBound() const;

  /*! Return \b m_selectionBbox.
Can be different from getSelectionBound() after a free deform transformation. */
  TRectD getSelectionBbox() const;
  void setSelectionBbox(const TRectD &rect);

  void selectNone() override;

  //! Take a rect in world coordinates and put it in the selection.
  void select(const TRectD &rect);
  void select(TStroke &stroke);
  void selectAll();
  bool isEmpty() const override;

  void enableCommands() override;

  bool isFloating() const;
  void transform(const TAffine &affine);

  // commands

  //! Build the floating image using m_strokes.
  void makeFloating();
  //! Paste the floating image over the current image and empty the selection.
  void pasteFloatingSelection();
  //! Delete the floating image.
  void deleteSelection();
  //! Copy the floating image  in the clipboard.
  void copySelection();
  //! Cut the floating image.
  void cutSelection();
  //! Create the floating image using clipboard Data.
  void pasteSelection();

  bool isTransformed();

  bool isEditable();
};

#endif  // RASTER_SELECTION_H
