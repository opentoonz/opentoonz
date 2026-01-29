

#include "exportcameratrackpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "filebrowser.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/colorfield.h"
#include "toonzqt/filefield.h"
#include "toonzqt/doublefield.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tcamera.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"
#include "toonz/dpiscale.h"
#include "toonz/tproject.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "filebrowserpopup.h"

// TnzCore includes
#include "trop.h"
#include "tsystem.h"
#include "tenv.h"
#include "tropcm.h"
#include "tpalette.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QMouseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRegularExpressionValidator>
#include <QPolygonF>
#include <QVector2D>
#include <QFontMetricsF>

TEnv::DoubleVar CameraTrackExportBgOpacity("CameraTrackExportBgOpacity", 0.5);
TEnv::StringVar CameraTrackExportLineColor(
    "CameraTrackExportLineColor", QColor(Qt::red).name().toStdString());
TEnv::IntVar CameraTrackExportCamRectOnKeys("CameraTrackExportCamRectOnKeys",
                                            1);
TEnv::IntVar CameraTrackExportCamRectOnTags("CameraTrackExportCamRectOnTags",
                                            0);
TEnv::IntVar CameraTrackExportLineTL("CameraTrackExportLineTL", 0);
TEnv::IntVar CameraTrackExportLineTR("CameraTrackExportLineTR", 0);
TEnv::IntVar CameraTrackExportLineCenter("CameraTrackExportLineCenter", 1);
TEnv::IntVar CameraTrackExportLineBL("CameraTrackExportLineBL", 0);
TEnv::IntVar CameraTrackExportLineBR("CameraTrackExportLineBR", 0);
TEnv::IntVar CameraTrackExportGraduationInterval(
    "CameraTrackExportGraduationInterval", 1);
TEnv::IntVar CameraTrackExportNumberAt("CameraTrackExportNumberAt",
                                       static_cast<int>(Qt::TopLeftCorner));
TEnv::IntVar CameraTrackExportNumbersOnLine("CameraTrackExportNumbersOnLine",
                                            1);
TEnv::StringVar CameraTrackExportFont("CameraTrackExportFont", "");
TEnv::IntVar CameraTrackExportFontSize("CameraTrackExportFontSize", 30);

namespace {

void getCameraPlacement(TAffine& aff, TXsheet* xsh, double row,
                        const TStageObjectId& objId,
                        const TStageObjectId& cameraId) {
  TStageObject* pegbar =
      xsh->getStageObjectTree()->getStageObject(objId, false);
  if (!pegbar) return;

  TAffine objAff  = pegbar->getPlacement(row);
  double objZ     = pegbar->getZ(row);
  double noScaleZ = pegbar->getGlobalNoScaleZ();

  TStageObject* camera = xsh->getStageObject(cameraId);
  TAffine cameraAff    = camera->getPlacement(row);
  double cameraZ       = camera->getZ(row);

  bool isVisible = TStageObject::perspective(aff, cameraAff, cameraZ, objAff,
                                             objZ, noScaleZ);

  aff = aff.inv() * cameraAff;
}

// recursively find key frame
bool isKey(int frame, TStageObject* obj, TXsheet* xsh) {
  if (obj->isKeyframe(frame)) return true;
  if (obj->getParent() != TStageObjectId::NoneId)
    return isKey(frame, xsh->getStageObject(obj->getParent()), xsh);
  return false;
}

void drawOutlinedText(QPainter& p, const QPointF& pos, const QString& str) {
  QPainterPath path;
  path.addText(pos, p.font(), str);
  p.setPen(QPen(Qt::white, 3));
  p.drawPath(path);
  p.setPen(Qt::NoPen);
  p.drawPath(path);
}

// {0,1,2,3,6,8,9,10} => "1-4,7,9-11"
QString framesToString(const QList<int>& frames) {
  QString frameStr;
  bool prevIsHyphen = false;
  for (int i = 0; i < frames.size(); i++) {
    int f = frames.at(i);
    if (i == 0) {
      frameStr = QString::number(f + 1);
      continue;
    }
    if (i == frames.size() - 1) {
      if (prevIsHyphen)
        frameStr += QString::number(f + 1);
      else
        frameStr += ", " + QString::number(f + 1);
      break;
    }
    if (frames.at(i - 1) == f - 1) {
      if (frames.at(i + 1) == f + 1) {
        if (prevIsHyphen)
          continue;
        else {
          frameStr += "-";
          prevIsHyphen = true;
        }
      } else {
        if (prevIsHyphen) {
          frameStr += QString::number(f + 1);
          prevIsHyphen = false;
        } else {
          frameStr += ", " + QString::number(f + 1);
        }
      }
    } else {
      frameStr += ", " + QString::number(f + 1);
    }
  }
  return frameStr;
}
}  // namespace

//-----------------------------------------------------------------------------
CameraTrackPreviewPane::CameraTrackPreviewPane(QWidget* parent)
    : QWidget(parent), m_scaleFactor(1.0) {}

