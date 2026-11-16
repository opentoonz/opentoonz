#include "toonz/fxcommand.h"
#include "toonz/txsheethandle.h"
#include "toonzqt/paramfield.h"
#include "toonzqt/spectrumfield.h"
#include "tdoublekeyframe.h"
#include "tfxparam.h"
#include "trasterfx.h"
#include "tundo.h"

#include <QApplication>
#include <QLineEdit>
#include <QMouseEvent>

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>

class ResetSpectrumFx final : public TRasterFx {
  FX_DECLARATION(ResetSpectrumFx)

public:
  TSpectrumParamP ramp = new TSpectrumParam;

  ResetSpectrumFx() { bindParam(this, "ramp", ramp); }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool canHandle(const TRenderSettings&, double) override { return true; }
  std::string getPluginId() const override { return "TEST"; }
  bool doGetBBox(double, TRectD&, const TRenderSettings&) override {
    return false;
  }
  void doCompute(TTile&, double, const TRenderSettings&) override {}
};

FX_IDENTIFIER(ResetSpectrumFx, "pr7244ResetSpectrumFx")

namespace {
int checks          = 0;
int failures        = 0;
constexpr int frame = 4;

void check(bool passed, const char* message) {
  ++checks;
  if (passed) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

void require(bool passed, const char* message) {
  if (!passed) throw std::runtime_error(message);
}

void testMessages(QtMsgType type, const QMessageLogContext&,
                  const QString& message) {
  if (type == QtWarningMsg || type == QtCriticalMsg) ++failures;
  std::cerr << message.toStdString() << '\n';
}

std::array<TDoubleParamP, 5> curves(const TSpectrumParamP& ramp, int stop) {
  const auto color = ramp->getColor(stop);
  return {ramp->getPosition(stop), color->getRed(), color->getGreen(),
          color->getBlue(), color->getMatte()};
}

bool equalKey(const TDoubleKeyframe& a, const TDoubleKeyframe& b) {
  return std::tie(a.m_frame, a.m_value, a.m_type, a.m_prevType, a.m_speedIn,
                  a.m_speedOut, a.m_step, a.m_linkedHandles) ==
         std::tie(b.m_frame, b.m_value, b.m_type, b.m_prevType, b.m_speedIn,
                  b.m_speedOut, b.m_step, b.m_linkedHandles);
}

void checkRamp(const TSpectrumParamP& actual, const TSpectrumParamP& expected) {
  check(actual->getKeyCount() == expected->getKeyCount(),
        "history restores stop count");
  if (actual->getKeyCount() != expected->getKeyCount()) return;
  for (int stop = 0; stop < actual->getKeyCount(); ++stop) {
    const auto values = curves(actual, stop);
    const auto saved  = curves(expected, stop);
    for (int channel = 0; channel < 5; ++channel) {
      check(values[channel]->getDefaultValue() ==
                saved[channel]->getDefaultValue(),
            "history restores channel default");
      check(values[channel]->getValue(frame) == saved[channel]->getValue(frame),
            "history restores live channel at reset frame");
      const int count = values[channel]->getKeyframeCount();
      check(count == saved[channel]->getKeyframeCount(),
            "history restores key count");
      if (count != saved[channel]->getKeyframeCount()) continue;
      for (int key = 0; key < count; ++key)
        check(equalKey(values[channel]->getKeyframe(key),
                       saved[channel]->getKeyframe(key)),
              "history restores animation values and interpolation");
    }
  }
}

TSpectrumParamP snapshot(const TSpectrumParamP& ramp) {
  return TParamP(ramp->clone());
}

struct Fixture {
  TFxP fx                = new ResetSpectrumFx;
  TFxP defaults          = new ResetSpectrumFx;
  TSpectrumParamP actual = static_cast<ResetSpectrumFx*>(fx.getPointer())->ramp;
  TSpectrumParamP preview;
  std::array<TPixelParamP, 2> colors;
  std::array<std::array<TDoubleParamP, 5>, 2> original;
  TFxHandle handle;
  TXsheetHandle xsheet;
  std::unique_ptr<SpectrumParamField> field;

  explicit Fixture(bool alias) {
    for (int stop = 0; stop < 2; ++stop) {
      colors[stop]   = actual->getColor(stop);
      original[stop] = curves(actual, stop);
      for (int channel = 0; channel < 5; ++channel) {
        const double value = 0.12345678901234566 + 0.1 * channel + 0.2 * stop;
        original[stop][channel]->setDefaultValue(value);
        for (int time : {0, 9}) {
          TDoubleKeyframe key(time, value + 0.01 * time);
          key.m_type          = TDoubleKeyframe::SpeedInOut;
          key.m_speedIn       = TPointD(-2, -0.03);
          key.m_speedOut      = TPointD(2, 0.03);
          key.m_linkedHandles = false;
          original[stop][channel]->setKeyframe(key);
        }
      }
    }
    preview = alias ? actual : snapshot(actual);
    ParamField::setFxHandle(&handle);
    field = std::make_unique<SpectrumParamField>(nullptr, "Ramp", actual);
    field->setProperty("preserveColorPrecision", true);
    field->setParam(preview, actual, frame);
    QObject::connect(&handle, &TFxHandle::fxChanged, field.get(),
                     [this] { field->setParam(preview, actual, frame); });
  }

  ~Fixture() {
    TUndoManager::manager()->reset();
    ParamField::setFxHandle(nullptr);
  }

  void checkIdentity() {
    require(actual->getKeyCount() == 2, "expected original stop count");
    for (int stop = 0; stop < 2; ++stop) {
      check(actual->getColor(stop) == colors[stop],
            "history retains original color object");
      check(curves(actual, stop) == original[stop],
            "history retains original channel objects");
    }
  }
};

enum class Edit { Numeric, Insert, Remove, DeleteKey };

void mouse(DVGui::SpectrumBar* bar, QEvent::Type type, const QPoint& position) {
  const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
  const auto buttons =
      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
  QMouseEvent event(type, position, button, buttons, Qt::NoModifier);
  QApplication::sendEvent(bar, &event);
}

void edit(Fixture& fixture, Edit operation) {
  auto* bar = fixture.field->findChild<DVGui::SpectrumBar*>();
  require(bar != nullptr, "spectrum bar missing");
  bar->resize(221, bar->height());
  switch (operation) {
  case Edit::Numeric: {
    auto* line = fixture.field->findChild<QLineEdit*>("spectrumPreciseRed");
    require(line != nullptr, "precise red editor missing");
    line->setText("2.987654321012345");
    require(QMetaObject::invokeMethod(line, "editingFinished",
                                      Qt::DirectConnection),
            "cannot finish numeric edit");
    break;
  }
  case Edit::Insert:
    bar->addKeyAt(110);
    require(fixture.actual->getKeyCount() == 3, "insertion did not add a stop");
    break;
  case Edit::Remove: {
    const QPoint position(11, bar->height() / 2);
    const QPoint outside(11, bar->height() + 20);
    mouse(bar, QEvent::MouseButtonPress, position);
    mouse(bar, QEvent::MouseMove, outside);
    mouse(bar, QEvent::MouseButtonRelease, outside);
    require(fixture.actual->getKeyCount() == 1,
            "removal did not delete an original stop");
    break;
  }
  case Edit::DeleteKey: {
    auto* toggle = fixture.field->findChild<ParamFieldKeyToggle*>();
    require(toggle != nullptr, "key toggle missing");
    toggle->keyToggled();
    require(!fixture.actual->isKeyframe(frame),
            "key toggle did not delete reset keys");
    break;
  }
  }
}

void resetHistoryTest(bool alias, Edit operation) {
  Fixture fixture(alias);
  const auto before = snapshot(fixture.actual);
  TFxCommand::resetFxFrame(fixture.fx.getPointer(),
                           fixture.defaults.getPointer(), frame,
                           &fixture.xsheet, &fixture.handle);
  auto* history = TUndoManager::manager();
  require(history->getHistoryCount() == 1,
          "Shift+Reset must create an undo command");
  check(fixture.actual->getColor(0)->getRed()->getValue(frame) == 0,
        "Shift+Reset applies factory channel value");
  const auto reset = snapshot(fixture.actual);
  edit(fixture, operation);
  require(history->getHistoryCount() == 2,
          "ramp edit must create a second command");
  const auto after = snapshot(fixture.actual);
  for (int cycle = 0; cycle < 2; ++cycle) {
    require(history->undo(), "ramp undo unavailable");
    checkRamp(fixture.actual, reset);
    fixture.checkIdentity();
    require(history->undo(), "Shift+Reset undo unavailable");
    checkRamp(fixture.actual, before);
    checkRamp(fixture.preview, before);
    fixture.checkIdentity();
    require(history->redo(), "Shift+Reset redo unavailable");
    checkRamp(fixture.actual, reset);
    require(history->redo(), "ramp redo unavailable");
    checkRamp(fixture.actual, after);
    checkRamp(fixture.preview, after);
  }
}
}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  const auto previousHandler = qInstallMessageHandler(testMessages);
  try {
    for (bool alias : {false, true})
      for (Edit operation :
           {Edit::Numeric, Edit::Insert, Edit::Remove, Edit::DeleteKey})
        resetHistoryTest(alias, operation);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    ++failures;
  }
  qInstallMessageHandler(previousHandler);
  std::cout << checks << " reset history checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
