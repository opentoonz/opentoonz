#include "toonzqt/lutcalibrator.h"

// Tnzlib includes
#include "toonz/preferences.h"

// TnzCore includes
#include "tmsgcore.h"

#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
inline bool execWarning(const QString& s) {
  DVGui::MsgBox(DVGui::WARNING, s);
  return false;
}

constexpr int kMax3DLutSize = 129;

struct ParsedLut {
  int meshSize = 0;
  std::vector<float> data;
  float domainMin[3] = {0.0f, 0.0f, 0.0f};
  float domainMax[3] = {1.0f, 1.0f, 1.0f};
};

bool readDataLine(QTextStream& stream, QString& line, int& lineNumber) {
  while (!stream.atEnd()) {
    ++lineNumber;
    line = stream.readLine().trimmed();
    if (!line.isEmpty() && !line.startsWith('#')) return true;
  }
  return false;
}

QStringList splitFields(const QString& line) {
  return line.simplified().split(' ', Qt::SkipEmptyParts);
}

bool parseFiniteFloat(const QString& text, float& value) {
  bool ok = false;
  value   = text.toFloat(&ok);
  return ok && std::isfinite(value);
}

float clamp01(float value) {
  return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

bool parseFloatTriple(const QStringList& fields, float values[3]) {
  if (fields.size() != 3) return false;
  for (int channel = 0; channel < 3; ++channel)
    if (!parseFiniteFloat(fields.at(channel), values[channel])) return false;
  return true;
}

QString lineError(int lineNumber, const QString& message) {
  return QObject::tr("Line %1: %2").arg(lineNumber).arg(message);
}

bool parse3dl(QTextStream& stream, ParsedLut& lut, QString& error) {
  QString line;
  int lineNumber = 0;

  if (!readDataLine(stream, line, lineNumber) || line != "3DMESH") {
    error = lineError(lineNumber, QObject::tr("Expected the 3DMESH keyword."));
    return false;
  }

  if (!readDataLine(stream, line, lineNumber)) {
    error = QObject::tr("The Mesh header is missing.");
    return false;
  }

  QStringList fields = splitFields(line);
  bool inputOk = false, outputOk = false;
  int inputBitDepth  = fields.size() == 3 ? fields.at(1).toInt(&inputOk) : 0;
  int outputBitDepth = fields.size() == 3 ? fields.at(2).toInt(&outputOk) : 0;
  if (fields.size() != 3 || fields.at(0) != "Mesh" || !inputOk || !outputOk ||
      inputBitDepth < 0 || inputBitDepth > 7 || outputBitDepth < 1 ||
      outputBitDepth > 30) {
    error = lineError(
        lineNumber,
        QObject::tr("Expected Mesh [input bit depth] [output bit depth]."));
    return false;
  }

  lut.meshSize = (1 << inputBitDepth) + 1;
  if (lut.meshSize > kMax3DLutSize) {
    error = QObject::tr("The LUT grid may not exceed %1 points per axis.")
                .arg(kMax3DLutSize);
    return false;
  }

  if (!readDataLine(stream, line, lineNumber)) {
    error = QObject::tr("The input grid line is missing.");
    return false;
  }
  fields = splitFields(line);
  if (fields.size() != lut.meshSize) {
    error = lineError(lineNumber,
                      QObject::tr("The input grid has the wrong size."));
    return false;
  }

  const size_t entryCount =
      static_cast<size_t>(lut.meshSize) * lut.meshSize * lut.meshSize;
  lut.data.resize(entryCount * 3);
  const float maxValue = std::ldexp(1.0f, outputBitDepth) - 1.0f;

  size_t entry = 0;
  for (int r = 0; r < lut.meshSize; ++r) {
    for (int g = 0; g < lut.meshSize; ++g) {
      for (int b = 0; b < lut.meshSize; ++b, ++entry) {
        if (!readDataLine(stream, line, lineNumber)) {
          error = QObject::tr("The LUT contains %1 entries; expected %2.")
                      .arg(static_cast<qulonglong>(entry))
                      .arg(static_cast<qulonglong>(entryCount));
          return false;
        }
        fields = splitFields(line);
        if (fields.size() != 3) {
          error = lineError(
              lineNumber, QObject::tr("Expected three integer color values."));
          return false;
        }
        const size_t offset =
            (static_cast<size_t>(b) * lut.meshSize * lut.meshSize +
             static_cast<size_t>(g) * lut.meshSize + r) *
            3;
        for (int channel = 0; channel < 3; ++channel) {
          bool ok         = false;
          const int value = fields.at(channel).toInt(&ok);
          if (!ok) {
            error =
                lineError(lineNumber,
                          QObject::tr("Expected three integer color values."));
            return false;
          }
          lut.data[offset + channel] = static_cast<float>(value) / maxValue;
        }
      }
    }
  }

  return true;
}

bool parseCube(QTextStream& stream, ParsedLut& lut, QString& error) {
  QString line;
  int lineNumber     = 0;
  bool hasSize       = false;
  bool dataHasBegun  = false;
  bool hasDomainTags = false;
  bool hasRangeTag   = false;

  while (readDataLine(stream, line, lineNumber)) {
    const QStringList fields = splitFields(line);
    float firstValue         = 0.0f;
    if (parseFiniteFloat(fields.at(0), firstValue)) {
      if (!hasSize) {
        error = lineError(lineNumber,
                          QObject::tr("LUT_3D_SIZE must precede LUT data."));
        return false;
      }
      dataHasBegun = true;
      float values[3];
      if (!parseFloatTriple(fields, values)) {
        error = lineError(lineNumber,
                          QObject::tr("Expected three floating-point values."));
        return false;
      }
      lut.data.insert(lut.data.end(), values, values + 3);
      const size_t expectedValues =
          static_cast<size_t>(lut.meshSize) * lut.meshSize * lut.meshSize * 3;
      if (lut.data.size() > expectedValues) {
        error = QObject::tr("The .cube file contains too many LUT entries.");
        return false;
      }
      continue;
    }

    if (dataHasBegun) {
      error = lineError(lineNumber,
                        QObject::tr("Only color triples may follow LUT data."));
      return false;
    }

    const QString keyword = fields.at(0).toUpper();
    if (keyword == "TITLE") {
      continue;
    } else if (keyword == "LUT_1D_SIZE" || keyword == "LUT_1D_INPUT_RANGE") {
      error =
          lineError(lineNumber,
                    QObject::tr("1D and shaper .cube LUTs are not supported."));
      return false;
    } else if (keyword == "LUT_2D_SIZE") {
      error = lineError(lineNumber,
                        QObject::tr("2D .cube LUTs are not supported."));
      return false;
    } else if (keyword == "LUT_3D_SIZE") {
      bool ok        = false;
      const int size = fields.size() == 2 ? fields.at(1).toInt(&ok) : 0;
      if (!ok || size < 2 || size > kMax3DLutSize || hasSize) {
        error = lineError(
            lineNumber,
            QObject::tr("LUT_3D_SIZE must be a single value from 2 to %1.")
                .arg(kMax3DLutSize));
        return false;
      }
      lut.meshSize = size;
      lut.data.reserve(static_cast<size_t>(size) * size * size * 3);
      hasSize = true;
    } else if (keyword == "DOMAIN_MIN" || keyword == "DOMAIN_MAX") {
      float values[3];
      if (hasRangeTag || fields.size() != 4 ||
          !parseFloatTriple(fields.mid(1), values)) {
        error = lineError(
            lineNumber,
            QObject::tr("%1 must contain three floating-point values and may "
                        "not be combined with LUT_3D_INPUT_RANGE.")
                .arg(keyword));
        return false;
      }
      float* domain = keyword == "DOMAIN_MIN" ? lut.domainMin : lut.domainMax;
      std::copy(values, values + 3, domain);
      hasDomainTags = true;
    } else if (keyword == "LUT_3D_INPUT_RANGE") {
      float values[2];
      if (hasDomainTags || fields.size() != 3 ||
          !parseFiniteFloat(fields.at(1), values[0]) ||
          !parseFiniteFloat(fields.at(2), values[1])) {
        error = lineError(
            lineNumber,
            QObject::tr("LUT_3D_INPUT_RANGE must contain two floating-point "
                        "values and may not be combined with DOMAIN_MIN/MAX."));
        return false;
      }
      for (int channel = 0; channel < 3; ++channel) {
        lut.domainMin[channel] = values[0];
        lut.domainMax[channel] = values[1];
      }
      hasRangeTag = true;
    } else {
      error = lineError(lineNumber,
                        QObject::tr("Unsupported .cube header: %1").arg(line));
      return false;
    }
  }

  if (!hasSize) {
    error = QObject::tr("The .cube file does not contain LUT_3D_SIZE.");
    return false;
  }

  const size_t expectedValues =
      static_cast<size_t>(lut.meshSize) * lut.meshSize * lut.meshSize * 3;
  if (lut.data.size() != expectedValues) {
    error = QObject::tr("The .cube file contains %1 entries; expected %2.")
                .arg(static_cast<qulonglong>(lut.data.size() / 3))
                .arg(static_cast<qulonglong>(expectedValues / 3));
    return false;
  }

  for (int channel = 0; channel < 3; ++channel) {
    if (lut.domainMin[channel] >= lut.domainMax[channel]) {
      error = QObject::tr(
          "Each .cube input-domain minimum must be less than "
          "its maximum.");
      return false;
    }
  }

  return true;
}
};  // namespace

