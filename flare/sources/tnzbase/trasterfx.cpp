

#include "trasterfx.h"

// Core-system includes
#include "tsystem.h"
#include "tthreadmessage.h"

// Fx basics
#include "tparamcontainer.h"
#include "tbasefx.h"
#include "tfxattributes.h"

// Images components
#include "timagecache.h"
#include "trasterimage.h"
#include "trop.h"

// Optimization components
#include "trenderresourcemanager.h"
#include "tfxcachemanager.h"
#include "trenderer.h"

// Diagnostics
// #define DIAGNOSTICS
#ifdef DIAGNOSTICS
#include "diagnostics.h"

#endif  // DIAGNOSTICS

//===================================================

namespace {
inline bool areEqual(const TRasterFxRenderDataP &d1,
                     const TRasterFxRenderDataP &d2) {
  return (*d1 == *d2);
}

//--------------------------------------------------

inline int myCeil(double x) {
  return ((x - (int)(x)) > TConsts::epsilon ? (int)(x) + 1 : (int)(x));
}

//--------------------------------------------------

inline TRect myConvert(const TRectD &r, TPointD &dp) {
  TRect ri(tfloor(r.x0), tfloor(r.y0), myCeil(r.x1), myCeil(r.y1));
  dp.x = r.x0 - ri.x0;
  dp.y = r.y0 - ri.y0;
  assert(dp.x >= 0 && dp.y >= 0);
  return ri;
}

//--------------------------------------------------

inline bool myIsEmpty(const TRectD &r) {
  return r.getLx() < 1 || r.getLy() < 1;
}

//--------------------------------------------------

inline TRect myConvert(const TRectD &rect) {
  return TRect(tround(rect.x0), tround(rect.y0), tround(rect.x1) - 1,
               tround(rect.y1) - 1);
}

//--------------------------------------------------

inline TRectD myConvert(const TRect &rect) {
  return TRectD(rect.x0, rect.y0, rect.x1 + 1, rect.y1 + 1);
}

//--------------------------------------------------

inline void enlargeToI(TRectD &r) {
  TRectD temp(tfloor(r.x0), tfloor(r.y0), tceil(r.x1), tceil(r.y1));
  // NOTE: If we enlarge a TConsts::infiniteRectD or one which trespass
  // ints' numerical bounds, the rect may become empty.
  if (!myIsEmpty(temp)) r = temp;
}

//--------------------------------------------------

// Calculates the 2-norm of the passed affine A - that is, the max modulus
// of A*A's eigenvalues (*A being the adjoint).
double norm2(const TAffine &aff) {
  double a11 = aff.a11 * aff.a11 + aff.a12 * aff.a12;
  double a12 = aff.a11 * aff.a21 + aff.a12 * aff.a22;
  double a21 = aff.a21 * aff.a11 + aff.a22 * aff.a12;
  double a22 = aff.a21 * aff.a21 + aff.a22 * aff.a22;

  double a11plusa22 = a11 + a22;
  double delta      = sq(a11 - a22) + 4 * a12 * a21;

  delta = sqrt(delta);

  double eig1 = a11plusa22 + delta;
  double eig2 = a11plusa22 - delta;
  return std::max(sqrt(eig1 / 2.0), sqrt(eig2 / 2.0));
}

//--------------------------------------------------

inline int getResampleFilterRadius(const TRenderSettings &info) {
  switch (info.m_quality) {
  case TRenderSettings::StandardResampleQuality:
    return 1;
  case TRenderSettings::ImprovedResampleQuality:
    return 2;
  case TRenderSettings::HighResampleQuality:
    return 3;
  case TRenderSettings::Triangle_FilterResampleQuality:
    return 1;
  case TRenderSettings::Mitchell_FilterResampleQuality:
    return 2;
  case TRenderSettings::Cubic5_FilterResampleQuality:
    return 2;
  case TRenderSettings::Cubic75_FilterResampleQuality:
    return 2;
  case TRenderSettings::Cubic1_FilterResampleQuality:
    return 2;
  case TRenderSettings::Hann2_FilterResampleQuality:
    return 2;
  case TRenderSettings::Hann3_FilterResampleQuality:
    return 3;
  case TRenderSettings::Hamming2_FilterResampleQuality:
    return 2;
  case TRenderSettings::Hamming3_FilterResampleQuality:
    return 3;
  case TRenderSettings::Lanczos2_FilterResampleQuality:
    return 2;
  case TRenderSettings::Lanczos3_FilterResampleQuality:
    return 3;
  case TRenderSettings::Gauss_FilterResampleQuality:
    return 2;
  case TRenderSettings::ClosestPixel_FilterResampleQuality:
    return 1;
  case TRenderSettings::Bilinear_FilterResampleQuality:
    return 1;
  default:
    assert(false);
    return -1;
  }
}

//--------------------------------------------------

inline QString traduce(const TRectD &rect) {
  return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " " +
         QString::number(rect.x1) + " " + QString::number(rect.y1) + "]";
}

//--------------------------------------------------

inline QString traduce(const TRect &rect) {
  return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " " +
         QString::number(rect.x1) + " " + QString::number(rect.y1) + "]";
}

//--------------------------------------------------

inline QString qTraduce(const TAffine &aff) {
  return "[" + QString::number(aff.a11, 'g', 15) + "," +
         QString::number(aff.a12, 'g', 15) + "," +
         QString::number(aff.a13, 'g', 15) + "," +
         QString::number(aff.a21, 'g', 15) + "," +
         QString::number(aff.a22, 'g', 15) + "," +
         QString::number(aff.a23, 'g', 15) + "]";
}

//--------------------------------------------------

inline std::string traduce(const TAffine &aff) {
  return
      // Observe that toString distinguishes + and - 0. That is a problem
      // when comparing aliases - so near 0 values are explicitly rounded to 0.
      (areAlmostEqual(aff.a11, 0.0) ? "0" : ::to_string(aff.a11, 5)) + "," +
      (areAlmostEqual(aff.a12, 0.0) ? "0" : ::to_string(aff.a12, 5)) + "," +
      (areAlmostEqual(aff.a13, 0.0) ? "0" : ::to_string(aff.a13, 5)) + "," +
      (areAlmostEqual(aff.a21, 0.0) ? "0" : ::to_string(aff.a21, 5)) + "," +
      (areAlmostEqual(aff.a22, 0.0) ? "0" : ::to_string(aff.a22, 5)) + "," +
      (areAlmostEqual(aff.a23, 0.0) ? "0" : ::to_string(aff.a23, 5));
}

}  // namespace

