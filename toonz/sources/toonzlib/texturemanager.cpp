

#include "texturemanager.h"
#include "tsystem.h"
#include "toonz/preferences.h"

#include <sstream>

//==============================================================================
//
// TextureManager
//
//==============================================================================

TextureManager *TextureManager::instance() {
  if (!m_instance) m_instance = new TextureManager();
  return m_instance;
}

#ifdef _WIN32
TDimensionI TextureManager::getMaxSize(bool isRGBM) {
  GLenum fmt, type;
  getFmtAndType(isRGBM, fmt, type);
  if ((m_textureSize.lx == 0) || (m_textureSize.ly == 0)) {
    glEnable(GL_TEXTURE_2D);
    int texLx, texLy;
    int outX, outY;
    int shift = 0;
    do {
      ++shift;
      texLx = 64 << shift;
      texLy = 64 << shift;

      glTexImage2D(GL_PROXY_TEXTURE_2D,
                   0,      // is one level only
                   4,      // number of component of a pixel
                   texLx,  // size width
                   texLy,  // height
                   0,      // size of a border
                   fmt, type, 0);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &outX);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                               &outY);

#ifdef _DEBUG
      int intFmt, rSize, gSize, bSize, aSize, cmpt;

      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
                               GL_TEXTURE_INTERNAL_FORMAT, &intFmt);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE,
                               &rSize);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE,
                               &gSize);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_BLUE_SIZE,
                               &bSize);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE,
                               &aSize);
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_COMPONENTS,
                               &cmpt);

      if (outX && outY) {
        std::stringstream os;
        os << "texture size = " << outX << "x" << outY << " fmt " << intFmt
           << " cmpt# " << cmpt << " " << rSize << "," << gSize << "," << bSize
           << "," << aSize << '\n'
           << '\0';
        TSystem::outputDebug(os.str());
      }
#endif
    } while ((outX == texLx) && (outY == texLy));

    int s = Preferences::instance()->getTextureSize();
    if (s)
      s = std::min(s, 64 << (shift - 1));
    else
      s              = 64 << (shift - 1);
    m_textureSize.lx = s;
    m_textureSize.ly = s;

    glDisable(GL_TEXTURE_2D);
  }
  return TDimension(std::min(m_textureSize.lx, 2048),
                    std::min(m_textureSize.ly, 2048));
}
#else
TDimension TextureManager::getMaxSize(bool isRGBM) {
  m_textureSize = TDimension(512, 512);
  return m_textureSize;
}
#endif

void TextureManager::getFmtAndType(bool isRGBM, GLenum &fmt, GLenum &type) {
  if (isRGBM) {
    fmt =
#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        GL_BGRA_EXT
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
        GL_ABGR_EXT
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        GL_RGBA
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
        GL_BGRA
#else
        @undefined chan order
#endif
        ;
    type =
#ifdef TNZ_MACHINE_CHANNEL_ORDER_MRGB
        GL_UNSIGNED_INT_8_8_8_8_REV
#else
        GL_UNSIGNED_BYTE
#endif
        ;
  } else {
    fmt  = GL_LUMINANCE;
    type = GL_UNSIGNED_BYTE;
  }
}

TDimension TextureManager::selectTexture(TDimension reqSize, bool isRGBM) {
  int lx                     = 1;
  int ly                     = 1;
  while (lx < reqSize.lx) lx = lx << 1;
  while (ly < reqSize.ly) ly = ly << 1;
  TDimension textureSize     = instance()->getMaxSize(isRGBM);
  assert(lx <= textureSize.lx);
  assert(ly <= textureSize.ly);
  GLenum fmt, type;
  getFmtAndType(isRGBM, fmt, type);

  glTexImage2D(GL_TEXTURE_2D,  // target (is a 2D texture)
               0,              // is one level only
               4,              //  number of component of a pixel
               lx,             // size width
               ly,             //      height
               0,              // size of a border
               fmt, type, 0);
  return TDimension(lx, ly);
}

UCHAR *m_transpRow;  // comune a RGBM e GR8...

TextureManager::TextureManager() : m_textureSize(0, 0), m_isRGBM(true) {}

TextureManager *TextureManager::m_instance;
