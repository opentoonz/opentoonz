#pragma once

#ifndef CANVAS_INCLUDED
#define CANVAS_INCLUDED

//#include "tcommon.h"
#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TCanvas : public TWidget
{
protected:
public:
	TCanvas(TWidget *parent, string name = "canvas");
	~TCanvas();

	virtual void draw();
};

#endif
