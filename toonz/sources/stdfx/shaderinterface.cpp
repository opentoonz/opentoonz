

// Glew include
#include <GL/glew.h>

// TnzCore includes
#include "tstream.h"
#include "tmsgcore.h"

// Qt includes
#include <QOpenGLShaderProgram>
#include <QOpenGLShader>
#include <QDir>

#include "stdfx/shaderinterface.h"

//=========================================================

PERSIST_IDENTIFIER(ShaderInterface, "ShaderInterface")
PERSIST_IDENTIFIER(ShaderInterface::ParameterConcept,
                   "ShaderInterface::ParameterConcept")
PERSIST_IDENTIFIER(ShaderInterface::Parameter, "ShaderInterface::Parameter")
PERSIST_IDENTIFIER(ShaderInterface::ShaderData, "ShaderInterface::ShaderData")

//**********************************************************************
//    Local Namespace stuff
//**********************************************************************

namespace {

// Filescope declarations

typedef std::pair<QOpenGLShaderProgram *, QDateTime> CompiledShader;

struct CaselessCompare {
  const QString &m_str;
  CaselessCompare(const QString &str) : m_str(str) {}
  bool operator()(const QString &str) const {
    return (m_str.compare(str, Qt::CaseInsensitive) == 0);
  }
};

// Filescope variables

const static QString l_typeNames[ShaderInterface::TYPESCOUNT] = {
    "",    "bool",  "float", "vec2",  "vec3", "vec4",
    "int", "ivec2", "ivec3", "ivec4", "rgba", "rgb"};

const static QString l_conceptNames[ShaderInterface::CONCEPTSCOUNT] = {
    "none",      "percent",  "length",   "angle",    "point",
    "radius_ui", "width_ui", "angle_ui", "point_ui", "xy_ui",
    "vector_ui", "polar_ui", "size_ui",  "quad_ui",  "rect_ui"};

const static QString l_hwtNames[ShaderInterface::HWTCOUNT] = {"none", "any",
                                                              "isotropic"};

enum Names {
  MAIN_PROGRAM,
  INPUT_PORTS,
  INPUT_PORT,
  PORTS_PROGRAM,
  PARAMETERS,
  PARAMETER,
  NAME,
  PROGRAM_FILE,
  CONCEPT,
  DEFAULT_,
  RANGE,
  HANDLED_WORLD_TRANSFORMS,
  BBOX_PROGRAM,
  NAMESCOUNT
};

const static std::string l_names[NAMESCOUNT] = {
    "MainProgram", "InputPorts", "InputPort", "PortsProgram",
    "Parameters",  "Parameter",  "Name",      "ProgramFile",
    "Concept",     "Default",    "Range",     "HandledWorldTransforms",
    "BBoxProgram"};

// Filescope functions

inline bool loadShader(QOpenGLShader::ShaderType type, const TFilePath &fp,
                       CompiledShader &cs) {
  QOpenGLShader *shader = new QOpenGLShader(type, cs.first);

  const QString &qfp = QString::fromStdWString(fp.getWideString());

  QFileInfo shaderFileInfo(qfp);
  cs.second = shaderFileInfo.lastModified();

  return shader->compileSourceFile(qfp) && cs.first->addShader(shader);
}

void dumpError(TIStream &is, const std::wstring &err = std::wstring()) {
  DVGui::info("Error reading " +
              QString::fromStdWString(is.getFilePath().getLevelNameW()) +
              " (line " + QString::number(is.getLine()) + ")" +
              (err.empty() ? QString() : QString::fromStdWString(L": " + err)));
}

void skipTag(TIStream &is, const std::string &tagName) {
  DVGui::info("Error reading " +
              QString::fromStdWString(is.getFilePath().getLevelNameW()) +
              " (line " + QString::number(is.getLine()) + "): Unknown tag '<" +
              QString::fromStdString(tagName) + ">'");

  is.skipCurrentTag();
}

}  // namespace

