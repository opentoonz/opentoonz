#pragma once

#ifndef T_FILEPATH_INCLUDED
#define T_FILEPATH_INCLUDED

#include "tcommon.h"
#include "texception.h"

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QString>

//-----------------------------------------------------------------------------
/*
  Example of using TFilePath and TFrameId classes.
*/

//!\include frameid_ex.cpp
//! The TFrameId class describes a frame identified by a four-digit integer and
//! optionally by a character (necessary for appended frames)
class DVAPI TFrameId {
  int m_frame;
  QString
      m_letter;  // Used for "appended" frames like pippo.0001a.tzp => f=1 c='a'
  int m_zeroPadding;
  char m_startSeqInd;

public:
  enum {
    EMPTY_FRAME = -1,  // e.g., pippo..tif
    NO_FRAME    = -2   // e.g., pippo.tif
  };

  enum FrameFormat {
    FOUR_ZEROS,
    NO_PAD,
    UNDERSCORE_FOUR_ZEROS,  // pippo_0001.tif
    UNDERSCORE_NO_PAD,
    CUSTOM_PAD,
    UNDERSCORE_CUSTOM_PAD,
    USE_CURRENT_FORMAT
  };  // pippo_1.tif

  TFrameId(int f = EMPTY_FRAME)
      : m_frame(f), m_letter(""), m_zeroPadding(4), m_startSeqInd('.') {}
  TFrameId(int f, char c, int p = 4, char s = '.')
      : m_frame(f), m_zeroPadding(p), m_startSeqInd(s) {
    m_letter = (c == '\0') ? QString() : QString(1, QChar(c));
  }
  TFrameId(int f, QString str, int p = 4, char s = '.')
      : m_frame(f), m_letter(str), m_zeroPadding(p), m_startSeqInd(s) {}
  explicit TFrameId(const std::string &str, char s = '.');
  explicit TFrameId(const std::wstring &wstr, char s = '.');

  inline bool operator==(const TFrameId &f) const {
    return f.m_frame == m_frame && f.m_letter == m_letter;
  }
  inline bool operator!=(const TFrameId &f) const {
    return (m_frame != f.m_frame || m_letter != f.m_letter);
  }
  inline bool operator<(const TFrameId &f) const {
    return (m_frame < f.m_frame ||
            (m_frame == f.m_frame &&
             QString::localeAwareCompare(m_letter, f.m_letter) < 0));
  }
  inline bool operator>(const TFrameId &f) const { return f < *this; }
  inline bool operator>=(const TFrameId &f) const { return !operator<(f); }
  inline bool operator<=(const TFrameId &f) const { return !operator>(f); }

  const TFrameId &operator++();
  const TFrameId &operator--();

  TFrameId &operator=(const TFrameId &f) {
    m_frame       = f.m_frame;
    m_letter      = f.m_letter;
    m_zeroPadding = f.m_zeroPadding;
    m_startSeqInd = f.m_startSeqInd;
    return *this;
  }

  bool isEmptyFrame() const { return m_frame == EMPTY_FRAME; }
  bool isNoFrame() const { return m_frame == NO_FRAME; }

  // operator string() const;
  std::string expand(FrameFormat format = FOUR_ZEROS) const;
  int getNumber() const { return m_frame; }
  QString getLetter() const { return m_letter; }

  void setZeroPadding(int p) { m_zeroPadding = p; }
  int getZeroPadding() const { return m_zeroPadding; }

  void setStartSeqInd(char c) { m_startSeqInd = c; }
  char getStartSeqInd() const { return m_startSeqInd; }

  FrameFormat getCurrentFormat() const {
    switch (m_zeroPadding) {
    case 0:
      return (m_startSeqInd == '.' ? NO_PAD : UNDERSCORE_NO_PAD);
    case 4:
      return (m_startSeqInd == '.' ? FOUR_ZEROS : UNDERSCORE_FOUR_ZEROS);
    default:
      break;
    }

    return (m_startSeqInd == '.' ? CUSTOM_PAD : UNDERSCORE_CUSTOM_PAD);
  }
};

