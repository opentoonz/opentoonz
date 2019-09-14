

#include "toonz/ikjacobian.h"

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <iostream>

#include "tstopwatch.h"
using namespace std;

inline bool NearZero(double x, double tolerance) {
  return (fabs(x) <= tolerance);
}

/*
#ifdef _DYNAMIC
const double BASEMAXDIST = 0.02;
#else
const double MAXDIST = 0.08;
#endif


const double DELTA = 0.4;
const long double LAMBDA = 2.0;		// solo per DLS. ottimale : 0.24
const double NEARZERO = 0.0000000001;
*/

//*******************************************************
// Class VectorRn

VectorRn VectorRn::WorkVector;

double VectorRn::MaxAbs() const {
  double result = 0.0;
  double *t     = x;
  for (long i = length; i > 0; i--) {
    if ((*t) > result) {
      result = *t;
    } else if (-(*t) > result) {
      result = -(*t);
    }
    t++;
  }
  return result;
}

//*************************************************************************
// MatrixRmn

MatrixRmn MatrixRmn::WorkMatrix;  // Temporary work matrix

// Fill the diagonal entries with the value d.  The rest of the matrix is
// unchanged.
void MatrixRmn::SetDiagonalEntries(double d) {
  long diagLen = std::min(NumRows, NumCols);
  double *dPtr = x;
  for (; diagLen > 0; diagLen--) {
    *dPtr = d;
    dPtr += NumRows + 1;
  }
}

// Fill the diagonal entries with values in vector d.  The rest of the matrix is
// unchanged.
void MatrixRmn::SetDiagonalEntries(const VectorRn &d) {
  long diagLen = std::min(NumRows, NumCols);
  assert(d.length == diagLen);
  double *dPtr = x;
  double *from = d.x;
  for (; diagLen > 0; diagLen--) {
    *dPtr = *(from++);
    dPtr += NumRows + 1;
  }
}

// Fill the superdiagonal entries with the value d.  The rest of the matrix is
// unchanged.
void MatrixRmn::SetSuperDiagonalEntries(double d) {
  long sDiagLen = std::min(NumRows, (long)(NumCols - 1));
  double *to    = x + NumRows;
  for (; sDiagLen > 0; sDiagLen--) {
    *to = d;
    to += NumRows + 1;
  }
}

// Fill the superdiagonal entries with values in vector d.  The rest of the
// matrix is unchanged.
void MatrixRmn::SetSuperDiagonalEntries(const VectorRn &d) {
  long sDiagLen = std::min((long)(NumRows - 1), NumCols);
  assert(sDiagLen == d.length);
  double *to   = x + NumRows;
  double *from = d.x;
  for (; sDiagLen > 0; sDiagLen--) {
    *to = *(from++);
    to += NumRows + 1;
  }
}

// Fill the subdiagonal entries with the value d.  The rest of the matrix is
// unchanged.
void MatrixRmn::SetSubDiagonalEntries(double d) {
  long sDiagLen = std::min(NumRows, NumCols) - 1;
  double *to    = x + 1;
  for (; sDiagLen > 0; sDiagLen--) {
    *to = d;
    to += NumRows + 1;
  }
}

// Fill the subdiagonal entries with values in vector d.  The rest of the matrix
// is unchanged.
void MatrixRmn::SetSubDiagonalEntries(const VectorRn &d) {
  long sDiagLen = std::min(NumRows, NumCols) - 1;
  assert(sDiagLen == d.length);
  double *to   = x + 1;
  double *from = d.x;
  for (; sDiagLen > 0; sDiagLen--) {
    *to = *(from++);
    to += NumRows + 1;
  }
}

// Set the i-th column equal to d.
void MatrixRmn::SetColumn(long i, const VectorRn &d) {
  assert(NumRows == d.GetLength());
  double *to         = x + i * NumRows;
  const double *from = d.x;
  for (i = NumRows; i > 0; i--) {
    *(to++) = *(from++);
  }
}

// Set the i-th column equal to d.
void MatrixRmn::SetRow(long i, const VectorRn &d) {
  assert(NumCols == d.GetLength());
  double *to         = x + i;
  const double *from = d.x;
  for (i = NumRows; i > 0; i--) {
    *to = *(from++);
    to += NumRows;
  }
}

// Sets a "linear" portion of the array with the values from a vector d
// The first row and column position are given by startRow, startCol.
// Successive positions are found by using the deltaRow, deltaCol values
//	to increment the row and column indices.  There is no wrapping around.
void MatrixRmn::SetSequence(const VectorRn &d, long startRow, long startCol,
                            long deltaRow, long deltaCol) {
  long length = d.length;
  assert(startRow >= 0 && startRow < NumRows && startCol >= 0 &&
         startCol < NumCols);
  assert(startRow + (length - 1) * deltaRow >= 0 &&
         startRow + (length - 1) * deltaRow < NumRows);
  assert(startCol + (length - 1) * deltaCol >= 0 &&
         startCol + (length - 1) * deltaCol < NumCols);
  double *to   = x + startRow + NumRows * startCol;
  double *from = d.x;
  long stride  = deltaRow + NumRows * deltaCol;
  for (; length > 0; length--) {
    *to = *(from++);
    to += stride;
  }
}

// The matrix A is loaded, in into "this" matrix, based at (0,0).
//  The size of "this" matrix must be large enough to accommodate A.
//	The rest of "this" matrix is left unchanged.  It is not filled with
// zeroes!

void MatrixRmn::LoadAsSubmatrix(const MatrixRmn &A) {
  assert(A.NumRows <= NumRows && A.NumCols <= NumCols);
  int extraColStep = NumRows - A.NumRows;
  double *to       = x;
  double *from     = A.x;
  for (long i = A.NumCols; i > 0;
       i--) {  // Copy columns of A, one per time thru loop
    for (long j = A.NumRows; j > 0;
         j--) {  // Copy all elements of this column of A
      *(to++) = *(from++);
    }
    to += extraColStep;
  }
}

// The matrix A is loaded, in transposed order into "this" matrix, based at
// (0,0).
//  The size of "this" matrix must be large enough to accommodate A.
//	The rest of "this" matrix is left unchanged.  It is not filled with
// zeroes!
void MatrixRmn::LoadAsSubmatrixTranspose(const MatrixRmn &A) {
  assert(A.NumRows <= NumCols && A.NumCols <= NumRows);
  double *rowPtr = x;
  double *from   = A.x;
  for (long i = A.NumCols; i > 0; i--) {  // Copy columns of A, once per loop
    double *to = rowPtr;
    for (long j = A.NumRows; j > 0;
         j--) {  // Loop copying values from the column of A
      *to = *(from++);
      to += NumRows;
    }
    rowPtr++;
  }
}

// Calculate the Frobenius Norm (square root of sum of squares of entries of the
// matrix)
double MatrixRmn::FrobeniusNorm() const { return sqrt(FrobeniusNormSq()); }

// Multiply this matrix by column vector v.
// Result is column vector "result"
void MatrixRmn::Multiply(const VectorRn &v, VectorRn &result) const {
  assert(v.GetLength() == NumCols && result.GetLength() == NumRows);
  double *out          = result.GetPtr();  // Points to entry in result vector
  const double *rowPtr = x;  // Points to beginning of next row in matrix
  for (long j = NumRows; j > 0; j--) {
    const double *in = v.GetPtr();
    const double *m  = rowPtr++;
    *out             = 0.0f;
    for (long i = NumCols; i > 0; i--) {
      *out += (*(in++)) * (*m);
      m += NumRows;
    }
    out++;
  }
}

