#pragma once

#ifndef _TRASTERFX_
#define _TRASTERFX_

// TnzCore includes
#include "tnotanimatableparam.h"

// TnzBase includes
#include "tfx.h"
#include "trasterfxrenderdata.h"
#include <QOffscreenSurface>

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

//===================================================================

//  Forward declarations

class TFlash;
class TPalette;

//===================================================================

//******************************************************************************
//    TRenderSettings  declaration
//******************************************************************************

/*!
  \brief    Data class containing the relevant additional informations needed in
            a render process.

  \details  This class stores most of the data required by Toonz and single fxs
  to
            perform a render. While a good part of this data group is used
  internally
            by Toonz to deal with global aspects of a rendering process, another
  part
            must be actively referenced when rendering an fx.

            The most important member of this class is \p m_affine, which
  specifies
            the affine transform that must be applied by an fx if it were to
  render
            on the default reference.

  \sa       TRasterFx::compute() for further informations.
*/

class DVAPI TRenderSettings {
public:
  //! Specifies the filter quality applied on affine transforms.
  enum ResampleQuality {
    HighResampleQuality =
        1,  //!< Use TRop::Hammin3 filter  - pixel blur factor: 3.
    ImprovedResampleQuality =
        2,  //!< Use TRop::Hann2 filter    - pixel blur factor: 2.
    StandardResampleQuality =
        3,  //!< Use TRop::Triangle filter - pixel blur factor: 1.
    Triangle_FilterResampleQuality,
    Mitchell_FilterResampleQuality,
    Cubic5_FilterResampleQuality,
    Cubic75_FilterResampleQuality,
    Cubic1_FilterResampleQuality,
    Hann2_FilterResampleQuality,
    Hann3_FilterResampleQuality,
    Hamming2_FilterResampleQuality,
    Hamming3_FilterResampleQuality,
    Lanczos2_FilterResampleQuality,
    Lanczos3_FilterResampleQuality,
    Gauss_FilterResampleQuality,
    ClosestPixel_FilterResampleQuality,
    Bilinear_FilterResampleQuality
  };

  //! Specifies the field prevalence of a rendering request.
  enum FieldPrevalence {
    NoField,    //!< Common rendering.
    EvenField,  //!< Field rendering, even frame.
    OddField    //!< Field rendering, odd frame.
  };

public:
  // Note - fields are sorted by decreasing size. This ensures minimal padding.

  TAffine m_affine;  //!< Affine that \a should be applied \a after the fx has
                     //! been rendered.
  //!  \sa TRasterFx::compute().
  std::vector<TRasterFxRenderDataP>
      m_data;  //!< Fx-specific data inserted by upstream fxs.

  TRasterP m_mark;  //!< Raster image that is overed on rendered frames in
                    //!  demo versions of Toonz

  double m_gamma;  //!< Gamma modifier for the output frame. \note Should be
                   //! moved to TOutputProperties.
  double m_timeStretchFrom,  //!< Fps source stretch variable. \note Should be
                             //! moved to TOutputProperties.
      m_timeStretchTo;  //!< Fps destination stretch variable. \note Should be
                        //! moved to TOutputProperties.
  double m_stereoscopicShift;  //!< X-axis camera shift for stereoscopy, in
                               //! inches. \sa m_stereoscopic. \note Should be
  //! moved to TOutputProperties.

  int m_bpp;  //!< Bits-per-pixel required in the output frame. \remark This
              //! data
  //!  must be accompanied by a tile of the suitable type. \sa
  //!  TRasterFx::compute().
  int m_maxTileSize;  //!< Maximum size (in MegaBytes) of a tile cachable during
                      //! a render process.
  //!  Used by the predictive cache manager to subdivide an fx calculation into
  //!  tiles. \sa TRasterFx::compute().
  int m_shrinkX,  //!< Required horizontal shrink. \warning Obsolete, do not
                  //! use. \todo Must be removed.
      m_shrinkY;  //!< Required vertical shrink. \warning Obsolete, do not use.
                  //!\todo Must be removed.

  ResampleQuality m_quality;  //!< Filter quality used on affine transforms.
  FieldPrevalence
      m_fieldPrevalence;  //!< Field prevalence of the required frame.

  bool m_stereoscopic;  //!< Whether stereoscopic render (3D effect) is in use.
                        //!\note Should be moved to TOutputProperties.
  bool
      m_isSwatch;  //!< Whether this render instance comes from a swatch viewer.
  //!  This info could be used by computationally intensive fxs to
  //!  implement a simplified render during user interactions.
  bool m_userCachable;  //!< Whether the user can manually cache this render
                        //! request. \sa TRasterFx::compute()