//-----------------------------------------------------------------------------
/*! \relates TFrameId*/

inline std::ostream &operator<<(std::ostream &out, const TFrameId &f) {
  if (f.isNoFrame())
    out << "<noframe>";
  else if (f.isEmptyFrame())
    out << "''";
  else
    out << f.expand().c_str();
  return out;
}

//-----------------------------------------------------------------------------

/*!The TFilePath class defines a file path. Its constructor creates a string
   according to the platform and specific rules. For more information see the
   constructor.*/
class DVAPI TFilePath {
  static bool m_underscoreFormatAllowed;

  // Specifies file path condition for sequential images per project.
  // See filepathproperties.h
  static bool m_useStandard;
  static bool m_acceptNonAlphabetSuffix;
  static int m_letterCountForSuffix;

  std::wstring m_path;

  struct TFilePathInfo {
    QString parentDir;  // with slash
    QString levelName;
    QChar sepChar;  // either "." or "_"
    TFrameId fId;
    QString extension;
  };

  void setPath(std::wstring path);

public:
  /*! This static method allows correct reading of levels in the form
   * pippo_0001.tif (always represented as pippo..tif) */
  static void setUnderscoreFormatAllowed(bool state) {
    m_underscoreFormatAllowed = state;
  }

  // Called from TProjectManager::getCurrentProject() and
  // ProjectPopup::updateProjectFromFields Returns true if something changed
  static bool setFilePathProperties(bool useStandard, bool acceptNonAlphaSuffix,
                                    int letterCountForSuffix) {
    if (m_useStandard == useStandard &&
        m_acceptNonAlphabetSuffix == acceptNonAlphaSuffix &&
        m_letterCountForSuffix == letterCountForSuffix)
      return false;
    m_useStandard             = useStandard;
    m_acceptNonAlphabetSuffix = acceptNonAlphaSuffix;
    m_letterCountForSuffix    = letterCountForSuffix;
    return true;
  }
  static bool useStandard() { return m_useStandard; }
  static QString fidRegExpStr();

  /*!This constructor creates a string by removing redundancies ('//', './',
     etc.) and trailing slashes, correcting (redressing) "twisted" slashes.
      Note: if the current directory is ".", it becomes "".
      If the path is "<alpha>:", a slash will be added*/

  explicit TFilePath(const char *path = "");
  explicit TFilePath(const std::string &path);
  explicit TFilePath(const std::wstring &path);
  explicit TFilePath(const QString &path);

  ~TFilePath() {}
  TFilePath(const TFilePath &fp) : m_path(fp.m_path) {}
  TFilePath &operator=(const TFilePath &fp) {
    m_path = fp.m_path;
    return *this;
  }

  bool operator==(const TFilePath &fp) const;
  inline bool operator!=(const TFilePath &fp) const {
    return !(m_path == fp.m_path);
  }
  bool operator<(const TFilePath &fp) const;

  const std::wstring getWideString() const;
  QString getQString() const;

  /*!Returns:
   a.  "" if there is no file extension
   b. "." if there is a file extension but no frame
   c.".." if there are both a file extension and a frame */
  std::string getDots() const;  // Returns "" (no extension), "." (extension, no
                                // frame) or ".." (extension and frame)

  /*!Returns the file extension, including the leading period (.).
      Returns "" if there is no file extension.*/
  std::string getDottedType()
      const;  // Returns the extension WITH the DOT (if no extension returns "")

  /*!Returns the file extension, excluding the leading period (.).
      Returns "" if there is no file extension.*/
  std::string getUndottedType() const;  // Returns the extension WITHOUT the DOT

  /*!Same as getUndottedType():
      Returns the file extension, excluding the leading period (.).
      Returns "" if there is no file extension.*/
  std::string getType() const {
    return getUndottedType();
  }  // Returns the extension WITHOUT the DOT

