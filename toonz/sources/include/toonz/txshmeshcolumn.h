

#ifndef TXSHMESHCOLUMN_H
#define TXSHMESHCOLUMN_H

#include "toonz/txshcolumn.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//*******************************************************************************
//    TXshMeshColumn  declaration
//*******************************************************************************

class DVAPI TXshMeshColumn : public TXshCellColumn
{
	PERSIST_DECLARATION(TXshMeshColumn)

public:
	TXshMeshColumn();

	TXshColumn::ColumnType getColumnType() const { return eMeshType; }
	TXshMeshColumn *getMeshColumn() { return this; }

	TXshColumn *clone() const;

	bool canSetCell(const TXshCell &cell) const;

	void loadData(TIStream &is);
	void saveData(TOStream &is);

private:
	// Not copiable
	TXshMeshColumn(const TXshMeshColumn &);
	TXshMeshColumn &operator=(const TXshMeshColumn &);
};

//---------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TXshMeshColumn>;
#endif

typedef TSmartPointerT<TXshMeshColumn> TXshMeshColumnP;

#endif // TXSHMESHCOLUMN_H
