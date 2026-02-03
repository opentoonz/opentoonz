#pragma once

#ifndef TMETAIMAGE_INCLUDED
#define TMETAIMAGE_INCLUDED

#include "timage.h"
#include "tthreadmessage.h"
#include "tsmartpointer.h"
#include "tvariant.h"
#include "tconstwrapper.h"

#include <QString>
#include <QReadLocker>
#include <QWriteLocker>
#include <QReadWriteLock>

#include <string>

#undef DVAPI
#undef DVVAR
#ifdef TMETAIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class TMetaObject;
class TMetaObjectHandler;
typedef TSmartPointerT<TMetaObject> TMetaObjectP;  //!< smart pointer to TMetaObject
typedef TMetaObjectP::Holder        TMetaObjectH;  //!< smart holder of TMetaObject
typedef TMetaObjectP::Const         TMetaObjectPC; //!< smart pointer to constant TMetaObject
typedef std::vector<TMetaObjectP> TMetaObjectList;
typedef TConstArrayWrapperT<TMetaObjectPC, TMetaObjectP> TMetaObjectListCW; // TMetaObjectListConstWrapper

//-------------------------------------------------------------------

class DVAPI TMetaObjectType {
public:
  const TStringId name;

  TMetaObjectType(const TStringId &name);
  virtual ~TMetaObjectType();

  void registerAlias(const TStringId &alias);
  void unregisterAlias(const TStringId &alias);

  virtual TMetaObjectHandler* createHandler(TMetaObject &obj) const
    { return 0; }
  virtual QString getLocalName() const
    { return QString::fromStdString(name.str()); }
};

//-------------------------------------------------------------------

class DVAPI TMetaObject: public TSmartObject, public TVariantOwner {
public:
  typedef TMetaObjectHandler* (*Fabric)(TMetaObject&);
  typedef std::map<TStringId, const TMetaObjectType*> Registry;

  struct LinkedList {
    TMetaObject *first, *last;
    LinkedList(): first(), last() { }
  };
  typedef std::map<TStringId, LinkedList> LinkedMap;
  typedef LinkedMap::iterator LinkedMapEntry;

private:
  LinkedMapEntry m_typeLink;
  TMetaObject *m_previous, *m_next;
  const TMetaObjectType *m_typeDesc;
  TMetaObjectHandler *m_handler;
  TVariant m_data;

  static Registry& registry();
  static LinkedMap& linkedMap();

  static void rewrapAll(const TStringId &type);
  void rewrap(const TStringId &type);

  void linkToType(const TStringId &type);
  void unlinkFromType();

  TMetaObject(const TMetaObject &other);

public:
  explicit TMetaObject(const TStringId &typeName = TStringId(), const TVariant &data = TVariant());
  explicit TMetaObject(const std::string &typeName, const TVariant &data = TVariant());
  ~TMetaObject();

  void setType(const TStringId &name);
  inline void setType(const std::string &name)
    { setType(TStringId(name)); }
  inline void resetType()
    { setType(TStringId()); }

  inline const TMetaObjectType* getTypeDesc() const
    { return m_typeDesc; }
  inline const TStringId& getType() const
    { return m_typeLink->first; }
  inline const std::string& getTypeName() const
    { return getType().str(); }
  inline const TVariant& data() const
    { return m_data; }
  inline TVariant& data()
    { return m_data; }

  void setDefaults();

  template<typename T>
  const T* getHandler() const
    { return dynamic_cast<const T*>(m_handler); }
  template<typename T>
  T* getHandler()
    { return dynamic_cast<T*>(m_handler); }

  TMetaObjectHandler* handler() { return m_handler; }
  const TMetaObjectHandler* handler() const { return m_handler; }

  void onVariantChanged(const TVariant &value) override;

  virtual TMetaObject* clone() const;

public:
  static const Registry& getRegistry() { return registry(); }
  static void registerType(const TStringId &name, const TMetaObjectType &type); //!< register new or add alias
  static void unregisterType(const TStringId &name); //!< unregister single alias
  static void unregisterType(const TMetaObjectType &type); //!< unregister all aliases
  static const TMetaObjectType* findType(const TStringId &name);
};

//-------------------------------------------------------------------

class DVAPI TMetaObjectHandler {
protected:
  class LockEvents {
  public:
    TMetaObjectHandler &owner;
    explicit LockEvents(TMetaObjectHandler &owner):
      owner(owner) { ++owner.m_locks; }
    ~LockEvents() { --owner.m_locks; }
  };

private:
  TMetaObject &m_object;
  const TMetaObjectType &m_typeDesc;
  TAtomicVar m_locks;

public:
  TMetaObjectHandler(TMetaObject &object):
    m_object(object),
    m_typeDesc(*m_object.getTypeDesc())
      { assert(m_object.getTypeDesc()); }
  virtual ~TMetaObjectHandler() { }

  inline const TMetaObjectType& getTypeDesc() const
    { return m_typeDesc; }
  inline const TStringId& getType() const
    { return getTypeDesc().name; }
  inline const std::string& getTypeName() const
    { return getType().str(); }
  inline const TMetaObject& object() const
    { return m_object; }
  inline TMetaObject& object()
    { return m_object; }
  inline const TStringId& getAlias() const
    { return object().getType(); }
  inline const std::string& getAliasName() const
    { return getAlias().str(); }
  inline const TVariant& data() const
    { return object().data(); }
  inline TVariant& data()
    { return object().data(); }

protected:
  virtual void onSetDefaults() { }
  virtual void onDataChanged(const TVariant &value) { }
  virtual void onFixData() { }

public:
  void setDefaults() {
    { LockEvents lock(*this); onSetDefaults(); }
    data().touch();
  }
  void dataChanged(const TVariant &value)
    { LockEvents lock(*this); if (m_locks == 1) onDataChanged(value); }
  void fixData()
    { LockEvents lock(*this); onFixData(); }
};

//-------------------------------------------------------------------

//! An image containing an assistants for painting.

class DVAPI TMetaImage final : public TImage {
public:
  class Reader: public QReadLocker {
  private:
    const TMetaImage &m_image;
    const TMetaObjectListCW m_objects;
  public:
    Reader(const TMetaImage &image):
      QReadLocker(&image.m_rwLock),
      m_image(image),
      m_objects(image.m_objects) { }
    const TMetaImage& image() const
      { return m_image; }
    const TMetaObjectListCW& get() const
      { return m_objects; }
    const TMetaObjectListCW& operator*() const
      { return get(); }
    const TMetaObjectListCW* operator->() const
      { return &get(); }
  };

  class Writer: public QWriteLocker {
  private:
    TMetaImage &m_image;
  public:
    Writer(TMetaImage &image):
      QWriteLocker(&image.m_rwLock), m_image(image) { }
    TMetaImage& image() const
      { return m_image; }
    TMetaObjectList& get() const
      { return m_image.m_objects; }
    TMetaObjectList& operator*() const
      { return get(); }
    TMetaObjectList* operator->() const
      { return &get(); }
  };

private:
  mutable QReadWriteLock m_rwLock;
  TMetaObjectList m_objects;

  TMetaImage(const TMetaImage &other);

  //! not implemented
  TMetaImage &operator=(const TMetaImage &) { return *this; }

public:
  TMetaImage();
  ~TMetaImage();

  //! Return the image type
  TImage::Type getType() const override { return TImage::META; }
  //! Return a clone of image
  TImage* cloneImage() const override;
  //! Return the bbox of the image
  TRectD getBBox() const override;
};

#endif
