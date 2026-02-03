#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "ttzpimagefx.h"
#include "texception.h"
#include "tfxparam.h"
#include "stdfx.h"
#include "trasterfx.h"

//-----------------------------------------------------------------------------------------
class BlendTzFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(BlendTzFx)

  TRasterFxPort m_input;
  TStringParamP m_colorIndex;
  TBoolParamP m_noBlending;
  TDoubleParamP m_amount;
  TDoubleParamP m_smoothness;

public:
  BlendTzFx()
      : m_colorIndex(L"1,2,3")
      , m_noBlending(false)
      , m_amount(10)
      , m_smoothness(10) {
    m_amount->setMeasureName("fxLength");
    bindParam(this, "Color_Index", m_colorIndex);
    bindParam(this, "Amount", m_amount);
    bindParam(this, "Smoothness", m_smoothness);
    bindParam(this, "noBlending", m_noBlending);
    addInputPort("Source", m_input);
    m_amount->setValueRange(0, std::numeric_limits<double>::max());
    m_smoothness->setValueRange(0, std::numeric_limits<double>::max());
  }

  ~BlendTzFx() {}

  //----------------------------------------------------------------------------

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      // Build the render data
      TRenderSettings info2(info);
      buildBlendData(info2, frame);

      return m_input->doGetBBox(frame, bBox, info2);
    } else {
      bBox = TRectD();
      return false;
    }
  }

  //-----------------------------------------------------------------------------

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  //-----------------------------------------------------------------------------

  bool allowUserCacheOnPort(int port) override { return false; }

  //-----------------------------------------------------------------------------

private:
  void getValues(const char *argv[], int argc, double frame) {
    double values[6];
    values[2] = m_smoothness->getValue(frame);
    values[3] = m_amount->getValue(frame) * 0.5;
    values[4] = m_noBlending->getValue() ? 1.0 : 0.0;
    convertParam(values, argv, argc);
  }
  //----------------------------------------------------------------------

  char *strsave(const char *t) {
    char *s;
    s = (char *)malloc(strlen(t) + 1);
    strcpy(s, t);
    return s;
  }

  //-----------------------------------------------------------------------

  void convertParam(double param[], const char *cParam[], int cParamLen) {
    std::string app;
    for (int i = 2; i < cParamLen - 1; i++) {
      app       = std::to_string(param[i]);
      cParam[i] = strsave(app.c_str());
    }
  }

  //-------------------------------------------------------------------------------

  int getBorder(int argc, int shrink, double frame) {
    double blendSize;
    int defaultBorder = 5;
    if (argc != 6) return defaultBorder;
    blendSize                 = m_amount->getValue(frame) * 0.5;
    if (shrink > 0) blendSize = blendSize / (double)shrink;
    // if (blendcmapImgDpi>0.0 )
    //	blendSize*=(blendcmapImgDpi*0.01);
    return (int)blendSize + 1 + defaultBorder;
  }

  //-------------------------------------------------------------------------------

  void buildBlendData(TRenderSettings &ri, double frame) {
    int shrink = tround((ri.m_shrinkX + ri.m_shrinkY) / 2.0);
    int argc   = 6;
    const char *argv[6];
    argv[0] = strsave(::to_string(m_colorIndex->getValue()).c_str());
    argv[1] = argv[0];
    argv[5] = "1";
    getValues(argv, argc, frame);
    SandorFxRenderData *blendData =
        new SandorFxRenderData(BlendTz, argc, argv, 0, shrink);
    BlendTzParams &params = blendData->m_blendParams;
    params.m_amount       = m_amount->getValue(frame) * 0.5;
    params.m_colorIndex   = m_colorIndex->getValue();
    params.m_smoothness   = m_smoothness->getValue(frame);
    params.m_noBlending   = m_noBlending->getValue();
    ri.m_data.push_back(blendData);
  }
};

FX_PLUGIN_IDENTIFIER(BlendTzFx, "blendTzFx")

//------------------------------------------------------------------------------

void BlendTzFx::transform(double frame, int port, const TRectD &rectOnOutput,
                          const TRenderSettings &infoOnOutput,
                          TRectD &rectOnInput, TRenderSettings &infoOnInput) {
  rectOnInput                = rectOnOutput;
  infoOnInput                = infoOnOutput;
  infoOnInput.m_userCachable = false;
  buildBlendData(infoOnInput, frame);
}

//------------------------------------------------------------------------------

void BlendTzFx::doCompute(TTile &tile, double frame,
                          const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  TRenderSettings ri2(ri);
  buildBlendData(ri2, frame);
  ri2.m_userCachable = false;

  m_input->compute(tile, frame, ri2);
}
