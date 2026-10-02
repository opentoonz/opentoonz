#include "common/tvrender/patternsource.h"
#include "common/tvrender/patterntexturecache.h"

#include <array>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

TRaster32P solid(const TPixel32 &pixel) {
  TRaster32P raster(4, 4);
  raster->fill(pixel);
  return raster;
}

void checkSmallArtwork() {
  TRaster32P raster(16384, 32);
  raster->clear();
  raster->pixels(15)[8000] = TPixel32(40, 80, 120, 1);
  TRect bounds;
  TRop::computeBBox(raster, bounds);
  require(bounds == TRect(8000, 15, 8000, 15),
          "Original low-alpha pixel was not measured");
  const TDimension size = PatternTextures::sourceSize(bounds, 1);
  require(size == TDimension(3, 3), "Tiny artwork retained its huge canvas");
  TRaster32P prepared = PatternTextures::prepareSource(raster, bounds, size);
  require(prepared->pixels(1)[1].m == 1, "Reduction lost the tiny artwork");
  require(prepared->pixels(0)[0].m == 0, "Transparent gutter was not retained");
}

void checkSharedCanvas() {
  TRaster32P first(16, 16), second(32, 24);
  first->clear();
  second->clear();
  first->pixels(2)[3]   = TPixel32(255, 0, 0, 255);
  second->pixels(8)[11] = TPixel32(0, 0, 255, 255);
  TRect bounds, secondBounds;
  TRop::computeBBox(first, bounds);
  TRop::computeBBox(second, secondBounds);
  bounds += secondBounds;
  const TDimension size = PatternTextures::sourceSize(bounds, 3);
  TRaster32P a          = PatternTextures::prepareSource(first, bounds, size);
  TRaster32P b          = PatternTextures::prepareSource(second, bounds, size);
  require(a->pixels(1)[1].m == 255, "First animation position changed");
  require(b->pixels(7)[9].m == 255, "Second animation position changed");
  first->clear();
  TRaster32P blank = PatternTextures::prepareSource(first, bounds, size);
  TRect empty;
  TRop::computeBBox(blank, empty);
  require(empty.isEmpty() && blank->getSize() == size,
          "Blank frame lost its shared canvas");
}

void checkMemoryBudget() {
  for (int count : {1, 10, 100, 500}) {
    const TDimension size =
        PatternTextures::sourceSize(TRect(0, 0, 16383, 16383), count);
    require(size.lx <= 2048 && size.ly <= 2048, "Source exceeded side limit");
    const size_t bytes = size_t(size.lx) * size.ly * sizeof(TPixel32) * count;
    require(bytes <= PatternTextures::maxSourceBytes,
            "Animated source exceeded byte budget");
  }
  bool rejected = false;
  try {
    PatternTextures::sourceSize(TRect(), 0);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "Empty level was accepted");
  rejected = false;
  try {
    PatternTextures::sourceSize(TRect(), std::numeric_limits<int>::max());
  } catch (const std::length_error &) {
    rejected = true;
  }
  require(rejected, "Unrepresentable frame budget was accepted");
}

void checkCachePixels() {
  PatternTextures::Cache cache;
  TRaster32P source = solid(TPixel32(40, 80, 120, 200));
  TTranspFader low(0.25), high(0.75);
  TRaster32P a = cache.getTexture(source, source->getSize(), &low);
  TRaster32P b = cache.getTexture(source, source->getSize(), &high);
  require(a->pixels(0)[0].m == 50 && b->pixels(0)[0].m == 150,
          "Cached opacity is stale");
  require(cache.getTexture(source, source->getSize(), &low) == a,
          "Cache hit rebuilt texture");
  TRaster32P replacement = solid(TPixel32(10, 20, 30, 200));
  require(cache.getTexture(replacement, replacement->getSize(), &low)
                  ->pixels(0)[0]
                  .r == 10,
          "Replacement reused old source pixels");
  TColumnColorFilterFunction red(TPixel32(255, 0, 0, 255));
  TColumnColorFilterFunction blue(TPixel32(0, 0, 255, 255));
  require(
      cache.getTexture(source, source->getSize(), &red)->pixels(0)[0].r == 255,
      "Red filter did not reach texture");
  require(
      cache.getTexture(source, source->getSize(), &blue)->pixels(0)[0].b == 255,
      "Blue filter reused red texture");
}

