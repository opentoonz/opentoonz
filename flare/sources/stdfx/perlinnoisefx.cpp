

#include "trop.h"
#include "tfxparam.h"
#include "perlinnoise.h"
#include "stdfx.h"
#include "trasterfx.h"
#include "tparamuiconcept.h"

//==================================================================

class PerlinNoiseFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(PerlinNoiseFx)
  TRasterFxPort m_input;
  TIntEnumParamP m_type;
  TDoubleParamP m_size;
  TDoubleParamP m_min;
  TDoubleParamP m_max;
  TDoubleParamP m_evol;
  TDoubleParamP m_offsetx;
  TDoubleParamP m_offsety;

  TDoubleParamP m_intensity;
  TBoolParamP m_matte;

public:
  PerlinNoiseFx()
      : m_type(new TIntEnumParam(PNOISE_CLOUDS, "Clouds"))
      , m_size(100.0)
      , m_min(0.0)
      , m_max(1.0)
      , m_evol(0.0)
      , m_intensity(40.0)
      , m_offsetx(0.0)
      , m_offsety(0.0)
      , m_matte(1.0) {
    m_offsetx->setMeasureName("fxLength");
    m_offsety->setMeasureName("fxLength");
    bindParam(this, "type", m_type);
    m_type->addItem(PNOISE_WOODS, "Marble/Wood");
    bindParam(this, "size", m_size);
    bindParam(this, "evolution", m_evol);
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "offsetx", m_offsetx);
    bindParam(this, "offsety", m_offsety);
    bindParam(this, "matte", m_matte);
    addInputPort("Source", m_input);
    m_size->setValueRange(0, 200);
    m_intensity->setValueRange(0, 300);
    m_min->setValueRange(0, 1.0);
    m_max->setValueRange(0, 1.0);

    enableComputeInFloat(true);
  }
  ~PerlinNoiseFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      m_input->doGetBBox(frame, bBox, info);
      bBox = bBox.enlarge(m_intensity->getValue(frame));
      return true;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::POINT_2;
    concepts[0].m_label = "Offset";
    concepts[0].m_params.push_back(m_offsetx);
    concepts[0].m_params.push_back(m_offsety);
  }
};

//==============================================================================
template <typename PIXEL, typename CHANNEL_TYPE>
void doPerlinNoise(const TRasterPT<PIXEL> &rasOut,
                   const TRasterPT<PIXEL> &rasIn, TPointD &tilepos,
                   double evolution, double size, double min, double max,
                   double offsetx, double offsety, int type, int brad,
                   int matte, double scale) {
  PerlinNoise Noise;
  rasOut->lock();

  TAffine aff = TScale(1 / scale);

  if (type == PNOISE_CLOUDS)

    for (int j = 0; j < rasOut->getLy(); ++j) {
      PIXEL *pixout    = rasOut->pixels(j);
      PIXEL *endPixOut = pixout + rasOut->getLx();
      PIXEL *pix       = rasIn->pixels(j + brad) + brad;
      TPointD pos      = tilepos;
      pos.y += j;
      while (pixout < endPixOut) {
        TPointD posAff = aff * pos;
        double pnoise = Noise.Turbolence(posAff.x + offsetx, posAff.y + offsety,
                                         evolution, size, min, max);
        int sval      = (int)(brad * (pnoise - 0.5));
        int pixshift  = sval + rasIn->getWrap() * (sval);
        pos.x += 1.0;

        if (matte) {
          pixout->r = (CHANNEL_TYPE)((pix + pixshift)->r * pnoise);
          pixout->g = (CHANNEL_TYPE)((pix + pixshift)->g * pnoise);
          pixout->b = (CHANNEL_TYPE)((pix + pixshift)->b * pnoise);
          pixout->m = (CHANNEL_TYPE)((pix + pixshift)->m * pnoise);
        } else {
          pixout->r = (CHANNEL_TYPE)((pix + pixshift)->r);
          pixout->g = (CHANNEL_TYPE)((pix + pixshift)->g);
          pixout->b = (CHANNEL_TYPE)((pix + pixshift)->b);
          pixout->m = (CHANNEL_TYPE)((pix + pixshift)->m);
        }
        pix++;
        pixout++;
      }
    }
  else
    for (int j = 0; j < rasOut->getLy(); ++j) {
      PIXEL *pixout    = rasOut->pixels(j);
      PIXEL *endPixOut = pixout + rasOut->getLx();
      PIXEL *pix       = rasIn->pixels(j + brad) + brad;
      TPointD pos      = tilepos;
      pos.y += j;
      while (pixout < endPixOut) {
        TPointD posAff = aff * pos;
        double pnoisex = Noise.Marble(posAff.x + offsetx, posAff.y + offsety,
                                      evolution, size, min, max);
        double pnoisey = Noise.Marble(posAff.x + offsetx, posAff.y + offsety,
                                      evolution + 100, size, min, max);
        int svalx      = (int)(brad * (pnoisex - 0.5));
        int svaly      = (int)(brad * (pnoisey - 0.5));
        int pixshift   = svalx + rasIn->getWrap() * (svaly);
        pos.x += 1.0;

        if (matte) {
          pixout->r = (CHANNEL_TYPE)((pix + pixshift)->r * pnoisex);
          pixout->g = (CHANNEL_TYPE)((pix + pixshift)->g * pnoisex);
          pixout->b = (CHANNEL_TYPE)((pix + pixshift)->b * pnoisex);
          pixout->m = (CHANNEL_TYPE)((pix + pixshift)->m * pnoisex);
        } else {
          pixout->r = (CHANNEL_TYPE)((pix + pixshift)->r);
          pixout->g = (CHANNEL_TYPE)((pix + pixshift)->g);
          pixout->b = (CHANNEL_TYPE)((pix + pixshift)->b);
          pixout->m = (CHANNEL_TYPE)((pix + pixshift)->m);
        }
        pix++;
        pixout++;
      }
    }
  rasOut->unlock();
}

