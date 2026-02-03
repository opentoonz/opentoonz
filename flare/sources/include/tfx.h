#pragma once

#ifndef TFX_INCLUDED
#define TFX_INCLUDED

#include <memory>

// TnzCore includes
#include "tsmartpointer.h"
#include "tpersist.h"
#include "texception.h"
#include "ttile.h"
#include "tgeometry.h"

// TnzBase includes
#include "tparamchange.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

//    Forward declarations

class TFxImp;
class TFx;
class TParam;
class TFxAttributes;
class TParamContainer;
class TParamVar;
class TRenderSettings;
class TParamUIConcept;

//===================================================================

class DVAPI TFxPort {
  friend class TFx;

protected:
  TFx *m_owner;    //!< This is an input port of m_owner
  int m_groupIdx;  //!< Dynamic group index this belongs to in m_owner (-1 if
                   //! none)
  bool m_isControl;

public:
  TFxPort(bool isControl)
      : m_owner(0), m_groupIdx(-1), m_isControl(isControl) {}
  virtual ~TFxPort() {}

  virtual TFx *getFx() const = 0;
  virtual void setFx(TFx *)  = 0;

  bool isConnected() const { return getFx() != 0; }
  bool isaControlPort() const { return m_isControl; }

  int getGroupIndex() const { return m_groupIdx; }

  TFx *getOwnerFx() const { return m_owner; }
  void setOwnerFx(TFx *fx) { m_owner = fx; }

private:
  // Not copiable
  TFxPort(const TFxPort &);
  TFxPort &operator=(const TFxPort &);
};

//-------------------------------------------------------------------

template <class T>
class TFxPortT : public TFxPort {
  friend class TFx;

protected:
  T *m_fx;

public:
  TFxPortT(bool isControl = false) : TFxPort(isControl), m_fx(0) {}
  ~TFxPortT() {
    if (m_fx) {
      m_fx->removeOutputConnection(this);
      m_fx->release();
    }
  }

  TFx *getFx() const override { return m_fx; }

  void setFx(TFx *fx) override {
    if (m_fx) m_fx->removeOutputConnection(this);

    if (fx == 0) {
      if (m_fx) m_fx->release();
      m_fx = 0;
    } else {
      T *fxt = dynamic_cast<T *>(fx);
      if (!fxt) throw TException("Fx: port type mismatch");

      fxt->addRef();
      if (m_fx) m_fx->release();

      m_fx = fxt;
      m_fx->addOutputConnection(this);
    }
  }

  T *operator->() {
    assert(m_fx);
    return m_fx;
  }
};

//===================================================================

/*
  \brief A TFxPortDynamicGroup represents a group of fx ports with the
  same name prefix whose ports that are added or removed dynamically by
  Toonz on user request.

  \sa The TFx::dynamicPortsGroup() method.
*/
class DVAPI TFxPortDynamicGroup {
public:
  typedef std::vector<TFxPort *> PortsContainer;

public:
  TFxPortDynamicGroup(const std::string &portsPrefix, int minPortsCount = 1);
  ~TFxPortDynamicGroup();

  //! Returns the group's displayed ports prefix (ports added to the group \b
  //! must
  //! have this prefix).
  const std::string &portsPrefix() const { return m_portsPrefix; }

  //! Returns the minimal number of ports to be displayed in the group. The
  //! group
  //! <B> must not <\B> be initialized in an fx implementation with more ports
  //! than
  //! this number.
  int minPortsCount() const { return m_minPortsCount; }

  //! Returns the list of ports currently in the group (may contain empty
  //! ports).
  const PortsContainer &ports() const { return m_ports; }

  //! Equivalent to checking the portName prefix against the stored one.
  bool contains(const std::string &portName) const {
    return (strncmp(m_portsPrefix.c_str(), portName.c_str(),
                    m_portsPrefix.size()) == 0);
  }

private:
  std::string m_portsPrefix;  //!< Name prefix of each stored port
  int m_minPortsCount;        //!< Ports count \a should not be smaller

  std::vector<TFxPort *> m_ports;  //!< \b Owned ports (deleted on destruction)

private:
  friend class TFx;

