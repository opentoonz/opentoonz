

// TnzCore includes
#include "tpixelutils.h"
#include "tstroke.h"
#include "tofflinegl.h"
#include "tstencilcontrol.h"
#include "tvectorgl.h"
#include "tvectorrenderdata.h"
#include "tcolorfunctions.h"
#include "tpalette.h"
#include "tropcm.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tmeshimage.h"
#include "tcolorstyles.h"
#include "timage_io.h"
#include "tregion.h"
#include "toonz/toonzscene.h"

// TnzBase includes
#include "tenv.h"

// TnzExt includes
#include "ext/meshutils.h"
#include "ext/plasticskeleton.h"
#include "ext/plasticskeletondeformation.h"
#include "ext/plasticdeformerstorage.h"

// TnzLib includes
#include "toonz/stageplayer.h"
#include "toonz/stage.h"
#include "toonz/stage2.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txsheet.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/onionskinmask.h"
#include "toonz/dpiscale.h"
#include "toonz/imagemanager.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/glrasterpainter.h"
#include "toonz/preferences.h"
#include "toonz/fill.h"
#include "toonz/levelproperties.h"
#include "toonz/autoclose.h"
#include "toonz/txshleveltypes.h"
#include "imagebuilders.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"

// Qt includes
#include <QImage>
#include <QPainter>
#include <QPolygon>
#include <QThreadStorage>
#include <QMatrix>
#include <QThread>
#include <QGuiApplication>

#include "toonz/stagevisitor.h"

//**********************************************************************************************
//    Stage namespace
//**********************************************************************************************

/*! \namespace Stage
    \brief The Stage namespace provides objects, classes and methods useful to
   view or display images.
*/

using namespace Stage;

/*! \var Stage::inch
                For historical reasons camera stand is defined in a coordinate
   system in which
                an inch is equal to 'Stage::inch' unit.
                Pay attention: modify this value condition apparent line
   thickness of
                images .pli.
*/
// const double Stage::inch = 53.33333;

//**********************************************************************************************
//    Local namespace
//**********************************************************************************************

namespace {

QImage rasterToQImage(const TRaster32P &ras) {
  QImage image(ras->getRawData(), ras->getLx(), ras->getLy(),
               QImage::Format_ARGB32_Premultiplied);
  return image;
}

//----------------------------------------------------------------

QImage rasterToQImage(const TRasterGR8P &ras) {
  QImage image(ras->getLx(), ras->getLy(), QImage::Format_ARGB32_Premultiplied);
  int lx = ras->getLx(), ly = ras->getLy();

  for (int y = 0; y < ly; y++) {
    TPixelGR8 *pix    = ras->pixels(y);
    TPixelGR8 *endPix = pix + lx;
    QRgb *outPix      = (QRgb *)image.scanLine(y);
    for (; pix < endPix; ++pix) {
      int value = pix->value;
      *outPix++ = qRgba(value, value, value, 255);
    }
  }
  return image;
}

//----------------------------------------------------------------

QImage rasterToQImage(const TRasterP &ras) {
  if (TRaster32P src32 = ras)
    return rasterToQImage(src32);
  else if (TRasterGR8P srcGr8 = ras)
    return rasterToQImage(srcGr8);

  // assert(!"Cannot use drawImage with this image!");
  return QImage();
}

//----------------------------------------------------------------

//! Draw orthogonal projection of \b bbox onto x-axis and y-axis.
void draw3DShadow(const TRectD &bbox, double z, double phi) {
  // bruttino assai, ammetto

  double a = bigBoxSize[0];
  double b = bigBoxSize[1];

  glColor3d(0.9, 0.9, 0.86);
  glBegin(GL_LINE_STRIP);
  glVertex3d(bbox.x0, bbox.y0, z);
  glVertex3d(bbox.x0, bbox.y1, z);
  glVertex3d(bbox.x1, bbox.y1, z);
  glVertex3d(bbox.x1, bbox.y0, z);
  glVertex3d(bbox.x0, bbox.y0, z);
  glEnd();

  double y = -b;
  double x = phi >= 0 ? a : -a;

  double xm = 0.5 * (bbox.x0 + bbox.x1);
  double ym = 0.5 * (bbox.y0 + bbox.y1);

  if (bbox.y0 > y) {
    glBegin(GL_LINE_STRIP);
    glVertex3d(xm, y, z);
    glVertex3d(xm, bbox.y0, z);
    glEnd();
  } else if (bbox.y1 < y) {
    glBegin(GL_LINE_STRIP);
    glVertex3d(xm, y, z);
    glVertex3d(xm, bbox.y1, z);
    glEnd();
  }

  if (bbox.x0 > x) {
    glBegin(GL_LINE_STRIP);
    glVertex3d(x, ym, z);
    glVertex3d(bbox.x0, ym, z);
    glEnd();
  } else if (bbox.x1 < x) {
    glBegin(GL_LINE_STRIP);
    glVertex3d(x, ym, z);
    glVertex3d(bbox.x1, ym, z);
    glEnd();
  }

  glColor3d(0.0, 0.0, 0.0);

  glBegin(GL_LINE_STRIP);
  glVertex3d(bbox.x0, -b, z);
  glVertex3d(bbox.x1, -b, z);
  glEnd();

  glBegin(GL_LINE_STRIP);
  glVertex3d(x, bbox.y0, z);
  glVertex3d(x, bbox.y1, z);
  glEnd();
}

//=====================================================================

//  Plastic function declarations

/*!
  Returns from the specified player the stage object to be plastic
  deformed - or 0 if current Toonz rules prevent it from being deformed.
*/
TStageObject *plasticDeformedObj(const Stage::Player &player,
                                 const PlasticVisualSettings &pvs);

//! Draws the specified mesh image
void onMeshImage(TMeshImage *mi, const Stage::Player &player,
                 const ImagePainter::VisualSettings &vs,
                 const TAffine &viewAff);

//! Applies Plastic deformation of the specified player's stage object.
void onPlasticDeformedImage(TStageObject *playerObj,
                            const Stage::Player &player,
                            const ImagePainter::VisualSettings &vs,
                            const TAffine &viewAff);

}  // namespace

//**********************************************************************************************
//    Picker  implementation
//**********************************************************************************************

Picker::Picker(const TAffine &viewAff, const TPointD &point,
               const ImagePainter::VisualSettings &vs)
    : Visitor(vs)
    , m_viewAff(viewAff)
    , m_point(point)
    , m_columnIndexes()
    , m_minDist2(1.0e10) {}

//-----------------------------------------------------------------------------

void Picker::setDistance(double d) { m_minDist2 = d * d; }

//-----------------------------------------------------------------------------

