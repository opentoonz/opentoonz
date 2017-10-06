#pragma once

#ifndef PARAMFIELD_H
#define PARAMFIELD_H

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

#include "tcommon.h"
#include <QWidget>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>

#include "tgeometry.h"
#include "tparam.h"
#include "tnotanimatableparam.h"
#include "tspectrumparam.h"
#include "ttonecurveparam.h"
#include "tdoubleparam.h"
#include "toonz/tfxhandle.h"
#include "historytypes.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QString;
class QComboBox;
class QHBoxLayout;
class TFxHandle;

namespace DVGui {
class LineEdit;
class IntField;
class DoubleField;
class MeasuredDoubleField;
class MeasuredDoublePairField;
class ColorField;
class SpectrumField;
class ToneCurveField;
class CheckBox;
}

//=============================================================================
/*! \brief ParamField.

                Inherits \b QWidget.
*/
class DVAPI ParamField : public QWidget {
  Q_OBJECT

protected:
  QHBoxLayout *m_layout;
  QString m_paramName;

  static TFxHandle *m_fxHandleStat;
  QString m_interfaceName;
  QString m_description;

public:
  ParamField(QWidget *parent, QString paramName, const TParamP &param,
             bool addEmptyLabel = true);
  ~ParamField();

  QString getParamName() const { return m_paramName; }
  QString getUIName() const { return m_interfaceName; }
  QString getDescription() const { return m_description; }

  virtual void setParam(const TParamP &current, const TParamP &actual,
                        int frame) = 0;

  virtual void update(int frame) = 0;

  static ParamField *create(QWidget *parent, QString name,
                            const TParamP &param);

  virtual void setPointValue(const TPointD &p){};

  virtual QSize getPreferedSize() { return QSize(200, 28); }

  static void setFxHandle(TFxHandle *fxHandle);

  virtual void setPrecision(int precision) {}
signals:
  void currentParamChanged();
  void actualParamChanged();
  void paramKeyToggle();
};

//=============================================================================
// ParamFieldKeyToggle
//-----------------------------------------------------------------------------

class DVAPI ParamFieldKeyToggle final : public QWidget {
  Q_OBJECT

public:
  enum Status { NOT_ANIMATED, NOT_KEYFRAME, MODIFIED, KEYFRAME };

private:
  Status m_status;
  bool m_highlighted;

public:
  ParamFieldKeyToggle(QWidget *parent,
                      std::string name = "ParamFieldKeyToggle");

  void setStatus(Status status);
  Status getStatus() const;

  void setStatus(bool hasKeyframe, bool isKeyframe, bool hasBeenChanged);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;

signals:
  void keyToggled();
};

//=============================================================================
// FxSettingsKeyToggleUndo
//=============================================================================
template <class T, class ParamP>
class FxSettingsKeyToggleUndo final : public TUndo {
  TFxHandle *m_fxHandle;
  QString m_name;
  bool m_wasKeyframe;
  int m_frame;

  ParamP m_param;
  T m_currentValue;

public:
  FxSettingsKeyToggleUndo(ParamP param, T currentValue, bool wasKeyFrame,
                          QString name, int frame, TFxHandle *fxHandle)
      : m_param(param)
      , m_currentValue(currentValue)
      , m_wasKeyframe(wasKeyFrame)
      , m_name(name)
      , m_frame(frame)
      , m_fxHandle(fxHandle) {}

  // void notify();

  void undo() const override {
    if (m_wasKeyframe)
      m_param->setValue(m_frame, m_currentValue);
    else
      m_param->deleteKeyframe(m_frame);

    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  void redo() const override {
    if (m_wasKeyframe)
      m_param->deleteKeyframe(m_frame);
    else
      m_param->setValue(m_frame, m_currentValue);

    if (m_fxHandle) m_fxHandle->notifyFxChanged();
  }

  int getSize() const override { return sizeof(*this); }
  int getHistoryType() override { return HistoryType::Fx; }
  QString getHistoryString() override {
    QString str =
        QObject::tr("Modify Fx Param : %1 Key : %2  Frame %3")
            .arg((m_wasKeyframe) ? QObject::tr("Delete") : QObject::tr("Set"))
            .arg(m_name)
            .arg(QString::number(m_frame + 1));

    return str;
  }
};

//=============================================================================
// AnimatedParamField
//-----------------------------------------------------------------------------

template <class T, class ParamP>
class DVAPI AnimatedParamField : public ParamField {
protected:
  ParamP m_currentParam, m_actualParam;
  int m_frame;
  ParamFieldKeyToggle *m_keyToggle;

public:
  AnimatedParamField(QWidget *parent, QString name, const ParamP &param,
                     bool addEmptyLabel = true)
      : ParamField(parent, name, param, addEmptyLabel), m_frame(0) {
    m_keyToggle = new ParamFieldKeyToggle(this);
  }

