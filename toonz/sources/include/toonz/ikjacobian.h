#pragma once

#ifndef JACOBIAN_H
#define JACOBIAN_H

#include "iknode.h"
#include "ikskeleton.h"
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//******************** IK Utility *********************
//*****************************************************

//***********************************************************************
// CLASS VectorRN
// void DVAPI Dump(const TPointD &point, double* v);

class DVAPI VectorRn {
  friend class MatrixRmn;

public:
  VectorRn();             // Null constructor
  VectorRn(long length);  // Constructor with length
  ~VectorRn();            // Destructor

  void SetLength(long newLength);
  long GetLength() const { return length; }

  void SetZero();
  void Fill(double d);
  void Load(const double *d);
  void LoadScaled(const double *d, double scaleFactor);
  void Set(const VectorRn &src);

  // Two access methods identical in functionality
  // Subscripts are ZERO-BASED!!
  const double &operator[](long i) const {
    assert(0 <= i && i < length);
    return *(x + i);
  }
  double &operator[](long i) {
    assert(0 <= i && i < length);
    return *(x + i);
  }
  double Get(long i) const {
    assert(0 <= i && i < length);
    return *(x + i);
  }

  // Use GetPtr to get pointer into the array (efficient)
  // Is friendly in that anyone can change the array contents (be careful!)
  const double *GetPtr(long i) const {
    assert(0 <= i && i < length);
    return (x + i);
  }
  double *GetPtr(long i) {
    assert(0 <= i && i < length);
    return (x + i);
  }
  const double *GetPtr() const { return x; }
  double *GetPtr() { return x; }

  void Set(long i, double val) { assert(0 <= i && i < length), *(x + i) = val; }
  void SetCouple(long i, const TPointD &u);

  VectorRn &operator+=(const VectorRn &src);
  VectorRn &operator-=(const VectorRn &src);
  void AddScaled(const VectorRn &src, double scaleFactor);

  VectorRn &operator*=(double f);
  double NormSq() const;
  double Norm() const { return sqrt(NormSq()); }

  double MaxAbs() const;

private:
  long length;       // Logical or actual length
  long AllocLength;  // Allocated length
  double *x;         // Array of vector entries

  static VectorRn WorkVector;  // Serves as a temporary vector
  static VectorRn &GetWorkVector() { return WorkVector; }
  static VectorRn &GetWorkVector(long len) {
    WorkVector.SetLength(len);
    return WorkVector;
  }
};

inline VectorRn::VectorRn() {
  length      = 0;
  AllocLength = 0;
  x           = 0;
}

inline VectorRn::VectorRn(long initLength) {
  length      = 0;
  AllocLength = 0;
  x           = 0;
  SetLength(initLength);
}

inline VectorRn::~VectorRn() { delete x; }

// Resize.
// If the array is shortened, the information about the allocated length is
// lost.
inline void VectorRn::SetLength(long newLength) {
  assert(newLength > 0);
  if (newLength > AllocLength) {
    delete x;
    AllocLength = std::max(newLength, AllocLength << 1);
    x           = new double[AllocLength];
  }
  length = newLength;
}

// Zero out the entire vector
inline void VectorRn::SetZero() {
  double *target = x;
  for (long i = length; i > 0; i--) {
    *(target++) = 0.0;
  }
}

// Set the value of the i-th triple of entries in the vector
inline void VectorRn::SetCouple(long i, const TPointD &u) {
  long j = 2 * i;
  assert(0 <= j && j + 1 < length);
  x[j]     = u.x;
  x[j + 1] = u.y;
}

inline void VectorRn::Fill(double d) {
  double *to = x;
  for (long i = length; i > 0; i--) {
    *(to++) = d;
  }
}

inline void VectorRn::Load(const double *d) {
  double *to = x;
  for (long i = length; i > 0; i--) {
    *(to++) = *(d++);
  }
}

inline void VectorRn::LoadScaled(const double *d, double scaleFactor) {
  double *to = x;
  for (long i = length; i > 0; i--) {
    *(to++) = (*(d++)) * scaleFactor;
  }
}

