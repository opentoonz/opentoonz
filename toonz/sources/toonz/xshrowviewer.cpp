

#include "xshrowviewer.h"
#include "xsheetviewer.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "xsheetdragtool.h"
#include "toonzqt/gutil.h"
#include "onionskinmaskgui.h"
#include "cellselection.h"
#include "menubarcommandids.h"
#include "toonzqt/menubarcommand.h"

#include "toonz/toonzscene.h"
#include "tconvert.h"

#include "toonz/txsheet.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"

#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QToolTip>

//=============================================================================

namespace XsheetGUI
{

//=============================================================================
// RowArea
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
RowArea::RowArea(XsheetViewer *parent, Qt::WindowFlags flags)
#else
RowArea::RowArea(XsheetViewer *parent, Qt::WFlags flags)
#endif
	: QWidget(parent, flags), m_viewer(parent), m_xa(ColumnWidth / 2 - 12), m_row(-1), m_showOnionToSet(false), m_pos(-1, -1), m_playRangeActiveInMousePress(false), m_mousePressRow(-1), m_tooltip(tr("")), m_r0(0), m_r1(5), m_isPanning(false)
{
	setFocusPolicy(Qt::NoFocus);
	setMouseTracking(true);
	connect(TApp::instance()->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()),
			this, SLOT(update()));
}

//-----------------------------------------------------------------------------

RowArea::~RowArea()
{
}

//-----------------------------------------------------------------------------

DragTool *RowArea::getDragTool() const { return m_viewer->getDragTool(); }
void RowArea::setDragTool(DragTool *dragTool) { m_viewer->setDragTool(dragTool); }

//-----------------------------------------------------------------------------

