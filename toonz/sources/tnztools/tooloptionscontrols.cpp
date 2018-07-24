

// TnzCore includes
#include "tpalette.h"
#include "tcolorstyles.h"
#include "tundo.h"

// TnzBase includes
#include "tproperty.h"

// TnzLib includes
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/stage2.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzTools includes
#include "tools/tool.h"
#include "rasterselectiontool.h"
#include "vectorselectiontool.h"
// to enable the increase/decrease shortcuts while hiding the tool option
#include "tools/toolhandle.h"
// to enable shortcuts only when the viewer is focused
#include "tools/tooloptions.h"

// Qt includes
#include <QPainter>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QButtonGroup>
#include <QMenu>
#include <QListView>

#include "tooloptionscontrols.h"

using namespace DVGui;

//***********************************************************************************
//    ToolOptionControl  implementation
//***********************************************************************************

ToolOptionControl::ToolOptionControl(TTool *tool, std::string propertyName,
                                     ToolHandle *toolHandle)
    : m_tool(tool), m_propertyName(propertyName), m_toolHandle(toolHandle) {}

//-----------------------------------------------------------------------------

void ToolOptionControl::notifyTool(bool addToUndo) {
  std::string tempPropertyName = m_propertyName;
  if (addToUndo && m_propertyName == "Maximum Gap")
    tempPropertyName = tempPropertyName + "withUndo";
  m_tool->onPropertyChanged(tempPropertyName);
}

//-----------------------------------------------------------------------------
/*! return true if the control is belonging to the visible combo viewer. very
 * dirty implementation.
 */
bool ToolOptionControl::isInVisibleViewer(QWidget *widget) {
  if (!widget) return false;

  if (widget->isVisible()) return true;

  ToolOptionsBox *parentTOB =
      dynamic_cast<ToolOptionsBox *>(widget->parentWidget());
  if (!parentTOB) return false;

  ToolOptions *grandParentTO =
      dynamic_cast<ToolOptions *>(parentTOB->parentWidget());
  if (!grandParentTO) return false;

  // There must be a QFrame between a ComboViewerPanel and a ToolOptions
  QFrame *greatGrandParentFrame =
      dynamic_cast<QFrame *>(grandParentTO->parentWidget());
  if (!greatGrandParentFrame) return false;

  return greatGrandParentFrame->isVisible();
}

//***********************************************************************************
//    ToolOptionControl derivative  implementations
//***********************************************************************************

ToolOptionCheckbox::ToolOptionCheckbox(TTool *tool, TBoolProperty *property,
                                       ToolHandle *toolHandle, QWidget *parent)
    : CheckBox(parent)
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  setText(property->getQStringName());
  m_property->addListener(this);
  updateStatus();
  // synchronize the state with the same widgets in other tool option bars
  if (toolHandle)
    connect(this, SIGNAL(clicked(bool)), toolHandle, SIGNAL(toolChanged()));
}

//-----------------------------------------------------------------------------

void ToolOptionCheckbox::updateStatus() {
  bool check = m_property->getValue();

  if (!actions().isEmpty() && actions()[0]->isCheckable() &&
      actions()[0]->isChecked() != check)
    actions()[0]->setChecked(check);

  if (isChecked() == check) return;

  setCheckState(check ? Qt::Checked : Qt::Unchecked);
}

//-----------------------------------------------------------------------------

void ToolOptionCheckbox::nextCheckState() {
  QAbstractButton::nextCheckState();
  m_property->setValue(checkState() == Qt::Checked);
  notifyTool();
}

//-----------------------------------------------------------------------------

void ToolOptionCheckbox::doClick(bool checked) {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  if (isChecked() == checked) return;

  setChecked(checked);
  m_property->setValue(checked);
  notifyTool();

  // for updating a cursor without any effect to the tool options
  m_toolHandle->notifyToolCursorTypeChanged();
}

//=============================================================================

ToolOptionSlider::ToolOptionSlider(TTool *tool, TDoubleProperty *property,
                                   ToolHandle *toolHandle)
    : DoubleField()
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  m_property->addListener(this);
  TDoubleProperty::Range range = property->getRange();
  setRange(range.first, range.second);

  // calculate maximum text length which includes length for decimals (for now
  // it's fixed to 2) and period
  int textMaxLength = std::max(QString::number((int)range.first).length(),
                               QString::number((int)range.second).length()) +
                      m_lineEdit->getDecimals() + 1;
  QString txt;
  // set the maximum width of the widget according to the text length (with 5
  // pixels margin)
  txt.fill('0', textMaxLength);
  int widgetWidth = fontMetrics().width(txt) + 5;
  m_lineEdit->parentWidget()->setMaximumWidth(widgetWidth);
  // set the maximum width of the slider to 250 pixels
  setMaximumWidth(250 + widgetWidth);

  updateStatus();
  connect(this, SIGNAL(valueChanged(bool)), SLOT(onValueChanged(bool)));
  // synchronize the state with the same widgets in other tool option bars
  if (toolHandle)
    connect(this, SIGNAL(valueEditedByHand()), toolHandle,
            SIGNAL(toolChanged()));
}

//-----------------------------------------------------------------------------

void ToolOptionSlider::updateStatus() {
  double v = m_property->getValue();
  if (getValue() == v) return;

  setValue(v);
}

//-----------------------------------------------------------------------------

void ToolOptionSlider::onValueChanged(bool isDragging) {
  m_property->setValue(getValue());
  notifyTool(!isDragging);
}

//-----------------------------------------------------------------------------

void ToolOptionSlider::increase() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  double value = getValue();
  double minValue, maxValue;
  getRange(minValue, maxValue);

  value += 1;
  if (value > maxValue) value = maxValue;

  setValue(value);
  m_property->setValue(getValue());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionSlider::decrease() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  double value = getValue();
  double minValue, maxValue;
  getRange(minValue, maxValue);

  value -= 1;
  if (value < minValue) value = minValue;

  setValue(value);
  m_property->setValue(getValue());
  notifyTool();
  // update the interface
  repaint();
}

//=============================================================================

ToolOptionPairSlider::ToolOptionPairSlider(TTool *tool,
                                           TDoublePairProperty *property,
                                           const QString &leftName,
                                           const QString &rightName,
                                           ToolHandle *toolHandle)
    : DoublePairField(0, property->isMaxRangeLimited())
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  m_property->addListener(this);
  TDoublePairProperty::Value value = property->getValue();
  TDoublePairProperty::Range range = property->getRange();
  setRange(range.first, range.second);

  // calculate maximum text length which includes length for decimals (for now
  // it's fixed to 2) and period
  int textMaxLength = std::max(QString::number((int)range.first).length(),
                               QString::number((int)range.second).length()) +
                      m_leftLineEdit->getDecimals() + 1;
  QString txt;
  // set the maximum width of the widget according to the text length (with 5
  // pixels margin)
  txt.fill('0', textMaxLength);
  int widgetWidth = fontMetrics().width(txt) + 5;
  m_leftLineEdit->setFixedWidth(widgetWidth);
  m_rightLineEdit->setFixedWidth(widgetWidth);
  m_leftMargin  = widgetWidth + 12;
  m_rightMargin = widgetWidth + 12;
  // set the maximum width of the slider to 300 pixels
  setMaximumWidth(300 + m_leftMargin + m_rightMargin);

  setLeftText(leftName);
  setRightText(rightName);

  updateStatus();
  connect(this, SIGNAL(valuesChanged(bool)), SLOT(onValuesChanged(bool)));
}

