

#include "tools/tooloptions.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

#include "selectiontool.h"
#include "vectorselectiontool.h"
#include "rasterselectiontool.h"
#include "brushtool.h"
#include "fullcolorbrushtool.h"
#include "tooloptionscontrols.h"

//#include "rgbpickertool.h"
#include "rulertool.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvscrollwidget.h"
// iwsw commented out temporarily
//#include "toonzqt/ghibli_3dlut_converter.h"

// TnzLib includes
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobject.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tframehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/preferences.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/mypaintbrushstyle.h"

// TnzCore includes
#include "tproperty.h"
#include "tenv.h"

// Qt includes
#include <QPainter>
#include <QToolBar>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QResizeEvent>
#include <QList>
#include <QSignalMapper>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QStackedWidget>

// tcg includes
#include "tcg/tcg_deleter_types.h"

TEnv::IntVar ArrowGlobalKeyFrame("EditToolGlobalKeyFrame", 0);

//=============================================================================
// ToolOptionToolBar

ToolOptionToolBar::ToolOptionToolBar(QWidget *parent) : QToolBar(parent) {
  setObjectName("toolOptionsPanel");
  setMaximumHeight(25);
}

//-----------------------------------------------------------------------------

void ToolOptionToolBar::addSpacing(int width) {
  QWidget *spaceW = new QWidget(this);
  spaceW->setFixedWidth(width);
  addWidget(spaceW);
}

//=============================================================================
// ToolOptionsBox

ToolOptionsBox::ToolOptionsBox(QWidget *parent) : QFrame(parent) {
  setObjectName("toolOptionsPanel");
  setStyleSheet("#toolOptionsPanel {border: 0px; margin: 1px;}");

  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_layout = new QHBoxLayout;
  m_layout->setMargin(0);
  m_layout->setSpacing(5);
  m_layout->addSpacing(5);
  setLayout(m_layout);
}

//-----------------------------------------------------------------------------

ToolOptionsBox::~ToolOptionsBox() {
  std::for_each(m_controls.begin(), m_controls.end(),
                tcg::deleter<ToolOptionControl>());
  std::for_each(m_labels.begin(), m_labels.end(), tcg::deleter<QLabel>());
}

//-----------------------------------------------------------------------------

