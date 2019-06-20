

// TnzCore includes
#include "tstream.h"
#include "tconvert.h"
#include "tsystem.h"
#include "tcubicbezier.h"
#include "tundo.h"

// TnzBase includes
#include "tdoublekeyframe.h"
#include "tdoubleparamfile.h"
#include "texpression.h"
#include "tgrammar.h"
#include "tparser.h"
#include "tunit.h"

// STD includes
#include <set>

#include "tdoubleparam.h"

using namespace std;

//===============================

class TActualDoubleKeyframe final : public TDoubleKeyframe {
public:
  mutable TExpression m_expression;
  mutable TDoubleParamFileData m_fileData;

  TActualDoubleKeyframe(double frame = 0, double value = 0)
      : TDoubleKeyframe(frame, value), m_unit(0) {}
  explicit TActualDoubleKeyframe(const TDoubleKeyframe &src) : m_unit(0) {
    TDoubleKeyframe::operator=(src);
    if (m_type == Expression || m_type == SimilarShape)
      m_expression.setText(m_expressionText);
    else if (m_type == File)
      m_fileData.setParams(m_fileParams);
  }
  TActualDoubleKeyframe &operator=(const TDoubleKeyframe &src) {
    TDoubleKeyframe::operator=(src);
    m_unit                   = 0;
    if (m_type == Expression || m_type == SimilarShape) {
      m_expression.setText(m_expressionText);
    } else if (m_type == File) {
      m_fileData.setParams(m_fileParams);
    }
    return *this;
  }

  const TUnit *updateUnit(const TMeasure *measure) {
    if (!measure) {
      m_unit     = 0;
      m_unitName = "";
    } else {
      if (m_unitName != "")
        m_unit = measure->getUnit(::to_wstring(m_unitName));
      else
        m_unit = 0;
      if (!m_unit) {
        m_unit = measure->getCurrentUnit();
        if (m_unit) {
          QString app = QString::fromStdWString(m_unit->getDefaultExtension());
          m_unitName  = app.toStdString();
        }
      }
    }
    assert(measure || m_unit == 0 && m_unitName == "");
    assert((m_unit == 0) == (m_unitName == ""));
    QString app = QString::fromStdString(m_unitName);
    assert(m_unit == 0 || m_unit->isExtension(app.toStdWString()));
    return m_unit;
  }

  double convertFrom(TMeasure *measure, double value) const {
    if (!m_unit) const_cast<TActualDoubleKeyframe *>(this)->updateUnit(measure);
    if (m_unit) value = m_unit->convertFrom(value);
    return value;
  }

private:
  mutable const TUnit *m_unit;
};

typedef vector<TActualDoubleKeyframe> DoubleKeyframeVector;

//===================================================================

inline double getConstantValue(const TActualDoubleKeyframe &k0,
                               const TActualDoubleKeyframe &k1, double f) {
  return (f == k1.m_frame) ? k1.m_value : k0.m_value;
}

//---------------------------------------------------------

inline double getLinearValue(const TActualDoubleKeyframe &k0,
                             const TActualDoubleKeyframe &k1, double f) {
  return k0.m_value +
         (f - k0.m_frame) * (k1.m_value - k0.m_value) /
             (k1.m_frame - k0.m_frame);
}

//---------------------------------------------------------

static void truncateSpeeds(double aFrame, double bFrame, TPointD &aSpeedTrunc,
                           TPointD &bSpeedTrunc) {
  double deltaX                        = bFrame - aFrame;
  if (aSpeedTrunc.x < 0) aSpeedTrunc.x = 0;
  if (bSpeedTrunc.x > 0) bSpeedTrunc.x = 0;

  if (aFrame + aSpeedTrunc.x > bFrame) {
    if (aSpeedTrunc.x != 0) {
      aSpeedTrunc = aSpeedTrunc * (deltaX / aSpeedTrunc.x);
    }
  }

  if (bFrame + bSpeedTrunc.x < aFrame) {
    if (bSpeedTrunc.x != 0) {
      bSpeedTrunc = -bSpeedTrunc * (deltaX / bSpeedTrunc.x);
    }
  }
}

//---------------------------------------------------------

inline double getSpeedInOutValue(const TActualDoubleKeyframe &k0,
                                 const TActualDoubleKeyframe &k1,
                                 const TPointD &speed0, const TPointD &speed1,
                                 double frame) {
  double aFrame = k0.m_frame;
  double bFrame = k1.m_frame;

  double aValue = k0.m_value;
  double bValue = k1.m_value;

  if (frame <= aFrame)
    return aValue;
  else if (frame >= bFrame)
    return bValue;

  TPointD aSpeedTrunc = speed0;
  TPointD bSpeedTrunc = speed1;
  truncateSpeeds(aFrame, bFrame, aSpeedTrunc, bSpeedTrunc);

  return getCubicBezierY(frame, TPointD(aFrame, aValue), aSpeedTrunc,
                         bSpeedTrunc, TPointD(bFrame, bValue));
}

//---------------------------------------------------------

