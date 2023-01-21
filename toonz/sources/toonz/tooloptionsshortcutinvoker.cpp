#include "tooloptionsshortcutinvoker.h"

#include "tapp.h"
#include "tools/toolhandle.h"

#include "tools/toolcommandids.h"
#include "tools/tool.h"

#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include <QSignalMapper>

using namespace ToolOptionsShortcutWorker;

//-----------------------------------------------------------------
ToolOptionShortcutConnector::ToolOptionShortcutConnector(TTool* tool)
    : m_tool(tool) {}

//-----------------------------------------------------------------

void ToolOptionShortcutConnector::visit(TDoubleProperty* p) {
  if (p->getName() != "Size:" && p->getName() != "ModifierSize" &&
      p->getName() != "Hardness:")
    return;

  ToolOptionsShortcutInvoker* invoker = ToolOptionsShortcutInvoker::instance();
  DoubleWorker* worker                = new DoubleWorker(invoker, m_tool, p);
  CommandManager* cm                  = CommandManager::instance();
  QAction* a;
  if (p->getName() == "Size:") {
    a = cm->getAction("A_IncreaseMaxBrushThickness");
    QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increase()));
    a = cm->getAction("A_DecreaseMaxBrushThickness");
    QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decrease()));
  } else if (p->getName() == "ModifierSize") {
    a = cm->getAction("A_IncreaseMaxBrushThickness");
    QObject::connect(a, SIGNAL(triggered()), worker,
                     SLOT(increaseFractional()));
    a = cm->getAction("A_DecreaseMaxBrushThickness");
    QObject::connect(a, SIGNAL(triggered()), worker,
                     SLOT(decreaseFractional()));
  } else if (p->getName() == "Hardness:") {
    a = cm->getAction("A_IncreaseBrushHardness");
    QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increase()));
    a = cm->getAction("A_DecreaseBrushHardness");
    QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decrease()));
  }
}

//-----------------------------------------------------------------