void CameraTrackPreviewPane::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.setRenderHint(QPainter::Antialiasing, true);
  QSize pmSize(static_cast<double>(m_pixmap.width()) * m_scaleFactor,
               static_cast<double>(m_pixmap.height()) * m_scaleFactor);
  painter.drawPixmap(
      0, 0,
      m_pixmap.scaled(pmSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraTrackPreviewPane::setPixmap(const QPixmap& pm) {
  m_pixmap = pm;
  resize(pm.size() * m_scaleFactor);
  update();
}

void CameraTrackPreviewPane::doZoom(double d_scale) {
  m_scaleFactor += d_scale;
  if (m_scaleFactor > 1.0)
    m_scaleFactor = 1.0;
  else if (m_scaleFactor < 0.1)
    m_scaleFactor = 0.1;

  resize(m_pixmap.size() * m_scaleFactor);
  update();
}

void CameraTrackPreviewPane::fitScaleTo(QSize size) {
  double tmp_scaleFactor = std::min(
      static_cast<double>(size.width()) / static_cast<double>(m_pixmap.width()),
      static_cast<double>(size.height()) /
          static_cast<double>(m_pixmap.height()));

  m_scaleFactor = tmp_scaleFactor;
  if (m_scaleFactor > 1.0)
    m_scaleFactor = 1.0;
  else if (m_scaleFactor < 0.1)
    m_scaleFactor = 0.1;

  resize(m_pixmap.size() * m_scaleFactor);
  update();
}

//-----------------------------------------------------------------------------
void CameraTrackPreviewArea::mousePressEvent(QMouseEvent* e) {
  m_mousePos = e->pos();
}

void CameraTrackPreviewArea::mouseMoveEvent(QMouseEvent* e) {
  QPoint d = m_mousePos - e->pos();
  horizontalScrollBar()->setValue(horizontalScrollBar()->value() + d.x());
  verticalScrollBar()->setValue(verticalScrollBar()->value() + d.y());
  m_mousePos = e->pos();
}

void CameraTrackPreviewArea::contextMenuEvent(QContextMenuEvent* event) {
  QMenu* menu        = new QMenu(this);
  QAction* fitAction = menu->addAction(tr("Fit To Window"));
  connect(fitAction, &QAction::triggered, this,
          &CameraTrackPreviewArea::fitToWindow);
  menu->exec(event->globalPos());
}

void CameraTrackPreviewArea::fitToWindow() {
  dynamic_cast<CameraTrackPreviewPane*>(widget())->fitScaleTo(rect().size());
}

void CameraTrackPreviewArea::wheelEvent(QWheelEvent* event) {
  QPoint numPixels  = event->pixelDelta();
  QPoint numDegrees = event->angleDelta() / 8;

  int delta = 0;

  if (!numPixels.isNull()) {
    delta = numPixels.y();
  } else if (!numDegrees.isNull()) {
    QPoint numSteps = numDegrees / 15;
    delta           = numSteps.y();
  }

  if (delta == 0) {
    event->accept();
    return;
  }

  dynamic_cast<CameraTrackPreviewPane*>(widget())->doZoom((delta > 0) ? 0.1
                                                                      : -0.1);
  event->accept();
}

//********************************************************************************
//    ExportCameraTrackPopup  implementation
//********************************************************************************

ExportCameraTrackPopup::ExportCameraTrackPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), false, false,
                    "ExportCameraTrack") {
  setWindowTitle(tr("Export Camera Track"));

  m_previewPane = new CameraTrackPreviewPane(this);
  m_previewArea = new CameraTrackPreviewArea(this);

  m_targetColumnCombo = new QComboBox(this);
  m_bgOpacityField    = new DVGui::DoubleField(this);
  m_lineColorFld      = new DVGui::ColorField(this, false, TPixel32(255, 0, 0));

  m_cameraRectOnKeysCB   = new QCheckBox(tr("Draw On Keyframes"), this);
  m_cameraRectOnTagsCB   = new QCheckBox(tr("Draw On Navigation Tags"), this);
  m_cameraRectFramesEdit = new QLineEdit(this);

  m_lineTL_CB               = new QCheckBox(tr("Top Left"), this);
  m_lineTR_CB               = new QCheckBox(tr("Top Right"), this);
  m_lineCenter_CB           = new QCheckBox(tr("Center"), this);
  m_lineBL_CB               = new QCheckBox(tr("Bottom Left"), this);
  m_lineBR_CB               = new QCheckBox(tr("Bottom Right"), this);
  m_graduationIntervalCombo = new QComboBox(this);

  m_numberAtCombo   = new QComboBox(this);
  m_numbersOnLineCB = new QCheckBox(tr("Draw Numbers On Track Line"), this);
  m_fontCombo       = new QFontComboBox(this);
  m_fontSizeEdit    = new DVGui::IntLineEdit(this, 30, 5, 300);

  QPushButton* exportButton = new QPushButton(tr("Export"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

  //----------
  m_previewArea->setWidget(m_previewPane);
  m_previewArea->setAlignment(Qt::AlignCenter);
  m_previewArea->setBackgroundRole(QPalette::Dark);
  m_previewArea->setStyleSheet("background-color:gray;");

  m_targetColumnCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_bgOpacityField->setRange(0.0, 1.0);
  m_bgOpacityField->setValue(0.5);

  QRegularExpressionValidator* validator = new QRegularExpressionValidator(
      QRegularExpression("^(\\d+,)*\\d+$"), this);
  m_cameraRectFramesEdit->setValidator(validator);
  m_cameraRectFramesEdit->setToolTip(
      tr("Specify frame numbers where the camera rectangles will be drawn. "
         "Separate numbers by comma \",\" ."));

  m_numberAtCombo->addItem(tr("Top Left"), static_cast<int>(Qt::TopLeftCorner));
  m_numberAtCombo->addItem(tr("Top Right"),
                           static_cast<int>(Qt::TopRightCorner));
  m_numberAtCombo->addItem(tr("Bottom Left"),
                           static_cast<int>(Qt::BottomLeftCorner));
  m_numberAtCombo->addItem(tr("Bottom Right"),
                           static_cast<int>(Qt::BottomRightCorner));

  m_graduationIntervalCombo->addItem(tr("None"), 0);
  m_graduationIntervalCombo->addItem(tr("All frames"), 1);
  m_graduationIntervalCombo->addItem(tr("Every 2 frames"), 2);
  m_graduationIntervalCombo->addItem(tr("Every 3 frames"), 3);
  m_graduationIntervalCombo->addItem(tr("Every 4 frames"), 4);
  m_graduationIntervalCombo->addItem(tr("Every 5 frames"), 5);
  m_graduationIntervalCombo->addItem(tr("Every 6 frames"), 6);
  m_graduationIntervalCombo->addItem(tr("Every 8 frames"), 8);
  m_graduationIntervalCombo->addItem(tr("Every 10 frames"), 10);
  m_graduationIntervalCombo->addItem(tr("Every 12 frames"), 12);

  exportButton->setFocusPolicy(Qt::NoFocus);
  cancelButton->setFocusPolicy(Qt::NoFocus);
  //----------
  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(m_previewArea, 1);

    QVBoxLayout* rightLay = new QVBoxLayout();
    rightLay->setContentsMargins(10, 10, 10, 10);
    rightLay->setSpacing(10);

    QGridLayout* appearanceLay = new QGridLayout();
    appearanceLay->setContentsMargins(0, 0, 0, 0);
    appearanceLay->setHorizontalSpacing(5);
    appearanceLay->setVerticalSpacing(10);
    {
      appearanceLay->addWidget(new QLabel(tr("Target Column:"), this), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      appearanceLay->addWidget(m_targetColumnCombo, 0, 1, Qt::AlignLeft);

      appearanceLay->addWidget(new QLabel(tr("Background:"), this), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      appearanceLay->addWidget(m_bgOpacityField, 1, 1);

      appearanceLay->addWidget(new QLabel(tr("Line Color:"), this), 2, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      appearanceLay->addWidget(m_lineColorFld, 2, 1, Qt::AlignLeft);
    }
    appearanceLay->setColumnStretch(1, 1);
    rightLay->addLayout(appearanceLay, 0);

    QGroupBox* cameraRectGB    = new QGroupBox(tr("Camera Rectangles"), this);
    QGridLayout* cameraRectLay = new QGridLayout();
    cameraRectLay->setContentsMargins(10, 10, 10, 10);
    cameraRectLay->setHorizontalSpacing(5);
    cameraRectLay->setVerticalSpacing(10);
    {
      cameraRectLay->addWidget(m_cameraRectOnKeysCB, 0, 0, 1, 2, Qt::AlignLeft);
      cameraRectLay->addWidget(m_cameraRectOnTagsCB, 1, 0, 1, 2, Qt::AlignLeft);

      cameraRectLay->addWidget(new QLabel(tr("Specify Frames Manually:"), this),
                               2, 0, Qt::AlignRight | Qt::AlignVCenter);
      cameraRectLay->addWidget(m_cameraRectFramesEdit, 2, 1);
    }
    cameraRectLay->setColumnStretch(1, 1);
    cameraRectGB->setLayout(cameraRectLay);
    rightLay->addWidget(cameraRectGB, 0);

    QGroupBox* trackLineGB    = new QGroupBox(tr("Track Lines"), this);
    QGridLayout* trackLineLay = new QGridLayout();
    trackLineLay->setContentsMargins(10, 10, 10, 10);
    trackLineLay->setHorizontalSpacing(10);
    trackLineLay->setVerticalSpacing(10);
    {
      trackLineLay->addWidget(m_lineTL_CB, 0, 0);
      trackLineLay->addWidget(m_lineTR_CB, 0, 1);
      trackLineLay->addWidget(m_lineCenter_CB, 1, 0, Qt::AlignRight);
      trackLineLay->addWidget(m_lineBL_CB, 2, 0);
      trackLineLay->addWidget(m_lineBR_CB, 2, 1);
      trackLineLay->addWidget(
          new QLabel(tr("Graduation Marks Interval:"), this), 3, 0,
          Qt::AlignRight | Qt::AlignVCenter);
      trackLineLay->addWidget(m_graduationIntervalCombo, 3, 1, Qt::AlignLeft);
    }
    trackLineLay->setColumnStretch(1, 1);
    trackLineGB->setLayout(trackLineLay);
    rightLay->addWidget(trackLineGB, 0);

    QGroupBox* frameNumberGB    = new QGroupBox(tr("Frame Numbers"), this);
    QGridLayout* frameNumberLay = new QGridLayout();
    frameNumberLay->setContentsMargins(10, 10, 10, 10);
    frameNumberLay->setHorizontalSpacing(5);
    frameNumberLay->setVerticalSpacing(10);
    {
      frameNumberLay->addWidget(new QLabel(tr("Camera Rect Corner:"), this), 0,
                                0, Qt::AlignRight | Qt::AlignVCenter);
      frameNumberLay->addWidget(m_numberAtCombo, 0, 1, Qt::AlignLeft);

      frameNumberLay->addWidget(m_numbersOnLineCB, 1, 0, 1, 2);

      frameNumberLay->addWidget(new QLabel(tr("Font Family:"), this), 2, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
      frameNumberLay->addWidget(m_fontCombo, 2, 1, Qt::AlignLeft);

      frameNumberLay->addWidget(new QLabel(tr("Font Size:"), this), 3, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
      frameNumberLay->addWidget(m_fontSizeEdit, 3, 1, Qt::AlignLeft);
    }
    frameNumberLay->setColumnStretch(1, 1);
    frameNumberGB->setLayout(frameNumberLay);
    rightLay->addWidget(frameNumberGB, 0);

    rightLay->addStretch(1);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(5, 5, 5, 5);
    buttonsLay->setSpacing(20);
    {
      buttonsLay->addWidget(exportButton, 0);
      buttonsLay->addWidget(cancelButton, 0);
    }
    rightLay->setAlignment(Qt::AlignCenter);
    rightLay->addLayout(buttonsLay, 0);

    mainLay->addLayout(rightLay, 0);
  }
  m_topLayout->addLayout(mainLay, 1);

  //----------

  loadSettings();

  connect(m_targetColumnCombo, QOverload<int>::of(&QComboBox::activated), this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_bgOpacityField, &DVGui::DoubleField::valueEditedByHand, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_lineColorFld, &DVGui::ColorField::colorChanged, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_cameraRectOnKeysCB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_cameraRectOnTagsCB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_cameraRectFramesEdit, &QLineEdit::editingFinished, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_lineTL_CB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_lineTR_CB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_lineCenter_CB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_lineBL_CB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_lineBR_CB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_graduationIntervalCombo, QOverload<int>::of(&QComboBox::activated),
          this, &ExportCameraTrackPopup::updatePreview);

  connect(m_numberAtCombo, QOverload<int>::of(&QComboBox::activated), this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_numbersOnLineCB, &QCheckBox::clicked, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_fontCombo, &QFontComboBox::currentFontChanged, this,
          &ExportCameraTrackPopup::updatePreview);
  connect(m_fontSizeEdit, &DVGui::IntLineEdit::editingFinished, this,
          &ExportCameraTrackPopup::updatePreview);

  connect(cancelButton, &QPushButton::clicked, this, &QWidget::close);
  connect(exportButton, &QPushButton::clicked, this,
          &ExportCameraTrackPopup::onExport);
}

//--------------------------------------------------------------

// register settings to the user env file on close
void ExportCameraTrackPopup::saveSettings() {
  CameraTrackExportBgOpacity = m_bgOpacityField->getValue();
  TPixel32 col               = m_lineColorFld->getColor();
  CameraTrackExportLineColor = QColor(col.r, col.g, col.b).name().toStdString();
  CameraTrackExportCamRectOnKeys = (m_cameraRectOnKeysCB->isChecked()) ? 1 : 0;
  CameraTrackExportCamRectOnTags = (m_cameraRectOnTagsCB->isChecked()) ? 1 : 0;
  CameraTrackExportLineTL        = (m_lineTL_CB->isChecked()) ? 1 : 0;
  CameraTrackExportLineTR        = (m_lineTR_CB->isChecked()) ? 1 : 0;
  CameraTrackExportLineCenter    = (m_lineCenter_CB->isChecked()) ? 1 : 0;
  CameraTrackExportLineBL        = (m_lineBL_CB->isChecked()) ? 1 : 0;
  CameraTrackExportLineBR        = (m_lineBR_CB->isChecked()) ? 1 : 0;
  CameraTrackExportGraduationInterval =
      m_graduationIntervalCombo->currentData().toInt();
  CameraTrackExportNumberAt      = m_numberAtCombo->currentData().toInt();
  CameraTrackExportNumbersOnLine = (m_numbersOnLineCB->isChecked()) ? 1 : 0;
  CameraTrackExportFont     = m_fontCombo->currentFont().family().toStdString();
  CameraTrackExportFontSize = m_fontSizeEdit->getValue();
}

//--------------------------------------------------------------
//
// load settings from the user env file on ctor
void ExportCameraTrackPopup::loadSettings() {
  m_bgOpacityField->setValue(CameraTrackExportBgOpacity);
  QColor lineColor(QString::fromStdString(CameraTrackExportLineColor));
  m_lineColorFld->setColor(
      TPixel32(lineColor.red(), lineColor.green(), lineColor.blue()));
  m_cameraRectOnKeysCB->setChecked(CameraTrackExportCamRectOnKeys != 0);
  m_cameraRectOnTagsCB->setChecked(CameraTrackExportCamRectOnTags != 0);
  m_lineTL_CB->setChecked(CameraTrackExportLineTL != 0);
  m_lineTR_CB->setChecked(CameraTrackExportLineTR != 0);
  m_lineCenter_CB->setChecked(CameraTrackExportLineCenter != 0);
  m_lineBL_CB->setChecked(CameraTrackExportLineBL != 0);
  m_lineBR_CB->setChecked(CameraTrackExportLineBR != 0);
  m_graduationIntervalCombo->setCurrentIndex(
      m_graduationIntervalCombo->findData(
          static_cast<int>(CameraTrackExportGraduationInterval)));
  m_numberAtCombo->setCurrentIndex(
      m_numberAtCombo->findData(static_cast<int>(CameraTrackExportNumberAt)));
  m_numbersOnLineCB->setChecked(CameraTrackExportNumbersOnLine != 0);
  QString tmplFont = QString::fromStdString(CameraTrackExportFont);
  if (!tmplFont.isEmpty()) m_fontCombo->setCurrentFont(QFont(tmplFont));
  m_fontSizeEdit->setValue(CameraTrackExportFontSize);
}

//--------------------------------------------------------------

void ExportCameraTrackPopup::initialize() {
  updateTargetColumnComboItems();
  updatePreview();
}

//--------------------------------------------------------------

void ExportCameraTrackPopup::updateTargetColumnComboItems() {
  m_targetColumnCombo->clear();

  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet* xsh      = TApp::instance()->getCurrentXsheet()->getXsheet();

  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshLevelP level;
    int r0, r1;
    xsh->getCellRange(col, r0, r1);
    if (r1 < 0) continue;
    for (int r = r0; r <= r1; r++)
      if ((level = xsh->getCell(r, col).m_level)) {
        break;
      }

    if (!level) continue;

    int type = level->getType();
    if (!(type & RASTER_TYPE)) continue;

    TXshSimpleLevelP sl = level->getSimpleLevel();
    if (!sl) continue;

    QString itemName = tr("Col %1 (%2)")
                           .arg(col + 1)
                           .arg(QString::fromStdWString(sl->getName()));

    m_targetColumnCombo->addItem(itemName, col);
  }
}

//--------------------------------------------------------------

QImage ExportCameraTrackPopup::generateCameraTrackImg(
    const ExportCameraTrackInfo& info, bool isPreview) {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet* xsh      = TApp::instance()->getCurrentXsheet()->getXsheet();

  // obtain target level
  TXshLevelP level;
  TFrameId fId;
  int r0, r1;
  xsh->getCellRange(info.columnId, r0, r1);
  if (r1 < 0) return QImage();
  for (int r = r0; r <= r1; r++)
    if ((level = xsh->getCell(r, info.columnId).m_level)) {
      fId = xsh->getCell(r, info.columnId).getFrameId();
      break;
    }
  if (!level) return QImage();
  int type = level->getType();
  if (!(type & RASTER_TYPE)) return QImage();
  TXshSimpleLevelP sl = level->getSimpleLevel();
  if (!sl) return QImage();

  // construct output image
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TCamera* camera         = xsh->getStageObject(cameraId)->getCamera();
  TDimension camDim       = camera->getRes();

  TDimension imgRes = sl->getResolution();

  TImageP tImg = sl->getFullsampledFrame(fId, ImageManager::dontPutInCache);
  TRaster32P imgRas(tImg->raster()->getSize());
  TRaster32P src32   = tImg->raster();
  TRasterCM32P srcCM = tImg->raster();
  if (src32)
    imgRas = src32;
  else if (srcCM)
    TRop::convert(imgRas, srcCM, tImg->getPalette(), TRect());
  else
    TRop::convert(imgRas, tImg->raster());

  QImage colImg(imgRas->getRawData(), imgRes.lx, imgRes.ly,
                QImage::Format_ARGB32_Premultiplied);

  QImage img = colImg.mirrored(false, true);

  TPointD imgDpi = sl->getImageDpi();
  if (imgDpi != TPointD()) {
    img.setDotsPerMeterX(static_cast<int>(std::round(imgDpi.x / 0.0254)));
    img.setDotsPerMeterY(static_cast<int>(std::round(imgDpi.y / 0.0254)));
  }

  // draw

  enum CornerId {
    TopLeft     = Qt::TopLeftCorner,
    TopRight    = Qt::TopRightCorner,
    BottomLeft  = Qt::BottomLeftCorner,
    BottomRight = Qt::BottomRightCorner,
    Center      = Qt::BottomRightCorner + 1
  };

  QMap<int, QPainterPath> trackPaths;  // [ CornerId, Path data ]
  QList<QMap<int, QPointF>>
      cornerPointsTrack;  // [ CornerId, Position ] for each frame

  if (info.lineTL) trackPaths.insert(static_cast<int>(TopLeft), QPainterPath());
  if (info.lineTR)
    trackPaths.insert(static_cast<int>(TopRight), QPainterPath());
  if (info.lineCenter)
    trackPaths.insert(static_cast<int>(Center), QPainterPath());
  if (info.lineBL)
    trackPaths.insert(static_cast<int>(BottomLeft), QPainterPath());
  if (info.lineBR)
    trackPaths.insert(static_cast<int>(BottomRight), QPainterPath());

  TAffine aff;
  TAffine dpiAffInv = getDpiAffine(sl.getPointer(), fId, true).inv();
  TAffine camDpiAff = getDpiAffine(camera);

  TStageObjectId colId = TStageObjectId::ColumnId(info.columnId);

  QMap<int, TPointD> camCorners = {
      {static_cast<int>(TopLeft), TPointD(-camDim.lx / 2, camDim.ly / 2)},
      {static_cast<int>(TopRight), TPointD(camDim.lx / 2, camDim.ly / 2)},
      {static_cast<int>(BottomLeft), TPointD(-camDim.lx / 2, -camDim.ly / 2)},
      {static_cast<int>(BottomRight), TPointD(camDim.lx / 2, -camDim.ly / 2)},
      {static_cast<int>(Center), TPointD()}};

  for (int f = 0; f < scene->getFrameCount(); f++) {
    getCameraPlacement(aff, xsh, static_cast<double>(f), colId, cameraId);
    TAffine affTmp = dpiAffInv * aff * camDpiAff;
    //  corner points
    QMap<int, QPointF> cornerPoints;
    for (int c = TopLeft; c <= Center; c++) {
      TPointD p = affTmp * camCorners[c];
      cornerPoints.insert(c, QPointF(p.x, -p.y));
    }
    cornerPointsTrack.append(cornerPoints);

    if (trackPaths.isEmpty()) continue;

    // track paths will plot every 0.1 frames
    for (int df = 0; df < 10; df++) {
      double tmpF = static_cast<double>(f) + static_cast<double>(df) * 0.1;
      getCameraPlacement(aff, xsh, tmpF, colId, cameraId);
      affTmp = dpiAffInv * aff * camDpiAff;

      for (int c = TopLeft; c <= Center; c++) {
        if (!trackPaths.contains(c)) continue;

        TPointD p = affTmp * camCorners[c];
        if (f == 0 && df == 0)
          trackPaths[c].moveTo(QPointF(p.x, -p.y));
        else
          trackPaths[c].lineTo(QPointF(p.x, -p.y));
      }

      if (f == scene->getFrameCount() - 1) break;
    }
  }

  QPainter p(&img);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  p.setBrush(QColor(255, 255, 255, 255. * (1. - info.bgOpacity)));
  p.setPen(Qt::NoPen);
  p.drawRect(0, 0, imgRes.lx, imgRes.ly);
  p.translate(imgRes.lx / 2.0, imgRes.ly / 2.0);

  // camera rect
  QSet<int> rectFrames = info.cameraRectFrames;
  if (info.cameraRectOnKeys || info.cameraRectOnTags) {
    // check keyframes
    for (int f = 0; f < scene->getFrameCount(); f++) {
      if (rectFrames.contains(f)) continue;

      if (info.cameraRectOnKeys &&
          (isKey(f, xsh->getStageObject(cameraId), xsh) ||
           isKey(f, xsh->getStageObject(colId), xsh)))
        rectFrames.insert(f);

      else if (info.cameraRectOnTags && xsh->isFrameTagged(f))
        rectFrames.insert(f);
    }
  }

  struct cameraRectData {
    QPolygonF polygon;
    QVector<QPointF> centerCrossPoints;
    QPointF textPos;
    QVector2D offsetVec;
    QList<int> frames;
  };

  QList<cameraRectData>
      camRectDataList;  // gather frames with the same camera rectangle

  for (auto rectFrame : rectFrames) {
    if (rectFrame < 0 || cornerPointsTrack.size() <= rectFrame) continue;
    QMap<int, QPointF> cornerPoints = cornerPointsTrack.at(rectFrame);
    QPolygonF camRectPolygon({cornerPoints[TopLeft], cornerPoints[TopRight],
                              cornerPoints[BottomRight],
                              cornerPoints[BottomLeft]});
    QVector<QPointF> centerCrossPoints = {
        cornerPoints[TopLeft] * 0.51 + cornerPoints[BottomRight] * 0.49,
        cornerPoints[TopLeft] * 0.49 + cornerPoints[BottomRight] * 0.51,
        cornerPoints[TopRight] * 0.51 + cornerPoints[BottomLeft] * 0.49,
        cornerPoints[TopRight] * 0.49 + cornerPoints[BottomLeft] * 0.51};
    QPointF textPos = cornerPoints[static_cast<int>(info.numberAt)];
    int oppositeId;
    switch (static_cast<int>(info.numberAt)) {
    case TopLeft:
      oppositeId = TopRight;
      break;
    case TopRight:
      oppositeId = TopLeft;
      break;
    case BottomLeft:
      oppositeId = BottomRight;
      break;
    case BottomRight:
      oppositeId = BottomLeft;
      break;
    }
    QVector2D offsetVec =
        QVector2D(textPos - cornerPoints[oppositeId]).normalized();

    bool found = false;
    for (int i = 0; i < camRectDataList.size(); i++)
      if (camRectDataList[i].polygon == camRectPolygon) {
        found = true;
        camRectDataList[i].frames.append(rectFrame);
        break;
      }
    if (!found)
      camRectDataList.append(
          {camRectPolygon, centerCrossPoints, textPos, offsetVec, {rectFrame}});
  }

  p.setFont(info.font);

  for (auto& data : camRectDataList) {
    p.setPen(QPen(info.lineColor, 2));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(data.polygon);

    // draw cross mark at center of the frame
    bool drawCross = true;
    // if the graduation mark is at the camera center, do not draw the cross
    if (info.lineCenter && info.graduationInterval > 0) {
      for (auto frame : data.frames)
        if (frame % info.graduationInterval == 0) {
          drawCross = false;
          break;
        }
    }
    if (drawCross) {
      p.setPen(QPen(info.lineColor, 1));
      p.drawLines(data.centerCrossPoints);
    }

    // generate frame number string
    std::sort(data.frames.begin(), data.frames.end());
    QString frameStr = framesToString(data.frames);

    // draw frame number string
    QFontMetricsF fm(info.font);
    QRectF textRect = fm.boundingRect(frameStr).adjusted(-5, 0, 5, 0);
    QPointF textPos = data.textPos + QPointF(5, 0);
    textRect.translate(textPos);

    while (data.polygon.intersects(textRect)) {
      textRect.translate(data.offsetVec.toPointF());
      textPos += QPointF(data.offsetVec.toPointF());
    }

    p.setBrush(info.lineColor);
    drawOutlinedText(p, textPos, frameStr);
  }

  QFont smallFont(info.font);
  smallFont.setPixelSize(info.font.pixelSize() * 2 / 3);
  p.setFont(smallFont);

  // track lines
  QMap<int, QPainterPath>::const_iterator itr = trackPaths.constBegin();
  while (itr != trackPaths.constEnd()) {
    if (info.lineCenter && itr.key() != static_cast<int>(Center))
      p.setPen(QPen(info.lineColor, 1, Qt::DashLine));
    else
      p.setPen(QPen(info.lineColor, 1));
    p.setBrush(Qt::NoBrush);

    p.drawPath(itr.value());

    if (info.graduationInterval == 0 ||
        (info.lineCenter && itr.key() != static_cast<int>(Center))) {
      ++itr;
      continue;
    }

    // draw graduation
    QList<QPair<QPointF, QList<int>>> graduations;

    for (int f = 0; f < scene->getFrameCount(); f++) {
      if (f % info.graduationInterval != 0) continue;

      QPointF gPos = cornerPointsTrack[f].value(itr.key());
      QPointF prev =
          (f == 0) ? gPos : cornerPointsTrack[f - 1].value(itr.key());
      QPointF next = (f == scene->getFrameCount() - 1)
                         ? gPos
                         : cornerPointsTrack[f + 1].value(itr.key());

      QPointF graduationVec =
          QVector2D(next - prev).normalized().toPointF() * 5.0;
      graduationVec = QPointF(-graduationVec.y(), graduationVec.x());

      p.drawLine(gPos - graduationVec, gPos + graduationVec);

      // draw frame
      if (!info.numbersOnLine) continue;
      if (itr.key() != static_cast<int>(Center) && rectFrames.contains(f))
        continue;

      bool found = false;

      for (auto& g : graduations) {
        if (g.first == gPos) {
          found = true;
          g.second.append(f);
          break;
        }
      }
      if (!found) graduations.append(QPair<QPointF, QList<int>>(gPos, {f}));
    }

    for (auto& g : graduations) {
      std::sort(g.second.begin(), g.second.end());
      QString frameStr = framesToString(g.second);
      QFontMetricsF fm(smallFont);
      QRectF textRect = fm.boundingRect(frameStr).adjusted(-5, 0, 5, 0);
      QPointF pos     = g.first + QPointF(5, 0);
      if (info.numberAt == Qt::TopLeftCorner ||
          info.numberAt == Qt::BottomLeftCorner)
        pos += QPointF(-textRect.width(), 0);

      p.setBrush(info.lineColor);
      drawOutlinedText(p, pos, frameStr);
    }

    ++itr;
  }

  return img;
}

//--------------------------------------------------------------

void ExportCameraTrackPopup::getInfoFromUI(ExportCameraTrackInfo& info) {
  // target column
  if (m_targetColumnCombo->count() == 0) return;
  info.columnId = m_targetColumnCombo->currentData().toInt();
  // appearance settimgs
  info.bgOpacity    = m_bgOpacityField->getValue();
  TPixel32 lineTCol = m_lineColorFld->getColor();
  info.lineColor =
      QColor(static_cast<int>(lineTCol.r), static_cast<int>(lineTCol.g),
             static_cast<int>(lineTCol.b));
  // camera rect settings
  info.cameraRectOnKeys = m_cameraRectOnKeysCB->isChecked();
  info.cameraRectOnTags = m_cameraRectOnTagsCB->isChecked();
  QStringList framesStrList =
      m_cameraRectFramesEdit->text().split(",", Qt::SkipEmptyParts);
  for (auto fStr : framesStrList) {
    bool ok;
    int f = fStr.toInt(&ok);
    if (ok) info.cameraRectFrames.insert(f - 1);
  }
  // track line settings
  info.lineTL             = m_lineTL_CB->isChecked();
  info.lineTR             = m_lineTR_CB->isChecked();
  info.lineCenter         = m_lineCenter_CB->isChecked();
  info.lineBL             = m_lineBL_CB->isChecked();
  info.lineBR             = m_lineBR_CB->isChecked();
  info.graduationInterval = m_graduationIntervalCombo->currentData().toInt();
  // frame number settings
  info.numberAt =
      static_cast<Qt::Corner>(m_numberAtCombo->currentData().toInt());
  info.numbersOnLine = m_numbersOnLineCB->isChecked();
  info.font          = m_fontCombo->currentFont();
  info.font.setPixelSize(m_fontSizeEdit->getValue());
}

//--------------------------------------------------------------

void ExportCameraTrackPopup::updatePreview() {
  ExportCameraTrackInfo info;
  getInfoFromUI(info);
  if (info.columnId == -1) return;

  QImage img = generateCameraTrackImg(info, true);

  m_previewPane->setPixmap(QPixmap::fromImage(img));
}

//--------------------------------------------------------------

void ExportCameraTrackPopup::onExport() {
  ExportCameraTrackInfo info;
  getInfoFromUI(info);
  if (info.columnId == -1) return;

  QImage img = generateCameraTrackImg(info, false);

  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();

  static GenericSaveFilePopup* savePopup = nullptr;
  if (!savePopup) {
    savePopup =
        new GenericSaveFilePopup(QObject::tr("Export Camera Track Image"));
    savePopup->setFilterTypes({"jpg", "jpeg", "bmp", "png", "tif"});
  }

  if (!scene->isUntitled())
    savePopup->setFolder(scene->getScenePath().getParentDir());
  else
    savePopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  TXsheet* xsh            = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  std::string cameraName  = xsh->getStageObject(cameraId)->getName();
  savePopup->setFilename(TFilePath(cameraName + ".tif"));

  TFilePath fp = savePopup->getPath();
  if (fp.isEmpty()) return;

  std::string type = fp.getType();
  if (type.empty())
    fp = fp.withType("tif");
  else if (type != "jpg" && type != "jpeg" && type != "bmp" && type != "png" &&
           type != "tif") {
    DVGui::MsgBoxInPopup(DVGui::WARNING,
                         tr("Please specify one of the following file formats; "
                            "jpg, jpeg, bmp, png, and tif"));
    return;
  }

  img.save(fp.getQString());
}

//********************************************************************************
//    Export Camera Track Command  instantiation
//********************************************************************************

OpenPopupCommandHandler<ExportCameraTrackPopup> ExportCameraTrackPopupCommand(
    MI_ExportCameraTrack);

//--------------------------------------------------------------

void ExportCameraTrackPopup::showEvent(QShowEvent* event) {
  initialize();
  DVGui::Dialog::showEvent(event);
}

//--------------------------------------------------------------

void ExportCameraTrackPopup::closeEvent(QCloseEvent* event) {
  saveSettings();
  DVGui::Dialog::closeEvent(event);
}