DV_EXPORT_API void splitSpeedInOutSegment(TDoubleKeyframe &k,
                                          TDoubleKeyframe &k0,
                                          TDoubleKeyframe &k1) {
  if (k.m_frame <= k0.m_frame) {
    k = k0;
    return;
  } else if (k.m_frame >= k1.m_frame) {
    k = k1;
    return;
  }

  TPointD aSpeed = k0.m_speedOut;
  TPointD bSpeed = k1.m_speedIn;
  truncateSpeeds(k0.m_frame, k1.m_frame, aSpeed, bSpeed);

  TPointD p0(k0.m_frame, k0.m_value);
  TPointD p3(k1.m_frame, k1.m_value);
  TPointD p1 = p0 + aSpeed;
  TPointD p2 = p3 + bSpeed;
  double t   = invCubicBezierX(k.m_frame, p0, aSpeed, bSpeed, p3);
  t          = tcrop(t, 0.0, 1.0);

  TPointD p01 = (1 - t) * p0 + t * p1;
  TPointD p12 = (1 - t) * p1 + t * p2;
  TPointD p23 = (1 - t) * p2 + t * p3;

  TPointD p012 = (1 - t) * p01 + t * p12;
  TPointD p123 = (1 - t) * p12 + t * p23;

  TPointD p = (1 - t) * p012 + t * p123;

  assert(fabs(p.x - k.m_frame) < 0.1e-3);
  k.m_value = p.y;

  k0.m_speedOut = p01 - p0;
  k.m_speedIn   = p012 - p;
  k.m_speedOut  = p123 - p;
  k1.m_speedIn  = p23 - p3;
}

//---------------------------------------------------------

inline double getEaseInOutValue(const TActualDoubleKeyframe &k0,
                                const TActualDoubleKeyframe &k1, double frame,
                                bool percentage) {
  double x3 = k1.m_frame - k0.m_frame;
  if (x3 <= 0.0) return k0.m_value;
  double x = frame - k0.m_frame;
  if (x <= 0)
    return k0.m_value;
  else if (x >= x3)
    return k1.m_value;
  double e0 = std::max(k0.m_speedOut.x, 0.0);
  double e1 = std::max(-k1.m_speedIn.x, 0.0);
  if (percentage) {
    e0 *= x3 * 0.01;
    e1 *= x3 * 0.01;
  }
  if (e0 + e1 >= x3) {
    double x = tcrop((e0 + x3 - e1) / 2, 0.0, x3);
    e0       = x;
    e1       = x3 - x;
  }
  double x1 = e0, x2 = x3 - e1;
  if (0 < x1 - x2 && x1 - x2 < 0.1e-5)
    x1 = x2 = (x1 + x2) * 0.5;  // against rounding problems
  assert(0 <= x1 && x1 <= x2 && x2 <= x3);
  double v     = 2 / (x3 + x2 - x1);
  double value = 0;
  if (x < x1) {
    double a = v / e0;
    value    = 0.5 * a * x * x;
  } else if (x > x2) {
    double a = v / e1;
    value    = 1 - 0.5 * a * (x3 - x) * (x3 - x);
  } else {
    double c = -0.5 * v * e0;
    value    = x * v + c;
  }
  return (1 - value) * k0.m_value + value * k1.m_value;
}

//---------------------------------------------------------

inline double getExponentialValue(const TActualDoubleKeyframe &k0,
                                  const TActualDoubleKeyframe &k1,
                                  double frame) {
  double aFrame = k0.m_frame;
  double bFrame = k1.m_frame;
  double deltaX = bFrame - aFrame;

  double aValue = k0.m_value;
  double bValue = k1.m_value;

  // if min(aValue,bValue)<=0 => error => linear
  if (aValue <= 0 || bValue <= 0) return getLinearValue(k0, k1, frame);

  double t = (frame - aFrame) / deltaX;
  // if aValue<bValue then v = aValue * exp(t * log(bValue/aValue))
  // if bValue<aValue then v = bValue * exp((1-t) * log(aValue/bValue))
  if (bValue < aValue) {
    t = 1 - t;
    std::swap(aValue, bValue);
  }
  return aValue * exp(t * log(bValue / aValue));
}

//---------------------------------------------------------

inline double getExpressionValue(const TActualDoubleKeyframe &k0,
                                 const TActualDoubleKeyframe &k1, double frame,
                                 const TMeasure *measure) {
  double t = 0, rframe = frame - k0.m_frame;
  if (k1.m_frame > k0.m_frame) t  = rframe / (k1.m_frame - k0.m_frame);
  TSyntax::Calculator *calculator = k0.m_expression.getCalculator();
  if (calculator) {
    calculator->setUnit(
        (const_cast<TActualDoubleKeyframe &>(k0)).updateUnit(measure));
    return calculator->compute(t, frame + 1, rframe + 1);
  } else if (measure)
    return measure->getDefaultValue();
  else
    return 0;
}

//---------------------------------------------------------

inline double getSimilarShapeValue(const TActualDoubleKeyframe &k0,
                                   const TActualDoubleKeyframe &k1,
                                   double frame, const TMeasure *measure) {
  double offset = k0.m_similarShapeOffset;
  double rv0    = getExpressionValue(k0, k1, k0.m_frame + offset, measure);
  double rv1    = getExpressionValue(k0, k1, k1.m_frame + offset, measure);
  double rv     = getExpressionValue(k0, k1, frame + offset, measure);
  double v0     = k0.m_value;
  double v1     = k1.m_value;
  if (rv1 != rv0)
    return v0 + (v1 - v0) * (rv - rv0) / (rv1 - rv0);
  else if (measure)
    return measure->getDefaultValue();
  else
    return 0;
}

//===================================================================

class TDoubleParam::Imp {
public:
  const TSyntax::Grammar *m_grammar;
  string m_measureName;
  TMeasure *m_measure;
  double m_defaultValue, m_minValue, m_maxValue;
  DoubleKeyframeVector m_keyframes;
  bool m_cycleEnabled;

  std::set<TParamObserver *> m_observers;

  Imp(double v = 0.0)
      : m_grammar(0)
      , m_measureName()
      , m_measure(0)
      , m_defaultValue(v)
      , m_minValue(-(std::numeric_limits<double>::max)())
      , m_maxValue((std::numeric_limits<double>::max)())
      , m_cycleEnabled(false) {}

  ~Imp() {}

