#pragma once

#ifndef TNZ_COMBOBOX_INCLUDED
#define TNZ_COMBOBOX_INCLUDED

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

// forward declaration
class TTextField;
class TComboMenu;
class TComboBoxActionInterface;

//-------------------------------------------------------------------

class DVAPI TComboBox : public TWidget {
  TTextField *m_textField;
  TComboMenu *m_menu;

  vector<pair<string, string>> *m_options;
  vector<TComboBoxActionInterface *> *m_actions;
  void sendCommand();

public:
  TComboBox(TWidget *parent, string name = "combobox");
  ~TComboBox();

  void draw();

  void configureNotify(const TDimension &size);

  void leftButtonDown(const TMouseEvent &);
  /*

void leftButtonDrag(const TPoint &pos, UCHAR pressure);
*/
  TPoint getHotSpot() const;

  string getText() const;
  void setText(string s);
  void addOption(string s, string help);
  void deleteOptions();

  void addAction(TComboBoxActionInterface *action);

  friend class TComboMenu;
  //	int getOptionsCount() const;
  //  string getOption(int index) const;
};

class DVAPI TComboBoxActionInterface {
public:
  TComboBoxActionInterface() {}
  virtual ~TComboBoxActionInterface() {}
  virtual void triggerAction(TComboBox *cb, string text) = 0;
};

template <class T>
class TComboBoxAction : public TComboBoxActionInterface {
  typedef void (T::*Method)(TComboBox *vf, string text);
  T *m_target;
  Method m_method;

public:
  TComboBoxAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void triggerAction(TComboBox *vf, string text) {
    (m_target->*m_method)(vf, text);
  }
};

template <class T>
inline void tconnect(TComboBox &src, T *target,
                     void (T::*method)(TComboBox *vf, string text)) {
  src.addAction(new TComboBoxAction<T>(target, method));
}

#endif
