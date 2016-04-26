

#include <QPushButton>
#include <QDialog>
#include <QLabel>
#include <QtGui>
#include <QColor>
#include <QLineEdit>
#include <QIntValidator>
#include "licensegui.h"
#include "licensecontroller.h"
#include "tconvert.h"
#include "tapp.h"

using namespace DVGui;

const int LICENSE_CODE_LENGTH = 40;

QString LicenseWizard::getDaysLeftString(int dayLeft) const
{
	if (dayLeft == 0)
		return tr("Your license has just expired today.");
	else if (dayLeft == -1)
		return tr("Your license has expired yesterday.");
	else if (dayLeft < -1)
		return tr("Your license has expired %n days ago.", "", -dayLeft);
	else if (dayLeft == 1)
		return tr("You have one day left.");
	else
		return tr("You have %n days left.", "", dayLeft);
}

QString LicenseWizard::getLicenseStatusString() const
{
	std::string license = License::getInstalledLicense();
	if (license == "")
		return tr("No license installed.");
	if (License::isTemporaryLicense(license)) {
		int daysLeft = License::getDaysLeft(license);
		return getDaysLeftString(daysLeft);
	} else if (License::checkLicense(license) == License::INVALID_LICENSE) {
		return tr("Corrupted license.");
	} else {
		// non dovrebbe succedere (se la licenza e' valida e non temporanea non si dovrebbe
		// aprire il LicenseWizard
		return tr("Valid license.");
	}
}
LicenseWizardPage::LicenseWizardPage(QWidget *parent) : QWidget(parent)
{
	m_layout = new QVBoxLayout;
	m_layout->setAlignment(Qt::AlignCenter);
	m_layout->setMargin(0);
	m_layout->setSpacing(0);
	QWidget *emptyWidget = new QWidget();
	emptyWidget->setFixedHeight(290);
	m_layout->addWidget(emptyWidget);
	this->setFixedSize(542, 358);
}
void LicenseWizardPage::changeSplashScreen(int id)
{
	m_screenPath = ":Resources/splashLT_trial_activate.png";
	update();
}
void LicenseWizardPage::addWidget(QWidget *widget)
{
	m_layout->addWidget(widget);
	this->setLayout(m_layout);
}
void LicenseWizardPage::paintEvent(QPaintEvent *)
{
	QPixmap pixmap(m_screenPath);
	QPainter painter(this);
	// Disegno lo sfondo
	painter.drawPixmap(1, 1, pixmap);
}

QWidget *LicenseWizard::createConnectionPage()
{
	LicenseWizard *parent = this;
	QWidget *page = new QWidget();
	m_stateConnectionLbl = new QLabel("");
	m_stateConnectionLbl->setStyleSheet("background-color: rgb(255,255,255,0);");

	setStyleSheet("LicenseWizard { border: 0px; }");
	createConnectionButtonBox();
	LicenseWizardPage *splashScreen = new LicenseWizardPage();
	splashScreen->changeSplashScreen(2);
	splashScreen->addWidget(m_stateConnectionLbl);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(splashScreen);
	// layout->addWidget(m_stateConnectionLbl);
	page->setLayout(layout);

	return page;
}
void LicenseWizard::paintEvent(QPaintEvent *)
{
	// disegno il bordo
	QPainter painter(this);
	QPen pen; // = new QPen;
	pen.setColor(1);
	pen.setWidth(3);
	painter.setPen(pen);
	int height = this->height();
	int width = this->width();
	painter.drawRect(0, 0, width - 1, height - 1);
}
QWidget *LicenseWizard::createTryBuyActivatePage()
{
	LicenseWizard *parent = this;
	QWidget *page = new QWidget();

	createTryBuyActivateButtonBox();
	QLabel *label = new QLabel(getLicenseStatusString());
	label->setStyleSheet("background-color: rgb(255,255,255,0);");

	LicenseWizardPage *splashScreen = new LicenseWizardPage();
	splashScreen->changeSplashScreen(0);

	splashScreen->addWidget(label);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(splashScreen);
	// layout->addWidget(label);
	page->setLayout(layout);
	return page;
}

