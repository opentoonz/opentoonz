#pragma once

#ifndef IKENGINE_H
#define IKENGINE_H

#include "ikskeleton.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class Jacobian;

class DVAPI IKEngine {
  IKSkeleton m_skeleton;

public:
  IKEngine();

  // n.b. root index == 0
  void setRoot(const TPointD &pos);

  int addJoint(const TPointD &pos, int parentIndex);

  void lock(int index);
  void unlock(int index);

  bool isLocked(int index);

  void clear() {
    m_skeleton.clear();
    target.clear();
  }

  int getJointCount() const { return m_skeleton.getNodeCount(); }

  const TPointD getJoint(int index) const {
    assert(0 <= index && index < (int)m_skeleton.getNodeCount());
    TPointD jointPos = m_skeleton.getNode(index)->getPos();
    return jointPos;
  }

  int getJointParent(int index) const {
    assert(index > -1 && index < m_skeleton.getNodeCount());
    IKNode *node = m_skeleton.getNode(index)->getParent();
    return node ? node->getIndex() : -1;
  }

  double getJointAngle(int index);

  // trascino il punto index
  void drag(TPointD &pos);

private:
  std::vector<TPointD> target;
  void doUpdateStep(Jacobian &jacobian);
  void setSequenceJoints();
};

//#endif
#endif  // IKENGINE_H
