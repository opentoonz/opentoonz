

#include "startuppopup.h"

// Tnz6 includes
#include "mainwindow.h"
#include "tapp.h"
#include "iocommand.h"
#include "toutputproperties.h"
#include "toonzqt/flipconsole.h"
#include "menubarcommandids.h"
#include "tenv.h"
#include "toonz/stage.h"

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
#include <QMessageBox>
#include <QTextStream>

using namespace std;
using namespace DVGui;

namespace {

	// the first value in the preset list
	const QString custom = QObject::tr("<custom>");

	QString removeZeros(QString srcStr) {
		if (!srcStr.contains('.')) return srcStr;

		for (int i = srcStr.length() - 1; i >= 0; i--) {
			if (srcStr.at(i) == '0')
				srcStr.chop(1);
			else if (srcStr.at(i) == '.') {
				srcStr.chop(1);
				break;
			}
			else
				break;
		}
		return srcStr;
	}
}  // namespace

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
  m_dpiLabel = new QLabel(tr("DPI:"));
  m_dpiFld = new DoubleLineEdit(0, 120);
  m_resLabel        = new QLabel(this);
  m_resTextLabel	= new QLabel(tr("Resolution:"), this);
  m_fpsLabel        = new QLabel(tr("Frame Rate:"));
  m_fpsFld          = new DoubleLineEdit(0, 24.0);
  m_cameraSettingsWidget = new CameraSettingsWidget(false);
  m_presetCombo = new QComboBox();
  m_addPresetBtn = new QPushButton(tr("Add"));
  m_removePresetBtn = new QPushButton(tr("Remove"));
  m_showAtStartCB = new QCheckBox(tr("Show this window when OpenToonz starts."), this);
  m_usePixelsCB =
      new QCheckBox(tr("Use pixels as the default unit in OpenToonz."), this);
  QPushButton *closeButton  = new QPushButton(tr("Close"), this);
  QPushButton *createButton = new QPushButton(tr("Create New Scene"), this);
  QPushButton *newProjectButton = new QPushButton(tr("New Project"), this);
  QPushButton *loadOtherSceneButton =
      new QPushButton(tr("Load Other Scene"), this);
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
  m_dpiFld->setRange(1.0, (std::numeric_limits<double>::max)());
  m_usePixelsCB->setChecked(Preferences::instance()->getPixelsOnly());
  m_showAtStartCB->setChecked(Preferences::instance()->isStartupPopupEnabled());
  m_showAtStartCB->setStyleSheet("QCheckBox{ background-color: none; }");
  m_dpiFld->setMinimumWidth(100);
  m_dpiFld->setMaximumWidth(400);
  m_addPresetBtn->setStyleSheet("QPushButton { padding-left: 4px; padding-right: 4px;}");
  m_removePresetBtn->setStyleSheet("QPushButton { padding-left: 4px; padding-right: 4px;}");
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
		guiLay->addWidget(new QLabel(tr("Preset:")), 4, 0,
			Qt::AlignRight | Qt::AlignVCenter);
	  QHBoxLayout *resListLay = new QHBoxLayout();
	  resListLay->setSpacing(3);
	  resListLay->setMargin(1);
	  {
		  resListLay->addWidget(m_presetCombo, 1);
		  resListLay->addWidget(m_addPresetBtn, 0);
		  resListLay->addWidget(m_removePresetBtn, 0);
	  }
	  guiLay->addLayout(resListLay, 4, 1, 1, 3, Qt::AlignLeft);
      // Width - Height

	  guiLay->addWidget(m_fpsLabel, 5, 0, Qt::AlignRight | Qt::AlignVCenter);
	  guiLay->addWidget(m_fpsFld, 5, 1, 1, 1);
	  guiLay->addWidget(m_dpiLabel, 5, 2, Qt::AlignRight | Qt::AlignVCenter);
	  guiLay->addWidget(m_dpiFld, 5, 3, 1, 1);
	  guiLay->addWidget(m_widthLabel, 6, 0, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_widthFld, 6, 1);
      guiLay->addWidget(m_heightLabel, 6, 2, Qt::AlignRight | Qt::AlignVCenter);
      guiLay->addWidget(m_heightFld, 6, 3);
	  guiLay->addWidget(m_resTextLabel, 7, 0, 1, 1, Qt::AlignRight);
	  guiLay->addWidget(m_resLabel, 7, 1, 1, 1, Qt::AlignLeft);
	  guiLay->addWidget(m_usePixelsCB, 8, 1, 1, 3, Qt::AlignLeft);
      guiLay->addWidget(createButton, 9, 1, 1, 3, Qt::AlignLeft);
	  guiLay->addWidget(new QLabel(" ", this), 10, 0, 1, 1, Qt::AlignLeft);


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
			if (i > 6) break;
          QString justName = QString::fromStdString(TFilePath(name).getName());
          recentNamesLabels[i] = new StartupLabel(justName, this, i);
          guiLay->addWidget(recentNamesLabels[i], i + 2, 5, 1, 1,
                            Qt::AlignLeft);
          i++;
        }
      }
      guiLay->addWidget(loadOtherSceneButton, 9, 5, 1, 1,
                        Qt::AlignLeft);
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
  TApp *app                 = TApp::instance();
  TSceneHandle *sceneHandle = app->getCurrentScene();

  //---- signal-slot connections
  bool ret = true;
  ret      = ret && connect(sceneHandle, SIGNAL(sceneChanged()), this,
                       SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneChanged()));
  ret = ret && connect(newProjectButton, SIGNAL(clicked()), this,
                       SLOT(onNewProjectButtonPressed()));
  ret = ret && connect(loadOtherSceneButton, SIGNAL(clicked()), this,
                       SLOT(onLoadSceneButtonPressed()));
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
  ret = ret && connect(m_dpiFld, SIGNAL(editingFinished()), this, SLOT(onDpiChanged()));
  ret = ret && connect(m_presetCombo, SIGNAL(activated(const QString &)),
	  SLOT(onPresetSelected(const QString &)));
  ret = ret && connect(m_addPresetBtn, SIGNAL(clicked()), SLOT(addPreset()));
  
  ret = ret &&
	 connect(m_removePresetBtn, SIGNAL(clicked()), SLOT(removePreset()));
  for (int i = 0; i < recentNamesLabels.count() && i < 7; i++) {
    ret = ret && connect(recentNamesLabels[i], SIGNAL(wasClicked(int)), this,
                         SLOT(onRecentSceneClicked(int)));
  }
  assert(ret);

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
                         ->getScenesPath()
                         .getQString());
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
  m_xRes = cameraRes.lx;
  m_yRes = cameraRes.ly;
  m_resLabel->setText(QString::number(m_xRes) + " X " + QString::number(m_yRes));
  loadPresetList();
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
  if (TSystem::doesExistFileOrLevel(
          TFilePath(m_pathFld->getPath()) +
          TFilePath(m_nameFld->text().trimmed().toStdWString() + L".tnz"))) {
    QString question;
    question = QObject::tr(
        "The file name already exists."
        "\nDo you want to overwrite it?");
    int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"), 0);
    if (ret == 0 || ret == 2) {
      // no (or closed message box window)
      return;
      ;
    }
  }
  TApp::instance()->getCurrentScene()->getScene()->setScenePath(
      TFilePath(m_pathFld->getPath()) +
      TFilePath(m_nameFld->text().trimmed().toStdWString()));
  TDimensionD size =
      TDimensionD(m_widthFld->getValue(), m_heightFld->getValue());
  TDimension res = TDimension(m_xRes, m_yRes);
  double fps = m_fpsFld->getValue();
  TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getProperties()
      ->getOutputProperties()
      ->setFrameRate(fps);
  TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->setSize(
      size);
  TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->setRes(res);
  // this one is debatable - should the scene be saved right away?
  //IoCmd::saveScene();
  // this makes sure the scene viewers update to the right fps
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
                         ->getScenesPath()
                         .getQString());
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
                         ->getScenesPath()
                         .getQString());
}