//**********************************************************************
//    ShaderInterface  implementation
//**********************************************************************

ShaderInterface::ShaderInterface() : m_hwt(ANY) {}

//---------------------------------------------------------

void ShaderInterface::clear() {
  m_mainShader = m_portsShader = ShaderData();
  m_parameters.clear();
}

//---------------------------------------------------------

bool ShaderInterface::isValid() const { return m_mainShader.isValid(); }

//---------------------------------------------------------

const std::vector<ShaderInterface::Parameter> &ShaderInterface::parameters()
    const {
  return m_parameters;
}

//---------------------------------------------------------

const std::vector<QString> &ShaderInterface::inputPorts() const {
  return m_ports;
}

//---------------------------------------------------------

const ShaderInterface::ShaderData &ShaderInterface::mainShader() const {
  return m_mainShader;
}

//---------------------------------------------------------

const ShaderInterface::ShaderData &ShaderInterface::inputPortsShader() const {
  return m_portsShader;
}

//---------------------------------------------------------

const ShaderInterface::ShaderData &ShaderInterface::bboxShader() const {
  return m_bboxShader;
}

//---------------------------------------------------------

ShaderInterface::HandledWorldTransformsType ShaderInterface::hwtType() const {
  return m_hwt;
}

//---------------------------------------------------------

std::pair<QOpenGLShaderProgram *, QDateTime> ShaderInterface::makeProgram(
    const ShaderData &sd, int varyingsCount,
    const GLchar **varyingNames) const {
  CompiledShader result;

  if (!isValid()) return result;

  result.first = new QOpenGLShaderProgram;

  ::loadShader(sd.m_type, sd.m_path, result);

  if (varyingsCount > 0)
    glTransformFeedbackVaryings(result.first->programId(), varyingsCount,
                                varyingNames, GL_INTERLEAVED_ATTRIBS);
  // NOTE: Since we'll be drawing a single vertex, GL_INTERLEAVED_ATTRIBS is
  // less restrictive than GL_SEPARATE_ATTRIBS.

  result.first->link();

  return result;
}

//---------------------------------------------------------

void ShaderInterface::saveData(TOStream &os) {
  struct locals {
    inline static TFilePath getRelativePath(const TFilePath &file,
                                            const TFilePath &relTo) {
      QDir relToDir(
          QString::fromStdWString(relTo.getParentDir().getWideString()));
      QString relFileStr(relToDir.relativeFilePath(
          QString::fromStdWString(file.getWideString())));
      return TFilePath(relFileStr.toStdWString());
    }
  };

  assert(isValid());

  if (isValid()) {
    os.openChild(l_names[MAIN_PROGRAM]);
    os << m_mainShader;
    os.closeChild();

    os.openChild(l_names[INPUT_PORTS]);
    {
      int i, iCount = int(m_ports.size());
      for (i = 0; i != iCount; ++i) {
        os.openChild(l_names[INPUT_PORT]);
        os << m_ports[i];
        os.closeChild();
      }

      if (m_portsShader.isValid()) {
        os.openChild(l_names[PORTS_PROGRAM]);
        os << m_portsShader;
        os.closeChild();
      }
    }
    os.closeChild();

    if (m_bboxShader.isValid()) {
      os.openChild(l_names[BBOX_PROGRAM]);
      os << m_bboxShader;
      os.closeChild();
    }

    if (m_hwt != ANY) {
      os.openChild(l_names[HANDLED_WORLD_TRANSFORMS]);
      os << l_names[m_hwt];
      os.closeChild();
    }

    os.openChild(l_names[PARAMETERS]);
    {
      int p, pCount = int(m_parameters.size());
      for (p = 0; p != pCount; ++p) {
        os.openChild(l_names[PARAMETER]);
        os << m_parameters[p];
        os.closeChild();
      }
    }
    os.closeChild();
  }
}

//---------------------------------------------------------

