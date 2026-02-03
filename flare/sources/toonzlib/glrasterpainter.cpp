

#include "toonz/glrasterpainter.h"
#include "tgl.h"
#include "texturemanager.h"
#include "tpalette.h"
#include "tropcm.h"
#include "tvectorimage.h"
#include "tvectorrenderdata.h"
#include "tvectorgl.h"

namespace {

void doDrawRaster(const TAffine &aff, UCHAR *buffer, int wrap, int bpp,
                  const TDimension &rasDim, const TRect &bbox, bool showBBox,
                  GLenum magFilter, GLenum minFilter, bool premultiplied) {
  if (!buffer) return;

  bool isRGBM = (bpp == 4);

  if (!isRGBM) {
    if (bpp != 1) return;
  }

  TDimension maxSize = TextureManager::instance()->getMaxSize(isRGBM);

  if (bbox.getLx() > maxSize.lx) {
    TRect leftBox(bbox.getP00(), TDimension(maxSize.lx, bbox.getLy()));
    TRect rightBox(TPoint(bbox.getP00().x + maxSize.lx, bbox.getP00().y),
                   bbox.getP11());

    assert(leftBox.getLx() == maxSize.lx);
    assert(rightBox.getLx() == bbox.getLx() - maxSize.lx);
    assert(leftBox.getLy() == bbox.getLy());
    assert(rightBox.getLy() == bbox.getLy());

    doDrawRaster(aff, buffer, wrap, bpp, rasDim, leftBox, showBBox, magFilter,
                 minFilter, premultiplied);
    doDrawRaster(aff, buffer, wrap, bpp, rasDim, rightBox, showBBox, magFilter,
                 minFilter, premultiplied);
    return;
  }

  if (bbox.getLy() > maxSize.ly) {
    TRect bottomBox(bbox.getP00(), TDimension(bbox.getLx(), maxSize.ly));
    TRect topBox(TPointI(bbox.getP00().x, bbox.getP00().y + maxSize.ly),
                 bbox.getP11());

    assert(bottomBox.getLy() == maxSize.ly);
    assert(topBox.getLy() == bbox.getLy() - maxSize.ly);
    assert(bottomBox.getLx() == bbox.getLx());
    assert(topBox.getLx() == bbox.getLx());

    doDrawRaster(aff, buffer, wrap, bpp, rasDim, bottomBox, showBBox, magFilter,
                 minFilter, premultiplied);
    doDrawRaster(aff, buffer, wrap, bpp, rasDim, topBox, showBBox, magFilter,
                 minFilter, premultiplied);

    return;
  }

  glPushMatrix();
  TTranslation T((bbox.getP00().x - (rasDim.lx - bbox.getLx()) / 2.),
                 (bbox.getP00().y - (rasDim.ly - bbox.getLy()) / 2.));
  tglMultMatrix(aff * T);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);

  if (premultiplied)
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  else
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  TDimension ts =
      TextureManager::instance()->selectTexture(bbox.getSize(), isRGBM);

  GLenum fmt, type;
  TextureManager::instance()->getFmtAndType(isRGBM, fmt, type);

  int width  = bbox.getLx();
  int height = bbox.getLy();
  int y      = bbox.getP00().y;
  int x      = bbox.getP00().x;
  buffer += (x + y * wrap) * bpp;

  glPixelStorei(GL_UNPACK_ROW_LENGTH, wrap);

  glTexSubImage2D(GL_TEXTURE_2D,  // target (is a 2D texture)
                  0,              // is one level only
                  0, 0, width, height, fmt, type, buffer);
  CHECK_ERRORS_BY_GL

  double halfWidth  = 0.5 * bbox.getLx();
  double halfHeight = 0.5 * bbox.getLy();

  TPointD v0((-halfWidth), (-halfHeight));
  TPointD v1((halfWidth), (-halfHeight));
  TPointD v2((-halfWidth), (halfHeight));
  TPointD v3((halfWidth), (halfHeight));

  double s = (bbox.getLx()) / (double)ts.lx;
  double t = (bbox.getLy()) / (double)ts.ly;

  glColor3d(0, 0, 0);
  glBegin(GL_QUAD_STRIP);
  glTexCoord2d(0, 0);
  glVertex2d(v0.x, v0.y);
  glTexCoord2d(s, 0);
  glVertex2d(v1.x, v1.y);
  glTexCoord2d(0, t);
  glVertex2d(v2.x, v2.y);
  glTexCoord2d(s, t);
  glVertex2d(v3.x, v3.y);
  glEnd();

  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);

  if (showBBox) {
    glBegin(GL_LINE_LOOP);
    glVertex2d(v0.x, v0.y);
    glVertex2d(v1.x, v1.y);
    glVertex2d(v3.x, v3.y);
    glVertex2d(v2.x, v2.y);
    glEnd();
  }
  glPopMatrix();
}

//----------------------------------------------------------------------------
void doDrawRaster(const TAffine &aff, const TRasterImageP &ri,
                  const TRectI &bbox, bool showBBox, GLenum magFilter,
                  GLenum minFilter, bool premultiplied) {
  TRasterP r = ri->getRaster();
  r->lock();
  doDrawRaster(aff, r->getRawData(), r->getWrap(), r->getPixelSize(),
               r->getSize(), bbox, showBBox, magFilter, minFilter,
               premultiplied);
  r->unlock();
}

}  // namespace

//===================================================================

void GLRasterPainter::drawRaster(const TAffine &aff, UCHAR *buffer, int wrap,
                                 int bpp, const TDimension &rasSize,
                                 bool premultiplied) {
  if (!buffer) return;

  doDrawRaster(aff, buffer, wrap, bpp, rasSize, rasSize, false, GL_NEAREST,
               GL_LINEAR, premultiplied);
}

//----------------------------------------------------------------------------

void GLRasterPainter::drawRaster(const TAffine &aff, const TRasterImageP &ri,
                                 bool premultiplied) {
  if (!ri || !ri->getRaster()) return;

  doDrawRaster(aff, ri, ri->getRaster()->getBounds(), false, GL_NEAREST,
               GL_LINEAR, premultiplied);
}

//----------------------------------------------------------------------------

void GLRasterPainter::drawRaster(const TAffine &aff, const TToonzImageP &ti,
                                 bool showSavebox) {
  TRect saveBox = ti->getSavebox();
  if (saveBox.isEmpty()) return;

  TRasterCM32P ras  = ti->getRaster();
  TPaletteP palette = ti->getPalette();
  TRaster32P ras32(ras->getSize());

  TRop::convert(ras32, ras, palette, saveBox);

  TRasterImageP rasImg(ras32);
  double dpix, dpiy;
  ti->getDpi(dpix, dpiy);
  rasImg->setDpi(dpix, dpiy);

  bool premultiplied = true;
  doDrawRaster(aff, rasImg, saveBox, showSavebox, GL_NEAREST, GL_LINEAR,
               premultiplied);
}