#ifdef WIN32

#include <QSettings>
#include <QByteArray>

namespace {
// obtain monitor information from registry
QStringList getMonitorNames() {
  QStringList subPathSet;
  // QSettings regSys("SYSTEM\\CurrentControlSet\\Enum\\DISPLAY",
  // QSettings::NativeFormat);
  QSettings regSys(
      "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Enum\\DISPLAY",
      QSettings::NativeFormat);
  QStringList children = regSys.childGroups();
  // return value
  QStringList nameList;

  if (children.isEmpty()) {
    std::cout << "getMonitorNames : Failed to Open Registry" << std::endl;
    return nameList;
  }

  for (int c = 0; c < children.size(); c++) {
    // Parent Key : DISPLAY
    // Child Keys : AVO0000, ENC2174, etc.
    // Grandchild Key : 5&388..
    // Find grandchild key which contains a great-grandchild key named "Control"

    regSys.beginGroup(children.at(c));  // Child keys : AVO0000, ENC2174, etc.
    QStringList grandChildren = regSys.childGroups();
    for (int gc = 0; gc < grandChildren.size(); gc++) {
      regSys.beginGroup(grandChildren.at(gc));  // Grandchild key : 5&388..

      QStringList greatGrandChildren = regSys.childGroups();

      if (greatGrandChildren.contains(
              "Control"))  // If the key "Control" is found
      {
        // Obtain variable "EDID" from the key "Device Parameters"
        regSys.beginGroup("Device Parameters");

        // the key may be not "EDID", but "BAD_EDID"
        if (regSys.contains("EDID")) {
          QString subPath = regSys.group().replace("/", "\\").prepend(
              "SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\");
          subPathSet.push_back(subPath);
        }
        regSys.endGroup();
      }
      regSys.endGroup();
    }
    // subPath may not be one...?
    // if(!subPath.isEmpty())
    //	break;
    regSys.endGroup();
  }

  if (subPathSet.isEmpty()) {
    std::cout << "getMonitorNames : Failed to Find Current EDID" << std::endl;
    return nameList;
  }

  // for each subPath ( it may become more than one when using submonitor )
  for (int sp = 0; sp < subPathSet.size(); sp++) {
    QString subPath = subPathSet.at(sp);

    HKEY handle = 0;

    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                             reinterpret_cast<const wchar_t*>(subPath.utf16()),
                             0, KEY_READ, &handle);

    if (res == ERROR_SUCCESS && handle) {
      QString keyStr("EDID");

      // get the size and type of the value
      DWORD dataType;
      DWORD dataSize;
      LONG res = RegQueryValueExW(
          handle, reinterpret_cast<const wchar_t*>(keyStr.utf16()), 0,
          &dataType, 0, &dataSize);

      if (res != ERROR_SUCCESS) {
        RegCloseKey(handle);
        continue;
      }

      // get the value
      QByteArray ba(dataSize, 0);
      res = RegQueryValueExW(
          handle, reinterpret_cast<const wchar_t*>(keyStr.utf16()), 0, 0,
          reinterpret_cast<unsigned char*>(ba.data()), &dataSize);

      if (res != ERROR_SUCCESS) {
        RegCloseKey(handle);
        continue;
      }

      QString s;
      if (dataSize) {
        s = QString::fromUtf16((const ushort*)ba.constData(), ba.size() / 2);
      }

      QString valStr;
      QList<int> valArray;
      for (int b = 0; b < s.length(); b++) {
        QChar c1((int)s[b].unicode() % 256);
        QChar c2((int)s[b].unicode() / 256);
        valStr.append(c1.toLatin1());
        valStr.append(c2.toLatin1());
        valArray.append((int)s[b].unicode() % 256);
        valArray.append((int)s[b].unicode() / 256);
      }

      // machine name starts from "FC 00", end with "0A"
      int index1 = valArray.indexOf(252);         // FC
      int index2 = valArray.indexOf(10, index1);  // 0A

      if (index1 > 0 && index2 > 0) {
        QString machineName = valStr.mid(index1 + 2, index2 - index1 - 2);
        nameList.push_back(machineName);
        // std::wcout << "machine name = " << machineName.toStdWString() <<
        // std::endl;
      }
      RegCloseKey(handle);

    } else {
      std::cout << "getMonitorNames : failed to get handle" << std::endl;
      continue;
    }
  }

  if (nameList.isEmpty())
    std::cout << "getMonitorNames : No Monitor Name Found" << std::endl;

  return nameList;
}
};  // namespace
#endif

