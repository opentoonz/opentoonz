#pragma once

#ifndef TNZ_TEXTFIELD_INCLUDED
#define TNZ_TEXTFIELD_INCLUDED

#include "tw/tw.h"
#include "tw/textlistener.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

class DVAPI TTextField : public TWidget, public TTextListener {
public:
  class Action {
  public:
    virtual ~Action() {}
    virtual void sendCommand(std::wstring) = 0;
  };

  class Listener {
  public:
    virtual void onKeyPressed(int key){};
    virtual void onFocusChange(bool on){};
    virtual ~Listener() {}
  };

protected:
  std::wstring m_text, m_oldText;
  int m_pos;
  int m_sel0, m_sel1;
  int m_off;
  bool m_mouseDown;
  int xToIndex(int x);

  std::vector<Action *> m_actions;
  std::vector<Action *> m_commitActions;
  Listener *m_listener;

  void updateOffset();

public:
  TTextField(TWidget *parent, std::string name = "textfield");
  ~TTextField();

  virtual void commit() {}

  void draw();
  void keyDown(int, TUINT32, const TPoint &);
  void onDrop(std::wstring s);

  void leftButtonDoubleClick(const TMouseEvent &e);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  std::wstring getText() const;
  void setText(std::wstring s);
  void setText(std::string s);
  // NON invoca l'azione registrata

  void select(int s0, int s1);
  void selectAll();

  void addAction(Action *action);
  void addCommitAction(Action *action);
  void setListener(Listener *listener);

  void onFocusChange(bool status);
  FocusHandling getFocusHandling() const { return LISTEN_TO_FOCUS; }

  virtual void pasteText(std::wstring text);
  virtual std::wstring copyText();
  virtual std::wstring cutText();

  virtual void drawFieldText(const TPoint &origin, std::wstring text);

#ifndef MACOSX
  // pezza dovuta al baco del gcc3.3.1. Togliere quando lo si aggiorna al 3.3.2
  // o superiori
  bool getCaret(TPoint &p, int &height);
#endif

  // toppa per aggiustare una bruttura grafica in Tab.
  // va ripensata tutta la logica del posizionamento del
  // testo
  int m_dy;

  virtual void sendCommand();
  virtual void sendCommitCommand();
};

//-------------------------------------------------------------------

template <class T>
class TTextFieldAction : public TTextField::Action {
  typedef void (T::*Method)(std::wstring text);
  T *m_target;
  Method m_method;

public:
  TTextFieldAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(std::wstring s) { (m_target->*m_method)(s); }
};

template <class T>
void tconnect(TTextField *fld, T *target, void (T::*method)(std::wstring s)) {
  fld->addCommitAction(new TTextFieldAction<T>(target, method));
}

//===================================================================

class DVAPI TNumField : public TTextField {
public:
  class Event {
  public:
    TNumField *m_field;
    double m_value;
    enum Reason { KeyPressed, FocusChange, ReturnPressed };
    Reason m_reason;
    Event(TNumField *field)
        : m_field(field), m_value(field->getValue()), m_reason(KeyPressed) {}
  };

  class Action {
  public:
    virtual ~Action() {}
    virtual void sendCommand(const Event &ev) = 0;
  };

private:
  double m_minValue, m_maxValue;
  std::vector<Action *> m_numActions;
  bool m_isInteger;
  int m_precision;

protected:
  double m_value;
  virtual void valueToText();
  virtual void textToValue();

  void sendCommand();

public:
  TNumField(TWidget *parent, std::string name = "numfield");
  ~TNumField();

  void keyDown(int, TUINT32, const TPoint &);

  bool getIsInteger() const { return m_isInteger; }
  void setIsInteger(bool isInteger);

  void setRange(double minValue, double maxValue);
  void getRange(double &minValue, double &maxValue);

  void setPrecision(int precision) { m_precision = precision; }
  int getPrecision() { return m_precision; }

  double getValue() const { return m_value; }
  void setValue(double value);
  // NON invoca l'azione registrata

  void addAction(Action *action);

  void onFocusChange(bool status);
  void pasteText(std::wstring text);
};

template <class T>
class TNumFieldAction : public TNumField::Action {
  T *m_target;
  typedef void (T::*Method)(const TNumField::Event &e);
  Method m_method;

public:
  TNumFieldAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(const TNumField::Event &e) { (m_target->*m_method)(e); }
};

template <class T>
void tconnect(TNumField *nf, T *target,
              void (T::*method)(const TNumField::Event &e)) {
  nf->addAction(new TNumFieldAction<T>(target, method));
}

//===================================================================

class TMeasuredValue;

class DVAPI TMeasuredValueField : public TTextField {
public:
  class Action {
  public:
    virtual ~Action() {}
    virtual void sendCommand(TMeasuredValueField *field) = 0;
  };

private:
  TMeasuredValue *m_value;
  std::vector<Action *> m_actions;

public:
  TMeasuredValueField(TWidget *parent, std::string name = "numfield");
  ~TMeasuredValueField();

  void setMeasure(std::string name);

  TMeasuredValue *getMeasuredValue() const { return m_value; }

  void setValue(double v);
  double getValue() const;

  void addAction(Action *action);

  // void onFocusChange(bool status);
  // void pasteText(wstring text);

  void commit();
};

template <class T>
class TMeasuredValueFieldAction : public TMeasuredValueField::Action {
  typedef void (T::*Method)(TMeasuredValueField *fld);
  T *m_target;
  Method m_method;

public:
  TMeasuredValueFieldAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(TMeasuredValueField *fld) { (m_target->*m_method)(fld); }
};

template <class T>
void tconnect(TMeasuredValueField *fld, T *target,
              void (T::*method)(TMeasuredValueField *fld)) {
  fld->addAction(new TMeasuredValueFieldAction<T>(target, method));
}

#ifdef WIN32
#pragma warning(pop)
#endif

#endif
