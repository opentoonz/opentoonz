

#include "scenesettingspopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "previewer.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"
#include "toutputproperties.h"

// TnzBase includes
#include "tunit.h"

// TnzCore includes
#include "trop.h"

// Qt includes
#include <QLayout>
#include <QComboBox>
#include <QLabel>
#include <QApplication>
#include <QMainWindow>
#include <QPainter>

using namespace DVGui;

//-----------------------------------------------------------------------------

namespace {

const int labelSize = 110;

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
}  //  namespace
//-----------------------------------------------------------------------------

/*
class CheckBoardRect final : public QWidget
{
TRaster32P m_ras;

public:
CheckBoardRect(QWidget *parent, int sizeX, int sizeY)
        : m_ras(sizeX,sizeY)
  {
  setFixedSize(sizeX,sizeY);
  setColors(TPixel32(0, 0, 0), TPixel32(255, 255, 255));
  }
void setColors(const TPixel32 &color1, const TPixel32 &color2)
  {
  TRop::checkBoard(m_ras, color1, color2, TDimensionD(m_ras->getLx()/8,
m_ras->getLy()/8),TPointD(0, 0));
  update();
  }
protected:
void paintEvent(QPaintEvent *event)
  {
        QPainter painter(this);
        QImage img(m_ras->getRawData(), m_ras->getLx(), m_ras->getLy(),
QImage::Format_ARGB32);
  painter.drawImage(0,0,img);
  }
};
*/

//=============================================================================
// SceneSettingsPopup
//-----------------------------------------------------------------------------

