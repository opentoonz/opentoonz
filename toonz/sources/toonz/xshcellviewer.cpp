

#include "xshcellviewer.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "keyframeselection.h"
#include "filmstripselection.h"
#include "filmstripcommand.h"
#include "xsheetdragtool.h"
#include "iocommand.h"
#include "castselection.h"
#include "cellkeyframeselection.h"
#include "keyframedata.h"
#include "columnselection.h"

// TnzTools includes
#include "tools/cursormanager.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/tselectionhandle.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/sceneproperties.h"
#include "toonz/columnfan.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshnoteset.h"
#include "toonz/txshcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshsoundtextlevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tdoublekeyframe.h"

// TnzCore includes
#include "tconvert.h"
#include "tundo.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QUrl>
#include <QToolTip>
#include <QApplication>
#include <QClipboard>

namespace
{

bool selectionContainTlvImage(TCellSelection *selection, TXsheet *xsheet, bool onlyTlv = false)
{
	int r0, r1, c0, c1;
	selection->getSelectedCells(r0, c0, r1, c1);

	//Verifico se almeno un livello della selezione e' un tlv
	int c, r;
	for (c = c0; c <= c1; c++)
		for (r = r0; r <= r1; r++) {
			TXshCell cell = xsheet->getCell(r, c);
			TXshSimpleLevel *level = cell.isEmpty() ? (TXshSimpleLevel *)0 : cell.getSimpleLevel();
			if (!level)
				continue;

			if (level->getType() != TZP_XSHLEVEL && onlyTlv)
				return false;
			if (level->getType() == TZP_XSHLEVEL && !onlyTlv)
				return true;
		}

	return false;
}

//-----------------------------------------------------------------------------

bool selectionContainRasterImage(TCellSelection *selection, TXsheet *xsheet, bool onlyRaster = false)
{
	int r0, r1, c0, c1;
	selection->getSelectedCells(r0, c0, r1, c1);

	//Verifico se almeno un livello della selezione e' un tlv
	int c, r;
	for (c = c0; c <= c1; c++)
		for (r = r0; r <= r1; r++) {
			TXshCell cell = xsheet->getCell(r, c);
			TXshSimpleLevel *level = cell.isEmpty() ? (TXshSimpleLevel *)0 : cell.getSimpleLevel();
			if (!level)
				continue;

			if (level->getType() != OVL_XSHLEVEL && onlyRaster)
				return false;
			if (level->getType() == OVL_XSHLEVEL && !onlyRaster)
				return true;
		}

	return false;
}

//-----------------------------------------------------------------------------

bool selectionContainLevelImage(TCellSelection *selection, TXsheet *xsheet)
{
	int r0, r1, c0, c1;
	selection->getSelectedCells(r0, c0, r1, c1);
	//Verifico se almeno un livello della selezione e' un tlv, un pli o un fullcolor (!= .psd)
	int c, r;
	for (c = c0; c <= c1; c++)
		for (r = r0; r <= r1; r++) {
			TXshCell cell = xsheet->getCell(r, c);
			TXshSimpleLevel *level = cell.isEmpty() ? (TXshSimpleLevel *)0 : cell.getSimpleLevel();
			if (!level)
				continue;

			string ext = level->getPath().getType();
			int type = level->getType();
			if (type == TZP_XSHLEVEL || type == PLI_XSHLEVEL || (type == OVL_XSHLEVEL && ext != "psd"))
				return true;
		}

	return false;
}

//-----------------------------------------------------------------------------
/*! convert the last one digit of the frame number to alphabet
    Ex.  12 -> 1B    21 -> 2A   30 -> 3
*/
void parse_with_letter(const QString &text, wstring &levelName, TFrameId &fid)
{
	QRegExp spaces("\\t|\\s");
	QRegExp numbers("\\d+");
	QRegExp caracters("[^\\d+]");
	QString str = text;

	//remove final spaces
	int size = str.size();
	while (str.lastIndexOf(spaces) == size - 1 && size > 0)
		str.remove(str.size() - 1, 1);
	if (str.isEmpty()) {
		levelName = L"";
		fid = TFrameId::NO_FRAME;
		return;
	}

	int lastSpaceIndex = str.lastIndexOf(spaces);
	// if input only digits / alphabet
	if (lastSpaceIndex == -1) {
		//in case of only level name
		if (str.contains(caracters) && !str.contains(numbers)) {
			levelName = text.toStdWString();
			fid = TFrameId::NO_FRAME;
		}
		//in case of input frame number + alphabet
		else if (str.contains(numbers) && str.contains(caracters)) {
			levelName = L"";

			QString appendix = str.right(1);

			int appendNum = 0;

			if (appendix == QString('A') || appendix == QString('a'))
				appendNum = 1;
			else if (appendix == QString('B') || appendix == QString('b'))
				appendNum = 2;
			else if (appendix == QString('C') || appendix == QString('c'))
				appendNum = 3;
			else if (appendix == QString('D') || appendix == QString('d'))
				appendNum = 4;
			else if (appendix == QString('E') || appendix == QString('e'))
				appendNum = 5;
			else if (appendix == QString('F') || appendix == QString('f'))
				appendNum = 6;
			else if (appendix == QString('G') || appendix == QString('g'))
				appendNum = 7;
			else if (appendix == QString('H') || appendix == QString('h'))
				appendNum = 8;
			else if (appendix == QString('I') || appendix == QString('i'))
				appendNum = 9;

			str.chop(1);

			if (str.contains(caracters) || !str.contains(numbers)) {
				levelName = text.toStdWString();
				fid = TFrameId::NO_FRAME;
				return;
			}

			fid = TFrameId(str.toInt() * 10 + appendNum);
		}
		//in case of input only frame number
		else if (str.contains(numbers)) {
			levelName = L"";
			fid = TFrameId(str.toInt() * 10);
		}
	}
	//if input both the level name and frame number
	else {
		QString lastString = str.right(str.size() - 1 - lastSpaceIndex);
		if (lastString.contains(numbers)) {
			//level name
			QString firstString = str.left(lastSpaceIndex);
			levelName = firstString.toStdWString();

			//frame number + alphabet
			if (lastString.contains(caracters)) {

				QString appendix = lastString.right(1);

				int appendNum = 0;

				if (appendix == QString('A') || appendix == QString('a'))
					appendNum = 1;
				else if (appendix == QString('B') || appendix == QString('b'))
					appendNum = 2;
				else if (appendix == QString('C') || appendix == QString('c'))
					appendNum = 3;
				else if (appendix == QString('D') || appendix == QString('d'))
					appendNum = 4;
				else if (appendix == QString('E') || appendix == QString('e'))
					appendNum = 5;
				else if (appendix == QString('F') || appendix == QString('f'))
					appendNum = 6;
				else if (appendix == QString('G') || appendix == QString('g'))
					appendNum = 7;
				else if (appendix == QString('H') || appendix == QString('h'))
					appendNum = 8;
				else if (appendix == QString('I') || appendix == QString('i'))
					appendNum = 9;

				lastString.chop(1);

				if (lastString.contains(caracters) || !lastString.contains(numbers)) {
					levelName = text.toStdWString();
					fid = TFrameId::NO_FRAME;
					return;
				}

				fid = TFrameId(lastString.toInt() * 10 + appendNum);
			}
			//only frame number
			else if (lastString.contains(numbers)) {
				fid = TFrameId(lastString.toInt() * 10);
			}

		} else {
			levelName = text.toStdWString();
			fid = TFrameId::NO_FRAME;
		}
	}
}

//-----------------------------------------------------------------------------

void parse(const QString &text, wstring &levelName, TFrameId &fid)
{
	QRegExp spaces("\\t|\\s");
	QRegExp numbers("\\d+");
	QRegExp caracters("[^\\d+]");
	QString str = text;

	//remove final spaces
	int size = str.size();
	while (str.lastIndexOf(spaces) == size - 1 && size > 0)
		str.remove(str.size() - 1, 1);
	if (str.isEmpty()) {
		levelName = L"";
		fid = TFrameId::NO_FRAME;
		return;
	}
	int lastSpaceIndex = str.lastIndexOf(spaces);
	if (lastSpaceIndex == -1) {
		if (str.contains(numbers) && !str.contains(caracters)) {
			levelName = L"";
			fid = TFrameId(str.toInt());
		} else if (str.contains(caracters)) {
			levelName = text.toStdWString();
			fid = TFrameId::NO_FRAME;
		}
	} else {
		QString lastString = str.right(str.size() - 1 - lastSpaceIndex);
		if (lastString.contains(numbers) && !lastString.contains(caracters)) {
			QString firstString = str.left(lastSpaceIndex);
			levelName = firstString.toStdWString();
			fid = TFrameId(lastString.toInt());
		} else if (lastString.contains(caracters)) {
			levelName = text.toStdWString();
			fid = TFrameId::NO_FRAME;
		}
	}
}

//-----------------------------------------------------------------------------

bool isGlobalKeyFrameWithSameTypeDiffFromLinear(TStageObject *stageObject, int frame)
{
	if (!stageObject->isFullKeyframe(frame))
		return false;
	TDoubleKeyframe::Type type = stageObject->getParam(TStageObject::T_Angle)->getKeyframeAt(frame).m_type;
	if (type == TDoubleKeyframe::Linear)
		return false;
	if (type != stageObject->getParam(TStageObject::T_X)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_Y)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_Z)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_SO)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_ScaleX)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_ScaleY)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_Scale)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_Path)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_ShearX)->getKeyframeAt(frame).m_type ||
		type != stageObject->getParam(TStageObject::T_ShearY)->getKeyframeAt(frame).m_type)
		return false;
	return true;
}

//-----------------------------------------------------------------------------

bool rectContainsPos(QList<QRect> rects, QPoint pos)
{
	int i;
	for (i = 0; i < rects.size(); i++)
		if (rects.at(i).contains(pos))
			return true;
	return false;
}

//-----------------------------------------------------------------------------