inline void VectorRn::Set(const VectorRn &src) {
  assert(src.length == this->length);
  double *to   = x;
  double *from = src.x;
  for (long i = length; i > 0; i--) {
    *(to++) = *(from++);
  }
}

inline VectorRn &VectorRn::operator+=(const VectorRn &src) {
  assert(src.length == this->length);
  double *to   = x;
  double *from = src.x;
  for (long i = length; i > 0; i--) {
    *(to++) += *(from++);
  }
  return *this;
}

inline VectorRn &VectorRn::operator-=(const VectorRn &src) {
  assert(src.length == this->length);
  double *to   = x;
  double *from = src.x;
  for (long i = length; i > 0; i--) {
    *(to++) -= *(from++);
  }
  return *this;
}

inline void VectorRn::AddScaled(const VectorRn &src, double scaleFactor) {
  assert(src.length == this->length);
  double *to   = x;
  double *from = src.x;
  for (long i = length; i > 0; i--) {
    *(to++) += (*(from++)) * scaleFactor;
  }
}

inline VectorRn &VectorRn::operator*=(double f) {
  double *target = x;
  for (long i = length; i > 0; i--) {
    *(target++) *= f;
  }
  return *this;
}

inline double VectorRn::NormSq() const {
  double *target = x;
  double res     = 0.0;
  for (long i = length; i > 0; i--) {
    res += (*target) * (*target);
    target++;
  }
  return res;
}

inline double DVAPI Dot(const VectorRn &u, const VectorRn &v) {
  assert(u.GetLength() == v.GetLength());
  double res      = 0.0;
  const double *p = u.GetPtr();
  const double *q = v.GetPtr();
  for (long i = u.GetLength(); i > 0; i--) {
    res += (*(p++)) * (*(q++));
  }
  return res;
}

//*******************************************************************
// MatrixRmn

class DVAPI MatrixRmn {
public:
  MatrixRmn();                            // Null constructor
  MatrixRmn(long numRows, long numCols);  // Constructor with length
  ~MatrixRmn();                           // Destructor

  void SetSize(long numRows, long numCols);
  long getNumRows() const { return NumRows; }
  long getNumColumns() const { return NumCols; }
  void SetZero();

  // Return entry in row i and column j.
  double Get(long i, long j) const;
  void GetCouple(long i, long j, TPointD *retValue) const;

  // Use GetPtr to get pointer into the array (efficient)
  // Is friendly in that anyone can change the array contents (be careful!)
  // The entries are in column order!!!
  // Use this with care.  You may call GetRowStride and GetColStride to navigate
  //			within the matrix.  I do not expect these values to ever
  // change.
  const double *GetPtr() const;
  double *GetPtr();
  const double *GetPtr(long i, long j) const;
  double *GetPtr(long i, long j);
  const double *GetColumnPtr(long j) const;
  double *GetColumnPtr(long j);
  const double *GetRowPtr(long i) const;
  double *GetRowPtr(long i);
  long GetRowStride() const {
    return NumRows;
  }                                        // Step size (stride) along a row
  long GetColStride() const { return 1; }  // Step size (stide) along a column

  void Set(long i, long j, double val);
  void SetCouple(long i, long c, const TPointD &u);

  void SetIdentity();
  void SetDiagonalEntries(double d);
  void SetDiagonalEntries(const VectorRn &d);
  void SetSuperDiagonalEntries(double d);
  void SetSuperDiagonalEntries(const VectorRn &d);
  void SetSubDiagonalEntries(double d);
  void SetSubDiagonalEntries(const VectorRn &d);
  void SetColumn(long i, const VectorRn &d);
  void SetRow(long i, const VectorRn &d);
  void SetSequence(const VectorRn &d, long startRow, long startCol,
                   long deltaRow, long deltaCol);

  // Loads matrix in as a sub-matrix.  (i,j) is the base point. Defaults to
  // (0,0).
  // The "Tranpose" versions load the transpose of A.
  void LoadAsSubmatrix(const MatrixRmn &A);
  void LoadAsSubmatrix(long i, long j, const MatrixRmn &A);
  void LoadAsSubmatrixTranspose(const MatrixRmn &A);
  void LoadAsSubmatrixTranspose(long i, long j, const MatrixRmn &A);

