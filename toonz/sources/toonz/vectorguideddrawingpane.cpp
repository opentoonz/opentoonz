#include "vectorguideddrawingpane.h"
#include "menubarcommandids.h"
#include "tapp.h"
#include "sceneviewer.h"

#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"

#include "toonzqt/menubarcommand.h"

#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

using namespace DVGui;

//=============================================================================

#if QT_VERSION >= 0x050500
VectorGuidedDrawingPane::VectorGuidedDrawingPane(QWidget *parent,
                                                 Qt::WindowFlags flags)
#else
VectorGuidedDrawingPane::VectorGuidedDrawingPane(QWidget *parent,
                                                 Qt::WindowFlags flags)
#endif
    : QFrame(parent) {

  setObjectName("cornerWidget");
  setObjectName("VectorGuidedDrawingToolbar");

  m_guidedTypeCB = new QComboBox();
  QStringList inputs;
  inputs << tr("Off") << tr("Closest") << tr("Farthest") << tr("All");
  m_guidedTypeCB->addItems(inputs);
  int guidedIndex = Preferences::instance()->getGuidedDrawingType();

  m_guidedTypeCB->setCurrentIndex(guidedIndex);

  m_autoInbetween = new QCheckBox(tr("Auto Inbetween"), this);
  m_autoInbetween->setChecked(
      Preferences::instance()->getGuidedAutoInbetween());
  connect(m_autoInbetween, SIGNAL(stateChanged(int)), this,
          SLOT(onAutoInbetweenChanged()));

  m_interpolationTypeCB = new QComboBox();
  inputs.clear();
  inputs << tr("Linear") << tr("Ease In") << tr("Ease Out") << tr("EaseIn/Out");
  m_interpolationTypeCB->addItems(inputs);
  int interpolationIndex =
      Preferences::instance()->getGuidedInterpolation() - 1;
  if (interpolationIndex < 0) {
    // Was set off, force to Linear
    Preferences::instance()->setValue(guidedInterpolationType, 1);
    interpolationIndex = 0;
  }
  m_interpolationTypeCB->setCurrentIndex(interpolationIndex);

  QAction *action;
  m_selectPrevGuideBtn = new QPushButton(tr("Previous"), this);
  action = CommandManager::instance()->getAction(MI_SelectPrevGuideStroke);
  m_selectPrevGuideBtn->addAction(action);
  connect(m_selectPrevGuideBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_selectNextGuideBtn = new QPushButton(tr("Next"), this);
  action = CommandManager::instance()->getAction(MI_SelectNextGuideStroke);
  m_selectNextGuideBtn->addAction(action);
  connect(m_selectNextGuideBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_selectBothGuideBtn = new QPushButton(tr("Both"), this);
  action = CommandManager::instance()->getAction(MI_SelectBothGuideStrokes);
  m_selectBothGuideBtn->addAction(action);
  connect(m_selectBothGuideBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_resetGuidesBtn = new QPushButton(tr("Reset"), this);
  action = CommandManager::instance()->getAction(MI_SelectGuideStrokeReset);
  m_resetGuidesBtn->addAction(action);
  connect(m_resetGuidesBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_tweenSelectedGuidesBtn =
      new QPushButton(tr("Tween Selected Guide Strokes"), this);
  action = CommandManager::instance()->getAction(MI_TweenGuideStrokes);
  m_tweenSelectedGuidesBtn->addAction(action);
  connect(m_tweenSelectedGuidesBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_tweenToSelectedStrokeBtn =
      new QPushButton(tr("Tween Guide Strokes to Selected"), this);
  action = CommandManager::instance()->getAction(MI_TweenGuideStrokeToSelected);
  m_tweenToSelectedStrokeBtn->addAction(action);
  connect(m_tweenToSelectedStrokeBtn, SIGNAL(clicked()), action,
          SLOT(trigger()));

  m_SelectAndTweenBtn =
      new QPushButton(tr("Select Guide Strokes && Tween Mode"), this);
  action = CommandManager::instance()->getAction(MI_SelectGuidesAndTweenMode);
  m_SelectAndTweenBtn->addAction(action);
  connect(m_SelectAndTweenBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_FlipNextDirectionBtn = new QPushButton(tr("Next"), this);
  action = CommandManager::instance()->getAction(MI_FlipNextGuideStroke);
  m_FlipNextDirectionBtn->addAction(action);
  connect(m_FlipNextDirectionBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_FlipPrevDirectionBtn = new QPushButton(tr("Previous"), this);
  action = CommandManager::instance()->getAction(MI_FlipPrevGuideStroke);
  m_FlipPrevDirectionBtn->addAction(action);
  connect(m_FlipPrevDirectionBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  QGridLayout *mainlayout = new QGridLayout();
  mainlayout->setMargin(5);
  mainlayout->setSpacing(2);
  {
    QLabel *guideFrameLabel = new QLabel(this);
    guideFrameLabel->setText(tr("Guide Frames:"));
    mainlayout->addWidget(guideFrameLabel, 0, 0, Qt::AlignRight);
    mainlayout->addWidget(m_guidedTypeCB, 0, 1);

    QLabel *selectGuideStrokeLabel = new QLabel(this);
    selectGuideStrokeLabel->setText(tr("Select Guide Stroke:"));
    mainlayout->addWidget(selectGuideStrokeLabel, 1, 0, Qt::AlignRight);
    QHBoxLayout *selectBtnLayout = new QHBoxLayout();
    selectBtnLayout->setMargin(0);
    selectBtnLayout->setSpacing(2);
    {
      selectBtnLayout->addWidget(m_selectPrevGuideBtn, 0);
      selectBtnLayout->addWidget(m_selectNextGuideBtn, 0);
      selectBtnLayout->addWidget(m_selectBothGuideBtn, 0);
      selectBtnLayout->addWidget(m_resetGuidesBtn, 0);
    }
    mainlayout->addLayout(selectBtnLayout, 1, 1);

    QLabel *flipGuideStrokeLabel = new QLabel(this);
    flipGuideStrokeLabel->setText(tr("Flip Guide Stroke:"));
    mainlayout->addWidget(flipGuideStrokeLabel, 2, 0, Qt::AlignRight);
    QHBoxLayout *flipBtnLayout = new QHBoxLayout();
    flipBtnLayout->setMargin(0);
    flipBtnLayout->setSpacing(2);
    {
      flipBtnLayout->addWidget(m_FlipPrevDirectionBtn, 0);
      flipBtnLayout->addWidget(m_FlipNextDirectionBtn, 0);
    }
    mainlayout->addLayout(flipBtnLayout, 2, 1);

    mainlayout->addWidget(new DVGui::Separator("", this, true), 3, 0, 1, 2);

    mainlayout->addWidget(m_autoInbetween, 4, 1);

    QLabel *interpolationLabel = new QLabel(this);
    interpolationLabel->setText(tr("Interpolation:"));
    mainlayout->addWidget(interpolationLabel, 5, 0, Qt::AlignRight);
    mainlayout->addWidget(m_interpolationTypeCB, 5, 1);

    mainlayout->addWidget(new DVGui::Separator("", this, true), 6, 0, 1, 2);

    mainlayout->addWidget(m_tweenSelectedGuidesBtn, 7, 0, 1, 2);
    mainlayout->addWidget(m_tweenToSelectedStrokeBtn, 8, 0, 1, 2);
    mainlayout->addWidget(m_SelectAndTweenBtn, 9, 0, 1, 2);
  }

  setLayout(mainlayout);

  connect(m_guidedTypeCB, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onGuidedTypeChanged()));
  connect(m_interpolationTypeCB, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onInterpolationTypeChanged()));
  connect(TApp::instance()->getCurrentScene(),
          SIGNAL(preferenceChanged(const QString &)), this,
          SLOT(onPreferenceChanged(const QString &)));

  updateStatus();
}

void VectorGuidedDrawingPane::updateStatus() {
  int guidedType = m_guidedTypeCB->currentIndex();
  if (guidedType == 0 || guidedType == 3) {  // Off or All
    m_selectPrevGuideBtn->setDisabled(true);
    m_selectNextGuideBtn->setDisabled(true);
    m_selectBothGuideBtn->setDisabled(true);
    m_autoInbetween->setDisabled(true);
    m_interpolationTypeCB->setDisabled(true);
    m_tweenSelectedGuidesBtn->setDisabled(true);
    m_tweenToSelectedStrokeBtn->setDisabled(true);
    m_SelectAndTweenBtn->setDisabled(true);
    m_FlipNextDirectionBtn->setDisabled(true);
    m_FlipPrevDirectionBtn->setDisabled(true);
  } else {  // Closest/Farthest
    m_selectPrevGuideBtn->setDisabled(false);
    m_selectNextGuideBtn->setDisabled(false);
    m_selectBothGuideBtn->setDisabled(false);
    m_autoInbetween->setDisabled(false);
    m_interpolationTypeCB->setDisabled(false);
    m_tweenSelectedGuidesBtn->setDisabled(false);
    m_tweenToSelectedStrokeBtn->setDisabled(false);
    m_SelectAndTweenBtn->setDisabled(false);
    m_FlipNextDirectionBtn->setDisabled(false);
    m_FlipPrevDirectionBtn->setDisabled(false);
  }
}

void VectorGuidedDrawingPane::onGuidedTypeChanged() {
  int guidedIndex = m_guidedTypeCB->currentIndex();
  // 0 == Off 1 = closest, 2 = farthest, 3 = all
  Preferences::instance()->setValue(guidedDrawingType, guidedIndex);

  QAction *guidedDrawingAction =
      CommandManager::instance()->getAction(MI_VectorGuidedDrawing);
  if (guidedDrawingAction)
    guidedDrawingAction->setChecked(
        Preferences::instance()->isGuidedDrawingEnabled());

  TApp::instance()->getActiveViewer()->update();
  updateStatus();
}

void VectorGuidedDrawingPane::onAutoInbetweenChanged() {
  Preferences::instance()->setValue(guidedAutoInbetween,
                                    m_autoInbetween->isChecked());
}

void VectorGuidedDrawingPane::onInterpolationTypeChanged() {
  int interpolationIndex = m_interpolationTypeCB->currentIndex() + 1;
  // 1 = Linear, 2 = Ease In, 3 = Ease Out, 4 = Ease In/Out
  Preferences::instance()->setValue(guidedInterpolationType,
                                    interpolationIndex);
}

//----------------------------------------------------------------------------

void VectorGuidedDrawingPane::onPreferenceChanged(const QString &propertyName) {
  if (propertyName.isEmpty()) return;

  if (propertyName == "GuidedDrawingFrame")
    m_guidedTypeCB->setCurrentIndex(
        Preferences::instance()->getGuidedDrawingType());
  else if (propertyName == "GuidedDrawingAutoInbetween")
    m_autoInbetween->setChecked(
        Preferences::instance()->getGuidedAutoInbetween());
  else if (propertyName == "GuidedDrawingInterpolation")
    m_interpolationTypeCB->setCurrentIndex(
        Preferences::instance()->getGuidedInterpolation() - 1);
  else
    return;

  updateStatus();
}