//-----------------------------------------------------------------------------

LutCalibrator::LutCalibrator() {
  LutManager::instance()->registerCalibrator(this);
}

//-----------------------------------------------------------------------------

LutCalibrator::~LutCalibrator() {
  LutManager::instance()->removeCalibrator(this);
}

//-----------------------------------------------------------------------------

void LutCalibrator::initialize() {
  initializeOpenGLFunctions();
  m_isInitialized = true;

  if (!LutManager::instance()->isValid()) return;

  // create shader
  if (!initializeLutTextureShader()) {
    if (m_shader.program) delete m_shader.program;
    if (m_shader.vert) delete m_shader.vert;
    if (m_shader.frag) delete m_shader.frag;
    return;
  }
  createViewerVBO();

  // input 3dlut data to the shader
  assignLutTexture();

  m_isValid = true;
}

//-----------------------------------------------------------------------------

void LutCalibrator::cleanup() {
  m_isInitialized = false;
  if (!isValid()) return;
  // release shader
  if (m_shader.program) {
    delete m_shader.program;
    m_shader.program = NULL;
  }
  if (m_shader.vert) {
    delete m_shader.vert;
    m_shader.vert = NULL;
  }
  if (m_shader.frag) {
    delete m_shader.frag;
    m_shader.frag = NULL;
  }
  // release VBO
  if (m_viewerVBO.isCreated()) m_viewerVBO.destroy();
  // release LUT texture
  if (m_lutTex && m_lutTex->isCreated()) {
    m_lutTex->destroy();
    delete m_lutTex;
    m_lutTex = NULL;
  }
  m_isValid = false;
}