void RowArea::drawRows(QPainter &p, int r0, int r1)
{
	int playR0, playR1, step;
	XsheetGUI::getPlayRange(playR0, playR1, step);

	if (!XsheetGUI::isPlayRangeEnabled()) {
		TXsheet *xsh = m_viewer->getXsheet();
		playR1 = xsh->getFrameCount() - 1;
		playR0 = 0;
	}

#ifdef _WIN32
	static QFont font("Arial", XSHEET_FONT_SIZE, QFont::Bold);
#else
	static QFont font("Helvetica", XSHEET_FONT_SIZE, QFont::Normal);
#endif
	p.setFont(font);

	// marker interval
	int distance, offset;
	TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(distance, offset);

	//default value
	if (distance == 0)
		distance = 6;

	QRect visibleRect = visibleRegion().boundingRect();

	int x0 = visibleRect.left();
	int x1 = visibleRect.right();
	int y0 = visibleRect.top();
	int y1 = visibleRect.bottom();

	for (int r = r0; r <= r1; r++) {
		int y = m_viewer->rowToY(r);

		//--- draw horizontal line
		QColor color = ((r - offset) % distance != 0) ? m_viewer->getLightLineColor() : m_viewer->getMarkerLineColor();
		p.setPen(color);
		p.drawLine(x0, y, x1, y);

		// draw frame text
		if (playR0 <= r && r <= playR1) {
			p.setPen(((r - m_r0) % step == 0) ? m_viewer->getPreviewFrameTextColor() : m_viewer->getTextColor());
		}
		//not in preview range
		else
			p.setPen(m_viewer->getTextColor());

		switch (m_viewer->getFrameDisplayStyle()) {
		case XsheetViewer::SecAndFrame: {
			int frameRate = TApp::instance()->getCurrentScene()->getScene()->getProperties()->getOutputProperties()->getFrameRate();
			QString str;
			int koma = (r + 1) % frameRate;
			if (koma == 1) {
				int sec = (r + 1) / frameRate;
				str = QString("%1' %2\"").arg(QString::number(sec).rightJustified(2, '0')).arg(QString::number(koma).rightJustified(2, '0'));
			} else {
				if (koma == 0)
					koma = frameRate;
				str = QString("%1\"").arg(QString::number(koma).rightJustified(2, '0'));
			}

			p.drawText(QRect(width() / 2 - 15, y + 1, width() / 2 + 7, RowHeight - 2), Qt::AlignRight | Qt::AlignBottom, str);

			break;
		}

		case XsheetViewer::Frame: {
			QString number = QString::number(r + 1);
			p.drawText(QRect(width() / 2 - 2, y + 1, width() / 2, RowHeight - 2), Qt::AlignHCenter | Qt::AlignBottom, number);
			break;
		}

		//6 second sheet (144frames per page)
		case XsheetViewer::SixSecSheet: {
			int frameRate = TApp::instance()->getCurrentScene()->getScene()->getProperties()->getOutputProperties()->getFrameRate();
			QString str;
			int koma = (r + 1) % (frameRate * 6);
			if ((r + 1) % frameRate == 1) {
				int page = (r + 1) / (frameRate * 6) + 1;
				str = QString("p%1  %2").arg(QString::number(page)).arg(QString::number(koma).rightJustified(3, '0'));
			} else {
				if (koma == 0)
					koma = frameRate * 6;
				str = QString("%1").arg(QString::number(koma).rightJustified(3, '0'));
			}
			p.drawText(QRect(width() / 2 - 21, y + 1, width() / 2 + 7, RowHeight - 2), Qt::AlignRight | Qt::AlignBottom, str);
			break;
		}
		//3 second sheet (72frames per page)
		case XsheetViewer::ThreeSecSheet: {
			int frameRate = TApp::instance()->getCurrentScene()->getScene()->getProperties()->getOutputProperties()->getFrameRate();
			QString str;
			int koma = (r + 1) % (frameRate * 3);
			if ((r + 1) % frameRate == 1) {
				int page = (r + 1) / (frameRate * 3) + 1;
				str = QString("p%1  %2").arg(QString::number(page)).arg(QString::number(koma).rightJustified(2, '0'));
			} else {
				if (koma == 0)
					koma = frameRate * 3;
				str = QString("%1").arg(QString::number(koma).rightJustified(2, '0'));
			}
			p.drawText(QRect(width() / 2 - 21, y + 1, width() / 2 + 7, RowHeight - 2), Qt::AlignRight | Qt::AlignBottom, str);
			break;
		}
		}
	}

	// hide the top-most horizontal line
	if (r0 == 0) {
		p.setPen(m_viewer->getLightLineColor());
		p.drawLine(x0, m_viewer->rowToY(0), x1, m_viewer->rowToY(0));
	}
}

//-----------------------------------------------------------------------------

void RowArea::drawPlayRange(QPainter &p, int r0, int r1)
{
	bool playRangeEnabled = XsheetGUI::isPlayRangeEnabled();

	// Update the play range internal fields
	if (playRangeEnabled) {
		int step;
		XsheetGUI::getPlayRange(m_r0, m_r1, step);
	}
	// if the preview range is not set, then put markers at the first and the last frames of the scene
	else {
		TXsheet *xsh = m_viewer->getXsheet();
		m_r1 = xsh->getFrameCount() - 1;
		if (m_r1 == -1)
			return;
		m_r0 = 0;
	}

	QColor ArrowColor = (playRangeEnabled) ? QColor(255, 255, 255) : grey150;

	if (m_r0 > r0 - 1 && r1 + 1 > m_r0) {
		int y0 = m_viewer->rowToY(m_r0);
		drawArrow(p, QPointF(m_xa, y0 + 1),
				  QPointF(m_xa + 10, y0 + 1),
				  QPointF(m_xa, y0 + 11),
				  true, ArrowColor);
	}

	if (m_r1 > r0 - 1 && r1 + 1 > m_r1) {
		int y1 = m_viewer->rowToY(m_r1 + 1) - 12;
		drawArrow(p, QPointF(m_xa, y1 + 1),
				  QPointF(m_xa + 10, y1 + 11),
				  QPointF(m_xa, y1 + 11),
				  true, ArrowColor);
	}
}

//-----------------------------------------------------------------------------

