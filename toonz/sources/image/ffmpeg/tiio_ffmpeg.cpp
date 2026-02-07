

#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"

#include "tiio_ffmpeg.h"
#include "tsystem.h"
#include "tsound.h"
#include "timageinfo.h"
#include "toonz/stage.h"
#include "trop.h"

#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include "tmsgcore.h"
#include "thirdparty.h"

Ffmpeg::Ffmpeg() {
  int userTimeoutSec = ThirdParty::getFFmpegTimeout();
  m_ffmpegTimeoutMs  = (userTimeoutSec > 0) ? userTimeoutSec * 1000 : 30000;

  m_intermediateFormat = "png";
  m_startNumber        = std::numeric_limits<int>::max();
  m_frameCount         = 0;
  m_lx = m_ly = m_bpp = 0;
  m_frameRate         = 0.0;
  m_hasSoundTrack     = false;
  m_sampleRate = m_channelCount = m_bitsPerSample = 0;
}

Ffmpeg::~Ffmpeg() { cleanUpFiles(); }

bool Ffmpeg::checkFormat(std::string format) {
  // Cache with reload every hour (avoids becoming outdated if ffmpeg changes)
  static QString cachedFormats;
  static QDateTime lastCheck;

  QDateTime now = QDateTime::currentDateTime();
  if (cachedFormats.isEmpty() || lastCheck.secsTo(now) > 3600) {  // 1 hour
    QStringList args;
    args << "-formats";

    QProcess ffmpeg;
    ThirdParty::runFFmpeg(ffmpeg, args);
    ffmpeg.waitForFinished(8000);

    cachedFormats =
        ffmpeg.readAllStandardError() + ffmpeg.readAllStandardOutput();
    ffmpeg.close();

    lastCheck = now;
  }

  return cachedFormats.contains(QString::fromStdString(format),
                                Qt::CaseInsensitive);
}

TFilePath Ffmpeg::getFfmpegCache() const {
  QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  TFilePath cachePath(cacheRoot + "/ffmpeg");
  if (!TSystem::doesExistFileOrLevel(cachePath)) {
    try {
      TSystem::mkDir(cachePath);
    } catch (const TSystemException &e) {
      DVGui::warning(QObject::tr("Cannot create FFmpeg cache directory: %1")
                         .arg(QString::fromStdWString(e.getMessage())));
    }
  }
  return cachePath;
}

void Ffmpeg::setFrameRate(double fps) { m_frameRate = fps; }

void Ffmpeg::setPath(const TFilePath &path) { m_path = path; }

void Ffmpeg::createIntermediateImage(const TImageP &img, int frameIndex) {
  m_frameCount++;
  int idx = frameIndex - 1;
  if (idx < m_startNumber) m_startNumber = idx;

  // Name without UUID, with 6-digit padding to match %06d in ffmpeg
  QString tempFileName = QString::fromStdString(m_path.getName()) + "tempOut" +
                         QString("%1").arg(idx, 6, 10, QChar('0')) + "." +
                         m_intermediateFormat;

  QString tempPath =
      getFfmpegCache().getQString() + QDir::separator() + tempFileName;

  TRasterImageP tempImage(img);
  if (!tempImage) {
    DVGui::warning(QObject::tr("Cannot save non-raster image to FFmpeg"));
    return;
  }

  TRasterP raster = tempImage->getRaster();
  if (!raster || raster->getLx() <= 0 || raster->getLy() <= 0) {
    DVGui::warning(QObject::tr("Invalid or empty raster for FFmpeg"));
    return;
  }

  m_lx  = raster->getLx();
  m_ly  = raster->getLy();
  m_bpp = raster->getPixelSize();

  TRaster32P raster32;

  if (m_bpp == 4) {
    // Already 32-bit integer fast path, clone and mirror
    raster32 = raster->clone();
    if (!raster32) {
      DVGui::warning(QObject::tr("Failed to clone raster for FFmpeg"));
      return;
    }
    raster32->yMirror();  // Mirror only once here
  } else {
    // FP32 or 64-bit convert safely
    raster32 = TRaster32P(m_lx, m_ly);
    if (!raster32) {
      DVGui::warning(
          QObject::tr("Failed to allocate 32-bit raster for FFmpeg"));
      return;
    }

    // float (16 bytes/pixel), do clamp
    if (m_bpp == 16) {
      TRasterPT<TPixelF> rasterFloat(raster);
      if (rasterFloat) {
        rasterFloat->lock();
        for (int y = 0; y < m_ly; ++y) {
          TPixelF *pix = rasterFloat->pixels(y);
          for (int x = 0; x < m_lx; ++x, ++pix) {
            pix->r = tcrop(pix->r, 0.0f, 1.0f);
            pix->g = tcrop(pix->g, 0.0f, 1.0f);
            pix->b = tcrop(pix->b, 0.0f, 1.0f);
            pix->m = tcrop(pix->m, 0.0f, 1.0f);
          }
        }
        rasterFloat->unlock();
      }
    }

    // Convert to 32-bit ARGB
    try {
      TRop::convert(raster32, raster);
    } catch (...) {
      DVGui::warning(
          QObject::tr("Failed to convert raster to 32-bit ARGB (after clamp)"));
      return;
    }
    raster32->yMirror();  // Mirror only once here
  }

  QImage qi(raster32->getRawData(), m_lx, m_ly, raster32->getWrap() * 4,
            QImage::Format_ARGB32);

  if (qi.isNull()) {
    DVGui::warning(
        QObject::tr("Failed to create QImage for intermediate file"));
    return;
  }

  // qi = qi.copy();
  // Currently redundant,  removed for performance & clarity.
  // If raster-to-QImage sharing is needed in the future, we can revisit.

  QByteArray formatBa = m_intermediateFormat.toUpper().toLatin1();
  const char *format  = formatBa.constData();

  if (!qi.save(tempPath, format, -1)) {
    DVGui::warning(
        QObject::tr("Failed to save intermediate image: %1").arg(tempPath));
    return;
  }

  m_cleanUpList.push_back(tempPath);
}