//-----------------------------------------------------------------------------

bool LutCalibrator::initializeLutTextureShader() {
  m_shader.vert = new QOpenGLShader(QOpenGLShader::Vertex);
  const char* simple_vsrc =
      "#version 330 core\n"
      "// Input vertex data, different for all executions of this shader.\n"
      "layout(location = 0) in vec3 vertexPosition;\n"
      "layout(location = 1) in vec2 texCoord;\n"
      "// Output data ; will be interpolated for each fragment.\n"
      "out vec2 UV;\n"
      "// Values that stay constant for the whole mesh.\n"
      "void main() {\n"
      "  // Output position of the vertex, in clip space : MVP * position\n"
      "  gl_Position = vec4(vertexPosition, 1);\n"
      "  // UV of the vertex. No special space for this one.\n"
      "  UV = texCoord;\n"
      "}\n";
  bool ret = m_shader.vert->compileSourceCode(simple_vsrc);
  if (!ret)
    return execWarning(
        QObject::tr("Failed to compile m_textureShader.vert.", "gl"));

  m_shader.frag = new QOpenGLShader(QOpenGLShader::Fragment);
  const char* simple_fsrc =
      "#version 330 core \n"
      "// Interpolated values from the vertex shaders \n"
      "in vec2 UV; \n"
      "// Output data \n"
      "out vec4 color; \n"
      "// Values that stay constant for the whole mesh. \n"
      "uniform sampler2D tex; \n"
      "uniform sampler3D lut; \n"
      "uniform vec3 lutSize; \n"
      "uniform vec3 domainMin; \n"
      "uniform vec3 domainMax; \n"
      "void main() { \n"
      "  vec3 rawColor = clamp((texture(tex,UV).rgb - domainMin) / "
      "(domainMax - domainMin), 0.0, 1.0); \n"
      "  vec3 scale = (lutSize - 1.0) / lutSize; \n"
      "  vec3 offset = 1.0 / (2.0 * lutSize); \n"
      "  color = vec4(texture(lut, scale * rawColor + offset).rgb, 1.0); \n"
      "} \n";
  ret = m_shader.frag->compileSourceCode(simple_fsrc);
  if (!ret)
    return execWarning(QObject::tr("Failed to compile m_shader.frag.", "gl"));

  m_shader.program = new QOpenGLShaderProgram();
  // add shaders
  ret = m_shader.program->addShader(m_shader.vert);
  if (!ret)
    return execWarning(QObject::tr("Failed to add m_shader.vert.", "gl"));
  ret = m_shader.program->addShader(m_shader.frag);
  if (!ret)
    return execWarning(QObject::tr("Failed to add m_shader.frag.", "gl"));
  // link shaders
  ret = m_shader.program->link();
  if (!ret)
    return execWarning(QObject::tr("Failed to link simple shader: %1", "gl")
                           .arg(m_shader.program->log()));
  // obtain parameter locations
  m_shader.vertexAttrib = m_shader.program->attributeLocation("vertexPosition");
  if (m_shader.vertexAttrib == -1)
    return execWarning(
        QObject::tr("Failed to get attribute location of %1", "gl")
            .arg("vertexPosition"));
  m_shader.texCoordAttrib = m_shader.program->attributeLocation("texCoord");
  if (m_shader.texCoordAttrib == -1)
    return execWarning(
        QObject::tr("Failed to get attribute location of %1", "gl")
            .arg("texCoord"));
  m_shader.texUniform = m_shader.program->uniformLocation("tex");
  if (m_shader.texUniform == -1)
    return execWarning(
        QObject::tr("Failed to get uniform location of %1", "gl").arg("tex"));
  m_shader.lutUniform = m_shader.program->uniformLocation("lut");
  if (m_shader.lutUniform == -1)
    return execWarning(
        QObject::tr("Failed to get uniform location of %1", "gl").arg("lut"));
  m_shader.lutSizeUniform = m_shader.program->uniformLocation("lutSize");
  if (m_shader.lutSizeUniform == -1)
    return execWarning(QObject::tr("Failed to get uniform location of %1", "gl")
                           .arg("lutSize"));
  m_shader.domainMinUniform = m_shader.program->uniformLocation("domainMin");
  if (m_shader.domainMinUniform == -1)
    return execWarning(QObject::tr("Failed to get uniform location of %1", "gl")
                           .arg("domainMin"));
  m_shader.domainMaxUniform = m_shader.program->uniformLocation("domainMax");
  if (m_shader.domainMaxUniform == -1)
    return execWarning(QObject::tr("Failed to get uniform location of %1", "gl")
                           .arg("domainMax"));

  return true;
}

