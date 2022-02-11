

#ifdef _WIN32
//#define UNICODE  // per le funzioni di conversione da/a UNC
#include <windows.h>
#include <lm.h>

const char slash     = '\\';
const char auxslash  = '/';
const char wslash    = L'\\';
const char wauxslash = L'/';
#else
const char slash     = '/';
const char auxslash  = '\\';
const char wslash    = '/';
const char wauxslash = '\\';
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
#include <QRegExp>

bool TFilePath::m_underscoreFormatAllowed = true;

// specifies file path condition for sequential image for each project.
// See filepathproperties.h
bool TFilePath::m_useStandard             = true;
bool TFilePath::m_acceptNonAlphabetSuffix = false;
int TFilePath::m_letterCountForSuffix     = 1;

namespace {

/*-- fromSeg位置 と
 * toSeg位置は含まず、それらの間に挟まれている文字列が「数字4ケタ」ならtrueを返す
 * --*/
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
    if ((str[pos] >= 'A' && str[pos] <= 'Z') ||
        (str[pos] >= 'a' && str[pos] <= 'z')) {
      // Not the right format if we ran into a letter without first finding a
      // number
      if (!numDigits) return false;

      // We'll keep track of the number of letters we find.
      // NOTE: From here on out we should only see letters
      numLetters++;
    } else if (str[pos] >= '0' && str[pos] <= '9') {
      // Not the right format if we ran into a number that followed a letter.
      // This format is not something we expect currently
      if (numLetters) return false;  // not right format

      // We'll keep track of the number of digits we find.
      numDigits++;
    } else  // Not the right format if we found something we didn't expect
      return false;
  }

  // Not the right format if we see too many letters.
  // At the time of this logic, we only expect 1 letter.  Can expand to 2 or
  // more later, if we want.
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
};  // namespace

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

// cerca l'ultimo '/' o '\'. Se non c'e' ritorna -1
// di modo che la sottostringa che parte da getLastSlash() + 1 e'
// nome.frame.tipo
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
  // elimino i '//', './' e '/' finali; raddrizzo gli slash 'storti'.
  // se il path comincia con  "<alpha>:" aggiungo uno slash
  int length =path.length();
  int pos = 0;
  if(path.length()>=2 && isalpha(path[0]) && path[1] == ':')
    {
     m_path.append(path,0,2);
     pos=2;
     if(path.length()==2 || !isSlash(path[pos])) m_path.append(1,slash);
    }
#ifdef _WIN32
  else  //se si tratta di un path in formato UNC e' del tipo "\\\\MachineName"
        //RICONTROLLARE! SE SI HA IP ADDRESS FALLIVA!
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

    // rimuovo l'eventuale '/' finale (a meno che m_path == "/" o m_path ==
"<letter>:\"
    // oppure sia UNC (Windows only) )
    if(!(m_path.length()==1 && m_path[0] == slash ||
         m_path.length()==3 && isalpha(m_path[0]) && m_path[1] == ':' &&
m_path[2] == slash)
      && m_path.length()>1 && m_path[m_path.length()-1] == slash)
      m_path.erase(m_path.length()-1, 1);

  if (isUncName && m_path.find_last_of('\\') == 1) // e' indicato solo il nome