  virtual void updateField(T value) = 0;

  void setParam(const TParamP &current, const TParamP &actual,
                int frame) override {
    m_currentParam = current;
    m_actualParam  = actual;
    assert(m_currentParam);
    assert(m_actualParam);
    update(frame);
  }
  void update(int frame) override {
    m_frame = frame;
    if (!m_actualParam || !m_currentParam) return;
    T value = m_actualParam->getValue(m_frame);
    if (m_actualParam->isKeyframe(m_frame))
      m_currentParam->setValue(m_frame, value);
    else if (!m_actualParam.getPointer()->hasKeyframes())
      m_currentParam->setDefaultValue(value);
    updateField(value);
    updateKeyToggle();
  }

  void updateKeyToggle() {
    T stroke  = m_actualParam->getValue(m_frame);
    T stroke2 = m_currentParam->getValue(m_frame);
    m_keyToggle->setStatus(
        m_actualParam->hasKeyframes(), m_actualParam->isKeyframe(m_frame),
        m_actualParam->getValue(m_frame) != m_currentParam->getValue(m_frame));
  }

  /*--
   * エフェクトの位置パラメータをSwatchViewerでドラッグして編集するときに呼ばれる
   * ---*/
  void setValue(T value) {
    if (m_currentParam->getValue(m_frame) == value) return;
    m_currentParam->setValue(m_frame, value);
    /*-- キーフレーム上で操作した場合 --*/
    if (m_actualParam->isKeyframe(m_frame)) {
      m_actualParam->setValue(m_frame, value);
      emit actualParamChanged();
    }
    /*-- キーフレーム無い場合 --*/
    else if (!m_actualParam.getPointer()->hasKeyframes()) {
      m_actualParam->setDefaultValue(value);
      emit actualParamChanged();
    }
    /*-- 他にキーフレームがあって、キーフレーム以外のフレームで操作した場合 --*/
    emit currentParamChanged();
    updateKeyToggle();
  }

  void onKeyToggle() {
    T currentVal = m_currentParam->getValue(m_frame);
    bool wasKeyFrame;

    if (m_keyToggle->getStatus() == ParamFieldKeyToggle::KEYFRAME) {
      m_actualParam->deleteKeyframe(m_frame);
      update(m_frame);
      wasKeyFrame = true;
    } else {
      m_actualParam->setValue(m_frame, m_currentParam->getValue(m_frame));
      updateKeyToggle();
      wasKeyFrame = false;
    }
    emit actualParamChanged();
    emit paramKeyToggle();

    TUndoManager::manager()->add(new FxSettingsKeyToggleUndo<T, ParamP>(
        m_actualParam, currentVal, wasKeyFrame, m_interfaceName, m_frame,
        ParamField::m_fxHandleStat));
  }
};

//=============================================================================
// MeasuredDoubleParamField
//-----------------------------------------------------------------------------

class DVAPI MeasuredDoubleParamField final
    : public AnimatedParamField<double, TDoubleParamP> {
  Q_OBJECT

  DVGui::MeasuredDoubleField *m_measuredDoubleField;

public:
  MeasuredDoubleParamField(QWidget *parent, QString name,
                           const TDoubleParamP &param);

  void updateField(double value) override;

  QSize getPreferedSize() override { return QSize(260, 28); }

protected slots:
  void onChange(bool);
  void onKeyToggled();
};

//=============================================================================
// RangeParamField
//-----------------------------------------------------------------------------

class DVAPI MeasuredRangeParamField final
    : public AnimatedParamField<DoublePair, TRangeParamP> {
  Q_OBJECT

  DVGui::MeasuredDoublePairField *m_valueField;

public:
  MeasuredRangeParamField(QWidget *parent, QString name,
                          const TRangeParamP &param);

  void updateField(DoublePair value) override;

  QSize getPreferedSize() override { return QSize(300, 20); }
  void setPrecision(int precision) override;

protected slots:
  void onChange(bool);
  void onKeyToggled();
};

//=============================================================================
// PointParamField
//-----------------------------------------------------------------------------

class DVAPI PointParamField final
    : public AnimatedParamField<TPointD, TPointParamP> {
  Q_OBJECT

  DVGui::MeasuredDoubleField *m_xFld, *m_yFld;

public:
  PointParamField(QWidget *parent, QString name, const TPointParamP &param);

  void setPointValue(const TPointD &p) override;

  void updateField(TPointD value) override;

  QSize getPreferedSize() override { return QSize(270, 28); }

protected slots:
  void onChange(bool);
  void onKeyToggled();
};

//=============================================================================
// PixelParamField
//-----------------------------------------------------------------------------

class DVAPI PixelParamField final
    : public AnimatedParamField<TPixel32, TPixelParamP> {
  Q_OBJECT

  DVGui::ColorField *m_colorField;

public:
  PixelParamField(QWidget *parent, QString name, const TPixelParamP &param);

  void updateField(TPixel32 value) override;

  QSize getPreferedSize() override { return QSize(480, 40); }

  /*-- RgbLinkButtonの実行のため --*/
  TPixel32 getColor();
  void setColor(TPixel32 value);

protected:
  void setParams();

protected slots:
  void onChange(const TPixel32 &value, bool isDragging);
  void onKeyToggled();
};

//=============================================================================
// RGB Link Button
//-----------------------------------------------------------------------------

class DVAPI RgbLinkButton final : public QPushButton {
  Q_OBJECT
  PixelParamField *m_field1, *m_field2;

public:
  RgbLinkButton(QString str, QWidget *parent, PixelParamField *field1,
                PixelParamField *field2);

protected slots:
  void onButtonClicked();
};

//=============================================================================
// SpectrumParamField
//-----------------------------------------------------------------------------

class DVAPI SpectrumParamField final
    : public AnimatedParamField<TSpectrum, TSpectrumParamP> {
  Q_OBJECT

  DVGui::SpectrumField *m_spectrumField;

public:
  SpectrumParamField(QWidget *parent, QString name,
                     const TSpectrumParamP &param);

  void updateField(TSpectrum value) override;

  void setParams();

  QSize getPreferedSize() override { return QSize(477, 60); }

protected slots:
  void onKeyToggled();
  void onChange(bool isDragging);
  void onKeyAdded(int keyIndex);
  void onKeyRemoved(int keyIndex);
};

//=============================================================================
// EnumParamField
//-----------------------------------------------------------------------------

class EnumParamField final : public ParamField {
  Q_OBJECT

