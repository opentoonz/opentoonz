

#include "tfilepath.h"
#include <math.h>
#include <ctype.h>

#ifdef WIN32
const char slash    = '\\';
const char auxslash = '/';
#else
const char slash    = '/';
const char auxslash = '\\';
#endif

//=============================================================================

// TFrameId::operator string() const
std::string TFrameId::expand(FrameFormat format) const {
  if (m_frame == EMPTY_FRAME)
    return "";
  else if (m_frame == NO_FRAME)
    return "-";
  std::stringstream o_buff;
  if (format == FOUR_ZEROS) {
    o_buff.fill('0');
    o_buff.width(4);
    o_buff << m_frame;
    o_buff.width(0);
  } else {
    o_buff << m_frame;
  }
  if (m_letter != '\0') o_buff << m_letter;
  return o_buff.str();
}

//-------------------------------------------------------------------

const TFrameId &TFrameId::operator++() {
  ++m_frame;
  m_letter = 0;
  return *this;
}

//-------------------------------------------------------------------

const TFrameId &TFrameId::operator--() {
  if (m_letter > 0)
    m_letter = 0;
  else
    --m_frame;
  return *this;
}

//=============================================================================

inline bool isSlash(char c) { return c == slash || c == auxslash; }

//-----------------------------------------------------------------------------

// cerca l'ultimo '/' o '\'. Se non c'e' ritorna -1
// di modo che la sottostringa che parte da getLastSlash() + 1 e'
// nome.frame.tipo
inline int getLastSlash(const string &path) {
  int i;
  for (i = path.length() - 1; i >= 0 && !isSlash(path[i]); i--) {
  }
  return i;
}

//-----------------------------------------------------------------------------

void TFilePath::setPath(string path) {
  bool isUncName = false;
  // elimino i '//', './' e '/' finali; raddrizzo gli slash 'storti'.
  // se il path comincia con  "<alpha>:" aggiungo uno slash
  int length = path.length();
  int pos    = 0;
  if (path.length() >= 2 && isalpha(path[0]) && path[1] == ':') {
    m_path.append(path, 0, 2);
    pos = 2;
    if (path.length() == 2 || !isSlash(path[pos])) m_path.append(1, slash);
  }
#ifdef WIN32
  else  // se si tratta di un path in formato UNC e' del tipo "\\\\MachineName"
      if (path.length() >= 3 && path[0] == '\\' && path[1] == '\\' &&
          isalpha(path[2])) {
    isUncName = true;
    m_path.append(path, 0, 3);
    pos = 3;
  }
#endif
  for (; pos < length; pos++)
    if (path[pos] == '.') {
      pos++;
      if (pos >= length) {
        if (pos > 1) m_path.append(1, '.');
      } else if (!isSlash(path[pos]))
        m_path.append(path, pos - 1, 2);
      else {
        while (pos + 1 < length && isSlash(path[pos + 1])) pos++;
      }
    } else if (isSlash(path[pos])) {
      do
        pos++;
      while (pos < length && isSlash(path[pos]));
      pos--;
      m_path.append(1, slash);
    } else {
      m_path.append(1, path[pos]);
    }

  // rimuovo l'eventuale '/' finale (a meno che m_path == "/" o m_path ==
  // "<letter>:\"
  // oppure sia UNC (Windows only) )
  if (!(m_path.length() == 1 && m_path[0] == slash ||
        m_path.length() == 3 && isalpha(m_path[0]) && m_path[1] == ':' &&
            m_path[2] == slash) &&
      m_path.length() > 1 && m_path[m_path.length() - 1] == slash)
    m_path.erase(m_path.length() - 1, 1);

  if (isUncName &&
      m_path.find_last_of('\\') ==
          1)  // e' indicato solo il nome della macchina...
    m_path.append(1, slash);
}

//-----------------------------------------------------------------------------

TFilePath::TFilePath(string path) : m_path("") { setPath(path); }

//-----------------------------------------------------------------------------

TFilePath::TFilePath(const char *path) : m_path("") { setPath(string(path)); }

