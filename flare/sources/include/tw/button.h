#pragma once

#ifndef TNZ_BUTTON_INCLUDED
#define TNZ_BUTTON_INCLUDED

#include "tw/tw.h"
#include "tw/action.h"
#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TButtonSet;

class DVAPI TButton : public TWidget, public TCommandSource {
protected:
  bool m_pressed, m_active;
  int m_border;

  vector<TButtonSet *> *m_buttonSets;
  friend class TButtonSet;

public:
  TButton(TWidget *parent, string name = "button");
  ~TButton();

  void repaint();
  void enter(const TPoint &p);
  void leave(const TPoint &p);
  void leftButtonDown(const TMouseEvent &);
  void leftButtonUp(const TMouseEvent &);

  virtual void setTitle(string title) { m_name = title; }
};

class DVAPI TIconButton : public TButton {
  TRaster32P m_rasterUp, m_rasterDown;

public:
  TIconButton(TWidget *parent, TRaster32P raster, string name = "button");
  TIconButton(TWidget *parent, TRaster32P rasterUp, TRaster32P rasterDown,
              string name = "button");
  void repaint();
  void leftButtonDown(const TMouseEvent &);
  void leftButtonUp(const TMouseEvent &);
};

class DVAPI TIconToggle : public TButton {
  TRaster32P m_up, m_down;

public:
  TIconToggle(TWidget *parent, TRaster32P up, TRaster32P down,
              string name = "button");
  void repaint();
  void setStatus(bool v);
  bool getStatus();
  void leftButtonDown(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);
};

class TButtonSetAction {
public:
  TButtonSetAction(){};
  virtual ~TButtonSetAction(){};
  virtual void sendCommand(string value) = 0;
};

template <class T>
class TButtonSetActionT : public TButtonSetAction {
public:
  typedef void (T::*CommandMethod)(string);

  T *m_target;
  CommandMethod m_method;

  TButtonSetActionT<T>(T *target, CommandMethod method)
      : m_target(target), m_method(method){};
  void sendCommand(string value) { (m_target->*m_method)(value); };
};

class DVAPI TButtonSet {
  std::map<TButton *, string> *m_buttons;
  string m_currentValue;
  TButtonSetAction *m_action;

public:
  TButtonSet();
  ~TButtonSet();

private:
  // not implemented
  TButtonSet(const TButtonSet &);
  TButtonSet &operator=(const TButtonSet &);

public:
  void addButton(TButton *button, string value);
  void removeButton(TButton *button);
  void setValue(string value);
  void setValue(TButton *value);
  // Note: setValue fires the buttons actions as well as the buttonset action
  string getValue() { return m_currentValue; };

  void setAction(TButtonSetAction *action);

  void enable(bool on);
};

#endif
