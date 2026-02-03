

#include "toonzqt/infoviewer.h"
#include "toonzqt/intfield.h"
#include "tsystem.h"
#include "tlevel.h"
#include "tpalette.h"
#include "tlevel_io.h"
#include "tsound_io.h"
#include "tiio.h"
#include "tstream.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "toonz/toonzscene.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toutputproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/levelset.h"
#include "tcontenthistory.h"
#include "tfiletype.h"
#include <QSlider>
#include <QLabel>
#include <QTextEdit>
#include <QDateTime>

using namespace DVGui;

//----------------------------------------------------------------

class InfoViewerImp {
public:
  enum {
    eFullpath = 0,
    eFileType,
    eFrames,
    eOwner,
    eSize,
    eCreated,
    eModified,
    eLastAccess,
    eImageSize,
    eSaveBox,
    eBitsSample,
    eSamplePixel,
    eDpi,
    eOrientation,
    eCompression,
    eQuality,
    eSmoothing,
    eCodec,
    eAlphaChannel,
    eByteOrdering,
    eHPos,
    ePalettePages,
    ePaletteStyles,
    eCamera,
    eCameraDpi,
    eFrameCount,
    eLevelCount,
    eOutputPath,
    eEndianness,

    // sound info
    eLength,
    eChannels,
    eSampleRate,
    eSampleSize,
    eSampleType,
    eHowMany
  };

  TFilePath m_path;
  TLevelP m_level;
  std::vector<TFrameId> m_fids;
  QStringList m_formats;
  int m_currentIndex;
  int m_frameCount;
  TPalette *m_palette;
  QLabel m_framesLabel;
  IntField m_framesSlider;
  std::vector<std::pair<QLabel *, QLabel *>> m_labels;
  QLabel m_historyLabel;
  QTextEdit m_history;
  Separator m_separator1, m_separator2;
  void setFileInfo(const TFileStatus &status);
  void setImageInfo();
  void setSoundInfo();
  // void cleanFileInfo();
  void cleanLevelInfo();
  void setToonzSceneInfo();
  void setPaletteInfo();
  void setGeneralFileInfo(const TFilePath &path);
  QString getTypeString();
  void onSliderChanged();
  InfoViewerImp();
  ~InfoViewerImp();
  void clear();
  bool setLabel(TPropertyGroup *pg, int index, std::string type);
  void create(int index, QString str);
  void loadPalette(const TFilePath &path);

  inline void setVal(int index, const QString &str) {
    m_labels[index].second->setText(str);
  }

public slots:

  bool setItem(const TLevelP &level, TPalette *palette, const TFilePath &path);
};

//----------------------------------------------------------------

InfoViewer::InfoViewer(QWidget *parent)
    : Dialog(parent), m_imp(new InfoViewerImp()) {
  setWindowTitle(tr("File Info"));
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  // setAttribute(Qt::WA_DeleteOnClose);

  int i;
  for (i = 0; i < (int)m_imp->m_labels.size(); i++) {
    addWidgets(m_imp->m_labels[i].first, m_imp->m_labels[i].second);
    if (i == InfoViewerImp::eLastAccess) addWidget(&m_imp->m_separator1);
  }

  addWidget(&m_imp->m_separator2);
  addWidget(&m_imp->m_historyLabel);
  addWidget(&m_imp->m_history);

  addWidgets(&m_imp->m_framesLabel, &m_imp->m_framesSlider);

  connect(&m_imp->m_framesSlider, SIGNAL(valueChanged(bool)), this,
          SLOT(onSliderChanged(bool)));
  hide();
}

InfoViewer::~InfoViewer() {}

//----------------------------------------------------------------

void InfoViewer::hideEvent(QHideEvent *) { m_imp->m_level = TLevelP(); }

//----------------------------------------------------------------
void InfoViewer::onSliderChanged(bool) { m_imp->onSliderChanged(); }

//----------------------------------------------------------------

void InfoViewerImp::onSliderChanged() {
  if (m_framesSlider.getValue() - 1 == m_currentIndex) return;

  m_currentIndex = m_framesSlider.getValue() - 1;
  setImageInfo();
}

//----------------------------------------------------------------

namespace {
void setLabelStyle(QLabel *l) { l->setObjectName("TitleTxtLabel"); }
}  // namespace

