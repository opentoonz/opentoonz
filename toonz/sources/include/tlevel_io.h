#pragma once

#ifndef TLEVEL_IO_INCLUDED
#define TLEVEL_IO_INCLUDED

#include "tlevel.h"
#include "timage_io.h"
#include "tproperty.h"

#ifdef _MSC_VER

#pragma warning(disable : 4290)
#pragma warning(disable : 4251)

#include <typeinfo>
namespace std {
using ::type_info;
}
#endif

#undef DVAPI
#undef DVVAR
#ifdef TIMAGE_IO_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================
// forward declaration
class TSoundTrack;
class TImageInfo;
class TContentHistory;

//---------------------------------------------------------

class TLevelReader;
typedef TLevelReader *TLevelReaderCreateProc(const TFilePath &path);

//---------------------------------------------------------

class DVAPI TLevelReader : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  TImageInfo *m_info;
  TFilePath m_path;
  TContentHistory *m_contentHistory;

public:
  TLevelReader(const TFilePath &path);
  virtual ~TLevelReader();

private:
  // not implemented
  TLevelReader(const TLevelReader &);
  TLevelReader &operator=(const TLevelReader &src);

public:
  virtual TLevelP loadInfo();
  virtual QString getCreator() { return ""; }

  virtual void doReadPalette(bool) {}
  virtual void enableRandomAccessRead(bool) {}
  virtual TImageReaderP getFrameReader(TFrameId);

  // TLevelReader keeps ownership: DO NOT DELETE
  virtual const TImageInfo *getImageInfo(TFrameId);
  virtual const TImageInfo *getImageInfo();

  TImageReaderP getFrameReader(int f) { return getFrameReader(TFrameId(f)); };

  virtual TSoundTrack *loadSoundTrack();

  TFilePath getFilePath() const { return m_path; }

  static void getSupportedFormats(QStringList &names);

  enum FormatType { UnsupportedFormat, RasterLevel, VectorLevel };

  static FormatType getFormatType(std::string extension);

  static void define(
      QString extension, int reader,
      // nel caso in cui ci siano piu' lettori per lo stesso formato
      // (es. flash)

      TLevelReaderCreateProc *proc);

  static inline void define(QString extension, TLevelReaderCreateProc *proc) {
    define(extension, 0, proc);
  }

  //! TLevelReader keeps the ownership of TContentHistory. Don't delete it
  const TContentHistory *getContentHistory() const { return m_contentHistory; }

private:
  TFrameId::FrameFormat m_frameFormat;
};

//-----------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TLevelReader>;
#endif

class DVAPI TLevelReaderP final : public TSmartPointerT<TLevelReader> {
public:
  // il costruttore "non banale"
  TLevelReaderP(const TFilePath &filepath, int reader = 0);
  // il costruttore di default
  TLevelReaderP() {}
};

//===========================================================

class TLevelWriter;
class TPropertyGroup;

typedef TLevelWriter *TLevelWriterCreateProc(const TFilePath &path,
                                             TPropertyGroup *);

typedef TPropertyGroup *TWriterInfoCreateProc(const std::string &ext);

//-----------------------------------------------------------

class DVAPI TLevelWriter : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  TFilePath m_path;
  double m_frameRate;
  TPropertyGroup *m_properties;
  TContentHistory *m_contentHistory;
  QString m_creator;

public:
  TLevelWriter(const TFilePath &path,
               TPropertyGroup *winfo = 0);  // ottiene l'ownership
  virtual ~TLevelWriter();

  void setCreator(const QString &creator) { m_creator = creator; }

  virtual void setPalette(TPalette *){};

  virtual void setOverwritePaletteFlag(bool overwrite){};

  //! Maps a list of existing level frames to a new list of frames.
  /*!
This function allows an existing level to reorganize (or discard)
its \a disk content. It is typically implemented by TLevelWriters
that support <I> random access writing <\I>, which can therefore
write new frames on top of an existing level.
\n\n
This function requires that frames present on disk \a and in the
table be remapped, while frames on disk \a not in the table be
\b deleted. Eventual frames present in the table but not on disk
are ignored.
\n\n
The default implementation provides renumbering for standard
Toonz multi-file level types (levelName.####c.ext), and does nothing
if the specified path is not compatible with the multi-file format.
*/
  virtual void renumberFids(const std::map<TFrameId, TFrameId> &table);

  // Usate per le TLV
  // Introdotte per le TLV (versione 13) che salvano l'iconcina
  // nel file stesso.
  virtual void setIconSize(TDimension){};
  virtual TDimension getIconSize() const { return TDimension(); };

private:
  // not implemented
  TLevelWriter(const TLevelWriter &);
  TLevelWriter &operator=(const TLevelWriter &src);

public:
  virtual TImageWriterP getFrameWriter(TFrameId);

  TImageWriterP getFrameWriter(int f) { return getFrameWriter(TFrameId(f)); }

  TPropertyGroup *getProperties() { return m_properties; }

  void save(const TLevelP &level);
  virtual void saveSoundTrack(TSoundTrack *st);

  virtual void setFrameRate(double fps);

  TFilePath getFilePath() const { return m_path; }

  static void getSupportedFormats(QStringList &names, bool onlyRenderFormats);

  // note gets the contentHistory. can be 0
  const TContentHistory *getContentHistory() const { return m_contentHistory; }
  TContentHistory *getContentHistory() { return m_contentHistory; }

  // destroys the old contentHistory and replaces it with the new one. Gets
  // ownership
  // note. set the content history first
  void setContentHistory(TContentHistory *contentHistory);

  static void define(QString extension, TLevelWriterCreateProc *proc,
                     bool isRenderFormat);
};

//-----------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TLevelWriter>;
#endif

class DVAPI TLevelWriterP final : public TSmartPointerT<TLevelWriter> {
public:
  // il costruttore "non banale"
  TLevelWriterP(const TFilePath &filepath,
                TPropertyGroup *winfo =
                    0);  // non si prende l'ownership del TPropertyGroup
  // il costruttore di default
  TLevelWriterP() {}
};

//==============================================================================

//  Some useful utility inlines

inline bool isMovieType(std::string type) {
  return (type == "mov" || type == "avi" || type == "3gp" || type == "webm" ||
          type == "mp4");
}

//-----------------------------------------------------------

inline bool isMovieType(const TFilePath &fp) {
  std::string type(fp.getType());
  return isMovieType(type);
}

//-----------------------------------------------------------

inline bool doesSupportRandomAccess(const TFilePath &fp,
                                    bool isToonzOutput = false) {
  return (fp.getDots() == "..") || (isToonzOutput && fp.getType() == "mov");
}

#endif
