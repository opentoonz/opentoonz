#pragma once

#ifndef TSMARTPOINTER_INCLUDED
#define TSMARTPOINTER_INCLUDED

#include "tutil.h"
#include "tatomicvar.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

#ifndef NDEBUG
#define INSTANCE_COUNT_ENABLED
#endif

//=========================================================

class DVAPI TSmartObject {
  TAtomicVar m_refCount;

#ifdef INSTANCE_COUNT_ENABLED
  const TINT32 m_classCodeRef;
  static const TINT32 m_unknownClassCode;
#endif

public:
  typedef short ClassCode;

  TSmartObject(ClassCode
#ifdef INSTANCE_COUNT_ENABLED
                   classCode
#endif
               )
      : m_refCount()
#ifdef INSTANCE_COUNT_ENABLED
      , m_classCodeRef(classCode)
#endif
  {
#ifdef INSTANCE_COUNT_ENABLED
    incrementInstanceCount();
#endif
  }

  TSmartObject()
      : m_refCount()
#ifdef INSTANCE_COUNT_ENABLED
      , m_classCodeRef(m_unknownClassCode)
#endif
  {
#ifdef INSTANCE_COUNT_ENABLED
    incrementInstanceCount();
#endif
  }

  virtual ~TSmartObject() {
    assert(m_refCount == 0);
#ifdef INSTANCE_COUNT_ENABLED
    decrementInstanceCount();
#endif
  }

  inline void addRef() { ++m_refCount; }
  inline void release() {
    if ((--m_refCount) <= 0) delete this;
  };
  inline TINT32 getRefCount() const { return m_refCount; }

  static TINT32 getInstanceCount(ClassCode code);

private:
  void incrementInstanceCount();
  void decrementInstanceCount();

private:
  // not implemented
  TSmartObject(const TSmartObject &);
  TSmartObject &operator=(const TSmartObject &);
};

#define DECLARE_CLASS_CODE                                                     \
  \
private:                                                                       \
  static const TSmartObject::ClassCode m_classCode;                            \
  \
public:                                                                        \
  inline static TINT32 getInstanceCount() {                                    \
    return TSmartObject::getInstanceCount(m_classCode);                        \
  }

#define DEFINE_CLASS_CODE(T, ID)                                               \
  const TSmartObject::ClassCode T::m_classCode = ID;

//=========================================================

template <class T>
class TSmartHolderT {
public:
  typedef T Type;

protected:
  T *m_pointer;
  T& reference() const
    { assert(m_pointer); return *m_pointer; }

public:
  TSmartHolderT() : m_pointer(0) {}
  TSmartHolderT(const TSmartHolderT &src) : m_pointer(src.m_pointer)
    { if (m_pointer) m_pointer->addRef(); }
  explicit TSmartHolderT(T *pointer) : m_pointer(pointer)
    { if (m_pointer) m_pointer->addRef(); }
  virtual ~TSmartHolderT()
    { if (m_pointer) { m_pointer->release(); m_pointer = 0; } }

  TSmartHolderT& operator=(const TSmartHolderT &src) { set(src); return *this; }

  void set(T *pointer) {
    if (m_pointer != pointer) {
      // call 'addRef' before 'release'
      if (pointer) pointer->addRef();
      if (m_pointer) m_pointer->release();
      m_pointer = pointer;
    }
  }
  void set(const TSmartHolderT &src)
    { set(src.m_pointer); }
  void reset()
    { set(0); }
  bool operator!() const
    { return m_pointer == 0; }
  operator bool() const
    { return m_pointer != 0; }

  bool operator==(const T *p) const { return m_pointer == p; }
  bool operator!=(const T *p) const { return m_pointer != p; }
  bool operator< (const T *p) const { return m_pointer <  p; }
  bool operator> (const T *p) const { return m_pointer >  p; }
  bool operator<=(const T *p) const { return m_pointer <= p; }
  bool operator>=(const T *p) const { return m_pointer >= p; }

  template<class TT>
  bool equal(const TT *p) const { return m_pointer == p; }
  template<class TT>
  bool less(const TT *p) const { return m_pointer < p; }
  template<class TT>
  bool greater(const TT *p) const { return m_pointer > p; }

  template<class TT>
  bool operator==(const TSmartHolderT<TT> &p) const { return p.equal(m_pointer); }
  template<class TT>
  bool operator!=(const TSmartHolderT<TT> &p) const { return !p.equal(m_pointer); }
  template<class TT>
  bool operator< (const TSmartHolderT<TT> &p) const { return p.greater(m_pointer); }
  template<class TT>
  bool operator> (const TSmartHolderT<TT> &p) const { return p.less(m_pointer); }
  template<class TT>
  bool operator<=(const TSmartHolderT<TT> &p) const { return !p.less(m_pointer); }
  template<class TT>
  bool operator>=(const TSmartHolderT<TT> &p) const { return !p.greater(m_pointer); }
};

//=========================================================

template <class T>
class TSmartPointerConstT: public TSmartHolderT<T> {
public:
  typedef TSmartHolderT<T> Holder;
  TSmartPointerConstT() {}
  TSmartPointerConstT(const TSmartPointerConstT &src): Holder(src) {}
  explicit TSmartPointerConstT(T *pointer): Holder(pointer) {}
  TSmartPointerConstT& operator=(const TSmartPointerConstT &src) { Holder::set(src); return *this; }
  const T* getConstPointer() const { return Holder::m_pointer; }
  const T* operator->() const { return &Holder::reference(); }
  const T& operator*() const { return Holder::reference(); }
  const T* getPointer() const { return Holder::m_pointer; }
};

//=========================================================

template <class T>
class TSmartPointerT: public TSmartPointerConstT<T> {
public:
  typedef TSmartPointerConstT<T> Const;
  TSmartPointerT() {}
  TSmartPointerT(const TSmartPointerT &src): Const(src) {}
  TSmartPointerT(T *pointer): Const(pointer) {}
  TSmartPointerT& operator=(const TSmartPointerT &src) { Const::set(src); return *this; }
  T* operator->() const { return &Const::reference(); }
  T& operator*() const { return Const::reference(); }
  T* getPointer() const { return Const::m_pointer; }
};

//=========================================================

template <class DERIVED, class BASE>
class TDerivedSmartPointerT : public TSmartPointerT<DERIVED> {
public:
  typedef TDerivedSmartPointerT<DERIVED, BASE> DerivedSmartPointer;

  TDerivedSmartPointerT() { };
  TDerivedSmartPointerT(DERIVED *pointer):
    TSmartPointerT<DERIVED>(pointer) { }
  TDerivedSmartPointerT(const TSmartPointerT<BASE> &p):
    TSmartPointerT<DERIVED>(dynamic_cast<DERIVED*>(p.getPointer())) { }
};

//=========================================================

typedef TSmartPointerT<TSmartObject> TSmartObjectP;

#endif
