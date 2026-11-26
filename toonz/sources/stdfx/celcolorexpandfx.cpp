#include "stdfx.h"
#include "tfxparam.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>

namespace {
double expandRadius(double value) {
  if (!std::isfinite(value))
    throw TException("Cel Color Expand: radius must be finite");
  return std::max(0.0, value);
}
int expandHalo(double radius) {
  if (!std::isfinite(radius) ||
      radius > (std::numeric_limits<int>::max() - 1) / 2)
    throw TException("Cel Color Expand: radius exceeds raster dimensions");
  return int(std::ceil(radius));
}
TDimension expandSize(int width, int height, int halo,
                      int pixelBytes = sizeof(TPixelF)) {
  const int64_t w = int64_t(width) + 2LL * halo;
  const int64_t h = int64_t(height) + 2LL * halo;
  if (w <= 0 || h <= 0 || w > std::numeric_limits<int>::max() ||
      h > std::numeric_limits<int>::max() || pixelBytes <= 0 ||
      w * h > std::numeric_limits<int>::max() / pixelBytes)
    throw TException("Cel Color Expand: requested raster is too large");
  return TDimension(int(w), int(h));
}
class ExpandRasterLock {
  TRasterP m_raster;

public:
  explicit ExpandRasterLock(const TRasterP &raster) : m_raster(raster) {
    m_raster->lock();
  }
  ~ExpandRasterLock() { m_raster->unlock(); }
};
template <class Pixel>
bool expandSeed(const Pixel &p) {
  return p.m > 0 && std::isfinite(double(p.m)) && std::isfinite(double(p.r)) &&
         std::isfinite(double(p.g)) && std::isfinite(double(p.b));
}

// Dilation must move coverage too: copying the nearest nonzero-alpha pixel
// extends an antialiased edge as a nearly invisible fringe. Search the disk
// for the strongest coverage, then choose the nearest complete source pixel
// at that coverage. This also preserves intentionally translucent artwork.
template <class Pixel>
class ExpandColorSearch {
  struct Node {
    int index, x0, y0, x1, y1;
    double maxAlpha;
  };
  static_assert(sizeof(Node) + sizeof(Pixel) <= 64,
                "Update Cel Color Expand's memory estimate");
  struct Match {
    int index        = -1;
    double alpha     = -1;
    int64_t distance = std::numeric_limits<int64_t>::max();
  };
  const TRasterPT<Pixel> &m_source;
  const int m_width;
  std::vector<Node> m_nodes;