bool isGlobalKeyFrameWithSamePrevTypeDiffFromLinear(TStageObject *stageObject, int frame)
{
	if (!stageObject->isFullKeyframe(frame))
		return false;
	TDoubleKeyframe::Type type = stageObject->getParam(TStageObject::T_Angle)->getKeyframeAt(frame).m_prevType;
	if (type == TDoubleKeyframe::Linear)
		return false;
	if (type != stageObject->getParam(TStageObject::T_X)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_Y)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_Z)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_SO)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_ScaleX)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_ScaleY)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_Scale)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_Path)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_ShearX)->getKeyframeAt(frame).m_prevType ||
		type != stageObject->getParam(TStageObject::T_ShearY)->getKeyframeAt(frame).m_prevType)
		return false;
	return true;
}

//-----------------------------------------------------------------------------

#ifdef LINETEST

int getParamStep(TStageObject *stageObject, int frame)
{
	TDoubleKeyframe keyFrame = stageObject->getParam(TStageObject::T_Angle)->getKeyframeAt(frame);
	return keyFrame.m_step;
}

//-----------------------------------------------------------------------------

void setParamStep(int indexKeyframe, int step, TDoubleParam *param)
{
	KeyframeSetter setter(param, indexKeyframe);
	setter.setStep(step);
}

#endif
//=============================================================================
// RenameCellUndo
//-----------------------------------------------------------------------------

class RenameCellUndo : public TUndo
{
	int m_row;
	int m_col;
	const TXshCell m_oldCell;
	const TXshCell m_newCell;

public:
	// indices sono le colonne inserite
	RenameCellUndo(int row, int col, TXshCell oldCell, TXshCell newCell)
		: m_row(row), m_col(col), m_oldCell(oldCell), m_newCell(newCell)
	{
	}

	void setcell(const TXshCell cell) const
	{
		TApp *app = TApp::instance();
		TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
		assert(xsh);
		if (cell.isEmpty())
			xsh->clearCells(m_row, m_col);
		else
			xsh->setCell(m_row, m_col, cell);
		app->getCurrentXsheet()->notifyXsheetChanged();
	}

	void undo() const
	{
		setcell(m_oldCell);
	}

	void redo() const
	{
		setcell(m_newCell);
	}

	int getSize() const
	{
		return sizeof *this;
	}

	QString getHistoryString()
	{
		return QObject::tr("Rename Cell  at Column %1  Frame %2")
			.arg(QString::number(m_col + 1))
			.arg(QString::number(m_row + 1));
	}
	int getHistoryType()
	{
		return HistoryType::Xsheet;
	}
};

// display upper-directional smart tab only when pressing ctrl key
bool isCtrlPressed = false;

//-----------------------------------------------------------------------------
} //namespace
//-----------------------------------------------------------------------------

namespace XsheetGUI
{

//=============================================================================
// RenameCellField
//-----------------------------------------------------------------------------

RenameCellField::RenameCellField(QWidget *parent, XsheetViewer *viewer)
	: QLineEdit(parent), m_viewer(viewer), m_row(-1), m_col(-1)
{
	setFixedSize(XsheetGUI::ColumnWidth + 3, XsheetGUI::RowHeight + 4);
	connect(this, SIGNAL(returnPressed()), SLOT(onReturnPressed()));
	setContextMenuPolicy(Qt::PreventContextMenu);
}

//-----------------------------------------------------------------------------

void RenameCellField::showInRowCol(int row, int col)
{
	m_viewer->scrollTo(row, col);

	m_row = row;
	m_col = col;

	move(QPoint(m_viewer->columnToX(col) - 1, m_viewer->rowToY(row) - 2));
	static QFont font("Helvetica", XSHEET_FONT_SIZE, QFont::Normal);
	setFont(font);

	//Se la cella non e' vuota setto il testo
	TXsheet *xsh = m_viewer->getXsheet();
	TXshCell cell = xsh->getCell(row, col);
	if (!cell.isEmpty()) {
		TFrameId fid = cell.getFrameId();
		wstring levelName = cell.m_level->getName();

		// convert the last one digit of the frame number to alphabet
		// Ex.  12 -> 1B    21 -> 2A   30 -> 3
		if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
			setText((fid.isEmptyFrame() || fid.isNoFrame()) ? QString::fromStdWString(levelName)
															: QString::fromStdWString(levelName) + QString(" ") + m_viewer->getFrameNumberWithLetters(fid.getNumber()));
		else {
			string frameNumber("");
			if (fid.getNumber() > 0)
				frameNumber = toString(fid.getNumber());
			if (fid.getLetter() != 0)
				frameNumber.append(1, fid.getLetter());
			setText((frameNumber.empty()) ? QString::fromStdWString(levelName)
										  : QString::fromStdWString(levelName) + QString(" ") + QString::fromStdString(frameNumber));
		}
		selectAll();
	}
	// clear the field if the empty cell is clicked
	else {
		setText("");
	}
	show();
	raise();
	setFocus();
}

//-----------------------------------------------------------------------------

void RenameCellField::renameCell()
{
	QString s = text();
	wstring newName = s.toStdWString();

	setText("");

	wstring levelName;
	TFrameId fid;

	// convert the last one digit of the frame number to alphabet
	// Ex.  12 -> 1B    21 -> 2A   30 -> 3
	if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
		parse_with_letter(QString::fromStdWString(newName), levelName, fid);
	else
		parse(QString::fromStdWString(newName), levelName, fid);
	TXshLevel *xl = 0;
	TXsheet *xsheet = m_viewer->getXsheet();

	bool animationSheetEnabled = Preferences::instance()->isAnimationSheetEnabled();
	bool levelDefined =
		xsheet->getCell(m_row, m_col).getSimpleLevel() != 0 ||
		m_row > 0 && xsheet->getCell(m_row - 1, m_col).getSimpleLevel() != 0;

	if (animationSheetEnabled && levelDefined) {
		TXshCell cell = xsheet->getCell(m_row, m_col);
		TXshSimpleLevel *sl = cell.getSimpleLevel();
		if (sl) {
			QRegExp fidRe("([0-9]+)([a-z]?)");
			if (fidRe.exactMatch(s)) {
#if QT_VERSION >= 0x050500
				fid = TFrameId(fidRe.cap(1).toInt(), fidRe.cap(2) == "" ? 0 : fidRe.cap(2).toLatin1()[0]);
#else
				fid = TFrameId(fidRe.cap(1).toInt(), fidRe.cap(2) == "" ? 0 : fidRe.cap(2).toAscii()[0]);
#endif
				FilmstripCmd::renumberDrawing(sl, cell.m_frameId, fid);
			}
		}
		return;
	}

	if (levelName == L"") {
		// prendo il livello dalla cella precedente. Se non c'e' dalla corrente
		// (forse sto modificando una cella non vuota)

		// no: faccio il contrario. cfr #6152. celle A-1,B-1. Edito B-1 e la rinomino in 2. Quindi devo prima verificare
		// che la cella corrente non sia vuota

		TXshCell cell = xsheet->getCell(m_row, m_col);
		if (cell.isEmpty() && m_row > 0) {
			cell = xsheet->getCell(m_row - 1, m_col);
		}
		xl = cell.m_level.getPointer();
		if (!xl || (xl->getType() == OVL_XSHLEVEL && xl->getPath().getFrame() == TFrameId::NO_FRAME))
			return;
		if (fid == TFrameId::NO_FRAME)
			fid = cell.m_frameId;
	} else {
		ToonzScene *scene = m_viewer->getXsheet()->getScene();
		TLevelSet *levelSet = scene->getLevelSet();
		xl = levelSet->getLevel(levelName);
		if (!xl && fid != TFrameId::NO_FRAME) {
			if (animationSheetEnabled) {
				Preferences *pref = Preferences::instance();
				int levelType = pref->getDefLevelType();
				xl = scene->createNewLevel(levelType, levelName);
				TXshSimpleLevel *sl = xl->getSimpleLevel();
				if (levelType == TZP_XSHLEVEL || levelType == OVL_XSHLEVEL)
					sl->setFrame(fid, sl->createEmptyFrame());
			} else
				xl = scene->createNewLevel(TZI_XSHLEVEL, levelName);
		}
	}
	if (!xl)
		return;
	TXshCell cell(xl, fid);

	//register undo only if the cell is modified
	if (cell == xsheet->getCell(m_row, m_col))
		return;

	RenameCellUndo *undo = new RenameCellUndo(m_row, m_col, xsheet->getCell(m_row, m_col), cell);
	xsheet->setCell(m_row, m_col, cell);
	TUndoManager::manager()->add(undo);
	TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
	TApp *app = TApp::instance();
	if (m_row == app->getCurrentFrame()->getFrame()) {
		app->getCurrentTool()->onImageChanged((TImage::Type)app->getCurrentImageType());
		xsheet->getStageObjectTree()->invalidateAll();
	}
}

//-----------------------------------------------------------------------------

void RenameCellField::onReturnPressed()
{
	renameCell();
	showInRowCol(m_row + 1, m_col);
}

//-----------------------------------------------------------------------------

void RenameCellField::focusOutEvent(QFocusEvent *e)
{
	hide();

	QLineEdit::focusOutEvent(e);
}

//-----------------------------------------------------------------------------

void RenameCellField::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Up && m_row > 0) {
		renameCell();
		showInRowCol(m_row - 1, m_col);
	} else if (event->key() == Qt::Key_Down) {
		renameCell();
		showInRowCol(m_row + 1, m_col);
	}
	QLineEdit::keyPressEvent(event);
}

//=============================================================================
// CellArea
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
CellArea::CellArea(XsheetViewer *parent, Qt::WindowFlags flags)
#else
CellArea::CellArea(XsheetViewer *parent, Qt::WFlags flags)
#endif
	: QWidget(parent, flags), m_viewer(parent), m_levelExtenderRect(), m_soundLevelModifyRects(), m_isPanning(false), m_isMousePressed(false), m_pos(-1, -1), m_tooltip(tr("")), m_renameCell(new RenameCellField(this, m_viewer))
{
	setAcceptDrops(true);
	setMouseTracking(true);
	m_renameCell->hide();
	setFocusPolicy(Qt::NoFocus);
}

//-----------------------------------------------------------------------------

CellArea::~CellArea()
{
}

//-----------------------------------------------------------------------------

DragTool *CellArea::getDragTool() const { return m_viewer->getDragTool(); }
void CellArea::setDragTool(DragTool *dragTool) { m_viewer->setDragTool(dragTool); }

//-----------------------------------------------------------------------------