//------------------------------------------------------------------------------

// The following should be cleared - and its functionalities surrendered
// directly
// to an appropriate resource manager...

//! Declares an image to be kept in cache until the render ends or is canceled.
void addRenderCache(const std::string &alias, TImageP image) {
  TFxCacheManager::instance()->add(alias, image);
}

//------------------------------------------------------------------------------

//! The inverse function to addRenderCache.
void removeRenderCache(const std::string &alias) {
  TFxCacheManager::instance()->remove(alias);
}

//==============================================================================
//
// TrFx   (Affine Transformer Fx)
//
//------------------------------------------------------------------------------

//! Internal fx node which applies the implicit affine stored in the
//! TRenderSettings structure,
//! replacing the old TRasterFx::doCompute.
class TrFx final : public TBaseRasterFx {
  FX_DECLARATION(TrFx)

  TRasterFx *m_fx;

public:
  TrFx() { enableComputeInFloat(true); }
  ~TrFx() {}

  //-----------------------------------------------------------

  void setFx(TRasterFx *fx) { m_fx = fx; }

  //-----------------------------------------------------------

  bool isCachable() const override {
    return true;
  }  // Currently cachable as a test. Observe that it was NOT in Toonz 6.1

  //-----------------------------------------------------------

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  //-----------------------------------------------------------

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override {
    // NOTE: TrFx are not present at this recursive level. Affines dealing is
    // currently handled by inserting the
    // rendering affine AFTER a getAlias call. Ever.
    std::string alias = getFxType();
    return alias + "[" + m_fx->getAlias(frame, info) + "]";
  }

  //-----------------------------------------------------------

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    // NOTE: TrFx are not present at this recursive level. Affines dealing is
    // still handled by TGeometryFxs here....
    return m_fx->doGetBBox(frame, bBox, info);
  }

  //-----------------------------------------------------------

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override {
    const TRectD &rectOut =
        myConvert(tile.getRaster()->getBounds()) + tile.m_pos;

    TRectD rectIn;
    TRenderSettings infoIn(info);
    TAffine appliedAff;

    if (!buildInput(rectOut, frame, info, rectIn, infoIn, appliedAff)) return;

    const TRect &rectInI = myConvert(rectIn);

    // rasIn e' un raster dello stesso tipo di tile.getRaster()

    TTile inTile;
    m_fx->allocateAndCompute(inTile, rectIn.getP00(),
                             TDimension(rectInI.getLx(), rectInI.getLy()),
                             tile.getRaster(), frame, infoIn);

    infoIn.m_affine = appliedAff;
    TRasterFx::applyAffine(tile, inTile, infoIn);
  }

  //-----------------------------------------------------------

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    TRectD rectIn;
    TRenderSettings infoIn(info);
    TAffine appliedAff;

    if (!buildInput(rect, frame, info, rectIn, infoIn, appliedAff)) return;

    m_fx->dryCompute(rectIn, frame, infoIn);
  }

  //-----------------------------------------------------------

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    TRectD rectIn;
    TRenderSettings infoIn(info);
    TAffine appliedAff;

    if (!buildInput(rect, frame, info, rectIn, infoIn, appliedAff)) return 0;

    return TRasterFx::memorySize(rectIn, info.m_bpp);
  }

  //-----------------------------------------------------------

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override {
    return tileIsLinear;
  }

