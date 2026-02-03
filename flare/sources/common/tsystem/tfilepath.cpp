

#ifdef _WIN32
// #define UNICODE  // for UNC conversion functions
#include <windows.h>
#include <lm.h>

const char slash        = '\\';
const char auxslash     = '/';
const wchar_t wslash    = L'\\';
const wchar_t wauxslash = L'/';
#else
const char slash        = '/';
const char auxslash     = '\\';
const wchar_t wslash    = L'/';
const wchar_t wauxslash = L'\\';
#endif

//=============================================================================

#include "tfilepath.h"
#include "tconvert.h"
#include "tfiletype.h"
#include <cmath>
#include <cctype>
#include <sstream>

// QT
#include <QObject>
#include <QRegularExpression>

bool TFilePath::m_underscoreFormatAllowed = true;

// specifies file path condition for sequential image for each project.
// See filepathproperties.h
bool TFilePath::m_useStandard             = true;
bool TFilePath::m_acceptNonAlphabetSuffix = false;
int TFilePath::m_letterCountForSuffix     = 1;

namespace {

// Returns true if the string between the fromSeg position and the toSeg
// position is "4 digits".
bool isNumbers(std::wstring str, int fromSeg, int toSeg) {
  /*
    if (toSeg - fromSeg != 5) return false;
    for (int pos = fromSeg + 1; pos < toSeg; pos++) {
      if (str[pos] < '0' || str[pos] > '9') return false;
    }
  */
  // Let's check if it follows the format ####A (i.e 00001 or 00001a)
  int numDigits = 0, numLetters = 0;
  for (int pos = fromSeg + 1; pos < toSeg; pos++) {
    if ((str[pos] >= L'A' && str[pos] <= L'Z') ||
        (str[pos] >= L'a' && str[pos] <= L'z')) {
      // Not the right format if we ran into a letter without first finding a
      // number
      if (!numDigits) return false;

      // We'll keep track of the number of letters we find.
      // NOTE: From here on out we should only see letters
      numLetters++;
    } else if (str[pos] >= L'0' && str[pos] <= L'9') {
      // Not the right format if we ran into a number that followed a letter.
      // This format is not something we expect currently
      if (numLetters) return false;  // not right format

      // We'll keep track of the number of digits we find.
      numDigits++;
    } else  // Not the right format if we found something we didn't expect
      return false;
  }

  // Not the right format if we see too many letters. At the time of this
  // logic, we only expect 1 letter.  Can expand to 2 or more later.

  if (numLetters > 1) return false;

  return true;  // We're good!
}

bool checkForSeqNum(QString type) {
  TFileType::Type typeInfo = TFileType::getInfoFromExtension(type);
  if ((typeInfo & TFileType::IMAGE) && !(typeInfo & TFileType::LEVEL))
    return true;
  else
    return false;
}

bool parseFrame(const std::wstring &str, int &frame, QString &letter,
                int &padding) {
  if (str.empty()) return false;

  int i = 0, number = 0;
  while (i < (int)str.size() && str[i] >= L'0' && str[i] <= L'9')
    number = number * 10 + str[i++] - L'0';
  int digits = i;
  wchar_t l  = str[i] >= L'a' && str[i] <= L'z'   ? str[i++]
               : str[i] >= L'A' && str[i] <= L'Z' ? str[i++]
                                                  : L'\0';
  if (digits <= 0 || i < (int)str.size()) return false;

  frame   = number;
  letter  = l ? QString(1, QChar(l)) : QString();
  padding = str[0] == L'0' ? digits : 0;
  return true;
}

};  // namespace

TFrameId::TFrameId(const std::string &str, char s)
    : m_frame(EMPTY_FRAME), m_letter(), m_zeroPadding(4), m_startSeqInd(s) {
  if (str.empty()) return;
  if (!parseFrame(to_wstring(str), m_frame, m_letter, m_zeroPadding))
    m_frame = NO_FRAME;
}

TFrameId::TFrameId(const std::wstring &str, char s)
    : m_frame(EMPTY_FRAME), m_letter(), m_zeroPadding(4), m_startSeqInd(s) {
  if (str.empty()) return;
  if (!parseFrame(str, m_frame, m_letter, m_zeroPadding)) m_frame = NO_FRAME;
}

// TFrameId::operator string() const
std::string TFrameId::expand(FrameFormat format) const {
  if (m_frame == EMPTY_FRAME)
    return "";
  else if (m_frame == NO_FRAME)
    return "-";
  std::ostringstream o_buff;
  if (format == FOUR_ZEROS || format == UNDERSCORE_FOUR_ZEROS) {
    o_buff.fill('0');
    o_buff.width(4);
    o_buff << m_frame;
    o_buff.width(0);
  } else if (format == CUSTOM_PAD || format == UNDERSCORE_CUSTOM_PAD) {
    o_buff.fill('0');
    o_buff.width(m_zeroPadding);
    o_buff << m_frame;
    o_buff.width(0);
  } else {
    o_buff << m_frame;
  }
  if (m_letter.isEmpty())
    return o_buff.str();
  else
    return o_buff.str() + m_letter.toStdString();
}