void CellArea::drawCells(QPainter &p, const QRect toBeUpdated)
{
	TXsheet *xsh = m_viewer->getXsheet();
	ColumnFan *columnFan = xsh->getColumnFan();

	TCellSelection *cellSelection = m_viewer->getCellSelection();
	int rS0, cS0, rS1, cS1;
	if (!cellSelection->isEmpty())
		cellSelection->getSelectedCells(rS0, cS0, rS1, cS1);

	int r0, r1, c0, c1; // range di righe e colonne visibili
	r0 = m_viewer->yToRow(toBeUpdated.top());
	r1 = m_viewer->yToRow(toBeUpdated.bottom());
	c0 = m_viewer->xToColumn(toBeUpdated.left());
	c1 = m_viewer->xToColumn(toBeUpdated.right());

	// Sfondo bianco se row < xsh->getFrameCount()
	int xshRowCount = xsh->getFrameCount();

	if (xshRowCount > 0) {
		int filledCol;
		for (filledCol = xsh->getColumnCount() - 1; filledCol >= 0; filledCol--) {
			TXshColumn *currentColumn = xsh->getColumn(filledCol);
			if (!currentColumn)
				continue;
			if (currentColumn->isEmpty() == false) {
				p.fillRect(1, 0,
						   m_viewer->columnToX(filledCol + 1),
						   m_viewer->rowToY(xshRowCount),
						   QBrush(m_viewer->getNotEmptyColumnColor()));
				break;
			}
		}
	}
	int xS0, xS1, yS0, yS1;

	if (!cellSelection->isEmpty()) {
		xS0 = m_viewer->columnToX(cS0);
		xS1 = m_viewer->columnToX(cS1 + 1) - 1;
		yS0 = m_viewer->rowToY(rS0);
		yS1 = m_viewer->rowToY(rS1 + 1) - 1;
		QRect rect = QRect(xS0, yS0, xS1 - xS0 + 1, yS1 - yS0 + 1);
		p.fillRect(rect, QBrush(m_viewer->getSelectedEmptyCellColor()));
	}

	// marker interval
	int distance, offset;
	TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(distance, offset);
	if (distance == 0)
		distance = 6;

	int currentRow = m_viewer->getCurrentRow();
	int col, row;

	int x0 = tmax(1, toBeUpdated.left());
	int x1 = tmin(width(), toBeUpdated.right());

	int y0 = tmax(1, toBeUpdated.top());
	int y1 = tmin(height() - 2, toBeUpdated.bottom());
	m_soundLevelModifyRects.clear();
	for (col = c0; col <= c1; col++) {
		int x = m_viewer->columnToX(col);
		// Disegno la colonna
		if (!columnFan->isActive(col)) {
			p.fillRect(x + 1, y0, 2, y1 - y0 + 1, QBrush(Qt::white));
			p.fillRect(x + 4, y0, 2, y1 - y0 + 1, QBrush(Qt::white));
			p.fillRect(x + 7, y0, 2, y1 - y0 + 1, QBrush(Qt::white));
			p.setPen(m_viewer->getLightLineColor());
			p.drawLine(x, y0, x, y1);
			p.drawLine(x + 3, y0, x + 3, y1);
			p.drawLine(x + 6, y0, x + 6, y1);
			continue;
		}

		TXshColumn *column = xsh->getColumn(col);
		bool isColumn = (column) ? true : false;
		bool isSoundColumn = false;
		bool isPaletteColumn = false;
		bool isSoundTextColumn = false;
		if (isColumn) {
			isSoundColumn = column->getSoundColumn() != 0;
			isPaletteColumn = column->getPaletteColumn() != 0;
			isSoundTextColumn = column->getSoundTextColumn() != 0;
		}
		//check if the column is reference
		bool isReference = true;
		if (column) { // Verifico se la colonna e' una mask
			if (column->isControl())
				isReference = false;
			if (column->isRendered())
				isReference = false;
		}

		x0 = x + 1;
		x1 = m_viewer->columnToX(col + 1);

		//draw vertical lines
		p.setPen(Qt::black);
		if (x > 0)
			p.drawLine(x, y0, x, y1);
		p.setPen(Qt::white);
		if (x > 1)
			p.drawLine(x - 1, y0, x - 1, y1);

		for (row = r0; row <= r1; row++) {
			//draw horizontal lines
			QColor color = ((row - offset) % distance != 0) ? m_viewer->getLightLineColor() : m_viewer->getMarkerLineColor();

			p.setPen(color);
			int y = m_viewer->rowToY(row);
			p.drawLine(x0, y, x1, y);
			if (!isColumn)
				continue;
			// Disegno le celle a seconda del tipo di colonna
			if (isSoundColumn)
				drawSoundCell(p, row, col);
			else if (isPaletteColumn)
				drawPaletteCell(p, row, col, isReference);
			else if (isSoundTextColumn)
				drawSoundTextCell(p, row, col);
			else
				drawLevelCell(p, row, col, isReference);
		}

		//hide top-most interval line
		p.setPen(m_viewer->getLightLineColor());
		p.drawLine(x - 1, 0, x1 - 1, 0);
	}

	// smart tab
	if (!cellSelection->isEmpty() && !m_viewer->areSoundCellsSelected()) {
		m_levelExtenderRect = QRect(xS1 - 20, yS1 + 1, 19, 8);
		p.setPen(Qt::black);
		p.setBrush(SmartTabColor);
		p.drawRoundRect(m_levelExtenderRect, 30, 75);
		QColor color = ((rS1 + 1 - offset) % distance != 0) ? m_viewer->getLightLineColor() : m_viewer->getMarkerLineColor();
		p.setPen(color);
		p.drawLine(xS1 - 20, yS1 + 1, xS1 - 1, yS1 + 1);

		// upper-directional smart tab
		if (isCtrlPressed &&
			rS0 > 0 &&
			!m_viewer->areCellsSelectedEmpty()) {
			m_upperLevelExtenderRect = QRect(xS1 - 20, yS0 - 8, 19, 8);
			p.setPen(Qt::black);
			p.setBrush(SmartTabColor);
			p.drawRoundRect(m_upperLevelExtenderRect, 30, 75);
			QColor color = ((rS0 - offset) % distance != 0) ? m_viewer->getLightLineColor() : m_viewer->getMarkerLineColor();
			p.setPen(color);
			p.drawLine(xS1 - 20, yS0, xS0 + 20, yS0);
		}

		p.setBrush(Qt::NoBrush);
	}
}

//-----------------------------------------------------------------------------

void CellArea::drawSoundCell(QPainter &p, int row, int col)
{
	TXshSoundColumn *soundColumn = m_viewer->getXsheet()->getColumn(col)->getSoundColumn();
	int x = m_viewer->columnToX(col);
	int y = m_viewer->rowToY(row);
	QRect rect = QRect(x + 1, y, ColumnWidth - 1, RowHeight);
	int maxNumFrame = soundColumn->getMaxFrame() + 1;
	int startFrame = soundColumn->getFirstRow();
	TXshCell cell = soundColumn->getCell(row);
	if (soundColumn->isCellEmpty(row) || cell.isEmpty() || row > maxNumFrame || row < startFrame)
		return;

	TXshSoundLevelP soundLevel = cell.getSoundLevel();

	int r0, r1;
	if (!soundColumn->getLevelRange(row, r0, r1))
		return;
	bool isFirstRow = (row == r0);
	bool isLastRow = (row == r1);

	TCellSelection *cellSelection = m_viewer->getCellSelection();
	TColumnSelection *columnSelection = m_viewer->getColumnSelection();
	bool isSelected = cellSelection->isCellSelected(row, col) || columnSelection->isColumnSelected(col);

	// sfondo celle
	QRect backgroundRect = QRect(x + 1, y + 1, ColumnWidth - 1, RowHeight - 1);
	p.fillRect(backgroundRect, QBrush((isSelected) ? SelectedSoundColumnColor : SoundColumnColor));
	if (isLastRow) {
		QPainterPath path(QPointF(x, y));
		path.lineTo(QPointF(x + 6, y));
		path.lineTo(QPointF(x + 6, y + 2));
		path.lineTo(QPointF(x, y + RowHeight - 2));
		p.fillPath(path, QBrush(SoundColumnBorderColor));
	} else
		p.fillRect(QRect(x, y, 6, RowHeight), QBrush(SoundColumnBorderColor));

	int x1 = rect.x() + 5;
	int x2 = rect.x() + rect.width();
	int x1Bis = x2 - 6;

	int offset = row - cell.getFrameId().getNumber();
	int y0 = rect.y();
	int y1 = rect.bottomLeft().y();
	int soundY0 = y0 - m_viewer->rowToY(offset);

	int trackWidth = (x1Bis - x1) / 2;
	int lastMin, lastMax;
	DoublePair minmax;
	soundLevel->getValueAtPixel(soundY0, minmax);

	double pmin = minmax.first + trackWidth / 2.;
	double pmax = minmax.second + trackWidth / 2.;

	int delta = x1 + trackWidth / 2;
	lastMin = tcrop((int)pmin, 0, trackWidth / 2) + delta;
	lastMax = tcrop((int)pmax, trackWidth / 2, trackWidth - 1) + delta;

	bool scrub = m_viewer->isScrubHighlighted(row, col);

	int i;
	for (i = y0; i <= y1; i++) {
		DoublePair minmax;
		soundLevel->getValueAtPixel(soundY0, minmax);
		soundY0++;
		int min, max;
		double pmin = minmax.first + trackWidth / 2.;
		double pmax = minmax.second + trackWidth / 2.;

		int delta = x1 + trackWidth / 2;
		min = tcrop((int)pmin, 0, trackWidth / 2) + delta;
		max = tcrop((int)pmax, trackWidth / 2, trackWidth - 1) + delta;

		if (i != y0 || !isFirstRow) {
			// trattini a destra della colonna
			if (i % 2) {
				p.setPen((isSelected) ? SelectedSoundColumnColor : SoundColumnColor);
				p.drawLine(x1, i, x1Bis, i);
			} else {
				p.setPen(SoundColumnTrackColor);
				p.drawLine(x1Bis + 1, i, x2, i);
			}
		}

		if (scrub && i % 2) {
			p.setPen(SoundColumnHlColor);
			p.drawLine(x1Bis + 1, i, x2, i);
		}

		if (i != y0) {
			// "traccia audio" al centro della colonna
			p.setPen(SoundColumnTrackColor);
			p.drawLine(lastMin, i, min, i);
			p.drawLine(lastMax, i, max, i);
		}

		lastMin = min;
		lastMax = max;
	}

	p.setPen(SoundColumnExtenderColor);
	int r0WithoutOff, r1WithoutOff;
	bool ret = soundColumn->getLevelRangeWithoutOffset(row, r0WithoutOff, r1WithoutOff);
	assert(ret);

	if (isFirstRow) {
		if (r0 != r0WithoutOff) {
			p.drawLine(x1, y0 + 1, x2, y0 + 1);
			p.drawLine(x1, y0 + 2, x2, y0 + 2);
		}
		QRect modifierRect(x1, y0 + 1, XsheetGUI::ColumnWidth, 2);
		m_soundLevelModifyRects.append(modifierRect);
	}
	if (isLastRow) {
		if (r1 != r1WithoutOff) {
			p.drawLine(x, y1, x2, y1);
			p.drawLine(x, y1 - 1, x2, y1 - 1);
		}
		QRect modifierRect(x1, y1 - 1, XsheetGUI::ColumnWidth, 2);
		m_soundLevelModifyRects.append(modifierRect);
	}
}

