

#include "keyframeselection.h"

// Tnz6 includes
#include "keyframedata.h"
#include "cellkeyframedata.h"
#include "tapp.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/stageobjectutil.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshcolumn.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//-----------------------------------------------------------------------------
namespace
{
//-----------------------------------------------------------------------------

struct PegbarArgument {
	TStageObject *m_stageObject;
	std::set<int> m_frames;
};

//-----------------------------------------------------------------------------

bool shiftKeyframesWithoutUndo(int r0, int r1, int c0, int c1, bool cut)
{
	int delta = cut ? -(r1 - r0 + 1) : r1 - r0 + 1;
	if (delta == 0)
		return false;
	TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
	bool isShifted = false;
	int x;
	for (x = c0; x <= c1; x++) {
		TStageObject *stObj = xsh->getStageObject(x >= 0 ? TStageObjectId::ColumnId(x) : TStageObjectId::CameraId(0));
		std::set<int> keyToShift;
		int kr0, kr1;
		stObj->getKeyframeRange(kr0, kr1);
		int i = r0;
		while (i <= kr1) {
			if (stObj->isKeyframe(i))
				keyToShift.insert(i);
			i++;
		}
		isShifted = stObj->moveKeyframes(keyToShift, delta);
	}
	return isShifted;
}

//-----------------------------------------------------------------------------

void copyKeyframesWithoutUndo(std::set<TKeyframeSelection::Position> *positions)
{
	QClipboard *clipboard = QApplication::clipboard();
	TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
	TKeyframeData *data = new TKeyframeData();
	data->setKeyframes(*positions, xsh);
	clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

bool pasteKeyframesWithoutUndo(const TKeyframeData *data, std::set<TKeyframeSelection::Position> *positions)
{
	if (!data || positions->empty())
		return false;
	TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
	if (!data->getKeyframes(*positions, xsh))
		return false;
	return true;
}

//-----------------------------------------------------------------------------

bool deleteKeyframesWithoutUndo(std::set<TKeyframeSelection::Position> *positions)
{
	TApp *app = TApp::instance();
	assert(app);
	TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
	TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();

	if (positions->empty())
		return false;

	std::set<TKeyframeSelection::Position>::iterator it = positions->begin();
	bool areAllColumnLocked = true;
	for (; it != positions->end(); ++it) {
		int row = it->first;
		int col = it->second;
		TStageObject *pegbar = xsh->getStageObject(col >= 0 ? TStageObjectId::ColumnId(col) : cameraId);
		if (pegbar->getId().isColumn() && xsh->getColumn(col) && xsh->getColumn(col)->isLocked())
			continue;
		areAllColumnLocked = false;
		assert(pegbar);
		pegbar->removeKeyframeWithoutUndo(row);
	}
	if (areAllColumnLocked)
		return false;

	positions->clear();
	return true;
}

//=============================================================================
//  PasteKeyframesUndo
//-----------------------------------------------------------------------------

class PasteKeyframesUndo : public TUndo
{
	TKeyframeSelection *m_selection;
	QMimeData *m_newData;
	QMimeData *m_oldData;
	int m_r0, m_r1, m_c0, m_c1;

public:
	PasteKeyframesUndo(TKeyframeSelection *selection, QMimeData *newData, QMimeData *oldData, int r0, int r1, int c0, int c1)
		: m_selection(selection), m_newData(newData), m_oldData(oldData), m_r0(r0), m_r1(r1), m_c0(c0), m_c1(c1)
	{
	}

	~PasteKeyframesUndo()
	{
		delete m_selection;
		delete m_newData;
		delete m_oldData;
	}
	//data->xsh
	void setXshFromData(QMimeData *data) const
	{
		const TKeyframeData *keyframeData = dynamic_cast<TKeyframeData *>(data);
		if (keyframeData)
			pasteKeyframesWithoutUndo(keyframeData, &m_selection->getSelection());
	}
	void undo() const
	{
		//Delete merged data
		deleteKeyframesWithoutUndo(&m_selection->getSelection());
		if (-(m_r1 - m_r0 + 1) != 0)
			shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, true);
		if (m_oldData)
			setXshFromData(m_oldData);
		TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
	}
	void redo() const
	{
		if (m_r1 - m_r0 + 1 != 0)
			shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, false);
		//Delete merged data
		setXshFromData(m_newData);
		TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
	}
	int getSize() const
	{
		return sizeof(*this);
	}

	QString getHistoryString()
	{
		return QObject::tr("Paste Key Frames");
	}
};

//=============================================================================
//  DeleteKeyframesUndo
//-----------------------------------------------------------------------------

class DeleteKeyframesUndo : public TUndo
{
	TKeyframeSelection *m_selection;
	QMimeData *m_data;
	int m_r0, m_r1, m_c0, m_c1;

public:
	DeleteKeyframesUndo(TKeyframeSelection *selection, QMimeData *data, int r0, int r1, int c0, int c1)
		: m_selection(selection), m_data(data), m_r0(r0), m_r1(r1), m_c0(c0), m_c1(c1)
	{
	}

	~DeleteKeyframesUndo()
	{
		delete m_selection;
		delete m_data;
	}

	void undo() const
	{
		const TKeyframeData *keyframeData = dynamic_cast<TKeyframeData *>(m_data);
		if (m_r1 - m_r0 + 1 != 0)
			shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, false);
		if (keyframeData)
			pasteKeyframesWithoutUndo(keyframeData, &m_selection->getSelection());
		TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
	}

	void redo() const
	{
		deleteKeyframesWithoutUndo(&m_selection->getSelection());
		if (m_r1 - m_r0 + 1 != 0)
			shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, true);
		TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
	}

	int getSize() const
	{
		return sizeof(*this);
	}

	QString getHistoryString()
	{
		return QObject::tr("Delete Key Frames");
	}
};