//-------------------------------------------------------------------

const TFrameId &TFrameId::operator++() {
  ++m_frame;
  m_letter = "";
  // m_letter = 0;
  return *this;
}

//-------------------------------------------------------------------

const TFrameId &TFrameId::operator--() {
  if (!m_letter.isEmpty()) m_letter = "";
  // if (m_letter > 0)
  //  m_letter = 0;
  else
    --m_frame;
  return *this;
}

//=============================================================================

inline bool isSlash(char c) { return c == slash || c == auxslash; }

//-----------------------------------------------------------------------------

inline bool isSlash(wchar_t c) { return c == wslash || c == wauxslash; }

//-----------------------------------------------------------------------------

// look for the last '/' or '\'. If not found return -1
// so that the substring starting from getLastSlash() + 1 is name.frame.type
inline int getLastSlash(const std::wstring &path) {
  int i;
  for (i = path.length() - 1; i >= 0 && !isSlash(path[i]); i--) {
  }
  return i;
}

//-----------------------------------------------------------------------------

/*
void TFilePath::setPath(string path)
{
bool isUncName = false;
  // remove '//', './' and trailing '/'; fix wrong slashes.
  // if path starts with "<alpha>:" add a slash
  int length =path.length();
  int pos = 0;
  if(path.length()>=2 && isalpha(path[0]) && path[1] == ':')
    {
     m_path.append(path,0,2);
     pos=2;
     if(path.length()==2 || !isSlash(path[pos])) m_path.append(1,slash);
    }
#ifdef _WIN32
  else  // if it's a UNC path it's like "\\\\MachineName"
        // DOUBLE CHECK! IF WE HAVE IP ADDRESS IT FAILED!
    if (path.length() >= 3 && path[0] == '\\' &&  path[1] == '\\' &&
(isalpha(path[2]) || isdigit(path[2])) )
      {
      isUncName = true;
      m_path.append(path,0,3);
      pos = 3;
      }
#endif
  for(;pos<length;pos++)
    if(path[pos] == '.')
    {
      pos++;
      if(pos>=length)
      {
        if(pos>1) m_path.append(1,'.');
      }
      else if(!isSlash(path[pos])) m_path.append(path,pos-1,2);
      else {
         while(pos+1<length && isSlash(path[pos+1]))
           pos++;
      }
    }
    else if(isSlash(path[pos]))
    {
      do pos++;
      while(pos<length && isSlash(path[pos]));
      pos--;
      m_path.append(1,slash);
    }
    else
    {
      m_path.append(1,path[pos]);
    }

    // remove trailing '/' (unless m_path == "/" or m_path == "<letter>:\"
    // or it's UNC (Windows only) )
    if(!(m_path.length()==1 && m_path[0] == slash ||
         m_path.length()==3 && isalpha(m_path[0]) && m_path[1] == ':' &&
m_path[2] == slash)
      && m_path.length()>1 && m_path[m_path.length()-1] == slash)
      m_path.erase(m_path.length()-1, 1);

  if (isUncName && m_path.find_last_of('\\') == 1) // only machine name is
specified... m_path.append(1, slash);
}
*/

//-----------------------------------------------------------------------------
/*
void append(string &out, wchar_t c)
{
  if(32 <= c && c<=127 && c!='&') out.append(1, (char)c);
  else if(c=='&') out.append("&amp;");
  else
    {
     ostringstream ss;
     ss << "&#" <<  c << ";" << '\0';
     out += ss.str();
     ss.freeze(0);
    }
}
*/
//-----------------------------------------------------------------------------

