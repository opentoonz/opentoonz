#include "stdfx.h"
#include "tfxparam.h"
#include "celdirectionalmath.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
enum DirectionalParameter {
  directionAngle,
  directionGain,
  directionSize,
  frontStrength,
  frontFade,
  frontSharp,
  backStrength,
  backFade,
  backSharp,
  noiseVariation,
  noiseType,
  noiseSeed,
  noiseOffset,
  noiseThickness,
  directionCount
};
const std::array<const char *, directionCount> directionalNames{
    {"angle", "gain", "sizeVariation", "frontStrength", "frontFade",
     "frontSharp", "backStrength", "backFade", "backSharp", "noiseVariation",
     "noiseType", "noiseSeed", "noiseOffset", "noiseThickness"}};
double directionalValue(double value, double low, double high) {
  return std::isfinite(value) ? std::max(low, std::min(high, value)) : low;
}
int directionalMemoryMB(const TRectD &region, int bytesPerPixel) {
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
double directionalChannelScale() {
  return Pixel::maxChannelValue;
}
template <>
double directionalChannelScale<TPixelF>() {
  return 1;
}

template <class Pixel>
bool readDirectional(const TRasterPT<Pixel> &raster, CelMotion::Image &image,
                     const int *canceled) {
  image.width  = raster->getLx();
  image.height = raster->getLy();
  image.pixels.resize(CelMotion::checkedArea(image.width, image.height));
  const double scale = directionalChannelScale<Pixel>();
  raster->lock();
  for (int y = 0; y < image.height; ++y) {
    if (canceled && *canceled) {
      raster->unlock();
      return false;
    }
    const auto *row = raster->pixels(y);
    for (int x = 0; x < image.width; ++x) {
      const auto &pixel = row[x];
      if (pixel.m <= 0 || !std::isfinite(double(pixel.m)) ||
          !std::isfinite(double(pixel.r)) || !std::isfinite(double(pixel.g)) ||
          !std::isfinite(double(pixel.b)))
        continue;
      image.pixels[std::size_t(y) * image.width + x] = {
          {pixel.r / scale, pixel.g / scale, pixel.b / scale, pixel.m / scale}};
    }
  }
  raster->unlock();
  return true;
}
bool readDirectional(const TTile &tile, CelMotion::Image &image,
                     const int *canceled) {
  image.originX = tile.m_pos.x;
  image.originY = tile.m_pos.y;
  if (TRaster32P raster = tile.getRaster())
    return readDirectional<TPixel32>(raster, image, canceled);
  if (TRaster64P raster = tile.getRaster())
    return readDirectional<TPixel64>(raster, image, canceled);
  if (TRasterFP raster = tile.getRaster())
    return readDirectional<TPixelF>(raster, image, canceled);
  throw TException("Cel Directional Blur: unsupported input raster");
}
template <class Pixel>
bool writeDirectional(const CelMotion::Image &image,
                      const TRasterPT<Pixel> &raster, const int *canceled) {
  const double scale = directionalChannelScale<Pixel>();
  raster->lock();
  for (int y = 0; y < image.height; ++y) {
    if (canceled && *canceled) {
      raster->unlock();
      return false;
    }
    auto *row = raster->pixels(y);
    for (int x = 0; x < image.width; ++x) {
      const auto &p = image.pixels[std::size_t(y) * image.width + x];
      auto channel  = [=](double v) {
        return scale == 1
                    ? directionalValue(v, -std::numeric_limits<float>::max(),
                                       std::numeric_limits<float>::max())
                    : std::floor(directionalValue(v, 0, 1) * scale + 0.5);
      };
      auto rgb = [&](double v) {
        return channel(scale == 1 ? v : std::min(v, p[3]));
      };
      row[x] = Pixel(rgb(p[0]), rgb(p[1]), rgb(p[2]), channel(p[3]));
    }
  }
  raster->unlock();
  return true;
}
}  // namespace

class CelDirectionalBlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelDirectionalBlurFx)
  TRasterFxPort m_source, m_noise;
  std::array<TDoubleParamP, directionCount> m_parameters;

  CelDirectional::Settings parameters(double frame,
                                      const TRenderSettings &info) const {
    auto value = [&](int index, double low, double high) {
      return directionalValue(m_parameters[index]->getValue(frame), low, high);
    };
    CelDirectional::Settings result;
    result.angle = std::fmod(value(directionAngle, -1e12, 1e12), 360.0) *
                   3.14159265358979323846 / 180;
    result.gain            = value(directionGain, 0, 10);
    result.sizeVariation   = value(directionSize, 0, 100) / 100;
    result.front.strength  = value(frontStrength, 0, 4000);
    result.front.edgeFade  = value(frontFade, 0, 100) / 100;
    result.front.sharpTail = value(frontSharp, 0, 100) / 100;
    result.back.strength   = value(backStrength, 0, 4000);
    result.back.edgeFade   = value(backFade, 0, 100) / 100;
    result.back.sharpTail  = value(backSharp, 0, 100) / 100;
    result.noiseVariation  = value(noiseVariation, 0, 100) / 100;
    result.noise.type      = CelNoise::Type(int(value(noiseType, 0, 2) + 0.5));
    result.noise.seed      = std::uint32_t(value(noiseSeed, 1, 1000) + 0.5);
    result.noise.offset    = value(noiseOffset, -1e12, 1e12) / 360;
    result.noise.thickness = value(noiseThickness, 1, 100);
    const TAffine nativeToRender =
        info.m_affine * CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
    result.scale = std::sqrt(std::abs(nativeToRender.det()));
    if (!std::isfinite(result.scale) || result.scale <= 0)
      throw TException("Cel Directional Blur: invalid render scale");
    return result;
  }
  TRectD sourceDomain(double frame, const TRenderSettings &info) {
    TRenderSettings boundsInfo   = info;
    boundsInfo.m_getFullSizeBBox = true;
    TRectD box;
    if (!m_source.isConnected() || !m_source->getBBox(frame, box, boundsInfo))
      return TRectD();
    if (box == TConsts::infiniteRectD) box = info.m_cameraBox;
    if (box.isEmpty()) return TRectD();
    if (!std::isfinite(box.x0) || !std::isfinite(box.y0) ||
        !std::isfinite(box.x1) || !std::isfinite(box.y1) ||
        box == TConsts::infiniteRectD ||
        box.getLx() > std::numeric_limits<int>::max() / 4 ||
        box.getLy() > std::numeric_limits<int>::max() / 4)
      throw TException(
          "Cel Directional Blur: connect a finite source or set a finite "
          "camera domain");
    box = TRectD(std::floor(box.x0), std::floor(box.y0), std::ceil(box.x1),
                 std::ceil(box.y1));
    CelMotion::checkedArea(int(box.getLx()), int(box.getLy()));
    return box;
  }