  // Not copyable
  TFxPortDynamicGroup(const TFxPortDynamicGroup &);
  TFxPortDynamicGroup &operator=(const TFxPortDynamicGroup &);

  void addPort(TFxPort *port);
  void removePort(
      TFxPort *port);  //!< Removes <I> and deletes <\I> the specified port
  void clear();
};

typedef TFxPortDynamicGroup TFxPortDG;

//===================================================================

class DVAPI TFxTimeRegion {
public:
  TFxTimeRegion();
  TFxTimeRegion(double start, double end);

  static TFxTimeRegion createUnlimited();

  TFxTimeRegion &operator+=(const TFxTimeRegion &rhs) {
    m_start = std::min(m_start, rhs.m_start);
    m_end   = std::max(m_end, rhs.m_end);
    return *this;
  }

  TFxTimeRegion &operator+=(double shift) {
    m_start += shift;
    m_end += shift;
    return *this;
  }

  TFxTimeRegion &operator-=(double shift) { return operator+=(-shift); }

  bool contains(double time) const;
  bool isUnlimited() const;
  bool isEmpty() const;

  bool getFrameCount(int &count) const;
  int getFirstFrame() const;
  int getLastFrame() const;

  double m_start;
  double m_end;
};

inline TFxTimeRegion operator+(const TFxTimeRegion &tr1,
                               const TFxTimeRegion &tr2) {
  return TFxTimeRegion(tr1) += tr2;
}

inline TFxTimeRegion operator+(const TFxTimeRegion &tr1, double shift) {
  return TFxTimeRegion(tr1) += shift;
}

inline TFxTimeRegion operator-(const TFxTimeRegion &tr1, double shift) {
  return TFxTimeRegion(tr1) -= shift;
}

//===================================================================

class DVAPI TFxChange {
public:
  TFx *m_fx;

  double m_firstAffectedFrame;
  double m_lastAffectedFrame;
  bool m_dragging;

  static double m_minFrame;
  static double m_maxFrame;

public:
  TFxChange(TFx *fx, double firstAffectedFrame, double lastAffectedFrame,
            bool dragging)
      : m_fx(fx)
      , m_firstAffectedFrame(firstAffectedFrame)
      , m_lastAffectedFrame(lastAffectedFrame)
      , m_dragging(dragging) {}

private:
  TFxChange();
};

//------------------------------------------------------------------------------

class TFxParamChange final : public TFxChange {
public:
  TFxParamChange(TFx *fx, double firstAffectedFrame, double lastAffectedFrame,
                 bool dragging);
  TFxParamChange(TFx *fx, const TParamChange &src);
};

//------------------------------------------------------------------------------

class TFxPortAdded final : public TFxChange {
public:
  TFxPortAdded(TFx *fx) : TFxChange(fx, m_minFrame, m_maxFrame, false) {}
  ~TFxPortAdded() {}
};

//------------------------------------------------------------------------------

class TFxPortRemoved final : public TFxChange {
public:
  TFxPortRemoved(TFx *fx) : TFxChange(fx, m_minFrame, m_maxFrame, false) {}
  ~TFxPortRemoved() {}
};

//------------------------------------------------------------------------------

class TFxParamAdded final : public TFxChange {
public:
  TFxParamAdded(TFx *fx) : TFxChange(fx, m_minFrame, m_maxFrame, false) {}
  ~TFxParamAdded() {}
};

//------------------------------------------------------------------------------

class TFxParamRemoved final : public TFxChange {
public:
  TFxParamRemoved(TFx *fx) : TFxChange(fx, m_minFrame, m_maxFrame, false) {}
  ~TFxParamRemoved() {}
};

//------------------------------------------------------------------------------

class TFxParamsUnlinked final : public TFxChange {
public:
  TFxParamsUnlinked(TFx *fx) : TFxChange(fx, m_minFrame, m_maxFrame, false) {}
  ~TFxParamsUnlinked() {}
};

//===================================================================

class DVAPI TFxObserver {
public:
  TFxObserver() {}
  virtual ~TFxObserver() {}

  virtual void onChange(const TFxChange &change) = 0;

