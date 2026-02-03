#pragma once

#ifndef TDOUBLEPARAMFILE_INCLUDED
#define TDOUBLEPARAMFILE_INCLUDED

#include "tdoublekeyframe.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TDoubleParamFileData {
  TDoubleKeyframe::FileParams m_params;
  std::vector<double> m_values;
  bool m_dirtyFlag;

  void read();

public:
  TDoubleParamFileData();
  ~TDoubleParamFileData();

  void setParams(const TDoubleKeyframe::FileParams &params);
  const TDoubleKeyframe::FileParams &getParams() { return m_params; }

  void invalidate();

  double getValue(double frame, double defaultValue = 0);
};

#endif