void Picker::onImage(const Stage::Player &player) {
  // if m_currentColumnIndex is other than the default value (-1),
  // then pick only the current column.
  if (m_currentColumnIndex != -1 &&
      m_currentColumnIndex != player.m_ancestorColumnIndex)
    return;

  bool picked   = false;
  TAffine aff   = m_viewAff * player.m_placement;
  TPointD point = aff.inv() * m_point;

  const TImageP &img = player.image();

  if (TVectorImageP vi = img) {
    double w         = 0;
    UINT strokeIndex = 0;
    double dist2     = 0;
    TRegion *r       = vi->getRegion(point);
    int styleId      = 0;
    if (r) styleId   = r->getStyle();
    if (styleId != 0)
      picked = true;
    else if (vi->getNearestStroke(point, w, strokeIndex, dist2) &&
             dist2 < m_minDist2)
      picked = true;
  } else if (TRasterImageP ri = img) {
    TRaster32P ras = ri->getRaster();
    if (!ras) return;

    ras->lock();
    TPointD pp = player.m_dpiAff.inv() * point + ras->getCenterD();
    TPoint p(tround(pp.x), tround(pp.y));
    if (!ras->getBounds().contains(p)) return;

    TPixel32 *pix               = ras->pixels(p.y);
    if (pix[p.x].m != 0) picked = true;

    TAffine aff2 = (aff * player.m_dpiAff).inv();

    TPointD pa(p.x, p.y);
    TPointD dx = aff2 * (m_point + TPointD(3, 0)) - aff2 * m_point;
    TPointD dy = aff2 * (m_point + TPointD(0, 3)) - aff2 * m_point;
    double rx  = dx.x * dx.x + dx.y * dx.y;
    double ry  = dy.x * dy.x + dy.y * dy.y;
    int radius = tround(sqrt(rx > ry ? rx : ry));
    TRect rect = TRect(p.x - radius, p.y - radius, p.x + radius, p.y + radius) *
                 ras->getBounds();
    for (int y = rect.y0; !picked && y <= rect.y1; y++) {
      pix = ras->pixels(y);
      for (int x                  = rect.x0; !picked && x <= rect.x1; x++)
        if (pix[x].m != 0) picked = true;
    }

    ras->unlock();
  } else if (TToonzImageP ti = img) {
    TRasterCM32P ras = ti->getRaster();
    if (!ras) return;

    ras->lock();
    TPointD pp = player.m_dpiAff.inv() * point + ras->getCenterD();
    TPoint p(tround(pp.x), tround(pp.y));
    if (!ras->getBounds().contains(p)) return;

    TPixelCM32 *pix = ras->pixels(p.y) + p.x;
    if (!pix->isPurePaint() || pix->getPaint() != 0) picked = true;

    ras->unlock();
  }

  if (picked) {
    int columnIndex = player.m_ancestorColumnIndex;
    if (m_columnIndexes.empty() || m_columnIndexes.back() != columnIndex)
      m_columnIndexes.push_back(columnIndex);

    int row = player.m_frame;
    if (m_rows.empty() || m_rows.back() != row) m_rows.push_back(row);
  }
}

//-----------------------------------------------------------------------------

void Picker::beginMask() {}

//-----------------------------------------------------------------------------

void Picker::endMask() {}

//-----------------------------------------------------------------------------

void Picker::enableMask() {}

//-----------------------------------------------------------------------------

void Picker::disableMask() {}

//-----------------------------------------------------------------------------

int Picker::getColumnIndex() const {
  if (m_columnIndexes.empty())
    return -1;
  else
    return m_columnIndexes.back();
}

//-----------------------------------------------------------------------------

void Picker::getColumnIndexes(std::vector<int> &indexes) const {
  indexes = m_columnIndexes;
}
//-----------------------------------------------------------------------------

int Picker::getRow() const {
  if (m_rows.empty())
    return -1;
  else
    return m_rows.back();
}

//**********************************************************************************************
//    RasterPainter  implementation
//**********************************************************************************************

RasterPainter::RasterPainter(const TDimension &dim, const TAffine &viewAff,
                             const TRect &rect,
                             const ImagePainter::VisualSettings &vs,
                             bool checkFlags)
    : Visitor(vs)
    , m_dim(dim)
    , m_viewAff(viewAff)
    , m_clipRect(rect)
    , m_maskLevel(0)
    , m_singleColumnEnabled(false)
    , m_checkFlags(checkFlags)
    , m_doRasterDarkenBlendedView(false) {}

//-----------------------------------------------------------------------------