// Multiply transpose of this matrix by column vector v.
//    Result is column vector "result"
// Equivalent to mult by row vector on left
void MatrixRmn::MultiplyTranspose(const VectorRn &v, VectorRn &result) const {
  assert(v.GetLength() == NumRows && result.GetLength() == NumCols);
  double *out          = result.GetPtr();  // Points to entry in result vector
  const double *colPtr = x;  // Points to beginning of next column in matrix
  for (long i = NumCols; i > 0; i--) {
    const double *in = v.GetPtr();
    *out             = 0.0f;
    for (long j = NumRows; j > 0; j--) {
      *out += (*(in++)) * (*(colPtr++));
    }
    out++;
  }
}

// Form the dot product of a vector v with the i-th column of the array
double MatrixRmn::DotProductColumn(const VectorRn &v, long colNum) const {
  assert(v.GetLength() == NumRows);
  double *ptrC = x + colNum * NumRows;
  double *ptrV = v.x;
  double ret   = 0.0;
  for (long i = NumRows; i > 0; i--) {
    ret += (*(ptrC++)) * (*(ptrV++));
  }
  return ret;
}

// Add a constant to each entry on the diagonal
MatrixRmn &MatrixRmn::AddToDiagonal(double d)  // Adds d to each diagonal entry
{
  long diagLen = std::min(NumRows, NumCols);
  double *dPtr = x;
  for (; diagLen > 0; diagLen--) {
    *dPtr += d;
    dPtr += NumRows + 1;
  }
  return *this;
}

// Aggiunge i temini del vettore alla diagonale
MatrixRmn &MatrixRmn::AddToDiagonal(
    const VectorRn &v)  // Adds d to each diagonal entry
{
  long diagLen     = std::min(NumRows, NumCols);
  double *dPtr     = x;
  const double *dv = v.x;
  for (; diagLen > 0; diagLen--) {
    *dPtr += *(dv++);
    dPtr += NumRows + 1;
  }
  return *this;
}

MatrixRmn &MatrixRmn::MultiplyScalar(const MatrixRmn &A, double k,
                                     MatrixRmn &dst) {
  long length  = A.NumCols;
  double *dPtr = dst.x;
  for (long i = dst.NumCols; i > 0; i--) {
    double *aPtr = A.x;  // Points to beginning of row in A

    for (long j = dst.NumRows; j > 0; j--) {
      *dPtr = *aPtr * k;
      dPtr++;
      aPtr++;
    }
    aPtr += A.NumRows;
  }

  return dst;
}
// Multiply two MatrixRmn's
MatrixRmn &MatrixRmn::Multiply(const MatrixRmn &A, const MatrixRmn &B,
                               MatrixRmn &dst) {
  assert(A.NumCols == B.NumRows && A.NumRows == dst.NumRows &&
         B.NumCols == dst.NumCols);
  long length  = A.NumCols;
  double *bPtr = B.x;  // Points to beginning of column in B
  double *dPtr = dst.x;
  for (long i = dst.NumCols; i > 0; i--) {
    double *aPtr = A.x;  // Points to beginning of row in A
    for (long j = dst.NumRows; j > 0; j--) {
      *dPtr = DotArray(length, aPtr, A.NumRows, bPtr, 1);
      dPtr++;
      aPtr++;
    }
    bPtr += B.NumRows;
  }

  return dst;
}

// Multiply two MatrixRmn's,  Transpose the first matrix before multiplying
MatrixRmn &MatrixRmn::TransposeMultiply(const MatrixRmn &A, const MatrixRmn &B,
                                        MatrixRmn &dst) {
  assert(A.NumRows == B.NumRows && A.NumCols == dst.NumRows &&
         B.NumCols == dst.NumCols);
  long length = A.NumRows;

  double *bPtr = B.x;  // bPtr Points to beginning of column in B
  double *dPtr = dst.x;
  for (long i = dst.NumCols; i > 0; i--) {  // Loop over all columns of dst
    double *aPtr = A.x;  // aPtr Points to beginning of column in A
    for (long j = dst.NumRows; j > 0; j--) {  // Loop over all rows of dst
      *dPtr = DotArray(length, aPtr, 1, bPtr, 1);
      dPtr++;
      aPtr += A.NumRows;
    }
    bPtr += B.NumRows;
  }

  return dst;
}

// Multiply two MatrixRmn's.  Transpose the second matrix before multiplying
MatrixRmn &MatrixRmn::MultiplyTranspose(const MatrixRmn &A, const MatrixRmn &B,
                                        MatrixRmn &dst) {
  assert(A.NumCols == B.NumCols && A.NumRows == dst.NumRows &&
         B.NumRows == dst.NumCols);
  long length = A.NumCols;

  double *bPtr = B.x;  // Points to beginning of row in B
  double *dPtr = dst.x;
  for (long i = dst.NumCols; i > 0; i--) {
    double *aPtr = A.x;  // Points to beginning of row in A
    for (long j = dst.NumRows; j > 0; j--) {
      *dPtr = DotArray(length, aPtr, A.NumRows, bPtr, B.NumRows);
      dPtr++;
      aPtr++;
    }
    bPtr++;
  }

  return dst;
}

// Solves the equation   (*this)*xVec = b;
// Uses row operations.  Assumes *this is square and invertible.
// No error checking for divide by zero or instability (except with asserts)
void MatrixRmn::Solve(const VectorRn &b, VectorRn *xVec) const {
  assert(NumRows == NumCols && NumCols == xVec->GetLength() &&
         NumRows == b.GetLength());

  // Copy this matrix and b into an Augmented Matrix
  MatrixRmn &AugMat = GetWorkMatrix(NumRows, NumCols + 1);
  AugMat.LoadAsSubmatrix(*this);
  AugMat.SetColumn(NumRows, b);

  // Put into row echelon form with row operations
  AugMat.ConvertToRefNoFree();

  // Solve for x vector values using back substitution
  double *xLast = xVec->x + NumRows - 1;  // Last entry in xVec
  double *endRow =
      AugMat.x + NumRows * NumCols - 1;  // Last entry in the current row of the
                                         // coefficient part of Augmented Matrix
  double *bPtr = endRow + NumRows;  // Last entry in augmented matrix (end of
                                    // last column, in augmented part)
  for (long i = NumRows; i > 0; i--) {
    double accum = *(bPtr--);
    // Next loop computes back substitution terms
    double *rowPtr =
        endRow;  // Points to entries of the current row for back substitution.
    double *xPtr = xLast;  // Points to entries in the x vector (also for back
                           // substitution)
    for (long j = NumRows - i; j > 0; j--) {
      accum -= (*rowPtr) * (*(xPtr--));
      rowPtr -= NumCols;  // Previous entry in the row
    }
    assert(*rowPtr !=
           0.0);  // Are not supposed to be any free variables in this matrix
    *xPtr = accum / (*rowPtr);
    endRow--;
  }
}

