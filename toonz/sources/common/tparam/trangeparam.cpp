

#include "tparamset.h"
#include "tdoubleparam.h"
#include "texception.h"
#include "tstream.h"

//=========================================================

class TRangeParamImp {
public:
  TRangeParamImp(const DoublePair &v)
      : m_min(new TDoubleParam(v.first)), m_max(new TDoubleParam(v.second)) {}
  TRangeParamImp(const TRangeParamImp &src)
      : m_min(src.m_min->clone()), m_max(src.m_max->clone()) {}

  ~TRangeParamImp() {}

  TDoubleParamP m_min, m_max;
};

//---------------------------------------------------------

PERSIST_IDENTIFIER(TRangeParam, "rangeParam")

TRangeParam::TRangeParam(const DoublePair &v) : m_data(new TRangeParamImp(v)) {
  addParam(m_data->m_min, "min");
  addParam(m_data->m_max, "max");
}

//---------------------------------------------------------

TRangeParam::TRangeParam(const TRangeParam &src)
    : TParamSet(src.getName()), m_data(new TRangeParamImp(*src.m_data)) {
  addParam(m_data->m_min, "min");
  addParam(m_data->m_max, "max");
}

//---------------------------------------------------------

TRangeParam::~TRangeParam() { delete m_data; }

//---------------------------------------------------------

void TRangeParam::copy(TParam *src) {
  TRangeParam *p = dynamic_cast<TRangeParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_data->m_min->copy(p->m_data->m_min.getPointer());
  m_data->m_max->copy(p->m_data->m_max.getPointer());
}

//---------------------------------------------------------

DoublePair TRangeParam::getDefaultValue() const {
  return DoublePair(m_data->m_min->getDefaultValue(),
                    m_data->m_max->getDefaultValue());
}

//---------------------------------------------------------

DoublePair TRangeParam::getValue(double frame) const {
  return DoublePair(m_data->m_min->getValue(frame),
                    m_data->m_max->getValue(frame));
}

//---------------------------------------------------------

bool TRangeParam::setValue(double frame, const DoublePair &v) {
  beginParameterChange();
  m_data->m_min->setValue(frame, v.first);
  m_data->m_max->setValue(frame, v.second);
  endParameterChange();
  return true;
}

//---------------------------------------------------------

void TRangeParam::setDefaultValue(const DoublePair &v) {
  m_data->m_min->setDefaultValue(v.first);
  m_data->m_max->setDefaultValue(v.second);
}

//---------------------------------------------------------

void TRangeParam::loadData(TIStream &is) {
  std::string childName;
  while (is.openChild(childName)) {
    if (childName == "min")
      m_data->m_min->loadData(is);
    else if (childName == "max")
      m_data->m_max->loadData(is);
    else
      throw TException("unknown tag");
    is.closeChild();
  }
}

//---------------------------------------------------------

void TRangeParam::saveData(TOStream &os) {
  os.openChild("min");
  m_data->m_min->saveData(os);
  os.closeChild();
  os.openChild("max");
  m_data->m_max->saveData(os);
  os.closeChild();
}

//---------------------------------------------------------

TDoubleParamP &TRangeParam::getMin() { return m_data->m_min; }

//---------------------------------------------------------

TDoubleParamP &TRangeParam::getMax() { return m_data->m_max; }
#ifdef BUTTA
//---------------------------------------------------------

int TRangeParam::getNextKeyframe(double frame) const {
  int f_min = m_data->m_min->getNextKeyframe(frame);
  int f_max = m_data->m_max->getNextKeyframe(frame);
  if (f_min <= f_max && f_min != -1)
    return f_min;
  else {
    if (f_max != -1)
      return f_max;
    else
      return -1;
  }
}

//---------------------------------------------------------

int TRangeParam::getPrevKeyframe(double frame) const {
  int f_min = m_data->m_min->getPrevKeyframe(frame);
  int f_max = m_data->m_max->getPrevKeyframe(frame);
  if (f_min >= f_max)
    return f_min;
  else
    return f_max;
}

//---------------------------------------------------------

void TRangeParam::deleteKeyframe(double frame, bool undoing) {
  m_data->m_min->deleteKeyframe(frame, undoing);
  m_data->m_max->deleteKeyframe(frame, undoing);
}

//---------------------------------------------------------

bool TRangeParam::isKeyframe(double frame) const {
  bool min, max;
  min = m_data->m_min->isKeyframe(frame);
  max = m_data->m_max->isKeyframe(frame);
  return (min || max);
}
//---------------------------------------------------------

#endif
