#pragma once

#ifndef TPROPERTYTYPE_H
#define TPROPERTYTYPE_H

#include <vector>
using namespace std;

template <typename T>
class TEnumerationT {
public:
  typedef T value_type;

  TEnumerationT() {}

  void addItem(const string &id, T item) {
    m_items.push_back(std::make_pair(id, item));
  }

  int getItemCount() const { return m_items.size(); }
  void getItem(unsigned int i, string &idstring, T &value) const {
    assert(i < m_items.size());
    ItemData vp = m_items[i];
    idstring    = vp.first;
    value       = vp.second;
  }

  T getItem(unsigned int i) const {
    assert(i < m_items.size());
    ItemData vp = m_items[i];
    return vp.second;
  }

  bool isValid(T item) const {
    typename vector<ItemData>::const_iterator it =
        find_if(m_items.begin(), m_items.end(), MatchesItem(item));
    return it != m_items.end();
  }

private:
  typedef pair<string, T> ItemData;
  vector<ItemData> m_items;

  class MatchesItem {
  public:
    MatchesItem(const T &item) : m_item(item) {}

    bool operator()(const ItemData &itemData) {
      return itemData.second == m_item;
    }

  private:
    T m_item;
  };
};

typedef TEnumerationT<int> TIntEnumeration;

template <typename T>
class TValueRangeT {
public:
  typedef T value_type;
  TValueRangeT() : m_min(-1), m_max(1) {}
  TValueRangeT(T min, T max) : m_min(min), m_max(max) {}

  void getRange(T &min, T &max) const;

  bool isValid(T value) const { return m_min <= value && value <= m_max; }

private:
  T m_min, m_max;
};

typedef TValueRangeT<int> IntValueRange;
typedef TValueRangeT<double> DoubleValueRange;

#endif
