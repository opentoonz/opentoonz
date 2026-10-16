#include "image/png/tiio_png.h"
#include "image/pli/tiio_pli.h"
#include "tiio.h"
#include "tfiletype.h"
#include "tstroke.h"
#include "tcolorfunctions.h"
#include "tpalette.h"
#include "tsimplecolorstyles.h"
#include "tofflinegl.h"
#include "tvectorimage.h"
#include "tvectorrenderdata.h"
#include "tenv.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <array>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

void writePatterns(const QString &root) {
  require(QDir().mkpath(root + "/custom styles"),
          "Could not create test library");
  const std::array<QRgb, 3> colors{qRgba(255, 0, 0, 255), qRgba(0, 255, 0, 255),
                                   qRgba(0, 0, 255, 255)};
  for (int frame = 0; frame < 3; ++frame) {
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int y = 12; y < 20; ++y)
      for (int x = 12; x < 20; ++x) image.setPixel(x, y, colors[frame]);
    require(image.save(root + QString("/custom styles/cycle.%1.png")
                                  .arg(frame + 1, 4, 10, QChar('0'))),
            "Could not write test pattern");
    QImage tiny(16384, 32, QImage::Format_ARGB32);
    tiny.fill(Qt::transparent);
    tiny.setPixel(8000, 15, colors[frame]);
    require(tiny.save(root + QString("/custom styles/tiny.%1.png")
                                 .arg(frame + 1, 4, 10, QChar('0'))),
            "Could not write tiny-artwork pattern");
  }
}

void writeVectorPatterns(const QString &root) {
  TPaletteP palette = new TPalette;
  palette->setStyle(1, TPixel32(255, 0, 0, 255));
  palette->addStyle(TPixel32(0, 255, 0, 255));
  palette->addStyle(TPixel32(0, 0, 255, 255));
  palette->getPage(0)->addStyle(2);
  palette->getPage(0)->addStyle(3);
  for (const QString &name : {QString("vector"), QString("rasterized")}) {
    TLevelWriterPli writer(TFilePath(root + "/custom styles/" + name + ".pli"),
                           nullptr);
    for (int frame = 0; frame < 3; ++frame) {
      TVectorImageP image(new TVectorImage);
      image->setPalette(palette.getPointer());
      auto *stroke = new TStroke(
          std::vector<TThickPoint>{{-20, 0, 40}, {0, 0, 40}, {20, 0, 40}});
      stroke->setStyle(frame + 1);
      image->addStroke(stroke, false);
      writer.getFrameWriter(TFrameId(frame + 1))->save(image);
    }
  }
}

TVectorImageP makeImage(int offset, int step, double length = 0.01,
                        const std::string &resource = "cycle") {
  TVectorImageP image(new TVectorImage);
  TPaletteP palette = new TPalette;
  TColorStyle *style;
  if (resource == "vector") {
    auto *trail = new TVectorImagePatternStrokeStyle(resource);
    require(trail->getLevelFrameCount() == 3,
            "Vector Trail did not load all frames");
    style = trail;
  } else {
    auto *trail = new TRasterImagePatternStrokeStyle(resource);
    require(trail->getLevelFrameCount() == 3,
            "Raster Trail did not load all frames");
    style = trail;
  }
  style->setParamValue(0, 0.0);
  palette->setStyle(1, style);
  image->setPalette(palette.getPointer());
  auto *stroke = new TStroke(std::vector<TThickPoint>{
      {0, 0, 12}, {length / 2, 0, 12}, {length, 0, 12}});
  stroke->setStyle(1);
  stroke->outlineOptions().m_patternFrameOffset = offset;
  stroke->outlineOptions().m_patternFrameStep   = step;
  image->addStroke(stroke, false);
  return image;
}

TRaster32P render(const TVectorImageP &image, const TRect &clip = TRect(),
                  const TColorFunction *function = nullptr, double zoom = 1.0) {
  TOfflineGL context(TDimension(256, 256));
  context.clear(TPixel32(0, 0, 0, 0));
  const TVectorRenderData data(TTranslation(64, 128) * TScale(zoom), clip,
                               image->getPalette(), function, true, true);
  context.draw(image, data, true);
  return context.getRaster();
}

void checkFrameSelection() {
  for (const std::string resource : {"cycle", "vector", "rasterized"}) {
    for (int frame = 0; frame < 3; ++frame) {
      TRaster32P raster    = render(makeImage(frame, 0, 0.01, resource));
      const TPixel32 pixel = raster->pixels(128)[64];
      require(pixel.m > 0, "Trail disappeared in offscreen output");
      const std::array<int, 3> channels{pixel.r, pixel.g, pixel.b};
      if (channels[frame] <= 200)
        throw std::runtime_error(
            "Wrong frame for " + resource + " at " + std::to_string(frame) +
            ": " + std::to_string(pixel.r) + "," + std::to_string(pixel.g) +
            "," + std::to_string(pixel.b));
    }
  }
}

