

#include "startuppopup.h"

// Tnz6 includes
#include "mainwindow.h"
#include "tapp.h"
#include "iocommand.h"
#include "toutputproperties.h"
#include "toonzqt/flipconsole.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/levelproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/preferences.h"
#include "toonz/tproject.h"

// TnzCore includes
#include "tsystem.h"
#include "filebrowsermodel.h"

// Qt includes
#include <QHBoxLayout>
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
  m_resLabel        = new QLabel(this);
  m_resTextLabel	= new QLabel(tr("Resolution:"), this);
  m_fpsLabel        = new QLabel(tr("Frame Rate:"));
  m_fpsFld          = new DoubleLineEdit(0, 66.76);
  m_showAtStartCB = new QCheckBox(tr("Show this window when OpenToonz starts."), this);
  m_usePixelsCB =
      new QCheckBox(tr("Use pixels as the default unit in OpenToonz."), this);
  QPushButton *closeButton  = new QPushButton(tr("Close"), this);
  QPushButton *createButton = new QPushButton(tr("Create New Scene"), this);
  QPushButton *newProjectButton = new QPushButton(tr("New Project"), this);
  QPushButton *loadOtherSceneButton = new QPushButton(tr("Load Other Scene"), this);
  m_projectsCB = new QComboBox(this);
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
  m_showAtStartCB->setChecked(Preferences::instance()->isStartupPopupEnabled());
  m_showAtStartCB->setStyleSheet("QCheckBox{ background-color: none; }");
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
	  guiLay->addWidget(new QLabel(tr("Project:")), 1, 0,
		  Qt::AlignRight | Qt::AlignVCenter);
	  guiLay->addWidget(m_projectsCB, 1, 1, 1, 2, Qt::AlignLeft);
	  guiLay->addWidget(newProjectButton, 1, 3, Qt::AlignRight);
      guiLay->addWidget(new QLabel(tr("Scene Name:")), 2, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_nameFld, 2, 1, 1, 3);

      // Save In
      guiLay->addWidget(new QLabel(tr("Save In:")), 3, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_pathFld, 3, 1, 1, 3);

      // Width - Height
	  guiLay->addWidget(m_fpsLabel, 4, 0, Qt::AlignRight | Qt::AlignVCenter);
	  guiLay->addWidget(m_fpsFld, 4, 1, 1, 3);
	  guiLay->addWidget(m_widthLabel, 5, 0, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_widthFld, 5, 1);
      guiLay->addWidget(m_heightLabel, 5, 2, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_heightFld, 5, 3);
	  guiLay->addWidget(m_resTextLabel, 6, 0, 1, 1, Qt::AlignRight);
	  guiLay->addWidget(m_resLabel, 6, 1, 1, 3, Qt::AlignLeft);
	  guiLay->addWidget(m_usePixelsCB, 7, 1, 1, 3, Qt::AlignLeft);
      guiLay->addWidget(createButton, 8, 1, 1, 3, Qt::AlignLeft);
	  guiLay->addWidget(new QLabel(" ", this), 9, 0, 1, 1, Qt::AlignLeft);
	  //guiLay->addWidget(new QLabel(" ", this), 10, 0, 1, 1, Qt::AlignLeft);
	  // put the don't show again checkbox at the bottom left
      //guiLay->addWidget(m_showAtStartCB,
      //                  names.count() < 11 ? 11 : names.count() + 2, 0, 1, 4,
      //                  Qt::AlignLeft);

      // Recent Scene List
      guiLay->addWidget(new QLabel(" ", this), 4, 0, 1, 1, Qt::AlignLeft);
      guiLay->addWidget(new QLabel(tr("Open Recent Scene: "), this), 1, 5, 1, 1,
                        Qt::AlignLeft);
      if (names.count() <= 0) {
        guiLay->addWidget(new QLabel(tr("No Recent Scenes"), this), 2, 5, 1, 1,
                          Qt::AlignLeft);
      } else {
        int i = 0;
        for (QString name : names) {
			QString justName = QString::fromStdString(TFilePath(name).getName());
            recentNamesLabels[i] =
            new StartupLabel(justName, this, i);
            guiLay->addWidget(recentNamesLabels[i], i + 2, 5, 1, 1,
                            Qt::AlignLeft);
          i++;
        }
      }
	  guiLay->addWidget(loadOtherSceneButton, names.count() + 2, 5, 1, 1, Qt::AlignLeft);
    }
    guiLay->setColumnStretch(0, 0);
    guiLay->setColumnStretch(1, 0);
    guiLay->setColumnStretch(2, 0);
    guiLay->setColumnStretch(3, 0);
    guiLay->setColumnStretch(4, 1);
    guiLay->setColumnMinimumWidth(4, 20);
	guiLay->setColumnMinimumWidth(5, 200);

    m_topLayout->addLayout(guiLay, 1);
  }

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(30);
  {
    
	  m_buttonLayout->addWidget(m_showAtStartCB, Qt::AlignLeft);
	  m_buttonLayout->addStretch(1);
    m_buttonLayout->addWidget(closeButton, 0);
    //m_buttonLayout->addStretch(1);
  }

  updateProjectCB();
  TApp *app = TApp::instance();
  TSceneHandle *sceneHandle = app->getCurrentScene();

  //---- signal-slot connections
  bool ret = true;
  ret = ret &&connect(sceneHandle, SIGNAL(sceneChanged()), this,
	  SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
	  SLOT(onSceneChanged()));
  ret = ret && connect(newProjectButton, SIGNAL(clicked()), this, SLOT(onNewProjectButtonPressed()));
  ret = ret && connect(loadOtherSceneButton, SIGNAL(clicked()), this, SLOT(onLoadSceneButtonPressed()));
  ret = ret && connect(m_projectsCB, SIGNAL(currentIndexChanged(int)),
	  SLOT(onProjectChanged(int)));
  ret = ret && connect(closeButton, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret &&
        connect(createButton, SIGNAL(clicked()), this, SLOT(onCreateButton()));
  ret = ret && connect(m_usePixelsCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUsePixelsChanged(int)));
  ret = ret && connect(m_showAtStartCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onShowAtStartChanged(int)));
  ret = ret && connect(m_widthFld, SIGNAL(valueChanged()), this, SLOT(updateResolution()));
  ret = ret && connect(m_heightFld, SIGNAL(valueChanged()), this, SLOT(updateResolution()));
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
	  ->getScenesPath().getQString());
  TDimensionD cameraSize = TApp::instance()
                               ->getCurrentScene()
                               ->getScene()
                               ->getCurrentCamera()
                               ->getSize();
  TDimension cameraRes = TApp::instance()
	  ->getCurrentScene()
	  ->getScene()
	  ->getCurrentCamera()
	  ->getRes();
  double fps = TApp::instance()
                   ->getCurrentScene()
                   ->getScene()
                   ->getProperties()
                   ->getOutputProperties()
                   ->getFrameRate();
  if (Preferences::instance()->getCameraUnits() == "pixel") {
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
	m_resLabel->hide();
	m_resTextLabel->hide();
  } else {
    m_widthFld->setDecimals(4);
    m_heightFld->setDecimals(4);
	m_resLabel->show();
	m_resTextLabel->show();
  }
  m_widthFld->setValue(cameraSize.lx);
  m_heightFld->setValue(cameraSize.ly);
  m_fpsFld->setValue(fps);

  m_dpi = cameraRes.lx / cameraSize.lx;

  m_resLabel->setText(QString::number(cameraSize.lx * m_dpi) + " X " + QString::number(cameraSize.ly * m_dpi));

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
  if (TSystem::doesExistFileOrLevel(TFilePath(m_pathFld->getPath()) +
	  TFilePath(m_nameFld->text().trimmed().toStdWString() + L".tnz"))) {
	  QString question;
	  question = QObject::tr("The file name already exists."
		  "\nDo you want to overwrite it?");
	  int ret = DVGui::MsgBox(question, QObject::tr("Yes"),
		  QObject::tr("No"), 0);
	  if (ret == 0 || ret == 2) {
		  // no (or closed message box window)
		  return;;
	  }
  }
  TApp::instance()->getCurrentScene()->getScene()->setScenePath(
      TFilePath(m_pathFld->getPath()) +
      TFilePath(m_nameFld->text().trimmed().toStdWString()));
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

