

#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include "tundo.h"
#include "tunit.h"
#include <QString>
#include <map>

//=============================================================================
//
// Keyframes Undo
//
//-----------------------------------------------------------------------------

class KeyframesUndo final : public TUndo {
  TDoubleParamP m_param;
  typedef std::map<int, TDoubleKeyframe> Keyframes;
  Keyframes m_oldKeyframes;
  Keyframes m_newKeyframes;

public:
  KeyframesUndo(TDoubleParam *param) : m_param(param) {}
  void addKeyframe(int kIndex) {
    if (m_oldKeyframes.count(kIndex) > 0) return;
    assert(0 <= kIndex && kIndex < m_param->getKeyframeCount());
    m_oldKeyframes[kIndex] = m_param->getKeyframe(kIndex);
  }
  int createKeyframe(double frame) {
    TDoubleKeyframe oldKeyframe = m_param->getKeyframeAt(frame);
    if (oldKeyframe.m_isKeyframe) {
      int kIndex = m_param->getClosestKeyframe(oldKeyframe.m_frame);
      assert(0 <= kIndex && kIndex < m_param->getKeyframeCount());
      assert(m_param->keyframeIndexToFrame(kIndex) == oldKeyframe.m_frame);
      return kIndex;
    }
    m_param->setKeyframe(oldKeyframe);
    int kIndex = m_param->getClosestKeyframe(oldKeyframe.m_frame);
    assert(0 <= kIndex && kIndex < m_param->getKeyframeCount());
    assert(m_param->keyframeIndexToFrame(kIndex) == oldKeyframe.m_frame);
    assert(m_oldKeyframes.count(kIndex) == 0);
    m_oldKeyframes[kIndex] = oldKeyframe;
    return kIndex;
  }

  void onAdd() override {
    Keyframes::iterator it;
    for (it = m_oldKeyframes.begin(); it != m_oldKeyframes.end(); ++it) {
      int kIndex = it->first;
      assert(0 <= kIndex && kIndex < m_param->getKeyframeCount());
      m_newKeyframes[kIndex] = m_param->getKeyframe(kIndex);
    }
  }
  void undo() const override {
    m_param->setKeyframes(m_oldKeyframes);
    Keyframes::const_iterator it;
    for (it = m_oldKeyframes.begin(); it != m_oldKeyframes.end(); ++it)
      if (!it->second.m_isKeyframe) m_param->deleteKeyframe(it->second.m_frame);
  }
  void redo() const override {
    Keyframes::const_iterator it;
    for (it = m_oldKeyframes.begin(); it != m_oldKeyframes.end(); ++it)
      if (!it->second.m_isKeyframe) m_param->setKeyframe(it->second);
    m_param->setKeyframes(m_newKeyframes);
  }
  int getSize() const override {
    return sizeof(*this) +
           sizeof(*m_oldKeyframes.begin()) * 2 * m_oldKeyframes.size();
  }
  QString getHistoryString() override { return QObject::tr("Set Keyframe"); }
};

//=============================================================================
//
// KeyframeSetter
//
//-----------------------------------------------------------------------------

KeyframeSetter::KeyframeSetter(TDoubleParam *param, int kIndex, bool enableUndo)
    : m_param(param)
    , m_kIndex(-1)
    , m_extraDFrame(0)
    , m_enableUndo(enableUndo)
    , m_undo(new KeyframesUndo(param))
    , m_changed(false)
    , m_pixelRatio(1) {
  if (kIndex >= 0) selectKeyframe(kIndex);
}

KeyframeSetter::~KeyframeSetter() {
  if (m_enableUndo)
    addUndo();
  else if (m_undo) {
    delete m_undo;
    m_undo = 0;
  }
}

void KeyframeSetter::addUndo() {
  if (m_undo) {
    if (m_changed)
      TUndoManager::manager()->add(m_undo);
    else
      delete m_undo;
    m_undo = 0;
  }
}

void KeyframeSetter::selectKeyframe(int kIndex) {
  if (m_indices.count(kIndex) == 0) {
    m_indices.insert(kIndex);
    m_undo->addKeyframe(kIndex);
  }
  m_kIndex   = kIndex;
  m_keyframe = m_param->getKeyframe(m_kIndex);
}

