#include "locatorpopup.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"
#include "toonz/stage2.h"

// Tnz6 includes
#include "tapp.h"
#include "sceneviewer.h"

#include <QVBoxLayout>

LocatorPopup::LocatorPopup(QWidget *parent)
    : QDialog(parent), m_initialZoom(true) {
  m_viewer = new SceneViewer(NULL);
  m_viewer->setParent(parent);
  m_viewer->setIsLocator();

  //---- layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->addWidget(m_viewer, 1);
  setLayout(mainLayout);

  bool ret = true;
  // When zoom changed, change window title.
  ret = connect(m_viewer, SIGNAL(onZoomChanged()), SLOT(changeWindowTitle()));
  ret = ret &&
        connect(m_viewer, SIGNAL(previewToggled()), SLOT(changeWindowTitle()));
  assert(ret);

  resize(400, 400);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onChangeViewAff(const TPointD &pos) {
  TAffine curAff = m_viewer->getSceneMatrix();
  TAffine newAff(curAff.a11, 0, -pos.x * curAff.a11, 0, curAff.a22,
                 -pos.y * curAff.a22);
  m_viewer->setViewMatrix(newAff, 0);
  m_viewer->setViewMatrix(newAff, 1);
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void LocatorPopup::showEvent(QShowEvent *) {
  // zoom the locator for the first time
  if (m_initialZoom) {
    for (int z    = 0; z < 4; z++) m_viewer->zoomQt(true, false);
    m_initialZoom = false;
  }

  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();

  bool ret = true;
  ret      = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(changeWindowTitle()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
                       SLOT(changeWindowTitle()));
  assert(ret);

  changeWindowTitle();
}

//-----------------------------------------------------------------------------

void LocatorPopup::hideEvent(QHideEvent *) {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentLevel());
  disconnect(app->getCurrentFrame());
}

//-----------------------------------------------------------------------------

void LocatorPopup::changeWindowTitle() {
  TApp *app = TApp::instance();
  // put the titlebar texts in this string
  QString name = tr("Locator");

  bool showZoomFactor = false;

  // if the frame type is "scene editing"
  if (app->getCurrentFrame()->isEditingScene()) {
    if (m_viewer->isPreviewEnabled()) showZoomFactor = true;

    // If the current level exists and some option is set in the preference,
    // set the zoom value to the current level's dpi
    else if (Preferences::instance()
                 ->isActualPixelViewOnSceneEditingModeEnabled() &&
             app->getCurrentLevel()->getSimpleLevel() &&
             !CleanupPreviewCheck::instance()
                  ->isEnabled()  // cleanup preview must be OFF
             &&
             !CameraTestCheck::instance()
                  ->isEnabled())  // camera test mode must be OFF neither
      showZoomFactor = true;
  }
  // if the frame type is "level editing"
  else {
    TXshLevel *level          = app->getCurrentLevel()->getLevel();
    if (level) showZoomFactor = true;
  }

  if (showZoomFactor) {
    name = name + "  Zoom : " +
           QString::number((int)(100.0 * sqrt(m_viewer->getViewMatrix().det()) *
                                 m_viewer->getDpiFactor())) +
           "%";
  }
  setWindowTitle(name);
}