// ConvertToRefNoFree
// Converts the matrix (in place) to row echelon form
// For us, row echelon form allows any non-zero values, not just 1's, in the
//		position for a lead variable.
// The "NoFree" version operates on the assumption that no free variable will be
// found.
// Algorithm uses row operations and row pivoting (only).
// Augmented matrix is correctly accommodated.  Only the first square part
// participates
//		in the main work of row operations.
void MatrixRmn::ConvertToRefNoFree() {
  // Loop over all columns (variables)
  // Find row with most non-zero entry.
  // Swap to the highest active row
  // Subtract appropriately from all the lower rows (row op of type 3)
  long numIters       = std::min(NumRows, NumCols);
  double *rowPtr1     = x;
  const long diagStep = NumRows + 1;
  long lenRowLeft     = NumCols;
  for (; numIters > 1; numIters--) {
    // Find row with most non-zero entry.
    double *rowPtr2  = rowPtr1;
    double maxAbs    = fabs(*rowPtr1);
    double *rowPivot = rowPtr1;
    long i;
    for (i = numIters - 1; i > 0; i--) {
      const double &newMax = *(++rowPivot);
      if (newMax > maxAbs) {
        maxAbs  = *rowPivot;
        rowPtr2 = rowPivot;
      } else if (-newMax > maxAbs) {
        maxAbs  = -newMax;
        rowPtr2 = rowPivot;
      }
    }
    // Pivot step: Swap the row with highest entry to the current row
    if (rowPtr1 != rowPtr2) {
      double *to = rowPtr1;
      for (long i = lenRowLeft; i > 0; i--) {
        double temp = *to;
        *to         = *rowPtr2;
        *rowPtr2    = temp;
        to += NumRows;
        rowPtr2 += NumRows;
      }
    }
    // Subtract this row appropriately from all the lower rows (row operation of
    // type 3)
    rowPtr2 = rowPtr1;
    for (i = numIters - 1; i > 0; i--) {
      rowPtr2++;
      double *to   = rowPtr2;
      double *from = rowPtr1;
      assert(*from != 0.0);
      double alpha = (*to) / (*from);
      *to          = 0.0;
      for (long j = lenRowLeft - 1; j > 0; j--) {
        to += NumRows;
        from += NumRows;
        *to -= (*from) * alpha;
      }
    }
    // Update for next iteration of loop
    rowPtr1 += diagStep;
    lenRowLeft--;
  }
}

// Calculate the c=cosine and s=sine values for a Givens transformation.
// The matrix M = ( (c, -s), (s, c) ) in row order transforms the
//   column vector (a, b)^T to have y-coordinate zero.
void MatrixRmn::CalcGivensValues(double a, double b, double *c, double *s) {
  double denomInv = sqrt(a * a + b * b);
  if (denomInv == 0.0) {
    *c = 1.0;
    *s = 0.0;
  } else {
    denomInv = 1.0 / denomInv;
    *c       = a * denomInv;
    *s       = -b * denomInv;
  }
}

// Applies Givens transform to columns i and i+1.
// Equivalent to postmultiplying by the matrix
//      ( c  -s )
//		( s   c )
// with non-zero entries in rows i and i+1 and columns i and i+1
void MatrixRmn::PostApplyGivens(double c, double s, long idx) {
  assert(0 <= idx && idx < NumCols);
  double *colA = x + idx * NumRows;
  double *colB = colA + NumRows;
  for (long i = NumRows; i > 0; i--) {
    double temp = *colA;
    *colA       = (*colA) * c + (*colB) * s;
    *colB       = (*colB) * c - temp * s;
    colA++;
    colB++;
  }
}

// Applies Givens transform to columns idx1 and idx2.
// Equivalent to postmultiplying by the matrix
//      ( c  -s )
//		( s   c )
// with non-zero entries in rows idx1 and idx2 and columns idx1 and idx2
void MatrixRmn::PostApplyGivens(double c, double s, long idx1, long idx2) {
  assert(idx1 != idx2 && 0 <= idx1 && idx1 < NumCols && 0 <= idx2 &&
         idx2 < NumCols);
  double *colA = x + idx1 * NumRows;
  double *colB = x + idx2 * NumRows;
  for (long i = NumRows; i > 0; i--) {
    double temp = *colA;
    *colA       = (*colA) * c + (*colB) * s;
    *colB       = (*colB) * c - temp * s;
    colA++;
    colB++;
  }
}

// ********************************************************************************************
// Singular value decomposition.
// Return othogonal matrices U and V and diagonal matrix with diagonal w such
// that
//     (this) = U * Diag(w) * V^T     (V^T is V-transpose.)
// Diagonal entries have all non-zero entries before all zero entries, but are
// not
//		necessarily sorted.  (Someday, I will write ComputedSortedSVD
// that
// handles
//		sorting the eigenvalues by magnitude.)
// ********************************************************************************************
void MatrixRmn::ComputeSVD(MatrixRmn &U, VectorRn &w, MatrixRmn &V) const {
  assert(U.NumRows == NumRows && V.NumCols == NumCols &&
         U.NumRows == U.NumCols && V.NumRows == V.NumCols &&
         w.GetLength() == std::min(NumRows, NumCols));

  double temp         = 0.0;
  VectorRn &superDiag = VectorRn::GetWorkVector(
      w.GetLength() - 1);  // Some extra work space.  Will get passed around.

  // Choose larger of U, V to hold intermediate results
  // If U is larger than V, use U to store intermediate results
  // Otherwise use V.  In the latter case, we form the SVD of A transpose,
  //		(which is essentially identical to the SVD of A).
  MatrixRmn *leftMatrix;
  MatrixRmn *rightMatrix;
  if (NumRows >= NumCols) {
    U.LoadAsSubmatrix(*this);  // Copy A into U
    leftMatrix  = &U;
    rightMatrix = &V;
  } else {
    V.LoadAsSubmatrixTranspose(*this);  // Copy A-transpose into V
    leftMatrix  = &V;
    rightMatrix = &U;
  }

  // Do the actual work to calculate the SVD
  // Now matrix has at least as many rows as columns

  CalcBidiagonal(*leftMatrix, *rightMatrix, w, superDiag);
  ConvertBidiagToDiagonal(*leftMatrix, *rightMatrix, w, superDiag);
}

// ************************************************ CalcBidiagonal
// **************************
// Helper routine for SVD computation
// U is a matrix to be bidiagonalized.
// On return, U and V are orthonormal and w holds the new diagonal
//	  elements and superDiag holds the super diagonal elements.

void MatrixRmn::CalcBidiagonal(MatrixRmn &U, MatrixRmn &V, VectorRn &w,
                               VectorRn &superDiag) {
  assert(U.NumRows >= V.NumRows);

  // The diagonal and superdiagonal entries of the bidiagonalized
  //	  version of the U matrix
  //	  are stored in the vectors w and superDiag (temporarily).

  // Apply Householder transformations to U.
  // Householder transformations come in pairs.
  //   First, on the left, we map a portion of a column to zeros
  //   Second, on the right, we map a portion of a row to zeros
  const long rowStep   = U.NumCols;
  const long diagStep  = U.NumCols + 1;
  double *diagPtr      = U.x;
  double *wPtr         = w.x;
  double *superDiagPtr = superDiag.x;
  long colLengthLeft   = U.NumRows;
  long rowLengthLeft   = V.NumCols;
  while (true) {
    // Apply a Householder xform on left to zero part of a column
    SvdHouseholder(diagPtr, colLengthLeft, rowLengthLeft, 1, rowStep, wPtr);

    if (rowLengthLeft == 2) {
      *superDiagPtr = *(diagPtr + rowStep);
      break;
    }

    // Apply a Householder xform on the right to zero part of a row
    SvdHouseholder(diagPtr + rowStep, rowLengthLeft - 1, colLengthLeft, rowStep,
                   1, superDiagPtr);

    rowLengthLeft--;
    colLengthLeft--;
    diagPtr += diagStep;
    wPtr++;
    superDiagPtr++;
  }

  int extra = 0;
  diagPtr += diagStep;
  wPtr++;
  if (colLengthLeft > 2) {
    extra = 1;
    // Do one last Householder transformation when the matrix is not square
    colLengthLeft--;
    SvdHouseholder(diagPtr, colLengthLeft, 1, 1, 0, wPtr);
  } else {
    *wPtr = *diagPtr;
  }

  // Form U and V from the Householder transformations
  V.ExpandHouseholders(V.NumCols - 2, 1, U.x + U.NumRows, U.NumRows, 1);
  U.ExpandHouseholders(V.NumCols - 1 + extra, 0, U.x, 1, U.NumRows);

  // Done with bidiagonalization
  return;
}