// set key frame at frame and returns its index
int KeyframeSetter::createKeyframe(double frame) {
  /*--- If there is already a keyframe there, just return its Index ---*/
  int kIndex = m_param->getClosestKeyframe(frame);
  if (kIndex >= 0 && m_param->getKeyframe(kIndex).m_frame == frame) {
    selectKeyframe(kIndex);
    return kIndex;
  }
  m_changed = true;
  kIndex    = m_undo->createKeyframe(frame);
  m_indices.insert(kIndex);
  m_kIndex   = kIndex;
  m_keyframe = m_param->getKeyframe(m_kIndex);

  setStep(Preferences::instance()->getAnimationStep());

  int kCount = m_param->getKeyframeCount();
  if (kCount <= 1) {
    // a single keyframe created in a empty curve
  } else if (kCount == 2) {
    // two keyframes => a linear segment
    TDoubleKeyframe::Type type =
        TDoubleKeyframe::Type(Preferences::instance()->getKeyframeType());
    setType(0, type);
  } else {
    // there are at least other two keyframes (and therefore at least a segment)
    if (m_kIndex == 0) {
      // a new segment added before the others. use the preference value
      setType(
          TDoubleKeyframe::Type(Preferences::instance()->getKeyframeType()));
    } else if (m_kIndex == kCount - 1) {
      // a new segment added after the others. use the preference value too
      setType(m_kIndex - 1, TDoubleKeyframe::Type(
                                Preferences::instance()->getKeyframeType()));
    } else {
      // a new point in a segment
      TDoubleKeyframe ka                = m_param->getKeyframe(m_kIndex - 1);
      TDoubleKeyframe kb                = m_param->getKeyframe(m_kIndex + 1);
      TDoubleKeyframe::Type segmentType = ka.m_type;
      m_keyframe.m_type                 = ka.m_type;
      m_keyframe.m_step = ka.m_step;  // An existing segment step should prevail
                                      // over the preference

      /*---When a Key is entered in a Segment, the Step value should also
       * inherit the value of the original Segment.---*/
      m_param->setKeyframe(m_kIndex, m_keyframe);

      if (segmentType == TDoubleKeyframe::SpeedInOut ||
          segmentType == TDoubleKeyframe::EaseInOut ||
          segmentType == TDoubleKeyframe::EaseInOutPercentage) {
        std::map<int, TDoubleKeyframe> keyframes;
        if (segmentType == TDoubleKeyframe::SpeedInOut) {
          splitSpeedInOutSegment(m_keyframe, ka, kb);
        } else if (segmentType == TDoubleKeyframe::EaseInOut) {
          m_keyframe.m_speedIn = m_keyframe.m_speedOut = TPointD();
          if (ka.m_frame + ka.m_speedOut.x > m_keyframe.m_frame)
            ka.m_speedOut.x = m_keyframe.m_frame - ka.m_frame;
          if (kb.m_frame + kb.m_speedIn.x < m_keyframe.m_frame)
            ka.m_speedIn.x = m_keyframe.m_frame - kb.m_frame;
        } else  // easeinpercentage
        {
          m_keyframe.m_speedIn = m_keyframe.m_speedOut = TPointD();
          double segmentWidth = kb.m_frame - ka.m_frame;
        }
        keyframes[m_kIndex - 1] = ka;
        keyframes[m_kIndex]     = m_keyframe;
        keyframes[m_kIndex + 1] = kb;
        m_undo->addKeyframe(m_kIndex - 1);
        m_undo->addKeyframe(m_kIndex + 1);
        m_param->setKeyframes(keyframes);
      } else if (segmentType == TDoubleKeyframe::Expression ||
                 segmentType == TDoubleKeyframe::SimilarShape) {
        std::string expressionText = ka.m_expressionText;

        setExpression(expressionText);
        setType(ka.m_type);
      } else
        setType(ka.m_type);
    }
  }
  return kIndex;
}

bool KeyframeSetter::isSpeedInOut(int segmentIndex) const {
  return 0 <= segmentIndex && segmentIndex + 1 < m_param->getKeyframeCount() &&
         m_param->getKeyframe(segmentIndex).m_type ==
             TDoubleKeyframe::SpeedInOut;
}

bool KeyframeSetter::isEaseInOut(int segmentIndex) const {
  if (0 <= segmentIndex && segmentIndex + 1 < m_param->getKeyframeCount()) {
    TDoubleKeyframe::Type type = m_param->getKeyframe(segmentIndex).m_type;
    if (type == TDoubleKeyframe::EaseInOut ||
        type == TDoubleKeyframe::EaseInOutPercentage)
      return true;
  }
  return false;
}

