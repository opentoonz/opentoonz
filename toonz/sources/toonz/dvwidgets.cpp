

#include "dvwidgets.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/checkbox.h"

#include <QLayout>
#include <QLabel>

using namespace DVGui;

//=============================================================================
// PropertyComboBox
//-----------------------------------------------------------------------------

PropertyComboBox::PropertyComboBox(QWidget *parent, TEnumProperty *prop)
    : QComboBox(parent), PropertyWidget(prop) {
  connect(this, SIGNAL(currentIndexChanged(const QString &)), this,
          SLOT(onCurrentIndexChanged(const QString &)));
  setMaximumHeight(WidgetHeight);
}

//-----------------------------------------------------------------------------

void PropertyComboBox::onCurrentIndexChanged(const QString &text) {
  TEnumProperty *prop  = dynamic_cast<TEnumProperty *>(m_property);
  std::wstring currVal = (currentData().isNull())
                             ? currentText().toStdWString()
                             : currentData().toString().toStdWString();
  if (prop && prop->getValue() != currVal) prop->setValue(currVal);
}

//-----------------------------------------------------------------------------

void PropertyComboBox::onPropertyChanged() {
  TEnumProperty *prop = dynamic_cast<TEnumProperty *>(m_property);
  if (prop) {
    QString str  = QString::fromStdWString(prop->getValue());
    int i        = findData(str);
    if (i < 0) i = findText(str);
    if (i >= 0 && i < count()) setCurrentIndex(i);
  }
}

//=============================================================================
// PropertyCheckBox
//-----------------------------------------------------------------------------

PropertyCheckBox::PropertyCheckBox(const QString &text, QWidget *parent,
                                   TBoolProperty *prop)
    : CheckBox(text, parent), PropertyWidget(prop) {
  connect(this, SIGNAL(stateChanged(int)), this, SLOT(onStateChanged(int)));
  setMaximumHeight(WidgetHeight);
}

//-----------------------------------------------------------------------------

void PropertyCheckBox::onStateChanged(int state) {
  TBoolProperty *prop = dynamic_cast<TBoolProperty *>(m_property);
  if (prop && prop->getValue() != isChecked()) prop->setValue(isChecked());
}

//-----------------------------------------------------------------------------

void PropertyCheckBox::onPropertyChanged() {
  TBoolProperty *prop = dynamic_cast<TBoolProperty *>(m_property);
  if (prop) setChecked(prop->getValue());
}

//=============================================================================
// PropertyLineEdit
//-----------------------------------------------------------------------------

PropertyLineEdit::PropertyLineEdit(QWidget *parent, TStringProperty *prop)
    : LineEdit(parent), PropertyWidget(prop) {
  connect(this, SIGNAL(textChanged(const QString &)), this,
          SLOT(onTextChanged(const QString &)));
  setMaximumSize(100, WidgetHeight);
}

//-----------------------------------------------------------------------------

void PropertyLineEdit::onTextChanged(const QString &text) {
  TStringProperty *prop = dynamic_cast<TStringProperty *>(m_property);
  if (prop && prop->getValue() != text.toStdWString())
    prop->setValue(text.toStdWString());
}

//-----------------------------------------------------------------------------

void PropertyLineEdit::onPropertyChanged() {
  TStringProperty *prop = dynamic_cast<TStringProperty *>(m_property);
  if (prop) setText(QString::fromStdWString(prop->getValue()));
}

//=============================================================================
// PropertyIntField
//-----------------------------------------------------------------------------

PropertyIntField::PropertyIntField(QWidget *parent, TIntProperty *prop)
    : IntField(parent), PropertyWidget(prop) {
  connect(this, SIGNAL(valueChanged(bool)), this, SLOT(onValueChanged(bool)));
}

//-----------------------------------------------------------------------------

void PropertyIntField::onValueChanged(bool isDragging) {
  TIntProperty *prop = dynamic_cast<TIntProperty *>(m_property);
  if (prop) prop->setValue(getValue());
}

//-----------------------------------------------------------------------------

void PropertyIntField::onPropertyChanged() {
  TIntProperty *prop = dynamic_cast<TIntProperty *>(m_property);
  if (prop) this->setValue(prop->getValue());
}