void Ffmpeg::runFfmpeg(const QStringList &preInputArgs,
                       const QStringList &postInputArgs, bool inputPathIncluded,
                       bool outputPathIncluded, bool overwrite,
                       bool asynchronous) {
  // Pattern with %06d to exactly match the generated names
  QString tempName = getFfmpegCache().getQString() + QDir::separator() +
                     QString::fromStdString(m_path.getName()) + "tempOut%06d." +
                     m_intermediateFormat;

  QStringList args = preInputArgs;

  if (!inputPathIncluded) {
    if (m_startNumber != 0 &&
        m_startNumber != std::numeric_limits<int>::max()) {
      args << "-start_number" << QString::number(m_startNumber);
    }
    args << "-i" << tempName;
  }

  if (m_hasSoundTrack) args.append(m_audioArgs);
  args.append(postInputArgs);

  if (overwrite && !outputPathIncluded) args << "-y";

  if (!outputPathIncluded) args << m_path.getQString();

  QProcess ffmpeg;
  ThirdParty::runFFmpeg(ffmpeg, args);

  bool success = waitFfmpeg(ffmpeg, asynchronous);
  if (!success) {
    DVGui::warning(
        QObject::tr("FFmpeg process failed for: %1").arg(m_path.getQString()));
  }

  ffmpeg.close();
}

void Ffmpeg::runFfmpeg(const QStringList &preInputArgs,
                       const QStringList &postInputArgs,
                       const TFilePath &outputPath) {
  setPath(outputPath);
  runFfmpeg(preInputArgs, postInputArgs, false, false, true, true);
}

QString Ffmpeg::runFfprobe(const QStringList &args) const {
  QProcess proc;
  ThirdParty::runFFprobe(proc, args);

  if (!waitFfmpeg(proc, false)) {
    throw TImageException(m_path, "error accessing ffprobe.");
  }

  QString results = proc.readAllStandardError() + proc.readAllStandardOutput();
  int exitCode    = proc.exitCode();
  proc.close();

  if (exitCode > 0) {
    throw TImageException(m_path, "error reading info.");
  }

  return results;
}

bool Ffmpeg::waitFfmpeg(QProcess &process, bool async) const {
  if (!async) {
    bool status = process.waitForFinished(m_ffmpegTimeoutMs);
    if (!status) {
      DVGui::warning(
          QObject::tr("FFmpeg timed out.\n"
                      "Please check the file for errors.\n"
                      "If the file doesn't play or is incomplete, \n"
                      "Please try raising the FFmpeg timeout in Preferences."));
    }
    return status;
  }

  int exitCode = ThirdParty::waitAsyncProcess(process, m_ffmpegTimeoutMs);
  if (exitCode == 0) return true;

  if (exitCode == -1) {
    DVGui::warning(
        QObject::tr("FFmpeg returned error-code: %1").arg(process.exitCode()));
  } else if (exitCode == -2) {
    DVGui::warning(
        QObject::tr("FFmpeg timed out.\n"
                    "Please check the file for errors.\n"
                    "If the file doesn't play or is incomplete, \n"
                    "Please try raising the FFmpeg timeout in Preferences."));
  }

  return false;
}

