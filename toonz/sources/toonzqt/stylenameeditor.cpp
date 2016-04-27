#include "toonzqt/stylenameeditor.h"

#include "tcommon.h"
#include "toonz/tpalettehandle.h"
#include "tcolorstyles.h"
#include "tpalette.h"
#include "toonz/palettecmd.h"

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

StyleNameEditor::StyleNameEditor(QWidget *parent)
	: QDialog(parent), m_paletteHandle(0)
{
	setWindowTitle(tr("Name Editor"));

	m_styleName = new QLineEdit(this);
	m_okButton = new QPushButton(tr("OK"), this);
	m_cancelButton = new QPushButton(tr("Cancel"), this);
	m_applyButton = new QPushButton(tr("Apply"), this);

	m_styleName->setEnabled(false);
	m_okButton->setEnabled(false);
	m_okButton->setFocusPolicy(Qt::NoFocus);
	m_applyButton->setEnabled(false);
	m_cancelButton->setFocusPolicy(Qt::NoFocus);

	m_styleName->setObjectName("RenameColorTextField");

	QVBoxLayout *mainLayout = new QVBoxLayout();
	mainLayout->setMargin(10);
	mainLayout->setSpacing(5);
	{
		QHBoxLayout *inputLayout = new QHBoxLayout();
		inputLayout->setMargin(0);
		inputLayout->setSpacing(3);
		{
			inputLayout->addWidget(new QLabel(tr("Style Name"), this), 0);
			inputLayout->addWidget(m_styleName, 1);
		}
		mainLayout->addLayout(inputLayout);

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->setMargin(0);
		buttonLayout->setSpacing(3);
		{
			buttonLayout->addWidget(m_okButton);
			buttonLayout->addWidget(m_applyButton);
			buttonLayout->addWidget(m_cancelButton);
		}
		mainLayout->addLayout(buttonLayout);
	}
	setLayout(mainLayout);

	setGeometry(640, 512, 420, 80);

	connect(m_okButton, SIGNAL(pressed()), this, SLOT(onOkPressed()));
	connect(m_cancelButton, SIGNAL(pressed()), this, SLOT(onCancelPressed()));
	connect(m_applyButton, SIGNAL(pressed()), this, SLOT(onApplyPressed()));
}

//-------
void StyleNameEditor::setPaletteHandle(TPaletteHandle *ph)
{
	m_paletteHandle = ph;
	connect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this, SLOT(onStyleSwitched()));
	connect(m_paletteHandle, SIGNAL(paletteSwitched()), this, SLOT(onStyleSwitched()));
	m_styleName->setEnabled(true);
	m_okButton->setEnabled(true);
	m_applyButton->setEnabled(true);
}

//-------
void StyleNameEditor::showEvent(QShowEvent *e)
{
	if (m_paletteHandle) {
		disconnect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this, SLOT(onStyleSwitched()));
		disconnect(m_paletteHandle, SIGNAL(paletteSwitched()), this, SLOT(onStyleSwitched()));
		connect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this, SLOT(onStyleSwitched()));
		connect(m_paletteHandle, SIGNAL(paletteSwitched()), this, SLOT(onStyleSwitched()));
	}
	//update view
	onStyleSwitched();
}

//-------disconnection
void StyleNameEditor::hideEvent(QHideEvent *e)
{
	disconnect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this, SLOT(onStyleSwitched()));
	disconnect(m_paletteHandle, SIGNAL(paletteSwitched()), this, SLOT(onStyleSwitched()));
}

//-----update display when the current style is switched
void StyleNameEditor::onStyleSwitched()
{

	if (!m_paletteHandle || !m_paletteHandle->getStyle())
		return;

	std::wstring styleName = m_paletteHandle->getStyle()->getName();
	m_styleName->setText(QString::fromStdWString(styleName));
	m_styleName->selectAll();

	int styleId = m_paletteHandle->getStyleIndex();
	setWindowTitle(tr("Name Editor: # %1").arg(styleId));
}

void StyleNameEditor::onOkPressed()
{
	onApplyPressed();
	close();
}
void StyleNameEditor::onApplyPressed()
{
	if (!m_paletteHandle || !m_paletteHandle->getStyle())
		return;
	if (m_styleName->text() == "")
		return;

	std::wstring newName = m_styleName->text().toStdWString();

	PaletteCmd::renamePaletteStyle(m_paletteHandle, newName);
}

void StyleNameEditor::onCancelPressed()
{
	close();
}

//focus when the mouse enters
void StyleNameEditor::enterEvent(QEvent *e)
{
	activateWindow();
	m_styleName->setFocus();
}