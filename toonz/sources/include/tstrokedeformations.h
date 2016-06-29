#pragma once

#ifndef TSTROKES_DEFORMATIONS_H
#define TSTROKES_DEFORMATIONS_H

// #include "tstrokeutil.h"
#include "tgeometry.h"
#include <memory>

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TStroke;

//=============================================================================
/*!
  TStrokeDeformation Abstract class to manage deformation
    in stroke
 */
class DVAPI TStrokeDeformation {
public:
  TStrokeDeformation() {}
  virtual ~TStrokeDeformation() {}

  /*!
return displacement to use with function increaseControlPoints
\par stroke to test
\par w stroke parameter
\ret displacement to apply to obtain deformation
\sa increaseControlPoints
*/
  virtual TThickPoint getDisplacement(const TStroke &stroke,
                                      double w) const = 0;

  /*!
return displacement to use with function  modifyControlPoints
\par stroke to test
\par n control point to get
\ret displacement to apply to obtain deformation
\sa modifyControlPoints
*/
  virtual TThickPoint getDisplacementForControlPoint(const TStroke &stroke,
                                                     UINT n) const = 0;
  virtual TThickPoint getDisplacementForControlPointLen(const TStroke &stroke,
                                                        double cpLen) const = 0;

  /*!
| d getDisplacement() / dw  |
\par stroke to test
\par w stroke parameter
\ret | d getDisplacement() / dw  |
*/
  virtual double getDelta(const TStroke &stroke, double w) const = 0;

  /*!
Max diff of delta (This value indicates when it's necessary
  to insert control point)
\ret max displacement permitted
*/
  virtual double getMaxDiff() const = 0;
};

//=============================================================================

/*!
    Manage the manipulation of a stroke using the metaball.

    Every tools have a constructor like:

    const TStroke *pStroke
    const TPointD &vect
    ... list of parameter.
 */
class DVAPI TStrokePointDeformation final : public TStrokeDeformation {
protected:
  struct Imp;
  std::unique_ptr<Imp> m_imp;

public:
  /*!
Use this constructor with increasePoints.
*/
  TStrokePointDeformation(const TPointD &center = TPointD(),
                          double radius         = 40.0);

  /*!
Use this constructor with modifyControlPoints.
*/
  TStrokePointDeformation(const TPointD &vect,
                          const TPointD &center = TPointD(),
                          double radius         = 40.0);

  virtual ~TStrokePointDeformation();

  TThickPoint getDisplacement(const TStroke &stroke, double s) const override;
  TThickPoint getDisplacementForControlPoint(const TStroke &s,
                                             UINT n) const override;
  TThickPoint getDisplacementForControlPointLen(const TStroke &stroke,
                                                double cpLen) const override;

  double getDelta(const TStroke &stroke, double w) const override;
  double getMaxDiff() const override;
};

//=============================================================================

/*!
  Manage deformation of a stroke using a gaussian potential.

  Function is b*exp( -((x-a)^2)/c ).
 */
class DVAPI TStrokeParamDeformation final : public TStrokeDeformation {
private:
  const TStroke *m_pRef;
  double m_startParameter;
  double m_lengthOfDeformation;
  TPointD *m_vect;

public:
  /*
Use this constructor with increasePoints.
\param  stroke  reference to stroke to deform.
\param  vect size of movement.
\param  param parameter of nearest point in stroke.
\param  lengthOfDeformation  length of piece of stroke to move.
*/
  TStrokeParamDeformation(const TStroke *stroke, double parameterOfNearest,
                          double lengthOfDeformation = 100);

  /*!
Use this constructor with movePoints.
*/
  TStrokeParamDeformation(const TStroke *stroke, const TPointD &vect,
                          double parameterOfNearest,
                          double lengthOfDeformation = 100);

  virtual ~TStrokeParamDeformation();

  TThickPoint getDisplacement(const TStroke &, double) const override;
  TThickPoint getDisplacementForControlPoint(const TStroke &stroke,
                                             UINT n) const override;
  TThickPoint getDisplacementForControlPointLen(const TStroke &stroke,
                                                double cpLen) const override;

  double getDelta(const TStroke &, double) const override;

  double getMaxDiff() const override;
};

//=============================================================================

/*!
  Manage the deformation of thick in a stroke.
 */