// Helper routine for CalcBidiagonal
// Performs a series of Householder transformations on a matrix
// Stores results compactly into the matrix:   The Householder vector u
// (normalized)
//   is stored into the first row/column being transformed.
// The leading term of that row (= plus/minus its magnitude is returned
//	 separately into "retFirstEntry"
void MatrixRmn::SvdHouseholder(double *basePt, long colLength, long numCols,
                               long colStride, long rowStride,
                               double *retFirstEntry) {
  // Calc norm of vector u
  double *cPtr = basePt;
  double norm  = 0.0;
  long i;

  double aa0 = *cPtr;
  double aa1 = *basePt;
  double aa2 = *retFirstEntry;

  for (i = colLength; i > 0; i--) {
    norm += Square(*cPtr);
    cPtr += colStride;
  }
  norm = sqrt(norm);  // Norm of vector to reflect to axis  e_1

  // Handle sign issues
  double imageVal;  // Choose sign to maximize distance
  if ((*basePt) < 0.0) {
    imageVal = norm;
    norm     = 2.0 * norm * (norm - (*basePt));
  } else {
    imageVal = -norm;
    norm     = 2.0 * norm * (norm + (*basePt));
  }
  norm = sqrt(norm);  // Norm is norm of reflection vector

  if (norm == 0.0) {  // If the vector being transformed is equal to zero
    // Force to zero in case of roundoff errors
    cPtr = basePt;
    for (i = colLength; i > 0; i--) {
      *cPtr = 0.0;
      cPtr += colStride;
    }
    *retFirstEntry = 0.0;
    return;
  }

  *retFirstEntry = imageVal;

  // Set up the normalized Householder vector
  *basePt -= imageVal;  // First component changes. Rest stay the same.
  // Normalize the vector
  norm = 1.0 / norm;  // Now it is the inverse norm
  cPtr = basePt;
  for (i = colLength; i > 0; i--) {
    *cPtr *= norm;
    cPtr += colStride;
  }

  // Transform the rest of the U matrix with the Householder transformation
  double *rPtr = basePt;
  for (long j = numCols - 1; j > 0; j--) {
    rPtr += rowStride;
    // Calc dot product with Householder transformation vector
    double dotP = DotArray(colLength, basePt, colStride, rPtr, colStride);
    // Transform with I - 2*dotP*(Householder vector)
    AddArrayScale(colLength, basePt, colStride, rPtr, colStride, -2.0 * dotP);
  }
}

// ********************************* ExpandHouseholders
// ********************************************
// The matrix will be square.
//   numXforms = number of Householder transformations to concatenate
//		Each Householder transformation is represented by a unit vector
//		Each successive Householder transformation starts one position
// later
//			and has one more implied leading zero
//	 basePt = beginning of the first Householder transform
//	 colStride, rowStride: Householder xforms are stored in "columns"
//   numZerosSkipped is the number of implicit zeros on the front each
//			Householder transformation vector (only values supported
// are
// 0 and 1).
void MatrixRmn::ExpandHouseholders(long numXforms, int numZerosSkipped,
                                   const double *basePt, long colStride,
                                   long rowStride) {
  // Number of applications of the last Householder transform
  //     (That are not trivial!)
  long numToTransform = NumCols - numXforms + 1 - numZerosSkipped;
  assert(numToTransform > 0);

  if (numXforms == 0) {
    SetIdentity();
    return;
  }

  // Handle the first one separately as a special case,
  // "this" matrix will be treated to simulate being preloaded with the identity
  long hDiagStride = rowStride + colStride;
  const double *hBase =
      basePt +
      hDiagStride * (numXforms - 1);  // Pointer to the last Householder vector
  const double *hDiagPtr =
      hBase +
      colStride * (numToTransform - 1);  // Pointer to last entry in that vector
  long i;
  double *diagPtr = x + NumCols * NumRows -
                    1;  // Last entry in matrix (points to diagonal entry)
  double *colPtr =
      diagPtr - (numToTransform - 1);  // Pointer to column in matrix
  for (i = numToTransform; i > 0; i--) {
    CopyArrayScale(numToTransform, hBase, colStride, colPtr, 1,
                   -2.0 * (*hDiagPtr));
    *diagPtr += 1.0;  // Add back in 1 to the diagonal entry (since xforming the
                      // identity)
    diagPtr -= (NumRows + 1);  // Next diagonal entry in this matrix
    colPtr -= NumRows;         // Next column in this matrix
    hDiagPtr -= colStride;
  }

  // Now handle the general case
  // A row of zeros must be in effect added to the top of each old column (in
  // each loop)
  double *colLastPtr = x + NumRows * NumCols - numToTransform - 1;
  for (i = numXforms - 1; i > 0; i--) {
    numToTransform++;  // Number of non-trivial applications of this Householder
                       // transformation
    hBase -= hDiagStride;  // Pointer to the beginning of the Householder
                           // transformation
    colPtr = colLastPtr;
    for (long j = numToTransform - 1; j > 0; j--) {
      // Get dot product
      double dotProd2N = -2.0 * DotArray(numToTransform - 1, hBase + colStride,
                                         colStride, colPtr + 1, 1);
      *colPtr = dotProd2N * (*hBase);  // Adding onto zero at initial point
      AddArrayScale(numToTransform - 1, hBase + colStride, colStride,
                    colPtr + 1, 1, dotProd2N);
      colPtr -= NumRows;
    }
    // Do last one as a special case (may overwrite the Householder vector)
    CopyArrayScale(numToTransform, hBase, colStride, colPtr, 1,
                   -2.0 * (*hBase));
    *colPtr += 1.0;  // Add back one one as identity
    // Done with this Householder transformation
    colLastPtr--;
  }

  if (numZerosSkipped != 0) {
    assert(numZerosSkipped == 1);
    // Fill first row and column with identity (More generally: first
    // numZerosSkipped many rows and columns)
    double *d  = x;
    *d         = 1;
    double *d2 = d;
    for (i = NumRows - 1; i > 0; i--) {
      *(++d)           = 0;
      *(d2 += NumRows) = 0;
    }
  }
}