// find the speedInOut handles which will change when moving keyframe kIndex
// rotatingSpeeds: { <speed, keyframe index> }
void KeyframeSetter::getRotatingSpeedHandles(
    std::vector<std::pair<double, int>> &rotatingSpeeds, TDoubleParam *param,
    int kIndex) const {
  const double epsilon = 1.0e-7;
  int ty[4]            = {0, 0, 0, 0};
  // ty[] refers to segments around the kIndex keyframe
  // 1 ==> linear or exponential (they can cause speed handle rotation)
  // 2 ==> speedinout
  for (int i = 0; i < 4; i++) {
    int k = kIndex + i - 2;
    if (0 <= k && k < param->getKeyframeCount()) {
      TDoubleKeyframe::Type type = param->getKeyframe(k).m_type;
      if (type == TDoubleKeyframe::Linear ||
          type == TDoubleKeyframe::Exponential)
        ty[i] = 1;
      else if (type == TDoubleKeyframe::SpeedInOut)
        ty[i] = 2;
    }
  }
  // SpeedInOut * Linear *
  if ((ty[0] == 2 && ty[1] == 1) || (ty[1] == 2 && ty[2] == 1)) {
    int k        = ty[1] == 1 ? kIndex - 1 : kIndex;
    double speed = getNorm(param->getSpeedIn(k));
    if (speed > epsilon) rotatingSpeeds.push_back(std::make_pair(-speed, k));
  }
  // * Linear * SpeedInOut
  if ((ty[1] == 1 && ty[2] == 2) || (ty[2] == 1 && ty[3] == 2)) {
    int k        = ty[2] == 2 ? kIndex : kIndex + 1;
    double speed = getNorm(param->getSpeedOut(k));
    if (speed > epsilon) rotatingSpeeds.push_back(std::make_pair(speed, k));
  }
}

