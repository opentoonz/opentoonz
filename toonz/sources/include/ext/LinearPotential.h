#pragma once

#ifndef LINEAR_POTENTIAL_H
#define LINEAR_POTENTIAL_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

//#include "tcommon.h"
//#include "tstroke.h"

#include "ext/Potential.h"

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
	LinearPotential
	: public Potential
{
public:
	virtual ~LinearPotential();

	virtual double
	value_(double radiusToTest) const;

	virtual void
	setParameters_(const TStroke *ref,
				   double w,
				   double actionLength);

	Potential *
	clone();

private:
	double
	compute_shape(double) const; // funzione ausiliaria per
	// il calcolo del parametro
	// da usare

	double
	compute_value(double) const; // funzione ausiliaria per
	// il calcolo del potenziale senza
	// controllo del parametro
	const TStroke *ref_;
	double
		/**
       * @brief Range of mapping
       * 
       * Only an interval of potential function will be used to
       * map the tranformation.
       */
		range_,
		par_,			//! Parameter of selection.
		actionLength_,  //! Action length, how many units to move.
		strokeLength_,  //! Stroke Length.
		lenghtAtParam_, //! Length at <code>par_</code>
		leftFactor_,	//! How many units to move on the left of curve.
		rightFactor_;   //! How many units to move on the right of curve.
};
}
#endif /* LINEAR_POTENTIAL_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
