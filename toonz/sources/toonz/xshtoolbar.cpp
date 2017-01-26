

#include "xshtoolbar.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txshnoteset.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheethandle.h"


// Qt includes
#include <QVariant>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QMainWindow>
#include <QButtonGroup>
#include <QComboBox>

namespace {

QString computeBackgroundStyleSheetString(QColor color) {
  QVariant vR(color.red());
  QVariant vG(color.green());
  QVariant vB(color.blue());
  QVariant vA(color.alpha());
  return QString("#noteTextEdit { border-image: 0; background: rgbm(") +
         vR.toString() + QString(",") + vG.toString() + QString(",") +
         vB.toString() + QString(",") + vA.toString() + QString("); }");
}
}

//=============================================================================

namespace XsheetGUI {

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
Toolbar::Toolbar(XsheetViewer *parent, Qt::WindowFlags flags)
#else
Toolbar::Toolbar(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QFrame(parent), m_viewer(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("cornerWidget");
  m_toolbar = new DVGui::ToolBar();
  m_toolbar->setFixedHeight(20);
  m_toolbar->setObjectName("XSheetToolbar");
  m_newVectorLevelButton   = new QToolButton(this);
  m_newVectorLevelButton->setFixedSize(20, 20);
  m_newVectorLevelButton->setIconSize(QSize(18, 18));
  QIcon newVectorIcon = createQIconPNG("new_vector_level");
  m_newVectorLevelButton->setIcon(newVectorIcon);
  m_newVectorLevelButton->setObjectName("XSheetToolbarToolButton");
  m_newVectorLevelButton->setToolTip(tr("New Vector Level"));
  //m_newVectorLevelButton->setFixedSize(18, 18);
  m_newToonzRasterLevelButton = new QToolButton(this);
  m_newToonzRasterLevelButton->setFixedSize(20, 20);
  m_newToonzRasterLevelButton->setIconSize(QSize(18, 18));
  QIcon newToonzRasterIcon = createQIconPNG("new_toonz_raster_level");
  m_newToonzRasterLevelButton->setIcon(newToonzRasterIcon);
  m_newToonzRasterLevelButton->setObjectName("XSheetToolbarToolButton");
  m_newToonzRasterLevelButton->setToolTip(tr("New Toonz Raster Level"));
  //m_newToonzRasterLevelButton->setFixedSize(18, 18);
  m_newRasterLevelButton = new QToolButton(this);
  m_newRasterLevelButton->setFixedSize(20, 20);
  m_newRasterLevelButton->setIconSize(QSize(18, 18));
  QIcon newRasterIcon = createQIconPNG("new_raster_level");
  m_newRasterLevelButton->setIcon(newRasterIcon);
  m_newRasterLevelButton->setObjectName("XSheetToolbarToolButton");
  m_newRasterLevelButton->setToolTip(tr("New Raster Level"));
  //m_newRasterLevelButton->setFixedSize(18, 18);
  m_reframe1sButton = new QPushButton(tr("1's"), this);
  m_reframe1sButton->setToolTip(tr("Reframe on 1's"));
  //m_reframe1sButton->setFixedSize(26, 18);
  m_reframe1sButton->setFixedHeight(20);
  m_reframe1sButton->setObjectName("XSheetToolbarButton");
  m_reframe2sButton = new QPushButton(tr("2's"), this);
  m_reframe2sButton->setToolTip(tr("Reframe on 2's"));
  //m_reframe2sButton->setFixedSize(26, 18);
  m_reframe2sButton->setFixedHeight(20);
  m_reframe2sButton->setObjectName("XSheetToolbarButton");
  m_reframe3sButton = new QPushButton(tr("3's"), this);
  m_reframe3sButton->setToolTip(tr("Reframe on 3's"));
  //m_reframe3sButton->setFixedSize(26, 18);
  m_reframe3sButton->setFixedHeight(20);
  m_reframe3sButton->setObjectName("XSheetToolbarButton");
 
  
  //m_repeatButton = new QPushButton(tr("Repeat"), this);
  m_repeatButton = new QToolButton(this);
  m_repeatButton->setFixedSize(20, 20);
  m_repeatButton->setIconSize(QSize(18, 18));
  QIcon repeatIcon = createQIconPNG("repeat_icon");
  m_repeatButton->setIcon(repeatIcon);
  m_repeatButton->setObjectName("XSheetToolbarToolButton");
  m_repeatButton->setToolTip(tr("Repeat Selection"));
  //m_repeatButton->setFixedSize(18, 18);

  TApp *app = TApp::instance();
  m_keyFrameButton = new ViewerKeyframeNavigator(this, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  //QLabel *newLevelLabel = new QLabel(tr("New: "));
  //QLabel *reframeLabel = new QLabel(tr("Reframe: "));
  // layout
  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(5);
  {
    mainLay->addStretch(1);
    QHBoxLayout *newLevelLayout = new QHBoxLayout();
    newLevelLayout->setSpacing(2);
    newLevelLayout->setMargin(0);
    {
	  //newLevelLayout->addWidget(newLevelLabel, 0, Qt::AlignLeft);
		m_toolbar->addWidget(m_newVectorLevelButton);
		m_toolbar->addWidget(m_newToonzRasterLevelButton);
		m_toolbar->addWidget(m_newRasterLevelButton);
		m_toolbar->addSeparator();
		newLevelLayout->addWidget(m_toolbar, 0, Qt::AlignLeft);
	  newLevelLayout->addSpacing(10);
	  //newLevelLayout->addWidget(reframeLabel, 0, Qt::AlignLeft);
	  newLevelLayout->addWidget(m_reframe1sButton, 0, Qt::AlignLeft);
	  newLevelLayout->addWidget(m_reframe2sButton, 0, Qt::AlignLeft);
	  newLevelLayout->addWidget(m_reframe3sButton, 0, Qt::AlignLeft);
	  newLevelLayout->addSpacing(10);
	  newLevelLayout->addWidget(m_repeatButton, 0, Qt::AlignLeft);
	  newLevelLayout->addSpacing(10);
	  newLevelLayout->addWidget(m_keyFrameButton, 0, Qt::AlignLeft);
	  newLevelLayout->addStretch(0);
    }
    mainLay->addLayout(newLevelLayout, 0);
    if (!Preferences::instance()->isShowNewLevelButtonsEnabled()) {
      m_newVectorLevelButton->hide();
      m_newToonzRasterLevelButton->hide();
      m_newRasterLevelButton->hide();
	  //newLevelLabel->hide();
	  //reframeLabel->hide();
	  m_reframe1sButton->hide();
	  m_reframe2sButton->hide();
	  m_reframe3sButton->hide();
	  m_repeatButton->hide();
    }

    mainLay->addStretch(1);
  }
  setLayout(mainLay);

  // signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_newVectorLevelButton, SIGNAL(released()), this,
                       SLOT(onNewVectorLevelButtonPressed()));
  ret = ret && connect(m_newToonzRasterLevelButton, SIGNAL(released()), this,
                       SLOT(onNewToonzRasterLevelButtonPressed()));
  ret = ret && connect(m_newRasterLevelButton, SIGNAL(released()), this,
                       SLOT(onNewRasterLevelButtonPressed()));
  ret = ret && connect(m_reframe1sButton, SIGNAL(released()), this,
	  SLOT(onReframe1sButtonPressed()));
  ret = ret && connect(m_reframe2sButton, SIGNAL(released()), this,
	  SLOT(onReframe2sButtonPressed()));
  ret = ret && connect(m_reframe3sButton, SIGNAL(released()), this,
	  SLOT(onReframe3sButtonPressed()));
  ret = ret && connect(m_repeatButton, SIGNAL(released()), this,
	  SLOT(onRepeatButtonPressed()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void Toolbar::onNewVectorLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(PLI_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void Toolbar::onNewToonzRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  // Preferences::instance()->setOldDefLevelType(defaultLevelType);
  Preferences::instance()->setDefLevelType(TZP_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void Toolbar::onNewRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(OVL_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void Toolbar::onReframe1sButtonPressed() {
	CommandManager::instance()->execute("MI_Reframe1");
}

//-----------------------------------------------------------------------------

void Toolbar::onReframe2sButtonPressed() {
	CommandManager::instance()->execute("MI_Reframe2");
}

//-----------------------------------------------------------------------------

void Toolbar::onReframe3sButtonPressed() {
	CommandManager::instance()->execute("MI_Reframe3");
}

//-----------------------------------------------------------------------------


void Toolbar::onRepeatButtonPressed() {
	CommandManager::instance()->execute("MI_Dup");
}

//-----------------------------------------------------------------------------

}  // namespace XsheetGUI;
