#include "celcolorkeyfx.cpp"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

struct KeyFixture {
  TRasterFP source{4, 1}, output{4, 1};
  std::array<CelKey::Entry, colorCapacity> entries;
  CelKey::Settings settings;

  KeyFixture() {
    source->pixels(0)[0]   = TPixelF(0.5f, 0, 0, 0.5f);
    source->pixels(0)[1]   = TPixelF(0, 0.75f, 0, 0.75f);
    source->pixels(0)[2]   = TPixelF(0, 0, 0.25f, 0.25f);
    source->pixels(0)[3]   = TPixelF::Transparent;
    entries[0].enabled     = true;
    entries[0].color       = {{1, 0, 0}};
    entries[0].replace     = true;
    entries[0].replacement = {{2.5, -0.125, 0.25}};
  }

  bool render(bool keep, bool replace, const int* canceled = nullptr) {
    output->clear();
    return applyKey<TPixelF>(source, output, 0, 0, entries, 1, settings, keep,
                             replace, 0, CelDistance::Metric::Euclidean, 0,
                             CelDistance::Metric::Euclidean,
                             CelDistance::Direction::Around, canceled);
  }
};

void replacementTest() {
  KeyFixture fixture;
  require(fixture.render(true, true), "replacement did not complete");
  const auto color = fixture.output->pixels(0)[0];
  require(color.r == 1.25f && color.g == -0.0625f && color.b == 0.125f &&
              color.m == 0.5f,
          "replacement lost HDR values or source alpha");
  fixture.entries[0].replacement = {{1e100, -1e100, 0.25}};
  require(fixture.render(true, true), "extreme replacement did not complete");
  const auto extreme = fixture.output->pixels(0)[0];
  const float limit  = std::numeric_limits<float>::max();
  require(extreme.r == limit && extreme.g == -limit && extreme.b == 0.125f &&
              extreme.m == 0.5f,
          "finite precise replacement overflowed the float raster");
  fixture.entries[0].replacement[0] = std::numeric_limits<double>::infinity();
  require(fixture.render(true, true), "invalid replacement did not complete");
  require(fixture.output->pixels(0)[0] == TPixelF::Transparent,
          "nonfinite replacement contaminated the output");
}

void selectionTest() {
  KeyFixture fixture;
  require(fixture.render(false, false), "remove-color render did not complete");
  require(fixture.output->pixels(0)[0] == TPixelF::Transparent,
          "selected color was not removed");
  require(fixture.output->pixels(0)[1] == fixture.source->pixels(0)[1],
          "removing a color changed an unselected pixel");
  require(fixture.render(true, false), "keep-color render did not complete");
  require(fixture.output->pixels(0)[0] == fixture.source->pixels(0)[0],
          "keeping a color changed its alpha or RGB");
  require(fixture.output->pixels(0)[1] == TPixelF::Transparent,
          "keep-color render retained an unselected pixel");
  int canceled = 1;
  require(!fixture.render(true, false, &canceled),
          "canceled filter reported completion");
}

void saveRaster(const TRasterFP& raster, const QString& path) {
  QImage image(raster->getLx(), raster->getLy(),
               QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < raster->getLy(); ++y) {
    for (int x = 0; x < raster->getLx(); ++x) {
      const TPixel32 pixel = toPixel32(toPixelD(raster->pixels(y)[x]));
      image.setPixel(x, y, qRgba(pixel.r, pixel.g, pixel.b, pixel.m));
    }
  }
  require(image.save(path), "could not save rendering evidence");
}

void saveEvidence(const QString& directory) {
  require(QDir().mkpath(directory), "could not create evidence directory");
  KeyFixture fixture;
  fixture.source = TRasterFP(192, 128);
  fixture.output = TRasterFP(192, 128);
  fixture.source->clear();
  for (int y = 16; y < 112; ++y) {
    for (int x = 16; x < 176; ++x) {
      auto& pixel = fixture.source->pixels(y)[x];
      if ((x - 48) * (x - 48) + (y - 64) * (y - 64) < 900)
        pixel = TPixelF(1, 0, 0, 1);
      else if (x >= 88 && x < 128 && y >= 32 && y < 96)
        pixel = TPixelF(0, 1, 0, 1);
      else if (x >= 140 && std::abs(y - 64) < (x - 140) * 1.2)
        pixel = TPixelF(0, 0, 1, 1);
    }
  }
  require(fixture.render(false, false), "evidence rendering did not complete");
  saveRaster(fixture.source,
             QDir(directory).filePath("cel-color-key-before.png"));
  saveRaster(fixture.output,
             QDir(directory).filePath("cel-color-key-after.png"));
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  try {
    selectionTest();
    replacementTest();
    if (argc == 2) saveEvidence(QString::fromLocal8Bit(argv[1]));
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  std::cout << "Cel Color Key raster selection and replacement checks passed\n";
}