  /*!Returns the base filename (no extension, no dots, no slash)*/
  std::string getName() const;       // noDot! noSlash!
  std::wstring getWideName() const;  // noDot! noSlash!

  /*!Returns the filename (with extension, excluding the frame number if
     present). ex.: TFilePath("/pippo/pluto.0001.gif").getLevelName() ==
     "pluto..gif"
   */
  std::string getLevelName()
      const;  // e.g., TFilePath("/pippo/pluto.0001.gif").getLevelName() ==
              // "pluto..gif"
  std::wstring getLevelNameW()
      const;  // e.g., TFilePath("/pippo/pluto.0001.gif").getLevelName() ==
              // "pluto..gif"

  /*!Returns the parent directory excluding any trailing slash.*/
  TFilePath getParentDir() const;  // noSlash!;

  TFrameId getFrame() const;
  bool isFfmpegType() const;
  bool isUneditable() const;
  bool isLevelName()
      const;  //{return getFrame() == TFrameId(TFrameId::EMPTY_FRAME);};
  bool isAbsolute() const;
  bool isRoot() const;
  bool isEmpty() const { return m_path == L""; }

  /*!Returns a TFilePath with the specified extension type.
type is a string indicating the file extension (e.g., .bmp or bmp)*/
  TFilePath withType(const std::string &type) const;
  /*!Returns a TFilePath with filename "name".*/
  TFilePath withName(const std::string &name) const;
  /*!Returns a TFilePath with filename "name". Unicode*/
  TFilePath withName(const std::wstring &name) const;
  /*!Returns a TFilePath with parent directory "dir".*/
  TFilePath withParentDir(const TFilePath &dir) const;
  /*!Returns a TFilePath without a parent directory */
  TFilePath withoutParentDir() const { return withParentDir(TFilePath()); }
  /*!Returns a TFilePath with frame "frame".*/
  TFilePath withFrame(
      const TFrameId &frame,
      TFrameId::FrameFormat format = TFrameId::USE_CURRENT_FORMAT) const;
  /*!Returns a TFilePath with a frame identified by an integer "f".*/
  TFilePath withFrame(int f) const { return withFrame(TFrameId(f)); }
  /*!Returns a TFilePath with a frame identified by an integer and a character*/
  TFilePath withFrame(int f, char letter) const {
    return withFrame(TFrameId(f, letter));
  }

  TFilePath withFrame() const {
    return withFrame(TFrameId(TFrameId::EMPTY_FRAME));
  }
  TFilePath withNoFrame() const {
    return withFrame(TFrameId(TFrameId::NO_FRAME));
  }  // pippo.tif

  TFilePath operator+(const TFilePath &fp) const;
  TFilePath &operator+=(const TFilePath &fp) /*{*this=*this+fp;return *this;}*/;

  inline TFilePath operator+(const std::string &s) const {
    return operator+(TFilePath(s));
  }
  inline TFilePath &operator+=(const std::string &s) {
    return operator+=(TFilePath(s));
  }

  TFilePath &operator+=(const std::wstring &s);
  TFilePath operator+(const std::wstring &s) const {
    TFilePath res(*this);
    return res += s;
  }

  bool isAncestorOf(const TFilePath &) const;

  TFilePath operator-(
      const TFilePath &fp) const;  // TFilePath("/a/b/c.txt") - TFilePath("/a/")
                                   // == TFilePath("b/c.txt");
  // If fp.isAncestorOf(this) == false, returns this

  bool match(const TFilePath &fp)
      const;  // They are equal except for the number of frame digits

  // '/a/b/c.txt' => head='a' tail='b/c.txt'
  void split(std::wstring &head, TFilePath &tail) const;

  TFilePathInfo analyzePath() const;

  QChar getSepChar() const;
};

//-----------------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TFilePath &path);

typedef std::list<TFilePath> TFilePathSet;

#endif  // T_FILEPATH_INCLUDED