void StartupPopup::updateProjectCB() {
	m_updating = true;
	m_projectPaths.clear();
	m_projectsCB->clear();

	TFilePath sandboxFp = TProjectManager::instance()->getSandboxProjectFolder() +
		"sandbox_otprj.xml";
	m_projectPaths.push_back(sandboxFp);
	m_projectsCB->addItem("sandbox");

	std::vector<TFilePath> prjRoots;
	TProjectManager::instance()->getProjectRoots(prjRoots);
	for (int i = 0; i < prjRoots.size(); i++) {
		TFilePathSet fps;
		TSystem::readDirectory_Dir_ReadExe(fps, prjRoots[i]);

		TFilePathSet::iterator it;
		for (it = fps.begin(); it != fps.end(); ++it) {
			TFilePath fp(*it);
			if (TProjectManager::instance()->isProject(fp)) {
				m_projectPaths.push_back(
					TProjectManager::instance()->projectFolderToProjectPath(fp));
				m_projectsCB->addItem(QString::fromStdString(fp.getName()));
			}
		}
	}
	int i;
	for (i = 0; i < m_projectPaths.size(); i++) {
		if (TProjectManager::instance()->getCurrentProjectPath() ==
			m_projectPaths[i]) {
			m_projectsCB->setCurrentIndex(i);
			break;
		}
	}
	m_pathFld->setPath(TApp::instance()
		->getCurrentScene()
		->getScene()
		->getProject()
		->getScenesPath().getQString());
	m_updating = false;
}

