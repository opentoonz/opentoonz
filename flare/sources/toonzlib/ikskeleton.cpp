

#include "toonz/ikskeleton.h"

void IKSkeleton::computeSkeleton(IKNode *node) {
  for (int i = 0; i < (int)m_nodes.size(); i++) {
    m_nodes[i]->computeS();
  }
}

void IKSkeleton::compute(void) {
  IKNode *root = this->getRoot();
  computeSkeleton(root);
}

void IKSkeleton::setPurpose(int nodeIndex, IKNode::Purpose purpose) {
  switch (purpose) {
  case IKNode::JOINT:
    m_nodes[nodeIndex]->setPurpose(purpose);
    break;
  case IKNode::EFFECTOR:
    if (m_nodes[nodeIndex]->getPurpose() != IKNode::EFFECTOR) {
      m_nodes[nodeIndex]->setPurpose(purpose);
      m_nodes[nodeIndex]->setSeqNumEffector(numEffector);
      numEffector++;
    }
    break;
  }
}

//========================================================================
