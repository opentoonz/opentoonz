#pragma once

#ifndef TVECTORRENDERDATA_H
#define TVECTORRENDERDATA_H

// TnzCore includes
#include "tgeometry.h"
#include "tpixel.h"

#undef DVAPI
#undef DVVAR
#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//========================================================================

//    Forward declarations

class TPalette;
class TColorFunction;

//========================================================================

//**********************************************************************************
//    TVectorRenderData  definition
//**********************************************************************************

/*!
  \brief    Stores parameters for rendering vector images in Toonz.
*/

class DVAPI TVectorRenderData {
public:
  /*! \brief  Tag representing default settings for work-quality rendering
        to be displayed on Toonz widgets.                                     */

  struct ViewerSettings {};

  /*! \brief  Tag representing default settings for production-quality
        rendering to be stored on image files.                                */

  struct ProductionSettings {};

public:
  const TColorFunction
      *m_cf;  //!< [\p not-owned] Transform to be used for drawing RGBM colors.
  const TColorFunction *m_guidedCf;
  const TPalette *m_palette;  //!< [\p not-owned] Palette to be used for
                              //! translating color indexes to
  //!                 RGBM colors.
  TAffine m_aff;  //!< Geometric transform to be applied on image rendering.

  TRect m_clippingRect;  //!< Integral rect where the image drawing will be
                         //! restricted on;
  //!  if empty, clipping is assumed to be disabled.
  //!  \internal  Coordinates reference?
  TPixel m_tCheckInk;    //!< Color to be used for <I>ink check</I> mode.
  TPixel m_tCheckPaint;  //!< Color to be used for <I>paint check</I> mode.

  int m_colorCheckIndex;   //!< Color index to be highlighted in <I>color
                           //! check</I> mode.
  int m_indexToHighlight;  // for guided vector drawing

  bool m_alphaChannel,      //!< Whether alpha channel is enabled.
      m_antiAliasing,       //!< Whether antialiasing must be applied.
      m_isImagePattern,     //!< \internal  Seems like a bad bug-patch - inquire
                            //! further...
      m_drawRegions,        //!< Inks only mode.
      m_tcheckEnabled,      //!< Transparency check mode.
      m_inkCheckEnabled,    //!< Ink check mode.
      m_paintCheckEnabled,  //!< Paint check mode.
      m_blackBgEnabled,     //!< Black background mode.
      m_isIcon,             //!< Whether image rendering is for an icon.
      m_is3dView,           //!< Whether image rendering is in 3D mode.
      m_show0ThickStrokes,  //!< Whether strokes with 0 thickness should be
                            //! rendered anyway.
      m_regionAntialias,    //!< Whether regions should be rendered with
                            //! antialiasing at boundaries.
      m_isOfflineRender,    //!< Whether image rendering is in render or
                            //! camera-stand (preview) mode.
      m_showGuidedDrawing,  // Whether this image is being used for guided
                            // drawing
      m_highLightNow;       // Show highlight on active stroke
  bool m_animatedGuidedDrawing = false;
  //!  \deprecated  Use the above individual options instead.
  //!  \todo  Remove it ASAP.
public:
  TVectorRenderData(ViewerSettings, const TAffine &aff,
                    const TRect &clippingRect, const TPalette *palette,
                    const TColorFunction *cf = 0)
      : m_cf(cf)
      , m_palette(palette)
      , m_aff(aff)
      , m_clippingRect(clippingRect)
      , m_tCheckInk(TPixel::Black)
      , m_tCheckPaint(TPixel(128, 128, 128))
      , m_colorCheckIndex(-1)
      , m_alphaChannel(
            false)  // Camera stand-like widgets have opaque background
      , m_antiAliasing(
            true)  // Jaggy-free pretty images, antialias is not that costly
      , m_isImagePattern(false)  // Patch variable - internal use
      , m_drawRegions(true)      // Paint regions painted
      , m_tcheckEnabled(false)   // No checks
      , m_inkCheckEnabled(false)
      , m_paintCheckEnabled(false)
      , m_blackBgEnabled(false)
      , m_isIcon(false)    // Not an icon by default
      , m_is3dView(false)  // Standard view by default
      , m_show0ThickStrokes(
            true)                 // Wanna see every stroke, even invisible ones
      , m_regionAntialias(false)  // No need for pretty region boundaries,
                                  // typically shadowed by strokes
      // This is also related to interference with the directly above param
      , m_isOfflineRender(false)  // By definition
      , m_indexToHighlight(-1)
      , m_highLightNow(false)
      , m_guidedCf(0)
      , m_showGuidedDrawing(false) {}