//----------------------------------------------------------------

void InfoViewerImp::create(int index, QString str) {
  m_labels[index] =
      std::pair<QLabel *, QLabel *>(new QLabel(str), new QLabel(""));
  setLabelStyle(m_labels[index].first);
}

//----------------------------------------------------------------

InfoViewerImp::InfoViewerImp()
    : m_palette(0)
    , m_framesLabel(QObject::tr("Current Frame: "))
    , m_framesSlider()
    , m_history()
    , m_historyLabel(QObject::tr("File History")) {
  setLabelStyle(&m_framesLabel);

  TLevelReader::getSupportedFormats(m_formats);
  TSoundTrackReader::getSupportedFormats(m_formats);

  m_labels.resize(eHowMany);

  create(eFullpath, QObject::tr("Fullpath:     "));
  create(eFileType, QObject::tr("File Type:    "));
  create(eFrames, QObject::tr("Frames:       "));
  create(eOwner, QObject::tr("Owner:        "));
  create(eSize, QObject::tr("Size:         "));

  create(eCreated, QObject::tr("Created:      "));
  create(eModified, QObject::tr("Modified:     "));
  create(eLastAccess, QObject::tr("Last Access:  "));

  // level info

  create(eImageSize, QObject::tr("Image Size:   "));
  create(eSaveBox, QObject::tr("SaveBox:      "));
  create(eBitsSample, QObject::tr("Bits/Sample:  "));
  create(eSamplePixel, QObject::tr("Sample/Pixel: "));
  create(eDpi, QObject::tr("Dpi:          "));
  create(eOrientation, QObject::tr("Orientation:  "));
  create(eCompression, QObject::tr("Compression:  "));
  create(eQuality, QObject::tr("Quality:      "));
  create(eSmoothing, QObject::tr("Smoothing:    "));
  create(eCodec, QObject::tr("Codec:        "));
  create(eAlphaChannel, QObject::tr("Alpha Channel:"));
  create(eByteOrdering, QObject::tr("Byte Ordering:"));
  create(eHPos, QObject::tr("H Pos:"));
  create(ePalettePages, QObject::tr("Palette Pages:"));
  create(ePaletteStyles, QObject::tr("Palette Styles:"));

  create(eCamera, QObject::tr("Camera Size:      "));
  create(eCameraDpi, QObject::tr("Camera Dpi:       "));
  create(eFrameCount, QObject::tr("Number of Frames: "));
  create(eLevelCount, QObject::tr("Number of Levels: "));
  create(eOutputPath, QObject::tr("Output Path:      "));
  create(eEndianness, QObject::tr("Endianness:      "));

  // sound info
  create(eLength, QObject::tr("Length:       "));
  create(eChannels, QObject::tr("Channels: "));
  create(eSampleRate, QObject::tr("Sample Rate: "));
  create(eSampleSize, QObject::tr("Sample Size:      "));
  create(eSampleType, QObject::tr("Sample Type: "));

  m_historyLabel.setStyleSheet("color: rgb(0, 0, 200);");
  m_history.setStyleSheet("font-size: 12px; font-family: \"courier\";");
  // m_history.setStyleSheet("font-family: \"courier\";");
  m_history.setReadOnly(true);
  m_history.setFixedWidth(490);
}

//----------------------------------------------------------------

void InfoViewerImp::clear() {
  int i;

  for (i = 0; i < (int)m_labels.size(); i++) setVal(i, "");

  m_history.clear();
}

//----------------------------------------------------------------

InfoViewerImp::~InfoViewerImp() {
  int i;
  for (i = 0; i < (int)m_labels.size(); i++) {
    delete m_labels[i].first;
    delete m_labels[i].second;
  }

  m_labels.clear();
}

//----------------------------------------------------------------

void InfoViewerImp::setFileInfo(const TFileStatus &status) {
  // m_fPath.setText(status.
}

//----------------------------------------------------------------