  // Norms
  double FrobeniusNormSq() const;
  double FrobeniusNorm() const;

  // Operations on VectorRn's
  void Multiply(const VectorRn &v,
                VectorRn &result) const;  // result = (this)*(v)
  void MultiplyTranspose(const VectorRn &v, VectorRn &result)
      const;  // Equivalent to mult by row vector on left
  double DotProductColumn(const VectorRn &v, long colNum)
      const;  // Returns dot product of v with i-th column

  // Operations on MatrixRmn's
  MatrixRmn &operator*=(double);
  MatrixRmn &operator/=(double d) {
    assert(d != 0.0);
    *this *= (1.0 / d);
    return *this;
  }
  MatrixRmn &AddScaled(const MatrixRmn &B, double factor);
  MatrixRmn &operator+=(const MatrixRmn &B);
  MatrixRmn &operator-=(const MatrixRmn &B);
  static MatrixRmn &Multiply(const MatrixRmn &A, const MatrixRmn &B,
                             MatrixRmn &dst);  // Sets dst = A*B.
  static MatrixRmn &MultiplyScalar(const MatrixRmn &A, double k,
                                   MatrixRmn &result);
  static MatrixRmn &MultiplyTranspose(
      const MatrixRmn &A, const MatrixRmn &B,
      MatrixRmn &dst);  // Sets dst = A*(B-tranpose).
  static MatrixRmn &TransposeMultiply(
      const MatrixRmn &A, const MatrixRmn &B,
      MatrixRmn &dst);  // Sets dst = (A-transpose)*B.

  // Miscellaneous operation
  MatrixRmn &AddToDiagonal(double d);  // Adds d to each diagonal
  MatrixRmn &AddToDiagonal(
      const VectorRn &v);  // Adds vector elements to diagonal

  // Solving systems of linear equations
  void Solve(const VectorRn &b, VectorRn *x) const;  // Solves the equation
                                                     // (*this)*x = b;    Uses
                                                     // row operations.  Assumes
                                                     // *this is invertible.

  // Row Echelon Form and Reduced Row Echelon Form routines
  // Row echelon form here allows non-negative entries (instead of 1's) in the
  // positions of lead variables.
  void ConvertToRefNoFree();  // Converts the matrix in place to row echelon
                              // form -- assumption is no free variables will be
                              // found
  void ConvertToRef(int numVars);  // Converts the matrix in place to row
                                   // echelon form -- numVars is number of
                                   // columns to work with.
  void ConvertToRef(
      int numVars,
      double eps);  // Same, but eps is the measure of closeness to zero

  // Givens transformation
  static void CalcGivensValues(double a, double b, double *c, double *s);
  void PostApplyGivens(
      double c, double s,
      long idx);  // Applies Givens transform to columns idx and idx+1.
  void PostApplyGivens(
      double c, double s, long idx1,
      long idx2);  // Applies Givens transform to columns idx1 and idx2.

  // Singular value decomposition
  void ComputeSVD(MatrixRmn &U, VectorRn &w, MatrixRmn &V) const;
  // Good for debugging SVD computations (I recommend this be used for any new
  // application to check for bugs/instability).
  bool DebugCheckSVD(const MatrixRmn &U, const VectorRn &w,
                     const MatrixRmn &V) const;

  // Some useful routines for experts who understand the inner workings of these
  // classes.
  inline static double DotArray(long length, const double *ptrA, long strideA,
                                const double *ptrB, long strideB);
  inline static void CopyArrayScale(long length, const double *from,
                                    long fromStride, double *to, long toStride,
                                    double scale);
  inline static void AddArrayScale(long length, const double *from,
                                   long fromStride, double *to, long toStride,
                                   double scale);

private:
  long NumRows;    // Number of rows
  long NumCols;    // Number of columns
  double *x;       // Array of vector entries - stored in column order
  long AllocSize;  // Allocated size of the x array