//-----------------------------------------------------------------------------

void StartupPopup::loadPresetList() {
	
	m_presetCombo->clear();
	m_presetCombo->addItem("...");
	m_presetListFile = ToonzFolder::getReslistPath(false).getQString();
	QFile file(m_presetListFile);
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QTextStream in(&file);
		while (!in.atEnd()) {
			QString line = in.readLine().trimmed();
			if (line != "") m_presetCombo->addItem(line);
		}
	}
	m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void StartupPopup::savePresetList() {
	QFile file(m_presetListFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
	QTextStream out(&file);
	int n = m_presetCombo->count();
	for (int i = 1; i < n; i++) out << m_presetCombo->itemText(i) << "\n";
}

//-----------------------------------------------------------------------------

void StartupPopup::addPreset() {
	int xRes = (int)(m_widthFld->getValue() * m_dpi);
	int yRes = (int)(m_heightFld->getValue() * m_dpi);
	double lx = m_widthFld->getValue();
	double ly = m_heightFld->getValue();
	double ar = m_widthFld->getValue() / m_heightFld->getValue();

	QString presetString;
	presetString = QString::number(xRes) + "x" + QString::number(yRes) + ", " +
	removeZeros(QString::number(lx)) + "x" +
	removeZeros(QString::number(ly)) + ", " +
	aspectRatioValueToString(ar);
	

	bool ok;
	QString qs;
	while (1) {
		qs = DVGui::getText(tr("Preset name"),
			tr("Enter the name for %1").arg(presetString), "", &ok);

		if (!ok) return;

		if (qs.indexOf(",") != -1)
			QMessageBox::warning(this, tr("Error : Preset Name is Invalid"),
				tr("The preset name must not use ','(comma)."));
		else
			break;
	}

	int oldn = m_presetCombo->count();
	m_presetCombo->addItem(qs + "," + presetString);
	int newn = m_presetCombo->count();
	m_presetCombo->blockSignals(true);
	m_presetCombo->setCurrentIndex(m_presetCombo->count() - 1);
	m_presetCombo->blockSignals(false);

	savePresetList();
}

//-----------------------------------------------------------------------------

void StartupPopup::removePreset() {
	int index = m_presetCombo->currentIndex();
	if (index <= 0) return;

	// confirmation dialog
	int ret = DVGui::MsgBox(QObject::tr("Deleting \"%1\".\nAre you sure?")
		.arg(m_presetCombo->currentText()),
		QObject::tr("Delete"), QObject::tr("Cancel"));
	if (ret == 0 || ret == 2) return;

	m_presetCombo->removeItem(index);
	m_presetCombo->setCurrentIndex(0);
	savePresetList();
}

//-----------------------------------------------------------------------------

void StartupPopup::onPresetSelected(const QString &str) {
	if (str == custom || str.isEmpty()) return;
	QString name, arStr;
	int xres = 0, yres = 0;
	double fx = -1.0, fy = -1.0;
	QString xoffset = "", yoffset = "";
	double ar;

	if (parsePresetString(str, name, xres, yres, fx, fy, xoffset, yoffset, ar,
		false)) {
		m_xRes = xres;
		m_yRes = yres;
		// The current solution is to preserve the DPI so that scenes are less 
		// likely to become incompatible with pixels only mode in the future
		// Commented below is the default behavior of the camera settings widget
		//m_widthFld->setValue(m_heightFld->getValue() * ar);
		//m_dpiFld->setValue(m_xRes / m_widthFld->getValue());

		// here is the system that preserves dpi
		m_widthFld->setValue(xres / m_dpi);
		m_heightFld->setValue(yres / m_dpi);
		//_heightFld->setValue(yres / m_dpi);
		if (Preferences::instance()->getPixelsOnly()) {
			m_widthFld->setValue(xres / Stage::standardDpi);
			m_heightFld->setValue(yres / Stage::standardDpi);
			m_dpiFld->setValue(Stage::standardDpi);
		}
		m_resLabel->setText(QString::number(m_xRes) + " X " + QString::number(m_yRes));
	}
	else {
		QMessageBox::warning(this, tr("Bad camera preset"),
			tr("'%1' doesn't seem to be a well formed camera preset. \n"
				"Possibly the preset file has been corrupted")
			.arg(str));
	}
}

//--------------------------------------------------------------------------

bool StartupPopup::parsePresetString(const QString &str, QString &name,
	int &xres, int &yres, double &fx,
	double &fy, QString &xoffset,
	QString &yoffset, double &ar,
	bool forCleanup) {
	/*
	parsing preset string with QString::split().
	!NOTE! fx/fy (camera size in inch) and xoffset/yoffset (camera offset used in
	cleanup camera) are optional,
	in order to keep compatibility with default (Harlequin's) reslist.txt
	*/

	QStringList tokens = str.split(",", QString::SkipEmptyParts);

	if (!(tokens.count() == 3 ||
		(!forCleanup && tokens.count() == 4) || /*- with "fx x fy" token -*/
		(forCleanup &&
			tokens.count() ==
			6))) /*- with "fx x fy", xoffset and yoffset tokens -*/
		return false;
	/*- name -*/
	name = tokens[0];

	/*- xres, yres  (like:  1024x768) -*/
	QStringList values = tokens[1].split("x");
	if (values.count() != 2) return false;
	bool ok;
	xres = values[0].toInt(&ok);
	if (!ok) return false;
	yres = values[1].toInt(&ok);
	if (!ok) return false;

	if (tokens.count() >= 4) {
		/*- fx, fy -*/
		values = tokens[2].split("x");
		if (values.count() != 2) return false;
		fx = values[0].toDouble(&ok);
		if (!ok) return false;
		fy = values[1].toDouble(&ok);
		if (!ok) return false;

		/*- xoffset, yoffset -*/
		if (forCleanup) {
			xoffset = tokens[3];
			yoffset = tokens[4];
			/*- remove single space -*/
			if (xoffset.startsWith(' ')) xoffset.remove(0, 1);
			if (yoffset.startsWith(' ')) yoffset.remove(0, 1);
		}
	}

	/*- AR -*/
	ar = aspectRatioStringToValue(tokens.last());

	return true;
}

//-----------------------------------------------------------------------------

double StartupPopup::aspectRatioStringToValue(const QString &s) {
	if (s == "") {
		return 1;
	}
	int i = s.indexOf("/");
	if (i <= 0 || i + 1 >= s.length()) return s.toDouble();
	int num = s.left(i).toInt();
	int den = s.mid(i + 1).toInt();
	if (den <= 0) den = 1;
	return (double)num / (double)den;
}

//-----------------------------------------------------------------------------

// A/R : value => string (e.g. '4/3' or '1.23')
QString StartupPopup::aspectRatioValueToString(double value, int width,
	int height) {
	double v = value;

	if (width != 0 && height != 0) {
		if (areAlmostEqual(value, (double)width / (double)height,
			1e-3)) 
			return QString("%1/%2").arg(width).arg(height);
	}

	double iv = tround(v);
	if (fabs(iv - v) > 0.01) {
		for (int d = 2; d < 20; d++) {
			int n = tround(v * d);
			if (fabs(n - v * d) <= 0.01)
				return QString::number(n) + "/" + QString::number(d);
		}
		return QString::number(value, 'f', 5);
	}
	else {
		return QString::number((int)iv);
	}
}

//-----------------------------------------------------------------------------

void StartupPopup::onNewProjectButtonPressed() {
  CommandManager::instance()->execute(MI_NewProject);
}

//-----------------------------------------------------------------------------

void StartupPopup::onSceneChanged() {
	// close the box if a recent scene has been selected
  if (!TApp::instance()->getCurrentScene()->getScene()->isUntitled()) {
    hide();
  } else {
    updateProjectCB();
  }
}

//-----------------------------------------------------------------------------

void StartupPopup::onDpiChanged() {
	m_dpi = m_dpiFld->getValue();
	updateResolution();
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
	m_widthFld->setMeasure("camera.lx");
	m_heightFld->setMeasure("camera.ly");
	m_widthFld->setValue(width);
	m_heightFld->setValue(height);
  } else {
    Preferences::instance()->setPixelsOnly(true);
    pref->setUnits("pixel");
    pref->setCameraUnits("pixel");
	m_widthFld->setDecimals(0);
	m_heightFld->setDecimals(0);
	m_resLabel->hide();
	m_resTextLabel->hide();
	m_dpiFld->setValue(Stage::standardDpi);
	m_widthFld->setMeasure("camera.lx");
	m_heightFld->setMeasure("camera.ly");
	m_widthFld->setValue(m_xRes / Stage::standardDpi);
	m_heightFld->setValue(m_yRes / Stage::standardDpi);
  }
  
}

//-----------------------------------------------------------------------------

void StartupPopup::onShowAtStartChanged(int index) {
  Preferences::instance()->enableStartupPopup(index);
}

//-----------------------------------------------------------------------------

void StartupPopup::updateResolution() {
	if (Preferences::instance()->getPixelsOnly()) {
		if (m_dpiFld->getValue() != Stage::standardDpi) {
			m_dpiFld->setValue(Stage::standardDpi);
		}
		m_xRes = m_widthFld->getValue() * Stage::standardDpi;
		m_yRes = m_heightFld->getValue() * Stage::standardDpi;
		m_resLabel->setText(QString::number(m_xRes) + " X " + QString::number(m_yRes));
	}
	else {
		m_xRes = m_widthFld->getValue() * m_dpi;
		m_yRes = m_heightFld->getValue() * m_dpi;
		m_resLabel->setText(QString::number(m_xRes) + " X " + QString::number(m_yRes));
	}
	m_presetCombo->setCurrentIndex(0);
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

OpenPopupCommandHandler<StartupPopup> openStartupPopup(MI_StartupPopup);
