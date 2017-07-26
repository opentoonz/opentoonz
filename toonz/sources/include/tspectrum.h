#pragma once

#ifndef TSPECTRUM_INCLUDED
#define TSPECTRUM_INCLUDED

#include "tpixelutils.h"
#include "tutil.h"

#undef DVAPI
#undef DVVAR
#ifdef TCOLOR_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

//===================================================================

template <class T>
class DVAPI TSpectrumT {
public:
  typedef std::pair<double, T> ColorKey;
  typedef ColorKey Key;

private:
  typedef std::vector<ColorKey> KeyTable;
  KeyTable m_keys, m_sortedKeys;
  typedef std::pair<T, T> Pair;  // premultiplied/not premultiplied
  std::vector<Pair> m_samples;

  inline T getActualValue(double s) {
    assert(!m_sortedKeys.empty());
    typename std::vector<ColorKey>::const_iterator b;
    b = std::lower_bound(m_sortedKeys.begin(), m_sortedKeys.end(),
                         ColorKey(s, T()));
    if (b == m_sortedKeys.end())
      return m_sortedKeys.rbegin()->second;
    else if (b == m_sortedKeys.begin() || areAlmostEqual(b->first, s))
      return b->second;
    else {
      typename KeyTable::const_iterator a = b;
      a--;
      assert(a->first < s && s <= b->first);
      double f = (s - a->first) / (b->first - a->first);
      return blend(a->second, b->second, f);
    }
  }

  void update() {
    assert(!m_keys.empty());
    m_sortedKeys = m_keys;
    std::sort(m_sortedKeys.begin(), m_sortedKeys.end());
    if (m_samples.empty()) m_samples.resize(100);
    int n = m_samples.size();
    for (int i = 0; i < n; i++) {
      T v                 = getActualValue((double)i / (double)(n - 1));
      m_samples[i].second = v;
      m_samples[i].first  = premultiply(v);
    }
  }

public:
  TSpectrumT() : m_keys(), m_sortedKeys(), m_samples() {}

  TSpectrumT(const TSpectrumT<T> &s)
      : m_keys(s.m_keys)
      , m_sortedKeys(s.m_sortedKeys)
      , m_samples(s.m_samples) {}

  TSpectrumT(int keyCount, ColorKey keys[], int sampleCount = 100)
      : m_keys(keys, keys + keyCount) {
    m_samples.resize(sampleCount);
    update();
  }

  /*SpectrumT(std::map<double, T> &keys, int sampleCount=100)
{
m_keys = keys;
updateTable(sampleCount);
}
*/

  TSpectrumT(const T &a, const T &b, int sampleCount = 100) {
    m_keys.push_back(ColorKey(0, a));
    m_keys.push_back(ColorKey(1, b));
    m_samples.resize(sampleCount);
    update();
  }

  bool operator==(const TSpectrumT<T> &s) const {
    if (m_keys.size() != s.m_keys.size()) return false;
    typename KeyTable::const_iterator i0, i1;
    for (i0 = m_keys.begin(), i1 = s.m_keys.begin(); i0 != m_keys.end();
         ++i0, ++i1) {
      assert(i1 != s.m_keys.end());
      if (i0->first != i1->first) return false;
      if (i0->second != i1->second) return false;
    }
    return true;
  }

  bool operator!=(const TSpectrumT<T> &s) const { return !operator==(s); }

  T getValue(double s) const  // non premoltiplicati
  {
    assert(!m_keys.empty());
    int m = m_samples.size();
    assert(m > 1);
    if (s <= 0)
      return m_samples.front().second;
    else if (s >= 1)
      return m_samples.back().second;
    s     = s * (m - 1);
    int i = tfloor(s);
    assert(0 <= i && i < m - 1);
    s -= i;
    assert(0 <= s && s < 1);
    T a = m_samples[i].second;
    T b = m_samples[i + 1].second;
    return blend<T>(a, b, s);
  }

  T getPremultipliedValue(double s) const  // non premoltiplicati
  {
    assert(!m_keys.empty());
    int m = m_samples.size();
    assert(m > 1);
    if (s <= 0)
      return m_samples.front().first;
    else if (s >= 1)
      return m_samples.back().first;
    s     = s * (m - 1);
    int i = tfloor(s);
    assert(0 <= i && i < m - 1);
    s -= i;
    assert(0 <= s && s < 1);
    T a = m_samples[i].first;
    T b = m_samples[i + 1].first;
    return blend<T>(a, b, s);
  }

  int getKeyCount() const { return m_keys.size(); }

  Key getKey(int index) const {
    assert(0 <= index && index < getKeyCount());
    return m_keys[index];
  }

  void setKey(int index, const Key &key) {
    assert(0 <= index && index < getKeyCount());
    assert(0 <= key.first && key.first <= 1);
    m_keys[index] = key;
    update();
  }

  void addKey(const Key &key) {
    m_keys.push_back(key);
    update();
  }

  void removeKey(int index) {
    if (m_keys.size() <= 1) return;
    assert(0 <= index && index < getKeyCount());
    m_keys.erase(m_keys.begin() + index);
    update();
  }
};

DVAPI TSpectrumT<TPixel64> convert(const TSpectrumT<TPixel32> &s);

#ifdef _WIN32
template class DVAPI TSpectrumT<TPixel32>;
template class DVAPI TSpectrumT<TPixel64>;
#endif

typedef TSpectrumT<TPixel32> TSpectrum;
typedef TSpectrumT<TPixel64> TSpectrum64;

#ifdef _MSC_VER
#pragma warning(default : 4251)
#endif

#endif