  void copy(std::unique_ptr<Imp> const &src) {
    m_grammar      = src->m_grammar;
    m_measureName  = src->m_measureName;
    m_measure      = src->m_measure;
    m_defaultValue = src->m_defaultValue;
    m_minValue     = src->m_minValue;
    m_maxValue     = src->m_maxValue;
    m_keyframes    = src->m_keyframes;
    m_cycleEnabled = src->m_cycleEnabled;
  }

  void notify(const TParamChange &change) {
    std::set<TParamObserver *>::iterator it = m_observers.begin();
    for (; it != m_observers.end(); ++it) (*it)->onChange(change);
  }

  double getValue(int segmentIndex, double frame);
  double getSpeed(int segmentIndex, double frame);
  TPointD getSpeedIn(int kIndex);
  TPointD getSpeedOut(int kIndex);
};

//---------------------------------------------------------

double TDoubleParam::Imp::getValue(int segmentIndex, double frame) {
  assert(0 <= segmentIndex && segmentIndex + 1 < (int)m_keyframes.size());
  const TActualDoubleKeyframe &k0 = m_keyframes[segmentIndex];
  const TActualDoubleKeyframe &k1 = m_keyframes[segmentIndex + 1];

  double value     = m_defaultValue;
  bool convertUnit = false;
  switch (k0.m_type) {
  case TDoubleKeyframe::Constant:
    value = getConstantValue(k0, k1, frame);
    break;
  case TDoubleKeyframe::Linear:
    value = getLinearValue(k0, k1, frame);
    break;
  case TDoubleKeyframe::SpeedInOut:
    value = getSpeedInOutValue(k0, k1, getSpeedOut(segmentIndex),
                               getSpeedIn(segmentIndex + 1), frame);
    break;
  case TDoubleKeyframe::EaseInOut:
    value = getEaseInOutValue(k0, k1, frame, false);
    break;
  case TDoubleKeyframe::EaseInOutPercentage:
    value = getEaseInOutValue(k0, k1, frame, true);
    break;
  case TDoubleKeyframe::Exponential:
    value = getExponentialValue(k0, k1, frame);
    break;
  case TDoubleKeyframe::Expression:
    value       = getExpressionValue(k0, k1, frame, m_measure);
    convertUnit = true;
    break;
  case TDoubleKeyframe::File:
    value       = k0.m_fileData.getValue(frame, m_defaultValue);
    convertUnit = true;
    break;
  case TDoubleKeyframe::SimilarShape:
    value = getSimilarShapeValue(k0, k1, frame, m_measure);
    break;
  }
  if (convertUnit) value = k0.convertFrom(m_measure, value);
  return value;
}

//---------------------------------------------------------

double TDoubleParam::Imp::getSpeed(int segmentIndex, double frame) {
  const double h = 0.00001;
  return (getValue(segmentIndex, frame + h) -
          getValue(segmentIndex, frame - h)) /
         (2 * h);
}

//---------------------------------------------------------

TPointD TDoubleParam::Imp::getSpeedIn(int kIndex) {
  assert(1 <= kIndex && kIndex < (int)m_keyframes.size());
  const TActualDoubleKeyframe &kf0 = m_keyframes[kIndex - 1];
  const TActualDoubleKeyframe &kf1 = m_keyframes[kIndex];
  assert(kf0.m_type == TDoubleKeyframe::SpeedInOut);
  if (!kf1.m_linkedHandles) return kf1.m_speedIn;
  TPointD speedIn = kf1.m_speedIn;
  if (kIndex + 1 >= (int)m_keyframes.size()) {
    // speedIn.y = 0;
  } else {
    if (kf1.m_type != TDoubleKeyframe::SpeedInOut &&
        (kf1.m_type != TDoubleKeyframe::Expression ||
         !kf1.m_expression.isCycling())) {
      double speed = getSpeed(kIndex, kf1.m_frame);
      speedIn.y    = speedIn.x * speed;
    }
  }
  return speedIn;
}

//---------------------------------------------------------

TPointD TDoubleParam::Imp::getSpeedOut(int kIndex) {
  assert(0 <= kIndex && kIndex < (int)m_keyframes.size());
  const TDoubleKeyframe &kf1 = m_keyframes[kIndex];
  assert(kf1.m_type == TDoubleKeyframe::SpeedInOut);
  if (!kf1.m_linkedHandles) return kf1.m_speedOut;
  TPointD speedOut = kf1.m_speedOut;
  if (kIndex == 0) {
    // speedOut.y = 0;
  } else {
    const TDoubleKeyframe &kf0 = m_keyframes[kIndex - 1];
    if (kf0.m_type != TDoubleKeyframe::SpeedInOut) {
      double speed = getSpeed(kIndex - 1, kf1.m_frame);
      speedOut.y   = speedOut.x * speed;
    }
  }
  return speedOut;
}

//---------------------------------------------------------

TPointD TDoubleParam::getSpeedIn(int kIndex) const {
  return m_imp->getSpeedIn(kIndex);
}
TPointD TDoubleParam::getSpeedOut(int kIndex) const {
  return m_imp->getSpeedOut(kIndex);
}

//=========================================================

PERSIST_IDENTIFIER(TDoubleParam, "doubleParam")

//---------------------------------------------------------

TDoubleParam::TDoubleParam(double v) : m_imp(new TDoubleParam::Imp(v)) {}

//---------------------------------------------------------

TDoubleParam::TDoubleParam(const TDoubleParam &src)
    : TParam(src.getName()), m_imp(new TDoubleParam::Imp()) {
  m_imp->copy(src.m_imp);
}

//---------------------------------------------------------

TDoubleParam::~TDoubleParam() {}

//---------------------------------------------------------