QLabel *ToolOptionsBox::addLabel(QString name) {
  QLabel *label = new QLabel(name);
  label->setFixedHeight(20);
  m_layout->addWidget(label);
  m_labels[name.toStdString()] = label;
  return label;
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::addLabel(std::string propName, QLabel *label) {
  m_labels[propName] = label;
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::addSeparator() {
  DVGui::Separator *separator = new DVGui::Separator();
  separator->setOrientation(false);
  separator->setFixedWidth(17);
  m_layout->addWidget(separator, 0);
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::addControl(ToolOptionControl *control) {
  m_controls[control->propertyName()] = control;
}

//-----------------------------------------------------------------------------

ToolOptionControl *ToolOptionsBox::control(
    const std::string &controlName) const {
  QMap<std::string, ToolOptionControl *>::const_iterator ct =
      m_controls.find(controlName);

  return (ct == m_controls.end()) ? 0 : ct.value();
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//=============================================================================
// ToolOptionControlBuilder

ToolOptionControlBuilder::ToolOptionControlBuilder(ToolOptionsBox *panel,
                                                   TTool *tool,
                                                   TPaletteHandle *pltHandle,
                                                   ToolHandle *toolHandle)
    : m_panel(panel)
    , m_tool(tool)
    , m_pltHandle(pltHandle)
    , m_toolHandle(toolHandle)
    , m_singleValueWidgetType(SLIDER)
    , m_enumWidgetType(COMBOBOX) {}

//-----------------------------------------------------------------------------

QLabel *ToolOptionControlBuilder::addLabel(TProperty *p) {
  QLabel *label = new QLabel(p->getQStringName());
  hLayout()->addWidget(label, 0);
  return label;
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TDoubleProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ToolOptionSlider *control = new ToolOptionSlider(m_tool, p, m_toolHandle);
  hLayout()->addWidget(control, 100);
  // storing the control in the map for updating values later
  m_panel->addControl(control);
  CommandManager *cm = CommandManager::instance();
  if (p->getName() == "Size:") {
    QAction *a;
    a = cm->getAction("A_IncreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increase()));
    a = cm->getAction("A_DecreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decrease()));
  }
  if (p->getName() == "Hardness:") {
    QAction *a;
    a = cm->getAction("A_IncreaseBrushHardness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increase()));
    a = cm->getAction("A_DecreaseBrushHardness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decrease()));
  }
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TDoublePairProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ToolOptionPairSlider *control = new ToolOptionPairSlider(
      m_tool, p, QObject::tr("Min:"), QObject::tr("Max:"), m_toolHandle);
  hLayout()->addWidget(control, 150);
  m_panel->addControl(control);
  if (p->getName() == "Size:" || p->getName() == "Size") {
    CommandManager *cm = CommandManager::instance();
    QAction *a;
    a = cm->getAction("A_IncreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increaseMaxValue()));
    a = cm->getAction("A_DecreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decreaseMaxValue()));

    a = cm->getAction("A_IncreaseMinBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increaseMinValue()));
    a = cm->getAction("A_DecreaseMinBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decreaseMinValue()));
  }
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TIntPairProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ToolOptionIntPairSlider *control = new ToolOptionIntPairSlider(
      m_tool, p, QObject::tr("Min:"), QObject::tr("Max:"), m_toolHandle);
  hLayout()->addWidget(control, 100);
  m_panel->addControl(control);
  if (p->getName() == "Size:" || p->getName() == "Thickness" ||
      p->getName() == "Size") {
    CommandManager *cm = CommandManager::instance();
    QAction *a;
    a = cm->getAction("A_IncreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increaseMaxValue()));
    a = cm->getAction("A_DecreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decreaseMaxValue()));

    a = cm->getAction("A_IncreaseMinBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increaseMinValue()));
    a = cm->getAction("A_DecreaseMinBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decreaseMinValue()));
  }
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TIntProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);

  ToolOptionIntSlider *control =
      new ToolOptionIntSlider(m_tool, p, m_toolHandle);

  if (m_singleValueWidgetType == FIELD) {
    control->enableSlider(false);
    control->setFixedWidth(45);
  }
  hLayout()->addWidget(control, 100);
  m_panel->addControl(control);
  if (p->getName() == "Size:") {
    CommandManager *cm = CommandManager::instance();
    QAction *a;
    a = cm->getAction("A_IncreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(increase()));
    a = cm->getAction("A_DecreaseMaxBrushThickness");
    control->addAction(a);
    QObject::connect(a, SIGNAL(triggered()), control, SLOT(decrease()));
  }
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TBoolProperty *p) {
  ToolOptionCheckbox *control = new ToolOptionCheckbox(m_tool, p, m_toolHandle);
  hLayout()->addWidget(control, 0);

  m_panel->addControl(control);

  if (p->getId() != "") {
    std::string actionName = "A_ToolOption_" + p->getId();
    QAction *a = CommandManager::instance()->getAction(actionName.c_str());
    if (a) {
      control->addAction(a);
      QObject::connect(a, SIGNAL(triggered()), control, SLOT(doClick()));
    }
  }
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TStringProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);

  ToolOptionTextField *control = new ToolOptionTextField(m_tool, p);
  m_panel->addControl(control);

  hLayout()->addWidget(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TEnumProperty *p) {
  QWidget *widget;
  ToolOptionControl *control;
  switch (m_enumWidgetType) {
  case POPUPBUTTON: {
    ToolOptionPopupButton *obj = new ToolOptionPopupButton(m_tool, p);
    obj->setToolTip(p->getQStringName());
    control = obj;
    widget  = obj;
    break;
  }

  case COMBOBOX:
  default: {
    if (p->getQStringName() != "") {
      QLabel *label = addLabel(p);
      m_panel->addLabel(p->getName(), label);
    }
    ToolOptionCombo *obj = new ToolOptionCombo(m_tool, p, m_toolHandle);
    control              = obj;
    widget               = obj;
    break;
  }
  }

  hLayout()->addWidget(widget, 100);
  m_panel->addControl(control);
  hLayout()->addSpacing(5);

  if (p->getId() != "") {
    std::string actionName = "A_ToolOption_" + p->getId();
    QAction *a = CommandManager::instance()->getAction(actionName.c_str());
    if (a) {
      widget->addAction(a);
      QObject::connect(a, SIGNAL(triggered()), widget, SLOT(doShowPopup()));
    }

    TEnumProperty::Range range = p->getRange();
    TEnumProperty::Range::iterator it;
    QSignalMapper *signalMapper = 0;
    int index                   = 0;
    for (it = range.begin(); it != range.end(); ++it, ++index) {
      std::string item           = ::to_string(*it);
      std::string itemActionName = actionName + ":" + item;
      a = CommandManager::instance()->getAction(itemActionName.c_str());
      if (a) {
        widget->addAction(a);
        if (signalMapper == 0) {
          signalMapper = new QSignalMapper(widget);
          QObject::connect(signalMapper, SIGNAL(mapped(int)), widget,
                           SLOT(doOnActivated(int)));
        }
        QObject::connect(a, SIGNAL(triggered()), signalMapper, SLOT(map()));
        signalMapper->setMapping(a, index);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TStyleIndexProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  StyleIndexFieldAndChip *chip =
      new StyleIndexFieldAndChip(m_tool, p, m_pltHandle, m_toolHandle);
  hLayout()->addWidget(chip, 0);
  m_panel->addControl(chip);
}

//----------------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TPointerProperty *p) {
  assert(!"not implemented");
}

//=============================================================================
// GenericToolOptionsBox

GenericToolOptionsBox::GenericToolOptionsBox(QWidget *parent, TTool *tool,
                                             TPaletteHandle *pltHandle,
                                             int propertyGroupIdx,
                                             ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  setObjectName("toolOptionsPanel");

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(propertyGroupIdx))
    tool->getProperties(propertyGroupIdx)->accept(builder);

  m_layout->addStretch(1);
}

//-----------------------------------------------------------------------------

ArrowToolOptionsBox::ArrowToolOptionsBox(
    QWidget *parent, TTool *tool, TPropertyGroup *pg, TFrameHandle *frameHandle,
    TObjectHandle *objHandle, TXsheetHandle *xshHandle, ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_pg(pg)
    , m_splined(false)
    , m_tool(tool)
    , m_frameHandle(frameHandle)
    , m_objHandle(objHandle)
    , m_xshHandle(xshHandle) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("toolOptionsPanel");
  setFixedHeight(26);

  m_mainStackedWidget = new QStackedWidget(this);

  /* --- General Parts --- */

  // enable to choose target pegbar with combobox
  m_currentStageObjectCombo = new QComboBox(this);

  TEnumProperty *activeAxisProp =
      dynamic_cast<TEnumProperty *>(m_pg->getProperty("Active Axis"));
  if (activeAxisProp)
    m_chooseActiveAxisCombo =
        new ToolOptionCombo(m_tool, activeAxisProp, toolHandle);
  TEnumProperty *autoSelectProp =
      dynamic_cast<TEnumProperty *>(m_pg->getProperty("Auto Select Column"));
  if (autoSelectProp)
    m_pickCombo = new ToolOptionCombo(m_tool, autoSelectProp, toolHandle);

  /* --- Position --- */
  m_motionPathPosField =
      new PegbarChannelField(m_tool, TStageObject::T_Path, "field", frameHandle,
                             objHandle, xshHandle, this);
  m_ewPosField =
      new PegbarChannelField(m_tool, TStageObject::T_X, "field", frameHandle,
                             objHandle, xshHandle, this);
  m_nsPosField =
      new PegbarChannelField(m_tool, TStageObject::T_Y, "field", frameHandle,
                             objHandle, xshHandle, this);
  m_zField = new PegbarChannelField(m_tool, TStageObject::T_Z, "field",
                                    frameHandle, objHandle, xshHandle, this);
  m_noScaleZField = new NoScaleField(m_tool, "field");
  m_ewPosLabel    = new QLabel(tr("E/W:"), this);
  m_nsPosLabel    = new QLabel(tr("N/S:"), this);
  // Lock E/W
  TBoolProperty *lockProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Position E/W"));
  if (lockProp)
    m_lockEWPosCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock N/S
  lockProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Position N/S"));
  if (lockProp)
    m_lockNSPosCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // stacking order
  m_soLabel = new QLabel(tr("SO:"), this);
  m_soField = new PegbarChannelField(m_tool, TStageObject::T_SO, "field",
                                     frameHandle, objHandle, xshHandle, this);

  /* --- Rotation --- */
  m_rotationField =
      new PegbarChannelField(m_tool, TStageObject::T_Angle, "field",
                             frameHandle, objHandle, xshHandle, this);

  /* --- Scale --- */
  m_globalScaleField =
      new PegbarChannelField(m_tool, TStageObject::T_Scale, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_scaleHField =
      new PegbarChannelField(m_tool, TStageObject::T_ScaleX, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_scaleVField =
      new PegbarChannelField(m_tool, TStageObject::T_ScaleY, "field",
                             frameHandle, objHandle, xshHandle, this);
  TEnumProperty *scaleConstraintProp =
      dynamic_cast<TEnumProperty *>(m_pg->getProperty("Scale Constraint:"));
  if (scaleConstraintProp)
    m_maintainCombo =
        new ToolOptionCombo(m_tool, scaleConstraintProp, toolHandle);
  // Lock Scale H
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Scale H"));
  if (lockProp)
    m_lockScaleHCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock Scale V
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Scale V"));
  if (lockProp)
    m_lockScaleVCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);

  /* --- Shear --- */
  m_shearHField =
      new PegbarChannelField(m_tool, TStageObject::T_ShearX, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_shearVField =
      new PegbarChannelField(m_tool, TStageObject::T_ShearY, "field",
                             frameHandle, objHandle, xshHandle, this);
  // Lock Shear H
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Shear H"));
  if (lockProp)
    m_lockShearHCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock Scale V
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Shear V"));
  if (lockProp)
    m_lockShearVCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);

  /* --- Center Position --- */
  m_ewCenterField =
      new PegbarCenterField(m_tool, 0, "field", objHandle, xshHandle, this);
  m_nsCenterField =
      new PegbarCenterField(m_tool, 1, "field", objHandle, xshHandle, this);
  // Lock E/W Center
  lockProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Center E/W"));
  if (lockProp)
    m_lockEWCenterCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock N/S Center
  lockProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Center N/S"));
  if (lockProp)
    m_lockNSCenterCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);

  TBoolProperty *prop =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Global Key"));
  if (prop)
    m_globalKey = new ToolOptionCheckbox(m_tool, prop, toolHandle, this);

  m_lockEWPosCheckbox->setObjectName("EditToolLockButton");
  m_lockNSPosCheckbox->setObjectName("EditToolLockButton");
  m_lockEWCenterCheckbox->setObjectName("EditToolLockButton");
  m_lockNSCenterCheckbox->setObjectName("EditToolLockButton");
  m_lockScaleHCheckbox->setObjectName("EditToolLockButton");
  m_lockScaleVCheckbox->setObjectName("EditToolLockButton");
  m_lockShearHCheckbox->setObjectName("EditToolLockButton");
  m_lockShearVCheckbox->setObjectName("EditToolLockButton");
  m_lockEWPosCheckbox->setText("");
  m_lockNSPosCheckbox->setText("");
  m_lockEWCenterCheckbox->setText("");
  m_lockNSCenterCheckbox->setText("");
  m_lockScaleHCheckbox->setText("");
  m_lockScaleVCheckbox->setText("");
  m_lockShearHCheckbox->setText("");
  m_lockShearVCheckbox->setText("");

  m_zField->setPrecision(4);
  m_noScaleZField->setPrecision(4);

  bool splined                        = isCurrentObjectSplined();
  if (splined != m_splined) m_splined = splined;
  setSplined(m_splined);

  const int ITEM_SPACING  = 10;
  const int LABEL_SPACING = 3;
  /* --- Layout --- */
  /* --- Layout --- */
  QHBoxLayout *mainLay = m_layout;
  {
    mainLay->addWidget(m_currentStageObjectCombo, 0);
    mainLay->addWidget(m_chooseActiveAxisCombo, 0);

    addSeparator();

    mainLay->addWidget(m_mainStackedWidget, 1);
    {
      // Position
      QFrame *posFrame    = new QFrame(this);
      QHBoxLayout *posLay = new QHBoxLayout();
      posLay->setMargin(0);
      posLay->setSpacing(0);
      posFrame->setLayout(posLay);
      m_mainStackedWidget->addWidget(posFrame);
      {
        posLay->addWidget(new QLabel(tr("Position"), this), 0);
        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(m_motionPathPosField, 0);

        posLay->addWidget(m_ewPosLabel, 0);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(m_ewPosField, 10);
        posLay->addWidget(m_lockEWPosCheckbox, 0);

        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(m_nsPosLabel, 0);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(m_nsPosField, 10);
        posLay->addWidget(m_lockNSPosCheckbox, 0);

        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(new QLabel(tr("Z:"), this), 0);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(m_zField, 10);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(new QLabel(tr("("), this), 0);
        posLay->addWidget(m_noScaleZField, 10);
        posLay->addWidget(new QLabel(tr(")"), this), 0);

        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(m_soLabel, 0);
        posLay->addWidget(m_soField, 10);

        posLay->addStretch(1);
      }

      // Rotation
      QFrame *rotFrame    = new QFrame(this);
      QHBoxLayout *rotLay = new QHBoxLayout();
      rotLay->setMargin(0);
      rotLay->setSpacing(0);
      rotFrame->setLayout(rotLay);
      m_mainStackedWidget->addWidget(rotFrame);
      {
        rotLay->addWidget(new QLabel(tr("Rotation"), this), 0);
        rotLay->addSpacing(ITEM_SPACING);

        rotLay->addWidget(m_rotationField, 10);

        rotLay->addStretch(1);
      }

      // Scale
      QFrame *scaleFrame    = new QFrame(this);
      QHBoxLayout *scaleLay = new QHBoxLayout();
      scaleLay->setMargin(0);
      scaleLay->setSpacing(0);
      scaleFrame->setLayout(scaleLay);
      m_mainStackedWidget->addWidget(scaleFrame);
      {
        scaleLay->addWidget(new QLabel(tr("Scale"), this), 0);
        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(new QLabel(tr("Global:"), this), 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_globalScaleField, 10);

        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(new QLabel(tr("H:"), this), 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_scaleHField, 10);
        scaleLay->addWidget(m_lockScaleHCheckbox, 0);

        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(new QLabel(tr("V:"), this), 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_scaleVField, 10);
        scaleLay->addWidget(m_lockScaleVCheckbox, 0);

        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(new QLabel(tr("Maintain:"), this), 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_maintainCombo, 0);

        scaleLay->addStretch(1);
      }

      // Shear
      QFrame *shearFrame    = new QFrame(this);
      QHBoxLayout *shearLay = new QHBoxLayout();
      shearLay->setMargin(0);
      shearLay->setSpacing(0);
      shearFrame->setLayout(shearLay);
      m_mainStackedWidget->addWidget(shearFrame);
      {
        shearLay->addWidget(new QLabel(tr("Shear"), this), 0);
        shearLay->addSpacing(ITEM_SPACING);

        shearLay->addWidget(new QLabel(tr("H:"), this), 0);
        shearLay->addSpacing(LABEL_SPACING);
        shearLay->addWidget(m_shearHField, 10);
        shearLay->addWidget(m_lockShearHCheckbox, 0);

        shearLay->addSpacing(ITEM_SPACING);

        shearLay->addWidget(new QLabel(tr("V:"), this), 0);
        shearLay->addSpacing(LABEL_SPACING);
        shearLay->addWidget(m_shearVField, 10);
        shearLay->addWidget(m_lockShearVCheckbox, 0);

        shearLay->addSpacing(ITEM_SPACING);

        shearLay->addStretch(1);
      }

      // Center Position
      QFrame *centerPosFrame    = new QFrame(this);
      QHBoxLayout *centerPosLay = new QHBoxLayout();
      centerPosLay->setMargin(0);
      centerPosLay->setSpacing(0);
      centerPosFrame->setLayout(centerPosLay);
      m_mainStackedWidget->addWidget(centerPosFrame);
      {
        centerPosLay->addWidget(new QLabel(tr("Center Position"), this), 0);
        centerPosLay->addSpacing(ITEM_SPACING);

        centerPosLay->addWidget(new QLabel(tr("E/W:"), this), 0);
        centerPosLay->addSpacing(LABEL_SPACING);
        centerPosLay->addWidget(m_ewCenterField, 10);
        centerPosLay->addWidget(m_lockEWCenterCheckbox, 0);

        centerPosLay->addSpacing(ITEM_SPACING);

        centerPosLay->addWidget(new QLabel(tr("N/S:"), this), 0);
        centerPosLay->addSpacing(LABEL_SPACING);
        centerPosLay->addWidget(m_nsCenterField, 10);
        centerPosLay->addWidget(m_lockNSCenterCheckbox, 0);

        centerPosLay->addStretch(1);
      }
    }

    addSeparator();

    mainLay->addWidget(m_globalKey, 0);

    mainLay->addSpacing(5);

    QHBoxLayout *pickLay = new QHBoxLayout();
    pickLay->setMargin(0);
    pickLay->setSpacing(0);
    mainLay->addLayout(pickLay, 0);
    {
      pickLay->addWidget(new QLabel(tr("Pick:"), this), 0);
      pickLay->addWidget(m_pickCombo, 0);
    }
  }

  /* --- signal-slot connections --- */
  // swap page when the active axis is changed
  connect(m_chooseActiveAxisCombo, SIGNAL(currentIndexChanged(int)),
          m_mainStackedWidget, SLOT(setCurrentIndex(int)));
  // when the current stage object is changed via combo box, then switch the
  // current stage object in the scene
  connect(m_currentStageObjectCombo, SIGNAL(activated(int)), this,
          SLOT(onCurrentStageObjectComboActivated(int)));

  /* --- Assigning shortcut keys --- */
  if (activeAxisProp->getId() != "") {
    std::string actionName = "A_ToolOption_" + activeAxisProp->getId();
    QAction *a = CommandManager::instance()->getAction(actionName.c_str());

    if (a) {
      m_chooseActiveAxisCombo->addAction(a);
      QObject::connect(a, SIGNAL(triggered()), m_chooseActiveAxisCombo,
                       SLOT(doShowPopup()));
    }

    TEnumProperty::Range range = activeAxisProp->getRange();
    TEnumProperty::Range::iterator it;
    QSignalMapper *signalMapper = 0;
    int index                   = 0;
    for (it = range.begin(); it != range.end(); ++it, ++index) {
      std::string item           = ::to_string(*it);
      std::string itemActionName = actionName + ":" + item;
      a = CommandManager::instance()->getAction(itemActionName.c_str());
      if (a) {
        m_chooseActiveAxisCombo->addAction(a);
        if (signalMapper == 0) {
          signalMapper = new QSignalMapper(m_chooseActiveAxisCombo);
          QObject::connect(signalMapper, SIGNAL(mapped(int)),
                           m_chooseActiveAxisCombo, SLOT(doOnActivated(int)));
        }
        QObject::connect(a, SIGNAL(triggered()), signalMapper, SLOT(map()));
        signalMapper->setMapping(a, index);
      }
    }
  }

  if (scaleConstraintProp) {
    m_scaleHField->onScaleTypeChanged(m_maintainCombo->currentIndex());
    m_scaleVField->onScaleTypeChanged(m_maintainCombo->currentIndex());
    connect(m_maintainCombo, SIGNAL(currentIndexChanged(int)), m_scaleHField,
            SLOT(onScaleTypeChanged(int)));
    connect(m_maintainCombo, SIGNAL(currentIndexChanged(int)), m_scaleVField,
            SLOT(onScaleTypeChanged(int)));
  }
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::showEvent(QShowEvent *) {
  connect(m_frameHandle, SIGNAL(frameSwitched()), SLOT(onFrameSwitched()));
  // if some stage object is added/removed, then reflect it to the combobox
  connect(m_xshHandle, SIGNAL(xsheetSwitched()), this,
          SLOT(updateStageObjectComboItems()));
  connect(m_xshHandle, SIGNAL(xsheetChanged()), this,
          SLOT(updateStageObjectComboItems()));
  // If the current stage object is switched, then synchronize it to the
  // combobox
  connect(m_objHandle, SIGNAL(objectSwitched()), this,
          SLOT(syncCurrentStageObjectComboItem()));

  // update the item list in m_currentStageObjectCombo
  updateStageObjectComboItems();
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::hideEvent(QShowEvent *) {
  disconnect(m_frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(onFrameSwitched()));

  disconnect(m_xshHandle, SIGNAL(xsheetSwitched()), this,
             SLOT(updateStageObjectComboItems()));
  disconnect(m_xshHandle, SIGNAL(xsheetChanged()), this,
             SLOT(updateStageObjectComboItems()));
  disconnect(m_objHandle, SIGNAL(objectSwitched()), this,
             SLOT(syncCurrentStageObjectComboItem()));
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::setSplined(bool on) {
  m_splined = on;
  // Activate on selecting spline
  m_motionPathPosField->setVisible(on);
  // DEactivate on selecting spline
  m_ewPosField->setVisible(!on);
  m_nsPosField->setVisible(!on);
  m_ewPosLabel->setVisible(!on);
  m_nsPosLabel->setVisible(!on);
  m_lockEWPosCheckbox->setVisible(!on);
  m_lockNSPosCheckbox->setVisible(!on);
}

//-----------------------------------------------------------------------------

bool ArrowToolOptionsBox::isCurrentObjectSplined() const {
  TStageObjectId objId = m_objHandle->getObjectId();
  return m_xshHandle->getXsheet()->getStageObject(objId)->getSpline() != 0;
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::updateStatus() {
  // General
  m_chooseActiveAxisCombo->updateStatus();
  m_pickCombo->updateStatus();

  // Position
  m_motionPathPosField->updateStatus();
  m_ewPosField->updateStatus();
  m_nsPosField->updateStatus();
  m_zField->updateStatus();
  m_noScaleZField->updateStatus();
  m_soField->updateStatus();

  // Rotation
  m_rotationField->updateStatus();

  // Scale
  m_globalScaleField->updateStatus();
  m_scaleHField->updateStatus();
  m_scaleVField->updateStatus();
  m_maintainCombo->updateStatus();

  // Shear
  m_shearHField->updateStatus();
  m_shearVField->updateStatus();

  // Center Position
  m_ewCenterField->updateStatus();
  m_nsCenterField->updateStatus();

  bool splined = isCurrentObjectSplined();
  if (splined != m_splined) setSplined(splined);
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::onStageObjectChange() { updateStatus(); }

//-----------------------------------------------------------------------------
/*! update the object list in combobox
*/
void ArrowToolOptionsBox::updateStageObjectComboItems() {
  // clear items
  m_currentStageObjectCombo->clear();

  TXsheet *xsh = m_xshHandle->getXsheet();

  TStageObjectId id;
  for (int i = 0; i < xsh->getStageObjectTree()->getStageObjectCount(); i++) {
    id = xsh->getStageObjectTree()->getStageObject(i)->getId();
    if (id.isColumn()) {
      int columnIndex = id.getIndex();
      if (xsh->isColumnEmpty(columnIndex)) continue;
    }

    TStageObject *pegbar = xsh->getStageObject(id);
    QString itemName     = QString::fromStdString(pegbar->getName());
    // store the item with ObjectId data
    m_currentStageObjectCombo->addItem(itemName, (int)id.getCode());
  }

  // synchronize
  syncCurrentStageObjectComboItem();
}

//------------------------------------------------------------------------------
/*! syncronize the current item in the combobox to the selected stage object
*/
void ArrowToolOptionsBox::syncCurrentStageObjectComboItem() {
  TStageObjectId curObjId = m_objHandle->getObjectId();

  int index = m_currentStageObjectCombo->findData((int)curObjId.getCode());

  // if the item is found
  if (index >= 0) m_currentStageObjectCombo->setCurrentIndex(index);
  // if not found, add a new item. (This may happens when selecting the empty
  // column.)
  else {
    TStageObject *pegbar = m_xshHandle->getXsheet()->getStageObject(curObjId);
    QString itemName     = QString::fromStdString(pegbar->getName());
    // store the item with ObjectId data
    m_currentStageObjectCombo->addItem(itemName, (int)curObjId.getCode());
    // move the current index to it
    m_currentStageObjectCombo->setCurrentIndex(
        m_currentStageObjectCombo->findText(itemName));
  }
}

//------------------------------------------------------------------------------
/*!change the current stage object when user changes it via combobox by hand
*/
void ArrowToolOptionsBox::onCurrentStageObjectComboActivated(int index) {
  int code = m_currentStageObjectCombo->itemData(index).toInt();
  TStageObjectId id;
  id.setCode(code);
  if (id == TStageObjectId::NoneId) {
    std::cout << "Warning: "
                 "ArrowToolOptionsBox::onCurrentStageObjectComboActivated \nNo "
                 "stage object linked to the selected item found in the scene."
              << std::endl;
    return;
  }
  // switch the current object
  m_objHandle->setObjectId(id);
}

//=============================================================================
//
// SelectionToolOptionsBox
//
//=============================================================================

IconViewField::IconViewField(QWidget *parent, IconType iconType)
    : QWidget(parent), m_iconType(iconType) {
  setMinimumSize(21, 25);
}

void IconViewField::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  // La pixmap e' alta 17 px, il widget 23. Per centrarla faccio il draw a 3 px.
  p.drawPixmap(QRect(0, 3, 21, 17), m_pm[m_iconType]);
}

//-----------------------------------------------------------------------------

SelectionToolOptionsBox::SelectionToolOptionsBox(QWidget *parent, TTool *tool,
                                                 TPaletteHandle *pltHandle,
                                                 ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_tool(tool)
    , m_isVectorSelction(false)
    , m_setSaveboxCheckbox(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  RasterSelectionTool *rasterSelectionTool =
      dynamic_cast<RasterSelectionTool *>(tool);

  SelectionTool *selectionTool = dynamic_cast<SelectionTool *>(tool);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  IconViewField *iconView =
      new IconViewField(this, IconViewField::Icon_ScalePeg);
  m_scaleXLabel = new QLabel(tr("H:"), this);
  m_scaleXField = new SelectionScaleField(selectionTool, 0, "Scale X");
  m_scaleYLabel = new QLabel(tr("V:"), this);
  m_scaleYField = new SelectionScaleField(selectionTool, 1, "Scale Y");
  m_scaleLink   = new DVGui::CheckBox(tr("Link"), this);

  IconViewField *rotIconView =
      new IconViewField(this, IconViewField::Icon_Rotation);
  m_rotationField = new SelectionRotationField(selectionTool, tr("Rotation"));

  IconViewField *moveIconView =
      new IconViewField(this, IconViewField::Icon_Position);
  m_moveXLabel = new QLabel(tr("E/W:"), this);
  m_moveXField = new SelectionMoveField(selectionTool, 0, "Move X");
  m_moveYLabel = addLabel(tr("N/S:"));
  m_moveYField = new SelectionMoveField(selectionTool, 1, "Move Y");

  if (rasterSelectionTool) {
    TBoolProperty *modifySetSaveboxProp =
        rasterSelectionTool->getModifySaveboxProperty();
    if (modifySetSaveboxProp)
      m_setSaveboxCheckbox =
          new ToolOptionCheckbox(rasterSelectionTool, modifySetSaveboxProp);
  }

  m_scaleXLabel->setEnabled(false);
  m_scaleYLabel->setEnabled(false);
  m_moveXLabel->setEnabled(false);
  m_moveYLabel->setEnabled(false);

  //--- layout ----

  addSeparator();

  hLayout()->addWidget(iconView, 0);
  hLayout()->addWidget(m_scaleXLabel, 0);
  hLayout()->addWidget(m_scaleXField, 10);
  hLayout()->addWidget(m_scaleYLabel, 0);
  hLayout()->addWidget(m_scaleYField, 10);
  hLayout()->addSpacing(4);
  hLayout()->addWidget(m_scaleLink, 0);

  addSeparator();

  hLayout()->addWidget(rotIconView, 0);
  hLayout()->addWidget(m_rotationField, 10);

  addSeparator();

  hLayout()->addWidget(moveIconView, 0);
  hLayout()->addWidget(m_moveXLabel, 0);
  hLayout()->addWidget(m_moveXField, 10);
  hLayout()->addWidget(m_moveYLabel, 0);
  hLayout()->addWidget(m_moveYField, 10);
  if (m_setSaveboxCheckbox) {
    addSeparator();
    hLayout()->addWidget(m_setSaveboxCheckbox, 0);
  }

  VectorSelectionTool *vectorSelectionTool =
      dynamic_cast<VectorSelectionTool *>(tool);
  if (vectorSelectionTool) {
    m_isVectorSelction = true;

    // change Thick
    IconViewField *thicknessIconView =
        new IconViewField(this, IconViewField::Icon_Thickness);
    m_thickChangeField = new ThickChangeField(selectionTool, tr("Thickness"));

    addSeparator();
    hLayout()->addWidget(thicknessIconView, 0);
    hLayout()->addWidget(m_thickChangeField, 10);
    // Outline options
    // Outline options
    ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
    builder.setEnumWidgetType(ToolOptionControlBuilder::POPUPBUTTON);
    builder.setSingleValueWidgetType(ToolOptionControlBuilder::FIELD);

    addSeparator();
    if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);

    m_capStyle = dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Cap"));
    m_joinStyle =
        dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Join"));

    m_miterField =
        dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Miter:"));
    m_miterField->setEnabled(m_joinStyle->currentIndex() ==
                             TStroke::OutlineOptions::MITER_JOIN);

    onPropertyChanged();
  }

  hLayout()->addStretch(1);
  // assert(ret);
  bool ret = connect(m_scaleXField, SIGNAL(valueChange()),
                     SLOT(onScaleXValueChanged()));
  ret = ret && connect(m_scaleYField, SIGNAL(valueChange()),
                       SLOT(onScaleYValueChanged()));
  if (m_setSaveboxCheckbox)
    ret = ret && connect(m_setSaveboxCheckbox, SIGNAL(toggled(bool)),
                         SLOT(onSetSaveboxCheckboxChanged(bool)));

  // assert(ret);

  updateStatus();
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();

  if (m_setSaveboxCheckbox) {
    bool disable = m_setSaveboxCheckbox->checkState() == Qt::Checked;
    for (int i = 0; i < hLayout()->count(); i++) {
      QWidget *w = hLayout()->itemAt(i)->widget();
      if (w && w != m_setSaveboxCheckbox) w->setEnabled(!disable);
    }
    if (disable) return;
  }

  m_scaleXField->updateStatus();
  m_scaleXLabel->setEnabled(m_scaleXField->isEnabled());
  m_scaleYField->updateStatus();
  m_scaleYLabel->setEnabled(m_scaleXField->isEnabled());
  m_rotationField->updateStatus();
  m_moveXField->updateStatus();
  m_moveXLabel->setEnabled(m_moveXField->isEnabled());
  m_moveYField->updateStatus();
  m_moveYLabel->setEnabled(m_moveYField->isEnabled());

  if (m_isVectorSelction) {
    m_thickChangeField->updateStatus();
    onPropertyChanged();
  }
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onScaleXValueChanged() {
  if (!m_scaleLink->isChecked() ||
      m_scaleXField->getValue() == m_scaleYField->getValue())
    return;
  m_scaleYField->setValue(m_scaleXField->getValue());
  m_scaleYField->applyChange();
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onScaleYValueChanged() {
  if (!m_scaleLink->isChecked() ||
      m_scaleXField->getValue() == m_scaleYField->getValue())
    return;
  m_scaleXField->setValue(m_scaleYField->getValue());
  m_scaleXField->applyChange();
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onSetSaveboxCheckboxChanged(bool) {
  updateStatus();
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onPropertyChanged() {
  // Check the selection's outline styles group.
  VectorSelectionTool *tool = (VectorSelectionTool *)m_tool;

  int capStyle, joinStyle;
  tool->selectionOutlineStyle(capStyle, joinStyle);

  // Show a void icon when no homogeneous style is found
  if (capStyle < 0)
    m_capStyle->setIcon(QPixmap());
  else
    // m_capStyle->setIcon(m_capStyle->currentItem()->icon());
    m_capStyle->setCurrentIndex(capStyle);

  if (joinStyle < 0)
    m_joinStyle->setIcon(QPixmap());
  else
    m_joinStyle->setCurrentIndex(joinStyle);
  // m_joinStyle->setIcon(m_joinStyle->currentItem()->icon());

  // Enable the miter field only in case the join is of type miter
  m_miterField->setEnabled(joinStyle == TStroke::OutlineOptions::MITER_JOIN);
}

//=============================================================================
//
// GeometricToolOptionsBox
//
//=============================================================================

GeometricToolOptionsBox::GeometricToolOptionsBox(QWidget *parent, TTool *tool,
                                                 TPaletteHandle *pltHandle,
                                                 ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_targetType(tool->getTargetType())
    , m_poligonSideLabel(0)
    , m_hardnessLabel(0)
    , m_hardnessField(0)
    , m_poligonSideField(0)
    , m_shapeField(0)
    , m_snapCheckbox(0)
    , m_snapSensitivityCombo(0)
    , m_tool(tool)
    , m_pencilMode(0) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);
  builder.setEnumWidgetType(ToolOptionControlBuilder::POPUPBUTTON);
  builder.setSingleValueWidgetType(ToolOptionControlBuilder::FIELD);
  addSeparator();
  if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);

  hLayout()->addStretch(1);
  m_hardnessField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Hardness:"));
  if (m_hardnessField)
    m_hardnessLabel = m_labels.value(m_hardnessField->propertyName());

  m_shapeField = dynamic_cast<ToolOptionCombo *>(m_controls.value("Shape:"));
  m_poligonSideField =
      dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Polygon Sides:"));
  if (m_poligonSideField)
    m_poligonSideLabel = m_labels.value(m_poligonSideField->propertyName());
  m_pencilMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pencil Mode"));

  if (m_shapeField->currentText().toStdWString() != L"Polygon") {
    m_poligonSideLabel->setEnabled(false);
    m_poligonSideField->setEnabled(false);
  }
  bool ret = connect(m_shapeField, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onShapeValueChanged()));

  if (m_pencilMode) {
    if (m_pencilMode->isChecked()) {
      m_hardnessLabel->setEnabled(false);
      m_hardnessField->setEnabled(false);
    }
    ret = ret && connect(m_pencilMode, SIGNAL(toggled(bool)), this,
                         SLOT(onPencilModeToggled(bool)));
  }

  if (tool->getTargetType() & TTool::Vectors) {
    m_snapCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Snap"));
    m_snapSensitivityCombo =
        dynamic_cast<ToolOptionCombo *>(m_controls.value("Sensitivity:"));
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
  }

  ToolOptionPopupButton *m_joinStyle =
      dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Join"));
  m_miterField =
      dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Miter:"));
  m_miterField->setEnabled(m_joinStyle->currentIndex() ==
                           TStroke::OutlineOptions::MITER_JOIN);

  assert(m_joinStyle && m_miterField);
  connect(m_joinStyle, SIGNAL(activated(int)), this,
          SLOT(onJoinStyleChanged(int)));

  assert(ret);
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
  if (m_tool->getTargetType() & TTool::Vectors) {
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
  }
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::onShapeValueChanged() {
  bool enabled = m_shapeField->currentText() == QString("Polygon");
  m_poligonSideLabel->setEnabled(enabled);
  m_poligonSideField->setEnabled(enabled);
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::onPencilModeToggled(bool value) {
  m_hardnessLabel->setEnabled(!value);
  m_hardnessField->setEnabled(!value);
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::onJoinStyleChanged(int joinStyle) {
  m_miterField->setEnabled(joinStyle == TStroke::OutlineOptions::MITER_JOIN);
}

//=============================================================================
//
// TypeToolOptionsBox
//
//=============================================================================

TypeToolOptionsBox::TypeToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent), m_tool(tool) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_layout->addStretch(0);

  bool ret = true;
  ToolOptionCombo *fontField =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Font:"));
  ret &&connect(fontField, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onFieldChanged()));

#ifndef MACOSX
  ToolOptionCombo *styleField =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Style:"));
  ret &&connect(styleField, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onFieldChanged()));
#endif

  ToolOptionCombo *sizeField =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Size:"));
  ret &&connect(sizeField, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onFieldChanged()));

  ToolOptionCheckbox *orientationField = dynamic_cast<ToolOptionCheckbox *>(
      m_controls.value("Vertical Orientation"));
  ret = ret && connect(orientationField, SIGNAL(stateChanged(int)), this,
                       SLOT(onFieldChanged()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void TypeToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void TypeToolOptionsBox::onFieldChanged() {
  assert(m_tool);
  m_tool->getViewer()->setFocus();
}

//=============================================================================
//
// PaintbrushToolOptionsBox
//
//=============================================================================

PaintbrushToolOptionsBox::PaintbrushToolOptionsBox(QWidget *parent, TTool *tool,
                                                   TPaletteHandle *pltHandle,
                                                   ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);
  hLayout()->addStretch(1);

  m_colorMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode:"));
  m_selectiveMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Selective"));

  if (m_colorMode->currentText().toStdWString() == L"Lines")
    m_selectiveMode->setEnabled(false);

  bool ret = connect(m_colorMode, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onColorModeChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void PaintbrushToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void PaintbrushToolOptionsBox::onColorModeChanged() {
  bool enabled = m_colorMode->currentText() != QString("Lines");
  m_selectiveMode->setEnabled(enabled);
}

//=============================================================================
//
// FillToolOptionsBox
//
//=============================================================================

FillToolOptionsBox::FillToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_targetType(tool->getTargetType())
    , m_fillDepthLabel(0)
    , m_fillDepthField(0)
    , m_segmentMode(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_layout->addStretch(0);

  m_toolType  = dynamic_cast<ToolOptionCombo *>(m_controls.value("Type:"));
  m_colorMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode:"));
  m_selectiveMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Selective"));
  m_fillDepthField =
      dynamic_cast<ToolOptionPairSlider *>(m_controls.value("Fill Depth"));
  if (m_fillDepthField)
    m_fillDepthLabel = m_labels.value(m_fillDepthField->propertyName());
  m_segmentMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Segment"));
  m_onionMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Onion Skin"));
  m_multiFrameMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Frame Range"));

  bool ret = connect(m_colorMode, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onColorModeChanged()));
  ret = ret && connect(m_toolType, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onToolTypeChanged()));
  ret = ret && connect(m_onionMode, SIGNAL(toggled(bool)), this,
                       SLOT(onOnionModeToggled(bool)));
  ret = ret && connect(m_multiFrameMode, SIGNAL(toggled(bool)), this,
                       SLOT(onMultiFrameModeToggled(bool)));
  assert(ret);
  if (m_colorMode->currentText().toStdWString() == L"Lines") {
    m_selectiveMode->setEnabled(false);
    if (m_fillDepthLabel && m_fillDepthField) {
      m_fillDepthLabel->setEnabled(false);
      m_fillDepthField->setEnabled(false);
    }
    if (m_toolType->currentText() == QString("Normal") ||
        m_multiFrameMode->isChecked())
      m_onionMode->setEnabled(false);
  }
  if (m_toolType->currentText().toStdWString() != L"Normal") {
    if (m_segmentMode) m_segmentMode->setEnabled(false);
    if (m_colorMode->currentText() == QString("Lines") ||
        m_multiFrameMode->isChecked())
      m_onionMode->setEnabled(false);
  }
  if (m_onionMode->isChecked()) m_multiFrameMode->setEnabled(false);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onColorModeChanged() {
  bool enabled = m_colorMode->currentText() != QString("Lines");
  m_selectiveMode->setEnabled(enabled);
  if (m_fillDepthLabel && m_fillDepthField) {
    m_fillDepthLabel->setEnabled(enabled);
    m_fillDepthField->setEnabled(enabled);
  }
  if (m_segmentMode) {
    enabled = m_colorMode->currentText() != QString("Areas");
    m_segmentMode->setEnabled(
        enabled ? m_toolType->currentText() == QString("Normal") : false);
  }
  enabled = m_colorMode->currentText() != QString("Lines") &&
            !m_multiFrameMode->isChecked();
  m_onionMode->setEnabled(enabled);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onToolTypeChanged() {
  bool enabled = m_toolType->currentText() == QString("Normal");
  if (m_segmentMode)
    m_segmentMode->setEnabled(
        enabled ? m_colorMode->currentText() != QString("Areas") : false);
  enabled = enabled || (m_colorMode->currentText() != QString("Lines") &&
                        !m_multiFrameMode->isChecked());
  m_onionMode->setEnabled(enabled);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onOnionModeToggled(bool value) {
  m_multiFrameMode->setEnabled(!value);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onMultiFrameModeToggled(bool value) {
  m_onionMode->setEnabled(!value);
}

//=============================================================================
//
// BrushToolOptionsBox classes
//
//=============================================================================

class BrushToolOptionsBox::PresetNamePopup final : public DVGui::Dialog {
  DVGui::LineEdit *m_nameFld;

public:
  PresetNamePopup() : Dialog(0, true) {
    setWindowTitle(tr("Preset Name"));
    m_nameFld = new DVGui::LineEdit();
    addWidget(m_nameFld);

    QPushButton *okBtn = new QPushButton(tr("OK"), this);
    okBtn->setDefault(true);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

    addButtonBarWidget(okBtn, cancelBtn);
  }

  QString getName() { return m_nameFld->text(); }
  void removeName() { m_nameFld->setText(QString("")); }
};

//=============================================================================
//
// BrushToolOptionsBox
//
//=============================================================================

BrushToolOptionsBox::BrushToolOptionsBox(QWidget *parent, TTool *tool,
                                         TPaletteHandle *pltHandle,
                                         ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_tool(tool)
    , m_presetNamePopup(0)
    , m_pencilMode(0)
    , m_hardnessLabel(0)
    , m_joinStyleCombo(0)
    , m_snapCheckbox(0)
    , m_snapSensitivityCombo(0)
    , m_miterField(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_hardnessField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Hardness:"));
  if (m_hardnessField)
    m_hardnessLabel = m_labels.value(m_hardnessField->propertyName());
  m_pencilMode = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pencil"));
  m_presetCombo = dynamic_cast<ToolOptionCombo *>(m_controls.value("Preset:"));

  // Preset +/- buttons
  m_addPresetButton    = new QPushButton(QString("+"));
  m_removePresetButton = new QPushButton(QString("-"));

  m_addPresetButton->setFixedSize(QSize(20, 20));
  m_removePresetButton->setFixedSize(QSize(20, 20));

  hLayout()->addWidget(m_addPresetButton);
  hLayout()->addWidget(m_removePresetButton);

  connect(m_addPresetButton, SIGNAL(clicked()), this, SLOT(onAddPreset()));
  connect(m_removePresetButton, SIGNAL(clicked()), this,
          SLOT(onRemovePreset()));

  if (tool->getTargetType() & TTool::ToonzImage) {
    assert(m_pencilMode);
    bool ret = connect(m_pencilMode, SIGNAL(toggled(bool)), this,
                       SLOT(onPencilModeToggled(bool)));
    assert(ret);

    if (m_pencilMode->isChecked()) {
      m_hardnessLabel->setEnabled(false);
      m_hardnessField->setEnabled(false);
    }
  } else if (tool->getTargetType() & TTool::Vectors) {
    // Further vector options
    builder.setEnumWidgetType(ToolOptionControlBuilder::POPUPBUTTON);
    builder.setSingleValueWidgetType(ToolOptionControlBuilder::FIELD);

    addSeparator();
    if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);
    m_snapCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Snap"));
    m_snapSensitivityCombo =
        dynamic_cast<ToolOptionCombo *>(m_controls.value("Sensitivity:"));
    m_joinStyleCombo =
        dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Join"));
    m_miterField =
        dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Miter:"));
    m_miterField->setEnabled(m_joinStyleCombo->currentIndex() ==
                             TStroke::OutlineOptions::MITER_JOIN);
  }
  hLayout()->addStretch(1);
  filterControls();
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::filterControls() {
  // show or hide widgets which modify imported brush (mypaint)

  bool showModifiers = false;
  if (FullColorBrushTool *fullColorBrushTool =
          dynamic_cast<FullColorBrushTool *>(m_tool))
    showModifiers = fullColorBrushTool->getBrushStyle();

  for (QMap<std::string, QLabel *>::iterator it = m_labels.begin();
       it != m_labels.end(); it++) {
    bool isModifier = (it.key().substr(0, 8) == "Modifier");
    bool isCommon   = (it.key() == "Pressure" || it.key() == "Preset:");
    bool visible    = isCommon || (isModifier == showModifiers);
    it.value()->setVisible(visible);
  }

  for (QMap<std::string, ToolOptionControl *>::iterator it = m_controls.begin();
       it != m_controls.end(); it++) {
    bool isModifier = (it.key().substr(0, 8) == "Modifier");
    bool isCommon   = (it.key() == "Pressure" || it.key() == "Preset:");
    bool visible    = isCommon || (isModifier == showModifiers);
    if (QWidget *widget = dynamic_cast<QWidget *>(it.value()))
      widget->setVisible(visible);
  }
  if (m_tool->getTargetType() & TTool::Vectors) {
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
  }
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::updateStatus() {
  filterControls();

  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();

  if (m_miterField)
    m_miterField->setEnabled(m_joinStyleCombo->currentIndex() ==
                             TStroke::OutlineOptions::MITER_JOIN);
  if (m_snapCheckbox)
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::onPencilModeToggled(bool value) {
  m_hardnessLabel->setEnabled(!value);
  m_hardnessField->setEnabled(!value);
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::onAddPreset() {
  // Initialize preset name popup
  if (!m_presetNamePopup) m_presetNamePopup = new PresetNamePopup;

  if (!m_presetNamePopup->getName().isEmpty()) m_presetNamePopup->removeName();

  // Retrieve the preset name
  bool ret = m_presetNamePopup->exec();
  if (!ret) return;

  QString name(m_presetNamePopup->getName());
  m_presetNamePopup->removeName();

  switch (m_tool->getTargetType() & TTool::CommonImages) {
  case TTool::VectorImage:
  case TTool::ToonzImage: {
    static_cast<BrushTool *>(m_tool)->addPreset(name);
    break;
  }

  case TTool::RasterImage: {
    static_cast<FullColorBrushTool *>(m_tool)->addPreset(name);
    break;
  }
  }

  m_presetCombo->loadEntries();
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::onRemovePreset() {
  switch (m_tool->getTargetType() & TTool::CommonImages) {
  case TTool::VectorImage:
  case TTool::ToonzImage: {
    static_cast<BrushTool *>(m_tool)->removePreset();
    break;
  }

  case TTool::RasterImage: {
    static_cast<FullColorBrushTool *>(m_tool)->removePreset();
    break;
  }
  }

  m_presetCombo->loadEntries();
}

//=============================================================================
//
// EraserToolOptionsBox
//
//=============================================================================

EraserToolOptionsBox::EraserToolOptionsBox(QWidget *parent, TTool *tool,
                                           TPaletteHandle *pltHandle,
                                           ToolHandle *toolHandle)
    : ToolOptionsBox(parent), m_pencilMode(0), m_colorMode(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  hLayout()->addStretch(1);

  m_toolType = dynamic_cast<ToolOptionCombo *>(m_controls.value("Type:"));
  m_hardnessField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Hardness:"));
  if (m_hardnessField)
    m_hardnessLabel = m_labels.value(m_hardnessField->propertyName());
  m_colorMode  = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode:"));
  m_invertMode = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Invert"));
  m_multiFrameMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Frame Range"));
  m_pencilMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pencil Mode"));

  bool ret = true;
  if (m_pencilMode) {
    ret = ret && connect(m_pencilMode, SIGNAL(toggled(bool)), this,
                         SLOT(onPencilModeToggled(bool)));
    ret = ret && connect(m_colorMode, SIGNAL(currentIndexChanged(int)), this,
                         SLOT(onColorModeChanged()));
  }

  ret = ret && connect(m_toolType, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onToolTypeChanged()));

  if (m_pencilMode && m_pencilMode->isChecked()) {
    assert(m_hardnessField && m_hardnessLabel);
    m_hardnessField->setEnabled(false);
    m_hardnessLabel->setEnabled(false);
  }

  if (m_toolType && m_toolType->currentText() == QString("Normal")) {
    m_invertMode->setEnabled(false);
    m_multiFrameMode->setEnabled(false);
  }

  if (m_colorMode && m_colorMode->currentText() == QString("Areas")) {
    assert(m_hardnessField && m_hardnessLabel && m_pencilMode);
    m_pencilMode->setEnabled(false);
    m_hardnessField->setEnabled(false);
    m_hardnessLabel->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::onPencilModeToggled(bool value) {
  if (m_hardnessField && m_hardnessLabel) {
    m_hardnessField->setEnabled(!value);
    m_hardnessLabel->setEnabled(!value);
  }
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::onToolTypeChanged() {
  bool value = m_toolType && m_toolType->currentText() != QString("Normal");
  m_invertMode->setEnabled(value);
  m_multiFrameMode->setEnabled(value);
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::onColorModeChanged() {
  bool enabled = m_colorMode->currentText() != QString("Areas");
  if (m_pencilMode && m_hardnessField && m_hardnessLabel) {
    m_pencilMode->setEnabled(enabled);
    m_hardnessField->setEnabled(enabled ? !m_pencilMode->isChecked() : false);
    m_hardnessLabel->setEnabled(enabled ? !m_pencilMode->isChecked() : false);
  }
}

//=============================================================================
//
// RulerToolOptionsBox
//
//=============================================================================
class ToolOptionsBarSeparator final : public QWidget {
public:
  ToolOptionsBarSeparator(QWidget *parent) : QWidget(parent) {
    setFixedSize(2, 26);
  }

  void paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setPen(QColor(64, 64, 64));
    p.drawLine(0, 0, 0, 25);
    p.setPen(Qt::white);
    p.drawLine(1, 0, 1, 25);
  }
};

RulerToolOptionsBox::RulerToolOptionsBox(QWidget *parent, TTool *tool)
    : ToolOptionsBox(parent), m_tool(tool) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_Xfld = new MeasuredValueField(this);
  m_Yfld = new MeasuredValueField(this);
  m_Wfld = new MeasuredValueField(this);
  m_Hfld = new MeasuredValueField(this);
  m_Afld = new MeasuredValueField(this);
  m_Lfld = new MeasuredValueField(this);

  m_XpixelFld = new QLabel(this);
  m_YpixelFld = new QLabel(this);
  m_WpixelFld = new QLabel(this);
  m_HpixelFld = new QLabel(this);

  m_Afld->setMeasure("angle");

  if (Preferences::instance()->getUnits() == "pixel") {
    m_Xfld->setMeasure("length.x");
    m_Yfld->setMeasure("length.y");
    m_Wfld->setMeasure("length.x");
    m_Hfld->setMeasure("length.y");
    m_Lfld->setMeasure("length.x");
  }

  m_Xfld->setObjectName("RulerToolOptionValues");
  m_Yfld->setObjectName("RulerToolOptionValues");
  m_Wfld->setObjectName("RulerToolOptionValues");
  m_Hfld->setObjectName("RulerToolOptionValues");
  m_Afld->setObjectName("RulerToolOptionValues");
  m_Lfld->setObjectName("RulerToolOptionValues");
  setStyleSheet(
      "#RulerToolOptionValues {border:0px; background: rgb(196,196,196);}");

  m_Xfld->setMaximumWidth(70);
  m_Yfld->setMaximumWidth(70);
  m_Wfld->setMaximumWidth(70);
  m_Hfld->setMaximumWidth(70);
  m_Afld->setMaximumWidth(70);
  m_Lfld->setMaximumWidth(70);

  m_Xfld->setReadOnly(true);
  m_Yfld->setReadOnly(true);
  m_Wfld->setReadOnly(true);
  m_Hfld->setReadOnly(true);
  m_Afld->setReadOnly(true);
  m_Lfld->setReadOnly(true);

  // layout
  QHBoxLayout *lay = new QHBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(3);
  {
    lay->addWidget(new QLabel("X:", this), 0);
    lay->addWidget(m_Xfld, 0);
    lay->addWidget(m_XpixelFld, 0);

    lay->addSpacing(3);

    lay->addWidget(new QLabel("Y:", this), 0);
    lay->addWidget(m_Yfld, 0);
    lay->addWidget(m_YpixelFld, 0);

    lay->addSpacing(3);
    lay->addWidget(new ToolOptionsBarSeparator(this), 0);
    lay->addSpacing(3);

    lay->addWidget(new QLabel("W:", this), 0);
    lay->addWidget(m_Wfld, 0);
    lay->addWidget(m_WpixelFld, 0);

    lay->addSpacing(3);

    lay->addWidget(new QLabel("H:", this), 0);
    lay->addWidget(m_Hfld, 0);
    lay->addWidget(m_HpixelFld, 0);

    lay->addSpacing(3);
    lay->addWidget(new ToolOptionsBarSeparator(this), 0);
    lay->addSpacing(3);

    lay->addWidget(new QLabel("A:", this), 0);
    lay->addWidget(m_Afld, 0);

    lay->addSpacing(3);

    lay->addWidget(new QLabel("L:", this), 0);
    lay->addWidget(m_Lfld, 0);
  }
  m_layout->addLayout(lay, 0);
  m_layout->addStretch(1);
}

void RulerToolOptionsBox::updateValues(bool isRasterLevelEditing, double X,
                                       double Y, double W, double H, double A,
                                       double L, int Xpix, int Ypix, int Wpix,
                                       int Hpix) {
  m_Xfld->setValue(X);
  m_Yfld->setValue(Y);
  m_Wfld->setValue(W);
  m_Hfld->setValue(H);
  m_Afld->setValue(A);
  m_Lfld->setValue(L);

  m_XpixelFld->setVisible(isRasterLevelEditing);
  m_YpixelFld->setVisible(isRasterLevelEditing);
  m_WpixelFld->setVisible(isRasterLevelEditing);
  m_HpixelFld->setVisible(isRasterLevelEditing);

  if (isRasterLevelEditing) {
    m_XpixelFld->setText(QString("(%1)").arg(Xpix));
    m_YpixelFld->setText(QString("(%1)").arg(Ypix));
    m_WpixelFld->setText(QString("(%1)").arg(Wpix));
    m_HpixelFld->setText(QString("(%1)").arg(Hpix));
  }
  repaint();
}

void RulerToolOptionsBox::resetValues() {
  m_Xfld->setValue(0);
  m_Yfld->setValue(0);
  m_Wfld->setValue(0);
  m_Hfld->setValue(0);
  m_Afld->setValue(0);
  m_Lfld->setValue(0);

  m_XpixelFld->setVisible(false);
  m_YpixelFld->setVisible(false);
  m_WpixelFld->setVisible(false);
  m_HpixelFld->setVisible(false);

  update();
}

//=============================================================================
//
// TapeToolOptionsBox
//
//=============================================================================

TapeToolOptionsBox::TapeToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_smoothMode(0)
    , m_joinStrokesMode(0)
    , m_toolMode(0)
    , m_autocloseLabel(0)
    , m_autocloseField(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  hLayout()->addStretch(1);
  if (!(tool->getTargetType() & TTool::Vectors)) return;

  m_smoothMode = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Smooth"));
  m_joinStrokesMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("JoinStrokes"));
  m_toolMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode"));
  m_typeMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Type"));
  m_autocloseField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Distance"));
  if (m_autocloseField)
    m_autocloseLabel = m_labels.value(m_autocloseField->propertyName());

  bool isNormalType = m_typeMode->currentText() == "Normal";
  m_toolMode->setEnabled(isNormalType);
  m_autocloseField->setEnabled(!isNormalType);
  m_autocloseLabel->setEnabled(!isNormalType);

  bool isLineToLineMode = m_toolMode->currentText() == "Line to Line";
  m_joinStrokesMode->setEnabled(!isLineToLineMode);

  bool isJoinStrokes = m_joinStrokesMode->isChecked();
  m_smoothMode->setEnabled(!isLineToLineMode && isJoinStrokes);

  bool ret = connect(m_typeMode, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onToolTypeChanged()));
  ret = ret && connect(m_toolMode, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onToolModeChanged()));
  ret = ret && connect(m_joinStrokesMode, SIGNAL(toggled(bool)), this,
                       SLOT(onJoinStrokesModeChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::onToolTypeChanged() {
  bool isNormalType = m_typeMode->currentText() == "Normal";
  m_toolMode->setEnabled(isNormalType);
  m_autocloseField->setEnabled(!isNormalType);
  m_autocloseLabel->setEnabled(!isNormalType);
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::onToolModeChanged() {
  bool isLineToLineMode = m_toolMode->currentText() == "Line to Line";
  m_joinStrokesMode->setEnabled(!isLineToLineMode);
  bool isJoinStrokes = m_joinStrokesMode->isChecked();
  m_smoothMode->setEnabled(!isLineToLineMode && isJoinStrokes);
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::onJoinStrokesModeChanged() {
  bool isLineToLineMode = m_toolMode->currentText() == "Line to Line";
  bool isJoinStrokes    = m_joinStrokesMode->isChecked();
  m_smoothMode->setEnabled(!isLineToLineMode && isJoinStrokes);
}

//=============================================================================
// RGBPickerToolOptions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*! Label with background color
*/
class RGBLabel final : public QWidget {
  QColor m_color;

public:
  RGBLabel(QColor color, QWidget *parent) : QWidget(parent), m_color(color) {
    setFixedSize(120, 20);
  }

  ~RGBLabel() {}

  void setColorAndUpdate(QColor color) {
    if (m_color == color) return;
    m_color = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent *pe) {
    QPainter p(this);
    p.setPen(Qt::NoPen);

    // iwsw commented out temporarily
    /*
    if (Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled())
    {
            QColor convertedColor(m_color);
            Ghibli3DLutConverter::instance()->convert(convertedColor);
            p.setBrush(convertedColor);
    }
    else
    */
    p.setBrush(m_color);
    p.setBrush(m_color);

    p.drawRect(rect());

    // white text on dark color. black text on light color.
    int val = m_color.red() * 30 + m_color.green() * 59 + m_color.blue() * 11;
    if (val < 12800)
      p.setPen(Qt::white);
    else
      p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);

    p.drawText(rect(), Qt::AlignCenter, QString("R:%1 G:%2 B:%3")
                                            .arg(m_color.red())
                                            .arg(m_color.green())
                                            .arg(m_color.blue()));
  }
};

//-----------------------------------------------------------------------------

RGBPickerToolOptionsBox::RGBPickerToolOptionsBox(
    QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
    ToolHandle *toolHandle, PaletteController *paletteController)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_currentRGBLabel = new RGBLabel(QColor(128, 128, 128), this);

  QAction *pickScreenAction =
      CommandManager::instance()->getAction("A_ToolOption_PickScreen");

  QPushButton *button = new QPushButton(tr("Pick Screen"));
  button->setFixedHeight(20);
  button->addAction(pickScreenAction);
  connect(button, SIGNAL(clicked()), pickScreenAction, SLOT(trigger()));

  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_realTimePickMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Passive Pick"));

  m_layout->addWidget(m_currentRGBLabel, 0);
  m_layout->addStretch(1);
  m_layout->addWidget(button, 0);  // new in 6.4

  if (m_realTimePickMode) {
    connect(m_realTimePickMode, SIGNAL(toggled(bool)), m_currentRGBLabel,
            SLOT(setVisible(bool)));
    m_currentRGBLabel->setVisible(m_realTimePickMode->isChecked());
  }

  // for passive pick
  connect(paletteController, SIGNAL(colorPassivePicked(const QColor &)), this,
          SLOT(updateRealTimePickLabel(const QColor &)));
}

//-----------------------------------------------------------------------------

void RGBPickerToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void RGBPickerToolOptionsBox::updateRealTimePickLabel(const QColor &pix) {
  if (isVisible()) m_currentRGBLabel->setColorAndUpdate(pix);
}

//=============================================================================
// StylePickerToolOptions
//-----------------------------------------------------------------------------

StylePickerToolOptionsBox::StylePickerToolOptionsBox(
    QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
    ToolHandle *toolHandle, PaletteController *paletteController)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_currentStyleLabel = new QLabel(" - - - ", this);
  m_currentStyleLabel->setObjectName("SytlePickerValue");
  setStyleSheet(
      "#SytlePickerValue {color: black; border:0px; background: "
      "rgb(196,196,196);}");

  m_currentStyleLabel->setFixedSize(200, 20);
  m_currentStyleLabel->setAlignment(Qt::AlignCenter);

  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_realTimePickMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Passive Pick"));

  m_layout->addWidget(m_currentStyleLabel, 0);
  m_layout->addStretch(1);
  // retrieve the "organize palette" checkbox from the layout and insert
  // into rightmost of the tool option bar
  ToolOptionCheckbox *organizePaletteCB =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Organize Palette"));
  m_layout->removeWidget(organizePaletteCB);
  m_layout->addWidget(new ToolOptionsBarSeparator(this), 0);
  m_layout->addWidget(organizePaletteCB);
  organizePaletteCB->setToolTip(
      tr("With this option being activated, the picked style will be\nmoved to "
         "the end of the first page of the palette."));

  if (m_realTimePickMode) {
    connect(m_realTimePickMode, SIGNAL(toggled(bool)), m_currentStyleLabel,
            SLOT(setVisible(bool)));
    m_currentStyleLabel->setVisible(m_realTimePickMode->isChecked());
  }

  // for passive pick
  connect(paletteController,
          SIGNAL(stylePassivePicked(const int, const int, const int)), this,
          SLOT(updateRealTimePickLabel(const int, const int, const int)));
}

//-----------------------------------------------------------------------------

void StylePickerToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void StylePickerToolOptionsBox::updateRealTimePickLabel(const int ink,
                                                        const int paint,
                                                        const int tone) {
  if (isVisible()) {
    if (ink < 0)
      m_currentStyleLabel->setText(" - - - ");
    else
      m_currentStyleLabel->setText(QString("INK: #%1  PAINT: #%2  TONE: %3")
                                       .arg(ink)
                                       .arg(paint)
                                       .arg(tone));
  }
}

//=============================================================================
// ToolOptions
//-----------------------------------------------------------------------------

ToolOptions::ToolOptions() : m_panel(0) {
  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

ToolOptions::~ToolOptions() {}

//-----------------------------------------------------------------------------

void ToolOptions::showEvent(QShowEvent *) {
  TTool::Application *app = TTool::getApplication();
  ToolHandle *currTool    = app->getCurrentTool();
  if (currTool) {
    onToolSwitched();
    connect(currTool, SIGNAL(toolSwitched()), SLOT(onToolSwitched()));
    connect(currTool, SIGNAL(toolChanged()), SLOT(onToolChanged()));
  }

  TObjectHandle *currObject = app->getCurrentObject();
  if (currObject) {
    onStageObjectChange();
    connect(currObject, SIGNAL(objectSwitched()), SLOT(onStageObjectChange()));
    connect(currObject, SIGNAL(objectChanged(bool)),
            SLOT(onStageObjectChange()));
  }

  TXshLevelHandle *currLevel = app->getCurrentLevel();

  if (currLevel)
    connect(currLevel, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
            SLOT(onStageObjectChange()));
}

//-----------------------------------------------------------------------------

void ToolOptions::hideEvent(QShowEvent *) {
  TTool::Application *app = TTool::getApplication();
  ToolHandle *currTool    = app->getCurrentTool();
  if (currTool) currTool->disconnect(this);

  TObjectHandle *currObject = app->getCurrentObject();
  if (currObject) currObject->disconnect(this);

  TXshLevelHandle *currLevel = app->getCurrentLevel();
  if (currLevel) currLevel->disconnect(this);
}

//-----------------------------------------------------------------------------

void ToolOptions::onToolSwitched() {
  if (m_panel) m_panel->hide();
  m_panel = 0;

  TTool::Application *app = TTool::getApplication();

  // Quando i seguenti non serviranno piu', rimuovere gli header corrispondenti
  // in cima, thx
  TFrameHandle *currFrame   = app->getCurrentFrame();
  TObjectHandle *currObject = app->getCurrentObject();
  TXsheetHandle *currXsheet = app->getCurrentXsheet();
  TPaletteHandle *currPalette =
      app->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = app->getCurrentTool();

  TTool *tool = app->getCurrentTool()->getTool();
  if (tool) {
    // c'e' un tool corrente
    ToolOptionsBox *panel = 0;
    std::map<TTool *, ToolOptionsBox *>::iterator it = m_panels.find(tool);
    if (it == m_panels.end()) {
      // ... senza panel associato
      if (tool->getName() == T_Edit) {
        TPropertyGroup *pg = tool->getProperties(0);
        panel = new ArrowToolOptionsBox(0, tool, pg, currFrame, currObject,
                                        currXsheet, currTool);
      } else if (tool->getName() == T_Selection)
        panel = new SelectionToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Geometric)
        panel = new GeometricToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Type)
        panel = new TypeToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_PaintBrush)
        panel = new PaintbrushToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Fill)
        panel = new FillToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Eraser)
        panel = new EraserToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Tape)
        panel = new TapeToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_RGBPicker)
        panel = new RGBPickerToolOptionsBox(0, tool, currPalette, currTool,
                                            app->getPaletteController());
      else if (tool->getName() == T_Ruler) {
        RulerToolOptionsBox *p = new RulerToolOptionsBox(0, tool);
        panel                  = p;
        RulerTool *rt          = dynamic_cast<RulerTool *>(tool);
        if (rt) rt->setToolOptionsBox(p);
      } else if (tool->getName() == T_StylePicker)
        panel = new StylePickerToolOptionsBox(0, tool, currPalette, currTool,
                                              app->getPaletteController());
      else
        panel = tool->createOptionsBox();  // Only this line should remain out
                                           // of that if/else monstrosity

      /* DANIELE: Regola per il futuro - NON FARE PIU' COME SOPRA.
Bisogna cominciare a collegare il metodo virtuale
createOptionsBox() di ogni tool.

Chi ha tempo si adoperi un pochino per normalizzare
la situazione anche per i tool sopra, plz - basta spostare
un po' di codice... */

      m_panels[tool] = panel;
      layout()->addWidget(panel);
      emit newPanelCreated();
    } else {
      // il panel c'e' gia'.
      panel = it->second;
      panel->updateStatus();
    }
    m_panel = panel;
    m_panel->show();
  } else {
    // niente tool
  }
}

//-----------------------------------------------------------------------------

void ToolOptions::onToolChanged() {
  assert(m_panel);
  ToolOptionsBox *optionBox = dynamic_cast<ToolOptionsBox *>(m_panel);
  assert(optionBox);
  optionBox->updateStatus();
}

//-----------------------------------------------------------------------------

void ToolOptions::onStageObjectChange() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  std::map<TTool *, ToolOptionsBox *>::iterator it = m_panels.find(tool);
  if (it == m_panels.end()) return;

  ToolOptionsBox *panel = it->second;
  panel->onStageObjectChange();
}
