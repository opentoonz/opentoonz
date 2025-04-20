#pragma once

#ifndef _TAUTOCLOSE_H_
#define _TAUTOCLOSE_H_

#include <memory>
#include <unordered_map>
#include <mutex>

#include "tgeometry.h"
#include "traster.h"
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TAutocloser {
public:
  typedef std::pair<TPoint, TPoint> Segment;

  TAutocloser(const TRasterP &r, int distance, double angle, int index,
              int opacity,std::set<UINT> autoPaintStyles = std::set<UINT>());
  ~TAutocloser();

  // calcola i segmenti e li disegna sul raster
  void exec();
  void exec(std::string id);

  // non modifica il raster. Si limita a calcolare i segmenti
  void compute(std::vector<Segment> &segments);

  // disegna sul raster i segmenti
  void draw(const std::vector<Segment> &segments);
  static bool hasSegmentCache(const std::string &id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.find(id) != m_cache.end();
  }

  static const std::vector<Segment> &getSegmentCache(const std::string &id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(id);
    if (it != m_cache.end()) {
      return it->second;
    }
    static const std::vector<Segment> empty;
    return empty;
  }

  static void setSegmentCache(const std::string &id,
                              const std::vector<Segment> &segments) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache[id] = segments;
  }

  static void invalidateSegmentCache(const std::string &id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.erase(id);
  }

  static void clearSegmentCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
  }

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

  static std::unordered_map<std::string, std::vector<TAutocloser::Segment>>
      m_cache;
  static std::mutex m_mutex;
  std::set<UINT> m_autoPaintStyles;

  // not implemented
  TAutocloser();
  TAutocloser(const TAutocloser &a);
  TAutocloser &operator=(const TAutocloser &a);
};

#endif
