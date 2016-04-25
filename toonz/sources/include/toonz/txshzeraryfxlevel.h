#pragma once

#ifndef TXSHZERARYFXLEVEL_INCLUDED
#define TXSHZERARYFXLEVEL_INCLUDED

#include "toonz/txshlevel.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//forward declarations
class TXsheet;
class TXshZeraryFxColumn;

//=============================================================================
//!The TXshZeraryFxLevel class provides a zerary fx level of xsheet and allows its management.
/*!Inherits \b TXshLevel.
 */
//=============================================================================

class DVAPI TXshZeraryFxLevel : public TXshLevel
{
	PERSIST_DECLARATION(TXshZeraryFxLevel)

	TXshZeraryFxColumn *m_zeraryFxColumn;

	DECLARE_CLASS_CODE
public:
	TXshZeraryFxLevel();
	~TXshZeraryFxLevel();

	/*!
    Return the \b TXshZeraryFxLevel zerary fx level.
  */
	TXshZeraryFxLevel *getZeraryFxLevel() { return this; }

	void setColumn(TXshZeraryFxColumn *column);
	TXshZeraryFxColumn *getColumn() const { return m_zeraryFxColumn; }

	void loadData(TIStream &is);
	void saveData(TOStream &os);

	void load();
	void save();

private:
	// not implemented
	TXshZeraryFxLevel(const TXshZeraryFxLevel &);
	TXshZeraryFxLevel &operator=(const TXshZeraryFxLevel &);
};

typedef TSmartPointerT<TXshZeraryFxLevel> TXshZeraryFxLevelP;

#endif
