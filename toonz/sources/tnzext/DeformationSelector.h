#pragma once

#ifndef DEFORMATIONSELECTOR_H
#define DEFORMATIONSELECTOR_H

#include "ext/StrokeDeformationImpl.h"

namespace ToonzExt {
/**
   * @brief This class is a Singleton that selects the correct
   *  StrokeDeformationImpl that will use StrokeDeformation.
   *
   * The selection use aspect of stroke (type of corner, smoothness,
   * ...) and priority of StrokeDeformationImpl itself.
   * Priority can manage simply some strange case (corner that
   * seems to be straight corner and smooth corner).
   *
   * @note Selection can be overriden by user.
   * @note Private use only.
   */
class DeformationSelector {
  typedef std::pair<StrokeDeformationImpl *, int> Deformation;
  std::vector<Deformation> ref_;

  DeformationSelector();
  DeformationSelector(const DeformationSelector &);
  DeformationSelector operator=(const DeformationSelector &);

public:
  ~DeformationSelector();

  static DeformationSelector *instance();

  void add(StrokeDeformationImpl *deformation, int priority);

  StrokeDeformationImpl *getDeformation(const ContextStatus *status);
};
}

/**
 * @brief This macro manage the registration in internal catalog
 * of deformations.
 */
#define REGISTER(myClass, priority)                                            \
  static bool add_in_array() {                                                 \
    DeformationSelector::instance()->add(myClass::instance(), priority);       \
    return true;                                                               \
  }                                                                            \
  static bool done = add_in_array();

#endif /* DEFORMATIONSELECTOR_H */

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
