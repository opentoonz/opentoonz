

// Glew include
#include <GL/glew.h>

// TnzCore includes
#include "tgl.h"

// Qt includes
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>

#include <QGLPixelBuffer>
#include <QGLFramebufferObject>
#include <QGLShaderProgram>
#include <QOpenGLContext>

// STD includes
#include <map>
#include <memory>

#include "stdfx/shadingcontext.h"

//*****************************************************************
//    Local Namespace stuff
//*****************************************************************

namespace {

typedef std::unique_ptr<QGLPixelBuffer> QGLPixelBufferP;
typedef std::unique_ptr<QGLFramebufferObject> QGLFramebufferObjectP;
typedef std::unique_ptr<QGLShaderProgram> QGLShaderProgramP;

struct CompiledShader {
  QGLShaderProgramP m_program;
  QDateTime m_lastModified;

public:
  CompiledShader() {}
  CompiledShader(const CompiledShader &) { assert(!m_program.get()); }
};

}  // namespace

//*****************************************************************
//    ShadingContext::Imp  definition
//*****************************************************************

struct ShadingContext::Imp {
  QGLPixelBufferP m_pixelBuffer;  //!< OpenGL context.
  QGLFramebufferObjectP m_fbo;    //!< Output buffer.

  std::map<QString,
           CompiledShader>
      m_shaderPrograms;  //!< Shader Programs stored in the context.
                         //!  \warning   Values have \p unique_ptr members.
public:
  Imp();

  static QGLFormat format();

  void initMatrix(int lx, int ly);

private:
  // Not copyable
  Imp(const Imp &);
  Imp &operator=(const Imp &);
};

//--------------------------------------------------------

ShadingContext::Imp::Imp()
    : m_pixelBuffer(new QGLPixelBuffer(1, 1, format())) {
  m_pixelBuffer->context()->contextHandle()->moveToThread(nullptr);
}

//--------------------------------------------------------

QGLFormat ShadingContext::Imp::format() {
  QGLFormat fmt;

#ifdef MACOSX
  fmt.setVersion(3, 2);
  fmt.setProfile(QGLFormat::CompatibilityProfile);
#endif

  return fmt;
}

//--------------------------------------------------------

void ShadingContext::Imp::initMatrix(int lx, int ly) {
  glViewport(0, 0, lx, ly);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, lx, 0, ly);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

//*****************************************************************
//    ShadingContext  implementation
//*****************************************************************

ShadingContext::ShadingContext() : m_imp(new Imp) {
  makeCurrent();
  glewExperimental = GL_TRUE;
  glewInit();
  doneCurrent();
}

//--------------------------------------------------------

ShadingContext::~ShadingContext() {
  // Destructor of QGLPixelBuffer calls QOpenGLContext::makeCurrent() internally,
  // so the current thread must be the owner of QGLPixelBuffer context,
  // when the destructor of m_imp->m_context is called.
  m_imp->m_pixelBuffer->context()->contextHandle()->moveToThread(QThread::currentThread());
}

//--------------------------------------------------------

ShadingContext::Support ShadingContext::support() {
  return !QGLPixelBuffer::hasOpenGLPbuffers()
             ? NO_PIXEL_BUFFER
             : !QGLShaderProgram::hasOpenGLShaderPrograms() ? NO_SHADERS : OK;
}

//--------------------------------------------------------

bool ShadingContext::isValid() const { return m_imp->m_pixelBuffer->isValid(); }

//--------------------------------------------------------
/*
QGLFormat ShadingContext::defaultFormat(int channelsSize)
{
  QGL::FormatOptions opts =
    QGL::SingleBuffer     |
    QGL::NoAccumBuffer    |
    QGL::NoDepthBuffer    |                           // I guess it could be
necessary to let at least
    QGL::NoOverlay        |                           // the depth buffer
enabled... Fragment shaders could
    QGL::NoSampleBuffers  |                           // use it...
    QGL::NoStencilBuffer  |
    QGL::NoStereoBuffers;

  QGLFormat fmt(opts);
  fmt.setDirectRendering(true);                       // Just to be explicit -
USE HARDWARE ACCELERATION

  fmt.setRedBufferSize(channelsSize);
  fmt.setGreenBufferSize(channelsSize);
  fmt.setBlueBufferSize(channelsSize);
  fmt.setAlphaBufferSize(channelsSize);

  // TODO: 64-bit mode should be settable here

  return fmt;
}
*/
//--------------------------------------------------------

void ShadingContext::makeCurrent() {
  m_imp->m_pixelBuffer->context()->contextHandle()->moveToThread(QThread::currentThread());
  m_imp->m_pixelBuffer->makeCurrent();
}

//--------------------------------------------------------

void ShadingContext::doneCurrent() {
  m_imp->m_pixelBuffer->doneCurrent();
  m_imp->m_pixelBuffer->context()->contextHandle()->moveToThread(nullptr);
}

//--------------------------------------------------------

void ShadingContext::resize(int lx, int ly,
                            const QGLFramebufferObjectFormat &fmt) {
  if (m_imp->m_fbo.get() && m_imp->m_fbo->width() == lx &&
      m_imp->m_fbo->height() == ly && m_imp->m_fbo->format() == fmt)
    return;

  if (lx == 0 || ly == 0) {
    m_imp->m_fbo.reset(0);
  } else {
    m_imp->m_fbo.reset(new QGLFramebufferObject(lx, ly, fmt));
    assert(m_imp->m_fbo->isValid());

    m_imp->m_fbo->bind();
  }
}

//--------------------------------------------------------

QGLFramebufferObjectFormat ShadingContext::format() const {
  QGLFramebufferObject *fbo = m_imp->m_fbo.get();
  return fbo ? m_imp->m_fbo->format() : QGLFramebufferObjectFormat();
}

