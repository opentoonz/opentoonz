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
#include <QElapsedTimer>
#include <QImage>
#include <QTemporaryDir>
#include <QThread>

#include <iostream>

class ChoiceColorFx final : public TRasterFx {
  FX_DECLARATION(ChoiceColorFx)
  TRasterFxPort m_input;

public:
  TDoubleParamP choice = new TDoubleParam(0.0);

  ChoiceColorFx() {
    choice->setUIChoices({"Red", "Blue"});
    bindParam(this, "choice", choice);
    addInputPort("Source", m_input);
  }

  bool canHandle(const TRenderSettings&, double) override { return true; }
  std::string getPluginId() const override { return "TEST"; }

  bool doGetBBox(double, TRectD& box, const TRenderSettings&) override {
    box = TConsts::infiniteRectD;
    return true;
  }

  void doCompute(TTile& tile, double frame, const TRenderSettings&) override {
    TRaster32P raster = tile.getRaster();
    raster->fill(choice->getValue(frame) == 0.0 ? TPixel32::Red
                                                : TPixel32::Blue);
  }
};

FX_IDENTIFIER(ChoiceColorFx, "pr7241ChoiceColorFx")

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
      if (!image.save(QString(PR7241_TEST_OUTPUT) + "/swatch_" + name + ".png"))
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
  TFxP fx       = new ChoiceColorFx;
  auto* colorFx = dynamic_cast<ChoiceColorFx*>(fx.getPointer());
  TDoubleKeyframe first(0, 0.0), last(9, 1.0);
  first.m_type = last.m_type = TDoubleKeyframe::Constant;
  colorFx->choice->setKeyframe(first);
  colorFx->choice->setKeyframe(last);
  fx->setNewIdentifier();
  fx->setFxId(L"pr7241ChoiceColorFx1");
  fxHandle.setFx(fx.getPointer());
  frameHandle.setFrameIndex(9);

  FxSettings settings(nullptr, TPixel32::White, TPixel32::Black);
  settings.setFxHandle(&fxHandle);
  settings.setFrameHandle(&frameHandle);
  settings.setSceneHandle(&sceneHandle);
  settings.resize(400, 350);
  settings.show();
  QApplication::processEvents();
  auto* viewer      = settings.findChild<SwatchViewer*>();
  auto* choiceField = settings.findChild<AnimatedChoiceParamField*>();
  if (!viewer || !choiceField) {
    std::cerr
        << "FAIL: FX Settings did not create the swatch and choice control\n";
    ParamField::setFxHandle(nullptr);
    return false;
  }
  viewer->parentWidget()->show();
  viewer->setEnable(true);
  bool passed = waitForColor(viewer, QColor(Qt::blue), "initial keyed value");
  auto* key   = choiceField->findChild<ParamFieldKeyToggle*>();
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
  TEnv::setStuffDir(TFilePath(runtime.path().toStdString()));
  TEnv::setArgPathValue(TEnv::getRootVarName(), runtime.path().toStdString());
  TEnv::setArgPathValue(TEnv::getSystemVarPrefix() + "PROFILES",
                        PR7241_TEST_PROFILES);
  TThread::init();
  const bool passed = runSwatchTest();
  TThread::shutdown();
  qInstallMessageHandler(previousHandler);
  if (!passed || unexpectedWarnings) return 1;
  std::cout << "FX Settings rendered swatch checks passed\n";
  return 0;
}