  TIntEnumParamP m_currentParam, m_actualParam;
  QComboBox *m_om;

public:
  EnumParamField(QWidget *parent, QString name, const TIntEnumParamP &param);

  void setParam(const TParamP &current, const TParamP &actual,
                int frame) override;
  void update(int frame) override;

  QSize getPreferedSize() override { return QSize(150, 20); }

protected slots:
  void onChange(const QString &str);
};

//=============================================================================
// BoolParamField
//-----------------------------------------------------------------------------

class DVAPI BoolParamField final : public ParamField {
  Q_OBJECT

  TBoolParamP m_currentParam, m_actualParam;
  DVGui::CheckBox *m_checkBox;

public:
  BoolParamField(QWidget *parent, QString name, const TBoolParamP &param);

  void setParam(const TParamP &current, const TParamP &actual,
                int frame) override;
  void update(int frame) override;

  QSize getPreferedSize() override { return QSize(20, 15); }

protected slots:
  void onToggled(bool checked);

  /*-- visibleToggle UIで使用する --*/
signals:
  void toggled(bool);
};

//=============================================================================
// IntParamField
//-----------------------------------------------------------------------------

class DVAPI IntParamField final : public ParamField {
  Q_OBJECT

  TIntParamP m_currentParam, m_actualParam;
  DVGui::IntField *m_intField;
  typedef IntParamField This;

public:
  IntParamField(QWidget *parent = 0, QString name = 0,
                const TIntParamP &param = 0);

  void setParam(const TParamP &current, const TParamP &actual,
                int frame) override;
  void update(int frame) override;

  QSize getPreferedSize() override { return QSize(50, 20); }

protected slots:
  void onChange(bool isDragging = false);
};

//=============================================================================
// StringParamField
//-----------------------------------------------------------------------------

class DVAPI StringParamField final : public ParamField {
  Q_OBJECT

  TStringParamP m_currentParam, m_actualParam;
  DVGui::LineEdit *m_textFld;

public:
  StringParamField(QWidget *parent, QString name, const TStringParamP &param);

  void setParam(const TParamP &current, const TParamP &actual,
                int frame) override;
  void update(int frame) override;