  static int64_t square(int64_t value) { return value * value; }
  static int64_t distance(const Node &node, int x, int y) {
    const int64_t dx =
        std::max({int64_t(node.x0) - x, int64_t(0), int64_t(x) - node.x1});
    const int64_t dy =
        std::max({int64_t(node.y0) - y, int64_t(0), int64_t(y) - node.y1});
    return square(dx) + square(dy);
  }
  static void include(Node &node, const Node &child) {
    node.x0       = std::min(node.x0, child.x0);
    node.y0       = std::min(node.y0, child.y0);
    node.x1       = std::max(node.x1, child.x1);
    node.y1       = std::max(node.y1, child.y1);
    node.maxAlpha = std::max(node.maxAlpha, child.maxAlpha);
  }
  bool build(size_t begin, size_t end, bool vertical, const int *canceled) {
    if (begin == end) return true;
    if (canceled && *canceled) return false;
    const size_t middle = begin + (end - begin) / 2;
    std::nth_element(
        m_nodes.begin() + begin, m_nodes.begin() + middle,
        m_nodes.begin() + end, [this, vertical](const Node &a, const Node &b) {
          const int av = vertical ? a.index / m_width : a.index % m_width;
          const int bv = vertical ? b.index / m_width : b.index % m_width;
          return av < bv || (av == bv && a.index < b.index);
        });
    if (!build(begin, middle, !vertical, canceled) ||
        !build(middle + 1, end, !vertical, canceled))
      return false;
    if (begin < middle)
      include(m_nodes[middle], m_nodes[begin + (middle - begin) / 2]);
    if (middle + 1 < end)
      include(m_nodes[middle], m_nodes[middle + 1 + (end - middle - 1) / 2]);
    return true;
  }
  void search(size_t begin, size_t end, int x, int y, double radius2,
              Match &best) const {
    if (begin == end) return;
    const size_t middle   = begin + (end - begin) / 2;
    const Node &node      = m_nodes[middle];
    const int64_t minimum = distance(node, x, y);
    if (minimum > radius2 || node.maxAlpha < best.alpha ||
        (node.maxAlpha == best.alpha && minimum > best.distance))
      return;
    const int index    = node.index;
    const double alpha = m_source->pixels(index / m_width)[index % m_width].m;
    const int64_t d    = square(int64_t(index % m_width) - x) +
                      square(int64_t(index / m_width) - y);
    if (d <= radius2 &&
        (alpha > best.alpha ||
         (alpha == best.alpha &&
          (d < best.distance || (d == best.distance && index < best.index))))) {
      best.index    = index;
      best.alpha    = alpha;
      best.distance = d;
    }
    // Visit the more opaque subtree first so most lower-coverage branches
    // can be skipped. At equal coverage, visit the nearer bounds first.
    bool rightFirst = false;
    if (begin < middle && middle + 1 < end) {
      const Node &left  = m_nodes[begin + (middle - begin) / 2];
      const Node &right = m_nodes[middle + 1 + (end - middle - 1) / 2];
      rightFirst        = right.maxAlpha > left.maxAlpha ||
                   (right.maxAlpha == left.maxAlpha &&
                    distance(right, x, y) < distance(left, x, y));
    }
    if (rightFirst) {
      search(middle + 1, end, x, y, radius2, best);
      search(begin, middle, x, y, radius2, best);
    } else {
      search(begin, middle, x, y, radius2, best);
      search(middle + 1, end, x, y, radius2, best);
    }
  }

public:
  explicit ExpandColorSearch(const TRasterPT<Pixel> &source)
      : m_source(source), m_width(source->getLx()) {}
  bool build(const int *canceled) {
    size_t count = 0;
    for (int y = 0; y < m_source->getLy(); ++y) {
      if (canceled && *canceled) return false;
      const Pixel *row = m_source->pixels(y);
      for (int x = 0; x < m_width; ++x) count += expandSeed(row[x]);
    }
    m_nodes.reserve(count);
    for (int y = 0; y < m_source->getLy(); ++y) {
      if (canceled && *canceled) return false;
      const Pixel *row = m_source->pixels(y);
      for (int x = 0; x < m_width; ++x)
        if (expandSeed(row[x]))
          m_nodes.push_back({y * m_width + x, x, y, x, y, double(row[x].m)});
    }
    return build(0, m_nodes.size(), false, canceled);
  }
  int find(int x, int y, double radius) const {
    Match best;
    search(0, m_nodes.size(), x, y, radius * radius, best);
    return best.index;
  }
};

template <class Pixel>
void expandColors(const TRasterPT<Pixel> &source,
                  const TRasterPT<Pixel> &output, int halo, double radius,
                  const int *canceled) {
  const int w = source->getLx();
  ExpandRasterLock sourceLock(source);
  ExpandColorSearch<Pixel> search(source);
  if (!search.build(canceled)) return;
  ExpandRasterLock outputLock(output);
  for (int y = 0; y < output->getLy(); ++y) {
    if (canceled && *canceled) {
      output->clear();
      return;
    }
    Pixel *row = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      const int nearest = search.find(x + halo, y + halo, radius);
      if (nearest >= 0) row[x] = source->pixels(nearest / w)[nearest % w];
    }
  }
}
}  // namespace

class CelColorExpandFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelColorExpandFx)
  TRasterFxPort m_source;
  TDoubleParamP m_radius;
  double pixelRadius(double frame, const TRenderSettings &settings) const {
    const TAffine drawing = settings.m_affine * CelFxSourceGrid::nativeToWorld(
                                                    m_source.getFx(), frame);
    const double scale = std::sqrt(std::abs(drawing.det()));
    return expandRadius(m_radius->getValue(frame)) *
           (std::abs(scale - 1) < 1e-10 ? 1 : scale);
  }