// **************** ConvertBidiagToDiagonal
// ***********************************************
// Do the iterative transformation from bidiagonal form to diagonal form using
//		Givens transformation.  (Golub-Reinsch)
// U and V are square.  Size of U less than or equal to that of U.
void MatrixRmn::ConvertBidiagToDiagonal(MatrixRmn &U, MatrixRmn &V, VectorRn &w,
                                        VectorRn &superDiag) const {
  // These two index into the last bidiagonal block  (last in the matrix, it
  // will be
  //	first one handled.
  long lastBidiagIdx  = V.NumRows - 1;
  long firstBidiagIdx = 0;
  // togliere
  double aa = w.MaxAbs();
  double bb = superDiag.MaxAbs();

  double eps = 1.0e-15 * std::max(w.MaxAbs(), superDiag.MaxAbs());

  while (true) {
    bool workLeft =
        UpdateBidiagIndices(&firstBidiagIdx, &lastBidiagIdx, w, superDiag, eps);
    if (!workLeft) {
      break;
    }

    // Get ready for first Givens rotation
    // Push non-zero to M[2,1] with Givens transformation
    double *wPtr        = w.x + firstBidiagIdx;
    double *sdPtr       = superDiag.x + firstBidiagIdx;
    double extraOffDiag = 0.0;
    if ((*wPtr) == 0.0) {
      ClearRowWithDiagonalZero(firstBidiagIdx, lastBidiagIdx, U, wPtr, sdPtr,
                               eps);
      if (firstBidiagIdx > 0) {
        if (NearZero(*(--sdPtr), eps)) {
          *sdPtr = 0.0;
        } else {
          ClearColumnWithDiagonalZero(firstBidiagIdx, V, wPtr, sdPtr, eps);
        }
      }
      continue;
    }

    // Estimate an eigenvalue from bottom four entries of M
    // This gives a lambda value which will shift the Givens rotations
    // Last four entries of M^T * M are  ( ( A, B ), ( B, C ) ).
    double A;
    A = (firstBidiagIdx < lastBidiagIdx - 1)
            ? Square(superDiag[lastBidiagIdx - 2])
            : 0.0;
    double BSq = Square(w[lastBidiagIdx - 1]);
    A += BSq;  // The "A" entry of M^T * M
    double C = Square(superDiag[lastBidiagIdx - 1]);
    BSq *= C;                       // The squared "B" entry
    C += Square(w[lastBidiagIdx]);  // The "C" entry
    double lambda;                  // lambda will hold the estimated eigenvalue
    lambda = sqrt(Square((A - C) * 0.5) +
                  BSq);  // Use the lambda value that is closest to C.
    if (A > C) {
      lambda = -lambda;
    }
    lambda += (A + C) *
              0.5;  // Now lambda equals the estimate for the last eigenvalue
    double t11 = Square(w[firstBidiagIdx]);
    double t12 = w[firstBidiagIdx] * superDiag[firstBidiagIdx];

    double c, s;
    CalcGivensValues(t11 - lambda, t12, &c, &s);
    ApplyGivensCBTD(c, s, wPtr, sdPtr, &extraOffDiag, wPtr + 1);
    V.PostApplyGivens(c, -s, firstBidiagIdx);
    long i;
    for (i = firstBidiagIdx; i < lastBidiagIdx - 1; i++) {
      // Push non-zero from M[i+1,i] to M[i,i+2]
      CalcGivensValues(*wPtr, extraOffDiag, &c, &s);
      ApplyGivensCBTD(c, s, wPtr, sdPtr, &extraOffDiag, extraOffDiag, wPtr + 1,
                      sdPtr + 1);
      U.PostApplyGivens(c, -s, i);
      // Push non-zero from M[i,i+2] to M[1+2,i+1]
      CalcGivensValues(*sdPtr, extraOffDiag, &c, &s);
      ApplyGivensCBTD(c, s, sdPtr, wPtr + 1, &extraOffDiag, extraOffDiag,
                      sdPtr + 1, wPtr + 2);
      V.PostApplyGivens(c, -s, i + 1);
      wPtr++;
      sdPtr++;
    }
    // Push non-zero value from M[i+1,i] to M[i,i+1] for i==lastBidiagIdx-1
    CalcGivensValues(*wPtr, extraOffDiag, &c, &s);
    ApplyGivensCBTD(c, s, wPtr, &extraOffDiag, sdPtr, wPtr + 1);
    U.PostApplyGivens(c, -s, i);

    // DEBUG
    // DebugCalcBidiagCheck( V, w, superDiag, U );
  }
}

// This is called when there is a zero diagonal entry, with a non-zero
// superdiagonal entry on the same row.
// We use Givens rotations to "chase" the non-zero entry across the row; when it
// reaches the last
//	column, it is finally zeroed away.
// wPtr points to the zero entry on the diagonal.  sdPtr points to the non-zero
// superdiagonal entry on the same row.
void MatrixRmn::ClearRowWithDiagonalZero(long firstBidiagIdx,
                                         long lastBidiagIdx, MatrixRmn &U,
                                         double *wPtr, double *sdPtr,
                                         double eps) {
  double curSd = *sdPtr;  // Value being chased across the row
  *sdPtr       = 0.0;
  long i       = firstBidiagIdx + 1;
  while (true) {
    // Rotate row i and row firstBidiagIdx (Givens rotation)
    double c, s;
    CalcGivensValues(*(++wPtr), curSd, &c, &s);
    U.PostApplyGivens(c, -s, i, firstBidiagIdx);
    *wPtr = c * (*wPtr) - s * curSd;
    if (i == lastBidiagIdx) {
      break;
    }
    curSd = s * (*(++sdPtr));  // New value pops up one column over to the right
    *sdPtr = c * (*sdPtr);
    i++;
  }
}

// This is called when there is a zero diagonal entry, with a non-zero
// superdiagonal entry in the same column.
// We use Givens rotations to "chase" the non-zero entry up the column; when it
// reaches the last
//	column, it is finally zeroed away.
// wPtr points to the zero entry on the diagonal.  sdPtr points to the non-zero
// superdiagonal entry in the same column.
void MatrixRmn::ClearColumnWithDiagonalZero(long endIdx, MatrixRmn &V,
                                            double *wPtr, double *sdPtr,
                                            double eps) {
  double curSd = *sdPtr;  // Value being chased up the column
  *sdPtr       = 0.0;
  long i       = endIdx - 1;
  while (true) {
    double c, s;
    CalcGivensValues(*(--wPtr), curSd, &c, &s);
    V.PostApplyGivens(c, -s, i, endIdx);
    *wPtr = c * (*wPtr) - s * curSd;
    if (i == 0) {
      break;
    }
    curSd = s * (*(--sdPtr));  // New value pops up one row above
    if (NearZero(curSd, eps)) {
      break;
    }
    *sdPtr = c * (*sdPtr);
    i--;
  }
}

// Matrix A is  ( ( a c ) ( b d ) ), i.e., given in column order.
// Mult's G[c,s]  times  A, replaces A.
void MatrixRmn::ApplyGivensCBTD(double cosine, double sine, double *a,
                                double *b, double *c, double *d) {
  double temp = *a;
  *a          = cosine * (*a) - sine * (*b);
  *b          = sine * temp + cosine * (*b);
  temp        = *c;
  *c          = cosine * (*c) - sine * (*d);
  *d          = sine * temp + cosine * (*d);
}

// Now matrix A given in row order, A = ( ( a b c ) ( d e f ) ).
// Return G[c,s] * A, replace A.  d becomes zero, no need to return.
//  Also, it is certain the old *c value is taken to be zero!
void MatrixRmn::ApplyGivensCBTD(double cosine, double sine, double *a,
                                double *b, double *c, double d, double *e,
                                double *f) {
  *a          = cosine * (*a) - sine * d;
  double temp = *b;
  *b          = cosine * (*b) - sine * (*e);
  *e          = sine * temp + cosine * (*e);
  *c          = -sine * (*f);
  *f          = cosine * (*f);
}

// Helper routine for SVD conversion from bidiagonal to diagonal
bool MatrixRmn::UpdateBidiagIndices(long *firstBidiagIdx, long *lastBidiagIdx,
                                    VectorRn &w, VectorRn &superDiag,
                                    double eps) {
  long lastIdx = *lastBidiagIdx;
  double *sdPtr =
      superDiag.GetPtr(lastIdx - 1);  // Entry above the last diagonal entry
  while (NearZero(*sdPtr, eps)) {
    *(sdPtr--) = 0.0;
    lastIdx--;
    if (lastIdx == 0) {
      return false;
    }
  }
  *lastBidiagIdx = lastIdx;
  long firstIdx  = lastIdx - 1;
  double *wPtr   = w.GetPtr(firstIdx);
  while (firstIdx > 0) {
    if (NearZero(*wPtr, eps)) {  // If this diagonal entry (near) zero
      *wPtr = 0.0;
      break;
    }
    if (NearZero(
            *(--sdPtr),
            eps)) {  // If the entry above the diagonal entry is (near) zero
      *sdPtr = 0.0;
      break;
    }
    wPtr--;
    firstIdx--;
  }
  *firstBidiagIdx = firstIdx;
  return true;
}

// ******************************************DEBUG STUFFF

