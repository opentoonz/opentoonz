#pragma once

#ifndef FILMSTRIP_H
#define FILMSTRIP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/selection.h"

// Qt includes
#include <QScrollArea>
#include <QKeyEvent>

// STD includes
#include <vector>
#include <map>

// forward declaration
class TFrameId;
class TFilmstripSelection;
class FilmstripFrameHeadGadget;
class TXshSimpleLevel;
class QComboBox;
class InbetweenDialog;
class TXshLevel;

const int fs_leftMargin = 2;
const int fs_rightMargin = 3;
const int fs_frameSpacing = 6;
const int fs_iconMarginLR = 5;
const int fs_iconMarginTop = 5;
const int fs_iconMarginBottom = 15;

//=============================================================================
// FilmstripFrames
// (inserita dentro Filmstrip : QScrollArea)
//-----------------------------------------------------------------------------

class FilmstripFrames : public QFrame, public TSelection::View
{
	Q_OBJECT

public:
#if QT_VERSION >= 0x050500
	FilmstripFrames(QScrollArea *parent = 0, Qt::WindowFlags flags = 0);
#else
	FilmstripFrames(QScrollArea *parent = 0, Qt::WFlags flags = 0);
#endif
	~FilmstripFrames();

	void setBGColor(const QColor &color) { m_bgColor = color; }
	QColor getBGColor() const { return m_bgColor; }
	void setLightLineColor(const QColor &color) { m_lightLineColor = color; }
	QColor getLightLineColor() const { return m_lightLineColor; }
	void setDarkLineColor(const QColor &color) { m_darkLineColor = color; }
	QColor getDarkLineColor() const { return m_darkLineColor; }

	// helper method: ritorna il livello corrente
	TXshSimpleLevel *getLevel() const;

	QSize getIconSize() const { return m_iconSize; }
	int getFrameLabelWidth() const { return m_frameLabelWidth; }

	// la y si riferisce al margine superiore dell'iconcina index-esima
	int y2index(int y) const;
	int index2y(int index) const;

	// se c'e' un livello e 0<=index<frameCount ritorna il frameid index-esimo
	// altrimenti ritorna TFrameId()
	TFrameId index2fid(int index) const;

	// se c'e' un livello e fid e' un frame del livello ritorna l'indice. altrimenti -1
	int fid2index(const TFrameId &fid) const;

	// restituisce l'altezza dei frames esistenti (piu' uno vuoto)
	int getFramesHeight() const;

	// aggiorna le dimensioni del QWidget in base al numero di fotogrammi del livello
	// la dimensione verticale e' sempre >= minimumHeight. Questo permette di gestire
	// lo scroll anche oltre i frame esistenti.
	// se minimumHeight == -1 viene utilizzato visibleRegion().boundingRect().bottom()
	void updateContentHeight(int minimumHeight = -1);

	// assicura che il frame index-esimo sia visibile (eventualmente facendo scroll)
	void exponeFrame(int index);

	// esegue uno scroll di dy pixel. se dy<0 fa scorrere i fotogrammi verso l'alto
	// lo scroll e' illimitato verso il basso. aggiorna contentHeight
	void scroll(int dy);

	// overriden from TSelection::View
	void onSelectionChanged() { update(); }

	enum SelectionMode {
		SIMPLE_SELECT,
		SHIFT_SELECT,
		CTRL_SELECT,
		START_DRAG_SELECT,
		DRAG_SELECT,
		ONLY_SELECT
	};
	void select(int index, SelectionMode mode = SIMPLE_SELECT);

	int getOneFrameHeight();

protected:
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);
	void paintEvent(QPaintEvent *);

	enum { F_NORMAL = 0,
		   F_INBETWEEN_RANGE = 0x1,
		   F_INBETWEEN_LAST = 0x2 }; // Flags
	void drawFrameIcon(QPainter &p, const QRect &r, int index, const TFrameId &fid, int flags);

	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *);
	void keyPressEvent(QKeyEvent *event);
	void wheelEvent(QWheelEvent *event);

	void startAutoPanning();
	void stopAutoPanning();
	void timerEvent(QTimerEvent *);

	void contextMenuEvent(QContextMenuEvent *event);

	void startDragDrop();

	void inbetween();

	void execNavigatorPan(const QPoint &point);
	void mouseDoubleClickEvent(QMouseEvent *event);

protected slots:
	void onLevelChanged();
	void onLevelSwitched(TXshLevel *);
	void onFrameSwitched();

private:
	// QSS Properties

	QColor m_bgColor;
	QColor m_lightLineColor;
	QColor m_darkLineColor;

	Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)
	Q_PROPERTY(QColor LightLineColor READ getLightLineColor WRITE setLightLineColor)
	Q_PROPERTY(QColor DarkLineColor READ getDarkLineColor WRITE setDarkLineColor)

private:
	// Widgets

	QScrollArea *m_scrollArea;
	TFilmstripSelection *m_selection;
	FilmstripFrameHeadGadget *m_frameHeadGadget;
	InbetweenDialog *m_inbetweenDialog;

	// State data

	QPoint m_pos; //!< Last mouse position.

	const QSize m_iconSize;
	const int m_frameLabelWidth;

	std::pair<int, int> m_selectingRange;

	int m_scrollSpeed,
		m_dragSelectionStartIndex, //!< Starting level index during drag selections.
		m_dragSelectionEndIndex,   //!< Ending level index during drag selections.
		m_timerId;				   // per l'autoscroll

	bool m_selecting,
		m_dragDropArmed,
		m_readOnly;

	QPoint m_naviRectPos;
	QPointF m_icon2ViewerRatio;
	bool m_isNavigatorPanning;
};

//=============================================================================
// Filmstrip
//-----------------------------------------------------------------------------

class Filmstrip : public QWidget
{
	Q_OBJECT

	FilmstripFrames *m_frames;

	QScrollArea *m_frameArea;
	QComboBox *m_chooseLevelCombo;

	std::vector<TXshSimpleLevel *> m_levels;
	std::map<TXshSimpleLevel *, TFrameId> m_workingFrames;

public:
#if QT_VERSION >= 0x050500
	Filmstrip(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
	Filmstrip(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
	~Filmstrip();

protected:
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);
	void resizeEvent(QResizeEvent *);
	//void keyPressEvent(QKeyEvent* event){
	//  event->ignore();
	//}

public slots:
	//!Aggiorna il "titolo" del widget e rinfresca la filmstrip se c'e' qualche "check" attivo.
	void onLevelSwitched(TXshLevel *oldLevel);
	// void ensureValuesVisible(int x, int y);

	void onSliderMoved(int);
	void onLevelChanged();
	// update combo items when the contents of scene cast are changed
	void updateChooseLevelComboItems();
	// change the current level when the combo item selected
	void onChooseLevelComboChanged(int index);

	void onFrameSwitched();

private:
	void updateWindowTitle();
	//synchronize the current index of combo to the current level
	void updateCurrentLevelComboItem();
};

//=============================================================================
// inbetweenDialog
//-----------------------------------------------------------------------------

class InbetweenDialog : public DVGui::Dialog
{
	Q_OBJECT
	QComboBox *m_comboBox;

public:
	InbetweenDialog(QWidget *parent);

	void setValue(const QString &value);
	QString getValue();

	int getIndex(const QString &text);
};

#endif // FILMSTRIP_H
