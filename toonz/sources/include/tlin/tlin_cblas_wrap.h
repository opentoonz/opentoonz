#pragma once

#ifndef TLIN_CBLAS_WRAP
#define TLIN_CBLAS_WRAP

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

#include "tlin_matrix.h"

/*
  EXPLANATION:

    The tlin_cblas_wrap files implement a simplified wrapper interface to CBLAS
  routines.
*/

//================================================================================

namespace tlin {

//***************************************************************************
//    CBLAS-related routines
//***************************************************************************

void DVAPI sum(int n, const double *x, double *&y);

void DVAPI multiply(int rows, int cols, const double *A, const double *x,
                    double *&y);
void DVAPI multiply(const mat &A, const double *x, double *&y);

}  // namespace tlin

#endif  // TLIN_CBLAS_WRAP
