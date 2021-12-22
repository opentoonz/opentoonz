#pragma once

#ifndef TNZ_VALUEFIELD_INCLUDED
#define TNZ_VALUEFIELD_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TTextField;
class TNumField;

class TValueFieldActionInterface;
class TValuePairFieldActionInterface;

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

//-------------------------------------------------------------------

class DVAPI TValueField : public TWidget {
protected:
  wstring m_labelText;
  int m_labelWidth, m_fieldWidth;
  double m_value, m_minValue, m_maxValue;

  // serve nel drag
  int m_deltaPos;
  TPoint m_lastPos;
  bool m_draggingArrow;
  bool m_draggingSlider;

  TRect m_sliderRect;
  TRect m_arrowRect;
  TNumField *m_textField;
  bool m_arrowEnabled, m_sliderEnabled;
  vector<TValueFieldActionInterface *> m_actions;

  double posToValue(int x) const;
  int valueToPos(double v) const;
  int m_precision;

  void sendCommand(bool dragging);

public:
  TValueField(TWidget *parent, string name = "floatfield",
              bool withField = true);
  ~TValueField();

  void enableSlider(bool on);
  void enableArrow(bool on);

  void repaint();
  void configureNotify(const TDimension &d);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void addAction(TValueFieldActionInterface *action);

  double getValue() const { return m_value; }
  void setValue(double value, bool sendCommand = true);

  void onTextFieldChange();
  void onTextFieldChange(bool dragging);

  void getValueRange(double &min, double &max) const {
    min = m_minValue;
    max = m_maxValue;
  };
  void setValueRange(double min, double max);

  void setLabel(string text);
  void setLabelWidth(int lx);
  void setFieldWidth(int lx);

  void setPrecision(int d);
  int getPrecision() const { return m_precision; }
};

//-------------------------------------------------------------------

class DVAPI TValueFieldActionInterface {
public:
  TValueFieldActionInterface() {}
  virtual ~TValueFieldActionInterface() {}
  virtual void triggerAction(TValueField *vf, double value, bool dragging) = 0;
};

template <class T>
class TValueFieldAction : public TValueFieldActionInterface {
  typedef void (T::*Method)(TValueField *vf, double value, bool dragging);
  T *m_target;
  Method m_method;

public:
  TValueFieldAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void triggerAction(TValueField *vf, double value, bool dragging) {
    (m_target->*m_method)(vf, value, dragging);
  }
};

template <class T>
inline void tconnect(TValueField &src, T *target,
                     void (T::*method)(TValueField *vf, double value,
                                       bool dragging)) {
  src.addAction(new TValueFieldAction<T>(target, method));
}

//-------------------------------------------------------------------

class DVAPI TValuePairField : public TWidget {
protected:
  wstring m_labelText;
  int m_labelWidth;
  double m_value0, m_value1, m_minValue, m_maxValue;
  double m_newValue0, m_newValue1;
  bool m_first;
  // servono nel drag
  int m_deltaPos;
  int m_flags;
  TRect m_sliderRect, m_arrow0Rect, m_arrow1Rect;
  TNumField *m_textField0, *m_textField1;
  bool m_arrowEnabled, m_sliderEnabled;
  vector<TValuePairFieldActionInterface *> m_actions;

  double posToValue(int x) const;
  int valueToPos(double v) const;

  void sendCommand(bool dragging);
  void onMMTimer(TUINT64 tick);

public:
  TValuePairField(TWidget *parent, string name = "valuepairfield");
  ~TValuePairField();

  void enableSlider(bool on);
  void enableArrow(bool on);

  void repaint();
  void configureNotify(const TDimension &d);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void addAction(TValuePairFieldActionInterface *action);

  double getValue0() const { return m_value0; }
  double getValue1() const { return m_value1; }
  void setValue0(double value, bool dragging = true);
  void setValue1(double value, bool dragging = true);
  void setValues(double value0, double value1, bool dragging = true);

  void getValueRange(double &min, double &max) const {
    min = m_minValue;
    max = m_maxValue;
  };
  void setValueRange(double min, double max);

  void setLabel(string text);
  void setLabelWidth(int width);

  void onTextFieldChange(bool first);
};

//-------------------------------------------------------------------

class DVAPI TValuePairFieldActionInterface {
public:
  TValuePairFieldActionInterface() {}
  virtual ~TValuePairFieldActionInterface() {}
  virtual void triggerAction(TValuePairField *vf, double value0, double value1,
                             bool dragging) = 0;
};

template <class T>
class TValuePairFieldAction : public TValuePairFieldActionInterface {
  typedef void (T::*Method)(TValuePairField *vf, double value0, double value1,
                            bool dragging);
  T *m_target;
  Method m_method;

public:
  TValuePairFieldAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void triggerAction(TValuePairField *vf, double value0, double value1,
                     bool dragging) {
    (m_target->*m_method)(vf, value0, value1, dragging);
  }
};

template <class T>
inline void tconnect(TValuePairField &src, T *target,
                     void (T::*method)(TValuePairField *vf, double value0,
                                       double value1, bool dragging)) {
  src.addAction(new TValuePairFieldAction<T>(target, method));
}

#ifdef WIN32
#pragma warning(pop)
#endif

#endif
