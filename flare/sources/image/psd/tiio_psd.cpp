#include "tiio_psd.h"
#include "trasterimage.h"
#include "timageinfo.h"
#if (defined(x64) || defined(__LP64__))
#include "toonz/preferences.h"
#include <QtCore>
#endif

// forward declaration
// class TImageReaderLayerPsd;

// if path = :
//  filename.psd                to load float image. TLevel frames number is 1
//  filename#layerId.psd        to load only one psd layer from layerId. TLevel
//  frames number is 1
//  filename#layerId#frames.psd to load only one psd layer from layerId. TLevel
//  frames number is psd layers number
//  filename#layerId#group.psd  to load only one psd layer from layerId. TLevel
//  frames number is psd layers folder number
//                              where fodler is the folder belongs psd layer.

TLevelReaderPsd::TLevelReaderPsd(const TFilePath &path)
    : TLevelReader(path), m_path(path), m_layerId(0) {
  m_psdreader           = new TPSDReader(m_path);
  TPSDHeaderInfo header = m_psdreader->getPSDHeaderInfo();
  m_lx                  = header.cols;
  m_ly                  = header.rows;
  m_layersCount         = header.layersCount;
  // set m_info
  m_info                   = new TImageInfo();
  m_info->m_lx             = m_lx;
  m_info->m_ly             = m_ly;
  m_info->m_dpix           = header.hres;
  m_info->m_dpiy           = header.vres;
  m_info->m_bitsPerSample  = header.depth;
  m_info->m_samplePerPixel = header.channels;

  QString name     = m_path.getName().c_str();
  QStringList list = name.split("#");
  if (list.size() >= 2) {
    // There's a layer id, otherwise the level is loaded in FLAT mode
    const QString &layerStr = list.at(1);

#ifdef REF_LAYER_BY_NAME
#if (defined(x64) || defined(__LP64__))
    if (layerStr != "frames") {
      QTextCodec *layerNameCodec = QTextCodec::codecForName(
          Preferences::instance()->getLayerNameEncoding().c_str());
      TPSDParser psdparser(m_path);
      m_layerId = psdparser.getLevelIdByName(
          layerNameCodec->fromUnicode(layerStr).toStdString());
    } else {
      m_layerId = layerStr.toInt();
    }
#endif
#else
    m_layerId = layerName.toInt();
#endif
  }
}

//------------------------------------------------

TLevelReaderPsd::~TLevelReaderPsd() { delete m_psdreader; }

//------------------------------------------------

void TLevelReaderPsd::load(TRasterImageP &rasP, int shrinkX, int shrinkY,
                           TRect region) {
  QMutexLocker sl(&m_mutex);
  TRasterImageP img;
  m_psdreader->setShrink(shrinkX, shrinkY);
  m_psdreader->setRegion(region);
  m_psdreader->load(img, m_layerId);  // if layerId==0 load Merged
  rasP = img;
}

TLevelP TLevelReaderPsd::loadInfo() {
  TPSDParser *psdparser = new TPSDParser(m_path);
  assert(m_layerId >= 0);
  int framesCount = psdparser->getFramesCount(m_layerId);
  TLevelP level;
  level->setName(psdparser->getLevelName(m_layerId));
  m_frameTable.clear();
  for (int i = 0; i < framesCount; i++) {
    TFrameId frame(i + 1);
    m_frameTable.insert(
        std::make_pair(frame, psdparser->getFrameId(m_layerId, i)));
    level->setFrame(frame, TImageP());
  }
  return level;
}

//------------------------------------------------

TLevelReader *TLevelReaderPsd::create(const TFilePath &f) {
  TLevelReaderPsd *reader = new TLevelReaderPsd(f);
  return reader;
}

//-----------------------------------------------------------
//  TImageReaderLayerPsd
//-----------------------------------------------------------

class TImageReaderLayerPsd final : public TImageReader {
public:
  TImageReaderLayerPsd(const TFilePath &, int layerId, TLevelReaderPsd *lr,
                       TImageInfo *info);
  ~TImageReaderLayerPsd() { m_lr->release(); }

private:
  // not implemented
  TImageReaderLayerPsd(const TImageReaderLayerPsd &);
  TImageReaderLayerPsd &operator=(const TImageReaderLayerPsd &src);

public:
  TImageP load() override;

  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TPSDReader *m_psdreader;
  TLevelReaderPsd *m_lr;
  TImageInfo *m_info;
  int m_layerId;
};

TImageReaderLayerPsd::TImageReaderLayerPsd(const TFilePath &path, int layerId,
                                           TLevelReaderPsd *lr,
                                           TImageInfo *info)
    : TImageReader(path), m_lr(lr), m_layerId(layerId), m_info(info) {
  m_lr->setLayerId(layerId);
  m_lr->addRef();
}

TImageP TImageReaderLayerPsd::load() {
  TRasterImageP rasP;
  m_lr->load(rasP, m_shrink, m_shrink, m_region);
  return TRasterImageP(rasP);
}

TImageReaderP TLevelReaderPsd::getFrameReader(TFrameId fid) {
  int layerId = m_frameTable[fid];
  TImageReaderLayerPsd *ir =
      new TImageReaderLayerPsd(m_path, layerId, this, m_info);
  return TImageReaderP(ir);
}

//------------------------------------------------------------------------------
//  TImageWriterPsd
//------------------------------------------------------------------------------

class TImageWriterPsd final : public TImageWriter {
  int m_layerId;

public:
  TImageWriterPsd(const TFilePath &, int frameIndex, TLevelWriterPsd *);
  ~TImageWriterPsd() { m_lwm->release(); }

private:
  // not implemented
  TImageWriterPsd(const TImageWriterPsd &);
  TImageWriterPsd &operator=(const TImageWriterPsd &src);

public:
  // not implemented
  void save(const TImageP &) override;

private:
  TLevelWriterPsd *m_lwm;
};

//-----------------------------------------------------------
//-----------------------------------------------------------
//        TImageWriterPsd
//-----------------------------------------------------------

TImageWriterPsd::TImageWriterPsd(const TFilePath &path, int layerId,
                                 TLevelWriterPsd *lwm)
    : TImageWriter(path), m_lwm(lwm), m_layerId(layerId) {
  m_lwm->addRef();
}

//-----------------------------------------------------------
void TImageWriterPsd::save(const TImageP &img) { m_lwm->save(img, m_layerId); }

//-----------------------------------------------------------

void TLevelWriterPsd::save(const TImageP &img, int frameIndex) {}

//-----------------------------------------------------------
//        TLevelWriterPsd
//-----------------------------------------------------------

TLevelWriterPsd::TLevelWriterPsd(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {}

//-----------------------------------------------------------

TLevelWriterPsd::~TLevelWriterPsd() {}

//-----------------------------------------------------------

TImageWriterP TLevelWriterPsd::getFrameWriter(TFrameId fid) {
  int layerId          = 0;
  TImageWriterPsd *iwm = new TImageWriterPsd(m_path, layerId, this);
  return TImageWriterP(iwm);
}
