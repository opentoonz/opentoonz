#include <memory>

#include "ext/meshtexturizer.h"

// TnzCore includes
#include "tgl.h"  // OpenGL includes

// Qt includes
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>

// tcg includes
#include "tcg/tcg_macros.h"
#include "tcg/tcg_list.h"
#include "tcg/tcg_misc.h"

#include "trop.h"

#define COPIED_BORDER 1  // Amount of tile border from the original image
#define TRANSP_BORDER 1  // Amount of transparent tile border
#define NONPREM_BORDER                                                         \
  1  // Amount of nonpremultiplied copied transparent border
#define TOTAL_BORDER                                                           \
  (COPIED_BORDER + TRANSP_BORDER)  // Overall border to texture tiles above
#define TOTAL_BORDER_2 (2 * TOTAL_BORDER)  // Twice the above

TCG_STATIC_ASSERT(COPIED_BORDER >
                  0);  // Due to GL_LINEAR alpha blending on tile seams
TCG_STATIC_ASSERT(TRANSP_BORDER > 0);  // Due to GL_CLAMP beyond tile limits
TCG_STATIC_ASSERT(NONPREM_BORDER <=
                  TRANSP_BORDER);  // The nonpremultiplied border is transparent

namespace {
int getMaxTextureTileSize() {
  static int maxTexSize = -1;
  if (maxTexSize == -1) {
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    maxTexSize = std::min(maxTexSize, 1 << 11);
  }
  return maxTexSize;
}
};

//******************************************************************************************
//    MeshTexturizer::Imp  definition
//******************************************************************************************

class MeshTexturizer::Imp {
  typedef MeshTexturizer::TextureData TextureData;

public:
  QReadWriteLock m_lock;  //!< Lock for synchronized access
  tcg::list<std::shared_ptr<TextureData>>
      m_textureDatas;  //!< Pool of texture datas

public:
  Imp() : m_lock(QReadWriteLock::Recursive) {}

  bool testTextureAlloc(int lx, int ly);
  GLuint textureAlloc(const TRaster32P &ras, const TRaster32P &aux, int x,
                      int y, int textureLx, int textureLy, bool premultiplied);

  void allocateTextures(int groupIdx, const TRaster32P &ras,
                        const TRaster32P &aux, int x, int y, int textureLx,
                        int textureLy, bool premultiplied);

  TextureData *getTextureData(int groupIdx);
};

//---------------------------------------------------------------------------------

bool MeshTexturizer::Imp::testTextureAlloc(int lx, int ly) {
  GLuint texName;
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &texName);
  glBindTexture(GL_TEXTURE_2D, texName);

  lx += TOTAL_BORDER_2, ly += TOTAL_BORDER_2;  // Add border

  int max_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

  glTexImage2D(GL_PROXY_TEXTURE_2D,
               0,                 // one level only
               GL_RGBA,           // number of pixel channels
               lx,                // width
               ly,                // height
               0,                 // border size
               TGL_FMT,           // pixel format
               GL_UNSIGNED_BYTE,  // pixel data type
               0);

  int outLx;
  glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &outLx);

  glDeleteTextures(1, &texName);
  assert(glGetError() == GL_NO_ERROR);
  return (lx == outLx);
}

//---------------------------------------------------------------------------------