void TFilePath::setPath(std::wstring path) {
  bool isUncName = false;
  // remove '//', './' and trailing '/'; fix wrong slashes.
  // if path starts with "<alpha>:" add a slash
  int length = path.length();
  int pos    = 0;

  if (path.length() >= 2 && iswalpha(path[0]) && path[1] == L':') {
    m_path.append(1, (wchar_t)path[0]);
    m_path.append(1, L':');
    // m_path.append(path,0,2);
    pos = 2;
    if (path.length() == 2 || !isSlash(path[pos])) m_path.append(1, wslash);
  }
  // if it's a UNC path it's like "\\\\MachineName"
  else if ((path.length() >= 3 && path[0] == L'\\' && path[1] == L'\\' &&
            iswalnum(path[2])) ||
           (path.length() >= 3 && path[0] == L'/' && path[1] == L'/' &&
            iswalnum(path[2]))) {
    isUncName = true;
    m_path.append(2, L'\\');
    m_path.append(1, path[2]);
    pos = 3;
  }
  for (; pos < length; pos++)
    if (path[pos] == L'.') {
      pos++;
      if (pos >= length) {
        if (pos > 1) m_path.append(1, L'.');
      } else if (!isSlash(path[pos])) {
        m_path.append(1, L'.');
        m_path.append(1, path[pos]);
      } else {
        while (pos + 1 < length && isSlash(path[pos + 1])) pos++;
      }
    } else if (isSlash(path[pos])) {
      int firstSlashPos = pos;
      do pos++;
      while (pos < length && isSlash(path[pos]));
      if (firstSlashPos == 0 && pos == 4)  // Case "\\\\MachineName"
        m_path.append(2, wslash);
      else
        m_path.append(1, wslash);
      pos--;
    } else {
      m_path.append(1, path[pos]);
    }

  // remove trailing '/' (unless m_path == "/" or m_path == "<letter>:\"
  // or it's UNC (Windows only) )
  if (!((m_path.length() == 1 && m_path[0] == wslash) ||
        (m_path.length() == 3 && iswalpha(m_path[0]) && m_path[1] == L':' &&
         m_path[2] == wslash)) &&
      (m_path.length() > 1 && m_path[m_path.length() - 1] == wslash))
    m_path.erase(m_path.length() - 1, 1);

  if (isUncName &&
      !(m_path.find_last_of(L'\\') > 1 ||
        m_path.find_last_of(L'/') > 1))  // only machine name is specified...
    m_path.append(1, wslash);
}

//-----------------------------------------------------------------------------

TFilePath::TFilePath(const char *path) { setPath(::to_wstring(path)); }

//-----------------------------------------------------------------------------

TFilePath::TFilePath(const std::string &path) { setPath(::to_wstring(path)); }

//-----------------------------------------------------------------------------

TFilePath::TFilePath(const std::wstring &path) { setPath(path); }

//-----------------------------------------------------------------------------

TFilePath::TFilePath(const QString &path) { setPath(path.toStdWString()); }

//-----------------------------------------------------------------------------

bool TFilePath::operator==(const TFilePath &fp) const {
#ifdef _WIN32
  return _wcsicmp(m_path.c_str(), fp.m_path.c_str()) == 0;
#else
  return m_path == fp.m_path;
#endif
}

//-----------------------------------------------------------------------------

bool TFilePath::operator<(const TFilePath &fp) const {
  std::wstring iName = m_path;
  std::wstring jName = fp.m_path;
  int i1 = 0, j1 = 0;
  int i2 = m_path.find(L"\\");
  int j2 = fp.m_path.find(L"\\");
  if (i2 == j2 && j2 == -1)
#ifdef _WIN32
    return _wcsicmp(m_path.c_str(), fp.m_path.c_str()) < 0;
#else
    return m_path < fp.m_path;
#endif
  if (!i2) {
    ++i1;
    i2 = m_path.find(L"\\", i1);
  }
  if (!j2) {
    ++j1;
    j2 = fp.m_path.find(L"\\", j1);
  }
  while (i2 != -1 || j2 != -1) {
    iName = (i2 != -1) ? m_path.substr(i1, i2 - i1) : m_path;
    jName = (j2 != -1) ? fp.m_path.substr(j1, j2 - j1) : fp.m_path;
// if the two path parts, between slashes, are equal
// iterate the comparison process otherwise return
#ifdef _WIN32
    char differ;
    differ = _wcsicmp(iName.c_str(), jName.c_str());
    if (differ != 0) return differ < 0 ? true : false;
#else
    if (TFilePath(iName) != TFilePath(jName))
      return TFilePath(iName) < TFilePath(jName);
#endif
    i1 = (i2 != -1) ? i2 + 1 : m_path.size();
    j1 = (j2 != -1) ? j2 + 1 : fp.m_path.size();
    i2 = m_path.find(L"\\", i1);
    j2 = fp.m_path.find(L"\\", j1);
  }

  iName = m_path.substr(i1, m_path.size() - i1);
  jName = fp.m_path.substr(j1, fp.m_path.size() - j1);
#ifdef _WIN32
  return _wcsicmp(iName.c_str(), jName.c_str()) < 0;
#else
  return TFilePath(iName) < TFilePath(jName);
#endif
}

#ifdef LEVO
bool TFilePath::operator<(const TFilePath &fp) const {
  /*
wstring a = m_path, b = fp.m_path;
for(;;)
{
wstring ka,kb;
int i;
i = a.find_first_of("/\\");
if(i==wstring::npos) {ka = a; a = L"";}
i = b.find_first_of("/\\");
if(i==wstring::npos) {ka = a; a = L"";}

}
*/
  wstring a = toLower(m_path), b = toLower(fp.m_path);
  return a < b;
}
#endif

//-----------------------------------------------------------------------------

