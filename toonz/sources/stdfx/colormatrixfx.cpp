// Color Matrix FX: a 4x5 matrix on unpremultiplied colors.
//
//   red'   = clamp(rr*r + rg*g + rb*b + ra*a + ro/255), ...
//   alpha' = clamp(ar*r + ag*g + ab*b + aa*a + ao/255)
//
// with channels in [0, 1] and offsets in 0..255 units, as the color matrix
// filter and the color transforms of Flash / Adobe Animate compute them
// (multipliers and offsets of a color effect are the diagonal and the last
// column). Fully transparent pixels stay transparent, as Flash leaves them.
// The identity matrix leaves the image unchanged.

#include "stdfx.h"
#include "texception.h"
#include "tfxparam.h"

#include <algorithm>
#include <cmath>

class ColorMatrixFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ColorMatrixFx)

  TRasterFxPort m_input;
  TDoubleParamP m_matrix[20];

public:
  ColorMatrixFx() {
    static const char *rows[]    = {"red", "green", "blue", "alpha"};
    static const char *columns[] = {"red", "green", "blue", "alpha", "offset"};
    for (int row = 0; row < 4; ++row)
      for (int column = 0; column < 5; ++column) {
        TDoubleParamP &param = m_matrix[row * 5 + column];
        param                = TDoubleParamP(column == row ? 1.0 : 0.0);
        if (column == 4)
          param->setValueRange(-1024, 1024);
        else
          param->setValueRange(-16, 16);
        bindParam(this, std::string(rows[row]) + "_" + columns[column], param);
      }
    addInputPort("Source", m_input);
    enableComputeInFloat(true);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) return m_input->doGetBBox(frame, bBox, info);
    bBox = TRectD();
    return false;
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
};

namespace {

template <class PIXEL>
void applyMatrix(const TRasterPT<PIXEL> &ras, const double *m) {
  const double max = PIXEL::maxChannelValue;
  ras->lock();
  for (int y = 0; y < ras->getLy(); ++y) {
    PIXEL *pix = ras->pixels(y), *end = pix + ras->getLx();
    for (; pix < end; ++pix) {
      if (pix->m <= 0) continue;
      const double a    = pix->m / max;
      const double in[] = {pix->r / max / a, pix->g / max / a, pix->b / max / a,
                           a};
      double out[4];
      for (int row = 0; row < 4; ++row) {
        const double *coefficients = m + row * 5;
        double value               = coefficients[4] / 255;
        for (int column = 0; column < 4; ++column)
          value += coefficients[column] * in[column];
        out[row] = std::max(0.0, std::min(1.0, value));
      }
      auto channel = [max](double v) {
        return (typename PIXEL::Channel)std::lround(v * max);
      };
      pix->r = channel(out[0] * out[3]);
      pix->g = channel(out[1] * out[3]);
      pix->b = channel(out[2] * out[3]);
      pix->m = channel(out[3]);
    }
  }
  ras->unlock();
}

template <>
void applyMatrix<TPixelF>(const TRasterFP &ras, const double *m) {
  ras->lock();
  for (int y = 0; y < ras->getLy(); ++y) {
    TPixelF *pix = ras->pixels(y), *end = pix + ras->getLx();
    for (; pix < end; ++pix) {
      if (pix->m <= 0.f) continue;
      const double a    = pix->m;
      const double in[] = {pix->r / a, pix->g / a, pix->b / a, a};
      double out[4];
      for (int row = 0; row < 4; ++row) {
        const double *coefficients = m + row * 5;
        double value               = coefficients[4] / 255;
        for (int column = 0; column < 4; ++column)
          value += coefficients[column] * in[column];
        // Colors may exceed 1 in float renders; alpha may not.
        out[row] = row == 3 ? std::max(0.0, std::min(1.0, value))
                            : std::max(0.0, value);
      }
      pix->r = float(out[0] * out[3]);
      pix->g = float(out[1] * out[3]);
      pix->b = float(out[2] * out[3]);
      pix->m = float(out[3]);
    }
  }
  ras->unlock();
}

}  // namespace

void ColorMatrixFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;
  m_input->compute(tile, frame, ri);
  double m[20];
  bool identity = true;
  for (int index = 0; index < 20; ++index) {
    m[index] = m_matrix[index]->getValue(frame);
    identity &= m[index] == (index % 6 == 0 ? 1.0 : 0.0);
  }
  if (identity) return;
  if (TRaster32P ras = tile.getRaster())
    applyMatrix(ras, m);
  else if (TRaster64P ras = tile.getRaster())
    applyMatrix(ras, m);
  else if (TRasterFP ras = tile.getRaster())
    applyMatrix(ras, m);
  else
    throw TException("Color Matrix: unsupported pixel type");
}

FX_PLUGIN_IDENTIFIER(ColorMatrixFx, "colorMatrixFx")
