#include "toonzqt/fxsettings.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "tdoublekeyframe.h"
#include "tenv.h"
#include "tfxparam.h"
#include "trasterfx.h"
#include "tundo.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QThread>

#include <iostream>

class PreciseColorFx final : public TRasterFx {
  FX_DECLARATION(PreciseColorFx)
  TRasterFxPort m_input;

public:
  TPixelParamP color = new TPixelParam(TPixel32::Red);

  PreciseColorFx() {
    bindParam(this, "color", color);
    addInputPort("Source", m_input);
  }

  bool requiresFullPrecisionSerialization() const override { return true; }

  bool canHandle(const TRenderSettings&, double) override { return true; }
  std::string getPluginId() const override { return "TEST"; }

  bool doGetBBox(double, TRectD& box, const TRenderSettings&) override {
    box = TConsts::infiniteRectD;
    return true;
  }

  void doCompute(TTile& tile, double frame, const TRenderSettings&) override {
    TRaster32P raster = tile.getRaster();
    raster->fill(color->getValue(frame));
  }
};

FX_IDENTIFIER(PreciseColorFx, "pr7243PreciseColorFx")

namespace {
int unexpectedWarnings = 0;

void testMessageHandler(QtMsgType type, const QMessageLogContext&,
                        const QString& message) {
  // Existing nested-layout warnings and the offscreen backend's unsupported
  // size-hint propagation do not affect pixel assertions. Keep other warnings.
  if (type == QtWarningMsg &&
      (message == "This plugin does not support propagateSizeHints()" ||
       message == "QLayout: Attempting to add QLayout \"\" to ParamViewer "
                  "\"\", which already has a layout" ||
       message == "QLayout: Attempting to add QLayout \"\" to QWidget \"\", "
                  "which already has a layout"))
    return;
  if (type == QtWarningMsg || type == QtCriticalMsg) ++unexpectedWarnings;
  std::cerr << message.toStdString() << '\n';
}

bool waitForColor(SwatchViewer* viewer, QColor expected,
                  const char* operation) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 5000) {
    QApplication::processEvents(QEventLoop::AllEvents, 10);
    const QImage image = viewer->grab().toImage();
    if (!image.isNull() &&
        image.pixelColor(image.width() / 2, image.height() / 2) == expected) {
      const QString name = QString::fromLatin1(operation).replace(' ', '_');
      if (!image.save(QString(PR7243_TEST_OUTPUT) + "/swatch_" + name + ".png"))
        return false;
      return true;
    }
    QThread::msleep(1);
  }
  std::cerr << "FAIL: swatch color after " << operation << '\n';
  return false;
}

bool runSwatchTest() {
  TFxHandle fxHandle;
  TFrameHandle frameHandle;
  TSceneHandle sceneHandle;
  TFxP fx       = new PreciseColorFx;
  auto* colorFx = dynamic_cast<PreciseColorFx*>(fx.getPointer());
  colorFx->color->setValueD(0, TPixelD(1, 0, 0, 1));
  colorFx->color->setValueD(9, TPixelD(0, 0, 1, 1));
  fx->setNewIdentifier();
  fx->setFxId(L"pr7243PreciseColorFx1");
  fxHandle.setFx(fx.getPointer());
  frameHandle.setFrameIndex(9);

  FxSettings settings(nullptr, TPixel32::White, TPixel32::Black);
  settings.setFxHandle(&fxHandle);
  settings.setFrameHandle(&frameHandle);
  settings.setSceneHandle(&sceneHandle);
  settings.resize(400, 350);
  settings.show();
  QApplication::processEvents();
  auto* viewer     = settings.findChild<SwatchViewer*>();
  auto* colorField = settings.findChild<PixelParamField*>();
  if (!viewer || !colorField) {
    std::cerr
        << "FAIL: FX Settings did not create the swatch and color control\n";
    ParamField::setFxHandle(nullptr);
    return false;
  }
  viewer->parentWidget()->show();
  viewer->setEnable(true);
  bool passed = waitForColor(viewer, QColor(Qt::blue), "initial keyed value");
  auto* key   = colorField->findChild<ParamFieldKeyToggle*>();
  key->keyToggled();
  passed = waitForColor(viewer, QColor(Qt::red), "key deletion") && passed;
  passed = TUndoManager::manager()->undo() && passed;
  passed = waitForColor(viewer, QColor(Qt::blue), "undo") && passed;
  passed = TUndoManager::manager()->redo() && passed;
  passed = waitForColor(viewer, QColor(Qt::red), "redo") && passed;
  SwatchViewer::suspendRendering(true);
  settings.hide();
  TUndoManager::manager()->reset();
  ParamField::setFxHandle(nullptr);
  return passed;
}
}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  const auto previousHandler = qInstallMessageHandler(testMessageHandler);
  QTemporaryDir runtime;
  const QString profiles = runtime.path() + "/profiles";
  const QString layout   = "/layouts/fxs/pr7243PreciseColorFx.xml";
  if (!runtime.isValid() || !QDir().mkpath(profiles + "/layouts/fxs") ||
      !QFile::copy(QString(PR7243_TEST_PROFILES) + layout, profiles + layout)) {
    std::cerr << "FAIL: cannot prepare temporary color FX profile\n";
    return 1;
  }
  TEnv::setStuffDir(TFilePath(runtime.path().toStdString()));
  TEnv::setArgPathValue(TEnv::getRootVarName(), runtime.path().toStdString());
  TEnv::setArgPathValue(TEnv::getSystemVarPrefix() + "PROFILES",
                        profiles.toStdString());
  TThread::init();
  const bool passed = runSwatchTest();
  TThread::shutdown();
  qInstallMessageHandler(previousHandler);
  if (!passed || unexpectedWarnings) return 1;
  std::cout << "FX Settings rendered swatch checks passed\n";
  return 0;
}
