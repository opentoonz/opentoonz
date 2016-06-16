#pragma once

#ifndef TNZ_CHECKBOX_INCLUDED
#define TNZ_CHECKBOX_INCLUDED

//#include "tcommon.h"
#include "tw/tw.h"
#include "tw/action.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TCheckBoxActionInterface;

class DVAPI TCheckBox : public TWidget {
  class TCheckBoxData;
  TCheckBoxData *m_data;

public:
  TCheckBox(TWidget *parent, string name = "button");
  ~TCheckBox();

  void repaint();
  void leftButtonDown(const TMouseEvent &);
  void leftButtonUp(const TMouseEvent &);

  bool isSelected() const;
  void addAction(TCheckBoxActionInterface *);

  void select(bool on);

  bool isGray() const;
  void setIsGray(bool on);

  // per default switchOff e' abilitato. se disabilitato il checkbox si puo'
  // spegnare solo con select(false) (e non con il mouse). Questo serve
  // per implementare un gruppo di radio button (in cui si puo' fare click
  // solo su quelli spenti)
  void enableSwitchOff(bool enabled);
};

class DVAPI TCheckBoxActionInterface {
public:
  TCheckBoxActionInterface() {}
  virtual ~TCheckBoxActionInterface() {}
  virtual void triggerAction(TCheckBox *checkbox, bool selected) = 0;
};

template <class T>
class TCheckBoxAction : public TCheckBoxActionInterface {
  typedef void (T::*Method)(TCheckBox *checkbox, bool selected);
  T *m_target;
  Method m_method;

public:
  TCheckBoxAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void triggerAction(TCheckBox *checkbox, bool selected) {
    (m_target->*m_method)(checkbox, selected);
  }
};

template <class T>
inline void tconnect(TCheckBox &src, T *target,
                     void (T::*method)(TCheckBox *checkbox, bool selected)) {
  src.addAction(new TCheckBoxAction<T>(target, method));
}

#endif