//==============================================================================

void PerlinNoiseFx::transform(double frame, int port,
                              const TRectD &rectOnOutput,
                              const TRenderSettings &infoOnOutput,
                              TRectD &rectOnInput,
                              TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;
  TRectD rectOut(rectOnOutput);

  double scale = sqrt(fabs(infoOnInput.m_affine.det()));
  int brad     = (int)m_intensity->getValue(frame) * scale;

  if (!brad) {
    rectOnInput = rectOut;
    return;
  }

  int rasInLx = tround(rectOut.getLx());
  int rasInLy = tround(rectOut.getLy());
  rectOnInput = TRectD(rectOut.getP00(), TDimensionD(rasInLx, rasInLy));
  rectOnInput.enlarge(brad);
}

//==============================================================================

void PerlinNoiseFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double scale = sqrt(fabs(ri.m_affine.det()));

  double evolution = m_evol->getValue(frame);
  double size      = m_size->getValue(frame);
  int brad         = (int)m_intensity->getValue(frame) * scale;
  int matte        = (int)m_matte->getValue();
  int type         = (int)m_type->getValue();
  double offsetx   = m_offsetx->getValue(frame);
  double offsety   = m_offsety->getValue(frame);
  double min       = m_min->getValue(frame);
  double max       = m_max->getValue(frame);

  if (brad < 0) brad = abs(brad);
  if (size < 0.01) size = 0.01;

  if (!brad) {
    m_input->compute(tile, frame, ri);
    return;
  }
  TRectD rectIn = convert(tile.getRaster()->getBounds()) + tile.m_pos;
  rectIn        = rectIn.enlarge(brad);

  int rasInLx = tround(rectIn.getLx());
  int rasInLy = tround(rectIn.getLy());

  TTile tileIn;
  m_input->allocateAndCompute(tileIn, rectIn.getP00(),
                              TDimension(rasInLx, rasInLy), tile.getRaster(),
                              frame, ri);

  TPointD pos = tile.m_pos;

  TRaster32P rasOut32 = tile.getRaster();
  TRaster64P rasOut64 = tile.getRaster();
  TRasterFP rasOutF   = tile.getRaster();

  if (rasOut32) {
    TRaster32P rasIn32 = tileIn.getRaster();
    doPerlinNoise<TPixel32, UCHAR>(rasOut32, rasIn32, pos, evolution, size, min,
                                   max, offsetx, offsety, type, brad, matte,
                                   scale);
  } else if (rasOut64) {
    TRaster64P rasIn64 = tileIn.getRaster();
    doPerlinNoise<TPixel64, USHORT>(rasOut64, rasIn64, pos, evolution, size,
                                    min, max, offsetx, offsety, type, brad,
                                    matte, scale);
  } else if (rasOutF) {
    TRasterFP rasInF = tileIn.getRaster();
    doPerlinNoise<TPixelF, float>(rasOutF, rasInF, pos, evolution, size, min,
                                  max, offsetx, offsety, type, brad, matte,
                                  scale);
  } else
    throw TException("PerlinNoise: unsupported Pixel Type");
}

//------------------------------------------------------------------

int PerlinNoiseFx::getMemoryRequirement(const TRectD &rect, double frame,
                                        const TRenderSettings &info) {
  double scale = sqrt(fabs(info.m_affine.det()));
  int brad     = (int)m_intensity->getValue(frame) * scale;

  return TRasterFx::memorySize(rect.enlarge(brad), info.m_bpp);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(PerlinNoiseFx, "perlinNoiseFx")