QWidget *LicenseWizard::createActivatePage()
{
	LicenseWizard *parent = this;
	QWidget *page = new QWidget();
	setTopMargin(0);
	createActivateButtonBox();

	LicenseWizardPage *splashScreen = new LicenseWizardPage();
	splashScreen->changeSplashScreen(1);

	// info
	QLabel *info1 = new QLabel(tr(
		"Enter your Activation Code, or your License Code, here:")); // QString::fromStdString(License::getMachineCode()));
	info1->setStyleSheet("background-color: rgb(255,255,255,0);");
	m_info = new QLabel;
	m_info->setStyleSheet("background-color: rgb(255,255,255,0);");
	m_info->setTextFormat(Qt::RichText);
	m_info->setText(QString(tr("<font color=\"#ff1010\">The Activation Code requires an active "
							   "Internet connection.</font>")));
	m_checkFld = new QLabel("");
	m_checkFld->setStyleSheet("background-color: rgb(255,255,255,0);");
	m_checkFld->setFixedSize(22, 22);
	m_checkFld->setAlignment(Qt::AlignLeft);
	// code
	QLineEdit *codeFld = new QLineEdit();
	codeFld->setFixedWidth(320);
	codeFld->setInputMask(QString(">NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN"));
	// m_codeFormat = ">NNNNNN";
	m_markCount = 0;
	m_codeFld = codeFld;
	connect(codeFld, SIGNAL(textChanged(const QString &)), this,
			SLOT(codeChanged(const QString &)));
	connect(codeFld, SIGNAL(cursorPositionChanged(int, int)), this,
			SLOT(slotCursorPositionChanged()));
	QVBoxLayout *layout = new QVBoxLayout;

	QWidget *centerWidget = new QWidget;
	QGridLayout *gridLayout = new QGridLayout;
	gridLayout->addWidget(info1, 0, 0);
	gridLayout->addWidget(m_codeFld, 1, 0);
	gridLayout->addWidget(m_checkFld, 1, 1, Qt::AlignLeft);
	gridLayout->addWidget(m_info, 2, 0);
	centerWidget->setFixedWidth(350);
	centerWidget->setLayout(gridLayout);

	gridLayout->setMargin(0);
	gridLayout->setSpacing(2);
	layout->setMargin(0);
	layout->setSpacing(0);

	splashScreen->addWidget(centerWidget);
	layout->addWidget(splashScreen);
	page->setLayout(layout);
	return page;
}
void LicenseWizard::slotCursorPositionChanged()
{
	QString currentCode = m_codeFld->text();
	if (currentCode == "")
		m_codeFld->setCursorPosition(0);
}
void LicenseWizard::createConnectionButtonBox()
{
	LicenseWizard *parent = this;
	// bottoni
	m_connectionButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
	QPushButton *cancelButton = m_connectionButtonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setMinimumSize(65, 25);
	cancelButton->setText(tr("Cancel"));
	m_connectionButtonBox->setCenterButtons(true);
	connect(m_connectionButtonBox, SIGNAL(rejected()), parent, SLOT(gotoActivatePage()));
}

void LicenseWizard::createTryBuyActivateButtonBox()
{
	LicenseWizard *parent = this;
	// bottoni: Try, Buy, Activate, Cancel
	m_TryBuyActivationPageButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
	QPushButton *cancelButton = m_TryBuyActivationPageButtonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setText(tr("Cancel"));
	cancelButton->setMinimumSize(65, 25);
	m_TryBuyActivationPageButtonBox->setCenterButtons(true);
	connect(m_TryBuyActivationPageButtonBox, SIGNAL(accepted()), parent, SLOT(accept()));
	connect(m_TryBuyActivationPageButtonBox, SIGNAL(rejected()), parent, SLOT(reject()));

	// activate
	QPushButton *activateButton = new QPushButton(tr("&Activate"));
	activateButton->setMinimumSize(65, 25);
	m_TryBuyActivationPageButtonBox->addButton(activateButton, QDialogButtonBox::ActionRole);
	connect(activateButton, SIGNAL(pressed()), parent, SLOT(gotoActivatePage()));

	// buy
	QPushButton *buyButton = new QPushButton(tr("&Buy"));
	buyButton->setMinimumSize(65, 25);
	m_TryBuyActivationPageButtonBox->addButton(buyButton, QDialogButtonBox::ActionRole);
	connect(buyButton, SIGNAL(pressed()), parent, SLOT(buy()));

	// try
	QPushButton *tryButton = new QPushButton(tr("&Try"));
	tryButton->setMinimumSize(65, 25);
	tryButton->setDefault(true);
#ifdef LINETEST
	m_TryBuyActivationPageButtonBox->addButton(tryButton, QDialogButtonBox::ActionRole);
	connect(tryButton, SIGNAL(pressed()), parent, SLOT(tryAct()));
#else
	m_TryBuyActivationPageButtonBox->addButton(tryButton, QDialogButtonBox::AcceptRole);
	tryButton->setDisabled(License::checkLicense() == License::INVALID_LICENSE);
#endif
	return;
}