private:
  bool buildInput(const TRectD &rectOut, double frame,
                  const TRenderSettings &infoOut, TRectD &rectIn,
                  TRenderSettings &infoIn, TAffine &appliedAff) {
    if (myIsEmpty(rectOut)) return false;

    // Build the affines
    infoIn.m_affine = m_fx->handledAffine(infoOut, frame);
    appliedAff      = infoOut.m_affine * infoIn.m_affine.inv();

    // Quick fix - prevent near-singular transforms.
    // NOTE: This check **SHOULD BE REMOVED** and dealt with at the RESAMPLING
    // level.
    //       Observe how the tolerance value is quite arbitrary! Yeah I know
    //       it's shameful... :(
    if (fabs(appliedAff.det()) < 1e-6)  // TO BE REMOVED
      return false;                     // TO BE REMOVED

    // Build the input rect
    const TAffine &appliedAffInv = appliedAff.inv();
    int filterRadius             = getResampleFilterRadius(infoOut);

    TRectD bbox;
    m_fx->getBBox(frame, bbox, infoIn);

    rectIn =
        ((appliedAffInv * rectOut)
             .enlarge(
                 filterRadius) +  // The filter size applies in input during
         (appliedAffInv * rectOut.enlarge(filterRadius))) *
        bbox;  // magnifications, and in output during
               // minifications. Thus, they basically cumulate.
    if (myIsEmpty(rectIn)) return false;

    enlargeToI(rectIn);
    return true;
  }
};

FX_IDENTIFIER_IS_HIDDEN(TrFx, "trFx")

//==============================================================================
//
// FxResourceBuilder
//
//------------------------------------------------------------------------------

// This class is the internal interface with the cache regarding intermediate
// render
// results. Please refer to the ResourceBuilder documentation in
// tfxcachemanager.cpp
class FxResourceBuilder final : public ResourceBuilder {
  TRasterFxP m_rfx;
  double m_frame;
  const TRenderSettings *m_rs;

  TTile *m_outTile;
  TTile *m_currTile;
  TTile m_newTile;

  TRectD m_outRect;

public:
  FxResourceBuilder(const std::string &resourceName, const TRasterFxP &fx,
                    const TRenderSettings &rs, double frame)
      : ResourceBuilder(resourceName, fx.getPointer(), frame, rs)
      , m_rfx(fx)
      , m_frame(frame)
      , m_rs(&rs)
      , m_currTile(0) {}

  inline void build(TTile &tile);

protected:
  void simCompute(const TRectD &rect) override {
    TRectD rectCpy(
        rect);  // Why the hell dryCompute(..) has non-const TRectD& input ????
    m_rfx->doDryCompute(rectCpy, m_frame, *m_rs);
  }

  void buildTileToCalculate(const TRectD &tileRect);
  void compute(const TRectD &tileRect) override;

  void upload(TCacheResourceP &resource) override;
  bool download(TCacheResourceP &resource) override;
};

//------------------------------------------------------------------------------

inline void FxResourceBuilder::build(TTile &tile) {
  m_outTile = &tile;

  TDimension dim(tile.getRaster()->getSize());
  m_outRect = TRectD(tile.m_pos, TDimensionD(dim.lx, dim.ly));

  ResourceBuilder::build(m_outRect);
}

//------------------------------------------------------------------------------

void FxResourceBuilder::buildTileToCalculate(const TRectD &tileGeom) {
  if (tileGeom == m_outRect) {
    m_currTile = m_outTile;
    return;
  }

  m_newTile.m_pos = tileGeom.getP00();

  TRasterP outRas(m_outTile->getRaster());

  // If possible, try to reuse outRas's buffer.

  TDimension outputSize(outRas->getSize());
  TDimension requiredSize(tceil(tileGeom.getLx()), tceil(tileGeom.getLy()));

  TRasterP ras;
  if (outputSize.lx >= requiredSize.lx && outputSize.ly >= requiredSize.ly) {
    // Reuse fxOutput's buffer
    TRect rect(0, 0, requiredSize.lx - 1, requiredSize.ly - 1);
    ras = outRas->extract(rect);
    ras->clear();
  } else {
    ras = outRas->create(requiredSize.lx, requiredSize.ly);
    ras->setLinear(outRas->isLinear());
  }

  m_newTile.setRaster(ras);

  m_currTile = &m_newTile;
}

//------------------------------------------------------------------------------

void FxResourceBuilder::compute(const TRectD &tileRect) {
#ifdef DIAGNOSTICS
  TStopWatch sw;
  sw.start();
#endif

  buildTileToCalculate(tileRect);
  m_rfx->doCompute(*m_currTile, m_frame, *m_rs);

#ifdef DIAGNOSTICS
  sw.stop();

  DIAGNOSTICS_THRSET("FComputeTime", sw.getTotalTime());
#endif
}

//------------------------------------------------------------------------------

void FxResourceBuilder::upload(TCacheResourceP &resource) {
  resource->upload(*m_currTile);
  if (m_currTile == &m_newTile) m_newTile.setRaster(0);
}

//------------------------------------------------------------------------------

bool FxResourceBuilder::download(TCacheResourceP &resource) {
  // In case the output tile was used to calculate the fx, avoid downloading
  if (m_currTile == m_outTile) return true;

  return resource->downloadAll(*m_outTile);
}

//==============================================================================
//
// TRasterFx
//
//------------------------------------------------------------------------------

class TRasterFx::TRasterFxImp {
public:
  bool m_cacheEnabled;
  TTile m_cachedTile;
  double m_frame;
  bool m_isEnabled;

  TRenderSettings m_info;
  std::string m_interactiveCacheId;
  mutable TThread::Mutex m_mutex;  // brutto

  bool m_canComputeInFloat;
  bool m_canComputeInLinearColorSpace;

  TRasterFxImp()
      : m_cacheEnabled(false)
      , m_isEnabled(true)
      , m_cachedTile(0)
      , m_canComputeInFloat(false) {}

  ~TRasterFxImp() {}

