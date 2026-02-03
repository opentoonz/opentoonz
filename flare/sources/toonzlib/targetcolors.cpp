

#include "toonz/targetcolors.h"
#include <vector>

/*----------------------------------------------------------------------------*/

TargetColors::TargetColors() {}

/*----------------------------------------------------------------------------*/

TargetColors::~TargetColors() {}

/*----------------------------------------------------------------------------*/

int TargetColors::getColorCount() const { return m_colors.size(); }

/*----------------------------------------------------------------------------*/

const TargetColor &TargetColors::getColor(int i) const {
  assert(i >= 0);
  assert((unsigned int)i < m_colors.size());
  return m_colors[i];
}

/*----------------------------------------------------------------------------*/