QString InfoViewerImp::getTypeString() {
  QString ext = QString::fromStdString(m_path.getType());

  if (ext == "tlv" || ext == "tzp" || ext == "tzu")
    return "Toonz Cmapped Raster Level";
  else if (ext == "pli" || ext == "svg")
    return "Toonz Vector Level";
  else if (ext == "mov" || ext == "avi" || ext == "3gp")
    return "Movie File";
  else if (ext == "tnz")
    return "Toonz Scene";
  else if (ext == "tab")
    return "Tab Scene";
  else if (ext == "plt")
    return "Toonz Palette";
  else if (ext == "wav" || ext == "aiff" || ext == "aif" || ext == "raw" ||
           ext == "mp3" || ext == "ogg" || ext == "flac")
    return "Audio File";
  else if (ext == "mesh")
    return "Toonz Mesh Level";
  else if (ext == "tzm")
    return "Toonz Meta Level";
  else if (ext == "pic")
    return "Pic File";
  else if (Tiio::makeReader(ext.toStdString()))
    return (m_fids.size() == 1) ? "Single Raster Image" : "Raster Image Level";
  else if (ext == "psd")
    return "Photoshop Image";
  else
    return "Unmanaged File Type";
}

//----------------------------------------------------------------

void InfoViewerImp::setGeneralFileInfo(const TFilePath &path) {
  QFileInfo fi = toQString(path);
  assert(fi.exists());

  setVal(eFullpath, fi.absoluteFilePath());
  setVal(eFileType, getTypeString());
  if (fi.owner() != "") setVal(eOwner, fi.owner());
  setVal(eSize, fileSizeString(fi.size()));
  setVal(eCreated, fi.birthTime().toString());
  setVal(eModified, fi.lastModified().toString());
  setVal(eLastAccess, fi.lastRead().toString());
  m_separator1.show();
}

//----------------------------------------------------------------

bool InfoViewerImp::setLabel(TPropertyGroup *pg, int index, std::string type) {
  TProperty *p = pg->getProperty(type);
  if (!p) return false;
  QString str = QString::fromStdString(p->getValueAsString());
  if (dynamic_cast<TBoolProperty *>(p))
    setVal(index, str == "0" ? "No" : "Yes");
  else
    setVal(index, str);
  return true;
}

//----------------------------------------------------------------

void InfoViewerImp::setImageInfo() {
  if (m_path != TFilePath() && !m_fids.empty())
    setGeneralFileInfo(m_path.getType() == "tlv" || !m_path.isLevelName()
                           ? m_path
                           : m_path.withFrame(m_fids[m_currentIndex]));

  assert(m_level);

  setVal(eFrames, QString::number(m_level->getFrameCount()));

  TLevelReaderP lr(m_path);
  const TImageInfo *ii;
  try {
    ii = lr->getImageInfo(m_fids[m_currentIndex]);
  } catch (...) {
    return;
  }
  if (!m_fids.empty() && lr && ii) {
    setVal(eImageSize,
           QString::number(ii->m_lx) + " X " + QString::number(ii->m_ly));
    if (ii->m_x0 <= ii->m_x1)
      setVal(eSaveBox, "(" + QString::number(ii->m_x0) + ", " +
                           QString::number(ii->m_y0) + ", " +
                           QString::number(ii->m_x1) + ", " +
                           QString::number(ii->m_y1) + ")");
    if (ii->m_bitsPerSample > 0)
      setVal(eBitsSample, QString::number(ii->m_bitsPerSample));
    if (ii->m_samplePerPixel > 0)
      setVal(eSamplePixel, QString::number(ii->m_samplePerPixel));
    if (ii->m_dpix > 0 || ii->m_dpiy > 0)
      setVal(eDpi, "(" + QString::number(ii->m_dpix) + ", " +
                       QString::number(ii->m_dpiy) + ")");
    TPropertyGroup *pg = ii->m_properties;
    if (pg) {
      setLabel(pg, eOrientation, "Orientation");
      if (!setLabel(pg, eCompression, "Compression") &&
          !setLabel(pg, eCompression, "Compression Type") &&
          !setLabel(pg, eCompression, "RLE-Compressed"))
        setLabel(pg, eCompression, "File Compression");
      setLabel(pg, eQuality, "Quality");
      setLabel(pg, eSmoothing, "Smoothing");
      setLabel(pg, eCodec, "Codec");
      setLabel(pg, eAlphaChannel, "Alpha Channel");
      setLabel(pg, eByteOrdering, "Byte Ordering");
      setLabel(pg, eEndianness, "Endianness");
    }
  } else
    m_separator1.hide();

  const TContentHistory *ch = 0;
  if (lr) ch = lr->getContentHistory();

  if (ch) {
    QString str = ch->serialize();
    str         = str.remove('\n');
    str         = str.remove(QChar(0));
    str         = str.replace("||", "\n");
    str         = str.remove('|');
    m_history.setPlainText(str);
  }

  TImageP img        = m_level->frame(m_fids[m_currentIndex]);
  TToonzImageP timg  = (TToonzImageP)img;
  TRasterImageP rimg = (TRasterImageP)img;
  TVectorImageP vimg = (TVectorImageP)img;

  if (img) {
    TRect r = convert(timg->getBBox());
    if (r.x0 <= r.x1)
      setVal(eSaveBox,
             "(" + QString::number(r.x0) + ", " + QString::number(r.y0) + ", " +
                 QString::number(r.x1) + ", " + QString::number(r.y1) + ")");
  }

  double dpix, dpiy;

  if (timg) {
    // setVal(eHPos, QString::number(timg->gethPos()));
    timg->getDpi(dpix, dpiy);
    setVal(eDpi,
           "(" + QString::number(dpix) + ", " + QString::number(dpiy) + ")");
    TDimension dim = timg->getRaster()->getSize();
    setVal(eImageSize,
           QString::number(dim.lx) + " X " + QString::number(dim.ly));
    m_palette = timg->getPalette();
  } else if (rimg) {
    rimg->getDpi(dpix, dpiy);
    setVal(eDpi,
           "(" + QString::number(dpix) + ", " + QString::number(dpiy) + ")");
    TDimension dim = rimg->getRaster()->getSize();
    setVal(eImageSize,
           QString::number(dim.lx) + " X " + QString::number(dim.ly));
  } else if (vimg)
    m_palette = vimg->getPalette();

  // TImageP img = m_level->frame(m_fids[m_currentIndex]);
}