//-----------------------------------------------------------------------------

void CellArea::drawLevelCell(QPainter &p, int row, int col, bool isReference)
{
	TXsheet *xsh = m_viewer->getXsheet();
	TXshCell cell = xsh->getCell(row, col);
	TXshCell prevCell;

	TCellSelection *cellSelection = m_viewer->getCellSelection();
	TColumnSelection *columnSelection = m_viewer->getColumnSelection();
	bool isSelected = cellSelection->isCellSelected(row, col) || columnSelection->isColumnSelected(col);

	if (row > 0)
		prevCell = xsh->getCell(row - 1, col);
	//nothing to draw
	if (cell.isEmpty() && prevCell.isEmpty())
		return;
	TXshCell nextCell;
	nextCell = xsh->getCell(row + 1, col);

	int x = m_viewer->columnToX(col);
	int y = m_viewer->rowToY(row);
	QRect rect = QRect(x + 1, y + 1, ColumnWidth - 1, RowHeight - 1);
	if (cell.isEmpty()) { // vuol dire che la precedente non e' vuota
		p.setPen(XsheetGUI::LevelEndCrossColor);
		p.drawLine(rect.topLeft(), rect.bottomRight());
		p.drawLine(rect.topRight(), rect.bottomLeft());
		return;
	}

	// get cell colors
	QColor cellColor, sideColor;
	if (isReference) {
		cellColor = (isSelected) ? m_viewer->getSelectedReferenceColumnColor() : m_viewer->getReferenceColumnColor();
		sideColor = m_viewer->getReferenceColumnBorderColor();
	} else {
		int levelType;
		m_viewer->getCellTypeAndColors(levelType, cellColor, sideColor, cell, isSelected);
	}

	// check if the level is scanned but not cleanupped
	bool yetToCleanupCell = false;
	if (!cell.isEmpty() && cell.getSimpleLevel()) {
		int frameStatus = cell.getSimpleLevel()->getFrameStatus(cell.m_frameId);
		const int mask = TXshSimpleLevel::Cleanupped | TXshSimpleLevel::Scanned;
		const int yetToCleanupValue = TXshSimpleLevel::Scanned;
		yetToCleanupCell = (frameStatus & mask) == yetToCleanupValue;
	}

	//paint cell
	p.fillRect(rect, QBrush(cellColor));
	// paint side bar
	p.fillRect(QRect(x, y, 7, RowHeight), QBrush(sideColor));

	if (yetToCleanupCell)
		p.fillRect(rect.adjusted(rect.width() / 2, 0, 0, 0),
				   (isSelected) ? SelectedFullcolorColumnColor : FullcolorColumnColor);

	//draw dot line if the column is locked
	TXshColumn *column = xsh->getColumn(col);
	if (column->isLocked()) {
		p.setPen(QPen(Qt::gray, 2, Qt::DotLine));
		p.drawLine(x + 3, y, x + 3, y + RowHeight);
	}
	// draw "end of the level"
	if (nextCell.isEmpty() || cell.m_level.getPointer() != nextCell.m_level.getPointer()) {
		QPainterPath path(QPointF(x, y + RowHeight));
		path.lineTo(QPointF(x + 7, y + RowHeight));
		path.lineTo(QPointF(x + 7, y + RowHeight - 7));
		path.lineTo(QPointF(x, y + RowHeight));
		p.fillPath(path, QBrush(cellColor));
	}

	bool sameLevel = prevCell.m_level.getPointer() == cell.m_level.getPointer();

	int distance, offset;
	TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(distance, offset);
	if (distance == 0)
		distance = 6;
	bool isAfterMarkers = (row - offset) % distance == 0;

	//draw marker interval
	if (isAfterMarkers) {
		p.setPen(m_viewer->getMarkerLineColor());
		p.drawLine(x, y, x + 6, y);
	}

	QRect nameRect = rect.adjusted(7, 4, -6, 0);

	//draw text in red if the file does not exist
	bool isRed = false;
	TXshSimpleLevel *sl = cell.getSimpleLevel();
	if (sl && !sl->isFid(cell.m_frameId))
		isRed = true;
	TXshChildLevel *cl = cell.getChildLevel();
	if (cl && cell.getFrameId().getNumber() - 1 >= cl->getFrameCount())
		isRed = true;
	p.setPen(isRed ? m_viewer->getSelectedColumnTextColor() : m_viewer->getTextColor());

#ifdef WIN32
	static QFont font("Arial", XSHEET_FONT_SIZE, QFont::Normal);
#else
	static QFont font("Helvetica", XSHEET_FONT_SIZE, QFont::Normal);
#endif
	p.setFont(font);

	//if the same level & same fId with the previous cell, then draw vertical line
	if (sameLevel && prevCell.m_frameId == cell.m_frameId) { // cella uguale a quella precedente (non sulla marker line):
		int x = (Preferences::instance()->isLevelNameOnEachMarkerEnabled()) ? rect.right() - 11 : rect.center().x();
		p.drawLine(x, rect.top(), x, rect.bottom());
	}
	//draw frame number
	else {
		TFrameId fid = cell.m_frameId;

		// convert the last one digit of the frame number to alphabet
		// Ex.  12 -> 1B    21 -> 2A   30 -> 3
		if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
			p.drawText(nameRect, Qt::AlignRight, m_viewer->getFrameNumberWithLetters(fid.getNumber()));
		else {
			string frameNumber("");
			//set number
			if (fid.getNumber() > 0)
				frameNumber = toString(fid.getNumber());
			//add letter
			if (fid.getLetter() != 0)
				frameNumber.append(1, fid.getLetter());
			p.drawText(nameRect, Qt::AlignRight, QString::fromStdString(frameNumber));
		}
	}

	//draw level name
	if (!sameLevel ||
		(isAfterMarkers && Preferences::instance()->isLevelNameOnEachMarkerEnabled())) {
		wstring levelName = cell.m_level->getName();
		QString text = QString::fromStdWString(levelName);
#if QT_VERSION >= 0x050500
		QFontMetrics fm(font);
		QString elidaName = elideText(text, fm, nameRect.width(), QString("~"));
#else
		QString elidaName = elideText(text, font, nameRect.width());
#endif
		p.drawText(nameRect, Qt::AlignLeft, elidaName);
	}
}

//-----------------------------------------------------------------------------

void CellArea::drawSoundTextCell(QPainter &p, int row, int col)
{
	TXsheet *xsh = m_viewer->getXsheet();
	TXshCell cell = xsh->getCell(row, col);
	TXshCell prevCell;
	if (row > 0)
		prevCell = xsh->getCell(row - 1, col);
	TXshCell nextCell;
	nextCell = xsh->getCell(row + 1, col);

	if (cell.isEmpty() && prevCell.isEmpty())
		return;
	int x = m_viewer->columnToX(col);
	int y = m_viewer->rowToY(row);
	QRect rect = QRect(x + 1, y + 1, ColumnWidth - 1, RowHeight - 1);
	if (cell.isEmpty()) { // vuol dire che la precedente non e' vuota
		p.setPen(m_viewer->getLightLineColor());
		p.drawLine(rect.topLeft(), rect.bottomRight());
		p.drawLine(rect.topRight(), rect.bottomLeft());
		return;
	}

	int levelType;
	QColor cellColor, sideColor;
	m_viewer->getCellTypeAndColors(levelType, cellColor, sideColor, cell);
	p.fillRect(rect, QBrush(cellColor));

	if (nextCell.isEmpty() || cell.m_level.getPointer() != nextCell.m_level.getPointer()) {
		QPainterPath path(QPointF(x, y));
		path.lineTo(QPointF(x + 6, y));
		path.lineTo(QPointF(x + 6, y + 2));
		path.lineTo(QPointF(x, y + RowHeight - 2));
		p.fillPath(path, QBrush(sideColor));
	} else
		p.fillRect(QRect(x, y, 6, RowHeight), QBrush(sideColor));

	bool sameLevel = prevCell.m_level.getPointer() == cell.m_level.getPointer();

	TFrameId fid = cell.m_frameId;
	if (sameLevel && prevCell.m_frameId == fid) { // cella uguale a quella precedente
		// non scrivo nulla e disegno una linea verticale
		QPen oldPen = p.pen();
		p.setPen(QPen(Qt::black, 1, Qt::DotLine));
		int x = rect.center().x();
		p.drawLine(x, rect.top(), x, rect.bottom());
		p.setPen(oldPen);
		return;
	}
	QString text = cell.getSoundTextLevel()->getFrameText(fid.getNumber() - 1);
	p.setPen(Qt::black);
	QRect nameRect = rect.adjusted(8, -H_ADJUST, -10, H_ADJUST);
	// il nome va scritto se e' diverso dalla cella precedente oppure se
	// siamo su una marker line
	static QFont font("Helvetica", XSHEET_FONT_SIZE, QFont::Normal);
	p.setFont(font);

#if QT_VERSION >= 0x050500
	QFontMetrics metric(font);
	QString elidaName = elideText(text, metric, nameRect.width(), "~");
#else
	QString elidaName = elideText(text, font, nameRect.width(), "~");
#endif

	if (!sameLevel || prevCell.m_frameId != fid)
		p.drawText(nameRect, Qt::AlignLeft, elidaName);
}