  virtual void onChange(const TFxPortAdded &change) {
    onChange(static_cast<const TFxChange &>(change));
  }

  virtual void onChange(const TFxPortRemoved &change) {
    onChange(static_cast<const TFxChange &>(change));
  }

  virtual void onChange(const TFxParamAdded &change) {
    onChange(static_cast<const TFxChange &>(change));
  }

  virtual void onChange(const TFxParamRemoved &change) {
    onChange(static_cast<const TFxChange &>(change));
  }

  virtual void onChange(const TFxParamsUnlinked &change) {
    onChange(static_cast<const TFxChange &>(change));
  }
};

//===================================================================

class TFxInfo {
public:
  std::string m_name;
  bool m_isHidden;

public:
  TFxInfo() {}
  TFxInfo(const std::string &name, bool isHidden)
      : m_name(name), m_isHidden(isHidden) {}
};

//===================================================================

#ifdef _WIN32
template class DVAPI TSmartPointerT<TFx>;
#endif
typedef TSmartPointerT<TFx> TFxP;

//===================================================================

class DVAPI TFx : public TSmartObject, public TPersist, public TParamObserver {
  DECLARE_CLASS_CODE

  TFxImp *m_imp;

public:
  typedef TFx *CreateProc();

  TFx();
  virtual ~TFx();

  virtual std::wstring getName() const;
  void setName(std::wstring name);
  std::wstring getFxId() const;
  void setFxId(std::wstring id);

  TParamContainer *getParams();
  const TParamContainer *getParams() const;
  void addParamVar(TParamVar *var);

  virtual TFx *clone(bool recursive = true) const;
  TFx *clone(TFx *fx, bool recursive) const;

  void unlinkParams();
  virtual void linkParams(TFx *src);

  TFx *getLinkedFx() const;

  bool addInputPort(const std::string &name, TFxPort &p);  //!< Adds a port with
                                                           //! given name,
  //! returns false on
  //! duplicate names.
  //!  Ownership of the port belongs to derived implementations of TFx.
  bool addInputPort(const std::string &name, TFxPort *p,
                    int groupIndex);  //!< Adds a port with given name to the
                                      //! specified dynamic group,
  //!  returns false on duplicate names. Ownership is transferred to the group.
  bool removeInputPort(const std::string &name);  //!< Removes the port with
                                                  //! given name, returns false
  //! if not found.

  bool renamePort(const std::string &oldName, const std::string &newName);

  bool connect(
      const std::string &name,
      TFx *other);  //!< Equivalent to getInputPort(name)->setFx(other).
  bool disconnect(const std::string
                      &name);  //!< Equivalent to getInputPort(name)->setFx(0).

  int getInputPortCount() const;
  TFxPort *getInputPort(int index) const;
  TFxPort *getInputPort(const std::string &name) const;
  std::string getInputPortName(int index) const;

  virtual int dynamicPortGroupsCount() const { return 0; }
  virtual const TFxPortDG *dynamicPortGroup(int g) const { return 0; }
  bool hasDynamicPortGroups() const { return (dynamicPortGroupsCount() > 0); }
  void clearDynamicPortGroup(
      int g);  //!< \warning Users must ensure that the group's minimal
               //!  ports count is respected - this method does \b not.
  bool addOutputConnection(TFxPort *port);
  bool removeOutputConnection(TFxPort *port);

  static void listFxs(std::vector<TFxInfo> &fxInfos);
  static TFxInfo getFxInfo(const std::string &fxIdentifier);  //!< Returns info
                                                              //! associated to
  //! an fx
  //! identifier, or
  //! an
  //!  unnamed one if none was found.
  virtual bool isZerary() const { return getInputPortCount() == 0; }

  // returns the column index that provides reference frame for the FX.
  // (TColumnFx and Zerary fxs return their column indexes, n-ary FXs return
  // the reference column index of their first argument)
  // note: it returns -1 if the column is undefined (e.g. a n-ary FX with
  // arguments undefined yet)
  virtual int getReferenceColumnIndex() const;

  // se getXsheetPort() != 0 e la porta non e' connessa la si considera connessa
  // a tutte le colonne precedenti a quella corrente.
  // cfr. AddFx
  virtual TFxPort *getXsheetPort() const { return 0; }

