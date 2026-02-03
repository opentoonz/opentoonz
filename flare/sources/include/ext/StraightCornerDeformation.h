#pragma once

#ifndef STRAIGHTCORNERDEFORMATION_H
#define STRAIGHTCORNERDEFORMATION_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

//#include "tcommon.h"
#include "tgeometry.h"

//#include "ext/TriParam.h"
#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "StrokeDeformationImpl.h"

namespace ToonzExt {
class DVAPI StraightCornerDeformation final : public StrokeDeformationImpl {
  StraightCornerDeformation();

public:
  virtual ~StraightCornerDeformation();

  bool check_(const ContextStatus *status) override;

  bool findExtremes_(const ContextStatus *, Interval &) override;

  void draw(Designer *) override;

  double findActionLength() override;

  static StraightCornerDeformation *instance();
};
}
#endif /* STRAIGHTCORNERDEFORMATION_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
