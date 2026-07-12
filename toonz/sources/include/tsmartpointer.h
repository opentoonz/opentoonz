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

  explicit TSmartObject(ClassCode classCode)
      : m_refCount()
#ifdef INSTANCE_COUNT_ENABLED
      , m_classCodeRef(classCode)
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
    if (--m_refCount <= 0) delete this;
  }

  inline TINT32 getRefCount() const { return m_refCount; }

  static TINT32 getInstanceCount(ClassCode code);

private:
  void incrementInstanceCount();
  void decrementInstanceCount();

  TSmartObject(const TSmartObject&);
  TSmartObject& operator=(const TSmartObject&);
};

//=========================================================

#define DECLARE_CLASS_CODE                                                     \
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
protected:
  T* m_pointer = nullptr;

  T& reference() const {
    assert(m_pointer != nullptr);
    return *m_pointer;
  }

public:
  TSmartHolderT() = default;

  TSmartHolderT(T* ptr) : m_pointer(ptr) {
    if (m_pointer) m_pointer->addRef();
  }

  TSmartHolderT(const TSmartHolderT& other) {
    m_pointer = other.m_pointer;         // Assign the pointer first
    if (m_pointer) m_pointer->addRef();  // Then increment the reference count
  }

  TSmartHolderT& operator=(const TSmartHolderT& other) {
    if (this != &other) set(other.m_pointer);
    return *this;
  }

  TSmartHolderT(TSmartHolderT&& other) noexcept : m_pointer(other.m_pointer) {
    other.m_pointer = nullptr;
  }

  TSmartHolderT& operator=(TSmartHolderT&& other) noexcept {
    if (this != &other) {
      if (m_pointer) m_pointer->release();
      m_pointer       = other.m_pointer;
      other.m_pointer = nullptr;
    }
    return *this;
  }

  ~TSmartHolderT() {
    if (m_pointer) {
      m_pointer->release();
      m_pointer = nullptr;
    }
  }

  void set(T* ptr) {
    if (m_pointer != ptr) {
      if (ptr) ptr->addRef();
      if (m_pointer) m_pointer->release();
      m_pointer = ptr;
    }
  }

  void set(const TSmartHolderT& other) { set(other.m_pointer); }
  void reset() { set(nullptr); }

  operator bool() const noexcept { return m_pointer != nullptr; }
  bool operator!() const noexcept { return m_pointer == nullptr; }

  T* getPointer() const noexcept { return m_pointer; }

  // Comparison operators with raw pointers of same type
  bool operator==(const T* p) const noexcept { return m_pointer == p; }
  bool operator!=(const T* p) const noexcept { return m_pointer != p; }

  // Comparison operators with same type TSmartHolderT
  bool operator==(const TSmartHolderT& other) const noexcept {
    return m_pointer == other.m_pointer;
  }
  bool operator!=(const TSmartHolderT& other) const noexcept {
    return m_pointer != other.m_pointer;
  }

  // These template comparison operators allow comparing TSmartHolderT<T>
  // with TSmartHolderT<U> where T and U are different types.
  // This is essential for comparing TRasterP with TRaster32P, etc.

  // Helper template functions for comparing with raw pointers of different
  // types
  template <class TT>
  bool equal(const TT* p) const {
    return m_pointer == p;  // Direct pointer comparison
  }

  template <class TT>
  bool less(const TT* p) const {
    return m_pointer < p;  // Direct pointer comparison
  }

  template <class TT>
  bool greater(const TT* p) const {
    return m_pointer > p;  // Direct pointer comparison
  }

  // Template operators for comparing with TSmartHolderT of different types
  template <class TT>
  bool operator==(const TSmartHolderT<TT>& p) const {
    // Use the other object's equal method with our pointer
    return p.equal(m_pointer);
  }

  template <class TT>
  bool operator!=(const TSmartHolderT<TT>& p) const {
    return !p.equal(m_pointer);
  }

  template <class TT>
  bool operator<(const TSmartHolderT<TT>& p) const {
    return p.greater(m_pointer);
  }

  template <class TT>
  bool operator>(const TSmartHolderT<TT>& p) const {
    return p.less(m_pointer);
  }

  template <class TT>
  bool operator<=(const TSmartHolderT<TT>& p) const {
    return !p.less(m_pointer);
  }

  template <class TT>
  bool operator>=(const TSmartHolderT<TT>& p) const {
    return !p.greater(m_pointer);
  }
};

//=========================================================

template <class T>
class TSmartPointerConstT : public TSmartHolderT<T> {
  using Base = TSmartHolderT<T>;

public:
  typedef TSmartHolderT<T> Holder;

  TSmartPointerConstT() = default;
  TSmartPointerConstT(T* p) : Base(p) {}
  TSmartPointerConstT(const TSmartPointerConstT&)                = default;
  TSmartPointerConstT& operator=(const TSmartPointerConstT&)     = default;
  TSmartPointerConstT(TSmartPointerConstT&&) noexcept            = default;
  TSmartPointerConstT& operator=(TSmartPointerConstT&&) noexcept = default;

  const T* operator->() const { return this->m_pointer; }
  const T& operator*() const { return Base::reference(); }
  const T* getPointer() const noexcept { return this->m_pointer; }
};

//=========================================================

template <class T>
class TSmartPointerT : public TSmartPointerConstT<T> {
  using Base = TSmartPointerConstT<T>;

public:
  typedef TSmartPointerConstT<T> Const;

  TSmartPointerT() = default;
  TSmartPointerT(T* p) : Base(p) {}
  TSmartPointerT(const TSmartPointerT&)                = default;
  TSmartPointerT& operator=(const TSmartPointerT&)     = default;
  TSmartPointerT(TSmartPointerT&&) noexcept            = default;
  TSmartPointerT& operator=(TSmartPointerT&&) noexcept = default;

  T* operator->() const { return this->m_pointer; }
  T& operator*() const { return Base::reference(); }
  T* getPointer() const noexcept { return this->m_pointer; }
};

//=========================================================

template <class DERIVED, class BASE>
class TDerivedSmartPointerT : public TSmartPointerT<DERIVED> {
public:
  typedef TDerivedSmartPointerT<DERIVED, BASE> DerivedSmartPointer;

  TDerivedSmartPointerT() = default;
  TDerivedSmartPointerT(DERIVED* p) : TSmartPointerT<DERIVED>(p) {}
  TDerivedSmartPointerT(const TSmartPointerT<BASE>& p)
      : TSmartPointerT<DERIVED>(dynamic_cast<DERIVED*>(p.getPointer())) {}
};

//=========================================================

typedef TSmartPointerT<TSmartObject> TSmartObjectP;

#endif  // TSMARTPOINTER_INCLUDED
