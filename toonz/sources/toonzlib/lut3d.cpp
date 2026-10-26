#include "toonz/lut3d.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kMax3DLutSize = 129;

float clamp01(float value) {
  if (std::isnan(value)) return 0.0f;
  return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

bool readDataLine(QTextStream &stream, QString &line, int &lineNumber) {
  while (!stream.atEnd()) {
    ++lineNumber;
    line = stream.readLine().trimmed();
    if (!line.isEmpty() && !line.startsWith('#')) return true;
  }
  return false;
}

QStringList splitFields(const QString &line) {
  return line.simplified().split(' ', Qt::SkipEmptyParts);
}

bool parseFiniteFloat(const QString &text, float &value) {
  bool ok = false;
  value   = text.toFloat(&ok);
  return ok && std::isfinite(value);
}

bool parseFloatTriple(const QStringList &fields, float values[3]) {
  if (fields.size() != 3) return false;
  for (int channel = 0; channel < 3; ++channel)
    if (!parseFiniteFloat(fields.at(channel), values[channel])) return false;
  return true;
}

QString lineError(int lineNumber, const QString &message) {
  return QObject::tr("Line %1: %2").arg(lineNumber).arg(message);
}

bool parse3dl(QTextStream &stream, int &meshSize, std::vector<float> &data,
              float domainMin[3], float domainMax[3], QString &error) {
  QString line;
  int lineNumber = 0;
  if (!readDataLine(stream, line, lineNumber) || line != "3DMESH") {
    error = lineError(
        lineNumber,
        QObject::tr("Only Lustre-format .3dl LUTs with a 3DMESH header are "
                    "supported. Flame-format .3dl LUTs are not supported."));
    return false;
  }
  if (!readDataLine(stream, line, lineNumber)) {
    error = QObject::tr("The Mesh header is missing.");
    return false;
  }
  const QStringList fields = splitFields(line);
  bool inputOk = false, outputOk = false;
  const int inputBitDepth =
      fields.size() == 3 ? fields.at(1).toInt(&inputOk) : 0;
  const int outputBitDepth =
      fields.size() == 3 ? fields.at(2).toInt(&outputOk) : 0;
  if (fields.size() != 3 || fields.at(0) != "Mesh" || !inputOk || !outputOk ||
      inputBitDepth < 0 || inputBitDepth > 7 || outputBitDepth < 1 ||
      outputBitDepth > 30) {
    error = lineError(
        lineNumber,
        QObject::tr("Expected Mesh [input bit depth] [output bit depth]."));
    return false;
  }
  meshSize = (1 << inputBitDepth) + 1;
  if (meshSize > kMax3DLutSize) {
    error = QObject::tr("The LUT grid may not exceed %1 points per axis.")
                .arg(kMax3DLutSize);
    return false;
  }
  if (!readDataLine(stream, line, lineNumber) ||
      splitFields(line).size() != meshSize) {
    error = lineError(lineNumber,
                      QObject::tr("The input grid has the wrong size."));
    return false;
  }
  const QStringList gridFields = splitFields(line);
  std::vector<int> grid(meshSize);
  for (int i = 0; i < meshSize; ++i) {
    bool ok = false;
    grid[i] = gridFields.at(i).toInt(&ok);
    if (!ok || grid[i] < 0 || (i > 0 && grid[i] <= grid[i - 1])) {
      error = lineError(
          lineNumber,
          QObject::tr("The input grid must contain strictly increasing "
                      "nonnegative integers."));
      return false;
    }
  }
  bool hasFullRange = false;
  for (int maximum : {255, 1023, 4095, 65535})
    if (std::abs(grid.back() - maximum) <= 1) hasFullRange = true;
  if (grid.front() != 0 || !hasFullRange) {
    error = lineError(
        lineNumber,
        QObject::tr("Only full-range .3dl input grids starting at 0 and ending "
                    "at 255, 1023, 4095 or 65535 (within one code value) are "
                    "supported."));
    return false;
  }
  for (int i = 1; i < meshSize - 1; ++i) {
    const double expected =
        static_cast<double>(i) * grid.back() / (meshSize - 1);
    if (std::abs(grid[i] - expected) > 1.0) {
      error = lineError(
          lineNumber,
          QObject::tr("Nonuniform .3dl input grids are not supported. Grid "
                      "points must be evenly spaced within one code value."));
      return false;
    }
  }
  const size_t entryCount = static_cast<size_t>(meshSize) * meshSize * meshSize;
  data.resize(entryCount * 3);
  const float maxValue = std::ldexp(1.0f, outputBitDepth) - 1.0f;
  size_t entry         = 0;
  for (int r = 0; r < meshSize; ++r)
    for (int g = 0; g < meshSize; ++g)
      for (int b = 0; b < meshSize; ++b, ++entry) {
        if (!readDataLine(stream, line, lineNumber)) {
          error = QObject::tr("The LUT contains %1 entries; expected %2.")
                      .arg(static_cast<qulonglong>(entry))
                      .arg(static_cast<qulonglong>(entryCount));
          return false;
        }
        const QStringList values = splitFields(line);
        if (values.size() != 3) {
          error = lineError(
              lineNumber, QObject::tr("Expected three integer color values."));
          return false;
        }
        const size_t offset = (static_cast<size_t>(b) * meshSize * meshSize +
                               static_cast<size_t>(g) * meshSize + r) *
                              3;
        for (int channel = 0; channel < 3; ++channel) {
          bool ok         = false;
          const int value = values.at(channel).toInt(&ok);
          if (!ok) {
            error =
                lineError(lineNumber,
                          QObject::tr("Expected three integer color values."));
            return false;
          }
          data[offset + channel] = static_cast<float>(value) / maxValue;
        }
      }
  if (readDataLine(stream, line, lineNumber)) {
    if (line != "LUT8") {
      error = lineError(
          lineNumber,
          QObject::tr("Unexpected data after the .3dl color table. Only the "
                      "LUT8 / gamma 1 footer is supported."));
      return false;
    }
    if (readDataLine(stream, line, lineNumber)) {
      const QStringList footer = splitFields(line);
      float gamma              = 0.0f;
      if (footer.size() != 2 || footer.at(0) != "gamma" ||
          !parseFiniteFloat(footer.at(1), gamma) || gamma != 1.0f) {
        error = lineError(
            lineNumber, QObject::tr("Only gamma 1 is supported in .3dl LUTs."));
        return false;
      }
      if (readDataLine(stream, line, lineNumber)) {
        error = lineError(
            lineNumber, QObject::tr("Unexpected data after the .3dl footer."));
        return false;
      }
    }
  }
  std::fill(domainMin, domainMin + 3, 0.0f);
  std::fill(domainMax, domainMax + 3, 1.0f);
  return true;
}

bool parseCube(QTextStream &stream, int &meshSize, std::vector<float> &data,
               float domainMin[3], float domainMax[3], QString &error) {
  QString line;
  int lineNumber = 0;
  bool hasSize = false, dataHasBegun = false, hasDomainTags = false,
       hasRangeTag = false;
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
      data.insert(data.end(), values, values + 3);
      const size_t expected =
          static_cast<size_t>(meshSize) * meshSize * meshSize * 3;
      if (data.size() > expected) {
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
    if (keyword == "TITLE") continue;
    if (keyword == "LUT_1D_SIZE" || keyword == "LUT_1D_INPUT_RANGE") {
      error =
          lineError(lineNumber,
                    QObject::tr("1D and shaper .cube LUTs are not supported."));
      return false;
    }
    if (keyword == "LUT_2D_SIZE") {
      error = lineError(lineNumber,
                        QObject::tr("2D .cube LUTs are not supported."));
      return false;
    }
    if (keyword == "LUT_3D_SIZE") {
      bool ok        = false;
      const int size = fields.size() == 2 ? fields.at(1).toInt(&ok) : 0;
      if (!ok || size < 2 || size > kMax3DLutSize || hasSize) {
        error = lineError(
            lineNumber,
            QObject::tr("LUT_3D_SIZE must be a single value from 2 to %1.")
                .arg(kMax3DLutSize));
        return false;
      }
      meshSize = size;
      data.reserve(static_cast<size_t>(size) * size * size * 3);
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
      std::copy(values, values + 3,
                keyword == "DOMAIN_MIN" ? domainMin : domainMax);
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
        domainMin[channel] = values[0];
        domainMax[channel] = values[1];
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
  const size_t expected =
      static_cast<size_t>(meshSize) * meshSize * meshSize * 3;
  if (data.size() != expected) {
    error = QObject::tr("The .cube file contains %1 entries; expected %2.")
                .arg(static_cast<qulonglong>(data.size() / 3))
                .arg(static_cast<qulonglong>(expected / 3));
    return false;
  }
  for (int channel = 0; channel < 3; ++channel)
    if (domainMin[channel] >= domainMax[channel]) {
      error = QObject::tr(
          "Each .cube input-domain minimum must be less than its maximum.");
      return false;
    }
  return true;
}
}  // namespace

bool Lut3D::load(const QString &path, QString *error) {
  m_meshSize = 0;
  m_data.clear();
  std::fill(m_domainMin, m_domainMin + 3, 0.0f);
  std::fill(m_domainMax, m_domainMax + 3, 1.0f);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = QObject::tr("Failed to open 3D LUT file.");
    return false;
  }
  QTextStream stream(&file);
  QString parseError;
  const QString suffix = QFileInfo(path).suffix();
  const bool loaded    = suffix.compare("cube", Qt::CaseInsensitive) == 0
                             ? parseCube(stream, m_meshSize, m_data, m_domainMin,
                                         m_domainMax, parseError)
                         : suffix.compare("3dl", Qt::CaseInsensitive) == 0
                             ? parse3dl(stream, m_meshSize, m_data, m_domainMin,
                                        m_domainMax, parseError)
                             : false;
  if (!loaded) {
    m_meshSize = 0;
    m_data.clear();
    if (error)
      *error = parseError.isEmpty()
                   ? QObject::tr("Supported file types are .3dl and .cube.")
                   : parseError;
  }
  return loaded;
}

void Lut3D::convert(float &r, float &g, float &b) const {
  if (!isValid()) return;
  float raw[3] = {r, g, b};
  for (int c = 0; c < 3; ++c)
    raw[c] = clamp01(static_cast<float>(
        (static_cast<double>(raw[c]) - m_domainMin[c]) /
        (static_cast<double>(m_domainMax[c]) - m_domainMin[c])));
  int index[3][2];
  float ratio[3];
  for (int c = 0; c < 3; ++c) {
    const float v = raw[c] * (m_meshSize - 1);
    index[c][0]   = static_cast<int>(v);
    index[c][1]   = raw[c] >= 1.0f ? index[c][0] : index[c][0] + 1;
    ratio[c]      = v - index[c][0];
  }
  const auto sample = [&](int rr, int gg, int bb, int c) {
    return m_data[(static_cast<size_t>(bb) * m_meshSize * m_meshSize +
                   static_cast<size_t>(gg) * m_meshSize + rr) *
                      3 +
                  c];
  };
  const auto lerp = [](float a, float z, float t) {
    return a * (1.0f - t) + z * t;
  };
  float result[3];
  for (int c = 0; c < 3; ++c) {
    const float c00 =
        lerp(sample(index[0][0], index[1][0], index[2][0], c),
             sample(index[0][0], index[1][0], index[2][1], c), ratio[2]);
    const float c01 =
        lerp(sample(index[0][0], index[1][1], index[2][0], c),
             sample(index[0][0], index[1][1], index[2][1], c), ratio[2]);
    const float c10 =
        lerp(sample(index[0][1], index[1][0], index[2][0], c),
             sample(index[0][1], index[1][0], index[2][1], c), ratio[2]);
    const float c11 =
        lerp(sample(index[0][1], index[1][1], index[2][0], c),
             sample(index[0][1], index[1][1], index[2][1], c), ratio[2]);
    result[c] =
        lerp(lerp(c00, c01, ratio[1]), lerp(c10, c11, ratio[1]), ratio[0]);
  }
  r = result[0];
  g = result[1];
  b = result[2];
}
