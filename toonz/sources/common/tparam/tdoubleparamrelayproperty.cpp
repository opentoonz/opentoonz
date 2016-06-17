

#include "tdoubleparamrelayproperty.h"

//*****************************************************************************
//    TDoubleParamRelayProperty  implementation
//*****************************************************************************

TDoubleParamRelayProperty::TDoubleParamRelayProperty(const std::string &name,
                                                     TDoubleParamP param)
    : TProperty(name), m_frame() {
  if (param) setParam(param);
}

//-------------------------------------------------------------------

TDoubleParamRelayProperty::~TDoubleParamRelayProperty() {
  if (m_param) m_param->removeObserver(this);
}

//-------------------------------------------------------------------

TDoubleParamRelayProperty::TDoubleParamRelayProperty(
    const TDoubleParamRelayProperty &other)
    : TProperty(other), m_param(other.m_param), m_frame(other.m_frame) {
  if (m_param) m_param->addObserver(this);
}

//-------------------------------------------------------------------

TDoubleParamRelayProperty &TDoubleParamRelayProperty::operator=(
    const TDoubleParamRelayProperty &other) {
  TProperty::operator=(other);

  if (m_param) m_param->removeObserver(this);

  m_param = other.m_param;
  m_frame = other.m_frame;

  if (m_param) m_param->addObserver(this);

  return *this;
}

//-------------------------------------------------------------------

TProperty *TDoubleParamRelayProperty::clone() const {
  return new TDoubleParamRelayProperty(*this);
}

//-------------------------------------------------------------------

std::string TDoubleParamRelayProperty::getValueAsString() {
  return m_param ? std::to_string(m_param->getValue(m_frame)) : "";
}

//-------------------------------------------------------------------

void TDoubleParamRelayProperty::setParam(const TDoubleParamP &param) {
  if (m_param == param) return;

  if (m_param) m_param->removeObserver(this);

  m_param = param;

  if (param) param->addObserver(this);
}

//-------------------------------------------------------------------

void TDoubleParamRelayProperty::setValue(double v) {
  if (m_param) m_param->setValue(m_frame, v);
}

//-------------------------------------------------------------------

double TDoubleParamRelayProperty::getValue() const {
  return m_param ? m_param->getValue(m_frame) : 0.0;
}

//-------------------------------------------------------------------

void TDoubleParamRelayProperty::accept(TProperty::Visitor &v) {
  if (TDoubleParamRelayProperty::Visitor *vv =
          dynamic_cast<TDoubleParamRelayProperty::Visitor *>(&v))
    vv->visit(this);
}

//-------------------------------------------------------------------

void TDoubleParamRelayProperty::onChange(const TParamChange &) {
  notifyListeners();
}
