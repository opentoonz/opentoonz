

#include "startuppopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "mainwindow.h"
#include "tapp.h"
#include "camerasettingspopup.h"
#include "iocommand.h"
#include "toutputproperties.h"
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
  QVector<StartupLabel *> recentNamesLabels = QVector<StartupLabel *>(names.count());
  m_cameraSettings    = new CameraSettingsPopup();
  m_nameFld     = new LineEdit(this);
  //m_fromFld     = new DVGui::IntLineEdit(this);
  //m_toFld       = new DVGui::IntLineEdit(this);
  //m_stepFld     = new DVGui::IntLineEdit(this);
  //m_incFld      = new DVGui::IntLineEdit(this);
  //m_levelTypeOm = new QComboBox();

  m_pathFld     = new FileField(0);
  m_widthLabel  = new QLabel(tr("Width:"));
  m_widthFld    = new DVGui::MeasuredDoubleLineEdit(0);
  m_heightLabel = new QLabel(tr("Height:"));
  m_heightFld   = new DVGui::MeasuredDoubleLineEdit(0);
  m_fpsLabel    = new QLabel(tr("FPS:"));
  m_fpsFld      = new DoubleLineEdit(0, 66.76);

  //QPushButton *okBtn     = new QPushButton(tr("OK"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  QPushButton *applyBtn  = new QPushButton(tr("Apply"), this);

  // Exclude all character which cannot fit in a filepath (Win).
  // Dots are also prohibited since they are internally managed by Toonz.
  QRegExp rx("[^\\\\/:?*.\"<>|]+");
  m_nameFld->setValidator(new QRegExpValidator(rx, this));


  m_widthFld->setMeasure("camera.lx");
  m_heightFld->setMeasure("camera.ly");


  m_widthFld->setRange(0.1, (std::numeric_limits<double>::max)());
  m_heightFld->setRange(0.1, (std::numeric_limits<double>::max)());
  m_fpsFld->setRange(0.1, (std::numeric_limits<double>::max)());

  //okBtn->setDefault(true);

  //--- layout
  m_topLayout->setMargin(0);
  m_topLayout->setSpacing(0);
  {
    
	  QVBoxLayout *newSceneLay = new QVBoxLayout();
	  newSceneLay->setMargin(10);
	  //newSceneLay->setVerticalSpacing(10);
	  //newSceneLay->setHorizontalSpacing(5);
	  {
	  }
    QGridLayout *guiLay = new QGridLayout();
    guiLay->setMargin(10);
    guiLay->setVerticalSpacing(10);
    guiLay->setHorizontalSpacing(5);
    {
      // Name
      guiLay->addWidget(new QLabel(tr("Name:")), 0, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_nameFld, 0, 1, 1, 3);

      // Save In
      guiLay->addWidget(new QLabel(tr("Save In:")), 1, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_pathFld, 1, 1, 1, 3);

      // Width - Height
      guiLay->addWidget(m_widthLabel, 2, 0, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_widthFld, 2, 1);
      guiLay->addWidget(m_heightLabel, 2, 2, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_heightFld, 2, 3);

      // DPI
      guiLay->addWidget(m_fpsLabel, 3, 0, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_fpsFld, 3, 1, 1, 3);
	  guiLay->addWidget(new QLabel(" ", this), 4, 0, 1, 1, Qt::AlignLeft);
	  guiLay->addWidget(new QLabel(tr("Open Recent File: "), this), 0, 5, 1, 1, Qt::AlignLeft);
	  int i = 0;
	  for (QString name : names) {
		  recentNamesLabels[i] = new StartupLabel(name.section(" ", 1), this, i);
		  guiLay->addWidget(recentNamesLabels[i], i + 1, 5, 1, 1, Qt::AlignLeft);
		  i++;
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
    //m_buttonLayout->addWidget(okBtn, 0);
    m_buttonLayout->addWidget(applyBtn, 0);
    m_buttonLayout->addWidget(cancelBtn, 0);
    m_buttonLayout->addStretch(1);
  }

  //---- signal-slot connections
  bool ret = true;
  //ret      = ret &&
  //      connect(m_levelTypeOm, SIGNAL(currentIndexChanged(const QString &)),
  //              SLOT(onLevelTypeChanged(const QString &)));
  //ret = ret && connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtn()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  ret =
      ret && connect(applyBtn, SIGNAL(clicked()), this, SLOT(onApplyButton()));
  for (int i = 0; i < recentNamesLabels.count(); i++) {
	  ret = ret && connect(recentNamesLabels[i], SIGNAL(wasClicked(int)), this, SLOT(onRecentSceneClicked(int)));
  }
}

//-----------------------------------------------------------------------------

//void StartupPopup::updatePath() {
//  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
//  TFilePath defaultPath;
//  //defaultPath = scene->getDefaultLevelPath(getLevelType()).getParentDir();
//  //m_pathFld->setPath(toQString(defaultPath));
//}

//-----------------------------------------------------------------------------

void StartupPopup::showEvent(QShowEvent *) {
  update();
  m_nameFld->setFocus();
  m_pathFld->setPath(TApp::instance()->getCurrentScene()->getScene()->getProject()->getProjectFolder().getQString());
  TDimensionD size = TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->getSize();
  TPointD dpi = TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->getDpi();
  double fps = TApp::instance()->getCurrentScene()->getScene()->getProperties()->getOutputProperties()->getFrameRate();
  if (Preferences::instance()->getUnits() == "pixel") {
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
  } else {
    m_widthFld->setDecimals(4);
    m_heightFld->setDecimals(4);
  }
  m_widthFld->setValue(size.lx);
  m_heightFld->setValue(size.ly);
  m_fpsFld->setValue(fps);
}

//-----------------------------------------------------------------------------

void StartupPopup::onApplyButton() {
	TApp::instance()->getCurrentScene()->getScene()->setScenePath(TFilePath(m_pathFld->getPath()) + TFilePath(m_nameFld->text().toStdWString()));
	TDimensionD size = TDimensionD(m_widthFld->getValue(), m_heightFld->getValue());
	double fps = m_fpsFld->getValue();
	TApp::instance()->getCurrentScene()->getScene()->getProperties()->getOutputProperties()->setFrameRate(fps);
	TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->setSize(size);
	IoCmd::saveScene();
    m_nameFld->setFocus();
	hide();
}

//-----------------------------------------------------------------------------

void StartupPopup::onRecentSceneClicked(int index) {
	//int index = names.indexOf(name);
	if (index < 0) return;
	QString path =
		RecentFiles::instance()->getFilePath(index, RecentFiles::Scene);
	IoCmd::loadScene(TFilePath(path.toStdWString()), false);
	RecentFiles::instance()->moveFilePath(index, 0, RecentFiles::Scene);
	hide();
}

//-----------------------------------------------------------------------------

StartupLabel::StartupLabel(const QString& text, QWidget* parent, int index)
	: QLabel(parent), m_index(index)
{
	setText(text);
}

StartupLabel::~StartupLabel()
{
}

void StartupLabel::mousePressEvent(QMouseEvent* event)
{
	m_text = text();
	std::string strText = m_text.toStdString();
	emit wasClicked(m_index);
}

OpenPopupCommandHandler<StartupPopup> openStartupPopup(MI_StartupPopup);