GLuint MeshTexturizer::Imp::textureAlloc(const TRaster32P &ras,
                                         const TRaster32P &aux, int x, int y,
                                         int textureLx, int textureLy,
                                         bool premultiplied) {
  struct locals {
    static void clearMatte(const TRaster32P &ras, int xBegin, int yBegin,
                           int xEnd, int yEnd) {
      for (int y = yBegin; y != yEnd; ++y) {
        TPixel32 *line = ras->pixels(y), *pixEnd = line + xEnd;

        for (TPixel32 *pix = line + xBegin; pix != pixEnd; ++pix) pix->m = 0;
      }
    }

    static void clearMatte_border(const TRaster32P &ras, int border0,
                                  int border1) {
      assert(border0 < border1);

      // Horizontal
      clearMatte(ras, border0, border0, ras->getLx() - border0, border1);
      clearMatte(ras, border0, ras->getLy() - border1, ras->getLx() - border0,
                 ras->getLy() - border0);

      // Vertical
      clearMatte(ras, border0, border1, border1, ras->getLy() - border1);
      clearMatte(ras, ras->getLx() - border1, border1, ras->getLx() - border0,
                 ras->getLy() - border1);
    }

  };  // locals

  // Prepare the texture tile
  assert(aux->getLx() >= textureLx + TOTAL_BORDER_2 &&
         aux->getLy() >= textureLy + TOTAL_BORDER_2);

  TRect rasRect(x, y, x + textureLx - 1, y + textureLy - 1);
  rasRect = rasRect.enlarge(premultiplied ? COPIED_BORDER
                                          : COPIED_BORDER + NONPREM_BORDER);
  rasRect = rasRect * ras->getBounds();

  TRect auxRect(rasRect - TPoint(x - TOTAL_BORDER, y - TOTAL_BORDER));

  // An auxiliary raster must be used to supply the transparent border
  TRaster32P tex(aux->extract(0, 0, textureLx + TOTAL_BORDER_2 - 1,
                              textureLy + TOTAL_BORDER_2 - 1));
  tex->clear();
  aux->extract(auxRect)->copy(ras->extract(rasRect));

  if (!premultiplied && NONPREM_BORDER > 0) {
    locals::clearMatte_border(tex, TRANSP_BORDER - NONPREM_BORDER,
                              TRANSP_BORDER);
    TRop::expandColor(tex, true);  // precise is always true for now
  }

  // Pass the raster into VRAM
  GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  GL_CLAMP);  // These must be used on a bound texture,
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  GL_CLAMP);  // and are remembered in the OpenGL context.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR);  // They can be set here, no need for
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR);  // the user to do it.

  glPixelStorei(GL_UNPACK_ROW_LENGTH, tex->getWrap());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(GL_TEXTURE_2D,
               0,                 // one level only
               GL_RGBA,           // pixel channels count
               tex->getLx(),      // width
               tex->getLy(),      // height
               0,                 // border size
               TGL_FMT,           // pixel format
               GL_UNSIGNED_BYTE,  // pixel data type
               (GLvoid *)tex->getRawData());
  assert(glGetError() == GL_NO_ERROR);

  return texId;
}

//---------------------------------------------------------------------------------

void MeshTexturizer::Imp::allocateTextures(int groupIdx, const TRaster32P &ras,
                                           const TRaster32P &aux, int x, int y,
                                           int textureLx, int textureLy,
                                           bool premultiplied) {
  TextureData *data = m_textureDatas[groupIdx].get();

  // Test the specified texture allocation
  if (testTextureAlloc(textureLx, textureLy)) {
    TPointD scale(data->m_geom.getLx() / (double)ras->getLx(),
                  data->m_geom.getLy() / (double)ras->getLy());
    TRectD tileGeom(TRectD(scale.x * (x - TOTAL_BORDER),
                           scale.y * (y - TOTAL_BORDER),
                           scale.x * (x + textureLx + TOTAL_BORDER),
                           scale.y * (y + textureLy + TOTAL_BORDER)) +
                    data->m_geom.getP00());

    GLuint texId =
        textureAlloc(ras, aux, x, y, textureLx, textureLy, premultiplied);

    TextureData::TileData td = {texId, tileGeom};
    data->m_tileDatas.push_back(td);

    assert(glGetError() == GL_NO_ERROR);
    return;
  }

  if (textureLx <= 1 && textureLy <= 1) return;  // No texture can be allocated

  // The texture could not be allocated. Then, bisecate and branch.
  if (textureLx > textureLy) {
    int textureLx_2 = textureLx >> 1;
    allocateTextures(groupIdx, ras, aux, x, y, textureLx_2, textureLy,
                     premultiplied);
    allocateTextures(groupIdx, ras, aux, x + textureLx_2, y, textureLx_2,
                     textureLy, premultiplied);
  } else {
    int textureLy_2 = textureLy >> 1;
    allocateTextures(groupIdx, ras, aux, x, y, textureLx, textureLy_2,
                     premultiplied);
    allocateTextures(groupIdx, ras, aux, x, y + textureLy_2, textureLx,
                     textureLy_2, premultiplied);
  }
}

//---------------------------------------------------------------------------------

MeshTexturizer::TextureData *MeshTexturizer::Imp::getTextureData(int groupIdx) {
  typedef MeshTexturizer::TextureData TextureData;

  // Copy tile datas container
  return m_textureDatas[groupIdx].get();
}

//******************************************************************************************
//    MeshTexturizer  implementation
//******************************************************************************************