//! Utilizzato solo per TAB Pro
void RasterPainter::beginMask() {
  flushRasterImages();  // per evitare che venga fatto dopo il beginMask
  ++m_maskLevel;
  TStencilControl::instance()->beginMask();
}
//! Utilizzato solo per TAB Pro
void RasterPainter::endMask() {
  flushRasterImages();  // se ci sono delle immagini raster nella maschera
                        // devono uscire ora
  --m_maskLevel;
  TStencilControl::instance()->endMask();
}
//! Utilizzato solo per TAB Pro
void RasterPainter::enableMask() {
  TStencilControl::instance()->enableMask(TStencilControl::SHOW_INSIDE);
}
//! Utilizzato solo per TAB Pro
void RasterPainter::disableMask() {
  flushRasterImages();  // se ci sono delle immagini raster mascherate devono
                        // uscire ora
  TStencilControl::instance()->disableMask();
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

TEnv::DoubleVar AutocloseDistance("InknpaintAutocloseDistance", 10.0);
TEnv::DoubleVar AutocloseAngle("InknpaintAutocloseAngle", 60.0);
TEnv::IntVar AutocloseInk("InknpaintAutocloseInk", 1);
TEnv::IntVar AutocloseOpacity("InknpaintAutocloseOpacity", 255);

//-----------------------------------------------------------------------------

int RasterPainter::getNodesCount() { return m_nodes.size(); }

//-----------------------------------------------------------------------------

void RasterPainter::clearNodes() { m_nodes.clear(); }

//-----------------------------------------------------------------------------

TRasterP RasterPainter::getRaster(int index, QMatrix &matrix) {
  if ((int)m_nodes.size() <= index) return TRasterP();

  if (m_nodes[index].m_onionMode != Node::eOnionSkinNone) return TRasterP();

  if (m_nodes.empty()) return TRasterP();

  double delta = sqrt(fabs(m_nodes[0].m_aff.det()));
  TRectD bbox  = m_nodes[0].m_bbox.enlarge(delta);

  int i;
  for (i = 1; i < (int)m_nodes.size(); i++) {
    delta = sqrt(fabs(m_nodes[i].m_aff.det()));
    bbox += m_nodes[i].m_bbox.enlarge(delta);
  }
  TRect rect(tfloor(bbox.x0), tfloor(bbox.y0), tceil(bbox.x1), tceil(bbox.y1));
  rect = rect * TRect(0, 0, m_dim.lx - 1, m_dim.ly - 1);

  TAffine aff = TTranslation(-rect.x0, -rect.y0) * m_nodes[index].m_aff;
  matrix      = QMatrix(aff.a11, aff.a21, aff.a12, aff.a22, aff.a13, aff.a23);

  return m_nodes[index].m_raster;
}

//-----------------------------------------------------------------------------

/*! Make frame visualization.
\n	If onon-skin is active, create a new raster with dimension containing
all
                frame with onion-skin; recall \b TRop::quickPut with argument
each frame
                with onion-skin and new raster. If onion-skin is not active
recall
                \b TRop::quickPut with argument current frame and new raster.
*/

namespace {
QThreadStorage<std::vector<char> *> threadBuffers;
}

void RasterPainter::flushRasterImages() {
  if (m_nodes.empty()) return;

  // Build nodes bbox union
  double delta = sqrt(fabs(m_nodes[0].m_aff.det()));
  TRectD bbox  = m_nodes[0].m_bbox.enlarge(delta);

  int i, nodesCount = m_nodes.size();
  for (i = 1; i < nodesCount; ++i) {
    delta = sqrt(fabs(m_nodes[i].m_aff.det()));
    bbox += m_nodes[i].m_bbox.enlarge(delta);
  }

  TRect rect(tfloor(bbox.x0), tfloor(bbox.y0), tceil(bbox.x1), tceil(bbox.y1));
  rect = rect * TRect(0, 0, m_dim.lx - 1, m_dim.ly - 1);

  int lx = rect.getLx(), ly = rect.getLy();
  TDimension dim(lx, ly);

  // Build a raster buffer of sufficient size to hold said union.
  // The buffer is per-thread cached in order to improve the rendering speed.
  if (!threadBuffers.hasLocalData())
    threadBuffers.setLocalData(new std::vector<char>());

  int size = dim.lx * dim.ly * sizeof(TPixel32);

  std::vector<char> *vbuff = (std::vector<char> *)threadBuffers.localData();
  if (size > (int)vbuff->size()) vbuff->resize(size);

  TRaster32P ras(dim.lx, dim.ly, dim.lx, (TPixel32 *)&(*vbuff)[0]);
  TRaster32P ras2;

  if (m_vs.m_colorMask != 0) {
    ras2 = TRaster32P(ras->getSize());
    ras->clear();
  } else
    ras2 = ras;

  // Clear the buffer - it will hold all the stacked nodes content to be overed
  // on top of the OpenGL buffer through a glDrawPixel()
  ras->lock();

  ras->clear();  // ras is typically reused - and we need it transparent first

  TRect r                 = rect - rect.getP00();
  TRaster32P viewedRaster = ras->extract(r);

  int current = -1;

  // Retrieve preferences-related data
  int tc    = m_checkFlags ? ToonzCheck::instance()->getChecks() : 0;
  int index = ToonzCheck::instance()->getColorIndex();

  TPixel32 frontOnionColor, backOnionColor;
  bool onionInksOnly;

  Preferences::instance()->getOnionData(frontOnionColor, backOnionColor,
                                        onionInksOnly);

  // Stack every node on top of the raster buffer
  for (i = 0; i < nodesCount; ++i) {
    if (m_nodes[i].m_isCurrentColumn) current = i;

    TAffine aff         = TTranslation(-rect.x0, -rect.y0) * m_nodes[i].m_aff;
    TDimension imageDim = m_nodes[i].m_raster->getSize();
    TPointD offset(0.5, 0.5);
    aff *= TTranslation(offset);  // very quick and very dirty fix: in
                                  // camerastand the images seems shifted of an
                                  // half pixel...it's a quickput approximation?

    TPixel32 colorscale = TPixel32(0, 0, 0, m_nodes[i].m_alpha);
    int inksOnly;

    if (m_nodes[i].m_onionMode != Node::eOnionSkinNone) {
      inksOnly = onionInksOnly;

      if (m_nodes[i].m_onionMode == Node::eOnionSkinFront)
        colorscale = TPixel32(frontOnionColor.r, frontOnionColor.g,
                              frontOnionColor.b, m_nodes[i].m_alpha);
      else if (m_nodes[i].m_onionMode == Node::eOnionSkinBack)
        colorscale = TPixel32(backOnionColor.r, backOnionColor.g,
                              backOnionColor.b, m_nodes[i].m_alpha);
    } else {
      if (m_nodes[i].m_filterColor != TPixel32::Black) {
        colorscale   = m_nodes[i].m_filterColor;
        colorscale.m = m_nodes[i].m_alpha;
      }
      inksOnly = tc & ToonzCheck::eInksOnly;
    }

    if (TRaster32P src32 = m_nodes[i].m_raster)
      TRop::quickPut(viewedRaster, src32, aff, colorscale,
                     m_nodes[i].m_doPremultiply, m_nodes[i].m_whiteTransp,
                     m_nodes[i].m_isFirstColumn, m_doRasterDarkenBlendedView);
    else if (TRasterGR8P srcGr8 = m_nodes[i].m_raster)
      TRop::quickPut(viewedRaster, srcGr8, aff, colorscale);
    else if (TRasterCM32P srcCm = m_nodes[i].m_raster) {
      assert(m_nodes[i].m_palette);
      int oldframe = m_nodes[i].m_palette->getFrame();
      m_nodes[i].m_palette->setFrame(m_nodes[i].m_frame);

      TPaletteP plt;
      if ((tc & ToonzCheck::eGap || tc & ToonzCheck::eAutoclose) &&
          m_nodes[i].m_isCurrentColumn) {
        srcCm          = srcCm->clone();
        plt            = m_nodes[i].m_palette->clone();
        int styleIndex = plt->addStyle(TPixel::Magenta);
        if (tc & ToonzCheck::eAutoclose)
          TAutocloser(srcCm, AutocloseDistance, AutocloseAngle, styleIndex,
                      AutocloseOpacity)
              .exec();
        if (tc & ToonzCheck::eGap)
          AreaFiller(srcCm).rectFill(m_nodes[i].m_savebox, 1, true, true,
                                     false);
      } else
        plt = m_nodes[i].m_palette;

      if (tc == 0 || tc == ToonzCheck::eBlackBg ||
          !m_nodes[i].m_isCurrentColumn)
        TRop::quickPut(viewedRaster, srcCm, plt, aff, colorscale, inksOnly);
      else {
        TRop::CmappedQuickputSettings settings;

        settings.m_globalColorScale = colorscale;
        settings.m_inksOnly         = inksOnly;
        settings.m_transparencyCheck =
            tc & (ToonzCheck::eTransparency | ToonzCheck::eGap);
        settings.m_blackBgCheck = tc & ToonzCheck::eBlackBg;
        /*-- InkCheck, Ink#1Check, PaintCheckはカレントカラムにのみ有効 --*/
        settings.m_inkIndex =
            m_nodes[i].m_isCurrentColumn
                ? (tc & ToonzCheck::eInk ? index
                                         : (tc & ToonzCheck::eInk1 ? 1 : -1))
                : -1;
        settings.m_paintIndex = m_nodes[i].m_isCurrentColumn
                                    ? (tc & ToonzCheck::ePaint ? index : -1)
                                    : -1;

        Preferences::instance()->getTranspCheckData(
            settings.m_transpCheckBg, settings.m_transpCheckInk,
            settings.m_transpCheckPaint);

        TRop::quickPut(viewedRaster, srcCm, plt, aff, settings);
      }

      srcCm = TRasterCM32P();
      plt   = TPaletteP();

      m_nodes[i].m_palette->setFrame(oldframe);
    } else
      assert(!"Cannot use quickput with this raster combination!");
  }

  if (m_vs.m_colorMask != 0) {
    TRop::setChannel(ras, ras, m_vs.m_colorMask, false);
    TRop::quickPut(ras2, ras, TAffine());
  }

  // Now, output the raster buffer on top of the OpenGL buffer
  glPushAttrib(GL_COLOR_BUFFER_BIT);  // Preserve blending and stuff

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE,
              GL_ONE_MINUS_SRC_ALPHA);  // The raster buffer is intended in
  // premultiplied form - thus the GL_ONE on src
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DITHER);
  glDisable(GL_LOGIC_OP);

/* disable, since these features are never enabled, and cause OpenGL to assert
 * on systems that don't support them: see #591 */
#if 0
#ifdef GL_EXT_convolution
  glDisable(GL_CONVOLUTION_1D_EXT);
  glDisable(GL_CONVOLUTION_2D_EXT);
  glDisable(GL_SEPARABLE_2D_EXT);
#endif

#ifdef GL_EXT_histogram
  glDisable(GL_HISTOGRAM_EXT);
  glDisable(GL_MINMAX_EXT);
#endif
#endif

