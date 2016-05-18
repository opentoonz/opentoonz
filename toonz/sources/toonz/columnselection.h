#pragma once

#ifndef TCOLUMNSELECTION_H
#define TCOLUMNSELECTION_H

#include "toonzqt/selection.h"
#include "tgeometry.h"
#include <set>

//=============================================================================
// TColumnSelection
//-----------------------------------------------------------------------------

class TColumnSelection : public TSelection
{
	std::set<int> m_indices;

public:
	TColumnSelection();
	~TColumnSelection();

	bool isEmpty() const;
	void selectColumn(int col, bool on = true);
	void selectNone();

	bool isColumnSelected(int col) const;

	const std::set<int> &getIndices() const { return m_indices; }

	void enableCommands();

	void copyColumns();
	void pasteColumns();
	void deleteColumns();
	void cutColumns();
	void insertColumns();

	void collapse();
	void explodeChild();
	void resequence();
	void cloneChild();

	void hideColumns();

	void reframeCells(int count);
	void reframe1Cells() { reframeCells(1); }
	void reframe2Cells() { reframeCells(2); }
	void reframe3Cells() { reframeCells(3); }
	void reframe4Cells() { reframeCells(4); }
};

#endif //TCELLSELECTION_H
