

#include "ttessellator.h"

//#include "tvectorrenderdata.h"
#include "tregionoutline.h"
#include "drawutil.h"
#include "tcolorfunctions.h"
#include "tthread.h"
#include "tcg/tcg_numeric_ops.h"
#include "trop.h"

//#include "tlevel_io.h"

#ifndef _WIN32
#define CALLBACK
#endif
// To avoid linking problems with HP ZX2000
#if defined(LINUX) || defined(FREEBSD)
#ifdef GLU_VERSION_1_2
#undef GLU_VERSION_1_2
#endif
#ifndef GLU_VERSION_1_1
#define GLU_VERSION_1_1
#endif
#endif
//==================================================================

#ifndef checkErrorsByGL
#define checkErrorsByGL                                                        \
  {                                                                            \
    GLenum err = glGetError();                                                 \
    assert(err != GL_INVALID_ENUM);                                            \
    assert(err != GL_INVALID_VALUE);                                           \
    assert(err != GL_INVALID_OPERATION);                                       \
    assert(err != GL_STACK_OVERFLOW);                                          \
    assert(err != GL_STACK_UNDERFLOW);                                         \
    assert(err != GL_OUT_OF_MEMORY);                                           \
    assert(err == GL_NO_ERROR);                                                \
  }
#endif

TglTessellator::GLTess::GLTess() {
  m_tess = gluNewTess();
  assert(m_tess);
}

//------------------------------------------------------------------

TglTessellator::GLTess::~GLTess() { gluDeleteTess(m_tess); }

//==================================================================

namespace {

extern "C" {
static void CALLBACK tessellateTexture(const GLdouble *tex) {
  double u = tex[0] * 0.01;
  double v = tex[1] * 0.01;
  glTexCoord2d(u, v);
  glVertex2dv(tex);
}
}

//-------------------------------------------------------------------

TThread::Mutex CombineDataGuard;

static std::list<GLdouble *> Combine_data;

extern "C" {
static void CALLBACK myCombine(GLdouble coords[3], GLdouble *d[4], GLfloat w[4],
                               GLdouble **dataOut) {
  GLdouble *newCoords = new GLdouble[3];

  newCoords[0] = coords[0];
  newCoords[1] = coords[1];
  newCoords[2] = coords[2];
  Combine_data.push_back(newCoords);
  *dataOut = newCoords;
}
}

//===================================================================

// typedef std::vector<T3DPointD>::iterator Vect3D_iter;

//-------------------------------------------------------------------
}

//-------------------------------------------------------------------

#ifdef _WIN32
typedef GLvoid(CALLBACK *GluCallback)(void);
#endif

#if defined(MACOSX) || defined(LINUX) || defined(FREEBSD) || defined(HAIKU)

typedef GLvoid (*GluCallback)();

#endif

void TglTessellator::doTessellate(GLTess &glTess, const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline outline, const TAffine &aff) {
  QMutexLocker sl(&CombineDataGuard);

  Combine_data.clear();
  assert(glTess.m_tess);

  gluTessCallback(glTess.m_tess, GLU_TESS_BEGIN, (GluCallback)glBegin);
  gluTessCallback(glTess.m_tess, GLU_TESS_END, (GluCallback)glEnd);

  gluTessCallback(glTess.m_tess, GLU_TESS_COMBINE, (GluCallback)myCombine);

#ifdef GLU_VERSION_1_2
  gluTessBeginPolygon(glTess.m_tess, NULL);
  gluTessProperty(glTess.m_tess, GLU_TESS_WINDING_RULE,
                  GLU_TESS_WINDING_POSITIVE);
#else
#ifdef GLU_VERSION_1_1
  gluBeginPolygon(glTess.m_tess);
#else
  assert(false);
#endif
#endif

  for (TRegionOutline::Boundary::iterator poly_it = outline.m_exterior.begin();
       poly_it != outline.m_exterior.end(); ++poly_it) {
#ifdef GLU_VERSION_1_2
    gluTessBeginContour(glTess.m_tess);
#else
#ifdef GLU_VERSION_1_1
    gluNextContour(glTess.m_tess, GLU_EXTERIOR);
#else
    assert(false);
#endif
#endif

    for (TRegionOutline::PointVector::iterator it = poly_it->begin();
         it != poly_it->end(); ++it) {
      // T3DPointD p = *it;
      it->x = aff.a11 * it->x + aff.a12 * it->y;
      it->y = aff.a21 * it->x + aff.a22 * it->y;

      gluTessVertex(glTess.m_tess, &(it->x), &(it->x));
    }
#ifdef GLU_VERSION_1_2
    gluTessEndContour(glTess.m_tess);
#endif
  }

  int subRegionNumber = outline.m_interior.size();
  if (subRegionNumber > 0) {
    for (TRegionOutline::Boundary::iterator poly_it =
             outline.m_interior.begin();
         poly_it != outline.m_interior.end(); ++poly_it) {
#ifdef GLU_VERSION_1_2
      gluTessBeginContour(glTess.m_tess);
#else
#ifdef GLU_VERSION_1_1
      gluNextContour(glTess.m_tess, GLU_INTERIOR);
#else
      assert(false);
#endif
#endif

      for (TRegionOutline::PointVector::reverse_iterator rit =
               poly_it->rbegin();
           rit != poly_it->rend(); ++rit) {
        // T3DPointD p = *rit;
        rit->x = aff.a11 * rit->x + aff.a12 * rit->y;
        rit->y = aff.a21 * rit->x + aff.a22 * rit->y;
        gluTessVertex(glTess.m_tess, &(rit->x), &(rit->x));
      }

#ifdef GLU_VERSION_1_2
      gluTessEndContour(glTess.m_tess);
#endif
    }
  }

#ifdef GLU_VERSION_1_2
  gluTessEndPolygon(glTess.m_tess);
#else
#ifdef GLU_VERSION_1_1
  gluEndPolygon(glTess.m_tess);
#else
  assert(false);
#endif
#endif

  std::list<GLdouble *>::iterator beginIt, endIt;
  endIt   = Combine_data.end();
  beginIt = Combine_data.begin();
  for (; beginIt != endIt; ++beginIt) delete[](*beginIt);
}