#ifdef GL_EXT_texture3D
  glDisable(GL_TEXTURE_3D_EXT);
#endif

  glPushMatrix();
  glLoadIdentity();

  glRasterPos2d(rect.x0, rect.y0);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glDrawPixels(ras2->getLx(), ras2->getLy(),            // Perform the over
               TGL_FMT, TGL_TYPE, ras2->getRawData());  //

  ras->unlock();
  glPopMatrix();

  glPopAttrib();  // Restore blending status

  if (m_vs.m_showBBox && current > -1) {
    glPushMatrix();
    glLoadIdentity();
    tglColor(TPixel(200, 200, 200));
    tglMultMatrix(m_nodes[current].m_aff);
    tglDrawRect(m_nodes[current].m_raster->getBounds());
    glPopMatrix();
  }

  m_nodes.clear();
}

//-----------------------------------------------------------------------------
/*! Make frame visualization in QPainter.
\n	Draw in painter mode just raster image in m_nodes.
\n  Onon-skin or channel mode are not considered.
*/
void RasterPainter::drawRasterImages(QPainter &p, QPolygon cameraPol) {
  if (m_nodes.empty()) return;

  double delta = sqrt(fabs(m_nodes[0].m_aff.det()));
  TRectD bbox  = m_nodes[0].m_bbox.enlarge(delta);

  int i;
  for (i = 1; i < (int)m_nodes.size(); i++) {
    delta = sqrt(fabs(m_nodes[i].m_aff.det()));
    bbox += m_nodes[i].m_bbox.enlarge(delta);
  }
  TRect rect(tfloor(bbox.x0), tfloor(bbox.y0), tceil(bbox.x1), tceil(bbox.y1));
  rect = rect * TRect(0, 0, m_dim.lx - 1, m_dim.ly - 1);

  TRect r = rect - rect.getP00();
  TAffine flipY(1, 0, 0, 0, -1, m_dim.ly);
  p.setClipRegion(QRegion(cameraPol));
  for (i = 0; i < (int)m_nodes.size(); i++) {
    if (m_nodes[i].m_onionMode != Node::eOnionSkinNone) continue;
    p.resetMatrix();
    TRasterP ras = m_nodes[i].m_raster;
    TAffine aff  = TTranslation(-rect.x0, -rect.y0) * flipY * m_nodes[i].m_aff;
    QMatrix matrix(aff.a11, aff.a21, aff.a12, aff.a22, aff.a13, aff.a23);
    QImage image = rasterToQImage(ras);
    if (image.isNull()) continue;
    p.setMatrix(matrix);
    p.drawImage(rect.getP00().x, rect.getP00().y, image);
  }

  p.resetMatrix();
  m_nodes.clear();
}

static void buildAutocloseImage(
    TVectorImage *vaux, TVectorImage *vi,
    const std::vector<std::pair<int, double>> &startPoints,
    const std::vector<std::pair<int, double>> &endPoints) {
  for (UINT i = 0; i < startPoints.size(); i++) {
    TThickPoint p1 = vi->getStroke(startPoints[i].first)
                         ->getThickPoint(startPoints[i].second);
    TThickPoint p2 =
        vi->getStroke(endPoints[i].first)->getThickPoint(endPoints[i].second);
    std::vector<TThickPoint> points(3);
    points[0]       = p1;
    points[1]       = 0.5 * (p1 + p2);
    points[2]       = p2;
    points[0].thick = points[1].thick = points[2].thick = 0.0;
    TStroke *auxStroke                                  = new TStroke(points);
    auxStroke->setStyle(2);
    vaux->addStroke(auxStroke);
  }
}

TEnv::DoubleVar AutocloseFactor("InknpaintAutocloseFactor", 4.0);

static void drawAutocloses(TVectorImage *vi, TVectorRenderData &rd) {
  static TPalette *plt = 0;
  if (!plt) {
    plt = new TPalette();
    plt->addStyle(TPixel::Magenta);
  }

  std::vector<std::pair<int, double>> startPoints, endPoints;
  getClosingPoints(vi->getBBox(), AutocloseFactor, vi, startPoints, endPoints);
  TVectorImage *vaux = new TVectorImage();

  rd.m_palette = plt;
  buildAutocloseImage(vaux, vi, startPoints, endPoints);
  tglDraw(rd, vaux);
  delete vaux;
}

//-----------------------------------------------------------------------------

/*! Take image from \b Stage::Player \b data and recall the right method for
                this kind of image, for vector image recall \b onVectorImage(),
   for raster
                image recall \b onRasterImage() for toonz image recall \b
   onToonzImage().
*/
void RasterPainter::onImage(const Stage::Player &player) {
  if (m_singleColumnEnabled && !player.m_isCurrentColumn) return;

  // Attempt Plastic-deformed drawing
  // For now generating icons of plastic-deformed image causes crash as
  // QOffscreenSurface is created outside the gui thread.
  // As a quick workaround, ignore the deformation if this is called from
  // non-gui thread (i.e. icon generator thread)
  // 12/1/2018 Now the scene icon is rendered without deformation either
  // since it causes unknown error with opengl contexts..
  TStageObject *obj =
      ::plasticDeformedObj(player, m_vs.m_plasticVisualSettings);
  if (obj && QThread::currentThread() == qGuiApp->thread() &&
      !m_vs.m_forSceneIcon) {
    flushRasterImages();
    ::onPlasticDeformedImage(obj, player, m_vs, m_viewAff);
  } else {
    // Common image draw
    const TImageP &img = player.image();

    if (TVectorImageP vi = img)
      onVectorImage(vi.getPointer(), player);
    else if (TRasterImageP ri = img)
      onRasterImage(ri.getPointer(), player);
    else if (TToonzImageP ti = img)
      onToonzImage(ti.getPointer(), player);
    else if (TMeshImageP mi = img) {
      flushRasterImages();
      ::onMeshImage(mi.getPointer(), player, m_vs, m_viewAff);
    }
  }
}

