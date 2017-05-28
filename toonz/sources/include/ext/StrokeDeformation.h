#pragma once

#ifndef STROKE_DEFORMATION_H
#define STROKE_DEFORMATION_H

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

#include "tgeometry.h"
//#include "ext/TriParam.h"
#include "ext/Types.h"
#include "ext/ContextStatus.h"

#include <vector>

// forward declarations

class TStroke;

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
// to avoid annoying warning
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace ToonzExt {
class Designer;
class Potential;
class StrokeParametricDeformer;
class StrokeDeformationImpl;

//===========================================================================

/**
   * @brief This class is the interface for manipulation
   *        algorihms.
   *
   * StrokeDeformation is a wrapper for for StrokeDeformationImpl,
   * its function is to verify state and parameteres before
   * to call the implementation methods of deformation.
   *
   * There is an internal status to verify that actions:
   * @arg @c CREATED on constructor
   * @arg @c ACTIVE on active
   * @arg @c UPDATING on updating
   * @arg @c DEACTIVE on deactive
   *
   * If some error of state occurs @c reset method is called.
   * ContextStatus contains information about deformation parameter,
   * and it occurs to reduce the number of parameters required.
   */
class DVAPI StrokeDeformation {
private:
  StrokeDeformationImpl *deformationImpl_;

  /**
*@brief Internal status.
*/
  enum StrokeDeformationState { CREATED, ACTIVE, UPDATING, DEACTIVE, RESETTED };

  StrokeDeformationState state_;

  /**
*@brief Recover from an invalid state
*/
  void recover();

  /**
*@brief Retrieve the current deformator.
*/
  StrokeDeformationImpl *retrieveDeformator(const ContextStatus *status);

public:
  StrokeDeformation();

  ~StrokeDeformation();

  /**
*@brief Init deformation and add control points.
*/
  void activate(const ContextStatus *);

  /**
*@brief Modify stroke.
*/
  void update(const TPointD &delta);

  /**
*@brief Return a stroke deformed.
*@return A stroke deformed.
*/
  TStroke *deactivate();

  /**
*@brief Clear inner status of Deformation.
*/
  void reset();

  /**
*@brief Just select correct Deformation.
*/
  void check(const ContextStatus *);

  /**
*@brief Apply a designer on current deformation.
*/
  void draw(Designer *);

  /**
*@brief Retrieve valid extremes for current manipulator/deformator.
*/
  ToonzExt::Interval getExtremes() const;

  /**
*@brief Return the stroke selected from user.
*/
  const TStroke *getStroke() const;

  /**
*@brief Return the stroke selected from user.
*/
  const TStroke *getCopiedStroke() const;

  /**
*@brief Return a reference to the stroke created to be manipulated.
*@note This stroke is different from stroke selected by user.
*@sa getStroke
*/
  const TStroke *getTransformedStroke() const;

  /**
*@brief Return the internal status of current deformation.
*@sa ContextStatus
*/
  const ContextStatus *getStatus() const;

  /**
*@brief Retrieve cursor associated to current deformator.
*/
  int getCursorId() const;

#ifdef _DEBUG
  /**
*@brief Return the potential used in implementation.
*@note This is useful just for debug, please do not use directly.
*@sa Potential
*/
  const Potential *getPotential() const;

  /**
*@brief Return the potential used in implementation.
*@note This is useful just for debug, please do not use directly.
*@sa Potential
*/
  const StrokeDeformationImpl *getDeformationImpl() const;
#endif
};
}

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(pop)
#endif

#endif /* STROKE_DEFORMATION_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
