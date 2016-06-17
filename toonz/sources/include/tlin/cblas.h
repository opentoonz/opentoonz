#pragma once

/*
  NOTE: This is a reduced version of cblas.h without some complex-type routines.
        It was necessary to avoid C99 complex types (used by GOTOBLAS
  implementation,
        for instance) issues. Complex numbers are currently NOT supported in
  tlin
        by the way...
*/

#ifndef CBLAS_H
#define CBLAS_H

#define CBLAS_INDEX size_t

enum CBLAS_ORDER { CblasRowMajor = 101, CblasColMajor = 102 };
enum CBLAS_TRANSPOSE {
  CblasNoTrans     = 111,
  CblasTrans       = 112,
  CblasConjTrans   = 113,
  CblasConjNoTrans = 114
};
enum CBLAS_UPLO { CblasUpper = 121, CblasLower = 122 };
enum CBLAS_DIAG { CblasNonUnit = 131, CblasUnit = 132 };
enum CBLAS_SIDE { CblasLeft = 141, CblasRight = 142 };

float cblas_sdsdot(blasint n, float, float *x, blasint incx, float *y,
                   blasint incy);
double cblas_dsdot(blasint n, float *x, blasint incx, float *y, blasint incy);
float cblas_sdot(blasint n, float *x, blasint incx, float *y, blasint incy);
double cblas_ddot(blasint n, double *x, blasint incx, double *y, blasint incy);

float cblas_sasum(blasint n, float *x, blasint incx);
double cblas_dasum(blasint n, double *x, blasint incx);
float cblas_scasum(blasint n, float *x, blasint incx);
double cblas_dzasum(blasint n, double *x, blasint incx);

float cblas_snrm2(blasint N, float *X, blasint incX);
double cblas_dnrm2(blasint N, double *X, blasint incX);
float cblas_scnrm2(blasint N, float *X, blasint incX);
double cblas_dznrm2(blasint N, double *X, blasint incX);

CBLAS_INDEX cblas_isamax(blasint n, float *x, blasint incx);
CBLAS_INDEX cblas_idamax(blasint n, double *x, blasint incx);
CBLAS_INDEX cblas_icamax(blasint n, float *x, blasint incx);
CBLAS_INDEX cblas_izamax(blasint n, double *x, blasint incx);

void cblas_saxpy(blasint n, float, float *x, blasint incx, float *y,
                 blasint incy);
void cblas_daxpy(blasint n, double, double *x, blasint incx, double *y,
                 blasint incy);
void cblas_caxpy(blasint n, float *, float *x, blasint incx, float *y,
                 blasint incy);
void cblas_zaxpy(blasint n, double *, double *x, blasint incx, double *y,
                 blasint incy);

void cblas_scopy(blasint n, float *x, blasint incx, float *y, blasint incy);
void cblas_dcopy(blasint n, double *x, blasint incx, double *y, blasint incy);
void cblas_ccopy(blasint n, float *x, blasint incx, float *y, blasint incy);
void cblas_zcopy(blasint n, double *x, blasint incx, double *y, blasint incy);

void cblas_sswap(blasint n, float *x, blasint incx, float *y, blasint incy);
void cblas_dswap(blasint n, double *x, blasint incx, double *y, blasint incy);
void cblas_cswap(blasint n, float *x, blasint incx, float *y, blasint incy);
void cblas_zswap(blasint n, double *x, blasint incx, double *y, blasint incy);

void cblas_srot(blasint N, float *X, blasint incX, float *Y, blasint incY,
                float c, float s);
void cblas_drot(blasint N, double *X, blasint incX, double *Y, blasint incY,
                double c, double s);

void cblas_srotg(float *a, float *b, float *c, float *s);
void cblas_drotg(double *a, double *b, double *c, double *s);

void cblas_srotm(blasint N, float *X, blasint incX, float *Y, blasint incY,
                 float *P);
void cblas_drotm(blasint N, double *X, blasint incX, double *Y, blasint incY,
                 double *P);

void cblas_srotmg(float *d1, float *d2, float *b1, float b2, float *P);
void cblas_drotmg(double *d1, double *d2, double *b1, double b2, double *P);

void cblas_sscal(blasint N, float alpha, float *X, blasint incX);
void cblas_dscal(blasint N, double alpha, double *X, blasint incX);
void cblas_cscal(blasint N, float *alpha, float *X, blasint incX);
void cblas_zscal(blasint N, double *alpha, double *X, blasint incX);
void cblas_csscal(blasint N, float alpha, float *X, blasint incX);
void cblas_zdscal(blasint N, double alpha, double *X, blasint incX);

