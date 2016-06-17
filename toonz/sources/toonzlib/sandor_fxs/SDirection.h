#pragma once

// SDirection.h: interface for the CSDirection class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SDIRECTION_H__C672AFF1_1A65_11D6_B99E_0040F674BE6A__INCLUDED_)
#define AFX_SDIRECTION_H__C672AFF1_1A65_11D6_B99E_0040F674BE6A__INCLUDED_

#include "SDef.h"

#include <memory>
#include <array>
#include <vector>

//#define MSIZE 3

typedef struct {
  char val;
  UCHAR dir;
} SVD;

#define NBDIR 4

class CSDirection {
  int m_lX, m_lY;
  std::unique_ptr<UCHAR[]> m_dir;
  std::array<std::unique_ptr<SXYW[]>, NBDIR> m_df;
  int m_lDf;

  void null();
  void makeDir(UCHAR *sel);
  UCHAR getDir(const int xx, const int yy, UCHAR *sel);
  void makeDirFilter(const int sens);
  UCHAR equalizeDir_GTE50(UCHAR *sel, const int xx, const int yy, const int d);
  UCHAR equalizeDir_LT50(UCHAR *sel, const int xx, const int yy, const int d);
  void equalizeDir(UCHAR *sel, const int d);
  double adjustAngle(const short sum[4], const int Ima, const int Im45,
                     const int Ip45);
  double getAngle(const short sum[4], short ma);
  UCHAR blurRadius(UCHAR *sel, const int xx, const int yy, const int dBlur);
  void blurRadius(const int dBlur);
  void setDir01();
  bool isContourBorder(const int xx, const int yy, const int border);
  void setContourBorder(const int border);

public:
  CSDirection();
  CSDirection(const int lX, const int lY, const UCHAR *sel, const int sens);
  CSDirection(const int lX, const int lY, const UCHAR *sel, const int sens,
              const int border);
  virtual ~CSDirection();

  void doDir();
  void doRadius(const double rH, const double rLR, const double rV,
                const double rRL, const int dBlur);
  void getResult(UCHAR *sel);
};

#endif  // !defined(AFX_SDIRECTION_H__C672AFF1_1A65_11D6_B99E_0040F674BE6A__INCLUDED_)