  // Toonz-relevant data (used by Toonz, fx writers should *IGNORE* them while
  // rendering a single fx)
  // NOTE: The signed options should to be moved in TOutputProperties class.

  bool m_applyShrinkToViewer;  //!< Whether shrink must be considered.   \note
                               //! Should be moved to TOutputProperties.

  /*-- カメラサイズ --*/
  TRectD m_cameraBox;
  /*-- 途中でPreview計算がキャンセルされたときのフラグ --*/
  int *m_isCanceled;

  // pointer to QOffscreenSurface which is created on
  // TRendererImp::startRendering()
  // for offscreen rendering to be done in non-GUI thread.
  // For now it is used only in the plasticDeformerFx.
  std::shared_ptr<QOffscreenSurface> m_offScreenSurface;

public:
  TRenderSettings();
  ~TRenderSettings();

  bool operator==(const TRenderSettings &rhs) const;
  bool operator!=(const TRenderSettings &rhs) const;

  //! Returns a textual description of all data that can affect the result of a
  //! frame computation.
  std::string toString() const;
};

//******************************************************************************
//    TRasterFx  declaration
//******************************************************************************

//! TRasterFx is the base class for any \a renderable Toonz Fx.
/*!
  A standard Toonz Fx is roughly composed of 2 parts: one providing the
necessary
  interface for access by a GUI interface (covered by the TFx class, which this
class
  inherits), the other for the actual computation of the fx - which is the focus
  of the TRasterFx interface.
\n\n
  So, the essential part of this class lies in the compute() and doCompute()
methods that
  are invoked to perform an fx computation. The former method is the publicly
accessed one, while
  the latter is the one that fx writers must reimplement to supply their
rendering code.
\n
  The allocateAndCompute() method can be used to automatically allocate the tile
to be rendered
  right before invoking compute().
\n\n
  The dryCompute() / doDryCompute() methods are required by Toonz to perform
predictive caching
  in order to minimize needless recomputations of the same fx. Fx writers must
reimplement
  doDryCompute() to mimic the behaviour of doCompute() in terms of compute()
invocations for children
  fx nodes. Those compute() invocations must be replaced with dryCompute().
\n\n
  The getBBox() / doGetBBox() methods are used to obtain the geometry of the
maximal output that an
  fx can produce with similar inputs to compute(). Again, doCompute() is the
method to reimplement
  by subclasses. Observe that, unlike doCompute() and doDryCompute(), in this
case the affine supplied
  with the render settings must be \b ignored (see the method itself for further
details).
\n\n
  The canHandle() / handledAffine() methods are used by the compute() function
to separate the part of
  an affine that the doCompute() can apply to the result on its own from the
rest (which is automatically
  applied on the fx result using TRasterFx::applyAffine() ).
  Note that every Toonz fx should be able to handle at least the scale part of
an affine (applying
  TRasterFx::applyAffine() on image enlargements typically hampers image
quality).
\n\n
  Further methods whose reimplementation depends on the fx itself are
getAlias(), which must return
  a string uniquely descripting a rendered object, and getMemoryRequirement() to
declare the amount of
  memory that will be used by the fx.
*/
class DVAPI TRasterFx : public TFx {
  class TRasterFxImp;
  TRasterFxImp *m_rasFxImp;

protected:
  virtual void doCompute(TTile &tile, double frame,
                         const TRenderSettings &settings) = 0;

  virtual void doDryCompute(TRectD &rect, double frame,
                            const TRenderSettings &info);

public:
  TRasterFx();
  ~TRasterFx();

  virtual void compute(TTile &tile, double frame, const TRenderSettings &info);

  virtual void compute(TFlash &flash, int frame);

  void allocateAndCompute(TTile &tile, const TPointD &pos,
                          const TDimension &size, TRasterP templateRas,
                          double frame, const TRenderSettings &info);

  virtual bool doGetBBox(double frame, TRectD &bBox,
                         const TRenderSettings &info) = 0;

  bool getBBox(double frame, TRectD &bBox, const TRenderSettings &info);

  virtual bool isCachable() const { return true; }

  virtual void transform(double frame, int port, const TRectD &rectOnOutput,
                         const TRenderSettings &infoOnOutput,
                         TRectD &rectOnInput, TRenderSettings &infoOnInput);

  virtual bool canHandle(const TRenderSettings &info, double frame) = 0;
  virtual TAffine handledAffine(const TRenderSettings &info, double frame);

  static TRasterP applyAffine(TTile &tileOut, const TTile &tileIn,
                              const TRenderSettings &info);

