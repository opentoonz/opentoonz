

// TnzLib includes
#include "toonz/stage2.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshlevel.h"

// TnzBase includes
#include "tenv.h"

#include "toonz/palettecontroller.h"

TEnv::IntVar PaletteControllerAutoApplyState("PaletteControllerAutoApplyState",
                                             1);

PaletteController::PaletteController()
    : QObject()
    , m_currentLevelPalette(0)
    , m_currentCleanupPalette(0)
    , m_originalCurrentPalette(0)
    , m_currentPalette(0)
    , m_colorAutoApplyEnabled(PaletteControllerAutoApplyState != 0)
    , m_colorSample() {
  m_currentLevelPalette   = new TPaletteHandle;
  m_currentCleanupPalette = new TPaletteHandle;
  m_currentPalette        = new TPaletteHandle;

  QObject::connect(m_currentCleanupPalette, SIGNAL(paletteSwitched()), this,
                   SLOT(editCleanupPalette()));
  QObject::connect(m_currentCleanupPalette, SIGNAL(colorStyleSwitched()), this,
                   SLOT(editCleanupPalette()));

  QObject::connect(m_currentLevelPalette, SIGNAL(paletteSwitched()), this,
                   SLOT(editLevelPalette()));
  QObject::connect(m_currentLevelPalette, SIGNAL(colorStyleSwitched()), this,
                   SLOT(editLevelPalette()));
  // QObject::connect(m_currentLevelPalette, SIGNAL(colorStyleSwitched()), this,
  // SLOT(setColorCheckIndex()));

  QObject::connect(m_currentLevelPalette, SIGNAL(paletteLockChanged()), this,
                   SLOT(editLevelPalette()));
}

//-----------------------------------------------------------------------------

PaletteController::~PaletteController() {
  delete m_currentLevelPalette;
  delete m_currentCleanupPalette;
  delete m_currentPalette;
}

//-----------------------------------------------------------------------------

void PaletteController::setCurrentPalette(TPaletteHandle *paletteHandle) {
  // Current palette redirects to the palette of another palette handle -
  // namely one of either the current level palette or current cleanup
  // palette - even though m_currentPalette is a standalone palette handle.

  // In order to correctly support the notification system notify*() functions
  // perform signal BROADCASTING to all palette handles implicitly mapping to
  // the associated palette:

  // in case the handle is not changed, skip the reconnection
  if (m_originalCurrentPalette == paletteHandle) {
    if (!paletteHandle) return;
    m_currentPalette->setPalette(paletteHandle->getPalette(),
                                 paletteHandle->getStyleIndex());
    return;
  }

  if (m_originalCurrentPalette) {
    m_originalCurrentPalette->disconnectBroadcasts(m_currentPalette);
    m_currentPalette->disconnectBroadcasts(m_originalCurrentPalette);
  }

  m_originalCurrentPalette = paletteHandle;

  if (!paletteHandle) return;

  m_currentPalette->setPalette(paletteHandle->getPalette(),
                               paletteHandle->getStyleIndex());

  m_originalCurrentPalette->connectBroadcasts(m_currentPalette);
  m_currentPalette->connectBroadcasts(m_originalCurrentPalette);
}

//-----------------------------------------------------------------------------

void PaletteController::editLevelPalette() {
  setCurrentPalette(m_currentLevelPalette);
  emit(checkPaletteLock());
}

//-----------------------------------------------------------------------------

void PaletteController::editCleanupPalette() {
  setCurrentPalette(m_currentCleanupPalette);
}

//-----------------------------------------------------------------------------

void PaletteController::enableColorAutoApply(bool enabled) {
  if (m_colorAutoApplyEnabled != enabled) {
    m_colorAutoApplyEnabled         = enabled;
    PaletteControllerAutoApplyState = (enabled) ? 1 : 0;
    emit colorAutoApplyEnabled(m_colorAutoApplyEnabled);
  }
}

//-----------------------------------------------------------------------------

void PaletteController::setColorSample(const TPixel32 &color) {
  if (m_colorSample != color) {
    m_colorSample = color;
    emit colorSampleChanged(m_colorSample);
  }
}
