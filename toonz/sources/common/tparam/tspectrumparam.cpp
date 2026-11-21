

// TnzCore includes
#include "tundo.h"
#include "tconvert.h"
#include "tstream.h"

// TnzBase includes
#include "tdoubleparam.h"

// STD includes
#include <set>

#include "tspectrumparam.h"

typedef std::pair<TDoubleParamP, TPixelParamP> ColorKeyParam;

//=========================================================

class TSpectrumParamImp {
  TSpectrumParam *m_sp;

  std::vector<ColorKeyParam> m_keys;

public:
  bool m_draggingEnabled;
  bool m_notificationEnabled;
  bool m_isMatteEnabled;

  std::set<TParamObserver *> m_observers;

public:
  TSpectrumParamImp(TSpectrumParam *sp)
      : m_sp(sp)
      , m_keys()
      , m_draggingEnabled(false)
      , m_notificationEnabled(true)
      , m_isMatteEnabled(true) {}

  TSpectrumParamImp(const TSpectrumParamImp &s) { copy(s); }

  void copy(const TSpectrumParamImp &src) {
    m_keys.clear();

    std::vector<ColorKeyParam>::const_iterator it = src.m_keys.begin();
    for (; it != src.m_keys.end(); ++it) {
      TDoubleParamP s(it->first->clone());
      TPixelParamP c(it->second->clone());
      m_keys.push_back(std::make_pair(s, c));
    }
  }

  void addKey(const ColorKeyParam &colorKey) {
    /*
m_sp->addParam(colorKey.first);
m_sp->addParam(colorKey.second);
*/
    m_keys.push_back(colorKey);
  }

  void insertKey(int index, ColorKeyParam &colorKey) {
    /*
m_sp->addParam(colorKey.first);
m_sp->addParam(colorKey.second);
*/
    std::vector<ColorKeyParam>::iterator it = m_keys.begin() + index;
    m_keys.insert(it, colorKey);
  }

  void eraseKey(int index) {
    std::vector<ColorKeyParam>::iterator colorKeyIt = m_keys.begin() + index;
    /*
m_sp->removeParam((*colorKeyIt).first);
m_sp->removeParam((*colorKeyIt).second);
*/
    m_keys.erase(colorKeyIt);
  }

  int getKeyCount() const { return m_keys.size(); }

  ColorKeyParam getKey(int index) const { return m_keys[index]; }

  void clearKeys() { m_keys.clear(); }

  void notify(const TParamChange &change) {
    for (std::set<TParamObserver *>::iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
      (*it)->onChange(change);
  }

private:
  TSpectrumParamImp &operator=(const TSpectrumParamImp &);  // not implemented
};

//=========================================================

PERSIST_IDENTIFIER(TSpectrumParam, "spectrumParam")

//---------------------------------------------------------

TSpectrumParam::TSpectrumParam()
    : m_imp(new TSpectrumParamImp(this))  // brutto...
{
  ColorKeyParam ck1(TDoubleParamP(0.0), TPixelParamP(TPixel32::Black));
  ColorKeyParam ck2(TDoubleParamP(1.0), TPixelParamP(TPixel32::White));
  m_imp->addKey(ck1);
  m_imp->addKey(ck2);
}

//---------------------------------------------------------

TSpectrumParam::TSpectrumParam(const TSpectrumParam &src)
    : TParam(src.getName()), m_imp(new TSpectrumParamImp(*src.m_imp)) {}

//---------------------------------------------------------

void TSpectrumParam::addObserver(TParamObserver *obs) {
  m_imp->m_observers.insert(obs);
}

//---------------------------------------------------------

void TSpectrumParam::removeObserver(TParamObserver *obs) {
  m_imp->m_observers.erase(obs);
}

//---------------------------------------------------------

TSpectrumParam::TSpectrumParam(std::vector<TSpectrum::ColorKey> const &keys)
    : m_imp(new TSpectrumParamImp(this)) {
  for (auto const &key : keys) {
    double v     = key.first;
    TPixel32 pix = key.second;
    TDoubleParamP dp(v);
    TPixelParamP pp(pix);
    pp->enableMatte(m_imp->m_isMatteEnabled);
    ColorKeyParam ck(dp, pp);
    m_imp->addKey(ck);
  }
}

//---------------------------------------------------------

void TSpectrumParam::copy(TParam *src) {
  TSpectrumParam *p = dynamic_cast<TSpectrumParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_imp->copy(*(p->m_imp));
}

//---------------------------------------------------------

TSpectrumParam::~TSpectrumParam() {}

//---------------------------------------------------------

TSpectrum TSpectrumParam::getValue(double frame) const {
  assert(m_imp);
  std::vector<TSpectrum::ColorKey> keys;
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam paramKey = m_imp->getKey(i);
    TSpectrum::ColorKey key(paramKey.first->getValue(frame),
                            paramKey.second->getValue(frame));
    keys.push_back(key);
  }
  return TSpectrum(keys.size(), &keys[0]);
}

