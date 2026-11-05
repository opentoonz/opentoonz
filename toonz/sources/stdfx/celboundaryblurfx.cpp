#include "stdfx.h"
#include "tfxparam.h"
#include "celblur.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
double blurValue(double value, double low, double high) {
  return std::isfinite(value) ? std::max(low, std::min(high, value)) : low;
}
int blurMemoryMB(const TRectD &region, int bytesPerPixel) {
  if (region.isEmpty()) return 0;
  const long double bytes =
      std::ceil(region.getLx()) *
      static_cast<long double>(std::ceil(region.getLy())) * bytesPerPixel;
  const long double megabytes = std::ceil(bytes / (1024 * 1024));
  return !std::isfinite(megabytes) ||
                 megabytes > std::numeric_limits<int>::max()
             ? std::numeric_limits<int>::max()
             : int(megabytes);
}

template <class Pixel>
double blurChannelScale() {
  return Pixel::maxChannelValue;
}
template <>
double blurChannelScale<TPixelF>() {
  return 1;
}

template <class Pixel>
bool blurRaster(const TRasterPT<Pixel> &input, const TRasterPT<Pixel> &output,
                int offsetX, int offsetY, const CelBlur::Settings &settings,
                const int *canceled) {
  const int width = input->getLx(), height = input->getLy();
  std::vector<CelBlur::Pixel> pixels(CelBlur::checkedArea(width, height));
  const double scale = blurChannelScale<Pixel>();
  input->lock();
  for (int y = 0; y < height; ++y) {
    if (canceled && *canceled) {
      input->unlock();
      return false;
    }
    const Pixel *row = input->pixels(y);
    for (int x = 0; x < width; ++x) {
      if (row[x].m <= 0 || !std::isfinite(double(row[x].m)) ||
          !std::isfinite(double(row[x].r)) ||
          !std::isfinite(double(row[x].g)) || !std::isfinite(double(row[x].b)))
        continue;
      pixels[std::size_t(y) * width + x] = {{row[x].r / scale, row[x].g / scale,
                                             row[x].b / scale,
                                             row[x].m / scale}};
    }
  }
  input->unlock();
  if (!CelBlur::filter(pixels, width, height, settings, canceled)) return false;
  output->lock();
  for (int y = 0; y < output->getLy(); ++y) {
    if (canceled && *canceled) {
      output->unlock();
      return false;
    }
    Pixel *row = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      const int sx = x + offsetX, sy = y + offsetY;
      if (sx < 0 || sy < 0 || sx >= width || sy >= height) {
        row[x] = Pixel::Transparent;
        continue;
      }
      const auto &p = pixels[std::size_t(sy) * width + sx];
      auto channel  = [=](double value) {
        return scale == 1 ? value
                           : std::floor(blurValue(value, 0, 1) * scale + 0.5);
      };
      row[x] =
          Pixel(channel(p[0]), channel(p[1]), channel(p[2]), channel(p[3]));
    }
  }
  output->unlock();
  return true;
}
}  // namespace

class CelBoundaryBlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelBoundaryBlurFx)
  TRasterFxPort m_source;
  TDoubleParamP m_amount, m_smoothness, m_repeats, m_bias, m_legacy;

  CelBlur::Settings parameters(double frame,
                               const TRenderSettings &info) const {
    CelBlur::Settings result;
    const TAffine nativeToRender =
        info.m_affine * CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
    result.radius = blurValue(m_amount->getValue(frame), 1, 1000) *
                    std::sqrt(std::abs(nativeToRender.det()));
    result.blend    = blurValue(m_smoothness->getValue(frame), 1, 100) / 100;
    result.repeats  = int(blurValue(m_repeats->getValue(frame), 0, 10) + 0.5);
    result.vertical = blurValue(m_bias->getValue(frame), 0, 1) < 0.5;
    result.legacy   = blurValue(m_legacy->getValue(frame), 0, 1) >= 0.5;
    return result;
  }

  TRectD inputRegion(const TRectD &output, double frame,
                     const TRenderSettings &info) {
    if (!m_source.isConnected() || output.isEmpty()) return TRectD();
    const auto values = parameters(frame, info);
    if (values.repeats == 0) return output;
    TRectD request = output.enlarge(CelBlur::support(values));
    TRectD bounds;
    if (!m_source->getBBox(frame, bounds, info) || bounds.isEmpty())
      return TRectD();
    if (bounds == TConsts::infiniteRectD) return request;
    // Transparent samples never generate alpha. Cropping to source bounds is
    // therefore exact even for the largest repeated kernel. Keep the request
    // on the output sampling lattice, including fractional tile origins.
    bounds = TRectD(std::floor(bounds.x0 - output.x0) + output.x0,
                    std::floor(bounds.y0 - output.y0) + output.y0,
                    std::ceil(bounds.x1 - output.x0) + output.x0,
                    std::ceil(bounds.y1 - output.y0) + output.y0);
    return request * bounds;
  }

