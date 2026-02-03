

#include "stdfx.h"
#include "tfxparam.h"
#include "warp.h"
#include "trop.h"
#include "trasterfx.h"
#include "tspectrumparam.h"
#include "gradients.h"
#include "timage_io.h"
#include "tparamuiconcept.h"

//-------------------------------------------------------------------

class RippleFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RippleFx)
protected:
  TRasterFxPort m_warped;
  TDoubleParamP m_intensity;
  TDoubleParamP m_gridStep;
  TPointParamP m_center;
  TDoubleParamP m_period;
  TDoubleParamP m_count;
  TDoubleParamP m_cycle;
  TDoubleParamP m_scaleX;
  TDoubleParamP m_scaleY;
  TDoubleParamP m_angle;
  TBoolParamP m_sharpen;

public:
  RippleFx()
      : m_intensity(20)
      , m_gridStep(2)
      , m_center(TPointD(0.0, 0.0))
      , m_period(100)  // args, "Period")
      , m_count(2)     // args, "Count")
      , m_cycle(0.0)
      , m_scaleX(100.0)
      , m_scaleY(100.0)
      , m_angle(0.0)
      , m_sharpen(false) {
    m_center->getX()->setMeasureName("fxLength");
    m_center->getY()->setMeasureName("fxLength");
    m_period->setMeasureName("fxLength");

    addInputPort("Source", m_warped);
    bindParam(this, "period", m_period);
    bindParam(this, "count", m_count);
    bindParam(this, "cycle", m_cycle);
    bindParam(this, "center", m_center);
    bindParam(this, "scalex", m_scaleX);
    bindParam(this, "scaley", m_scaleY);
    bindParam(this, "angle", m_angle);
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "sensitivity", m_gridStep);
    bindParam(this, "sharpen", m_sharpen);
    m_intensity->setValueRange(-1000, 1000);
    m_gridStep->setValueRange(2, 20);
    m_period->setValueRange(0, (std::numeric_limits<double>::max)());
    m_cycle->setValueRange(0, (std::numeric_limits<double>::max)());
    m_count->setValueRange(0, (std::numeric_limits<double>::max)());
    m_angle->setMeasureName("angle");

    enableComputeInFloat(true);
  }
  virtual ~RippleFx() {}

  //-------------------------------------------------------------------

  bool canHandle(const TRenderSettings &info, double frame) override {
    return isAlmostIsotropic(info.m_affine);
  }

  //-------------------------------------------------------------------

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_warped.isConnected()) {
      int ret = m_warped->doGetBBox(frame, bBox, info);

      if (ret && !bBox.isEmpty()) {
        if (bBox != TConsts::infiniteRectD) {
          WarpParams params;
          params.m_intensity = m_intensity->getValue(frame);

          bBox = bBox.enlarge(getWarpRadius(params));
        }
        return true;
      }
    }

    bBox = TRectD();
    return false;
  }

  //-------------------------------------------------------------------

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    bool isWarped = m_warped.isConnected();
    if (!isWarped) return;
    if (fabs(m_intensity->getValue(frame)) < 0.01) {
      m_warped->dryCompute(rect, frame, info);
      return;
    }

    double scale    = sqrt(fabs(info.m_affine.det()));
    double gridStep = 1.5 * m_gridStep->getValue(frame);

    WarpParams params;
    params.m_intensity   = m_intensity->getValue(frame) / gridStep;
    params.m_warperScale = scale * gridStep;
    params.m_sharpen     = m_sharpen->getValue();

    TRectD warpedBox, warpedComputeRect, tileComputeRect;
    m_warped->getBBox(frame, warpedBox, info);

    getWarpComputeRects(tileComputeRect, warpedComputeRect, warpedBox, rect,
                        params);

    if (tileComputeRect.getLx() <= 0 || tileComputeRect.getLy() <= 0) return;
    if (warpedComputeRect.getLx() <= 0 || warpedComputeRect.getLy() <= 0)
      return;

    m_warped->dryCompute(warpedComputeRect, frame, info);
  }

  //-------------------------------------------------------------------

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override {
    bool isWarped = m_warped.isConnected();

    if (!isWarped) return;

    if (fabs(m_intensity->getValue(frame)) < 0.01) {
      m_warped->compute(tile, frame, info);
      return;
    }

    int shrink      = (info.m_shrinkX + info.m_shrinkY) / 2;
    double scale    = sqrt(fabs(info.m_affine.det()));
    double gridStep = 1.5 * m_gridStep->getValue(frame);

    WarpParams params;
    params.m_intensity   = m_intensity->getValue(frame) / gridStep;
    params.m_warperScale = scale * gridStep;
    params.m_sharpen     = m_sharpen->getValue();
    params.m_shrink      = shrink;
    double period        = m_period->getValue(frame) / info.m_shrinkX;
    double count         = m_count->getValue(frame);
    double cycle         = m_cycle->getValue(frame) / info.m_shrinkX;
    double scaleX        = m_scaleX->getValue(frame) / 100.0;
    double scaleY        = m_scaleY->getValue(frame) / 100.0;
    double angle         = -m_angle->getValue(frame);
    TPointD center       = m_center->getValue(frame) * (1.0 / info.m_shrinkX);

    // The warper is calculated on a standard reference, with fixed dpi. This
    // makes sure
    // that the lattice created for the warp does not depend on camera
    // transforms and resolution.
    TRenderSettings warperInfo(info);
    double warperScaleFactor = 1.0 / params.m_warperScale;
    warperInfo.m_affine      = TScale(warperScaleFactor) * info.m_affine;

    // Retrieve tile's geometry
    TRectD tileRect;
    {
      TRasterP tileRas = tile.getRaster();
      tileRect =
          TRectD(tile.m_pos, TDimensionD(tileRas->getLx(), tileRas->getLy()));
    }

    // Build the compute rect
    TRectD warpedBox, warpedComputeRect, tileComputeRect;
    m_warped->getBBox(frame, warpedBox, info);

    getWarpComputeRects(tileComputeRect, warpedComputeRect, warpedBox, tileRect,
                        params);

    if (tileComputeRect.getLx() <= 0 || tileComputeRect.getLy() <= 0) return;
    if (warpedComputeRect.getLx() <= 0 || warpedComputeRect.getLy() <= 0)
      return;

    TRectD warperComputeRect(TScale(warperScaleFactor) * tileComputeRect);
    double warperEnlargement = getWarperEnlargement(params);
    warperComputeRect        = warperComputeRect.enlarge(warperEnlargement);
    warperComputeRect.x0     = tfloor(warperComputeRect.x0);
    warperComputeRect.y0     = tfloor(warperComputeRect.y0);
    warperComputeRect.x1     = tceil(warperComputeRect.x1);
    warperComputeRect.y1     = tceil(warperComputeRect.y1);

    // Compute the warped tile
    TTile tileIn;
    m_warped->allocateAndCompute(
        tileIn, warpedComputeRect.getP00(),
        TDimension(warpedComputeRect.getLx(), warpedComputeRect.getLy()),
        tile.getRaster(), frame, info);
    TRasterP rasIn = tileIn.getRaster();

    // Compute the warper tile
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(0.5, TPixel32::Black),
        TSpectrum::ColorKey(1, TPixel32::White)};
    TSpectrumParamP ripplecolors = TSpectrumParamP(colors);

    // Build the multiradial
    warperInfo.m_affine = warperInfo.m_affine * TTranslation(center) *
                          TRotation(angle) * TScale(scaleX, scaleY);
    TAffine aff      = warperInfo.m_affine.inv();
    TPointD posTrasf = aff * (warperComputeRect.getP00());
    TRasterP rasWarper =
        rasIn->create(warperComputeRect.getLx(), warperComputeRect.getLy());
    multiRadial(rasWarper, posTrasf, ripplecolors, period, count, cycle, aff,
                frame);
    // TImageWriter::save(TFilePath("C:\\ripple.tif"), rasWarper);

    // Warp
    TPointD db;
    TRect rasComputeRectI(convert(tileComputeRect - tileRect.getP00(), db));
    TRasterP tileRas = tile.getRaster()->extract(rasComputeRectI);

    TPointD rasInPos(warpedComputeRect.getP00() - tileComputeRect.getP00());
    TPointD warperPos(
        (TScale(params.m_warperScale) * warperComputeRect.getP00()) -
        tileComputeRect.getP00());
    warp(tileRas, rasIn, rasWarper, rasInPos, warperPos, params);
  }

  //-------------------------------------------------------------------

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 2];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Center";
    concepts[0].m_params.push_back(m_center);

    concepts[1].m_type  = TParamUIConcept::RADIUS;
    concepts[1].m_label = "Period";
    concepts[1].m_params.push_back(m_period);
    concepts[1].m_params.push_back(m_center);
  }

  //-------------------------------------------------------------------

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    // return -1;   //Deactivated. This fx is currently very inefficient if
    // subdivided!

    int shrink      = (info.m_shrinkX + info.m_shrinkY) / 2;
    double scale    = sqrt(fabs(info.m_affine.det()));
    double gridStep = 1.5 * m_gridStep->getValue(frame);

    WarpParams params;
    params.m_intensity   = m_intensity->getValue(frame) / gridStep;
    params.m_warperScale = scale * gridStep;
    params.m_sharpen     = m_sharpen->getValue();
    params.m_shrink      = shrink;

    double warperScaleFactor = 1.0 / params.m_warperScale;

    TRectD warpedBox, warpedComputeRect, tileComputeRect;
    m_warped->getBBox(frame, warpedBox, info);

    getWarpComputeRects(tileComputeRect, warpedComputeRect, warpedBox, rect,
                        params);

    TRectD warperComputeRect(TScale(warperScaleFactor) * tileComputeRect);
    double warperEnlargement = getWarperEnlargement(params);
    warperComputeRect        = warperComputeRect.enlarge(warperEnlargement);

    return std::max(TRasterFx::memorySize(warpedComputeRect, info.m_bpp),
                    TRasterFx::memorySize(warperComputeRect, info.m_bpp));
  }
};

//-------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(RippleFx, "rippleFx")
