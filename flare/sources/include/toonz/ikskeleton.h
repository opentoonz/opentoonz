#pragma once

#ifndef IKSKELETON_H
#define IKSKELETON_H

#include "tgeometry.h"
#include "iknode.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI IKSkeleton {
  std::vector<IKNode *> m_nodes;
  int numEffector;
  int numJoint;

public:
  IKSkeleton() {
    numEffector = 0;
    numJoint    = 0;
  }
  ~IKSkeleton() { clear(); }

  void compute();
  void clear() {
    std::vector<IKNode *>::iterator it;
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) delete *it;
    m_nodes.clear();
    numEffector = 0;
    numJoint    = 0;
  }
  void addNode(IKNode *node) { m_nodes.push_back(node); }
  int getNodeCount() const { return (int)m_nodes.size(); }
  IKNode *getNode(int index) const {
    assert(0 <= index && index < getNodeCount());
    return m_nodes[index];
  }
  IKNode *getRoot() const { return m_nodes[0]; }

  int getNumNode() const { return (numJoint + numEffector); }
  int getNumEffector() const { return numEffector; }
  int getNumJoint() const { return numJoint; }

  void setRoot(int nodeIndex) {
    m_nodes[nodeIndex]->setR(m_nodes[nodeIndex]->getPos());
  }
  void setNode(int nodeIndex, const TPointD &pos, IKNode::Purpose purpose) {
    assert(0 <= nodeIndex && nodeIndex < getNodeCount());
    m_nodes[nodeIndex]->setPos(pos);
    m_nodes[nodeIndex]->setS(pos);
    m_nodes[nodeIndex]->setPurpose(purpose);
    m_nodes[nodeIndex]->unFreeze();
    m_nodes[nodeIndex]->setIndex(nodeIndex);
  }
  void setPurpose(int nodeIndex, IKNode::Purpose purpose);
  void setParent(int nodeIndex, int parentIndex) {
    assert(0 <= nodeIndex && nodeIndex < getNodeCount());
    assert(0 <= parentIndex && parentIndex < getNodeCount());
    m_nodes[nodeIndex]->setParent(m_nodes[parentIndex]);
    // Setto la posizione relativa
    m_nodes[nodeIndex]->setR(m_nodes[nodeIndex]->getPos() -
                             m_nodes[parentIndex]->getPos());
  }

  IKNode *getParent(const IKNode *node) {
    // assert(0<=nodeIndex && nodeIndex<getNodeCount());
    return node->getParent();
  }

private:
  void computeSkeleton(IKNode *);
};

#endif  // IKSKELETON_H