bool MatrixRmn::DebugCheckSVD(const MatrixRmn &U, const VectorRn &w,
                              const MatrixRmn &V) const {
  // Special SVD test code

  MatrixRmn IV(V.getNumRows(), V.getNumColumns());
  IV.SetIdentity();
  MatrixRmn VTV(V.getNumRows(), V.getNumColumns());
  MatrixRmn::TransposeMultiply(V, V, VTV);
  IV -= VTV;
  double error = IV.FrobeniusNorm();

  MatrixRmn IU(U.getNumRows(), U.getNumColumns());
  IU.SetIdentity();
  MatrixRmn UTU(U.getNumRows(), U.getNumColumns());
  MatrixRmn::TransposeMultiply(U, U, UTU);
  IU -= UTU;
  error += IU.FrobeniusNorm();

  MatrixRmn Diag(U.getNumRows(), V.getNumRows());
  Diag.SetZero();
  Diag.SetDiagonalEntries(w);
  MatrixRmn B(U.getNumRows(), V.getNumRows());
  MatrixRmn C(U.getNumRows(), V.getNumRows());
  MatrixRmn::Multiply(U, Diag, B);
  MatrixRmn::MultiplyTranspose(B, V, C);
  C -= *this;
  error += C.FrobeniusNorm();

  bool ret = (fabs(error) <= 1.0e-13 * w.MaxAbs());
  assert(ret);
  return ret;
}

//=============================================================================

const double PI               = 3.1415926535897932384626433832795028841972;
const double RadiansToDegrees = 180.0 / PI;
const double DegreesToRadians = PI / 180;

const double Jacobian::DefaultDampingLambda = 1.1;

const double Jacobian::PseudoInverseThresholdFactor = 0.001;
const double Jacobian::MaxAngleJtranspose           = 30.0 * DegreesToRadians;
const double Jacobian::MaxAnglePseudoinverse        = 5.0 * DegreesToRadians;
const double Jacobian::MaxAngleDLS                  = 5.0 * DegreesToRadians;
const double Jacobian::MaxAngleSDLS                 = 45.0 * DegreesToRadians;
const double Jacobian::BaseMaxTargetDist            = 3.4;

Jacobian::Jacobian(IKSkeleton *skeleton, std::vector<TPointD> &targetPos) {
  Jacobian::skeleton = skeleton;
  nEffector          = skeleton->getNumEffector();
  nJoint             = skeleton->getNodeCount() -
           nEffector;  // numero dei giunti meno gli effettori
  nRow = 2 * nEffector;
  nCol = nJoint;

  target = (targetPos);

  Jend.SetSize(nRow, nCol);  // Matrice jacobiana
  Jend.SetZero();

  Jtarget.SetSize(
      nRow,
      nCol);  // Matrice jacobiana basta sulle posizioni dei targets (non usata)
  Jtarget.SetZero();

  U.SetSize(nRow, nRow);  // matrice U per il calcolo SVD
  w.SetLength(min(nRow, nCol));
  V.SetSize(nCol, nCol);  // matrice V per il calcolo SVD

  dS.SetLength(nRow);      // (Posizione Target ) - (posizione End effector)
  dTheta.SetLength(nCol);  // Cambiamenti degli angoli dei Joints
  dPreTheta.SetLength(nCol);

  // Usato nel: metodo del trasposto dello Jacobiano  & DLS & SDLS
  dT.SetLength(nRow);

  // Usato nel Selectively Damped Least Squares Method
  dSclamp.SetLength(nEffector);

  Jnorms.SetSize(nEffector, nCol);  // Memorizza le norme della matrice attiva J

  DampingLambdaSqV.SetLength(nRow);
  diagMatIdentity.SetLength(nCol);

  Reset();
}

void Jacobian::Reset() {
  // Usato nel Damped Least Squares Method
  DampingLambda   = DefaultDampingLambda;
  DampingLambdaSq = Square(DampingLambda);
  for (int i            = 0; i < DampingLambdaSqV.GetLength(); i++)
    DampingLambdaSqV[i] = DampingLambdaSq;
  for (int i           = 0; i < diagMatIdentity.GetLength(); i++)
    diagMatIdentity[i] = 1.0;
  // DampingLambdaSDLS = 1.5*DefaultDampingLambda;

  dSclamp.Fill(HUGE_VAL);
}

// Calcola il vettore deltaS vector, dS, (l' errore tra end effector e target
// Calcola le matrce jacobiana J
void Jacobian::computeJacobian() {
  // Scorro tutto lo skeleton per trovare tutti gli end effectors

  int numNode = skeleton->getNodeCount();
  for (int index = 0; index < numNode; index++) {
    IKNode *n         = skeleton->getNode(index);
    int effectorCount = skeleton->getNumEffector();
    if (n->IsEffector()) {
      int i                    = n->getEffectorNum();
      const TPointD &targetPos = target[i];
      TPointD temp;
      // Calcolo i valori di deltaS (differenza tra end effectors e target
      // positions.)
      temp      = targetPos;
      TPointD a = n->GetS();
      temp -= n->GetS();
      if (i < effectorCount - 1) {
        temp.x = 100 * temp.x;
        temp.y = 100 * temp.y;
      }
      dS.SetCouple(i, temp);

      // Find all ancestors (they will usually all be joints)
      // Set the corresponding entries in the Jacobians J, K.
      IKNode *m = skeleton->getParent(n);

      while (m) {
        int j = m->getJointNum();
        // assert(j>=0 && j<skeleton->GetNumJoint());
        int numnode = skeleton->getNodeCount();
        assert(0 <= i && i < nEffector && 0 <= j &&
               j < (skeleton->getNodeCount() - skeleton->getNumEffector()));
        if (m->isFrozen()) {
          Jend.SetCouple(i, j, TPointD(0.0, 0.0));

        } else {
          temp = m->GetS();   // joint pos.
          temp -= n->GetS();  // -(end effector pos. - joint pos.)

          double tx = temp.x;
          temp.x    = temp.y;
          temp.y    = -tx;

          if (i < effectorCount - 1) {
            temp.x = 100 * temp.x;
            temp.y = 100 * temp.y;
          }
          Jend.SetCouple(i, j, temp);
        }
        m = skeleton->getParent(m);
      }
    }
  }
}

// The delta theta values have been computed in dTheta array
// Apply the delta theta values to the joints
// Nothing is done about joint limits for now.
void Jacobian::UpdateThetas() {
  // Update the joint angles
  for (int index = 0; index < skeleton->getNodeCount(); index++) {
    IKNode *n = skeleton->getNode(index);
    if (n->IsJoint()) {
      int i = n->getJointNum();
      n->AddToTheta(dTheta[i]);
    }
  }
  // Aggiorno le posizioni dei joint
  skeleton->compute();
}

bool Jacobian::checkJointsLimit() {
  bool clampingDetected = false;
  /*
  Node* n = skeleton->getNode(3);
  int indexJoint = n->getJointNum();
  double theta = n->getTheta();
  double upperLimit = PI;
  double lowerLimit = 0.0;
  if(theta >upperLimit || theta <lowerLimit)
  {
          if(theta<upperLimit)	upperLimit = lowerLimit;
          clampingDetected = true;
          double clampingVar = theta - upperLimit;
          for(int i=0;i<Jactive->getNumRows();i++)
          {
                  double tmp = clampingVar*Jactive->Get(i,indexJoint);
                  dS[i] -= tmp;
                  Jactive->Set(i,indexJoint,0.0);
          }
          n->setTheta(upperLimit);
          diagMatIdentity.Set(indexJoint, 0.0);

  }*/
  return clampingDetected;
}

void Jacobian::ZeroDeltaThetas() { dTheta.SetZero(); }

// Find the delta theta values using inverse Jacobian method
// Uses a greedy method to decide scaling factor
void Jacobian::CalcDeltaThetasTranspose() {
  const MatrixRmn &J = Jend;

  J.MultiplyTranspose(dS, dTheta);

  // Scale back the dTheta values greedily
  J.Multiply(dTheta, dT);  // dT = J * dTheta
  double alpha = Dot(dS, dT) / dT.NormSq();
  assert(alpha > 0.0);
  // Also scale back to be have max angle change less than MaxAngleJtranspose
  double maxChange = dTheta.MaxAbs();
  double beta      = MaxAngleJtranspose / maxChange;
  dTheta *= min(alpha, beta);
}

