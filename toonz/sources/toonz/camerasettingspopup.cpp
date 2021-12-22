

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "camerasettingspopup.h"
#include "reslist.h"
#include "castselection.h"

// TnzTools includes
#include "tools/tooloptions.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/camerasettingswidget.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tcamera.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/cleanupparameters.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/stage2.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tconvert.h"
#include "tsystem.h"
#include "tfilepath_io.h"

// Qt includes
#include <QLabel>
#include <QGridLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QPushButton>
#include <QMoveEvent>
#include <QMainWindow>

using namespace std;
using namespace DVGui;

class CameraSettingsPopup;

//=============================================================================

class OpenCameraStageCommandHandler final : public MenuItemHandler {
  CommandId m_id;

public:
  OpenCameraStageCommandHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}
  void execute() override {
    TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
    TStageObjectId cameraId =
        TApp::instance()->getCurrentObject()->getObjectId();
    CameraSettingsPopup *popup = CameraSettingsPopup::createPopup(cameraId);
    int index                  = cameraId.getIndex();
    popup->move(popup->pos() + QPoint(20, 10) * index);
    popup->show();
    popup->raise();
    popup->activateWindow();
  }
} OpenCameraStageCommandHandler(MI_CameraStage);

//=============================================================================

std::map<TStageObjectId, CameraSettingsPopup *> CameraSettingsPopup::m_popups;

CameraSettingsPopup::CameraSettingsPopup()
    : QDialog(TApp::instance()->getMainWindow()) {
  m_nameFld              = new LineEdit();
  m_cameraSettingsWidget = new CameraSettingsWidget();

  m_cameraSettingsWidget->setPresetListFile(ToonzFolder::getReslistPath(false));

  //---- layout
  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setMargin(5);
  mainLay->setSpacing(8);
  {
    QHBoxLayout *nameLay = new QHBoxLayout();
    nameLay->setMargin(0);
    nameLay->setSpacing(3);
    {
      nameLay->addWidget(new QLabel(tr("Name:")), 0);
      nameLay->addWidget(m_nameFld, 1);
    }
    mainLay->addLayout(nameLay, 0);

    mainLay->addWidget(m_cameraSettingsWidget, 1);
  }
  setLayout(mainLay);

  //---- signal-slot connections
  bool ret = true;
  ret      = ret &&
        connect(m_cameraSettingsWidget, SIGNAL(changed()), SLOT(onChanged()));
  ret = ret &&
        connect(m_nameFld, SIGNAL(editingFinished()), SLOT(onNameChanged()));
  assert(ret);
}

void CameraSettingsPopup::moveEvent(QMoveEvent *e) {
  QPoint p    = pos();
  QPoint oldP = e->oldPos();
  QDialog::moveEvent(e);
}

void CameraSettingsPopup::showEvent(QShowEvent *e) {
  updateWindowTitle();
  updateFields();
  m_cameraSettingsWidget->setCurrentLevel(
      TApp::instance()->getCurrentLevel()->getLevel());

  TSceneHandle *sceneHandle    = TApp::instance()->getCurrentScene();
  TObjectHandle *objectHandle  = TApp::instance()->getCurrentObject();
  TXsheetHandle *xsheetHandle  = TApp::instance()->getCurrentXsheet();
  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();

  bool ret = true;
  ret =
      ret && connect(sceneHandle, SIGNAL(sceneChanged()), SLOT(updateFields()));
  ret = ret &&
        connect(sceneHandle, SIGNAL(sceneSwitched()), SLOT(updateFields()));
  ret = ret && connect(objectHandle, SIGNAL(objectChanged(bool)),
                       SLOT(updateFields(bool)));
  ret = ret &&
        connect(objectHandle, SIGNAL(objectSwitched()), SLOT(updateFields()));
  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetSwitched()), SLOT(updateFields()));
  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetChanged()), SLOT(updateFields()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)),
                       SLOT(onLevelSwitched(TXshLevel *)));
  assert(ret);
}