void KeyframeSetter::moveKeyframes(int dFrame, double dValue) {
  int n = m_param->getKeyframeCount();
  if (n == 0) return;

  // to keep constant the "length" of speed handles which are modified as side
  // effects
  std::vector<std::pair<double, int>> rotatingSpeedHandles;
  if (m_indices.size() == 1)
    getRotatingSpeedHandles(rotatingSpeedHandles, m_param.getPointer(),
                            *m_indices.begin());

  // update the frame change (m_extraDFrame represent old moves
  // which has not performed, e.g.  because of keyframe collisions)
  dFrame += m_extraDFrame;
  m_extraDFrame = 0;

  if (dFrame != 0) {
    // check frame constraints (keyframe collisions and segment type swaps)
    double preferredDFrame = dFrame;
    double dFrameSgn       = dFrame < 0 ? -1 : 1;
    dFrame *= dFrameSgn;

    typedef std::pair<TDoubleKeyframe::Type, TDoubleKeyframe::Type> TypePair;
    std::vector<std::pair<double, TypePair>> keyframes(n);
    keyframes[0].second.first = TDoubleKeyframe::None;
    for (int i = 0; i < n; i++) {
      TDoubleKeyframe kf = m_param->getKeyframe(i);
      keyframes[i].first = kf.m_frame;
      if (i < n - 1) {
        keyframes[i].second.second    = kf.m_type;
        keyframes[i + 1].second.first = kf.m_type;
      } else
        keyframes[i].second.second = TDoubleKeyframe::None;
    }

    while (dFrame > 0) {
      std::vector<std::pair<double, TypePair>> mk(
          keyframes);  // mk = moved keyframes
      std::set<int>::iterator it;
      for (it = m_indices.begin(); it != m_indices.end(); ++it)
        mk[*it].first += dFrame * dFrameSgn;
      std::sort(mk.begin(), mk.end());
      bool ok = true;
      int i;
      for (i = 0; i + 1 < n && ok; i++) {
        // check keyframe collisions
        if (fabs(mk[i + 1].first - mk[i].first) < 1.e-8) ok = false;
        // check segment type swaps
        if (mk[i].second.second != mk[i + 1].second.first &&
            mk[i].second.second != TDoubleKeyframe::None &&
            mk[i + 1].second.first != TDoubleKeyframe::None)
          ok = false;
      }
      if (ok) break;
      dFrame -= 1;
    }

    dFrame = dFrameSgn * std::max(0, dFrame);
    if (dFrame != preferredDFrame) m_extraDFrame = preferredDFrame - dFrame;
    // at this point dFrame (possibly ==0) is ok (no keyframe collisions, no
    // segment type mismatches)
  }

  std::map<int, TDoubleKeyframe> change;
  if (dFrame == 0) {
    // only value changes

    std::set<int>::iterator it;
    int i     = 0;
    m_changed = true;
    for (it = m_indices.begin(); it != m_indices.end(); ++it, i++) {
      TDoubleKeyframe keyframe = m_param->getKeyframe(*it);
      keyframe.m_value += dValue;
      change[*it] = keyframe;
      m_undo->addKeyframe(i);
    }
  } else {
    // frame+value changes

    int n = m_param->getKeyframeCount();
    std::vector<std::pair<TDoubleKeyframe, int>> keyframes(
        n);  // (keyframe, index)
    for (int i = 0; i < n; i++)
      keyframes[i] = std::make_pair(m_param->getKeyframe(i), i);

    // change frame and value of selected keyframes
    std::set<int>::iterator it;
    for (it = m_indices.begin(); it != m_indices.end(); ++it) {
      int i = *it;
      keyframes[i].first.m_frame += dFrame;
      keyframes[i].first.m_value += dValue;
      // keyframes[i].second = -1;  // to mark keyframes involved in the move
    }
    // keyframes.back().first.m_type = TDoubleKeyframe::None;

    // clear selection (indices can change: we have to rebuild it)

    std::set<int> oldSelection(m_indices);
    m_indices.clear();

    // sort keyframes (according to their - updated - frames)
    std::sort(keyframes.begin(), keyframes.end());

    for (std::set<int>::iterator it = oldSelection.begin();
         it != oldSelection.end(); ++it)
      m_indices.insert(keyframes[*it].second);

    // update segment types (because of swaps)
    /*
for(int i=0;i+1<n;i++)
{
TDoubleKeyframe &kfa = keyframes[i].first;
TDoubleKeyframe &kfb = keyframes[i+1].first;
if(kfa.m_type == TDoubleKeyframe::None)
{
kfa.m_type = TDoubleKeyframe::SpeedInOut;
kfa.m_speedOut = TPointD(0,0);
}
if(kfb.m_prevType == TDoubleKeyframe::None)
{
kfa.m_prevType = TDoubleKeyframe::SpeedInOut;
kfa.m_speedIn = TPointD(0,0);
}
}
*/

    m_changed = true;
    for (int i = 0; i < n; i++) {
      if (keyframes[i].second != i) {
        /*
// i-th keyframe has changed is order position
if(keyframes[i].first.m_type == TDoubleKeyframe::None)
{
if(i==n-1) keyframes[i].first.m_type = TDoubleKeyframe::Linear;
else keyframes[i].first.m_type = keyframes[i+1].first.m_type;
}
*/

        m_undo->addKeyframe(i);
        change[i] = keyframes[i].first;
      } else if (m_indices.count(i) > 0)
        change[i] = keyframes[i].first;
    }
  }
  m_param->setKeyframes(change);
  /*
if(!rotatingSpeedHandles.empty())
{
for(int i=0;i<(int)rotatingSpeedHandles.size();i++)
{
double speedLength = rotatingSpeedHandles[i].first;
int kIndex = rotatingSpeedHandles[i].second;
TDoubleKeyframe kf = m_param->getKeyframe(kIndex);
TPointD speed = speedLength<0 ? m_param->getSpeedIn(kIndex) :
m_param->getSpeedOut(kIndex);
speed = fabs(speedLength/getNorm(speed)) * speed;
if(speedLength<0)
  kf.m_speedIn = speed;
else
  kf.m_speedOut = speed;
m_param->setKeyframe(kf);
}
}
*/
}

