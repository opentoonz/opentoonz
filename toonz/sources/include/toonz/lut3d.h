#pragma once

#include "tcommon.h"

#include <QString>

#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// A platform-independent 3D LUT used by both display calibration and render
// effects. Values are sampled with trilinear interpolation.
class DVAPI Lut3D final {
  int m_meshSize = 0;
  std::vector<float> m_data;
  float m_domainMin[3] = {0.0f, 0.0f, 0.0f};
  float m_domainMax[3] = {1.0f, 1.0f, 1.0f};

public:
  bool load(const QString &path, QString *error = nullptr);
  bool isValid() const { return m_meshSize > 1 && !m_data.empty(); }
  int meshSize() const { return m_meshSize; }
  const float *data() const { return m_data.data(); }
  const float *domainMin() const { return m_domainMin; }
  const float *domainMax() const { return m_domainMax; }

  // Input is normalized through the LUT domain and clamped at its boundaries.
  // Output is left unclamped so callers can choose their output policy.
  void convert(float &r, float &g, float &b) const;
};
