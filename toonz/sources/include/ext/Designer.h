#pragma once

#ifndef DESIGNER_H
#define DESIGNER_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"

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
class SmoothDeformation;
class CornerDeformation;
class StraightCornerDeformation;
class StrokeDeformation;
class Selector;

/**
   * @brief This class is a visitor to manage properly Draw method.
   */
class DVAPI Designer {
public:
  Designer();
  virtual ~Designer();
  /**
*/
  virtual void draw(SmoothDeformation *);

  /**
*/
  virtual void draw(CornerDeformation *);

  virtual void draw(StraightCornerDeformation *);

  virtual void draw(StrokeDeformation *);

  virtual void draw(Selector *);

  double getPixelSize2() const;
};
}
#endif  // !defined(DESIGNER_H)
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