void KeyframeSetter::setType(int kIndex, TDoubleKeyframe::Type type) {
  assert(0 <= kIndex && kIndex < m_param->getKeyframeCount());
  // get the current keyframe value
  TDoubleKeyframe keyframe = m_param->getKeyframe(kIndex);
  // get the next keyframe (if any) and compute the segment length
  TDoubleKeyframe nextKeyframe;
  double segmentWidth = 1;
  if (kIndex + 1 < m_param->getKeyframeCount()) {
    nextKeyframe = m_param->getKeyframe(kIndex + 1);
    segmentWidth = nextKeyframe.m_frame - keyframe.m_frame;
  } else if (kIndex + 1 > m_param->getKeyframeCount()) {
    // kIndex is the last keyframe. no segment is defined (therefore no segment
    // type)
    type = TDoubleKeyframe::Linear;
  }
  if (type == keyframe.m_type) return;

  // I'm going to change kIndex. Make sure it is selected. set the dirty flag
  m_undo->addKeyframe(kIndex);
  // m_indices.insert(kIndex);
  m_changed    = true;
  double ease0 = 0, ease1 = 0;

  std::map<int, TDoubleKeyframe> keyframes;
  switch (type) {
  case TDoubleKeyframe::SpeedInOut:
    keyframe.m_speedOut    = TPointD(segmentWidth / 3, 0);
    nextKeyframe.m_speedIn = TPointD(-segmentWidth / 3, 0);
    if (nextKeyframe.m_linkedHandles && nextKeyframe.m_speedOut.x > 0.01)
      nextKeyframe.m_speedIn = -nextKeyframe.m_speedOut;
    if (keyframe.m_linkedHandles && keyframe.m_speedIn.x < -0.01)
      keyframe.m_speedOut = -keyframe.m_speedIn;
    keyframe.m_type       = type;
    keyframes[kIndex]     = keyframe;
    keyframes[kIndex + 1] = nextKeyframe;
    m_param->setKeyframes(keyframes);
    break;

  case TDoubleKeyframe::EaseInOut:
  case TDoubleKeyframe::EaseInOutPercentage:
    if (keyframe.m_type == TDoubleKeyframe::EaseInOut) {
      // absolute -> percentage
      ease0 = keyframe.m_speedOut.x * 100.0 / segmentWidth;
      ease1 = -nextKeyframe.m_speedIn.x * 100.0 / segmentWidth;
      // rounding could break constraints. crop parameters
      ease0 = tcrop(ease0, 0.0, 100.0);
      ease1 = tcrop(ease1, 0.0, 100.0 - ease0);
    } else if (keyframe.m_type == TDoubleKeyframe::EaseInOutPercentage) {
      // percentage -> absolute
      ease0 = keyframe.m_speedOut.x * 0.01 * segmentWidth;
      ease1 = -nextKeyframe.m_speedIn.x * 0.01 * segmentWidth;
      // rounding could break constraints. crop parameters
      ease0 = tcrop(ease0, 0.0, segmentWidth);
      ease1 = tcrop(ease1, 0.0, segmentWidth - ease0);
    } else {
      ease1 = ease0 = segmentWidth / 3;
    }
    keyframe.m_speedOut    = TPointD(ease0, 0);
    nextKeyframe.m_speedIn = TPointD(-ease1, 0);

    keyframe.m_type       = type;
    keyframes[kIndex]     = keyframe;
    keyframes[kIndex + 1] = nextKeyframe;
    m_param->setKeyframes(keyframes);
    break;

  case TDoubleKeyframe::Expression:
    keyframe.m_type = type;
    {
      double value      = m_param->getValue(keyframe.m_frame);
      const TUnit *unit = 0;
      if (m_param->getMeasure()) unit = m_param->getMeasure()->getCurrentUnit();
      if (unit) value = unit->convertTo(value);
      keyframe.m_expressionText = QString::number(value).toStdString();
    }
    m_param->setKeyframe(kIndex, keyframe);
    break;

  default:
    keyframe.m_type = type;
    m_param->setKeyframe(kIndex, keyframe);
  }
}

void KeyframeSetter::setType(TDoubleKeyframe::Type type) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  setType(m_kIndex, type);
}

