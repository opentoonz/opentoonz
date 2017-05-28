#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "ttzpimagefx.h"
#include "texception.h"
#include "tfxparam.h"
#include "stdfx.h"
#include "trasterfx.h"
#include "tspectrumparam.h"

class ArtContourFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ArtContourFx)

  TRasterFxPort m_input;
  TRasterFxPort m_controller;
  TStringParamP m_colorIndex;
  TBoolParamP m_keepColor;
  TBoolParamP m_keepLine;
  TBoolParamP m_includeAlpha;
  TDoubleParamP m_density;
  TRangeParamP m_distance;
  TBoolParamP m_randomness;
  TRangeParamP m_orientation;
  TRangeParamP m_size;

public:
  ArtContourFx()
      : m_colorIndex(L"1,2,3")
      , m_keepColor(false)
      , m_keepLine(false)
      , m_includeAlpha(true)
      , m_density(0.0)
      , m_distance(DoublePair(30.0, 30.0))
      , m_randomness(true)
      , m_orientation(DoublePair(0.0, 180.0))
      , m_size(DoublePair(30.0, 30.0)) {
    bindParam(this, "Color_Index", m_colorIndex);
    bindParam(this, "Keep_color", m_keepColor);
    bindParam(this, "Keep_Line", m_keepLine);
    bindParam(this, "Include_Alpha", m_includeAlpha);
    bindParam(this, "Density", m_density);
    bindParam(this, "Distance", m_distance);
    bindParam(this, "Randomness", m_randomness);
    bindParam(this, "Orientation", m_orientation);
    bindParam(this, "Size", m_size);
    addInputPort("Source", m_input);
    addInputPort("Controller", m_controller);
    m_density->setValueRange(0.0, 100.0);
    m_distance->getMin()->setValueRange(0.0, 1000.0);
    m_distance->getMax()->setValueRange(0.0, 1000.0);
    m_orientation->getMin()->setValueRange(-180.0, 180.0);
    m_orientation->getMax()->setValueRange(-180.0, 180.0);
    m_orientation->getMin()->setMeasureName("angle");
    m_orientation->getMax()->setMeasureName("angle");
    m_size->getMin()->setValueRange(0.0, 1000.0);
    m_size->getMax()->setValueRange(0.0, 1000.0);
  }

  ~ArtContourFx() {}

  //----------------------------------------------------------------------------

  SandorFxRenderData *buildRenderData(double frame, int shrink,
                                      const TRectD &controlBox,
                                      const std::string &controllerAlias) {
    int argc = 12;
    const char *argv[12];
    argv[0] = strsave(::to_string(m_colorIndex->getValue()).c_str());
    getValues(argv, argc, frame);

    SandorFxRenderData *artContourData =
        new SandorFxRenderData(ArtAtContour, argc, argv, 0, shrink, controlBox);
    ArtAtContourParams &params        = artContourData->m_contourParams;
    params.m_density                  = m_density->getValue(frame) / 100;
    params.m_colorIndex               = m_colorIndex->getValue();
    params.m_keepLine                 = m_keepLine->getValue();
    params.m_includeAlpha             = m_includeAlpha->getValue();
    params.m_maxOrientation           = m_orientation->getValue(frame).second;
    params.m_maxDistance              = m_distance->getValue(frame).second / 10;
    params.m_maxSize                  = m_size->getValue(frame).second / 100;
    params.m_minOrientation           = m_orientation->getValue(frame).first;
    params.m_minDistance              = m_distance->getValue(frame).first / 10;
    params.m_minSize                  = m_size->getValue(frame).first / 100;
    params.m_randomness               = m_randomness->getValue();
    params.m_keepColor                = m_keepColor->getValue();
    artContourData->m_controllerAlias = controllerAlias;

    return artContourData;
  }

  //----------------------------------------------------------------------------

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override {
    if (m_input.isConnected() && m_controller.isConnected()) {
      TRectD controlBox, inputBox;

      TRenderSettings ri2(ri);
      ri2.m_affine = TAffine();

      m_controller->getBBox(frame, controlBox, ri2);

      TRenderSettings ri3(ri);

      int shrink = tround((ri.m_shrinkX + ri.m_shrinkY) / 2.0);
      // Should be there no need for the alias...
      SandorFxRenderData *artContourData =
          buildRenderData(frame, shrink, controlBox, "");
      ri3.m_data.push_back(artContourData);

      return m_input->doGetBBox(frame, bBox, ri3);
    } else if (m_input.isConnected()) {
      m_input->doGetBBox(frame, bBox, ri);
      return false;
    }
    bBox = TRectD();
    return false;
  }

  //-----------------------------------------------------------------------------

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  //-----------------------------------------------------------------------------

  bool allowUserCacheOnPort(int port) override { return port != 0; }

  //-----------------------------------------------------------------------------

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &ri) override;

  //-----------------------------------------------------------------------------

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  //-----------------------------------------------------------------------------

