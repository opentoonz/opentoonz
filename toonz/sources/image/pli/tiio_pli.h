#pragma once

#ifndef TTIO_PLI_INCLUDED
#define TTIO_PLI_INCLUDED

#include <memory>

#include "tlevel_io.h"

class GroupTag;
class ParsedPli;
class ImageTag;
class TImageWriterPli;
class TImageReaderPli;

//===========================================================================
/*
class TWriterInfoPli final : public TWriterInfo {

public:
 ~TWriterInfoPli() {}
  static TWriterInfo *create(const std::string &ext) { return new
TWriterInfoPli(); }
  TWriterInfo *clone() const {
  return new TWriterInfoPli(*this); }

private:
  TWriterInfoPli() {}
  TWriterInfoPli(const TWriterInfoPli&) {}
};
*/
//===========================================================================

/*!
  TLevelWriterPli:
 */
class TLevelWriterPli final : public TLevelWriter {
  //! object to manage a pli
  std::unique_ptr<ParsedPli> m_pli;

  //! number of frame in pli
  UINT m_frameNumber;

  //  vettore da utilizzare per il calcolo della palette
  std::vector<TPixel> m_colorArray;

public:
  TLevelWriterPli(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterPli();
  TImageWriterP getFrameWriter(TFrameId fid) override;

  friend class TImageWriterPli;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterPli(f, winfo);
  }

private:
  // not implemented
  TLevelWriterPli(const TLevelWriterPli &);
  TLevelWriterPli &operator=(const TLevelWriterPli &);
};

//===========================================================================

typedef std::pair<ImageTag *, bool> pliFrameInfo;

/*!
  TLevelReaderPli:
 */
class TLevelReaderPli final : public TLevelReader {
public:
  TLevelReaderPli(const TFilePath &path);
  ~TLevelReaderPli();

  /*!
Return info about current pli
*/
  TLevelP loadInfo() override;
  void doReadPalette(bool doReadIt) override;

  /*!
Return an image with Reader information
*/
  TImageReaderP getFrameReader(TFrameId fid) override;

  QString getCreator() override;
  friend class TImageReaderPli;

private:
  bool m_init;
  //! struct which contanins reference to frame
  std::map<TFrameId, pliFrameInfo> m_mapOfImage;

  //! Reference to pli palette
  TPixel *m_palette;
  bool m_readPalette;
  //!
  TUINT32 m_paletteCount;

  //! flag to check if file exists
  bool m_doesExist;

  //! object to manage a pli
  ParsedPli *m_pli;
  TLevelP m_level;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderPli(f);
  }

private:
  // not implemented
  TLevelReaderPli(const TLevelReaderPli &);
  TLevelReaderPli &operator=(const TLevelReaderPli &);
};

//===========================================================================
/*
Classe locale per la lettura di un frame del livello.
*/
class TImageReaderPli final : public TImageReader {
public:
  TFrameId m_frameId;  //<! Current frame id

private:
  // not implemented
  TImageReaderPli(const TImageReaderPli &);
  TImageReaderPli &operator=(const TImageReaderPli &src);

public:
  TImageReaderPli(const TFilePath &f, const TFrameId &frameId,
                  TLevelReaderPli *);
  ~TImageReaderPli() {}

  TImageP load() override;
  TImageP doLoad();

  TDimension getSize() const;

  TRect getBBox() const;

private:
  //! Size of image
  int m_lx, m_ly;

  //! Reference to level reader
  TLevelReaderPli *m_lrp;
};

// Functions

TPalette *readPalette(GroupTag *paletteTag, int majorVersion, int minorVersion);

#endif  // TTIO_PLI_INCLUDED
