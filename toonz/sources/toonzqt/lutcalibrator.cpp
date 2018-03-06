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
#include <QColor>

namespace {
inline bool execWarning(const QString& s) {
  DVGui::MsgBox(DVGui::WARNING, s);
  return false;
}
};

#ifdef WIN32

#include <QStringList>
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
};
#endif

//-----------------------------------------------------------------------------

LutCalibrator* LutCalibrator::instance() {
  static LutCalibrator _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void LutCalibrator::initialize() {
  static bool hasInitialized = false;
  if (hasInitialized) return;
  hasInitialized = true;

  initializeOpenGLFunctions();

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
  return;
}

//-----------------------------------------------------------------------------

void LutCalibrator::finalize() {
  if (!m_isValid) return;
  // release shader
  if (m_shader.program) delete m_shader.program;
  if (m_shader.vert) delete m_shader.vert;
  if (m_shader.frag) delete m_shader.frag;
  // release VBO
  if (m_viewerVBO.isCreated()) m_viewerVBO.destroy();
  // release LUT texture
  if (m_lut.tex && m_lut.tex->isCreated()) {
    m_lut.tex->destroy();
    delete m_lut.tex;
  }
  if (m_lut.data) delete[] m_lut.data;
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
      "// Ouput data \n"
      "out vec4 color; \n"
      "// Values that stay constant for the whole mesh. \n"
      "uniform sampler2D tex; \n"
      "uniform sampler3D lut; \n"
      "uniform vec3 lutSize; \n"
      "void main() { \n"
      "  vec3 rawColor = texture(tex,UV).rgb; \n"
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
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, textureId);

  glActiveTexture(GL_TEXTURE5);
  m_lut.tex->bind();

  glPushMatrix();
  glLoadIdentity();

  m_shader.program->bind();
  m_shader.program->setUniformValue(m_shader.texUniform,
                                    4);  // use texture unit 4
  m_shader.program->setUniformValue(m_shader.lutUniform,
                                    5);  // use texture unit 5
  GLfloat size = (GLfloat)m_lut.meshSize;
  m_shader.program->setUniformValue(m_shader.lutSizeUniform, size, size, size);

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
  glDisable(GL_TEXTURE_2D);

  assert((glGetError()) == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------

QString& LutCalibrator::getMonitorName() const {
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

bool LutCalibrator::loadLutFile(const QString& fp) {
  struct locals {
    // skip empty or comment lines
    static inline QString readDataLine(QTextStream& stream) {
      while (1) {
        if (stream.atEnd()) return QString();
        QString ret = stream.readLine();
        if (!ret.isEmpty() && ret[0] != QChar('#')) return ret;
      }
    }

    static inline int lutCoords(int r, int g, int b, int meshSize) {
      return b * meshSize * meshSize * 3 + g * meshSize * 3 + r * 3;
    }
  };

  QFile file(fp);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return execWarning(QObject::tr("Failed to Open 3DLUT File."));

  QTextStream stream(&file);
  QString line;

  //---- read the 3DLUT files

  // The first line shoud start from "3DMESH" keyword (case sensitive)
  line = locals::readDataLine(stream);
  if (line != "3DMESH") {
    file.close();
    return execWarning(
        QObject::tr("Failed to Load 3DLUT File.\nIt should start with "
                    "\"3DMESH\" keyword."));
  }

  // The second line is "Mesh [Input bit depth] [Output bit depth]"
  // "Mesh" is a keyword (case sensitive) 
  line             = locals::readDataLine(stream);
  QStringList list = line.split(" ");
  if (list.size() != 3 || list.at(0) != "Mesh") {
    file.close();
    return execWarning(
        QObject::tr("Failed to Load 3DLUT File.\nThe second line should be "
                    "\"Mesh [Input bit depth] [Output bit depth]\""));
  }

  int inputBitDepth  = list.at(1).toInt();
  int outputBitDepth = list.at(2).toInt();

  m_lut.meshSize = (int)pow(2.0, inputBitDepth) + 1;

  float maxValue = pow(2.0, outputBitDepth) - 1.0f;

  // The third line is corrections of values at each LUT grid
  line = locals::readDataLine(stream);
  list = line.split(" ", QString::SkipEmptyParts);
  if (list.size() != m_lut.meshSize) {
    file.close();
    return execWarning(QObject::tr("Failed to Load 3DLUT File."));
  }

  m_lut.data = new float[m_lut.meshSize * m_lut.meshSize * m_lut.meshSize * 3];

  for (int k = 0; k < m_lut.meshSize; ++k)  // r
  {
    for (int j = 0; j < m_lut.meshSize; ++j)  // g
    {
      for (int i = 0; i < m_lut.meshSize; ++i)  // b
      {
        line = locals::readDataLine(stream);
        list = line.split(" ", QString::SkipEmptyParts);
        if (list.size() != 3) {
          file.close();
          delete[] m_lut.data;
          return execWarning(QObject::tr("Failed to Load 3DLUT File."));
        }
        float* lut = m_lut.data + locals::lutCoords(k, j, i, m_lut.meshSize);
        *lut       = (float)(list.at(0).toInt()) / maxValue;
        lut++;
        *lut = (float)(list.at(1).toInt()) / maxValue;
        lut++;
        *lut = (float)(list.at(2).toInt()) / maxValue;
      }
    }
  }

  file.close();
  return true;
}

//-----------------------------------------------------------------------------

void LutCalibrator::assignLutTexture() {
  assert(glGetError() == GL_NO_ERROR);

  m_lut.tex = new QOpenGLTexture(QOpenGLTexture::Target3D);
  m_lut.tex->setSize(m_lut.meshSize, m_lut.meshSize, m_lut.meshSize);
  m_lut.tex->setFormat(QOpenGLTexture::RGB32F);
  m_lut.tex->setLayers(1);
  m_lut.tex->setMipLevels(1);
  m_lut.tex->allocateStorage();
  m_lut.tex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
  m_lut.tex->setWrapMode(QOpenGLTexture::ClampToEdge);

  m_lut.tex->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32, m_lut.data);

  assert(glGetError() == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------

// input : 0-1
void LutCalibrator::convert(float& r, float& g, float& b) {
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

  float vertex_color[2][2][2][3];  //補間用の１ボクセルの頂点色

  for (int c = 0; c < 3; c++) {
    float val   = rawVal[c] * (float)(m_lut.meshSize - 1);
    index[c][0] = (int)val;
    // boundary condition: if rawVal == 1 the value will not be interporated
    index[c][1] = (rawVal[c] >= 1.0f) ? index[c][0] : index[c][0] + 1;
    ratio[c]    = val - (float)index[c][0];
  }

  for (int rr = 0; rr < 2; rr++)
    for (int gg = 0; gg < 2; gg++)
      for (int bb = 0; bb < 2; bb++) {
        float* val = &m_lut.data[locals::getCoord(
            index[0][rr], index[1][gg], index[2][bb], m_lut.meshSize)];
        for (int chan                    = 0; chan < 3; chan++, val++)
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

  r = result[0];
  g = result[1];
  b = result[2];
}

//-----------------------------------------------------------------------------

void LutCalibrator::convert(QColor& col) {
  if (!m_isValid) return;
  float r = col.redF();
  float g = col.greenF();
  float b = col.blueF();
  convert(r, g, b);
  // 0.5 offset is necessary for converting to 255 grading
  col = QColor((int)(r * 255.0 + 0.5), (int)(g * 255.0 + 0.5),
               (int)(b * 255.0 + 0.5), col.alpha());
}

//-----------------------------------------------------------------------------

void LutCalibrator::convert(TPixel32& col) {
  if (!m_isValid) return;
  float r = (float)col.r / 255.0;
  float g = (float)col.g / 255.0;
  float b = (float)col.b / 255.0;
  convert(r, g, b);
  col = TPixel32((int)(r * 255.0 + 0.5), (int)(g * 255.0 + 0.5),
                 (int)(b * 255.0 + 0.5), col.m);
}