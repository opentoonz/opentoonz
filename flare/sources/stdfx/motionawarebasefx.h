#pragma once

#ifndef MOTIONAWAREBASEFX_H
#define MOTIONAWAREBASEFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tfxattributes.h"

enum MotionObjectType {
  OBJTYPE_OWN = 0, /*-- 自分自身の動きでぼかす --*/
  OBJTYPE_COLUMN,
  OBJTYPE_PEGBAR,
  OBJTYPE_TABLE,
  OBJTYPE_CAMERA
};

class MotionAwareBaseFx : public TStandardRasterFx {
protected:
  TDoubleParamP
      m_shutterStart; /*-- 現時点より前のシャッター解放時間 (単位Frame) --*/
  TDoubleParamP
      m_shutterEnd; /*-- 現時点より後のシャッター解放時間 (単位Frame) --*/
  TIntParamP m_traceResolution;
  /*-- 他のカラム/Pegの動きを参照できるように --*/
  TIntEnumParamP m_motionObjectType;
  TIntParamP m_motionObjectIndex;

public:
  MotionAwareBaseFx()
      : m_shutterStart(0.05)
      , m_shutterEnd(0.05)
      , m_traceResolution(4)
      , m_motionObjectType(new TIntEnumParam(OBJTYPE_OWN, "Own Motion"))
      , m_motionObjectIndex(1) {
    m_shutterStart->setValueRange(0.0, 1.0);
    m_shutterEnd->setValueRange(0.0, 1.0);
    m_traceResolution->setValueRange(1, 20);
    m_motionObjectType->addItem(OBJTYPE_COLUMN, "Column");
    m_motionObjectType->addItem(OBJTYPE_PEGBAR, "Pegbar");
    m_motionObjectType->addItem(OBJTYPE_TABLE, "Table");
    m_motionObjectType->addItem(OBJTYPE_CAMERA, "Camera");

    getAttributes()->setIsSpeedAware(true);
  }

  /*-- 軌跡情報を得るのに必要なパラメータを取得させる --*/
  TDoubleParamP getShutterStart() { return m_shutterStart; }
  TDoubleParamP getShutterEnd() { return m_shutterEnd; }
  TIntParamP getTraceResolution() { return m_traceResolution; }
  MotionObjectType getMotionObjectType() {
    return (MotionObjectType)m_motionObjectType->getValue();
  }
  TIntParamP getMotionObjectIndex() { return m_motionObjectIndex; }
};

// flow motion blurで使う

class MotionAwareAffineFx : public TStandardZeraryFx {
protected:
  TDoubleParamP m_shutterLength;  // 前後のシャッター解放時間

  TIntEnumParamP m_motionObjectType;
  TIntParamP m_motionObjectIndex;

public:
  MotionAwareAffineFx()
      : m_shutterLength(0.1)
      , m_motionObjectType(new TIntEnumParam(OBJTYPE_OWN, "Own Motion"))
      , m_motionObjectIndex(1) {
    m_shutterLength->setValueRange(0.01, 1.0);
    m_motionObjectType->addItem(OBJTYPE_COLUMN, "Column");
    m_motionObjectType->addItem(OBJTYPE_PEGBAR, "Pegbar");
    m_motionObjectType->addItem(OBJTYPE_TABLE, "Table");
    m_motionObjectType->addItem(OBJTYPE_CAMERA, "Camera");

    getAttributes()->setIsSpeedAware(true);
  }

  /*-- 軌跡情報を得るのに必要なパラメータを取得させる --*/
  TDoubleParamP getShutterLength() { return m_shutterLength; }
  MotionObjectType getMotionObjectType() {
    return (MotionObjectType)m_motionObjectType->getValue();
  }
  TIntParamP getMotionObjectIndex() { return m_motionObjectIndex; }
};

#endif
