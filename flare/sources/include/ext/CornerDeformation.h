#pragma once

#ifndef CORNERDEFORMATION_H
#define CORNERDEFORMATION_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"
//#include "tgeometry.h"

//#include "ext/TriParam.h"
#include "ext/StrokeDeformationImpl.h"

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

namespace ToonzExt {
class DVAPI CornerDeformation final : public StrokeDeformationImpl {
  CornerDeformation();

public:
  virtual ~CornerDeformation();

  bool check_(const ContextStatus *status) override;

  bool findExtremes_(const ContextStatus *, Interval &) override;

  double findActionLength() override;

  void draw(Designer *) override;

  static CornerDeformation *instance();
};
}
#endif /* CORNERDEFORMATION_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
