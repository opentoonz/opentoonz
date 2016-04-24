

#ifndef TTONECURVEPARAM_H
#define TTONECURVEPARAM_H

#include "tcommon.h"
#include "tparamset.h"
#include "tstroke.h"
#include "tnotanimatableparam.h"

#include <QList>

#ifdef _WIN32
#pragma warning(disable : 4251)
#endif

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//---------------------------------------------------------

class DVAPI TToneCurveParam : public TParam
{

	PERSIST_DECLARATION(TToneCurveParam)

	TParamSetP m_rgbaParamSet;
	TParamSetP m_rgbParamSet;
	TParamSetP m_rParamSet;
	TParamSetP m_gParamSet;
	TParamSetP m_bParamSet;
	TParamSetP m_aParamSet;

	TBoolParamP m_isLinear;

	enum ToneChannel {
		RGBA = 0,
		RGB,
		Red,
		Green,
		Blue,
		Alpha
	} m_toneChannel;

public:
	TToneCurveParam();
	TToneCurveParam(const TToneCurveParam &src);
	~TToneCurveParam() {}

	TParamSetP getParamSet(ToneChannel channel) const;
	TParamSetP getCurrentParamSet() const;
	TBoolParamP getIsLinearParam() const { return m_isLinear; }

	void setCurrentChannel(ToneChannel channel);
	ToneChannel getCurrentChannel() const { return m_toneChannel; }

	TParam *clone() const { return new TToneCurveParam(*this); }
	void copy(TParam *src);

	void addObserver(TParamObserver *);
	void removeObserver(TParamObserver *);

	QList<TPointD> getValue(double frame) const;
	void setValue(double frame, const QList<TPointD> &value, bool undoing = false);
	void setDefaultValue(const QList<TPointD> &value);

	bool isLinear() const;
	void setIsLinear(bool isLinear);

	void addValue(double frame, const QList<TPointD> &value, int index);
	void removeValue(double frame, int index);

	//  virtual void enableNotification(bool on) {}
	//  virtual bool isNotificationEnabled() const { return true;}

	std::string getValueAlias(double frame, int precision);

	bool isAnimatable() const { return true; };
	bool isKeyframe(double frame) const;
	void deleteKeyframe(double frame);
	void clearKeyframes();
	void assignKeyframe(double frame, const TSmartPointerT<TParam> &src,
						double srcFrame, bool changedOnly = false);

	void getKeyframes(std::set<double> &frames) const;
	bool hasKeyframes() const;
	int getNextKeyframe(double frame) const;
	int getPrevKeyframe(double frame) const;
	double keyframeIndexToFrame(int index) const;

	void loadData(TIStream &is);
	void saveData(TOStream &os);
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<TToneCurveParam>;
template class DVAPI TDerivedSmartPointerT<TToneCurveParam, TParam>;
#endif

class DVAPI TToneCurveParamP : public TDerivedSmartPointerT<TToneCurveParam, TParam>
{
public:
	TToneCurveParamP() {}
	TToneCurveParamP(TToneCurveParam *p) : TDerivedSmartPointerT<TToneCurveParam, TParam>(p) {}
	TToneCurveParamP(const TParamP &p) : TDerivedSmartPointerT<TToneCurveParam, TParam>(p) {}
	operator TParamP() const { return TParamP(m_pointer); }
};

#endif
