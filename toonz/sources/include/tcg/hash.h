#pragma once

#ifndef TCG_HASH_H
#define TCG_HASH_H

// tcg includes
#include "list.h"

namespace tcg {

//============================================================================================

/*!
  The hash class implements a hash map using tcg lists
*/

template <typename K, typename T, typename Hash_functor = size_t (*)(const K &)>
class hash {
public:
  typedef K key_type;
  typedef T value_type;
  typedef Hash_functor hash_type;

  struct BucketNode {
    K m_key;
    T m_val;
    size_t m_next;
    size_t m_prev;

    BucketNode(const K &key, const T &val)
        : m_key(key), m_val(val), m_next(-1), m_prev(-1) {}
    BucketNode(const std::pair<K, T> &pair)
        : m_key(pair.first), m_val(pair.second), m_next(-1), m_prev(-1) {}
    ~BucketNode() {}
  };

  typedef typename tcg::list<BucketNode>::size_t size_t;

  typedef typename tcg::list<BucketNode>::iterator iterator;
  typedef typename tcg::list<BucketNode>::const_iterator const_iterator;

private:
  std::vector<size_t> m_bucketsIdx;
  tcg::list<BucketNode> m_items;
  Hash_functor m_hash;

private:
  bool createItem(const K &key, const T &val) {
    m_items.push_back(BucketNode(key, val));

    size_t itemsCount = m_items.size(), bucketsCount = m_bucketsIdx.size();
    if (itemsCount > bucketsCount) {
      do
        bucketsCount =
            2 * bucketsCount + 1;  // Please, note that 2n here would be moronic
      while (itemsCount > bucketsCount);

      rehash(bucketsCount);
      return true;
    }
    return false;
  }

  size_t touchKey(const K &key, const T &val = T()) {
    size_t hashValue = m_hash(key) % m_bucketsIdx.size();
    size_t bucketIdx = m_bucketsIdx[hashValue];
    size_t oldIdx    = _neg;

    if (bucketIdx == _neg) {
      // A new bucket is created
      bool rehashed = createItem(key, val);
      bucketIdx     = m_items.last().m_idx;

      if (!rehashed)
        // Need to manually update the stored bucket index
        m_bucketsIdx[hashValue] = bucketIdx;

      return bucketIdx;
    } else {
      // Bucket exists - search key in it
      for (; bucketIdx != _neg && m_items[bucketIdx].m_key != key;
           oldIdx = bucketIdx, bucketIdx = m_items[bucketIdx].m_next)
        ;
    }

    if (bucketIdx == _neg) {
      // Key was not found - create an item and insert it
      bool rehashed = createItem(key, val);
      bucketIdx     = m_items.last().m_idx;

      if (!rehashed && oldIdx != _neg) {
        // Need to relink manually
        m_items[oldIdx].m_next    = bucketIdx;
        m_items[bucketIdx].m_prev = oldIdx;
      }
    }

    return bucketIdx;
  }

public:
  // NOTE: The defaulted 89 is a *good* initial buckets size when expanding by
  // the (2n+1) rule.
  // See http://www.concentric.net/~ttwang/tech/hashsize.htm for details

  hash(const Hash_functor &func = Hash_functor(), size_t bucketsCount = 89)
      : m_bucketsIdx(bucketsCount, _neg), m_hash(func) {}

  template <typename ForIt>
  hash(ForIt begin, ForIt end, const Hash_functor &func = Hash_functor(),
       size_t bucketsCount = 89)
      : m_items(begin, end), m_hash(func) {
    for (size_t nCount = m_items.nodesCount(); bucketsCount < nCount;
         bucketsCount  = 2 * bucketsCount + 1)
      ;
    rehash(bucketsCount);
  }

  //! Constructs from a range of (index, value) item pairs
  template <typename BidIt>
  hash(BidIt begin, BidIt end, size_t nodesCount,
       const Hash_functor &func = Hash_functor(), size_t bucketsCount = 89)
      : m_items(begin, end, nodesCount), m_hash(func) {
    for (size_t nCount = m_items.nodesCount(); bucketsCount < nCount;
         bucketsCount  = 2 * bucketsCount + 1)
      ;
    rehash(bucketsCount);
  }

