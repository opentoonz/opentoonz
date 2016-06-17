#pragma once

#ifndef TDRAGDROP_INCLUDED
#define TDRAGDROP_INCLUDED

//#include "tw/tw.h"
// #include "tdata.h"

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TDataObject;
class TData;

//////////////////
// Exceptions
//////////////////

class DVAPI TOutOfMemory {};
class DVAPI TInitFailed {};
class DVAPI TDragDropExpt {};

//////////////////
// TDropSource/////
//////////////////

class DVAPI TDropSource {
#if defined(__GNUC__)
public:
#endif
  class Imp;
#if defined(__GNUC__)
private:
#endif

  Imp *m_imp;

private:
  // not implemented
  TDropSource(const TDropSource &);
  TDropSource &operator=(const TDropSource &);

public:
  enum DropEffect {
    None = 0,
    Copy,
    Move,
    Link,
    CopyScroll,
    MoveScroll,
    LinkScroll
  };

  TDropSource();
  virtual ~TDropSource();

  bool isValid() const;

  DropEffect doDragDrop(const TDataObject &data);

  // viene chiamata durante il drag su un target passando come
  // argomento il valore ritornato dalla onOver() del target
  // virtual CursorToUse setCursor(DropEffect dropEffect);

  virtual void setCursor(DropEffect dropEffect);
};

//////////////////
// TDropTarget/////
//////////////////
//! \brief Ascoltatore degli eventi per il Drag&Drop
class DVAPI TDragDropListener {
public:
  class Event {
  public:
    const TData *const m_data;
    TPoint m_pos;
    unsigned int m_buttonMask;

    Event(TData *data) : m_data(data), m_buttonMask(0){};
  };

  TDragDropListener(){};
  virtual ~TDragDropListener(){};
  //! Funzione per il drop di un file - ingresso nel pannello
  virtual TDropSource::DropEffect onEnter(const Event &event) = 0;
  //! Funzione per il drop di un file - interno al pannello
  virtual TDropSource::DropEffect onOver(const Event &event) = 0;
  //! Funzione per il drop di un file - rilascio del file
  virtual TDropSource::DropEffect onDrop(const Event &event) = 0;
  //! Funzione per il drop di un file - uscita dal pannello
  virtual void onLeave() = 0;
};

//////////////////
// TDataObject/////
//////////////////

class DVAPI TDataObject {
public:
  class Imp;
  Imp *m_imp;

  // not implemented
  TDataObject(const TDataObject &);
  TDataObject &operator=(const TDataObject &);

public:
  enum DataType { Text, File, Bitmap };

  TDataObject();
  ~TDataObject();

  // Costruttore per Text
  TDataObject(const std::string &str);
  TDataObject(const std::wstring &str);
  // Costruttore per File
  TDataObject(const std::vector<std::string> &vStr);
  TDataObject(const std::vector<std::wstring> &vStr);
  // Costruttore per dt_bitmap o eventualmente un tipo proprietario
  TDataObject(DataType dataType, const unsigned char *data,
              const unsigned int dataLen);

  bool getDataTypes(std::vector<DataType> &dataType) const;

  bool getData(string &str) const;
  bool getData(std::vector<string> &vStr) const;
  bool getData(DataType dataType, unsigned char *&data,
               unsigned int *dataLen) const;

  // friend class TDropSource::Imp;
  // friend class TDropTarget::Imp;
  friend class TEnumFormatEtc;
};

// void DVAPI uffa(TWidget *w);

//
// Gestione (discutibilissima) del drag&drop "interno" con tipi speciali
//
// nel dropSource:
//   MyType *data = ...
//   TDNDDataHolder<MyType> holder(data);
//   holder.doDragDrop();
//
// nel target
//   if(MyType *data = TDNDDataHolder<MyType>::getCurrentValue())
//     {
//     }

class DVAPI TDNDGenericDataHolder {
protected:
  TDNDGenericDataHolder() {}
  static TDNDGenericDataHolder *m_holder;

public:
  virtual ~TDNDGenericDataHolder() {}
  static bool isDraggingCustomData() { return m_holder != 0; }
  TDropSource::DropEffect doDragDrop();
};

template <class T>
class TDNDDataHolder : public TDNDGenericDataHolder {
  T *m_value;
  string m_name;

public:
  TDNDDataHolder(T *value, string name = "") : m_value(value), m_name(name) {
    m_holder = this;
  }
  ~TDNDDataHolder() { m_holder = 0; }
  T *getValue() const { return m_value; }
  string getName() const { return m_name; }
  static T *getCurrentValue() {
    TDNDDataHolder<T> *holder = dynamic_cast<TDNDDataHolder<T> *>(m_holder);
    return holder ? holder->getValue() : 0;
  }
};

#endif