  TVectorRenderData(ProductionSettings, const TAffine &aff,
                    const TRect &clippingRect, const TPalette *palette,
                    const TColorFunction *cf = 0)
      : m_cf(cf)
      , m_palette(palette)
      , m_aff(aff)
      , m_clippingRect(clippingRect)
      , m_tCheckInk(TPixel::Black)
      , m_tCheckPaint(TPixel(128, 128, 128))
      , m_colorCheckIndex(-1)
      , m_alphaChannel(
            true)  // No opaque background - freestanding images with alpha
      , m_antiAliasing(true)     // Jaggy-free pretty images
      , m_isImagePattern(false)  // Patch variable - internal use
      , m_drawRegions(true)      // Paint regions painted
      , m_tcheckEnabled(false)   // No checks
      , m_inkCheckEnabled(false)
      , m_paintCheckEnabled(false)
      , m_blackBgEnabled(false)
      , m_isIcon(false)             // Not an icon by default
      , m_is3dView(false)           // Definitely standard view
      , m_show0ThickStrokes(false)  // Invisible strokes must be invisible
      , m_regionAntialias(true)  // Pretty region boundaries under invisible or
                                 // semitransparent strokes
      , m_isOfflineRender(true)  // By definition
      , m_indexToHighlight(-1)
      , m_highLightNow(false)
      , m_guidedCf(0)
      , m_showGuidedDrawing(false) {}

  TVectorRenderData(const TVectorRenderData &other, const TAffine &aff,
                    const TRect &clippingRect, const TPalette *palette,
                    const TColorFunction *cf = 0)
      : m_cf(cf)
      , m_palette(palette)
      , m_aff(aff)
      , m_clippingRect(clippingRect)
      , m_tCheckInk(other.m_tCheckInk)
      , m_tCheckPaint(other.m_tCheckPaint)
      , m_colorCheckIndex(other.m_colorCheckIndex)
      , m_alphaChannel(other.m_alphaChannel)
      , m_antiAliasing(other.m_antiAliasing)
      , m_isImagePattern(other.m_isImagePattern)
      , m_drawRegions(other.m_drawRegions)
      , m_tcheckEnabled(other.m_tcheckEnabled)
      , m_inkCheckEnabled(other.m_inkCheckEnabled)
      , m_paintCheckEnabled(other.m_paintCheckEnabled)
      , m_blackBgEnabled(other.m_blackBgEnabled)
      , m_isIcon(other.m_isIcon)
      , m_is3dView(other.m_is3dView)
      , m_show0ThickStrokes(other.m_show0ThickStrokes)
      , m_regionAntialias(other.m_regionAntialias)
      , m_isOfflineRender(other.m_isOfflineRender)
      , m_indexToHighlight(other.m_indexToHighlight)
      , m_highLightNow(other.m_highLightNow)
      , m_guidedCf(other.m_guidedCf)
      , m_showGuidedDrawing(other.m_showGuidedDrawing) {
  }  //!< Constructs from explicit primary context settings while
     //!  copying the rest from another instance.

  TVectorRenderData(const TAffine &aff, const TRect &clippingRect,
                    const TPalette *palette, const TColorFunction *cf,
                    bool alphaChannel = false, bool antiAliasing = true,
                    bool is3dView = false)
      : m_cf(cf)
      , m_palette(palette)
      , m_aff(aff)
      , m_clippingRect(clippingRect)
      , m_tCheckInk(TPixel::Black)
      , m_tCheckPaint(TPixel(128, 128, 128))
      , m_colorCheckIndex(-1)
      , m_alphaChannel(alphaChannel)
      , m_antiAliasing(antiAliasing)
      , m_isImagePattern(false)
      , m_drawRegions(true)
      , m_tcheckEnabled(false)
      , m_inkCheckEnabled(false)
      , m_paintCheckEnabled(false)
      , m_blackBgEnabled(false)
      , m_isIcon(false)
      , m_is3dView(is3dView)
      , m_show0ThickStrokes(true)
      , m_regionAntialias(false)
      , m_isOfflineRender(false)
      , m_indexToHighlight(-1)
      , m_highLightNow(false)
      , m_guidedCf(0)
      , m_showGuidedDrawing(false) {
  }  //!< Constructs settings with default ViewerSettings.
     //!  \deprecated   Use constructors with explicit settings type tag.
};

#endif  // TVECTORRENDERDATA_H
