#pragma once

#ifndef TNZ_CLIPBOARD_INCLUDED
#define TNZ_CLIPBOARD_INCLUDED

//#include "tw/tw.h"
#include "tdata.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class DVAPI TClipboard
{ // singleton
	static TClipboard *m_instance;
	TClipboard();
	~TClipboard();

public:
	static TClipboard *instance();
	//! la clipboard mantiene l'ownership

	TDataP getData() const;

	//! la clipboard acquisisce l'ownership
	void setData(const TDataP &data);
};

//-------------------------------------------------------------------

#endif
