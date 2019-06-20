#pragma once

#ifndef STAGEVISITOR_H
#define STAGEVISITOR_H

// TnzCore includes
#include "timage.h"
#include "trastercm.h"
#include "tgl.h"

// TnzExt includes
#include "ext/plasticvisualsettings.h"

// TnzLib includes
#include "toonz/imagepainter.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//    Forward declarations

class ToonzScene;
class TXsheet;
class TXshSimpleLevel;
class TXshLevel;
class TFrameId;
class TFlash;
class OnionSkinMask;
class TFx;
class TXshColumn;
class TVectorImage;
class TRasterImage;
class TToonzImage;
class TMeshImage;
class QPainter;
class QPolygon;
class QMatrix;

namespace Stage {
class Player;
}

//=============================================================================

namespace Stage {

DVVAR extern const double inch;

//**********************************************************************************************
//    Visitor  declaration
//**********************************************************************************************

//! Stage::Visitor is the abstract class model related to <I> scene visiting
//! <\I>.
/*!
  Iterating a scene to perform some task on its content is a typical task in
Toonz.
  Scene visiting is here intended as an implementation of the <I>
visitor-visitable <\I>
  pattern applied to Toonz scenes, where the \a visitable objects are of type
Stage::Player,
  and visitors are reimplemented from the Stage::Visitor class.
\n\n
  The Stage::Player class is the rough equivalent of a level at a specific scene
frame (the term
  \a Player being more legacy rather than having a definite sense I guess),
which in particular
  can be seen as an \a image occurring in scene compositing. Stage::Player
actually have a lot
  more insights about onion skinning, image placement, and other stuff - be sure
to check it
  thorough.
\n\n
  When visiting a scene, the Players are supplied to Stage::Visitor instances
sorted in compositing
  order - so e.g. the first Player is intended \a below the last one.

\sa The Stage::Player class and the global visit() functions.
*/

class DVAPI Visitor {
public:
  const ImagePainter::VisualSettings
      &m_vs;  //!< Generic painting visual options

public:
  Visitor(const ImagePainter::VisualSettings &vs) : m_vs(vs) {}
  virtual ~Visitor() {}

  virtual void onImage(
      const Player &player) = 0;  //!< The \a visitation function.

  // I've not checked the actual meaning of the methods below. They are unused
  // in Toonz, but *are*
  // used in Toonz derivative works such as Tab or LineTest. They deal with
  // OpenGL stencil buffer.

  virtual void enableMask()  = 0;
  virtual void disableMask() = 0;

  virtual void beginMask() = 0;
  virtual void endMask()   = 0;
};

//**********************************************************************************************
//    Visitation  functions
//**********************************************************************************************

// This class (sadly?) disappears from Stage::Visitor's visibility - it's just a
// visit() option.

struct DVAPI VisitArgs {
  ToonzScene *m_scene;
  TXsheet *m_xsh;
  int m_row;
  int m_col;

  const OnionSkinMask *m_osm;
  int m_xsheetLevel;
  TFrameId m_currentFrameId;
  bool m_camera3d;
  bool m_isPlaying;
  bool m_onlyVisible;
  bool m_checkPreviewVisibility;
  bool m_rasterizePli;
  int m_isGuidedDrawingEnabled;

public:
  VisitArgs()
      : m_scene(0)
      , m_xsh(0)
      , m_row(0)
      , m_col(0)
      , m_osm(0)
      , m_xsheetLevel(0)
      , m_camera3d(false)
      , m_isPlaying(false)
      , m_onlyVisible(false)
      , m_checkPreviewVisibility(false)
      , m_isGuidedDrawingEnabled(0)
      , m_rasterizePli(false) {}
};

//=============================================================================

DVAPI void visit(Visitor &visitor, const VisitArgs &args);

//-----------------------------------------------------------------------------

DVAPI void visit(Visitor &visitor, ToonzScene *scene, TXsheet *xsh, int row);

//-----------------------------------------------------------------------------

DVAPI void visit(Visitor &visitor, TXshSimpleLevel *level, const TFrameId &fid,
                 const OnionSkinMask &osm, bool isPlaying);

//-----------------------------------------------------------------------------

DVAPI void visit(Visitor &visitor, TXshLevel *level, const TFrameId &fid,
                 const OnionSkinMask &osm, bool isPlaying);

//**********************************************************************************************
//    Specific Visitor  declarations
//**********************************************************************************************

//! Stage::RasterPainter is the object responsible for drawing scene contents on
//! a standard
//! Toonz SceneViewer panel.
/*!
  This class performs standard <I> camera-stand <\I> scene renderization.
  It renders every supported Toonz image type (including Plastic's texturized
  mesh-based
  image deformation), deals with onion-skin, and other stuff.

  \warning The RasterPainter visitor expects an active OpenGL context.
*/

class DVAPI RasterPainter final : public Visitor {
private:
  //! Class used to deal with composition of raster images. A Node instance
  //! represents a
  //! raster image to be flushed on VRAM. Observe that at the moment raster
  //! composition
  //! is on the RAM side - the composed product is then flushed entirely.
  struct Node {
    // NOTE: This class should be considered obsolete. In theory, each onImage()
    // should be
    // able to write on top of the composition on its own.

    enum OnionMode { eOnionSkinNone, eOnionSkinFront, eOnionSkinBack };

