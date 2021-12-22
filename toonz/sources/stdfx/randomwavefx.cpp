

#include "stdfx.h"
#include "tfxparam.h"
#include "warp.h"
#include "trop.h"
#include "trasterfx.h"
#include "tspectrumparam.h"
#include "gradients.h"
#include "timage_io.h"
#include "perlinnoise.h"
#include "tparamuiconcept.h"

//-------------------------------------------------------------------

class RandomWaveFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RandomWaveFx)
protected:
  TRasterFxPort m_warped;
  TDoubleParamP m_intensity;
  TDoubleParamP m_gridStep;
  TDoubleParamP m_evol;
  TDoubleParamP m_posx;
  TDoubleParamP m_posy;
  TBoolParamP m_sharpen;

public:
  RandomWaveFx()
      : m_intensity(20)
      , m_gridStep(2)
      , m_evol(0.0)
      , m_posx(0.0)
      , m_posy(0.0)
      , m_sharpen(false) {
    m_posx->setMeasureName("fxLength");
    m_posy->setMeasureName("fxLength");

    addInputPort("Source", m_warped);
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "sensitivity", m_gridStep);
    bindParam(this, "evolution", m_evol);
    bindParam(this, "positionx", m_posx);
    bindParam(this, "positiony", m_posy);
    bindParam(this, "sharpen", m_sharpen);
    m_intensity->setValueRange(-1000, 1000);
    m_gridStep->setValueRange(2, 20);
  }

  //-------------------------------------------------------------------

  virtual ~RandomWaveFx() {}

  //-------------------------------------------------------------------

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::POINT_2;
    concepts[0].m_label = "Position";
    concepts[0].m_params.push_back(m_posx);
    concepts[0].m_params.push_back(m_posy);
  }

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
    double evolution     = m_evol->getValue(frame);
    double size          = 100.0 / info.m_shrinkX;
    TPointD pos(m_posx->getValue(frame), m_posy->getValue(frame));

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
        TSpectrum::ColorKey(1, TPixel32::Black)};
    TSpectrumParamP cloudscolors = TSpectrumParamP(colors);

    // Build the warper
    warperInfo.m_affine = warperInfo.m_affine;
    TAffine aff         = warperInfo.m_affine.inv();

    TTile warperTile;
    TRasterP rasWarper =
        rasIn->create(warperComputeRect.getLx(), warperComputeRect.getLy());
    warperTile.m_pos = warperComputeRect.getP00();
    warperTile.setRaster(rasWarper);

    {
      TRenderSettings info2(warperInfo);

      // Now, separate the part of the affine the Fx can handle from the rest.
      TAffine fxHandledAffine = handledAffine(warperInfo, frame);
      info2.m_affine          = fxHandledAffine;

      TAffine aff = warperInfo.m_affine * fxHandledAffine.inv();
      aff.a13 /= warperInfo.m_shrinkX;
      aff.a23 /= warperInfo.m_shrinkY;

      TRectD rectIn = aff.inv() * warperComputeRect;

      // rectIn = rectIn.enlarge(getResampleFilterRadius(info));  //Needed to
      // counter the resample filter

      TRect rectInI(tfloor(rectIn.x0), tfloor(rectIn.y0), tceil(rectIn.x1) - 1,
                    tceil(rectIn.y1) - 1);

      // rasIn e' un raster dello stesso tipo di tile.getRaster()

      TTile auxtile(
          warperTile.getRaster()->create(rectInI.getLx(), rectInI.getLy()),
          convert(rectInI.getP00()));

      TPointD mypos(auxtile.m_pos - pos);

      double scale2 = sqrt(fabs(info2.m_affine.det()));
      doClouds(auxtile.getRaster(), cloudscolors, mypos, evolution, size, 0.0,
               1.0, PNOISE_CLOUDS, scale2, frame);

      info2.m_affine = aff;
      TRasterFx::applyAffine(warperTile, auxtile, info2);
    }

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

FX_PLUGIN_IDENTIFIER(RandomWaveFx, "randomWaveFx")