//----------------------------------------------------------------

void InfoViewerImp::setSoundInfo() {
  if (m_path != TFilePath()) setGeneralFileInfo(m_path);
  TSoundTrackP sndTrack;
  try {
    TSoundTrackReaderP sr(m_path);
    if (sr) sndTrack = sr->load();
  } catch (...) {
    return;
  }
  if (!sndTrack) return;

  int seconds = sndTrack->getDuration();
  int minutes = seconds / 60;
  seconds     = seconds % 60;
  QString label;
  if (minutes > 0) label += QString::number(minutes) + " min ";
  label += QString::number(seconds) + " sec";
  setVal(eLength, label);

  label = QString::number(sndTrack->getChannelCount());
  setVal(eChannels, label);

  TUINT32 sampleRate = sndTrack->getSampleRate();
  label              = QString::number(sampleRate / 1000) + " KHz";
  setVal(eSampleRate, label);

  label = QString::number(sndTrack->getBitPerSample()) + " bit";
  setVal(eSampleSize, label);

  switch (sndTrack->getSampleType()) {
  case TSound::INT:
    label = "Signed integer";
    break;
  case TSound::UINT:
    label = "Unsigned integer";
    break;
  case TSound::FLOAT:
    label = "Floating-point";
    break;
  default:
    label = "Unknown";
    break;
  }
  setVal(eSampleType, label);
}

//----------------------------------------------------------------

void InfoViewerImp::cleanLevelInfo() {}

//----------------------------------------------------------------

void InfoViewer::setItem(const TLevelP &level, TPalette *palette,
                         const TFilePath &path) {
  if (m_imp->setItem(level, palette, path))
    show();
  else
    hide();
}

//----------------------------------------------------------------