//-----------------------------------------------------------------------------

void ToolOptionPairSlider::updateStatus() {
  TDoublePairProperty::Value value = m_property->getValue();
  setValues(value);
}

//-----------------------------------------------------------------------------

void ToolOptionPairSlider::onValuesChanged(bool isDragging) {
  m_property->setValue(getValues());
  notifyTool();
  // synchronize the state with the same widgets in other tool option bars
  if (m_toolHandle) m_toolHandle->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void ToolOptionPairSlider::increaseMaxValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<double, double> values = getValues();
  double minValue, maxValue;
  getRange(minValue, maxValue);
  values.second += 1;
  if (values.second > maxValue) values.second = maxValue;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionPairSlider::decreaseMaxValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<double, double> values = getValues();
  values.second -= 1;
  if (values.second < values.first) values.second = values.first;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionPairSlider::increaseMinValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<double, double> values = getValues();
  values.first += 1;
  if (values.first > values.second) values.first = values.second;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionPairSlider::decreaseMinValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<double, double> values = getValues();
  double minValue, maxValue;
  getRange(minValue, maxValue);
  values.first -= 1;
  if (values.first < minValue) values.first = minValue;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//=============================================================================

ToolOptionIntPairSlider::ToolOptionIntPairSlider(TTool *tool,
                                                 TIntPairProperty *property,
                                                 const QString &leftName,
                                                 const QString &rightName,
                                                 ToolHandle *toolHandle)
    : IntPairField(0, property->isMaxRangeLimited())
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  setLeftText(leftName);
  setRightText(rightName);
  m_property->addListener(this);
  TIntPairProperty::Value value = property->getValue();
  TIntPairProperty::Range range = property->getRange();
  setRange(range.first, range.second);
  setMaximumWidth(300);
  updateStatus();
  connect(this, SIGNAL(valuesChanged(bool)), SLOT(onValuesChanged(bool)));
}

//-----------------------------------------------------------------------------

void ToolOptionIntPairSlider::updateStatus() {
  TIntPairProperty::Value value = m_property->getValue();
  setValues(value);
}

//-----------------------------------------------------------------------------

void ToolOptionIntPairSlider::onValuesChanged(bool isDragging) {
  m_property->setValue(getValues());
  notifyTool();
  // synchronize the state with the same widgets in other tool option bars
  if (m_toolHandle) m_toolHandle->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void ToolOptionIntPairSlider::increaseMaxValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<int, int> values = getValues();
  int minValue, maxValue;
  getRange(minValue, maxValue);
  values.second += 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (values.second == 3 && m_tool->isPencilModeActive()) values.second += 1;

  if (values.second > maxValue) values.second = maxValue;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionIntPairSlider::decreaseMaxValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<int, int> values = getValues();
  values.second -= 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (values.second == 3 && m_tool->isPencilModeActive()) values.second -= 1;

  if (values.second < values.first) values.second = values.first;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionIntPairSlider::increaseMinValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<int, int> values = getValues();
  values.first += 1;
  if (values.first > values.second) values.first = values.second;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionIntPairSlider::decreaseMinValue() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  std::pair<int, int> values = getValues();
  int minValue, maxValue;
  getRange(minValue, maxValue);
  values.first -= 1;
  if (values.first < minValue) values.first = minValue;
  setValues(values);
  m_property->setValue(getValues());
  notifyTool();
  // update the interface
  repaint();
}

//=============================================================================

ToolOptionIntSlider::ToolOptionIntSlider(TTool *tool, TIntProperty *property,
                                         ToolHandle *toolHandle)
    : IntField(0, property->isMaxRangeLimited())
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  m_property->addListener(this);
  TIntProperty::Range range = property->getRange();
  setRange(range.first, range.second);
  setMaximumWidth(300);
  updateStatus();
  connect(this, SIGNAL(valueChanged(bool)), SLOT(onValueChanged(bool)));
  // synchronize the state with the same widgets in other tool option bars
  if (toolHandle)
    connect(this, SIGNAL(valueEditedByHand()), toolHandle,
            SIGNAL(toolChanged()));
}

//-----------------------------------------------------------------------------

void ToolOptionIntSlider::updateStatus() {
  int v = m_property->getValue();
  if (getValue() == v) return;

  setValue(v);
}

//-----------------------------------------------------------------------------

void ToolOptionIntSlider::increase() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  int value = getValue();
  int minValue, maxValue;
  getRange(minValue, maxValue);
  value += 1;
  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (value == 3 && m_tool->isPencilModeActive()) value += 1;

  if (value > maxValue) value = maxValue;

  setValue(value);
  m_property->setValue(getValue());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionIntSlider::decrease() {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;

  int value = getValue();
  int minValue, maxValue;
  getRange(minValue, maxValue);
  value -= 1;

  // a "cross-like shape" of the brush size = 3 is hard to use. so skip it
  if (value == 3 && m_tool->isPencilModeActive()) value -= 1;

  if (value < minValue) value = minValue;

  setValue(value);
  m_property->setValue(getValue());
  notifyTool();
  // update the interface
  repaint();
}

//-----------------------------------------------------------------------------

void ToolOptionIntSlider::onValueChanged(bool isDragging) {
  m_property->setValue(getValue());
  notifyTool();
}

//=============================================================================

ToolOptionCombo::ToolOptionCombo(TTool *tool, TEnumProperty *property,
                                 ToolHandle *toolHandle)
    : QComboBox()
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  setMinimumWidth(65);
  m_property->addListener(this);
  loadEntries();
  setSizeAdjustPolicy(QComboBox::AdjustToContents);
  connect(this, SIGNAL(activated(int)), this, SLOT(onActivated(int)));
  // synchronize the state with the same widgets in other tool option bars
  if (toolHandle) {
    connect(this, SIGNAL(activated(int)), toolHandle, SIGNAL(toolChanged()));
  }
}

//-----------------------------------------------------------------------------

void ToolOptionCombo::reloadComboBoxList(std::string id) {
  if (id == "" || m_property->getName() != id) return;
  loadEntries();
}

//-----------------------------------------------------------------------------