void TglTessellator::doTessellate(GLTess &glTess, const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &outline) {
  QMutexLocker sl(&CombineDataGuard);

  Combine_data.clear();
  assert(glTess.m_tess);

  gluTessCallback(glTess.m_tess, GLU_TESS_BEGIN, (GluCallback)glBegin);
  gluTessCallback(glTess.m_tess, GLU_TESS_END, (GluCallback)glEnd);

  gluTessCallback(glTess.m_tess, GLU_TESS_COMBINE, (GluCallback)myCombine);

#ifdef GLU_VERSION_1_2
  gluTessBeginPolygon(glTess.m_tess, NULL);
  gluTessProperty(glTess.m_tess, GLU_TESS_WINDING_RULE,
                  GLU_TESS_WINDING_POSITIVE);
#else
#ifdef GLU_VERSION_1_1
  gluBeginPolygon(glTess.m_tess);
#else
  assert(false);
#endif
#endif

  for (TRegionOutline::Boundary::iterator poly_it = outline.m_exterior.begin();
       poly_it != outline.m_exterior.end(); ++poly_it) {
#ifdef GLU_VERSION_1_2
    gluTessBeginContour(glTess.m_tess);
#else
#ifdef GLU_VERSION_1_1
    gluNextContour(glTess.m_tess, GLU_EXTERIOR);
#else
    assert(false);
#endif
#endif

    for (TRegionOutline::PointVector::iterator it = poly_it->begin();
         it != poly_it->end(); ++it)
      gluTessVertex(glTess.m_tess, &(it->x), &(it->x));

#ifdef GLU_VERSION_1_2
    gluTessEndContour(glTess.m_tess);
#endif
  }

  int subRegionNumber = outline.m_interior.size();
  if (subRegionNumber > 0) {
    for (TRegionOutline::Boundary::iterator poly_it =
             outline.m_interior.begin();
         poly_it != outline.m_interior.end(); ++poly_it) {
#ifdef GLU_VERSION_1_2
      gluTessBeginContour(glTess.m_tess);
#else
#ifdef GLU_VERSION_1_1
      gluNextContour(glTess.m_tess, GLU_INTERIOR);
#else
      assert(false);
#endif
#endif

      for (TRegionOutline::PointVector::reverse_iterator rit =
               poly_it->rbegin();
           rit != poly_it->rend(); ++rit)
        gluTessVertex(glTess.m_tess, &(rit->x), &(rit->x));

#ifdef GLU_VERSION_1_2
      gluTessEndContour(glTess.m_tess);
#endif
    }
  }

#ifdef GLU_VERSION_1_2
  gluTessEndPolygon(glTess.m_tess);
#else
#ifdef GLU_VERSION_1_1
  gluEndPolygon(glTess.m_tess);
#else
  assert(false);
#endif
#endif

  std::list<GLdouble *>::iterator beginIt, endIt;
  endIt   = Combine_data.end();
  beginIt = Combine_data.begin();
  for (; beginIt != endIt; ++beginIt) delete[](*beginIt);
}

//------------------------------------------------------------------

