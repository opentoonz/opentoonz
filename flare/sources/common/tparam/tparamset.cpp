

#include "tparamset.h"
#include "tundo.h"
//#include "tparam.h"
#include "tdoubleparam.h"
#include "tstream.h"

#include <set>

//---------------------------------------------------------

namespace {
void doRelease(const std::pair<TParam *, std::string> &param) {
  param.first->release();
}
};

//------------------------------------------------------------------------------

class TParamSetImp final : public TParamObserver {
  friend class TParamSet;
  TParamSet *m_param;
  std::vector<std::pair<TParam *, std::string>> m_params;

  // ChangeBlock *m_changeBlock;
  bool m_draggingEnabled, m_notificationEnabled;

public:
  TParamSetImp(TParamSet *param)
      : m_param(param)
      //, m_changeBlock(0)
      , m_draggingEnabled(false)
      , m_notificationEnabled(true) {}

  ~TParamSetImp() {
    std::for_each(m_params.begin(), m_params.end(), doRelease);
  }
  // std::set<TParamSetObserver*> m_observers;
  std::set<TParamObserver *> m_paramObservers;

  template <typename T>
  void notify(const T &change);

  void onChange(const TParamChange &change) override {}
};

//---------------------------------------------------------

TParamSet::TParamSet(std::string name)
    : TParam(name), m_imp(new TParamSetImp(this)) {}

//---------------------------------------------------------

TParamSet::TParamSet(const TParamSet &src)
    : TParam(src.getName()), m_imp(new TParamSetImp(this)) {}

//---------------------------------------------------------

TParamSet::~TParamSet() { delete m_imp; }

//---------------------------------------------------------
/*
template <class Container >
class MyBackInsertIterator final : public
std::iterator<std::output_iterator_tag,
void, void, void, void>
{
protected:
  Container &container;
public:
  MyBackInsertIterator(Container &c) : container(c) {}
  MyBackInsertIterator<Container > &operator=(const typename
Container::value_type &value)
  {
    container.push_back(value);
    return *this;
  }
  MyBackInsertIterator<Container>&operator*()
  {
  return *this;
  }
  MyBackInsertIterator<Container>&operator++()
  {
  return *this;
  }
  MyBackInsertIterator<Container>&operator++(int)
  {
  return *this;
  }
};
*/

//---------------------------------------------------------

void TParamSet::beginParameterChange() {
  //  assert(0);

  // std::set<TParamSetObserver*>::iterator it = m_imp->m_observers.begin();
  // for (;it != m_imp->m_observers.end(); ++it)
  //  (*it)->onBeginChangeBlock(this);

  // assert(!m_imp->m_changeBlock);

  std::vector<TParam *> params;

  /*
MyBackInsertIterator<vector<pair<TParam*, string> > >
myBackInsertIterator(params);
copy(m_imp->m_params.begin(), m_imp->m_params.end(), myIterator);
*/

  std::vector<std::pair<TParam *, std::string>>::iterator it2 =
      m_imp->m_params.begin();
  for (; it2 != m_imp->m_params.end(); ++it2) params.push_back(it2->first);

  // m_imp->m_changeBlock = new ChangeBlock;
};
//---------------------------------------------------------

void TParamSet::endParameterChange(){
    //  assert(0);
    // assert(m_imp->m_changeBlock);
    /*
TParamSetChange change(this, m_imp->m_changeBlock->m_firstAffectedFrame,
                         m_imp->m_changeBlock->m_lastAffectedFrame,
                         m_imp->m_changeBlock->m_changes, false);

change.m_dragging = m_imp->m_draggingEnabled;

//delete m_imp->m_changeBlock;
//m_imp->m_changeBlock = 0;
//m_imp->notify(change);

//std::set<TParamSetObserver*>::iterator it = m_imp->m_observers.begin();
//for (;it != m_imp->m_observers.end(); ++it)
//  (*it)->onEndChangeBlock(this);

m_imp->notify(change);
*/
};

//---------------------------------------------------------

void TParamSet::addParam(const TParamP &param, const std::string &name) {
  std::pair<TParam *, std::string> paramToInsert =
      std::make_pair(param.getPointer(), name);
  std::vector<std::pair<TParam *, std::string>>::iterator it =
      std::find(m_imp->m_params.begin(), m_imp->m_params.end(), paramToInsert);

  if (it == m_imp->m_params.end()) {
    param->addRef();
    param->addObserver(m_imp);
    m_imp->m_params.push_back(paramToInsert);
    // TParamSetParamAdded psParamAdded(this, param.getPointer(), name, false);
    if (param->getName().empty()) param->setName(name);
    // m_imp->notify(psParamAdded);
  }
}

//---------------------------------------------------------

