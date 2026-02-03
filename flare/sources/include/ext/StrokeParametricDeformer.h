#pragma once

#ifndef STROKE_PARAMETRIC_DEFORMER_H
#define STROKE_PARAMETRIC_DEFORMER_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"
#include "tstrokedeformations.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
// to avoid annoying warning
#pragma warning(push)
#pragma warning(disable : 4290)
#endif

class TStroke;

namespace ToonzExt {
class Potential;
/**
   * @brief This class implements new deformer.
   *
   * New stroke deformer doesn't change last point of stroke.
   */
class DVAPI StrokeParametricDeformer final : public TStrokeDeformation {
public:
  StrokeParametricDeformer(double actionLength, double startParameter,
                           TStroke *s, Potential *);

  ~StrokeParametricDeformer();

  /**
*@brief Set mouse movement from last valid position.
*@param vx  horyzontal
*@param vy  vertical
*/
  void setMouseMove(double vx, double vy);

  /**
*@brief Return displacement to use with function increaseControlPoints
*@param stroke to test
*@param w stroke parameter
*@return displacement to apply to obtain deformation
*@sa increaseControlPoints
*/
  TThickPoint getDisplacement(const TStroke &stroke, double w) const override;

  /**
*@brief Return displacement to use with function  modifyControlPoints
*@param stroke to test
*@param n control point to get
*@return displacement to apply to obtain deformation
*@sa modifyControlPoints
*/
  TThickPoint getDisplacementForControlPoint(const TStroke &stroke,
                                             UINT n) const override;
  TThickPoint getDisplacementForControlPointLen(const TStroke &stroke,
                                                double cpLen) const override;

  /**
*@brief This method compute the delta (gradient) referred to stroke in
*       at parameter w.
*
* This value is the result of \f$ \frac{getDisplacement(stroke,w)}{dw} \f$.
*@note Sometimes this value can be approximated.
*@param stroke Stroke to test
*@param w Stroke parameter
*@return the @b gradient in w
*/
  double getDelta(const TStroke &stroke, double w) const override;

  /**
*@brief Max diff of delta (This value indicates when it's necessary
*  to insert control point)
*@return max displacement permitted
*/
  double getMaxDiff() const override;

  // just for debug
  const Potential *getPotential() const { return pot_; }

  /**
*@brief Change sensitivity of deformer (just for debug).
*/
  void setDiff(double diff) { diff_ = diff; }

  /**
*@brief Retrieve the parameters range where is applied deformation.
*/
  void getRange(double &from, double &to);

private:
  StrokeParametricDeformer(const StrokeParametricDeformer &);
  StrokeParametricDeformer &operator=(const StrokeParametricDeformer &);

  // mouse incremental movement
  double vx_, vy_;

  // parameter where is applicated action
  double startParameter_;

  // like startParameter_ but recover length
  double startLength_;

  // how many traits move
  double actionLength_;

  // deformation shape
  Potential *pot_;

  // sensitivity of deformer
  // Indica il valore minimo a partire dal quale
  //  l'inseritore comincia a mettere punti di controllo
  double diff_;

  TStroke *ref_copy_;
};
}

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(pop)
#endif

#endif /* STROKE_PARAMETRIC_DEFORMER_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