//-----------------------------------------------------------------------------
/*! View a vector cell images.
\n	If onion-skin is active compute \b TOnionFader value.
                Create and boot a \b TVectorRenderData and recall \b tglDraw().
*/
void RasterPainter::onVectorImage(TVectorImage *vi,
                                  const Stage::Player &player) {
  flushRasterImages();

  // When loaded, vectorimages needs to have regions recomputed, but doing that
  // while loading them
  // is quite slow (think about loading whole scenes!..). They are recomputed
  // the first time they
  // are selected and shown on screen...except when playing back, to avoid
  // slowness!

  // (Daniele) This function should *NOT* be responsible of that.
  //           It's the *image itself* that should recalculate or initialize
  //           said data
  //           if queried about it and turns out it's not available...

  if (!player.m_isPlaying && player.m_isCurrentColumn)
    vi->recomputeRegionsIfNeeded();

  const Preferences &prefs = *Preferences::instance();

  TColorFunction *cf = 0, *guidedCf = 0;
  TPalette *vPalette = vi->getPalette();
  TPixel32 bgColor   = TPixel32::White;

  int tc = (m_checkFlags && player.m_isCurrentColumn)
               ? ToonzCheck::instance()->getChecks()
               : 0;
  bool inksOnly = tc & ToonzCheck::eInksOnly;

  int oldFrame = vPalette->getFrame();
  vPalette->setFrame(player.m_frame);

  if (player.m_onionSkinDistance != c_noOnionSkin) {
    TPixel32 frontOnionColor, backOnionColor;

    if (player.m_onionSkinDistance != 0 &&
        (!player.m_isShiftAndTraceEnabled ||
         Preferences::instance()->areOnionColorsUsedForShiftAndTraceGhosts())) {
      prefs.getOnionData(frontOnionColor, backOnionColor, inksOnly);
      bgColor =
          (player.m_onionSkinDistance < 0) ? backOnionColor : frontOnionColor;
    }

    double m[4] = {1.0, 1.0, 1.0, 1.0}, c[4];

    // Weighted addition to RGB and matte multiplication
    m[3] = 1.0 -
           ((player.m_onionSkinDistance == 0)
                ? 0.1
                : OnionSkinMask::getOnionSkinFade(player.m_onionSkinDistance));
    c[0] = (1.0 - m[3]) * bgColor.r, c[1] = (1.0 - m[3]) * bgColor.g,
    c[2] = (1.0 - m[3]) * bgColor.b;
    c[3] = 0.0;

    cf = new TGenericColorFunction(m, c);
  } else if (player.m_filterColor != TPixel::Black) {
    TPixel32 colorScale = player.m_filterColor;
    colorScale.m        = player.m_opacity;
    cf                  = new TColumnColorFilterFunction(colorScale);
  } else if (player.m_opacity < 255)
    cf = new TTranspFader(player.m_opacity / 255.0);

  TVectorRenderData rd(m_viewAff * player.m_placement, TRect(), vPalette, cf,
                       true  // alpha enabled
                       );

  rd.m_drawRegions           = !inksOnly;
  rd.m_inkCheckEnabled       = tc & ToonzCheck::eInk;
  rd.m_paintCheckEnabled     = tc & ToonzCheck::ePaint;
  rd.m_blackBgEnabled        = tc & ToonzCheck::eBlackBg;
  rd.m_colorCheckIndex       = ToonzCheck::instance()->getColorIndex();
  rd.m_show0ThickStrokes     = prefs.getShow0ThickLines();
  rd.m_regionAntialias       = prefs.getRegionAntialias();
  rd.m_animatedGuidedDrawing = prefs.getAnimatedGuidedDrawing();
  if (player.m_onionSkinDistance < 0 &&
      (player.m_isCurrentColumn || player.m_isCurrentXsheetLevel)) {
    if (player.m_isGuidedDrawingEnabled == 3         // show guides on all
        || (player.m_isGuidedDrawingEnabled == 1 &&  // show guides on closest
            player.m_onionSkinDistance == player.m_firstBackOnionSkin) ||
        (player.m_isGuidedDrawingEnabled == 2 &&  // show guides on farthest
         player.m_onionSkinDistance == player.m_onionSkinBackSize) ||
        (player.m_isEditingLevel &&  // fix for level editing mode sending extra
                                     // players
         player.m_isGuidedDrawingEnabled == 2 &&
         player.m_onionSkinDistance == player.m_lastBackVisibleSkin)) {
      rd.m_showGuidedDrawing = player.m_isGuidedDrawingEnabled > 0;
      int currentStrokeCount = 0;
      int totalStrokes       = vi->getStrokeCount();
      TXshSimpleLevel *sl    = player.m_sl;

      if (sl) {
        TImageP image          = sl->getFrame(player.m_currentFrameId, false);
        TVectorImageP vecImage = image;
        if (vecImage) currentStrokeCount = vecImage->getStrokeCount();
        if (currentStrokeCount < totalStrokes)
          rd.m_indexToHighlight = currentStrokeCount;

        double guidedM[4] = {1.0, 1.0, 1.0, 1.0}, guidedC[4];
        TPixel32 bgColor  = TPixel32::Blue;
        guidedM[3] =
            1.0 -
            ((player.m_onionSkinDistance == 0)
                 ? 0.1
                 : OnionSkinMask::getOnionSkinFade(player.m_onionSkinDistance));

        guidedC[0] = (1.0 - guidedM[3]) * bgColor.r,
        guidedC[1] = (1.0 - guidedM[3]) * bgColor.g,
        guidedC[2] = (1.0 - guidedM[3]) * bgColor.b;
        guidedC[3] = 0.0;

        guidedCf      = new TGenericColorFunction(guidedM, guidedC);
        rd.m_guidedCf = guidedCf;
      }
    }
  }

  if (tc & (ToonzCheck::eTransparency | ToonzCheck::eGap)) {
    TPixel dummy;
    rd.m_tcheckEnabled = true;

    if (rd.m_blackBgEnabled)
      prefs.getTranspCheckData(rd.m_tCheckInk, dummy, rd.m_tCheckPaint);
    else
      prefs.getTranspCheckData(dummy, rd.m_tCheckInk, rd.m_tCheckPaint);
  }

  if (m_vs.m_colorMask != 0) {
    glColorMask((m_vs.m_colorMask & TRop::RChan) ? GL_TRUE : GL_FALSE,
                (m_vs.m_colorMask & TRop::GChan) ? GL_TRUE : GL_FALSE,
                (m_vs.m_colorMask & TRop::BChan) ? GL_TRUE : GL_FALSE, GL_TRUE);
  }
  TVectorImageP viDelete;
  if (tc & ToonzCheck::eGap) {
    viDelete = vi->clone();
    vi       = viDelete.getPointer();
    vi->selectFill(vi->getBBox(), 0, 1, true, true, false);
  }

  TStroke *guidedStroke = 0;
  if (m_maskLevel > 0)
    tglDrawMask(rd, vi);
  else
    tglDraw(rd, vi, &guidedStroke);

  if (tc & ToonzCheck::eAutoclose) drawAutocloses(vi, rd);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  vPalette->setFrame(oldFrame);

  delete cf;
  delete guidedCf;

  if (guidedStroke) m_guidedStrokes.push_back(guidedStroke);
}

//-----------------------------------------------------

/*! Create a \b Node and put it in \b m_nodes.
*/
void RasterPainter::onRasterImage(TRasterImage *ri,
                                  const Stage::Player &player) {
  TRasterP r = ri->getRaster();

  TAffine aff;
  aff = m_viewAff * player.m_placement * player.m_dpiAff;
  aff = TTranslation(m_dim.lx * 0.5, m_dim.ly * 0.5) * aff *
        TTranslation(-r->getCenterD() +
                     convert(ri->getOffset()));  // this offset is !=0 only if
                                                 // in cleanup camera test mode

  TRectD bbox = TRectD(0, 0, m_dim.lx, m_dim.ly);
  bbox *= convert(m_clipRect);
  if (bbox.isEmpty()) return;

  int alpha                 = 255;
  Node::OnionMode onionMode = Node::eOnionSkinNone;
  if (player.m_onionSkinDistance != c_noOnionSkin) {
    // GetOnionSkinFade va bene per il vettoriale mentre il raster funziona al
    // contrario
    // 1 opaco -> 0 completamente trasparente
    // inverto quindi il risultato della funzione stando attento al caso 0
    // (in cui era scolpito il valore 0.9)
    double onionSkiFade = player.m_onionSkinDistance == 0
                              ? 0.9
                              : (1.0 - OnionSkinMask::getOnionSkinFade(
                                           player.m_onionSkinDistance));
    alpha = tcrop(tround(onionSkiFade * 255.0), 0, 255);
    if (player.m_isShiftAndTraceEnabled &&
        !Preferences::instance()->areOnionColorsUsedForShiftAndTraceGhosts())
      onionMode = Node::eOnionSkinNone;
    else {
      onionMode =
          (player.m_onionSkinDistance > 0)
              ? Node::eOnionSkinFront
              : ((player.m_onionSkinDistance < 0) ? Node::eOnionSkinBack
                                                  : Node::eOnionSkinNone);
    }
  } else if (player.m_opacity < 255)
    alpha             = player.m_opacity;
  TXshSimpleLevel *sl = player.m_sl;
  bool doPremultiply  = false;
  bool whiteTransp    = false;
  if (sl) {
    LevelProperties *levelProp = sl->getProperties();
    if (levelProp->doPremultiply())
      doPremultiply = true;
    else if (levelProp->whiteTransp())
      whiteTransp = true;
  }

  bool ignoreAlpha =
      (Preferences::instance()->isIgnoreAlphaonColumn1Enabled() &&
       player.m_column == 0 &&
       isSubsheetChainOnColumn0(sl->getScene()->getTopXsheet(), player.m_xsh,
                                player.m_frame));

  m_nodes.push_back(Node(r, 0, alpha, aff, ri->getSavebox(), bbox,
                         player.m_frame, player.m_isCurrentColumn, onionMode,
                         doPremultiply, whiteTransp, ignoreAlpha,
                         player.m_filterColor));
}

