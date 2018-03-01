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

class QString;

//-----------------------------------------------------------------------------
/*
  This is an example of how to use TFilePath and TFrameId classes.
*/

//!\include frameid_ex.cpp
//! The class TFrameId describes a frame identified by an integer number of four
//! figures and, in case, by a character (necessary for added frames)
class DVAPI TFrameId {
  int m_frame;
  char m_letter;  // serve per i frame "aggiunti" del tipo pippo.0001a.tzp =>
                  // f=1 c='a'
  int m_zeroPadding;
  char m_startSeqInd;

public:
  enum {
    EMPTY_FRAME = -1,  // es. pippo..tif
    NO_FRAME    = -2   // es. pippo.tif
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
      : m_frame(f), m_letter(0), m_zeroPadding(4), m_startSeqInd('.') {}
  TFrameId(int f, char c)
      : m_frame(f), m_letter(c), m_zeroPadding(4), m_startSeqInd('.') {}
  TFrameId(int f, char c, int p)
      : m_frame(f), m_letter(c), m_zeroPadding(p), m_startSeqInd('.') {}
  TFrameId(int f, char c, int p, char s)
      : m_frame(f), m_letter(c), m_zeroPadding(p), m_startSeqInd(s) {}

  inline bool operator==(const TFrameId &f) const {
    return f.m_frame == m_frame && f.m_letter == m_letter;
  }
  inline bool operator!=(const TFrameId &f) const {
    return (m_frame != f.m_frame || m_letter != f.m_letter);
  }
  inline bool operator<(const TFrameId &f) const {
    return (m_frame < f.m_frame ||
            (m_frame == f.m_frame && m_letter < f.m_letter));
  }
  inline bool operator>(const TFrameId &f) const { return f < *this; }
  inline bool operator>=(const TFrameId &f) const { return !operator<(f); }
  inline bool operator<=(const TFrameId &f) const { return !operator>(f); }

  const TFrameId &operator++();
  const TFrameId &operator--();

  TFrameId &operator=(const TFrameId &f) {
    m_frame  = f.m_frame;
    m_letter = f.m_letter;
    return *this;
  }

  bool isEmptyFrame() const { return m_frame == EMPTY_FRAME; }
  bool isNoFrame() const { return m_frame == NO_FRAME; }

  // operator string() const;
  std::string expand(FrameFormat format = FOUR_ZEROS) const;
  int getNumber() const { return m_frame; }
  char getLetter() const { return m_letter; }

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

/*!The class TFilePath defines a file path.Its constructor creates a string
   according
   to the platform and to certain rules.For more information see the
   constructor.*/
class DVAPI TFilePath {
  static bool m_underscoreFormatAllowed;
  std::wstring m_path;
  void setPath(std::wstring path);

public:
  /*! this static method allows correct reading of levels in the form
   * pippo_0001.tif (represented  always as pippo..tif) */
  static void setUnderscoreFormatAllowed(bool state) {
    m_underscoreFormatAllowed = state;
  }

  /*!This constructor creates a string removing redundances ('//', './',etc.)
and final slashes,
      correcting (redressing) the "twisted" slashes.
      Note that if the current directory is ".", it becomes "".
If the path is "<alpha>:" a slash will be added*/

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
   a.  "" if there is no filename extension
   b. "." if there is a filename extension but no frame
   c.".." if there are both filename extension and frame */
  std::string getDots() const;  // ritorna ""(no estensione), "."(estensione, no
                                // frame) o ".."(estensione e frame)

  /*!Returns the filename extension, including leading period (.).
      Returns "" if there is no filename extension.*/
  std::string getDottedType() const;  // ritorna l'estensione con il PUNTO (se
                                      // non c'e' estensione ritorna "")

  /*!Returns the filename extension, escluding leading period (.).
      Returns "" if there is no filename extension.*/
  std::string getUndottedType() const;  // ritorna l'estensione senza PUNTO

  /*!It is equal to getUndottedType():
      Returns the filename extension, excluding leading period (.).
      Returns "" if there is no filename extension.*/
  std::string getType() const {
    return getUndottedType();
  }  // ritorna l'estensione SENZA PUNTO
  /*!Returns the base filename (no extension, no dots, no slash)*/
  std::string getName() const;       // noDot! noSlash!
  std::wstring getWideName() const;  // noDot! noSlash!

  /*!Returns the filename (with extension, escluding in case the frame number).
      ex.: TFilePath("/pippo/pluto.0001.gif").getLevelName() == "pluto..gif"
   */
  std::string getLevelName()
      const;  // es. TFilePath("/pippo/pluto.0001.gif").getLevelName() ==
              // "pluto..gif"
  std::wstring getLevelNameW()
      const;  // es. TFilePath("/pippo/pluto.0001.gif").getLevelName() ==
              // "pluto..gif"

  /*!Returns the parent directory escluding the eventual final slash.*/
  TFilePath getParentDir() const;  // noSlash!;

  TFrameId getFrame() const;
  bool isFfmpegType() const;
  bool isLevelName()
      const;  //{return getFrame() == TFrameId(TFrameId::EMPTY_FRAME);};
  bool isAbsolute() const;
  bool isRoot() const;
  bool isEmpty() const { return m_path == L""; }

  /*!Return a TFilePath with extension type.
type is a string that indicate the filename extension(ex:. bmp or .bmp)*/
  TFilePath withType(const std::string &type) const;
  /*!Return a TFilePath with filename "name".*/
  TFilePath withName(const std::string &name) const;
  /*!Return a TFilePath with filename "name". Unicode*/
  TFilePath withName(const std::wstring &name) const;
  /*!Return a TFilePath with parent directory "dir".*/
  TFilePath withParentDir(const TFilePath &dir) const;
  /*!Return a TFilePath without parent directory */
  TFilePath withoutParentDir() const { return withParentDir(TFilePath()); }
  /*!Return a TFilePath with frame "frame".*/
  TFilePath withFrame(
      const TFrameId &frame,
      TFrameId::FrameFormat format = TFrameId::USE_CURRENT_FORMAT) const;
  /*!Return a TFilePath with a frame identified by an integer number "f".*/
  TFilePath withFrame(int f) const { return withFrame(TFrameId(f)); }
  /*!Return a TFilePath with a frame identified by an integer and by a
   * character*/
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
      const TFilePath &fp) const;  // TFilePath("/a/b/c.txt")-TFilePath("/a/")
                                   // == TFilePath("b/c.txt");
  // se fp.isAncestorOf(this) == false ritorna this

  bool match(const TFilePath &fp)
      const;  // sono uguali a meno del numero di digits del frame

  // '/a/b/c.txt' => head='a' tail='b/c.txt'
  void split(std::wstring &head, TFilePath &tail) const;
};

//-----------------------------------------------------------------------------

class TMalformedFrameException final : public TException {
public:
  TMalformedFrameException(const TFilePath &fp,
                           const std::wstring &msg = std::wstring())
      : TException(fp.getWideName() + L":" + msg) {}

private:
  TMalformedFrameException();
};

//-----------------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TFilePath &path);

typedef std::list<TFilePath> TFilePathSet;

#endif  // T_FILEPATH_INCLUDED
