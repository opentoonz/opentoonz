#include "toonzqt/colorfield.h"
#include "toonzqt/paramfield.h"
#include "tdoublekeyframe.h"
#include "tundo.h"

#include <QApplication>

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace {
int failures = 0;

void check(bool passed, const char *message) {
  if (passed) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void require(bool passed, const char *message) {
  if (!passed) throw std::runtime_error(message);
}

std::array<TDoubleParamP, 4> channels(const TPixelParamP &pixel) {
  return {pixel->getRed(), pixel->getGreen(), pixel->getBlue(),
          pixel->getMatte()};
}

std::array<double, 4> components(const TPixelD &pixel) {
  return {pixel.r, pixel.g, pixel.b, pixel.m};
}

void setDefaults(const TPixelParamP &pixel, const TPixelD &value) {
  const auto curves = channels(pixel);
  const auto values = components(value);
  for (int i = 0; i < 4; ++i) curves[i]->setDefaultValue(values[i]);
}

bool equalKey(const TDoubleKeyframe &a, const TDoubleKeyframe &b) {
  const bool values =
      std::tie(a.m_frame, a.m_value, a.m_isKeyframe, a.m_step) ==
      std::tie(b.m_frame, b.m_value, b.m_isKeyframe, b.m_step);
  const bool interpolation = std::tie(a.m_type, a.m_prevType, a.m_speedIn,
                                      a.m_speedOut, a.m_linkedHandles) ==
                             std::tie(b.m_type, b.m_prevType, b.m_speedIn,
                                      b.m_speedOut, b.m_linkedHandles);
  const bool expression = std::tie(a.m_expressionText, a.m_fileParams.m_path,
                                   a.m_fileParams.m_fieldIndex, a.m_unitName,
                                   a.m_similarShapeOffset) ==
                          std::tie(b.m_expressionText, b.m_fileParams.m_path,
                                   b.m_fileParams.m_fieldIndex, b.m_unitName,
                                   b.m_similarShapeOffset);
  return values && interpolation && expression;
}

bool equalCurve(const TDoubleParamP &a, const TDoubleParamP &b) {
  if (a->getDefaultValue() != b->getDefaultValue() ||
      a->getKeyframeCount() != b->getKeyframeCount())
    return false;
  for (int i = 0; i < a->getKeyframeCount(); ++i) {
    if (!equalKey(a->getKeyframe(i), b->getKeyframe(i))) return false;
  }
  return true;
}

bool equalPixel(const TPixelParamP &a, const TPixelParamP &b) {
  const auto first = channels(a), second = channels(b);
  for (int i = 0; i < 4; ++i) {
    if (!equalCurve(first[i], second[i])) return false;
  }
  return true;
}

TPixelParamP copyPixel(const TPixelParamP &pixel) {
  return TParamP(pixel->clone());
}

void addKey(const TDoubleParamP &curve, int frame, double value) {
  TDoubleKeyframe key(frame, value);
  key.m_type          = TDoubleKeyframe::SpeedInOut;
  key.m_speedIn       = TPointD(-2, -0.03);
  key.m_speedOut      = TPointD(2, 0.03);
  key.m_linkedHandles = false;
  key.m_step          = 2;
  curve->setKeyframe(key);
}

enum class Mode { Default, Keyed, Between, Asymmetric };

struct Fixture {
  TPixelParamP actual = new TPixelParam;
  TPixelParamP preview;
  TFxHandle handle;
  std::unique_ptr<PixelParamField> field;
  int frame         = 0;
  int notifications = 0;

  Fixture(Mode mode, const TPixelD &value, bool precise = true,
          bool matte = true) {
    setDefaults(actual, value);
    if (!matte) actual->enableMatte(false);
    if (mode != Mode::Default) {
      const auto curves = channels(actual);
      const auto values = components(value);
      for (int i = 0; i < 4; ++i) {
        addKey(curves[i], 0, values[i] / 2);
        addKey(curves[i], 9, values[i]);
      }
      frame = mode == Mode::Keyed ? 9 : 4;
      if (mode == Mode::Asymmetric) addKey(actual->getRed(), 4, value.r);
    }
    preview = copyPixel(actual);
    ParamField::setFxHandle(&handle);
    field = std::make_unique<PixelParamField>(nullptr, "Color", actual);
    field->setProperty("preserveColorPrecision", precise);
    field->setParam(preview, actual, frame);
    QObject::connect(&handle, &TFxHandle::fxChanged, field.get(),
                     [this] { field->setParam(preview, actual, frame); });
    QObject::connect(field.get(), &ParamField::currentParamChanged, field.get(),
                     [this] { ++notifications; });
  }

  ~Fixture() {
    TUndoManager::manager()->reset();
    ParamField::setFxHandle(nullptr);
  }

  DVGui::IntLineEdit *edit(int channel) {
    auto *color = field->findChild<DVGui::ColorField *>();
    require(color != nullptr, "color widget missing");
    const auto fields = color->findChildren<DVGui::ChannelField *>(
        QString(), Qt::FindDirectChildrenOnly);
    require(fields.size() == 4, "four RGBA channel controls required");
    auto *line = fields[channel]->findChild<DVGui::IntLineEdit *>();
    require(line != nullptr, "channel numeric entry missing");
    return line;
  }

  void finishEdit(int channel, int value) {
    auto *line = edit(channel);
    line->setText(QString::number(value));
    require(QMetaObject::invokeMethod(line, "editingFinished",
                                      Qt::DirectConnection),
            "cannot finish channel edit");
  }

  void toggle() {
    auto *key = field->findChild<ParamFieldKeyToggle *>();
    require(key != nullptr, "color key toggle missing");
    key->keyToggled();
  }

  void status(ParamFieldKeyToggle::Status expected) {
    check(field->findChild<ParamFieldKeyToggle *>()->getStatus() == expected,
          "key indicator reflects complete preview curve");
    check(field->getColor() == preview->getValue(frame),
          "displayed color matches current preview");
  }
};

void noOp(Mode mode, const TPixelD &value, int channel) {
  Fixture fixture(mode, value);
  const auto actual  = copyPixel(fixture.actual),
             preview = copyPixel(fixture.preview);
  fixture.finishEdit(channel, fixture.edit(channel)->getValue());
  check(equalPixel(fixture.actual, actual), "no-op leaves actual curves exact");
  check(equalPixel(fixture.preview, preview),
        "no-op leaves preview curves exact");
  check(TUndoManager::manager()->getHistoryCount() == 0,
        "no-op creates no undo item");
  check(fixture.notifications == 0, "no-op emits no preview refresh");
}

void channelEdit(Mode mode, const TPixelD &value, int channel) {
  Fixture fixture(mode, value);
  const auto actualBefore  = copyPixel(fixture.actual);
  const auto previewBefore = copyPixel(fixture.preview);
  const int typed          = (fixture.edit(channel)->getValue() + 37) % 256;
  const bool commit        = mode != Mode::Between;
  fixture.finishEdit(channel, typed);
  check(channels(fixture.preview)[channel]->getValue(fixture.frame) ==
            typed / 255.0,
        "edited channel receives requested byte value");
  for (int i = 0; i < 4; ++i) {
    if (i == channel) continue;
    check(equalCurve(channels(fixture.actual)[i], channels(actualBefore)[i]),
          "edit preserves untouched actual channel curve exactly");
    check(equalCurve(channels(fixture.preview)[i], channels(previewBefore)[i]),
          "edit preserves untouched preview channel curve exactly");
  }
  const auto after = copyPixel(fixture.preview);
  fixture.field->update(fixture.frame);
  check(equalPixel(fixture.preview, after),
        "ordinary refresh preserves edited preview");
  fixture.status(commit ? (mode == Mode::Default
                               ? ParamFieldKeyToggle::NOT_ANIMATED
                               : ParamFieldKeyToggle::KEYFRAME)
                        : ParamFieldKeyToggle::MODIFIED);
  check(TUndoManager::manager()->getHistoryCount() == (commit ? 1 : 0),
        "only committed channel edits create undo items");
  if (!commit) {
    check(equalPixel(fixture.actual, actualBefore),
          "between-key edit leaves actual curves exact");
    return;
  }
  check(TUndoManager::manager()->undo(), "channel edit undo available");
  check(equalPixel(fixture.actual, actualBefore),
        "undo restores complete actual curves");
  check(equalPixel(fixture.preview, actualBefore),
        "undo restores complete preview curves");
  check(TUndoManager::manager()->redo(), "channel edit redo available");
  check(equalPixel(fixture.actual, after),
        "redo restores complete actual edit curves");
  check(equalPixel(fixture.preview, after),
        "redo restores complete preview edit curves");
}

void deleteHistory(bool replacePreview) {
  Fixture fixture(Mode::Keyed, TPixelD(0.7, 0.12345678901234566, 0.9, 0.85));
  const auto before       = copyPixel(fixture.actual);
  int synchronizedSignals = 0;
  QObject::connect(
      fixture.field.get(), &ParamField::currentParamChanged,
      fixture.field.get(), [&] {
        check(equalPixel(fixture.preview, fixture.actual),
              "refresh signal observes synchronized complete curves");
        ++synchronizedSignals;
      });
  fixture.toggle();
  const auto after = copyPixel(fixture.actual);
  check(!fixture.preview->isKeyframe(9),
        "deletion removes current preview key");
  check(equalPixel(fixture.preview, after),
        "deletion synchronizes full preview curves");
  fixture.status(ParamFieldKeyToggle::NOT_KEYFRAME);
  if (replacePreview) {
    fixture.preview = copyPixel(fixture.actual);
    fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
  }
  check(TUndoManager::manager()->undo(), "key deletion undo available");
  check(equalPixel(fixture.actual, before),
        "undo restores exact actual key curves");
  check(equalPixel(fixture.preview, before),
        "undo restores exact preview key curves");
  fixture.status(ParamFieldKeyToggle::KEYFRAME);
  check(TUndoManager::manager()->redo(), "key deletion redo available");
  check(equalPixel(fixture.actual, after),
        "redo restores exact actual deletion curves");
  check(equalPixel(fixture.preview, after),
        "redo restores exact preview deletion curves");
  fixture.status(ParamFieldKeyToggle::NOT_KEYFRAME);
  check(synchronizedSignals >= 3,
        "deletion undo and redo refresh preview consumers");
}

void externalEditAndLifetime() {
  Fixture fixture(Mode::Keyed, TPixelD(0.7, 0.12345678901234566, 0.9, 0.85));
  fixture.actual->getGreen()->deleteKeyframe(9);
  fixture.field->update(9);
  check(equalPixel(fixture.preview, fixture.actual),
        "external child deletion refreshes preview");
  const auto old  = fixture.actual;
  fixture.actual  = copyPixel(old);
  fixture.preview = copyPixel(fixture.actual);
  fixture.field->setParam(fixture.preview, fixture.actual, 9);
  fixture.notifications = 0;
  old->getRed()->deleteKeyframe(9);
  fixture.field->update(9);
  check(fixture.notifications == 0,
        "old actual child observers detach on rebind");
  fixture.actual->getRed()->deleteKeyframe(9);
  fixture.field->update(9);
  check(equalPixel(fixture.preview, fixture.actual),
        "new actual child observer refreshes preview");
  check(fixture.notifications > 0,
        "external actual change refreshes preview consumers");
  fixture.toggle();
  fixture.field.reset();
  for (auto &curve : channels(fixture.actual)) curve->deleteKeyframe(0);
  for (auto &curve : channels(old)) curve->deleteKeyframe(0);
  check(TUndoManager::manager()->undo(),
        "history remains safe after field destruction");
  check(TUndoManager::manager()->redo(),
        "redo remains safe after field destruction");
}

void legacyControl() {
  Fixture fixture(Mode::Default, TPixelD(0.4, 0.2, 0.6, 1), false);
  fixture.finishEdit(0, 200);
  check(fixture.actual->getValue(0) == TPixel32(200, 51, 153, 255),
        "non-opted-in control retains ordinary byte color edit");
  check(TUndoManager::manager()->getHistoryCount() == 1,
        "legacy edit creates normal undo");
}

void hiddenAlphaAndObserverSwap() {
  {
    Fixture fixture(Mode::Default,
                    TPixelD(-0.25, 0.12345678901234566, 1.25, 0.85), true,
                    false);
    const auto before = copyPixel(fixture.actual);
    fixture.finishEdit(0, 100);
    check(equalCurve(fixture.actual->getMatte(), before->getMatte()),
          "hidden alpha actual curve survives RGB edit exactly");
    check(equalCurve(fixture.preview->getMatte(), before->getMatte()),
          "hidden alpha preview curve survives RGB edit exactly");
  }
  Fixture fixture(Mode::Default, TPixelD(0.7, 0.12345678901234566, 0.9, 0.85));
  const auto oldMatte = fixture.actual->getMatte();
  fixture.actual->enableMatte(false);
  fixture.field->setParam(fixture.preview, fixture.actual, 0);
  fixture.notifications = 0;
  oldMatte->setDefaultValue(0.987654321);
  fixture.field->update(0);
  check(fixture.notifications == 0, "replaced matte curve observer detaches");
  fixture.actual->getMatte()->setDefaultValue(0.654321987);
  fixture.field->update(0);
  check(equalPixel(fixture.preview, fixture.actual),
        "replacement matte curve refreshes preview");
  check(fixture.notifications > 0,
        "replacement matte curve notifies preview consumers");
  fixture.field.reset();
  oldMatte->setDefaultValue(0.3);
  fixture.actual->getMatte()->setDefaultValue(0.6);
}
}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    const TPixelD values[] = {
        TPixelD(0.43210987654321, 0.12345678901234566, 0.7654321, 0.8765432),
        TPixelD(-0.25, 0.12345678901234566, 1.25, 0.8765432)};
    for (const auto &value : values) {
      for (Mode mode :
           {Mode::Default, Mode::Keyed, Mode::Between, Mode::Asymmetric}) {
        for (int channel = 0; channel < 4; ++channel) {
          noOp(mode, value, channel);
          channelEdit(mode, value, channel);
        }
      }
    }
    deleteHistory(false);
    deleteHistory(true);
    externalEditAndLifetime();
    hiddenAlphaAndObserverSwap();
    legacyControl();
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  if (failures) {
    std::cerr << failures << " failed pixel parameter checks\n";
    return 1;
  }
  std::cout << "Pixel parameter regression checks passed\n";
  return 0;
}
