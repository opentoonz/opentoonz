#pragma once

#ifndef TPARAMSET_H
#define TPARAMSET_H

#include "tparam.h"
#include "tparamchange.h"
#include "tgeometry.h"

#include "tpixel.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration

class TParamSetImp;

class TParamSetChange final : public TParamChange {
public:
  std::vector<TParamChange *> m_paramChanges;

public:
  TParamSetChange(TParam *param, double firstAffectedFrame,
                  double lastAffectedFrame,
                  const std::vector<TParamChange *> &paramChanges)
      : TParamChange(param, firstAffectedFrame, lastAffectedFrame, true, false,
                     false)
      , m_paramChanges(paramChanges) {}

  ~TParamSetChange() { clearPointerContainer(m_paramChanges); }

  TParamChange *clone() const { return new TParamSetChange(*this); }
};

//------------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

class DVAPI TParamSet : public TParam {
  PERSIST_DECLARATION(TParamSet)
public:
  TParamSet(std::string name = "");
  TParamSet(const TParamSet &src);
  ~TParamSet();

  void addParam(const TParamP &param, const std::string &name);
  void insertParam(const TParamP &param, const std::string &name, int index);
  void removeParam(const TParamP &param);
  void removeAllParam();

  int getParamCount() const;
  TParamP getParam(int index) const;
  std::string getParamName(int index) const;
  int getParamIdx(const std::string &name) const;

  void getAnimatableParams(std::vector<TParamP> &params, bool recursive = true);

  void addObserver(TParamObserver *observer) override;
  void removeObserver(TParamObserver *observer) override;

  void beginParameterChange();
  void endParameterChange();

  void enableDragging(bool on);
  void enableNotification(bool on) override;
  bool isNotificationEnabled() const override;

  bool isAnimatable() const override { return true; }
  bool isKeyframe(double frame) const override;
  void deleteKeyframe(double frame) override;
  void clearKeyframes() override;
  void assignKeyframe(double frame, const TSmartPointerT<TParam> &src,
                      double srcFrame, bool changedOnly = false) override;

  void getKeyframes(std::set<double> &frames) const override;
  int getKeyframeCount() const;

  double keyframeIndexToFrame(int index) const override;

  TParam *clone() const override;
  void copy(TParam *src) override;
  void loadData(TIStream &) override;
  void saveData(TOStream &) override;

  int getNextKeyframe(double frame) const override;
  int getPrevKeyframe(double frame) const override;

  bool hasKeyframes() const override;

  std::string getValueAlias(double frame, int precision) override;

private:
  TParamSetImp *m_imp = nullptr;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef _WIN32
template class DVAPI TSmartPointerT<TParamSet>;
template class DVAPI TDerivedSmartPointerT<TParamSet, TParam>;
#endif

typedef TDerivedSmartPointerT<TParamSet, TParam> TParamSetP;

//------------------------------------------------------------------------------

class TPointParamObserver {
public:
  TPointParamObserver() {}
  virtual ~TPointParamObserver() {}

  virtual void onChange(const TParamChange &) = 0;
};

//------------------------------------------------------------------------------

#ifdef _WIN32
class TPointParam;
template class DVAPI TPersistDeclarationT<TPointParam>;
#endif

//------------------------------------------------------------------------------

class TPointParamImp;
class TDoubleParamP;

class DVAPI TPointParam final : public TParamSet {
  PERSIST_DECLARATION(TPointParam)
  TPointParamImp *m_data;
  bool m_from_plugin;

public:
  TPointParam(const TPointD &p = TPointD(), bool from_plugin = false);
  TPointParam(const TPointParam &src);
  ~TPointParam();

  TParam *clone() const override { return new TPointParam(*this); }
  void copy(TParam *src) override;

  TPointD getDefaultValue() const;
  TPointD getValue(double frame) const;
  bool setValue(double frame, const TPointD &p);
  void setDefaultValue(const TPointD &p);

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  TDoubleParamP &getX();
  TDoubleParamP &getY();

  bool isFromPlugin() const { return m_from_plugin; }
};

DEFINE_PARAM_SMARTPOINTER(TPointParam, TPointD)

//------------------------------------------------------------------------------

#ifdef _WIN32
class TPixelParam;
template class DVAPI TPersistDeclarationT<TPixelParam>;
#endif

class TPixelParamObserver {
public:
  TPixelParamObserver() {}
  virtual ~TPixelParamObserver() {}
  virtual void onChange(const TParamChange &) = 0;
};

//------------------------------------------------------------------------------

class TPixelParamImp;

class DVAPI TPixelParam final : public TParamSet {
  PERSIST_DECLARATION(TPixelParam)
  TPixelParamImp *m_data;

public:
  TPixelParam(const TPixel32 &pix = TPixel32::Black);
  TPixelParam(const TPixelParam &);
  ~TPixelParam();

  TParam *clone() const override { return new TPixelParam(*this); }
  void copy(TParam *src) override;

  TPixel32 getDefaultValue() const;
  TPixelD getValueD(double frame) const;
  TPixel32 getValue(double frame) const;
  TPixel64 getValue64(double frame) const;
  TPixel32 getPremultipliedValue(double frame) const;

  void setDefaultValue(const TPixel32 &pix);
  bool setValueD(double frame, const TPixelD &pix);
  bool setValue(double frame, const TPixel32 &pix);
  bool setValue64(double frame, const TPixel64 &pix);

  void enableMatte(bool on);

  bool isMatteEnabled() const;
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  TDoubleParamP &getRed();
  TDoubleParamP &getGreen();
  TDoubleParamP &getBlue();
  TDoubleParamP &getMatte();
};

DEFINE_PARAM_SMARTPOINTER(TPixelParam, TPixel32)

//------------------------------------------------------------------------------

class TRangeParamObserver {
public:
  TRangeParamObserver() {}
  virtual ~TRangeParamObserver() {}
  virtual void onChange(const TParamChange &) = 0;
};

//------------------------------------------------------------------------------

#ifdef _WIN32
class TRangeParam;
template class DVAPI TPersistDeclarationT<TRangeParam>;
#endif

//------------------------------------------------------------------------------

class TRangeParamImp;
class TDoubleParamP;

class DVAPI TRangeParam final : public TParamSet {
  PERSIST_DECLARATION(TRangeParam)
  TRangeParamImp *m_data;

public:
  TRangeParam(const DoublePair &range = DoublePair(0, 0));
  TRangeParam(const TRangeParam &src);
  ~TRangeParam();

  TParam *clone() const override { return new TRangeParam(*this); }
  void copy(TParam *src) override;

  DoublePair getDefaultValue() const;
  DoublePair getValue(double frame) const;
  bool setValue(double frame, const DoublePair &v);
  void setDefaultValue(const DoublePair &v);

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  int getKeyframeCount() const;

  TDoubleParamP &getMin();
  TDoubleParamP &getMax();
};

DEFINE_PARAM_SMARTPOINTER(TRangeParam, DoublePair)

#endif