  void enableCache(bool on) {
    QMutexLocker sl(&m_mutex);
    m_cacheEnabled = on;
    if (!m_cacheEnabled) {
      m_interactiveCacheId = "";
      m_frame              = 0;
      m_info               = TRenderSettings();
      m_cachedTile.setRaster(0);
      m_cachedTile.m_pos = TPointD();
    }
  }

  bool isCacheEnabled() const { return m_cacheEnabled; }

  bool isEnabled() const {
    QMutexLocker sl(&m_mutex);  // a che serve??
    return m_isEnabled;
  }

  void enable(bool on) {
    QMutexLocker sl(&m_mutex);  // a che serve
    m_isEnabled = on;
  }

  void enableComputeInFloat(bool on) { m_canComputeInFloat = on; }
  bool canComputeInFloat() { return m_canComputeInFloat; }
};

//--------------------------------------------------

TRasterFx::TRasterFx() : m_rasFxImp(new TRasterFxImp) {}

//--------------------------------------------------

TRasterFx::~TRasterFx() { delete m_rasFxImp; }

//--------------------------------------------------

TAffine TRasterFx::handledAffine(const TRenderSettings &info, double frame) {
  return (info.m_affine.a11 == info.m_affine.a22 && info.m_affine.a12 == 0 &&
          info.m_affine.a21 == 0 && info.m_affine.a13 == 0 &&
          info.m_affine.a23 == 0)
             ? info.m_affine
             : TScale(norm2(info.m_affine));
}

//--------------------------------------------------

bool TRasterFx::getBBox(double frame, TRectD &bBox,
                        const TRenderSettings &info) {
  bool ret = doGetBBox(frame, bBox, info);
  if (!bBox.isEmpty()) {  // TODO: check if bbox can always be empty when ret ==
                          // false
    bBox = info.m_affine * bBox;
    enlargeToI(bBox);
  }
  return ret;
}

//--------------------------------------------------

void TRasterFx::transform(double frame, int port, const TRectD &rectOnOutput,
                          const TRenderSettings &infoOnOutput,
                          TRectD &rectOnInput, TRenderSettings &infoOnInput) {
  rectOnInput = rectOnOutput;
  infoOnInput = infoOnOutput;
}

//--------------------------------------------------

int TRasterFx::memorySize(const TRectD &rect, int bpp) {
  if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) return 0;

  return (unsigned long)(rect.getLx() + 1) * (unsigned long)(rect.getLy() + 1) *
             (bpp >> 3) >>
         20;
}

//--------------------------------------------------

//! Specifies the approximate size (in MegaBytes) of the maximum allocation
//! instance that this fx
//! will need to perform in order to render the passed input.
//! This method should be reimplemented in order to make the Toonz rendering
//! process aware of
//! the use of big raster memory chunks, at least.
//! Observe that the passed tile geometry is implicitly <i> already allocated
//! <\i> for the fx output.
//! \n
//! The default implementation returns 0, assuming that the passed implicit
//! memory size is passed
//! below in node computation without further allocation of resources. Fxs can
//! reimplement this
//! to -1 to explicitly disable the Toonz's smart memory handling.
//! \n \n
//! \note The rendering process ensures that the passed render settings are \a
//! handled
//! by the fx before this function is invoked - do not waste code for it.
//! \sa TRasterFx::memorySize and TRasterFx::canHandle methods.

int TRasterFx::getMemoryRequirement(const TRectD &rect, double frame,
                                    const TRenderSettings &info) {
  return 0;
}

//--------------------------------------------------

std::string TRasterFx::getAlias(double frame,
                                const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)
  int i;
  for (i = 0; i < getInputPortCount(); i++) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  // alias dei valori dei parametri dell'effetto al frame dato
  for (i = 0; i < getParams()->getParamCount(); i++) {
    TParam *param = getParams()->getParam(i);
    alias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }

  alias += "]";
  return alias;
}

//--------------------------------------------------