//-----------------------------------------------------------------------------
/*! Create a \b Node and put it in \b m_nodes.
*/
void RasterPainter::onToonzImage(TToonzImage *ti, const Stage::Player &player) {
  TRasterCM32P r = ti->getRaster();
  if (!ti->getPalette()) return;

  TAffine aff = m_viewAff * player.m_placement * player.m_dpiAff;
  aff         = TTranslation(m_dim.lx / 2.0, m_dim.ly / 2.0) * aff *
        TTranslation(-r->getCenterD());

  TRectD bbox = TRectD(0, 0, m_dim.lx, m_dim.ly);
  bbox *= convert(m_clipRect);
  if (bbox.isEmpty()) return;

  int alpha                 = 255;
  Node::OnionMode onionMode = Node::eOnionSkinNone;
  if (player.m_onionSkinDistance != c_noOnionSkin) {
    // GetOnionSkinFade va bene per il vettoriale mentre il raster funziona al
    // contrario
    // 1 opaco -> 0 completamente trasparente
    // inverto quindi il risultato della funzione stando attento al caso 0
    // (in cui era scolpito il valore 0.9)
    double onionSkiFade = player.m_onionSkinDistance == 0
                              ? 0.9
                              : (1.0 - OnionSkinMask::getOnionSkinFade(
                                           player.m_onionSkinDistance));
    alpha = tcrop(tround(onionSkiFade * 255.0), 0, 255);

    if (player.m_isShiftAndTraceEnabled &&
        !Preferences::instance()->areOnionColorsUsedForShiftAndTraceGhosts())
      onionMode = Node::eOnionSkinNone;
    else {
      onionMode =
          (player.m_onionSkinDistance > 0)
              ? Node::eOnionSkinFront
              : ((player.m_onionSkinDistance < 0) ? Node::eOnionSkinBack
                                                  : Node::eOnionSkinNone);
    }

  } else if (player.m_opacity < 255)
    alpha = player.m_opacity;

  m_nodes.push_back(Node(r, ti->getPalette(), alpha, aff, ti->getSavebox(),
                         bbox, player.m_frame, player.m_isCurrentColumn,
                         onionMode, false, false, false, player.m_filterColor));
}

//**********************************************************************************************
//    OpenGLPainter  implementation
//**********************************************************************************************

OpenGlPainter::OpenGlPainter(const TAffine &viewAff, const TRect &rect,
                             const ImagePainter::VisualSettings &vs,
                             bool isViewer, bool alphaEnabled)
    : Visitor(vs)
    , m_viewAff(viewAff)
    , m_clipRect(rect)
    , m_camera3d(false)
    , m_phi(0)
    , m_maskLevel(0)
    , m_isViewer(isViewer)
    , m_alphaEnabled(alphaEnabled)
    , m_paletteHasChanged(false)
    , m_minZ(0) {}

//-----------------------------------------------------------------------------

void OpenGlPainter::onImage(const Stage::Player &player) {
  if (player.m_z < m_minZ) m_minZ = player.m_z;

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushMatrix();

  if (m_camera3d)
    glTranslated(
        0, 0,
        player.m_z);  // Ok, move object along z as specified in the player

  // Attempt Plastic-deformed drawing
  if (TStageObject *obj =
          ::plasticDeformedObj(player, m_vs.m_plasticVisualSettings))
    ::onPlasticDeformedImage(obj, player, m_vs, m_viewAff);
  else if (const TImageP &image = player.image()) {
    if (TVectorImageP vi = image)
      onVectorImage(vi.getPointer(), player);
    else if (TRasterImageP ri = image)
      onRasterImage(ri.getPointer(), player);
    else if (TToonzImageP ti = image)
      onToonzImage(ti.getPointer(), player);
    else if (TMeshImageP mi = image)
      onMeshImage(mi.getPointer(), player, m_vs, m_viewAff);
  }

  glPopMatrix();
  glPopAttrib();
}

//-----------------------------------------------------------------------------

void OpenGlPainter::onVectorImage(TVectorImage *vi,
                                  const Stage::Player &player) {
  if (m_camera3d && (player.m_onionSkinDistance == c_noOnionSkin ||
                     player.m_onionSkinDistance == 0)) {
    const TRectD &bbox = player.m_placement * player.m_dpiAff * vi->getBBox();
    draw3DShadow(bbox, player.m_z, m_phi);
  }

  TColorFunction *cf = 0;
  TOnionFader fader;

  TPalette *vPalette = vi->getPalette();

  int oldFrame = vPalette->getFrame();
  vPalette->setFrame(player.m_frame);  // Hehe. Should be locking
                                       // vPalette's mutex here...
  if (player.m_onionSkinDistance != c_noOnionSkin) {
    TPixel32 bgColor = TPixel32::White;
    fader            = TOnionFader(
        bgColor, OnionSkinMask::getOnionSkinFade(player.m_onionSkinDistance));
    cf = &fader;
  }

  TVectorRenderData rd =
      isViewer() ? TVectorRenderData(TVectorRenderData::ViewerSettings(),
                                     m_viewAff * player.m_placement, m_clipRect,
                                     vPalette)
                 : TVectorRenderData(TVectorRenderData::ProductionSettings(),
                                     m_viewAff * player.m_placement, m_clipRect,
                                     vPalette);

  rd.m_alphaChannel = m_alphaEnabled;
  rd.m_is3dView     = m_camera3d;

  if (m_maskLevel > 0)
    tglDrawMask(rd, vi);
  else
    tglDraw(rd, vi);

  vPalette->setFrame(oldFrame);
}

//-----------------------------------------------------------------------------

