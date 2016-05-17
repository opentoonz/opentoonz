#pragma once

#ifndef BRIGHTNESSANDCONTRASTPOPUP_H
#define BRIGHTNESSANDCONTRASTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include "traster.h"

class QSlider;
class ImageViewer;
class TSelection;

namespace DVGui
{
class IntField;
}

//=============================================================================
// BrightnessAndContrastPopup
//-----------------------------------------------------------------------------

class BrightnessAndContrastPopup : public DVGui::Dialog
{
	Q_OBJECT

	DVGui::IntField *m_brightnessField;
	DVGui::IntField *m_contrastField;
	QPushButton *m_okBtn;
	TRasterP m_startRas;

private:
	class Swatch;
	Swatch *m_viewer;

public:
	BrightnessAndContrastPopup();

protected:
	void showEvent(QShowEvent *e);
	void hideEvent(QHideEvent *e);

protected slots:

	void setCurrentSampleRaster();

public slots:

	void apply();
	void onValuesChanged(bool dragging);
};

#endif // BRIGHTNESSANDCONTRASTPOPUP_H