SceneSettingsPopup::SceneSettingsPopup()
    : QDialog(TApp::instance()->getMainWindow()) {
  setWindowTitle(tr("Scene Settings"));
  setObjectName("SceneSettings");
  TSceneProperties *sprop = getProperties();

  // Frame Rate
  double frameRate = sprop->getOutputProperties()->getFrameRate();
  m_frameRateFld   = new DoubleLineEdit(this, frameRate);
  m_frameRateFld->setRange(1, 100);
  m_frameRateFld->setDecimals(2);

  // Camera BG color
  m_bgColorFld = new ColorField(this, true, sprop->getBgColor());

  // Field Guide Size - A/R
  int fieldGuideSize = sprop->getFieldGuideSize();
  m_fieldGuideFld    = new DVGui::IntLineEdit(this, fieldGuideSize, 0, 50);
  m_aspectRatioFld   = new DoubleLineEdit(this, 1.38);
  m_aspectRatioFld->setRange(-10000.0, 10000.0);
  m_aspectRatioFld->setDecimals(5);

  // Image Subsampling  - Tlv Subsampling
  int fullcolorSubsampling = sprop->getFullcolorSubsampling();
  m_fullcolorSubsamplingFld =
      new DVGui::IntLineEdit(this, fullcolorSubsampling, 1);

  int tlvSubsampling  = sprop->getTlvSubsampling();
  m_tlvSubsamplingFld = new DVGui::IntLineEdit(this, tlvSubsampling, 1);

  // Marker Interval - Start Frame
  int distance, offset;
  sprop->getMarkers(distance, offset);
  m_markerIntervalFld = new DVGui::IntLineEdit(this, distance, 0);
  m_startFrameFld     = new DVGui::IntLineEdit(this, offset);

  // Whether the column color filter and transparency is available also in
  // render
  m_colorFilterOnRenderCB = new DVGui::CheckBox(
      tr("Enable Column Color Filter and Transparency for Rendering"), this);
  m_colorFilterOnRenderCB->setChecked(
      sprop->isColumnColorFilterOnRenderEnabled());

  // layout
  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setMargin(10);
  mainLayout->setHorizontalSpacing(5);
  mainLayout->setVerticalSpacing(15);
  {
    // Frame Rate
    mainLayout->addWidget(new QLabel(tr("Frame Rate:"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_frameRateFld, 0, 1);
    // Camera BG color
    mainLayout->addWidget(new QLabel(tr("Camera BG Color:"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_bgColorFld, 1, 1, 1, 4);
    // Field Guide Size - A/R
    mainLayout->addWidget(new QLabel(tr("Field Guide Size:"), this), 2, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_fieldGuideFld, 2, 1);
    mainLayout->addWidget(new QLabel(tr("A/R:"), this), 2, 2,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_aspectRatioFld, 2, 3);
    // Image Subsampling  - Tlv Subsampling
    mainLayout->addWidget(new QLabel(tr("Image Subsampling:"), this), 3, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_fullcolorSubsamplingFld, 3, 1);
    if (m_tlvSubsamplingFld) {
      mainLayout->addWidget(new QLabel(tr("TLV Subsampling:"), this), 4, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      mainLayout->addWidget(m_tlvSubsamplingFld, 4, 1);
    }
    // Marker Interval - Start Frame
    mainLayout->addWidget(new QLabel(tr("Marker Interval:"), this), 5, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_markerIntervalFld, 5, 1);
    mainLayout->addWidget(new QLabel(tr("  Start Frame:"), this), 5, 2,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_startFrameFld, 5, 3);

    // Use Color Filter and Transparency for Rendering
    mainLayout->addWidget(m_colorFilterOnRenderCB, 6, 0, 1, 4);
  }
  mainLayout->setColumnStretch(0, 0);
  mainLayout->setColumnStretch(1, 0);
  mainLayout->setColumnStretch(2, 0);
  mainLayout->setColumnStretch(3, 0);
  mainLayout->setColumnStretch(4, 1);
  mainLayout->setRowStretch(7, 1);
  setLayout(mainLayout);

  // signal-slot connections
  bool ret = true;
  // Frame Rate
  ret = ret && connect(m_frameRateFld, SIGNAL(editingFinished()), this,
                       SLOT(onFrameRateEditingFinished()));
  // Camera BG color
  ret =
      ret && connect(m_bgColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                     this, SLOT(setBgColor(const TPixel32 &, bool)));
  // Field Guide Size - A/R
  ret = ret && connect(m_fieldGuideFld, SIGNAL(editingFinished()), this,
                       SLOT(onFieldGuideSizeEditingFinished()));
  ret = ret && connect(m_aspectRatioFld, SIGNAL(editingFinished()), this,
                       SLOT(onFieldGuideAspectRatioEditingFinished()));
  // Image Subsampling  - Tlv Subsampling
  ret = ret && connect(m_fullcolorSubsamplingFld, SIGNAL(editingFinished()),
                       this, SLOT(onFullColorSubsampEditingFinished()));
  if (m_tlvSubsamplingFld)
    ret = ret && connect(m_tlvSubsamplingFld, SIGNAL(editingFinished()), this,
                         SLOT(onTlvSubsampEditingFinished()));
  // Marker Interval - Start Frame
  ret = ret && connect(m_markerIntervalFld, SIGNAL(editingFinished()), this,
                       SLOT(onMakerIntervalEditingFinished()));
  ret = ret && connect(m_startFrameFld, SIGNAL(editingFinished()), this,
                       SLOT(onStartFrameEditingFinished()));
  // Use Color Filter and Transparency for Rendering
  ret = ret && connect(m_colorFilterOnRenderCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onColorFilterOnRenderChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::showEvent(QShowEvent *e) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneChanged()), SLOT(update()));
  connect(sceneHandle, SIGNAL(sceneSwitched()), SLOT(update()));

  update();
}

void SceneSettingsPopup::hideEvent(QHideEvent *e) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(update()));
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------

TSceneProperties *SceneSettingsPopup::getProperties() const {
  return TApp::instance()->getCurrentScene()->getScene()->getProperties();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::update() {
  TSceneProperties *sprop = getProperties();

  QString str;
  m_frameRateFld->setValue(sprop->getOutputProperties()->getFrameRate());

  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  m_bgColorFld->setChessboardColors(col1, col2);

  TPixel bgColor = sprop->getBgColor();
  m_bgColorFld->setColor(bgColor);

  m_fieldGuideFld->setValue(sprop->getFieldGuideSize());
  m_aspectRatioFld->setValue(sprop->getFieldGuideAspectRatio());

  UnitParameters::setFieldGuideAspectRatio(sprop->getFieldGuideAspectRatio());
  m_fullcolorSubsamplingFld->setValue(sprop->getFullcolorSubsampling());
  if (m_tlvSubsamplingFld)
    m_tlvSubsamplingFld->setValue(sprop->getTlvSubsampling());
  int markerDistance = 0, markerOffset = 0;
  sprop->getMarkers(markerDistance, markerOffset);
  m_markerIntervalFld->setValue(markerDistance);
  m_startFrameFld->setValue(markerOffset + 1);
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFrameRateEditingFinished() {
  TSceneProperties *sprop  = getProperties();
  double frameRate         = sprop->getOutputProperties()->getFrameRate();
  double frameRateFldValue = m_frameRateFld->getValue();
  if (frameRate == frameRateFldValue) return;
  sprop->getOutputProperties()->setFrameRate(frameRateFldValue);
  TApp::instance()->getCurrentScene()->getScene()->updateSoundColumnFrameRate();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFieldGuideSizeEditingFinished() {
  TSceneProperties *sprop = getProperties();
  int fieldGuideSize      = sprop->getFieldGuideSize();
  int fieldGuideSizefld   = m_fieldGuideFld->text().toInt();
  if (fieldGuideSize == fieldGuideSizefld) return;
  sprop->setFieldGuideSize(fieldGuideSizefld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFieldGuideAspectRatioEditingFinished() {
  TSceneProperties *sprop      = getProperties();
  double fieldGuideAspectRatio = sprop->getFieldGuideAspectRatio();
  double aspectRatioFld        = m_aspectRatioFld->text().toDouble();
  if (fieldGuideAspectRatio == aspectRatioFld) return;
  sprop->setFieldGuideAspectRatio(aspectRatioFld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  UnitParameters::setFieldGuideAspectRatio(m_aspectRatioFld->text().toDouble());
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFullColorSubsampEditingFinished() {
  TSceneProperties *sprop     = getProperties();
  int fullcolorSubsampling    = sprop->getFullcolorSubsampling();
  int fullcolorSubsamplingFld = m_fullcolorSubsamplingFld->getValue();
  if (fullcolorSubsampling == fullcolorSubsamplingFld) return;
  sprop->setFullcolorSubsampling(fullcolorSubsamplingFld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onTlvSubsampEditingFinished() {
  if (!m_tlvSubsamplingFld) return;
  TSceneProperties *sprop = getProperties();
  int tlvSubsampling      = sprop->getTlvSubsampling();
  int tlvSubsamplingFld   = m_tlvSubsamplingFld->getValue();
  if (tlvSubsamplingFld == tlvSubsampling) return;
  sprop->setTlvSubsampling(tlvSubsamplingFld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onMakerIntervalEditingFinished() {
  TSceneProperties *sprop = getProperties();
  int distance, offset;
  sprop->getMarkers(distance, offset);
  int markerDistance = m_markerIntervalFld->text().toInt();
  int markerOffset   = m_startFrameFld->text().toInt() - 1;
  assert(offset == markerOffset);
  if (distance == markerDistance) return;
  sprop->setMarkers(markerDistance, markerOffset);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onStartFrameEditingFinished() {
  TSceneProperties *sprop = getProperties();
  int distance, offset;
  sprop->getMarkers(distance, offset);
  int markerDistance = m_markerIntervalFld->text().toInt();
  int markerOffset   = m_startFrameFld->text().toInt() - 1;
  assert(markerDistance == distance);
  if (offset == markerOffset) return;
  sprop->setMarkers(markerDistance, markerOffset);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::setBgColor(const TPixel32 &bgColor, bool isDragging) {
  TSceneProperties *sprop = getProperties();
  sprop->setBgColor(bgColor);
  // TODO: forse sarebbe meglio usare una notifica piu' specifica
  if (!isDragging) Previewer::clearAll();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onColorFilterOnRenderChanged() {
  TSceneProperties *sprop = getProperties();
  sprop->enableColumnColorFilterOnRender(m_colorFilterOnRenderCB->isChecked());
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//=============================================================================

OpenPopupCommandHandler<SceneSettingsPopup> openSceneSettingsPopup(
    MI_SceneSettings);
