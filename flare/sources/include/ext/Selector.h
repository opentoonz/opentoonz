#pragma once

#ifndef SELECTOR_H
#define SELECTOR_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tgeometry.h"
#include "tstroke.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "ext/Types.h"
#include "ext/Designer.h"

namespace ToonzExt {
class DVAPI Selector {
  enum Selection { NONE = 0, POSITION = 1, LENGTH = 2 };

  Selection isSelected_;

  const TStroke *strokeRef_;

  double w_, height_, stroke_length_, original_stroke_length_, signum_,
      pixel_size_;

  Selection getSelection(const TPointD &pos);

  TPointD click_, curr_, prev_;

  TPointD range_;

  bool isVisible_;

  void init();

public:
  Selector(double stroke_length, double min_val, double max_val);
  virtual ~Selector();

  virtual void draw(Designer *designer);

  virtual void mouseDown(const TPointD &pos);

  virtual void mouseUp(const TPointD &pos);

  virtual void mouseMove(const TPointD &pos);

  virtual void mouseDrag(const TPointD &pos);

  void setStroke(const TStroke *ref);

  const TStroke *getStroke() const { return strokeRef_; }

  double getW() const { return w_; }

  double getLength() const;

  void setLength(double);

  bool isSelected() const { return (isVisible_ && (isSelected_ != NONE)); }

  void setVisibility(bool val) { isVisible_ = val; }

  bool isVisible() const { return isVisible_; }

  TPointD getUp() const;
};
}
#endif /* SELECTOR_H */