void TRasterFx::dryCompute(TRectD &rect, double frame,
                           const TRenderSettings &info) {
  if (checkActiveTimeRegion() && !getActiveTimeRegion().contains(frame)) return;

  if (!getAttributes()->isEnabled() || !m_rasFxImp->isEnabled()) {
    if (getInputPortCount() == 0) return;

    TFxPort *port = getInputPort(getPreferredInputPort());

    if (port->isConnected())
      TRasterFxP(port->getFx())->dryCompute(rect, frame, info);

    return;
  }

  // If the input tile has a fractionary position, it is passed to the
  // rendersettings' accumulated affine.
  TPoint intTilePos(tfloor(rect.x0), tfloor(rect.y0));
  TPointD fracTilePos(rect.x0 - intTilePos.x, rect.y0 - intTilePos.y);
  TPointD fracInfoTranslation(info.m_affine.a13 - fracTilePos.x,
                              info.m_affine.a23 - fracTilePos.y);
  TPoint intInfoTranslation(tfloor(fracInfoTranslation.x),
                            tfloor(fracInfoTranslation.y));
  TPointD newTilePos(intTilePos.x - intInfoTranslation.x,
                     intTilePos.y - intInfoTranslation.y);

  if (rect.getP00() != newTilePos) {
    TRenderSettings newInfo(info);
    newInfo.m_affine.a13 = fracInfoTranslation.x - intInfoTranslation.x;
    newInfo.m_affine.a23 = fracInfoTranslation.y - intInfoTranslation.y;

    TRectD newRect(newTilePos, rect.getSize());

    dryCompute(newRect, frame, newInfo);
    return;
  }

  // If the fx can't handle the whole affine passed with the TRenderSettings,
  // the part
  // of it that the fx can't handle is retained and applied by an affine
  // transformer fx (TrFx)
  // after the node has been computed.
  bool canHandleAffine =
      canHandle(info, frame) || (handledAffine(info, frame) == info.m_affine);
  if (!info.m_affine.isIdentity() && !canHandleAffine) {
    TrFx *transformerFx = new TrFx;
    TFxP locker(transformerFx);
    transformerFx->setFx(this);
    transformerFx->dryCompute(rect, frame, info);
    return;
  }

  std::string alias = getAlias(frame, info) + "[" + ::traduce(info.m_affine) +
                      "][" + std::to_string(info.m_bpp) + "][" +
                      std::to_string(info.m_linearColorSpace) + "][" +
                      std::to_string(info.m_colorSpaceGamma) + "]";

  int renderStatus =
      TRenderer::instance().getRenderStatus(TRenderer::renderId());
  TFxCacheManager *cacheManager = TFxCacheManager::instance();

  if (renderStatus == TRenderer::FIRSTRUN) {
    TRectD bbox;
    // ret = getBBox... puo' darsi che l'enlarge del trFx (o naturale del bbox)
    // faccia
    // diventare TRectD() non vuoto!!
    getBBox(frame, bbox, info);
    enlargeToI(bbox);

    TRectD interestingRect(rect * bbox);
    if (myIsEmpty(interestingRect)) return;

    // Declare the tile to the tiles manager
    ResourceBuilder::declareResource(alias, this, interestingRect, frame, info);

    doDryCompute(interestingRect, frame, info);
  } else {
    TRectD bbox;
    getBBox(frame, bbox, info);
    enlargeToI(bbox);

    TRectD interestingRect(rect * bbox);
    if (myIsEmpty(interestingRect)) return;

    // Invoke the fx-specific simulation process
    FxResourceBuilder rBuilder(alias, this, info, frame);
    rBuilder.simBuild(interestingRect);
  }
}

//--------------------------------------------------

//! Declares the computation scheme of this fx for rendering optimization
//! purposes.
//! This is an important function of the Toonz rendering API, and should be
//! reimplemented
//! with the necessary care.
//! The Toonz rendering process makes use of this function to enact most of the
//! optimization steps about the fx computation, in particular fx caching.
//! A correct implementation of this method should follow these rules:
//! <li> The invocation of child node computations should be faithfully
//! reproduced.
//! <li> TRasterFx::compute and TRasterFx::allocateAndCompute calls have to be
//!      translated to TRasterFx::dryCompute calls.
//! <li> This fx is intended for precomputation stage, so the hard rendering
//! code
//!      should be skipped here.
//! By default, this method raises a dryCompute call to each input port in
//! increasing
//! order, using the TRasterFx::transform method to identify the tiles to be
//! passed
//! on input precomputation.
void TRasterFx::doDryCompute(TRectD &rect, double frame,
                             const TRenderSettings &info) {
  int inputPortCount = getInputPortCount();
  for (int i = 0; i < inputPortCount; ++i) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRectD rectOnInput;
      TRenderSettings infoOnInput;

      TRasterFxP fx = port->getFx();
      transform(frame, i, rect, info, rectOnInput, infoOnInput);

      if (!myIsEmpty(rectOnInput))
        fx->dryCompute(rectOnInput, frame, infoOnInput);
    }
  }
}

//--------------------------------------------------

//! This is an overloaded member function that deals with
//! the allocation of an input tile before invoking the TRasterFx::compute
//! method on it.
void TRasterFx::allocateAndCompute(TTile &tile, const TPointD &pos,
                                   const TDimension &size, TRasterP templateRas,
                                   double frame, const TRenderSettings &info) {
  if (templateRas) {
    TRaster32P ras32(templateRas);
    TRaster64P ras64(templateRas);
    TRasterFP rasF(templateRas);
    bool isLinear = templateRas->isLinear();
    templateRas = 0;  // Release the reference to templateRas before allocation

    TRasterP tileRas;
    if (ras32)
      tileRas = TRaster32P(size.lx, size.ly);
    else if (ras64)
      tileRas = TRaster64P(size.lx, size.ly);
    else if (rasF)
      tileRas = TRasterFP(size.lx, size.ly);
    else {
      assert(false);
      return;
    }
    tileRas->setLinear(isLinear);
    tile.setRaster(tileRas);
  } else {
    if (info.m_bpp == 32) {
      TRaster32P tileRas(size.lx, size.ly);
      tile.setRaster(tileRas);
    } else if (info.m_bpp == 64) {
      TRaster64P tileRas(size.lx, size.ly);
      tile.setRaster(tileRas);
    } else if (info.m_bpp == 128) {
      TRasterFP tileRas(size.lx, size.ly);
      tile.setRaster(tileRas);
    } else
      assert(false);
    tile.getRaster()->setLinear(info.m_linearColorSpace);
  }

  tile.m_pos = pos;
  compute(tile, frame, info);
}

//-----------------------------------------------------------------------

//! This method supplies the actual fx rendering code.

