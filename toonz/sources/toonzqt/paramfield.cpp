

#include "toonzqt/paramfield.h"
#include "toonzqt/gutil.h"
#include "toonzqt/fxsettings.h"
#include "toonzqt/intfield.h"
#include "toonzqt/spectrumfield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/doublepairfield.h"
#include "toonzqt/tonecurvefield.h"
#include "toonzqt/checkbox.h"

#include "tdoubleparam.h"
#include "tnotanimatableparam.h"
#include "tparamset.h"
#include "tw/stringtable.h"

#include <QString>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QComboBox>

using namespace DVGui;

//-----------------------------------------------------------------------------
/*! FxSettingsに共通のUndo
*/
class FxSettingsUndo : public TUndo {
protected:
  TFxHandle *m_fxHandle;
  QString m_name;

public:
  FxSettingsUndo(QString name, TFxHandle *fxHandle)
      : m_name(name), m_fxHandle(fxHandle) {}

  int getSize() const override { return sizeof(*this); }
  int getHistoryType() override { return HistoryType::Fx; }
};

class AnimatableFxSettingsUndo : public FxSettingsUndo {
protected:
  bool m_wasKeyframe;
  int m_frame;

public:
  AnimatableFxSettingsUndo(QString name, int frame, TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle), m_frame(frame) {}

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Param : %1").arg(m_name);
    if (m_wasKeyframe)
      str += QString("  Frame : %1").arg(QString::number(m_frame + 1));
    else
      str += QString("  (Default Value)");
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! MeasuredDoubleParamField Undo
*/
class MeasuredDoubleParamFieldUndo final : public AnimatableFxSettingsUndo {
  TDoubleParamP m_param;
  double m_oldValue, m_newValue;

public:
  MeasuredDoubleParamFieldUndo(const TDoubleParamP param, QString name,
                               int frame, TFxHandle *fxHandle)
      : AnimatableFxSettingsUndo(name, frame, fxHandle), m_param(param) {
    m_oldValue    = param->getValue(frame);
    m_newValue    = m_oldValue;
    m_wasKeyframe = m_param->isKeyframe(frame);
  }

  void onAdd() override { m_newValue = m_param->getValue(m_frame); }

  void undo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_oldValue);
    else
      m_param->setValue(m_frame, m_oldValue);

    if (m_fxHandle) {
      m_fxHandle->notifyFxChanged();
    }
  }

  void redo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_newValue);
    else
      m_param->setValue(m_frame, m_newValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }
};

//-----------------------------------------------------------------------------
/*! RangeParamField Undo
*/
class RangeParamFieldUndo final : public AnimatableFxSettingsUndo {
  TRangeParamP m_param;
  DoublePair m_oldValue, m_newValue;

public:
  RangeParamFieldUndo(const TRangeParamP param, QString name, int frame,
                      TFxHandle *fxHandle)
      : AnimatableFxSettingsUndo(name, frame, fxHandle), m_param(param) {
    m_oldValue    = param->getValue(frame);
    m_newValue    = m_oldValue;
    m_wasKeyframe = m_param->isKeyframe(frame);
  }

  void onAdd() override { m_newValue = m_param->getValue(m_frame); }

  void undo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_oldValue);
    else
      m_param->setValue(m_frame, m_oldValue);

    if (m_fxHandle) {
      m_fxHandle->notifyFxChanged();
    }
  }

  void redo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_newValue);
    else
      m_param->setValue(m_frame, m_newValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }
};

//-----------------------------------------------------------------------------
/*! PixelParamField Undo
*/
class PixelParamFieldUndo final : public AnimatableFxSettingsUndo {
  TPixelParamP m_param;
  TPixel32 m_oldValue, m_newValue;

public:
  PixelParamFieldUndo(const TPixelParamP param, QString name, int frame,
                      TFxHandle *fxHandle)
      : AnimatableFxSettingsUndo(name, frame, fxHandle), m_param(param) {
    m_oldValue    = param->getValue(frame);
    m_newValue    = m_oldValue;
    m_wasKeyframe = m_param->isKeyframe(frame);
  }

  void onAdd() override { m_newValue = m_param->getValue(m_frame); }

  void undo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_oldValue);
    else
      m_param->setValue(m_frame, m_oldValue);

    if (m_fxHandle) {
      m_fxHandle->notifyFxChanged();
    }
  }

  void redo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_newValue);
    else
      m_param->setValue(m_frame, m_newValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }
};

//-----------------------------------------------------------------------------
/*! PointParamField Undo
*/
class PointParamFieldUndo final : public AnimatableFxSettingsUndo {
  TPointParamP m_param;
  TPointD m_oldValue, m_newValue;

public:
  PointParamFieldUndo(const TPointParamP param, QString name, int frame,
                      TFxHandle *fxHandle)
      : AnimatableFxSettingsUndo(name, frame, fxHandle), m_param(param) {
    m_oldValue    = param->getValue(frame);
    m_newValue    = m_oldValue;
    m_wasKeyframe = m_param->isKeyframe(frame);
  }

  void onAdd() override { m_newValue = m_param->getValue(m_frame); }

  void undo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_oldValue);
    else
      m_param->setValue(m_frame, m_oldValue);

    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_newValue);
    else
      m_param->setValue(m_frame, m_newValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }
};

//-----------------------------------------------------------------------------
/*! EnumParamField Undo
*/
class EnumParamFieldUndo final : public FxSettingsUndo {
  TIntEnumParamP m_param;
  std::string m_oldString, m_newString;

public:
  EnumParamFieldUndo(const TIntEnumParamP param, std::string oldString,
                     std::string newString, QString name, TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle)
      , m_param(param)
      , m_oldString(oldString)
      , m_newString(newString) {}