void ShaderInterface::loadData(TIStream &is) {
  struct locals {
    inline static TFilePath getAbsolutePath(const TFilePath &file,
                                            const TFilePath &relTo) {
      QDir relToDir(
          QString::fromStdWString(relTo.getParentDir().getWideString()));
      QString absFileStr(relToDir.absoluteFilePath(
          QString::fromStdWString(file.getWideString())));
      return TFilePath(absFileStr.toStdWString());
    }

    static bool nameMatch(const QString &name, const Parameter &param) {
      return (name == param.m_name);
    }
  };

  std::string tagName;

  try {
    while (is.openChild(tagName)) {
      if (tagName == l_names[MAIN_PROGRAM]) {
        is >> m_mainShader;
        m_mainShader.m_type = QOpenGLShader::Fragment;
        is.closeChild();
      } else if (tagName == l_names[INPUT_PORTS]) {
        while (is.openChild(tagName)) {
          if (tagName == l_names[INPUT_PORT]) {
            QString portName;
            is >> portName;

            m_ports.push_back(portName);

            is.closeChild();
          } else if (tagName == l_names[PORTS_PROGRAM]) {
            is >> m_portsShader;
            m_portsShader.m_type = QOpenGLShader::Vertex;
            is.closeChild();
          } else
            ::skipTag(is, tagName);
        }

        is.closeChild();
      } else if (tagName == l_names[BBOX_PROGRAM]) {
        is >> m_bboxShader;
        m_bboxShader.m_type = QOpenGLShader::Vertex;
        is.closeChild();
      } else if (tagName == l_names[HANDLED_WORLD_TRANSFORMS]) {
        QString hwtName;
        is >> hwtName;

        m_hwt = HandledWorldTransformsType(
            std::find_if(l_hwtNames, l_hwtNames + HWTCOUNT,
                         ::CaselessCompare(hwtName)) -
            l_hwtNames);

        if (m_hwt == HWTCOUNT) {
          m_hwt = HWT_UNKNOWN;
          ::dumpError(is, L"Unrecognized HandledWorldTransforms type '" +
                              hwtName.toStdWString() + L"'");
        }

        is.closeChild();
      } else if (tagName == l_names[PARAMETERS]) {
        while (is.openChild(tagName)) {
          if (tagName == l_names[PARAMETER]) {
            Parameter param;
            is >> param;

            m_parameters.push_back(param);

            is.closeChild();
          } else
            ::skipTag(is, tagName);
        }

        is.closeChild();
      } else if (tagName == l_names[CONCEPT]) {
        ParameterConcept concept;
        is >> concept;

        m_parConcepts.push_back(concept);

        is.closeChild();
      } else
        ::skipTag(is, tagName);
    };
  } catch (const TException &e) {
    ::dumpError(is, e.getMessage());
    clear();
  } catch (...) {
    ::dumpError(is);
    clear();
  }
}

//**********************************************************************
//    ShaderInterface::ShaderData  implementation
//**********************************************************************

void ShaderInterface::ShaderData::saveData(TOStream &os) {
  struct locals {
    inline static TFilePath getRelativePath(const TFilePath &file,
                                            const TFilePath &relTo) {
      QDir relToDir(
          QString::fromStdWString(relTo.getParentDir().getWideString()));
      QString relFileStr(relToDir.relativeFilePath(
          QString::fromStdWString(file.getWideString())));
      return TFilePath(relFileStr.toStdWString());
    }
  };

  os.openChild(l_names[NAME]);
  os << m_name;
  os.closeChild();

  os.openChild(l_names[PROGRAM_FILE]);
  os << locals::getRelativePath(m_path, os.getFilePath());
  os.closeChild();
}

//---------------------------------------------------------

