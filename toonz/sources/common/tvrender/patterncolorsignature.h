#pragma once

#include "tcolorfunctions.h"

#include <array>
#include <typeindex>

namespace PatternTextures {

// Parameters identify the state within a concrete color-function type.
struct ColorSignature {
  std::type_index type;
  std::array<double, 8> values;
  bool cacheable;

  explicit ColorSignature(const TColorFunction *function)
      : type(function ? typeid(*function) : typeid(void)) {
    TColorFunction::Parameters parameters;
    cacheable = !function || function->getParameters(parameters);
    values    = {parameters.m_mR, parameters.m_mG, parameters.m_mB,
                 parameters.m_mM, parameters.m_cR, parameters.m_cG,
                 parameters.m_cB, parameters.m_cM};
  }

  bool operator==(const ColorSignature &other) const {
    return cacheable && other.cacheable && type == other.type &&
           values == other.values;
  }
};

}  // namespace PatternTextures
