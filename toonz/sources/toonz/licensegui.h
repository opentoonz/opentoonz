

#ifndef LICENSEGUI_INCLUDED
#define LICENSEGUI_INCLUDED

#include "toonzqt/dvdialog.h"

#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>

class QStackedWidget;
class QLineEdit;
class QLabel;
class QPushButton;

using namespace DVGui;

class LicenseWizardPage : public QWidget
{
	Q_OBJECT
	QString m_screenPath;
	QVBoxLayout *m_layout;

  public:
	LicenseWizardPage(QWidget *parent = 0);
	void changeSplashScreen(int id);
	void addWidget(QWidget *widget);

  protected:
	void paintEvent(QPaintEvent *event);
};

class LicenseWizard : public Dialog
{
	Q_OBJECT
	QStackedWidget *m_pages;
	LicenseWizard *m_licenseWizard;
	QDialogButtonBox *m_activationPageButtonBox;
	QDialogButtonBox *m_TryBuyActivationPageButtonBox;
	QDialogButtonBox *m_connectionButtonBox;

	QLineEdit *m_codeFld;
	QLabel *m_checkFld;
	QLabel *m_info;
	QPushButton *m_activateBtn;
	QString m_codeFormat; // gestisce la formattazione(i trattini) di attivationCode e License.
	int m_markCount;

	QWidget *createTryBuyActivatePage();
	QWidget *createActivatePage();
	QWidget *createConnectionPage();

  public:
	LicenseWizard(QWidget *parent = 0);
	QLabel *m_stateConnectionLbl;
	QString getDaysLeftString(int dayLeft) const;
	QString getLicenseStatusString() const;
	void setPage(int pageIndex) { m_pages->setCurrentIndex(pageIndex); }
  protected:
	void paintEvent(QPaintEvent *);

  protected slots:
	void buy();
#ifdef LINETEST
	void tryAct();
#endif
	void gotoActivatePage();
	void gotoConnectionPage();
	void gotoFirstPage();
	void codeChanged(const QString &text);
	void slotCursorPositionChanged();
	void activate();

  private:
	LicenseWizard(const LicenseWizard &);
	void createActivateButtonBox();
	void createConnectionButtonBox();
	void createTryBuyActivateButtonBox();
	void operator=(const LicenseWizard &);
};

class UpgradeLicense : public Dialog
{
	Q_OBJECT
	QPushButton *m_upgradeButton;
	QLabel *m_checkFld;
	QLabel *m_info;
	QLineEdit *m_codeFld;
	int m_markCount;

  public:
	UpgradeLicense(QWidget *parent = 0);
  protected slots:
	void upgrade();
	void codeChanged(const QString &text);
	void slotCursorPositionChanged();

  protected:
	void paintEvent(QPaintEvent *event);
};

class MyWidget : public QWidget
{
	Q_OBJECT
	QPixmap *m_pixmap;

  public:
	MyWidget(QWidget *parent = 0);

  protected:
	void paintEvent(QPaintEvent *event);
};

#endif
