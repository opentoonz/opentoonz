#pragma once

#ifndef SHADERINTERFACE_H
#define SHADERINTERFACE_H

// Glew includes
#include <GL/glew.h>  // Must be included before tgl.h

// TnzBase includes
#include "tparamuiconcept.h"

// TnzCore includes
#include "tpersist.h"
#include "tfilepath.h"
#include "tgl.h"

// Qt includes
#include <QString>
#include <QDateTime>
#include <QOpenGLShader>

// STD includes
#include <vector>

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

class QOpenGLShaderProgram;

//=========================================================

//**********************************************************************
//    ShaderInterface  declaration
//**********************************************************************

/*!
  A ShaderInterface represents interface data to a GLSL shader program.

\par Rationale

  OpenGL shaders are supplied as text to be compiled at run-time by a
  GLSL compiler. In our case, the specified text is assumed to be stored in
  one or more files on hard-disk.
\n\n
  However, in order for an executable to correctly interface a GLSL
  shader program, further interface data must be supplied in some form.

  Interface data include the path of the GLSL shader program file and
  parameters to be bound by the executable before the shader is run.

\par Usage

  A ShaderInterface is loaded from an xml file. Since an interface could
  be loaded incorrectly, remember to check the isValid() method to ensure
  that the loading process succeeds.

  A loaded ShaderInterface allows access to shader parameters and
  acts as a factory object to compiled QGLShaderProgram instances.
*/
class DVAPI ShaderInterface final : public TPersist {
public:  // Enums
  enum ParameterConceptType {
    CONCEPT_NONE,
    PERCENT,
    LENGTH,
    ANGLE,
    POINT,
    RADIUS_UI,
    WIDTH_UI,
    ANGLE_UI,
    POINT_UI,
    XY_UI,
    VECTOR_UI,
    POLAR_UI,
    SIZE_UI,
    QUAD_UI,
    RECT_UI,

    CONCEPTSCOUNT,
    UI_CONCEPTS = RADIUS_UI
  };

  enum ParameterType {
    PARAMETER_UNKNOWN,
    BOOL,
    FLOAT,
    VEC2,
    VEC3,
    VEC4,
    INT,
    IVEC2,
    IVEC3,
    IVEC4,
    RGBA,
    RGB,
    TYPESCOUNT
  };

  enum HandledWorldTransformsType { HWT_UNKNOWN, ANY, ISOTROPIC, HWTCOUNT };

public:  // Sub-classes
  class ParameterConcept final : public TPersist {
    PERSIST_DECLARATION(ParameterConcept)

  public:
    ParameterConceptType m_type;  //!< The concept type (typically composite)
    QString m_label;              //!< Name to be shown for ui concepts
    std::vector<QString>
        m_parameterNames;  //!< The involved parameters (by name)

  public:
    ParameterConcept() : m_type(CONCEPT_NONE) {}
    ParameterConcept(ParameterConceptType type, const QString &label)
        : m_type(type), m_label(label) {}

    bool isUI() const {
      return m_type >= UI_CONCEPTS && m_type < CONCEPTSCOUNT;
    }

  protected:
    void saveData(TOStream &os) override;
    void loadData(TIStream &is) override;
  };

  union ParameterValue {
    GLboolean m_bool;
    GLfloat m_float;
    GLfloat m_vec2[2];
    GLfloat m_vec3[3];
    GLfloat m_vec4[4];
    GLint m_int;
    GLint m_ivec2[2];
    GLint m_ivec3[3];
    GLint m_ivec4[4];
    GLubyte m_rgba[4];
    GLubyte m_rgb[3];
  };

  class Parameter final : public TPersist {
    PERSIST_DECLARATION(Parameter)

  public:
    ParameterType m_type;
    QString m_name;

    ParameterValue m_default;
    ParameterValue m_range[2];

    ParameterConcept m_concept;

  public:
    Parameter() : m_type(PARAMETER_UNKNOWN) {}
    Parameter(ParameterType type, const QString &name)
        : m_type(type), m_name(name) {}

  protected:
    void saveData(TOStream &os) override;
    void loadData(TIStream &is) override;
  };

  class ShaderData final : public TPersist {
    PERSIST_DECLARATION(ShaderData)

  public:
    QString m_name;    //!< A name associated to the shader action
    TFilePath m_path;  //!< The shader program's file path
    QOpenGLShader::ShaderType m_type;  //!< The shader type

  public:
    bool isValid() const { return !m_path.isEmpty(); }

  protected:
    void saveData(TOStream &os) override;
    void loadData(TIStream &is) override;
  };

public:  // Public methods
  ShaderInterface();

  bool isValid() const;

  const std::vector<Parameter> &parameters() const;
  const std::vector<QString> &inputPorts() const;

  const ShaderData &mainShader() const;
  const ShaderData &inputPortsShader() const;
  const ShaderData &bboxShader() const;

  HandledWorldTransformsType hwtType() const;

  /*!
Returns a compiled shader program against current OpenGL context, and the
last modified date of the associated shader file.
*/
  std::pair<QOpenGLShaderProgram *, QDateTime> makeProgram(
      const ShaderData &sd, int varyingsCount = 0,
      const GLchar **varyingNames = 0) const;

protected:
  void clear();

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

private:
  PERSIST_DECLARATION(ShaderInterface)

  ShaderData m_mainShader;              //!< Main (fragment) shader data
  std::vector<Parameter> m_parameters;  //!< List of parameters for the shader
  std::vector<ParameterConcept>
      m_parConcepts;  //!< List of (composite) parameter concepts

  std::vector<QString> m_ports;
  ShaderData m_portsShader;  //!< (Vertex) shader dedicated to input
                             //!< ports' geometries calculation
  ShaderData m_bboxShader;   //!< (Vertex) shader dedicated to extracting
                             //!< the fx's bbox
  HandledWorldTransformsType m_hwt;  //!< World transform types \a handled by
                                     //!< associated ShaderFx instances
};

#endif  // SHADERINTERFACE_H