  //--------------------------------------------------------------------------

  void rehash(size_t newSize) {
    m_bucketsIdx.clear();
    m_bucketsIdx.resize(newSize, _neg);

    size_t bucketIdx;

    iterator it, iEnd = end();
    for (it = begin(); it != iEnd; ++it) {
      bucketIdx   = m_hash(it->m_key) % newSize;
      size_t &idx = m_bucketsIdx[bucketIdx];

      it->m_next = idx;
      it->m_prev = _neg;

      if (idx != _neg) m_items[idx].m_prev = it.m_idx;
      idx                                  = it.m_idx;
    }
  }

  //--------------------------------------------------------------------------

  size_t size() const { return m_items.size(); }
  bool empty() { return size() == 0; }

  void clear() {
    m_items.clear();
    m_bucketsIdx.clear();
    m_bucketsIdx.resize(1, _neg);
  }

  //--------------------------------------------------------------------------

  iterator begin() { return m_items.begin(); }
  iterator last() { return m_items.last(); }
  iterator end() { return m_items.end(); }

  //--------------------------------------------------------------------------

  const_iterator begin() const { return m_items.begin(); }
  const_iterator last() const { return m_items.last(); }
  const_iterator end() const { return m_items.end(); }

  //--------------------------------------------------------------------------

  iterator find(const K &key) {
    size_t hashValue = m_hash(key) % m_bucketsIdx.size();
    size_t bucketIdx = m_bucketsIdx[hashValue];

    if (bucketIdx == _neg) return end();

    for (; bucketIdx != _neg && m_items[bucketIdx].m_key != key;
         bucketIdx = m_items[bucketIdx].m_next)
      ;

    return bucketIdx == _neg ? end() : iterator(&m_items, bucketIdx);
  }

  const_iterator find(const K &key) const {
    return const_iterator(&m_items, const_cast<hash *>(this)->find(key).m_idx);
  }

  //--------------------------------------------------------------------------

  iterator insert(const K &key, const T &val) {
    size_t idx         = touchKey(key);
    m_items[idx].m_val = val;
    return iterator(&m_items, idx);
  }

  //--------------------------------------------------------------------------

  //!\warning Assignment of the kind hash_map[i1] = hash_map[i2] are DANGEROUS!
  //! The
  //! reference returned on the right may be INVALIDATED if the first key is
  //! inserted!
  T &operator[](const K &key) { return m_items[touchKey(key)].m_val; }

  //--------------------------------------------------------------------------

  //!\warning The same remark of operator[] applies here!
  T &touch(const K &key, const T &val) {
    return m_items[touchKey(key, val)].m_val;
  }

  //--------------------------------------------------------------------------

  iterator insert(const std::pair<K, T> &pair) {
    return insert(pair.first, pair.second);
  }

  //--------------------------------------------------------------------------

  void erase(iterator it) {
    BucketNode &node                                     = *it;
    if (node.m_next != _neg) m_items[node.m_next].m_prev = node.m_prev;
    if (node.m_prev != _neg)
      m_items[node.m_prev].m_next = node.m_next;
    else
      m_bucketsIdx[m_hash(node.m_key) % m_bucketsIdx.size()] = _neg;

    m_items.erase(it);
  }

  //--------------------------------------------------------------------------

  void erase(const K &key) {
    iterator it = find(key);
    if (it != end()) erase(it);
  }

  //--------------------------------------------------------------------------

  friend void swap(hash &a, hash &b) {
    using std::swap;

    a.m_bucketsIdx.swap(b.m_bucketsIdx);
    swap(a.m_items, b.m_items);
    swap(a.m_hash, b.m_hash);
  }

  //--------------------------------------------------------------------------

  const tcg::list<BucketNode> &items() const { return m_items; }

  //--------------------------------------------------------------------------

  const std::vector<size_t> &buckets() const { return m_bucketsIdx; }

  //--------------------------------------------------------------------------

  const Hash_functor &hashFunctor() const { return m_hash; }

  //! Remember to rehash() if the hash functor changes
  Hash_functor &hashFunctor() { return m_hash; }
};

}  // namespace tcg

#endif  // TCG_HASH_H
