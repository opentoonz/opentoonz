

#include "startuppopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "mainwindow.h"
#include "tapp.h"
#include "camerasettingspopup.h"
#include "iocommand.h"
#include "toutputproperties.h"
#include "toonzqt/flipconsole.h"
// TnzTools includes
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/preferences.h"
#include "toonz/palettecontroller.h"
#include "toonz/tproject.h"
#include "toonz/namebuilder.h"

// TnzCore includes
#include "tsystem.h"
#include "tpalette.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "timagecache.h"
#include "tundo.h"
#include "filebrowsermodel.h"

// Qt includes
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QMainWindow>
#include <QApplication>
#include <QDesktopWidget>

using namespace DVGui;

//=============================================================================
/*! \class StartupPopup
                \brief The StartupPopup class provides a modal dialog to
   bring up recent files or create a new scene.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

StartupPopup::StartupPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "StartupPopup") {
  setWindowTitle(tr("OpenToonz Startup"));
  names = RecentFiles::instance()->getFilesNameList(RecentFiles::Scene);
  int namesCount = names.count();
  QVector<StartupLabel *> recentNamesLabels =
      QVector<StartupLabel *>(names.count());
  m_nameFld         = new LineEdit(this);
  m_pathFld         = new FileField(0);
  m_widthLabel      = new QLabel(tr("Width:"));
  m_widthFld        = new DVGui::MeasuredDoubleLineEdit(0);
  m_heightLabel     = new QLabel(tr("Height:"));
  m_heightFld       = new DVGui::MeasuredDoubleLineEdit(0);
  m_fpsLabel        = new QLabel(tr("FPS:"));
  m_fpsFld          = new DoubleLineEdit(0, 66.76);
  m_dontShowAgainCB = new QCheckBox(tr("Don't show this again."), this);
  m_usePixelsCB =
      new QCheckBox(tr("Use pixels as the default unit in OpenToonz."), this);
  QPushButton *closeButton  = new QPushButton(tr("Close"), this);
  QPushButton *createButton = new QPushButton(tr("Create New Scene"), this);

  // Exclude all character which cannot fit in a filepath (Win).
  // Dots are also prohibited since they are internally managed by Toonz.
  QRegExp rx("[^\\\\/:?*.\"<>|]+");
  m_nameFld->setValidator(new QRegExpValidator(rx, this));

  m_widthFld->setMeasure("camera.lx");
  m_heightFld->setMeasure("camera.ly");

  m_widthFld->setRange(0.1, (std::numeric_limits<double>::max)());
  m_heightFld->setRange(0.1, (std::numeric_limits<double>::max)());
  m_fpsFld->setRange(1.0, (std::numeric_limits<double>::max)());
  m_usePixelsCB->setChecked(Preferences::instance()->getPixelsOnly());
  // okBtn->setDefault(true);
  QLabel *label = new QLabel();
  label->setPixmap(QPixmap(":Resources/startup.png"));
  //--- layout
  m_topLayout->setMargin(0);
  m_topLayout->setSpacing(0);
  {
    QGridLayout *guiLay = new QGridLayout();
    guiLay->setMargin(10);
    guiLay->setVerticalSpacing(10);
    guiLay->setHorizontalSpacing(5);
    {
      // Name
      guiLay->addWidget(label, 0, 0, 1, 6, Qt::AlignCenter);
      guiLay->addWidget(new QLabel(tr("Name:")), 1, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_nameFld, 1, 1, 1, 3);

      // Save In
      guiLay->addWidget(new QLabel(tr("Save In:")), 2, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_pathFld, 2, 1, 1, 3);

      // Width - Height
      guiLay->addWidget(m_widthLabel, 3, 0, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_widthFld, 3, 1);
      guiLay->addWidget(m_heightLabel, 3, 2, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_heightFld, 3, 3);
      guiLay->addWidget(m_fpsLabel, 4, 0, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_fpsFld, 4, 1, 1, 3);
      guiLay->addWidget(m_usePixelsCB, 5, 1, 1, 3, Qt::AlignLeft);
      guiLay->addWidget(createButton, 6, 0, 1, 4, Qt::AlignCenter);
      // put the don't show again checkbox at the bottom left
      guiLay->addWidget(m_dontShowAgainCB,
                        names.count() < 9 ? 9 : names.count() + 1, 0, 1, 4,
                        Qt::AlignLeft);
      // Recent Scene List
      guiLay->addWidget(new QLabel(" ", this), 4, 0, 1, 1, Qt::AlignLeft);
      guiLay->addWidget(new QLabel(tr("Open Recent File: "), this), 1, 5, 1, 1,
                        Qt::AlignLeft);
      if (names.count() <= 0) {
        guiLay->addWidget(new QLabel(tr("No Recent Scenes"), this), 2, 5, 1, 1,
                          Qt::AlignLeft);
      } else {
        int i = 0;
        for (QString name : names) {
          recentNamesLabels[i] =
              new StartupLabel(name.section(" ", 1), this, i);
          guiLay->addWidget(recentNamesLabels[i], i + 2, 5, 1, 1,
                            Qt::AlignLeft);
          i++;
        }
      }
    }
    guiLay->setColumnStretch(0, 0);
    guiLay->setColumnStretch(1, 0);
    guiLay->setColumnStretch(2, 0);
    guiLay->setColumnStretch(3, 0);
    guiLay->setColumnStretch(4, 1);
    guiLay->setColumnMinimumWidth(4, 20);

    m_topLayout->addLayout(guiLay, 1);
  }

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(30);
  {
    m_buttonLayout->addStretch(1);
    m_buttonLayout->addWidget(closeButton, 0);
    m_buttonLayout->addStretch(1);
  }

  //---- signal-slot connections
  bool ret = true;
  ret = ret && connect(closeButton, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret &&
        connect(createButton, SIGNAL(clicked()), this, SLOT(onCreateButton()));
  ret = ret && connect(m_usePixelsCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUsePixelsChanged(int)));
  ret = ret && connect(m_dontShowAgainCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onDontShowAgainChanged(int)));
  for (int i = 0; i < recentNamesLabels.count(); i++) {
    ret = ret && connect(recentNamesLabels[i], SIGNAL(wasClicked(int)), this,
                         SLOT(onRecentSceneClicked(int)));
  }

  this->move(QApplication::desktop()->screen()->rect().center() -
             this->rect().center());
}

//-----------------------------------------------------------------------------

void StartupPopup::showEvent(QShowEvent *) {
  update();
  m_nameFld->setFocus();
  m_pathFld->setPath(TApp::instance()
                         ->getCurrentScene()
                         ->getScene()
                         ->getProject()
                         ->getProjectFolder()
                         .getQString());
  TDimensionD cameraSize = TApp::instance()
                               ->getCurrentScene()
                               ->getScene()
                               ->getCurrentCamera()
                               ->getSize();
  TPointD dpi = TApp::instance()
                    ->getCurrentScene()
                    ->getScene()
                    ->getCurrentCamera()
                    ->getDpi();
  double fps = TApp::instance()
                   ->getCurrentScene()
                   ->getScene()
                   ->getProperties()
                   ->getOutputProperties()
                   ->getFrameRate();
  if (Preferences::instance()->getUnits() == "pixel") {
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
  } else {
    m_widthFld->setDecimals(4);
    m_heightFld->setDecimals(4);
  }
  m_widthFld->setValue(cameraSize.lx);
  m_heightFld->setValue(cameraSize.ly);
  m_fpsFld->setValue(fps);
}

//-----------------------------------------------------------------------------

void StartupPopup::onCreateButton() {
  if (m_nameFld->text().trimmed() == "") {
    DVGui::warning(tr("The name cannot be empty."));
    m_nameFld->setFocus();
    return;
  }
  if (!TSystem::doesExistFileOrLevel(TFilePath(m_pathFld->getPath()))) {
    DVGui::warning(tr("The chosen file path is not valid."));
    m_pathFld->setFocus();
    return;
  }

  if (m_widthFld->getValue() < 1) {
    DVGui::warning(tr("The width must be 1 or more."));
    m_widthFld->setFocus();
    return;
  }
  if (m_heightFld->getValue() < 1) {
    DVGui::warning(tr("The height must be 1 or more."));
    m_heightFld->setFocus();
    return;
  }
  if (m_fpsFld->getValue() < 1) {
    DVGui::warning(tr("The frame rate must be 1 or more."));
    m_fpsFld->setFocus();
    return;
  }
  TApp::instance()->getCurrentScene()->getScene()->setScenePath(
      TFilePath(m_pathFld->getPath()) +
      TFilePath(m_nameFld->text().toStdWString()));
  TDimensionD size =
      TDimensionD(m_widthFld->getValue(), m_heightFld->getValue());
  double fps = m_fpsFld->getValue();
  TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getProperties()
      ->getOutputProperties()
      ->setFrameRate(fps);
  TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->setSize(
      size);
  IoCmd::saveScene();
  TApp::instance()->getCurrentScene()->notifySceneSwitched();

  hide();
}

//-----------------------------------------------------------------------------

void StartupPopup::onRecentSceneClicked(int index) {
  if (index < 0) return;
  QString path =
      RecentFiles::instance()->getFilePath(index, RecentFiles::Scene);
  IoCmd::loadScene(TFilePath(path.toStdWString()), false);
  RecentFiles::instance()->moveFilePath(index, 0, RecentFiles::Scene);
  RecentFiles::instance()->refreshRecentFilesMenu(RecentFiles::Scene);
  hide();
}

//-----------------------------------------------------------------------------

void StartupPopup::onUsePixelsChanged(int index) {
  Preferences *pref = Preferences::instance();
  double width      = m_widthFld->getValue();
  double height     = m_heightFld->getValue();
  if (index <= 0) {
    pref->setPixelsOnly(false);
  } else {
    Preferences::instance()->setPixelsOnly(true);
    pref->setUnits("pixel");
    pref->setCameraUnits("pixel");
  }
  m_widthFld->setMeasure("camera.lx");
  m_heightFld->setMeasure("camera.ly");
  m_widthFld->setValue(width);
  m_heightFld->setValue(height);
}

//-----------------------------------------------------------------------------

void StartupPopup::onDontShowAgainChanged(int index) {
  Preferences::instance()->enableStartupPopup(index);
}

//-----------------------------------------------------------------------------

StartupLabel::StartupLabel(const QString &text, QWidget *parent, int index)
    : QLabel(parent), m_index(index) {
  setText(text);
}

StartupLabel::~StartupLabel() {}

void StartupLabel::mousePressEvent(QMouseEvent *event) {
  m_text              = text();
  std::string strText = m_text.toStdString();
  emit wasClicked(m_index);
}
