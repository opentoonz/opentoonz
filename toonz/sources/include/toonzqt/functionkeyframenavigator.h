#pragma once

#ifndef FUNCTION_KEYFRAME_NAVIGATOR_INCLUDED
#define FUNCTION_KEYFRAME_NAVIGATOR_INCLUDED

#include "toonzqt/keyframenavigator.h"
#include "tdoubleparam.h"

#include <QToolBar>
class FunctionPanel;
class TFrameHandle;
class FrameNavigator;

class DVAPI FunctionKeyframeNavigator : public KeyframeNavigator
{
	Q_OBJECT
	TDoubleParamP m_curve;

public:
	FunctionKeyframeNavigator(QWidget *parent);

	void setCurve(TDoubleParam *curve);

protected:
	bool hasNext() const;
	bool hasPrev() const;
	bool hasKeyframes() const;
	bool isKeyframe() const;
	bool isFullKeyframe() const { return isKeyframe(); }
	void toggle();
	void goNext();
	void goPrev();

	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

public slots:
	void onFrameSwitched() { update(); }
};

#endif
