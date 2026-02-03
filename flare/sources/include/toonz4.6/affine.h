#pragma once

#ifndef _AFFINE_H_
#define _AFFINE_H_

#undef TNZAPI
#ifdef TNZ_IS_RASTERLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

typedef struct {        /* to be PRE-multiplied with a COLUMN vector, i.e., */
  double a11, a12, a13; /* x = a11*u + a12*v + a13 */
  double a21, a22, a23; /* y = a21*u + a22*v + a23 */
} AFFINE;

#define AFF_M_V_1(AFF, V1, V2) ((AFF).a11 * (V1) + (AFF).a12 * (V2) + (AFF).a13)
#define AFF_M_V_2(AFF, V1, V2) ((AFF).a21 * (V1) + (AFF).a22 * (V2) + (AFF).a23)

static const AFFINE Aff_I = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};

TNZAPI AFFINE aff_shift(double dx, double dy, AFFINE aff);
TNZAPI AFFINE aff_shift_post(double dx, double dy, AFFINE aff);
TNZAPI AFFINE aff_rot(double degrees, AFFINE aff);
TNZAPI AFFINE aff_scale(double sx, double sy, AFFINE aff);
TNZAPI AFFINE aff_shear(double shx, double shy, AFFINE aff);
TNZAPI AFFINE aff_place(double u, double v, double x, double y, AFFINE aff);
TNZAPI AFFINE aff_mult(AFFINE aff1, AFFINE aff2);
TNZAPI AFFINE aff_inv(AFFINE aff);
TNZAPI double aff_det(AFFINE aff);

#define AFF_FILL(AFF, A11, A12, A13, A21, A22, A23)                            \
  {                                                                            \
    (AFF).a11 = A11;                                                           \
    (AFF).a12 = A12;                                                           \
    (AFF).a13 = A13;                                                           \
    (AFF).a21 = A21;                                                           \
    (AFF).a22 = A22;                                                           \
    (AFF).a23 = A23;                                                           \
  }

#define AFF_EQ(A, B)                                                           \
  ((A).a11 == (B).a11 && (A).a12 == (B).a12 && (A).a13 == (B).a13 &&           \
   (A).a21 == (B).a21 && (A).a22 == (B).a22 && (A).a23 == (B).a23)

#define AFF_NE(A, B)                                                           \
  ((A).a11 != (B).a11 || (A).a12 != (B).a12 || (A).a13 != (B).a13 ||           \
   (A).a21 != (B).a21 || (A).a22 != (B).a22 || (A).a23 != (B).a23)

#define ROTATE(ang, cxin, cyin, cxout, cyout, aff)                             \
  aff_shift(cxout, cyout, aff_rot(ang, aff_shift(-cxin, -cyin, aff)))

#define ROTATE_POST(ang, cxin, cyin, cxout, cyout, aff)                        \
  aff_mult(aff, aff_shift(cxout, cyout,                                        \
                          aff_rot(ang, aff_shift(-cxin, -cyin, Aff_I))))

#define SCALE(sx, sy, cxin, cyin, cxout, cyout, aff)                           \
  aff_shift(cxout, cyout, aff_scale(sx, sy, aff_shift(-cxin, -cyin, aff)))

#define SCALE_POST(sx, sy, cxin, cyin, cxout, cyout, aff)                      \
  aff_mult(aff, aff_shift(cxout, cyout,                                        \
                          aff_scale(sx, sy, aff_shift(-cxin, -cyin, Aff_I))))

#endif