della macchina...
    m_path.append(1, slash);
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
  // elimino i '//', './' e '/' finali; raddrizzo gli slash 'storti'.
  // se il path comincia con  "<alpha>:" aggiungo uno slash
  int length = path.length();
  int pos    = 0;

  if (path.length() >= 2 && iswalpha(path[0]) && path[1] == L':') {
    m_path.append(1, (wchar_t)path[0]);
    m_path.append(1, L':');
    // m_path.append(path,0,2);
    pos = 2;
    if (path.length() == 2 || !isSlash(path[pos])) m_path.append(1, wslash);
  }
  // se si tratta di un path in formato UNC e' del tipo "\\\\MachineName"
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
      do
        pos++;
      while (pos < length && isSlash(path[pos]));
      if (firstSlashPos == 0 && pos == 4)  // Caso "\\\\MachineName"
        m_path.append(2, wslash);
      else
        m_path.append(1, wslash);
      pos--;
    } else {
      m_path.append(1, path[pos]);
    }

  // rimuovo l'eventuale '/' finale (a meno che m_path == "/" o m_path ==
  // "<letter>:\"
  // oppure sia UNC (Windows only) )
  if (!((m_path.length() == 1 && m_path[0] == wslash) ||
        (m_path.length() == 3 && iswalpha(m_path[0]) && m_path[1] == L':' &&
         m_path[2] == wslash)) &&
      (m_path.length() > 1 && m_path[m_path.length() - 1] == wslash))
    m_path.erase(m_path.length() - 1, 1);

  if (isUncName && !(m_path.find_last_of(L'\\') > 1 ||
                     m_path.find_last_of(L'/') >
                         1))  // e' indicato solo il nome della macchina...
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
// se le due parti di path, conpresi tra slash sono uguali
// itero il processo di confronto altrimenti ritorno
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
  assert(!fp.isAbsolute());
  if (fp.isEmpty())
    return *this;
  else if (isEmpty()) {
    *this = fp;
    return *this;
  } else if (m_path.length() != 1 || m_path[0] != slash) {
    assert(!m_path.empty());
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

// ritorna ""(niente tipo, niente punto), "." (file con tipo) o ".." (file con
// tipo e frame)
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
  // potrei anche avere a.b.c.d dove d e' l'estensione
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
  // potrei anche avere a.b.c.d dove d e' l'estensione
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
    const  // ritorna l'estensione con PUNTO (se c'e')
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

std::string TFilePath::getUndottedType()
    const  // ritorna l'estensione senza PUNTO
{
  if (!TFilePath::m_useStandard) {
    QString ext = analyzePath().extension;
    if (ext.isEmpty())
      return "";
    else
      return ext.toLower().toStdString();
  }

  //-----
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
  //-----

  QString type     = QString::fromStdString(getType()).toLower();
  int i            = getLastSlash(m_path);  // cerco l'ultimo slash
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
// es. TFilePath("/pippo/pluto.0001.gif").getLevelName() == "pluto..gif"
std::string TFilePath::getLevelName() const {
  return ::to_string(getLevelNameW());
}

//-----------------------------------------------------------------------------
// es. TFilePath("/pippo/pluto.0001.gif").getLevelName() == "pluto..gif"

std::wstring TFilePath::getLevelNameW() const {
  if (!TFilePath::m_useStandard) {
    TFilePathInfo info = analyzePath();
    if (info.extension.isEmpty()) return info.levelName.toStdWString();
    QString name = info.levelName;
    if (!info.sepChar.isNull()) name += info.sepChar;
    name += "." + info.extension;
    return name.toStdWString();
  }
  //-----

  int i            = getLastSlash(m_path);  // cerco l'ultimo slash
  std::wstring str = m_path.substr(i + 1);  // str e' m_path senza directory
  QString type     = QString::fromStdString(getType()).toLower();
  if (isFfmpegType()) return str;
  int j = str.rfind(L".");                       // str[j..] = ".type"
  if (j == (int)std::wstring::npos) return str;  // no frame; no type
  i = str.substr(0, j).rfind(L'.');
  if (i == (int)std::wstring::npos && m_underscoreFormatAllowed)
    i = str.substr(0, j).rfind(L'_');

  if (j == i || j - i == 1)  // prova.tif o prova..tif
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
  int i = getLastSlash(m_path);  // cerco l'ultimo slash
  if (i < 0) {
    if (m_path.length() >= 2 &&
        (('a' <= m_path[0] && m_path[0] <= 'z') ||
         ('A' <= m_path[0] && m_path[0] <= 'Z')) &&
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
  //-----

  QString type = QString::fromStdString(getType()).toLower();
  if (isFfmpegType() || !checkForSeqNum(type)) return false;
  try {
    return getFrame() == TFrameId(TFrameId::EMPTY_FRAME);
  }

  catch (...) {
    return false;
  }
}

TFrameId TFilePath::getFrame() const {
  if (!TFilePath::m_useStandard) {
    return analyzePath().fId;
  }

  //-----
  int i            = getLastSlash(m_path);  // cerco l'ultimo slash
  std::wstring str = m_path.substr(i + 1);  // str e' il path senza parentdir
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

  // 間が数字でない場合（ファイル名にまぎれた"_" や "."がある場合）を除外する
  if (!checkForSeqNum(type) || !isNumbers(str, j, i))
    return TFrameId(TFrameId::NO_FRAME);

  int k, number = 0, digits = 0;
  for (k = j + 1; k < i && iswdigit(str[k]); k++) {
    digits++;
    number = number * 10 + str[k] - L'0';
  }
  char letter = '\0';
  if (iswalpha(str[k])) letter = str[k++] + ('a' - L'a');

  //  if (number == 0 || k < i)  // || letter!='\0')
  //    throw TMalformedFrameException(
  //        *this,
  //        str + L": " + QObject::tr("Malformed frame name").toStdWString());
  int padding = 0;

  if (str[j + 1] == '0') padding = digits;

  return TFrameId(number, letter, padding, str[j]);
}

//-----------------------------------------------------------------------------

bool TFilePath::isFfmpegType() const {
  QString type = QString::fromStdString(getType()).toLower();
  if (type == "gif" || type == "mp4" || type == "webm")
    return true;
  else
    return false;
}

//-----------------------------------------------------------------------------
TFilePath TFilePath::withType(const std::string &type) const {
  assert(type.length() < 2 || type.substr(0, 2) != "..");
  int i            = getLastSlash(m_path);  // cerco l'ultimo slash
  std::wstring str = m_path.substr(i + 1);  // str e' il path senza parentdir
  int j            = str.rfind(L'.');
  if (j == (int)std::wstring::npos || str == L"..")
  // il path originale non ha tipo
  {
    if (type == "")
      return *this;
    else if (type[0] == '.')
      return TFilePath(m_path + ::to_wstring(type));
    else
      return TFilePath(m_path + ::to_wstring("." + type));
  } else
  // il path originale ha gia' il tipo
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

  int i            = getLastSlash(m_path);  // cerco l'ultimo slash
  std::wstring str = m_path.substr(i + 1);  // str e' il path senza parentdir
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
  int i = getLastSlash(m_path);  // cerco l'ultimo slash
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
  int i            = getLastSlash(m_path);  // cerco l'ultimo slash
  std::wstring str = m_path.substr(i + 1);  // str e' il path senza parentdir
  QString type     = QString::fromStdString(getType()).toLower();
  assert(str != dot && str != dotDot);
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
                    j))))  //-- "_." の並びか、"_[数字]."の並びのとき --
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
    // il punto e' antenato di tutti i path non assoluti
    return !possibleDescendent.isAbsolute();
  }

  // un path e' antenato di se stesso
  if (m_path == possibleDescendent.m_path) return true;

  int possibleDescendentLen = possibleDescendent.m_path.length();
  // altrimenti l'antenato deve essere piu' corto
  if ((int)len >= possibleDescendentLen) return false;
  // e deve coincidere con la prima parte del discendente
  if (toLower(m_path) != toLower(possibleDescendent.m_path.substr(0, len)))
    return false;
  // se l'antenato non finisce con slash ci deve essere uno slash nel
  // discendente, subito dopo
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
  QString countLetter = (m_letterCountForSuffix == 0)
                            ? "{0,}"
                            : (QString("{0,%1}").arg(m_letterCountForSuffix));
  return QString("(\\d+)(%1%2)").arg(suffixLetter).arg(countLetter);
  // const QString fIdRegExp("(\\d+)([a-zA-Z]?)");
}