void TRasterFx::compute(TTile &tile, double frame,
                        const TRenderSettings &info) {
  // If the render was aborted, avoid everything
  // if(TRenderer::instance().isAborted(TRenderer::renderId()))
  //  throw TException("Render canceled");

  if (checkActiveTimeRegion() && !getActiveTimeRegion().contains(frame)) return;

  if (!getAttributes()->isEnabled() || !m_rasFxImp->isEnabled()) {
    if (getInputPortCount() == 0) return;

    TFxPort *port = getInputPort(0);

    // la porta 0 non deve essere una porta di controllo
    assert(port->isaControlPort() == false);

    if (port->isConnected()) {
      TRasterFxP(port->getFx())->compute(tile, frame, info);
      return;
    }

    return;
  }

  // If the input tile has a fractionary position, it is passed to the
  // rendersettings' accumulated affine. At the same time, the integer part of
  // such affine is transferred to the tile.
  TPoint intTilePos(tfloor(tile.m_pos.x), tfloor(tile.m_pos.y));
  TPointD fracTilePos(tile.m_pos.x - intTilePos.x, tile.m_pos.y - intTilePos.y);
  TPointD fracInfoTranslation(info.m_affine.a13 - fracTilePos.x,
                              info.m_affine.a23 - fracTilePos.y);
  TPoint intInfoTranslation(tfloor(fracInfoTranslation.x),
                            tfloor(fracInfoTranslation.y));
  TPointD newTilePos(intTilePos.x - intInfoTranslation.x,
                     intTilePos.y - intInfoTranslation.y);
  /*-- If the position of the input tile had a fractional value --*/
  if (tile.m_pos != newTilePos) {
    /*-- Add the offset to the affine matrix in RenderSettings --*/
    TRenderSettings newInfo(info);
    newInfo.m_affine.a13 = fracInfoTranslation.x - intInfoTranslation.x;
    newInfo.m_affine.a23 = fracInfoTranslation.y - intInfoTranslation.y;
    /*-- Tile position should be an integer value --*/
    TPointD oldPos(tile.m_pos);
    tile.m_pos = newTilePos;

    compute(tile, frame, newInfo);

    tile.m_pos = oldPos;
    return;
  }

  bool canHandleAffine =
      canHandle(info, frame) || (handledAffine(info, frame) == info.m_affine);
  if (!info.m_affine.isIdentity() && !canHandleAffine) {
    TrFx *transformerFx = new TrFx;
    TFxP locker(transformerFx);
    transformerFx->setFx(this);
    transformerFx->compute(tile, frame, info);
    return;
  }

  // Retrieve tile's geometry
  TRectD tilePlacement = myConvert(tile.getRaster()->getBounds()) + tile.m_pos;

  TRectD bbox;
  getBBox(frame, bbox, info);
  enlargeToI(bbox);

  TRectD interestingRect(tilePlacement * bbox);
  if (myIsEmpty(interestingRect)) return;

  TDimension tileSize = tile.getRaster()->getSize();
  // The format of the incoming raster depends on whether the previous node
  // supports floating point rendering or not.
  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  TRasterFP rasF   = tile.getRaster();
  if (info.m_bpp == 128) {
    if (rasF && !canComputeInFloat()) {
      TRasterP rasAux = TRaster64P(tileSize);
      TRop::convert(rasAux, tile.getRaster());
      tile.setRaster(rasAux);
    } else if ((ras32 || ras64) && canComputeInFloat()) {
      TRasterFP rasAuxF = TRasterFP(tileSize);
      TRop::convert(rasAuxF, tile.getRaster());
      tile.setRaster(rasAuxF);
    }
  }
  // convert to linear
  bool isLinear = tile.getRaster()->isLinear();
  bool computeInLinear =
      toBeComputedInLinearColorSpace(info.m_linearColorSpace, isLinear);
  if (isLinear != computeInLinear) {
    if (isLinear) {  //  && !computeInLinear
      TRop::tosRGB(tile.getRaster(), info.m_colorSpaceGamma);
    } else  // !isLinear && computeInLinear
      TRop::toLinearRGB(tile.getRaster(), info.m_colorSpaceGamma);
  }

  // Build the fx result alias (in other words, its name)
  std::string alias = getAlias(frame, info) + "[" + ::traduce(info.m_affine) +
                      "][" + std::to_string(info.m_bpp) + "][" +
                      std::to_string(computeInLinear) + "][" +
                      std::to_string(info.m_colorSpaceGamma) + "]";

  // Extract the interesting tile from requested one
  TTile interestingTile;
  interestingTile.m_pos = interestingRect.getP00();
  TRect interestingRectI(myConvert(interestingRect - tilePlacement.getP00()));
  interestingTile.setRaster(tile.getRaster()->extract(interestingRectI));

#ifdef DIAGNOSTICS

  // 1. Push fx name on call stack
  QString fxName = QString::fromStdString(getDeclaration()->getId());

  DIAGNOSTICS_PUSH("FName", fxName);
  DIAGNOSTICS_THRSET("FComputeTime", 0);

  TStopWatch sw;
  sw.start();

#endif

  // Invoke the fx-specific computation process
  FxResourceBuilder rBuilder(alias, this, info, frame);
  rBuilder.build(interestingTile);

  // convert to linear
  if (isLinear != computeInLinear) {
    if (isLinear)  //  && !computeInLinear
      TRop::toLinearRGB(tile.getRaster(), info.m_colorSpaceGamma);
    else  // !isLinear && computeInLinear
      TRop::tosRGB(tile.getRaster(), info.m_colorSpaceGamma);
  }

  if (info.m_bpp == 128) {
    if (rasF && !canComputeInFloat()) {
      TRop::convert(rasF, tile.getRaster());
      tile.setRaster(rasF);
    } else if ((ras32 || ras64) && canComputeInFloat()) {
      if (ras64) {
        TRop::convert(ras64, tile.getRaster());
        tile.setRaster(ras64);
      } else {
        TRop::convert(ras32, tile.getRaster());
        tile.setRaster(ras32);
      }
    }
  }

#ifdef DIAGNOSTICS
  sw.stop();

  QString countsStr("#ftimes.txt | 4. " + fxName + " | 4. Calls count");
  QString fxStr("fcum_4. " + fxName);

  long computeTime      = DIAGNOSTICS_THRGET("FComputeTime");
  long fxTime           = sw.getTotalTime();
  long fxCumulativeTime = DIAGNOSTICS_GLOGET(fxStr);
  long count            = DIAGNOSTICS_GET(countsStr);

  // 2. Add this time to fx time, and subtract it from parent time
  DIAGNOSTICS_GLOADD(fxStr, computeTime);
  DIAGNOSTICS_ADD(countsStr, 1);
  DIAGNOSTICS_SET("#ftimes.txt | 4. " + fxName + " | 3. Mean time",
                  (fxCumulativeTime + computeTime) / (count + 1));

  DIAGNOSTICS_POP("FName", 1);
  QString parentFxName = DIAGNOSTICS_STACKGET("FName");
  if (!parentFxName.isEmpty())
    DIAGNOSTICS_GLOADD("fcum_4. " + parentFxName, -fxTime);

  DIAGNOSTICS_GLOADD("fcum_03. Cached Fxs Retrieval", fxTime - computeTime);
#endif

#ifdef WRITEIMAGES
  static int iCount = 0;
  QString qPath("C:\\butta\\image_" +
                QString::number(++iCount).rightJustified(3, '0') + ".tif");
  TImageWriter::save(TFilePath(qPath.toStdWString()), tile.getRaster());

  if (iCount >= 35) int aa = 1;
#endif

  /*
return;

tryCanceled:

//if(TRenderer::instance().isAborted(TRenderer::renderId()))
//  throw TException("Render canceled");
;
*/
}