    TRasterP m_raster;    //!< The raster to be rendered
    TPalette *m_palette;  //!< Its palette for colormap rasters
    TAffine m_aff;        //!< Affine which must transform the raster
    TRectD m_bbox;        //!< The clipping rect (on output)
    TRect m_savebox;      //!< The clipping rect (on input)
    int m_alpha;          //!< The node's transparency (I guess up to 255)
    OnionMode m_onionMode;
    int m_frame;
    bool m_isCurrentColumn;
    bool m_isFirstColumn;
    bool m_doPremultiply;  //!< Whether the image must be premultiplied
    bool m_whiteTransp;    //!< Whether white must be intended as transparent

    TPixel32 m_filterColor;

  public:
    Node(const TRasterP &raster, TPalette *palette, int alpha,
         const TAffine &aff, const TRect &savebox, const TRectD &bbox,
         int frame, bool isCurrentColumn, OnionMode onionMode,
         bool doPremultiply, bool whiteTransp, bool isFirstColumn,
         TPixel32 filterColor = TPixel32::Black)
        : m_raster(raster)
        , m_aff(aff)
        , m_savebox(savebox)
        , m_bbox(bbox)
        , m_palette(palette)
        , m_alpha(alpha)
        , m_frame(frame)
        , m_isCurrentColumn(isCurrentColumn)
        , m_onionMode(onionMode)
        , m_doPremultiply(doPremultiply)
        , m_whiteTransp(whiteTransp)
        , m_isFirstColumn(isFirstColumn)
        , m_filterColor(filterColor) {}
  };

  struct VisualizationOptions {
    bool m_singleColumnEnabled;  //!< Whether only current column should be
                                 //! rendered
    bool m_checkFlags;           //   ... o.o? ....
  };

private:
  TDimension m_dim;   //!< Size of the associated OpenGL context to render on
  TRect m_clipRect;   //!< Clipping rect on the OpenGL context
  TAffine m_viewAff;  //!< Affine each image must be transformed through
  std::vector<Node> m_nodes;  //!< Stack of raster images to be flushed to VRAM
  int m_maskLevel;
  bool m_singleColumnEnabled;
  bool m_checkFlags;

  // darken blended view mode for viewing the non-cleanuped and stacked drawings
  bool m_doRasterDarkenBlendedView;

  std::vector<TStroke *> m_guidedStrokes;

public:
  RasterPainter(const TDimension &dim, const TAffine &viewAff,
                const TRect &rect, const ImagePainter::VisualSettings &vs,
                bool checkFlags);

  void onImage(const Stage::Player &data) override;
  void onVectorImage(TVectorImage *vi, const Stage::Player &data);
  void onRasterImage(TRasterImage *ri, const Stage::Player &data);
  void onToonzImage(TToonzImage *ri, const Stage::Player &data);

  void beginMask() override;
  void endMask() override;
  void enableMask() override;
  void disableMask() override;

  int getNodesCount();
  void clearNodes();

  TRasterP getRaster(int index, QMatrix &matrix);

  void flushRasterImages();
  void drawRasterImages(QPainter &p, QPolygon cameraRect);

  void enableSingleColumn(bool on) { m_singleColumnEnabled = on; }
  bool isSingleColumnEnabled() const { return m_singleColumnEnabled; }

  void setRasterDarkenBlendedView(bool on) { m_doRasterDarkenBlendedView = on; }

  std::vector<TStroke *> &getGuidedStrokes() { return m_guidedStrokes; }
};

//=============================================================================

class DVAPI Picker final : public Visitor {
  std::vector<int> m_columnIndexes, m_rows;
  TPointD m_point;
  TAffine m_viewAff;
  double m_minDist2;

  int m_currentColumnIndex = -1;

public:
  Picker(const TAffine &viewAff, const TPointD &p,
         const ImagePainter::VisualSettings &vs);

  void setDistance(double d);

  void onImage(const Stage::Player &data) override;
  void beginMask() override;
  void endMask() override;
  void enableMask() override;
  void disableMask() override;

  int getColumnIndex() const;
  void getColumnIndexes(std::vector<int> &indexes) const;
  int getRow() const;

  void setCurrentColumnIndex(int index) { m_currentColumnIndex = index; }
};

//=============================================================================

//! The derived Stage::Visitor responsible for camera-stand rendering in 3D mode

class DVAPI OpenGlPainter final : public Visitor  // Yep, the name sucks...
{
  TAffine m_viewAff;
  TRect m_clipRect;
  bool m_camera3d;
  double m_phi;
  int m_maskLevel;
  bool m_isViewer, m_alphaEnabled, m_paletteHasChanged;
  double m_minZ;

public:
  OpenGlPainter(const TAffine &viewAff, const TRect &rect,
                const ImagePainter::VisualSettings &vs, bool isViewer,
                bool isAlphaEnabled);

  bool isViewer() const { return m_isViewer; }
  void enableCamera3D(bool on) { m_camera3d = on; }
  void setPhi(double phi) { m_phi = phi; }
  void setPaletteHasChanged(bool on) { m_paletteHasChanged = on; }

  void onImage(const Stage::Player &data) override;
  void onVectorImage(TVectorImage *vi, const Stage::Player &data);
  void onRasterImage(TRasterImage *ri, const Stage::Player &data);
  void onToonzImage(TToonzImage *ti, const Stage::Player &data);

  void beginMask() override;
  void endMask() override;
  void enableMask() override;
  void disableMask() override;

  double getMinZ() const { return m_minZ; }
};

}  // namespace Stage

#endif  // STAGEVISITOR_H
