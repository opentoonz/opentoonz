#pragma once

#ifndef MULTIPLESELECTION_H
#define MULTIPLESELECTION_H

// TnzQt includes
#include "toonzqt/selection.h"

// STD includes
#include <algorithm>

//**********************************************************************
//    MultipleSelection  definition
//**********************************************************************

/*!
  \brief      Represents a selection of multiple objects.

  \details    This template class implements a TSelection storing
              multiple objects.

  \remark     The stored objects must support operator<().
*/

template <typename T>
class MultipleSelection : public TSelection {
public:
  typedef T object_type;
  typedef std::vector<T> objects_container;

public:
  MultipleSelection() {}
  MultipleSelection(const T &t) : m_objects(1, t) {}
  MultipleSelection(const std::vector<T> &objects) : m_objects(objects) {
    std::sort(m_objects.begin(), m_objects.end());
  }

  bool isEmpty() const override { return m_objects.empty(); }
  void selectNone() override {
    m_objects.clear();
    notifyView();
  }

  bool contains(int v) const {
    return std::binary_search(m_objects.begin(), m_objects.end(), v);
  }

  bool contains(const MultipleSelection &other) const {
    return std::includes(m_objects.begin(), m_objects.end(),
                         other.m_objects.begin(), other.m_objects.end());
  }

  const objects_container &objects() const { return m_objects; }
  void setObjects(const objects_container &objects) {
    m_objects = objects;
    std::sort(m_objects.begin(), m_objects.end());
  }

  bool hasSingleObject() const { return (m_objects.size() == 1); }

protected:
  objects_container m_objects;  //!< Selected objects
};

#endif  // MULTIPLESELECTION_H