public:
  CelColorExpandFx() : m_radius(4.0) {
    addInputPort("Source", m_source);
    bindParam(this, "radius", m_radius);
    m_radius->setValueRange(0, std::numeric_limits<double>::max());
    // No unit: the radius is in the level's own pixels (not the length unit
    // of the user, which the field would convert it to and from)
    enableComputeInFloat(true);
  }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &, double frame) override {
    return expandRadius(m_radius->getValue(frame)) == 0;
  }
  TAffine handledAffine(const TRenderSettings &settings,
                        double frame) override {
    return expandRadius(m_radius->getValue(frame)) == 0
               ? settings.m_affine
               : CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override {
    box = TRectD();
    if (!m_source.isConnected() || !m_source->doGetBBox(frame, box, settings))
      return false;
    if (!box.isEmpty() && box != TConsts::infiniteRectD) {
      const TAffine placement =
          CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
      const double radius = expandRadius(m_radius->getValue(frame));
      const double dx     = radius * std::hypot(placement.a11, placement.a12);
      const double dy     = radius * std::hypot(placement.a21, placement.a22);
      box = TRectD(box.x0 - dx, box.y0 - dy, box.x1 + dx, box.y1 + dy);
    }
    return !box.isEmpty();
  }
  void transform(double frame, int, const TRectD &output,
                 const TRenderSettings &settings, TRectD &input,
                 TRenderSettings &inputSettings) override {
    inputSettings = settings;
    input         = output.enlarge(expandHalo(pixelRadius(frame, settings)));
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return;
    TRectD input = rect.enlarge(expandHalo(pixelRadius(frame, settings)));
    m_source->dryCompute(input, frame, settings);
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &settings) override {
    const double radius = pixelRadius(frame, settings);
    if (radius == 0) return 0;
    const double halo = expandHalo(radius);
    // Source pixels and the coverage/color search tree (at most one node per
    // source pixel). The 64-byte estimate covers all three pixel formats.
    const double bytes =
        (rect.getLx() + 2 * halo) * (rect.getLy() + 2 * halo) * 64;
    // In MB, as the tile subdivision compares it with its limit in MB
    const double megabytes = std::ceil(bytes / (1024.0 * 1024.0));
    return int(std::min(double(std::numeric_limits<int>::max()),
                        std::max(0.0, megabytes)));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    std::ostringstream alias;
    alias.imbue(std::locale::classic());
    alias << TRasterFx::getAlias(frame, settings) << "[celExpand:3:"
          << std::setprecision(std::numeric_limits<double>::max_digits10)
          << expandRadius(m_radius->getValue(frame));
    const TAffine grid = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    for (double value :
         {grid.a11, grid.a12, grid.a13, grid.a21, grid.a22, grid.a23})
      alias << ',' << value;
    alias << ']';
    return alias.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() ||
        (settings.m_isCanceled && *settings.m_isCanceled))
      return;
    const double radius = pixelRadius(frame, settings);
    if (radius == 0) {
      m_source->compute(tile, frame, settings);
      if (settings.m_isCanceled && *settings.m_isCanceled)
        tile.getRaster()->clear();
      return;
    }
    const int halo = expandHalo(radius);
    const TDimension size =
        expandSize(tile.getRaster()->getLx(), tile.getRaster()->getLy(), halo,
                   tile.getRaster()->getPixelSize());
    TTile input;
    input.setRaster(tile.getRaster()->create(size.lx, size.ly));
    input.m_pos = tile.m_pos - TPointD(halo, halo);
    input.getRaster()->clear();
    m_source->compute(input, frame, settings);
    if (settings.m_isCanceled && *settings.m_isCanceled) return;
    if (TRaster32P output = tile.getRaster())
      expandColors<TPixel32>(input.getRaster(), output, halo, radius,
                             settings.m_isCanceled);
    else if (TRaster64P output = tile.getRaster())
      expandColors<TPixel64>(input.getRaster(), output, halo, radius,
                             settings.m_isCanceled);
    else if (TRasterFP output = tile.getRaster())
      expandColors<TPixelF>(input.getRaster(), output, halo, radius,
                            settings.m_isCanceled);
    else
      throw TException("Cel Color Expand: unsupported pixel type");
  }
};
FX_PLUGIN_IDENTIFIER(CelColorExpandFx, "celColorExpandFx")