  // cache interna all'effetto
  void enableCache(bool on);
  bool isCacheEnabled() const;

  // resituisce una stringa che identifica univocamente il sottoalbero
  // avente come radice l'effetto
  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;

  virtual void dryCompute(TRectD &rect, double frame,
                          const TRenderSettings &info);

  virtual int getMemoryRequirement(const TRectD &rect, double frame,
                                   const TRenderSettings &info);

  //! Returns the size (MB) of a tile with passed specs. It can be used to
  //! supply the
  //! TRasterFx::getMemoryRequirement info. If rect is empty, returns 0.
  static int memorySize(const TRectD &rect, int bpp);

  virtual bool allowUserCacheOnPort(int port) { return true; }

  virtual bool isPlugin() const { return false; };

private:
  friend class FxResourceBuilder;
};

class TZeraryColumnFx;

class TPluginInterface {
public:
  virtual bool isPlugin() const { return false; };
  virtual bool isPluginZerary() const { return false; };
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TRasterFx>;
template class DVAPI TDerivedSmartPointerT<TRasterFx, TFx>;
#endif

//-------------------------------------------------------------------

class DVAPI TRasterFxP final : public TDerivedSmartPointerT<TRasterFx, TFx> {
public:
  TRasterFxP() {}
  TRasterFxP(TRasterFx *fx) : DerivedSmartPointer(fx) {}
  TRasterFxP(TFx *fx) : DerivedSmartPointer(TFxP(fx)) {}
  TRasterFxP(TFxP fx) : DerivedSmartPointer(fx) {}
  operator TFxP() { return TFxP(m_pointer); }
};

//===================================================================

#ifdef _WIN32
template class DVAPI TFxPortT<TRasterFx>;
#endif
typedef TFxPortT<TRasterFx> TRasterFxPort;

//******************************************************************************
//    TGeometryFx  declaration
//******************************************************************************

class DVAPI TGeometryFx : public TRasterFx {
public:
  TGeometryFx();

  virtual TAffine getPlacement(double frame)       = 0;
  virtual TAffine getParentPlacement(double frame) = 0;

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override;
  void compute(TFlash &flash, int frame) override;

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override;

  virtual bool checkTimeRegion() const { return false; }

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TGeometryFx>;
template class DVAPI TDerivedSmartPointerT<TGeometryFx, TFx>;
#endif

//-------------------------------------------------------------------

class DVAPI TGeometryFxP final
    : public TDerivedSmartPointerT<TGeometryFx, TFx> {
public:
  TGeometryFxP() {}
  TGeometryFxP(TGeometryFx *fx) : DerivedSmartPointer(fx) {}
  TGeometryFxP(TFx *fx) : DerivedSmartPointer(fx) {}
  TGeometryFxP(TFxP fx) : DerivedSmartPointer(fx) {}
  TGeometryFxP(TRasterFx *fx) : DerivedSmartPointer(TRasterFxP(fx)) {}
  TGeometryFxP(TRasterFxP fx) : DerivedSmartPointer(fx) {}

  operator TRasterFxP() { return TRasterFxP(m_pointer); }
};

//===================================================================

#ifdef _WIN32
template class DVAPI TFxPortT<TGeometryFx>;
#endif

class DVAPI TGeometryPort final : public TFxPortT<TGeometryFx> {
public:
  TGeometryPort() : TFxPortT<TGeometryFx>(true) {}
  TAffine getPlacement(double frame) { return m_fx->getPlacement(frame); }
};

//******************************************************************************
//    NaAffineFx  declaration
//******************************************************************************

class DVAPI NaAffineFx final : public TGeometryFx {
  FX_DECLARATION(NaAffineFx)
public:
  ~NaAffineFx() {}
  NaAffineFx(bool isDpiAffine = false);

  TFx *clone(bool recursive) const override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void compute(TFlash &flash, int frame) override;

  TAffine getPlacement(double frame) override { return m_aff; }
  TAffine getParentPlacement(double frame) override { return TAffine(); }

  void setAffine(const TAffine &aff) {
    assert(aff != TAffine());
    m_aff = aff;
  }
  bool isDpiAffine() const { return m_isDpiAffine; }
  std::string getPluginId() const override { return std::string(); }

protected:
  TRasterFxPort m_port;

private:
  TAffine m_aff;
  bool m_isDpiAffine;

  // not implemented
  NaAffineFx(const NaAffineFx &);
  NaAffineFx &operator=(const NaAffineFx &);
};

//******************************************************************************
//    Global functions
//******************************************************************************

void DVAPI addRenderCache(const std::string &alias, TImageP image);
void DVAPI removeRenderCache(const std::string &alias);

//-------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