TDoubleParam &TDoubleParam::operator=(const TDoubleParam &dp) {
  setName(dp.getName());
  m_imp->copy(dp.m_imp);
  return *this;
}

//---------------------------------------------------------

void TDoubleParam::copy(TParam *src) {
  TDoubleParam *p = dynamic_cast<TDoubleParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_imp->copy(p->m_imp);

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));
}

//---------------------------------------------------------

void TDoubleParam::setValueRange(double min, double max, double step) {
  if (min > max) min = max;

  m_imp->m_minValue = min;
  m_imp->m_maxValue = max;
  // m_imp->m_valueStep = step;
}

//---------------------------------------------------------

bool TDoubleParam::getValueRange(double &min, double &max, double &step) const {
  min  = m_imp->m_minValue;
  max  = m_imp->m_maxValue;
  step = 1;  // m_imp->m_valueStep;
  return min < max;
}

//---------------------------------------------------------

double TDoubleParam::getDefaultValue() const {
  assert(m_imp);
  return m_imp->m_defaultValue;
}

//---------------------------------------------------------

void TDoubleParam::enableCycle(bool enabled) {
  m_imp->m_cycleEnabled = enabled;

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));
}

//---------------------------------------------------------

bool TDoubleParam::isCycleEnabled() const { return m_imp->m_cycleEnabled; }

//=========================================================

//=========================================================

double TDoubleParam::getValue(double frame, bool leftmost) const {
  double value = 0;
  assert(m_imp);
  const DoubleKeyframeVector &keyframes = m_imp->m_keyframes;
  if (keyframes.empty()) {
    // no keyframes: return the default value
    value = m_imp->m_defaultValue;
  } else if (keyframes.size() == 1) {
    // a single keyframe. Type must be keyframe based (no expression/file)
    value = keyframes[0].m_value;
  } else {
    // keyframes range is [f0,f1]
    double f0 = keyframes.begin()->m_frame;
    double f1 = keyframes.back().m_frame;
    if (frame < f0)
      frame = f0;
    else if (frame > f1 && !m_imp->m_cycleEnabled)
      frame            = f1;
    double valueOffset = 0;

    if (m_imp->m_cycleEnabled) {
      double dist   = (f1 - f0);
      double dvalue = keyframes.back().m_value - keyframes.begin()->m_value;
      while (frame >= f1) {
        if (frame != f1 || !leftmost) {
          frame -= dist;
          valueOffset += dvalue;
        } else
          break;
      }
    }

    // frame is in [f0,f1]
    assert(f0 <= frame && frame <= f1);

    DoubleKeyframeVector::const_iterator b;
    b = std::lower_bound(keyframes.begin(), keyframes.end(),
                         TDoubleKeyframe(frame));
    assert(b != keyframes.end());
    DoubleKeyframeVector::const_iterator a;
    if (b->m_frame == frame && (b + 1) != keyframes.end()) {
      a = b;
      b++;
    } else {
      assert(b != keyframes.begin());
      a = b - 1;
    }

    if (leftmost && frame - a->m_frame < 0.00001 && a != keyframes.begin()) {
      a--;
      b--;
    }

    // segment (a,b) contains frame
    assert(a != keyframes.end());
    assert(b != keyframes.end());
    assert(a->m_frame <= frame);
    assert(b->m_frame >= frame);

    int kIndex = std::distance(keyframes.begin(), a);

    vector<TActualDoubleKeyframe> tmpKeyframe(3);

    // if segment is keyframe based ....
    if (TDoubleKeyframe::isKeyframeBased(a->m_type)) {
      // .. and next segment is not then update the b value
      if ((b + 1) != keyframes.end() &&
          !TDoubleKeyframe::isKeyframeBased(b->m_type)) {
        tmpKeyframe[0] = *b;
        if (b->m_type != TDoubleKeyframe::Expression ||
            !b->m_expression.isCycling())
          tmpKeyframe[0].m_value = getValue(b->m_frame);
        b                        = tmpKeyframe.begin();
      }
      // .. and/or if prev segment is not then update the a value
      if (a != keyframes.begin() &&
          !TDoubleKeyframe::isKeyframeBased(a[-1].m_type)) {
        tmpKeyframe[1]         = *a;
        tmpKeyframe[1].m_value = getValue(a->m_frame, true);
        a                      = tmpKeyframe.begin() + 1;
      }
    }

    if (a->m_step > 1) {
      tmpKeyframe[2] = *b;
      b              = tmpKeyframe.begin() + 2;

      int relPos = tfloor(b->m_frame - a->m_frame),
          step   = std::min(a->m_step, relPos);

      tmpKeyframe[2].m_frame        = a->m_frame + tfloor(relPos, step);
      if (frame > b->m_frame) frame = b->m_frame;

      frame = a->m_frame + tfloor(tfloor(frame - a->m_frame), step);
    }

    assert(0 <= kIndex && kIndex + 1 < (int)m_imp->m_keyframes.size());
    value            = m_imp->m_defaultValue;
    bool convertUnit = false;
    switch (a->m_type) {
    case TDoubleKeyframe::Constant:
      value = getConstantValue(*a, *b, frame);
      break;
    case TDoubleKeyframe::Linear:
      value = getLinearValue(*a, *b, frame);
      break;
    case TDoubleKeyframe::SpeedInOut:
      value = getSpeedInOutValue(*a, *b, getSpeedOut(kIndex),
                                 getSpeedIn(kIndex + 1), frame);
      break;
    case TDoubleKeyframe::EaseInOut:
      value = getEaseInOutValue(*a, *b, frame, false);
      break;
    case TDoubleKeyframe::EaseInOutPercentage:
      value = getEaseInOutValue(*a, *b, frame, true);
      break;
    case TDoubleKeyframe::Exponential:
      value = getExponentialValue(*a, *b, frame);
      break;
    case TDoubleKeyframe::Expression:
      value       = getExpressionValue(*a, *b, frame, m_imp->m_measure);
      convertUnit = true;
      break;
    case TDoubleKeyframe::File:
      value       = a->m_fileData.getValue(frame, m_imp->m_defaultValue);
      convertUnit = true;
      break;
    case TDoubleKeyframe::SimilarShape:
      value = getSimilarShapeValue(*a, *b, frame, m_imp->m_measure);
      // convertUnit = true;
      break;

    default:
      value = 0.0;
    }
    value += valueOffset;
    if (convertUnit) value = a->convertFrom(m_imp->m_measure, value);
  }

  // if (cropped)
  //  value = tcrop(value, m_imp->m_minValue, m_imp->m_maxValue);
  return value;
}

