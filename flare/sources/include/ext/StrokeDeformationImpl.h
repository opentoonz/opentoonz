#pragma once

#ifndef STROKEDEFORMATION_IMPL_H
#define STROKEDEFORMATION_IMPL_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "ext/StrokeDeformation.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "ExtUtil.h"

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
// to avoid annoying warning
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace ToonzExt {
class DVAPI StrokeDeformationImpl {
private:
  bool init(const ContextStatus *);

  bool computeStroke2Transform(const ContextStatus *s,
                               TStroke *&stroke2transform, double &w,
                               ToonzExt::Interval &extremes);

  ToonzExt::Potential *potential_;

  static TStroke
      *copyOfLastSelectedStroke_;  // deep copy stroke selected previously

  int cursorId_;

protected:
  TStroke *stroke2manipulate_;

  static TStroke *&getLastSelectedStroke();

  static void setLastSelectedStroke(TStroke *);

  static int &getLastSelectedDegree();

  static void setLastSelectedDegree(int degree);

  static ToonzExt::Intervals &getSpiresList();

  static ToonzExt::Intervals &getStraightsList();

  ToonzExt::StrokeParametricDeformer *deformer_;

  int shortcutKey_;

  double old_w0_;

  TPointD old_w0_pos_;

  // ref to simplify access
  std::vector<TStroke *> strokes_;

  StrokeDeformationImpl();

  /**
* Is this the correct deformation?
*/
  virtual bool check_(const ContextStatus *) = 0;

  /**
* Retrieve the nearest extremes to point selected.
*/
  virtual bool findExtremes_(const ContextStatus *, Interval &) = 0;

  virtual double findActionLength() = 0;

public:
  virtual ~StrokeDeformationImpl();

  ToonzExt::Interval getExtremes();

  static const ContextStatus *&getImplStatus();

  const TStroke *getStrokeSelected() const {
    if (getImplStatus()) return this->getImplStatus()->stroke2change_;
    return 0;
  }

  const TStroke *getCopiedStroke() const { return this->getStrokeSelected(); }

  /**
*Wrapper for activate.
*/
  virtual bool activate_impl(const ContextStatus *);

  /**
* Modify stroke.
*/
  virtual void update_impl(const TPointD &delta);

  /**
* Reduce control points and change stroke.
*/
  virtual TStroke *deactivate_impl();

  bool check(const ContextStatus *);

  /**
* Restore last good status.
*/
  virtual void reset();

  void setPotential(Potential *);

  /**
*Apply a designer on current deformation.
*/
  virtual void draw(Designer *);

  TStroke *getTransformedStroke();

  ToonzExt::Potential *getPotential();

  TPointD &oldW0();

  int getShortcutKey() const { return shortcutKey_; }

  int getCursorId() const { return cursorId_; }
  void setCursorId(int id) { cursorId_ = id; }
};
}

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(pop)
#endif

#endif /* STROKEDEFORMATION_IMPL_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
