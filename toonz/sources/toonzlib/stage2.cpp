

#include "toonz/stage2.h"
#include "toonz/imagemanager.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzfolders.h"
#include "toonz/glrasterpainter.h"
#include "tpalette.h"
#include "tropcm.h"
#include "tcolorfunctions.h"
#include "tgl.h"
#include "tvectorgl.h"
#include "tvectorrenderdata.h"
#include "tstream.h"
#include "tsystem.h"
#include "tstencilcontrol.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include <QAction>
//=============================================================================
namespace Stage {
//-----------------------------------------------------------------------------

const double bigBoxSize[] = {500, 500, 1000};

//=============================================================================

//-----------------------------------------------------------------------------
}  // namespace Stage
//=============================================================================

// for all the checks: transparency check, etc.

ToonzCheck *ToonzCheck::instance() {
  static ToonzCheck _instance;
  return &_instance;
}

//=============================================================================
/*! \class CameraTestCheck
                \brief The CameraTestCheck class allows visualization in
   "CameraTest" mode, from cleanup menu
*/
//=============================================================================

CameraTestCheck::CameraTestCheck() : m_toggle(0) {}

//-----------------------------------------------------------------------------
/*! Return current \b CameraTestCheck instance.
*/
CameraTestCheck *CameraTestCheck::instance() {
  static CameraTestCheck _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

bool CameraTestCheck::isEnabled() const {
  return m_toggle ? m_toggle->isChecked() : false;
}

//-----------------------------------------------------------------------------

void CameraTestCheck::setIsEnabled(bool on) {
  if (!m_toggle) return;

  if (m_toggle->isChecked() != on)
    m_toggle->trigger();  // Please, note that this is different than using
                          // setEnabled(bool)
}

//-----------------------------------------------------------------------------

void CameraTestCheck::setToggle(QAction *toggle) { m_toggle = toggle; }

//-----------------------------------------------------------------------------
/*! Boot \b CameraTestCheck color to color saved in file \b tcheckColors.xml.
*/

//=============================================================================
/*! \class CleanupViewCheck
                \brief The CameraTestCheck class allows visualization in
   "CameraTest" mode, from cleanup menu
*/
//=============================================================================

CleanupPreviewCheck::CleanupPreviewCheck() : m_toggle(0) {}

//-----------------------------------------------------------------------------
/*! Return current \b TransparencyCheck instance.
*/
CleanupPreviewCheck *CleanupPreviewCheck::instance() {
  static CleanupPreviewCheck _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

bool CleanupPreviewCheck::isEnabled() const {
  return m_toggle ? m_toggle->isChecked() : false;
}

//-----------------------------------------------------------------------------

void CleanupPreviewCheck::setIsEnabled(bool on) {
  if (!m_toggle) return;

  if (m_toggle->isChecked() != on)
    m_toggle->trigger();  // Please, note that this is different than using
                          // setEnabled(bool)
}

//-----------------------------------------------------------------------------

void CleanupPreviewCheck::setToggle(QAction *toggle) { m_toggle = toggle; }

//=============================================================================
/*! \class SetScanCropBoxCheck
                \brief The SetScanCropBoxCheck class allows visualization in
   "Set Crop Box" mode, from scan menu
*/
//=============================================================================

SetScanCropboxCheck::SetScanCropboxCheck() : m_enabled(false), m_toggle(0) {}

//-----------------------------------------------------------------------------

/*! Return current \b SetScanCropBoxCheck instance.
*/
SetScanCropboxCheck *SetScanCropboxCheck::instance() {
  static SetScanCropboxCheck _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void SetScanCropboxCheck::setToggle(QAction *toggle) { m_toggle = toggle; }

//-----------------------------------------------------------------------------

void SetScanCropboxCheck::setIsEnabled(bool on) {
  if (!m_toggle) return;
  m_enabled = on;
  m_toggle->setChecked(on);
}

//-----------------------------------------------------------------------------

void SetScanCropboxCheck::uncheck() {
  if (isEnabled()) m_toggle->trigger();
}
