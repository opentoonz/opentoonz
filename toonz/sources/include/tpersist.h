#pragma once

#ifndef TPERSIST_INCLUDED
#define TPERSIST_INCLUDED

//#include "tsmartpointer.h"
//#include "tpixel.h"

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TSTREAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

class TPersistDeclaration;
class TIStream;
class TOStream;

//===================================================================
//! This is an abstract class for load and save data from and to the file system
//! (i.e files on the disk)
class DVAPI TPersist {
public:
  virtual ~TPersist(){};
  /*!
          This is a pure virtual function and must be overridden with a method
     that implements
          the object loading from a stream (i.e take the object's features from
     a formatted data stream).
          For example the TStageObject class use his implementation to take the
     values of the parameters
          that characterizes a stage object as name, coordinates, etc... from a
     file.
          \a is is an input file stream
  */
  virtual void loadData(TIStream &is) = 0;
  /*!
          This is a pure virtual function and must be overridden with a method
     that implements
          the object saving to a data stream (i.e put the object's features on a
     formatted data stream).
          For example the TStageObject class use his implementation to save the
     values of the parameters
          that characterizes a stage object as name, coordinates, etc... on a
     file
          \a os is an output file stream.
  */
  virtual void saveData(TOStream &os) = 0;
  /*!
          Returns the string identifier of the object. For example a TXsheet
     object is identified by "xsheet",
          a TStageObjectTree is identified by PegbarTree.
  */
  inline std::string getStreamTag() const;

  /*!
          This pure virtual method is used to define a global pointer to the
object.
          This method is overridden with the macro PERSIST_DECLARATION(T).
  \n	For example:
          \code
class DVAPI TStageObjectTree final : public TPersist {
PERSIST_DECLARATION(TStageObjectTree)
public:

...

          \endcode
  */
  virtual const TPersistDeclaration *getDeclaration() const = 0;

  typedef TPersist *CreateProc();
  static void declare(CreateProc *);
  /*!
          If the object identified by \a name doesn't exist,
          this method creates it through TPersistDeclarationT class template.
          \sa getDeclaration()
  */
  static TPersist *create(const std::string &name);
};

//===================================================================
/*!
                This class is used to store and retrieve the id associated to an
   object.
                \sa TPersist::getStreamTag().
                The class is istantiated by the macro  PERSIST_DECLARATION(T).
        */
class DVAPI TPersistDeclaration {
  std::string m_id;

public:
  TPersistDeclaration(const std::string &id);
  virtual ~TPersistDeclaration() {}
  std::string getId() const { return m_id; };
  virtual TPersist *create() const = 0;
};

//-------------------------------------------------------------------

inline std::string TPersist::getStreamTag() const {
  return getDeclaration()->getId();
}

//-------------------------------------------------------------------
/*!
                This template class is used to create an istance of the class \a
   T.
        */
template <class T>
class TPersistDeclarationT final : public TPersistDeclaration {
public:
  /*!
          This is the constructor. Its argument is the id of the object.
          \sa TPersist::getStreamTag()
  */
  TPersistDeclarationT(const std::string &id) : TPersistDeclaration(id) {}
  /*!
          Returns a pointer to a newly created object of type TPersist.
          This template class is called by the macro PERSIST_DECLARATION(T).
          A class that calls PERSIST_DECLARATION(T) must inherits TPersist.
  */
  TPersist *create() const override { return new T; };
};

//-------------------------------------------------------------------
/*!	\file tpersist.h
        */
/*!
                This macro must be included at the beginning of a class
   declaration,
                and permits to create a new object of type T with the use of
   template
                class TPersistDeclarationT
        */
#define PERSIST_DECLARATION(T)                                                 \
  \
private:                                                                       \
  static TPersistDeclarationT<T> m_declaration;                                \
  \
public:                                                                        \
  const TPersistDeclaration *getDeclaration() const override {                 \
    return &m_declaration;                                                     \
  }

#define PERSIST_IDENTIFIER(T, I) TPersistDeclarationT<T> T::m_declaration(I);

//===================================================================

#endif
