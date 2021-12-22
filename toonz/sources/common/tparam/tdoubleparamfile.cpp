

#include "tdoubleparamfile.h"
#include "tfilepath_io.h"
#include "tconvert.h"

namespace {

//---------------------------------------------------------

bool parseDouble(double &value, char *&s) {
  if (!(*s == '-' || *s == '.' || ('0' <= *s && *s <= '9'))) return false;
  char *t = s;
  if (*s == '-') s++;
  while ('0' <= *s && *s <= '9') s++;
  if (*s == '.') {
    s++;
    while ('0' <= *s && *s <= '9') s++;
    if (*s == 'e' || *s == 'E') {
      s++;
      if (*s == '+' || *s == '-') s++;
      while ('0' <= *s && *s <= '9') s++;
    }
  }
  std::string w(t, s - t);
  value = std::stod(w);
  return true;
}

//---------------------------------------------------------

int split(std::vector<double> &values, char *s) {
  int count = 0;
  for (;;) {
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') break;
    double value = 0;
    bool found   = parseDouble(value, s);
    if (found) {
      count++;
      values.push_back(value);
      while (*s == ' ' || *s == '\t') s++;
    }
    if (*s == ',' || *s == ';') {
      if (!found) {
        count++;
        values.push_back(0);
      }
      s++;
    } else {
      if (!found) break;
    }
  }
  return count;
}

//---------------------------------------------------------
}  // namespace
//---------------------------------------------------------

TDoubleParamFileData::TDoubleParamFileData() : m_params(), m_dirtyFlag(false) {}

//---------------------------------------------------------

TDoubleParamFileData::~TDoubleParamFileData() {}

//---------------------------------------------------------

void TDoubleParamFileData::setParams(
    const TDoubleKeyframe::FileParams &params) {
  m_params = params;
  invalidate();
}

//---------------------------------------------------------

void TDoubleParamFileData::invalidate() {
  m_dirtyFlag = true;
  m_values.clear();
}

//---------------------------------------------------------

double TDoubleParamFileData::getValue(double frame, double defaultValue) {
  if (frame < 0) return defaultValue;
  int intFrame = (int)frame;
  if (m_dirtyFlag) read();
  if (intFrame >= (int)m_values.size())
    return defaultValue;
  else
    return m_values[intFrame];
}

//---------------------------------------------------------

void TDoubleParamFileData::read() {
  m_dirtyFlag = false;
  m_values.clear();
  int k = m_params.m_fieldIndex;
  if (k < 0) return;
  TFilePath fp = m_params.m_path;
  Tifstream is(fp);
  char buffer[2048];
  memset(buffer, 0, sizeof buffer);
  while (is) {
    is.getline(buffer, sizeof(buffer) - 1);
    std::vector<double> fields;
    QString line(buffer);
    if (line.size() == 0 || line.startsWith("#")) continue;
    split(fields, buffer);
    double value                      = 0;
    if (k < (int)fields.size()) value = fields[k];
    m_values.push_back(value);
  }
}

//---------------------------------------------------------