void OpenGlPainter::onRasterImage(TRasterImage *ri,
                                  const Stage::Player &player) {
  if (m_camera3d && (player.m_onionSkinDistance == c_noOnionSkin ||
                     player.m_onionSkinDistance == 0)) {
    TRectD bbox(ri->getBBox());

    // Since bbox is in image coordinates, adjust to level coordinates first
    bbox -= ri->getRaster()->getCenterD() - convert(ri->getOffset());

    // Then, to world coordinates
    bbox = player.m_placement * player.m_dpiAff * bbox;

    draw3DShadow(bbox, player.m_z, m_phi);
  }

  bool tlvFlag = player.m_sl && player.m_sl->getType() == TZP_XSHLEVEL;

  if (tlvFlag &&
      m_paletteHasChanged)  // o.o! Raster images here - should never be
    assert(false);          // dealing with tlv stuff!

  bool premultiplied = tlvFlag;  // xD
  static std::vector<char>
      matteChan;  // Wtf this is criminal. Altough probably this
                  // stuff is used only in the main thread... hmmm....
  TRaster32P r = (TRaster32P)ri->getRaster();
  r->lock();

  if (c_noOnionSkin != player.m_onionSkinDistance) {
    double fade =
        0.5 - 0.45 * (1 - 1 / (1 + 0.15 * abs(player.m_onionSkinDistance)));
    if ((int)matteChan.size() < r->getLx() * r->getLy())
      matteChan.resize(r->getLx() * r->getLy());

    int k = 0;
    for (int y = 0; y < r->getLy(); ++y) {
      TPixel32 *pix    = r->pixels(y);
      TPixel32 *endPix = pix + r->getLx();

      while (pix < endPix) {
        matteChan[k++] = pix->m;
        pix->m         = (int)(pix->m * fade);
        pix++;
      }
    }

    premultiplied = false;  // pfff
  }

  TAffine aff = player.m_dpiAff;

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  GLRasterPainter::drawRaster(m_viewAff * player.m_placement * aff *
                                  TTranslation(convert(ri->getOffset())),
                              ri, premultiplied);

  glPopAttrib();

  if (c_noOnionSkin != player.m_onionSkinDistance) {
    int k = 0;
    for (int y = 0; y < r->getLy(); ++y) {
      TPixel32 *pix    = r->pixels(y);
      TPixel32 *endPix = pix + r->getLx();

      while (pix < endPix) pix++->m = matteChan[k++];
    }
  }

  r->unlock();
}

//-----------------------------------------------------------------------------

void OpenGlPainter::onToonzImage(TToonzImage *ti, const Stage::Player &player) {
  if (m_camera3d && (player.m_onionSkinDistance == c_noOnionSkin ||
                     player.m_onionSkinDistance == 0)) {
    TRectD bbox(ti->getBBox());

    bbox -= ti->getRaster()->getCenterD();
    bbox = player.m_placement * player.m_dpiAff * bbox;

    draw3DShadow(bbox, player.m_z, m_phi);
  }

  TRasterCM32P ras = ti->getRaster();
  TRaster32P ras32(ras->getSize());
  ras32->fill(TPixel32(0, 0, 0, 0));

  // onionSkin
  TRop::quickPut(ras32, ras, ti->getPalette(), TAffine());
  TAffine aff = player.m_dpiAff;

  TRasterImageP ri(ras32);

  GLRasterPainter::drawRaster(m_viewAff * player.m_placement * aff, ri, true);
}

//-----------------------------------------------------------------------------

void OpenGlPainter::beginMask() {
  ++m_maskLevel;
  TStencilControl::instance()->beginMask();
}
void OpenGlPainter::endMask() {
  --m_maskLevel;
  TStencilControl::instance()->endMask();
}
void OpenGlPainter::enableMask() {
  TStencilControl::instance()->enableMask(TStencilControl::SHOW_INSIDE);
}
void OpenGlPainter::disableMask() {
  TStencilControl::instance()->disableMask();
}

//**********************************************************************************************
//    Plastic functions  implementation
//**********************************************************************************************

