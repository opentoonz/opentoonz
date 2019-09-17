

#include "stdfx.h"
#include "tfxparam.h"
#include "tspectrumparam.h"

class Test_BoxFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Test_BoxFx)

  TRasterFxPort m_input;
  TDoubleParamP m_h;
  TDoubleParamP m_v;
  TPixelParamP m_color;
  TPointParamP m_point;
  TSpectrumParamP m_colors;
  TIntParamP m_int;
  TIntEnumParamP m_enum;
  TRangeParamP m_range;
  TBoolParamP m_bool;
  TStringParamP m_string;

public:
  Test_BoxFx()
      : m_h(30.0)
      , m_v(30.0)
      , m_color(TPixel32::Blue)
      , m_range(DoublePair(100., 100.))
      , m_int(12)
      , m_enum(new TIntEnumParam(0, "Color"))
      , m_point(TPointD(0.0, 0.0))
      , m_bool(true)
      , m_string(L"urka") {
    m_h->setValueRange(0, 100);
    m_enum->addItem(1, "Uno");
    m_enum->addItem(2, "Due");
    m_enum->addItem(3, "Tre");
    bindParam(this, "h", m_h);
    bindParam(this, "v", m_v);
    bindParam(this, "color", m_color);
    bindParam(this, "Point", m_point);
    bindParam(this, "enum", m_enum);
    m_enum->addItem(1, "Spectrum");
    bindParam(this, "int", m_int);
    bindParam(this, "range", m_range);
    bindParam(this, "bool", m_bool);
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(0.5, TPixel32::Yellow),
        TSpectrum::ColorKey(1, TPixel32::Red)};
    m_colors = TSpectrumParamP(colors);
    bindParam(this, "spectrum", m_colors);
    bindParam(this, "string", m_string);
    addInputPort("Source", m_input);
  }

  ~Test_BoxFx(){};

  bool canHandle(const TRenderSettings &info, double frame) { return false; }

  bool doGetBBox(double frame, TRectD &bBox, const TRenderSettings &info) {
    if (!m_input.isConnected()) {
      bBox = TRectD();
      return false;
    }
    return m_input->doGetBBox(frame, bBox, info);
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &);
};

//===================================================================

template <class PIX, class RAS>
void doTest_Box(RAS ras, double h, double v, TPointD pos, TPixel32 &color32,
                int my_enum, int my_int, const TSpectrumT<PIX> spectrum,
                TPointD point, DoublePair range, bool my_bool) {
  int lx = ras->getLx();
  int ly = ras->getLy();

  TPointD tmp = pos;

  const PIX color = PixelConverter<RAS::Pixel>::from(color32);
  int j, i;
  for (j = 0; j < ly; j++) {
    PIX *pix = ras->pixels(j);
    tmp.y++;
    tmp.x = pos.x;
    for (i = 0; i < lx; i++) {
      if (tmp.x > 0 && tmp.x < h && tmp.y > 0 && tmp.y < v) {
        if (my_enum && my_bool) {
          if (tmp.x > range.first && tmp.x < range.second)
            *pix = spectrum.getPremultipliedValue(
                sqrt((tmp.x - point.x) * (tmp.x - point.x) / (h * h) +
                     (tmp.y - point.y) * (tmp.y - point.y) / (v * v)));
          else
            *pix = color;
        } else
          *pix = color;
      }
      tmp.x++;
      pix++;
    }
  }
}

void Test_BoxFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;
  wstring text = m_string->getValue();

  m_input->compute(tile, frame, ri);
  TPixel32 color = m_color->getValue(frame);
  double h       = m_h->getValue(frame);
  double v       = m_v->getValue(frame);
  int my_enum    = m_enum->getValue();
  // TSpectrum spectrum = m_colors->getValue(frame);
  int my_int       = m_int->getValue();
  TPointD point    = m_point->getValue(frame);
  DoublePair range = m_range->getValue(frame);
  bool my_bool     = m_bool->getValue();
  if (TRaster32P raster32 = tile.getRaster())
    doTest_Box<TPixel32, TRaster32P>(raster32, h, v, tile.m_pos, color, my_enum,
                                     my_int, m_colors->getValue(frame), point,
                                     range, my_bool);
  else if (TRaster64P raster64 = tile.getRaster())
    doTest_Box<TPixel64, TRaster64P>(raster64, h, v, tile.m_pos, color, my_enum,
                                     my_int, m_colors->getValue64(frame), point,
                                     range, my_bool);
  else
    throw TException("Test_BoxFx: unsupported Pixel Type");
}

#ifdef _DEBUG
FX_PLUGIN_IDENTIFIER(Test_BoxFx, "test_BoxFx")
#endif
