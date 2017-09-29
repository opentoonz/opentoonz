

#include "toonzqt/cleanupcamerasettingswidget.h"
#include "toonzqt/camerasettingswidget.h"
#include "toonz/cleanupparameters.h"
#include "toonz/preferences.h"
#include "toonz/stage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QList>
#include <QGroupBox>
#include <QCheckBox>
#include "tfilepath.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"

using namespace std;
using namespace DVGui;

CleanupCameraSettingsWidget::CleanupCameraSettingsWidget() {
  m_cameraWidget = new CameraSettingsWidget(true);
  m_offsX        = new MeasuredDoubleLineEdit();
  m_offsY        = new MeasuredDoubleLineEdit();

  m_cameraWidget->setOffsetWidgetPointers(m_offsX, m_offsY);

  /*--- オフセットを軸ごとにロックするかどうか ---*/
  m_offsx_lock = new QCheckBox("", this);
  m_offsy_lock = new QCheckBox("", this);
  //---

  m_offsX->setMeasure("length.x");
  m_offsY->setMeasure("length.y");
  m_offsX->setDecimals(3);
  m_offsY->setDecimals(3);
  m_offsx_lock->setObjectName("EditToolLockButton");
  m_offsy_lock->setObjectName("EditToolLockButton");

  // m_cameraWidget->hideUseLevelSettingsBtn();

  //--- layout
  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setMargin(5);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(m_cameraWidget);

    QGridLayout *offsetLay = new QGridLayout();
    offsetLay->setHorizontalSpacing(3);
    offsetLay->setVerticalSpacing(3);
    offsetLay->setMargin(3);
    {
      offsetLay->addWidget(new QLabel(tr("N/S")), 0, 0);
      offsetLay->addWidget(m_offsY, 0, 1);
      offsetLay->addWidget(m_offsy_lock, 0, 2);
      offsetLay->addWidget(new QLabel(tr("E/W")), 1, 0);
      offsetLay->addWidget(m_offsX, 1, 1);
      offsetLay->addWidget(m_offsx_lock, 1, 2);
    }
    offsetLay->setColumnStretch(0, 0);
    offsetLay->setColumnStretch(1, 1);

    /*--- プリセットComboBoxの上にOffsetを挿入する ---*/
    QBoxLayout *camLay = qobject_cast<QBoxLayout *>(m_cameraWidget->layout());
    if (camLay) camLay->insertLayout(2, offsetLay, 0);
  }
  setLayout(mainLay);

  bool ret = true;
  ret      = ret && connect(m_offsX, SIGNAL(editingFinished()),
                       SIGNAL(cleanupSettingsChanged()));
  ret = ret && connect(m_offsY, SIGNAL(editingFinished()),
                       SIGNAL(cleanupSettingsChanged()));
  ret = ret && connect(m_offsx_lock, SIGNAL(clicked(bool)),
                       SIGNAL(cleanupSettingsChanged()));
  ret = ret && connect(m_offsy_lock, SIGNAL(clicked(bool)),
                       SIGNAL(cleanupSettingsChanged()));
  ret = ret && connect(m_cameraWidget, SIGNAL(changed()),
                       SIGNAL(cleanupSettingsChanged()));
  assert(ret);
}

CleanupCameraSettingsWidget::~CleanupCameraSettingsWidget() {}

void CleanupCameraSettingsWidget::setCameraPresetListFile(const TFilePath &fp) {
  m_cameraWidget->setPresetListFile(fp);
}

void CleanupCameraSettingsWidget::setFields(
    CleanupParameters *cleanupParameters) {
  if (Preferences::instance()->getPixelsOnly()) {
    TDimension res = cleanupParameters->m_camera.getRes();
    cleanupParameters->m_camera.setSize(
        TDimensionD(res.lx / Stage::standardDpi, res.ly / Stage::standardDpi));
  }
  m_cameraWidget->setFields(&cleanupParameters->m_camera);
  m_offsX->setValue(cleanupParameters->m_offx);
  m_offsY->setValue(cleanupParameters->m_offy);
  m_offsx_lock->setChecked(cleanupParameters->m_offx_lock);
  m_offsy_lock->setChecked(cleanupParameters->m_offy_lock);
}

void CleanupCameraSettingsWidget::getFields(
    CleanupParameters *cleanupParameters) {
  m_cameraWidget->getFields(&cleanupParameters->m_camera);
  cleanupParameters->m_offx      = m_offsX->getValue();
  cleanupParameters->m_offy      = m_offsY->getValue();
  cleanupParameters->m_offx_lock = m_offsx_lock->isChecked();
  cleanupParameters->m_offy_lock = m_offsy_lock->isChecked();
}

double CleanupCameraSettingsWidget::getClosestFieldValue() const { return 999; }

void CleanupCameraSettingsWidget::setImageInfo(const TFilePath &imgPath) {}

void CleanupCameraSettingsWidget::setImageInfo(int w, int h, double dpix,
                                               double dpiy) {}

void CleanupCameraSettingsWidget::setCurrentLevel(TXshLevel *level) {
  m_cameraWidget->setCurrentLevel(level);
}