//-----------------------------------------------------------------------------

void CellArea::drawPaletteCell(QPainter &p, int row, int col, bool isReference)
{
	TXsheet *xsh = m_viewer->getXsheet();
	TXshCell cell = xsh->getCell(row, col);
	TXshCell prevCell;

	TCellSelection *cellSelection = m_viewer->getCellSelection();
	bool isSelected = cellSelection->isCellSelected(row, col);

	if (row > 0)
		prevCell = xsh->getCell(row - 1, col);
	TXshCell nextCell = xsh->getCell(row + 1, col);

	if (cell.isEmpty() && prevCell.isEmpty())
		return;
	int x = m_viewer->columnToX(col);
	int y = m_viewer->rowToY(row);
	QRect rect = QRect(x + 1, y + 1, ColumnWidth - 1, RowHeight - 1);
	if (cell.isEmpty()) { // vuol dire che la precedente non e' vuota
		p.setPen(XsheetGUI::LevelEndCrossColor);
		p.drawLine(rect.topLeft(), rect.bottomRight());
		p.drawLine(rect.topRight(), rect.bottomLeft());
		return;
	}

	QColor cellColor, sideColor;
	if (isReference) {
		cellColor = (isSelected) ? m_viewer->getSelectedReferenceColumnColor() : m_viewer->getReferenceColumnColor();
		sideColor = m_viewer->getReferenceColumnBorderColor();
	} else {
		cellColor = (isSelected) ? m_viewer->getSelectedPaletteColumnColor() : m_viewer->getPaletteColumnColor();
		sideColor = m_viewer->getPaletteColumnBorderColor();
	}

	p.fillRect(rect, QBrush(cellColor));
	p.fillRect(QRect(x, y, 7, RowHeight), QBrush(sideColor));

	TXshColumn *column = xsh->getColumn(col);
	if (column->isLocked()) {
		p.setPen(QPen(Qt::gray, 2, Qt::DotLine));
		p.drawLine(x + 3, y, x + 3, y + RowHeight);
	}

	if (nextCell.isEmpty() || cell.m_level.getPointer() != nextCell.m_level.getPointer()) {
		QPainterPath path(QPointF(x, y + RowHeight));
		path.lineTo(QPointF(x + 7, y + RowHeight));
		path.lineTo(QPointF(x + 7, y + RowHeight - 7));
		path.lineTo(QPointF(x, y + RowHeight));
		p.fillPath(path, QBrush(cellColor));
	}

	bool sameLevel = prevCell.m_level.getPointer() == cell.m_level.getPointer();

	int distance, offset;
	TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(distance, offset);
	if (distance == 0)
		distance = 6;
	bool isAfterMarkers = (row - offset) % distance == 0;

	if (isAfterMarkers) {
		p.setPen(m_viewer->getMarkerLineColor());
		p.drawLine(x, y, x + 6, y);
	}

	if (sameLevel && prevCell.m_frameId == cell.m_frameId && !isAfterMarkers) { // cella uguale a quella precedente (non sulla marker line):
																				// non scrivo nulla e disegno una linea verticale
		QPen oldPen = p.pen();
		p.setPen(QPen(m_viewer->getTextColor(), 1));
		int x = rect.center().x();
		p.drawLine(x, rect.top(), x, rect.bottom());
		p.setPen(oldPen);
	} else {
		TFrameId fid = cell.m_frameId;

		wstring levelName = cell.m_level->getName();
		string frameNumber("");
		if (fid.getNumber() > 0)
			frameNumber = toString(fid.getNumber());
		if (fid.getLetter() != 0)
			frameNumber.append(1, fid.getLetter());

		QRect nameRect = rect.adjusted(7, 4, -12, 0);

		bool isRed = false;
		TXshPaletteLevel *pl = cell.getPaletteLevel();
		if (pl && !pl->getPalette())
			isRed = true;
		p.setPen(isRed ? m_viewer->getSelectedColumnTextColor() : m_viewer->getTextColor());
// il nome va scritto se e' diverso dalla cella precedente oppure se
// siamo su una marker line
#ifndef WIN32
		static QFont font("Arial", XSHEET_FONT_SIZE, QFont::Normal);
#else
		static QFont font("Helvetica", XSHEET_FONT_SIZE, QFont::Normal);
#endif
		p.setFont(font);
		QFontMetrics fm(font);

		// il numero va scritto se e' diverso dal precedente oppure se il livello
		// e' diverso dal precedente
		QString numberStr;
		if (!sameLevel || prevCell.m_frameId != cell.m_frameId) {
			numberStr = QString::fromStdString(frameNumber);
			p.drawText(nameRect, Qt::AlignRight, numberStr);
		}

		QString text = QString::fromStdWString(levelName);
#if QT_VERSION >= 0x050500
		QString elidaName = elideText(text, fm, nameRect.width() - fm.width(numberStr) - 2, QString("~"));
#else
		QString elidaName = elideText(text, font, nameRect.width() - fm.width(numberStr) - 2, QString("~"));
#endif

		if (!sameLevel || isAfterMarkers)
			p.drawText(nameRect, Qt::AlignLeft, elidaName);
	}
}

//-----------------------------------------------------------------------------

void CellArea::drawKeyframe(QPainter &p, const QRect toBeUpdated)
{
	int r0, r1, c0, c1; // range di righe e colonne visibili
	r0 = m_viewer->yToRow(toBeUpdated.top());
	r1 = m_viewer->yToRow(toBeUpdated.bottom());
	c0 = m_viewer->xToColumn(toBeUpdated.left());
	c1 = m_viewer->xToColumn(toBeUpdated.right());

	TXsheet *xsh = m_viewer->getXsheet();
	ColumnFan *columnFan = xsh->getColumnFan();
	int col;
	for (col = c0; col <= c1; col++) {
		if (!columnFan->isActive(col))
			continue;
		int x = m_viewer->columnToX(col);
		TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
		if (!pegbar)
			return;
		int row0, row1;
		bool emptyKeyframe = !pegbar->getKeyframeRange(row0, row1);
		if (emptyKeyframe)
			continue;
		bool emptyKeyframeRange = row0 >= row1;
		int row;
		row0 = tmax(row0, r0);
		row1 = tmin(row1, r1);
		for (row = row0; row <= row1; row++) {
			int y = m_viewer->rowToY(row);
			int r0, r1;
			double e0, e1;
			p.setPen(Qt::black);
			if (pegbar->isKeyframe(row)) {
				if (m_viewer->getKeyframeSelection() &&
					m_viewer->getKeyframeSelection()->isSelected(row, col)) {
					// keyframe selezionato
					static QPixmap selectedKey = QPixmap(":Resources/selected_key.bmp");
					p.drawPixmap(x + ColumnWidth - 11, y, selectedKey);
				} else {
					// keyframe non selezionato
					static QPixmap key = QPixmap(":Resources/key.bmp");
					p.drawPixmap(x + ColumnWidth - 11, y, key);
				}
			} else if (pegbar->getKeyframeSpan(row, r0, e0, r1, e1)) {
				// sono fra due keyframe: devo disegnare la linea e i bilancini
				int y1 = m_viewer->rowToY(r1 + 1);
				int x1 = x + ColumnWidth - 6;
				p.drawLine(x + ColumnWidth - 6, y, x + ColumnWidth - 6, y1);
				if (r1 - r0 > 4) {
					int rh0, rh1;
#ifndef STUDENT
					if (getEaseHandles(r0, r1, e0, e1, rh0, rh1)) {
						int e0Y = m_viewer->rowToY(rh0);
						drawArrow(p, QPointF(x1 - 4, e0Y + 2),
								  QPointF(x1 + 4, e0Y + 2),
								  QPointF(x1, e0Y + 6),
								  true, m_viewer->getLightLineColor());
						int e1Y = m_viewer->rowToY(rh1 + 1);
						drawArrow(p, QPointF(x1 - 4, e1Y - 2),
								  QPointF(x1 + 4, e1Y - 2),
								  QPointF(x1, e1Y - 6),
								  true, m_viewer->getLightLineColor());
					}
#endif
				}
			}
		}
		int y1 = m_viewer->rowToY(row1 + 1);
		int x1 = x + ColumnWidth - 11;
		if (!emptyKeyframeRange && row0 <= row1 + 1) {
			// c'e' piu' di un keyframe
			// disegno il bottone per il ciclo
			p.setBrush(Qt::white);
			p.setPen(Qt::black);
			p.drawRect(x1, y1, 10, 10);
			p.setBrush(Qt::NoBrush);
			// disegno il bordo in basso (arrotondato)
			p.drawLine(x1 + 1, y1 + 10, x1 + 9, y1 + 10);
			p.setPen(Qt::white);
			p.drawLine(x1 + 3, y1 + 10, x1 + 7, y1 + 10);
			p.setPen(Qt::black);
			p.drawLine(x1 + 3, y1 + 11, x1 + 7, y1 + 11);

			// disegno la freccia
			p.drawArc(QRect(x1 + 2, y1 + 3, 6, 6), 180 * 16, 270 * 16);
			p.drawLine(x1 + 5, y1 + 2, x1 + 5, y1 + 5);
			p.drawLine(x1 + 5, y1 + 2, x1 + 8, y1 + 2);
		}
		if (pegbar->isCycleEnabled()) {
			// la riga a zigzag sotto il bottone
			int ymax = m_viewer->rowToY(r1 + 1);
			int qy = y1 + 12;
			int zig = 2;
			int qx = x1 + 5;
			p.setPen(Qt::black);
			p.drawLine(qx, qy, qx - zig, qy + zig);
			while (qy < ymax) {
				p.drawLine(qx - zig, qy + zig, qx + zig, qy + 3 * zig);
				p.drawLine(qx + zig, qy + 3 * zig, qx - zig, qy + 5 * zig);
				qy += 4 * zig;
			}
		}
	}
}
//-----------------------------------------------------------------------------

