#include "stdfx.h"
#include "tfxparam.h"
#include "celdistance.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

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
template <class Pixel>
void expandColors(const TRasterPT<Pixel> &source,
                  const TRasterPT<Pixel> &output, int halo, double radius,
                  const int *canceled) {
  const int w = source->getLx(), h = source->getLy();
  std::vector<uint8_t> seeds(size_t(w) * h);
  ExpandRasterLock sourceLock(source);
  for (int y = 0; y < h; ++y) {
    if (canceled && *canceled) return;
    const Pixel *row = source->pixels(y);
    for (int x = 0; x < w; ++x) seeds[size_t(y) * w + x] = expandSeed(row[x]);
  }
  CelDistance::Field field;
  if (!CelDistance::transform(seeds, w, h, CelDistance::Metric::Euclidean,
                              field, canceled))
    return;
  ExpandRasterLock outputLock(output);
  for (int y = 0; y < output->getLy(); ++y) {
    if (canceled && *canceled) {
      output->clear();
      return;
    }
    Pixel *row = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      const size_t index = size_t(y + halo) * w + x + halo;
      const int nearest  = field.nearest[index];
      if (nearest >= 0 && field.distance[index] <= radius)
        row[x] = source->pixels(nearest / w)[nearest % w];
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
    // Source pixels, mask, two distance-transform passes and their scratch.
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
    alias << TRasterFx::getAlias(frame, settings) << "[celExpand:2:"
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
