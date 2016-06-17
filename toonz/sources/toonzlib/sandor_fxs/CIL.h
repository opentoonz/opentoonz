#pragma once

// CIL.h: interface for the CCIL class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CIL_H__2B094D96_25D9_11D6_B9C6_0040F674BE6A__INCLUDED_)
#define AFX_CIL_H__2B094D96_25D9_11D6_B9C6_0040F674BE6A__INCLUDED_

#define MAXNBCI 4096  // 512

class CCIL {
  bool isRange(const char *s) const;
  int getRangeBegin(const char *s) const;
  int getRangeEnd(const char *s) const;
  void strToColorIndex(const char *s, CCIL &cil, const int maxIndex);

public:
  int m_nb;
  int m_ci[MAXNBCI];

  CCIL() : m_nb(0){};
  virtual ~CCIL() { m_nb = 0; };
  void set(const char *s, const int maxIndex);
  bool isIn(const int ci);
  void print();
};

#endif  // !defined(AFX_CIL_H__2B094D96_25D9_11D6_B9C6_0040F674BE6A__INCLUDED_)
