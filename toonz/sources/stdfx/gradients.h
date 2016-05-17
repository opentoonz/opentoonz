#pragma once

#ifndef GRADIENTS_H
#define GRADIENTS_H

#include "tfxparam.h"
#include "trop.h"
#include "trasterfx.h"

struct MultiRAdialParams {
	int m_shrink;
	double m_scale;

	double m_intensity;
	double m_gridStep;
};

/*---------------------------------------------------------------------------*/

//!Deals with raster tiles and invokes multiradial functions

void multiRadial(
	const TRasterP &ras,
	TPointD posTrasf,
	const TSpectrumParamP colors,
	double period,
	double count,
	double cycle,
	const TAffine &aff,
	double frame);

void multiLinear(
	const TRasterP &ras,
	TPointD posTrasf,
	const TSpectrumParamP colors,
	double period,
	double count,
	double amplitude,
	double freq,
	double phase,
	double cycle,
	const TAffine &aff,
	double frame);

#endif
