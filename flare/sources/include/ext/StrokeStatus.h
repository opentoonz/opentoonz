#pragma once

#ifndef STROKESTATUS_H
#define STROKESTATUS_H

/*****************************************************************************\
*                                                                             *
*                           Author Fabrizio Morciano                          *
*                                                                             *
\*****************************************************************************/

#include "tcommon.h"
#include "ext/CompositeStatus.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace ToonzExt {
class DVAPI StrokeStatus : public CompositeStatus {
  // cached information
  TStroke *stroke2change_;

  // stroke number in vimage
  unsigned int n_;

  // parameter of selected stroke
  double w_;

  // length of selected stroke
  double strokeLength_;

public:
  StrokeStatus();

  StrokeStatus(TStroke *stroke2change, unsigned int n, double w,
               double strokeLength);

  virtual ~StrokeStatus();

  void init();

  TStroke *getItself() const;
  void setItself(TStroke *);

  double getW() const;
  void setW(double w);

  unsigned int getId() const;

  void setId(unsigned int);

  double getLength() const;
  void setLength(double l);
};

//---------------------------------------------------------------------------
}
#endif /* STROKESTATUS_H */