void Jacobian::CalcDeltaThetasPseudoinverse() {
  MatrixRmn &J = const_cast<MatrixRmn &>(Jend);

  // costruisco matrice J1
  MatrixRmn J1;
  J1.SetSize(2, J.getNumColumns());

  for (int i = 0; i < 2; i++)
    for (int j = 0; j < J.getNumColumns(); j++) J1.Set(i, j, J.Get(i, j));

  // COSTRUISCO VETTORI ds1 e ds2
  VectorRn dS1(2);

  for (int i = 0; i < 2; i++) dS1.Set(i, dS.Get(i));

  // calcolo dtheta1
  MatrixRmn U, V;
  VectorRn w;

  U.SetSize(J1.getNumRows(), J1.getNumRows());
  w.SetLength(min(J1.getNumRows(), J1.getNumColumns()));
  V.SetSize(J1.getNumColumns(), J1.getNumColumns());

  J1.ComputeSVD(U, w, V);

  // Next line for debugging only
  assert(J1.DebugCheckSVD(U, w, V));

  // Calculate response vector dTheta that is the DLS solution.
  //	Delta target values are the dS values
  //  We multiply by Moore-Penrose pseudo-inverse of the J matrix

  double pseudoInverseThreshold = PseudoInverseThresholdFactor * w.MaxAbs();

  long diagLength = w.GetLength();
  double *wPtr    = w.GetPtr();
  dTheta.SetZero();
  for (long i = 0; i < diagLength; i++) {
    double dotProdCol =
        U.DotProductColumn(dS1, i);  // Dot product with i-th column of U
    double alpha = *(wPtr++);
    if (fabs(alpha) > pseudoInverseThreshold) {
      alpha = 1.0 / alpha;
      MatrixRmn::AddArrayScale(V.getNumRows(), V.GetColumnPtr(i), 1,
                               dTheta.GetPtr(), 1, dotProdCol * alpha);
    }
  }

  MatrixRmn JcurrentPinv(V.getNumRows(),
                         J1.getNumRows());  // pseudoinversa di J1
  MatrixRmn JProjPre(JcurrentPinv.getNumRows(),
                     J1.getNumColumns());  // Proiezione di J1
  if (skeleton->getNumEffector() > 1) {
    // calcolo la pseudoinversa di J1
    MatrixRmn VD(V.getNumRows(), J1.getNumRows());  // matrice del prodotto V*w

    double *wPtr           = w.GetPtr();
    pseudoInverseThreshold = PseudoInverseThresholdFactor * w.MaxAbs();
    for (int j = 0; j < VD.getNumColumns(); j++) {
      double *VPtr = V.GetColumnPtr(j);
      double alpha = *(wPtr++);  // elemento matrice diagonale
      for (int i = 0; i < V.getNumRows(); i++) {
        if (fabs(alpha) > pseudoInverseThreshold) {
          double entry = *(VPtr++);
          VD.Set(i, j, entry * (1.0 / alpha));
        }
      }
    }
    MatrixRmn::MultiplyTranspose(VD, U, JcurrentPinv);

    // calcolo la proiezione J1
    MatrixRmn::Multiply(JcurrentPinv, J1, JProjPre);

    for (int j = 0; j < JProjPre.getNumColumns(); j++)
      for (int i = 0; i < JProjPre.getNumRows(); i++) {
        double temp = JProjPre.Get(i, j);
        JProjPre.Set(i, j, -1.0 * temp);
      }
    JProjPre.AddToDiagonal(diagMatIdentity);
  }

  // task priority strategy
  for (int i = 1; i < skeleton->getNumEffector(); i++) {
    // costruisco matrice Jcurrent (Ji)
    MatrixRmn Jcurrent(2, J.getNumColumns());
    for (int j = 0; j < J.getNumColumns(); j++)
      for (int k = 0; k < 2; k++) Jcurrent.Set(k, j, J.Get(k + 2 * i, j));

    // costruisco il vettore dScurrent
    VectorRn dScurrent(2);
    for (int k = 0; k < 2; k++) dScurrent.Set(k, dS.Get(k + 2 * i));

    // Moltiplico Jcurrent per la proiezione di J(i-1)
    MatrixRmn Jdst(Jcurrent.getNumRows(), JProjPre.getNumColumns());
    MatrixRmn::Multiply(Jcurrent, JProjPre, Jdst);

    // Calcolo la pseudoinversa di Jdst
    MatrixRmn UU(Jdst.getNumRows(), Jdst.getNumRows()),
        VV(Jdst.getNumColumns(), Jdst.getNumColumns());
    VectorRn ww(min(Jdst.getNumRows(), Jdst.getNumColumns()));

    Jdst.ComputeSVD(UU, ww, VV);
    assert(Jdst.DebugCheckSVD(UU, ww, VV));

    MatrixRmn VVD(VV.getNumRows(), J1.getNumRows());  // matrice V*w
    VVD.SetZero();
    pseudoInverseThreshold = PseudoInverseThresholdFactor * ww.MaxAbs();
    double *wwPtr          = ww.GetPtr();
    for (int j = 0; j < VVD.getNumColumns(); j++) {
      double *VVPtr = VV.GetColumnPtr(j);
      double alpha  = 50 * (*(wwPtr++));  // elemento matrice diagonale
      for (int i = 0; i < VV.getNumRows(); i++) {
        if (fabs(alpha) > 100 * pseudoInverseThreshold) {
          double entry = *(VVPtr++);
          VVD.Set(i, j, entry * (1.0 / alpha));
        }
      }
    }
    MatrixRmn JdstPinv(VV.getNumRows(), J1.getNumRows());
    MatrixRmn::MultiplyTranspose(VVD, UU, JdstPinv);

    VectorRn dTemp(J1.getNumRows());
    Jcurrent.Multiply(dTheta, dTemp);

    VectorRn dTemp2(dScurrent.GetLength());
    for (int k  = 0; k < dScurrent.GetLength(); k++)
      dTemp2[k] = dScurrent[k] - dTemp[k];

    // Moltiplico JdstPinv per dTemp2
    VectorRn dThetaCurrent(JdstPinv.getNumRows());
    JdstPinv.Multiply(dTemp2, dThetaCurrent);
    for (int k = 0; k < dTheta.GetLength(); k++) dTheta[k] += dThetaCurrent[k];

    // Infine mi calcolo la pseudoinversa di Jcurrent e quindi la sua proiezione
    // che servirà al passo successivo

    // calcolo la pseudoinversa di Jcurrent
    Jcurrent.ComputeSVD(U, w, V);
    assert(Jcurrent.DebugCheckSVD(U, w, V));

    MatrixRmn VD(V.getNumRows(),
                 Jcurrent.getNumRows());  // matrice del prodotto V*w

    double *wPtr           = w.GetPtr();
    pseudoInverseThreshold = PseudoInverseThresholdFactor * w.MaxAbs();
    for (int j = 0; j < VVD.getNumColumns(); j++) {
      double *VPtr = V.GetColumnPtr(j);
      double alpha = *(wPtr++);  // elemento matrice diagonale
      for (int i = 0; i < V.getNumRows(); i++) {
        if (fabs(alpha) > pseudoInverseThreshold) {
          double entry = *(VPtr++);
          VD.Set(i, j, entry * (1.0 / alpha));
        }
      }
    }
    MatrixRmn::MultiplyTranspose(VD, U, JcurrentPinv);

    // calcolo la proiezione Jcurrent
    MatrixRmn::Multiply(JcurrentPinv, Jcurrent, JProjPre);

    for (int j = 0; j < JProjPre.getNumColumns(); j++)
      for (int k = 0; k < JProjPre.getNumRows(); k++) {
        double temp = JProjPre.Get(k, j);
        JProjPre.Set(k, j, -1.0 * temp);
      }
    JProjPre.AddToDiagonal(diagMatIdentity);
  }

  // sw.stop();
  // std::ofstream os("C:\\buttami.txt", std::ios::app);
  // sw.print(os);
  // os.close();

  // Scale back to not exceed maximum angle changes
  double maxChange = 10 * dTheta.MaxAbs();
  if (maxChange > MaxAnglePseudoinverse) {
    dTheta *= MaxAnglePseudoinverse / maxChange;
  }
}