namespace {

TStageObject *plasticDeformedObj(const Stage::Player &player,
                                 const PlasticVisualSettings &pvs) {
  struct locals {
    static inline bool isDeformableMeshColumn(
        TXshColumn *column, const PlasticVisualSettings &pvs) {
      return (column->getColumnType() == TXshColumn::eMeshType) &&
             (pvs.m_showOriginalColumn != column);
    }

    static inline bool isDeformableMeshLevel(TXshSimpleLevel *sl) {
      return sl && (sl->getType() == MESH_XSHLEVEL);
    }
  };  // locals

  if (pvs.m_applyPlasticDeformation && player.m_column >= 0) {
    // Check whether the player's column is a direct stage-schematic child of a
    // mesh object
    TStageObject *playerObj =
        player.m_xsh->getStageObject(TStageObjectId::ColumnId(player.m_column));
    assert(playerObj);

    const TStageObjectId &parentId = playerObj->getParent();
    if (parentId.isColumn() && playerObj->getParentHandle()[0] != 'H') {
      TXshColumn *parentCol = player.m_xsh->getColumn(parentId.getIndex());

      if (locals::isDeformableMeshColumn(parentCol, pvs) &&
          !locals::isDeformableMeshLevel(player.m_sl)) {
        const SkDP &sd = player.m_xsh->getStageObject(parentId)
                             ->getPlasticSkeletonDeformation();

        const TXshCell &parentCell =
            player.m_xsh->getCell(player.m_frame, parentId.getIndex());
        TXshSimpleLevel *parentSl = parentCell.getSimpleLevel();

        if (sd && locals::isDeformableMeshLevel(parentSl)) return playerObj;
      }
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

void onMeshImage(TMeshImage *mi, const Stage::Player &player,
                 const ImagePainter::VisualSettings &vs,
                 const TAffine &viewAff) {
  assert(mi);

  static const double soMinColor[4] = {0.0, 0.0, 0.0,
                                       0.6};  // Translucent black
  static const double soMaxColor[4] = {1.0, 1.0, 1.0,
                                       0.6};  // Translucent white
  static const double rigMinColor[4] = {0.0, 1.0, 0.0,
                                        0.6};  // Translucent green
  static const double rigMaxColor[4] = {1.0, 0.0, 0.0, 0.6};  // Translucent red

  bool doOnionSkin    = (player.m_onionSkinDistance != c_noOnionSkin);
  bool onionSkinImage = doOnionSkin && (player.m_onionSkinDistance != 0);
  bool drawMeshes =
      vs.m_plasticVisualSettings.m_drawMeshesWireframe && !onionSkinImage;
  bool drawSO = vs.m_plasticVisualSettings.m_drawSO && !onionSkinImage;
  bool drawRigidity =
      vs.m_plasticVisualSettings.m_drawRigidity && !onionSkinImage;

  // Currently skipping onion skinned meshes
  if (onionSkinImage) return;

  // Build dpi
  TPointD meshSlDpi(player.m_sl->getDpi(player.m_fid, 0));
  assert(meshSlDpi.x != 0.0 && meshSlDpi.y != 0.0);

  // Build reference change affines

  const TAffine &worldMeshToMeshAff =
      TScale(meshSlDpi.x / Stage::inch, meshSlDpi.y / Stage::inch);
  const TAffine &meshToWorldMeshAff =
      TScale(Stage::inch / meshSlDpi.x, Stage::inch / meshSlDpi.y);
  const TAffine &worldMeshToWorldAff = player.m_placement;

  const TAffine &meshToWorldAff = worldMeshToWorldAff * meshToWorldMeshAff;

  // Prepare OpenGL
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);

  // Push mesh coordinates
  glPushMatrix();
  tglMultMatrix(viewAff * meshToWorldAff);

  // Fetch deformation
  PlasticSkeletonDeformation *deformation = 0;
  double sdFrame;

  if (vs.m_plasticVisualSettings.m_applyPlasticDeformation &&
      player.m_column >= 0) {
    TXshColumn *column = player.m_xsh->getColumn(player.m_column);

    if (column != vs.m_plasticVisualSettings.m_showOriginalColumn) {
      TStageObject *playerObj = player.m_xsh->getStageObject(
          TStageObjectId::ColumnId(player.m_column));

      deformation = playerObj->getPlasticSkeletonDeformation().getPointer();
      sdFrame     = playerObj->paramsTime(player.m_frame);
    }
  }

  if (deformation) {
    // Retrieve the associated plastic deformers data (this may eventually
    // update the deforms)
    const PlasticDeformerDataGroup *dataGroup =
        PlasticDeformerStorage::instance()->process(
            sdFrame, mi, deformation, deformation->skeletonId(sdFrame),
            worldMeshToMeshAff);

    // Draw faces first
    if (drawSO)
      tglDrawSO(*mi, (double *)soMinColor, (double *)soMaxColor, dataGroup,
                true);

    if (drawRigidity)
      tglDrawRigidity(*mi, (double *)rigMinColor, (double *)rigMaxColor,
                      dataGroup, true);

    // Draw edges next
    if (drawMeshes) {
      glColor4d(0.0, 1.0, 0.0, 0.7 * player.m_opacity / 255.0);  // Green
      tglDrawEdges(*mi, dataGroup);  // The mesh must be deformed
    }
  } else {
    // Draw un-deformed data

    // Draw faces first
    if (drawSO) tglDrawSO(*mi, (double *)soMinColor, (double *)soMaxColor);

    if (drawRigidity)
      tglDrawRigidity(*mi, (double *)rigMinColor, (double *)rigMaxColor);

    // Just draw the mesh image next
    if (drawMeshes) {
      glColor4d(0.0, 1.0, 0.0, 0.7 * player.m_opacity / 255.0);  // Green
      tglDrawEdges(*mi);
    }
  }

  // Cleanup OpenGL
  glPopMatrix();

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
}

//-----------------------------------------------------------------------------

//! Applies Plastic deformation of the specified player's stage object.
void onPlasticDeformedImage(TStageObject *playerObj,
                            const Stage::Player &player,
                            const ImagePainter::VisualSettings &vs,
                            const TAffine &viewAff) {
  bool doOnionSkin    = (player.m_onionSkinDistance != c_noOnionSkin);
  bool onionSkinImage = doOnionSkin && (player.m_onionSkinDistance != 0);

  // Deal with color scaling due to transparency / onion skin
  double pixScale[4] = {1.0, 1.0, 1.0, 1.0};

  if (doOnionSkin) {
    if (onionSkinImage) {
      TPixel32 frontOnionColor, backOnionColor;
      bool inksOnly;

      Preferences::instance()->getOnionData(frontOnionColor, backOnionColor,
                                            inksOnly);

      const TPixel32 &refColor =
          (player.m_onionSkinDistance < 0) ? backOnionColor : frontOnionColor;

      pixScale[3] =
          1.0 - OnionSkinMask::getOnionSkinFade(player.m_onionSkinDistance);
      pixScale[0] =
          (refColor.r / 255.0) * pixScale[3];  // refColor is not premultiplied
      pixScale[1] = (refColor.g / 255.0) * pixScale[3];
      pixScale[2] = (refColor.b / 255.0) * pixScale[3];
    }
  } else if (player.m_opacity < 255) {
    pixScale[3] = player.m_opacity / 255.0;
    pixScale[0] = pixScale[1] = pixScale[2] = 0.0;
  }

  // Build the Mesh-related data
  const TXshCell &cell =
      player.m_xsh->getCell(player.m_frame, playerObj->getParent().getIndex());

  TXshSimpleLevel *meshLevel = cell.getSimpleLevel();
  const TFrameId &meshFid    = cell.getFrameId();

  const TMeshImageP &mi = meshLevel->getFrame(meshFid, false);
  if (!mi) return;

  // Build deformation-related data
  TStageObject *parentObj =
      player.m_xsh->getStageObject(playerObj->getParent());
  assert(playerObj);

  const PlasticSkeletonDeformationP &deformation =
      parentObj->getPlasticSkeletonDeformation().getPointer();
  assert(deformation);

  double sdFrame = parentObj->paramsTime(player.m_frame);

  // Build dpis

  TPointD meshSlDpi(meshLevel->getDpi(meshFid, 0));
  assert(meshSlDpi.x != 0.0 && meshSlDpi.y != 0.0);

  TPointD slDpi(player.m_sl ? player.m_sl->getDpi(player.m_fid, 0) : TPointD());
  if (slDpi.x == 0.0 || slDpi.y == 0.0 ||
      player.m_sl->getType() == PLI_XSHLEVEL)
    slDpi.x = slDpi.y = Stage::inch;

  // Build reference transforms

  const TAffine &worldTextureToWorldMeshAff =
      playerObj->getLocalPlacement(player.m_frame);
  const TAffine &worldTextureToWorldAff = player.m_placement;

  if (fabs(worldTextureToWorldMeshAff.det()) < 1e-6)
    return;  // Skip near-singular mesh/texture placements

  const TAffine &worldMeshToWorldTextureAff = worldTextureToWorldMeshAff.inv();
  const TAffine &worldMeshToWorldAff =
      worldTextureToWorldAff * worldMeshToWorldTextureAff;

  const TAffine &meshToWorldMeshAff =
      TScale(Stage::inch / meshSlDpi.x, Stage::inch / meshSlDpi.y);
  const TAffine &worldMeshToMeshAff =
      TScale(meshSlDpi.x / Stage::inch, meshSlDpi.y / Stage::inch);
  const TAffine &worldTextureToTextureAff = TScale(
      slDpi.x / Stage::inch,
      slDpi.y /
          Stage::inch);  // ::getDpiScale().inv() should be used instead...

  const TAffine &meshToWorldAff   = worldMeshToWorldAff * meshToWorldMeshAff;
  const TAffine &meshToTextureAff = worldTextureToTextureAff *
                                    worldMeshToWorldTextureAff *
                                    meshToWorldMeshAff;

  // Retrieve a drawable texture from the player's simple level
  const DrawableTextureDataP &texData = player.texture();
  if (!texData) return;

  // Retrieve the associated plastic deformers data (this may eventually update
  // the deforms)
  const PlasticDeformerDataGroup *dataGroup =
      PlasticDeformerStorage::instance()->process(
          sdFrame, mi.getPointer(), deformation.getPointer(),
          deformation->skeletonId(sdFrame), worldMeshToMeshAff);
  assert(dataGroup);

  // Set up OpenGL stuff
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);

  // Push mesh coordinates
  glPushMatrix();

  tglMultMatrix(viewAff * meshToWorldAff);

  glEnable(GL_TEXTURE_2D);

  // Applying modulation by the specified transparency parameter

  glColor4d(pixScale[3], pixScale[3], pixScale[3], pixScale[3]);
  tglDraw(*mi, *texData, meshToTextureAff, *dataGroup);

  glDisable(GL_TEXTURE_2D);

  if (onionSkinImage) {
    glBlendFunc(GL_ONE, GL_ONE);

    // Add Onion skin color. Observe that this way we don't consider blending
    // with the texture's
    // alpha - to obtain that, there is no simple way...

    double k = (1.0 - pixScale[3]);
    glColor4d(k * pixScale[0], k * pixScale[1], k * pixScale[2], 0.0);
    tglDrawFaces(*mi, dataGroup);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  glPopMatrix();

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
  assert(glGetError() == GL_NO_ERROR);
}

}  // namespace
