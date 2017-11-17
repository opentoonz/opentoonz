#pragma once

#ifndef SHADINGCONTEXT_H
#define SHADINGCONTEXT_H

#include <memory>

// Glew include
#include <GL/glew.h>

// TnzCore includes
#include "traster.h"

// Qt includes
#include <QDateTime>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLWidget>

#undef DVAPI
#undef DVVAR
#ifdef TNZSTDFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//    Forward declarations

class QObject;
class QOpenGLShaderProgram;
class QDateTime;
class QOffscreenSurface;

//=========================================================

class DVAPI ShadingContext {
public:
  enum Support { OK, NO_PIXEL_BUFFER, NO_SHADERS };

public:
  ShadingContext(QOffscreenSurface *);
  ~ShadingContext();

  //! Returns the status of OpenGL shading support.
  static Support support();

  bool isValid() const;

  void makeCurrent();
  void doneCurrent();

  /*!
Resizes the output buffer to the specified size. Requires that
the contex is made current before invocation. In case lx or ly are 0,
the context's output buffer is destroyed.
*/
  void resize(int lx, int ly, const QOpenGLFramebufferObjectFormat &fmt =
                                  QOpenGLFramebufferObjectFormat());

  QOpenGLFramebufferObjectFormat format() const;
  TDimension size() const;

  //! Surrenders ownership of the supplied shader program to the shading
  //! context.
  void addShaderProgram(const QString &shaderName,
                        QOpenGLShaderProgram *program);
  void addShaderProgram(const QString &shaderName,
                        QOpenGLShaderProgram *program,
                        const QDateTime &lastModified);
  bool removeShaderProgram(const QString &shaderName);

  QOpenGLShaderProgram *shaderProgram(const QString &shaderName) const;
  QDateTime lastModified(const QString &shaderName) const;

  std::pair<QOpenGLShaderProgram *, QDateTime> shaderData(
      const QString &shaderName) const;

  GLuint loadTexture(const TRasterP &src, GLuint texUnit);  //!< Loads a texture
                                                            //! and binds it to
  //! the specified
  //! texture unit.
  //!  \return  The OpenGL texture id of the loaded texture.      \param src
  //!  Loaded texture.  \param texUnit  Unit the texture will be bound to.
  void unloadTexture(GLuint texId);  //!< Releases the specified texture id.

  //! Renders the active shader program to the specified raster.
  void draw(const TRasterP &dst);

  //! Performs transform feedback with the specified varying variables
  void transformFeedback(int varyingsCount, const GLsizeiptr *varyingSizes,
                         GLvoid **bufs);

private:
  struct Imp;
  std::unique_ptr<Imp> m_imp;

  // Not copyable
  ShadingContext(const ShadingContext &);
  ShadingContext &operator=(const ShadingContext &);
};

class TQOpenGLWidget : public QOpenGLWidget {
public:
  TQOpenGLWidget();
  void initializeGL() override;
};

#endif  // SHADINGCONTEXT_H