//---------------------------------------------------------

bool TDoubleParam::setValue(double frame, double value) {
  assert(m_imp);
  DoubleKeyframeVector &keyframes = m_imp->m_keyframes;
  DoubleKeyframeVector::iterator it;
  TActualDoubleKeyframe k(frame);
  it           = std::lower_bound(keyframes.begin(), keyframes.end(), k);
  int index    = 0;
  bool created = false;
  /*-- キーフレームが見つかった場合 --*/
  if (it != keyframes.end() && it->m_frame == frame) {
    // changing a keyframe value
    index                             = std::distance(keyframes.begin(), it);
    TActualDoubleKeyframe oldKeyframe = *it;
    if (oldKeyframe.m_type == TDoubleKeyframe::Expression ||
        oldKeyframe.m_type == TDoubleKeyframe::File)
      return false;

    it->m_value = value;

    m_imp->notify(TParamChange(this, 0, 0, true, false, false));
  }
  /*-- セグメントの部分なので、新たにキーフレームを作る --*/
  else {
    assert(it == keyframes.end() || it->m_frame > frame);

    // can't change value in a file/expression segment
    if (it != keyframes.end() && it > keyframes.begin() &&
        ((it - 1)->m_type == TDoubleKeyframe::Expression ||
         (it - 1)->m_type == TDoubleKeyframe::File))
      return false;

    // inserting a new keyframe
    k.m_value      = value;
    k.m_isKeyframe = true;
    k.m_expression.setGrammar(m_imp->m_grammar);
    k.m_expression.setOwnerParameter(this);
    it = keyframes.insert(it, k);
    if (it == keyframes.begin())
      it->m_prevType = TDoubleKeyframe::None;
    else {
      it->m_prevType = it[-1].m_type;
      /*-- FxGuiでSegment内にKeyを打った場合は、Step値も引き継ぐ --*/
      it->m_step = it[-1].m_step;
    }
    if (it + 1 != keyframes.end()) it[1].m_prevType = it->m_type;

    index = std::distance(keyframes.begin(), it);

    m_imp->notify(TParamChange(this, 0, 0, true, false, false));
    created = true;
  }
  assert(0 == index || keyframes[index - 1].m_frame < keyframes[index].m_frame);
  assert(getKeyframeCount() - 1 == index ||
         keyframes[index + 1].m_frame > keyframes[index].m_frame);

  return created;
}

//---------------------------------------------------------

void TDoubleParam::setKeyframe(int index, const TDoubleKeyframe &k) {
  DoubleKeyframeVector &keyframes = m_imp->m_keyframes;
  assert(0 <= index && index < (int)keyframes.size());

  TActualDoubleKeyframe &dst        = keyframes[index];
  TActualDoubleKeyframe oldKeyframe = dst;

  (TDoubleKeyframe &)dst = k;
  dst.updateUnit(m_imp->m_measure);

  if (dst.m_type == TDoubleKeyframe::Expression ||
      dst.m_type == TDoubleKeyframe::SimilarShape)
    dst.m_expression.setText(dst.m_expressionText);
  if (dst.m_type == TDoubleKeyframe::File)
    dst.m_fileData.setParams(dst.m_fileParams);

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));

  assert(0 == index || keyframes[index - 1].m_frame < keyframes[index].m_frame);
  assert(getKeyframeCount() - 1 == index ||
         keyframes[index + 1].m_frame > keyframes[index].m_frame);
  if (index == 0)
    dst.m_prevType = TDoubleKeyframe::None;
  else
    dst.m_prevType = keyframes[index - 1].m_type;
}

//---------------------------------------------------------

void TDoubleParam::setKeyframes(const std::map<int, TDoubleKeyframe> &ks) {
  DoubleKeyframeVector &keyframes = m_imp->m_keyframes;

  std::map<int, TDoubleKeyframe>::const_iterator it;
  for (it = ks.begin(); it != ks.end(); ++it) {
    int index = it->first;
    assert(0 <= index && index < (int)keyframes.size());

    TActualDoubleKeyframe oldKeyframe = keyframes[index];
    TActualDoubleKeyframe &dst        = keyframes[index];

    (TDoubleKeyframe &)dst = it->second;
    dst.updateUnit(m_imp->m_measure);

    if (dst.m_type == TDoubleKeyframe::Expression ||
        dst.m_type == TDoubleKeyframe::SimilarShape)
      dst.m_expression.setText(dst.m_expressionText);
    if (dst.m_type == TDoubleKeyframe::File)
      dst.m_fileData.setParams(dst.m_fileParams);
  }
  if (!keyframes.empty()) {
    keyframes[0].m_prevType = TDoubleKeyframe::None;
    for (int i                = 1; i < (int)keyframes.size(); i++)
      keyframes[i].m_prevType = keyframes[i - 1].m_type;
  }

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));