//-----------------------------------------------------------------------------
} // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TKeyframeSelection
//-----------------------------------------------------------------------------

void TKeyframeSelection::enableCommands()
{
	enableCommand(this, MI_Copy, &TKeyframeSelection::copyKeyframes);
	enableCommand(this, MI_Paste, &TKeyframeSelection::pasteKeyframes);
	enableCommand(this, MI_Cut, &TKeyframeSelection::cutKeyframes);
	enableCommand(this, MI_Clear, &TKeyframeSelection::deleteKeyframes);
}

//-----------------------------------------------------------------------------

int TKeyframeSelection::getFirstRow() const
{
	if (isEmpty())
		return 0;
	return m_positions.begin()->first;
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::unselectLockedColumn()
{
	TApp *app = TApp::instance();
	assert(app);
	TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
	std::set<Position> positions;
	std::set<Position>::iterator it;
	for (it = m_positions.begin(); it != m_positions.end(); ++it) {
		int col = it->second;
		if (col >= 0 && xsh->getColumn(col) && xsh->getColumn(col)->isLocked())
			continue;
		positions.insert(*it);
	}
	m_positions.swap(positions);
}

//-----------------------------------------------------------------------------

bool TKeyframeSelection::select(const TSelection *s)
{
	if (const TKeyframeSelection *ss = dynamic_cast<const TKeyframeSelection *>(s)) {
		std::set<Position> pos(ss->m_positions);
		m_positions.swap(pos);
		makeCurrent();
		return true;
	} else
		return false;
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::setKeyframes()
{
	TApp *app = TApp::instance();
	TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
	TXsheet *xsh = xsheetHandle->getXsheet();
	TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
	if (isEmpty())
		return;
	Position pos = *m_positions.begin();
	int row = pos.first;
	int col = pos.second;
	TStageObjectId id = col < 0 ? cameraId : TStageObjectId::ColumnId(col);
	TStageObject *pegbar = xsh->getStageObject(id);
	if (!pegbar)
		return;
	if (pegbar->isKeyframe(row)) {
		TStageObject::Keyframe key = pegbar->getKeyframe(row);
		pegbar->removeKeyframeWithoutUndo(row);
		UndoRemoveKeyFrame *undo = new UndoRemoveKeyFrame(id, row, key, xsheetHandle);
		undo->setObjectHandle(app->getCurrentObject());
		TUndoManager::manager()->add(undo);
	} else {
		pegbar->setKeyframeWithoutUndo(row);
		UndoSetKeyFrame *undo = new UndoSetKeyFrame(id, row, xsheetHandle);
		undo->setObjectHandle(app->getCurrentObject());
		TUndoManager::manager()->add(undo);
	}
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
	TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::copyKeyframes()
{
	if (isEmpty())
		return;
	copyKeyframesWithoutUndo(&m_positions);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::pasteKeyframes()
{
	pasteKeyframesWithShift(0, 0, -1, -1);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::deleteKeyframes()
{
	deleteKeyframesWithShift(0, -1, 0, -1);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::cutKeyframes()
{
	copyKeyframes();
	deleteKeyframesWithShift(0, -1, 0, -1);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::pasteKeyframesWithShift(int r0, int r1, int c0, int c1)
{
	unselectLockedColumn();

	//Retrieve keyframes to paste
	QClipboard *clipboard = QApplication::clipboard();
	const TKeyframeData *data = dynamic_cast<const TKeyframeData *>(clipboard->mimeData());
	if (!data) {
		const TCellKeyframeData *cellKeyframeData = dynamic_cast<const TCellKeyframeData *>(clipboard->mimeData());
		if (cellKeyframeData)
			data = cellKeyframeData->getKeyframeData();
	}
	if (!data)
		return;

	//Retrieve corresponding old keyframes
	std::set<TKeyframeSelection::Position> positions(m_positions);
	data->getKeyframes(positions);

	TKeyframeData *oldData = new TKeyframeData();
	TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
	oldData->setKeyframes(positions, xsh);

	bool isShift = shiftKeyframesWithoutUndo(r0, r1, c0, c1, false);
	bool isPaste = pasteKeyframesWithoutUndo(data, &m_positions);
	if (!isPaste && !isShift) {
		delete oldData;
		return;
	}

	TKeyframeData *newData = new TKeyframeData();
	newData->setKeyframes(m_positions, xsh);
	TKeyframeSelection *selection = new TKeyframeSelection(m_positions);
	TUndoManager::manager()->add(new PasteKeyframesUndo(selection, newData, oldData, r0, r1, c0, c1));
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
	TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::deleteKeyframesWithShift(int r0, int r1, int c0, int c1)
{
	unselectLockedColumn();

	TKeyframeData *data = new TKeyframeData();
	TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
	data->setKeyframes(m_positions, xsh);
	if (m_positions.empty()) {
		delete data;
		return;
	}
	TKeyframeSelection *selection = new TKeyframeSelection(m_positions);
	bool deleteKeyFrame = deleteKeyframesWithoutUndo(&m_positions);
	bool isShift = shiftKeyframesWithoutUndo(r0, r1, c0, c1, true);
	if (!deleteKeyFrame && !isShift) {
		delete selection;
		delete data;
		return;
	}
	TUndoManager::manager()->add(new DeleteKeyframesUndo(selection, data, r0, r1, c0, c1));
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
	TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}