class DVAPI TStrokeThicknessDeformation final : public TStrokeDeformation {
private:
  double m_lengthOfDeformation;
  double m_startParameter;
  double m_versus;
  TPointD *m_vect;
  const TStroke *m_pRef;

public:
  /*
Use this constructor with increasePoints.
\param  stroke  reference to stroke to deform.
\param  vect size of movement.
\param  param parameter of nearest point in stroke.
\param  lengthOfDeformation  length of piece of stroke to move.
*/
  TStrokeThicknessDeformation(const TStroke *stroke, double parameterOfNearest,
                              double lengthOfDeformation = 100);

  /*!
Use this constructor with movePoints.
*/
  TStrokeThicknessDeformation(const TStroke *stroke, const TPointD &vect,
                              double parameterOfNearest,
                              double lengthOfDeformation = 100,
                              double versus              = 1.0);

  virtual ~TStrokeThicknessDeformation();

  TThickPoint getDisplacement(const TStroke &, double) const override;
  TThickPoint getDisplacementForControlPoint(const TStroke &stroke,
                                             UINT n) const override;
  TThickPoint getDisplacementForControlPointLen(const TStroke &stroke,
                                                double cpLen) const override;

  double getDelta(const TStroke &, double) const override;

  double getMaxDiff() const override;
};
//=============================================================================
/*!
  Manage the bending of a stroke.
 */
class DVAPI TStrokeBenderDeformation final : public TStrokeDeformation {
private:
  const TStroke *m_pRef;
  double m_startLength;
  double m_lengthOfDeformation;
  TPointD *m_vect;
  int m_versus;

  double m_angle;

public:
  enum VERSUS { INNER = 0, OUTER };

  /*!
Use this constructor with increasePoints.
 \param stroke a refernce to a stroke
 \param parameterOfNearest
 \param lengthOfDeformation
*/
  TStrokeBenderDeformation(const TStroke *stroke, double parameterOfNearest,
                           double lengthOfDeformation = 50);

  /*!
Use this constructor with movePoints.
*/
  TStrokeBenderDeformation(const TStroke *stroke, const TPointD &centerOfRot,
                           double angle, double parameterOfNearest,
                           int innerOrOuter           = INNER,
                           double lengthOfDeformation = 50);

  virtual ~TStrokeBenderDeformation();

  TThickPoint getDisplacement(const TStroke &, double s) const override;
  TThickPoint getDisplacementForControlPoint(const TStroke &,
                                             UINT n) const override;
  TThickPoint getDisplacementForControlPointLen(const TStroke &stroke,
                                                double cpLen) const override;

  double getDelta(const TStroke &, double) const override;
  double getMaxDiff() const override;
};

//=============================================================================

/*!
  Manage wirling of a stroke.
 */
class DVAPI TStrokeTwirlDeformation final : public TStrokeDeformation {
private:
  TPointD m_center;
  double m_innerRadius2;
  TPointD m_vectorOfMovement;
  double m_outerRadius;

public:
  /*!
Use this constructor with increasePoints.
 seg is the segment used to compute intersection .
*/
  TStrokeTwirlDeformation(const TPointD &center, double radius);

  TStrokeTwirlDeformation(const TPointD &center, double radius,
                          const TPointD &v);

  virtual ~TStrokeTwirlDeformation();

  TThickPoint getDisplacement(const TStroke &, double s) const override;
  double getDelta(const TStroke &, double) const override;
  double getMaxDiff() const override;
};

//=============================================================================

/*!
  New deformer class. NOT YET COMPLETED...
 */
class DVAPI TPointDeformation {
  const TStroke *m_strokeRef;
  TPointD m_center;
  double m_radius;

  // return ratio density/length
  double getCPDensity(double s) const;

public:
  TPointDeformation(const TStroke *, const TPointD &, double center);

  TPointDeformation();

  virtual ~TPointDeformation();

  TThickPoint getDisplacement(double s) const;

  // return integral of density in [s0,s1]
  /*
  / s1
 |     density( s ) ds
/
 s0
*/
  double getCPCountInRange(double s0, double s1) const;

  // ritorna il valode di densita' sopra il quale aggiunge punti di controllo
  double getMinCPDensity() const;
};
//=============================================================================

#endif  // TSTROKES_DEFORMATIONS_H
