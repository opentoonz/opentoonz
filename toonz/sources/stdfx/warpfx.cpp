

#include "stdfx.h"
#include "tfxparam.h"
#include "trop.h"
#include "warp.h"
#include "trasterfx.h"
//#include "timage_io.h"

//-------------------------------------------------------------------

class WarpFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(WarpFx)

protected:
  TRasterFxPort m_warped, m_warper;
  TDoubleParamP m_intensity;
  TDoubleParamP m_gridStep;
  TBoolParamP m_sharpen;

public:
  WarpFx() : m_intensity(20), m_gridStep(2), m_sharpen(true) {
    addInputPort("Source", m_warped);
    addInputPort("warper", m_warper);
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "sensitivity", m_gridStep);
    bindParam(this, "sharpen", m_sharpen);

    m_intensity->setValueRange(-1000, 1000);
    m_gridStep->setValueRange(2, 20);
  }
  virtual ~WarpFx() {}

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
    bool isWarper = m_warper.isConnected();
    if (!isWarped) return;
    if (!isWarper || fabs(m_intensity->getValue(frame)) < 0.01) {
      m_warped->dryCompute(rect, frame, info);
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

    TRenderSettings warperInfo(info);
    double warperScaleFactor = 1.0 / params.m_warperScale;
    warperInfo.m_affine      = TScale(warperScaleFactor) * info.m_affine;

    TRectD warpedBox, warpedComputeRect, tileComputeRect;
    m_warped->getBBox(frame, warpedBox, info);

    getWarpComputeRects(tileComputeRect, warpedComputeRect, warpedBox, rect,
                        params);

    if (tileComputeRect.getLx() <= 0 || tileComputeRect.getLy() <= 0) return;
    if (warpedComputeRect.getLx() <= 0 || warpedComputeRect.getLy() <= 0)
      return;

    TPointD db;
    TRectD warperComputeRect(TScale(warperScaleFactor) * tileComputeRect);
    double warperEnlargement = getWarperEnlargement(params);
    warperComputeRect        = warperComputeRect.enlarge(warperEnlargement);
    warperComputeRect.x0     = tfloor(warperComputeRect.x0);
    warperComputeRect.y0     = tfloor(warperComputeRect.y0);
    warperComputeRect.x1     = tceil(warperComputeRect.x1);
    warperComputeRect.y1     = tceil(warperComputeRect.y1);

    m_warped->dryCompute(warpedComputeRect, frame, info);
    m_warper->dryCompute(warperComputeRect, frame, warperInfo);
  }

  //-------------------------------------------------------------------

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override {
    bool isWarped = m_warped.isConnected();
    bool isWarper = m_warper.isConnected();

    if (!isWarped) return;

    if (!isWarper) {
      m_warped->compute(tile, frame, info);
      return;
    }
    if (fabs(m_intensity->getValue(frame)) < 0.01) {
      m_warped->compute(tile, frame, info);
      return;
    }

    int shrink      = (info.m_shrinkX + info.m_shrinkY) / 2;
    double scale    = sqrt(fabs(info.m_affine.det()));
    double gridStep = 1.5 * m_gridStep->getValue(frame);

    // NOTE: The gridStep is absorbed by the warper scale and the intensity -
    // the former
    // balancing the latter.

    WarpParams params;
    params.m_intensity   = m_intensity->getValue(frame) / gridStep;
    params.m_warperScale = scale * gridStep;
    params.m_sharpen     = m_sharpen->getValue();
    params.m_shrink      = shrink;

    // The warper is calculated with a fixed dpi. This makes sure that the
    // lattice
    // created for the warp does not depend on camera resolutions / affine
    // scales.
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

    // Compute the warper tile
    TTile tileWarper;
    m_warper->allocateAndCompute(
        tileWarper, warperComputeRect.getP00(),
        TDimension(warperComputeRect.getLx(), warperComputeRect.getLy()),
        tile.getRaster(), frame, warperInfo);

    // Warp
    TRasterP rasIn     = tileIn.getRaster();
    TRasterP rasWarper = tileWarper.getRaster();

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

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    // return 0;   //For debug purpose

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

FX_PLUGIN_IDENTIFIER(WarpFx, "warpFx")
