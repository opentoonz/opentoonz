

#include "toonz/plasticdeformerfx.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshcell.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/dpiscale.h"
#include "toonz/stage.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticdeformerstorage.h"
#include "ext/ttexturesstorage.h"
#include "ext/plasticvisualsettings.h"
#include "ext/meshutils.h"

// TnzBase includes
#include "trenderer.h"

// TnzCore includes
#include "tgl.h"
#include "tofflinegl.h"
#include "tgldisplaylistsmanager.h"
#include "tconvert.h"
#include "trop.h"

#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QImage>

FX_IDENTIFIER_IS_HIDDEN(PlasticDeformerFx, "plasticDeformerFx")

//***************************************************************************************************
//    Local namespace
//***************************************************************************************************

namespace {

std::string toString(const TAffine &aff) {
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

//-----------------------------------------------------------------------------------

std::string toString(SkVD *vd, double sdFrame) {
  std::string result;

  for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
    result += ::to_string(vd->m_params[p]->getValue(sdFrame), 5) + " ";

  return result;
}

//-----------------------------------------------------------------------------------

std::string toString(const PlasticSkeleton::vertex_type &vx) {
  // TODO: Add z and rigidity
  return ::to_string(vx.P().x, 5) + " " + ::to_string(vx.P().y, 5);
}

//-----------------------------------------------------------------------------------

std::string toString(const PlasticSkeletonDeformationP &sd, double sdFrame) {
  std::string result;

  const PlasticSkeletonP &skeleton = sd->skeleton(sdFrame);
  if (!skeleton || skeleton->empty()) return result;

  const tcg::list<PlasticSkeleton::vertex_type> &vertices =
      skeleton->vertices();

  tcg::list<PlasticSkeleton::vertex_type>::const_iterator vt,
      vEnd(vertices.end());

  result = toString(*vertices.begin());
  for (vt = vertices.begin(); vt != vEnd; ++vt) {
    result += "; " + toString(*vt);
    result += " " + toString(sd->vertexDeformation(vt->name()), sdFrame);
  }

  return result;
}

}  // namespace

//***************************************************************************************************
//    PlasticDeformerFx  implementation
//***************************************************************************************************

PlasticDeformerFx::PlasticDeformerFx() : TRasterFx() {
  addInputPort("source", m_port);
}

//-----------------------------------------------------------------------------------

TFx *PlasticDeformerFx::clone(bool recursive) const {
  PlasticDeformerFx *fx =
      dynamic_cast<PlasticDeformerFx *>(TFx::clone(recursive));
  assert(fx);

  fx->m_xsh = m_xsh;
  fx->m_col = m_col;

  return fx;
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::canHandle(const TRenderSettings &info, double frame) {
  // Yep. Affines are handled. Well - it's easy, since OpenGL lets you do that
  // directly
  // with a glPushMatrix...

  return true;
}

//-----------------------------------------------------------------------------------

std::string PlasticDeformerFx::getAlias(double frame,
                                        const TRenderSettings &info) const {
  std::string alias(getFxType());
  alias += "[";

  if (m_port.isConnected()) {
    TRasterFxP ifx = m_port.getFx();
    assert(ifx);

    alias += ifx->getAlias(frame, info);
  }

  TStageObject *meshColumnObj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(m_col));
  const PlasticSkeletonDeformationP &sd =
      meshColumnObj->getPlasticSkeletonDeformation();
  if (sd) alias += ", " + toString(sd, meshColumnObj->paramsTime(frame));

  alias + "]";

  return alias;
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::doGetBBox(double frame, TRectD &bbox,
                                  const TRenderSettings &info) {
  if (!m_port.isConnected()) return false;

  // It's hard work to calculate the bounding box of a plastic deformation.
  // Decline.
  bbox = TConsts::infiniteRectD;
  return true;
}

//-----------------------------------------------------------------------------------

void PlasticDeformerFx::buildRenderSettings(double frame,
                                            TRenderSettings &info) {
  // As previously pointed out, this fx is able to handle affines. We can,
  // actually, *decide*
  // the input reference to work with.

  // So, the best choice is to let the *input fx* decide the appropriate
  // reference, by invoking
  // its handledAffine() method.

  info.m_bpp    = 32;  // We need to fix the input to 32-bpp
  info.m_affine = m_port->handledAffine(info, frame);
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::buildTextureDataSl(double frame, TRenderSettings &info,
                                           TAffine &worldLevelToLevelAff) {
  int row = (int)frame;

  // Initialize level vars
  TLevelColumnFx *lcfx       = (TLevelColumnFx *)m_port.getFx();
  TXshLevelColumn *texColumn = lcfx->getColumn();

  const TXshCell &texCell = texColumn->getCell(row);

  TXshSimpleLevel *texSl = texCell.getSimpleLevel();
  const TFrameId &texFid = texCell.getFrameId();

  if (!texSl || texSl->getType() == MESH_XSHLEVEL) return false;

  // Build dpi data
  TPointD texDpi(texSl->getDpi(texFid, 0));
  if (texDpi.x == 0.0 || texDpi.y == 0.0 || texSl->getType() == PLI_XSHLEVEL)
    texDpi.x = texDpi.y = Stage::inch;

  // Build reference transforms data

  // NOTE: TAffine() corresponds to IMAGE coordinates here, not WORLD
  // coordinates. This is achieved
  // by removing the level's dpi affine during render-tree build-up (see
  // scenefx.cpp).

  worldLevelToLevelAff = TScale(texDpi.x / Stage::inch, texDpi.y / Stage::inch);

  // Initialize input render settings

  // In the case of vector images, in order to retain the image quality required
  // by info.m_affine,
  // the scale component is allowed too.

  // In the raster image case, we'll use the original image reference IF the
  // affine is a magnification
  // (ie the scale is > 1.0) - OTHERWISE, the OpenGL minification filter is too
  // crude since it renders
  // a fragment using its 4 adjacent pixels ONLY; in this case, we'll pass the
  // affine below.

  const TAffine &handledAff = TRasterFx::handledAffine(info, frame);

  if (texSl->getType() == PLI_XSHLEVEL) {
    info.m_affine = handledAff;
    buildRenderSettings(frame, info);
  } else {
    info.m_affine = TAffine();
    buildRenderSettings(frame, info);

    // NOTE: scale = handledAff.a11 / worldLevelToLevelAff.a11
    if (handledAff.a11 < worldLevelToLevelAff.a11)
      info.m_affine =
          TScale(handledAff.a11 / worldLevelToLevelAff.a11) * info.m_affine;
  }

  return true;
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::buildTextureData(double frame, TRenderSettings &info,
                                         TAffine &worldLevelToLevelAff) {
  // Common case (typically happen with sub-xsheets)

  buildRenderSettings(frame, info);  // Adjust the info
  worldLevelToLevelAff = TAffine();  // Reference match

  return true;
}

//-----------------------------------------------------------------------------------

void PlasticDeformerFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &info) {
  if (!m_port.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  int row = (int)frame;

  // Build texture data
  TRenderSettings texInfo(info);
  TAffine worldTexLevelToTexLevelAff;

  if (dynamic_cast<TLevelColumnFx *>(m_port.getFx())) {
    if (!buildTextureDataSl(frame, texInfo, worldTexLevelToTexLevelAff)) return;
  } else
    buildTextureData(frame, texInfo, worldTexLevelToTexLevelAff);

  // Initialize mesh level vars

  const TXshCell &meshCell = m_xsh->getCell(row, m_col);

  TXshSimpleLevel *meshSl = meshCell.getSimpleLevel();
  const TFrameId &meshFid = meshCell.getFrameId();

  if (!meshSl || meshSl->getType() != MESH_XSHLEVEL) return;

  // Retrieve mesh image and deformation

  TStageObject *meshColumnObj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(m_col));

  TMeshImageP mi(meshSl->getFrame(meshFid, false));
  if (!mi) return;

  // Retrieve deformation data

  const PlasticSkeletonDeformationP &sd =
      meshColumnObj->getPlasticSkeletonDeformation();
  assert(sd);

  double sdFrame = meshColumnObj->paramsTime(frame);

  // Build dpi data

  TPointD meshDpi(meshSl->getDpi(meshFid, 0));
  assert(meshDpi.x != 0.0 && meshDpi.y != 0.0);

  // Build reference transforms data

  // Build affines

  const TAffine &imageToTextureAff           = texInfo.m_affine;
  const TAffine &worldTexLevelToWorldMeshAff = m_texPlacement;
  const TAffine &meshToWorldMeshAff =
      TScale(Stage::inch / meshDpi.x, Stage::inch / meshDpi.y);

  const TAffine &meshToTexLevelAff = worldTexLevelToTexLevelAff *
                                     worldTexLevelToWorldMeshAff.inv() *
                                     meshToWorldMeshAff;
  const TAffine &meshToTextureAff = imageToTextureAff * meshToTexLevelAff;

  // Retrieve deformer data

  TScale worldMeshToMeshAff(meshDpi.x / Stage::inch, meshDpi.y / Stage::inch);

  std::unique_ptr<const PlasticDeformerDataGroup> dataGroup(
      PlasticDeformerStorage::instance()->processOnce(
          sdFrame, mi.getPointer(), sd.getPointer(), sd->skeletonId(sdFrame),
          worldMeshToMeshAff));

  // Build texture

  // Build the mesh's bounding box and map it to input reference
  TRectD meshBBox(meshToTextureAff * mi->getBBox());

  // Now, build the tile's geometry
  TRectD texBBox;
  m_port->getBBox(frame, texBBox, texInfo);

  TRectD bbox = texBBox * meshBBox;
  if (bbox.getLx() <= 0.0 || bbox.getLy() <= 0.0) return;

  bbox.x0 = tfloor(bbox.x0);
  bbox.y0 = tfloor(bbox.y0);
  bbox.x1 = tceil(bbox.x1);
  bbox.y1 = tceil(bbox.y1);

  TDimension tileSize(tround(bbox.getLx()), tround(bbox.getLy()));

  // Then, compute the input image
  TTile inTile;
  m_port->allocateAndCompute(inTile, bbox.getP00(), tileSize, TRasterP(), frame,
                             texInfo);
  QOpenGLContext *context;
  // Draw the textured mesh
  {
    // Prepare texture
    TRaster32P tex(inTile.getRaster());
    TRop::depremultiply(tex);  // Textures must be stored depremultiplied.
                               // See docs about the tglDraw() below.
    static TAtomicVar var;
    const std::string &texId = "render_tex " + std::to_string(++var);

    // Prepare an OpenGL context
    context = new QOpenGLContext();
    if (QOpenGLContext::currentContext())
      context->setShareContext(QOpenGLContext::currentContext());
    context->setFormat(QSurfaceFormat::defaultFormat());
    context->create();
    context->makeCurrent(info.m_offScreenSurface.get());

    TDimension d = tile.getRaster()->getSize();
    QOpenGLFramebufferObject fb(d.lx, d.ly);

    fb.bind();

    // Load texture into the context
    TTexturesStorage *ts                = TTexturesStorage::instance();
    const DrawableTextureDataP &texData = ts->loadTexture(texId, tex, bbox);

    // Draw
    glViewport(0, 0, d.lx, d.ly);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, d.lx, 0, d.ly);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    tglMultMatrix(TTranslation(-tile.m_pos) * info.m_affine *
                  meshToWorldMeshAff);

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    tglDraw(*mi, *texData, meshToTextureAff, *dataGroup);

    // Retrieve drawing and copy to output tile

    QImage img = fb.toImage().scaled(QSize(d.lx, d.ly), Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
    int wrap      = tile.getRaster()->getLx() * sizeof(TPixel32);
    uchar *srcPix = img.bits();
    uchar *dstPix = tile.getRaster()->getRawData() + wrap * (d.ly - 1);
    for (int y = 0; y < d.ly; y++) {
      memcpy(dstPix, srcPix, wrap);
      dstPix -= wrap;
      srcPix += wrap;
    }
    fb.release();

    // context->getRaster(tile.getRaster());
    glFlush();
    glFinish();
    // Cleanup

    // No need to disable stuff - the context dies here

    // ts->unloadTexture(texId);                                // Auto-released
    // due to display list destruction
    context->deleteLater();
    // context->doneCurrent();
  }
  assert(glGetError() == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------------

void PlasticDeformerFx::doDryCompute(TRectD &rect, double frame,
                                     const TRenderSettings &info) {
  if (!m_port.isConnected()) return;

  int row = (int)frame;

  TRenderSettings texInfo(info);
  TAffine worldTexLevelToTexLevelAff;

  if (dynamic_cast<TLevelColumnFx *>(m_port.getFx())) {
    if (!buildTextureDataSl(frame, texInfo, worldTexLevelToTexLevelAff)) return;
  } else
    buildTextureData(frame, texInfo, worldTexLevelToTexLevelAff);

  const TXshCell &meshCell = m_xsh->getCell(row, m_col);

  TXshSimpleLevel *meshSl = meshCell.getSimpleLevel();
  const TFrameId &meshFid = meshCell.getFrameId();

  if (!meshSl || meshSl->getType() == MESH_XSHLEVEL) return;

  TStageObject *meshColumnObj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(m_col));

  TMeshImageP mi(meshSl->getFrame(meshFid, false));
  if (!mi) return;

  const PlasticSkeletonDeformationP &sd =
      meshColumnObj->getPlasticSkeletonDeformation();
  assert(sd);

  TPointD meshDpi(meshSl->getDpi(meshFid, 0));
  assert(meshDpi.x != 0.0 && meshDpi.y != 0.0);

  const TAffine &textureToImageAff        = texInfo.m_affine;
  const TAffine &worldImageToWorldMeshAff = m_texPlacement;
  const TAffine &meshToWorldMeshAff =
      TScale(Stage::inch / meshDpi.x, Stage::inch / meshDpi.y);

  const TAffine &meshToTextureAff =
      textureToImageAff.inv() * worldTexLevelToTexLevelAff *
      worldImageToWorldMeshAff.inv() * meshToWorldMeshAff;

  // Build the mesh's bounding box and map it to input reference
  TRectD meshBBox(meshToTextureAff * mi->getBBox());

  // Now, build the tile's geometry
  TRectD texBBox;
  m_port->getBBox(frame, texBBox, texInfo);

  TRectD bbox = texBBox * meshBBox;
  if (bbox.getLx() <= 0.0 || bbox.getLy() <= 0.0) return;

  bbox.x0 = tfloor(bbox.x0);
  bbox.y0 = tfloor(bbox.y0);
  bbox.x1 = tceil(bbox.x1);
  bbox.y1 = tceil(bbox.y1);

  m_port->dryCompute(bbox, frame, texInfo);
}
