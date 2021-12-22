#pragma once
#ifndef TEXTAWAREBASEFX_H
#define TEXTAWAREBASEFX_H

#include "tfxparam.h"
#include "stdfx.h"

class TextAwareBaseFx : public TStandardZeraryFx {
protected:
  QString m_noteLevelStr;

  TIntEnumParamP m_targetType;
  TIntParamP m_columnIndex;

public:
  enum SourceType { NEARBY_COLUMN, SPECIFIED_COLUMN, INPUT_TEXT };

  TextAwareBaseFx()
      : m_targetType(new TIntEnumParam(NEARBY_COLUMN, "Nearby Note Column"))
      , m_columnIndex(0) {
    m_targetType->addItem(SPECIFIED_COLUMN, "Specified Note Column");
    m_targetType->addItem(INPUT_TEXT, "Input Text");
  }

  bool isZerary() const override { return true; }

  void setNoteLevelStr(QString str) { m_noteLevelStr = str; }
  SourceType getSourceType() { return (SourceType)(m_targetType->getValue()); }
  int getNoteColumnIndex() { return m_columnIndex->getValue() - 1; }
};

#endif
