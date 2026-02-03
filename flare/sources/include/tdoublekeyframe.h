#pragma once

#ifndef TDOUBLEKEYFRAME_INCLUDED
#define TDOUBLEKEYFRAME_INCLUDED

#include "tgeometry.h"
#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TOStream;
class TIStream;
class TUnit;

class DVAPI TDoubleKeyframe {
public:
  enum Type {
    None = 0,
    Constant,
    Linear,
    SpeedInOut,
    EaseInOut,
    EaseInOutPercentage,
    Exponential,
    Expression,
    File,
    SimilarShape
  };

  class DVAPI FileParams {
  public:
    TFilePath m_path;
    int m_fieldIndex;
    FileParams() : m_path(), m_fieldIndex(0) {}
  };

  static inline bool isKeyframeBased(int type) {
    return type < TDoubleKeyframe::Expression &&
           type != TDoubleKeyframe::SimilarShape;
  }

  // private:
  Type m_type;
  Type m_prevType;
  double m_frame;
  double m_value;
  bool m_isKeyframe;
  int m_step;
  TPointD m_speedIn, m_speedOut;

  bool m_linkedHandles;
  std::string m_expressionText;
  FileParams m_fileParams;
  std::string m_unitName;  // file/expression only
  double m_similarShapeOffset;

  void saveData(TOStream &os) const;
  void loadData(TIStream &is);

public:
  TDoubleKeyframe(double frame = 0, double value = 0);
  ~TDoubleKeyframe();

  bool operator<(const TDoubleKeyframe &k) const { return m_frame < k.m_frame; }
};

#endif