//-----------------------------------------------------------------------------

void StartupPopup::onProjectChanged(int index) {
	if (m_updating) return;
	TFilePath projectFp = m_projectPaths[index];

	TProjectManager::instance()->setCurrentProjectPath(projectFp);
	
	IoCmd::newScene();
	m_pathFld->setPath(TApp::instance()
		->getCurrentScene()
		->getScene()
		->getProject()
		->getScenesPath().getQString());
}

//-----------------------------------------------------------------------------

void StartupPopup::onNewProjectButtonPressed() {
	CommandManager::instance()->execute(MI_NewProject);
}

//-----------------------------------------------------------------------------

void StartupPopup::onSceneChanged() {
	if (!TApp::instance()->getCurrentScene()->getScene()->isUntitled()) { hide(); }
	else {
		updateProjectCB();
	}
}

//-----------------------------------------------------------------------------

void StartupPopup::onLoadSceneButtonPressed() {
	CommandManager::instance()->execute(MI_LoadScene);
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
	m_widthFld->setDecimals(4);
	m_heightFld->setDecimals(4);
	m_resLabel->show();
	m_resTextLabel->show();
  } else {
    Preferences::instance()->setPixelsOnly(true);
    pref->setUnits("pixel");
    pref->setCameraUnits("pixel");
	m_widthFld->setDecimals(0);
	m_heightFld->setDecimals(0);
	m_resLabel->hide();
	m_resTextLabel->hide();
  }
  m_widthFld->setMeasure("camera.lx");
  m_heightFld->setMeasure("camera.ly");
  m_widthFld->setValue(width);
  m_heightFld->setValue(height);
}

//-----------------------------------------------------------------------------

void StartupPopup::onShowAtStartChanged(int index) {
  Preferences::instance()->enableStartupPopup(index);
}

//-----------------------------------------------------------------------------

void StartupPopup::updateResolution() {
	m_resLabel->setText(QString::number(m_widthFld->getValue() * m_dpi) + " X " + QString::number(m_heightFld->getValue() * m_dpi));
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