//-----------------------------------------------------------------------------

TFilePath::TFilePathInfo TFilePath::analyzePath() const {
  assert(!TFilePath::m_useStandard);

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

  // Extension：letters other than "._" or  \/:,;*?"<>|  or " "(space)
  const QString extensionRegExp("([^\\._ \\\\/:,;*?\"<>|]+)");

  // ignore frame numbers on non-sequential (i.e. movie) extension case :
  // hoge_0001.mp4
  // QRegExp rx_mf("^" + levelNameRegExp + "\\." + extensionRegExp + "$");
  // if (rx_mf.indexIn(levelName) != -1) {
  //  QString ext = rx_mf.cap(2);
  //  if (!checkForSeqNum(ext)) {
  //    info.levelName = rx_mf.cap(1);
  //    info.sepChar = QChar();
  //    info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0); //NO_PADで初期化する
  //    info.extension = ext;
  //    return info;
  //  }
  //}

  // hogehoge.0001a.jpg
  // empty frame case : hogehoge..jpg
  QRegExp rx("^" + levelNameRegExp + sepCharRegExp + "(?:" + fIdRegExp + ")?" +
             "\\." + extensionRegExp + "$");
  if (rx.indexIn(fileName) != -1) {
    assert(rx.captureCount() == 5);
    info.levelName = rx.cap(1);
    info.sepChar   = rx.cap(2)[0];
    info.extension = rx.cap(5);
    // ignore frame numbers on non-sequential (i.e. movie) extension case :
    // hoge_0001.mp4
    if (!checkForSeqNum(info.extension)) {
      info.levelName = rx.cap(1) + rx.cap(2);
      if (!rx.cap(3).isEmpty()) info.levelName += rx.cap(3);
      if (!rx.cap(4).isEmpty()) info.levelName += rx.cap(4);
      info.sepChar = QChar();
      info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0);  // initialize with NO_PAD
    } else {
      QString numberStr = rx.cap(3);
      if (numberStr.isEmpty())  // empty frame case : hogehoge..jpg
        info.fId =
            TFrameId(TFrameId::EMPTY_FRAME, 0, 4, info.sepChar.toLatin1());
      else {
        int number  = numberStr.toInt();
        int padding = 0;
        if (numberStr[0] == "0")  // with padding
          padding = numberStr.count();
        QString suffix;
        if (!rx.cap(4).isEmpty()) suffix = rx.cap(4);
        info.fId = TFrameId(number, suffix, padding, info.sepChar.toLatin1());
      }
    }
    return info;
  }

  // QRegExp rx_ef("^" + levelNameRegExp + sepCharRegExp + "\\." +
  // extensionRegExp + "$"); if (rx_ef.indexIn(levelName) != -1) {
  //  info.levelName = rx_ef.cap(1);
  //  info.sepChar = rx_ef.cap(2)[0];
  //  info.fId = TFrameId(TFrameId::EMPTY_FRAME, 0, 4, info.sepChar.toLatin1());
  //  info.extension = rx_ef.cap(3);
  //  return info;
  //}

  // no frame case : hogehoge.jpg
  // no level name case : .jpg
  QRegExp rx_nf("^(?:" + levelNameRegExp + ")?\\." + extensionRegExp + "$");
  if (rx_nf.indexIn(fileName) != -1) {
    if (!rx_nf.cap(1).isEmpty()) info.levelName = rx_nf.cap(1);
    info.sepChar = QChar();
    info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0);  // initialize with NO_PAD
    info.extension = rx_nf.cap(2);
    return info;
  }

  // no periods
  info.levelName = fileName;
  info.sepChar   = QChar();
  info.fId = TFrameId(TFrameId::NO_FRAME, 0, 0);  // initialize with NO_PAD
  info.extension = QString();
  return info;
}