#ifndef NDEBUG
  for (int i = 0; i + 1 < (int)keyframes.size(); i++) {
    assert(keyframes[i].m_frame <= keyframes[i + 1].m_frame);
  }
#endif
}

//---------------------------------------------------------

void TDoubleParam::setKeyframe(const TDoubleKeyframe &k) {
  DoubleKeyframeVector &keyframes = m_imp->m_keyframes;
  DoubleKeyframeVector::iterator it;
  it = std::lower_bound(keyframes.begin(), keyframes.end(), k);
  if (it != keyframes.end() && it->m_frame == k.m_frame) {
    // int index = std::distance(keyframes.begin(), it);
    TActualDoubleKeyframe &dst = *it;
    (TDoubleKeyframe &)dst     = k;
    dst.updateUnit(m_imp->m_measure);
  } else {
    it = keyframes.insert(it, TActualDoubleKeyframe(k));
    // int index = std::distance(keyframes.begin(), it);
    // TDoubleKeyframe oldKeyframe = *it;
    it->m_expression.setGrammar(m_imp->m_grammar);
    it->m_expression.setOwnerParameter(this);
    it->updateUnit(m_imp->m_measure);
  }
  it->m_isKeyframe = true;

  if (it->m_type == TDoubleKeyframe::Expression)
    it->m_expression.setText(it->m_expressionText);

  if (it->m_type == TDoubleKeyframe::File)
    it->m_fileData.setParams(it->m_fileParams);

  if (it == keyframes.begin())
    it->m_prevType = TDoubleKeyframe::None;
  else
    it->m_prevType = it[-1].m_type;

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));

  assert(it == keyframes.begin() || (it - 1)->m_frame < it->m_frame);
  assert(it + 1 == keyframes.end() || (it + 1)->m_frame > it->m_frame);
}

//---------------------------------------------------------

void TDoubleParam::setDefaultValue(double value) {
  assert(m_imp);
  if (m_imp->m_defaultValue != value) {
    m_imp->m_defaultValue = value;

    // gmt, 19-6-2010; needed to get a notification in the FxParamsGraphicEditor
    // (Camera Stand)
    // when a param (without keyframes) is changed
    m_imp->notify(TParamChange(this, 0, 0, true, false, false));
  }
}

//---------------------------------------------------------

bool TDoubleParam::isDefault() const {
  return m_imp->m_keyframes.empty() &&
         // m_imp->m_type == Keyframes &&
         m_imp->m_defaultValue == 0;
}

//---------------------------------------------------------

int TDoubleParam::getKeyframeCount() const { return m_imp->m_keyframes.size(); }

//---------------------------------------------------------

void TDoubleParam::getKeyframes(std::set<double> &frames) const {
  for (DoubleKeyframeVector::iterator it = m_imp->m_keyframes.begin();
       it != m_imp->m_keyframes.end(); ++it)
    frames.insert(it->m_frame);
}

//---------------------------------------------------------

bool TDoubleParam::hasKeyframes() const { return !m_imp->m_keyframes.empty(); }

//---------------------------------------------------------

const TDoubleKeyframe &TDoubleParam::getKeyframe(int index) const {
  assert(0 <= index && index < (int)m_imp->m_keyframes.size());
  return m_imp->m_keyframes[index];
}

//---------------------------------------------------------

const TDoubleKeyframe &TDoubleParam::getKeyframeAt(double frame) const {
  static TDoubleKeyframe k;
  k     = TDoubleKeyframe();
  int i = 0;
  for (i = 0; i < (int)m_imp->m_keyframes.size(); i++)
    if (m_imp->m_keyframes[i].m_frame >= frame) break;
  if (i < (int)m_imp->m_keyframes.size() &&
      m_imp->m_keyframes[i].m_frame == frame) {
    k = m_imp->m_keyframes[i];
    return k;
  }
  k.m_frame      = frame;
  k.m_value      = getValue(frame);
  k.m_isKeyframe = false;
  return k;
}

//---------------------------------------------------------

bool TDoubleParam::isKeyframe(double frame) const {
  return std::binary_search(m_imp->m_keyframes.begin(),
                            m_imp->m_keyframes.end(), TDoubleKeyframe(frame));
}

//---------------------------------------------------------

void TDoubleParam::deleteKeyframe(double frame) {
  DoubleKeyframeVector &keyframes = m_imp->m_keyframes;
  DoubleKeyframeVector::iterator it;
  it = std::lower_bound(keyframes.begin(), keyframes.end(),
                        TDoubleKeyframe(frame));
  if (it == keyframes.end() || it->m_frame != frame) return;

  TDoubleKeyframe::Type type                = it->m_prevType;
  it                                        = m_imp->m_keyframes.erase(it);
  if (it != keyframes.end()) it->m_prevType = type;

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));
}

//---------------------------------------------------------

void TDoubleParam::clearKeyframes() {
  m_imp->m_keyframes.clear();
  m_imp->notify(TParamChange(this, 0, 0, true, false, false));
}

//---------------------------------------------------------

void TDoubleParam::assignKeyframe(double frame, const TParamP &src,
                                  double srcFrame, bool changedOnly) {
  TDoubleParamP dp = src;
  if (!dp) return;
  double value = dp->getValue(srcFrame);
  if (!changedOnly || getValue(frame) != value) setValue(frame, value);
}

//---------------------------------------------------------