void ToolOptionCombo::loadEntries() {
  const TEnumProperty::Range &range = m_property->getRange();
  const TEnumProperty::Items &items = m_property->getItems();

  const int count = m_property->getCount();
  int maxWidth    = 0;

  clear();
  bool hasIcon = false;
  for (int i = 0; i < count; ++i) {
    QString itemStr = QString::fromStdWString(range[i]);
    if (items[i].iconName.isEmpty())
      addItem(items[i].UIName, itemStr);
    else {
      addItem(createQIcon(items[i].iconName.toUtf8()), items[i].UIName,
              itemStr);
      if (!hasIcon) {
        hasIcon = true;
        setIconSize(QSize(17, 17));
        // add margin between items if they are with icons
        setView(new QListView());
        view()->setIconSize(QSize(17, 17));
        setStyleSheet(
            "QComboBox  QAbstractItemView::item{ \
                       margin: 5 0 0 0;\
                      }");
      }
    }
    int tmpWidth                      = fontMetrics().width(items[i].UIName);
    if (tmpWidth > maxWidth) maxWidth = tmpWidth;
  }

  // set the maximum width according to the longest item with 25 pixels for
  // arrow button and margin
  setMaximumWidth(maxWidth + 25 + (hasIcon ? 23 : 0));

  updateStatus();
}

//-----------------------------------------------------------------------------

void ToolOptionCombo::updateStatus() {
  QString value = QString::fromStdWString(m_property->getValue());
  int index     = findData(value);
  if (index >= 0 && index != currentIndex()) setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void ToolOptionCombo::onActivated(int index) {
  const TEnumProperty::Range &range = m_property->getRange();
  if (index < 0 || index >= (int)range.size()) return;

  std::wstring item = range[index];
  m_property->setValue(item);
  notifyTool();
}

//-----------------------------------------------------------------------------

void ToolOptionCombo::doShowPopup() {
  if (Preferences::instance()->getDropdownShortcutsCycleOptions()) {
    const TEnumProperty::Range &range           = m_property->getRange();
    int theIndex                                = currentIndex() + 1;
    if (theIndex >= (int)range.size()) theIndex = 0;
    doOnActivated(theIndex);
  } else {
    if (isVisible()) showPopup();
  }
}

//-----------------------------------------------------------------------------

void ToolOptionCombo::doOnActivated(int index) {
  if (m_toolHandle && m_toolHandle->getTool() != m_tool) return;
  // active only if the belonging combo-viewer is visible
  if (!isInVisibleViewer(this)) return;
  bool cycleOptions =
      Preferences::instance()->getDropdownShortcutsCycleOptions();
  // Just move the index if the first item is not "Normal"
  if (m_property->indexOf(L"Normal") != 0) {
    onActivated(index);
    setCurrentIndex(index);
    // for updating the cursor
    m_toolHandle->notifyToolChanged();
    return;
  }

  // If the first item of this combo box is "Normal", enable shortcut key toggle
  // can "back and forth" behavior.
  if (currentIndex() == index) {
    // estimating that the "Normal" option is located at the index 0
    onActivated(0);
    setCurrentIndex(0);
  } else {
    onActivated(index);
    setCurrentIndex(index);
  }

  // for updating a cursor without any effect to the tool options
  m_toolHandle->notifyToolCursorTypeChanged();
}

//=============================================================================

ToolOptionFontCombo::ToolOptionFontCombo(TTool *tool, TEnumProperty *property,
                                         ToolHandle *toolHandle)
    : QFontComboBox()
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property) {
  setMaximumWidth(250);
  m_property->addListener(this);
  setSizeAdjustPolicy(QFontComboBox::AdjustToContents);
  connect(this, SIGNAL(activated(int)), this, SLOT(onActivated(int)));
  // synchronize the state with the same widgets in other tool option bars
  if (toolHandle)
    connect(this, SIGNAL(activated(int)), toolHandle, SIGNAL(toolChanged()));

  updateStatus();
}

//-----------------------------------------------------------------------------