void ShaderInterface::ShaderData::loadData(TIStream &is) {
  struct locals {
    inline static TFilePath getAbsolutePath(const TFilePath &file,
                                            const TFilePath &relTo) {
      QDir relToDir(
          QString::fromStdWString(relTo.getParentDir().getWideString()));
      QString absFileStr(relToDir.absoluteFilePath(
          QString::fromStdWString(file.getWideString())));
      return TFilePath(absFileStr.toStdWString());
    }
  };

  std::string tagName;

  while (is.openChild(tagName)) {
    if (tagName == l_names[NAME])
      is >> m_name, is.closeChild();
    else if (tagName == l_names[PROGRAM_FILE]) {
      is >> m_path;
      m_path = locals::getAbsolutePath(m_path, is.getFilePath());
      is.closeChild();
    } else
      ::skipTag(is, tagName);
  }
}

//**********************************************************************
//    ShaderInterface::ParameterConcept  implementation
//**********************************************************************

void ShaderInterface::ParameterConcept::saveData(TOStream &os) {
  os << l_conceptNames[m_type];

  if (!m_label.isEmpty()) {
    os.openChild(l_names[NAME]);
    os << m_label;
    os.closeChild();
  }

  int n, nCount = int(m_parameterNames.size());
  for (n = 0; n != nCount; ++n) {
    os.openChild(l_names[PARAMETER]);
    os << m_parameterNames[n];
    os.closeChild();
  }
}

//---------------------------------------------------------

void ShaderInterface::ParameterConcept::loadData(TIStream &is) {
  // Read the concept type
  QString conceptName;
  is >> conceptName;

  m_type = ParameterConceptType(std::find_if(l_conceptNames,
                                             l_conceptNames + CONCEPTSCOUNT,
                                             ::CaselessCompare(conceptName)) -
                                l_conceptNames);

  if (m_type == CONCEPTSCOUNT) {
    m_type = CONCEPT_NONE;
    ::dumpError(
        is, L"Unrecognized concept type '" + conceptName.toStdWString() + L"'");
  }

  // Read any associated parameter names
  std::string tagName;

  while (is.openChild(tagName)) {
    if (tagName == l_names[PARAMETER]) {
      QString name;
      is >> name;

      m_parameterNames.push_back(name);

      is.closeChild();
    } else if (tagName == l_names[NAME])
      is >> m_label, is.closeChild();
    else
      ::skipTag(is, tagName);
  }
}

//**********************************************************************
//    ShaderInterface::Parameter  implementation
//**********************************************************************

