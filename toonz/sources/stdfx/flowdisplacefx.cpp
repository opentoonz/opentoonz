#include "stdfx.h"
#include "tfxparam.h"
#include "flowdisplace.h"
#include "celpixels.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>

// Flow Displace: warps the source with a second image, a vector field or a
// grey map, weighted by a matte and animated by a drifting noise, for the
// secondary motion of capes, hair and water. The math is in flowdisplace.h.
using CelPixels::bounded;
using CelPixels::discrete;
using CelPixels::megabytes;
using CelPixels::renderScale;

class FlowDisplaceFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(FlowDisplaceFx)
  TRasterFxPort m_source, m_field, m_matte;
  TDoubleParamP m_mode, m_strength, m_direction, m_invert, m_noiseAmplitude,
      m_noiseSize, m_noiseSpeed, m_seed;

  FlowDisplace::Settings settings(double frame,
                                  const TRenderSettings &render) const {
    const double scale = renderScale(render);
    FlowDisplace::Settings s;
    s.mode      = discrete(m_mode->getValue(frame), 1) == 1
                      ? FlowDisplace::Mode::Map
                      : FlowDisplace::Mode::Flow;
    s.strength  = bounded(m_strength->getValue(frame), -1000, 1000) * scale;
    s.direction = bounded(m_direction->getValue(frame), -3600, 3600);
    s.invert    = discrete(m_invert->getValue(frame), 1) == 1;
    s.noiseAmplitude =
        bounded(m_noiseAmplitude->getValue(frame), 0, 1000) * scale;
    s.noiseSize  = bounded(m_noiseSize->getValue(frame), 1, 100) * scale;
    s.noiseSpeed = bounded(m_noiseSpeed->getValue(frame), -100, 100);
    s.seed       = std::uint32_t(discrete(m_seed->getValue(frame), 100000));
    s.useMatte   = m_matte.isConnected();
    return s;
  }

public:
  FlowDisplaceFx()
      : m_mode(0.0)
      , m_strength(10.0)
      , m_direction(0.0)
      , m_invert(0.0)
      , m_noiseAmplitude(0.0)
      , m_noiseSize(20.0)
      , m_noiseSpeed(0.1)
      , m_seed(1.0) {
    addInputPort("Source", m_source);
    addInputPort("Displacement", m_field);
    addInputPort("Matte", m_matte);
    m_mode->setValueRange(0, 1);
    m_mode->setUIChoices({"Vector Field", "Grey Map"});
    m_strength->setValueRange(-1000, 1000);
    m_strength->setUISliderRange(-50, 50);
    m_direction->setValueRange(-3600, 3600);
    m_direction->setMeasureName("angle");
    m_invert->setValueRange(0, 1);
    m_invert->setUIChoices({"Off", "On"}, true);
    m_noiseAmplitude->setValueRange(0, 1000);
    m_noiseAmplitude->setUISliderRange(0, 30);
    m_noiseSize->setValueRange(1, 100);
    m_noiseSpeed->setValueRange(-100, 100);
    m_noiseSpeed->setUISliderRange(0, 1);
    m_seed->setValueRange(0, 100000);
    m_seed->setUISliderRange(0, 1000);
    bindParam(this, "mode", m_mode);
    bindParam(this, "strength", m_strength);
    bindParam(this, "direction", m_direction);
    bindParam(this, "invert", m_invert);
    bindParam(this, "noiseAmplitude", m_noiseAmplitude);
    bindParam(this, "noiseSize", m_noiseSize);
    bindParam(this, "noiseSpeed", m_noiseSpeed);
    bindParam(this, "seed", m_seed);
    enableComputeInFloat(true);
  }
  bool canHandle(const TRenderSettings &info, double) override {
    return CelPixels::isotropic(info.m_affine);
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override {
    box = TRectD();
    if (!m_source.isConnected() || !m_source->doGetBBox(frame, box, settings))
      return false;
    if (box != TConsts::infiniteRectD)
      box = box.enlarge(FlowDisplace::halo(this->settings(frame, settings)));
    return !box.isEmpty();
  }
  void transform(double frame, int, const TRectD &output,
                 const TRenderSettings &settings, TRectD &input,
                 TRenderSettings &inputSettings) override {
    inputSettings = settings;
    input = output.enlarge(FlowDisplace::halo(this->settings(frame, settings)));
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &settings) override {
    const int halo = FlowDisplace::halo(this->settings(frame, settings));
    for (TRasterFxPort *port : {&m_source, &m_field, &m_matte})
      if (port->isConnected()) {
        TRectD input = rect.enlarge(halo);
        (*port)->dryCompute(input, frame, settings);
      }
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &settings) override {
    // Three inputs and the result
    return megabytes(rect, FlowDisplace::halo(this->settings(frame, settings)),
                     4 * 32);
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    std::ostringstream alias;
    alias.imbue(std::locale::classic());
    const FlowDisplace::Settings s = this->settings(frame, settings);
    alias << TRasterFx::getAlias(frame, settings) << "[flowDisplace:1:"
          << std::setprecision(std::numeric_limits<double>::max_digits10)
          << int(s.mode) << ',' << s.strength << ',' << s.direction << ','
          << s.invert << ',' << s.noiseAmplitude << ',' << s.noiseSize << ','
          << s.noiseSpeed << ',' << s.seed << ',' << s.useMatte;
    // The noise drifts with time: each frame is its own image
    if (s.noiseAmplitude > 0.0) alias << ",frame=" << frame;
    alias << ']';
    return alias.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() ||
        (settings.m_isCanceled && *settings.m_isCanceled))
      return;
    const FlowDisplace::Settings s = this->settings(frame, settings);
    const int halo                 = FlowDisplace::halo(s);
    using FlowDisplace::Color;
    // Every input covers the tile and its halo
    const TPointD origin = tile.m_pos - TPointD(halo, halo);
    auto inputOf         = [&](TRasterFxPort &port) {
      TTile input;
      input.setRaster(CelPixels::padded(tile.getRaster(), halo));
      input.m_pos = origin;
      if (port.isConnected()) port->compute(input, frame, settings);
      return input.getRaster();
    };
    const TRasterP source = inputOf(m_source);
    const int w = source->getLx(), h = source->getLy();
    std::vector<Color> field, matte;
    if (m_field.isConnected()) field = CelPixels::read<Color>(inputOf(m_field));
    if (m_matte.isConnected()) matte = CelPixels::read<Color>(inputOf(m_matte));
    if (settings.m_isCanceled && *settings.m_isCanceled) return;
    // The noise size scales with the render, so the pattern keeps its place
    // in the scene at any preview size
    std::vector<Color> out;
    if (!FlowDisplace::render(CelPixels::read<Color>(source), field, matte, w,
                              h, origin.x, origin.y, s, frame, out,
                              settings.m_isCanceled))
      return;
    CelPixels::write(out, w, halo, halo, tile.getRaster());
  }
};
FX_PLUGIN_IDENTIFIER(FlowDisplaceFx, "flowDisplaceFx")
