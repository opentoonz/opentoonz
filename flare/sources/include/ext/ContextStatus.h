#pragma once

#ifndef CONTEXTSTATUS_H
#define CONTEXTSTATUS_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"
//#include "tvectorimage.h"

#include "tstroke.h"

#include "ext/Types.h"

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
class StrokeParametricDeformer;
//---------------------------------------------------------------------------

/**
   * @brief This class maintains data required from manipulator.
   */
struct DVAPI ContextStatus {
  // useful for special key status
  enum { CTRL = 0x0001, ALT = 0x0002, SHIFT = 0x0004, NONE = 0x0000 };

  // cached information
  TStroke *stroke2change_;

  // parameter of selected stroke
  double w_;

  //  how much curve to move
  double lengthOfAction_;

  //  how much curve to move
  double deformerSensitivity_;

  // pixel size
  double pixelSize_;

  // degree of an angle to be a corner
  int cornerSize_;

  int key_event_;

  StrokeParametricDeformer *deformer_;

  /*
ToonzExt::Intervals*
spireCorners_;

ToonzExt::Intervals*
straightCorners_;
*/
  // select between manual or automatic mode
  bool isManual_;

  ContextStatus();
  ~ContextStatus();

  // not a deep copy is performed!!!
  ContextStatus(const ContextStatus &);

  // not a deep copy is performed!!!
  ContextStatus &operator=(const ContextStatus &);

  void init();
};
//---------------------------------------------------------------------------
}
#endif /* CONTEXTSTATUS_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