void Ffmpeg::saveSoundTrack(TSoundTrack *soundTrack) {
  if (!soundTrack) return;

  m_sampleRate        = soundTrack->getSampleRate();
  m_channelCount      = soundTrack->getChannelCount();
  m_bitsPerSample     = soundTrack->getBitPerSample();
  int bufSize         = static_cast<int>(soundTrack->getSampleCount() *
                                         soundTrack->getSampleSize());
  const UCHAR *buffer = soundTrack->getRawData();

  m_audioPath = getFfmpegCache().getQString() + QDir::separator() +
                QString::fromStdString(m_path.getName()) + "tempOut.raw";

  m_audioFormat = ((soundTrack->getSampleType() == TSound::FLOAT) ? "f" : "s") +
                  QString::number(m_bitsPerSample);
  if (m_bitsPerSample > 8) m_audioFormat.append("le");

  QByteArray data(reinterpret_cast<const char *>(buffer), bufSize);

  QFile file(m_audioPath);
  if (!file.open(QIODevice::WriteOnly)) {
    DVGui::warning(
        QObject::tr("Cannot open audio file for writing: %1").arg(m_audioPath));
    return;
  }

  if (file.write(data) != bufSize) {
    DVGui::warning(QObject::tr("Failed to write audio data to file"));
    file.close();
    return;
  }

  file.close();
  m_hasSoundTrack = true;

  m_audioArgs = QStringList{"-f",  m_audioFormat,
                            "-ar", QString::number(m_sampleRate),
                            "-ac", QString::number(m_channelCount),
                            "-i",  m_audioPath};

  m_cleanUpList.push_back(m_audioPath);
}

bool Ffmpeg::checkFilesExist() const {
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath = ffmpegCachePath + QDir::separator() + cleanPathSymbols() +
                     "In0001." + m_intermediateFormat;
  return TSystem::doesExistFileOrLevel(TFilePath(tempPath));
}

ffmpegFileInfo Ffmpeg::getInfo() const {
  int lx           = 0;
  int ly           = 0;
  int frameCount   = 0;
  double frameRate = 0.0;

  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath =
      ffmpegCachePath + QDir::separator() + cleanPathSymbols() + ".txt";

  if (QFile::exists(tempPath)) {
    QFile infoText(tempPath);
    if (infoText.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QByteArray ba = infoText.readAll();
      infoText.close();
      QString text         = QString::fromUtf8(ba.constData(), ba.size());
      QStringList textlist = text.split(" ", Qt::SkipEmptyParts);
      if (textlist.size() >= 4) {
        lx         = textlist[0].toInt();
        ly         = textlist[1].toInt();
        frameRate  = textlist[2].toDouble();
        frameCount = textlist[3].toInt();
      }
    }
  } else {
    QStringList failedOperations;
    QString errorDetails;

    try {
      TDimension size = getSize();
      lx              = size.lx;
      ly              = size.ly;
    } catch (const TImageException &e) {
      failedOperations.append("size");
      errorDetails += QObject::tr("Size error: %1\n")
                          .arg(QString::fromStdWString(e.getMessage()));
    }

    try {
      frameRate = getFrameRate();
    } catch (const TImageException &e) {
      failedOperations.append("frame rate");
      errorDetails += QObject::tr("Frame rate error: %1\n")
                          .arg(QString::fromStdWString(e.getMessage()));
    }

    try {
      frameCount = getFrameCount();
    } catch (const TImageException &e) {
      failedOperations.append("frame count");
      errorDetails += QObject::tr("Frame count error: %1\n")
                          .arg(QString::fromStdWString(e.getMessage()));
    }

    if (!failedOperations.isEmpty()) {
      QString operations = failedOperations.join(", ");
      DVGui::warning(
          QObject::tr(
              "Failed to retrieve %1 from video via ffprobe.\n"
              "Error details:\n%2"
              "Using available data and fallback values where necessary.\n"
              "Note: Zero values in dimensions, frame count or frame rate "
              "indicate "
              "that the information could not be retrieved.")
              .arg(operations)
              .arg(errorDetails));
    }

    // Always write to cache, even if we have fallback values (0)
    QFile infoText(tempPath);
    if (infoText.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QString infoToWrite = QString("%1 %2 %3 %4\n")
                                .arg(lx)
                                .arg(ly)
                                .arg(frameRate, 0, 'f', 3)
                                .arg(frameCount);
      infoText.write(infoToWrite.toUtf8());
      infoText.close();
    }
  }

  // Return the info structure
  // Note: Zero values in any field may indicate that ffprobe failed to retrieve
  // that particular information. Callers should handle these cases
  // appropriately.
  return ffmpegFileInfo{lx, ly, frameCount, frameRate};
}