void TParamSet::insertParam(const TParamP &param, const std::string &name,
                            int index) {
  std::pair<TParam *, std::string> paramToInsert =
      std::make_pair(param.getPointer(), name);
  std::vector<std::pair<TParam *, std::string>>::iterator it =
      std::find(m_imp->m_params.begin(), m_imp->m_params.end(), paramToInsert);

  if (it == m_imp->m_params.end()) {
    param->addRef();
    param->addObserver(m_imp);
    it = m_imp->m_params.begin();
    int f;
    for (f = 0; f < index; f++) it++;
    m_imp->m_params.insert(it, paramToInsert);
    if (param->getName().empty()) param->setName(name);
  }
}

//---------------------------------------------------------

namespace {
class matchesParam {
  TParamP m_param;

public:
  matchesParam(const TParamP &param) : m_param(param) {}
  bool operator()(const std::pair<TParam *, std::string> &param) {
    return m_param.getPointer() == param.first;
  }
};
}

//---------------------------------------------------------

void TParamSet::removeParam(const TParamP &param) {
  std::vector<std::pair<TParam *, std::string>>::iterator it = std::find_if(
      m_imp->m_params.begin(), m_imp->m_params.end(), matchesParam(param));
  if (it != m_imp->m_params.end()) {
    param->removeObserver(m_imp);

    // TParamSetParamRemoved psParamRemoved(this, param.getPointer(),
    // it->second, false);
    // m_imp->notify(psParamRemoved);

    param->release();
    m_imp->m_params.erase(it);
  }
}

//---------------------------------------------------------

void TParamSet::removeAllParam() {
  while (!m_imp->m_params.empty()) {
    std::vector<std::pair<TParam *, std::string>>::iterator it =
        m_imp->m_params.begin();
    TParam *param = it->first;
    param->removeObserver(m_imp);
    param->release();
    m_imp->m_params.erase(it);
  }
}

//---------------------------------------------------------

int TParamSet::getParamCount() const { return m_imp->m_params.size(); }

//---------------------------------------------------------

TParamP TParamSet::getParam(int i) const {
  assert(i >= 0 && i < (int)m_imp->m_params.size());
  return m_imp->m_params[i].first;
}

//---------------------------------------------------------

std::string TParamSet::getParamName(int i) const {
  assert(i >= 0 && i < (int)m_imp->m_params.size());
  return m_imp->m_params[i].second;
}

//---------------------------------------------------------

int TParamSet::getParamIdx(const std::string &name) const {
  int i, paramsCount = m_imp->m_params.size();
  for (i = 0; i < paramsCount; ++i)
    if (m_imp->m_params[i].second == name) break;

  return i;
}

//---------------------------------------------------------

void TParamSet::getAnimatableParams(std::vector<TParamP> &params,
                                    bool recursive) {
  std::vector<std::pair<TParam *, std::string>>::iterator it =
      m_imp->m_params.begin();
  for (; it != m_imp->m_params.end(); ++it) {
    TParam *param = it->first;

    TDoubleParamP dparam = TParamP(param);
    if (dparam)
      params.push_back(dparam);
    else {
      TParamSetP paramset = TParamP(param);
      if (paramset && recursive)
        paramset->getAnimatableParams(params, recursive);
    }
  }
}

//---------------------------------------------------------

void TParamSet::addObserver(TParamObserver *observer) {
  // TParamSetObserver *obs = dynamic_cast<TParamSetObserver *>(observer);
  // if (obs)
  //  m_imp->m_observers.insert(obs);
  // else
  m_imp->m_paramObservers.insert(observer);
}

//---------------------------------------------------------

template <typename T>
void TParamSetImp::notify(const T &change) {
  if (m_notificationEnabled) {
    //  for (std::set<TParamSetObserver*>::iterator it = m_observers.begin();
    //                                              it!= m_observers.end();
    //                                              ++it)
    //    (*it)->onChange(change);
    for (std::set<TParamObserver *>::iterator paramIt =
             m_paramObservers.begin();
         paramIt != m_paramObservers.end(); ++paramIt)
      (*paramIt)->onChange(change);
  }
}

//---------------------------------------------------------

void TParamSet::removeObserver(TParamObserver *observer) {
  // TParamSetObserver *obs = dynamic_cast<TParamSetObserver *>(observer);
  // if (obs)
  //  m_imp->m_observers.erase(obs);
  // else
  m_imp->m_paramObservers.erase(observer);
}

//---------------------------------------------------------

void TParamSet::enableDragging(bool on) {
  std::vector<std::pair<TParam *, std::string>>::iterator it =
      m_imp->m_params.begin();
  for (; it != m_imp->m_params.end(); ++it) {
    TDoubleParamP dparam(it->first);
    // if (dparam)
    //  dparam->enableDragging(on);
  }

  m_imp->m_draggingEnabled = on;
}

//---------------------------------------------------------
/*
namespace {

class DoEnableNotification final : public std::binary_function {
public:
  DoEnableNotification() {}

  void operator() (const pair<TParam*, string> &param, bool on)
  {
    return param->first->enableNotification(on);
  }
};
}
*/
//---------------------------------------------------------