//-----------------------------------------------------------------------------

bool TFilePath::operator==(const TFilePath &fp) const {
#ifdef WIN32
  return stricmp(m_path.c_str(), fp.m_path.c_str()) == 0;
#else
  return m_path == fp.m_path;
#endif
}

//-----------------------------------------------------------------------------

bool TFilePath::operator<(const TFilePath &fp) const {
  string iName = m_path;
  string jName = fp.m_path;
  int i1 = 0, j1 = 0;
  int i2 = m_path.find("\\");
  int j2 = fp.m_path.find("\\");
  if (i2 == j2 && j2 == -1)
#ifdef WIN32
    return stricmp(m_path.c_str(), fp.m_path.c_str()) < 0;
#else
    return m_path < fp.m_path;
#endif
  if (!i2) {
    ++i1;
    i2 = m_path.find("\\", i1);
  }
  if (!j2) {
    ++j1;
    j2 = fp.m_path.find("\\", j1);
  }
  while (i2 != -1 || j2 != -1) {
    iName = m_path.substr(i1, i2 - i1);
    jName = fp.m_path.substr(j1, j2 - j1);
// se le due parti di path, conpresi tra slash sono uguali
// itero il processo di confronto altrimenti ritorno
#ifdef WIN32
    char differ;
    differ = stricmp(iName.c_str(), jName.c_str());
    if (differ != 0) return differ < 0 ? true : false;
#else
    if (TFilePath(iName) != TFilePath(jName))
      return TFilePath(iName) < TFilePath(jName);
#endif
    i1 = i2 + 1;
    j1 = j2 + 1;
    i2 = m_path.find("\\", i1);
    j2 = fp.m_path.find("\\", j1);
  }
  iName = m_path.substr(i1, m_path.size() - i1);
  jName = fp.m_path.substr(j1, fp.m_path.size() - j1);
#ifdef WIN32
  return stricmp(iName.c_str(), jName.c_str()) < 0;
#else
  return TFilePath(iName) < TFilePath(jName);
#endif
}

//-----------------------------------------------------------------------------

TFilePath &TFilePath::operator+=(const TFilePath &fp) {
  assert(!fp.isAbsolute());
  if (fp.isEmpty())
    return *this;
  else if (isEmpty()) {
    *this = fp;
    return *this;
  } else if (m_path.length() != 1 || m_path[0] != slash) {
    *this = TFilePath(m_path + slash + fp.m_path);
    return *this;
  } else {
    *this = TFilePath(m_path + fp.m_path);
    return *this;
  }
}
//-----------------------------------------------------------------------------
TFilePath TFilePath::operator+(const TFilePath &fp) const {
  TFilePath ret(*this);
  ret += fp;
  return ret;
}
//-----------------------------------------------------------------------------
/*
TFilePath TFilePath::operator+ (const TFilePath &fp) const
{
assert(!fp.isAbsolute());
if(fp.isEmpty()) return *this;
else if(isEmpty()) return fp;
else if(m_path.length()!=1 || m_path[0] != slash)
  return TFilePath(m_path + slash + fp.m_path);
else
  return TFilePath(m_path + fp.m_path);
}
*/
//-----------------------------------------------------------------------------

bool TFilePath::isAbsolute() const {
  return m_path.length() >= 1 && m_path[0] == slash ||
         m_path.length() >= 2 && isalpha(m_path[0]) && m_path[1] == ':';
}

//-----------------------------------------------------------------------------

bool TFilePath::isRoot() const {
  return m_path.length() == 1 && m_path[0] == slash ||
         m_path.length() == 3 && isalpha(m_path[0]) && m_path[1] == ':' &&
             m_path[2] == slash;
}

//-----------------------------------------------------------------------------

// ritorna ""(niente tipo, niente punto), "." (file con tipo) o ".." (file con
// tipo e frame)
string TFilePath::getDots() const {
  int i      = getLastSlash(m_path);
  string str = m_path.substr(i + 1);
  // potrei anche avere a.b.c.d dove d e' l'estensione
  i = str.rfind(".");
  if (i == string::npos || str == "..") return "";
  return str.substr(0, i).rfind(".") == string::npos ? "." : "..";
}

