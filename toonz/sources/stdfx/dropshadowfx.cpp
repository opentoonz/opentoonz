// Drop Shadow FX: Flash-compatible soft, offset and inner shadows.
#include "stdfx.h"
#include "shadowbevelutils.h"
#include "texception.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tpixelutils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using shadowbevel::boxBlur;
using shadowbevel::FloatImage;
using shadowbevel::premultipliedColor;
using shadowbevel::readRaster;
using shadowbevel::ShadowGeometry;
using shadowbevel::writePixel;

class DropShadowFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(DropShadowFx)

  TRasterFxPort m_input;
  TPixelParamP m_color;
  TDoubleParamP m_blurX, m_blurY, m_strength, m_angle, m_distance;
  TIntParamP m_quality;
  TBoolParamP m_inner, m_knockout, m_hideObject;

public:
  DropShadowFx()
      : m_color(TPixel32::Black)
      , m_blurX(2.0)
      , m_blurY(2.0)
      , m_strength(1.0)
      , m_angle(45.0)
      , m_distance(2.0)
      , m_quality(1)
      , m_inner(false)
      , m_knockout(false)
      , m_hideObject(false) {
    m_color->enableMatte(true);
    m_blurX->setMeasureName("fxLength");
    m_blurY->setMeasureName("fxLength");
    m_distance->setMeasureName("fxLength");
    m_angle->setMeasureName("angle");
    bindParam(this, "color", m_color);
    bindParam(this, "blur_x", m_blurX);
    bindParam(this, "blur_y", m_blurY);
    bindParam(this, "quality", m_quality);
    bindParam(this, "strength", m_strength);
    bindParam(this, "angle", m_angle);
    bindParam(this, "distance", m_distance);
    bindParam(this, "inner", m_inner);
    bindParam(this, "knockout", m_knockout);
    bindParam(this, "hide_object", m_hideObject);
    m_blurX->setValueRange(0, (std::numeric_limits<double>::max)());
    m_blurY->setValueRange(0, (std::numeric_limits<double>::max)());
    m_strength->setValueRange(0, 255);
    m_quality->setValueRange(0, 15);
    addInputPort("Source", m_input);
    enableComputeInFloat(true);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  ShadowGeometry geometry(double frame, const TRenderSettings &info) {
    return ShadowGeometry(info, m_blurX->getValue(frame),
                          m_blurY->getValue(frame), m_quality->getValue(),
                          m_angle->getValue(frame),
                          m_distance->getValue(frame));
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (!m_input.isConnected()) {
      bBox = TRectD();
      return false;
    }
    const bool ret = m_input->doGetBBox(frame, bBox, info);
    // An inner shadow stays inside the source.
    if (m_inner->getValue() || bBox == TConsts::infiniteRectD) return ret;
    const ShadowGeometry g = geometry(frame, info);
    TRectD shadow = bBox.enlarge(g.reachX, g.reachY) + TPointD(g.dx, g.dy);
    bBox          = m_hideObject->getValue() && !m_knockout->getValue() ? shadow
                                                                        : bBox + shadow;
    return ret;
  }

  TRectD inputRect(const TRectD &rect, const ShadowGeometry &g) {
    return rect.enlarge(g.reachX + std::abs(g.dx), g.reachY + std::abs(g.dy));
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_input.isConnected()) return;
    TRectD in = inputRect(rect, geometry(frame, info));
    m_input->dryCompute(in, frame, info);
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return TRasterFx::memorySize(inputRect(rect, geometry(frame, info)),
                                 sizeof(float) << 3);
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override;
};

void DropShadowFx::doCompute(TTile &tile, double frame,
                             const TRenderSettings &info) {
  if (!m_input.isConnected()) return;
  const ShadowGeometry g = geometry(frame, info);
  const int padX = g.reachX + std::abs(g.dx), padY = g.reachY + std::abs(g.dy);
  const TRasterP out = tile.getRaster();
  const int lx = out->getLx(), ly = out->getLy();
  TTile source;
  m_input->allocateAndCompute(source, tile.m_pos - TPointD(padX, padY),
                              TDimension(lx + 2 * padX, ly + 2 * padY), out,
                              frame, info);
  FloatImage image;
  if (TRaster32P ras = source.getRaster())
    readRaster(ras, image);
  else if (TRaster64P ras = source.getRaster())
    readRaster(ras, image);
  else if (TRasterFP ras = source.getRaster())
    readRaster(ras, image);
  else
    throw TException("Drop Shadow: unsupported pixel type");
  std::vector<float> alpha(size_t(image.lx) * image.ly);
  for (size_t i = 0; i < alpha.size(); ++i) alpha[i] = image.pixels[4 * i + 3];
  boxBlur(alpha, image.lx, image.ly, g.widthX, g.widthY, g.passes);

  float color[4];
  premultipliedColor(toPixelD(m_color->getValue(frame)), color);
  const double colorAlpha = color[3];
  const double strength   = m_strength->getValue(frame);
  const bool inner = m_inner->getValue(), knockout = m_knockout->getValue();
  const bool composite = !m_hideObject->getValue();
  auto blurred         = [&](int x, int y) {
    return x < 0 || y < 0 || x >= image.lx || y >= image.ly
                       ? 0.f
                       : alpha[size_t(y) * image.lx + x];
  };
  auto clamp01 = [](double v) { return std::max(0.0, std::min(1.0, v)); };
  out->lock();
  source.getRaster()->lock();
  for (int y = 0; y < ly; ++y)
    for (int x = 0; x < lx; ++x) {
      const float *s = image.at(x + padX, y + padY);
      // The shadow at a pixel is the blurred alpha `offset` pixels back.
      const double b = blurred(x + padX - g.dx, y + padY - g.dy);
      float result[4];
      if (inner) {
        const double a = clamp01((1 - b) * strength);  // times the color alpha
        for (int c = 0; c < 4; ++c) {
          const double glow = color[c] * a * s[3];
          result[c]         = float(knockout || !composite
                                        ? glow
                                        : glow + s[c] * (1 - a * colorAlpha));
        }
      } else {
        const double a = clamp01(b * strength);
        for (int c = 0; c < 4; ++c) {
          const double glow = color[c] * a;
          result[c]         = float(knockout    ? glow * (1 - s[3])
                                    : composite ? glow * (1 - s[3]) + s[c]
                                                : glow);
        }
      }
      if (TRaster32P ras = out)
        writePixel(ras->pixels(y)[x], result);
      else if (TRaster64P ras = out)
        writePixel(ras->pixels(y)[x], result);
      else if (TRasterFP ras = out)
        writePixel(ras->pixels(y)[x], result);
    }
  source.getRaster()->unlock();
  out->unlock();
}

FX_PLUGIN_IDENTIFIER(DropShadowFx, "dropShadowFx")