void LicenseWizard::createActivateButtonBox()
{
	LicenseWizard *parent = this;
	// bottoni: Ok, Back
	m_activationPageButtonBox = new QDialogButtonBox();
	m_activationPageButtonBox->setCenterButtons(true);

	// back
	QPushButton *backButton = new QPushButton(tr("&Back"));
	backButton->setMinimumSize(65, 25);
	m_activationPageButtonBox->addButton(backButton, QDialogButtonBox::ActionRole);
	connect(backButton, SIGNAL(pressed()), parent, SLOT(gotoFirstPage()));

	// activate
	QPushButton *activateButton = new QPushButton(tr("&Activate"));
	activateButton->setMinimumSize(65, 25);
	m_activationPageButtonBox->addButton(activateButton, QDialogButtonBox::ActionRole);
	connect(activateButton, SIGNAL(pressed()), this, SLOT(activate()));
	m_activateBtn = activateButton;
	m_activateBtn->setEnabled(false);

	// parent->addButtonBarWidget(m_activationPageButtonBox);
	return;
}

void LicenseWizard::codeChanged(const QString &text)
{
	if (text.size() > 47 || text.size() == 0)
		return;
	int cursorPosition = m_codeFld->cursorPosition();
	bool enabled = false;

	// formattazione
	int i = 0;
	QString codeFormat;
	codeFormat = ">NNNNN";
	m_markCount = (text.size() - m_markCount) / 5;
	QString dim = text;
	dim = dim.remove("-", Qt::CaseInsensitive);

	if (m_markCount == 0)
		codeFormat = ">NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN";
	if (dim.size() % 5 == 0)
		m_markCount -= 1;
	for (i = 0; i < m_markCount; i++) {
		codeFormat += "-NNNNN";
	}
	if (dim.size() % 5 == 0 && dim.size() < 40)
		codeFormat += "N";
	m_codeFld->setInputMask(codeFormat);

	if (cursorPosition >= text.size())
		cursorPosition = dim.size() + m_markCount;
	m_codeFld->setCursorPosition(cursorPosition);
	std::string code = text.toStdString();
	if (License::checkLicense(code) != License::INVALID_LICENSE) {
		enabled = true;
		m_info->setTextFormat(Qt::RichText);
		m_info->setText(QString(tr(
			"<font color=\"#006600\">Click the Activate button to license the software.</font>")));
		QPixmap pixmap(":Resources/check.png");
		QPainter painter(this);
		painter.drawPixmap(0, 0, pixmap);
		m_checkFld->setPixmap(pixmap);
		// m_checkFld->setText("OK(L)");
	} else if (License::isActivationCode(code)) {
		enabled = true;
		m_info->setTextFormat(Qt::RichText);
		m_info->setText(QString(tr("<font color=\"#006600\">Click the Activate button to connect "
								   "and license the software.</font>")));
		QPixmap pixmap(":Resources/check.png");
		QPainter painter(this);
		painter.drawPixmap(0, 0, pixmap);
		m_checkFld->setPixmap(pixmap);
		// m_checkFld->setText("OK(A)");
	} else
		m_checkFld->setText(" ");
	m_activateBtn->setEnabled(enabled);
	if (!enabled) {
		m_info->setTextFormat(Qt::RichText);
		m_info->setText(QString("<font color=\"#ff1010\">The Activation Code requires an active "
								"Internet connection.</font>"));
	}
}

void LicenseWizard::activate()
{
	std::string code = m_codeFld->text().toStdString();
	if (License::isActivationCode(code)) {
		gotoConnectionPage();
		License::ActivateStatus status = License::activate(code, this);
		/*
	if(status==License::OK)
	{
	  QMessageBox::information(0, QString("Congratulations"), "Activation successfully!");
	  m_licenseWizard->accept();
	}
	else if(status==License::ACTIVATION_REFUSED)
	{
	  QMessageBox::information(0, QString("Activation failed"), "Activation refused");
	}
	*/
	} else if (License::checkLicense(code) != License::INVALID_LICENSE) {
		bool ok = License::setLicense(code);
		if (ok) {
			this->accept();
			// DvMsgBox(INFORMATION,tr("Activation successfully!"));
		}
	} else {
		MsgBox(CRITICAL, "Activation failed: the code is invalid.");
		return;
	}
}