int TDoubleParam::getClosestKeyframe(double frame) const {
  DoubleKeyframeVector &keyframes = m_imp->m_keyframes;
  typedef DoubleKeyframeVector::iterator Iterator;
  Iterator it = std::lower_bound(keyframes.begin(), keyframes.end(),
                                 TDoubleKeyframe(frame));
  if (it == keyframes.end()) {
    // frame > k.m_frame per qualsiasi k.
    // ritorna l'indice dell'ultimo keyframe (o -1 se l'array e' vuoto)
    return keyframes.size() - 1;
  } else {
    int index = std::distance<Iterator>(keyframes.begin(), it);
    if (it->m_frame == frame || index == 0)
      return index;
    else {
      double nextFrame = it->m_frame;
      double prevFrame = keyframes[index - 1].m_frame;
      assert(prevFrame < frame && frame < nextFrame);
      return (nextFrame - frame < frame - prevFrame) ? index : index - 1;
    }
  }
}

//---------------------------------------------------------

int TDoubleParam::getNextKeyframe(double frame) const {
  TDoubleKeyframe k(frame);
  typedef DoubleKeyframeVector::const_iterator Iterator;
  Iterator it =
      std::upper_bound(m_imp->m_keyframes.begin(), m_imp->m_keyframes.end(), k);
  if (it == m_imp->m_keyframes.end()) return -1;
  int index = std::distance<Iterator>(m_imp->m_keyframes.begin(), it);
  if (it->m_frame == frame) {
    ++index;
    return index < getKeyframeCount() ? index : -1;
  }
  return index;
}

//---------------------------------------------------------

int TDoubleParam::getPrevKeyframe(double frame) const {
  TDoubleKeyframe k(frame);
  typedef DoubleKeyframeVector::const_iterator Iterator;
  Iterator it =
      std::lower_bound(m_imp->m_keyframes.begin(), m_imp->m_keyframes.end(), k);
  if (it == m_imp->m_keyframes.end()) return m_imp->m_keyframes.size() - 1;
  int index = std::distance<Iterator>(m_imp->m_keyframes.begin(), it);
  return index - 1;
}

//---------------------------------------------------------

double TDoubleParam::keyframeIndexToFrame(int index) const {
  assert(0 <= index && index < getKeyframeCount());
  return getKeyframe(index).m_frame;
}

//---------------------------------------------------------

void TDoubleParam::addObserver(TParamObserver *observer) {
  m_imp->m_observers.insert(observer);
}

//---------------------------------------------------------

void TDoubleParam::removeObserver(TParamObserver *observer) {
  m_imp->m_observers.erase(observer);
}

//---------------------------------------------------------

const std::set<TParamObserver *> &TDoubleParam::observers() const {
  return m_imp->m_observers;
}

//---------------------------------------------------------