public:
  CelBoundaryBlurFx()
      : m_amount(5.0)
      , m_smoothness(100.0)
      , m_repeats(2.0)
      , m_bias(0.0)
      , m_legacy(0.0) {
    addInputPort("Source", m_source);
    bindParam(this, "amount", m_amount);
    bindParam(this, "smoothness", m_smoothness);
    bindParam(this, "repeats", m_repeats);
    bindParam(this, "bias", m_bias);
    bindParam(this, "legacy", m_legacy);
    m_amount->setValueRange(1, 1000);
    m_smoothness->setValueRange(1, 100);
    m_repeats->setValueRange(0, 10);
    m_bias->setValueRange(0, 1);
    m_bias->setUIChoices({"Vertical", "Horizontal"});
    m_legacy->setValueRange(0, 1);
    m_legacy->setUIChoices({"Off", "On"}, true);
    enableComputeInFloat(true);
  }

  bool requiresFullPrecisionSerialization() const override { return true; }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &, double) override { return false; }
  TAffine handledAffine(const TRenderSettings &, double frame) override {
    return CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &info) override {
    if (!m_source.isConnected()) {
      box = TRectD();
      return false;
    }
    return m_source->doGetBBox(frame, box, info);
  }
  void transform(double frame, int, const TRectD &out,
                 const TRenderSettings &info, TRectD &in,
                 TRenderSettings &inputInfo) override {
    inputInfo = info;
    in        = inputRegion(out, frame, info);
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_source.isConnected() || (info.m_isCanceled && *info.m_isCanceled))
      return;
    TRectD request = inputRegion(rect, frame, info);
    if (!request.isEmpty()) m_source->dryCompute(request, frame, info);
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return blurMemoryMB(inputRegion(rect, frame, info), 160);
  }
  std::string getAlias(double frame,
                       const TRenderSettings &info) const override {
    const auto values = parameters(frame, info);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << TRasterFx::getAlias(frame, info) << "[celBoundary1:"
        << std::setprecision(std::numeric_limits<double>::max_digits10)
        << values.radius << ',' << values.blend << ',' << values.repeats << ','
        << values.vertical << ',' << values.legacy << ']';
    const TAffine grid = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    for (double component :
         {grid.a11, grid.a12, grid.a13, grid.a21, grid.a22, grid.a23})
      out << ';' << component;
    return out.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() || (info.m_isCanceled && *info.m_isCanceled))
      return;
    const auto values = parameters(frame, info);
    if (!values.repeats) {
      m_source->compute(tile, frame, info);
      if (info.m_isCanceled && *info.m_isCanceled) tile.getRaster()->clear();
      return;
    }
    const auto region =
        inputRegion(TRectD(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                                   tile.getRaster()->getLy())),
                    frame, info);
    if (region.isEmpty()) return;
    if (region.getLx() > std::numeric_limits<int>::max() / 2 ||
        region.getLy() > std::numeric_limits<int>::max() / 2)
      throw TException("Cel Blur: requested image is too large");
    const int width = int(region.getLx()), height = int(region.getLy());
    CelBlur::checkedArea(width, height);
    TTile input;
    input.setRaster(tile.getRaster()->create(width, height));
    input.m_pos = region.getP00();
    input.getRaster()->setLinear(tile.getRaster()->isLinear());
    input.getRaster()->clear();
    m_source->compute(input, frame, info);
    const int offsetX = int(std::round(tile.m_pos.x - region.x0));
    const int offsetY = int(std::round(tile.m_pos.y - region.y0));
    bool complete     = false;
    if (TRaster32P output = tile.getRaster())
      complete = blurRaster<TPixel32>(input.getRaster(), output, offsetX,
                                      offsetY, values, info.m_isCanceled);
    else if (TRaster64P output = tile.getRaster())
      complete = blurRaster<TPixel64>(input.getRaster(), output, offsetX,
                                      offsetY, values, info.m_isCanceled);
    else if (TRasterFP output = tile.getRaster())
      complete = blurRaster<TPixelF>(input.getRaster(), output, offsetX,
                                     offsetY, values, info.m_isCanceled);
    else
      throw TException("Cel Blur: unsupported raster type");
    if (!complete) tile.getRaster()->clear();
  }
};

FX_PLUGIN_IDENTIFIER(CelBoundaryBlurFx, "celBoundaryBlurFx")
