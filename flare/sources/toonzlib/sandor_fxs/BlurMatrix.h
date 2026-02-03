#pragma once

// BlurMatrix.h: interface for the CBlurMatrix class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BLURMATRIX_H__8298C171_0035_11D6_B94F_0040F674BE6A__INCLUDED_)
#define AFX_BLURMATRIX_H__8298C171_0035_11D6_B94F_0040F674BE6A__INCLUDED_

#include <vector>

#include "SError.h"
#include "SDef.h"

#define NBRS 10  //  Number of Random Samples
typedef std::vector<SXYD> BLURSECTION;

class CBlurMatrix {
public:
  bool m_isSAC;  // Stop At Contour
  bool m_isRS;   // Random Sampling
  std::vector<BLURSECTION> m_m[NBRS];

  CBlurMatrix() : m_isSAC(false), m_isRS(false){};
  CBlurMatrix(const CBlurMatrix &m);  // throw(SBlurMatrixError);
  CBlurMatrix(const double d, const int nb, const bool isSAC, const bool isRS);
  //	throw(SBlurMatrixError) ;
  virtual ~CBlurMatrix();

  void createRandom(const double d, const int nb);  // throw(SBlurMatrixError);
  void createEqual(const double d, const int nb);   // throw(SBlurMatrixError);
  void addPath(std::vector<BLURSECTION>::iterator pBS);  // throw(exception);
  void addPath();  // throw(SBlurMatrixError);
  void print() const;
  bool isIn(const std::vector<BLURSECTION> &m, const SXYD &xyd) const;
};

#endif  // !defined(AFX_BLURMATRIX_H__8298C171_0035_11D6_B94F_0040F674BE6A__INCLUDED_)
