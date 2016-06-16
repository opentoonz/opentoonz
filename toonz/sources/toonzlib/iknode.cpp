

#include "toonz/iknode.h"

TPointD DVAPI rotatePoint(TPointD &point, double theta);

// Compute the global position of a single node
void IKNode::computeS(void) {
  IKNode *y = this->getParent();
  IKNode *w = this;

  s = r;  // Initialize to local (relative) position

  while (y) {
    s = rotatePoint(s, y->theta);
    y = y->getParent();
    w = w->getParent();
    s += w->r;
    m_pos = s;
  }
}

void IKNode::setPurpose(Purpose purpose) { m_purpose = purpose; }

TPointD rotatePoint(TPointD &point, double theta) {
  double costheta = cos(theta);
  double sintheta = sin(theta);
  double tempx    = point.x * costheta - point.y * sintheta;
  point.y         = point.y * costheta + point.x * sintheta;
  point.x         = tempx;
  return point;
}