  void undo() const override {
    m_param->setValue(m_oldString);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    m_param->setValue(m_newString);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Param : %1 : %2 -> %3")
                      .arg(m_name)
                      .arg(QString::fromStdString(m_oldString))
                      .arg(QString::fromStdString(m_newString));
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! IntParamFieldのUndo
*/
class IntParamFieldUndo final : public FxSettingsUndo {
  TIntParamP m_param;
  int m_oldValue, m_newValue;

public:
  IntParamFieldUndo(const TIntParamP param, QString name, TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle), m_param(param) {
    m_oldValue = param->getValue();
    m_newValue = m_oldValue;
  }

  void onAdd() override { m_newValue = m_param->getValue(); }

  void undo() const override {
    m_param->setValue(m_oldValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    m_param->setValue(m_newValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Param : %1 : %2 -> %3")
                      .arg(m_name)
                      .arg(QString::number(m_oldValue))
                      .arg(QString::number(m_newValue));
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! BoolParamFieldのUndo
*/
class BoolParamFieldUndo final : public FxSettingsUndo {
  TBoolParamP m_param;
  bool m_newState;

public:
  BoolParamFieldUndo(const TBoolParamP param, QString name, TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle), m_param(param) {
    m_newState = param->getValue();
  }

  void undo() const override {
    m_param->setValue(!m_newState);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    m_param->setValue(m_newState);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Param : ");
    if (m_newState)
      str += QObject::tr("ON : %1").arg(m_name);
    else
      str += QObject::tr("OFF : %1").arg(m_name);
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! SpectrumParamFieldのUndo
*/
class SpectrumParamFieldUndo final : public AnimatableFxSettingsUndo {
  TSpectrumParamP m_param;
  TSpectrum m_oldSpectrum, m_newSpectrum;

public:
  SpectrumParamFieldUndo(const TSpectrumParamP param, QString name, int frame,
                         TFxHandle *fxHandle)
      : AnimatableFxSettingsUndo(name, frame, fxHandle), m_param(param) {
    m_oldSpectrum = param->getValue(frame);
    m_newSpectrum = m_oldSpectrum;
    m_wasKeyframe = m_param->isKeyframe(frame);
  }

  void onAdd() override { m_newSpectrum = m_param->getValue(m_frame); }

  void undo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_oldSpectrum);
    else
      m_param->setValue(m_frame, m_oldSpectrum);

    if (m_fxHandle) {
      m_fxHandle->notifyFxChanged();
    }
  }

  void redo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_newSpectrum);
    else
      m_param->setValue(m_frame, m_newSpectrum);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }
};

//-----------------------------------------------------------------------------
/*! SpectrumParamField Undo
        ※
   SpectrumParamFieldは、表示更新時にactualParamとcurrentParamのKeyの数が
        一致していなくてはならないので、２つ同時に変更する必要が有る。
*/
class SpectrumParamFieldAddRemoveKeyUndo final : public FxSettingsUndo {
  TSpectrumParamP m_actualParam;
  TSpectrumParamP m_currentParam;
  TSpectrum::ColorKey m_key;
  int m_index;

  bool m_isAddUndo;

public:
  SpectrumParamFieldAddRemoveKeyUndo(const TSpectrumParamP actualParam,
                                     const TSpectrumParamP currentParam,
                                     TSpectrum::ColorKey key, int index,
                                     bool isAddUndo, QString name,
                                     TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle)
      , m_actualParam(actualParam)
      , m_currentParam(currentParam)
      , m_key(key)
      , m_index(index)
      , m_isAddUndo(isAddUndo) {}

  void removeKeys() const {
    m_actualParam->removeKey(m_index);
    m_currentParam->removeKey(m_index);
  }

  void addKeys() const {
    m_actualParam->insertKey(m_index, m_key.first, m_key.second);
    m_currentParam->insertKey(m_index, m_key.first, m_key.second);
  }

  void undo() const override {
    if (m_isAddUndo)
      removeKeys();
    else
      addKeys();

    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    if (m_isAddUndo)
      addKeys();
    else
      removeKeys();
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str =
        QObject::tr("Modify Fx Param : %1 : %2 Key")
            .arg(m_name)
            .arg((m_isAddUndo) ? QObject::tr("Add") : QObject::tr("Remove"));
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! StringParamField Undo
*/
class StringParamFieldUndo final : public FxSettingsUndo {
  TStringParamP m_param;
  std::wstring m_oldValue, m_newValue;

public:
  StringParamFieldUndo(const TStringParamP param, QString name,
                       TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle), m_param(param) {
    m_oldValue = param->getValue();
    m_newValue = m_oldValue;
  }

  void onAdd() override { m_newValue = m_param->getValue(); }

  void undo() const override {
    m_param->setValue(m_oldValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    m_param->setValue(m_newValue);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Param : %1 : %2 -> %3")
                      .arg(m_name)
                      .arg(QString::fromStdWString(m_oldValue))
                      .arg(QString::fromStdWString(m_newValue));
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! ToneCurveParamField Undo
*/
class ToneCurveParamFieldUndo final : public AnimatableFxSettingsUndo {
  TToneCurveParamP m_param;
  QList<TPointD> m_oldPoints, m_newPoints;

public:
  ToneCurveParamFieldUndo(const TToneCurveParamP param, QString name, int frame,
                          TFxHandle *fxHandle)
      : AnimatableFxSettingsUndo(name, frame, fxHandle), m_param(param) {
    m_oldPoints   = param->getValue(frame);
    m_newPoints   = m_oldPoints;
    m_wasKeyframe = m_param->isKeyframe(frame);
  }

  void onAdd() override { m_newPoints = m_param->getValue(m_frame); }

  void undo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_oldPoints);
    else
      m_param->setValue(m_frame, m_oldPoints);

    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    if (!m_wasKeyframe)
      m_param->setDefaultValue(m_newPoints);
    else
      m_param->setValue(m_frame, m_newPoints);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }
};

//-----------------------------------------------------------------------------
/*! ToneCurveParamField Undo
        ※
   ToneCurveParamFieldは、表示更新時にactualParamとcurrentParamのPointの数が
        一致していなくてはならないので、２つ同時に変更する必要が有る。
*/

class ToneCurveParamFieldAddRemovePointUndo final : public FxSettingsUndo {
  TToneCurveParamP m_actualParam;
  TToneCurveParamP m_currentParam;
  QList<TPointD> m_value;
  int m_index;

  bool m_isAddUndo;

public:
  ToneCurveParamFieldAddRemovePointUndo(const TToneCurveParamP actualParam,
                                        const TToneCurveParamP currentParam,
                                        QList<TPointD> value, int index,
                                        bool isAddUndo, QString name,
                                        TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle)
      , m_actualParam(actualParam)
      , m_currentParam(currentParam)
      , m_value(value)
      , m_index(index)
      , m_isAddUndo(isAddUndo) {}

  void removePoints() const {
    m_actualParam->removeValue(0, m_index);
    m_currentParam->removeValue(0, m_index);
  }

  void addPoints() const {
    m_actualParam->addValue(0, m_value, m_index);
    m_currentParam->addValue(0, m_value, m_index);
  }

  void undo() const override {
    if (m_isAddUndo)
      removePoints();
    else
      addPoints();

    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    if (m_isAddUndo)
      addPoints();
    else
      removePoints();
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str =
        QObject::tr("Modify Fx Param : %1 : %2 Point")
            .arg(m_name)
            .arg((m_isAddUndo) ? QObject::tr("Add") : QObject::tr("Remove"));
    return str;
  }
};

//-----------------------------------------------------------------------------
/*! ToneCurveParamField Undo (Linearのトグル)
*/
class ToneCurveParamFieldToggleLinearUndo final : public FxSettingsUndo {
  TToneCurveParamP m_actualParam;
  TToneCurveParamP m_currentParam;
  bool m_newState;

public:
  ToneCurveParamFieldToggleLinearUndo(const TToneCurveParamP actualParam,
                                      const TToneCurveParamP currentParam,
                                      QString name, TFxHandle *fxHandle)
      : FxSettingsUndo(name, fxHandle)
      , m_actualParam(actualParam)
      , m_currentParam(currentParam) {
    m_newState = actualParam->isLinear();
  }

  void undo() const override {
    m_actualParam->setIsLinear(!m_newState);
    m_currentParam->setIsLinear(!m_newState);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    m_actualParam->setIsLinear(m_newState);
    m_currentParam->setIsLinear(m_newState);
    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Param : ");
    if (m_newState)
      str += QObject::tr("%1 : Linear ON").arg(m_name);
    else
      str += QObject::tr("%1 : Linear OFF").arg(m_name);
    return str;
  }
};

//=============================================================================
// ParamField
//-----------------------------------------------------------------------------

ParamField::ParamField(QWidget *parent, QString paramName, const TParamP &param,
                       bool addEmptyLabel)
    : QWidget(parent)
    , m_paramName(paramName)
    , m_interfaceName(param->hasUILabel()
                          ? QString::fromStdString(param->getUILabel())
                          : paramName)
    , m_description(QString::fromStdString(param->getDescription())) {
  QString str;
  m_layout = new QHBoxLayout(this);
  m_layout->setMargin(0);
  m_layout->setSpacing(5);
}

//-----------------------------------------------------------------------------

ParamField::~ParamField() {}

//-----------------------------------------------------------------------------

TFxHandle *ParamField::m_fxHandleStat = 0;
void ParamField::setFxHandle(TFxHandle *fxHandle) {
  ParamField::m_fxHandleStat = fxHandle;
}

//=============================================================================
// ParamFieldKeyToggle
//-----------------------------------------------------------------------------

ParamFieldKeyToggle::ParamFieldKeyToggle(QWidget *parent, std::string name)
    : QWidget(parent), m_status(NOT_ANIMATED), m_highlighted(false) {
  setFixedSize(15, 15);
}

//-----------------------------------------------------------------------------

void ParamFieldKeyToggle::setStatus(Status status) {
  m_status = status;
  update();
}

//-----------------------------------------------------------------------------

void ParamFieldKeyToggle::setStatus(bool hasKeyframes, bool isKeyframe,
                                    bool hasBeenChanged) {
  if (!hasKeyframes)
    setStatus(NOT_ANIMATED);
  else if (hasBeenChanged)
    setStatus(MODIFIED);
  else if (isKeyframe)
    setStatus(KEYFRAME);
  else
    setStatus(NOT_KEYFRAME);
}

//-----------------------------------------------------------------------------

ParamFieldKeyToggle::Status ParamFieldKeyToggle::getStatus() const {
  return m_status;
}

//-----------------------------------------------------------------------------

void ParamFieldKeyToggle::paintEvent(QPaintEvent *e) {
  QPainter p(this);

  switch (m_status) {
  case NOT_ANIMATED:
    p.drawPixmap(rect(),
                 QPixmap(svgToPixmap(":Resources/keyframe_noanim.svg")));
    break;
  case KEYFRAME:
    p.drawPixmap(rect(), QPixmap(svgToPixmap(":Resources/keyframe_key.svg")));
    break;
  case MODIFIED:
    p.drawPixmap(rect(),
                 QPixmap(svgToPixmap(":Resources/keyframe_modified.svg")));
    break;
  default:
    p.drawPixmap(rect(),
                 QPixmap(svgToPixmap(":Resources/keyframe_inbetween.svg")));
    break;
  }
  if (m_highlighted) {
    p.fillRect(rect(), QBrush(QColor(50, 100, 255, 100)));
  }
}

//-----------------------------------------------------------------------------

void ParamFieldKeyToggle::mousePressEvent(QMouseEvent *e) { emit keyToggled(); }

//-----------------------------------------------------------------------------

void ParamFieldKeyToggle::enterEvent(QEvent *) {
  m_highlighted = true;
  update();
}

//-----------------------------------------------------------------------------

void ParamFieldKeyToggle::leaveEvent(QEvent *) {
  m_highlighted = false;
  update();
}

//=============================================================================
// MeasuredDoubleParamField
//-----------------------------------------------------------------------------

MeasuredDoubleParamField::MeasuredDoubleParamField(QWidget *parent,
                                                   QString name,
                                                   const TDoubleParamP &param)
    : AnimatedParamField<double, TDoubleParamP>(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());

  m_measuredDoubleField = new MeasuredDoubleField(this, false);

  m_measuredDoubleField->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Minimum);
  m_measuredDoubleField->setMeasure(param->getMeasureName());
  m_measuredDoubleField->setValue(param->getValue(m_frame));
  m_measuredDoubleField->setDecimals(3);

  double min = 0, max = 100, step = 1;
  param->getValueRange(min, max, step);
  assert(min < max);
  m_measuredDoubleField->setRange(min, max);

  //----layout
  m_layout->addWidget(m_keyToggle);
  m_layout->addWidget(m_measuredDoubleField);
  setLayout(m_layout);

  //----signal-slot connection
  bool ret = connect(m_measuredDoubleField, SIGNAL(valueChanged(bool)),
                     SLOT(onChange(bool)));
  ret = ret && connect(m_keyToggle, SIGNAL(keyToggled()), SLOT(onKeyToggled()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void MeasuredDoubleParamField::updateField(double value) {
  m_measuredDoubleField->setValue(value);
}

//-----------------------------------------------------------------------------

void MeasuredDoubleParamField::onChange(bool dragging) {
  if (dragging) return;

  TDoubleParamP doubleParam = m_actualParam;

  TUndo *undo = 0;
  /*-- Undoを登録する条件：
          値が変更されていて、かつ
          キーフレーム上か、または、まだキーフレームが無い
          （すなわち、実際にシーンの情報を変えることになる）場合
  --*/
  if (doubleParam &&
      doubleParam->getValue(m_frame) != m_measuredDoubleField->getValue() &&
      (m_actualParam->isKeyframe(m_frame) ||
       !m_actualParam.getPointer()->hasKeyframes()))
    undo = new MeasuredDoubleParamFieldUndo(
        doubleParam, m_interfaceName, m_frame, ParamField::m_fxHandleStat);

  setValue(m_measuredDoubleField->getValue());

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void MeasuredDoubleParamField::onKeyToggled() { onKeyToggle(); }

//=============================================================================
// MeasuredRangeParamField
//-----------------------------------------------------------------------------

MeasuredRangeParamField::MeasuredRangeParamField(QWidget *parent, QString name,
                                                 const TRangeParamP &param)
    : AnimatedParamField<DoublePair, TRangeParamP>(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());

  m_valueField = new MeasuredDoublePairField(this);
  m_valueField->setLabelsEnabled(false);
  m_valueField->setMeasure(param->getMin()->getMeasureName());
  m_valueField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  double a, b, c, min, max;
  param->getMin()->getValueRange(a, b, c);
  min = a;
  param->getMax()->getValueRange(a, b, c);
  max = b;
  if (min < max && max - min < 1e10)  // se min=-inf e max=inf
    m_valueField->setRange(min, max);

  m_layout->addWidget(m_keyToggle);
  m_layout->addWidget(m_valueField);
  setLayout(m_layout);

  bool ret =
      connect(m_valueField, SIGNAL(valuesChanged(bool)), SLOT(onChange(bool)));
  ret = ret && connect(m_keyToggle, SIGNAL(keyToggled()), SLOT(onKeyToggled()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void MeasuredRangeParamField::updateField(DoublePair value) {
  m_valueField->setValues(value);
}

//-----------------------------------------------------------------------------

void MeasuredRangeParamField::onChange(bool dragging) {
  if (dragging) return;

  TRangeParamP rangeParam = m_actualParam;

  DoublePair value = m_valueField->getValues();

  TUndo *undo = 0;

  /*-- Undoを登録する条件：
          値が変更されていて
          キーフレーム上か、または、まだキーフレームが無い
          （すなわち、実際にシーンの情報を変えることになる）場合
  --*/
  if (rangeParam && rangeParam->getValue(m_frame) != value &&
      (m_actualParam->isKeyframe(m_frame) ||
       !m_actualParam.getPointer()->hasKeyframes()))
    undo = new RangeParamFieldUndo(rangeParam, m_interfaceName, m_frame,
                                   ParamField::m_fxHandleStat);

  setValue(value);

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void MeasuredRangeParamField::onKeyToggled() { onKeyToggle(); }

//-----------------------------------------------------------------------------

void MeasuredRangeParamField::setPrecision(int precision) {
  m_valueField->setPrecision(precision);
}

//=============================================================================
// PointParamField
//-----------------------------------------------------------------------------

PointParamField::PointParamField(QWidget *parent, QString name,
                                 const TPointParamP &param)
    : AnimatedParamField<TPointD, TPointParamP>(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());

  QLabel *xLabel = new QLabel(tr("X:"), this);
  m_xFld         = new MeasuredDoubleField(this, false);
  QLabel *yLabel = new QLabel(tr("Y:"), this);
  m_yFld         = new MeasuredDoubleField(this, false);

  double xmin = -(std::numeric_limits<double>::max)(),
         xmax = (std::numeric_limits<double>::max)();
  double ymin = -(std::numeric_limits<double>::max)(),
         ymax = (std::numeric_limits<double>::max)();

#if 1
  /* これを有効にすれば PointParamField に範囲が設定できることが UI
     の見た目が変わってしまう.
     これまで誰も TPointParam に対して range
     を設定していないなら(どうせ効いてなかったのだから)無条件に設定してもよさそうだが
     実際は Pinned Texture などの FX
     が設定しており(効いてないなかったが、この修正により)動作が変わってしまうので
     plugin から要求された場合でのみ range を有効にする. */
  if (param->isFromPlugin()) {
    double xstep, ystep;
    param->getX()->getValueRange(xmin, xmax, xstep);
    param->getY()->getValueRange(ymin, ymax, ystep);
  }
#endif

  m_xFld->setMaximumWidth(100);
  m_xFld->setRange(xmin, xmax);
  m_xFld->setMeasure(param->getX()->getMeasureName());
  m_xFld->setValue(param->getX()->getValue(m_frame));

  m_yFld->setMaximumWidth(100);
  m_yFld->setRange(ymin, ymax);
  m_yFld->setMeasure(param->getY()->getMeasureName());
  m_yFld->setValue(param->getY()->getValue(m_frame));

  //----layout
  m_layout->addWidget(m_keyToggle);
  m_layout->addWidget(xLabel);
  m_layout->addWidget(m_xFld);

  m_layout->addSpacing(5);

  m_layout->addWidget(yLabel);
  m_layout->addWidget(m_yFld);

  m_layout->addStretch(1);

  setLayout(m_layout);

  //----signal-slot connections
  bool ret =
      connect(m_xFld, SIGNAL(valueChanged(bool)), this, SLOT(onChange(bool)));
  ret = ret &&
        connect(m_yFld, SIGNAL(valueChanged(bool)), this, SLOT(onChange(bool)));
  ret = ret && connect(m_keyToggle, SIGNAL(keyToggled()), SLOT(onKeyToggled()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void PointParamField::setPointValue(const TPointD &p) {
  m_xFld->setValue(p.x);
  m_yFld->setValue(p.y);
  setValue(p);
}

//-----------------------------------------------------------------------------

void PointParamField::updateField(TPointD value) {
  m_xFld->setValue(value.x);
  m_yFld->setValue(value.y);
}

//-----------------------------------------------------------------------------

void PointParamField::onChange(bool dragging) {
  if (dragging) return;

  TPointParamP pointParam = m_actualParam;
  TPointD pos(m_xFld->getValue(), m_yFld->getValue());

  TUndo *undo = 0;
  /*-- Undoを登録する条件：
          値が変更されていて
          キーフレーム上か、または、まだキーフレームが無い
          （すなわち、実際にシーンの情報を変えることになる）場合
  --*/
  if (pointParam && pointParam->getValue(m_frame) != pos &&
      (m_actualParam->isKeyframe(m_frame) ||
       !m_actualParam.getPointer()->hasKeyframes()))
    undo = new PointParamFieldUndo(pointParam, m_interfaceName, m_frame,
                                   ParamField::m_fxHandleStat);

  setValue(pos);

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void PointParamField::onKeyToggled() { onKeyToggle(); }

//=============================================================================
// PixelParamField
//-----------------------------------------------------------------------------

PixelParamField::PixelParamField(QWidget *parent, QString name,
                                 const TPixelParamP &param)
    : AnimatedParamField<TPixel32, TPixelParamP>(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());

  m_colorField = new ColorField(this, param->isMatteEnabled());

  //----layout
  m_layout->addWidget(m_keyToggle);
  m_layout->addWidget(m_colorField);
  m_layout->addStretch();
  setLayout(m_layout);

  //----signal-slot connections
  bool ret = connect(m_colorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
                     this, SLOT(onChange(const TPixel32 &, bool)));
  ret = ret && connect(m_keyToggle, SIGNAL(keyToggled()), SLOT(onKeyToggled()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void PixelParamField::updateField(TPixel32 value) {
  m_colorField->setColor(value);
}

//-----------------------------------------------------------------------------

void PixelParamField::onChange(const TPixel32 &value, bool isDragging) {
  if (isDragging) return;

  TPixelParamP pixelParam = m_actualParam;

  TUndo *undo = 0;
  /*-- Undoを登録する条件:
          値が変更されていて
          キーフレーム上か、または、まだキーフレームが無い
          （すなわち、実際にシーンの情報を変えることになる）場合
  --*/
  if (pixelParam && pixelParam->getValue(m_frame) != value &&
      (m_actualParam->isKeyframe(m_frame) ||
       !m_actualParam.getPointer()->hasKeyframes()))
    undo = new PixelParamFieldUndo(pixelParam, m_interfaceName, m_frame,
                                   ParamField::m_fxHandleStat);

  setValue(value);

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void PixelParamField::onKeyToggled() { onKeyToggle(); }

//-----------------------------------------------------------------------------

TPixel32 PixelParamField::getColor() { return m_colorField->getColor(); }

//-----------------------------------------------------------------------------

void PixelParamField::setColor(TPixel32 value) {
  m_colorField->setColor(value);
  setValue(value);
}

//=============================================================================
// RGB Link Button
//-----------------------------------------------------------------------------

RgbLinkButton::RgbLinkButton(QString str, QWidget *parent,
                             PixelParamField *field1, PixelParamField *field2)
    : QPushButton(str, parent), m_field1(field1), m_field2(field2) {}

//-----------------------------------------------------------------------------

void RgbLinkButton::onButtonClicked() {
  if (!m_field1 || !m_field2) return;
  TPixel32 val1 = m_field1->getColor();
  TPixel32 val2 = m_field2->getColor();

  /*-- Alphaは変えない --*/
  val1.m = val2.m;

  if (val1 == val2) return;

  m_field2->setColor(val1);
}

//=============================================================================
// SpectrumParamField
//-----------------------------------------------------------------------------

SpectrumParamField::SpectrumParamField(QWidget *parent, QString name,
                                       const TSpectrumParamP &param)
    : AnimatedParamField<TSpectrum, TSpectrumParamP>(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());

  m_spectrumField = new SpectrumField(this);
  m_spectrumField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_spectrumField->setCurrentKeyIndex(0);

  //--- layout
  m_layout->addWidget(m_keyToggle);
  m_layout->addWidget(m_spectrumField);
  setLayout(m_layout);

  //--- signal-slot connections
  bool ret = true;
  ret = ret && connect(m_spectrumField, SIGNAL(keyColorChanged(bool)), this,
                       SLOT(onChange(bool)));
  ret = ret && connect(m_spectrumField, SIGNAL(keyPositionChanged(bool)), this,
                       SLOT(onChange(bool)));
  ret = ret && connect(m_spectrumField, SIGNAL(keyAdded(int)), this,
                       SLOT(onKeyAdded(int)));
  ret = ret && connect(m_spectrumField, SIGNAL(keyRemoved(int)), this,
                       SLOT(onKeyRemoved(int)));
  ret = ret && connect(m_keyToggle, SIGNAL(keyToggled()), SLOT(onKeyToggled()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void SpectrumParamField::updateField(TSpectrum value) {
  if (m_spectrumField->getSpectrum() == value) return;
  m_spectrumField->setSpectrum(value);
}

//-----------------------------------------------------------------------------

void SpectrumParamField::setParams() {
  TSpectrum spectrum = m_spectrumField->getSpectrum();
  // if(m_currentParam->getValue(0) == spectrum) return; //Rivedi quando sistemi
  // lo SwatchViewer

  m_currentParam->setValue(m_frame, spectrum);
  if (m_actualParam->isKeyframe(m_frame)) {
    m_actualParam->setValue(m_frame, spectrum);
    emit actualParamChanged();
  } else if (!m_actualParam->hasKeyframes()) {
    m_actualParam->setDefaultValue(spectrum);
    emit actualParamChanged();
  }
  updateKeyToggle();

  emit currentParamChanged();
}

//-----------------------------------------------------------------------------

void SpectrumParamField::onKeyToggled() { onKeyToggle(); }

//-----------------------------------------------------------------------------

void SpectrumParamField::onChange(bool isDragging) {
  if (isDragging) return;

  TSpectrumParamP spectrumParam = m_actualParam;
  TUndo *undo                   = 0;
  if (spectrumParam &&
      spectrumParam->getValue(m_frame) != m_spectrumField->getSpectrum() &&
      (m_actualParam->isKeyframe(m_frame) ||
       !m_actualParam.getPointer()->hasKeyframes()))
    undo = new SpectrumParamFieldUndo(spectrumParam, m_interfaceName, m_frame,
                                      ParamField::m_fxHandleStat);

  setParams();

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void SpectrumParamField::onKeyAdded(int keyIndex) {
  TSpectrum::ColorKey key = m_spectrumField->getSpectrum().getKey(keyIndex);

  TSpectrumParamP actualSpectrumParam = m_actualParam;
  assert(actualSpectrumParam);
  actualSpectrumParam->addKey(key.first, key.second);

  TSpectrumParamP currentSpectrumParam = m_currentParam;
  assert(currentSpectrumParam);
  currentSpectrumParam->addKey(key.first, key.second);

  TUndoManager::manager()->add(new SpectrumParamFieldAddRemoveKeyUndo(
      actualSpectrumParam, currentSpectrumParam, key, keyIndex, true,
      m_interfaceName, ParamField::m_fxHandleStat));
}

//-----------------------------------------------------------------------------

void SpectrumParamField::onKeyRemoved(int keyIndex) {
  TUndo *undo                          = 0;
  TSpectrumParamP actualSpectrumParam  = m_actualParam;
  TSpectrumParamP currentSpectrumParam = m_currentParam;
  if (currentSpectrumParam && actualSpectrumParam) {
    TSpectrum::ColorKey key =
        actualSpectrumParam->getValue(m_frame).getKey(keyIndex);
    undo = new SpectrumParamFieldAddRemoveKeyUndo(
        actualSpectrumParam, currentSpectrumParam, key, keyIndex, false,
        m_interfaceName, ParamField::m_fxHandleStat);
  }

  m_currentParam->removeKey(keyIndex);
  m_actualParam->removeKey(keyIndex);

  setParams();

  if (undo) TUndoManager::manager()->add(undo);
}

//=============================================================================
// EnumParamField
//-----------------------------------------------------------------------------

EnumParamField::EnumParamField(QWidget *parent, QString name,
                               const TIntEnumParamP &param)
    : ParamField(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());
  m_om        = new QComboBox(this);
  m_om->setFixedHeight(20);

  for (int i = 0; i < param->getItemCount(); i++) {
    int value = 0;
    std::string caption;
    param->getItem(i, value, caption);
    QString str;
    m_om->addItem(str.fromStdString(caption));
  }
  connect(m_om, SIGNAL(activated(const QString &)), this,
          SLOT(onChange(const QString &)));
  m_layout->addWidget(m_om);

  m_layout->addStretch();

  setLayout(m_layout);
}

//-----------------------------------------------------------------------------

void EnumParamField::onChange(const QString &str) {
  TIntEnumParamP intEnumParam = m_actualParam;
  std::string newStdStr       = str.toStdString();
  TUndo *undo                 = 0;
  if (intEnumParam) {
    /*--クリックしただけで実際のカレントアイテムが変わっていない場合はreturn--*/
    std::string oldStr;
    for (int i = 0; i < intEnumParam->getItemCount(); i++) {
      int oldVal;
      intEnumParam->getItem(i, oldVal, oldStr);
      if (oldVal == intEnumParam->getValue()) {
        if (oldStr == newStdStr)
          return;
        else
          break;
      }
    }

    undo = new EnumParamFieldUndo(intEnumParam, oldStr, newStdStr,
                                  m_interfaceName, ParamField::m_fxHandleStat);
  }

  m_currentParam->setValue(newStdStr);
  m_actualParam->setValue(newStdStr);

  emit currentParamChanged();
  emit actualParamChanged();

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void EnumParamField::setParam(const TParamP &current, const TParamP &actual,
                              int frame) {
  m_currentParam = current;
  m_actualParam  = actual;
  assert(m_currentParam);
  assert(m_actualParam);
  update(frame);
}

//-----------------------------------------------------------------------------

void EnumParamField::update(int frame) {
  if (!m_actualParam || !m_currentParam) return;
  TIntEnumParamP param = m_actualParam;
  for (int i = 0; i < param->getItemCount(); i++) {
    int value = 0;
    std::string caption;
    param->getItem(i, value, caption);
    if (value != param->getValue()) continue;
    if (m_om->currentIndex() == i) return;
    m_om->setCurrentIndex(i);
    return;
  }
}

//=============================================================================
// BoolParamField
//-----------------------------------------------------------------------------

BoolParamField::BoolParamField(QWidget *parent, QString name,
                               const TBoolParamP &param)
    : ParamField(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());
  if (!param->hasUILabel()) m_interfaceName = name;

  m_checkBox = new CheckBox("", this);

  /*-- Undo時には反応しないように、toggledでなくclickedにする --*/
  connect(m_checkBox, SIGNAL(clicked(bool)), this, SLOT(onToggled(bool)));
  m_layout->addWidget(m_checkBox);
  m_layout->addStretch();
  setLayout(m_layout);

  /*-- visibleToggleインタフェースのため --*/
  connect(m_checkBox, SIGNAL(toggled(bool)), this, SIGNAL(toggled(bool)));
}

//-----------------------------------------------------------------------------

void BoolParamField::onToggled(bool checked) {
  m_actualParam->setValue(checked);
  m_currentParam->setValue(checked);

  emit currentParamChanged();
  emit actualParamChanged();

  TBoolParamP boolParam = m_actualParam;
  if (boolParam)
    TUndoManager::manager()->add(new BoolParamFieldUndo(
        boolParam, m_interfaceName, ParamField::m_fxHandleStat));
}

//-----------------------------------------------------------------------------

void BoolParamField::setParam(const TParamP &current, const TParamP &actual,
                              int frame) {
  m_currentParam = current;
  m_actualParam  = actual;
  assert(m_currentParam);
  assert(m_actualParam);
  update(frame);
}

//-----------------------------------------------------------------------------

void BoolParamField::update(int frame) {
  if (!m_actualParam || !m_currentParam) return;
  bool value = m_actualParam->getValue();
  if (m_checkBox->isChecked() == value) return;
  m_checkBox->setChecked(value);
}

//=============================================================================
// IntParamField
//-----------------------------------------------------------------------------

IntParamField::IntParamField(QWidget *parent, QString name,
                             const TIntParamP &param)
    : ParamField(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());
  m_intField  = new IntField(this, false);
  m_intField->setMaximumWidth(43);
  m_intField->enableSlider(false);
  m_intField->enableRoller(param->isWheelEnabled());
  int min, max;
  param->getValueRange(min, max);
  assert(min < max);
  m_intField->setRange(min, max);
  connect(m_intField, SIGNAL(valueChanged(bool)), SLOT(onChange(bool)));

  m_layout->addWidget(m_intField);
  m_layout->addStretch();

  setLayout(m_layout);
}

//-----------------------------------------------------------------------------

void IntParamField::onChange(bool isDragging) {
  if (isDragging) return;

  int value = m_intField->getValue();
  int min, max;
  m_intField->getRange(min, max);
  if (value > max)
    value = max;
  else if (value < min)
    value = min;

  TUndo *undo = 0;

  TIntParamP intParam = m_actualParam;
  if (intParam && intParam->getValue() != value)
    undo = new IntParamFieldUndo(intParam, m_interfaceName,
                                 ParamField::m_fxHandleStat);

  m_actualParam->setValue(value);
  emit currentParamChanged();
  m_currentParam->setValue(value);
  emit actualParamChanged();

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void IntParamField::setParam(const TParamP &current, const TParamP &actual,
                             int frame) {
  m_currentParam = current;
  m_actualParam  = actual;
  assert(m_currentParam);
  assert(m_actualParam);
  update(frame);
}

//-----------------------------------------------------------------------------

void IntParamField::update(int frame) {
  if (!m_actualParam || !m_currentParam) return;
  int value = m_actualParam->getValue();
  if (m_intField->getValue() == value) return;
  m_intField->setValue(value);
}

//=============================================================================
// StringParamField
//-----------------------------------------------------------------------------

StringParamField::StringParamField(QWidget *parent, QString name,
                                   const TStringParamP &param)
    : ParamField(parent, name, param) {
  QString str;
  m_paramName = str.fromStdString(param->getName());
  m_textFld   = new LineEdit(name, this);
  m_textFld->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  bool ret =
      connect(m_textFld, SIGNAL(editingFinished()), this, SLOT(onChange()));
  m_layout->addWidget(m_textFld);

  setLayout(m_layout);
  assert(ret);
}

//-----------------------------------------------------------------------------

void StringParamField::onChange() {
  std::wstring value = m_textFld->text().toStdWString();
  TUndo *undo        = 0;

  TStringParamP stringParam = m_actualParam;
  if (stringParam && stringParam->getValue() != value)
    undo = new StringParamFieldUndo(stringParam, m_interfaceName,
                                    ParamField::m_fxHandleStat);

  m_actualParam->setValue(value);
  m_currentParam->setValue(value);

  emit currentParamChanged();
  emit actualParamChanged();

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void StringParamField::setParam(const TParamP &current, const TParamP &actual,
                                int frame) {
  m_currentParam = current;
  m_actualParam  = actual;
  assert(m_currentParam);
  assert(m_actualParam);
  update(frame);
}

//-----------------------------------------------------------------------------

void StringParamField::update(int frame) {
  if (!m_actualParam || !m_currentParam) return;
  QString str;
  QString strValue = str.fromStdWString(m_actualParam->getValue());
  if (m_textFld->text() == strValue) return;
  m_textFld->setText(strValue);

  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa
  // da visualizzare e' piu' lunga del campo le cifre che vengono troncate sono
  // le ultime e non le prime (dovrebbero essere quelle dopo la virgola).
  m_textFld->setCursorPosition(0);
}

//=============================================================================
// ToneCurveParamField
//-----------------------------------------------------------------------------

ToneCurveParamField::ToneCurveParamField(QWidget *parent, QString name,
                                         const TToneCurveParamP &param)
    : AnimatedParamField<const QList<TPointD>, TToneCurveParamP>(parent, name,
                                                                 param, false) {
  QString str;
  m_paramName = str.fromStdString(param->getName());

  ParamsPage *paramsPage = dynamic_cast<ParamsPage *>(parent);
  FxHistogramRender *fxHistogramRender =
      (paramsPage) ? paramsPage->getFxHistogramRender() : 0;
  m_toneCurveField = new ToneCurveField(this, fxHistogramRender);
  m_toneCurveField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  //--- layout
  m_layout->addWidget(m_keyToggle);
  m_layout->addWidget(m_toneCurveField);
  m_layout->addStretch();
  setLayout(m_layout);

  connect(m_keyToggle, SIGNAL(keyToggled()), SLOT(onKeyToggled()));
  connect(m_toneCurveField, SIGNAL(currentChannelIndexChanged(int)),
          SLOT(onChannelChanged(int)));

  int i;
  for (i = 0; i < m_toneCurveField->getChannelCount(); i++) {
    ChennelCurveEditor *c = m_toneCurveField->getChannelEditor(i);

    connect(c, SIGNAL(controlPointChanged(bool)), this, SLOT(onChange(bool)));
    connect(c, SIGNAL(controlPointAdded(int)), this, SLOT(onPointAdded(int)));
    connect(c, SIGNAL(controlPointRemoved(int)), this,
            SLOT(onPointRemoved(int)));
  }
  connect(m_toneCurveField, SIGNAL(isLinearChanged(bool)), this,
          SLOT(onIsLinearChanged(bool)));
  updateField(param->getValue(0));
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::updateField(const QList<TPointD> value) {
  if (m_actualParam) {
    assert(m_currentParam &&
           m_currentParam->getCurrentChannel() ==
               m_actualParam->getCurrentChannel());
    m_toneCurveField->setCurrentChannel(m_actualParam->getCurrentChannel());
    assert(m_currentParam &&
           m_currentParam->isLinear() == m_actualParam->isLinear());
    m_toneCurveField->setIsLinearCheckBox(m_actualParam->isLinear());
  }
  m_toneCurveField->getCurrentChannelEditor()->setPoints(value);
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::setParams() {
  QList<TPointD> value =
      m_toneCurveField->getCurrentChannelEditor()->getPoints();
  m_currentParam->setValue(m_frame, value);
  if (m_actualParam->isKeyframe(m_frame)) {
    m_actualParam->setValue(m_frame, value);
    emit actualParamChanged();
  } else if (!m_actualParam->hasKeyframes()) {
    m_actualParam->setDefaultValue(value);
    emit actualParamChanged();
  }
  updateKeyToggle();

  emit currentParamChanged();
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::onChannelChanged(int channel) {
  if (m_actualParam->getCurrentChannel() ==
      TToneCurveParam::ToneChannel(channel)) {
    assert(m_currentParam->getCurrentChannel() ==
           TToneCurveParam::ToneChannel(channel));
    return;
  }
  m_currentParam->setCurrentChannel(TToneCurveParam::ToneChannel(channel));
  m_actualParam->setCurrentChannel(TToneCurveParam::ToneChannel(channel));
  updateField(m_currentParam->getValue(m_frame));
  updateKeyToggle();

  emit currentParamChanged();
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::onChange(bool isDragging) {
  if (isDragging) return;

  TToneCurveParamP toneCurveParam = m_actualParam;

  TUndo *undo = 0;
  /*--- Undoを登録する条件：
          値が変更されていて
          キーフレーム上か、または、まだキーフレームが無い
          （すなわち、実際にシーンの情報を変えることになる）場合
  ---*/
  if (toneCurveParam &&
      toneCurveParam->getValue(m_frame) !=
          m_toneCurveField->getCurrentChannelEditor()->getPoints() &&
      (m_actualParam->isKeyframe(m_frame) ||
       !m_actualParam.getPointer()->hasKeyframes()))
    undo = new ToneCurveParamFieldUndo(toneCurveParam, m_interfaceName, m_frame,
                                       ParamField::m_fxHandleStat);

  setParams();

  if (undo) TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::onPointAdded(int index) {
  QList<TPointD> value =
      m_toneCurveField->getCurrentChannelEditor()->getPoints();
  m_currentParam->addValue(0, value, index);
  m_actualParam->addValue(0, value, index);

  emit actualParamChanged();
  emit currentParamChanged();

  TToneCurveParamP toneCurveActualParam  = m_actualParam;
  TToneCurveParamP toneCurveCurrentParam = m_currentParam;
  if (toneCurveActualParam && toneCurveCurrentParam)
    TUndoManager::manager()->add(new ToneCurveParamFieldAddRemovePointUndo(
        toneCurveActualParam, toneCurveCurrentParam, value, index, true,
        m_interfaceName, ParamField::m_fxHandleStat));
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::onPointRemoved(int index) {
  TToneCurveParamP toneCurveActualParam  = m_actualParam;
  TToneCurveParamP toneCurveCurrentParam = m_currentParam;
  if (toneCurveActualParam && toneCurveCurrentParam) {
    QList<TPointD> value =
        m_toneCurveField->getCurrentChannelEditor()->getPoints();

    TUndoManager::manager()->add(new ToneCurveParamFieldAddRemovePointUndo(
        toneCurveActualParam, toneCurveCurrentParam, value, index, false,
        m_interfaceName, ParamField::m_fxHandleStat));
  }

  m_currentParam->removeValue(0, index);
  m_actualParam->removeValue(0, index);

  emit currentParamChanged();
  emit actualParamChanged();
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::onIsLinearChanged(bool isLinear) {
  m_currentParam->setIsLinear(isLinear);
  m_actualParam->setIsLinear(isLinear);

  emit actualParamChanged();
  emit currentParamChanged();

  TToneCurveParamP toneCurveActualParam  = m_actualParam;
  TToneCurveParamP toneCurveCurrentParam = m_currentParam;
  if (toneCurveActualParam && toneCurveCurrentParam)
    TUndoManager::manager()->add(new ToneCurveParamFieldToggleLinearUndo(
        toneCurveActualParam, toneCurveCurrentParam, m_interfaceName,
        ParamField::m_fxHandleStat));
}

//-----------------------------------------------------------------------------

void ToneCurveParamField::onKeyToggled() { onKeyToggle(); }

//=============================================================================
// ParamField::create()
//-----------------------------------------------------------------------------

ParamField *ParamField::create(QWidget *parent, QString name,
                               const TParamP &param) {
  if (TDoubleParamP doubleParam = param)
    return new MeasuredDoubleParamField(parent, name, doubleParam);
  else if (TRangeParamP rangeParam = param)
    return new MeasuredRangeParamField(parent, name, rangeParam);
  else if (TPixelParamP pixelParam = param)
    return new PixelParamField(parent, name, pixelParam);
  else if (TPointParamP pointParam = param)
    return new PointParamField(parent, name, pointParam);
  else if (TIntEnumParamP enumParam = param)
    return new EnumParamField(parent, name, enumParam);
  else if (TIntParamP intParam = param)
    return new IntParamField(parent, name, intParam);
  else if (TBoolParamP boolParam = param)
    return new BoolParamField(parent, name, boolParam);
  else if (TSpectrumParamP spectrumParam = param)
    return new SpectrumParamField(parent, name, spectrumParam);
  else if (TStringParamP stringParam = param)
    return new StringParamField(parent, name, stringParam);
  else if (TToneCurveParamP toneCurveParam = param)
    return new ToneCurveParamField(parent, name, toneCurveParam);
  else
    return 0;
}

//=============================================================================
// Custom Components
//-----------------------------------------------------------------------------
#include <sstream>

namespace component {
//
// LineEdit_double
//
LineEdit_double::LineEdit_double(QWidget *parent, QString name,
                                 TDoubleParamP const &param)
    : ParamField(parent, name, param), frame_(0) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QLineEdit(this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  value_->setText(QString::number(param->getValue(0)));

  connect(value_, SIGNAL(textChanged(QString const &)), this,
          SLOT(update_value(QString const &)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void LineEdit_double::setParam(TParamP const &current, TParamP const &actual,
                               int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void LineEdit_double::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const double value = actual_->getValue(frame);
  if (value == value_->text().toDouble()) {
    return;
  }

  value_->setText(QString::number(value));
}

void LineEdit_double::update_value(QString const &text) {
  const double value = text.toDouble();

  current_->setValue(frame_, value);
  emit currentParamChanged();

  actual_->setValue(frame_, value);
  emit actualParamChanged();
}

//
// Slider_double
//
Slider_double::Slider_double(QWidget *parent, QString name,
                             TDoubleParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QSlider(Qt::Horizontal, this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  value_->setValue(param->getValue(0));

  double minvalue;
  double maxvalue;
  double valuestep;
  if (param->getValueRange(minvalue, maxvalue, valuestep)) {
    value_->setRange(minvalue * 100, maxvalue * 100);
  }

  connect(value_, SIGNAL(valueChanged(int)), this, SLOT(update_value(int)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void Slider_double::setParam(TParamP const &current, TParamP const &actual,
                             int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void Slider_double::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const double value = actual_->getValue(frame);
  if (value == value_->value() / 100.0) {
    return;
  }

  value_->setValue(value * 100);
}

void Slider_double::update_value(int slider_value) {
  const double value = slider_value / 100.0;

  current_->setValue(frame_, value);
  emit currentParamChanged();

  actual_->setValue(frame_, value);
  emit actualParamChanged();
}

//
// SpinBox_double
//
SpinBox_double::SpinBox_double(QWidget *parent, QString name,
                               TDoubleParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QDoubleSpinBox(this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  double minvalue;
  double maxvalue;
  double valuestep;
  if (param->getValueRange(minvalue, maxvalue, valuestep)) {
    value_->setRange(minvalue, maxvalue);
    value_->setSingleStep(valuestep / 100.0);
  }

  connect(value_, SIGNAL(valueChanged(double)), this,
          SLOT(update_value(double)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void SpinBox_double::setParam(TParamP const &current, TParamP const &actual,
                              int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void SpinBox_double::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const double value = actual_->getValue(frame);
  if (value == value_->value()) {
    return;
  }

  value_->setValue(value);
}

void SpinBox_double::update_value(double value) {
  current_->setValue(frame_, value);
  emit currentParamChanged();

  actual_->setValue(frame_, value);
  emit actualParamChanged();
}
}  // end of namespace component

namespace component {
//
// LineEdit_int
//
LineEdit_int::LineEdit_int(QWidget *parent, QString name,
                           TIntParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QLineEdit(this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  value_->setText(QString::number(param->getValue()));

  connect(value_, SIGNAL(textChanged(QString const &)), this,
          SLOT(update_value(QString const &)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void LineEdit_int::setParam(TParamP const &current, TParamP const &actual,
                            int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void LineEdit_int::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const int value = actual_->getValue();
  if (value == value_->text().toInt()) {
    return;
  }

  value_->setText(QString::number(value));
}

void LineEdit_int::update_value(QString const &text) {
  const int value = text.toInt();

  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}

//
// Slider_int
//
Slider_int::Slider_int(QWidget *parent, QString name, TIntParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QSlider(Qt::Horizontal, this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  value_->setValue(param->getValue());

  int minvalue;
  int maxvalue;
  if (param->getValueRange(minvalue, maxvalue)) {
    value_->setRange(minvalue, maxvalue);
  } else {
    value_->setRange(0, 100);
  }

  connect(value_, SIGNAL(valueChanged(int)), this, SLOT(update_value(int)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void Slider_int::setParam(TParamP const &current, TParamP const &actual,
                          int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void Slider_int::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const int value = actual_->getValue();
  if (value == value_->value()) {
    return;
  }

  value_->setValue(value);
}

void Slider_int::update_value(int value) {
  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}

//
// SpinBox_int
//
SpinBox_int::SpinBox_int(QWidget *parent, QString name, TIntParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QSpinBox(this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  int minvalue;
  int maxvalue;
  if (param->getValueRange(minvalue, maxvalue)) {
    value_->setRange(minvalue, maxvalue);
  } else {
    value_->setRange(0, 100);
  }

  connect(value_, SIGNAL(valueChanged(int)), this, SLOT(update_value(int)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void SpinBox_int::setParam(TParamP const &current, TParamP const &actual,
                           int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void SpinBox_int::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const int value = actual_->getValue();
  if (value == value_->value()) {
    return;
  }

  value_->setValue(value);
}

void SpinBox_int::update_value(int value) {
  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}
}  // end of namespace component

namespace component {
//
// CheckBox_bool
//
CheckBox_bool::CheckBox_bool(QWidget *parent, QString name,
                             TBoolParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QCheckBox(this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  connect(value_, SIGNAL(stateChanged(int)), this, SLOT(update_value(int)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void CheckBox_bool::setParam(TParamP const &current, TParamP const &actual,
                             int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void CheckBox_bool::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  bool const value = actual_->getValue();
  if (value == (value_->checkState() != Qt::Unchecked)) {
    return;
  }

  value_->setCheckState(value ? Qt::Checked : Qt::Unchecked);
}

void CheckBox_bool::update_value(int checkbox_value) {
  bool const value = checkbox_value != Qt::Unchecked;

  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}
}  // end of namespace component

namespace component {
//
// RadioButton_enum
//
RadioButton_enum::RadioButton_enum(QWidget *parent, QString name,
                                   TIntEnumParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QButtonGroup(this);

  for (int i = 0, count = param->getItemCount(); i < count; i++) {
    int item;
    std::string caption;
    param->getItem(i, item, caption);

    QRadioButton *button = new QRadioButton(caption.c_str(), this);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    value_->addButton(button, item);
    m_layout->addWidget(button);
  }

  connect(value_, SIGNAL(buttonClicked(int)), this, SLOT(update_value(int)));

  setLayout(m_layout);
}

void RadioButton_enum::setParam(TParamP const &current, TParamP const &actual,
                                int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void RadioButton_enum::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const int value = actual_->getValue();
  if (value == value_->checkedId()) {
    return;
  }

  value_->button(value)->setChecked(true);
}

void RadioButton_enum::update_value(int value) {
  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}

//
// ComboBox_enum
//
ComboBox_enum::ComboBox_enum(QWidget *parent, QString name,
                             TIntEnumParamP const &param)
    : ParamField(parent, name, param) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QComboBox(this);
  value_->setFixedHeight(20);
  value_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  for (int i = 0, count = param->getItemCount(); i < count; i++) {
    int item;
    std::string caption;
    param->getItem(i, item, caption);

    value_->addItem(QString::fromStdString(caption));
  }

  connect(value_, SIGNAL(currentIndexChanged(int)), this,
          SLOT(update_value(int)));

  setLayout(m_layout);
}

void ComboBox_enum::setParam(TParamP const &current, TParamP const &actual,
                             int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void ComboBox_enum::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  const int value = actual_->getValue();
  if (value == value_->currentIndex()) {
    return;
  }

  value_->setCurrentIndex(value);
}

void ComboBox_enum::update_value(int value) {
  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}
}  // end of namespace component

namespace component {
//
// LineEdit_string
//
LineEdit_string::LineEdit_string(QWidget *parent, QString name,
                                 TStringParamP const &param)
    : ParamField(parent, name, param), frame_(0) {
  m_paramName = QString::fromStdString(param->getName());

  value_ = new QLineEdit(this);
  value_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  value_->setText(QString::fromStdWString(param->getValue()));

  connect(value_, SIGNAL(textChanged(QString const &)), this,
          SLOT(update_value(QString const &)));

  m_layout->addWidget(value_);

  setLayout(m_layout);
}

void LineEdit_string::setParam(TParamP const &current, TParamP const &actual,
                               int frame) {
  current_ = current;
  actual_  = actual;
  update(frame);
}

void LineEdit_string::update(int frame) {
  frame_ = frame;

  if (!actual_ || !current_) {
    return;
  }

  QString const value = QString::fromStdWString(actual_->getValue());
  if (value == value_->text()) {
    return;
  }

  value_->setText(value);
}

void LineEdit_string::update_value(QString const &text) {
  std::wstring const value = text.toStdWString();

  current_->setValue(value);
  emit currentParamChanged();

  actual_->setValue(value);
  emit actualParamChanged();
}
}  // end of namespace component

ParamField *make_lineedit(QWidget *parent, QString name, TParamP const &param) {
  if (0)
    ;
  else if (TDoubleParamP _ = param) {
    return new component::LineEdit_double(parent, name, _);
  } else if (TIntParamP _ = param) {
    return new component::LineEdit_int(parent, name, _);
  } else if (TStringParamP _ = param) {
    return new component::LineEdit_string(parent, name, _);
  }

  return NULL;
}

ParamField *make_slider(QWidget *parent, QString name, TParamP const &param) {
  if (0)
    ;
  else if (TDoubleParamP _ = param) {
    return new component::Slider_double(parent, name, _);
  } else if (TIntParamP _ = param) {
    return new component::Slider_int(parent, name, _);
  }

  return NULL;
}

ParamField *make_spinbox(QWidget *parent, QString name, TParamP const &param) {
  if (0)
    ;
  else if (TDoubleParamP _ = param) {
    return new component::SpinBox_double(parent, name, _);
  } else if (TIntParamP _ = param) {
    return new component::SpinBox_int(parent, name, _);
  }

  return NULL;
}

ParamField *make_checkbox(QWidget *parent, QString name, TParamP const &param) {
  if (0)
    ;
  else if (TBoolParamP _ = param) {
    return new component::CheckBox_bool(parent, name, _);
  }

  return NULL;
}

ParamField *make_radiobutton(QWidget *parent, QString name,
                             TParamP const &param) {
  if (0)
    ;
  else if (TIntEnumParamP _ = param) {
    return new component::RadioButton_enum(parent, name, _);
  }

  return NULL;
}

ParamField *make_combobox(QWidget *parent, QString name, TParamP const &param) {
  if (0)
    ;
  else if (TIntEnumParamP _ = param) {
    return new component::ComboBox_enum(parent, name, _);
  }

  return NULL;
}