//--------------------------------------------------------

TDimension ShadingContext::size() const {
  QGLFramebufferObject *fbo = m_imp->m_fbo.get();
  return fbo ? TDimension(fbo->width(), fbo->height()) : TDimension();
}

//--------------------------------------------------------

void ShadingContext::addShaderProgram(const QString &shaderName,
                                      QGLShaderProgram *program) {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms
          .insert(std::make_pair(shaderName, CompiledShader()))
          .first;

  st->second.m_program.reset(program);
}

//--------------------------------------------------------

void ShadingContext::addShaderProgram(const QString &shaderName,
                                      QGLShaderProgram *program,
                                      const QDateTime &lastModified) {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms
          .insert(std::make_pair(shaderName, CompiledShader()))
          .first;

  st->second.m_program.reset(program);
  st->second.m_lastModified = lastModified;
}

//--------------------------------------------------------

bool ShadingContext::removeShaderProgram(const QString &shaderName) {
  return (m_imp->m_shaderPrograms.erase(shaderName) > 0);
}

//--------------------------------------------------------

QGLShaderProgram *ShadingContext::shaderProgram(
    const QString &shaderName) const {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms.find(shaderName);

  return (st != m_imp->m_shaderPrograms.end()) ? st->second.m_program.get() : 0;
}

//--------------------------------------------------------

QDateTime ShadingContext::lastModified(const QString &shaderName) const {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms.find(shaderName);

  return (st != m_imp->m_shaderPrograms.end()) ? st->second.m_lastModified
                                               : QDateTime();
}

//--------------------------------------------------------

std::pair<QGLShaderProgram *, QDateTime> ShadingContext::shaderData(
    const QString &shaderName) const {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms.find(shaderName);

  return (st != m_imp->m_shaderPrograms.end())
             ? std::make_pair(st->second.m_program.get(),
                              st->second.m_lastModified)
             : std::make_pair((QGLShaderProgram *)0, QDateTime());
}

//--------------------------------------------------------

GLuint ShadingContext::loadTexture(const TRasterP &src, GLuint texUnit) {
  glActiveTexture(GL_TEXTURE0 + texUnit);

  GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  GL_CLAMP);  // These must be used on a bound texture,
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  GL_CLAMP);  // and are remembered in the OpenGL context.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_NEAREST);  // They can be set here, no need for
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST);  // the user to do it.

  glPixelStorei(GL_UNPACK_ROW_LENGTH, src->getWrap());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLenum chanType = TRaster32P(src) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;

  glTexImage2D(GL_TEXTURE_2D,
               0,             // one level only
               GL_RGBA,       // pixel channels count
               src->getLx(),  // width
               src->getLy(),  // height
               0,             // border size
               TGL_FMT,       // pixel format
               chanType,      // channel data type
               (GLvoid *)src->getRawData());

  assert(glGetError() == GL_NO_ERROR);

  return texId;
}

//----------------------------------------------------------------------

void ShadingContext::unloadTexture(GLuint texId) {
  glDeleteTextures(1, &texId);
}

//--------------------------------------------------------

void ShadingContext::draw(const TRasterP &dst) {
  assert("ShadingContext::resize() must be invoked at least once before this" &&
         m_imp->m_fbo.get());

  int lx = dst->getLx(),
      ly = dst->getLy();  // NOTE: We're not using m_imp->m_fbo's size, since
                          // it could be possibly greater than the required
                          // destination surface.

  m_imp->initMatrix(lx, ly);  // This call sets the OpenGL viewport to this
                              // size - and matches (1, 1) to dst's (lx, ly)

  {
    glBegin(GL_QUADS);

    glVertex2f(0.0, 0.0);
    glVertex2f(lx, 0.0);
    glVertex2f(lx, ly);
    glVertex2f(0.0, ly);

    glEnd();
  }

  glPixelStorei(GL_PACK_ROW_LENGTH, dst->getWrap());

  // Read the fbo to dst
  if (TRaster32P ras32 = dst)
    glReadPixels(0, 0, lx, ly, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                 dst->getRawData());
  else {
    assert(TRaster64P(dst));
    glReadPixels(0, 0, lx, ly, GL_BGRA_EXT, GL_UNSIGNED_SHORT,
                 dst->getRawData());
  }

  assert(glGetError() == GL_NO_ERROR);
}

//--------------------------------------------------------

void ShadingContext::transformFeedback(int varyingsCount,
                                       const GLsizeiptr *varyingSizes,
                                       GLvoid **bufs) {
  // Generate buffer objects
  std::vector<GLuint> bufferObjectNames(varyingsCount, 0);

  glGenBuffers(varyingsCount, &bufferObjectNames[0]);

  for (int v = 0; v != varyingsCount; ++v) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjectNames[v]);
    glBufferData(GL_ARRAY_BUFFER, varyingSizes[v], bufs[v], GL_STATIC_READ);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, v, bufferObjectNames[v]);
  }

  // Draw
  GLuint Query = 0;

  glGenQueries(1, &Query);
  {
    // Disable rasterization, vertices processing only!
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, Query);

    glBeginTransformFeedback(GL_POINTS);
    glBegin(GL_POINTS);
    glVertex2f(0.0f, 0.0f);
    glEnd();
    glEndTransformFeedback();

    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glDisable(GL_RASTERIZER_DISCARD);
  }

  GLint count = 0;
  glGetQueryObjectiv(Query, GL_QUERY_RESULT, &count);

  glDeleteQueries(1, &Query);

  // Retrieve transformed data
  for (int v = 0; v != varyingsCount; ++v) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjectNames[v]);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, varyingSizes[v], bufs[v]);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Delete buffer objects
  glDeleteBuffers(varyingsCount, &bufferObjectNames[0]);
}
