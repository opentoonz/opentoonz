

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
#include "toonz/preferences.h"

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
	: QWidget(parent, flags), m_viewer(parent), m_xa(ColumnWidth / 2 - 12), m_row(-1), m_showOnionToSet(None), m_pos(-1, -1), m_playRangeActiveInMousePress(false), m_mousePressRow(-1), m_tooltip(tr("")), m_r0(0), m_r1(5), m_isPanning(false)
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
	p.setBrush(QBrush(ArrowColor));

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

	// get onion colors
	TPixel frontPixel, backPixel;
	bool inksOnly;
	Preferences::instance()->getOnionData(frontPixel, backPixel, inksOnly);
	QColor frontColor((int)frontPixel.r, (int)frontPixel.g, (int)frontPixel.b, 128);
	QColor backColor((int)backPixel.r, (int)backPixel.g, (int)backPixel.b, 128);

	int onionDotDiam = 8;
	int onionHandleDiam = RowHeight - 1;
	int onionDotYPos = (RowHeight - onionDotDiam) / 2;

	// If the onion skin is disabled, draw dash line instead.
	if (osMask.isEnabled())
		p.setPen(Qt::red);
	else
	{
		QPen currentPen = p.pen();
		currentPen.setStyle(Qt::DashLine);
		currentPen.setColor(QColor(128, 128, 128, 255));
		p.setPen(currentPen);
	}

	// Draw onion skin extender handles.
	QRectF handleRect(3, m_viewer->rowToY(currentRow) + 1, onionHandleDiam, onionHandleDiam );
	int angle180 = 16 * 180;
	p.setBrush(QBrush(backColor));
	p.drawChord(handleRect, 0, angle180);
	p.setBrush(QBrush(frontColor));
	p.drawChord(handleRect, angle180, angle180);

	//-- draw movable onions

	// draw line between onion skin range
	int minMos = 0;
	int maxMos = 0;
	int mosCount = osMask.getMosCount();
	for (int i = 0; i < mosCount; i++) {
		int mos = osMask.getMos(i);
		if (minMos > mos)
			minMos = mos;
		if (maxMos < mos)
			maxMos = mos;
	}
	p.setBrush(Qt::NoBrush);
	if (minMos < 0) // previous frames
	{
		int y0 = m_viewer->rowToY(currentRow + minMos) + onionDotYPos + onionDotDiam;
		int y1 = m_viewer->rowToY(currentRow);
		p.drawLine(onionDotDiam*1.5, y0, onionDotDiam*1.5, y1);
	}
	if (maxMos > 0) // foward frames
	{
		int y0 = m_viewer->rowToY(currentRow + 1);
		int y1 = m_viewer->rowToY(currentRow + maxMos) + onionDotYPos;
		p.drawLine(onionDotDiam*1.5, y0, onionDotDiam*1.5, y1);
	}

	// draw onion skin dots
	p.setPen(Qt::red);
	for (int i = 0; i < mosCount; i++) {
		// mos : frame offset from the current frame
		int mos = osMask.getMos(i);
		// skip drawing if the frame is under the mouse cursor
		if (m_showOnionToSet == Mos && currentRow + mos == m_row)
			continue;
		int y = m_viewer->rowToY(currentRow + mos) + onionDotYPos;

		if (osMask.isEnabled())
			p.setBrush(mos < 0 ? backColor : frontColor);
		else
			p.setBrush(Qt::NoBrush);
		p.drawEllipse(onionDotDiam, y, onionDotDiam, onionDotDiam);
	}

	//-- draw fixed onions
	for (int i = 0; i < osMask.getFosCount(); i++) 
	{
		int fos = osMask.getFos(i);
		if (fos == currentRow) continue;
		// skip drawing if the frame is under the mouse cursor
		if (m_showOnionToSet == Fos && fos == m_row)
			continue;
		int y = m_viewer->rowToY(fos) + onionDotYPos;
		
		if (osMask.isEnabled())
			p.setBrush(QBrush(QColor(0, 255, 255, 128)));
		else
			p.setBrush(Qt::NoBrush);
		p.drawEllipse(0, y, onionDotDiam, onionDotDiam);
	}
	
	//-- draw highlighted onion
	if (m_showOnionToSet != None)
	{
		int y = m_viewer->rowToY(m_row) + onionDotYPos;
		int xPos = (m_showOnionToSet == Fos) ? 0 : onionDotDiam;
		p.setPen(QColor(255, 128, 0));
		p.setBrush(QBrush(QColor(255, 255, 0, 200)));
		p.drawEllipse(xPos, y, onionDotDiam, onionDotDiam);
	}
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

	if (TApp::instance()->getCurrentFrame()->isEditingScene())
		//current frame
		drawCurrentRowGadget(p, r0, r1);

	drawRows(p, r0, r1);

	if (TApp::instance()->getCurrentFrame()->isEditingScene() && Preferences::instance()->isOnionSkinEnabled())
			drawOnionSkinSelection(p);

	drawPlayRange(p, r0, r1);
}

