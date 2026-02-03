#pragma once

#ifndef PALETTECONTROLLER_INCLUDED
#define PALETTECONTROLLER_INCLUDED

// TnzCore includes
#include "tcommon.h"
#include "tpixel.h"

// Qt includes
#include <QObject>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=====================================================

//    Forward declarations

class TPaletteHandle;

//=====================================================

//************************************************************************
//    PaletteController  declaration
//************************************************************************

class DVAPI PaletteController final : public QObject {
  Q_OBJECT

  TPaletteHandle *m_currentLevelPalette;  //!< (\p owned) Handle to the palette
                                          //! of current level.
  TPaletteHandle *m_currentCleanupPalette;  //!< (\p owned) Handle to the
                                            //! palette of current cleanup
  //! settings.
  TPaletteHandle *m_currentPalette;  //!< (\p owned) Handle to the palette of
                                     //! currently selected object.

  TPaletteHandle *m_originalCurrentPalette;  //!< Pointer to the \a original
  //! current palette handle specified
  //! on
  //!  the last setCurrentPalette() invocation.
  TPixel32 m_colorSample;
  bool m_colorAutoApplyEnabled;

public:
  PaletteController();
  ~PaletteController();

  TPaletteHandle *getCurrentLevelPalette() const {
    return m_currentLevelPalette;
  }
  TPaletteHandle *getCurrentCleanupPalette() const {
    return m_currentCleanupPalette;
  }
  TPaletteHandle *getCurrentPalette() const { return m_currentPalette; }

  void setCurrentPalette(TPaletteHandle *paletteHandle);

  // centralized handling of color auto-apply. see StyleEditor.
  // when ColorAutoApply is disabled then changes should made on the ColorSample
  // instead than on the current style. (see rgb color picker)
  void enableColorAutoApply(bool enabled);
  bool isColorAutoApplyEnabled() const { return m_colorAutoApplyEnabled; }

  void setColorSample(const TPixel32 &color);
  TPixel32 getColorSample() const { return m_colorSample; }

  // used for "passive pick" feature in the tool options bar of the rgb picker
  // tool
  void notifyColorPassivePicked(const QColor &col) {
    emit colorPassivePicked(col);
  }
  // used for "passive pick" feature in the tool options bar of the style picker
  // tool
  void notifyStylePassivePicked(const int ink, const int paint,
                                const int tone) {
    emit stylePassivePicked(ink, paint, tone);
  }

public slots:

  void editLevelPalette();
  void editCleanupPalette();
  // void setColorCheckIndex();// sets the  index to be used to perform the
  // inkCheck and paintcheck;

signals:

  void colorAutoApplyEnabled(bool enabled);
  void colorSampleChanged(const TPixel32 &);
  void checkPaletteLock();

  // used for "passive pick" feature in the tool options bar of the rgb picker
  // tool
  void colorPassivePicked(const QColor &);
  // used for "passive pick" feature in the tool options bar of the style picker
  // tool
  void stylePassivePicked(const int, const int, const int);
};

#endif  // PALETTECONTROLLER_INCLUDED