  static MatrixRmn WorkMatrix;  // Temporary work matrix
  static MatrixRmn &GetWorkMatrix() { return WorkMatrix; }
  static MatrixRmn &GetWorkMatrix(long numRows, long numCols) {
    WorkMatrix.SetSize(numRows, numCols);
    return WorkMatrix;
  }

  // Internal helper routines for SVD calculations
  static void CalcBidiagonal(MatrixRmn &U, MatrixRmn &V, VectorRn &w,
                             VectorRn &superDiag);
  void ConvertBidiagToDiagonal(MatrixRmn &U, MatrixRmn &V, VectorRn &w,
                               VectorRn &superDiag) const;
  static void SvdHouseholder(double *basePt, long colLength, long numCols,
                             long colStride, long rowStride,
                             double *retFirstEntry);
  void ExpandHouseholders(long numXforms, int numZerosSkipped,
                          const double *basePt, long colStride, long rowStride);
  static bool UpdateBidiagIndices(long *firstDiagIdx, long *lastBidiagIdx,
                                  VectorRn &w, VectorRn &superDiag, double eps);
  static void ApplyGivensCBTD(double cosine, double sine, double *a, double *b,
                              double *c, double *d);
  static void ApplyGivensCBTD(double cosine, double sine, double *a, double *b,
                              double *c, double d, double *e, double *f);
  static void ClearRowWithDiagonalZero(long firstBidiagIdx, long lastBidiagIdx,
                                       MatrixRmn &U, double *wPtr,
                                       double *sdPtr, double eps);
  static void ClearColumnWithDiagonalZero(long endIdx, MatrixRmn &V,
                                          double *wPtr, double *sdPtr,
                                          double eps);
  bool DebugCalcBidiagCheck(const MatrixRmn &U, const VectorRn &w,
                            const VectorRn &superDiag,
                            const MatrixRmn &V) const;
};

inline MatrixRmn::MatrixRmn() {
  NumRows   = 0;
  NumCols   = 0;
  x         = 0;
  AllocSize = 0;
}

inline MatrixRmn::MatrixRmn(long numRows, long numCols) {
  NumRows   = 0;
  NumCols   = 0;
  x         = 0;
  AllocSize = 0;
  SetSize(numRows, numCols);
}

inline MatrixRmn::~MatrixRmn() { delete x; }

// Resize.
// If the array space is decreased, the information about the allocated length
// is lost.
inline void MatrixRmn::SetSize(long numRows, long numCols) {
  assert(numRows > 0 && numCols > 0);
  long newLength = numRows * numCols;
  if (newLength > AllocSize) {
    delete x;
    AllocSize = std::max(newLength, AllocSize << 1);
    x         = new double[AllocSize];
  }
  NumRows = numRows;
  NumCols = numCols;
}

// Zero out the entire vector
inline void MatrixRmn::SetZero() {
  double *target = x;
  for (long i = NumRows * NumCols; i > 0; i--) {
    *(target++) = 0.0;
  }
}

// Return entry in row i and column j.
inline double MatrixRmn::Get(long i, long j) const {
  assert(i < NumRows && j < NumCols);
  return *(x + j * NumRows + i);
}

// Return a VectorR3 out of a column.  Starts at row 3*i, in column j.
inline void MatrixRmn::GetCouple(long i, long j, TPointD *retValue) const {
  assert(i < 0);  // messo perchè sono sicuro non entra mai in questa funzione!
                  // e quindi commento ->Load alla riga successiva
  long ii = 2 * i;
  assert(0 <= i && ii + 1 < NumRows && 0 <= j && j < NumCols);
  // retValue->Load( x+j*NumRows + ii );
}

// Get a pointer to the (0,0) entry.
// The entries are in column order.
// This version gives read-only pointer
inline const double *MatrixRmn::GetPtr() const { return x; }

// Get a pointer to the (0,0) entry.
// The entries are in column order.
inline double *MatrixRmn::GetPtr() { return x; }