MyWidget::MyWidget(QWidget *parent) : QWidget(parent)
{
	QString screenPath = ":Resources/splashLT_trial_activate.png";
	m_pixmap = new QPixmap(screenPath);
	QSize sizePixmap = m_pixmap->size();
	this->setFixedSize(sizePixmap);
}
void MyWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.drawPixmap(1, 1, *(m_pixmap));
}
UpgradeLicense::UpgradeLicense(QWidget *parent) : Dialog(TApp::instance()->getMainWindow(), true)
{
	setStyleSheet("margin: 1px;");
	setTopMargin(0);
	setTopSpacing(0);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	QLabel *info = new QLabel("Enter StoryPlanner 3.3 license code:");
	info->setStyleSheet("background-color: rgb(255,255,255,0);");
	info->setAlignment(Qt::AlignLeft);
	// code
	m_codeFld = new QLineEdit();
	m_codeFld->setFixedWidth(320);
	m_codeFld->setInputMask(QString(">NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN"));
	m_markCount = 0;
	connect(m_codeFld, SIGNAL(textChanged(const QString &)), this,
			SLOT(codeChanged(const QString &)));
	connect(m_codeFld, SIGNAL(cursorPositionChanged(int, int)), this,
			SLOT(slotCursorPositionChanged()));
	m_checkFld = new QLabel("");
	m_checkFld->setStyleSheet("background-color: rgb(255,255,255,0);");
	m_checkFld->setFixedSize(22, 22);
	m_checkFld->setAlignment(Qt::AlignLeft);
	m_info = new QLabel("");
	m_info->setStyleSheet("background-color: rgb(255,255,255,0);");
	MyWidget *widget = new MyWidget();
	QGridLayout *gridLayout = new QGridLayout;

	QWidget *emptyWidget = new QWidget();
	emptyWidget->setFixedHeight(290);
	gridLayout->addWidget(emptyWidget, 0, 0);
	gridLayout->addWidget(info, 1, 0);

	gridLayout->addWidget(m_codeFld, 2, 0);
	gridLayout->addWidget(m_checkFld, 2, 1, Qt::AlignLeft);
	gridLayout->addWidget(m_info, 3, 0);

	gridLayout->setMargin(0);
	gridLayout->setSpacing(0);
	widget->setLayout(gridLayout);

	addWidget(widget);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
	QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setMinimumSize(65, 25);
	cancelButton->setText(tr("Cancel"));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	buttonBox->setCenterButtons(true);

	// back
	m_upgradeButton = new QPushButton(tr("&Upgrade"));
	m_upgradeButton->setMinimumSize(65, 25);
	m_upgradeButton->setEnabled(false);
	buttonBox->addButton(m_upgradeButton, QDialogButtonBox::ActionRole);
	connect(m_upgradeButton, SIGNAL(pressed()), this, SLOT(upgrade()));
	m_buttonFrame->setObjectName("LicenseDialogButtonFrame");
	m_buttonFrame->setStyleSheet(
		"LicenseDialogButtonFrame: {border: 0px; background-color: rgb(225,225,225);}");
	addButtonBarWidget(buttonBox);
}
void UpgradeLicense::upgrade()
{
	std::string code = m_codeFld->text().toStdString();
	if (License::checkLicense(code) == License::PRO_LICENSE) {
		bool ok = License::setLicense(code);
		if (ok) {
			this->accept();
		}
	}
}
void UpgradeLicense::codeChanged(const QString &text)
{
	if (text.size() > 47 || text.size() == 0)
		return;
	int cursorPosition = m_codeFld->cursorPosition();
	bool enabled = false;

	// formattazione
	int i = 0;
	QString codeFormat;
	codeFormat = ">NNNNN";
	m_markCount = (text.size() - m_markCount) / 5;
	QString dim = text;
	dim = dim.remove("-", Qt::CaseInsensitive);

	if (m_markCount == 0)
		codeFormat = ">NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN";
	if (dim.size() % 5 == 0)
		m_markCount -= 1;
	for (i = 0; i < m_markCount; i++) {
		codeFormat += "-NNNNN";
	}
	if (dim.size() % 5 == 0 && dim.size() < 40)
		codeFormat += "N";
	m_codeFld->setInputMask(codeFormat);

	if (cursorPosition >= text.size())
		cursorPosition = dim.size() + m_markCount;
	m_codeFld->setCursorPosition(cursorPosition);

	QString qcode = text;
	qcode = qcode.remove("-", Qt::CaseInsensitive);
	std::string code = qcode.toStdString();
	bool enable = false;
	m_checkFld->setText(" ");
	m_info->setText(" ");
	if (License::checkLicense(code) == License::PRO_LICENSE) {
		enable = true;
		m_info->setTextFormat(Qt::RichText);
		m_info->setText(QString(tr("<font color=\"#006600\">Click the Upgrade button.</font>")));
		QPixmap pixmap(":Resources/check.png");
		m_checkFld->setPixmap(pixmap);
	}
	m_upgradeButton->setEnabled(enable);
}
void UpgradeLicense::slotCursorPositionChanged()
{
	QString currentCode = m_codeFld->text();
	if (currentCode == "")
		m_codeFld->setCursorPosition(0);
}
void UpgradeLicense::paintEvent(QPaintEvent *)
{
	// disegno il bordo
	QPainter painter(this);
	QPen pen; // = new QPen;
	pen.setColor(1);
	pen.setWidth(3);
	painter.setPen(pen);
	int height = this->height();
	int width = this->width();
	painter.drawRect(0, 0, width - 1, height - 1);
}
LicenseWizard::LicenseWizard(QWidget *parent) : Dialog(TApp::instance()->getMainWindow(), true)
{
	setStyleSheet("margin: 1px;");
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	// setWindowTitle(tr("Try, Buy or Activate?"));
	setTopMargin(0);
	QStackedWidget *stackedWidget = new QStackedWidget(this);
	stackedWidget->addWidget(createTryBuyActivatePage());
	stackedWidget->addWidget(createActivatePage());
	stackedWidget->addWidget(createConnectionPage());
	m_pages = stackedWidget;
	addWidget(stackedWidget);
	m_buttonFrame->setObjectName("LicenseDialogButtonFrame");
	m_buttonFrame->setStyleSheet(
		"LicenseDialogButtonFrame: {border: 0px; background-color: rgb(225,225,225);}");
	createTryBuyActivateButtonBox();
	gotoFirstPage();
}

