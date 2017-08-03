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
class TSmartPointerT {
protected:
  T *m_pointer;

public:
  TSmartPointerT() : m_pointer(0) {}

  TSmartPointerT(const TSmartPointerT &src) : m_pointer(src.m_pointer) {
    if (m_pointer) m_pointer->addRef();
  }

  TSmartPointerT(T *pointer) : m_pointer(pointer) {
    if (m_pointer) m_pointer->addRef();
  }

  virtual ~TSmartPointerT() {
    if (m_pointer) {
      m_pointer->release();
      m_pointer = 0;
    }
  }

  TSmartPointerT &operator=(const TSmartPointerT &src) {
    // prima addRef  e poi release per evitare brutti scherzi
    // in caso di parentela
    T *old    = m_pointer;
    m_pointer = src.m_pointer;
    if (m_pointer) m_pointer->addRef();
    if (old) old->release();
    return *this;
  }

  T *operator->() const {
    assert(m_pointer);
    return m_pointer;
  }

  T &operator*() const {
    assert(m_pointer);
    return *m_pointer;
  }

  T *getPointer() const { return m_pointer; }

  bool operator!() const { return m_pointer == 0; }
  operator bool() const { return m_pointer != 0; }

  bool operator==(const TSmartPointerT &p) const {
    return m_pointer == p.m_pointer;
  }
  bool operator!=(const TSmartPointerT &p) const {
    return m_pointer != p.m_pointer;
  }

  bool operator<(const TSmartPointerT &p) const {
    return m_pointer < p.m_pointer;
  }
  bool operator>(const TSmartPointerT &p) const {
    return m_pointer > p.m_pointer;
  }
};

//=========================================================

template <class DERIVED, class BASE>
class TDerivedSmartPointerT : public TSmartPointerT<DERIVED> {
public:
  typedef TDerivedSmartPointerT<DERIVED, BASE> DerivedSmartPointer;

  TDerivedSmartPointerT(){};
  TDerivedSmartPointerT(DERIVED *pointer) : TSmartPointerT<DERIVED>(pointer) {}

  TDerivedSmartPointerT(const TSmartPointerT<BASE> &p) {
    TSmartPointerT<DERIVED>::m_pointer =
        dynamic_cast<DERIVED *>(p.getPointer());
    if (TSmartPointerT<DERIVED>::m_pointer)
      TSmartPointerT<DERIVED>::m_pointer->addRef();
  }
};

#endif