TRasterImageP Ffmpeg::getImage(int frameIndex) {
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath = ffmpegCachePath + QDir::separator() + cleanPathSymbols();
  QString number   = QString("%1").arg(frameIndex, 4, 10, QChar('0'));
  QString tempName = tempPath + QDir::separator() + "In" + number + ".png";

  if (TSystem::doesExistFileOrLevel(TFilePath(tempName))) {
    QImage tempImg(tempName);
    if (!tempImg.isNull()) {
      QImage convertedImg = tempImg.convertToFormat(QImage::Format_ARGB32);
      const uchar *bits   = convertedImg.constBits();

      TRasterPT<TPixelRGBM32> ret;
      ret.create(m_lx, m_ly);
      ret->lock();
      memcpy(ret->getRawData(), bits, m_lx * m_ly * 4);
      ret->unlock();
      ret->yMirror();
      return TRasterImageP(ret);
    }
  }
  return TRasterImageP();
}

double Ffmpeg::getFrameRate() const {
  QStringList fpsArgs;
  fpsArgs << "-v" << "error" << "-select_streams" << "v:0" << "-show_entries"
          << "stream=r_frame_rate" << "-of"
          << "default=noprint_wrappers=1:nokey=1" << m_path.getQString();

  QString fpsResults;
  try {
    fpsResults = runFfprobe(fpsArgs);
  } catch (const TImageException &) {
    fpsResults = "";
  }

  int fpsNum = 0, fpsDen = 0;
  if (!fpsResults.isEmpty()) {
    QStringList fpsResultsList = fpsResults.split('/');
    if (fpsResultsList.size() > 1) {
      fpsNum = fpsResultsList[0].toInt();
      fpsDen = fpsResultsList[1].toInt();
    }
  }

  if (!fpsDen) {
    fpsArgs.clear();
    fpsArgs << "-v" << "error" << "-select_streams" << "v:0" << "-show_entries"
            << "stream=avg_frame_rate" << "-of"
            << "default=noprint_wrappers=1:nokey=1" << m_path.getQString();

    try {
      fpsResults = runFfprobe(fpsArgs);
    } catch (const TImageException &) {
      fpsResults = "";
    }

    if (!fpsResults.isEmpty()) {
      QStringList fpsResultsList = fpsResults.split('/');
      if (fpsResultsList.size() > 1) {
        fpsNum = fpsResultsList[0].toInt();
        fpsDen = fpsResultsList[1].toInt();
      }
    }
  }

  return (fpsDen > 0) ? static_cast<double>(fpsNum) / fpsDen : 0.0;
}

TDimension Ffmpeg::getSize() const {
  QStringList sizeArgs;
  sizeArgs << "-v" << "error" << "-of" << "flat=s=_" << "-select_streams"
           << "v:0" << "-show_entries" << "stream=height,width"
           << m_path.getQString();

  QString sizeResults;
  try {
    sizeResults = runFfprobe(sizeArgs);
  } catch (const TImageException &) {
    sizeResults = "";
  }

  int lx = m_lx;
  int ly = m_ly;
  if (!sizeResults.isEmpty()) {
    QStringList lines = sizeResults.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
      if (line.contains("width=")) {
        lx = line.section('=', 1, 1).toInt();
      } else if (line.contains("height=")) {
        ly = line.section('=', 1, 1).toInt();
      }
    }
  }

  return TDimension(lx, ly);
}

