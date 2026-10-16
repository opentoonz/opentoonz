#pragma once

#include "patterncolorsignature.h"
#include "traster.h"
#include "trop.h"

#include <list>
#include <mutex>

namespace PatternTextures {

// Sources are immutable after loading. Retaining them prevents address reuse
// from making a reloaded pattern match an older texture.
class Cache {
public:
  TRaster32P getTexture(const TRaster32P &source, const TDimension &size,
                        const TColorFunction *function) {
    if (!source || size.lx < 2 || size.ly < 2) return TRaster32P();
    const ColorSignature signature(function);
    if (signature.cacheable) {
      std::lock_guard<std::mutex> lock(m_mutex);
      TRaster32P cached = find(source, size, signature);
      if (cached) return cached;
    }

    TRaster32P texture = prepare(source, size, function);
    const size_t bytes = byteCount(source) + byteCount(texture);
    if (!signature.cacheable || bytes > maxBytes) return texture;

    std::lock_guard<std::mutex> lock(m_mutex);
    TRaster32P cached = find(source, size, signature);
    if (cached) return cached;
    while (!m_entries.empty() &&
           (m_entries.size() >= maxEntries || m_bytes + bytes > maxBytes)) {
      m_bytes -= m_entries.back().bytes;
      m_entries.pop_back();
    }
    m_entries.push_front({source, texture, size, signature, bytes});
    m_bytes += bytes;
    return texture;
  }

private:
  struct Entry {
    TRaster32P source;
    TRaster32P texture;
    TDimension size;
    ColorSignature signature;
    size_t bytes;
  };

  static constexpr size_t maxEntries = 32;
  static constexpr size_t maxBytes   = 64 * 1024 * 1024;
  std::mutex m_mutex;
  std::list<Entry> m_entries;
  size_t m_bytes = 0;

  static size_t byteCount(const TRaster32P &raster) {
    return size_t(raster->getWrap()) * raster->getLy() * sizeof(TPixel32);
  }

  // Called with m_mutex held; hits become the most recently used entry.
  TRaster32P find(const TRaster32P &source, const TDimension &size,
                  const ColorSignature &signature) {
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
      if (it->source == source && it->size == size &&
          it->signature == signature) {
        TRaster32P texture = it->texture;
        m_entries.splice(m_entries.begin(), m_entries, it);
        return texture;
      }
    }
    return TRaster32P();
  }

  static TRaster32P prepare(const TRaster32P &source, const TDimension &size,
                            const TColorFunction *function) {
    TRaster32P texture = source;
    if (source->getSize() != size) {
      texture = TRaster32P(size);
      TRop::resample(texture, source,
                     TScale(double(size.lx) / source->getLx(),
                            double(size.ly) / source->getLy()));
    }
    if (!function) return texture;

    TRaster32P tinted(size);
    std::lock_guard<TRaster> sourceLock(*texture);
    std::lock_guard<TRaster> destinationLock(*tinted);
    for (int y = 0; y < size.ly; ++y) {
      const TPixel32 *src = texture->pixels(y);
      TPixel32 *dst       = tinted->pixels(y);
      for (int x = 0; x < size.lx; ++x) dst[x] = (*function)(src[x]);
    }
    return tinted;
  }
};

}  // namespace PatternTextures