void ShaderInterface::Parameter::saveData(TOStream &os) {
  os << l_typeNames[m_type] << m_name;

  os.openChild(l_names[CONCEPT]);
  os << m_concept;
  os.closeChild();

  os.openChild(l_names[DEFAULT_]);

  switch (m_type) {
  case BOOL:
    os << (int)m_default.m_bool;
    break;
  case FLOAT:
    os << (double)m_default.m_float;
    break;
  case VEC2:
    os << (double)m_default.m_vec2[0] << (double)m_default.m_vec2[1];
    break;
  case VEC3:
    os << (double)m_default.m_vec3[0] << (double)m_default.m_vec3[1]
       << (double)m_default.m_vec3[2];
    break;
  case VEC4:
    os << (double)m_default.m_vec4[0] << (double)m_default.m_vec4[1]
       << (double)m_default.m_vec4[2] << (double)m_default.m_vec4[3];
    break;
  case INT:
    os << m_default.m_int;
    break;
  case IVEC2:
    os << m_default.m_ivec2[0] << m_default.m_ivec2[1];
    break;
  case IVEC3:
    os << m_default.m_ivec3[0] << m_default.m_ivec3[1] << m_default.m_ivec3[2];
    break;
  case IVEC4:
    os << m_default.m_ivec4[0] << m_default.m_ivec4[1] << m_default.m_ivec4[2]
       << m_default.m_ivec4[3];
    break;
  case RGBA:
    os << (int)m_default.m_rgba[0] << (int)m_default.m_rgba[1]
       << (int)m_default.m_rgba[2] << (int)m_default.m_rgba[3];
    break;
  case RGB:
    os << (int)m_default.m_rgb[0] << (int)m_default.m_rgb[1]
       << (int)m_default.m_rgb[2];
    break;
  }

  os.closeChild();

  os.openChild(l_names[RANGE]);

  switch (m_type) {
  case FLOAT:
    os << (double)m_range[0].m_float << (double)m_range[1].m_float;
    break;
  case VEC2:
    os << (double)m_range[0].m_vec2[0] << (double)m_range[1].m_vec2[0]
       << (double)m_range[0].m_vec2[1] << (double)m_range[1].m_vec2[1];
    break;
  case VEC3:
    os << (double)m_range[0].m_vec3[0] << (double)m_range[1].m_vec3[0]
       << (double)m_range[0].m_vec3[1] << (double)m_range[1].m_vec3[1]
       << (double)m_range[0].m_vec3[2] << (double)m_range[1].m_vec3[2];
    break;
  case VEC4:
    os << (double)m_range[0].m_vec4[0] << (double)m_range[1].m_vec4[0]
       << (double)m_range[0].m_vec4[1] << (double)m_range[1].m_vec4[1]
       << (double)m_range[0].m_vec4[2] << (double)m_range[1].m_vec4[2]
       << (double)m_range[0].m_vec4[3] << (double)m_range[1].m_vec4[3];
    break;
  case INT:
    os << m_range[0].m_int << m_range[1].m_int;
    break;
  case IVEC2:
    os << m_range[0].m_ivec2[0] << m_range[1].m_ivec2[0]
       << m_range[0].m_ivec2[1] << m_range[1].m_ivec2[1];
    break;
  case IVEC3:
    os << m_range[0].m_ivec3[0] << m_range[1].m_ivec3[0]
       << m_range[0].m_ivec3[1] << m_range[1].m_ivec3[1]
       << m_range[0].m_ivec3[2] << m_range[1].m_ivec3[2];
    break;
  case IVEC4:
    os << m_range[0].m_ivec4[0] << m_range[1].m_ivec4[0]
       << m_range[0].m_ivec4[1] << m_range[1].m_ivec4[1]
       << m_range[0].m_ivec4[2] << m_range[1].m_ivec4[2]
       << m_range[0].m_ivec4[3] << m_range[1].m_ivec4[3];
    break;
  }

  os.closeChild();
}

//---------------------------------------------------------