//-----------------------------------------------------------------------------

string TFilePath::getDottedType()
    const  // ritorna l'estensione con PUNTO (se c'e')
{
  int i      = getLastSlash(m_path);
  string str = m_path.substr(i + 1);
  i          = str.rfind(".");
  if (i == string::npos) return "";
#ifdef WIN32
  return toLower(str.substr(i));
#else
  return str.substr(i);
#endif
}

//-----------------------------------------------------------------------------

string TFilePath::getUndottedType() const  // ritorna l'estensione senza PUNTO
{
  size_t i   = getLastSlash(m_path);
  string str = m_path.substr(i + 1);
  i          = str.rfind(".");
  if (i == string::npos || i == str.length() - 1) return "";
#ifdef WIN32
  return toLower(str.substr(i + 1));
#else
  return str.substr(i + 1);
#endif
}

//-----------------------------------------------------------------------------

string TFilePath::getName() const  // noDot! noSlash!
{
  int i      = getLastSlash(m_path);  // cerco l'ultimo slash
  string str = m_path.substr(i + 1);
  i          = str.rfind(".");
  if (i == string::npos) return str;
  int j                    = str.substr(0, i).rfind(".");
  if (j != string::npos) i = j;
  return str.substr(0, i);
}

//-----------------------------------------------------------------------------
// es. TFilePath("/pippo/pluto.0001.gif").getLevelName() == "pluto..gif"
string TFilePath::getLevelName() const {
  int i      = getLastSlash(m_path);  // cerco l'ultimo slash
  string str = m_path.substr(i + 1);  // str e' m_path senza directory
  i          = str.find(".");         // posizione del primo punto di str
  if (i == string::npos) return str;  // no frame; no type
  int j = str.rfind(".");             // str[j..] = ".type"
  if (j == i || j - i == 1)           // prova.tif o prova..tif
    return str;
  else  // prova.0001.tif
    return str.erase(i + 1, j - i - 1);
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::getParentDir() const  // noSlash!
{
  int i = getLastSlash(m_path);  // cerco l'ultimo slash
  if (i < 0) {
    if (m_path.length() >= 2 && ('a' <= m_path[0] && m_path[0] <= 'z' ||
                                 'A' <= m_path[0] && m_path[0] <= 'Z') &&
        m_path[1] == ':')
      return TFilePath(m_path.substr(0, 2));
    else
      return TFilePath("");
  } else if (i == 0)
    return TFilePath("/");
  else
    return TFilePath(m_path.substr(0, i));
}

//-----------------------------------------------------------------------------

TFrameId TFilePath::getFrame() const {
  int i      = getLastSlash(m_path);  // cerco l'ultimo slash
  string str = m_path.substr(i + 1);  // str e' il path senza parentdir
  i          = str.rfind('.');
  if (i == string::npos || str == "." || str == "..")
    return TFrameId(TFrameId::NO_FRAME);
  int j = str.substr(0, i).rfind('.');
  if (j == string::npos) return TFrameId(TFrameId::NO_FRAME);
  if (i == j + 1) return TFrameId(TFrameId::EMPTY_FRAME);

  int k, number = 0;
  for (k = j + 1; k < i && isdigit(str[k]); k++)
    number                    = number * 10 + str[k] - '0';
  char letter                 = '\0';
  if (isalpha(str[k])) letter = str[k++];
  if (number == 0 || k < i) throw TMalformedFrameException();
  return TFrameId(number, letter);
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withType(const string &type) const {
  const string dotDot = "..";
  assert(type.length() < 2 || type.substr(0, 2) != dotDot);
  int i      = getLastSlash(m_path);  // cerco l'ultimo slash
  string str = m_path.substr(i + 1);  // str e' il path senza parentdir
  int j      = str.rfind('.');
  if (j == string::npos || str == dotDot)
  // il path originale non ha tipo
  {
    if (type == "")
      return *this;
    else if (type[0] == '.')
      return TFilePath(m_path + type);
    else
      return TFilePath(m_path + "." + type);
  } else
  // il path originale ha gia' il tipo
  {
    if (type == "")
      return TFilePath(m_path.substr(0, i + j + 1));
    else if (type[0] == '.')
      return TFilePath(m_path.substr(0, i + j + 1) + type);
    else
      return TFilePath(m_path.substr(0, i + j + 2) + type);
  }
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withName(const string &name) const {
  int i      = getLastSlash(m_path);  // cerco l'ultimo slash
  string str = m_path.substr(i + 1);  // str e' il path senza parentdir
  int j      = str.rfind('.');
  if (j == string::npos) return TFilePath(m_path.substr(0, i + 1) + name);
  int k                    = str.substr(0, j).rfind(".");
  if (k == string::npos) k = j;
  return TFilePath(m_path.substr(0, i + 1) + name + str.substr(k));
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withParentDir(const TFilePath &dir) const {
  int i = getLastSlash(m_path);  // cerco l'ultimo slash
  return dir + TFilePath(m_path.substr(i + 1));
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withFrame(const TFrameId &frame,
                               TFrameId::FrameFormat format) const {
  const string dot = ".", dotDot = "..";
  int i      = getLastSlash(m_path);  // cerco l'ultimo slash
  string str = m_path.substr(i + 1);  // str e' il path senza parentdir
  assert(str != dot && str != dotDot);
  int j = str.rfind('.');
  if (j == string::npos) {
    if (frame.isEmptyFrame() || frame.isNoFrame())
      return *this;
    else
      return TFilePath(m_path + "." + frame.expand(format));
  }

  string frameString;
  if (frame.isNoFrame())
    frameString = "";
  else
    frameString = "." + frame.expand(format);

  int k = str.substr(0, j).rfind('.');
  if (k == string::npos)
    return TFilePath(m_path.substr(0, j + i + 1) + frameString + str.substr(j));
  else
    return TFilePath(m_path.substr(0, k + i + 1) + frameString + str.substr(j));
}

//-----------------------------------------------------------------------------

bool TFilePath::isAncestorOf(const TFilePath &fp) const {
  size_t len = m_path.length();

  if (len == 0) {
    // il punto e' antenato di tutti i path non assoluti
    return !fp.isAbsolute();
  }

  return len < fp.m_path.length()  // l'antenato deve essere piu' corto
         &&
         (m_path[len - 1] == slash  // deve finire con slash se e' "/" o "C:\"
          ||
          fp.m_path[len] ==
              slash)  // negli altri casi ci deve essere uno slash subito dopo
         &&
#ifdef WIN32
         toLower(m_path) == toLower(fp.m_path.substr(0, len));
#else
         m_path == fp.m_path.substr(0, len);
#endif
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::operator-(const TFilePath &fp) const {
#ifdef WIN32
  if (toLower(m_path) == toLower(fp.m_path)) return TFilePath("");
#else
  if (m_path == fp.m_path) return TFilePath("");
#endif
  if (!fp.isAncestorOf(*this)) return *this;
  int len = fp.m_path.length();
  if (len == 0 || fp.m_path[len - 1] != slash) len++;

  return TFilePath(m_path.substr(len));
}

//-----------------------------------------------------------------------------

bool TFilePath::match(const TFilePath &fp) const {
  return getParentDir() == fp.getParentDir() && getName() == fp.getName() &&
         getFrame() == fp.getFrame() && getType() == fp.getType();
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::decode(const std::map<string, string> &dictionary) const {
  TFilePath parent   = getParentDir();
  TFilePath filename = withParentDir("");
  std::map<string, string>::const_iterator it =
      dictionary.find(filename.getFullPath());
  if (it != dictionary.end()) filename = TFilePath(it->second);
  if (parent.isEmpty())
    return filename;
  else
    return parent.decode(dictionary) + filename;
}
