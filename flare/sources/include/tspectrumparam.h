#pragma once

#ifndef TSPECTRUMPARAM_H
#define TSPECTRUMPARAM_H

#include <memory>

#include "tspectrum.h"
#include "tparamset.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================

//    Forward declaration

class TParamSet;
class TSpectrumParamImp;
class TPixelParamP;

//=============================================================

#ifdef _WIN32
class TSpectrumParam;
template class DVAPI TPersistDeclarationT<TSpectrumParam>;
#endif

//---------------------------------------------------------

class DVAPI TSpectrumParam final : public TParam {
  PERSIST_DECLARATION(TSpectrumParam)

  std::unique_ptr<TSpectrumParamImp> m_imp;

public:
  TSpectrumParam();
  TSpectrumParam(std::vector<TSpectrum::ColorKey> const &keys);
  TSpectrumParam(const TSpectrumParam &);
  ~TSpectrumParam();

  TParam *clone() const override { return new TSpectrumParam(*this); }
  void copy(TParam *src) override;

  void addObserver(TParamObserver *) override;
  void removeObserver(TParamObserver *) override;

  TSpectrum getValue(double frame) const;
  TSpectrum64 getValue64(double frame) const;
  TSpectrumF getValueF(double frame) const;
  void setValue(double frame, const TSpectrum &value, bool undoing = false);
  void setDefaultValue(const TSpectrum &value);

  int getKeyCount() const;

  TDoubleParamP getPosition(int index) const;
  TPixelParamP getColor(int index) const;

  void setValue(double frame, int index, double s, const TPixel32 &color,
                bool undoing = false);

  void insertKey(int index, double s, const TPixel32 &color);
  void addKey(double s, const TPixel32 &color);
  void removeKey(int index);

  bool isAnimatable() const override { return true; }
  bool isKeyframe(double frame) const override;
  void deleteKeyframe(double frame) override;
  void clearKeyframes() override;
  void assignKeyframe(double frame, const TSmartPointerT<TParam> &src,
                      double srcFrame, bool changedOnly = false) override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void enableDragging(bool on);
  void enableNotification(bool on) override;
  bool isNotificationEnabled() const override;
  void enableMatte(bool on);

  bool isMatteEnabled() const;
  std::string getValueAlias(double frame, int precision) override;
  bool hasKeyframes() const override;
  void getKeyframes(std::set<double> &frames) const override;
  int getNextKeyframe(double frame) const override;
  int getPrevKeyframe(double frame) const override;
  double keyframeIndexToFrame(int index) const override;
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<TSpectrumParam>;
template class DVAPI TDerivedSmartPointerT<TSpectrumParam, TParam>;
#endif

class DVAPI TSpectrumParamP final
    : public TDerivedSmartPointerT<TSpectrumParam, TParam> {
public:
  TSpectrumParamP() {}
  TSpectrumParamP(std::vector<TSpectrum::ColorKey> const &keys)
      : TDerivedSmartPointerT<TSpectrumParam, TParam>(
            new TSpectrumParam(keys)) {}
  TSpectrumParamP(TSpectrumParam *p)
      : TDerivedSmartPointerT<TSpectrumParam, TParam>(p) {}
  TSpectrumParamP(const TParamP &p)
      : TDerivedSmartPointerT<TSpectrumParam, TParam>(p) {}
  operator TParamP() const { return TParamP(m_pointer); }
};

//---------------------------------------------------------

DVAPI TIStream &operator>>(TIStream &in, TSpectrumParamP &p);

#endif