  QSize getPreferedSize() override { return QSize(100, 20); }
protected slots:
  void onChange();
};

//=============================================================================
// ToneCurveParamField
//-----------------------------------------------------------------------------

class DVAPI ToneCurveParamField final
    : public AnimatedParamField<const QList<TPointD>, TToneCurveParamP> {
  Q_OBJECT

  DVGui::ToneCurveField *m_toneCurveField;

public:
  ToneCurveParamField(QWidget *parent, QString name,
                      const TToneCurveParamP &param);

  void updateField(const QList<TPointD> value) override;

  void setParams();

  QSize getPreferedSize() override { return QSize(400, 380); }

protected slots:
  void onChannelChanged(int);

  void onChange(bool isDragging);
  void onPointAdded(int index);
  void onPointRemoved(int index);
  void onIsLinearChanged(bool);

  void onKeyToggled();
};

namespace component {
class DVAPI LineEdit_double final : public ParamField {
  Q_OBJECT;  // could not use templates for Q_OBJECT

  int frame_;
  TDoubleParamP current_;
  TDoubleParamP actual_;
  QLineEdit *value_;

public:
  LineEdit_double(QWidget *parent, QString name, TDoubleParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(QString const &text);  // could not use MACROs for slots
};

class DVAPI Slider_double final : public ParamField {
  Q_OBJECT;

  int frame_;
  TDoubleParamP current_;
  TDoubleParamP actual_;
  QSlider *value_;

public:
  Slider_double(QWidget *parent, QString name, TDoubleParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(int);
};

class DVAPI SpinBox_double final : public ParamField {
  Q_OBJECT;

  int frame_;
  TDoubleParamP current_;
  TDoubleParamP actual_;
  QDoubleSpinBox *value_;

public:
  SpinBox_double(QWidget *parent, QString name, TDoubleParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(double);
};
}

namespace component {
class DVAPI LineEdit_int final : public ParamField {
  Q_OBJECT;

  int frame_;
  TIntParamP current_;
  TIntParamP actual_;
  QLineEdit *value_;

public:
  LineEdit_int(QWidget *parent, QString name, TIntParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(QString const &text);
};

class DVAPI Slider_int final : public ParamField {
  Q_OBJECT;

  int frame_;
  TIntParamP current_;
  TIntParamP actual_;
  QSlider *value_;

public:
  Slider_int(QWidget *parent, QString name, TIntParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(int);
};

class DVAPI SpinBox_int final : public ParamField {
  Q_OBJECT;

  int frame_;
  TIntParamP current_;
  TIntParamP actual_;
  QSpinBox *value_;

public:
  SpinBox_int(QWidget *parent, QString name, TIntParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(int);
};

}  // end of namespace component

namespace component {
class DVAPI CheckBox_bool final : public ParamField {
  Q_OBJECT;

  int frame_;
  TBoolParamP current_;
  TBoolParamP actual_;
  QCheckBox *value_;

public:
  CheckBox_bool(QWidget *parent, QString name, TBoolParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(int);
};

}  // end of namespace component

namespace component {
class DVAPI RadioButton_enum final : public ParamField {
  Q_OBJECT;

  int frame_;
  TIntEnumParamP current_;
  TIntEnumParamP actual_;
  QButtonGroup *value_;

public:
  RadioButton_enum(QWidget *parent, QString name, TIntEnumParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(int);
};

class DVAPI ComboBox_enum final : public ParamField {
  Q_OBJECT;

  int frame_;
  TIntEnumParamP current_;
  TIntEnumParamP actual_;
  QComboBox *value_;

public:
  ComboBox_enum(QWidget *parent, QString name, TIntEnumParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(int);
};
}  // end of namespace component

namespace component {
class DVAPI LineEdit_string final : public ParamField {
  Q_OBJECT;

  int frame_;
  TStringParamP current_;
  TStringParamP actual_;
  QLineEdit *value_;

public:
  LineEdit_string(QWidget *parent, QString name, TStringParamP const &param);

  void setParam(TParamP const &current, TParamP const &actual,
                int frame) override;
  void update(int frame) override;

protected slots:
  void update_value(QString const &);
};
}  // end of namespace component

#ifdef __cplusplus
extern "C" {
#endif

#define TOONZ_DECLARE_MAKE_WIDGET(NAME)                                        \
  ParamField *NAME(QWidget *parent, QString name, TParamP const &param)

TOONZ_DECLARE_MAKE_WIDGET(make_lineedit);
TOONZ_DECLARE_MAKE_WIDGET(make_slider);
TOONZ_DECLARE_MAKE_WIDGET(make_spinbox);
TOONZ_DECLARE_MAKE_WIDGET(make_checkbox);
TOONZ_DECLARE_MAKE_WIDGET(make_radiobutton);
TOONZ_DECLARE_MAKE_WIDGET(make_combobox);

#ifdef __cplusplus
}
#endif

#endif  // PARAMFIELD_H