void cblas_sgemv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE trans, blasint m,
                 blasint n, float alpha, float *a, blasint lda, float *x,
                 blasint incx, float beta, float *y, blasint incy);
void cblas_dgemv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE trans, blasint m,
                 blasint n, double alpha, double *a, blasint lda, double *x,
                 blasint incx, double beta, double *y, blasint incy);
void cblas_cgemv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE trans, blasint m,
                 blasint n, float *alpha, float *a, blasint lda, float *x,
                 blasint incx, float *beta, float *y, blasint incy);
void cblas_zgemv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE trans, blasint m,
                 blasint n, double *alpha, double *a, blasint lda, double *x,
                 blasint incx, double *beta, double *y, blasint incy);

void cblas_sger(enum CBLAS_ORDER order, blasint M, blasint N, float alpha,
                float *X, blasint incX, float *Y, blasint incY, float *A,
                blasint lda);
void cblas_dger(enum CBLAS_ORDER order, blasint M, blasint N, double alpha,
                double *X, blasint incX, double *Y, blasint incY, double *A,
                blasint lda);
void cblas_cgeru(enum CBLAS_ORDER order, blasint M, blasint N, float *alpha,
                 float *X, blasint incX, float *Y, blasint incY, float *A,
                 blasint lda);
void cblas_cgerc(enum CBLAS_ORDER order, blasint M, blasint N, float *alpha,
                 float *X, blasint incX, float *Y, blasint incY, float *A,
                 blasint lda);
void cblas_zgeru(enum CBLAS_ORDER order, blasint M, blasint N, double *alpha,
                 double *X, blasint incX, double *Y, blasint incY, double *A,
                 blasint lda);
void cblas_zgerc(enum CBLAS_ORDER order, blasint M, blasint N, double *alpha,
                 double *X, blasint incX, double *Y, blasint incY, double *A,
                 blasint lda);

void cblas_strsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *A, blasint lda, float *X, blasint incX);
void cblas_dtrsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *A, blasint lda, double *X, blasint incX);
void cblas_ctrsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *A, blasint lda, float *X, blasint incX);
void cblas_ztrsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *A, blasint lda, double *X, blasint incX);

void cblas_strmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *A, blasint lda, float *X, blasint incX);
void cblas_dtrmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *A, blasint lda, double *X, blasint incX);
void cblas_ctrmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *A, blasint lda, float *X, blasint incX);
void cblas_ztrmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *A, blasint lda, double *X, blasint incX);

void cblas_ssyr(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                float alpha, float *X, blasint incX, float *A, blasint lda);
void cblas_dsyr(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                double alpha, double *X, blasint incX, double *A, blasint lda);
void cblas_cher(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                float alpha, float *X, blasint incX, float *A, blasint lda);
void cblas_zher(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                double alpha, double *X, blasint incX, double *A, blasint lda);

void cblas_ssyr2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float alpha, float *X, blasint incX, float *Y, blasint incY,
                 float *A, blasint lda);
void cblas_dsyr2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double alpha, double *X, blasint incX, double *Y, blasint incY,
                 double *A, blasint lda);
void cblas_cher2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float *alpha, float *X, blasint incX, float *Y, blasint incY,
                 float *A, blasint lda);
void cblas_zher2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double *alpha, double *X, blasint incX, double *Y,
                 blasint incY, double *A, blasint lda);

void cblas_sgbmv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE TransA, blasint M,
                 blasint N, blasint KL, blasint KU, float alpha, float *A,
                 blasint lda, float *X, blasint incX, float beta, float *Y,
                 blasint incY);
void cblas_dgbmv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE TransA, blasint M,
                 blasint N, blasint KL, blasint KU, double alpha, double *A,
                 blasint lda, double *X, blasint incX, double beta, double *Y,
                 blasint incY);
void cblas_cgbmv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE TransA, blasint M,
                 blasint N, blasint KL, blasint KU, float *alpha, float *A,
                 blasint lda, float *X, blasint incX, float *beta, float *Y,
                 blasint incY);
void cblas_zgbmv(enum CBLAS_ORDER order, enum CBLAS_TRANSPOSE TransA, blasint M,
                 blasint N, blasint KL, blasint KU, double *alpha, double *A,
                 blasint lda, double *X, blasint incX, double *beta, double *Y,
                 blasint incY);

void cblas_ssbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 blasint K, float alpha, float *A, blasint lda, float *X,
                 blasint incX, float beta, float *Y, blasint incY);
void cblas_dsbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 blasint K, double alpha, double *A, blasint lda, double *X,
                 blasint incX, double beta, double *Y, blasint incY);

