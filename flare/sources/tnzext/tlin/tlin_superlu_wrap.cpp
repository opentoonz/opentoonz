

extern "C" {
#include <slu_ddefs.h>
#include <slu_util.h>
}

#include <algorithm>

#include "tlin/tlin_cblas_wrap.h"

#include "tlin/tlin_superlu_wrap.h"

//*************************************************************************
//    Preliminaries
//*************************************************************************

struct tlin::SuperMatrix final : public ::SuperMatrix {};
struct tlin::superlu_options_t final : public ::superlu_options_t {};

//=======================================================================

namespace {
static const tlin::spmat::HashMap::size_t neg = -1;

bool initialized = false;
static tlin::superlu_options_t defaultOpt;

struct DefaultOptsInitializer {
  DefaultOptsInitializer() {
    set_default_options(&defaultOpt);
    defaultOpt.PrintStat = NO;
  }
} _instance;

inline bool rowLess(const tlin::spmat::HashMap::BucketNode *a,
                    const tlin::spmat::HashMap::BucketNode *b) {
  return a->m_key.first < b->m_key.first;
}
}

//*************************************************************************
//    SuperLU-specific Functions
//*************************************************************************

void tlin::allocS(SuperMatrix *&A, int rows, int cols, int nnz) {
  A              = (SuperMatrix *)SUPERLU_MALLOC(sizeof(SuperMatrix));
  double *values = doubleMalloc(nnz);
  int *rowind    = intMalloc(nnz);
  int *colptr    = intMalloc(cols + 1);

  dCreate_CompCol_Matrix(A, rows, cols, nnz, values, rowind, colptr, SLU_NC,
                         SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::allocD(SuperMatrix *&A, int rows, int cols) {
  A = (SuperMatrix *)SUPERLU_MALLOC(sizeof(SuperMatrix));

  double *values = doubleMalloc(rows * cols * sizeof(double));
  dCreate_Dense_Matrix(A, rows, cols, values, rows, SLU_DN, SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::allocS(SuperMatrix *&A, int rows, int cols, int nnz, int *colptr,
                  int *rowind, double *values) {
  A = (SuperMatrix *)SUPERLU_MALLOC(sizeof(SuperMatrix));
  dCreate_CompCol_Matrix(A, rows, cols, nnz, values, rowind, colptr, SLU_NC,
                         SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::allocD(SuperMatrix *&A, int rows, int cols, int lda,
                  double *values) {
  A = (SuperMatrix *)SUPERLU_MALLOC(sizeof(SuperMatrix));
  dCreate_Dense_Matrix(A, rows, cols, values, lda, SLU_DN, SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::freeS(SuperMatrix *m) {
  if (!m) return;
  Destroy_CompCol_Matrix(m);
  SUPERLU_FREE(m);
}

//---------------------------------------------------------------

void tlin::freeD(SuperMatrix *m) {
  if (!m) return;
  Destroy_Dense_Matrix(m);
  SUPERLU_FREE(m);
}

//---------------------------------------------------------------

void tlin::createS(SuperMatrix &A, int rows, int cols, int nnz) {
  double *values = doubleMalloc(nnz);
  int *rowind    = intMalloc(nnz);
  int *colptr    = intMalloc(cols + 1);

  dCreate_CompCol_Matrix(&A, rows, cols, nnz, values, rowind, colptr, SLU_NC,
                         SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::createD(SuperMatrix &A, int rows, int cols) {
  double *values = doubleMalloc(rows * cols * sizeof(double));
  dCreate_Dense_Matrix(&A, rows, cols, values, rows, SLU_DN, SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::createS(SuperMatrix &A, int rows, int cols, int nnz, int *colptr,
                   int *rowind, double *values) {
  dCreate_CompCol_Matrix(&A, rows, cols, nnz, values, rowind, colptr, SLU_NC,
                         SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::createD(SuperMatrix &A, int rows, int cols, int lda,
                   double *values) {
  dCreate_Dense_Matrix(&A, rows, cols, values, lda, SLU_DN, SLU_D, SLU_GE);
}

//---------------------------------------------------------------

void tlin::destroyS(SuperMatrix &A, bool destroyData) {
  if (destroyData)
    Destroy_CompCol_Matrix(&A);
  else
    SUPERLU_FREE(A.Store);
}

//---------------------------------------------------------------

void tlin::destroyD(SuperMatrix &A, bool destroyData) {
  if (destroyData)
    Destroy_Dense_Matrix(&A);
  else
    SUPERLU_FREE(A.Store);
}

//---------------------------------------------------------------

void tlin::freeF(SuperFactors *F) {
  if (!F) return;
  Destroy_SuperNode_Matrix(F->L);
  Destroy_CompCol_Matrix(F->U);
  SUPERLU_FREE(F->L);
  SUPERLU_FREE(F->U);
  SUPERLU_FREE(F->perm_r);
  SUPERLU_FREE(F->perm_c);
  SUPERLU_FREE(F);
}

//---------------------------------------------------------------

void tlin::readDN(SuperMatrix *A, int &lda, double *&values) {
  assert(A->Stype == SLU_DN);
  DNformat *storage = (DNformat *)A->Store;

  lda    = storage->lda;
  values = (double *)storage->nzval;
}

//---------------------------------------------------------------

void tlin::readNC(SuperMatrix *A, int &nnz, int *&colptr, int *&rowind,
                  double *&values) {
  assert(A->Stype == SLU_NC);  // Only SLU_NC (CCS) format is supported here
  NCformat *storage = (NCformat *)A->Store;

  nnz    = storage->nnz;
  values = (double *)storage->nzval;
  rowind = (int *)storage->rowind;
  colptr = (int *)storage->colptr;
}

//---------------------------------------------------------------

void tlin::traduceD(const mat &m, SuperMatrix *&A) {
  int rows = m.rows(), cols = m.cols();

  if (!A) allocD(A, rows, cols);

  int lda;
  double *Avalues = 0;
  readDN(A, lda, Avalues);

  assert(A->nrow == rows && A->ncol == cols && lda == rows);

  memcpy(Avalues, m.values(), rows * cols * sizeof(double));
}

//---------------------------------------------------------------

void tlin::traduceS(spmat &m, SuperMatrix *&A) {
  int rows = m.rows(), cols = m.cols(), nnz = (int)m.entries().size();
  spmat::HashMap &entries = m.entries();

  // Build or extract pointers to out's data
  double *values;
  int *rowind, *colptr;

  if (!A) allocS(A, rows, cols, nnz);

  // Retrieve NC arrays from A
  int Annz;
  readNC(A, Annz, colptr, rowind, values);
  assert(A->nrow == rows && A->ncol == cols && Annz == nnz);

  // Rehash to cols buckets
  if (entries.hashFunctor().m_cols != cols) entries.hashFunctor().m_cols = cols;
  entries.rehash(cols);

  // Copy each bucket to the corresponding col
  const std::vector<spmat::HashMap::size_t> &buckets = m.entries().buckets();
  const tcg::list<spmat::HashMap::BucketNode> &nodes = m.entries().items();

  std::vector<const spmat::HashMap::BucketNode *> colEntries;
  std::vector<const spmat::HashMap::BucketNode *>::size_type j, size;

  double *currVal = values;
  int *currRowind = rowind;

  for (int i = 0; i < cols; ++i) {
    colptr[i]                      = (int)(currVal - values);
    spmat::HashMap::size_t nodeIdx = buckets[i];

    // Retrieve all column entry pointers.
    colEntries.clear();
    while (nodeIdx != neg) {
      const spmat::HashMap::BucketNode &node = nodes[nodeIdx];
      colEntries.push_back(&node);
      nodeIdx = nodes[nodeIdx].m_next;
    }

    // Sort them by row
    std::sort(colEntries.begin(), colEntries.end(), rowLess);

    // Finally, write them in the SuperMatrix
    size = colEntries.size();
    for (j = 0; j < size; ++j) {
      const spmat::HashMap::BucketNode &node = *colEntries[j];

      *currRowind = node.m_key.first;
      *currVal    = node.m_val;

      ++currVal, ++currRowind;
    }
  }

  colptr[cols] = nnz;
}

//---------------------------------------------------------------

void tlin::traduceD(const tlin::sparse_matrix<double> &m, SuperMatrix *&A) {
  int rows = m.rows(), cols = m.cols();
  const spmat::HashMap &entries = m.entries();

  // Build or extract pointers to out's data
  double *values;

  if (!A) allocD(A, rows, cols);

  // Retrieve DN arrays from A
  int lda;
  readDN(A, lda, values);
  assert(A->nrow == rows && A->ncol == cols && lda == rows);

  // Copy each value in entries to A
  spmat::HashMap::const_iterator it;
  for (it = entries.begin(); it != entries.end(); ++it)
    values[it->m_key.second * rows + it->m_key.first] = it->m_val;
}

//---------------------------------------------------------------

void tlin::factorize(SuperMatrix *A, SuperFactors *&F, superlu_options_t *opt) {
  assert(A->nrow == A->ncol);
  int n = A->nrow;

  if (!F) F = (SuperFactors *)SUPERLU_MALLOC(sizeof(SuperFactors));

  if (!opt) opt = &defaultOpt;

  F->perm_c = intMalloc(n);

  get_perm_c(3, A, F->perm_c);

  SuperMatrix AC;
  int *etree = intMalloc(n);

  sp_preorder(opt, A, F->perm_c, etree, &AC);

  F->L      = (SuperMatrix *)SUPERLU_MALLOC(sizeof(SuperMatrix));
  F->U      = (SuperMatrix *)SUPERLU_MALLOC(sizeof(SuperMatrix));
  F->perm_r = intMalloc(n);

  SuperLUStat_t stat;
  StatInit(&stat);

  int result;

#if defined(SUPERLU_MAJOR_VERSION) && (SUPERLU_MAJOR_VERSION >= 5)
  GlobalLU_t Glu;
  dgstrf(opt, &AC, sp_ienv(1), sp_ienv(2), etree, NULL, 0, F->perm_c, F->perm_r,
         F->L, F->U, &Glu, &stat, &result);
#else
  dgstrf(opt, &AC, sp_ienv(1), sp_ienv(2), etree, NULL, 0, F->perm_c, F->perm_r,
         F->L, F->U, &stat, &result);
#endif

  StatFree(&stat);

  Destroy_CompCol_Permuted(&AC);
  SUPERLU_FREE(etree);

  if (result != 0) freeF(F), F = 0;
}

//---------------------------------------------------------------

void tlin::solve(SuperFactors *F, SuperMatrix *BX, superlu_options_t *opt) {
  assert(F);

  if (!opt) opt = &defaultOpt;

  SuperLUStat_t stat;
  StatInit(&stat);

  int result;
  dgstrs(NOTRANS, F->L, F->U, F->perm_c, F->perm_r, BX, &stat, &result);

  StatFree(&stat);
}

//---------------------------------------------------------------

void tlin::solve(SuperFactors *F, SuperMatrix *B, SuperMatrix *&X,
                 superlu_options_t *opt) {
  if (!X) allocD(X, B->nrow, B->ncol);

  double *Bvalues = 0, *Xvalues = 0;
  int lda;
  readDN(B, lda, Bvalues);
  readDN(X, lda, Xvalues);
  memcpy(Xvalues, Bvalues, B->nrow * B->ncol * sizeof(double));

  solve(F, X, opt);
}

//---------------------------------------------------------------

void tlin::solve(SuperMatrix *A, SuperMatrix *BX, superlu_options_t *opt) {
  assert(A->nrow == A->ncol);
  int n = A->nrow;

  if (!opt) opt = &defaultOpt;

  SuperMatrix L, U;
  int *perm_c, *perm_r;

  perm_c = intMalloc(n);
  perm_r = intMalloc(n);

  SuperLUStat_t stat;
  StatInit(&stat);

  int result;
  dgssv(opt, A, perm_c, perm_r, &L, &U, BX, &stat, &result);

  Destroy_SuperNode_Matrix(&L);
  Destroy_CompCol_Matrix(&U);
  SUPERLU_FREE(perm_r);
  SUPERLU_FREE(perm_c);
  StatFree(&stat);
}

//---------------------------------------------------------------

void tlin::solve(SuperMatrix *A, SuperMatrix *B, SuperMatrix *&X,
                 superlu_options_t *opt) {
  if (!X) allocD(X, B->nrow, B->ncol);

  double *Bvalues = 0, *Xvalues = 0;
  int lda;
  readDN(B, lda, Bvalues);
  readDN(X, lda, Xvalues);
  memcpy(Xvalues, Bvalues, B->nrow * B->ncol * sizeof(double));

  solve(A, X, opt);
}

//---------------------------------------------------------------

void tlin::solve(SuperFactors *F, double *bx, superlu_options_t *opt) {
  SuperMatrix BX;
  int rows = F->L->nrow;

  createD(BX, rows, 1, rows, bx);
  tlin::solve(F, &BX, opt);
  SUPERLU_FREE(BX.Store);
}

//---------------------------------------------------------------

void tlin::solve(SuperFactors *F, double *b, double *&x,
                 superlu_options_t *opt) {
  SuperMatrix B, X;
  int rows = F->L->nrow;

  if (!x) x = (double *)malloc(rows * sizeof(double));

  createD(B, rows, 1, rows, b);
  createD(X, rows, 1, rows, x);

  SuperMatrix *Xptr = &X;  //&X is const
  tlin::solve(F, &B, Xptr, opt);

  SUPERLU_FREE(B.Store);
  SUPERLU_FREE(X.Store);
}

//---------------------------------------------------------------

void tlin::solve(SuperMatrix *A, double *bx, superlu_options_t *opt) {
  SuperMatrix BX;
  int rows = A->nrow;

  createD(BX, rows, 1, rows, bx);
  tlin::solve(A, &BX, opt);
  SUPERLU_FREE(BX.Store);
}

//---------------------------------------------------------------

void tlin::solve(SuperMatrix *A, double *b, double *&x,
                 superlu_options_t *opt) {
  SuperMatrix B, X;
  int rows = A->nrow;

  if (!x) x = (double *)malloc(rows * sizeof(double));

  createD(B, rows, 1, rows, b);
  createD(X, rows, 1, rows, x);

  SuperMatrix *Xptr = &X;  //&X is const
  tlin::solve(A, &B, Xptr, opt);
  SUPERLU_FREE(B.Store);
  SUPERLU_FREE(X.Store);
}

//*************************************************************************
//    BLAS-related Functions
//*************************************************************************

void tlin::multiplyS(const SuperMatrix *A, const double *x, double *&y) {
  /*
int sp_dgemv (char *, double, SuperMatrix *, double *,
                  int, double, double *, int);
*/

  if (!y) {
    y = (double *)malloc(A->nrow * sizeof(double));
    memset(y, 0, A->nrow * sizeof(double));
  }

  SuperMatrix *_A = const_cast<SuperMatrix *>(A);
  double *_x      = const_cast<double *>(x);

  sp_dgemv((char *)"N", 1.0, _A, _x, 1, 1.0, y, 1);
}

//---------------------------------------------------------------

void tlin::multiplyD(const SuperMatrix *A, const double *x, double *&y) {
  int lda;
  double *values;
  tlin::readDN(const_cast<SuperMatrix *>(A), lda, values);

  tlin::multiply(A->nrow, A->ncol, values, x, y);
}