//------------------------------------------------------------------------------

TRasterP TRasterFx::applyAffine(TTile &tileOut, const TTile &tileIn,
                                const TRenderSettings &info) {
  TAffine aff = info.m_affine;

  TRasterP src_ras = tileIn.getRaster();
  TRasterP dst_ras = tileOut.getRaster();

  if (aff.isTranslation()) {
    // Check the tile origins' fractionary displacement
    TPointD diff(tileOut.m_pos - tileIn.m_pos - TPointD(aff.a13, aff.a23));
    double fracX = diff.x - tfloor(diff.x);
    double fracY = diff.y - tfloor(diff.y);

    if ((fracX < 0.01 || fracX > 0.99) && (fracY < 0.01 || fracY > 0.99)) {
      // Just copy part of tileIn into tileOut
      TRect geomIn(src_ras->getBounds());
      TRect geomOut(dst_ras->getBounds());
      TPoint diffI(convert(diff));
      geomIn -= diffI;
      geomOut += diffI;
      geomIn *= dst_ras->getBounds();
      geomOut *= src_ras->getBounds();

      if (geomIn.isEmpty()) return dst_ras;

      TRasterP rasIn(src_ras->extract(geomOut));
      TRasterP rasOut(dst_ras->extract(geomIn));
      TRop::copy(rasOut, rasIn);

      return dst_ras;
    }
  }

  TRectD rectIn  = myConvert(src_ras->getBounds()) + tileIn.m_pos;
  TRectD rectOut = myConvert(dst_ras->getBounds()) + tileOut.m_pos;

  TRectD rectInAfter = aff * myConvert(src_ras->getBounds());
  TAffine rasterAff  = TTranslation((aff * rectIn).getP00() - rectOut.getP00() -
                                    rectInAfter.getP00()) *
                      aff;

  TRop::ResampleFilterType qual;
  switch (info.m_quality) {
  case TRenderSettings::StandardResampleQuality:
    qual = TRop::Triangle;
    break;
  case TRenderSettings::ImprovedResampleQuality:
    qual = TRop::Hann2;
    break;
  case TRenderSettings::HighResampleQuality:
    qual = TRop::Hamming3;
    break;
  case TRenderSettings::Triangle_FilterResampleQuality:
    qual = TRop::Triangle;
    break;
  case TRenderSettings::Mitchell_FilterResampleQuality:
    qual = TRop::Mitchell;
    break;
  case TRenderSettings::Cubic5_FilterResampleQuality:
    qual = TRop::Cubic5;
    break;
  case TRenderSettings::Cubic75_FilterResampleQuality:
    qual = TRop::Cubic75;
    break;
  case TRenderSettings::Cubic1_FilterResampleQuality:
    qual = TRop::Cubic1;
    break;
  case TRenderSettings::Hann2_FilterResampleQuality:
    qual = TRop::Hann2;
    break;
  case TRenderSettings::Hann3_FilterResampleQuality:
    qual = TRop::Hann3;
    break;
  case TRenderSettings::Hamming2_FilterResampleQuality:
    qual = TRop::Hamming2;
    break;
  case TRenderSettings::Hamming3_FilterResampleQuality:
    qual = TRop::Hamming3;
    break;
  case TRenderSettings::Lanczos2_FilterResampleQuality:
    qual = TRop::Lanczos2;
    break;
  case TRenderSettings::Lanczos3_FilterResampleQuality:
    qual = TRop::Lanczos3;
    break;
  case TRenderSettings::Gauss_FilterResampleQuality:
    qual = TRop::Gauss;
    break;
  case TRenderSettings::ClosestPixel_FilterResampleQuality:
    qual = TRop::ClosestPixel;
    break;
  case TRenderSettings::Bilinear_FilterResampleQuality:
    qual = TRop::Bilinear;
    break;
  default:
    assert(false);
  }

  TRop::resample(dst_ras, src_ras, rasterAff, qual);

  return dst_ras;
}

