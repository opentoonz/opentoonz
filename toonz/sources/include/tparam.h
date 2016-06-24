#pragma once

#ifndef T_PARAM_INCLUDED
#define T_PARAM_INCLUDED

#include "tpersist.h"
#include "tsmartpointer.h"
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TParamObserver;

//=========================================================
//! This class is an abstract class and is a generic interface to the features
//! of an object.
/*!
        It is used to access to the parameters of an object, with the purpose
        to store and retrieve the key features.
*/
class DVAPI TParam : public TSmartObject, public TPersist {
  DECLARE_CLASS_CODE
  std::string m_name;
  std::string m_description;
  std::string m_label;

public:
  /*!
          The contructor store the name of the parameter and initialize his
          interface with I/O through the class TPersist.
  */
  TParam(std::string name = "", std::string description = "",
         std::string label = "")
      : TSmartObject(m_classCode)
      , TPersist()
      , m_name(name)
      , m_description(description)
      , m_label(label) {}

  virtual ~TParam() {}
  /*!
          Returns the name of the parameter.
  */
  std::string getName() const { return m_name; };
  /*!
          This method sets the name of the parameter to \e name.
  */
  void setName(const std::string &name) { m_name = name; };

  /*!
          Return the description.
  */
  std::string getDescription() const { return m_description; }
  /*!
          Set the description.
  */
  void setDescription(const std::string &description) {
    m_description = description;
  }

  bool hasUILabel() const { return m_label != "" ? true : false; }
  void setUILabel(const std::string &l) { m_label = l; };
  std::string getUILabel() const { return m_label; };

  /*!
          This method must be implemented with a clone function, i.e a function
     that make
          a new copy of the parameter.
  */
  virtual TParam *clone() const = 0;
  /*!
          Performs a copy function of the parameter in an existing object.
  */
  virtual void copy(TParam *src) = 0;
  /*!
          An observer is a generic class that takes care and manages of canghes
     in the parameters.
          This method must be implemented with a function that add objects to an
     observer internal list.
  */
  virtual void addObserver(TParamObserver *) = 0;
  /*!
          Removes \e this from the observer.
          \sa addObserver()
  */
  virtual void removeObserver(TParamObserver *) = 0;
  /*!
          This method is used to sets the status changing notification of the
     parameter. i.e the
          observer must take care of the changes.
*/
  virtual void enableNotification(bool on) {}
  /*!
          This method must return \e true if the  notification status is
     enabled.
  */
  virtual bool isNotificationEnabled() const { return true; }

  /*!
          This pure virtual method must return a string with the value of the
     parameter
          and the precision needed.
  */
  virtual std::string getValueAlias(double frame, int precision) = 0;

  virtual bool isAnimatable() const = 0;
  /*!
          It must returns \e true if the \e frame is a keyframe.
  */
  virtual bool isKeyframe(double frame) const = 0;
  /*!
          Removes \e frame from the list of the keyframes associated to this
     parameter.
  */
  virtual void deleteKeyframe(double frame) = 0;
  /*!
          Removes all keyframes from this parameter.
  */
  virtual void clearKeyframes() = 0;
  /*!
          Makes the \e frame associated to this parameter a keyframe
  */
  virtual void assignKeyframe(double frame, const TSmartPointerT<TParam> &src,
                              double srcFrame, bool changedOnly = false) = 0;
  /*!
          This function must be overridden with a method that returns as a
     reference
          a list of keyframes in the form of a standard list.
  */
  virtual void getKeyframes(std::set<double> &frames) const {}
  /*!
          This method must return true if there are keyframes associated to this
     parameter.
  */
  virtual bool hasKeyframes() const { return false; }
  /*!
          This method must return the index of the keyframe (if any) after the
     \e frame, otherwiswe
          returns -1.
  */
  virtual int getNextKeyframe(double frame) const { return -1; }
  /*!
          This method must return the index of the keyframe (if any) before the
     \e frame, otherwiswe
          returns -1.
  */
  virtual int getPrevKeyframe(double frame) const { return -1; }
  /*!
          This method must returns a frame given the index \e index. A frame is
     a double value representing
          a frame.
  */
  virtual double keyframeIndexToFrame(int index) const { return 0.0; }

private:
  // not implemented
  TParam(const TParam &);
  TParam &operator=(const TParam &);
};

//---------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TParam>;
#endif
typedef TSmartPointerT<TParam> TParamP;

//=========================================================

#ifdef _WIN32
#define DVAPI_PARAM_SMARTPOINTER(PARAM)                                        \
  template class DVAPI TSmartPointerT<PARAM>;                                  \
  template class DVAPI TDerivedSmartPointerT<PARAM, TParam>;
#else
#define DVAPI_PARAM_SMARTPOINTER(PARAM)
#endif

#define DEFINE_PARAM_SMARTPOINTER(PARAM, TYPE)                                 \
  DVAPI_PARAM_SMARTPOINTER(PARAM)                                              \
                                                                               \
  class DVAPI PARAM##P final : public TDerivedSmartPointerT<PARAM, TParam> {   \
  public:                                                                      \
    PARAM##P(PARAM *p = 0) : DerivedSmartPointer(p) {}                         \
    PARAM##P(TYPE v) : DerivedSmartPointer(new PARAM(v)) {}                    \
    PARAM##P(TParamP &p) : DerivedSmartPointer(p) {}                           \
    PARAM##P(const TParamP &p) : DerivedSmartPointer(p) {}                     \
    operator TParamP() const { return TParamP(m_pointer); }                    \
  };

#endif
