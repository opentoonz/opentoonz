#pragma once

#ifndef NOT_Symmetric_EXP_POTENTIAL_H
#define NOT_Symmetric_EXP_POTENTIAL_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

/**
 * @brief This class is to use a not symmetric exponential potential.
 *
 *  @code
 *          __|_
 *        /   | |
 *    __/     |  \___
 *
 *                                         2
 *                    {   -[(x  - t) / d1]
 *                    {  e [             ]   for x < 0
 *                    {
 *      potential(x)= {
 *                    {                     2
 *                    {   -[(x  - t) / d2]
 *                    {  e [             ] for x >= 0
 *
 * con d   min( length_at_param, action_length *0.5)
 *      1
 * ed  d   min( curve_length - length_at_param, action_length *0.5)
 *      2
 *  @endcode
 */

#include "tcommon.h"
#include "tstroke.h"
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

namespace ToonzExt {
class DVAPI NotSymmetricExpPotential final : public Potential {
public:
  virtual ~NotSymmetricExpPotential();

  // chiama compute_value ma effettua un controllo del parametro
  double value_(double radiusToTest) const override;

  void setParameters_(const TStroke *ref, double par, double al) override;
  Potential *clone() override;

private:
  double compute_shape(double) const;  // funzione ausiliaria per
  // il calcolo del parametro
  // da usare

  double compute_value(double) const;  // funzione ausiliaria per
  // il calcolo del potenziale senza
  // controllo del parametro
  const TStroke *ref_;
  double range_;  // range of mapping
  double par_;
  double actionLength_;   // lunghezza dell'azione
  double strokeLength_;   // lunghezza stroke
  double lengthAtParam_;  // lunghezza nel pto di movimento
  double leftFactor_;     // fattore di shape x la curva a sinistra
  double rightFactor_;    // fattore di shape x la curva a dx
};
}
#endif /* NOT_Symmetric_EXP_POTENTIAL_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
