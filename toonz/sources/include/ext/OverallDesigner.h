#pragma once

#ifndef OVERALLDESIGNER_H
#define OVERALLDESIGNER_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"
//#include "tgeometry.h"

#include "ext/Designer.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace ToonzExt
{
class DVAPI
	OverallDesigner
	: public Designer
{
	int
		x_,
		y_;
	double
		scale_,
		pixelSize_;

	void
	setPosition();

public:
	OverallDesigner(int x,
					int y);
	virtual ~OverallDesigner();
	void draw(SmoothDeformation *);
	void draw(CornerDeformation *);
	void draw(StraightCornerDeformation *);
	void draw(StrokeDeformation *);
	void draw(Selector *);
};
}
#endif /* OVERALLDESIGNER_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
