#pragma once

#ifndef GLOBALCONTROLLABLEFX_H
#define GLOBALCONTROLLABLEFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tfxattributes.h"

class GlobalControllableFx : public TStandardRasterFx {
protected:
  TDoubleParamP m_globalIntensity;

public:
  GlobalControllableFx() : m_globalIntensity(1.0) {
    m_globalIntensity->setValueRange(0.0, 1.0);

    bindParam(this, "globalIntensity", m_globalIntensity);
    getAttributes()->setHasGlobalControl(true);
    m_globalIntensity->setUILabel("Fx Intensity");
  }

  double getGrobalControlValue(double frame) {
    return m_globalIntensity->getValue(frame);
  }
};

#endif