// Get a pointer to the (i,j) entry.
// The entries are in column order.
// This version gives read-only pointer
inline const double *MatrixRmn::GetPtr(long i, long j) const {
  assert(0 <= i && i < NumRows && 0 <= j && j < NumCols);
  return (x + j * NumRows + i);
}

// Get a pointer to the (i,j) entry.
// The entries are in column order.
// This version gives pointer to writable data
inline double *MatrixRmn::GetPtr(long i, long j) {
  assert(i < NumRows && j < NumCols);
  return (x + j * NumRows + i);
}

// Get a pointer to the j-th column.
// The entries are in column order.
// This version gives read-only pointer
inline const double *MatrixRmn::GetColumnPtr(long j) const {
  assert(0 <= j && j < NumCols);
  return (x + j * NumRows);
}

// Get a pointer to the j-th column.
// This version gives pointer to writable data
inline double *MatrixRmn::GetColumnPtr(long j) {
  assert(0 <= j && j < NumCols);
  return (x + j * NumRows);
}

/// Get a pointer to the i-th row
// The entries are in column order.
// This version gives read-only pointer
inline const double *MatrixRmn::GetRowPtr(long i) const {
  assert(0 <= i && i < NumRows);
  return (x + i);
}

// Get a pointer to the i-th row
// This version gives pointer to writable data
inline double *MatrixRmn::GetRowPtr(long i) {
  assert(0 <= i && i < NumRows);
  return (x + i);
}

// Set the (i,j) entry of the matrix
inline void MatrixRmn::Set(long i, long j, double val) {
  assert(i < NumRows && j < NumCols);
  *(x + j * NumRows + i) = val;
}

// Set the i-th triple in the j-th column to u's three values
inline void MatrixRmn::SetCouple(long i, long j, const TPointD &u) {
  long ii = 2 * i;
  assert(0 <= i && ii + 1 < NumRows && 0 <= j && j < NumCols);
  // u.Dump( x+j*NumRows + ii );
  double *v = x + j * NumRows + ii;
  v[0]      = u.x;
  v[1]      = u.y;
}

// Set to be equal to the identity matrix
inline void MatrixRmn::SetIdentity() {
  assert(NumRows == NumCols);
  SetZero();
  SetDiagonalEntries(1.0);
}

inline MatrixRmn &MatrixRmn::operator*=(double mult) {
  double *aPtr = x;
  for (long i = NumRows * NumCols; i > 0; i--) {
    (*(aPtr++)) *= mult;
  }
  return (*this);
}

inline MatrixRmn &MatrixRmn::AddScaled(const MatrixRmn &B, double factor) {
  assert(NumRows == B.NumRows && NumCols == B.NumCols);
  double *aPtr = x;
  double *bPtr = B.x;
  for (long i = NumRows * NumCols; i > 0; i--) {
    (*(aPtr++)) += (*(bPtr++)) * factor;
  }
  return (*this);
}

inline MatrixRmn &MatrixRmn::operator+=(const MatrixRmn &B) {
  assert(NumRows == B.NumRows && NumCols == B.NumCols);
  double *aPtr = x;
  double *bPtr = B.x;
  for (long i = NumRows * NumCols; i > 0; i--) {
    (*(aPtr++)) += *(bPtr++);
  }
  return (*this);
}

inline MatrixRmn &MatrixRmn::operator-=(const MatrixRmn &B) {
  assert(NumRows == B.NumRows && NumCols == B.NumCols);
  double *aPtr = x;
  double *bPtr = B.x;
  for (long i = NumRows * NumCols; i > 0; i--) {
    (*(aPtr++)) -= *(bPtr++);
  }
  return (*this);
}

template <class T>
inline T Square(T x) {
  return (x * x);
}

inline double MatrixRmn::FrobeniusNormSq() const {
  double *aPtr  = x;
  double result = 0.0;
  for (long i = NumRows * NumCols; i > 0; i--) {
    result += Square(*(aPtr++));
  }
  return result;
}