TFilePath &TFilePath::operator+=(const TFilePath &fp) {
  if (fp.isAbsolute()) {
    // Log error or handle appropriately
    qWarning("Cannot append absolute path to another path");
    return *this;
  }

  if (fp.isEmpty())
    return *this;
  else if (isEmpty()) {
    *this = fp;
    return *this;
  } else if (m_path.length() != 1 || m_path[0] != slash) {
    if (m_path.empty()) {
      // Handle empty path case
      *this = fp;
      return *this;
    }
    if (!isSlash(m_path[m_path.length() - 1])) m_path.append(1, wslash);
    m_path += fp.m_path;
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

TFilePath &TFilePath::operator+=(const std::wstring &s) {
  if (s.empty()) return *this;
  // if(m_path.length()!=1 || m_path[0] != slash)
  //  m_path += slash;
  if (m_path.length() > 0 && !isSlash(m_path[m_path.length() - 1]))
    m_path.append(1, wslash);
  m_path += s;
  return *this;
}

//-----------------------------------------------------------------------------

const std::wstring TFilePath::getWideString() const {
  return m_path;
  /*
std::wstring s;
int n = m_path.size();
for(int i=0;i<n; i++)
{
char c = m_path[i];
if(c!='&') s.append(1, (unsigned short)c);
else
 {
  i++;
  if(m_path[i] == '#')
    {
     unsigned short w = 0;
     i++;
     while(i<n)
       {
        c = m_path[i];
        if('0'<=c && c<='9')
          w = w*10 + c - '0';
        else if('a' <=c && c<='f')
          w = w*10 + c - 'a' + 10;
        else if('A' <=c && c<='F')
          w = w*10 + c - 'A' + 10;
        else
          break;
        i++;
       }
     s.append(1, w);
    }
 }
}
return s;
*/
}
/*
#else
const wstring TFilePath::getWideString() const
{
   wstring a(L"dummy string");
   return a;
}
#endif
*/

//-----------------------------------------------------------------------------

QString TFilePath::getQString() const {
  return QString::fromStdWString(getWideString());
}

//-----------------------------------------------------------------------------

std::ostream &operator<<(std::ostream &out, const TFilePath &path) {
  std::wstring w = path.getWideString();
  return out << ::to_string(w) << " ";
  //  string w = path.getString();
  //  return out << w << " ";
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
  return ((m_path.length() >= 1 && m_path[0] == slash) ||
          (m_path.length() >= 2 && iswalpha(m_path[0]) && m_path[1] == ':'));
}

//-----------------------------------------------------------------------------

bool TFilePath::isRoot() const {
  return ((m_path.length() == 1 && m_path[0] == slash) ||
          (m_path.length() == 3 && iswalpha(m_path[0]) && m_path[1] == ':' &&
           m_path[2] == slash) ||
          ((m_path.length() > 2 && m_path[0] == slash && m_path[1] == slash) &&
           (std::string::npos == m_path.find(slash, 2) ||
            m_path.find(slash, 2) == (m_path.size() - 1))));
}

//-----------------------------------------------------------------------------

// returns "" (no type, no dot), "." (file with type) or ".." (file with type
// and frame)
std::string TFilePath::getDots() const {
  if (!TFilePath::m_useStandard) {
    TFilePathInfo info = analyzePath();
    if (info.extension.isEmpty()) return "";
    if (info.sepChar.isNull()) return ".";
    // return ".." regardless of sepChar type (either "_" or ".")
    return "..";
  }
  //-----

  QString type = QString::fromStdString(getType()).toLower();
  if (isFfmpegType()) return ".";
  int i            = getLastSlash(m_path);
  std::wstring str = m_path.substr(i + 1);
  // I could also have a.b.c.d where d is the extension
  i = str.rfind(L".");
  if (i == (int)std::wstring::npos || str == L"..") return "";

  int j = str.substr(0, i).rfind(L".");
  if (j == (int)std::wstring::npos && m_underscoreFormatAllowed)
    j = str.substr(0, i).rfind(L"_");

  if (j != (int)std::wstring::npos)
    return (j == i - 1 || (checkForSeqNum(type) && isNumbers(str, j, i))) ? ".."
                                                                          : ".";
  else
    return ".";
}

//-----------------------------------------------------------------------------

QChar TFilePath::getSepChar() const {
  if (!TFilePath::m_useStandard) return analyzePath().sepChar;
  //-----
  QString type = QString::fromStdString(getType()).toLower();
  if (isFfmpegType()) return QChar();
  int i            = getLastSlash(m_path);
  std::wstring str = m_path.substr(i + 1);
  // I could also have a.b.c.d where d is the extension
  i = str.rfind(L".");
  if (i == (int)std::wstring::npos || str == L"..") return QChar();

  int j = str.substr(0, i).rfind(L".");

  if (j != (int)std::wstring::npos)
    return (j == i - 1 || (checkForSeqNum(type) && isNumbers(str, j, i)))
               ? QChar('.')
               : QChar();
  if (!m_underscoreFormatAllowed) return QChar();

  j = str.substr(0, i).rfind(L"_");
  if (j != (int)std::wstring::npos)
    return (j == i - 1 || (checkForSeqNum(type) && isNumbers(str, j, i)))
               ? QChar('_')
               : QChar();
  else
    return QChar();
}

//-----------------------------------------------------------------------------

std::string TFilePath::getDottedType()
    const  // returns extension with DOT (if present)
{
  if (!TFilePath::m_useStandard) {
    QString ext = analyzePath().extension;
    if (ext.isEmpty()) return "";
    return "." + ext.toLower().toStdString();
  }

  int i            = getLastSlash(m_path);
  std::wstring str = m_path.substr(i + 1);
  i                = str.rfind(L".");
  if (i == (int)std::wstring::npos) return "";

  return toLower(::to_string(str.substr(i)));
}

//-----------------------------------------------------------------------------

std::string TFilePath::getUndottedType() const  // returns extension without DOT
{
  if (!TFilePath::m_useStandard) {
    QString ext = analyzePath().extension;
    if (ext.isEmpty())
      return "";
    else
      return ext.toLower().toStdString();
  }

  size_t i         = getLastSlash(m_path);
  std::wstring str = m_path.substr(i + 1);
  i                = str.rfind(L".");
  if (i == std::wstring::npos || i == str.length() - 1) return "";
  return toLower(::to_string(str.substr(i + 1)));
}

//-----------------------------------------------------------------------------

std::wstring TFilePath::getWideName() const  // noDot! noSlash!
{
  if (!TFilePath::m_useStandard) {
    return analyzePath().levelName.toStdWString();
  }

  QString type     = QString::fromStdString(getType()).toLower();
  int i            = getLastSlash(m_path);  // look for the last slash
  std::wstring str = m_path.substr(i + 1);
  i                = str.rfind(L".");
  if (i == (int)std::wstring::npos) return str;
  int j = str.substr(0, i).rfind(L".");
  if (j != (int)std::wstring::npos) {
    if (checkForSeqNum(type) && isNumbers(str, j, i)) i = j;
  } else if (m_underscoreFormatAllowed) {
    j = str.substr(0, i).rfind(L"_");
    if (j != (int)std::wstring::npos && checkForSeqNum(type) &&
        isNumbers(str, j, i))
      i = j;
  }
  return str.substr(0, i);
}

//-----------------------------------------------------------------------------

std::string TFilePath::getName() const  // noDot! noSlash!
{
  return ::to_string(getWideName());
}

//-----------------------------------------------------------------------------
// e.g. TFilePath("/pippo/pluto.0001.gif").getLevelName() == "pluto..gif"
std::string TFilePath::getLevelName() const {
  return ::to_string(getLevelNameW());
}

//-----------------------------------------------------------------------------
// e.g. TFilePath("/pippo/pluto.0001.gif").getLevelName() == "pluto..gif"

std::wstring TFilePath::getLevelNameW() const {
  if (!TFilePath::m_useStandard) {
    TFilePathInfo info = analyzePath();
    if (info.extension.isEmpty()) return info.levelName.toStdWString();
    QString name = info.levelName;
    if (!info.sepChar.isNull()) name += info.sepChar;
    name += "." + info.extension;
    return name.toStdWString();
  }

  int i            = getLastSlash(m_path);  // look for the last slash
  std::wstring str = m_path.substr(i + 1);  // str is m_path without directory
  QString type     = QString::fromStdString(getType()).toLower();
  if (isFfmpegType()) return str;
  int j = str.rfind(L".");                       // str[j..] = ".type"
  if (j == (int)std::wstring::npos) return str;  // no frame; no type
  i = str.substr(0, j).rfind(L'.');
  if (i == (int)std::wstring::npos && m_underscoreFormatAllowed)
    i = str.substr(0, j).rfind(L'_');

  if (j == i || j - i == 1)  // prova.tif or prova..tif
    return str;

  if (!checkForSeqNum(type) || !isNumbers(str, i, j) ||
      i == (int)std::wstring::npos)
    return str;
  // prova.0001.tif
  return str.erase(i + 1, j - i - 1);
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::getParentDir() const  // noSlash!
{
  int i = getLastSlash(m_path);  // look for the last slash
  if (i < 0) {
    if (m_path.length() >= 2 &&
        ((L'a' <= m_path[0] && m_path[0] <= L'z') ||
         (L'A' <= m_path[0] && m_path[0] <= L'Z')) &&
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
// return true if the fID is EMPTY_FRAME
bool TFilePath::isLevelName() const {
  if (!TFilePath::m_useStandard) {
    return analyzePath().fId.getNumber() == TFrameId::EMPTY_FRAME;
  }

  QString type = QString::fromStdString(getType()).toLower();
  if (isFfmpegType() || !checkForSeqNum(type)) return false;
  try {
    return getFrame() == TFrameId(TFrameId::EMPTY_FRAME);
  } catch (...) {
    return false;
  }
}

TFrameId TFilePath::getFrame() const {
  if (!TFilePath::m_useStandard) {
    return analyzePath().fId;
  }

  //-----
  int i            = getLastSlash(m_path);  // look for the last slash
  std::wstring str = m_path.substr(i + 1);  // str is the path without parentdir
  QString type     = QString::fromStdString(getType()).toLower();
  i                = str.rfind(L'.');
  if (i == (int)std::wstring::npos || str == L"." || str == L"..")
    return TFrameId(TFrameId::NO_FRAME);
  int j;

  j = str.substr(0, i).rfind(L'.');
  if (j == (int)std::wstring::npos && m_underscoreFormatAllowed)
    j = str.substr(0, i).rfind(L'_');

  if (j == (int)std::wstring::npos) return TFrameId(TFrameId::NO_FRAME);
  if (i == j + 1) return TFrameId(TFrameId::EMPTY_FRAME);

  // Exclude cases with non-numeric characters inbetween. (In case the file name
  // contains "_" or ".")
  if (!checkForSeqNum(type) || !isNumbers(str, j, i))
    return TFrameId(TFrameId::NO_FRAME);

  return TFrameId(str.substr(j + 1, i - j - 1), str[j]);
}

//-----------------------------------------------------------------------------

bool TFilePath::isFfmpegType() const {
  QString type = QString::fromStdString(getType()).toLower();
  return (type == "gif" || type == "mp4" || type == "webm");
}

//-----------------------------------------------------------------------------

bool TFilePath::isUneditable() const {
  QString type = QString::fromStdString(getType()).toLower();
  return (type == "psd" || type == "gif" || type == "mp4" || type == "webm");
}

//-----------------------------------------------------------------------------
TFilePath TFilePath::withType(const std::string &type) const {
  if (type.length() >= 2 && type.substr(0, 2) == "..") {
    qWarning("Invalid type: %s", type.c_str());
    return *this;
  }

  int i            = getLastSlash(m_path);  // look for the last slash
  std::wstring str = m_path.substr(i + 1);  // str is the path without parentdir
  int j            = str.rfind(L'.');
  if (j == (int)std::wstring::npos || str == L"..")
  // the original path has no type
  {
    if (type == "")
      return *this;
    else if (type[0] == '.')
      return TFilePath(m_path + ::to_wstring(type));
    else
      return TFilePath(m_path + ::to_wstring("." + type));
  } else
  // the original path already has a type
  {
    if (type == "")
      return TFilePath(m_path.substr(0, i + j + 1));
    else if (type[0] == '.')
      return TFilePath(m_path.substr(0, i + j + 1) + ::to_wstring(type));
    else
      return TFilePath(m_path.substr(0, i + j + 2) + ::to_wstring(type));
  }
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withName(const std::string &name) const {
  return withName(::to_wstring(name));
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withName(const std::wstring &name) const {
  if (!TFilePath::m_useStandard) {
    TFilePathInfo info = analyzePath();

    QString ret = info.parentDir + QString::fromStdWString(name);
    if (info.fId.getNumber() != TFrameId::NO_FRAME) {
      QString sepChar = (info.sepChar.isNull()) ? "." : QString(info.sepChar);
      ret += sepChar + QString::fromStdString(
                           info.fId.expand(info.fId.getCurrentFormat()));
    }
    if (!info.extension.isEmpty()) ret += "." + info.extension;

    return TFilePath(ret);
  }

  int i            = getLastSlash(m_path);  // look for the last slash
  std::wstring str = m_path.substr(i + 1);  // str is the path without parentdir
  QString type     = QString::fromStdString(getType()).toLower();
  int j;
  j = str.rfind(L'.');

  if (j == (int)std::wstring::npos) {
    if (m_underscoreFormatAllowed) {
      j = str.rfind(L'_');
      if (j != (int)std::wstring::npos)
        return TFilePath(m_path.substr(0, i + 1) + name + str.substr(j));
    }
    return TFilePath(m_path.substr(0, i + 1) + name);
  }
  int k;

  k = str.substr(0, j).rfind(L".");
  if (k == (int)std::wstring::npos && m_underscoreFormatAllowed)
    k = str.substr(0, j).rfind(L"_");

  if (k == (int)(std::wstring::npos))
    k = j;
  else if (k != j - 1 && (!checkForSeqNum(type) || !isNumbers(str, k, j)))
    k = j;

  return TFilePath(m_path.substr(0, i + 1) + name + str.substr(k));
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withParentDir(const TFilePath &dir) const {
  int i = getLastSlash(m_path);  // look for the last slash
  return dir + TFilePath(m_path.substr(i + 1));
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::withFrame(const TFrameId &frame,
                               TFrameId::FrameFormat format) const {
  if (!TFilePath::m_useStandard) {
    TFilePathInfo info = analyzePath();
    // Override format input because it may be wrong.
    if (checkForSeqNum(info.extension)) format = frame.getCurrentFormat();
    // override format if the original fid is available
    else if (info.fId.getNumber() != TFrameId::NO_FRAME)
      format = info.fId.getCurrentFormat();

    if (info.extension.isEmpty()) {
      if (frame.isEmptyFrame() || frame.isNoFrame()) return *this;

      return TFilePath(m_path + L"." + ::to_wstring(frame.expand(format)));
    }
    if (frame.isNoFrame()) {
      return TFilePath(info.parentDir + info.levelName + "." + info.extension);
    }
    QString sepChar = (info.sepChar.isNull()) ? QString(frame.getStartSeqInd())
                                              : QString(info.sepChar);

    return TFilePath(info.parentDir + info.levelName + sepChar +
                     QString::fromStdString(frame.expand(format)) + "." +
                     info.extension);
  }

  //-----------------

  const std::wstring dot = L".", dotDot = L"..";
  int i            = getLastSlash(m_path);  // look for the last slash
  std::wstring str = m_path.substr(i + 1);  // str is the path without parentdir
  QString type     = QString::fromStdString(getType()).toLower();
  if (str == dot || str == dotDot) {
    qWarning("Invalid path for frame operation: %ls", str.c_str());
    return *this;
  }

  int j          = str.rfind(L'.');
  const char *ch = ".";
  // Override format input because it may be wrong.
  if (!isFfmpegType() && checkForSeqNum(type))
    format = frame.getCurrentFormat();
  if (m_underscoreFormatAllowed && (format == TFrameId::UNDERSCORE_FOUR_ZEROS ||
                                    format == TFrameId::UNDERSCORE_NO_PAD ||
                                    format == TFrameId::UNDERSCORE_CUSTOM_PAD))
    ch = "_";

  // no extension case
  if (j == (int)std::wstring::npos) {
    if (frame.isEmptyFrame() || frame.isNoFrame())
      return *this;
    else
      return TFilePath(m_path + ::to_wstring(ch + frame.expand(format)));
  }

  int k = str.substr(0, j).rfind(L'.');

  bool hasValidFrameNum = false;
  if (!isFfmpegType() && checkForSeqNum(type) && isNumbers(str, k, j))
    hasValidFrameNum = true;
  std::string frameString;
  if (frame.isNoFrame())
    frameString = "";
  else if (!frame.isEmptyFrame() && getDots() != "." && !hasValidFrameNum) {
    if (k != (int)std::wstring::npos) {
      std::wstring wstr = str.substr(k, j - k);
      std::string str2(wstr.begin(), wstr.end());
      frameString = str2;
    } else
      frameString = "";
  } else
    frameString = ch + frame.expand(format);

  if (k != (int)std::wstring::npos)
    return TFilePath(m_path.substr(0, k + i + 1) + ::to_wstring(frameString) +
                     str.substr(j));
  else if (m_underscoreFormatAllowed) {
    k = str.substr(0, j).rfind(L'_');
    if (k != (int)std::wstring::npos &&
        (k == j - 1 ||
         (checkForSeqNum(type) &&
          isNumbers(str, k,
                    j))))  // -- In case of "_." or "_[numbers]." --
      return TFilePath(m_path.substr(0, k + i + 1) +
                       ((frame.isNoFrame())
                            ? L""
                            : ::to_wstring("_" + frame.expand(format))) +
                       str.substr(j));
  }
  return TFilePath(m_path.substr(0, j + i + 1) + ::to_wstring(frameString) +
                   str.substr(j));
}

//-----------------------------------------------------------------------------

bool TFilePath::isAncestorOf(const TFilePath &possibleDescendent) const {
  size_t len = m_path.length();
  if (len == 0) {
    // the dot is ancestor of all non-absolute paths
    return !possibleDescendent.isAbsolute();
  }

  // a path is ancestor of itself
  if (m_path == possibleDescendent.m_path) return true;

  int possibleDescendentLen = possibleDescendent.m_path.length();
  // otherwise the ancestor must be shorter
  if ((int)len >= possibleDescendentLen) return false;
  // and must match the first part of the descendant
  if (toLower(m_path) != toLower(possibleDescendent.m_path.substr(0, len)))
    return false;
  // if the ancestor does not end with slash there must be a slash in the
  // descendant, right after
  if (m_path[len - 1] != wslash && possibleDescendent.m_path[len] != wslash)
    return false;

  return true;
}

//-----------------------------------------------------------------------------

TFilePath TFilePath::operator-(const TFilePath &fp) const {
  if (toLower(m_path) == toLower(fp.m_path)) return TFilePath("");
  if (!fp.isAncestorOf(*this)) return *this;
  int len = fp.m_path.length();
  if (len == 0 || fp.m_path[len - 1] != wslash) len++;

  return TFilePath(m_path.substr(len));
}

//-----------------------------------------------------------------------------

bool TFilePath::match(const TFilePath &fp) const {
  if (!TFilePath::m_useStandard) {
    if (getParentDir() != fp.getParentDir()) return false;

    TFilePathInfo info     = analyzePath();
    TFilePathInfo info_ext = fp.analyzePath();

    return (info.levelName == info_ext.levelName && info.fId == info_ext.fId &&
            info.extension == info_ext.extension);
  }

  return getParentDir() == fp.getParentDir() && getName() == fp.getName() &&
         getFrame() == fp.getFrame() && getType() == fp.getType();
}

//-----------------------------------------------------------------------------

void TFilePath::split(std::wstring &head, TFilePath &tail) const {
  TFilePath ancestor = getParentDir();
  if (ancestor == TFilePath()) {
    head = getWideString();
    tail = TFilePath();
    return;
  }
  for (;;) {
    if (ancestor.isRoot()) break;
    TFilePath p = ancestor.getParentDir();
    if (p == TFilePath()) break;
    ancestor = p;
  }
  head = ancestor.getWideString();
  tail = *this - ancestor;
}

//-----------------------------------------------------------------------------

QString TFilePath::fidRegExpStr() {
  if (m_useStandard) return QString("(\\d+)([a-zA-Z]?)");
  QString suffixLetter = (m_acceptNonAlphabetSuffix)
                             ? "[^\\._ \\\\/:,;*?\"<>|0123456789]"
                             : "[a-zA-Z]";
  QString countLetter  = (m_letterCountForSuffix == 0)
                             ? "{0,}"
                             : (QString("{0,%1}").arg(m_letterCountForSuffix));
  return QString("(\\d+)(%1%2)").arg(suffixLetter).arg(countLetter);
  // const QString fIdRegExp("(\\d+)([a-zA-Z]?)");
}

//-----------------------------------------------------------------------------

TFilePath::TFilePathInfo TFilePath::analyzePath() const {
  // analyzePath should only be called when m_useStandard is false
  if (TFilePath::m_useStandard) {
#ifdef QT_DEBUG
    qWarning(
        "analyzePath called with m_useStandard enabled - returning empty "
        "TFilePathInfo");
#endif
    // Return empty structure since standard path analysis should be used
    // instead
    return TFilePathInfo();
  }

  TFilePath::TFilePathInfo info;

  int i            = getLastSlash(m_path);
  std::wstring str = m_path.substr(i + 1);

  if (i >= 0) info.parentDir = QString::fromStdWString(m_path.substr(0, i + 1));

  QString fileName = QString::fromStdWString(str);

  // Level Name : letters other than  \/:,;*?"<>|
  const QString levelNameRegExp("([^\\\\/:,;*?\"<>|]+)");
  // Sep Char : period or underscore
  const QString sepCharRegExp("([\\._])");
  // Frame Number and Suffix
  QString fIdRegExp = TFilePath::fidRegExpStr();

  // Extension: letters other than "._" or  \/:,;*?"<>|  or " "(space)
  const QString extensionRegExp("([^\\._ \\\\/:,;*?\"<>|]+)");

  // Modern QRegularExpression implementation
  QRegularExpression rx("^(?:" + levelNameRegExp + ")?" + sepCharRegExp +
                        "(?:" + fIdRegExp + ")?" + "\\." + extensionRegExp +
                        "$");
  QRegularExpressionMatch match = rx.match(fileName);

  if (match.hasMatch()) {
    info.levelName = match.captured(1);
    info.sepChar =
        match.captured(2).isEmpty() ? QChar() : match.captured(2).at(0);
    info.extension = match.captured(5);

    // ignore frame numbers on non-sequential (i.e. movie) extension case
    if (!checkForSeqNum(info.extension)) {
      info.levelName = match.captured(1) + match.captured(2);
      if (!match.captured(3).isEmpty()) info.levelName += match.captured(3);
      if (!match.captured(4).isEmpty()) info.levelName += match.captured(4);
      info.sepChar = QChar();
      info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0);  // initialize with NO_PAD
    } else {
      QString numberStr = match.captured(3);
      if (numberStr.isEmpty()) {  // empty frame case : hogehoge..jpg
        info.fId =
            TFrameId(TFrameId::EMPTY_FRAME, 0, 4, info.sepChar.toLatin1());
      } else {
        int number  = numberStr.toInt();
        int padding = 0;
        if (!numberStr.isEmpty() && numberStr[0] == '0') {  // with padding
          padding = numberStr.length();
        }
        QString suffix;
        if (!match.captured(4).isEmpty()) suffix = match.captured(4);
        info.fId = TFrameId(number, suffix, padding, info.sepChar.toLatin1());
      }
    }
    return info;
  }

  // no frame case : hogehoge.jpg
  // no level name case : .jpg
  QRegularExpression rx_nf("^(?:" + levelNameRegExp + ")?\\." +
                           extensionRegExp + "$");
  match = rx_nf.match(fileName);
  if (match.hasMatch()) {
    if (!match.captured(1).isEmpty()) info.levelName = match.captured(1);
    info.sepChar = QChar();
    info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0);  // initialize with NO_PAD
    info.extension = match.captured(2);
    return info;
  }

  // no periods
  info.levelName = fileName;
  info.sepChar   = QChar();
  info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0);  // initialize with NO_PAD
  info.extension = QString();
  return info;
}