void RowArea::drawCurrentRowGadget(QPainter &p, int r0, int r1)
{
	int currentRow = m_viewer->getCurrentRow();
	int y = m_viewer->rowToY(currentRow);
	if (currentRow < r0 || r1 < currentRow)
		return;

	p.fillRect(1, y + 1, width() - 4, RowHeight - 1, m_viewer->getCurrentRowBgColor());
}

//-----------------------------------------------------------------------------

void RowArea::drawOnionSkinSelection(QPainter &p)
{
	TApp *app = TApp::instance();
	OnionSkinMask osMask = app->getCurrentOnionSkin()->getOnionSkinMask();

	TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
	assert(xsh);
	int currentRow = m_viewer->getCurrentRow();

	if (m_showOnionToSet) {
		int y = m_viewer->rowToY(m_row) + 3;
		QRect rect(m_xa - 6, y + 1, 4, 4);
		p.setPen(m_viewer->getDarkLineColor());
		p.drawRect(rect);
		p.fillRect(rect.adjusted(1, 1, 0, 0), QBrush(m_viewer->getLightLineColor()));
	}

	if (!osMask.isEnabled())
		return;

	int i;
	for (i = 0; i < osMask.getFosCount(); i++) {
		int fos = osMask.getFos(i);
		if (fos == currentRow)
			continue;
		int y = m_viewer->rowToY(fos) + 3;
		QRect rect(m_xa - 6, y + 1, 4, 4);
		p.setPen(Qt::black);
		p.drawRect(rect);
		p.fillRect(rect.adjusted(1, 1, 0, 0), QBrush(m_viewer->getDarkLineColor()));
	}
	int lastY;
	int xc = m_xa - 10;
#ifndef STUDENT
	int mosCount = osMask.getMosCount();
	for (i = 0; i < mosCount; i++) {
		int mos = osMask.getMos(i);
		int y = m_viewer->rowToY(currentRow + mos) + 3;
		QRect rect(m_xa - 12, y + 1, 4, 4);
		p.setPen(Qt::black);
		p.drawRect(rect);
		p.fillRect(rect.adjusted(1, 1, 0, 0), QBrush(m_viewer->getLightLineColor()));
		if (i > 0 || mos > 0) {
			int ya = y;
			int yb;
			if (i == 0 || mos > 0 && osMask.getMos(i - 1) < 0)
				yb = m_viewer->rowToY(currentRow) + RowHeight;
			else
				yb = lastY + 5;
			p.setPen(Qt::black);
			p.drawLine(xc, ya, xc, yb);
		}
		lastY = y;
		if (mos < 0 && (i == mosCount - 1 || osMask.getMos(i + 1) > 0)) {
			int ya = m_viewer->rowToY(currentRow);
			int yb = lastY + 5;
			p.setPen(Qt::black);
			p.drawLine(xc, ya, xc, yb);
		}
	}
#endif
}

//-----------------------------------------------------------------------------

void RowArea::paintEvent(QPaintEvent *event)
{
	QRect toBeUpdated = event->rect();

	QPainter p(this);

	int r0, r1; // range di righe visibili
	r0 = m_viewer->yToRow(toBeUpdated.top());
	r1 = m_viewer->yToRow(toBeUpdated.bottom());

	p.setClipRect(toBeUpdated);

	//fill background
	p.fillRect(toBeUpdated, m_viewer->getBGColor());

	if (TApp::instance()->getCurrentFrame()->isEditingScene()) {
		//current frame
		drawCurrentRowGadget(p, r0, r1);
		//TODO: Scene上のオニオンスキン表示、オプション化するか検討 2016/1/6 shun_iwasawa
		//drawOnionSkinSelection(p);
	}

	drawRows(p, r0, r1);
	drawPlayRange(p, r0, r1);
}

//-----------------------------------------------------------------------------

