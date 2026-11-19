#include "toonzqt/fxsettings.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "tdoublekeyframe.h"
#include "tenv.h"
#include "tfxparam.h"
#include "tfxutil.h"
#include "trasterfx.h"
#include "tmacrofx.h"
#include "tpixelutils.h"
#include "tspectrumparam.h"
#include "tundo.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QThread>

#include <iostream>
#include <stdexcept>
#include <string>

class PreciseSpectrumFx final : public TRasterFx {
  FX_DECLARATION(PreciseSpectrumFx)
  TRasterFxPort m_input;

public:
  TSpectrumParamP ramp = new TSpectrumParam;
  bool precise         = true;

  PreciseSpectrumFx() {
    bindParam(this, "ramp", ramp);
    addInputPort("Source", m_input);
  }

  bool requiresFullPrecisionSerialization() const override { return precise; }
  bool canHandle(const TRenderSettings &, double) override { return true; }
  std::string getPluginId() const override { return "TEST"; }

  bool doGetBBox(double, TRectD &box, const TRenderSettings &) override {
    box = TConsts::infiniteRectD;
    return true;
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override {
    TRaster32P raster = tile.getRaster();
    raster->fill(ramp->getColor(0)->getValue(frame));
  }
};

FX_IDENTIFIER(PreciseSpectrumFx, "pr7244PreciseSpectrumFx")

namespace {
int failures = 0;
int checks   = 0;

void check(bool passed, const char *message) {
  ++checks;
  if (passed) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

void require(bool passed, const char *message) {
  if (!passed) throw std::runtime_error(message);
}

void testMessageHandler(QtMsgType type, const QMessageLogContext &,
                        const QString &message) {
  // Pre-existing nested-layout and offscreen backend warnings do not affect
  // rendering assertions. Any other warning fails the regression test.
  if (type == QtWarningMsg &&
      (message == "This plugin does not support propagateSizeHints()" ||
       message == "QLayout: Attempting to add QLayout \"\" to ParamViewer "
                  "\"\", which already has a layout" ||
       message == "QLayout: Attempting to add QLayout \"\" to QWidget \"\", "
                  "which already has a layout"))
    return;
  if (type == QtWarningMsg || type == QtCriticalMsg) ++failures;
  std::cerr << message.toStdString() << '\n';
}

void setDefaults(const TPixelParamP &pixel, const TPixelD &value) {
  pixel->getRed()->setDefaultValue(value.r);
  pixel->getGreen()->setDefaultValue(value.g);
  pixel->getBlue()->setDefaultValue(value.b);
  pixel->getMatte()->setDefaultValue(value.m);
}

void seedTest(bool huge) {
  TFxP src     = new PreciseSpectrumFx;
  TFxP dst     = new PreciseSpectrumFx;
  auto *source = dynamic_cast<PreciseSpectrumFx *>(src.getPointer());
  auto *target = dynamic_cast<PreciseSpectrumFx *>(dst.getPointer());
  const TPixelD value(huge ? 1e100 : 2.5, 0.12345678901234566, -0.25,
                      0.8765432109876543);
  setDefaults(source->ramp->getColor(0), value);
  source->ramp->getPosition(0)->setDefaultValue(0.12345678901234566);
  TFxUtil::setKeyframe(dst, 4, src, 0);
  check(target->ramp->getColor(0)->getValueD(4) == value,
        "FX preview seeding preserves exact HDR channels");
  check(target->ramp->getPosition(0)->getValue(4) == 0.12345678901234566,
        "FX preview seeding preserves precise stop position");
  if (huge) return;
  source->precise = target->precise = false;
  TFxUtil::setKeyframe(dst, 8, src, 0);
  check(target->ramp->getColor(0)->getValue(8) ==
            source->ramp->getColor(0)->getValue(0),
        "ordinary spectrum seeding preserves established byte result");
}

struct MacroFixture {
  TFxP leaf     = new PreciseSpectrumFx;
  TFxP ordinary = new PreciseSpectrumFx;
  TFxP macro;

  explicit MacroFixture(bool nested) {
    leaf->setFxId(L"preciseLeaf");
    ordinary->setFxId(L"ordinaryRoot");
    dynamic_cast<PreciseSpectrumFx *>(ordinary.getPointer())->precise = false;
    require(ordinary->connect("Source", leaf.getPointer()),
            "macro source connection");
    macro = TMacroFx::create({leaf, ordinary});
    require(bool(macro), "create macro containing precise and ordinary FX");
    auto *inner = dynamic_cast<TMacroFx *>(macro.getPointer());
    inner->setParamExposed(leaf.getPointer(), "ramp", true);
    if (!nested) return;
    macro->setFxId(L"innerMacro");
    TFxP outer = new PreciseSpectrumFx;
    outer->setFxId(L"outerRoot");
    dynamic_cast<PreciseSpectrumFx *>(outer.getPointer())->precise = false;
    require(outer->connect("Source", macro.getPointer()),
            "nested macro connection");
    macro = TMacroFx::create({macro, outer});
    require(bool(macro), "create nested macro");
  }

  TSpectrumParamP ramp(bool precise) {
    const auto fx = precise ? leaf : ordinary;
    return dynamic_cast<PreciseSpectrumFx *>(fx.getPointer())->ramp;
  }
};

void macroSeedTest() {
  for (const bool nested : {false, true}) {
    MacroFixture source(nested), target(nested);
    const TPixelD exact(2.5, 0.12345678901234566, -0.25, 0.8765432109876543);
    setDefaults(source.ramp(true)->getColor(0), exact);
    setDefaults(source.ramp(false)->getColor(0), exact);
    TFxUtil::setKeyframe(target.macro, 4, source.macro, 0);
    check(target.ramp(true)->getColor(0)->getValueD(4) == exact,
          "macro preview seeding retains precise member channels");
    const auto byteValue =
        toPixelD(source.ramp(false)->getColor(0)->getValue(0));
    check(target.ramp(false)->getColor(0)->getValueD(4) == byteValue,
          "macro preview seeding retains ordinary member quantization");
    setDefaults(source.ramp(true)->getColor(0), TPixelD(1e100, 0.2, 0.3, 1));
    TFxUtil::setKeyframe(target.macro, 8, source.macro, 0);
    check(target.ramp(true)->getColor(0)->getValueD(8).r == 1e100,
          "macro preview seeding never converts extreme precise RGB to an "
          "integer");
  }
}

QColor displayColor(const TPixel32 &color) {
  return QColor(color.r, color.g, color.b, color.m);
}

void waitForColor(SwatchViewer *viewer, const QColor &expected,
                  const char *operation) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 5000) {
    QApplication::processEvents(QEventLoop::AllEvents, 10);
    const QImage image = viewer->grab().toImage();
    if (!image.isNull() &&
        image.pixelColor(image.width() / 2, image.height() / 2) == expected) {
      const QString name = QString::fromLatin1(operation).replace(' ', '_');
      check(image.save(QString(PR7244_TEST_OUTPUT) + "/spectrum_" + name +
                       ".png"),
            "rendered swatch evidence saved");
      return;
    }
    QThread::msleep(1);
  }
  std::cerr << "Swatch failed after " << operation << '\n';
  check(false, "painted swatch matches current saved ramp");
}

void runSwatchTest() {
  TFxHandle fxHandle;
  TFrameHandle frameHandle;
  TSceneHandle sceneHandle;
  TFxP fx          = new PreciseSpectrumFx;
  auto *spectrumFx = dynamic_cast<PreciseSpectrumFx *>(fx.getPointer());
  for (int stop = 0; stop < 2; ++stop) {
    spectrumFx->ramp->getColor(stop)->setValueD(0, TPixelD(1, 0, 0, 1));
    spectrumFx->ramp->getColor(stop)->setValueD(9, TPixelD(0, 0, 1, 1));
  }
  fx->setNewIdentifier();
  fx->setFxId(L"pr7244PreciseSpectrumFx1");
  fxHandle.setFx(fx.getPointer());
  frameHandle.setFrameIndex(9);
  FxSettings settings(nullptr, TPixel32::White, TPixel32::Black);
  settings.setFxHandle(&fxHandle);
  settings.setFrameHandle(&frameHandle);
  settings.setSceneHandle(&sceneHandle);
  settings.resize(600, 350);
  settings.show();
  QApplication::processEvents();
  auto *viewer = settings.findChild<SwatchViewer *>();
  auto *field  = settings.findChild<SpectrumParamField *>();
  require(viewer && field, "FX Settings must create ramp control and swatch");
  viewer->parentWidget()->show();
  viewer->setEnable(true);
  waitForColor(viewer, Qt::blue, "initial keyed value");
  auto *line = field->findChild<QLineEdit *>("spectrumPreciseRed");
  require(line != nullptr, "FX Settings must opt in to precise ramp fields");
  line->setText("0.5");
  require(
      QMetaObject::invokeMethod(line, "editingFinished", Qt::DirectConnection),
      "cannot finish precise channel edit");
  waitForColor(viewer, displayColor(spectrumFx->ramp->getColor(0)->getValue(9)),
               "precise edit");
  frameHandle.setFrameIndex(4);
  QApplication::processEvents();
  waitForColor(viewer, displayColor(spectrumFx->ramp->getColor(0)->getValue(4)),
               "frame changed");
  check(TUndoManager::manager()->undo(),
        "undo after frame-driven preview replacement");
  waitForColor(viewer, displayColor(spectrumFx->ramp->getColor(0)->getValue(4)),
               "undo after rebind");
  check(TUndoManager::manager()->redo(),
        "redo after frame-driven preview replacement");
  waitForColor(viewer, displayColor(spectrumFx->ramp->getColor(0)->getValue(4)),
               "redo after rebind");
  frameHandle.setFrameIndex(9);
  QApplication::processEvents();
  auto *key = field->findChild<ParamFieldKeyToggle *>();
  require(key != nullptr, "spectrum key toggle must exist");
  key->keyToggled();
  waitForColor(viewer, Qt::red, "key deletion");
  check(TUndoManager::manager()->undo(), "key deletion undo");
  waitForColor(viewer, displayColor(spectrumFx->ramp->getColor(0)->getValue(9)),
               "key deletion undo");
  check(TUndoManager::manager()->redo(), "key deletion redo");
  waitForColor(viewer, Qt::red, "key deletion redo");
  SwatchViewer::suspendRendering(true);
  settings.hide();
  TUndoManager::manager()->reset();
  ParamField::setFxHandle(nullptr);
}
}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  const auto previousHandler = qInstallMessageHandler(testMessageHandler);
  QTemporaryDir runtime;
  const QString profiles = runtime.path() + "/profiles";
  const QString layout   = "/layouts/fxs/pr7244PreciseSpectrumFx.xml";
  if (!runtime.isValid() || !QDir().mkpath(profiles + "/layouts/fxs") ||
      !QFile::copy(QString(PR7244_TEST_PROFILES) + layout, profiles + layout)) {
    std::cerr << "FAIL: cannot prepare temporary spectrum FX profile\n";
    return 1;
  }
  TEnv::setStuffDir(TFilePath(runtime.path().toStdString()));
  TEnv::setArgPathValue(TEnv::getRootVarName(), runtime.path().toStdString());
  TEnv::setArgPathValue(TEnv::getSystemVarPrefix() + "PROFILES",
                        profiles.toStdString());
  TThread::init();
  try {
    require(argc == 2, "expected one test case name");
    const std::string name(argv[1]);
    if (name == "seeding")
      seedTest(false);
    else if (name == "huge")
      seedTest(true);
    else if (name == "macro")
      macroSeedTest();
    else if (name == "swatch")
      runSwatchTest();
    else
      throw std::runtime_error("unknown test case: " + name);
  } catch (const std::exception &error) {
    ++failures;
    std::cerr << "FAIL: " << error.what() << '\n';
  }
  TThread::shutdown();
  qInstallMessageHandler(previousHandler);
  std::cout << checks << " spectrum integration checks, " << failures
            << " failures\n";
  return failures ? 1 : 0;
}
