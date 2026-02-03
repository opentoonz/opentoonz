#pragma once

#ifndef TNZ_SCROLLBAR_INCLUDED
#define TNZ_SCROLLBAR_INCLUDED

//#include "tw/action.h"
//#include "traster.h"
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

class TScrollbarGenericAction {
public:
  TScrollbarGenericAction() {}
  virtual ~TScrollbarGenericAction() {}
  virtual void notify(int value) = 0;
};

template <class T>
class TScrollbarAction : public TScrollbarGenericAction {
public:
  typedef void (T::*CommandMethod)(int value);

  T *m_target;
  CommandMethod m_method;

  TScrollbarAction<T>(T *target, CommandMethod method)
      : m_target(target), m_method(method){};
  void notify(int value) { (m_target->*m_method)(value); };
};

class DVAPI TScrollbar : public TWidget {
  int m_oldPos;
  int m_gPos, m_gMinPos, m_gMaxPos, m_gSize;
  int m_value, m_minValue, m_maxValue, m_cursorSize;
  const int m_buttonAreaSize;
  TScrollbarGenericAction *m_action;
  bool m_firstButtonPressed, m_secondButtonPressed;
  bool m_horizontal;
  bool m_minOverflowEnabled;
  bool m_maxOverflowEnabled;
  int m_buttonIncrement;

protected:
  void updatePositions();

  void drawHCursor(int x0, int x1);
  void drawVCursor(int y0, int y1);
  enum ButtonId { FIRST_BUTTON, SECOND_BUTTON };
  void drawButton(ButtonId id);

public:
  TScrollbar(TWidget *parent, string name = "scrollbar");
  ~TScrollbar();

  void repaint();

  void leftButtonDown(const TMouseEvent &);
  void leftButtonDrag(const TMouseEvent &);
  void leftButtonUp(const TMouseEvent &);

  void configureNotify(const TDimension &d);

  void setValue(int value, int min, int max, int size);
  int getValue() const { return m_value; }

  // min <= value <= max
  // il cursore occupa almeno size/(max-min+size) dell'area disponibile
  // se size = 0 non c'e' cursore e lo slider e' disabilitato
  // se size>=max-min il cursore occupa tutto lo spazio e lo slider e'
  // disabilitato
  //
  // value == min quando il cursore e' in alto
  // value == max quando il cursore e' in basso
  //
  // quando vengono premuti i bottoni value viene incrementato/decrementato di
  // uno
  //

  // abilita/disabilita la possibilita' di agire sui bottoni anche
  // quando lo slider e' gia' sul massimo/minimo
  // default: false/false
  void enableButtonOverflow(bool min, bool max);

  // default: 4
  void setButtonIncrement(int d);

  void setAction(TScrollbarGenericAction *action);
  void onTimer(int v);
};

#endif