//-----------------------------------------------------------------------------

void RowArea::mousePressEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(event->modifiers());
	if (event->button() == Qt::LeftButton) {
		bool frameAreaIsClicked = false;

		TApp *app = TApp::instance();
		TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
		TPoint pos(event->pos().x(), event->pos().y());
		int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();

		int row = m_viewer->yToRow(pos.y);

		int onionDotDiam = 8;
		if (Preferences::instance()->isOnionSkinEnabled() &&
			((row == currentFrame && pos.x < RowHeight + 2) || (pos.x < onionDotDiam * 2)))
		{
			if (row == currentFrame)
			{
				setDragTool(XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
				frameAreaIsClicked = true;
			}
			else if (pos.x <= onionDotDiam)
				setDragTool(XsheetGUI::DragTool::makeKeyOnionSkinMaskModifierTool(m_viewer, true));
			else
				setDragTool(XsheetGUI::DragTool::makeKeyOnionSkinMaskModifierTool(m_viewer, false));
		}
		else
		{
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
				frameAreaIsClicked = true;
			}
			else if (m_xa <= pos.x && pos.x <= m_xa + 10 && (row == playR0 || row == playR1)) {
				if (!playRangeEnabled)
					XsheetGUI::setPlayRange(playR0, playR1, step);
				setDragTool(XsheetGUI::DragTool::makePlayRangeModifierTool(m_viewer));
			}
			else
			{
				setDragTool(XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
				frameAreaIsClicked = true;
			}
		}
		//when shift+click the row area, select a single row region in the cell area
		if (frameAreaIsClicked && 0 != (event->modifiers() & Qt::ShiftModifier)) {
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

	m_showOnionToSet = None;

	if (getDragTool()) 
		return;

	int currentRow = TApp::instance()->getCurrentFrame()->getFrame();
	int row = m_viewer->yToRow(m_pos.y());
	if (row < 0)
		return;
	// "decide" se mostrare la possibilita' di settare l'onion skin
	if (Preferences::instance()->isOnionSkinEnabled())
	{
		int onionDotDiam = 8;
		if (x <= onionDotDiam && row != currentRow)
			m_showOnionToSet = Fos;
		else if (x <= onionDotDiam * 2 && row != currentRow)
			m_showOnionToSet = Mos;
	}
	update();

	if (m_xa <= x && x <= m_xa + 10 && row == m_r0)
		m_tooltip = tr("Playback Start Marker");
	else if (m_xa <= x && x <= m_xa + 10 && row == m_r1)
		m_tooltip = tr("Playback End Marker");
	else if (row == currentRow)
	{
		if (Preferences::instance()->isOnionSkinEnabled() && x < RowHeight + 2)
			m_tooltip = tr("Double Click to Toggle Onion Skin");
		else
			m_tooltip = tr("Curren Frame");
	}
	else if (m_showOnionToSet == Fos)
		m_tooltip = tr("Fixed Onion Skin Toggle");
	else if (m_showOnionToSet == Mos)
		m_tooltip = tr("Relative Onion Skin Toggle");
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

	if (Preferences::instance()->isOnionSkinEnabled())
	{
		OnioniSkinMaskGUI::addOnionSkinCommand(menu);
		menu->addSeparator();
	}

	CommandManager *cmdManager = CommandManager::instance();
	menu->addAction(cmdManager->getAction(MI_InsertSceneFrame));
	menu->addAction(cmdManager->getAction(MI_RemoveSceneFrame));
	menu->addAction(cmdManager->getAction(MI_InsertGlobalKeyframe));
	menu->addAction(cmdManager->getAction(MI_RemoveGlobalKeyframe));

	menu->addSeparator();
	menu->addAction(cmdManager->getAction(MI_ShiftTrace));
	menu->addAction(cmdManager->getAction(MI_EditShift));
	menu->addAction(cmdManager->getAction(MI_NoShift));
	menu->addAction(cmdManager->getAction(MI_ResetShift));

	menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void RowArea::mouseDoubleClickEvent(QMouseEvent* event)
{
	int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
	int row = m_viewer->yToRow(event->pos().y());
	if (TApp::instance()->getCurrentFrame()->isEditingScene() && 
		event->buttons() & Qt::LeftButton && 
		Preferences::instance()->isOnionSkinEnabled() && 
		row == currentFrame && 
		event->pos().x() < RowHeight + 2 )
	{
		TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
		OnionSkinMask osm = osmh->getOnionSkinMask();
		osm.enable(!osm.isEnabled());
		osmh->setOnionSkinMask(osm);
		osmh->notifyOnionSkinMaskChanged();
	}
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