//--------------------------------------------------

bool TRasterFx::isCacheEnabled() const { return m_rasFxImp->m_cacheEnabled; }

//--------------------------------------------------

void TRasterFx::enableCache(bool on) { m_rasFxImp->enableCache(on); }

//--------------------------------------------------

bool TRasterFx::canComputeInFloat() const {
  return m_rasFxImp->canComputeInFloat();
}

//--------------------------------------------------

void TRasterFx::enableComputeInFloat(bool on) {
  m_rasFxImp->enableComputeInFloat(on);
}

//==============================================================================
//
// TRenderSettings
//
//------------------------------------------------------------------------------

TRenderSettings::TRenderSettings()
    : m_gamma(1)
    , m_timeStretchFrom(25)
    , m_timeStretchTo(25)
    , m_stereoscopicShift(0.05)
    , m_bpp(32)
    , m_maxTileSize((std::numeric_limits<int>::max)())
    , m_shrinkX(1)
    , m_shrinkY(1)
    , m_quality(StandardResampleQuality)
    , m_fieldPrevalence(NoField)
    , m_stereoscopic(false)
    , m_isSwatch(false)
    , m_applyShrinkToViewer(false)
    , m_userCachable(true)
    , m_isCanceled(NULL)
    , m_getFullSizeBBox(false)
    , m_linearColorSpace(false)
    , m_colorSpaceGamma(2.2) {}

//------------------------------------------------------------------------------

TRenderSettings::~TRenderSettings() {}

//------------------------------------------------------------------------------

std::string TRenderSettings::toString() const {
  std::string ss =
      std::to_string(m_bpp) + ";" + std::to_string(m_quality) + ";" +
      std::to_string(m_gamma) + ";" + std::to_string(m_timeStretchFrom) + ";" +
      std::to_string(m_timeStretchTo) + ";" +
      std::to_string(m_fieldPrevalence) + ";" + std::to_string(m_shrinkX) +
      "," + std::to_string(m_shrinkY) + ";" + std::to_string(m_affine.a11) +
      "," + std::to_string(m_affine.a12) + "," + std::to_string(m_affine.a13) +
      "," + std::to_string(m_affine.a21) + "," + std::to_string(m_affine.a22) +
      "," + std::to_string(m_affine.a23) + ";" + std::to_string(m_maxTileSize) +
      ";" + std::to_string(m_isSwatch) + ";" + std::to_string(m_userCachable) +
      ";" + std::to_string(m_linearColorSpace) + ";" +
      std::to_string(m_colorSpaceGamma) + ";{";
  if (!m_data.empty()) {
    ss += m_data[0]->toString();
    for (int i = 1; i < (int)m_data.size(); i++)
      ss += "," + m_data[i]->toString();
  }
  ss += "}";
  return ss;
}

//------------------------------------------------------------------------------

bool TRenderSettings::operator==(const TRenderSettings &rhs) const {
  if (m_bpp != rhs.m_bpp || m_quality != rhs.m_quality ||
      m_fieldPrevalence != rhs.m_fieldPrevalence ||
      m_stereoscopic != rhs.m_stereoscopic ||
      m_stereoscopicShift != rhs.m_stereoscopicShift ||
      m_gamma != rhs.m_gamma || m_timeStretchFrom != rhs.m_timeStretchFrom ||
      m_timeStretchTo != rhs.m_timeStretchTo || m_shrinkX != rhs.m_shrinkX ||
      m_shrinkY != rhs.m_shrinkY ||
      m_applyShrinkToViewer != rhs.m_applyShrinkToViewer ||
      m_maxTileSize != rhs.m_maxTileSize || m_affine != rhs.m_affine ||
      m_mark != rhs.m_mark || m_isSwatch != rhs.m_isSwatch ||
      m_userCachable != rhs.m_userCachable ||
      m_linearColorSpace != rhs.m_linearColorSpace ||
      m_colorSpaceGamma != rhs.m_colorSpaceGamma)
    return false;

  return std::equal(m_data.begin(), m_data.end(), rhs.m_data.begin(), areEqual);
}

//------------------------------------------------------------------------------

bool TRenderSettings::operator!=(const TRenderSettings &rhs) const {
  return !operator==(rhs);
}