void checkVectorFixture(const QString &root) {
  TLevelReaderPli reader(TFilePath(root + "/custom styles/vector.pli"));
  TLevelP info = reader.loadInfo();
  for (int frame = 0; frame < 3; ++frame) {
    TVectorImageP image = reader.getFrameReader(TFrameId(frame + 1))->load();
    require(image && image->getStrokeCount() == 1, "Invalid vector fixture");
    const int style = image->getStroke(0)->getStyle();
    require(info->getPalette() && style < info->getPalette()->getStyleCount(),
            "Vector fixture references a missing palette style");
    const auto color = info->getPalette()->getStyle(style)->getMainColor();
    const std::array<int, 3> channels{color.r, color.g, color.b};
    if (channels[frame] != 255)
      throw std::runtime_error("Invalid vector fixture color at " +
                               std::to_string(frame) + " style " +
                               std::to_string(style));
  }
}

void checkTiles() {
  TVectorImageP image = makeImage(2, -1, 160);
  TRaster32P whole    = render(image);
  TRaster32P left     = render(image, TRect(0, 0, 127, 255));
  TRaster32P right    = render(image, TRect(128, 0, 255, 255));
  for (int y = 0; y < 256; ++y) {
    for (int x = 0; x < 256; ++x) {
      const TPixel32 expected = whole->pixels(y)[x];
      const TPixel32 actual =
          x < 128 ? left->pixels(y)[x] : right->pixels(y)[x];
      require(actual == expected, "Render tile changed Trail pixels");
    }
  }
  auto *style         = image->getPalette()->getStyle(1);
  const TRectD bounds = style->getStrokeBBox(image->getStroke(0));
  require(bounds.getLy() < 40,
          "Long Trail retains an oversized historical bbox");
}

void checkSequenceSteps() {
  for (const std::string resource : {"cycle", "vector"}) {
    for (int step : {-1, 0, 1, 2}) {
      TVectorImageP image = makeImage(-1, step, 144, resource);
      TRaster32P raster   = render(image);
      std::vector<TAffine> transforms;
      TColorStyle *style = image->getPalette()->getStyle(1);
      if (resource == "vector")
        dynamic_cast<TVectorImagePatternStrokeStyle *>(style)
            ->computeTransformations(transforms, image->getStroke(0));
      else
        dynamic_cast<TRasterImagePatternStrokeStyle *>(style)
            ->computeTransformations(transforms, image->getStroke(0));
      require(transforms.size() > 2,
              "Sequence fixture contains too few stamps");
      for (size_t stamp = 0; stamp < transforms.size(); ++stamp) {
        const TPointD center =
            transforms[stamp] * TPointD(0, 0) + TPointD(64, 128);
        const TPixel32 pixel = raster->pixels(int(center.y))[int(center.x)];
        const int frame      = ((-1 + int(stamp) * step) % 3 + 3) % 3;
        const std::array<int, 3> channels{pixel.r, pixel.g, pixel.b};
        require(channels[frame] > 200,
                "Saved frame step changed the source sequence");
      }
    }
  }
}

void checkColorFunctions() {
  TVectorImageP image = makeImage(0, 0);
  TTranspFader low(0.25), high(0.75);
  TRaster32P first  = render(image, TRect(), &low);
  TRaster32P second = render(image, TRect(), &high);
  require(first->pixels(128)[64].m < second->pixels(128)[64].m,
          "Offscreen opacity reused a stale texture");
  TRaster32P repeated = render(image, TRect(), &low);
  require(first->pixels(128)[64] == repeated->pixels(128)[64],
          "Opacity changed on cache hit");
}

void checkArtworkSizingAndThumbnail() {
  for (const std::string resource : {"cycle", "tiny"}) {
    TVectorImageP image = makeImage(0, 0, 0.01, resource);
    for (double zoom : {0.125, 1.0, 4.0}) {
      TRaster32P raster = render(image, TRect(), nullptr, zoom);
      int visible       = 0;
      for (int y = 0; y < raster->getLy(); ++y)
        for (int x = 0; x < raster->getLx(); ++x)
          if (raster->pixels(y)[x].m) ++visible;
      require(visible > 0,
              "Artwork disappeared when rendered at thumbnail scale");
      if (zoom == 1.0)
        require(visible > 16,
                "Transparent source padding still determines stamp size");
    }
  }
}

}  // namespace

int main(int argc, char **argv) try {
  QApplication application(argc, argv);
  QTemporaryDir directory;
  require(directory.isValid(), "Could not create render test directory");
  TEnv::setArgPathValue(TEnv::getRootVarName(), directory.path().toStdString());
  Tiio::defineReaderMaker("png", Tiio::makePngReader);
  TFileType::declare("png", TFileType::RASTER_IMAGE);
  TFileType::declare("pli", TFileType::VECTOR_LEVEL);
  TLevelReader::define("pli", TLevelReaderPli::create);
  writePatterns(directory.path());
  writeVectorPatterns(directory.path());
  checkVectorFixture(directory.path());
  TRasterImagePatternStrokeStyle::setRootDir(TFilePath(directory.path()));
  TVectorImagePatternStrokeStyle::setRootDir(TFilePath(directory.path()));
  checkFrameSelection();
  checkTiles();
  checkSequenceSteps();
  checkColorFunctions();
  checkArtworkSizingAndThumbnail();
} catch (const TException &error) {
  std::wcerr << error.getMessage() << '\n';
  return 1;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
