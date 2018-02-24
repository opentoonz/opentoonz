#pragma once

#ifndef LUT_CALIBRATOR_H
#define LUT_CALIBRATOR_H

#include "tcommon.h"
#include "tpixelutils.h"

#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QOpenGLFunctions>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QOpenGLShader;
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;
class QOpenGLTexture;

class QColor;

class DVAPI LutCalibrator : public QOpenGLFunctions  // sigleton
{
  bool m_isValid = false;

  struct LutTextureShader {
    QOpenGLShader* vert           = nullptr;
    QOpenGLShader* frag           = nullptr;
    QOpenGLShaderProgram* program = nullptr;
    int texUniform                = -1;
    int lutUniform                = -1;
    int lutSizeUniform            = -1;
    int vertexAttrib              = -1;
    int texCoordAttrib            = -1;
  } m_shader;

  struct Lut {
    int meshSize;
    float* data         = NULL;
    QOpenGLTexture* tex = NULL;
  } m_lut;

  QOpenGLBuffer m_viewerVBO;
  LutCalibrator() {}

  bool initializeLutTextureShader();
  void createViewerVBO();
  bool loadLutFile(const QString& path);
  void assignLutTexture();

public:
  static LutCalibrator* instance();

  // to be computed once through the software
  void initialize();
  void finalize();
  bool isValid() { return m_isValid; }

  ~LutCalibrator() { finalize(); }

  void onEndDraw(QOpenGLFramebufferObject*);

  QString& getMonitorName() const;

  void convert(float&, float&, float&);
  void convert(QColor&);
  void convert(TPixel32&);
};

#endif