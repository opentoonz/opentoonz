#pragma once

#ifndef TDOUBLEPARAMRELAYPROPERTY_H
#define TDOUBLEPARAMRELAYPROPERTY_H

// TnzCore includes
#include "tproperty.h"

// TnzBase includes
#include "tdoubleparam.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//*****************************************************************************
//    TDoubleParamRelayProperty  declaration
//*****************************************************************************

//! The TDoubleParamRelayProperty is a TProperty heir which can be used as
//! intermediary between a TDoubleParam instance and its GUI viewers.
class DVAPI TDoubleParamRelayProperty final : public TProperty,
                                              public TParamObserver {
  TDoubleParamP m_param;  //!< The referenced param
  double m_frame;         //!< Frame at which m_param returns values

public:
  class Visitor {
  public:
    virtual void visit(TDoubleParamRelayProperty *p) = 0;
  };

public:
  TDoubleParamRelayProperty(const std::string &name,
                            TDoubleParamP param = TDoubleParamP());
  ~TDoubleParamRelayProperty();

  TDoubleParamRelayProperty(const TDoubleParamRelayProperty &other);
  TDoubleParamRelayProperty &operator=(const TDoubleParamRelayProperty &other);

  TProperty *clone() const override;
  std::string getValueAsString() override;

  void setParam(const TDoubleParamP &param);
  const TDoubleParamP &getParam() const { return m_param; }

  void setValue(double v);
  double getValue() const;

  double frame() const { return m_frame; }
  double &frame() { return m_frame; }

  void accept(TProperty::Visitor &v) override;

  void onChange(const TParamChange &) override;
};

#endif  // TDOUBLEPARAMRELAYPROPERTY_H
