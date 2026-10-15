#include "stdfx.h"
#include "globalcontrollablefx.h"
#include "tfxparam.h"
#include "tnotanimatableparam.h"
#include "toonz/lut3d.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <memory>

namespace {
template <class PIXEL>
void applyLut(TRasterPT<PIXEL> raster, const Lut3D &lut) {
  const float maxValue = static_cast<float>(PIXEL::maxChannelValue);
  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    PIXEL *pixel = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); ++x, ++pixel) {
      if (pixel->m == 0) {
        pixel->r = pixel->g = pixel->b = 0;
        continue;
      }
      const float alpha = static_cast<float>(pixel->m) / maxValue;
      float r           = static_cast<float>(pixel->r) / maxValue / alpha;
      float g           = static_cast<float>(pixel->g) / maxValue / alpha;
      float b           = static_cast<float>(pixel->b) / maxValue / alpha;
      lut.convert(r, g, b);
      pixel->r = static_cast<typename PIXEL::Channel>(
          tcrop(r, 0.0f, 1.0f) * alpha * maxValue + 0.5f);
      pixel->g = static_cast<typename PIXEL::Channel>(
          tcrop(g, 0.0f, 1.0f) * alpha * maxValue + 0.5f);
      pixel->b = static_cast<typename PIXEL::Channel>(
          tcrop(b, 0.0f, 1.0f) * alpha * maxValue + 0.5f);
    }
  }
  raster->unlock();
}

template <>
void applyLut<TPixelF>(TRasterFP raster, const Lut3D &lut) {
  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    TPixelF *pixel = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); ++x, ++pixel) {
      if (pixel->m <= 0.0f) {
        pixel->r = pixel->g = pixel->b = 0.0f;
        continue;
      }
      const float alpha = pixel->m;
      float r = pixel->r / alpha, g = pixel->g / alpha, b = pixel->b / alpha;
      lut.convert(r, g, b);
      pixel->r = tcrop(r, 0.0f, 1.0f) * alpha;
      pixel->g = tcrop(g, 0.0f, 1.0f) * alpha;
      pixel->b = tcrop(b, 0.0f, 1.0f) * alpha;
    }
  }
  raster->unlock();
}
}  // namespace

class Lut3DBakeFx final : public GlobalControllableFx {
  FX_PLUGIN_DECLARATION(Lut3DBakeFx)

  struct LutCache {
    QMutex mutex;
    QString revision;
    QString error;
    std::shared_ptr<const Lut3D> lut;
  };

  TRasterFxPort m_input;
  TFilePathParamP m_lutPath;
  std::shared_ptr<LutCache> m_cache;

  QString fileRevision() const {
    const QFileInfo info(QString::fromStdWString(m_lutPath->getValue()));
    return info.absoluteFilePath() + ":" +
           QString::number(info.lastModified().toMSecsSinceEpoch()) + ":" +
           QString::number(info.size()) + ":" +
           QString::number(info.isFile() && info.isReadable());
  }

  std::shared_ptr<const Lut3D> loadCurrentLut(QString &error) const {
    const QString path     = QString::fromStdWString(m_lutPath->getValue());
    const QString revision = fileRevision();
    QMutexLocker lock(&m_cache->mutex);
    if (revision != m_cache->revision) {
      auto loaded = std::make_shared<Lut3D>();
      QString loadError;
      m_cache->lut      = loaded->load(path, &loadError) ? loaded : nullptr;
      m_cache->error    = loadError;
      m_cache->revision = revision;
    }
    error = m_cache->error;
    return m_cache->lut;
  }

public:
  Lut3DBakeFx() : m_lutPath(L""), m_cache(std::make_shared<LutCache>()) {
    addInputPort("Source", m_input);
    bindParam(this, "lutFile", m_lutPath);
    m_lutPath->setFileFilter("3D LUT files (*.cube *.3dl)");
    enableComputeInFloat(true);
  }

  TFx *clone(bool recursive = true) const override {
    auto fx     = static_cast<Lut3DBakeFx *>(TFx::clone(recursive));
    fx->m_cache = m_cache;
    return fx;
  }

  bool canHandle(const TRenderSettings &, double) override { return true; }

  // Match Preferences: sample display-referred RGB. The FX framework performs
  // the scene gamma conversions around this node when rendering in linear RGB.
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override {
    return TRasterFx::getAlias(frame, info) +
           "[LUT:" + fileRevision().toUtf8().toStdString() + "]";
  }

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override {
    if (!m_input.isConnected()) {
      bbox = TRectD();
      return false;
    }
    return m_input->doGetBBox(frame, bbox, info);
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_input.isConnected()) return;
    const QString path = QString::fromStdWString(m_lutPath->getValue());
    if (path.isEmpty()) {
      m_input->compute(tile, frame, ri);
      return;
    }
    QString error;
    const auto lut = loadCurrentLut(error);
    if (!lut)
      throw TException(QString("3D LUT Bake [%1]: %2\n%3")
                           .arg(QString::fromStdWString(getFxId()), path, error)
                           .toStdWString());

    m_input->compute(tile, frame, ri);

    // Keep an immutable snapshot alive while other render tiles run or reload.
    // Do not hold the file-cache mutex during pixel processing.
    if (TRasterFP raster = tile.getRaster())
      applyLut<TPixelF>(raster, *lut);
    else if (TRaster64P raster = tile.getRaster())
      applyLut<TPixel64>(raster, *lut);
    else if (TRaster32P raster = tile.getRaster())
      applyLut<TPixel32>(raster, *lut);
    else
      throw TException("3D LUT Bake: unsupported pixel type");
  }
};

FX_PLUGIN_IDENTIFIER(Lut3DBakeFx, "lut3DBakeFx")