void checkEvictionAndConcurrency() {
  PatternTextures::Cache cache;
  std::array<TRaster32P, 33> sources;
  std::array<TRaster32P, 32> textures;
  TTranspFader fader(0.5);
  for (int i = 0; i < 33; ++i) sources[i] = solid(TPixel32(i, 0, 0, 255));
  for (int i = 0; i < 32; ++i)
    textures[i] = cache.getTexture(sources[i], TDimension(4, 4), &fader);
  require(cache.getTexture(sources[0], TDimension(4, 4), &fader) == textures[0],
          "Initial hit failed");
  cache.getTexture(sources[32], TDimension(4, 4), &fader);
  require(cache.getTexture(sources[0], TDimension(4, 4), &fader) == textures[0],
          "Cache evicted MRU");
  require(
      !(cache.getTexture(sources[1], TDimension(4, 4), &fader) == textures[1]),
      "Cache did not evict LRU");
  std::array<std::future<TRaster32P>, 4> workers;
  for (auto &worker : workers)
    worker = std::async(std::launch::async, [&] {
      return cache.getTexture(sources[0], TDimension(8, 8), &fader);
    });
  TRaster32P first = workers[0].get();
  for (int i = 1; i < 4; ++i)
    require(workers[i].get() == first, "Concurrent insertion duplicated entry");
}

class UncacheableColor final : public TColorFunction {
public:
  TPixel32 color = TPixel32(255, 0, 0, 255);
  TPixel32 operator()(const TPixel32 &) const override { return color; }
  TColorFunction *clone() const override { return new UncacheableColor(*this); }
  bool getParameters(Parameters &) const override { return false; }
};

void checkCacheBypass() {
  PatternTextures::Cache cache;
  TRaster32P source = solid(TPixel32(255, 255, 255, 255));
  UncacheableColor function;
  TRaster32P first  = cache.getTexture(source, source->getSize(), &function);
  function.color    = TPixel32(0, 0, 255, 255);
  TRaster32P second = cache.getTexture(source, source->getSize(), &function);
  require(first->pixels(0)[0].r == 255 && second->pixels(0)[0].b == 255,
          "Uncacheable color function reused stale pixels");
  require(!cache.getTexture(TRaster32P(), TDimension(4, 4), nullptr),
          "Null source was accepted");
  require(!cache.getTexture(source, TDimension(1, 4), nullptr),
          "Invalid texture dimensions were accepted");
}

void checkCacheByteLimit() {
  PatternTextures::Cache cache;
  std::array<TRaster32P, 3> sources;
  TTranspFader fader(0.5);
  for (auto &source : sources) {
    source = TRaster32P(2048, 2048);
    source->fill(TPixel32(80, 40, 20, 200));
  }
  TRaster32P first =
      cache.getTexture(sources[0], sources[0]->getSize(), &fader);
  for (int i = 1; i < 3; ++i)
    cache.getTexture(sources[i], sources[i]->getSize(), &fader);
  TRaster32P reloaded =
      cache.getTexture(sources[0], sources[0]->getSize(), &fader);
  require(!(first == reloaded),
          "Cache retained more than its 64 MiB byte budget");
  require(first->pixels(0)[0] == reloaded->pixels(0)[0],
          "Eviction changed texture pixels");
}

}  // namespace

int main() try {
  checkSmallArtwork();
  checkSharedCanvas();
  checkMemoryBudget();
  checkCachePixels();
  checkEvictionAndConcurrency();
  checkCacheBypass();
  checkCacheByteLimit();
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