void LicenseWizard::buy()
{
	extern const char *applicationVersion;
	extern const char *applicationName;
	//  string MacAddress=getValidMacAddress();
	QString qtabversion, qappname;
	string spacedtabversion = string(applicationVersion);
	string spacedappname = string(applicationName);

	qtabversion = QString::fromStdString(spacedtabversion);
	qtabversion.remove(" ");

	qappname = QString::fromStdString(spacedappname);
	qappname.remove(" ");

	std::wstring tabversion = qtabversion.toStdWString();
	std::wstring appname = qappname.toStdWString();

#ifdef WIN32
	std::wstring url(L"http://www.the-tab.com/cgi-shl/buy.asp?Version=" + tabversion + L"&Name=" +
					 appname + L"&MAC_Address=" + toWideString(License::getFirstValidMacAddress()));
	int ret = (int)

		ShellExecute(0, L"open", url.c_str(), 0, 0, SW_SHOWNORMAL);
	if (ret <= 32) {
		throw TException(url + L" : can't open");
	}
#else
	std::wstring url(L"http://www.the-tab.com/cgi-shl/buy.asp?Version=" + tabversion + L"\\&Name=" +
					 appname + L"\\&MAC_Address=" +
					 toWideString(License::getFirstValidMacAddress()));

	string cmd = "open ";
	cmd = cmd + toString(url);
	system(cmd.c_str());
#endif
}

#ifdef LINETEST
void LicenseWizard::tryAct()
{
	if (License::checkLicense() == License::INVALID_LICENSE) {
		DVGui::requestTrialLicense();
		return;
	} else
		accept();
}
#endif

void LicenseWizard::gotoConnectionPage()
{
	clearButtonBar();
	m_activationPageButtonBox->hide();
	m_TryBuyActivationPageButtonBox->hide();
	addButtonBarWidget(m_connectionButtonBox);
	m_connectionButtonBox->show();
	m_pages->setCurrentIndex(2);
}

void LicenseWizard::gotoActivatePage()
{
	clearButtonBar();
	m_connectionButtonBox->hide();
	m_TryBuyActivationPageButtonBox->hide();
	addButtonBarWidget(m_activationPageButtonBox);
	m_activationPageButtonBox->show();
	m_pages->setCurrentIndex(1);
}

void LicenseWizard::gotoFirstPage()
{
	clearButtonBar();
	m_activationPageButtonBox->hide();
	m_connectionButtonBox->hide();
	addButtonBarWidget(m_TryBuyActivationPageButtonBox);
	m_TryBuyActivationPageButtonBox->show();
	m_pages->setCurrentIndex(0);
}