void InfoViewerImp::setToonzSceneInfo() {
  ToonzScene scene;
  try {
    scene.loadNoResources(m_path);
  } catch (...) {
    return;
  }

  TCamera *cam = scene.getCurrentCamera();
  if (!cam) return;

  TContentHistory *ch = scene.getContentHistory();
  if (ch) {
    QString str = ch->serialize();
    str         = str.remove('\n');
    str         = str.remove(QChar(0));
    str         = str.replace("||", "\n");
    str         = str.remove('|');
    m_history.setPlainText(str);
  }

  TLevelSet *set           = scene.getLevelSet();
  TSceneProperties *prop   = scene.getProperties();
  TOutputProperties *oprop = prop->getOutputProperties();

  setVal(eCamera, QString::number(cam->getRes().lx) + " X " +
                      QString::number(cam->getRes().ly));
  setVal(eCameraDpi, QString::number(cam->getDpi().x) + ", " +
                         QString::number(cam->getDpi().y));
  setVal(eFrameCount, QString::number(scene.getFrameCount()));
  if (set) setVal(eLevelCount, QString::number(set->getLevelCount()));

  if (oprop) setVal(eOutputPath, toQString(oprop->getPath()));
}

//----------------------------------------------------------------

void InfoViewerImp::setPaletteInfo() {
  if (!m_palette) return;

  setVal(ePalettePages, QString::number(m_palette->getPageCount()));
  setVal(ePaletteStyles, QString::number(m_palette->getStyleCount()));
}

//----------------------------------------------------------------

void InfoViewerImp::loadPalette(const TFilePath &path) {
  TIStream is(path);
  if (is) {
    TPersist *p = 0;
    is >> p;
    m_palette = dynamic_cast<TPalette *>(p);
  }
}

//----------------------------------------------------------------

bool InfoViewerImp::setItem(const TLevelP &level, TPalette *palette,
                            const TFilePath &path) {
  int i;
  clear();

  m_path  = path;
  m_level = level;
  m_fids.clear();
  m_currentIndex = 0;
  m_palette      = palette;
  m_framesLabel.hide();
  m_framesSlider.hide();
  m_separator1.hide();
  m_separator2.hide();

  QString ext = QString::fromStdString(m_path.getType());

  if (m_path != TFilePath() && !m_formats.contains(ext) &&
      !Tiio::makeReader(m_path.getType())) {
    // e' un file non  di immagine (plt, tnz, ...)
    assert(!m_level);

    if (!TSystem::doesExistFileOrLevel(m_path)) {
      DVGui::warning(QObject::tr("The file %1 does not exist.")
                         .arg(QString::fromStdWString(path.getWideString())));

      return false;
    }

    setGeneralFileInfo(m_path);

    if (ext == "plt") {
      assert(!m_level && !m_palette);
      loadPalette(m_path);
    } else if (ext == "tnz")
      setToonzSceneInfo();
  } else if (TFileType::getInfo(m_path) == TFileType::AUDIO_LEVEL) {
    setSoundInfo();
  } else {
    if (ext == "tlv") loadPalette(m_path.withNoFrame().withType("tpl"));

    if (!m_level) {
      assert(m_path != TFilePath());
      TLevelReaderP lr;
      try {
        lr = TLevelReaderP(m_path);
      } catch (...) {
        return false;
      }
      if (lr) {
        try {
          m_level = lr->loadInfo();
        } catch (...) {
          return false;
        }
      }
    }

    if (m_level) {
      // Image or level of images case

      // TLVs are not intended as movie file here (why?). Neither are those
      bool isMovieFile =
          (ext != "tlv" && m_formats.contains(ext) && !m_path.isLevelName());

      m_frameCount = m_level->getFrameCount();
      assert(m_frameCount);
      m_fids.resize(m_frameCount);
      TLevel::Iterator it = m_level->begin();
      for (i = 0; it != m_level->end(); ++it, ++i) m_fids[i] = it->first;

      if (m_frameCount > 1 && !isMovieFile) {
        m_framesSlider.setRange(1, m_frameCount);
        m_framesSlider.setValue(0);
        m_framesSlider.show();
        m_framesLabel.show();
      }

      setImageInfo();
    } else
      return false;
  }

  if (m_palette) setPaletteInfo();

  for (i = 0; i < (int)m_labels.size(); i++)
    if (m_labels[i].second->text() == "")
      m_labels[i].first->hide(), m_labels[i].second->hide();
    else
      m_labels[i].first->show(), m_labels[i].second->show();

  if (m_history.toPlainText() == "") {
    m_separator2.hide();
    m_historyLabel.hide();
    m_history.hide();
  } else {
    m_separator2.show();
    m_historyLabel.show();
    m_history.show();
  }

  return true;
}