public:
  CelDirectionalBlurFx() {
    const std::array<double, directionCount> defaults{
        {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 10}};
    const std::array<double, directionCount> maxima{{1e12, 10, 100, 4000, 100,
                                                     100, 4000, 100, 100, 100,
                                                     2, 1000, 1e12, 100}};
    for (int i = 0; i < directionCount; ++i) {
      m_parameters[i] = TDoubleParamP(defaults[i]);
      bindParam(this, directionalNames[i], m_parameters[i]);
      const double minimum = (i == directionAngle || i == noiseOffset) ? -1e12
                             : (i == noiseSeed || i == noiseThickness) ? 1
                                                                       : 0;
      m_parameters[i]->setValueRange(minimum, maxima[i]);
    }
    m_parameters[directionAngle]->setMeasureName("angle");
    m_parameters[noiseOffset]->setMeasureName("angle");
    m_parameters[noiseType]->setUIChoices({"Smooth", "Block", "Layer"});
    addInputPort("Source", m_source);
    addInputPort("Noise", m_noise);
    enableComputeInFloat(true);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &, double) override { return false; }
  TAffine handledAffine(const TRenderSettings &, double frame) override {
    return CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  void compute(TTile &tile, double frame,
               const TRenderSettings &settings) override {
    if (CelFxSourceGrid::usesCameraDomain(this, m_source.getFx(), frame,
                                          settings))
      CelFxSourceGrid::computeInCamera(this, tile, frame, settings);
    else
      TRasterFx::compute(tile, frame, settings);
  }
  void dryCompute(TRectD &rect, double frame,
                  const TRenderSettings &settings) override {
    if (CelFxSourceGrid::usesCameraDomain(this, m_source.getFx(), frame,
                                          settings))
      CelFxSourceGrid::dryComputeInCamera(this, rect, frame, settings);
    else
      TRasterFx::dryCompute(rect, frame, settings);
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
    TRenderSettings boundsInfo   = info;
    boundsInfo.m_getFullSizeBBox = true;
    if (!m_source->doGetBBox(frame, box, boundsInfo)) return false;
    if (box.isEmpty() || box == TConsts::infiniteRectD) return !box.isEmpty();
    TRenderSettings unitInfo = info;
    unitInfo.m_affine        = TAffine();
    const auto values        = parameters(frame, unitInfo);
    const TAffine placement =
        CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
    const TPointD direction =
        placement * TPointD(std::cos(values.angle), std::sin(values.angle)) -
        placement * TPointD();
    const double x = direction.x, y = direction.y;
    const double front = values.front.strength, back = values.back.strength;
    if (front == 0 && back == 0) return true;
    const double padX =
        std::max(1.0, std::abs(placement.a11) + std::abs(placement.a12));
    const double padY =
        std::max(1.0, std::abs(placement.a21) + std::abs(placement.a22));
    box = TRectD(box.x0 + std::min(-back * x, front * x) - padX,
                 box.y0 + std::min(-back * y, front * y) - padY,
                 box.x1 + std::max(-back * x, front * x) + padX,
                 box.y1 + std::max(-back * y, front * y) + padY);
    return true;
  }
  void transform(double frame, int port, const TRectD &output,
                 const TRenderSettings &info, TRectD &input,
                 TRenderSettings &inputInfo) override {
    inputInfo         = info;
    const auto values = parameters(frame, info);
    if (values.front.strength == 0 && values.back.strength == 0) {
      input = port == 0 ? output : TRectD();
      return;
    }
    input = (port == 1 && (values.noise.type != CelNoise::Layer ||
                           values.noiseVariation == 0))
                ? TRectD()
                : sourceDomain(frame, info);
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_source.isConnected() || (info.m_isCanceled && *info.m_isCanceled))
      return;
    const auto values = parameters(frame, info);
    if (values.front.strength == 0 && values.back.strength == 0) {
      m_source->dryCompute(rect, frame, info);
      return;
    }
    TRectD domain = sourceDomain(frame, info);
    if (domain.isEmpty()) return;
    m_source->dryCompute(domain, frame, info);
    if (values.noiseVariation > 0 && values.noise.type == CelNoise::Layer &&
        m_noise.isConnected()) {
      domain = sourceDomain(frame, info);
      m_noise->dryCompute(domain, frame, info);
    }
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    const auto values = parameters(frame, info);
    if (values.front.strength == 0 && values.back.strength == 0)
      return directionalMemoryMB(rect, 48);
    const long long total = static_cast<long long>(directionalMemoryMB(
                                sourceDomain(frame, info), 160)) +
                            directionalMemoryMB(rect, 48);
    return int(std::min(
        total, static_cast<long long>(std::numeric_limits<int>::max())));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &info) const override {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << TRasterFx::getAlias(frame, info) << "[celDirectional1:"
        << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (const auto &parameter : m_parameters)
      out << parameter->getValue(frame) << ',';
    const TAffine grid = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    for (double component :
         {grid.a11, grid.a12, grid.a13, grid.a21, grid.a22, grid.a23})
      out << component << ',';
    // Infinite sources are restricted to this stable composition domain.
    out << info.m_cameraBox.x0 << ',' << info.m_cameraBox.y0 << ','
        << info.m_cameraBox.x1 << ',' << info.m_cameraBox.y1 << ']';
    return out.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() || (info.m_isCanceled && *info.m_isCanceled))
      return;
    const auto values = parameters(frame, info);
    if (values.front.strength == 0 && values.back.strength == 0) {
      m_source->compute(tile, frame, info);
      if (info.m_isCanceled && *info.m_isCanceled) {
        tile.getRaster()->clear();
        return;
      }
      if (values.gain == 1) return;
      CelMotion::Image image;
      if (!readDirectional(tile, image, info.m_isCanceled)) {
        tile.getRaster()->clear();
        return;
      }
      for (auto &pixel : image.pixels)
        for (int c = 0; c < 3; ++c) pixel[c] *= values.gain;
      bool complete = false;
      if (TRaster32P raster = tile.getRaster())
        complete = writeDirectional<TPixel32>(image, raster, info.m_isCanceled);
      else if (TRaster64P raster = tile.getRaster())
        complete = writeDirectional<TPixel64>(image, raster, info.m_isCanceled);
      else if (TRasterFP raster = tile.getRaster())
        complete = writeDirectional<TPixelF>(image, raster, info.m_isCanceled);
      if (!complete) tile.getRaster()->clear();
      return;
    }
    const auto domain = sourceDomain(frame, info);
    if (domain.isEmpty()) return;
    TTile inputTile;
    const TDimension dimensions(int(domain.getLx()), int(domain.getLy()));
    inputTile.setRaster(tile.getRaster()->create(dimensions.lx, dimensions.ly));
    inputTile.m_pos = domain.getP00();
    inputTile.getRaster()->setLinear(tile.getRaster()->isLinear());
    inputTile.getRaster()->clear();
    m_source->compute(inputTile, frame, info);
    CelMotion::Image source, noise, output;
    if (!readDirectional(inputTile, source, info.m_isCanceled)) return;
    const bool useNoise = values.noiseVariation > 0 &&
                          values.noise.type == CelNoise::Layer &&
                          m_noise.isConnected();
    if (useNoise) {
      TTile noiseTile;
      noiseTile.setRaster(
          tile.getRaster()->create(dimensions.lx, dimensions.ly));
      noiseTile.m_pos = domain.getP00();
      noiseTile.getRaster()->setLinear(tile.getRaster()->isLinear());
      noiseTile.getRaster()->clear();
      m_noise->compute(noiseTile, frame, info);
      if (!readDirectional(noiseTile, noise, info.m_isCanceled)) return;
    }
    CelMotion::Shapes shapes;
    if (!CelMotion::analyze(source, shapes, info.m_isCanceled)) return;
    output.width   = tile.getRaster()->getLx();
    output.height  = tile.getRaster()->getLy();
    output.originX = tile.m_pos.x;
    output.originY = tile.m_pos.y;
    if (!CelDirectional::render(source, shapes, useNoise ? &noise : nullptr,
                                output, values, info.m_isCanceled))
      return;
    bool complete = false;
    if (TRaster32P raster = tile.getRaster())
      complete = writeDirectional<TPixel32>(output, raster, info.m_isCanceled);
    else if (TRaster64P raster = tile.getRaster())
      complete = writeDirectional<TPixel64>(output, raster, info.m_isCanceled);
    else if (TRasterFP raster = tile.getRaster())
      complete = writeDirectional<TPixelF>(output, raster, info.m_isCanceled);
    else
      throw TException("Cel Directional Blur: unsupported output raster");
    if (!complete) tile.getRaster()->clear();
  }
};

FX_PLUGIN_IDENTIFIER(CelDirectionalBlurFx, "celDirectionalBlurFx")
