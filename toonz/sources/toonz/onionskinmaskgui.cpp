

#include "onionskinmaskgui.h"
#include "tapp.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshsimplelevel.h"

#include "toonz/onionskinmask.h"

#include <QMenu>

//=============================================================================
// OnioniSkinMaskGUI::OnionSkinSwitcher
//-----------------------------------------------------------------------------

OnionSkinMask OnioniSkinMaskGUI::OnionSkinSwitcher::getMask() const {
  return TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::OnionSkinSwitcher::setMask(const OnionSkinMask &mask) {
  TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(mask);
  TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
}

//------------------------------------------------------------------------------

bool OnioniSkinMaskGUI::OnionSkinSwitcher::isActive() const {
  OnionSkinMask osm = getMask();
  return osm.isEnabled() && !osm.isEmpty();
}

//------------------------------------------------------------------------------

bool OnioniSkinMaskGUI::OnionSkinSwitcher::isWholeScene() const {
  OnionSkinMask osm = getMask();
  return osm.isWholeScene();
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::OnionSkinSwitcher::activate() {
  OnionSkinMask osm = getMask();
  if (osm.isEnabled() && !osm.isEmpty()) return;
  if (osm.isEmpty()) {
    osm.setMos(-1, true);
    osm.setMos(-2, true);
    osm.setMos(-3, true);
  }
  osm.enable(true);
  setMask(osm);
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::OnionSkinSwitcher::deactivate() {
  OnionSkinMask osm = getMask();
  if (!osm.isEnabled() || osm.isEmpty()) return;
  osm.enable(false);
  setMask(osm);
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::OnionSkinSwitcher::setWholeScene() {
  OnionSkinMask osm = getMask();
  osm.setIsWholeScene(true);
  setMask(osm);
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::OnionSkinSwitcher::setSingleLevel() {
  OnionSkinMask osm = getMask();
  osm.setIsWholeScene(false);
  setMask(osm);
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::addOnionSkinCommand(QMenu *menu, bool isFilmStrip) {
  static OnioniSkinMaskGUI::OnionSkinSwitcher switcher;
  if (switcher.isActive()) {
    QAction *dectivateOnionSkin =
        menu->addAction(QString(QObject::tr("Deactivate Onion Skin")));
    menu->connect(dectivateOnionSkin, SIGNAL(triggered()), &switcher,
                  SLOT(deactivate()));
    if (!isFilmStrip) {
      if (switcher.isWholeScene()) {
        QAction *limitOnionSkinToLevel =
            menu->addAction(QString(QObject::tr("Limit Onion Skin To Level")));
        menu->connect(limitOnionSkinToLevel, SIGNAL(triggered()), &switcher,
                      SLOT(setSingleLevel()));
      } else {
        QAction *extendOnionSkinToScene =
            menu->addAction(QString(QObject::tr("Extend Onion Skin To Scene")));
        menu->connect(extendOnionSkinToScene, SIGNAL(triggered()), &switcher,
                      SLOT(setWholeScene()));
      }
    }
  } else {
    QAction *activateOnionSkin =
        menu->addAction(QString(QObject::tr("Activate Onion Skin")));
    menu->connect(activateOnionSkin, SIGNAL(triggered()), &switcher,
                  SLOT(activate()));
  }
}

//------------------------------------------------------------------------------

void OnioniSkinMaskGUI::resetShiftTraceFrameOffset() {
  auto setGhostOffset = [](int firstOffset, int secondOffset) {
    OnionSkinMask osm =
        TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
    osm.setShiftTraceGhostFrameOffset(0, firstOffset);
    osm.setShiftTraceGhostFrameOffset(1, secondOffset);
    TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
  };

  TApp *app = TApp::instance();
  if (app->getCurrentFrame()->isEditingLevel()) {
    TXshSimpleLevel *level = app->getCurrentLevel()->getSimpleLevel();
    if (!level) {
      setGhostOffset(0, 0);
      return;
    }
    TFrameId fid     = app->getCurrentFrame()->getFid();
    int firstOffset  = (fid > level->getFirstFid()) ? -1 : 0;
    int secondOffset = (fid < level->getLastFid()) ? 1 : 0;

    setGhostOffset(firstOffset, secondOffset);
  } else {  // when scene frame is selected
    TXsheet *xsh       = app->getCurrentXsheet()->getXsheet();
    int col            = app->getCurrentColumn()->getColumnIndex();
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) {
      setGhostOffset(0, 0);
      return;
    }
    int r0, r1;
    column->getRange(r0, r1);
    int row         = app->getCurrentFrame()->getFrame();
    TXshCell cell   = xsh->getCell(row, col);
    int firstOffset = -1;
    while (1) {
      int r = row + firstOffset;
      if (r < r0) {
        firstOffset = 0;
        break;
      }
      if (!xsh->getCell(r, col).isEmpty() && xsh->getCell(r, col) != cell) {
        break;
      }
      firstOffset--;
    }
    int secondOffset = 1;
    while (1) {
      int r = row + secondOffset;
      if (r > r1) {
        secondOffset = 0;
        break;
      }
      if (!xsh->getCell(r, col).isEmpty() && xsh->getCell(r, col) != cell) {
        break;
      }
      secondOffset++;
    }
    setGhostOffset(firstOffset, secondOffset);
  }
}