void CellArea::drawNotes(QPainter &p, const QRect toBeUpdated)
{
	int r0, r1, c0, c1; // range di righe e colonne visibili
	r0 = m_viewer->yToRow(toBeUpdated.top());
	r1 = m_viewer->yToRow(toBeUpdated.bottom());
	c0 = m_viewer->xToColumn(toBeUpdated.left());
	c1 = m_viewer->xToColumn(toBeUpdated.right());

	TXsheet *xsh = m_viewer->getXsheet();
	if (!xsh)
		return;
	TXshNoteSet *notes = xsh->getNotes();
	int notesCount = notes->getCount();
	int i;
	for (i = 0; i < notesCount; i++) {
		QList<NoteWidget *> noteWidgets = m_viewer->getNotesWidget();
		int widgetCount = noteWidgets.size();
		NoteWidget *noteWidget = 0;
		if (i < widgetCount)
			noteWidget = noteWidgets.at(i);
		else {
			noteWidget = new NoteWidget(m_viewer, i);
			m_viewer->addNoteWidget(noteWidget);
		}
		if (!noteWidget)
			continue;
		int r = notes->getNoteRow(i);
		int c = notes->getNoteCol(i);
		if (r < r0 || r > r1 || c < c0 || c > c1)
			continue;
		TPointD pos = notes->getNotePos(i) + TPointD(m_viewer->columnToX(c), m_viewer->rowToY(r));
		noteWidget->paint(&p, QPoint(pos.x, pos.y), i == m_viewer->getCurrentNoteIndex());
	}
}

//-----------------------------------------------------------------------------

bool CellArea::getEaseHandles(
	int r0,
	int r1,
	double e0,
	double e1,
	int &rh0,
	int &rh1)
{
	if (r1 <= r0 + 4) {
		rh0 = r0;
		rh1 = r1;
		return false;
	}
	if (e0 < 0 || e1 < 0) {
		rh0 = r0;
		rh1 = r1;
		return false;
	}
	if (e0 <= 0 && e1 <= 0) {
		rh0 = r0 + 1;
		rh1 = r1 - 1;
	} else if (e0 <= 0) {
		rh0 = r0 + 1;
		int a = rh0 + 1;
		int b = r1 - 2;
		assert(a <= b);
		rh1 = tcrop((int)(r1 - e1 + 0.5), a, b);
	} else if (e1 <= 0) {
		rh1 = r1 - 1;
		int b = rh1 - 1;
		int a = r0 + 2;
		assert(a <= b);
		rh0 = tcrop((int)(r0 + e0 + 0.5), a, b);
	} else {
		int m = tfloor(0.5 * (r0 + e0 + r1 - e1));
		m = tcrop(m, r0 + 2, r1 - 2);
		int a = r0 + 2;
		int b = tmin(m, r1 - 3);
		assert(a <= b);
		rh0 = tcrop((int)(r0 + e0 + 0.5), a, b);
		a = rh0 + 1;
		b = r1 - 2;
		assert(a <= b);
		rh1 = tcrop((int)(r1 - e1 + 0.5), a, b);
	}
	return true;
}

//-----------------------------------------------------------------------------

void CellArea::paintEvent(QPaintEvent *event)
{
	QRect toBeUpdated = event->rect();

	QPainter p(this);
	p.setClipRect(toBeUpdated);
	p.fillRect(toBeUpdated, QBrush(m_viewer->getEmptyCellColor()));

	drawCells(p, toBeUpdated);
	//drawKeyframe(p, toBeUpdated);
	drawNotes(p, toBeUpdated);

	if (getDragTool())
		getDragTool()->drawCellsArea(p);

	int row = m_viewer->getCurrentRow();
	int col = m_viewer->getCurrentColumn();
	int x = m_viewer->columnToX(col);
	int y = m_viewer->rowToY(row);
	QRect rect = QRect(x + 1, y + 1, ColumnWidth - 3, RowHeight - 2);
	p.setPen(Qt::black);
	p.setBrush(Qt::NoBrush);
	p.drawRect(rect);
}

//-----------------------------------------------------------------------------

class CycleUndo : public TUndo
{
	TStageObject *m_pegbar;
	CellArea *m_area;

public:
	// indices sono le colonne inserite
	CycleUndo(TStageObject *pegbar, CellArea *area) : m_pegbar(pegbar), m_area(area) {}
	void undo() const
	{
		m_pegbar->enableCycle(!m_pegbar->isCycleEnabled());
		m_area->update();
	}
	void redo() const { undo(); }
	int getSize() const { return sizeof *this; }
};
//----------------------------------------------------------

void CellArea::mousePressEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(event->modifiers());
	assert(!m_isPanning);
	m_isMousePressed = true;
	if (event->button() == Qt::LeftButton) {
		assert(getDragTool() == 0);

		TPoint pos(event->pos().x(), event->pos().y());
		int row = m_viewer->yToRow(pos.y);
		int col = m_viewer->xToColumn(pos.x);
		int x0 = m_viewer->columnToX(col);
		int y1 = m_viewer->rowToY(row) - 1;
		int x = pos.x - x0;

		//Verifico se ho cliccato su una nota
		TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
		int i;
		for (i = notes->getCount() - 1; i >= 0; i--) {
			int r = notes->getNoteRow(i);
			int c = notes->getNoteCol(i);
			TPointD pos = notes->getNotePos(i) + TPointD(m_viewer->columnToX(c), m_viewer->rowToY(r));
			QRect rect(pos.x, pos.y, NoteWidth, NoteHeight);
			if (!rect.contains(event->pos()))
				continue;
			setDragTool(XsheetGUI::DragTool::makeNoteMoveTool(m_viewer));
			m_viewer->setCurrentNoteIndex(i);
			m_viewer->dragToolClick(event);
			event->accept();
			update();
			return;
		}
		//Se non ho cliccato su una nota e c'e' una nota selezionata la deseleziono.
		if (m_viewer->getCurrentNoteIndex() >= 0)
			m_viewer->setCurrentNoteIndex(-1);

		TXsheet *xsh = m_viewer->getXsheet();
		TXshColumn *column = xsh->getColumn(col);

		//Verifico se e' una colonna sound
		bool isSoundColumn = false;
		if (column) {
			TXshSoundColumn *soundColumn = column->getSoundColumn();
			isSoundColumn = (!soundColumn) ? false : true;
		}

		// TObjectHandle *oh = TApp::instance()->getCurrentObject();
		// oh->setObjectId(m_viewer->getObjectId(col));

		// gmt, 28/12/2009. Non dovrebbe essere necessario, visto che dopo
		// verra cambiata la colonna e quindi l'oggetto corrente
		// Inoltre, facendolo qui, c'e' un problema con il calcolo del dpi
		// (cfr. SceneViewer::onLevelChanged()): setObjectId() chiama (in cascata)
		// onLevelChanged(), ma la colonna corrente risulta ancora quella di prima e quindi
		// il dpi viene calcolato male. Quando si cambia la colonna l'oggetto corrente risulta
		// gia' aggiornato e quindi non ci sono altre chiamate a onLevelChanged()
		// cfr bug#5235

		TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));

		m_viewer->getKeyframeSelection()->selectNone();

		if ((!xsh->getCell(row, col).isEmpty()) && x < 6) {
			TXshColumn *column = xsh->getColumn(col);
			if (column && !m_viewer->getCellSelection()->isCellSelected(row, col)) {
				int r0, r1;
				column->getLevelRange(row, r0, r1);
				if (event->modifiers() & Qt::ControlModifier) {
					m_viewer->getCellKeyframeSelection()->makeCurrent();
					m_viewer->getCellKeyframeSelection()->selectCellsKeyframes(r0, col, r1, col);
				} else {
					m_viewer->getKeyframeSelection()->selectNone();
					m_viewer->getCellSelection()->makeCurrent();
					m_viewer->getCellSelection()->selectCells(r0, col, r1, col);
				}
				TApp::instance()->getCurrentSelection()->notifySelectionChanged();
			}
			TSelection *selection = TApp::instance()->getCurrentSelection()->getSelection();
			if (TCellKeyframeSelection *cellKeyframeSelection = dynamic_cast<TCellKeyframeSelection *>(selection))
				setDragTool(XsheetGUI::DragTool::makeCellKeyframeMoverTool(m_viewer));
			else
				setDragTool(XsheetGUI::DragTool::makeLevelMoverTool(m_viewer));
		} else {
			m_viewer->getKeyframeSelection()->selectNone();
			if (isSoundColumn && x > ColumnWidth - 6 && x < ColumnWidth)
				setDragTool(XsheetGUI::DragTool::makeSoundScrubTool(m_viewer, column->getSoundColumn()));
			else if (m_levelExtenderRect.contains(pos.x, pos.y))
				setDragTool(XsheetGUI::DragTool::makeLevelExtenderTool(m_viewer));
			else if (event->modifiers() & Qt::ControlModifier &&
					 m_upperLevelExtenderRect.contains(pos.x, pos.y))
				setDragTool(XsheetGUI::DragTool::makeLevelExtenderTool(m_viewer, true));
			else if (isSoundColumn && rectContainsPos(m_soundLevelModifyRects, event->pos()))
				setDragTool(XsheetGUI::DragTool::makeSoundLevelModifierTool(m_viewer));
			else
				setDragTool(XsheetGUI::DragTool::makeSelectionTool(m_viewer));
		}
		m_viewer->dragToolClick(event);
	} else if (event->button() == Qt::MidButton) {
		m_pos = event->pos();
		m_isPanning = true;
	}
	event->accept();
	update();
}

//-----------------------------------------------------------------------------