//---------------------------------------------------------

TSpectrum64 TSpectrumParam::getValue64(double frame) const {
  assert(m_imp);
  std::vector<TSpectrum64::ColorKey> keys;
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam paramKey = m_imp->getKey(i);
    TSpectrum64::ColorKey key(paramKey.first->getValue(frame),
                              toPixel64(paramKey.second->getValue(frame)));
    keys.push_back(key);
  }
  return TSpectrum64(keys.size(), &keys[0]);
}

//---------------------------------------------------------

TSpectrumF TSpectrumParam::getValueF(double frame) const {
  assert(m_imp);
  std::vector<TSpectrumF::ColorKey> keys;
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam paramKey = m_imp->getKey(i);
    TSpectrumF::ColorKey key(paramKey.first->getValue(frame),
                             toPixelF(paramKey.second->getValue(frame)));
    keys.push_back(key);
  }
  return TSpectrumF(keys.size(), &keys[0]);
}

//---------------------------------------------------------

void TSpectrumParam::setValue(double frame, const TSpectrum &spectrum,
                              bool undoing) {
  assert(getKeyCount() == spectrum.getKeyCount());
  int keyCount = getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    TSpectrum::Key key = spectrum.getKey(i);
    setValue(frame, i, key.first, key.second, undoing);
  }
}

//---------------------------------------------------------

TDoubleParamP TSpectrumParam::getPosition(int index) const {
  assert(index <= m_imp->getKeyCount());
  return m_imp->getKey(index).first;
}

//---------------------------------------------------------

TPixelParamP TSpectrumParam::getColor(int index) const {
  assert(index <= m_imp->getKeyCount());
  return m_imp->getKey(index).second;
}

//---------------------------------------------------------

int TSpectrumParam::getKeyCount() const {
  assert(m_imp);
  return m_imp->getKeyCount();
}

//---------------------------------------------------------

void TSpectrumParam::setValue(double frame, int index, double s,
                              const TPixel32 &color, bool undoing) {
  assert(m_imp);
  int keyCount = m_imp->getKeyCount();
  if (index < 0 || index >= keyCount)
    throw TException("TSpectrumParam::setValue. Index out of range");

  ColorKeyParam key = m_imp->getKey(index);

  //  beginParameterChange();
  key.first->setValue(frame, s);
  key.second->setValue(frame, color);
  //  endParameterChange();

  m_imp->notify(TParamChange(this, TParamChange::m_minFrame,
                             TParamChange::m_maxFrame, true,
                             m_imp->m_draggingEnabled, false));
}

//---------------------------------------------------------

void TSpectrumParam::setDefaultValue(const TSpectrum &value) {
  assert(value.getKeyCount() == getKeyCount());
  for (int i = 0; i < getKeyCount(); i++) {
    ColorKeyParam dstKeyParam = m_imp->getKey(i);
    TSpectrum::Key srcKey     = value.getKey(i);
    dstKeyParam.first->setDefaultValue(srcKey.first);
    dstKeyParam.second->setDefaultValue(srcKey.second);
  }
}

//---------------------------------------------------------