void KeyframeSetter::setStep(int step) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  if (m_keyframe.m_step == step) return;
  m_changed         = true;
  m_keyframe.m_step = step;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setExpression(std::string expression) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  m_changed                   = true;
  m_keyframe.m_expressionText = expression;
  m_keyframe.m_type           = TDoubleKeyframe::Expression;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setSimilarShape(std::string expression, double offset) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  m_changed                       = true;
  m_keyframe.m_expressionText     = expression;
  m_keyframe.m_similarShapeOffset = offset;
  m_keyframe.m_type               = TDoubleKeyframe::SimilarShape;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setFile(const TDoubleKeyframe::FileParams &params) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  m_changed               = true;
  m_keyframe.m_fileParams = params;
  m_keyframe.m_type       = TDoubleKeyframe::File;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setUnitName(std::string unitName) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  m_changed             = true;
  m_keyframe.m_unitName = unitName;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setValue(double value) {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  if (m_keyframe.m_value == value) return;
  m_changed          = true;
  m_keyframe.m_value = value;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::linkHandles() {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  if (m_keyframe.m_linkedHandles) return;
  m_changed                  = true;
  m_keyframe.m_linkedHandles = true;
  if (isSpeedInOut(m_kIndex) && isSpeedInOut(m_kIndex - 1)) {
    // keyframe is between two speedin/out segments (and therefore linkHandles
    // makes sense)
    TPointD d        = -m_keyframe.m_speedIn + m_keyframe.m_speedOut;
    const double eps = 0.1e-3;
    if (d.x <= eps) {
      m_keyframe.m_speedIn = m_keyframe.m_speedOut = TPointD(0, 0);
    } else {
      m_keyframe.m_speedIn.y  = d.y * m_keyframe.m_speedIn.x / d.x;
      m_keyframe.m_speedOut.y = d.y * m_keyframe.m_speedOut.x / d.x;
    }
  }
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::unlinkHandles() {
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  if (!m_keyframe.m_linkedHandles) return;
  m_changed                  = true;
  m_keyframe.m_linkedHandles = false;
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setSpeedIn(const TPointD &speedIn) {
  const double eps = 0.00001;
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  assert(isSpeedInOut(m_kIndex - 1));
  m_changed            = true;
  m_keyframe.m_speedIn = speedIn;
  if (m_keyframe.m_speedIn.x > 0) m_keyframe.m_speedIn.x = 0;
  if (m_keyframe.m_linkedHandles &&
      m_kIndex + 1 <= m_param->getKeyframeCount()) {
    double outNorm = getNorm(m_keyframe.m_speedOut);
    if (m_kIndex + 1 == m_param->getKeyframeCount() || isSpeedInOut(m_kIndex) ||
        (m_keyframe.m_type == TDoubleKeyframe::Expression &&
         m_keyframe.m_expressionText.find("cycle") != std::string::npos)) {
      // update next segment speed vector
      double inNorm = getNorm(m_keyframe.m_speedIn);
      if (inNorm < eps)
        m_keyframe.m_speedOut = TPointD(outNorm, 0);
      else
        m_keyframe.m_speedOut = -(outNorm / inNorm) * m_keyframe.m_speedIn;
    } else {
      // can't change next segment speed vector => adjust speedIn to be
      // collinear
      TPointD w     = rotate90(m_keyframe.m_speedOut);
      double wNorm2 = norm2(w);
      if (wNorm2 > eps * eps)
        m_keyframe.m_speedIn -= (1.0 / wNorm2) * (w * m_keyframe.m_speedIn) * w;
    }
  }
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setSpeedOut(const TPointD &speedOut) {
  const double eps = 0.00001;
  assert(m_kIndex >= 0 && m_indices.size() == 1);
  assert(isSpeedInOut(m_kIndex));
  m_changed             = true;
  m_keyframe.m_speedOut = speedOut;
  if (m_keyframe.m_speedOut.x < 0) m_keyframe.m_speedOut.x = 0;
  if (m_keyframe.m_linkedHandles && m_kIndex > 0) {
    double inNorm = getNorm(m_keyframe.m_speedIn);
    if (isSpeedInOut(m_kIndex - 1)) {
      // update previous segment speed vector
      double outNorm = getNorm(m_keyframe.m_speedOut);
      if (outNorm > eps)
        m_keyframe.m_speedIn = -inNorm / outNorm * m_keyframe.m_speedOut;
      // else
      //  m_keyframe.m_speedIn = TPointD(inNorm,0);

    } else {
      // can't change previous segment speed vector => adjust speedOut to be
      // collinear
      double h = 0.00001;
      double f = m_keyframe.m_frame;
      double v = (m_param->getValue(f) - m_param->getValue(f - h)) / h;
      TPointD w(-v, 1);
      double wNorm2 = norm2(w);
      if (wNorm2 > eps * eps)
        m_keyframe.m_speedOut -=
            (1.0 / wNorm2) * (w * m_keyframe.m_speedOut) * w;
    }
  }
  m_param->setKeyframe(m_kIndex, m_keyframe);
}

void KeyframeSetter::setEaseIn(double easeIn) {
  // easeIn <=0
  assert(m_kIndex >= 1 && m_indices.size() == 1);
  assert(isEaseInOut(m_kIndex - 1));
  m_changed                    = true;
  TDoubleKeyframe prevKeyframe = m_param->getKeyframe(m_kIndex - 1);
  bool isPercentage =
      prevKeyframe.m_type == TDoubleKeyframe::EaseInOutPercentage;
  if (!isPercentage) easeIn = floor(easeIn + 0.5);
  double segmentWidth =
      isPercentage ? 100.0 : m_keyframe.m_frame - prevKeyframe.m_frame;
  easeIn               = -tcrop(-easeIn, 0.0, segmentWidth);
  m_keyframe.m_speedIn = TPointD(easeIn, 0);
  if (prevKeyframe.m_speedOut.x - easeIn > segmentWidth) {
    m_undo->addKeyframe(m_kIndex - 1);
    prevKeyframe.m_speedOut.x = segmentWidth + easeIn;
    std::map<int, TDoubleKeyframe> keyframes;
    keyframes[m_kIndex - 1] = prevKeyframe;
    keyframes[m_kIndex]     = m_keyframe;
    m_param->setKeyframes(keyframes);
  } else {
    m_param->setKeyframe(m_kIndex, m_keyframe);
  }
}

void KeyframeSetter::setEaseOut(double easeOut) {
  assert(m_kIndex >= 0 && m_kIndex + 1 < m_param->getKeyframeCount() &&
         m_indices.size() == 1);
  assert(isEaseInOut(m_kIndex));
  m_changed                    = true;
  TDoubleKeyframe nextKeyframe = m_param->getKeyframe(m_kIndex + 1);
  bool isPercentage = m_keyframe.m_type == TDoubleKeyframe::EaseInOutPercentage;
  if (!isPercentage) easeOut = floor(easeOut + 0.5);
  double segmentWidth =
      isPercentage ? 100.0 : nextKeyframe.m_frame - m_keyframe.m_frame;
  easeOut               = tcrop(easeOut, 0.0, segmentWidth);
  m_keyframe.m_speedOut = TPointD(easeOut, 0);
  if (-nextKeyframe.m_speedIn.x + easeOut > segmentWidth) {
    m_undo->addKeyframe(m_kIndex + 1);
    nextKeyframe.m_speedIn.x = easeOut - segmentWidth;
    std::map<int, TDoubleKeyframe> keyframes;
    keyframes[m_kIndex + 1] = nextKeyframe;
    keyframes[m_kIndex]     = m_keyframe;
    m_param->setKeyframes(keyframes);
  } else {
    m_param->setKeyframe(m_kIndex, m_keyframe);
  }
}

//=============================================================================

// set the curve params adaptively by clicking apply button
void KeyframeSetter::setAllParams(
    int step, TDoubleKeyframe::Type comboType, const TPointD &speedIn,
    const TPointD &speedOut, std::string expressionText, std::string unitName,
    const TDoubleKeyframe::FileParams &fileParam, double similarShapeOffset) {
  assert(0 <= m_kIndex && m_kIndex < m_param->getKeyframeCount());

  TDoubleKeyframe keyframe = m_param->getKeyframe(m_kIndex);
  TDoubleKeyframe nextKeyframe;

  // get the next key
  if (m_kIndex + 1 < m_param->getKeyframeCount())
    nextKeyframe = m_param->getKeyframe(m_kIndex + 1);
  else
    comboType = TDoubleKeyframe::Linear;

  // I'm going to change kIndex. Make sure it is selected. set the dirty flag
  m_undo->addKeyframe(m_kIndex);
  m_indices.insert(m_kIndex);
  m_changed = true;

  // set step
  if (step < 1) step = 1;
  keyframe.m_step = step;

  // set type
  keyframe.m_type = comboType;

  // set parameters according to the type
  std::map<int, TDoubleKeyframe> keyframes;
  switch (comboType) {
  case TDoubleKeyframe::SpeedInOut:
    keyframe.m_speedOut    = speedOut;
    nextKeyframe.m_speedIn = speedIn;

    if (keyframe.m_speedOut.x < 0) keyframe.m_speedOut.x = 0;
    if (nextKeyframe.m_speedIn.x > 0) nextKeyframe.m_speedIn.x = 0;
    break;

  case TDoubleKeyframe::EaseInOut:
  case TDoubleKeyframe::EaseInOutPercentage:
    keyframe.m_speedOut    = speedOut;
    nextKeyframe.m_speedIn = speedIn;
    if (keyframe.m_type == TDoubleKeyframe::EaseInOut) {
      keyframe.m_speedOut.x    = floor(speedOut.x + 0.5);
      nextKeyframe.m_speedIn.x = floor(speedIn.x + 0.5);
    }
    break;

  case TDoubleKeyframe::Expression:
    keyframe.m_expressionText = expressionText;
    keyframe.m_unitName       = unitName;
    break;

  case TDoubleKeyframe::File:
    keyframe.m_fileParams = fileParam;
    keyframe.m_unitName   = unitName;
    break;

  case TDoubleKeyframe::SimilarShape:
    keyframe.m_expressionText     = expressionText;
    keyframe.m_similarShapeOffset = similarShapeOffset;
    break;

  default:
    break;
  }

  /*--- Processing Linked Curves ---*/
  const double eps = 0.00001;
  if (m_kIndex != 0 && keyframe.m_linkedHandles &&
      keyframe.m_prevType == TDoubleKeyframe::SpeedInOut) {
    double inNorm = getNorm(keyframe.m_speedIn);
    // update previous segment speed vector
    double outNorm = getNorm(keyframe.m_speedOut);
    if (outNorm > eps)
      keyframe.m_speedIn = -inNorm / outNorm * keyframe.m_speedOut;
  }

  // Next curve
  if (m_kIndex + 2 < m_param->getKeyframeCount() &&
      nextKeyframe.m_linkedHandles &&
      nextKeyframe.m_type == TDoubleKeyframe::SpeedInOut) {
    double outNorm = getNorm(nextKeyframe.m_speedOut);
    // update next segment speed vector
    double inNorm = getNorm(nextKeyframe.m_speedIn);
    if (inNorm < eps)
      nextKeyframe.m_speedOut = TPointD(outNorm, 0);
    else
      nextKeyframe.m_speedOut = -(outNorm / inNorm) * nextKeyframe.m_speedIn;
  }

  // set modified keyframe
  keyframes[m_kIndex] = keyframe;
  if (m_kIndex + 1 < m_param->getKeyframeCount())
    keyframes[m_kIndex + 1] = nextKeyframe;

  m_param->setKeyframes(keyframes);
}

//=============================================================================

class RemoveKeyframeUndo final : public TUndo {
  TDoubleParam *m_param;
  TDoubleKeyframe m_keyframe;

public:
  RemoveKeyframeUndo(TDoubleParam *param, int kIndex) : m_param(param) {
    m_param->addRef();
    m_keyframe = m_param->getKeyframe(kIndex);
  }
  ~RemoveKeyframeUndo() { m_param->release(); }
  void undo() const override { m_param->setKeyframe(m_keyframe); }
  void redo() const override { m_param->deleteKeyframe(m_keyframe.m_frame); }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Remove Keyframe"); }
};

//=============================================================================

void KeyframeSetter::removeKeyframeAt(TDoubleParam *curve, double frame) {
  int kIndex = curve->getClosestKeyframe(frame);
  if (kIndex < 0 || kIndex >= curve->getKeyframeCount() ||
      curve->keyframeIndexToFrame(kIndex) != frame)
    return;
  TUndoManager::manager()->add(new RemoveKeyframeUndo(curve, kIndex));
  curve->deleteKeyframe(frame);
}

//=============================================================================

class EnableCycleUndo final : public TUndo {
  TDoubleParam *m_param;
  TSceneHandle *m_sceneHandle;

public:
  EnableCycleUndo(TDoubleParam *param, TSceneHandle *sceneHandle)
      : m_param(param), m_sceneHandle(sceneHandle) {
    m_param->addRef();
  }
  ~EnableCycleUndo() { m_param->release(); }
  void invertCycleEnabled() const {
    bool isEnabled = m_param->isCycleEnabled();
    m_param->enableCycle(!isEnabled);
    // for now the scene handle is only available when RMB click in function
    // sheet
    if (m_sceneHandle) {
      m_sceneHandle->setDirtyFlag(true);
      m_sceneHandle->notifySceneChanged();
    }
  }
  void undo() const override { invertCycleEnabled(); }
  void redo() const override { invertCycleEnabled(); }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Cycle"); }
};

//=============================================================================

void KeyframeSetter::enableCycle(TDoubleParam *curve, bool enabled,
                                 TSceneHandle *sceneHandle) {
  curve->enableCycle(enabled);
  if (sceneHandle) sceneHandle->notifySceneChanged();
  TUndoManager::manager()->add(new EnableCycleUndo(curve, sceneHandle));
}