MeshTexturizer::MeshTexturizer() : m_imp(new Imp) {}

//---------------------------------------------------------------------------------

MeshTexturizer::~MeshTexturizer() {}

//---------------------------------------------------------------------------------

int MeshTexturizer::bindTexture(const TRaster32P &ras, const TRectD &geom,
                                PremultMode premultiplyMode) {
  QWriteLocker locker(&m_imp->m_lock);

  // Backup the state of some specific OpenGL variables that will be changed
  // throughout the code
  int row_length, alignment;
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);

  // Initialize a new texture data
  int dataIdx =
      m_imp->m_textureDatas.push_back(std::make_shared<TextureData>(geom));

  // Textures must have 2-power sizes. So, let's start with the smallest 2 power
  // >= ras's sizes.
  int textureLx =
      tcg::numeric_ops::GE_2Power((unsigned int)ras->getLx() + TOTAL_BORDER_2);
  int textureLy =
      tcg::numeric_ops::GE_2Power((unsigned int)ras->getLy() + TOTAL_BORDER_2);

  // We'll assume a strict granularity max of 2048 x 2048 textures, if it is
  // possible.
  textureLx = std::min(textureLx, getMaxTextureTileSize());
  textureLy = std::min(textureLy, getMaxTextureTileSize());

  // Allocate a suitable texture raster. The texture will include a transparent
  // 1-pix border
  // that is needed to perform texture mapping with GL_CLAMP transparent
  // wrapping
  TRaster32P tex(textureLx, textureLy);

  // Now, let's tile the specified raster. We'll start from the lower-left
  // corner in case
  // the raster can be completely included in just one tile

  int lx = ras->getLx(), ly = ras->getLy();
  int tileLx = textureLx - TOTAL_BORDER_2,
      tileLy = textureLy - TOTAL_BORDER_2;  // Texture size without border

  int xEntireCells = (lx - 1) / tileLx,
      yEntireCells =
          (ly - 1) /
          tileLy;  // +1 so in case l == tileL, we get the remainder case

  int lastTexLx = tcg::numeric_ops::GE_2Power(
      (unsigned int)(lx - xEntireCells * tileLx + TOTAL_BORDER_2));
  int lastTexLy = tcg::numeric_ops::GE_2Power(
      (unsigned int)(ly - yEntireCells * tileLy + TOTAL_BORDER_2));

  int lastTileLx = lastTexLx - TOTAL_BORDER_2,
      lastTileLy = lastTexLy - TOTAL_BORDER_2;

  bool premultiplied = (premultiplyMode == PREMULTIPLIED);

  int i, j;
  for (i = 0; i < yEntireCells; ++i) {
    for (j = 0; j < xEntireCells; ++j)
      // Perform a (possibly subdividing) allocation of the specified tile
      m_imp->allocateTextures(dataIdx, ras, tex, j * tileLx, i * tileLy, tileLx,
                              tileLy, premultiplied);

    m_imp->allocateTextures(dataIdx, ras, tex, j * tileLx, i * tileLy,
                            lastTileLx, tileLy, premultiplied);
  }

  for (j = 0; j < xEntireCells; ++j)
    m_imp->allocateTextures(dataIdx, ras, tex, j * tileLx, i * tileLy, tileLx,
                            lastTileLy, premultiplied);

  m_imp->allocateTextures(dataIdx, ras, tex, j * tileLx, i * tileLy, lastTileLx,
                          lastTileLy, premultiplied);

  // Restore OpenGL variables
  glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

  return dataIdx;
}

//---------------------------------------------------------------------------------

void MeshTexturizer::rebindTexture(int texId, const TRaster32P &ras,
                                   const TRectD &geom,
                                   PremultMode premultiplyMode) {
  QWriteLocker locker(&m_imp->m_lock);

  unbindTexture(texId);
  int newTexId = bindTexture(ras, geom, premultiplyMode);

  assert(texId == newTexId);
}

//---------------------------------------------------------------------------------

void MeshTexturizer::unbindTexture(int texId) {
  QWriteLocker locker(&m_imp->m_lock);
  m_imp->m_textureDatas.erase(texId);
}

//---------------------------------------------------------------------------------

MeshTexturizer::TextureData *MeshTexturizer::getTextureData(int textureId) {
  QReadLocker locker(&m_imp->m_lock);
  return m_imp->getTextureData(textureId);
}
