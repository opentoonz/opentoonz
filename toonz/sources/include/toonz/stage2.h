#pragma once

#ifndef STAGE2_INCLUDED
#define STAGE2_INCLUDED

// TnzLib includes
#include "toonz/stage.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//    Forward declarations

class QAction;

namespace Stage {
DVVAR extern const double bigBoxSize[];
}

//=========================================================

//*****************************************************************************
//    ToonzCheck  declaration
//*****************************************************************************

/*!
  \brief    Singleton class storing on/off switches related to special
            level viewing modes.
*/

class DVAPI ToonzCheck {
public:
  enum Type {
    eTransparency = 0x1,   //!< Transparency check.
    eBlackBg      = 0x2,   //!< Black background check.
    eInk          = 0x4,   //!< Ink check (show current color on inks).
    ePaint        = 0x8,   //!< Paint check (show current color on paints).
    eGap          = 0x10,  //!< Gaps check.
    eAutoclose    = 0x20,  //!< Autoclose check.
    eInksOnly     = 0x40,  //!< Inks only (transparent paints).
    eInk1         = 0x80   //!< Ink#1 check (show style#1 on inks).
  };

public:
  static ToonzCheck *instance();

  int getChecks() const { return m_mask; }
  void toggleCheck(int checkType) {
    m_mask ^= checkType;
    // make "ink check" and "ink#1 check" exclusive
    if (checkType == eInk)
      m_mask = m_mask & ~eInk1;
    else if (checkType == eInk1)
      m_mask = m_mask & ~eInk;
  }

  /*! \note   The colorIndex is used for eInk and ePaint */
  int getColorIndex() const { return m_colorIndex; }
  bool setColorIndex(int index) {
    if (index <= 0 || m_colorIndex == index) return false;

    m_colorIndex = index;
    return true;
  }

private:
  int m_mask;
  int m_colorIndex;

private:
  ToonzCheck() : m_mask(0), m_colorIndex(-1) {}
};

//*****************************************************************************
//    Isolated check  singletons
//*****************************************************************************

class DVAPI CameraTestCheck {
  QAction *m_toggle;

public:
  static CameraTestCheck *instance();

  void setToggle(QAction *toggle);

  bool isEnabled() const;
  void setIsEnabled(bool on);

private:
  CameraTestCheck();
};

//=============================================================================

class DVAPI CleanupPreviewCheck {
  QAction *m_toggle;

public:
  static CleanupPreviewCheck *instance();

  void setToggle(QAction *toggle);

  bool isEnabled() const;
  void setIsEnabled(bool on);

private:
  CleanupPreviewCheck();
};

//=============================================================================

class DVAPI SetScanCropboxCheck {
  bool m_enabled;
  QAction *m_toggle;

public:
  static SetScanCropboxCheck *instance();

  void setToggle(QAction *toggle);

  bool isEnabled() const { return m_enabled; }
  void setIsEnabled(bool on);

  void uncheck();

private:
  SetScanCropboxCheck();
};

#endif
