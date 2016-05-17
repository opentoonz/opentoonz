#pragma once

#ifndef SCANPOPUP_H
#define SCANPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tscanner.h"
#include "scanlist.h"

// forward declaration
namespace DVGui
{
class DoubleField;
class IntField;
class CheckBox;
}

class QComboBox;
class ProgressDialog;

//=============================================================================
// MyScannerListener
//-----------------------------------------------------------------------------

class MyScannerListener : public QObject,
						  public TScannerListener
{
	Q_OBJECT

	int m_current;
	int m_inc;
	ScanList m_scanList;
	bool m_isCanceled, m_isPreview;
	DVGui::ProgressDialog *m_progressDialog;

public:
	MyScannerListener(const ScanList &scanList);
	void onImage(const TRasterImageP &);
	void onError();
	void onNextPaper();
	void onAutomaticallyNextPaper();
	bool isCanceled();

protected slots:
	void cancelButtonPressed();
};

//=============================================================================
// DefineScannerPopup
//-----------------------------------------------------------------------------

class DefineScannerPopup : public DVGui::Dialog
{
	Q_OBJECT
	QComboBox *m_scanDriverOm;

public:
	DefineScannerPopup();

public slots:
	void accept();
};

//=============================================================================
// ScanSettingsPopup
//-----------------------------------------------------------------------------

class ScanSettingsPopup : public DVGui::Dialog
{
	Q_OBJECT
	QLabel *m_scannerNameLbl;
	DVGui::CheckBox *m_reverseOrderCB;
	DVGui::CheckBox *m_paperFeederCB;
	QComboBox *m_modeOm;
	DVGui::DoubleField *m_dpi;
	DVGui::IntField *m_threshold;
	DVGui::IntField *m_brightness;
	QComboBox *m_paperFormatOm;
	QLabel *m_formatLbl;
	QLabel *m_modeLbl;
	QLabel *m_thresholdLbl;
	QLabel *m_brightnessLbl;
	QLabel *m_dpiLbl;

public:
	ScanSettingsPopup();

protected:
	void showEvent(QShowEvent *event);
	void hideEvent(QHideEvent *event);
	void connectAll();
	void disconnectAll();

public slots:
	void updateUI();
	void onToggle(int);
	void onPaperChanged(const QString &format);
	void onValueChanged(bool);
	void onModeChanged(const QString &mode);
};

#ifdef LINETEST
//=============================================================================
// AutocenterPopup
//-----------------------------------------------------------------------------

class AutocenterPopup : public DVGui::Dialog
{
	Q_OBJECT
	DVGui::CheckBox *m_autocenter;
	QComboBox *m_pegbarHoles;
	QComboBox *m_fieldGuide;

public:
	AutocenterPopup();

protected:
	void showEvent(QShowEvent *event);

protected slots:
	void onAutocenterToggled(bool);
	void onPegbarHolesChanged(const QString &);
	void onFieldGuideChanged(const QString &);
};
#endif //LINETEST
#endif // SCANPOPUP_H
