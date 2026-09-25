#include "image/pli/pli_io.h"
#include "image/pli/tiio_pli.h"
#include "tpalette.h"
#include "tvectorimage.h"
#include "tenv.h"

#include <QTemporaryDir>
#include <QCoreApplication>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

TStroke::OutlineOptions readOptions(const TFilePath &path) {
  ParsedPli parsed(path);
  TPalette *palette        = nullptr;
  TContentHistory *history = nullptr;
  parsed.loadInfo(false, palette, history);
  ImageTag *image = parsed.loadFrame(TFrameId(1));
  require(image && image->m_numObjects == 1, "PLI fixture has no outline tag");
  auto *tag = dynamic_cast<StrokeOutlineOptionsTag *>(image->m_object[0]);
  require(tag != nullptr, "PLI fixture object is not outline options");
  return tag->m_options;
}

void writeOptions(const TFilePath &path,
                  const TStroke::OutlineOptions &options) {
  ParsedPli parsed(1, 2, 10, 1.0);
  auto objects = std::make_unique<PliObjectTag *[]>(1);
  objects[0]   = new StrokeOutlineOptionsTag(options);
  parsed.addTag(new ImageTag(TFrameId(1), 1, std::move(objects)));
  require(parsed.writePli(path), "Could not write PLI fixture");
}

void checkRoundTrips(const TFilePath &path) {
  for (int offset : {0, 2, -1, std::numeric_limits<int>::max(), -2147483647}) {
    for (int step : {1, -1, 0, 123456789, -123456789}) {
      for (int cap = 0; cap < 3; ++cap) {
        for (int join = 0; join < 3; ++join) {
          TStroke::OutlineOptions options(cap, join, 0, 4, offset, step);
          writeOptions(path, options);
          const auto result = readOptions(path);
          require(result.m_patternFrameOffset == offset &&
                      result.m_patternFrameStep == step,
                  "PLI changed Trail frame data");
          require(result.m_capStyle == cap && result.m_joinStyle == join &&
                      result.m_miterLower == 0 && result.m_miterUpper == 4,
                  "PLI changed outline data");
        }
      }
    }
  }
}

std::vector<unsigned char> readBytes(const TFilePath &path) {
  std::ifstream input(path.getQString().toStdString(), std::ios::binary);
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(input), {});
}

void replacePayload(const TFilePath &path, int length) {
  auto bytes              = readBytes(path);
  const unsigned char tag = PliTag::OUTLINE_OPTIONS_GOBJ | 0x40;
  const auto position     = std::find(bytes.begin(), bytes.end(), tag);
  require(position != bytes.end(), "Outline header missing from fixture");
  const size_t offset      = size_t(position - bytes.begin());
  const int previousLength = bytes[offset + 1];
  require(previousLength == 18, "Unexpected fixture payload width");
  if (length < previousLength)
    bytes.erase(bytes.begin() + offset + 2 + length,
                bytes.begin() + offset + 2 + previousLength);
  else
    bytes.insert(bytes.begin() + offset + 2 + previousLength,
                 length - previousLength, 0xab);
  bytes[offset + 1] = static_cast<unsigned char>(length);
  std::ofstream output(path.getQString().toStdString(), std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

void checkExtensions(const TFilePath &path) {
  const TStroke::OutlineOptions options(0, 0, 0, 4, 40000, -1);
  for (int length : {10, 11, 14, 17, 18, 21}) {
    writeOptions(path, options);
    replacePayload(path, length);
    const auto result   = readOptions(path);
    const bool extended = length >= 18;
    require(result.m_patternFrameOffset == (extended ? 40000 : 0) &&
                result.m_patternFrameStep == (extended ? -1 : 1),
            "Partial or trailing PLI extension was misread");
  }
  writeOptions(path, options);
  replacePayload(path, 1);
  bool rejected = false;
  try {
    readOptions(path);
  } catch (const TException &) {
    rejected = true;
  }
  require(rejected, "Truncated base outline payload was accepted");
}

void checkLevelRoundTrip(const TFilePath &path) {
  TVectorImageP image(new TVectorImage);
  TPaletteP palette = new TPalette;
  image->setPalette(palette.getPointer());
  auto *stroke =
      new TStroke(std::vector<TThickPoint>{{0, 0, 5}, {50, 0, 5}, {100, 0, 5}});
  stroke->setStyle(1);
  stroke->outlineOptions().m_patternFrameOffset = 2;
  stroke->outlineOptions().m_patternFrameStep   = 0;
  image->addStroke(stroke, false);
  {
    TLevelWriterPli writer(path, nullptr);
    writer.getFrameWriter(TFrameId(1))->save(image);
  }
  TLevelReaderPli reader(path);
  reader.loadInfo();
  TVectorImageP loaded = reader.getFrameReader(TFrameId(1))->load();
  require(loaded && loaded->getStrokeCount() == 1,
          "Level round trip lost stroke");
  const auto &options = loaded->getStroke(0)->outlineOptions();
  require(options.m_patternFrameOffset == 2 && options.m_patternFrameStep == 0,
          "Level round trip lost Repeat state");
}

void checkInvalidOptions(const TFilePath &path) {
  for (int field = 0; field < 4; ++field) {
    TStroke::OutlineOptions options;
    if (field == 0)
      options.m_patternFrameOffset = std::numeric_limits<int>::min();
    if (field == 1)
      options.m_patternFrameStep = std::numeric_limits<int>::min();
    if (field == 2)
      options.m_miterLower = std::numeric_limits<double>::infinity();
    if (field == 3) options.m_miterUpper = 1e20;
    bool rejected = false;
    try {
      writeOptions(path, options);
    } catch (const TException &) {
      rejected = true;
    }
    require(rejected, "Unrepresentable PLI outline value was accepted");
  }
  const TStroke::OutlineOptions options(0, 0, 0.125, 3000.5, 0, 1);
  writeOptions(path, options);
  const auto result = readOptions(path);
  require(result.m_miterLower == 0.125 && result.m_miterUpper == 3000.5,
          "PLI changed fractional or large miter limits");
}

void checkStrokeOperations() {
  TStroke original(
      std::vector<TThickPoint>{{0, 0, 5}, {50, 0, 5}, {100, 0, 5}});
  original.outlineOptions().m_patternFrameOffset = -2;
  original.outlineOptions().m_patternFrameStep   = 0;
  TStroke copy(original), first, second;
  original.split(0.5, first, second);
  copy.transform(TScale(2.0));
  for (const TStroke *stroke : {&copy, &first, &second}) {
    require(stroke->outlineOptions().m_patternFrameOffset == -2 &&
                stroke->outlineOptions().m_patternFrameStep == 0,
            "Copy, split or transform lost cycle metadata");
  }
}

}  // namespace

int main(int argc, char **argv) try {
  QCoreApplication application(argc, argv);
  QTemporaryDir directory;
  require(directory.isValid(), "Could not create PLI test directory");
  TEnv::setArgPathValue(TEnv::getRootVarName(), directory.path().toStdString());
  const TFilePath path(directory.path() + "/trail.pli");
  checkRoundTrips(path);
  checkExtensions(path);
  checkInvalidOptions(path);
  checkStrokeOperations();
  checkLevelRoundTrip(TFilePath(directory.path() + "/level.pli"));
} catch (const TException &error) {
  std::wcerr << error.getMessage() << '\n';
  return 1;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