void TDoubleParam::loadData(TIStream &is) {
  string tagName;
  /*
if(is.matchTag(tagName))
{
// nuovo formato (29 agosto 2005)
if(tagName!="type") throw TException("expected <type>");
int type = 0;
is >> type;
// m_imp->m_type = TDoubleParam::Type(type);
if(!is.matchEndTag()) throw TException(tagName + " : missing endtag");


//if(!is.matchTag(tagName) || tagName != "default") throw TException("expected
<default>");
//is >> m_imp->m_defaultValue;
//if(!is.matchEndTag()) throw TException(tagName + " : missing endtag");
//if(!is.matchTag(tagName) || tagName != "default") throw TException("expected
<default>");


}
else
{
// vecchio formato
is >> m_imp->m_defaultValue;
}
*/

  m_imp->m_keyframes.clear();
  int oldType = -1;
  while (is.matchTag(tagName)) {
    if (tagName == "type") {
      // (old) format 5.2. Since 6.0 param type is no used anymore
      is >> oldType;
    } else if (tagName == "default") {
      is >> m_imp->m_defaultValue;
    } else if (tagName == "cycle") {
      string dummy;
      is >> dummy;
      m_imp->m_cycleEnabled = true;
      // setExtrapolationAfter(Loop);
    } else if (tagName == "k") {
      // vecchio formato
      if (oldType != 0) continue;
      int linkStatus = 0;
      TActualDoubleKeyframe k;
      is >> k.m_frame >> k.m_value >> k.m_speedIn.x >> k.m_speedIn.y >>
          k.m_speedOut.x >> k.m_speedOut.y >> linkStatus;

      k.m_isKeyframe    = true;
      k.m_linkedHandles = (linkStatus & 1) != 0;

      if ((linkStatus & 1) != 0)
        k.m_type = TDoubleKeyframe::SpeedInOut;
      else if ((linkStatus & 2) != 0 || (linkStatus & 4) != 0)
        k.m_type = TDoubleKeyframe::EaseInOut;
      else
        k.m_type = TDoubleKeyframe::Linear;
      k.m_expression.setGrammar(m_imp->m_grammar);
      k.m_expression.setOwnerParameter(this);
      k.m_prevType = m_imp->m_keyframes.empty()
                         ? TDoubleKeyframe::None
                         : m_imp->m_keyframes.back().m_type;
      m_imp->m_keyframes.push_back(k);
    } else if (tagName == "expr") {
      // vecchio formato
      if (oldType != 1) continue;
      string text    = is.getTagAttribute("text");
      string enabled = is.getTagAttribute("enabled");
      TDoubleKeyframe kk1, kk2;
      kk1.m_frame          = -1000;
      kk2.m_frame          = 1000;
      kk1.m_isKeyframe     = true;
      kk2.m_isKeyframe     = true;
      kk1.m_expressionText = text;
      kk2.m_expressionText = text;
      kk1.m_type           = TDoubleKeyframe::Expression;
      kk2.m_type           = TDoubleKeyframe::Expression;
      TActualDoubleKeyframe k1(kk1);
      TActualDoubleKeyframe k2(kk2);
      k1.m_expression.setGrammar(m_imp->m_grammar);
      k1.m_expression.setOwnerParameter(this);
      k1.m_prevType = m_imp->m_keyframes.empty()
                          ? TDoubleKeyframe::None
                          : m_imp->m_keyframes.back().m_type;
      k2.m_expression.setGrammar(m_imp->m_grammar);
      k2.m_expression.setOwnerParameter(this);
      k2.m_prevType = m_imp->m_keyframes.empty()
                          ? TDoubleKeyframe::None
                          : m_imp->m_keyframes.back().m_type;
      m_imp->m_keyframes.push_back(k1);
      m_imp->m_keyframes.push_back(k2);
      continue;
    } else if (tagName == "file") {
      // vecchio formato
      if (oldType != 2) continue;
      TDoubleKeyframe::FileParams params;
      params.m_path       = TFilePath(is.getTagAttribute("path"));
      params.m_fieldIndex = std::stoi(is.getTagAttribute("index"));
      TActualDoubleKeyframe k1, k2;
      k1.m_frame      = -1000;
      k2.m_frame      = 1000;
      k1.m_isKeyframe = true;
      k2.m_isKeyframe = true;
      k1.m_fileData.setParams(params);
      k2.m_fileData.setParams(params);
      k1.m_type = TDoubleKeyframe::File;
      k2.m_type = TDoubleKeyframe::File;
      k1.m_expression.setGrammar(m_imp->m_grammar);
      k1.m_expression.setOwnerParameter(this);
      k1.m_prevType = m_imp->m_keyframes.empty()
                          ? TDoubleKeyframe::None
                          : m_imp->m_keyframes.back().m_type;
      k2.m_expression.setGrammar(m_imp->m_grammar);
      k2.m_expression.setOwnerParameter(this);
      k2.m_prevType = m_imp->m_keyframes.empty()
                          ? TDoubleKeyframe::None
                          : m_imp->m_keyframes.back().m_type;
      m_imp->m_keyframes.push_back(k1);
      m_imp->m_keyframes.push_back(k2);
      continue;
    } else if (tagName == "step") {
      int step = 0;
      is >> step;
      // if(step>1)
      //  m_imp->m_frameStep = step;
    } else if (tagName == "keyframes") {
      while (!is.eos()) {
        TDoubleKeyframe kk;
        kk.loadData(is);
        TActualDoubleKeyframe k(kk);
        k.m_expression.setGrammar(m_imp->m_grammar);
        k.m_expression.setOwnerParameter(this);
        k.m_prevType = m_imp->m_keyframes.empty()
                           ? TDoubleKeyframe::None
                           : m_imp->m_keyframes.back().m_type;
        m_imp->m_keyframes.push_back(k);
      }
    } else {
      throw TException(tagName + " : unexpected tag");
    }
    if (!is.matchEndTag()) throw TException(tagName + " : missing endtag");
  }
  if (m_imp->m_keyframes.empty() && !is.eos()) {
    // vecchio sistema (prima 16/1/2003)
    while (!is.eos()) {
      double t, v;
      is >> t >> v;
      m_imp->m_keyframes.push_back(TActualDoubleKeyframe(t, v));
    }
    if (!m_imp->m_keyframes.empty()) {
      m_imp->m_keyframes[0].m_prevType = TDoubleKeyframe::None;
      for (int i = 1; i < (int)m_imp->m_keyframes.size(); i++)
        m_imp->m_keyframes[i].m_prevType = m_imp->m_keyframes[i - 1].m_type;
    }
  }

  m_imp->notify(TParamChange(this, 0, 0, true, false, false));
}

//---------------------------------------------------------

void TDoubleParam::saveData(TOStream &os) {
  // os.child("type") << (int)m_imp->m_type;
  os.child("default") << m_imp->m_defaultValue;
  if (isCycleEnabled()) os.child("cycle") << std::string("enabled");
  // if(getExtrapolationAfter() == Loop)
  //  os.child("cycle") << string("loop");
  if (!m_imp->m_keyframes.empty()) {
    os.openChild("keyframes");
    DoubleKeyframeVector::const_iterator it;
    for (it = m_imp->m_keyframes.begin(); it != m_imp->m_keyframes.end(); ++it)
      it->saveData(os);
    os.closeChild();
  }
}

//---------------------------------------------------------

string TDoubleParam::getStreamTag() const { return "doubleParam"; }

//-------------------------------------------------------------------

string TDoubleParam::getValueAlias(double frame, int precision) {
  return ::to_string(getValue(frame), precision);
}

//-------------------------------------------------------------------

void TDoubleParam::setGrammar(const TSyntax::Grammar *grammar) {
  m_imp->m_grammar = grammar;
  for (int i = 0; i < (int)m_imp->m_keyframes.size(); i++)
    m_imp->m_keyframes[i].m_expression.setGrammar(grammar);
}

//-------------------------------------------------------------------

const TSyntax::Grammar *TDoubleParam::getGrammar() const {
  return m_imp->m_grammar;
}

//-------------------------------------------------------------------

void TDoubleParam::accept(TSyntax::CalculatorNodeVisitor &visitor) {
  for (int i = 0; i < (int)m_imp->m_keyframes.size(); i++)
    if (m_imp->m_keyframes[i].m_type == TDoubleKeyframe::Expression ||
        m_imp->m_keyframes[i].m_type == TDoubleKeyframe::SimilarShape)
      m_imp->m_keyframes[i].m_expression.accept(visitor);
}

//-------------------------------------------------------------------

string TDoubleParam::getMeasureName() const { return m_imp->m_measureName; }

//-------------------------------------------------------------------

void TDoubleParam::setMeasureName(string name) {
  m_imp->m_measureName = name;
  m_imp->m_measure     = TMeasureManager::instance()->get(name);
}

//-------------------------------------------------------------------

TMeasure *TDoubleParam::getMeasure() const { return m_imp->m_measure; }