void cblas_stbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, float *A, blasint lda, float *X, blasint incX);
void cblas_dtbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, double *A, blasint lda, double *X, blasint incX);
void cblas_ctbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, float *A, blasint lda, float *X, blasint incX);
void cblas_ztbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, double *A, blasint lda, double *X, blasint incX);

void cblas_stbsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, float *A, blasint lda, float *X, blasint incX);
void cblas_dtbsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, double *A, blasint lda, double *X, blasint incX);
void cblas_ctbsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, float *A, blasint lda, float *X, blasint incX);
void cblas_ztbsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 blasint K, double *A, blasint lda, double *X, blasint incX);

void cblas_stpmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *Ap, float *X, blasint incX);
void cblas_dtpmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *Ap, double *X, blasint incX);
void cblas_ctpmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *Ap, float *X, blasint incX);
void cblas_ztpmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *Ap, double *X, blasint incX);

void cblas_stpsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *Ap, float *X, blasint incX);
void cblas_dtpsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *Ap, double *X, blasint incX);
void cblas_ctpsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 float *Ap, float *X, blasint incX);
void cblas_ztpsv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE TransA, enum CBLAS_DIAG Diag, blasint N,
                 double *Ap, double *X, blasint incX);

void cblas_ssymv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float alpha, float *A, blasint lda, float *X, blasint incX,
                 float beta, float *Y, blasint incY);
void cblas_dsymv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double alpha, double *A, blasint lda, double *X, blasint incX,
                 double beta, double *Y, blasint incY);
void cblas_chemv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float *alpha, float *A, blasint lda, float *X, blasint incX,
                 float *beta, float *Y, blasint incY);
void cblas_zhemv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double *alpha, double *A, blasint lda, double *X, blasint incX,
                 double *beta, double *Y, blasint incY);

void cblas_sspmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float alpha, float *Ap, float *X, blasint incX, float beta,
                 float *Y, blasint incY);
void cblas_dspmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double alpha, double *Ap, double *X, blasint incX, double beta,
                 double *Y, blasint incY);

void cblas_sspr(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                float alpha, float *X, blasint incX, float *Ap);
void cblas_dspr(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                double alpha, double *X, blasint incX, double *Ap);

void cblas_chpr(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                float alpha, float *X, blasint incX, float *A);
void cblas_zhpr(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                double alpha, double *X, blasint incX, double *A);

void cblas_sspr2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float alpha, float *X, blasint incX, float *Y, blasint incY,
                 float *A);
void cblas_dspr2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double alpha, double *X, blasint incX, double *Y, blasint incY,
                 double *A);
void cblas_chpr2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float *alpha, float *X, blasint incX, float *Y, blasint incY,
                 float *Ap);
void cblas_zhpr2(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double *alpha, double *X, blasint incX, double *Y,
                 blasint incY, double *Ap);

void cblas_chbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 blasint K, float *alpha, float *A, blasint lda, float *X,
                 blasint incX, float *beta, float *Y, blasint incY);
void cblas_zhbmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 blasint K, double *alpha, double *A, blasint lda, double *X,
                 blasint incX, double *beta, double *Y, blasint incY);

void cblas_chpmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 float *alpha, float *Ap, float *X, blasint incX, float *beta,
                 float *Y, blasint incY);
void cblas_zhpmv(enum CBLAS_ORDER order, enum CBLAS_UPLO Uplo, blasint N,
                 double *alpha, double *Ap, double *X, blasint incX,
                 double *beta, double *Y, blasint incY);

void cblas_sgemm(enum CBLAS_ORDER Order, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_TRANSPOSE TransB, blasint M, blasint N, blasint K,
                 float alpha, float *A, blasint lda, float *B, blasint ldb,
                 float beta, float *C, blasint ldc);
void cblas_dgemm(enum CBLAS_ORDER Order, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_TRANSPOSE TransB, blasint M, blasint N, blasint K,
                 double alpha, double *A, blasint lda, double *B, blasint ldb,
                 double beta, double *C, blasint ldc);
void cblas_cgemm(enum CBLAS_ORDER Order, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_TRANSPOSE TransB, blasint M, blasint N, blasint K,
                 float *alpha, float *A, blasint lda, float *B, blasint ldb,
                 float *beta, float *C, blasint ldc);
void cblas_zgemm(enum CBLAS_ORDER Order, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_TRANSPOSE TransB, blasint M, blasint N, blasint K,
                 double *alpha, double *A, blasint lda, double *B, blasint ldb,
                 double *beta, double *C, blasint ldc);

