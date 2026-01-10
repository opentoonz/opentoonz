#pragma once

#ifndef _TAUTOCLOSE_H_
#define _TAUTOCLOSE_H_

#include <memory>
#include <unordered_map>
#include <mutex>

#include "tgeometry.h"
#include "traster.h"
#include <set>
#include "tenv.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

extern TEnv::StringVar AutocloseVectorType;
extern TEnv::IntVar AutocloseDistance;
extern TEnv::DoubleVar AutocloseAngle;
extern TEnv::IntVar AutocloseRange;
extern TEnv::IntVar AutocloseOpacity;
extern TEnv::IntVar AutocloseIgnoreAutoPaint;

struct AutocloseSettings {
  int m_closingDistance = 30;
  double m_spotAngle    = 60.0;
  int m_opacity         = 255;
  bool m_ignoreAPInks   = false;

  AutocloseSettings() = default;

  AutocloseSettings(int distance, double angle, int opacity, bool ignoreAPInks)
      : m_closingDistance(distance)
      , m_spotAngle(angle)
      , m_opacity(opacity)
      , m_ignoreAPInks(ignoreAPInks) {}
};

class DVAPI TAutocloser {
public:
  typedef std::pair<TPoint, TPoint> Segment;

  TAutocloser(const TRasterP &r, int distance, double angle, int index,
              int opacity, std::set<int> autoPaintStyles = std::set<int>());

  TAutocloser(const TRasterP &r, int ink, AutocloseSettings settings,
              std::set<int> autoPaintStyles = std::set<int>());
  ~TAutocloser();

  // calculates the segments and draws them on the raster
  void exec();
  void exec(std::string id);

  // does not modify the raster. It only calculates the segments
  void compute(std::vector<Segment> &segments);

  // draws the segments on the raster
  void draw(const std::vector<Segment> &segments);

  // Cache management functions
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

    constexpr size_t MAX_CACHE_SIZE = 50;  // maximum number of images in cache

    m_cache[id] = segments;  // Stores or replaces

    // Remove oldest entries if exceeding limit
    if (m_cache.size() > MAX_CACHE_SIZE) {
      auto it = m_cache.begin();
      std::advance(it, m_cache.size() - MAX_CACHE_SIZE);
      m_cache.erase(m_cache.begin(), it);
    }
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
  std::set<int> m_autoPaintStyles;

  // not implemented
  TAutocloser();
  TAutocloser(const TAutocloser &a);
  TAutocloser &operator=(const TAutocloser &a);
};

#endif
