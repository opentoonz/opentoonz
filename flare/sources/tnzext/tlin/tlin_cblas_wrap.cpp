

#include <stddef.h>
#include <stdlib.h>
#include <memory>
#include <cstring>

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
// dummy definition for linker
#include <conio.h>
extern "C" {
void __imp__cprintf(char const *const _Format) { _cprintf(_Format); }
}
#endif

// blasint may either be common 4 bytes or extended 8 (long)...
// Replace this and REBUILD the CBLAS with extended int if needed.
typedef int blasint;

extern "C" {
#include "tlin/cblas.h"
}

#include "tlin/tlin_cblas_wrap.h"

//========================================================================================

/*
  LITTLE RESUME OF BLAS NOMENCLATURE

  Function names behave like:

    cblas_[Precision][Matrix type][Operation specifier]

  Precision can be:

    S - REAL                C - COMPLEX
    D - DOUBLE PRECISION    Z - COMPLEX*16  (may not be supported by all
  machines)

    In current wrapper implementation only D routines will be adopted.

  Matrix type can be:

    GE - GEneral     GB - General Band
    SY - SYmmetric   SB - Sym. Band     SP - Sym. Packed
    HE - HErmitian   HB - Herm. Band    HP - Herm. Packed
    TR - TRiangular  TB - Triang. Band  TP - Triang. Packed

    In current wrapper implementation only GE routines will be adopted.

  Operation specifier depends on the operation. For example, mm means matrix *
  matrix, mv matrix * vector, etc..
*/

//========================================================================================

static void sum(int n, const double *x, double *&y) {
  /*
void cblas_daxpy(blasint n, double a, double *x, blasint incx, double *y,
blasint incy);

NOTE:

Returns a * x + y.    Output is stored in y.

incx and incy are the increments in array access - ie x[incx * i] and y[incy *
j] values only are
considered (=> we'll use 1).
*/
  double *_x = const_cast<double *>(x);

  cblas_daxpy(n, 1.0, _x, 1, y, 1);
}

//---------------------------------------------------------------------------------------------------

void tlin::multiply(int rows, int cols, const double *A, const double *x,
                    double *&y) {
  /*
void cblas_dgemv(enum CBLAS_ORDER order,  enum CBLAS_TRANSPOSE trans,  blasint
m, blasint n,
           double alpha, double  *a, blasint lda,  double  *x, blasint incx,
double beta,  double  *y, blasint incy);

NOTE:

Returns alpha*A*x + beta*y.    Output is stored in y.
*/

  if (!y) {
    y = (double *)malloc(rows * sizeof(double));
    memset(y, 0, rows * sizeof(double));
  }

  double *_A = const_cast<double *>(A);
  double *_x = const_cast<double *>(x);

  cblas_dgemv(CblasColMajor, CblasNoTrans, rows, cols, 1.0, _A, rows, _x, 1,
              1.0, y, 1);
}

//---------------------------------------------------------------------------------------------------

void tlin::multiply(const mat &A, const double *x, double *&y) {
  multiply(A.rows(), A.cols(), A.values(), x, y);
}