void ToolOptionShortcutConnector::visit(TDoublePairProperty* p) {
  if (p->getName() != "Size:" && p->getName() != "Size") return;

  ToolOptionsShortcutInvoker* invoker = ToolOptionsShortcutInvoker::instance();
  DoublePairWorker* worker = new DoublePairWorker(invoker, m_tool, p);
  CommandManager* cm       = CommandManager::instance();
  QAction* a;
  a = cm->getAction("A_IncreaseMaxBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increaseMaxValue()));
  a = cm->getAction("A_DecreaseMaxBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decreaseMaxValue()));

  a = cm->getAction("A_IncreaseMinBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increaseMinValue()));
  a = cm->getAction("A_DecreaseMinBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decreaseMinValue()));
}

//-----------------------------------------------------------------

void ToolOptionShortcutConnector::visit(TIntPairProperty* p) {
  if (p->getName() != "Size:" && p->getName() != "Thickness" &&
      p->getName() != "Size")
    return;

  ToolOptionsShortcutInvoker* invoker = ToolOptionsShortcutInvoker::instance();
  IntPairWorker* worker               = new IntPairWorker(invoker, m_tool, p);
  CommandManager* cm                  = CommandManager::instance();
  QAction* a;
  a = cm->getAction("A_IncreaseMaxBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increaseMaxValue()));
  a = cm->getAction("A_DecreaseMaxBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decreaseMaxValue()));

  a = cm->getAction("A_IncreaseMinBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increaseMinValue()));
  a = cm->getAction("A_DecreaseMinBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decreaseMinValue()));
}

//-----------------------------------------------------------------

void ToolOptionShortcutConnector::visit(TIntProperty* p) {
  if (p->getName() != "Size:") return;
  ToolOptionsShortcutInvoker* invoker = ToolOptionsShortcutInvoker::instance();
  IntWorker* worker                   = new IntWorker(invoker, m_tool, p);
  CommandManager* cm                  = CommandManager::instance();
  QAction* a;
  a = cm->getAction("A_IncreaseMaxBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(increase()));
  a = cm->getAction("A_DecreaseMaxBrushThickness");
  QObject::connect(a, SIGNAL(triggered()), worker, SLOT(decrease()));
}

//-----------------------------------------------------------------

void ToolOptionShortcutConnector::visit(TBoolProperty* p) {
  if (p->getId() == "") return;
  std::string actionName = "A_ToolOption_" + p->getId();
  QAction* a = CommandManager::instance()->getAction(actionName.c_str());
  if (!a) return;
  ToolOptionsShortcutInvoker* invoker = ToolOptionsShortcutInvoker::instance();
  BoolWorker* worker                  = new BoolWorker(invoker, m_tool, p, a);
  a->setCheckable(true);
  a->setChecked(p->getValue());
  QObject::connect(a, SIGNAL(triggered(bool)), worker, SLOT(doCheck(bool)));

  invoker->registerCheckProperty(m_tool, worker);
}

//-----------------------------------------------------------------

void ToolOptionShortcutConnector::visit(TEnumProperty* p) {
  if (p->getId() == "") return;

  ToolOptionsShortcutInvoker* invoker = ToolOptionsShortcutInvoker::instance();
  EnumWorker* worker                  = new EnumWorker(invoker, m_tool, p);
  bool hasAction                      = false;
  std::string actionName              = "A_ToolOption_" + p->getId();
  QAction* a = CommandManager::instance()->getAction(actionName.c_str());
  if (a) {
    QObject::connect(a, SIGNAL(triggered()), worker, SLOT(cycleOptions()));
    hasAction = true;
  }

  TEnumProperty::Range range = p->getRange();
  TEnumProperty::Range::iterator it;
  QSignalMapper* signalMapper = 0;
  int index                   = 0;
  for (it = range.begin(); it != range.end(); ++it, ++index) {
    std::string item           = ::to_string(*it);
    std::string itemActionName = actionName + ":" + item;
    a = CommandManager::instance()->getAction(itemActionName.c_str());
    if (a) {
      if (signalMapper == 0) {
        signalMapper = new QSignalMapper(worker);
        QObject::connect(signalMapper, SIGNAL(mapped(int)), worker,
                         SLOT(doOnActivated(int)));
      }
      QObject::connect(a, SIGNAL(triggered()), signalMapper, SLOT(map()));
      signalMapper->setMapping(a, index);
      hasAction = true;
    }
  }

  if (!hasAction) delete worker;
}

//=============================================================================

void DoubleWorker::increase(double step) {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TDoubleProperty::Range range = m_property->getRange();
  double value                 = m_property->getValue();
  value += step;
  if (value > range.second) value = range.second;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void DoubleWorker::increaseFractional() { increase(0.06); }

//-----------------------------------------------------------------

void DoubleWorker::decrease(double step) {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TDoubleProperty::Range range = m_property->getRange();
  double value                 = m_property->getValue();
  value -= step;
  if (value < range.first) value = range.first;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void DoubleWorker::decreaseFractional() { decrease(0.06); }

//=============================================================================

void DoublePairWorker::increaseMaxValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TDoublePairProperty::Value value = m_property->getValue();
  TDoublePairProperty::Range range = m_property->getRange();
  value.second += 1;
  if (value.second > range.second) value.second = range.second;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void DoublePairWorker::decreaseMaxValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TDoublePairProperty::Value value = m_property->getValue();
  value.second -= 1;
  if (value.second < value.first) value.second = value.first;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void DoublePairWorker::increaseMinValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TDoublePairProperty::Value value = m_property->getValue();
  value.first += 1;
  if (value.first > value.second) value.first = value.second;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void DoublePairWorker::decreaseMinValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TDoublePairProperty::Value value = m_property->getValue();
  TDoublePairProperty::Range range = m_property->getRange();
  value.first -= 1;
  if (value.first < range.first) value.first = range.first;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================

void IntPairWorker::increaseMaxValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TIntPairProperty::Value value = m_property->getValue();
  TIntPairProperty::Range range = m_property->getRange();
  value.second += 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (value.second == 3 && m_tool->isPencilModeActive()) value.second += 1;

  if (value.second > range.second) value.second = range.second;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void IntPairWorker::decreaseMaxValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TIntPairProperty::Value value = m_property->getValue();
  value.second -= 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (value.second == 3 && m_tool->isPencilModeActive()) value.second -= 1;

  if (value.second < value.first) value.second = value.first;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void IntPairWorker::increaseMinValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TIntPairProperty::Value value = m_property->getValue();
  value.first += 1;
  if (value.first > value.second) value.first = value.second;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void IntPairWorker::decreaseMinValue() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TIntPairProperty::Value value = m_property->getValue();
  TIntPairProperty::Range range = m_property->getRange();
  value.first -= 1;
  if (value.first < range.first) value.first = range.first;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================

void IntWorker::increase() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TIntProperty::Range range = m_property->getRange();
  int value                 = m_property->getValue();
  value += 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (value == 3 && m_tool->isPencilModeActive()) value += 1;

  if (value > range.second) value = range.second;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void IntWorker::decrease() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  TIntProperty::Range range = m_property->getRange();
  double value              = m_property->getValue();
  value -= 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (value == 3 && m_tool->isPencilModeActive()) value -= 1;

  if (value < range.first) value = range.first;
  m_property->setValue(value);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================

void BoolWorker::doCheck(bool checked) {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;

  if (m_property->getValue() == checked) return;

  m_property->setValue(checked);
  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------

void BoolWorker::syncActionState() {
  if (m_action && m_action->isCheckable() &&
      m_action->isChecked() != m_property->getValue())
    m_action->setChecked(m_property->getValue());
}

//=============================================================================

void EnumWorker::cycleOptions() {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;
  const TEnumProperty::Range& range = m_property->getRange();
  int theIndex                      = m_property->getIndex() + 1;
  if (theIndex >= (int)range.size()) theIndex = 0;
  doOnActivated(theIndex);
}

//-----------------------------------------------------------------

void EnumWorker::doOnActivated(int index) {
  // ignore if it is not the current tool
  if (TApp::instance()->getCurrentTool()->getTool() != m_tool) return;

  // check range
  const TEnumProperty::Range& range = m_property->getRange();
  if (index < 0 || index >= (int)range.size()) return;

  // Just move the index if the first item is not "Normal"
  if (m_property->indexOf(L"Normal") != 0) {
    m_property->setIndex(index);
    ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
    TApp::instance()->getCurrentTool()->notifyToolChanged();
    return;
  }

  // If the first item of this combo box is "Normal", enable shortcut key toggle
  // can "back and forth" behavior.
  if (m_property->getIndex() == index) {
    // estimating that the "Normal" option is located at the index 0
    m_property->setIndex(0);
  } else {
    m_property->setIndex(index);
  }

  ToolOptionsShortcutInvoker::instance()->notifyTool(m_tool, m_property);
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================

ToolOptionsShortcutInvoker* ToolOptionsShortcutInvoker::instance() {
  static ToolOptionsShortcutInvoker _instance;
  return &_instance;
}

//-----------------------------------------------------------------

void ToolOptionsShortcutInvoker::initialize() {
  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
          SLOT(onToolSwitched()));
  onToolSwitched();

  /*-- Animate tool + mode switching shortcuts --*/
  setCommandHandler(MI_EditNextMode, this,
                    &ToolOptionsShortcutInvoker::toggleEditNextMode);
  setCommandHandler(MI_EditPosition, this,
                    &ToolOptionsShortcutInvoker::toggleEditPosition);
  setCommandHandler(MI_EditRotation, this,
                    &ToolOptionsShortcutInvoker::toggleEditRotation);
  setCommandHandler(MI_EditScale, this,
                    &ToolOptionsShortcutInvoker::toggleEditNextScale);
  setCommandHandler(MI_EditShear, this,
                    &ToolOptionsShortcutInvoker::toggleEditNextShear);
  setCommandHandler(MI_EditCenter, this,
                    &ToolOptionsShortcutInvoker::toggleEditNextCenter);
  setCommandHandler(MI_EditAll, this,
                    &ToolOptionsShortcutInvoker::toggleEditNextAll);

  /*-- Selection tool + type switching shortcuts --*/
  setCommandHandler(MI_SelectionNextType, this,
                    &ToolOptionsShortcutInvoker::toggleSelectionNextType);
  setCommandHandler(MI_SelectionRectangular, this,
                    &ToolOptionsShortcutInvoker::toggleSelectionRectangular);
  setCommandHandler(MI_SelectionFreehand, this,
                    &ToolOptionsShortcutInvoker::toggleSelectionFreehand);
  setCommandHandler(MI_SelectionPolyline, this,
                    &ToolOptionsShortcutInvoker::toggleSelectionPolyline);

  /*-- Geometric tool + shape switching shortcuts --*/
  setCommandHandler(MI_GeometricNextShape, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricNextShape);
  setCommandHandler(MI_GeometricRectangle, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricRectangle);
  setCommandHandler(MI_GeometricCircle, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricCircle);
  setCommandHandler(MI_GeometricEllipse, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricEllipse);
  setCommandHandler(MI_GeometricLine, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricLine);
  setCommandHandler(MI_GeometricPolyline, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricPolyline);
  setCommandHandler(MI_GeometricArc, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricArc);
  setCommandHandler(MI_GeometricMultiArc, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricMultiArc);
  setCommandHandler(MI_GeometricPolygon, this,
                    &ToolOptionsShortcutInvoker::toggleGeometricPolygon);

  /*-- Type tool + style switching shortcuts --*/
  setCommandHandler(MI_TypeNextStyle, this,
                    &ToolOptionsShortcutInvoker::toggleTypeNextStyle);
  setCommandHandler(MI_TypeOblique, this,
                    &ToolOptionsShortcutInvoker::toggleTypeOblique);
  setCommandHandler(MI_TypeRegular, this,
                    &ToolOptionsShortcutInvoker::toggleTypeRegular);
  setCommandHandler(MI_TypeBoldOblique, this,
                    &ToolOptionsShortcutInvoker::toggleTypeBoldOblique);
  setCommandHandler(MI_TypeBold, this,
                    &ToolOptionsShortcutInvoker::toggleTypeBold);

  /*-- Paint Brush tool + mode switching shortcuts --*/
  setCommandHandler(MI_PaintBrushNextMode, this,
                    &ToolOptionsShortcutInvoker::togglePaintBrushNextMode);
  setCommandHandler(MI_PaintBrushAreas, this,
                    &ToolOptionsShortcutInvoker::togglePaintBrushAreas);
  setCommandHandler(MI_PaintBrushLines, this,
                    &ToolOptionsShortcutInvoker::togglePaintBrushLines);
  setCommandHandler(MI_PaintBrushLinesAndAreas, this,
                    &ToolOptionsShortcutInvoker::togglePaintBrushLinesAndAreas);

  /*-- Fill tool + type/mode switching shortcuts --*/
  setCommandHandler(MI_FillNextType, this,
                    &ToolOptionsShortcutInvoker::toggleFillNextType);
  setCommandHandler(MI_FillNormal, this,
                    &ToolOptionsShortcutInvoker::toggleFillNormal);
  setCommandHandler(MI_FillRectangular, this,
                    &ToolOptionsShortcutInvoker::toggleFillRectangular);
  setCommandHandler(MI_FillFreehand, this,
                    &ToolOptionsShortcutInvoker::toggleFillFreehand);
  setCommandHandler(MI_FillPolyline, this,
                    &ToolOptionsShortcutInvoker::toggleFillPolyline);
  setCommandHandler(MI_FillFreepick, this,
                    &ToolOptionsShortcutInvoker::toggleFillFreepick);
  setCommandHandler(MI_FillNextMode, this,
                    &ToolOptionsShortcutInvoker::toggleFillNextMode);
  setCommandHandler(MI_FillAreas, this,
                    &ToolOptionsShortcutInvoker::toggleFillAreas);
  setCommandHandler(MI_FillLines, this,
                    &ToolOptionsShortcutInvoker::toggleFillLines);
  setCommandHandler(MI_FillLinesAndAreas, this,
                    &ToolOptionsShortcutInvoker::toggleFillLinesAndAreas);

  /*-- Eraser tool + type switching shortcuts --*/
  setCommandHandler(MI_EraserNextType, this,
                    &ToolOptionsShortcutInvoker::toggleEraserNextType);
  setCommandHandler(MI_EraserNormal, this,
                    &ToolOptionsShortcutInvoker::toggleEraserNormal);
  setCommandHandler(MI_EraserRectangular, this,
                    &ToolOptionsShortcutInvoker::toggleEraserRectangular);
  setCommandHandler(MI_EraserFreehand, this,
                    &ToolOptionsShortcutInvoker::toggleEraserFreehand);
  setCommandHandler(MI_EraserPolyline, this,
                    &ToolOptionsShortcutInvoker::toggleEraserPolyline);
  setCommandHandler(MI_EraserSegment, this,
                    &ToolOptionsShortcutInvoker::toggleEraserSegment);

  /*-- Tape tool + type/mode switching shortcuts --*/
  setCommandHandler(MI_TapeNextType, this,
                    &ToolOptionsShortcutInvoker::toggleTapeNextType);
  setCommandHandler(MI_TapeNormal, this,
                    &ToolOptionsShortcutInvoker::toggleTapeNormal);
  setCommandHandler(MI_TapeRectangular, this,
                    &ToolOptionsShortcutInvoker::toggleTapeRectangular);
  setCommandHandler(MI_TapeNextMode, this,
                    &ToolOptionsShortcutInvoker::toggleTapeNextMode);
  setCommandHandler(MI_TapeEndpointToEndpoint, this,
                    &ToolOptionsShortcutInvoker::toggleTapeEndpointToEndpoint);
  setCommandHandler(MI_TapeEndpointToLine, this,
                    &ToolOptionsShortcutInvoker::toggleTapeEndpointToLine);
  setCommandHandler(MI_TapeLineToLine, this,
                    &ToolOptionsShortcutInvoker::toggleTapeLineToLine);

  /*-- Style Picker tool + mode switching shortcuts --*/
  setCommandHandler(MI_PickStyleNextMode, this,
                    &ToolOptionsShortcutInvoker::togglePickStyleNextMode);
  setCommandHandler(MI_PickStyleAreas, this,
                    &ToolOptionsShortcutInvoker::togglePickStyleAreas);
  setCommandHandler(MI_PickStyleLines, this,
                    &ToolOptionsShortcutInvoker::togglePickStyleLines);
  setCommandHandler(MI_PickStyleLinesAndAreas, this,
                    &ToolOptionsShortcutInvoker::togglePickStyleLinesAndAreas);

  /*-- RGB Picker tool + type switching shortcuts --*/
  setCommandHandler(MI_RGBPickerNextType, this,
                    &ToolOptionsShortcutInvoker::toggleRGBPickerNextType);
  setCommandHandler(MI_RGBPickerNormal, this,
                    &ToolOptionsShortcutInvoker::toggleRGBPickerNormal);
  setCommandHandler(MI_RGBPickerRectangular, this,
                    &ToolOptionsShortcutInvoker::toggleRGBPickerRectangular);
  setCommandHandler(MI_RGBPickerFreehand, this,
                    &ToolOptionsShortcutInvoker::toggleRGBPickerFreehand);
  setCommandHandler(MI_RGBPickerPolyline, this,
                    &ToolOptionsShortcutInvoker::toggleRGBPickerPolyline);

  /*-- Skeleton tool + mode switching shortcuts --*/
  setCommandHandler(MI_SkeletonNextMode, this,
                    &ToolOptionsShortcutInvoker::ToggleSkeletonNextMode);
  setCommandHandler(MI_SkeletonBuildSkeleton, this,
                    &ToolOptionsShortcutInvoker::ToggleSkeletonBuildSkeleton);
  setCommandHandler(MI_SkeletonAnimate, this,
                    &ToolOptionsShortcutInvoker::ToggleSkeletonAnimate);
  setCommandHandler(
      MI_SkeletonInverseKinematics, this,
      &ToolOptionsShortcutInvoker::ToggleSkeletonInverseKinematics);

  /*-- Plastic tool + mode switching shortcuts --*/
  setCommandHandler(MI_PlasticNextMode, this,
                    &ToolOptionsShortcutInvoker::TogglePlasticNextMode);
  setCommandHandler(MI_PlasticEditMesh, this,
                    &ToolOptionsShortcutInvoker::TogglePlasticEditMesh);
  setCommandHandler(MI_PlasticPaintRigid, this,
                    &ToolOptionsShortcutInvoker::TogglePlasticPaintRigid);
  setCommandHandler(MI_PlasticBuildSkeleton, this,
                    &ToolOptionsShortcutInvoker::TogglePlasticBuildSkeleton);
  setCommandHandler(MI_PlasticAnimate, this,
                    &ToolOptionsShortcutInvoker::TogglePlasticAnimate);
}

//-----------------------------------------------------------------------------

void ToolOptionsShortcutInvoker::notifyTool(TTool* tool, TProperty* p,
                                            bool addToUndo) {
  std::string tempPropertyName = p->getName();
  if (addToUndo && tempPropertyName == "Maximum Gap")
    tempPropertyName = tempPropertyName + "withUndo";
  tool->onPropertyChanged(tempPropertyName);
}

//-----------------------------------------------------------------------------

void ToolOptionsShortcutInvoker::registerCheckProperty(TTool* tool,
                                                       BoolWorker* worker) {
  m_checkProps.insert(tool, worker);
}

//-----------------------------------------------------------------------------

void ToolOptionsShortcutInvoker::onToolSwitched() {
  TTool* tool = TApp::instance()->getCurrentTool()->getTool();
  if (!m_tools.contains(tool)) {
    // amount of the property groups
    int pgCount = 1;
    if (tool->getName() == T_Geometric || tool->getName() == T_Type ||
        (tool->getName() == T_Selection &&
         (tool->getTargetType() & TTool::Vectors)) ||
        (tool->getName() == T_Brush &&
         (tool->getTargetType() & TTool::Vectors)))
      pgCount = 2;
    for (int pgId = 0; pgId < pgCount; pgId++) {
      TPropertyGroup* pg = tool->getProperties(pgId);
      if (!pg) continue;

      ToolOptionShortcutConnector connector(tool);
      // connect each property to an associated command
      pg->accept(connector);
    }
    m_tools.insert(tool);
  }
  // if the tool is already registered
  else {
    // synchronize action's check state to the property
    for (auto boolWorker : m_checkProps.values(tool)) {
      boolWorker->syncActionState();
    }
  }
}

//-----------------------------------------------------------------------------
/*-- Animate tool + mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleEditNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Edit)
    CommandManager::instance()
        ->getAction("A_ToolOption_EditToolActiveAxis")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Edit)->trigger();
}

void ToolOptionsShortcutInvoker::toggleEditPosition() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Position")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEditRotation() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Rotation")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEditNextScale() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Scale")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEditNextShear() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Shear")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEditNextCenter() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Center")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEditNextAll() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:All")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Selection tool + type switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleSelectionNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Selection)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Selection)->trigger();
}

void ToolOptionsShortcutInvoker::toggleSelectionRectangular() {
  CommandManager::instance()->getAction(T_Selection)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleSelectionFreehand() {
  CommandManager::instance()->getAction(T_Selection)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleSelectionPolyline() {
  CommandManager::instance()->getAction(T_Selection)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Geometric tool + shape switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleGeometricNextShape() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Geometric)
    CommandManager::instance()
        ->getAction("A_ToolOption_GeometricShape")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Geometric)->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricRectangle() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Rectangle")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricCircle() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Circle")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricEllipse() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Ellipse")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricLine() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Line")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricPolyline() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Polyline")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricArc() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Arc")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricMultiArc() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:MultiArc")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleGeometricPolygon() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Polygon")
      ->trigger();
}
//---------------------------------------------------------------------------------------
/*-- Type tool + mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleTypeNextStyle() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Type)
    CommandManager::instance()->getAction("A_ToolOption_TypeStyle")->trigger();
  else
    CommandManager::instance()->getAction(T_Type)->trigger();
}

void ToolOptionsShortcutInvoker::toggleTypeOblique() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Oblique")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleTypeRegular() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Regular")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleTypeBoldOblique() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Bold Oblique")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleTypeBold() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Bold")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Fill tool + type/mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleFillNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Fill)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Fill)->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillNormal() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillRectangular() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillFreehand() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillPolyline() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillFreepick() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freepick")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Fill)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_Fill)->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillAreas() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillLines() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

void ToolOptionsShortcutInvoker::toggleFillLinesAndAreas() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Lines & Areas")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Paint Brush tool + mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::togglePaintBrushNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_PaintBrush)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_PaintBrush)->trigger();
}
void ToolOptionsShortcutInvoker::togglePaintBrushAreas() {
  CommandManager::instance()->getAction(T_PaintBrush)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void ToolOptionsShortcutInvoker::togglePaintBrushLines() {
  CommandManager::instance()->getAction(T_PaintBrush)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

void ToolOptionsShortcutInvoker::togglePaintBrushLinesAndAreas() {
  CommandManager::instance()->getAction(T_PaintBrush)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Lines & Areas")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Eraser tool + type switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleEraserNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Eraser)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Eraser)->trigger();
}

void ToolOptionsShortcutInvoker::toggleEraserNormal() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void ToolOptionsShortcutInvoker::toggleEraserRectangular() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEraserFreehand() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEraserPolyline() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleEraserSegment() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Segment")->trigger();
}
//---------------------------------------------------------------------------------------
/*-- Tape tool + type/mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleTapeNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Tape)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Tape)->trigger();
}

void ToolOptionsShortcutInvoker::toggleTapeNormal() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void ToolOptionsShortcutInvoker::toggleTapeRectangular() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleTapeNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Tape)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_Tape)->trigger();
}

void ToolOptionsShortcutInvoker::toggleTapeEndpointToEndpoint() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Endpoint to Endpoint")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleTapeEndpointToLine() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Endpoint to Line")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleTapeLineToLine() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Line to Line")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Style Picker tool + mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::togglePickStyleNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_StylePicker)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_StylePicker)->trigger();
}

void ToolOptionsShortcutInvoker::togglePickStyleAreas() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void ToolOptionsShortcutInvoker::togglePickStyleLines() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

void ToolOptionsShortcutInvoker::togglePickStyleLinesAndAreas() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Lines & Areas")
      ->trigger();
}
//-----------------------------------------------------------------------------
/*-- RGB Picker tool + type switching shortcuts --*/
void ToolOptionsShortcutInvoker::toggleRGBPickerNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_RGBPicker)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_RGBPicker)->trigger();
}

void ToolOptionsShortcutInvoker::toggleRGBPickerNormal() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void ToolOptionsShortcutInvoker::toggleRGBPickerRectangular() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleRGBPickerFreehand() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void ToolOptionsShortcutInvoker::toggleRGBPickerPolyline() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}
//-----------------------------------------------------------------------------
/*-- Skeleton tool + type switching shortcuts --*/
void ToolOptionsShortcutInvoker::ToggleSkeletonNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Skeleton)
    CommandManager::instance()
        ->getAction("A_ToolOption_SkeletonMode")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Skeleton)->trigger();
}

void ToolOptionsShortcutInvoker::ToggleSkeletonBuildSkeleton() {
  CommandManager::instance()->getAction(T_Skeleton)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Build Skeleton")
      ->trigger();
}

void ToolOptionsShortcutInvoker::ToggleSkeletonAnimate() {
  CommandManager::instance()->getAction(T_Skeleton)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Animate")
      ->trigger();
}

void ToolOptionsShortcutInvoker::ToggleSkeletonInverseKinematics() {
  CommandManager::instance()->getAction(T_Skeleton)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Inverse Kinematics")
      ->trigger();
}

//-----------------------------------------------------------------------------
/*-- Plastic tool + mode switching shortcuts --*/
void ToolOptionsShortcutInvoker::TogglePlasticNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Plastic)
    CommandManager::instance()
        ->getAction("A_ToolOption_SkeletonMode")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Plastic)->trigger();
}

void ToolOptionsShortcutInvoker::TogglePlasticEditMesh() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Edit Mesh")
      ->trigger();
}

void ToolOptionsShortcutInvoker::TogglePlasticPaintRigid() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Paint Rigid")
      ->trigger();
}

void ToolOptionsShortcutInvoker::TogglePlasticBuildSkeleton() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Build Skeleton")
      ->trigger();
}

void ToolOptionsShortcutInvoker::TogglePlasticAnimate() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Animate")
      ->trigger();
}
