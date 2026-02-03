#pragma once

#ifndef TPERSISTSET_H
#define TPERSISTSET_H

// TnzCore includes
#include "tpersist.h"

// STD includes
#include <vector>

#include <memory>

//**************************************************************************************
//    TPersistSet  declaration
//**************************************************************************************

/*!
  Container of TPersist instances that are <I> unique per concrete type </I>.

  This class acts as a container of TPersist instances where only a single
  instance of a given most-derived type can be stored.

  This is typically useful to \a overwrite objects of a given type, typically
  configuration options, without discarding the other objects.
*/

class DVAPI TPersistSet final : public TPersist {
  PERSIST_DECLARATION(TPersistSet)

  std::vector<TPersist *> m_objects;  //!< #owned# Stored objects.

public:
  ~TPersistSet();  //!< Destroys the stored objects.

  const std::vector<TPersist *> &objects() const {
    return m_objects;
  }  //!< Returns the stored objects list

  void insert(std::unique_ptr<TPersist>
                  object);  //!< Overwrites an object type instance with
                            //!  the supplied one.
public:
  void saveData(TOStream &os) override;  //!< Saves data to stream
  void loadData(TIStream &is) override;  //!< Loads data from stream
};

#endif  // TPERSISTSET_H
