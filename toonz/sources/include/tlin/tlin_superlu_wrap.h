#pragma once

#ifndef TLIN_SUPERLU_WRAP
#define TLIN_SUPERLU_WRAP

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
#include "tlin_sparsemat.h"

/*
  EXPLANATION:

  The tlin_superlu_wrap files implement wrappers to the SuperLU library.
  They are used to provide utility interfaces to SuperLU from tlin, and
  overcome name collisions (in particular, SuperLU is pure C - no
  namespaces, and redefines DOUBLES and such already defined
  from <windows.h>...)
*/

//================================================================================

namespace tlin {

//*****************************************************************************
//    Forward declarations
//*****************************************************************************

/*
  The following structs are heirs of the actual SuperLU counterparts - this is
  necessary due to the "typedef struct {..} _Name" syntax that prevents forward
  declaration of "_Name" (different base types).
  No member is added to the original type - so they are accessible exactly the
  same way.
*/

struct SuperMatrix;
struct superlu_options_t;

struct SuperFactors {
  SuperMatrix *L;
  SuperMatrix *U;
  int *perm_c;
  int *perm_r;
};

//*****************************************************************************
//    SuperLU-specific routines
//*****************************************************************************

void DVAPI allocS(SuperMatrix *&A, int rows, int cols,
                  int nnz);  //!< Allocates A in NC (sparse) format
void DVAPI allocD(SuperMatrix *&A, int rows,
                  int cols);  //!< Allocates A in DN (dense) format

//! Allocates A with externally supplied initializer values
void DVAPI allocS(SuperMatrix *&A, int rows, int cols, int nnz, int *colptr,
                  int *rowind, double *values);
void DVAPI allocD(SuperMatrix *&A, int rows, int cols, int lda, double *values);

void DVAPI freeS(SuperMatrix *A);   //!< Frees A allocated with allocS
void DVAPI freeD(SuperMatrix *A);   //!< Frees A allocated with allocD
void DVAPI freeF(SuperFactors *F);  //!< Frees F returned by factorize

//! Initializes a local SuperMatrix (ie created on stack).
void DVAPI createS(SuperMatrix &A, int rows, int cols, int nnz);
void DVAPI createD(SuperMatrix &A, int rows, int cols);

//! Initializes a local SuperMatrix with externally supplied data.
void DVAPI createS(SuperMatrix &A, int rows, int cols, int nnz, int *colptr,
                   int *rowind, double *values);
void DVAPI createD(SuperMatrix &A, int rows, int cols, int lda, double *values);

//! Destroys A. To be used when A is local (ie with create). Can be told to
//! spare data deallocation.
void DVAPI destroyS(SuperMatrix &A, bool destroyData = true);
void DVAPI destroyD(SuperMatrix &A, bool destroyData = true);

void DVAPI readDN(SuperMatrix *A, int &lda,
                  double *&values);  //!< Reads values ptr from A
void DVAPI readNC(SuperMatrix *A, int &nnz, int *&colptr, int *&rowind,
                  double *&values);  //!< Reads array ptrs from A

//! Copies m's content to A. A could be either 0 or an already initialized
//! SuperMatrix.
void DVAPI traduceS(tlin::sparse_matrix<double> &m, SuperMatrix *&A);
void DVAPI traduceD(const tlin::matrix<double> &m, SuperMatrix *&A);
void DVAPI traduceD(const tlin::sparse_matrix<double> &m, SuperMatrix *&A);

/*!
  Returns A's factorization F, or 0 if the factorization failed (due to A's
  singularity,
  or memory shortage - see SuperLU docs for details).
*/
void DVAPI factorize(SuperMatrix *A, SuperFactors *&F,
                     superlu_options_t *opt = 0);

void DVAPI solve(SuperFactors *F, SuperMatrix *BX, superlu_options_t *opt = 0);
void DVAPI solve(SuperFactors *F, SuperMatrix *B, SuperMatrix *&X,
                 superlu_options_t *opt = 0);
void DVAPI solve(SuperMatrix *A, SuperMatrix *BX, superlu_options_t *opt = 0);
void DVAPI solve(SuperMatrix *A, SuperMatrix *B, SuperMatrix *&X,
                 superlu_options_t *opt = 0);

void DVAPI solve(SuperFactors *F, double *bx, superlu_options_t *opt = 0);
void DVAPI solve(SuperFactors *F, double *b, double *&x,
                 superlu_options_t *opt = 0);
void DVAPI solve(SuperMatrix *A, double *bx, superlu_options_t *opt = 0);
void DVAPI solve(SuperMatrix *A, double *b, double *&x,
                 superlu_options_t *opt = 0);

//*****************************************************************************
//    BLAS-related routines
//*****************************************************************************

void DVAPI multiplyS(const SuperMatrix *A, const double *v, double *&Av);
void DVAPI multiplyD(const SuperMatrix *A, const double *v, double *&Av);

}  // namespace tlin

#endif  // TLIN_SUPERLU_WRAP