void cblas_ssymm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, blasint M, blasint N, float alpha,
                 float *A, blasint lda, float *B, blasint ldb, float beta,
                 float *C, blasint ldc);
void cblas_dsymm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, blasint M, blasint N, double alpha,
                 double *A, blasint lda, double *B, blasint ldb, double beta,
                 double *C, blasint ldc);
void cblas_csymm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, blasint M, blasint N, float *alpha,
                 float *A, blasint lda, float *B, blasint ldb, float *beta,
                 float *C, blasint ldc);
void cblas_zsymm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, blasint M, blasint N, double *alpha,
                 double *A, blasint lda, double *B, blasint ldb, double *beta,
                 double *C, blasint ldc);

void cblas_ssyrk(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE Trans, blasint N, blasint K, float alpha,
                 float *A, blasint lda, float beta, float *C, blasint ldc);
void cblas_dsyrk(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE Trans, blasint N, blasint K, double alpha,
                 double *A, blasint lda, double beta, double *C, blasint ldc);
void cblas_csyrk(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE Trans, blasint N, blasint K, float *alpha,
                 float *A, blasint lda, float *beta, float *C, blasint ldc);
void cblas_zsyrk(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE Trans, blasint N, blasint K,
                 double *alpha, double *A, blasint lda, double *beta, double *C,
                 blasint ldc);

void cblas_ssyr2k(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                  enum CBLAS_TRANSPOSE Trans, blasint N, blasint K, float alpha,
                  float *A, blasint lda, float *B, blasint ldb, float beta,
                  float *C, blasint ldc);
void cblas_dsyr2k(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                  enum CBLAS_TRANSPOSE Trans, blasint N, blasint K,
                  double alpha, double *A, blasint lda, double *B, blasint ldb,
                  double beta, double *C, blasint ldc);
void cblas_csyr2k(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                  enum CBLAS_TRANSPOSE Trans, blasint N, blasint K,
                  float *alpha, float *A, blasint lda, float *B, blasint ldb,
                  float *beta, float *C, blasint ldc);
void cblas_zsyr2k(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                  enum CBLAS_TRANSPOSE Trans, blasint N, blasint K,
                  double *alpha, double *A, blasint lda, double *B, blasint ldb,
                  double *beta, double *C, blasint ldc);

void cblas_strmm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, float alpha,
                 float *A, blasint lda, float *B, blasint ldb);
void cblas_dtrmm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, double alpha,
                 double *A, blasint lda, double *B, blasint ldb);
void cblas_ctrmm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, float *alpha,
                 float *A, blasint lda, float *B, blasint ldb);
void cblas_ztrmm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, double *alpha,
                 double *A, blasint lda, double *B, blasint ldb);

void cblas_strsm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, float alpha,
                 float *A, blasint lda, float *B, blasint ldb);
void cblas_dtrsm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, double alpha,
                 double *A, blasint lda, double *B, blasint ldb);
void cblas_ctrsm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, float *alpha,
                 float *A, blasint lda, float *B, blasint ldb);
void cblas_ztrsm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, enum CBLAS_TRANSPOSE TransA,
                 enum CBLAS_DIAG Diag, blasint M, blasint N, double *alpha,
                 double *A, blasint lda, double *B, blasint ldb);

void cblas_chemm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, blasint M, blasint N, float *alpha,
                 float *A, blasint lda, float *B, blasint ldb, float *beta,
                 float *C, blasint ldc);
void cblas_zhemm(enum CBLAS_ORDER Order, enum CBLAS_SIDE Side,
                 enum CBLAS_UPLO Uplo, blasint M, blasint N, double *alpha,
                 double *A, blasint lda, double *B, blasint ldb, double *beta,
                 double *C, blasint ldc);

void cblas_cherk(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE Trans, blasint N, blasint K, float alpha,
                 float *A, blasint lda, float beta, float *C, blasint ldc);
void cblas_zherk(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                 enum CBLAS_TRANSPOSE Trans, blasint N, blasint K, double alpha,
                 double *A, blasint lda, double beta, double *C, blasint ldc);

void cblas_cher2k(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                  enum CBLAS_TRANSPOSE Trans, blasint N, blasint K,
                  float *alpha, float *A, blasint lda, float *B, blasint ldb,
                  float beta, float *C, blasint ldc);
void cblas_zher2k(enum CBLAS_ORDER Order, enum CBLAS_UPLO Uplo,
                  enum CBLAS_TRANSPOSE Trans, blasint N, blasint K,
                  double *alpha, double *A, blasint lda, double *B, blasint ldb,
                  double beta, double *C, blasint ldc);

void cblas_xerbla(blasint p, char *rout, char *form, ...);
#endif