void RowArea::mousePressEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(event->modifiers());
	if (event->button() == Qt::LeftButton) {
		bool playRangeModifierisClicked = false;

		TApp *app = TApp::instance();
		TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
		TPoint pos(event->pos().x(), event->pos().y());

		int row = m_viewer->yToRow(pos.y);

		QRect visibleRect = visibleRegion().boundingRect();
		int playR0, playR1, step;
		XsheetGUI::getPlayRange(playR0, playR1, step);

		bool playRangeEnabled = playR0 <= playR1;
		if (!playRangeEnabled) {
			TXsheet *xsh = m_viewer->getXsheet();
			playR1 = xsh->getFrameCount() - 1;
			playR0 = 0;
		}

		if (playR1 == -1) { //getFrameCount = 0 i.e. xsheet is empty
			setDragTool(XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
		} else if (m_xa <= pos.x && pos.x <= m_xa + 10 && (row == playR0 || row == playR1)) {
			if (!playRangeEnabled)
				XsheetGUI::setPlayRange(playR0, playR1, step);
			setDragTool(XsheetGUI::DragTool::makePlayRangeModifierTool(m_viewer));
			playRangeModifierisClicked = true;
		} else
			setDragTool(XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));

		//when shift+click the row area, select a single row region in the cell area
		if (!playRangeModifierisClicked && 0 != (event->modifiers() & Qt::ShiftModifier)) {
			int filledCol;
			for (filledCol = xsh->getColumnCount() - 1; filledCol >= 0; filledCol--) {
				TXshColumn *currentColumn = xsh->getColumn(filledCol);
				if (!currentColumn)
					continue;
				if (!currentColumn->isEmpty())
					break;
			}

			m_viewer->getCellSelection()->selectNone();
			m_viewer->getCellSelection()->selectCells(row, 0, row, tmax(0, filledCol));
			m_viewer->updateCellRowAree();
		}

		m_viewer->dragToolClick(event);
		event->accept();
	} // left-click
	// pan by middle-drag
	else if (event->button() == Qt::MidButton) {
		m_pos = event->pos();
		m_isPanning = true;
	}
}

//-----------------------------------------------------------------------------

void RowArea::mouseMoveEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(event->modifiers());
	QPoint pos = event->pos();

	// pan by middle-drag
	if (m_isPanning) {
		QPoint delta = m_pos - pos;
		delta.setX(0);
		m_viewer->scroll(delta);
		return;
	}

	m_row = m_viewer->yToRow(pos.y());
	int x = pos.x();

	if ((event->buttons() & Qt::LeftButton) != 0 && !visibleRegion().contains(pos)) {
		QRect bounds = visibleRegion().boundingRect();
		m_viewer->setAutoPanSpeed(bounds, QPoint(bounds.left(), pos.y()));
	} else
		m_viewer->stopAutoPan();

	m_pos = pos;

	m_viewer->dragToolDrag(event);
	if (getDragTool()) {
		m_showOnionToSet = false;
		return;
	}

	int currentRow = TApp::instance()->getCurrentFrame()->getFrame();
	int row = m_viewer->yToRow(m_pos.y());
	if (row < 0)
		return;
	// "decide" se mostrare la possibilita' di settare l'onion skin
	if (7 <= x && x <= 13 && row != currentRow)
		m_showOnionToSet = true;
	else
		m_showOnionToSet = false;
	update();

	if (m_xa <= x && x <= m_xa + 10 && row == m_r0)
		m_tooltip = tr("Playback Start Marker");
	else if (m_xa <= x && x <= m_xa + 10 && row == m_r1)
		m_tooltip = tr("Playback End Marker");
	else if (row == currentRow)
		m_tooltip = tr("Curren Frame");
	else
		m_tooltip = tr("");
}

//-----------------------------------------------------------------------------

void RowArea::mouseReleaseEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(0);
	m_viewer->stopAutoPan();
	m_isPanning = false;
	m_viewer->dragToolRelease(event);

	TPoint pos(event->pos().x(), event->pos().y());

	int row = m_viewer->yToRow(pos.y);
	if (m_playRangeActiveInMousePress && row == m_mousePressRow &&
		(13 <= pos.x && pos.x <= 26 && (row == m_r0 || row == m_r1)))
		onRemoveMarkers();
}