void TSpectrumParam::insertKey(int index, double s, const TPixel32 &color) {
  assert(m_imp);
  int keyCount = m_imp->getKeyCount();
  if (index < 0)
    index = 0;
  else if (index >= keyCount)
    index = keyCount;
  TDoubleParamP dp(s);
  TPixelParamP pp(color);
  pp->enableMatte(m_imp->m_isMatteEnabled);
  ColorKeyParam ck(dp, pp);

  m_imp->insertKey(index, ck);
}

//---------------------------------------------------------

void TSpectrumParam::addKey(double s, const TPixel32 &color) {
  /*
assert(m_imp);
insertKey(m_imp->getKeyCount(), s,color);
*/
  int index = m_imp->getKeyCount();
  assert(m_imp);
  int keyCount = m_imp->getKeyCount();
  if (index < 0)
    index = 0;
  else if (index >= keyCount)
    index = keyCount;
  TDoubleParamP dp(s);
  TPixelParamP pp(color);
  pp->enableMatte(m_imp->m_isMatteEnabled);
  ColorKeyParam ck(dp, pp);

  m_imp->insertKey(index, ck);
}

//---------------------------------------------------------

void TSpectrumParam::removeKey(int index) {
  assert(m_imp);
  int keyCount = m_imp->getKeyCount();
  if (index < 0 || index >= keyCount)
    throw TException("TSpectrumParam::removeKey. Index out of range");
  m_imp->eraseKey(index);
}

//---------------------------------------------------------

bool TSpectrumParam::isKeyframe(double frame) const {
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam currentKey = m_imp->getKey(i);
    if (currentKey.first->isKeyframe(frame)) return true;
    if (currentKey.second->isKeyframe(frame)) return true;
  }
  return false;
}

//---------------------------------------------------------

void TSpectrumParam::deleteKeyframe(double frame) {
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam currentKey = m_imp->getKey(i);
    currentKey.first->deleteKeyframe(frame);
    currentKey.second->deleteKeyframe(frame);
  }
}

//---------------------------------------------------------

void TSpectrumParam::clearKeyframes() {
  assert(m_imp);

  int k, keyCount = m_imp->getKeyCount();
  for (k = 0; k < keyCount; ++k) {
    const ColorKeyParam &key = m_imp->getKey(k);

    key.first->clearKeyframes();
    key.second->clearKeyframes();
  }

  m_imp->notify(TParamChange(this, TParamChange::m_minFrame,
                             TParamChange::m_maxFrame, true,
                             m_imp->m_draggingEnabled, false));
}

//---------------------------------------------------------

void TSpectrumParam::assignKeyframe(double frame, const TParamP &src,
                                    double srcFrame, bool changedOnly) {
  TSpectrumParamP spectrum = src;
  if (!spectrum) return;
  int keyCount = m_imp->getKeyCount();
  if (keyCount != spectrum->m_imp->getKeyCount()) return;
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam dstKey = m_imp->getKey(i);
    ColorKeyParam srcKey = spectrum->m_imp->getKey(i);
    dstKey.first->setValue(frame, srcKey.first->getValue(srcFrame));
    dstKey.second->setValue(frame, srcKey.second->getValue(srcFrame));
  }
}

//---------------------------------------------------------

void TSpectrumParam::loadData(TIStream &is) {
  assert(m_imp);
  m_imp->clearKeys();
  std::string tagName;
  is.openChild(tagName);
  assert(tagName == "spectrum");
  while (!is.eos()) {
    TDoubleParamP pos(0.0);
    TPixelParamP color(TPixel32::Black);
    is.openChild(tagName);
    pos->loadData(is);
    is.closeChild();
    is.openChild(tagName);
    color->loadData(is);
    is.closeChild();
    ColorKeyParam ck(pos, color);
    m_imp->addKey(ck);
  }
  is.closeChild();
}

//---------------------------------------------------------

void TSpectrumParam::saveData(TOStream &os) {
  assert(m_imp);
  int keyCount = m_imp->getKeyCount();
  os.openChild("spectrum");
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam key = m_imp->getKey(i);
    os.openChild("s_value");
    key.first->saveData(os);
    os.closeChild();
    os.openChild("col_value");
    key.second->saveData(os);
    os.closeChild();
  }
  os.closeChild();
}

