#pragma once

#ifndef TNZ_OPTIONMENU_INCLUDED
#define TNZ_OPTIONMENU_INCLUDED

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

class TPopupMenu;

#ifdef WIN32
#pragma warning(disable : 4251)
#endif

//-------------------------------------------------------------------

class DVAPI TGenericOptionMenuAction {
public:
  virtual ~TGenericOptionMenuAction() {}
  virtual void sendCommand(string item) = 0;
};

class DVAPI TGenericOptionMenuWAction {
public:
  virtual ~TGenericOptionMenuWAction() {}
  virtual void sendCommand(wstring item) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TOptionMenuAction : public TGenericOptionMenuAction {
public:
  typedef void (T::*Method)(string item);
  TOptionMenuAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(string item) { (m_target->*m_method)(item); }

private:
  T *m_target;
  Method m_method;
};

//-------------------------------------------------------------------
template <class T>
class TOptionMenuActionW : public TGenericOptionMenuWAction {
public:
  typedef void (T::*Method)(wstring item);
  TOptionMenuActionW(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(wstring item) { (m_target->*m_method)(item); }

private:
  T *m_target;
  Method m_method;
};

//-------------------------------------------------------------------

class DVAPI TOptionMenu : public TWidget {
public:
  TOptionMenu(TWidget *parent, string name = "optionMenu");
  ~TOptionMenu();

  void setAction(TGenericOptionMenuAction *action) { m_action = action; }

  void repaint();

  void configureNotify(const TDimension &size);

  void leftButtonDown(const TMouseEvent &);

  string getText() const;
  void setText(string s);

  void setLabel(wstring title);
  void setLabelWidth(int lx);
  wstring getLabel() const { return m_label; }
  int getLabelWidth() const { return m_labelWidth; }

  bool isOption(string s) const;
  void addOption(string cmdName);
  void addOption(string cmdName, wstring title);
  void deleteOption(string s);
  void deleteAllOptions();

private:
  TPopupMenu *m_menu;
  string m_currentOption;
  wstring m_currentOptionTitle;
  wstring m_label;
  int m_labelWidth;

  typedef std::vector<std::pair<string, wstring>> OptionList;
  OptionList m_options;

  TGenericOptionMenuAction *m_action;
};

//-------------------------------------------------------------------

template <class T>
class TOptionMenuWAction : public TGenericOptionMenuWAction {
public:
  typedef void (T::*Method)(wstring item);
  TOptionMenuWAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(wstring item) { (m_target->*m_method)(item); }

private:
  T *m_target;
  Method m_method;
};

//-------------------------------------------------------------------

class DVAPI TOptionMenuW : public TWidget {
public:
  TOptionMenuW(TWidget *parent, string name = "optionMenu");
  ~TOptionMenuW();

  void setAction(TGenericOptionMenuWAction *action) { m_action = action; }

  void repaint();

  void configureNotify(const TDimension &size);

  void leftButtonDown(const TMouseEvent &);

  wstring getText() const;
  void setText(wstring s);

  void setLabel(wstring label);
  void setLabelWidth(int width);

  bool isOption(wstring s) const;
  void addOption(wstring item);
  void deleteOption(wstring s);
  void deleteAllOptions();

  typedef std::vector<wstring> OptionList;
  const OptionList getOptions() const { return m_options; };

private:
  TPopupMenu *m_menu;
  wstring m_currentOption;
  wstring m_label;
  int m_labelWidth;

  OptionList m_options;

  TGenericOptionMenuWAction *m_action;
};

//-------------------------------------------------------------------

/*
class DVAPI TComboBoxActionInterface {
public:
  TComboBoxActionInterface() {}
  virtual ~TComboBoxActionInterface() {}
  virtual void triggerAction(TComboBox*cb, string text) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TComboBoxAction : public TComboBoxActionInterface {
  typedef void (T::*Method)(TComboBox *vf, string text);
  T *m_target;
  Method m_method;
public:
  TComboBoxAction(T*target, Method method) : m_target(target), m_method(method)
{}
  void triggerAction(TComboBox*vf, string text)
    {(m_target->*m_method)(vf, text); }
};

//-------------------------------------------------------------------

template <class T>
inline void tconnect(TComboBox&src, T *target, void (T::*method)(TComboBox *vf,
string text))
{
  src.addAction(new TComboBoxAction<T>(target, method));
}

*/

//-------------------------------------------------------------------

template <class T>
void tconnect(TOptionMenu &menu, T *target, void (T::*method)(string)) {
  menu.setAction(new TOptionMenuAction<T>(target, method));
}

//-------------------------------------------------------------------

template <class T>
void tconnect(TOptionMenuW &menu, T *target, void (T::*method)(wstring)) {
  menu.setAction(new TOptionMenuActionW<T>(target, method));
}

#endif
