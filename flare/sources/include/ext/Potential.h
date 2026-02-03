#pragma once

#ifndef POTENTIAL_H
#define POTENTIAL_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <assert.h>
#include <stdexcept>
#include <string>

class TStroke;

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
// to avoid annoying warning
#pragma warning(push)
#pragma warning(disable : 4290)
#endif

namespace ToonzExt {
/**
  * @brief Potential is an abstraction to maintain a
  * mathematical potential function.
  *
  * It is used from StrokeDeformation to change
  * the behaviour of deformation that user need to do.
  */
class DVAPI Potential {
  bool isValid_;

protected:
  /**
*@brief The value of potential at parameter w on
* the stroke.
*@param w The parameter on stroke.
*/
  virtual double value_(double w) const = 0;

  /**
*@brief Change the parameter of object that has been created.
*
*This method is the real implementation that a potential
*needs to implement.
*@param theStroke The stroke to change.
*@param w The parameter on stroke.
*@param actionLength How many stroke to change.
*/
  virtual void setParameters_(const TStroke *theStroke, double w,
                              double actionLength) = 0;

public:
  Potential();

  /**
*@brief Just a wrapper for setParameters_.
*@sa setParameters_
*/
  void setParameters(const TStroke *ref, double w, double actionLength);

  /**
*@brief Just a wrapper for value_.
*@sa value_
*/
  double value(double at) const;

  /**
*@brief This is method required to use a Prototype Pattern.
*/
  virtual Potential *clone() = 0;

  virtual ~Potential() {}
};
}

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(pop)
#endif

#endif /* POTENTIAL_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