//-----------------------------------------------------------------------------

void LutCalibrator::createViewerVBO() {
  GLfloat vertex[]   = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
  GLfloat texCoord[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

  m_viewerVBO.create();
  m_viewerVBO.bind();
  m_viewerVBO.allocate(4 * 4 * sizeof(GLfloat));
  m_viewerVBO.write(0, vertex, sizeof(vertex));
  m_viewerVBO.write(sizeof(vertex), texCoord, sizeof(texCoord));
  m_viewerVBO.release();
}

//-----------------------------------------------------------------------------

void LutCalibrator::onEndDraw(QOpenGLFramebufferObject* fbo) {
  assert((glGetError()) == GL_NO_ERROR);
  fbo->release();
  GLuint textureId = fbo->texture();

  glEnable(GL_TEXTURE_2D);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textureId);

  glActiveTexture(GL_TEXTURE2);
  m_lutTex->bind();

  glPushMatrix();
  glLoadIdentity();

  m_shader.program->bind();
  m_shader.program->setUniformValue(m_shader.texUniform,
                                    1);  // use texture unit 1
  m_shader.program->setUniformValue(m_shader.lutUniform,
                                    2);  // use texture unit 2
  GLfloat size = (GLfloat)LutManager::instance()->meshSize();
  m_shader.program->setUniformValue(m_shader.lutSizeUniform, size, size, size);
  const float* domainMin = LutManager::instance()->domainMin();
  const float* domainMax = LutManager::instance()->domainMax();
  m_shader.program->setUniformValue(m_shader.domainMinUniform, domainMin[0],
                                    domainMin[1], domainMin[2]);
  m_shader.program->setUniformValue(m_shader.domainMaxUniform, domainMax[0],
                                    domainMax[1], domainMax[2]);

  m_shader.program->enableAttributeArray(m_shader.vertexAttrib);
  m_shader.program->enableAttributeArray(m_shader.texCoordAttrib);

  m_viewerVBO.bind();
  m_shader.program->setAttributeBuffer(m_shader.vertexAttrib, GL_FLOAT, 0, 2);
  m_shader.program->setAttributeBuffer(m_shader.texCoordAttrib, GL_FLOAT,
                                       4 * 2 * sizeof(GLfloat), 2);
  m_viewerVBO.release();

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  m_shader.program->disableAttributeArray(m_shader.vertexAttrib);
  m_shader.program->disableAttributeArray(m_shader.texCoordAttrib);

  m_shader.program->release();

  glPopMatrix();

  glActiveTexture(GL_TEXTURE0);  // reset the active texture unit to 0
  glDisable(GL_TEXTURE_2D);

  assert((glGetError()) == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------

void LutCalibrator::assignLutTexture() {
  assert(glGetError() == GL_NO_ERROR);
  if (m_lutTex) delete m_lutTex;
  int meshSize = LutManager::instance()->meshSize();
  m_lutTex     = new QOpenGLTexture(QOpenGLTexture::Target3D);
  m_lutTex->setSize(meshSize, meshSize, meshSize);
  m_lutTex->setFormat(QOpenGLTexture::RGB32F);
  // m_lutTex->setLayers(1);
  m_lutTex->setMipLevels(1);
  m_lutTex->allocateStorage();
  m_lutTex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
  m_lutTex->setWrapMode(QOpenGLTexture::ClampToEdge);

  m_lutTex->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32,
                    LutManager::instance()->data());

  assert(glGetError() == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------

void LutCalibrator::update(bool textureChanged) {
  m_isValid = LutManager::instance()->isValid();

  // PreferencesPopup notifies each viewer immediately after
  // LutManager::update(). The viewer then rebuilds the calibrator while its
  // OpenGL context is current. Do not upload the texture here: this method runs
  // from the preferences UI, where none of the viewers' OpenGL contexts is
  // guaranteed to be current.
  (void)textureChanged;
}

//=============================================================================

LutManager* LutManager::instance() {
  static LutManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

LutManager::LutManager() : m_isValid(false), m_currentLutPath() {
  // check whether preference enables color calibration
  if (!Preferences::instance()->isColorCalibrationEnabled()) return;

  // obtain current monitor name
  QString monitorName = getMonitorName();

  // obtain 3dlut path associated to the monitor name
  QString lutPath =
      Preferences::instance()->getColorCalibrationLutPath(monitorName);

  if (lutPath.isEmpty()) return;

  // check existence of the 3dlut file
  // load 3dlut data
  if (!loadLutFile(lutPath)) return;
  m_currentLutPath = lutPath;
  m_isValid        = true;
}

//-----------------------------------------------------------------------------

LutManager::~LutManager() {
  if (m_lut.data) delete[] m_lut.data;
}

//-----------------------------------------------------------------------------

QString& LutManager::getMonitorName() const {
  static QString monitorName;
  if (!monitorName.isEmpty()) return monitorName;

#ifdef WIN32
  QStringList list = getMonitorNames();
  if (list.isEmpty())
    monitorName = "Any Monitor";  // this should not be translated
  else
    monitorName = list.at(0);  // for now only the first monitor is handled
#else
  monitorName = "Any Monitor";  // this should not be translated
#endif

  return monitorName;
}

//-----------------------------------------------------------------------------

bool LutManager::loadLutFile(const QString& fp) {
  QFile file(fp);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return execWarning(QObject::tr("Failed to Open 3D LUT File."));

  QTextStream stream(&file);
  ParsedLut parsed;
  QString error;
  const QString suffix = QFileInfo(fp).suffix();
  bool loaded          = false;
  if (suffix.compare("cube", Qt::CaseInsensitive) == 0)
    loaded = parseCube(stream, parsed, error);
  else if (suffix.compare("3dl", Qt::CaseInsensitive) == 0)
    loaded = parse3dl(stream, parsed, error);
  else
    error = QObject::tr("Supported file types are .3dl and .cube.");

  file.close();
  if (!loaded)
    return execWarning(
        QObject::tr("Failed to Load 3D LUT File.\n%1").arg(error));

  float* data = new float[parsed.data.size()];
  std::copy(parsed.data.begin(), parsed.data.end(), data);
  if (m_lut.data) delete[] m_lut.data;
  m_lut.data     = data;
  m_lut.meshSize = parsed.meshSize;
  std::copy(parsed.domainMin, parsed.domainMin + 3, m_lut.domainMin);
  std::copy(parsed.domainMax, parsed.domainMax + 3, m_lut.domainMax);
  return true;
}
//-----------------------------------------------------------------------------

// input : 0-1
void LutManager::convert(float& r, float& g, float& b) {
  struct locals {
    static inline float lerp(float val1, float val2, float ratio) {
      return val1 * (1.0f - ratio) + val2 * ratio;
    }
    static inline int getCoord(int r, int g, int b, int meshSize) {
      return b * meshSize * meshSize * 3 + g * meshSize * 3 + r * 3;
    }
  };

  if (!m_isValid) return;

  float ratio[3];   // RGB軸
  int index[3][2];  // rgb インデックス
  float rawVal[3] = {r, g, b};
  for (int c = 0; c < 3; c++) {
    rawVal[c] = (rawVal[c] - m_lut.domainMin[c]) /
                (m_lut.domainMax[c] - m_lut.domainMin[c]);
    rawVal[c] = clamp01(rawVal[c]);
  }

  float vertex_color[2][2][2][3];  // 補間用の１ボクセルの頂点色

  for (int c = 0; c < 3; c++) {
    float val   = rawVal[c] * (float)(m_lut.meshSize - 1);
    index[c][0] = (int)val;
    // boundary condition: if rawVal == 1 the value will not be interpolated
    index[c][1] = (rawVal[c] >= 1.0f) ? index[c][0] : index[c][0] + 1;
    ratio[c]    = val - (float)index[c][0];
  }

  for (int rr = 0; rr < 2; rr++)
    for (int gg = 0; gg < 2; gg++)
      for (int bb = 0; bb < 2; bb++) {
        float* val = &m_lut.data[locals::getCoord(
            index[0][rr], index[1][gg], index[2][bb], m_lut.meshSize)];
        for (int chan = 0; chan < 3; chan++, val++)
          vertex_color[rr][gg][bb][chan] = *val;
      }
  float result[3];

  for (int chan = 0; chan < 3; chan++) {
    result[chan] = locals::lerp(
        locals::lerp(locals::lerp(vertex_color[0][0][0][chan],
                                  vertex_color[0][0][1][chan], ratio[2]),
                     locals::lerp(vertex_color[0][1][0][chan],
                                  vertex_color[0][1][1][chan], ratio[2]),
                     ratio[1]),
        locals::lerp(locals::lerp(vertex_color[1][0][0][chan],
                                  vertex_color[1][0][1][chan], ratio[2]),
                     locals::lerp(vertex_color[1][1][0][chan],
                                  vertex_color[1][1][1][chan], ratio[2]),
                     ratio[1]),
        ratio[0]);
  }

  // CPU-converted UI colors use normalized integer-backed color types. Match
  // the framebuffer's clipping when a floating-point .cube stores HDR values.
  r = clamp01(result[0]);
  g = clamp01(result[1]);
  b = clamp01(result[2]);
}

//-----------------------------------------------------------------------------

void LutManager::convert(QColor& col) {
  if (!m_isValid) return;
  float r = col.redF();
  float g = col.greenF();
  float b = col.blueF();
  convert(r, g, b);
  col = QColor::fromRgbF(r, g, b, col.alphaF());
}

//-----------------------------------------------------------------------------

void LutManager::convert(TPixel32& col) {
  if (!m_isValid) return;
  float r = (float)col.r / 255.0;
  float g = (float)col.g / 255.0;
  float b = (float)col.b / 255.0;
  convert(r, g, b);
  col = TPixel32((int)(r * 255.0 + 0.5), (int)(g * 255.0 + 0.5),
                 (int)(b * 255.0 + 0.5), col.m);
}

//-----------------------------------------------------------------------------

void LutManager::registerCalibrator(LutCalibrator* calibrator) {
  assert(!m_calibrators.contains(calibrator));
  m_calibrators.insert(calibrator);
}

//-----------------------------------------------------------------------------

void LutManager::removeCalibrator(LutCalibrator* calibrator) {
  assert(m_calibrators.contains(calibrator));
  m_calibrators.remove(calibrator);
}

//-----------------------------------------------------------------------------

void LutManager::update() {
  m_isValid           = false;
  bool textureChanged = false;
  if (Preferences::instance()->isColorCalibrationEnabled()) {
    // obtain current monitor name
    QString monitorName = getMonitorName();
    // obtain 3dlut path associated to the monitor name
    QString lutPath =
        Preferences::instance()->getColorCalibrationLutPath(monitorName);
    if (m_currentLutPath == lutPath)
      m_isValid = true;
    else if (loadLutFile(lutPath)) {
      m_isValid        = true;
      m_currentLutPath = lutPath;
      textureChanged   = true;
    }
  }

  // update textures for all calibrators
  for (auto calibrator : m_calibrators) calibrator->update(textureChanged);
}
