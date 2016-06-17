#pragma once

//-----------------------------------------------------------------------------
// tellipticbrush.h: interface for the TEllipticBrush class.
//-----------------------------------------------------------------------------

#if !defined(TELLIPTIC_BRUSH_H)
#define TELLIPTIC_BRUSH_H

#ifdef PER_VECCHIO_ELLIPTIC_BRUSH

#include "tbrush.h"

#undef DVAPI
#undef DVVAR
#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declaration

class TStroke;

//=============================================================================

/*!

          |     /
        __b__ / \  angle in degree
       /  |  \   |
  ----|---o---a--|-- >
       \__|__/
          |

  N.B. Imp manages angle in radiant.


 */

//=============================================================================

class DVAPI TEllipticBrush : public TBrush {
  struct Imp;
  Imp *m_imp;

public:
  //  TEllipticBrush(double a = 1,double b = 1, double angleInDegree = 0);  //
  //  per brush ellittico
  TEllipticBrush();
  TEllipticBrush(const TEllipticBrush &brush);

  virtual ~TEllipticBrush();

  void makeOutline(const TStroke &stroke, TStrokeOutline &outline,
                   const OutlineParameter &param);
  void draw();
  /*
//  per brush ellittico
void setAngle(double angleInDegree);
double getAngle() const;

double getSemiAxisA() const;
void setSemiAxisA(double);

double getSemiAxisB() const;
void setSemiAxisB(double);
*/
  TBrush *clone();

private:
  // not implemented
  TEllipticBrush &operator=(const TEllipticBrush &brush);
};

#endif  // PER_VECCHIO_ELLIPTIC_BRUSH

#endif  // !defined(TELLIPTIC_BRUSH_H)
//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