void TParamSet::enableNotification(bool on) {
  //  std::for_each(m_imp->m_params.begin(), m_imp->m_params.end(),
  //  std::bind2nd(DoEnableNotification, on));

  std::vector<std::pair<TParam *, std::string>>::iterator it =
      m_imp->m_params.begin();
  for (; it != m_imp->m_params.end(); ++it) {
    it->first->enableNotification(on);
  }

  m_imp->m_notificationEnabled = on;
}

//---------------------------------------------------------

bool TParamSet::isNotificationEnabled() const {
  return m_imp->m_notificationEnabled;
}

//---------------------------------------------------------

bool TParamSet::isKeyframe(double frame) const {
  for (int i = 0; i < getParamCount(); i++)
    if (getParam(i)->isKeyframe(frame)) return true;
  return false;
}

//---------------------------------------------------------

void TParamSet::getKeyframes(std::set<double> &frames) const {
  for (int i = 0; i < getParamCount(); i++) getParam(i)->getKeyframes(frames);
}

//---------------------------------------------------------

double TParamSet::keyframeIndexToFrame(int index) const {
  std::set<double> frames;
  getKeyframes(frames);
  assert(0 <= index && index < (int)frames.size());
  std::set<double>::const_iterator it = frames.begin();
  std::advance(it, index);
  return *it;
}

//---------------------------------------------------------

int TParamSet::getNextKeyframe(double frame) const {
  std::set<double> frames;
  getKeyframes(frames);
  std::set<double>::iterator it = frames.upper_bound(frame);
  if (it == frames.end())
    return -1;
  else
    return std::distance(frames.begin(), it);
}

//---------------------------------------------------------

int TParamSet::getPrevKeyframe(double frame) const {
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

bool TParamSet::hasKeyframes() const {
  for (int i = 0; i < getParamCount(); i++)
    if (getParam(i)->hasKeyframes()) return true;
  return false;
}

//---------------------------------------------------------

int TParamSet::getKeyframeCount() const {
  std::set<double> frames;
  getKeyframes(frames);
  return frames.size();
}

//---------------------------------------------------------

void TParamSet::deleteKeyframe(double frame) {
  for (int i = 0; i < getParamCount(); i++) getParam(i)->deleteKeyframe(frame);
}

//---------------------------------------------------------

void TParamSet::clearKeyframes() {
  for (int i = 0; i < getParamCount(); i++) getParam(i)->clearKeyframes();
}

//---------------------------------------------------------

void TParamSet::assignKeyframe(double frame, const TParamP &src,
                               double srcFrame, bool changedOnly) {
  TParamSetP paramSetSrc = src;
  if (!paramSetSrc) return;
  if (getParamCount() != paramSetSrc->getParamCount()) return;
  for (int i = 0; i < getParamCount(); i++)
    getParam(i)->assignKeyframe(frame, paramSetSrc->getParam(i), srcFrame,
                                changedOnly);
}

//---------------------------------------------------------

TParam *TParamSet::clone() const { return new TParamSet(*this); }

//---------------------------------------------------------

void TParamSet::copy(TParam *src) {
  TParamSet *p = dynamic_cast<TParamSet *>(src);
  if (!p) throw TException("invalid source for copy");
  int srcParamCount = p->getParamCount();
  removeAllParam();
  int i;
  for (i = 0; i < srcParamCount; i++) {
    TParamP param = p->getParam(i);
    addParam(param->clone(), param->getName());
  }
}

//---------------------------------------------------------

void TParamSet::loadData(TIStream &is) {
  std::string tagName;
  is.openChild(tagName);
  while (!is.eos()) {
    std::string paramName;
    is.openChild(paramName);
    TPersist *p = 0;
    is >> p;
    TParam *param = dynamic_cast<TParam *>(p);
    assert(param);
    addParam(param, paramName);
    is.closeChild();
  }
  is.closeChild();
}

//---------------------------------------------------------

void TParamSet::saveData(TOStream &os) {
  os.openChild(getName());
  std::vector<std::pair<TParam *, std::string>>::iterator it =
      m_imp->m_params.begin();
  std::vector<std::pair<TParam *, std::string>>::iterator end =
      m_imp->m_params.end();
  while (it != end) {
    os.openChild(it->second);
    // it->first->saveData(os);
    os << it->first;
    os.closeChild();
    ++it;
  }
  os.closeChild();
}

//---------------------------------------------------------

std::string TParamSet::getValueAlias(double frame, int precision) {
  std::string alias = "(";

  std::vector<std::pair<TParam *, std::string>>::iterator end =
      m_imp->m_params.begin();
  std::advance(end, m_imp->m_params.size() - 1);

  std::vector<std::pair<TParam *, std::string>>::iterator it =
      m_imp->m_params.begin();
  for (; it != end; ++it)
    alias += it->first->getValueAlias(frame, precision) + ",";

  alias += it->first->getValueAlias(frame, precision);

  alias += ")";
  return alias;
}

//---------------------------------------------------------

TPersistDeclarationT<TParamSet> TParamSet::m_declaration("TParamSet");