// Helper routine to calculate dot product
inline double MatrixRmn::DotArray(long length, const double *ptrA, long strideA,
                                  const double *ptrB, long strideB) {
  double result = 0.0;
  for (; length > 0; length--) {
    result += (*ptrA) * (*ptrB);
    ptrA += strideA;
    ptrB += strideB;
  }
  return result;
}

// Helper routine: copies and scales an array (src and dest may be equal, or
// overlap)
inline void MatrixRmn::CopyArrayScale(long length, const double *from,
                                      long fromStride, double *to,
                                      long toStride, double scale) {
  for (; length > 0; length--) {
    *to = (*from) * scale;
    from += fromStride;
    to += toStride;
  }
}

// Helper routine: adds a scaled array
//	fromArray = toArray*scale.
inline void MatrixRmn::AddArrayScale(long length, const double *from,
                                     long fromStride, double *to, long toStride,
                                     double scale) {
  for (; length > 0; length--) {
    *to += (*from) * scale;
    from += fromStride;
    to += toStride;
  }
}

//=============================================================================

class DVAPI Jacobian {
public:
  enum UpdateMode {
    JACOB_Undefined         = 0,
    JACOB_JacobianTranspose = 1,
    JACOB_PseudoInverse     = 2,
    JACOB_DLS               = 3,
    JACOB_SDLS              = 4
  };

  Jacobian(IKSkeleton *skeleton, std::vector<TPointD> &target);

  void computeJacobian();

  // const MatrixRmn& ActiveJacobian() const { return *Jactive; }
  // void SetJendActive() { Jactive = &Jend; }

  void addTarget(TPointD targetPos) { target.push_back(targetPos); }
  void deletLastTarget() { target.pop_back(); }
  void setTarget(int i, TPointD targetPos) { target[i] = targetPos; }

  void ZeroDeltaThetas();
  void CalcDeltaThetasTranspose();
  void CalcDeltaThetasPseudoinverse();
  void CalcDeltaThetasDLS();
  void CalcDeltaThetasDLSwithSVD();
  void CalcDeltaThetasSDLS();

  void UpdateThetas();
  bool checkJointsLimit();

  void UpdatedSClampValue();
  void DrawEigenVectors() const;

  void SetCurrentMode(UpdateMode mode) { CurrentUpdateMode = mode; }
  UpdateMode GetCurrentMode() const { return CurrentUpdateMode; }
  void SetDampingDLS(double lambda) {
    DampingLambda   = lambda;
    DampingLambdaSq = lambda * lambda;
  }

  void Reset();

private:
  IKSkeleton *skeleton;  // skeleton associated with this Jacobian matrix
  std::vector<TPointD> target;
  int nEffector;  // Number of end effectors
  int nJoint;     // Number of Joints
  int nRow;       // matrix rows J(= 2 * number of end effectors)
  int nCol;       // number of columns of J

  MatrixRmn
      Jend;  // Jacobian matrix based on the positions of the end effectors
  MatrixRmn Jtarget;
  MatrixRmn Jnorms;  // Norms of 2-vectors in active Jacobian (solo SDLS)

  MatrixRmn U;  // J = U * Diag(w) * V^T	(SVD Singular Value
                // Decomposition)
  VectorRn w;
  MatrixRmn V;

  UpdateMode CurrentUpdateMode;

  VectorRn dS;  // delta s
  VectorRn dT;  // delta t
  VectorRn dSclamp;
  VectorRn dTheta;  // delta theta

  VectorRn dPreTheta;  // (vale solo per SDLS)

  // Parameters for pseudoinverse
  static const double PseudoInverseThresholdFactor;

  // Parameters for the Damped Least Squares method
  static const double DefaultDampingLambda;
  double DampingLambda;
  double DampingLambdaSq;
  VectorRn DampingLambdaSqV;
  VectorRn diagMatIdentity;
  // double DampingLambdaSDLS;

  static const double MaxAngleJtranspose;
  static const double MaxAnglePseudoinverse;
  static const double MaxAngleDLS;
  static const double MaxAngleSDLS;
  // MatrixRmn* Jactive;

  void CalcdTClampedFromdS();
  static const double BaseMaxTargetDist;
};

#endif  // JACOBIAN_H