void TglTessellator::tessellate(const TColorFunction *cf,
                                const bool antiAliasing,
                                TRegionOutline &outline, TPixel32 color) {
  if (cf) color = (*(cf))(color);

  if (color.m == 0) return;

  bool transparencyFlag = color.m < 255;

  tglColor(color);

  if (transparencyFlag) {
    tglEnableLineSmooth();
  }

  TglTessellator::GLTess glTess;
  gluTessCallback(glTess.m_tess, GLU_TESS_VERTEX, (GluCallback)glVertex3dv);

  //------------------------//
  doTessellate(glTess, cf, antiAliasing, outline);
  //------------------------//

  if (antiAliasing && outline.m_doAntialiasing) {
    tglEnableLineSmooth();

    for (TRegionOutline::Boundary::iterator poly_it =
             outline.m_exterior.begin();
         poly_it != outline.m_exterior.end(); ++poly_it) {
      std::vector<GLdouble> v;
      if (poly_it->size() == 0) continue;
      v.resize(poly_it->size() * 2);
      int i = 0;
      for (TRegionOutline::PointVector::iterator it = poly_it->begin();
           it != poly_it->end(); ++it) {
        v[i++] = it->x;
        v[i++] = it->y;
      }

      glEnableClientState(GL_VERTEX_ARRAY);

      glVertexPointer(2, GL_DOUBLE, sizeof(GLdouble) * 2, &v[0]);
      glDrawArrays(GL_LINE_LOOP, 0, v.size() / 2);

      glDisableClientState(GL_VERTEX_ARRAY);
    }

    for (TRegionOutline::Boundary::iterator poly_it =
             outline.m_interior.begin();
         poly_it != outline.m_interior.end(); ++poly_it) {
      std::vector<GLdouble> v;
      v.resize(poly_it->size() * 2);
      int i = 0;
      for (TRegionOutline::PointVector::iterator it = poly_it->begin();
           it != poly_it->end(); ++it) {
        v[i++] = it->x;
        v[i++] = it->y;
      }
      if (v.empty()) continue;
      glEnableClientState(GL_VERTEX_ARRAY);

      glVertexPointer(2, GL_DOUBLE, sizeof(GLdouble) * 2, &v[0]);
      glDrawArrays(GL_LINE_LOOP, 0, v.size() / 2);

      glDisableClientState(GL_VERTEX_ARRAY);
    }
  }
}

//------------------------------------------------------------------

void TglTessellator::tessellate(const TColorFunction *cf,
                                const bool antiAliasing,
                                TRegionOutline &outline, TRaster32P texture) {
  // QMutexLocker sl(m_mutex);
  checkErrorsByGL;
  glEnable(GL_TEXTURE_2D);
  glColor4d(1, 1, 1, 1);
  checkErrorsByGL;
  TextureInfoForGL texInfo;

  int pow2Lx = tcg::numeric_ops::GE_2Power((unsigned int)texture->getLx());
  int pow2Ly = tcg::numeric_ops::GE_2Power((unsigned int)texture->getLy());

  TAffine aff;
  if (texture->getLx() != pow2Lx || texture->getLy() != pow2Ly) {
    TRaster32P r(pow2Lx, pow2Ly);
    aff = TScale((double)pow2Lx / texture->getLx(),
                 (double)pow2Ly / texture->getLy());
    TRop::resample(r, texture,
                   aff.place(texture->getCenterD(), r->getCenterD()));
    texture = r;
    glPushMatrix();
    tglMultMatrix(aff.inv());
  }

  // If GL_BRGA isn't present make a proper texture to use (... obsolete?)
  texture->lock();
  TRasterP texImage = prepareTexture(texture, texInfo);
  checkErrorsByGL;
  if (texImage != texture) texImage->lock();

  assert(texImage->getLx() == texImage->getWrap());

  GLuint texId;
  glGenTextures(1, &texId);  // Generate a texture name
  checkErrorsByGL;
  glBindTexture(GL_TEXTURE_2D, texId);  // Bind it 'active'
  checkErrorsByGL;
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  GL_REPEAT);  // These must be invoked
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  GL_REPEAT);  // on a bound texture
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  //
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  //
  checkErrorsByGL;
  glTexEnvf(GL_TEXTURE_ENV,       // This too ?
            GL_TEXTURE_ENV_MODE,  // Better here anyway
            GL_MODULATE);         //
  checkErrorsByGL;
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  checkErrorsByGL;
  glTexImage2D(GL_TEXTURE_2D,
               0,                       // one level only
               texInfo.internalformat,  // pixel channels count
               texInfo.width,           // width
               texInfo.height,          // height
               0,                       // border size
               texInfo.type,    // pixel format           // crappy names
               texInfo.format,  // pixel data type        // oh, SO much
               texImage->getRawData());
  checkErrorsByGL;
  texture->unlock();
  if (texImage != texture) texImage->unlock();

  TglTessellator::GLTess glTess;
  gluTessCallback(glTess.m_tess, GLU_TESS_VERTEX,
                  (GluCallback)tessellateTexture);
  checkErrorsByGL;

  //------------------------//
  if (aff != TAffine())
    doTessellate(glTess, cf, antiAliasing, outline,
                 aff);  // Tessellate & render
  else
    doTessellate(glTess, cf, antiAliasing, outline);  // Tessellate & render
  checkErrorsByGL;
  //------------------------//
  if (aff != TAffine()) glPopMatrix();
  glDeleteTextures(1, &texId);  // Delete & unbind texture
  checkErrorsByGL;
  glDisable(GL_TEXTURE_2D);
  checkErrorsByGL;
}

//------------------------------------------------------------------

// TglTessellator::GLTess TglTessellator::m_glTess;

//=============================================================================