void CellArea::mouseMoveEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(event->modifiers());
	setCursor(Qt::ArrowCursor);
	QPoint pos = event->pos();
	QRect visibleRect = visibleRegion().boundingRect();
	if (m_isPanning) {
		//Pan tasto centrale
		m_viewer->scroll(m_pos - pos);
		return;
	}
	if ((event->buttons() & Qt::LeftButton) != 0 && !visibleRegion().contains(pos)) {
		QRect bounds = visibleRegion().boundingRect();
		m_viewer->setAutoPanSpeed(bounds, pos);
	} else
		m_viewer->stopAutoPan();

	m_pos = pos;
	if (getDragTool()) {
		getDragTool()->onDrag(event);
		return;
	}

	int row = m_viewer->yToRow(pos.y());
	int col = m_viewer->xToColumn(pos.x());
	int x0 = m_viewer->columnToX(col);
	int x = m_pos.x() - x0;

	TXsheet *xsh = m_viewer->getXsheet();

	//Verifico se e' una colonna sound
	TXshColumn *column = xsh->getColumn(col);
	bool isSoundColumn = false;
	bool isZeraryColumn = false;
	if (column) {
		TXshSoundColumn *soundColumn = column->getSoundColumn();
		isSoundColumn = (!soundColumn) ? false : true;
		TXshZeraryFxColumn *zeraryColumn = column->getZeraryFxColumn();
		isZeraryColumn = (!zeraryColumn) ? false : true;
	}

	if ((!xsh->getCell(row, col).isEmpty() || isSoundColumn) && x < 6)
		m_tooltip = tr("Click and drag to move the selection");
	else if (isZeraryColumn)
		m_tooltip = QString::fromStdWString(column->getZeraryFxColumn()->getZeraryColumnFx()->getZeraryFx()->getName());
	else if ((!xsh->getCell(row, col).isEmpty() && !isSoundColumn) && x > 6 && x < ColumnWidth) {
		TXshCell cell = xsh->getCell(row, col);
		TFrameId fid = cell.getFrameId();
		wstring levelName = cell.m_level->getName();

		// convert the last one digit of the frame number to alphabet
		// Ex.  12 -> 1B    21 -> 2A   30 -> 3
		if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
			m_tooltip = (fid.isEmptyFrame() || fid.isNoFrame()) ? QString::fromStdWString(levelName) : QString::fromStdWString(levelName) + QString(" ") + m_viewer->getFrameNumberWithLetters(fid.getNumber());
		} else {
			string frameNumber("");
			if (fid.getNumber() > 0)
				frameNumber = toString(fid.getNumber());
			if (fid.getLetter() != 0)
				frameNumber.append(1, fid.getLetter());
			m_tooltip = QString((frameNumber.empty()) ? QString::fromStdWString(levelName)
													  : QString::fromStdWString(levelName) + QString(" ") + QString::fromStdString(frameNumber));
		}
	} else if (isSoundColumn && x > ColumnWidth - 6 && x < ColumnWidth)
		m_tooltip = tr("Click and drag to play");
	else if (m_levelExtenderRect.contains(pos))
		m_tooltip = tr("Click and drag to repeat selected cells");
	else if (isSoundColumn && rectContainsPos(m_soundLevelModifyRects, pos)) {
		setCursor(Qt::SplitVCursor);
		m_tooltip = tr("");
	} else
		m_tooltip = tr("");
}

//-----------------------------------------------------------------------------

void CellArea::mouseReleaseEvent(QMouseEvent *event)
{
	m_viewer->setQtModifiers(0);
	m_isMousePressed = false;
	m_viewer->stopAutoPan();
	m_isPanning = false;
	m_viewer->dragToolRelease(event);
}

//-----------------------------------------------------------------------------

void CellArea::mouseDoubleClickEvent(QMouseEvent *event)
{
	TPoint pos(event->pos().x(), event->pos().y());
	int row = m_viewer->yToRow(pos.y);
	int col = m_viewer->xToColumn(pos.x);
	//Se la colonna e' sound non devo fare nulla
	TXshColumn *column = m_viewer->getXsheet()->getColumn(col);
	if (column && (column->getSoundColumn() || column->getSoundTextColumn()))
		return;

	//Se ho cliccato su una nota devo aprire il popup
	TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
	int i;
	for (i = notes->getCount() - 1; i >= 0; i--) {
		int r = notes->getNoteRow(i);
		int c = notes->getNoteCol(i);
		TPointD pos = notes->getNotePos(i) + TPointD(m_viewer->columnToX(c), m_viewer->rowToY(r));
		QRect rect(pos.x, pos.y, NoteWidth, NoteHeight);
		if (!rect.contains(event->pos()))
			continue;
		m_viewer->setCurrentNoteIndex(i);
		m_viewer->getNotesWidget().at(i)->openNotePopup();
		return;
	}

	TObjectHandle *oh = TApp::instance()->getCurrentObject();
	oh->setObjectId(m_viewer->getObjectId(col));

	if (col == -1)
		return;

	// in modalita' xsheet as animation sheet non deve essere possibile creare
	// livelli con doppio click: se la cella e' vuota non bisogna fare nulla
	if ((Preferences::instance()->isAnimationSheetEnabled() && m_viewer->getXsheet()->getCell(row, col).isEmpty()))
		return;

	m_renameCell->showInRowCol(row, col);
}

//-----------------------------------------------------------------------------

void CellArea::contextMenuEvent(QContextMenuEvent *event)
{
	TPoint pos(event->pos().x(), event->pos().y());
	int row = m_viewer->yToRow(pos.y);
	int col = m_viewer->xToColumn(pos.x);

	QMenu menu(this);

	//Verifico se ho cliccato su una nota
	TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
	int i;
	for (i = notes->getCount() - 1; i >= 0; i--) {
		int r = notes->getNoteRow(i);
		int c = notes->getNoteCol(i);
		TPointD pos = notes->getNotePos(i) + TPointD(m_viewer->columnToX(c), m_viewer->rowToY(r));
		QRect rect(pos.x, pos.y, NoteWidth, NoteHeight);
		if (!rect.contains(event->pos()))
			continue;
		m_viewer->setCurrentNoteIndex(i);
		createNoteMenu(menu);
		if (!menu.isEmpty())
			menu.exec(event->globalPos());
		return;
	}

	TXsheet *xsh = m_viewer->getXsheet();
	int x0 = m_viewer->columnToX(col);
	int y1 = m_viewer->rowToY(row) - 1;
	int x = pos.x - x0;
	TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
	int r0, r1, c0, c1;
	if (col >= 0)
		m_viewer->getCellSelection()->getSelectedCells(r0, c0, r1, c1);

	if (col >= 0 &&												  //Non e' la colonna di camera
		m_viewer->getCellSelection()->isCellSelected(row, col) && //La cella e' selezionata
		(abs(r1 - r0) > 0 || abs(c1 - c0) > 0))					  //Il numero di celle selezionate e' maggiore di 1
	{															  //Sono su una selezione di celle
		m_viewer->setCurrentColumn(col);
		int e, f;
		bool areCellsEmpty = false;
		for (e = r0; e <= r1; e++) {
			for (f = c0; f <= c1; f++)
				if (!xsh->getCell(e, f).isEmpty()) {
					areCellsEmpty = true;
					break;
				}
			if (areCellsEmpty)
				break;
		}
		createCellMenu(menu, areCellsEmpty);
	} else {
		if (col >= 0) {
			m_viewer->getCellSelection()->makeCurrent();
			m_viewer->getCellSelection()->selectCell(row, col);
			m_viewer->setCurrentColumn(col);
		}
		if (!xsh->getCell(row, col).isEmpty())
			createCellMenu(menu, true);
		else
			createCellMenu(menu, false);
	}
	if (!menu.isEmpty())
		menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void CellArea::dragEnterEvent(QDragEnterEvent *e)
{
	if (acceptResourceOrFolderDrop(e->mimeData()->urls()) ||
		e->mimeData()->hasFormat(CastItems::getMimeFormat()) ||
		e->mimeData()->hasFormat("application/vnd.toonz.drawings")) {
		setDragTool(XsheetGUI::DragTool::makeDragAndDropDataTool(m_viewer));
		m_viewer->dragToolClick(e);
		e->acceptProposedAction();
	}
}

//-----------------------------------------------------------------------------

void CellArea::dragLeaveEvent(QDragLeaveEvent *e)
{
	if (!getDragTool())
		return;
	m_viewer->dragToolLeave(e);
}

//-----------------------------------------------------------------------------

void CellArea::dragMoveEvent(QDragMoveEvent *e)
{
	if (!getDragTool())
		return;
	m_viewer->dragToolDrag(e);
	e->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void CellArea::dropEvent(QDropEvent *e)
{
	if (!getDragTool())
		return;
	m_viewer->dragToolRelease(e);
	if (e->source() == this) {
		e->setDropAction(Qt::MoveAction);
		e->accept();
	} else
		e->acceptProposedAction();
}

//-----------------------------------------------------------------------------

bool CellArea::event(QEvent *event)
{
	QEvent::Type type = event->type();
	if (type == QEvent::ToolTip) {
		if (!m_tooltip.isEmpty())
			QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
		else
			QToolTip::hideText();
	}
	if (type == QEvent::WindowDeactivate && m_isMousePressed) {
		QMouseEvent e(QEvent::MouseButtonRelease, m_pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
		mouseReleaseEvent(&e);
	}
	return QWidget::event(event);
}
//-----------------------------------------------------------------------------

void CellArea::onControlPressed(bool pressed)
{
	isCtrlPressed = pressed;
	update();
}

//-----------------------------------------------------------------------------
void CellArea::createCellMenu(QMenu &menu, bool isCellSelected)
{
	CommandManager *cmdManager = CommandManager::instance();

	bool soundCellsSelected = m_viewer->areSoundCellsSelected();

	if (m_viewer->areSoundTextCellsSelected())
		return; // Magpies stop here

	menu.addSeparator();

	if (!soundCellsSelected) {
		menu.addAction(cmdManager->getAction(MI_LoadLevel));
		menu.addAction(cmdManager->getAction(MI_NewLevel));
		menu.addSeparator();
	}

	if (isCellSelected) {
		if (!soundCellsSelected) {
			menu.addAction(cmdManager->getAction(MI_LevelSettings));

			//- force reframe
			QMenu *reframeSubMenu = new QMenu(tr("Reframe"), this);
			{
				reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe1));
				reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe2));
				reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe3));
				reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe4));
			}
			menu.addMenu(reframeSubMenu);

			QMenu *stepSubMenu = new QMenu(tr("Step"), this);
			{
				stepSubMenu->addAction(cmdManager->getAction(MI_Step2));
				stepSubMenu->addAction(cmdManager->getAction(MI_Step3));
				stepSubMenu->addAction(cmdManager->getAction(MI_Step4));
				stepSubMenu->addAction(cmdManager->getAction(MI_ResetStep));
				stepSubMenu->addAction(cmdManager->getAction(MI_IncreaseStep));
				stepSubMenu->addAction(cmdManager->getAction(MI_DecreaseStep));
			}
			menu.addMenu(stepSubMenu);
			QMenu *eachSubMenu = new QMenu(tr("Each"), this);
			{
				eachSubMenu->addAction(cmdManager->getAction(MI_Each2));
				eachSubMenu->addAction(cmdManager->getAction(MI_Each3));
				eachSubMenu->addAction(cmdManager->getAction(MI_Each4));
			}
			menu.addMenu(eachSubMenu);

			menu.addSeparator();

			menu.addAction(cmdManager->getAction(MI_Reverse));
			menu.addAction(cmdManager->getAction(MI_Swing));
			menu.addAction(cmdManager->getAction(MI_Random));
			menu.addAction(cmdManager->getAction(MI_Dup));

			menu.addAction(cmdManager->getAction(MI_Rollup));
			menu.addAction(cmdManager->getAction(MI_Rolldown));
			menu.addAction(cmdManager->getAction(MI_TimeStretch));
			menu.addSeparator();
			menu.addAction(cmdManager->getAction(MI_Autorenumber));
		}
		menu.addAction(cmdManager->getAction(MI_ReplaceLevel));

		menu.addAction(cmdManager->getAction(MI_ReplaceParentDirectory));

		{
			//replace with another level in scene cast
			std::vector<TXshLevel *> levels;
			TApp::instance()->getCurrentScene()->getScene()->getLevelSet()->listLevels(levels);
			if (!levels.empty()) {
				QMenu *replaceMenu = menu.addMenu(tr("Replace"));
				connect(replaceMenu, SIGNAL(triggered(QAction *)), this, SLOT(onReplaceByCastedLevel(QAction *)));
				for (int i = 0; i < (int)levels.size(); i++) {
					if (!levels[i]->getSimpleLevel() &&
						!levels[i]->getChildLevel())
						continue;

					if (levels[i]->getChildLevel() &&
						!TApp::instance()->getCurrentXsheet()->getXsheet()->isLevelUsed(levels[i]))
						continue;

					QString tmpLevelName = QString::fromStdWString(levels[i]->getName());
					QAction *tmpAction = new QAction(tmpLevelName, replaceMenu);
					tmpAction->setData(tmpLevelName);
					replaceMenu->addAction(tmpAction);
				}
			}
		}

		if (!soundCellsSelected) {
			if (selectionContainTlvImage(m_viewer->getCellSelection(), m_viewer->getXsheet()))
				menu.addAction(cmdManager->getAction(MI_RevertToCleanedUp));
			if (selectionContainLevelImage(m_viewer->getCellSelection(), m_viewer->getXsheet()))
				menu.addAction(cmdManager->getAction(MI_RevertToLastSaved));
		}
		menu.addSeparator();

		TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
		if (sl || soundCellsSelected)
			menu.addAction(cmdManager->getAction(MI_FileInfo));
		if (sl && (sl->getType() & LEVELCOLUMN_XSHLEVEL))
			menu.addAction(cmdManager->getAction(MI_ViewFile));

		menu.addSeparator();
		if (selectionContainRasterImage(m_viewer->getCellSelection(), m_viewer->getXsheet())) {
			menu.addAction(cmdManager->getAction(MI_AdjustLevels));
			menu.addAction(cmdManager->getAction(MI_LinesFade));
			menu.addAction(cmdManager->getAction(MI_BrightnessAndContrast));
			menu.addAction(cmdManager->getAction(MI_Antialias));
			menu.addAction(cmdManager->getAction(MI_CanvasSize));
		} else if (selectionContainTlvImage(m_viewer->getCellSelection(), m_viewer->getXsheet()))
			menu.addAction(cmdManager->getAction(MI_CanvasSize));
	}
	menu.addSeparator();
	if (!soundCellsSelected)
		menu.addAction(cmdManager->getAction(MI_ImportMagpieFile));
}
//-----------------------------------------------------------------------------
/*! replace level with another level in the cast
 */