  int getOutputConnectionCount() const;
  TFxPort *getOutputConnection(int i) const;

  virtual TFxTimeRegion getTimeRegion() const;

  void setActiveTimeRegion(const TFxTimeRegion &tr);
  TFxTimeRegion getActiveTimeRegion() const;

  virtual bool checkActiveTimeRegion() const { return true; }

  void disconnectAll();

  //! Returns a list of User Interface Concepts to be displayed when editing the
  //! fx parameters.
  //! \note Ownership of the returned array allocated with new[] is passed to
  //! callers.
  virtual void getParamUIs(TParamUIConcept *&params, int &length) {
    params = 0, length = 0;
  }

  inline std::string getFxType() const;
  virtual std::string getPluginId() const = 0;

  static TFx *create(std::string name);

  // TParamObserver-related methods
  void onChange(const TParamChange &c) override;

  void addObserver(TFxObserver *);
  void removeObserver(TFxObserver *);
  void notify(const TFxChange &change);
  void notify(const TFxPortAdded &change);
  void notify(const TFxPortRemoved &change);
  void notify(const TFxParamAdded &change);
  void notify(const TFxParamRemoved &change);

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void loadPreset(TIStream &is);  // solleva un eccezione se il preset non
                                  // corrisponde all'effetto
  void savePreset(TOStream &os);

  TFxAttributes *getAttributes() const;

  virtual std::string getAlias(double frame,
                               const TRenderSettings &info) const {
    return "";
  }

  //! Compatibility function - used to translate a port name from older Toonz
  //! versions into its current form.
  virtual void compatibilityTranslatePort(int majorVersion, int minorVersion,
                                          std::string &portName) {}

  /*-- InputPort used when the Rendering (eye) button is OFF --*/
  virtual int getPreferredInputPort() { return 0; }

  /* Virtual function for RasterFxPluginHost */
  virtual void callStartRenderHandler() {}
  virtual void callEndRenderHandler() {}
  virtual void callStartRenderFrameHandler(const TRenderSettings *rs,
                                           double frame) {}
  virtual void callEndRenderFrameHandler(const TRenderSettings *rs,
                                         double frame) {}

  // This function will be called in TFx::loadData whenever the obsolete
  // parameter is loaded. Do nothing by default.
  virtual void onObsoleteParamLoaded(const std::string &paramName) {}

  void setFxVersion(int);
  int getFxVersion() const;
  virtual void onFxVersionSet() {}

public:
  // Id-related functions

  unsigned long getIdentifier() const;
  void setIdentifier(unsigned long id);
  void setNewIdentifier();

private:
  // not implemented
  TFx(const TFx &);
  TFx &operator=(const TFx &);
};

//===================================================================

DVAPI TIStream &operator>>(TIStream &in, TFxP &p);

//===================================================================

//===================================================================

class DVAPI TFxDeclaration : public TPersistDeclaration {
public:
  TFxDeclaration(const TFxInfo &info);
};

template <class T>
class TFxDeclarationT final : public TFxDeclaration {
public:
  TFxDeclarationT(const TFxInfo &info) : TFxDeclaration(info) {}
  TPersist *create() const override { return new T; }
};

//-------------------------------------------------------------------

inline std::string TFx::getFxType() const { return getDeclaration()->getId(); }

//-------------------------------------------------------------------

#define FX_DECLARATION(T)                                                      \
                                                                               \
public:                                                                        \
  const TPersistDeclaration *getDeclaration() const override;

#define FX_IDENTIFIER(T, I)                                                    \
  namespace {                                                                  \
  TFxDeclarationT<T> info##T(TFxInfo(I, false));                               \
  }                                                                            \
  const TPersistDeclaration *T::getDeclaration() const { return &info##T; }

#define FX_IDENTIFIER_IS_HIDDEN(T, I)                                          \
  namespace {                                                                  \
  TFxDeclarationT<T> info##T(TFxInfo(I, true));                                \
  }                                                                            \
  const TPersistDeclaration *T::getDeclaration() const { return &info##T; }

//===================================================================

#endif