void CameraSettingsPopup::hideEvent(QHideEvent *e) {
  m_cameraSettingsWidget->setCurrentLevel(0);

  if (m_cameraId != TStageObjectId::NoneId) {
    // Remove the popup from currentlyOpened ones and schedule for deletion
    m_popups.erase(m_cameraId);
    m_cameraId = TStageObjectId::NoneId;
    deleteLater();
  }

  TSceneHandle *sceneHandle    = TApp::instance()->getCurrentScene();
  TObjectHandle *objectHandle  = TApp::instance()->getCurrentObject();
  TXsheetHandle *xsheetHandle  = TApp::instance()->getCurrentXsheet();
  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();

  bool ret = true;
  ret      = ret && disconnect(sceneHandle, SIGNAL(sceneChanged()), this,
                          SLOT(updateFields()));
  ret      = ret && disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
                          SLOT(updateFields()));
  ret      = ret && disconnect(objectHandle, SIGNAL(objectChanged(bool)), this,
                          SLOT(updateFields(bool)));
  ret      = ret && disconnect(objectHandle, SIGNAL(objectSwitched()), this,
                          SLOT(updateFields()));
  ret      = ret && disconnect(xsheetHandle, SIGNAL(xsheetSwitched()), this,
                          SLOT(updateFields()));
  ret      = ret && disconnect(xsheetHandle, SIGNAL(xsheetChanged()), this,
                          SLOT(updateFields()));
  ret = ret && disconnect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)),
                          this, SLOT(onLevelSwitched(TXshLevel *)));
  assert(ret);
}

CameraSettingsPopup *CameraSettingsPopup::createPopup(
    const TStageObjectId &id) {
  std::map<TStageObjectId, CameraSettingsPopup *>::iterator it =
      m_popups.find(id);
  if (it == m_popups.end()) {
    CameraSettingsPopup *popup = new CameraSettingsPopup();
    popup->attachToCamera(id);
    m_popups[id] = popup;
    return popup;
  } else
    return it->second;
}

TStageObject *CameraSettingsPopup::getCameraObject() {
  TXsheet *xsheet         = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = m_cameraId;
  if (!cameraId.isCamera())
    cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
  if (!cameraId.isCamera()) return 0;
  return xsheet->getStageObject(cameraId);
}

TCamera *CameraSettingsPopup::getCamera() {
  TStageObject *cameraObject = getCameraObject();
  return cameraObject ? cameraObject->getCamera() : 0;
}

void CameraSettingsPopup::updateWindowTitle() {
  if (m_cameraId.isCamera()) {
    setWindowTitle(tr("Camera#%1 Settings")
                       .arg(QString::number(1 + m_cameraId.getIndex())));
  } else {
    setWindowTitle(tr("Current Camera Settings"));
  }
}

void CameraSettingsPopup::onChanged() {
  TCamera *camera = getCamera();
  if (!camera) return;
  if (m_cameraSettingsWidget->getFields(camera)) {
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

    emit changed();
  }
}

void CameraSettingsPopup::onNameChanged() {
  TStageObject *cameraObject = getCameraObject();
  if (!cameraObject) return;
  std::string name    = m_nameFld->text().toStdString();
  std::string curName = cameraObject->getName();
  if (curName == name) return;
  cameraObject->setName(name);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void CameraSettingsPopup::updateFields() {
  TStageObject *cameraObject = getCameraObject();
  if (!cameraObject) return;  // it should never happen
  m_nameFld->setText(QString(cameraObject->getName().c_str()));
  TCamera *camera = cameraObject->getCamera();
  if (Preferences::instance()->getPixelsOnly()) {
    TDimension res = camera->getRes();
    camera->setSize(
        TDimensionD(res.lx / Stage::standardDpi, res.ly / Stage::standardDpi));
  }
  if (camera) m_cameraSettingsWidget->setFields(camera);
}

void CameraSettingsPopup::onLevelSwitched(TXshLevel *) {
  m_cameraSettingsWidget->setCurrentLevel(
      TApp::instance()->getCurrentLevel()->getLevel());
}

//=============================================================================

OpenPopupCommandHandler<CameraSettingsPopup> openCameraSettingsPopup(
    MI_CameraSettings);