int Ffmpeg::getFrameCount() const {
  QStringList frameCountArgs;
  frameCountArgs << "-v" << "error" << "-count_frames" << "-select_streams"
                 << "v:0" << "-show_entries" << "stream=duration" << "-of"
                 << "default=nokey=1:noprint_wrappers=1" << m_path.getQString();

  QString frameResults;
  try {
    frameResults = runFfprobe(frameCountArgs);
  } catch (const TImageException &) {
    frameResults = "";
  }

  int count = 0;
  if (!frameResults.isEmpty()) {
    bool ok         = false;
    double duration = frameResults.toDouble(&ok);
    if (ok && duration > 0) {
      count = static_cast<int>(duration * getFrameRate());
    }
  }

  if (!count) {
    frameCountArgs.clear();
    frameCountArgs << "-v" << "error" << "-count_frames" << "-select_streams"
                   << "v:0" << "-show_entries" << "stream=nb_read_frames"
                   << "-of" << "default=nokey=1:noprint_wrappers=1"
                   << m_path.getQString();

    try {
      frameResults = runFfprobe(frameCountArgs);
    } catch (const TImageException &) {
      frameResults = "";
    }

    if (!frameResults.isEmpty()) {
      count = frameResults.toInt();
    }
  }

  return count;
}

void Ffmpeg::getFramesFromMovie(int frame) {
  // ensure frame count before using
  if (m_frameCount <= 0) {
    m_frameCount = getFrameCount();
  }

  if (m_frameCount <= 0) {
    DVGui::warning(QObject::tr("Unable to determine frame count for: %1")
                       .arg(m_path.getQString()));
    return;
  }

  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath = ffmpegCachePath + QDir::separator() + cleanPathSymbols();
  QString tempName =
      tempPath + QDir::separator() + "In%04d." + m_intermediateFormat;

  QString tempStart;
  if (frame == -1) {
    tempStart = tempPath + QDir::separator() + "In0001." + m_intermediateFormat;
  } else {
    tempStart = tempPath + QDir::separator() +
                QString("In%1.").arg(frame, 4, 10, QChar('0')) +
                m_intermediateFormat;
  }

  if (!TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
    QStringList probeArgs;
    probeArgs << "-v" << "error" << "-select_streams" << "v:0"
              << "-show_entries" << "stream=codec_name" << "-of"
              << "default=noprint_wrappers=1:nokey=1" << m_path.getQString();

    QString codecName;
    try {
      codecName = runFfprobe(probeArgs).trimmed();
    } catch (const TImageException &) {
      codecName = "";
    }

    QStringList preIFrameArgs;
    if (codecName.contains("vp9", Qt::CaseInsensitive)) {
      preIFrameArgs << "-vcodec" << "libvpx-vp9";
    } else if (codecName.contains("vp8", Qt::CaseInsensitive)) {
      preIFrameArgs << "-vcodec" << "libvpx";
    } else if (codecName.contains("av1", Qt::CaseInsensitive)) {
      preIFrameArgs << "-vcodec" << "libaom-av1";
    }

    preIFrameArgs << "-threads" << "auto" << "-i" << m_path.getQString();

    QStringList postIFrameArgs;
    postIFrameArgs << "-y" << "-f" << "image2" << tempName;

    runFfmpeg(preIFrameArgs, postIFrameArgs, true, true, true, true);

    for (int i = 1; i <= m_frameCount; i++) {
      QString frameFile = tempPath + QDir::separator() +
                          QString("In%1.").arg(i, 4, 10, QChar('0')) +
                          m_intermediateFormat;
      // avoid duplicates in cleanup
      if (!m_cleanUpList.contains(frameFile)) {
        m_cleanUpList.push_back(frameFile);
      }
    }
  }
}

QString Ffmpeg::cleanPathSymbols() const {
  QString name = QString::fromStdString(m_path.getName());
  name.replace(QRegularExpression("[^a-zA-Z0-9_\\-\\.]"), "_");
  return name;
}

int Ffmpeg::getGifFrameCount() {
  int frame               = 1;
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath = ffmpegCachePath + QDir::separator() + cleanPathSymbols();

  while (true) {
    QString frameFile = tempPath + QDir::separator() +
                        QString("In%1.").arg(frame, 4, 10, QChar('0')) +
                        m_intermediateFormat;
    if (!TSystem::doesExistFileOrLevel(TFilePath(frameFile))) {
      break;
    }
    frame++;
  }

  return frame - 1;
}

void Ffmpeg::addToCleanUp(const QString &path) {
  if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
    m_cleanUpList.push_back(path);
  }
}