//---------------------------------------------------------

void TSpectrumParam::enableDragging(bool on) { m_imp->m_draggingEnabled = on; }

//---------------------------------------------------------

void TSpectrumParam::enableNotification(bool on) {
  m_imp->m_notificationEnabled = on;
}

//---------------------------------------------------------

bool TSpectrumParam::isNotificationEnabled() const {
  return m_imp->m_notificationEnabled;
}

//---------------------------------------------------------

namespace {

inline std::string to_string(const TPixel32 &color) {
  std::string alias = "(";
  alias += std::to_string(color.r) + ",";
  alias += std::to_string(color.g) + ",";
  alias += std::to_string(color.b) + ",";
  alias += std::to_string(color.m);
  alias += ")";
  return alias;
}

inline std::string toString(const TSpectrum::ColorKey &key, int precision) {
  std::string alias = "(";
  alias += ::to_string(key.first, precision) + ",";
  alias += to_string(key.second);
  alias += ")";
  return alias;
}

}  // namespace

std::string TSpectrumParam::getValueAlias(double frame, int precision) {
  std::vector<TSpectrum::ColorKey> keys;
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam paramKey = m_imp->getKey(i);
    TSpectrum::ColorKey key(paramKey.first->getValue(frame),
                            paramKey.second->getValue(frame));
    keys.push_back(key);
  }

  std::string alias = "(";

  if (!keys.empty()) {
    std::vector<TSpectrum::ColorKey>::iterator it  = keys.begin();
    std::vector<TSpectrum::ColorKey>::iterator end = keys.begin();
    std::advance(end, keys.size() - 1);
    for (; it != end; ++it) {
      alias += toString(*it, precision);
      alias += ",";
    }
    alias += toString(*it, precision);
  }

  alias += ")";
  return alias;
}

//=========================================================

//---------------------------------------------------------

void TSpectrumParam::enableMatte(bool on) { m_imp->m_isMatteEnabled = on; }
//---------------------------------------------------------

bool TSpectrumParam::isMatteEnabled() const { return m_imp->m_isMatteEnabled; }

//---------------------------------------------------------

bool TSpectrumParam::hasKeyframes() const {
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam currentKey = m_imp->getKey(i);
    if (currentKey.first->hasKeyframes() || currentKey.second->hasKeyframes())
      return true;
  }
  return false;
}
//---------------------------------------------------------

void TSpectrumParam::getKeyframes(std::set<double> &frames) const {
  int keyCount = m_imp->getKeyCount();
  for (int i = 0; i < keyCount; i++) {
    ColorKeyParam currentKey = m_imp->getKey(i);
    currentKey.first->getKeyframes(frames);
    currentKey.second->getKeyframes(frames);
  }
}

//---------------------------------------------------------

int TSpectrumParam::getNextKeyframe(double frame) const {
  std::set<double> frames;
  getKeyframes(frames);
  std::set<double>::iterator it = frames.upper_bound(frame);
  if (it == frames.end())
    return -1;
  else
    return std::distance(frames.begin(), it);
}

//---------------------------------------------------------

int TSpectrumParam::getPrevKeyframe(double frame) const {
  std::set<double> frames;
  getKeyframes(frames);
  std::set<double>::iterator it = frames.lower_bound(frame);
  if (it == frames.begin())
    return -1;
  else {
    --it;
    return std::distance(frames.begin(), it);
  }
}
//---------------------------------------------------------

double TSpectrumParam::keyframeIndexToFrame(int index) const {
  std::set<double> frames;
  getKeyframes(frames);
  assert(0 <= index && index < (int)frames.size());
  std::set<double>::const_iterator it = frames.begin();
  std::advance(it, index);
  return *it;
}

TIStream &operator>>(TIStream &in, TSpectrumParamP &p) {
  TPersist *tmp;
  in >> tmp;
  p = TSpectrumParamP(dynamic_cast<TSpectrumParam *>(tmp));
  return in;
}