void ShaderInterface::Parameter::loadData(TIStream &is) {
  // Load type and name

  QString typeName;
  is >> typeName >> m_name;

  m_type = ShaderInterface::ParameterType(
      std::find_if(l_typeNames, l_typeNames + TYPESCOUNT,
                   ::CaselessCompare(typeName)) -
      l_typeNames);

  if (m_type == TYPESCOUNT)
    throw TException(L"Unrecognized parameter type '" +
                     typeName.toStdWString() + L"'");

  // Load range
  std::string tagName;

  // Initialize default values
  m_concept = ParameterConcept();

  switch (m_type) {
  case BOOL:
    m_default.m_bool = false;
    break;
  case FLOAT:
    m_default.m_float  = 0.0;
    m_range[0].m_float = -(std::numeric_limits<GLfloat>::max)();
    m_range[1].m_float = (std::numeric_limits<GLfloat>::max)();
    break;
  case VEC2:
    m_default.m_vec2[0] = m_default.m_vec2[1] = 0.0;
    m_range[0].m_vec2[0]                      = m_range[0].m_vec2[1] =
        -(std::numeric_limits<GLfloat>::max)();
    m_range[1].m_vec2[0] = m_range[1].m_vec2[1] =
        (std::numeric_limits<GLfloat>::max)();
    break;
  case VEC3:
    m_default.m_vec3[0] = m_default.m_vec3[1] = m_default.m_vec3[1] = 0.0;
    m_range[0].m_vec3[0] = m_range[0].m_vec3[1] = m_range[0].m_vec3[2] =
        -(std::numeric_limits<GLfloat>::max)();
    m_range[1].m_vec3[0] = m_range[1].m_vec3[1] = m_range[1].m_vec3[2] =
        (std::numeric_limits<GLfloat>::max)();
    break;
  case VEC4:
    m_default.m_vec4[0] = m_default.m_vec4[1] = m_default.m_vec4[1] =
        m_default.m_vec4[1]                   = 0.0;
    m_range[0].m_vec4[0] = m_range[0].m_vec4[1] = m_range[0].m_vec4[2] =
        m_range[0].m_vec4[3] = -(std::numeric_limits<GLfloat>::max)();
    m_range[1].m_vec4[0] = m_range[1].m_vec4[1] = m_range[1].m_vec4[2] =
        m_range[1].m_vec4[3] = (std::numeric_limits<GLfloat>::max)();
    break;
  case INT:
    m_default.m_int  = 0;
    m_range[0].m_int = -(std::numeric_limits<GLint>::max)();
    m_range[1].m_int = (std::numeric_limits<GLint>::max)();
    break;
  case IVEC2:
    m_default.m_ivec2[0] = m_default.m_ivec2[1] = 0;
    m_range[0].m_ivec2[0]                       = m_range[0].m_ivec2[1] =
        -(std::numeric_limits<GLint>::max)();
    m_range[1].m_ivec2[0] = m_range[1].m_ivec2[1] =
        (std::numeric_limits<GLint>::max)();
    break;
  case IVEC3:
    m_default.m_ivec3[0] = m_default.m_ivec3[1] = m_default.m_ivec3[1] = 0;
    m_range[0].m_ivec3[0] = m_range[0].m_ivec3[1] = m_range[0].m_ivec3[2] =
        -(std::numeric_limits<GLint>::max)();
    m_range[1].m_ivec3[0] = m_range[1].m_ivec3[1] = m_range[1].m_ivec3[2] =
        (std::numeric_limits<GLint>::max)();
    break;
  case IVEC4:
    m_default.m_ivec4[0] = m_default.m_ivec4[1] = m_default.m_ivec4[1] =
        m_default.m_ivec4[1]                    = 0;
    m_range[0].m_ivec4[0] = m_range[0].m_ivec4[1] = m_range[0].m_ivec4[2] =
        m_range[0].m_ivec4[3] = -(std::numeric_limits<GLint>::max)();
    m_range[1].m_ivec4[0] = m_range[1].m_ivec4[1] = m_range[1].m_ivec4[2] =
        m_range[1].m_ivec4[3] = (std::numeric_limits<GLint>::max)();
    break;
  case RGBA:
    m_default.m_rgba[0] = m_default.m_rgba[1] = m_default.m_rgba[2] =
        m_default.m_rgba[3]                   = 255;
    break;
  case RGB:
    m_default.m_rgb[0] = m_default.m_rgb[1] = m_default.m_rgb[2] = 255;
    break;
  }

  // Attempt loading range from file
  while (is.openChild(tagName)) {
    if (tagName == l_names[CONCEPT])
      is >> m_concept, is.closeChild();
    else if (tagName == l_names[DEFAULT_]) {
      switch (m_type) {
      case BOOL: {
        int val;
        is >> val, m_default.m_bool = val;
        break;
      }

      case FLOAT: {
        double val;
        is >> val, m_default.m_float = val;
        break;
      }

      case VEC2: {
        double val;
        is >> val, m_default.m_vec2[0] = val;
        is >> val, m_default.m_vec2[1] = val;
        break;
      }

      case VEC3: {
        double val;
        is >> val, m_default.m_vec3[0] = val;
        is >> val, m_default.m_vec3[1] = val;
        is >> val, m_default.m_vec3[2] = val;
        break;
      }

      case VEC4: {
        double val;
        is >> val, m_default.m_vec4[0] = val;
        is >> val, m_default.m_vec4[1] = val;
        is >> val, m_default.m_vec4[2] = val;
        is >> val, m_default.m_vec4[3] = val;
        break;
      }

      case INT:
        is >> m_default.m_int;
        break;

      case IVEC2:
        is >> m_default.m_ivec2[0] >> m_default.m_ivec2[1];
        break;

      case IVEC3:
        is >> m_default.m_ivec3[0] >> m_default.m_ivec3[1] >>
            m_default.m_ivec3[2];
        break;

      case IVEC4:
        is >> m_default.m_ivec4[0] >> m_default.m_ivec4[1] >>
            m_default.m_ivec4[2] >> m_default.m_ivec4[3];
        break;

      case RGBA: {
        int val;
        is >> val, m_default.m_rgba[0] = val;
        is >> val, m_default.m_rgba[1] = val;
        is >> val, m_default.m_rgba[2] = val;
        is >> val, m_default.m_rgba[3] = val;
        break;
      }

      case RGB: {
        int val;
        is >> val, m_default.m_rgb[0] = val;
        is >> val, m_default.m_rgb[1] = val;
        is >> val, m_default.m_rgb[2] = val;
        break;
      }
      }

      is.closeChild();
    } else if (tagName == l_names[RANGE]) {
      switch (m_type) {
      case FLOAT: {
        double val;
        is >> val, m_range[0].m_float = val;
        is >> val, m_range[1].m_float = val;
        break;
      }

      case VEC2: {
        double val;
        is >> val, m_range[0].m_vec2[0] = val;
        is >> val, m_range[1].m_vec2[0] = val;
        is >> val, m_range[0].m_vec2[1] = val;
        is >> val, m_range[1].m_vec2[1] = val;
        break;
      }

      case VEC3: {
        double val;
        is >> val, m_range[0].m_vec3[0] = val;
        is >> val, m_range[1].m_vec3[0] = val;
        is >> val, m_range[0].m_vec3[1] = val;
        is >> val, m_range[1].m_vec3[1] = val;
        is >> val, m_range[0].m_vec3[2] = val;
        is >> val, m_range[1].m_vec3[2] = val;
        break;
      }

      case VEC4: {
        double val;
        is >> val, m_range[0].m_vec4[0] = val;
        is >> val, m_range[1].m_vec4[0] = val;
        is >> val, m_range[0].m_vec4[1] = val;
        is >> val, m_range[1].m_vec4[1] = val;
        is >> val, m_range[0].m_vec4[2] = val;
        is >> val, m_range[1].m_vec4[2] = val;
        is >> val, m_range[0].m_vec4[3] = val;
        is >> val, m_range[1].m_vec4[3] = val;
        break;
      }

      case INT:
        is >> m_range[0].m_int >> m_range[1].m_int;
        break;

      case IVEC2:
        is >> m_range[0].m_ivec2[0] >> m_range[1].m_ivec2[0] >>
            m_range[0].m_ivec2[1] >> m_range[1].m_ivec2[1];
        break;

      case IVEC3:
        is >> m_range[0].m_ivec3[0] >> m_range[1].m_ivec3[0] >>
            m_range[0].m_ivec3[1] >> m_range[1].m_ivec3[1] >>
            m_range[0].m_ivec3[2] >> m_range[1].m_ivec3[2];
        break;

      case IVEC4:
        is >> m_range[0].m_ivec4[0] >> m_range[1].m_ivec4[0] >>
            m_range[0].m_ivec4[1] >> m_range[1].m_ivec4[1] >>
            m_range[0].m_ivec4[2] >> m_range[1].m_ivec4[2] >>
            m_range[0].m_ivec4[3] >> m_range[1].m_ivec4[3];
        break;
      }

      is.closeChild();
    } else
      ::skipTag(is, tagName);
  }

  // Post-process loaded data
  if (m_concept.m_label.isEmpty()) m_concept.m_label = m_name;
}