private:
  void getValues(const char *argv[], int argc, double frame) {
    double values[12];
    values[1]  = m_size->getValue(frame).second / 100;
    values[2]  = m_size->getValue(frame).first / 100;
    values[3]  = m_orientation->getValue(frame).second;
    values[4]  = m_orientation->getValue(frame).first;
    values[5]  = m_randomness->getValue() ? 1.0 : 0.0;
    values[6]  = m_distance->getValue(frame).second / 10;
    values[7]  = m_distance->getValue(frame).first / 10;
    values[8]  = m_density->getValue(frame) / 100;
    values[9]  = m_keepLine->getValue() ? 1.0 : 0.0;
    values[10] = m_keepColor->getValue() ? 1.0 : 0.0;
    values[11] = m_includeAlpha->getValue() ? 1.0 : 0.0;
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
    for (int i = 1; i <= 11; i++) {
      app       = std::to_string(param[i]);
      cParam[i] = strsave(app.c_str());
    }
  }
};

FX_PLUGIN_IDENTIFIER(ArtContourFx, "artContourFx");

//-----------------------------------------------------------------------------

void ArtContourFx::doDryCompute(TRectD &rect, double frame,
                                const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;
  if (!m_controller.isConnected()) return;

  TRenderSettings ri2(ri);
  ri2.m_affine = TAffine();

  TRectD controlBox;
  m_controller->getBBox(frame, controlBox, ri2);

  TDimension dim = convert(controlBox).getSize();
  TRectD controlRect(controlBox.getP00(), TDimensionD(dim.lx, dim.ly));

  m_controller->dryCompute(controlRect, frame, ri2);

  TRenderSettings ri3(ri);

  int shrink               = tround((ri.m_shrinkX + ri.m_shrinkY) / 2.0);
  std::string controlAlias = m_controller->getAlias(frame, ri2);
  SandorFxRenderData *artContourData =
      buildRenderData(frame, shrink, controlBox, controlAlias);
  ri3.m_data.push_back(artContourData);
  ri3.m_userCachable = false;

  m_input->dryCompute(rect, frame, ri3);
}

//-----------------------------------------------------------------------------

void ArtContourFx::doCompute(TTile &tile, double frame,
                             const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  if (!m_controller.isConnected()) {
    m_input->compute(tile, frame, ri);
    return;
  }

  TRenderSettings ri2(ri);
  ri2.m_affine = TAffine();

  TRectD controlBox;
  m_controller->getBBox(frame, controlBox, ri2);
  TTile ctrTile;
  ctrTile.m_pos  = controlBox.getP00();
  TDimension dim = convert(controlBox).getSize();

  m_controller->allocateAndCompute(ctrTile, ctrTile.m_pos, dim,
                                   tile.getRaster(), frame, ri2);

  TRenderSettings ri3(ri);

  // Build the render data
  int shrink               = tround((ri.m_shrinkX + ri.m_shrinkY) / 2.0);
  std::string controlAlias = m_controller->getAlias(frame, ri2);
  SandorFxRenderData *artContourData =
      buildRenderData(frame, shrink, controlBox, controlAlias);

  // Add the controller raster
  artContourData->m_controller = ctrTile.getRaster();

  // Push the data among the others
  ri3.m_data.push_back(artContourData);
  ri3.m_userCachable = false;

  m_input->compute(tile, frame, ri3);
}
