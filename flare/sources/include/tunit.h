#pragma once

#ifndef TUNIT_INCLUDED
#define TUNIT_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZBASE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

//---------------------------

class DVAPI TUnitConverter {
public:
  virtual ~TUnitConverter() {}
  virtual TUnitConverter *clone() const      = 0;
  virtual double convertTo(double v) const   = 0;
  virtual double convertFrom(double v) const = 0;
};

//---------------------------

class DVAPI TSimpleUnitConverter final : public TUnitConverter {
  const double m_factor, m_offset;

public:
  TSimpleUnitConverter(double factor = 1, double offset = 0)
      : m_factor(factor), m_offset(offset) {}
  TUnitConverter *clone() const override {
    return new TSimpleUnitConverter(*this);
  }
  double convertTo(double v) const override { return v * m_factor + m_offset; }
  double convertFrom(double v) const override {
    return (v - m_offset) / m_factor;
  }
};

//---------------------------

class DVAPI TUnit {
  std::wstring m_defaultExtension;
  std::vector<std::wstring> m_extensions;
  TUnitConverter *m_converter;

public:
  TUnit(std::wstring ext, TUnitConverter *converter = 0);
  TUnit(const TUnit &);
  ~TUnit();

  TUnit *clone() const { return new TUnit(*this); }

  const std::vector<std::wstring> &getExtensions() const {
    return m_extensions;
  }
  void addExtension(std::wstring ext);
  bool isExtension(std::wstring ext) const;

  std::wstring getDefaultExtension() const { return m_defaultExtension; }
  void setDefaultExtension(std::wstring ext);

  double convertTo(double v) const { return m_converter->convertTo(v); }
  double convertFrom(double v) const { return m_converter->convertFrom(v); }

private:
  // not implemented
  TUnit &operator=(const TUnit &);
};

//---------------------------

class DVAPI TMeasure {
  std::string m_name;
  TUnit *m_mainUnit, *m_currentUnit, *m_standardUnit;
  std::map<std::wstring, TUnit *> m_extensions;
  double m_defaultValue;

public:
  TMeasure(std::string name, TUnit *mainUnit);
  TMeasure(const TMeasure &);
  ~TMeasure();

  std::string getName() const { return m_name; }
  void setName(std::string name) { m_name = name; }

  void add(TUnit *unit);

  // main unit e' quella usata "internamente". puo' essere "strana"
  const TUnit *getMainUnit() const { return m_mainUnit; }

  // current unit e' quella ad es. definita in preferences
  const TUnit *getCurrentUnit() const { return m_currentUnit; }
  void setCurrentUnit(TUnit *unit);

  // standard unit e' quella usata nelle espressioni
  const TUnit *getStandardUnit() const { return m_standardUnit; }
  void setStandardUnit(TUnit *unit);

  TUnit *getUnit(std::wstring ext) const;

  // defaultValue e' espresso in main unit
  double getDefaultValue() const { return m_defaultValue; }
  void setDefaultValue(double v) { m_defaultValue = v; }

private:
  // not implemented
  TMeasure &operator=(const TMeasure &);
};

//---------------------------

class DVAPI TMeasureManager {  // singleton
  std::map<std::string, TMeasure *> m_measures;
  TMeasureManager();

public:
  static TMeasureManager *instance() {
    static TMeasureManager _instance;
    return &_instance;
  }

  void add(TMeasure *m);
  TMeasure *get(std::string name) const;

  typedef double CameraSizeProvider();
  void addCameraMeasures(CameraSizeProvider *cameraSizeProvider);
};

//---------------------------

class DVAPI TMeasuredValue {
  const TMeasure *m_measure;
  double m_value;

public:
  TMeasuredValue(std::string measureName);
  ~TMeasuredValue();

  const TMeasure *getMeasure() const { return m_measure; }
  void setMeasure(const TMeasure *measure);
  void setMeasure(std::string measureName);

  // used for mouse dragging to change value
  // precision is how many times to divide by 10 to adjust value
  // nothing calls precision yet, since just basic functionality
  // is in place for dragging
  void modifyValue(double direction, int precision = 0);

  enum UnitType { MainUnit, CurrentUnit };

  double getValue(UnitType uType) const {
    return uType == MainUnit ? m_value
                             : m_measure->getCurrentUnit()->convertTo(m_value);
  }
  void setValue(UnitType uType, double value) {
    m_value = uType == MainUnit
                  ? value
                  : m_measure->getCurrentUnit()->convertFrom(value);
  }

  bool setValue(std::wstring s, int *pErr = 0);  // if pErr != then *pErr
                                                 // contains error code. *pErr
                                                 // == 0 means OK
  std::wstring toWideString(int decimals = 7) const;

private:
  // not implemented
  TMeasuredValue(const TMeasuredValue &);
  TMeasuredValue &operator=(const TMeasuredValue &);
};

//---------------------------

// bruttacchietto. L'aspect ratio della field guide viene usata nelle
// conversioni
// fra fld e inch sulla verticale

namespace UnitParameters {

typedef std::pair<double, double> (*CurrentDpiGetter)();

DVAPI void setFieldGuideAspectRatio(double ar);
DVAPI double getFieldGuideAspectRatio();

DVAPI void setCurrentDpiGetter(CurrentDpiGetter f);
}

//---------------------------

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
