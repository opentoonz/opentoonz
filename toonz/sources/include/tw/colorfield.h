#pragma once

#ifndef TNZ_COLORFIELD_INCLUDED
#define TNZ_COLORFIELD_INCLUDED

#include "tw/tw.h"
#include "tpixel.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------
class ColorChip;
class TValueField;

class DVAPI TColorField : public TWidget {
public:
  class Listener {
  public:
    virtual void onChange(TColorField *cfd, TPixel32 color, bool dragging) {}
    virtual ~Listener() {}
  };

  class ColorEditor {
  public:
    virtual void connectTo(TColorField *fld, bool openPopup = false) = 0;
    virtual bool isConnectedTo(TColorField *fld) const = 0;
    virtual void updateColor(const TPixel32 &color, bool dragging) = 0;
  };

private:
  Listener *m_listener;
  TPixel32 m_color;
  ColorChip *m_colorChip;
  TValueField *m_redFld, *m_greenFld, *m_blueFld, *m_matteFld;
  typedef TColorField This;
  int m_labelWidth;
  bool m_matteEnabled;
  ColorEditor *m_colorEditor;

protected:
  virtual void onChange(TPixel32 color, bool dragging) {}

public:
  TColorField(TWidget *parent, string name, int labelWidth, bool matteEnabled);

  void configureNotify(const TDimension &size);

  void setListener(Listener *listener);

  void setColor(TPixel32 color);

  TPixel32 getColor() const;

  void onChange(TValueField *vf, double value, bool dragging);

  void draw();
  void setColorEditor(ColorEditor *colorEditor) { m_colorEditor = colorEditor; }
  ColorEditor *getColorEditor() const { return m_colorEditor; }

  void notifyListener(bool dragging);
  bool isMatteEnabled() const { return m_matteEnabled; }

  void onEditedColorChange();
};

//-------------------------------------------------------------------

/*
// forward declarations
class TColorFieldActionInterface;
class TColorField2ActionInterface;
class TColorField3ActionInterface;
class TColorSlider;

//-------------------------------------------------------------------

class TColorField : public TWidget {
  TPixel32 m_color;
  int m_currentChannel;
  vector<TColorFieldActionInterface*> *m_actions;

  void onValueChange(bool dragging);
public:
  TColorField(TWidget *parent, string name = "colorfield");
  ~TColorField();

  void repaint();

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp  (const TMouseEvent &e);

  TPixel32 getValue() const;
  void setValue(const TPixel32 &);

  void addAction(TColorFieldActionInterface*action);

};

//-------------------------------------------------------------------

class TColorField2Data;

class TColorField2 : public TWidget {
public:
  TColorField2(TWidget *parent, string name = "colorfield2");
  ~TColorField2();

  void configureNotify(const TDimension &d);
  void repaint();

  TPixel32 getValue() const;
  void setValue(const TPixel32 &);

  void addAction(TColorField2ActionInterface*action);

private:
  TColorField2Data *m_data;
};


//-------------------------------------------------------------------

class DVAPI TColorFieldActionInterface {
public:
  TColorFieldActionInterface() {}
  virtual ~TColorFieldActionInterface() {}
  virtual void triggerAction(TColorField*vf, const TPixel32 &value, bool
dragging) = 0;
};


template <class T>
class TColorFieldAction : public TColorFieldActionInterface {
  typedef void (T::*Method)(TColorField *vf, const TPixel32 &value, bool
dragging);
  T *m_target;
  Method m_method;
public:
  TColorFieldAction(T*target, Method method) : m_target(target),
m_method(method) {}
  void triggerAction(TColorField*vf, const TPixel32 &value, bool dragging)
    {(m_target->*m_method)(vf, value, dragging); }
};


template <class T>
inline void tconnect(TColorField&src, T *target, void (T::*method)(TColorField
*vf, const TPixel32 &value, bool dragging))
{
  src.addAction(new TColorFieldAction<T>(target, method));
}


//-------------------------------------------------------------------

class DVAPI TColorField2ActionInterface {
public:
  TColorField2ActionInterface() {}
  virtual ~TColorField2ActionInterface() {}
  virtual void triggerAction(TColorField2*vf, const TPixel32 &value, bool
dragging) = 0;
};


template <class T>
class TColorField2Action : public TColorField2ActionInterface {
  typedef void (T::*Method)(TColorField2 *vf, const TPixel32 &value, bool
dragging);
  T *m_target;
  Method m_method;
public:
  TColorField2Action(T*target, Method method) : m_target(target),
m_method(method) {}
  void triggerAction(TColorField2*vf, const TPixel32 &value, bool dragging)
    {(m_target->*m_method)(vf, value, dragging); }
};


template <class T>
inline void tconnect(TColorField2&src, T *target, void (T::*method)(TColorField2
*vf, const TPixel32 &value, bool dragging))
{
  src.addAction(new TColorField2Action<T>(target, method));
}

//==============================================================================
// TColorField3
//==============================================================================


class TColorField3Data;

class TColorField3 : public TWidget {
public:
  TColorField3(TWidget *parent, string name = "colorfield3");
  ~TColorField3();

  void configureNotify(const TDimension &d);
  void repaint();

  TPixel32 getValue() const;
  void setValue(const TPixel32 &);

  void addAction(TColorField3ActionInterface*action);
  void leftButtonDoubleClick(const TMouseEvent &);

private:
  TColorField3Data *m_data;
};


class DVAPI TColorField3ActionInterface {
public:
  TColorField3ActionInterface() {}
  virtual ~TColorField3ActionInterface() {}
  virtual void triggerAction(TColorField3*vf, const TPixel32 &value, bool
dragging) = 0;
};


template <class T>
class TColorField3Action : public TColorField3ActionInterface {
  typedef void (T::*Method)(TColorField3 *vf, const TPixel32 &value, bool
dragging);
  T *m_target;
  Method m_method;
public:
  TColorField3Action(T*target, Method method) : m_target(target),
m_method(method) {}
  void triggerAction(TColorField3*vf, const TPixel32 &value, bool dragging)
    {(m_target->*m_method)(vf, value, dragging); }
};


template <class T>
inline void tconnect(TColorField3&src, T *target, void (T::*method)(TColorField3
*vf, const TPixel32 &value, bool dragging))
{
  src.addAction(new TColorField3Action<T>(target, method));
}
*/

#endif
