#pragma once

#ifndef INTERFACESTATUS_H
#define INTERFACESTATUS_H

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
class DVAPI InterfaceStatus : public CompositeStatus {
  // From Client information
  TPointD curr_,  // current position of pointer
      prev_,      // previous position of pointer (drag)
      down_;      // position of mouse click
  //  how much curve to move
  double lengthOfAction_;

  //  how much curve to move
  double deformerSensibility_;

  // pixel size
  double pixelSize_;

  // degree of an angle to be a corner
  int cornerSize_;

public:
  InterfaceStatus();

  InterfaceStatus(const TPointD &curr, const TPointD &prev, const TPointD &down,
                  double lengthOfAction, double deformerSensibility,
                  double pixelSize, int cornerSize);

  virtual ~InterfaceStatus();

  void init();

  const TPointD &curr() const { return curr_; }
  TPointD &curr() { return curr_; }

  const TPointD &prev() const { return prev_; }
  TPointD &prev() { return prev_; }

  const TPointD &down() const { return down_; }
  TPointD &down() { return down_; }

  double getDeformationLength() const;

  void setDeformationLength(double val);

  double getSensibility() const;

  void setSensibility(double val);

  double getPixelSize() const;
  void setPixelSize(double val);

  int getCornerSize() const;
  void setCornerSize(int val);
};

//---------------------------------------------------------------------------
}
#endif /* INTERFACESTATUS_H */
