#pragma once

#ifndef DVWIDGETS_H
#define DVWIDGETS_H

#include <QWidget>
#include <QComboBox>

#include "tproperty.h"
#include "toonzqt/intfield.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"

class QLabel;

//=============================================================================
namespace DVGui
{
//=============================================================================

class PropertyWidget
{

protected:
	TProperty *m_property;

public:
	PropertyWidget(TProperty *prop) : m_property(prop) {}

	void setProperty(TProperty *prop)
	{
		m_property = prop;
		onPropertyChanged();
	}

private:
	virtual void onPropertyChanged() = 0;
};

//-----------------------------------------------------------------------------

class PropertyComboBox : public QComboBox, public PropertyWidget
{
	Q_OBJECT

public:
	PropertyComboBox(QWidget *parent, TEnumProperty *prop);

protected slots:
	void onCurrentIndexChanged(const QString &text);

private:
	void onPropertyChanged();
};

//-----------------------------------------------------------------------------

class PropertyCheckBox : public CheckBox, public PropertyWidget
{
	Q_OBJECT

public:
	PropertyCheckBox(const QString &text, QWidget *parent, TBoolProperty *prop);

protected slots:
	void onStateChanged(int state);

private:
	void onPropertyChanged();
};

//-----------------------------------------------------------------------------

class PropertyLineEdit : public LineEdit, public PropertyWidget
{
	Q_OBJECT

public:
	PropertyLineEdit(QWidget *parent, TStringProperty *prop);

protected slots:
	void onTextChanged(const QString &text);

private:
	void onPropertyChanged();
};

//-----------------------------------------------------------------------------

class PropertyIntField : public IntField, public PropertyWidget
{
	Q_OBJECT

public:
	PropertyIntField(QWidget *parent, TIntProperty *prop);

protected slots:
	void onValueChanged(bool isDragging);

private:
	void onPropertyChanged();
};

//-----------------------------------------------------------------------------
} //namespace DVGui
//-----------------------------------------------------------------------------

#endif // DVWIDGETS_H