void ToolOptionFontCombo::updateStatus() {
  QString value = QString::fromStdWString(m_property->getValue());
  int index     = findText(value);
  if (index >= 0 && index != currentIndex()) setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void ToolOptionFontCombo::onActivated(int index) {
  const TEnumProperty::Range &range = m_property->getRange();
  if (index < 0 || index >= (int)range.size()) return;

  std::wstring item = range[index];
  m_property->setValue(item);
  notifyTool();
}

//-----------------------------------------------------------------------------

void ToolOptionFontCombo::doShowPopup() {
  if (!isInVisibleViewer(this)) return;
  if (Preferences::instance()->getDropdownShortcutsCycleOptions()) {
    const TEnumProperty::Range &range           = m_property->getRange();
    int theIndex                                = currentIndex() + 1;
    if (theIndex >= (int)range.size()) theIndex = 0;
    onActivated(theIndex);
    setCurrentIndex(theIndex);
  } else {
    if (isVisible()) showPopup();
  }
}

//=============================================================================

ToolOptionPopupButton::ToolOptionPopupButton(TTool *tool,
                                             TEnumProperty *property)
    : PopupButton()
    , ToolOptionControl(tool, property->getName())
    , m_property(property) {
  setObjectName(QString::fromStdString(property->getName()));
  setFixedHeight(20);
  m_property->addListener(this);

  const TEnumProperty::Items &items = m_property->getItems();
  const int count                   = m_property->getCount();
  for (int i = 0; i < count; ++i) {
    QAction *action = addItem(createQIcon(items[i].iconName.toUtf8()));
    // make the tooltip text
    action->setToolTip(items[i].UIName);
  }
  setCurrentIndex(0);
  updateStatus();
  connect(this, SIGNAL(activated(int)), this, SLOT(onActivated(int)));
}

//-----------------------------------------------------------------------------

void ToolOptionPopupButton::updateStatus() {
  int index = m_property->getIndex();
  if (index >= 0 && index != currentIndex()) setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void ToolOptionPopupButton::onActivated(int index) {
  const TEnumProperty::Range &range = m_property->getRange();
  if (index < 0 || index >= (int)range.size()) return;

  std::wstring item = range[index];
  m_property->setValue(item);
  notifyTool();
}

//-----------------------------------------------------------------------------

void ToolOptionPopupButton::doShowPopup() {
  if (isVisible()) showMenu();
}

//-----------------------------------------------------------------------------

void ToolOptionPopupButton::doSetCurrentIndex(int index) {
  if (isVisible()) setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void ToolOptionPopupButton::doOnActivated(int index) {
  if (isVisible()) onActivated(index);
}

//=============================================================================

ToolOptionTextField::ToolOptionTextField(TTool *tool, TStringProperty *property)
    : LineEdit()
    , ToolOptionControl(tool, property->getName())
    , m_property(property) {
  setFixedSize(100, 23);
  m_property->addListener(this);

  updateStatus();
  connect(this, SIGNAL(editingFinished()), SLOT(onValueChanged()));
}

//-----------------------------------------------------------------------------

void ToolOptionTextField::updateStatus() {
  QString newText = QString::fromStdWString(m_property->getValue());
  if (newText == text()) return;

  setText(newText);
}

//-----------------------------------------------------------------------------

void ToolOptionTextField::onValueChanged() {
  m_property->setValue(text().toStdWString());
  notifyTool();
  // synchronize the state with the same widgets in other tool option bars
  if (m_toolHandle) m_toolHandle->notifyToolChanged();
}

//=============================================================================

StyleIndexFieldAndChip::StyleIndexFieldAndChip(TTool *tool,
                                               TStyleIndexProperty *property,
                                               TPaletteHandle *pltHandle,
                                               ToolHandle *toolHandle)
    : StyleIndexLineEdit()
    , ToolOptionControl(tool, property->getName(), toolHandle)
    , m_property(property)
    , m_pltHandle(pltHandle) {
  m_property->addListener(this);

  updateStatus();
  connect(this, SIGNAL(textChanged(const QString &)),
          SLOT(onValueChanged(const QString &)));

  setPaletteHandle(pltHandle);
  connect(pltHandle, SIGNAL(colorStyleSwitched()), SLOT(updateColor()));
  connect(pltHandle, SIGNAL(colorStyleChanged(bool)), SLOT(updateColor()));
}

//-----------------------------------------------------------------------------

void StyleIndexFieldAndChip::updateStatus() {
  QString newText = QString::fromStdWString(m_property->getValue());
  if (newText == text()) return;

  setText(newText);
}

//-----------------------------------------------------------------------------

void StyleIndexFieldAndChip::onValueChanged(const QString &changedText) {
  QString style;

  // Aware of both "current" and translated string
  if (!QString("current").contains(changedText) &&
      !StyleIndexLineEdit::tr("current").contains(changedText)) {
    int index      = changedText.toInt();
    TPalette *plt  = m_pltHandle->getPalette();
    int indexCount = plt->getStyleCount();
    if (index > indexCount)
      style = QString::number(indexCount - 1);
    else
      style = text();
  }
  m_property->setValue(style.toStdWString());
  repaint();
  // synchronize the state with the same widgets in other tool option bars
  if (m_toolHandle) m_toolHandle->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void StyleIndexFieldAndChip::updateColor() { repaint(); }

//=============================================================================

ToolOptionParamRelayField::ToolOptionParamRelayField(
    TTool *tool, TDoubleParamRelayProperty *property, int decimals)
    : MeasuredDoubleLineEdit()
    , ToolOptionControl(tool, property->getName())
    , m_param()
    , m_measure()
    , m_property(property)
    , m_globalKey()
    , m_globalGroup() {
  setFixedSize(70, 20);
  m_property->addListener(this);

  setDecimals(decimals);
  updateStatus();
  connect(this, SIGNAL(valueChanged()), SLOT(onValueChanged()));
}

//-----------------------------------------------------------------------------

void ToolOptionParamRelayField::setGlobalKey(TBoolProperty *globalKey,
                                             TPropertyGroup *globalGroup) {
  m_globalKey = globalKey, m_globalGroup = globalGroup;
}

//-----------------------------------------------------------------------------

void ToolOptionParamRelayField::updateStatus() {
  TDoubleParamP param(m_property->getParam());
  if (param != m_param) {
    // Initialize param referencing
    m_param = param.getPointer();

    if (param) {
      m_measure = param->getMeasure();
      setMeasure(m_measure ? m_measure->getName() : "");

      setValue(m_property->getValue());
    }
  }

  if (!param) {
    setEnabled(false);
    m_measure = 0;
    setText("");

    return;
  }

  setEnabled(true);

  TMeasure *measure = param->getMeasure();
  if (measure != m_measure) {
    // Update measure if needed
    m_measure = measure;
    setMeasure(measure ? measure->getName() : "");
  }

  double v = m_property->getValue();
  if (getValue() == v) return;

  // Update value if needed
  setValue(v);
}

//-----------------------------------------------------------------------------

void ToolOptionParamRelayField::onValueChanged() {
  struct locals {
    static inline void setKeyframe(TDoubleParamRelayProperty *prop) {
      if (!prop) return;

      TDoubleParam *param = prop->getParam().getPointer();
      if (!param) return;

      double frame = prop->frame();
      if (!param->isKeyframe(frame)) {
        KeyframeSetter setter(param, -1, true);
        setter.createKeyframe(frame);
      }
    }

    //-----------------------------------------------------------------------------

    struct SetValueUndo final : public TUndo {
      TDoubleParamP m_param;      //!< The referenced param
      double m_oldVal, m_newVal;  //!< Values before and after the set action...
      double m_frame;             //!< ... at this frame

    public:
      SetValueUndo(const TDoubleParamP &param, double oldVal, double newVal,
                   double frame)
          : m_param(param)
          , m_oldVal(oldVal)
          , m_newVal(newVal)
          , m_frame(frame) {}

      int getSize() const {
        return sizeof(SetValueUndo) + sizeof(TDoubleParam);
      }
      void undo() const { m_param->setValue(m_frame, m_oldVal); }
      void redo() const { m_param->setValue(m_frame, m_newVal); }
    };
  };

  //-----------------------------------------------------------------------------

  // NOTE: Values are extracted upon entry, since setting a keyframe reverts the
  // lineEdit
  // field value back to the original value (due to feedback from the param
  // itself)...
  double oldVal = m_property->getValue(), newVal = getValue(),
         frame = m_property->frame();

  TDoubleParamP param = m_property->getParam();
  if (!param) return;

  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();

  if (m_globalKey && m_globalGroup && m_globalKey->getValue()) {
    // Set a keyframe for each of the TDoubleParam relayed in the globalGroup
    int p, pCount = m_globalGroup->getPropertyCount();
    for (p = 0; p != pCount; ++p) {
      TProperty *prop = m_globalGroup->getProperty(p);
      if (TDoubleParamRelayProperty *relProp =
              dynamic_cast<TDoubleParamRelayProperty *>(prop))
        locals::setKeyframe(relProp);
    }
  } else {
    // Set a keyframe just for our param
    locals::setKeyframe(m_property);
  }

  // Assign the edited value to the relayed param
  m_property->setValue(newVal);
  notifyTool();

  manager->add(new locals::SetValueUndo(param, oldVal, newVal, frame));
  manager->endBlock();
}

//=============================================================================
//
// Widget specifici di ArrowTool (derivati da ToolOptionControl)
//

// SPOSTA in un file a parte!

using namespace DVGui;

MeasuredValueField::MeasuredValueField(QWidget *parent, QString name)
    : LineEdit(name, parent)
    , m_isGlobalKeyframe(false)
    , m_modified(false)
    , m_errorHighlighting(false)
    , m_precision(2) {
  setObjectName("MeasuredValueField");

  m_value = new TMeasuredValue("length");
  setText(QString::fromStdWString(m_value->toWideString(m_precision)));
  connect(this, SIGNAL(textChanged(const QString &)), this,
          SLOT(onTextChanged(const QString &)));
  connect(this, SIGNAL(editingFinished()), SLOT(commit()));
  connect(&m_errorHighlightingTimer, SIGNAL(timeout()), this,
          SLOT(errorHighlightingTick()));
}

//-----------------------------------------------------------------------------

MeasuredValueField::~MeasuredValueField() { delete m_value; }

//-----------------------------------------------------------------------------

void MeasuredValueField::setMeasure(std::string name) {
  // for reproducing the precision
  int oldPrec = -1;

  delete m_value;
  m_value = new TMeasuredValue(name != "" ? name : "dummy");

  setText(QString::fromStdWString(m_value->toWideString(m_precision)));
}

//-----------------------------------------------------------------------------

void MeasuredValueField::commit() {
  if (!m_modified && !isReturnPressed()) return;
  // commit is called when the field comes out of focus.
  // mouse drag will call this - return if coming from mouse drag.
  // else undo is set twice
  if (m_mouseEdit) {
    m_mouseEdit = false;
    return;
  }
  int err    = 1;
  bool isSet = m_value->setValue(text().toStdWString(), &err);
  m_modified = false;
  if (err != 0) {
    setText(QString::fromStdWString(m_value->toWideString(m_precision)));
    m_errorHighlighting = 1;
    if (!m_errorHighlightingTimer.isActive())
      m_errorHighlightingTimer.start(40);
  } else {
    if (m_errorHighlightingTimer.isActive()) m_errorHighlightingTimer.stop();
    m_errorHighlighting = 0;
    setStyleSheet("");
  }

  if (!isSet && !isReturnPressed()) return;

  setText(QString::fromStdWString(m_value->toWideString(m_precision)));
  m_modified = false;
  emit measuredValueChanged(m_value);
}

//-----------------------------------------------------------------------------

void MeasuredValueField::onTextChanged(const QString &) { m_modified = true; }

//-----------------------------------------------------------------------------

void MeasuredValueField::setValue(double v) {
  if (getValue() == v) return;
  m_value->setValue(TMeasuredValue::MainUnit, v);
  setText(QString::fromStdWString(m_value->toWideString(m_precision)));
}

//-----------------------------------------------------------------------------

double MeasuredValueField::getValue() const {
  return m_value->getValue(TMeasuredValue::MainUnit);
}

//-----------------------------------------------------------------------------

void MeasuredValueField::errorHighlightingTick() {
  if (m_errorHighlighting < 0.01) {
    if (m_errorHighlightingTimer.isActive()) m_errorHighlightingTimer.stop();
    m_errorHighlighting = 0;
    setStyleSheet("");
  } else {
    int v               = 255 - (int)(m_errorHighlighting * 255);
    m_errorHighlighting = m_errorHighlighting * 0.8;
    int c               = 255 << 16 | v << 8 | v;
    setStyleSheet(QString("#MeasuredValueField {background-color:#%1}")
                      .arg(c, 6, 16, QLatin1Char('0')));
  }
}

//-----------------------------------------------------------------------------

void MeasuredValueField::setPrecision(int precision) {
  m_precision = precision;
  setText(QString::fromStdWString(m_value->toWideString(m_precision)));
}

//-----------------------------------------------------------------------------

void MeasuredValueField::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) return;
  if ((e->buttons() == Qt::MiddleButton) || m_labelClicked) {
    m_xMouse        = e->x();
    m_mouseEdit     = true;
    m_originalValue = m_value->getValue(TMeasuredValue::CurrentUnit);
  } else {
    QLineEdit::mousePressEvent(e);
    if (!m_isTyping) {  // only the first click will select all
      selectAll();
      m_isTyping = true;
    }
  }
}

//-----------------------------------------------------------------------------

void MeasuredValueField::mouseMoveEvent(QMouseEvent *e) {
  if (!isEnabled()) return;
  if ((e->buttons() == Qt::MiddleButton) || m_labelClicked) {
    m_value->modifyValue((e->x() - m_xMouse) / 2);
    setText(QString::fromStdWString(m_value->toWideString(m_precision)));
    m_xMouse = e->x();
    // measuredValueChanged to update the UI, but don't add to undo
    emit measuredValueChanged(m_value, false);
  } else
    QLineEdit::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

void MeasuredValueField::mouseReleaseEvent(QMouseEvent *e) {
  if (!isEnabled()) return;
  // m_mouseEdit will be set false in commit
  if (m_mouseEdit) {
    // This seems redundant, but this is necessary for undo to work
    double valueToRestore = m_value->getValue(TMeasuredValue::CurrentUnit);
    m_value->setValue(TMeasuredValue::CurrentUnit, m_originalValue);
    setText(QString::fromStdWString(m_value->toWideString(m_precision)));
    emit measuredValueChanged(m_value, false);
    // add this to undo
    m_value->setValue(TMeasuredValue::CurrentUnit, valueToRestore);
    setText(QString::fromStdWString(m_value->toWideString(m_precision)));
    emit measuredValueChanged(m_value, true);
    clearFocus();
  } else
    QLineEdit::mouseReleaseEvent(e);
}

//-----------------------------------------------------------------------------

void MeasuredValueField::focusOutEvent(QFocusEvent *e) {
  DVGui::LineEdit::focusOutEvent(e);
  m_isTyping = false;
}

//-----------------------------------------------------------------------------

void MeasuredValueField::receiveMousePress(QMouseEvent *e) {
  m_labelClicked = true;
  mousePressEvent(e);
}

void MeasuredValueField::receiveMouseMove(QMouseEvent *e) { mouseMoveEvent(e); }

void MeasuredValueField::receiveMouseRelease(QMouseEvent *e) {
  mouseReleaseEvent(e);
  m_labelClicked = false;
}

//=============================================================================

namespace {
// calculate maximum field size (once) with 10 pixels margin
int getMaximumWidthForEditToolField(QWidget *widget) {
  static int fieldMaxWidth = widget->fontMetrics().width("-0000.00 field") + 10;
  return fieldMaxWidth;
}
}  // namespace

PegbarChannelField::PegbarChannelField(TTool *tool,
                                       enum TStageObject::Channel actionId,
                                       QString name, TFrameHandle *frameHandle,
                                       TObjectHandle *objHandle,
                                       TXsheetHandle *xshHandle,
                                       QWidget *parent)
    : MeasuredValueField(parent, name)
    , ToolOptionControl(tool, "")
    , m_actionId(actionId)
    , m_frameHandle(frameHandle)
    , m_objHandle(objHandle)
    , m_xshHandle(xshHandle)
    , m_scaleType(eNone) {
  bool ret = connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
                     SLOT(onChange(TMeasuredValue *, bool)));
  assert(ret);
  // NOTA: per le unita' di misura controlla anche tpegbar.cpp
  switch (actionId) {
  case TStageObject::T_X:
    setMeasure("length.x");
    break;
  case TStageObject::T_Y:
    setMeasure("length.y");
    break;
  case TStageObject::T_Z:
    setMeasure("zdepth");
    break;
  case TStageObject::T_Path:
    setMeasure("percentage2");
    break;
  case TStageObject::T_ShearX:
  case TStageObject::T_ShearY:
    setMeasure("shear");
    break;
  case TStageObject::T_Angle:
    setMeasure("angle");
    break;
  case TStageObject::T_ScaleX:
  case TStageObject::T_ScaleY:
  case TStageObject::T_Scale:
    setMeasure("scale");
    break;
  default:
    setMeasure("dummy");
    break;
  }

  setMaximumWidth(getMaximumWidthForEditToolField(this));

  updateStatus();
}

//-----------------------------------------------------------------------------

void PegbarChannelField::onChange(TMeasuredValue *fld, bool addToUndo) {
  if (!m_tool->isEnabled()) return;

  // the camera will crash with a value of 0
  if (m_tool->getObjectId().isCamera()) {
    if (fld->getMeasure()->getName() == "scale" &&
        fld->getValue(TMeasuredValue::MainUnit) == 0) {
      fld->setValue(TMeasuredValue::MainUnit, 0.0001);
    }
  }
  bool modifyConnectedActionId = false;
  if (addToUndo) TUndoManager::manager()->beginBlock();
  // m_firstMouseDrag is set to true only if addToUndo is false
  // and only for the first drag
  // This should always fire if addToUndo is true
  if (!m_firstMouseDrag) {
    m_before = TStageObjectValues();
    m_before.setFrameHandle(m_frameHandle);
    m_before.setObjectHandle(m_objHandle);
    m_before.setXsheetHandle(m_xshHandle);
    m_before.add(m_actionId);
    if (m_scaleType != eNone) {
      modifyConnectedActionId = true;
      if (m_actionId == TStageObject::T_ScaleX)
        m_before.add(TStageObject::T_ScaleY);
      else if (m_actionId == TStageObject::T_ScaleY)
        m_before.add(TStageObject::T_ScaleX);
      else
        modifyConnectedActionId = false;
    }
    if (m_isGlobalKeyframe) {
      m_before.add(TStageObject::T_Angle);
      m_before.add(TStageObject::T_X);
      m_before.add(TStageObject::T_Y);
      m_before.add(TStageObject::T_Z);
      m_before.add(TStageObject::T_SO);
      m_before.add(TStageObject::T_ScaleX);
      m_before.add(TStageObject::T_ScaleY);
      m_before.add(TStageObject::T_Scale);
      m_before.add(TStageObject::T_Path);
      m_before.add(TStageObject::T_ShearX);
      m_before.add(TStageObject::T_ShearY);
    }
    m_before.updateValues();
  }
  TStageObjectValues after;
  after    = m_before;
  double v = fld->getValue(TMeasuredValue::MainUnit);
  if (modifyConnectedActionId) {
    double oldv1 = after.getValue(0);
    double oldv2 = after.getValue(1);
    double newV;
    if (m_scaleType == eAR)
      newV = (v / oldv1) * oldv2;
    else
      newV = (v == 0) ? 10000 : (1 / v) * (oldv1 * oldv2);
    after.setValues(v, newV);
  } else
    after.setValue(v);
  after.applyValues();

  TTool::Viewer *viewer = m_tool->getViewer();
  if (viewer) m_tool->invalidate();
  setCursorPosition(0);

  if (addToUndo) {
    UndoStageObjectMove *undo = new UndoStageObjectMove(m_before, after);
    undo->setObjectHandle(m_objHandle);
    TUndoManager::manager()->add(undo);
    TUndoManager::manager()->endBlock();
    m_firstMouseDrag = false;
  }
  if (!addToUndo && !m_firstMouseDrag) m_firstMouseDrag = true;
  m_objHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void PegbarChannelField::updateStatus() {
  TXsheet *xsh         = m_tool->getXsheet();
  int frame            = m_tool->getFrame();
  TStageObjectId objId = m_tool->getObjectId();
  if (m_actionId == TStageObject::T_Z)
    setMeasure(objId.isCamera() ? "zdepth.cam" : "zdepth");

  double v = xsh->getStageObject(objId)->getParam(m_actionId, frame);

  if (getValue() == v) return;
  setValue(v);
  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

void PegbarChannelField::onScaleTypeChanged(int type) {
  m_scaleType = (ScaleType)type;
}

//=============================================================================

PegbarCenterField::PegbarCenterField(TTool *tool, int index, QString name,
                                     TObjectHandle *objHandle,
                                     TXsheetHandle *xshHandle, QWidget *parent)
    : MeasuredValueField(parent, name)
    , ToolOptionControl(tool, "")
    , m_index(index)
    , m_objHandle(objHandle)
    , m_xshHandle(xshHandle) {
  TStageObjectId objId = m_tool->getObjectId();
  setMeasure(m_index == 0 ? "length.x" : "length.y");
  connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
          SLOT(onChange(TMeasuredValue *, bool)));
  updateStatus();
  setMaximumWidth(getMaximumWidthForEditToolField(this));
}

//-----------------------------------------------------------------------------

void PegbarCenterField::onChange(TMeasuredValue *fld, bool addToUndo) {
  if (!m_tool->isEnabled()) return;
  TXsheet *xsh         = m_tool->getXsheet();
  int frame            = m_tool->getFrame();
  TStageObjectId objId = m_tool->getObjectId();

  TStageObject *obj = xsh->getStageObject(objId);

  double v                           = fld->getValue(TMeasuredValue::MainUnit);
  TPointD center                     = obj->getCenter(frame);
  if (!m_firstMouseDrag) m_oldCenter = center;
  if (m_index == 0)
    center.x = v;
  else
    center.y = v;
  obj->setCenter(frame, center);
  m_tool->invalidate();

  if (addToUndo) {
    UndoStageObjectCenterMove *undo =
        new UndoStageObjectCenterMove(objId, frame, m_oldCenter, center);
    undo->setObjectHandle(m_objHandle);
    undo->setXsheetHandle(m_xshHandle);
    TUndoManager::manager()->add(undo);
    m_firstMouseDrag = false;
  }
  if (!addToUndo && !m_firstMouseDrag) m_firstMouseDrag = true;
  m_objHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void PegbarCenterField::updateStatus() {
  TXsheet *xsh         = m_tool->getXsheet();
  int frame            = m_tool->getFrame();
  TStageObjectId objId = m_tool->getObjectId();
  TStageObject *obj    = xsh->getStageObject(objId);
  TPointD center       = obj->getCenter(frame);

  double v = m_index == 0 ? center.x : center.y;
  if (getValue() == v) return;
  setValue(v);
}

//=============================================================================

NoScaleField::NoScaleField(TTool *tool, QString name)
    : MeasuredValueField(0, name), ToolOptionControl(tool, "") {
  TStageObjectId objId = m_tool->getObjectId();
  setMeasure("zdepth");
  connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
          SLOT(onChange(TMeasuredValue *, bool)));
  updateStatus();
  setMaximumWidth(getMaximumWidthForEditToolField(this));
}

//-----------------------------------------------------------------------------

void NoScaleField::onChange(TMeasuredValue *fld, bool addToUndo) {
  // addToUndo isn't needed here as the field denominator
  // doesn't have an undo
  if (!m_tool->isEnabled()) return;
  TXsheet *xsh         = m_tool->getXsheet();
  int frame            = m_tool->getFrame();
  TStageObjectId objId = m_tool->getObjectId();
  TStageObject *obj    = xsh->getStageObject(objId);

  if (m_isGlobalKeyframe)
    xsh->getStageObject(objId)->setKeyframeWithoutUndo(frame);

  double v = fld->getValue(TMeasuredValue::MainUnit);
  obj->setNoScaleZ(v);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void NoScaleField::updateStatus() {
  TXsheet *xsh         = m_tool->getXsheet();
  int frame            = m_tool->getFrame();
  TStageObjectId objId = m_tool->getObjectId();
  TStageObject *obj    = xsh->getStageObject(objId);

  double v = obj->getNoScaleZ();
  if (getValue() == v) return;
  setValue(v);
}

//=============================================================================

PropertyMenuButton::PropertyMenuButton(QWidget *parent, TTool *tool,
                                       QList<TBoolProperty *> properties,
                                       QIcon icon, QString tooltip)
    : QToolButton(parent)
    , ToolOptionControl(tool, "")
    , m_properties(properties) {
  setPopupMode(QToolButton::InstantPopup);
  setIcon(icon);
  setToolTip(tooltip);

  QMenu *menu                     = new QMenu(tooltip, this);
  if (!tooltip.isEmpty()) tooltip = tooltip + " ";

  QActionGroup *actiongroup = new QActionGroup(this);
  actiongroup->setExclusive(false);
  int i;
  for (i = 0; i < m_properties.count(); i++) {
    TBoolProperty *prop  = m_properties.at(i);
    QString propertyName = prop->getQStringName();
    // Se il tooltip essagnato e' contenuto nel nome della proprieta' lo levo
    // dalla stringa dell'azione
    if (propertyName.contains(tooltip)) propertyName.remove(tooltip);
    QAction *action = menu->addAction(propertyName);
    action->setCheckable(true);
    action->setChecked(prop->getValue());
    action->setData(QVariant(i));
    actiongroup->addAction(action);
  }
  bool ret = connect(actiongroup, SIGNAL(triggered(QAction *)),
                     SLOT(onActionTriggered(QAction *)));
  assert(ret);

  setMenu(menu);
}

//-----------------------------------------------------------------------------

void PropertyMenuButton::updateStatus() {
  QMenu *m = menu();
  assert(m);
  QList<QAction *> actionList = m->actions();
  assert(actionList.count() == m_properties.count());

  int i;
  for (i = 0; i < m_properties.count(); i++) {
    TBoolProperty *prop   = m_properties.at(i);
    QAction *action       = actionList.at(i);
    bool isPropertyLocked = prop->getValue();
    if (action->isChecked() != isPropertyLocked)
      action->setChecked(isPropertyLocked);
  }
}

//-----------------------------------------------------------------------------

void PropertyMenuButton::onActionTriggered(QAction *action) {
  int currentPropertyIndex = action->data().toInt();
  TBoolProperty *prop      = m_properties.at(currentPropertyIndex);
  bool isChecked           = action->isChecked();
  if (isChecked == prop->getValue()) return;
  prop->setValue(isChecked);
  notifyTool();

  emit onPropertyChanged(QString(prop->getName().c_str()));
}

//=============================================================================
namespace {
// calculate maximum field size (once) with 10 pixels margin
int getMaximumWidthForSelectionToolField(QWidget *widget) {
  static int fieldMaxWidth = widget->fontMetrics().width("-000.00 %") + 10;
  return fieldMaxWidth;
}
}  // namespace

// id == 0 Scale X
// id == 0 Scale Y
SelectionScaleField::SelectionScaleField(SelectionTool *tool, int id,
                                         QString name)
    : MeasuredValueField(0, name), m_tool(tool), m_id(id) {
  bool ret = connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
                     SLOT(onChange(TMeasuredValue *, bool)));
  assert(ret);
  setMeasure("scale");
  updateStatus();

  setMaximumWidth(getMaximumWidthForSelectionToolField(this));
}

//-----------------------------------------------------------------------------

bool SelectionScaleField::applyChange(bool addToUndo) {
  if (!m_tool || (m_tool->isSelectionEmpty() && !m_tool->isLevelType()))
    return false;
  DragSelectionTool::DragTool *scaleTool = createNewScaleTool(m_tool, 0);
  double p                               = getValue();
  if (p == 0) p                          = 0.00001;
  DragSelectionTool::FourPoints points   = m_tool->getBBox();
  TPointD center                         = m_tool->getCenter();
  TPointD p0M                            = points.getPoint(7);
  TPointD p1M                            = points.getPoint(5);
  TPointD pM1                            = points.getPoint(6);
  TPointD pM0                            = points.getPoint(4);
  int pointIndex;
  TPointD sign(1, 1);
  TPointD scaleFactor = m_tool->m_deformValues.m_scaleValue;
  TPointD newPos;
  if (m_id == 0) {
    if (p1M == p0M) return false;
    pointIndex      = 7;
    TPointD v       = normalize(p1M - p0M);
    double currentD = tdistance(p1M, p0M);
    double startD   = currentD / scaleFactor.x;
    double d      = (currentD - startD * p) * tdistance(center, p0M) / currentD;
    newPos        = TPointD(p0M.x + d * v.x, p0M.y + d * v.y);
    scaleFactor.x = p;
  } else if (m_id == 1) {
    if (pM1 == pM0) return false;
    pointIndex      = 4;
    TPointD v       = normalize(pM1 - pM0);
    double currentD = tdistance(pM1, pM0);
    double startD   = currentD / scaleFactor.y;
    double d      = (currentD - startD * p) * tdistance(center, pM0) / currentD;
    newPos        = TPointD(pM0.x + d * v.x, pM0.y + d * v.y);
    scaleFactor.y = p;
  }

  m_tool->m_deformValues.m_scaleValue =
      scaleFactor;  // Instruction order is relevant
  scaleTool->transform(pointIndex,
                       newPos);  // This line invokes GUI update using the
                                 // value set above
  if (!m_tool->isLevelType() && addToUndo) scaleTool->addTransformUndo();
  setCursorPosition(0);
  return true;
}

//-----------------------------------------------------------------------------

void SelectionScaleField::onChange(TMeasuredValue *fld, bool addToUndo) {
  if (!m_tool->isEnabled()) return;
  if (!applyChange(addToUndo)) return;
  emit valueChange(addToUndo);
}

//-----------------------------------------------------------------------------

void SelectionScaleField::updateStatus() {
  if (!m_tool || (m_tool->isSelectionEmpty() && !m_tool->isLevelType())) {
    setValue(0);
    setDisabled(true);
    return;
  }
  setDisabled(false);
  if (m_id == 0)
    setValue(m_tool->m_deformValues.m_scaleValue.x);
  else
    setValue(m_tool->m_deformValues.m_scaleValue.y);
  setCursorPosition(0);
}

//=============================================================================

SelectionRotationField::SelectionRotationField(SelectionTool *tool,
                                               QString name)
    : MeasuredValueField(0, name), m_tool(tool) {
  bool ret = connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
                     SLOT(onChange(TMeasuredValue *, bool)));
  assert(ret);
  setMeasure("angle");
  updateStatus();

  setMaximumWidth(getMaximumWidthForSelectionToolField(this));
}

//-----------------------------------------------------------------------------

void SelectionRotationField::onChange(TMeasuredValue *fld, bool addToUndo) {
  if (!m_tool || !m_tool->isEnabled() ||
      (m_tool->isSelectionEmpty() && !m_tool->isLevelType()))
    return;

  DragSelectionTool::DragTool *rotationTool = createNewRotationTool(m_tool);

  DragSelectionTool::DeformValues &deformValues = m_tool->m_deformValues;
  double p                                      = getValue();

  TAffine aff =
      TRotation(m_tool->getCenter(), p - deformValues.m_rotationAngle);

  deformValues.m_rotationAngle = p;  // Instruction order is relevant here
  rotationTool->transform(aff, p - deformValues.m_rotationAngle);  //

  if (!m_tool->isLevelType() && addToUndo) rotationTool->addTransformUndo();

  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

void SelectionRotationField::updateStatus() {
  if (!m_tool || (m_tool->isSelectionEmpty() && !m_tool->isLevelType())) {
    setValue(0);
    setDisabled(true);
    return;
  }
  setDisabled(false);
  setValue(m_tool->m_deformValues.m_rotationAngle);
  setCursorPosition(0);
}

//=============================================================================
// id == 0 Move X
// id == 0 Move Y
SelectionMoveField::SelectionMoveField(SelectionTool *tool, int id,
                                       QString name)
    : MeasuredValueField(0, name), m_tool(tool), m_id(id) {
  bool ret = connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
                     SLOT(onChange(TMeasuredValue *, bool)));
  assert(ret);
  if (m_id == 0)
    setMeasure("length.x");
  else
    setMeasure("length.y");
  updateStatus();

  // for translation value field, use size for the Edit Tool as it needs more
  // estate
  setMaximumWidth(getMaximumWidthForEditToolField(this));
}

//-----------------------------------------------------------------------------

void SelectionMoveField::onChange(TMeasuredValue *fld, bool addToUndo) {
  if (!m_tool || !m_tool->isEnabled() ||
      (m_tool->isSelectionEmpty() && !m_tool->isLevelType()))
    return;

  DragSelectionTool::DragTool *moveTool = createNewMoveSelectionTool(m_tool);

  double p        = getValue() * Stage::inch;
  TPointD delta   = (m_id == 0) ? TPointD(p, 1) : TPointD(1, p),
          oldMove = Stage::inch * m_tool->m_deformValues.m_moveValue,
          oldDelta =
              (m_id == 0) ? TPointD(oldMove.x, 1) : TPointD(1, oldMove.y),
          newMove = (m_id == 0) ? TPointD(delta.x, oldMove.y)
                                : TPointD(oldMove.x, delta.y);

  TAffine aff = TTranslation(-oldDelta) * TTranslation(delta);

  m_tool->m_deformValues.m_moveValue =
      1 / Stage::inch * newMove;  // Instruction order relevant here
  moveTool->transform(aff);       //

  if (!m_tool->isLevelType() && addToUndo) moveTool->addTransformUndo();

  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

void SelectionMoveField::updateStatus() {
  if (!m_tool || (m_tool->isSelectionEmpty() && !m_tool->isLevelType())) {
    setValue(0);
    setDisabled(true);
    return;
  }
  setDisabled(false);

  if (m_id == 0)
    setValue(m_tool->m_deformValues.m_moveValue.x);
  else
    setValue(m_tool->m_deformValues.m_moveValue.y);

  setCursorPosition(0);
}

//=============================================================================

ThickChangeField::ThickChangeField(SelectionTool *tool, QString name)
    : MeasuredValueField(0, name), m_tool(tool) {
  bool ret = connect(this, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
                     SLOT(onChange(TMeasuredValue *, bool)));
  assert(ret);
  setMeasure("");
  updateStatus();

  setMaximumWidth(getMaximumWidthForSelectionToolField(this));
}

//-----------------------------------------------------------------------------

void ThickChangeField::onChange(TMeasuredValue *fld, bool addToUndo) {
  if (!m_tool || (m_tool->isSelectionEmpty() && !m_tool->isLevelType())) return;

  DragSelectionTool::VectorChangeThicknessTool *changeThickTool =
      new DragSelectionTool::VectorChangeThicknessTool(
          (VectorSelectionTool *)m_tool);

  TVectorImageP vi = (TVectorImageP)m_tool->getImage(true);

  double p            = 0.5 * getValue();
  double newThickness = p - m_tool->m_deformValues.m_maxSelectionThickness;

  changeThickTool->setThicknessChange(newThickness);
  changeThickTool->changeImageThickness(*vi, newThickness);

  // DragSelectionTool::DeformValues deformValues = m_tool->m_deformValues;
  // // Like when I found it - it's a noop.
  // deformValues.m_maxSelectionThickness = p;
  // // Seems that the actual update is performed inside
  // the above change..() instruction...   >_<
  if (addToUndo) {
    changeThickTool->addUndo();
  }
  m_tool->computeBBox();
  m_tool->invalidate();
  m_tool->notifyImageChanged(m_tool->getCurrentFid());
}

//-----------------------------------------------------------------------------

void ThickChangeField::updateStatus() {
  if (!m_tool || m_tool->m_deformValues.m_isSelectionModified ||
      (m_tool->isSelectionEmpty() && !m_tool->isLevelType())) {
    setValue(0);
    setDisabled(true);
    return;
  }

  setDisabled(false);
  setValue(2 * m_tool->m_deformValues.m_maxSelectionThickness);
  setCursorPosition(0);
}

//=============================================================================

ClickableLabel::ClickableLabel(const QString &text, QWidget *parent,
                               Qt::WindowFlags f)
    : QLabel(text, parent, f) {}

//-----------------------------------------------------------------------------

ClickableLabel::~ClickableLabel() {}

//-----------------------------------------------------------------------------

void ClickableLabel::mousePressEvent(QMouseEvent *event) {
  emit onMousePress(event);
}

//-----------------------------------------------------------------------------

void ClickableLabel::mouseMoveEvent(QMouseEvent *event) {
  emit onMouseMove(event);
}

//-----------------------------------------------------------------------------

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event) {
  emit onMouseRelease(event);
}