void Ffmpeg::cleanUpFiles() {
  for (const QString &path : m_cleanUpList) {
    if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
      try {
        TSystem::deleteFile(TFilePath(path));
      } catch (const TSystemException &e) {
        DVGui::warning(QObject::tr("Failed to delete file: %1\n%2")
                           .arg(path)
                           .arg(QString::fromStdWString(e.getMessage())));
      }
    }
  }
  m_cleanUpList.clear();
}

void Ffmpeg::disablePrecompute() {
  Preferences::instance()->setPrecompute(false);
}

//===========================================================
//
//  TImageReaderFFmpeg
//
//===========================================================

class TImageReaderFFmpeg final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderFFmpeg(const TFilePath &path, int index, TLevelReaderFFmpeg *lra,
                     TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    if (m_lra) m_lra->addRef();
  }

  ~TImageReaderFFmpeg() override {
    if (m_lra) m_lra->release();
  }

  TImageP load() override {
    return m_lra ? m_lra->load(m_frameIndex) : TImageP();
  }

  TDimension getSize() const { return m_lra ? m_lra->getSize() : TDimension(); }

  TRect getBBox() const { return TRect(); }

  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderFFmpeg *m_lra;
  TImageInfo *m_info;

  TImageReaderFFmpeg(const TImageReaderFFmpeg &)            = delete;
  TImageReaderFFmpeg &operator=(const TImageReaderFFmpeg &) = delete;
};

//===========================================================
//
//  TLevelReaderFFmpeg
//
//===========================================================

TLevelReaderFFmpeg::TLevelReaderFFmpeg(const TFilePath &path)
    : TLevelReader(path)
    , m_ffmpegReader(nullptr)
    , m_frameCount(-1)
    , m_lx(0)
    , m_ly(0)
    , m_framesExtracted(false)
    , m_imageInfo(nullptr) {
  m_ffmpegReader = new Ffmpeg();
  if (!m_ffmpegReader) {
    throw TImageException(path, "Failed to create FFmpeg reader");
  }

  m_ffmpegReader->setPath(path);
  m_ffmpegReader->disablePrecompute();

  try {
    ffmpegFileInfo tempInfo = m_ffmpegReader->getInfo();
    double fps              = tempInfo.m_frameRate;
    m_frameCount            = tempInfo.m_frameCount;
    m_size                  = TDimension(tempInfo.m_lx, tempInfo.m_ly);
    m_lx                    = m_size.lx;
    m_ly                    = m_size.ly;

    m_imageInfo                   = new TImageInfo();
    m_imageInfo->m_frameRate      = fps;
    m_imageInfo->m_lx             = m_lx;
    m_imageInfo->m_ly             = m_ly;
    m_imageInfo->m_bitsPerSample  = 8;
    m_imageInfo->m_samplePerPixel = 4;
    m_imageInfo->m_dpix           = Stage::standardDpi;
    m_imageInfo->m_dpiy           = Stage::standardDpi;
  } catch (const TImageException &e) {
    delete m_ffmpegReader;
    m_ffmpegReader = nullptr;
    throw;
  }
}

TLevelReaderFFmpeg::~TLevelReaderFFmpeg() {
  delete m_ffmpegReader;
  delete m_imageInfo;
}

TLevelP TLevelReaderFFmpeg::loadInfo() {
  if (m_frameCount <= 0) return TLevelP();

  TLevelP level;
  for (int i = 1; i <= m_frameCount; ++i) {
    level->setFrame(i, TImageP());
  }

  return level;
}

TImageReaderP TLevelReaderFFmpeg::getFrameReader(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageReaderP(nullptr);
  int index = fid.getNumber();

  if (index < 1 || (m_frameCount > 0 && index > m_frameCount)) {
    return TImageReaderP(nullptr);
  }

  TImageReaderFFmpeg *irm =
      new TImageReaderFFmpeg(m_path, index, this, m_imageInfo);
  return TImageReaderP(irm);
}

TDimension TLevelReaderFFmpeg::getSize() { return m_size; }

TImageP TLevelReaderFFmpeg::load(int frameIndex) {
  if (!m_ffmpegReader) return TImageP();

  if (!m_framesExtracted) {
    try {
      m_ffmpegReader->getFramesFromMovie();
      m_framesExtracted = true;
    } catch (const TImageException &e) {
      DVGui::warning(QObject::tr("Failed to extract frames from movie: %1")
                         .arg(QString::fromStdWString(e.getMessage())));
      return TImageP();
    }
  }

  return m_ffmpegReader->getImage(frameIndex);
}