//-----------------------------------------------------------------------------

void RowArea::contextMenuEvent(QContextMenuEvent *event)
{
#ifndef STUDENT

	OnionSkinMask osMask = TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();

	QMenu *menu = new QMenu(this);
	QAction *setStartMarker = menu->addAction(tr("Set Start Marker"));
	connect(setStartMarker, SIGNAL(triggered()), SLOT(onSetStartMarker()));
	QAction *setStopMarker = menu->addAction(tr("Set Stop Marker"));
	connect(setStopMarker, SIGNAL(triggered()), SLOT(onSetStopMarker()));
	QAction *removeMarkers = menu->addAction(tr("Remove Markers"));
	connect(removeMarkers, SIGNAL(triggered()), SLOT(onRemoveMarkers()));

	//set both the from and to markers at the specified row
	QAction *previewThis = menu->addAction(tr("Preview This"));
	connect(previewThis, SIGNAL(triggered()), SLOT(onPreviewThis()));

	menu->addSeparator();

	CommandManager *cmdManager = CommandManager::instance();
	menu->addAction(cmdManager->getAction(MI_InsertSceneFrame));
	menu->addAction(cmdManager->getAction(MI_RemoveSceneFrame));
	menu->addAction(cmdManager->getAction(MI_InsertGlobalKeyframe));
	menu->addAction(cmdManager->getAction(MI_RemoveGlobalKeyframe));

#ifndef LINETEST
	menu->addSeparator();
	menu->addAction(cmdManager->getAction(MI_ShiftTrace));
	menu->addAction(cmdManager->getAction(MI_EditShift));
	menu->addAction(cmdManager->getAction(MI_NoShift));
	menu->addAction(cmdManager->getAction(MI_ResetShift));
#endif

	menu->exec(event->globalPos());
#endif
}

//-----------------------------------------------------------------------------

bool RowArea::event(QEvent *event)
{
	if (event->type() == QEvent::ToolTip) {
		if (!m_tooltip.isEmpty())
			QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
		else
			QToolTip::hideText();
	}
	return QWidget::event(event);
}

//-----------------------------------------------------------------------------

void RowArea::setMarker(int index)
{
	assert(m_row >= 0);
	// I use only the step value..
	int unused0, unused1, step;
	getPlayRange(unused0, unused1, step);
	if (m_r0 > m_r1) {
		m_r0 = 0;
		m_r1 = TApp::instance()->getCurrentScene()->getScene()->getFrameCount() - 1;
		if (m_r1 < 1)
			m_r1 = 1;
	}
	if (index == 0) {
		m_r0 = m_row;
		if (m_r1 < m_r0)
			m_r1 = m_r0;
	} else if (index == 1) {
		m_r1 = m_row;
		if (m_r1 < m_r0)
			m_r0 = m_r1;
		m_r1 -= (step == 0) ? (m_r1 - m_r0) : (m_r1 - m_r0) % step;
	}
	setPlayRange(m_r0, m_r1, step);
}

//-----------------------------------------------------------------------------

void RowArea::onSetStartMarker()
{
	setMarker(0);
	update();
}

//-----------------------------------------------------------------------------

void RowArea::onSetStopMarker()
{
	setMarker(1);
	update();
}

//-----------------------------------------------------------------------------
//set both the from and to markers at the specified row
void RowArea::onPreviewThis()
{
	assert(m_row >= 0);
	int r0, r1, step;
	getPlayRange(r0, r1, step);
	setPlayRange(m_row, m_row, step);
	update();
}

//-----------------------------------------------------------------------------

void RowArea::onRemoveMarkers()
{
	int step;
	XsheetGUI::getPlayRange(m_r0, m_r1, step);
	XsheetGUI::setPlayRange(0, -1, step);
	update();
}

//-----------------------------------------------------------------------------

} // namespace XsheetGUI;
