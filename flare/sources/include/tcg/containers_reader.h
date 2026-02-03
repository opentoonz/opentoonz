#pragma once

#ifndef TCG_CONTAINERS_READER_H
#define TCG_CONTAINERS_READER_H

// STD includes
#include <stack>

namespace tcg {

//******************************************************************************
//    Generic Reader declaration
//******************************************************************************

template <typename Val>
class generic_containers_reader {
public:
  typedef Val value_type;

public:
  virtual void openContainer()                     = 0;
  virtual void addElement(const value_type &value) = 0;
  virtual void closeContainer()                    = 0;

  // NOTE: Algorithms may require the openContainer() function to accept the
  // container's header data. Check the algorithms description if necessary.
};

//******************************************************************************
//    Some Actual Container Readers
//******************************************************************************

template <typename Container, typename Val = typename Container::value_type>
class sequential_reader {
  Container *m_values;

public:
  typedef Container container_type;
  typedef Val value_type;

public:
  sequential_reader(Container *container) : m_values(container) {}

  void openContainer() { m_values->clear(); }
  void addElement(const value_type &val) { m_values->push_back(val); }
  void closeContainer() {}

  const container_type *values() const { return m_values; }
  container_type *values() { return m_values; }
};

//------------------------------------------------------------------

template <typename Container, typename Val = typename Container::value_type>
class ordered_reader {
  Container *m_values;

public:
  typedef Container container_type;
  typedef Val value_type;

public:
  ordered_reader(Container &container) : m_values(container) {}

  void openContainer() { m_values->clear(); }
  void addElement(const value_type &val) { m_values->insert(val); }
  void closeContainer() {}

  const container_type *values() const { return m_values; }
  container_type *values() { return m_values; }
};

//------------------------------------------------------------------

template <typename Container, typename ReaderType,
          typename Val = typename ReaderType::value_type>
class multiple_reader {
public:
  typedef Container container_type;
  typedef ReaderType reader_type;
  typedef Val value_type;

protected:
  Container *m_storage;
  std::stack<reader_type> m_stack;

public:
  multiple_reader(Container *storage) : m_storage(storage) {}

  reader_type &reader() { return m_stack.top(); }

  void openContainer() {
    m_stack.push(reader_type(new typename reader_type::container_type));
  }
  void addElement(const value_type &val) { reader().addElement(val); }
  void closeContainer() {
    if (reader().values()->empty())
      delete reader().values();
    else
      m_storage->push_back(reader().values());

    m_stack.pop();
  }

  const container_type &storage() const { return m_storage; }
  container_type &storage() { return m_storage; }
};

}  // namespace tcg

#endif  // TCG_CONTAINERS_READER_H