void CellArea::onReplaceByCastedLevel(QAction *action)
{
	std::wstring levelName = action->data().toString().toStdWString();
	TXshLevel *level = TApp::instance()->getCurrentScene()->getScene()->getLevelSet()->getLevel(levelName);

	if (!level)
		return;
	TCellSelection *cellSelection = m_viewer->getCellSelection();
	if (cellSelection->isEmpty())
		return;
	int r0, c0, r1, c1;
	cellSelection->getSelectedCells(r0, c0, r1, c1);

	bool changed = false;

	TUndoManager *um = TUndoManager::manager();
	um->beginBlock();

	TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
	for (int c = c0; c <= c1; c++) {
		for (int r = r0; r <= r1; r++) {
			TXshCell cell = xsh->getCell(r, c);
			if (!cell.m_level.getPointer() ||
				cell.m_level.getPointer() == level)
				continue;

			TXshCell oldCell = cell;

			cell.m_level = TXshLevelP(level);
			xsh->setCell(r, c, cell);

			RenameCellUndo *undo = new RenameCellUndo(r, c, oldCell, cell);
			um->add(undo);

			changed = true;
		}
	}

	if (changed)
		TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

	um->endBlock();
}

//-----------------------------------------------------------------------------

void CellArea::createKeyMenu(QMenu &menu)
{
	CommandManager *cmdManager = CommandManager::instance();
	menu.addAction(cmdManager->getAction(MI_SelectRowKeyframes));
	menu.addAction(cmdManager->getAction(MI_SelectColumnKeyframes));
	menu.addAction(cmdManager->getAction(MI_SelectAllKeyframes));
	menu.addAction(cmdManager->getAction(MI_SelectAllKeyframesNotBefore));
	menu.addAction(cmdManager->getAction(MI_SelectAllKeyframesNotAfter));
	menu.addAction(cmdManager->getAction(MI_SelectPreviousKeysInColumn));
	menu.addAction(cmdManager->getAction(MI_SelectFollowingKeysInColumn));
	menu.addAction(cmdManager->getAction(MI_SelectPreviousKeysInRow));
	menu.addAction(cmdManager->getAction(MI_SelectFollowingKeysInRow));
	menu.addAction(cmdManager->getAction(MI_InvertKeyframeSelection));
	menu.addSeparator();
	menu.addAction(cmdManager->getAction(MI_Cut));
	menu.addAction(cmdManager->getAction(MI_Copy));
	menu.addAction(cmdManager->getAction(MI_Paste));
	menu.addAction(cmdManager->getAction(MI_Clear));
	menu.addSeparator();
	menu.addAction(cmdManager->getAction(MI_OpenFunctionEditor));
}

//-----------------------------------------------------------------------------

void CellArea::createKeyLineMenu(QMenu &menu, int row, int col)
{
	TStageObject *pegbar = m_viewer->getXsheet()->getStageObject(m_viewer->getObjectId(col));
	CommandManager *cmdManager = CommandManager::instance();
	int r0, r1, rh0, rh1;
	double e0, e1;
	if (pegbar->getKeyframeSpan(row, r0, e0, r1, e1) && getEaseHandles(r0, r1, e0, e1, rh0, rh1)) {
		menu.addAction(cmdManager->getAction(MI_SetAcceleration));
		menu.addAction(cmdManager->getAction(MI_SetDeceleration));
		menu.addAction(cmdManager->getAction(MI_SetConstantSpeed));
	} else {
		//Se le due chiavi non sono linear aggiungo il comando ResetInterpolation
		bool isR0FullK = pegbar->isFullKeyframe(r0);
		bool isR1FullK = pegbar->isFullKeyframe(r1);
		TDoubleKeyframe::Type r0Type = pegbar->getParam(TStageObject::T_X)->getKeyframeAt(r0).m_type;
		TDoubleKeyframe::Type r1Type = pegbar->getParam(TStageObject::T_X)->getKeyframeAt(r1).m_prevType;
		if (isGlobalKeyFrameWithSameTypeDiffFromLinear(pegbar, r0) && isGlobalKeyFrameWithSamePrevTypeDiffFromLinear(pegbar, r1))
			menu.addAction(cmdManager->getAction(MI_ResetInterpolation));
	}
#ifdef LINETEST
	menu.addSeparator();
	int paramStep = getParamStep(pegbar, r0);
	QActionGroup *actionGroup = new QActionGroup(this);
	int i;
	for (i = 1; i < 4; i++) {
		QAction *act = new QAction(QString("Step ") + QString::number(i), this);
		if (paramStep == i)
			act->setEnabled(false);
		QList<QVariant> list;
		list.append(QVariant(i));
		list.append(QVariant(r0));
		list.append(QVariant(col));
		act->setData(QVariant(list));
		actionGroup->addAction(act);
		menu.addAction(act);
	}
	connect(actionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onStepChanged(QAction *)));
#endif
}

//-----------------------------------------------------------------------------

void CellArea::createNoteMenu(QMenu &menu)
{
	QAction *openAct = menu.addAction(tr("Open Memo"));
	QAction *deleteAct = menu.addAction(tr("Delete Memo"));
	bool ret = connect(openAct, SIGNAL(triggered()), this, SLOT(openNote()));
	ret = ret && connect(deleteAct, SIGNAL(triggered()), this, SLOT(deleteNote()));
}

//-----------------------------------------------------------------------------

void CellArea::openNote()
{
	TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
	int currentIndex = m_viewer->getCurrentNoteIndex();
	m_viewer->getNotesWidget().at(currentIndex)->openNotePopup();
}

//-----------------------------------------------------------------------------

void CellArea::deleteNote()
{
	TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
	int currentIndex = m_viewer->getCurrentNoteIndex();
	notes->removeNote(currentIndex);
	m_viewer->discardNoteWidget();
}

//-----------------------------------------------------------------------------

void CellArea::onStepChanged(QAction *act)
{
#ifdef LINETEST
	QList<QVariant> list = act->data().toList();
	int step = list.at(0).toInt();
	int frame = list.at(1).toInt();
	int col = list.at(2).toInt();

	//Siamo in LineTest il keyframe Ã¨ globale quindi basta calcolare l'indice del primo parametro!!!!
	TUndoManager::manager()->beginBlock();
	TStageObject *stageObject = m_viewer->getXsheet()->getStageObject(m_viewer->getObjectId(col));
	TDoubleParam *param = stageObject->getParam(TStageObject::T_Angle);
	int keyFrameIndex = param->getClosestKeyframe(frame);
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Angle));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_X));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Y));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Z));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_SO));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_ScaleX));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_ScaleY));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Scale));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Path));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_ShearX));
	setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_ShearY));
	TUndoManager::manager()->endBlock();
#endif
}

//-----------------------------------------------------------------------------

} // namespace XsheetGUI;