void Jacobian::CalcDeltaThetasDLS() {
  const MatrixRmn &J = Jend;

  MatrixRmn::MultiplyTranspose(J, J, U);  // U = J * (J^T)

  U.AddToDiagonal(DampingLambdaSqV);

  // Use the next four lines instead of the succeeding two lines for the DLS
  // method with clamped error vector e.
  // CalcdTClampedFromdS();
  // VectorRn dTextra(2*nEffector);
  // U.Solve( dT, &dTextra );
  // J.MultiplyTranspose( dTextra, dTheta );

  // Use these two lines for the traditional DLS method
  // gennaro

  U.Solve(dS, &dT);
  J.MultiplyTranspose(dT, dTheta);

  // Scalo per non superare l'nagolo massimod i cambiamento
  double maxChange = 100 * dTheta.MaxAbs();
  if (maxChange > MaxAngleDLS) {
    dTheta *= MaxAngleDLS / maxChange;
  }
}

void Jacobian::CalcDeltaThetasDLSwithSVD() {
  const MatrixRmn &J = Jend;

  J.ComputeSVD(U, w, V);

  // For Debug
  assert(J.DebugCheckSVD(U, w, V));

  // Calculate response vector dTheta that is the DLS solution.
  //	Delta target values are the dS values
  //  We multiply by DLS inverse of the J matrix
  long diagLength = w.GetLength();
  double *wPtr    = w.GetPtr();
  dTheta.SetZero();
  for (long i = 0; i < diagLength; i++) {
    double dotProdCol =
        U.DotProductColumn(dS, i);  // Dot product with i-th column of U
    double alpha = *(wPtr++);
    alpha        = alpha / (Square(alpha) + DampingLambdaSq);
    MatrixRmn::AddArrayScale(V.getNumRows(), V.GetColumnPtr(i), 1,
                             dTheta.GetPtr(), 1, dotProdCol * alpha);
  }

  // Scale back to not exceed maximum angle changes
  double maxChange = dTheta.MaxAbs();
  if (maxChange > MaxAngleDLS) {
    dTheta *= MaxAngleDLS / maxChange;
  }
}

void Jacobian::CalcDeltaThetasSDLS() {
  const MatrixRmn &J = Jend;

  // Compute Singular Value Decomposition

  J.ComputeSVD(U, w, V);

  // Next line for debugging only
  assert(J.DebugCheckSVD(U, w, V));

  // Calculate response vector dTheta that is the SDLS solution.
  //	Delta target values are the dS values
  int nRows           = J.getNumRows();
  int numEndEffectors = skeleton->getNumEffector();  // Equals the number of
                                                     // rows of J divided by
                                                     // three
  int nCols = J.getNumColumns();
  dTheta.SetZero();

  // Calculate the norms of the 3-vectors in the Jacobian
  long i;
  const double *jx = J.GetPtr();
  double *jnx      = Jnorms.GetPtr();
  for (i = nCols * numEndEffectors; i > 0; i--) {
    double accumSq = Square(*(jx++));
    accumSq += Square(*(jx++));
    accumSq += Square(*(jx++));
    *(jnx++) = sqrt(accumSq);
  }

  // Clamp the dS values
  CalcdTClampedFromdS();

  // Loop over each singular vector
  for (i = 0; i < nRows; i++) {
    double wiInv = w[i];
    if (NearZero(wiInv, 1.0e-10)) {
      continue;
    }
    wiInv = 1.0 / wiInv;

    double N = 0.0;  // N is the quasi-1-norm of the i-th column of U
    double alpha =
        0.0;  // alpha is the dot product of dT and the i-th column of U

    const double *dTx = dT.GetPtr();
    const double *ux  = U.GetColumnPtr(i);
    long j;
    for (j = numEndEffectors; j > 0; j--) {
      double tmp;
      alpha += (*ux) * (*(dTx++));
      tmp = Square(*(ux++));
      alpha += (*ux) * (*(dTx++));
      tmp += Square(*(ux++));
      alpha += (*ux) * (*(dTx++));
      tmp += Square(*(ux++));
      N += sqrt(tmp);
    }

    // M is the quasi-1-norm of the response to angles changing according to the
    // i-th column of V
    //		Then is multiplied by the wiInv value.
    double M   = 0.0;
    double *vx = V.GetColumnPtr(i);
    jnx        = Jnorms.GetPtr();
    for (j = nCols; j > 0; j--) {
      double accum = 0.0;
      for (long k = numEndEffectors; k > 0; k--) {
        accum += *(jnx++);
      }
      M += fabs((*(vx++))) * accum;
    }
    M *= fabs(wiInv);

    double gamma = MaxAngleSDLS;
    if (N < M) {
      gamma *= N / M;  // Scale back maximum permissible joint angle
    }

    // Calculate the dTheta from pure pseudoinverse considerations
    double scale =
        alpha *
        wiInv;  // This times i-th column of V is the psuedoinverse response
    dPreTheta.LoadScaled(V.GetColumnPtr(i), scale);
    // Now rescale the dTheta values.
    double max     = dPreTheta.MaxAbs();
    double rescale = (gamma) / (gamma + max);
    dTheta.AddScaled(dPreTheta, rescale);
    /*if ( gamma<max) {
            dTheta.AddScaled( dPreTheta, gamma/max );
    }
    else {
            dTheta += dPreTheta;
    }*/
  }

  // Scale back to not exceed maximum angle changes
  double maxChange = dTheta.MaxAbs();
  if (maxChange > 100 * MaxAngleSDLS) {
    dTheta *= MaxAngleSDLS / (MaxAngleSDLS + maxChange);
    // dTheta *= MaxAngleSDLS/maxChange;
  }
}

void Jacobian::CalcdTClampedFromdS() {
  long len = dS.GetLength();
  long j   = 0;
  for (long i = 0; i < len; i += 2, j++) {
    double normSq = Square(dS[i]) + Square(dS[i + 1]);  //+Square(dS[i+2]);
    if (normSq > Square(dSclamp[j])) {
      double factor = dSclamp[j] / sqrt(normSq);
      dT[i]         = dS[i] * factor;
      dT[i + 1]     = dS[i + 1] * factor;
      // dT[i+2] = dS[i+2]*factor;
    } else {
      dT[i]     = dS[i];
      dT[i + 1] = dS[i + 1];
      // dT[i+2] = dS[i+2];
    }
  }
}

void Jacobian::UpdatedSClampValue() {
  // Traverse skeleton to find all end effectors
  TPointD temp;

  int numNode = skeleton->getNodeCount();
  for (int i = 0; i < numNode; i++) {
    IKNode *n = skeleton->getNode(i);
    if (n->IsEffector()) {
      int i                    = n->getEffectorNum();
      const TPointD &targetPos = target[i];

      // Compute the delta S value (differences from end effectors to target
      // positions.
      // While we are at it, also update the clamping values in dSclamp;
      temp = targetPos;
      temp -= n->GetS();
      double normSi      = sqrt(Square(dS[i]) + Square(dS[i + 1]));
      double norma       = sqrt(temp.x * temp.x + temp.y * temp.y);
      double changedDist = norma - normSi;

      if (changedDist > 0.0) {
        dSclamp[i] = BaseMaxTargetDist + changedDist;
      } else {
        dSclamp[i] = BaseMaxTargetDist;
      }
    }
  }
}
