#pragma once

#ifndef TDOUBLEPARAM_H
#define TDOUBLEPARAM_H

#include <memory>

// TnzCore includes
#include "tgeometry.h"
#include "tfilepath.h"

// TnzBase includes
#include "tparam.h"
#include "tparamchange.h"

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//    Forward declarations

class TDoubleParam;
class TDoubleKeyframe;
class TMeasure;
class TExpression;
class TDoubleKeyframe;

namespace TSyntax {
class Grammar;
class CalculatorNodeVisitor;
}

#ifdef _WIN32
template class DVAPI TPersistDeclarationT<TDoubleParam>;
#endif

//=========================================================

//**************************************************************************
//    TDoubleParam  declaration
//**************************************************************************

class DVAPI TDoubleParam final : public TParam {
  PERSIST_DECLARATION(TDoubleParam)

  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TDoubleParam(double v = 0.0);
  TDoubleParam(const TDoubleParam &src);
  ~TDoubleParam();

  TDoubleParam &operator=(const TDoubleParam &);

  TParam *clone() const override { return new TDoubleParam(*this); }
  void copy(TParam *src) override;

  std::string getMeasureName() const;
  void setMeasureName(std::string name);
  TMeasure *getMeasure() const;

  void setValueRange(double min, double max, double step = 1.0);
  bool getValueRange(double &min, double &max, double &step) const;

  double getDefaultValue() const;
  void setDefaultValue(double value);

  double getValue(double frame, bool leftmost = false) const;
  // note: if frame is a keyframe separating two segments of different types
  // (e.g. expression and linear) then getValue(frame,true) can be !=
  // getValue(frame,false)

  bool setValue(double frame, double value);

  // returns the incoming speed vector for keyframe kIndex. kIndex-1 must be
  // speedinout
  // if kIndex is not speedinout and handle are linked then recomputes speed.y
  // taking
  // in account the next segment
  TPointD getSpeedIn(int kIndex) const;

  // returns the outcoming speed vector for keyframe kIndex. kIndex must be
  // speedinout
  TPointD getSpeedOut(int kIndex) const;

  // TPointD getSpeed(double frame);

  // a specific grammar defines expressions as 'peg2.ns' and allows to
  // create a link to the appropriate data source (e.g.
  // the correct stageobject tree)
  const TSyntax::Grammar *getGrammar() const;
  void setGrammar(const TSyntax::Grammar *grammar);  // doesn't get ownership.

  void accept(TSyntax::CalculatorNodeVisitor &visitor);

  //! cycle controls extrapolation after the last keyframe
  void enableCycle(bool enabled);
  bool isCycleEnabled() const;

  int getKeyframeCount() const;
  void getKeyframes(std::set<double> &frames) const override;
  double keyframeIndexToFrame(int index) const override;

  const TDoubleKeyframe &getKeyframe(int index) const;
  const TDoubleKeyframe &getKeyframeAt(double frame) const;

  //! assign k to the kIndex-th keyframe; postcondition: m_frame order is
  //! maintained
  void setKeyframe(int kIndex, const TDoubleKeyframe &k);

  //! call setKeyframe(it.first,it.second) for each it in ks; postcondition:
  //! m_frame order is maintained
  void setKeyframes(const std::map<int, TDoubleKeyframe> &ks);

  //! create a keyframe in k.m_frame (if is needed) and assign k to it
  void setKeyframe(const TDoubleKeyframe &k);

  bool isKeyframe(double frame) const override;
  bool hasKeyframes() const override;
  void deleteKeyframe(double frame) override;
  void clearKeyframes() override;

  int getClosestKeyframe(double frame) const;
  int getNextKeyframe(double frame) const override;
  int getPrevKeyframe(double frame) const override;

  void assignKeyframe(double frame, const TParamP &src, double srcFrame,
                      bool changedOnly) override;

  bool isAnimatable() const override { return true; }

  void addObserver(TParamObserver *observer) override;
  void removeObserver(TParamObserver *observer) override;

  const std::set<TParamObserver *> &observers() const;

  //! no keyframes, default value not changed
  bool isDefault() const;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  std::string getStreamTag() const;

  std::string getValueAlias(double frame, int precision) override;
};

DVAPI void splitSpeedInOutSegment(TDoubleKeyframe &k, TDoubleKeyframe &k0,
                                  TDoubleKeyframe &k1);

//---------------------------------------------------------

DEFINE_PARAM_SMARTPOINTER(TDoubleParam, double)

#endif  // TDOUBLEPARAM_H
