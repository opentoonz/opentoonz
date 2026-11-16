#include "toonzqt/colorfield.h"
#include "toonzqt/paramfield.h"
#include "toonzqt/spectrumfield.h"
#include "tdoublekeyframe.h"
#include "tundo.h"

#include <QApplication>
#include <QLineEdit>
#include <QMouseEvent>

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
int failures = 0;
int checks   = 0;

void check(bool passed, const char *message) {
  ++checks;
  if (passed) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void require(bool passed, const char *message) {
  if (!passed) throw std::runtime_error(message);
}

void testMessages(QtMsgType type, const QMessageLogContext &,
                  const QString &message) {
  // The offscreen platform cannot propagate native window size hints.
  if (type == QtWarningMsg &&
      message == "This plugin does not support propagateSizeHints()")
    return;
  if (type == QtWarningMsg || type == QtCriticalMsg) ++failures;
  std::cerr << message.toStdString() << '\n';
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

bool equalStop(const TSpectrumParamP &a, const TSpectrumParamP &b, int stop) {
  if (!equalCurve(a->getPosition(stop), b->getPosition(stop))) return false;
  const auto first  = channels(a->getColor(stop));
  const auto second = channels(b->getColor(stop));
  for (int i = 0; i < 4; ++i) {
    if (!equalCurve(first[i], second[i])) return false;
  }
  return true;
}

bool equalSpectrum(const TSpectrumParamP &a, const TSpectrumParamP &b) {
  if (a->getKeyCount() != b->getKeyCount()) return false;
  for (int i = 0; i < a->getKeyCount(); ++i) {
    if (!equalStop(a, b, i)) return false;
  }
  return true;
}

TSpectrumParamP copySpectrum(const TSpectrumParamP &spectrum) {
  return TParamP(spectrum->clone());
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
  TSpectrumParamP actual = new TSpectrumParam;
  TSpectrumParamP preview;
  TFxHandle handle;
  std::unique_ptr<SpectrumParamField> field;
  int frame         = 0;
  int notifications = 0;

  explicit Fixture(Mode mode = Mode::Default, bool precise = true,
                   bool alias = false) {
    const TPixelD value(-0.25, 0.12345678901234566, 2.5, 0.8765432109876543);
    setDefaults(actual->getColor(0), value);
    setDefaults(actual->getColor(1), TPixelD(4, 0.987654321, -0.75, 0.54321));
    if (mode != Mode::Default) {
      for (int stop = 0; stop < 2; ++stop) {
        const auto curves = channels(actual->getColor(stop));
        const auto values = components(actual->getColor(stop)->getValueD(0));
        for (int i = 0; i < 4; ++i) {
          addKey(curves[i], 0, values[i] / 2);
          addKey(curves[i], 9, values[i]);
        }
      }
      frame = mode == Mode::Keyed ? 9 : 4;
      if (mode == Mode::Asymmetric)
        addKey(actual->getColor(0)->getRed(), 4, value.r);
    }
    preview = alias ? actual : copySpectrum(actual);
    ParamField::setFxHandle(&handle);
    field = std::make_unique<SpectrumParamField>(nullptr, "Ramp", actual);
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

  DVGui::SpectrumField *spectrum() {
    auto *widget = field->findChild<DVGui::SpectrumField *>();
    require(widget != nullptr, "spectrum widget missing");
    return widget;
  }

  DVGui::SpectrumBar *bar() {
    auto *widget = field->findChild<DVGui::SpectrumBar *>();
    require(widget != nullptr, "spectrum bar missing");
    return widget;
  }

  DVGui::IntLineEdit *byteEdit(int channel) {
    auto *color = field->findChild<DVGui::ColorField *>();
    require(color != nullptr, "color widget missing");
    const auto fields = color->findChildren<DVGui::ChannelField *>(
        QString(), Qt::FindDirectChildrenOnly);
    require(fields.size() == 4, "four RGBA byte controls required");
    auto *line = fields[channel]->findChild<DVGui::IntLineEdit *>();
    require(line != nullptr, "byte channel numeric entry missing");
    return line;
  }

  QLineEdit *preciseEdit(int channel) {
    const char *names[] = {"spectrumPreciseRed", "spectrumPreciseGreen",
                           "spectrumPreciseBlue", "spectrumPreciseAlpha"};
    auto *line          = field->findChild<QLineEdit *>(names[channel]);
    require(line != nullptr, "precise channel entry missing");
    return line;
  }

  void finish(QLineEdit *line, const QString &text) {
    line->setText(text);
    require(QMetaObject::invokeMethod(line, "editingFinished",
                                      Qt::DirectConnection),
            "cannot finish channel edit");
  }

  void focus(QLineEdit *line) {
    field->show();
    field->activateWindow();
    line->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    require(line->hasFocus(), "numeric entry must retain keyboard focus");
  }

  void toggle() {
    auto *key = field->findChild<ParamFieldKeyToggle *>();
    require(key != nullptr, "spectrum key toggle missing");
    key->keyToggled();
  }
};

void channelEdit(Mode mode, int channel, bool numeric, bool noOp) {
  Fixture fixture(mode);
  const auto beforeActual  = copySpectrum(fixture.actual);
  const auto beforePreview = copySpectrum(fixture.preview);
  const double value =
      numeric ? 0.321987654321987
              : ((fixture.byteEdit(channel)->getValue() + 37) % 256) / 255.0;
  QLineEdit *line =
      numeric ? fixture.preciseEdit(channel) : fixture.byteEdit(channel);
  const QString text = noOp      ? line->text()
                       : numeric ? QString::number(value, 'g', 17)
                                 : QString::number(int(value * 255));
  fixture.finish(line, text);
  if (noOp) {
    check(equalSpectrum(fixture.actual, beforeActual),
          "no-op preserves saved ramp exactly");
    check(equalSpectrum(fixture.preview, beforePreview),
          "no-op preserves preview exactly");
    check(TUndoManager::manager()->getHistoryCount() == 0,
          "no-op creates no history");
    check(fixture.notifications == 0, "no-op emits no preview refresh");
    return;
  }
  const bool commit = mode != Mode::Between;
  check(channels(fixture.preview->getColor(0))[channel]->getValue(
            fixture.frame) == value,
        "edited channel receives requested exact value");
  check(equalStop(fixture.actual, beforeActual, 1),
        "edit preserves other saved stop");
  check(equalStop(fixture.preview, beforePreview, 1),
        "edit preserves other preview stop");
  for (int i = 0; i < 4; ++i) {
    if (i == channel) continue;
    check(equalCurve(channels(fixture.actual->getColor(0))[i],
                     channels(beforeActual->getColor(0))[i]),
          "edit preserves untouched saved curve and metadata");
    check(equalCurve(channels(fixture.preview->getColor(0))[i],
                     channels(beforePreview->getColor(0))[i]),
          "edit preserves untouched preview curve and metadata");
  }
  const auto after     = copySpectrum(fixture.preview);
  const auto displayed = fixture.spectrum()->getSpectrum();
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, after),
        "refresh preserves pending preview curves");
  check(fixture.spectrum()->getSpectrum() == displayed,
        "refresh keeps the pending preview visible in the ramp widget");
  check(TUndoManager::manager()->getHistoryCount() == (commit ? 1 : 0),
        "only committed channel edit creates history");
  if (!commit) {
    check(equalSpectrum(fixture.actual, beforeActual),
          "between-key edit preserves saved ramp");
    return;
  }
  check(TUndoManager::manager()->undo(), "channel undo available");
  check(equalSpectrum(fixture.actual, beforeActual),
        "undo restores saved ramp curves");
  check(equalSpectrum(fixture.preview, beforeActual),
        "undo refreshes preview curves");
  check(TUndoManager::manager()->redo(), "channel redo available");
  check(equalSpectrum(fixture.actual, after), "redo restores saved edit");
  check(equalSpectrum(fixture.preview, after), "redo restores preview edit");
}

void channelsTest() {
  for (const Mode mode :
       {Mode::Default, Mode::Keyed, Mode::Between, Mode::Asymmetric})
    for (int channel = 0; channel < 4; ++channel)
      for (const bool numeric : {false, true}) {
        channelEdit(mode, channel, numeric, true);
        channelEdit(mode, channel, numeric, false);
      }
  Fixture fixture;
  const auto before = copySpectrum(fixture.actual);
  for (const QString &text : {QString("nan"), QString("inf"), QString("1e999"),
                              QString("invalid"), QString("")})
    fixture.finish(fixture.preciseEdit(0), text);
  for (const QString &text : {QString("-0.1"), QString("1.1")})
    fixture.finish(fixture.preciseEdit(3), text);
  check(equalSpectrum(fixture.actual, before),
        "invalid numeric input preserves saved ramp");
  check(TUndoManager::manager()->getHistoryCount() == 0,
        "invalid input creates no history");
  fixture.actual->getColor(0)->getBlue()->setDefaultValue(1e100);
  fixture.field->update(0);
  check(fixture.preciseEdit(2)->text().toDouble() == 1e100,
        "finite extreme RGB remains exact in the numeric editor");
  check(fixture.byteEdit(2)->getValue() == 255,
        "finite extreme RGB is clamped safely for the byte display");
  fixture.finish(fixture.byteEdit(0), "100");
  check(fixture.actual->getColor(0)->getBlue()->getValue(0) == 1e100,
        "byte red edit preserves finite extreme blue");
}

void mouse(DVGui::SpectrumBar *bar, QEvent::Type type, const QPoint &point) {
  const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
  const auto buttons =
      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
  QMouseEvent event(type, point, button, buttons, Qt::NoModifier);
  QApplication::sendEvent(bar, &event);
}

void selectionTest() {
  Fixture fixture;
  fixture.actual->getPosition(0)->setDefaultValue(0.3333333333333333);
  fixture.field->update(0);
  auto *bar = fixture.bar();
  bar->resize(221, bar->height());
  const auto before = copySpectrum(fixture.actual);
  const QPoint start(80, bar->height() / 2);
  mouse(bar, QEvent::MouseButtonPress, start);
  mouse(bar, QEvent::MouseButtonRelease, start);
  check(equalSpectrum(fixture.actual, before),
        "selection click cannot move a saved stop");
  check(equalSpectrum(fixture.preview, before),
        "selection click cannot move preview stop");
  check(TUndoManager::manager()->getHistoryCount() == 0,
        "selection click creates no history");
  mouse(bar, QEvent::MouseButtonPress, start);
  mouse(bar, QEvent::MouseMove, QPoint(110, start.y()));
  mouse(bar, QEvent::MouseButtonRelease, QPoint(110, start.y()));
  check(fixture.actual->getPosition(0)->getValue(0) == 0.5,
        "drag commits the requested stop position");
  check(equalCurve(fixture.actual->getColor(0)->getGreen(),
                   before->getColor(0)->getGreen()),
        "drag preserves precise color");
  check(TUndoManager::manager()->getHistoryCount() == 1,
        "drag creates one history item");
  check(TUndoManager::manager()->undo(), "stop drag undo available");
  check(equalSpectrum(fixture.actual, before),
        "stop drag undo restores exact position");
}

void shortAndOutsideDrags() {
  for (const bool outside : {false, true}) {
    Fixture fixture;
    fixture.actual->getPosition(0)->setDefaultValue(1.0 / 3.0);
    fixture.field->update(0);
    auto *bar = fixture.bar();
    bar->resize(221, bar->height());
    const auto before = copySpectrum(fixture.actual);
    const QPoint start(77, bar->height() / 2);
    const QPoint moved(outside ? 110 : 78, start.y());
    mouse(bar, QEvent::MouseButtonPress, start);
    mouse(bar, QEvent::MouseMove, moved);
    check(equalSpectrum(fixture.actual, before),
          "drag does not commit before release");
    mouse(bar, QEvent::MouseButtonRelease,
          outside ? QPoint(-2, start.y()) : moved);
    check(fixture.actual->getPosition(0)->getValue(0) == (outside ? 0.5 : 0.34),
          "one-pixel drag and outside release commit last valid position");
    check(TUndoManager::manager()->getHistoryCount() == 1,
          "one-pixel drag and outside release create one history item");
    check(TUndoManager::manager()->undo(), "bounded drag undo available");
    check(equalSpectrum(fixture.actual, before),
          "bounded drag undo restores exact ramp");
  }
}

void insertionTest() {
  for (bool alias : {false, true}) {
    Fixture fixture(Mode::Default, true, alias);
    setDefaults(fixture.actual->getColor(0),
                TPixelD(2, 0.12345678901234566, -2, 0.25));
    setDefaults(fixture.actual->getColor(1),
                TPixelD(4, 0.12345678901234566, 0, 0.75));
    fixture.field->update(0);
    const auto before = copySpectrum(fixture.actual);
    auto *bar         = fixture.bar();
    bar->resize(221, bar->height());
    bar->addKeyAt(110);
    check(fixture.actual->getKeyCount() == 3,
          "insertion adds exactly one saved stop");
    check(fixture.preview->getKeyCount() == 3,
          "insertion adds exactly one preview stop");
    const TPixelD expected(3, 0.12345678901234566, -1, 0.5);
    check(fixture.actual->getColor(2)->getValueD(0) == expected,
          "inserted midpoint samples double RGB and alpha");
    check(fixture.preview->getColor(2)->getValueD(0) == expected,
          "inserted preview stop keeps HDR and exact channels");
    const auto after = copySpectrum(fixture.actual);
    check(TUndoManager::manager()->undo(), "insertion undo available");
    check(equalSpectrum(fixture.actual, before),
          "insertion undo restores all saved curves");
    check(TUndoManager::manager()->redo(), "insertion redo available");
    check(equalSpectrum(fixture.actual, after),
          "insertion redo restores exact new stop");
    fixture.spectrum()->setCurrentKeyIndex(2);
    mouse(bar, QEvent::MouseButtonPress, QPoint(110, bar->height() / 2));
    mouse(bar, QEvent::MouseMove, QPoint(110, bar->height() + 20));
    mouse(bar, QEvent::MouseButtonRelease, QPoint(110, bar->height() + 20));
    check(equalSpectrum(fixture.actual, before),
          "dragging a stop outside removes it exactly once");
    check(equalSpectrum(fixture.preview, before),
          "stop removal updates preview structure");
    check(TUndoManager::manager()->undo(), "stop removal undo available");
    check(equalSpectrum(fixture.actual, after),
          "stop removal undo restores precise inserted color");
    check(TUndoManager::manager()->redo(), "stop removal redo available");
    check(equalSpectrum(fixture.actual, before),
          "stop removal redo restores saved ramp");
  }
}

void editPendingStops(Fixture& fixture) {
  fixture.spectrum()->setCurrentKeyIndex(0);
  fixture.finish(fixture.preciseEdit(0), "1.2345678901234567");
  const int last = fixture.preview->getKeyCount() - 1;
  fixture.spectrum()->setCurrentKeyIndex(last);
  fixture.finish(fixture.preciseEdit(1), "0.43219876543219876");
  auto* bar = fixture.bar();
  bar->resize(221, bar->height());
  const int y = bar->height() / 2;
  mouse(bar, QEvent::MouseButtonPress, QPoint(210, y));
  mouse(bar, QEvent::MouseMove, QPoint(190, y));
  mouse(bar, QEvent::MouseButtonRelease, QPoint(190, y));
  check(fixture.preview->getPosition(last)->getValue(fixture.frame) == 0.9,
        "position drag remains pending between keys");
  check(TUndoManager::manager()->getHistoryCount() == 0,
        "pending channel and position edits create no history");
}

void keyPendingPreview(Fixture& fixture, const TSpectrumParamP& expected) {
  auto* key = fixture.field->findChild<ParamFieldKeyToggle*>();
  require(key != nullptr, "spectrum key toggle missing");
  if (fixture.actual->hasKeyframes())
    check(key->getStatus() == ParamFieldKeyToggle::MODIFIED,
          "surviving pending values keep the modified key indicator");
  const auto saved = copySpectrum(fixture.actual);
  fixture.toggle();
  for (int stop = 0; stop < expected->getKeyCount(); ++stop) {
    check(fixture.actual->getColor(stop)->getValueD(fixture.frame) ==
              expected->getColor(stop)->getValueD(fixture.frame),
          "key toggle commits surviving pending channel values");
    check(fixture.actual->getPosition(stop)->getValue(fixture.frame) ==
              expected->getPosition(stop)->getValue(fixture.frame),
          "key toggle commits surviving pending positions");
  }
  const auto keyed = copySpectrum(fixture.actual);
  check(TUndoManager::manager()->undo(), "pending value key undo available");
  check(equalSpectrum(fixture.actual, saved),
        "key undo restores saved curves before pending values were committed");
  check(TUndoManager::manager()->redo(), "pending value key redo available");
  check(equalSpectrum(fixture.actual, keyed),
        "key redo restores committed pending values");
}

void pendingInsertionTest() {
  Fixture fixture(Mode::Between);
  const auto saved = copySpectrum(fixture.actual);
  editPendingStops(fixture);
  const auto pending  = copySpectrum(fixture.preview);
  const TPixelD a     = pending->getColor(0)->getValueD(fixture.frame);
  const TPixelD b     = pending->getColor(1)->getValueD(fixture.frame);
  const double weight = 0.5 / 0.9;
  const TPixelD expected(
      (1 - weight) * a.r + weight * b.r, (1 - weight) * a.g + weight * b.g,
      (1 - weight) * a.b + weight * b.b, (1 - weight) * a.m + weight * b.m);
  fixture.notifications = 0;
  fixture.bar()->addKeyAt(110);
  check(fixture.notifications > 0,
        "insertion notifies preview consumers of the new ramp");
  for (int stop = 0; stop < 2; ++stop) {
    check(equalStop(fixture.actual, saved, stop),
          "insertion leaves surviving saved curves uncommitted");
    check(equalStop(fixture.preview, pending, stop),
          "insertion preserves complete pending survivor curves");
  }
  check(fixture.actual->getColor(2)->getValueD(fixture.frame) == expected,
        "new saved stop samples pending exact color and position edits");
  check(fixture.preview->getColor(2)->getValueD(fixture.frame) == expected,
        "new preview stop agrees with its surviving pending neighbors");
  check(TUndoManager::manager()->getHistoryCount() == 1,
        "insertion creates only its structural history item");
  pending->addKey(0.5, TPixel32::Black);
  setDefaults(pending->getColor(2), expected);
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, pending),
        "ordinary refresh retains pending values after insertion");
  keyPendingPreview(fixture, pending);
}

void removeStop(Fixture& fixture, int index) {
  auto* bar = fixture.bar();
  bar->resize(221, bar->height());
  fixture.spectrum()->setCurrentKeyIndex(index);
  const int x = bar->getCurrentPos();
  mouse(bar, QEvent::MouseButtonPress, QPoint(x, bar->height() / 2));
  mouse(bar, QEvent::MouseMove, QPoint(x, bar->height() + 20));
  mouse(bar, QEvent::MouseButtonRelease, QPoint(x, bar->height() + 20));
}

void pendingRemovalTest() {
  for (const int index : {0, 1}) {
    Fixture fixture(Mode::Between);
    fixture.actual->insertKey(1, 0.5, TPixel32::Red);
    fixture.field->update(fixture.frame);
    const auto saved = copySpectrum(fixture.actual);
    editPendingStops(fixture);
    const auto pending = copySpectrum(fixture.preview);
    saved->removeKey(index);
    pending->removeKey(index);
    fixture.notifications = 0;
    removeStop(fixture, index);
    check(fixture.notifications > 0,
          "removal notifies preview consumers of the new ramp");
    check(equalSpectrum(fixture.actual, saved),
          "removal leaves surviving saved curves uncommitted");
    check(equalSpectrum(fixture.preview, pending),
          "removal preserves pending curves on survivors after index shifts");
    check(TUndoManager::manager()->getHistoryCount() == 1,
          "removal creates only its structural history item");
    fixture.field->update(fixture.frame);
    check(equalSpectrum(fixture.preview, pending),
          "ordinary refresh retains pending values after removal");
    keyPendingPreview(fixture, pending);
  }
}

void removeOnlyAnimatedStopTest() {
  Fixture fixture(Mode::Between);
  fixture.actual->getColor(1)->clearKeyframes();
  fixture.field->update(fixture.frame);
  fixture.spectrum()->setCurrentKeyIndex(1);
  fixture.finish(fixture.preciseEdit(0), "1.2345678901234567");
  const auto saved   = copySpectrum(fixture.actual);
  const auto pending = copySpectrum(fixture.preview);
  saved->removeKey(0);
  pending->removeKey(0);
  removeStop(fixture, 0);
  check(!fixture.actual->hasKeyframes(),
        "removing the only animated stop leaves a default-only saved ramp");
  check(equalSpectrum(fixture.actual, saved),
        "removing animation does not prematurely commit surviving edits");
  check(equalSpectrum(fixture.preview, pending),
        "pending edits survive removal of the only animated stop");
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, pending),
        "default-only refresh retains the surviving pending edit");
  keyPendingPreview(fixture, pending);
}

enum class TargetChange { Frame, Stop, Fx, Preview, FrameRefresh };

void pendingTarget(TargetChange change) {
  Fixture fixture(Mode::Keyed);
  auto *line = fixture.preciseEdit(0);
  fixture.focus(line);
  line->setText("0.9876123456789");
  const auto before = copySpectrum(fixture.actual);
  switch (change) {
  case TargetChange::Fx:
    fixture.actual = copySpectrum(fixture.actual);
    setDefaults(fixture.actual->getColor(0), TPixelD(0.8, 0.2, 0.4, 0.6));
    fixture.preview = copySpectrum(fixture.actual);
    fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
    break;
  case TargetChange::Stop:
    fixture.spectrum()->setCurrentKeyIndex(1);
    break;
  case TargetChange::Frame:
    fixture.frame = 0;
    fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
    break;
  case TargetChange::Preview:
    fixture.preview = copySpectrum(fixture.actual);
    fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
    break;
  case TargetChange::FrameRefresh:
    fixture.frame = 0;
    fixture.field->update(fixture.frame);
    break;
  }
  const auto target        = copySpectrum(fixture.actual);
  const auto previewTarget = copySpectrum(fixture.preview);
  require(
      QMetaObject::invokeMethod(line, "editingFinished", Qt::DirectConnection),
      "cannot finish stale numeric edit");
  check(equalSpectrum(fixture.actual, target),
        "pending text cannot edit the new target");
  check(equalSpectrum(fixture.preview, previewTarget),
        "pending text cannot edit new preview");
  if (change == TargetChange::Stop)
    check(equalStop(fixture.actual, before, 1),
          "selecting another stop leaves that stop exact");
}

void targetsTest() {
  for (TargetChange change :
       {TargetChange::Frame, TargetChange::Stop, TargetChange::Fx,
        TargetChange::Preview, TargetChange::FrameRefresh})
    pendingTarget(change);
}

void historyTest() {
  for (const bool deleteKey : {false, true}) {
    Fixture fixture(Mode::Keyed);
    const auto before = copySpectrum(fixture.actual);
    if (deleteKey)
      fixture.toggle();
    else
      fixture.finish(fixture.preciseEdit(0), "0.34567890123456789");
    const auto after = copySpectrum(fixture.actual);
    fixture.frame    = 4;
    fixture.preview  = copySpectrum(fixture.actual);
    fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
    fixture.notifications = 0;
    check(TUndoManager::manager()->undo(),
          "history undo after preview replacement available");
    check(equalSpectrum(fixture.actual, before),
          "undo restores saved complete curves");
    check(equalSpectrum(fixture.preview, before),
          "undo refreshes replacement preview between keys");
    check(fixture.notifications > 0,
          "undo requests repaint of the current preview");
    fixture.preview = copySpectrum(fixture.actual);
    fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
    fixture.notifications = 0;
    check(TUndoManager::manager()->redo(),
          "history redo after replacement available");
    check(equalSpectrum(fixture.actual, after),
          "redo restores saved complete curves");
    check(equalSpectrum(fixture.preview, after),
          "redo refreshes replacement preview between keys");
    check(fixture.notifications > 0,
          "redo requests repaint of current preview");
  }
}

void lifetimeTest() {
  Fixture fixture(Mode::Keyed);
  auto old        = fixture.actual;
  auto oldRed     = old->getColor(0)->getRed();
  fixture.actual  = copySpectrum(fixture.actual);
  fixture.preview = copySpectrum(fixture.actual);
  fixture.field->setParam(fixture.preview, fixture.actual, 4);
  fixture.frame         = 4;
  fixture.notifications = 0;
  oldRed->deleteKeyframe(9);
  fixture.field->update(4);
  check(fixture.notifications == 0, "rebind detaches old child observers");
  fixture.actual->getColor(0)->getGreen()->deleteKeyframe(9);
  fixture.field->update(4);
  check(equalSpectrum(fixture.actual, fixture.preview),
        "external child edit refreshes preview");
  fixture.frame = 9;
  fixture.field->update(9);
  fixture.finish(fixture.preciseEdit(0), "0.4567890123456789");
  check(TUndoManager::manager()->undo(), "lifetime setup undo available");
  fixture.actual->getColor(0)->getBlue()->deleteKeyframe(9);
  fixture.field->update(4);
  check(equalSpectrum(fixture.actual, fixture.preview),
        "observers follow children replaced by undo");
  fixture.field.reset();
  oldRed->setDefaultValue(0.123);
  fixture.actual->getColor(0)->getGreen()->deleteKeyframe(0);
  check(TUndoManager::manager()->redo(),
        "history remains safe after control destruction");
}

void structuralChangesTest() {
  Fixture fixture(Mode::Between);
  const auto removedColor = fixture.actual->getColor(1);
  auto removed            = removedColor->getGreen();
  fixture.actual->removeKey(1);
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, fixture.actual),
        "external removal refreshes the preview structure");
  fixture.actual->addKey(0.75, TPixel32::Red);
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, fixture.actual),
        "external insertion refreshes the preview structure");
  const auto replacement = copySpectrum(fixture.actual);
  replacement->getColor(0)->getBlue()->clearKeyframes();
  replacement->getColor(0)->getBlue()->setDefaultValue(1.2345678901234567);
  fixture.actual->copy(replacement.getPointer());
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, replacement),
        "external whole-ramp copy refreshes currently bound preview");
  fixture.notifications = 0;
  removed->setDefaultValue(0.777);
  fixture.field->update(fixture.frame);
  check(fixture.notifications == 0,
        "removed stop channel observer is detached");
  const auto oldMatte = fixture.actual->getColor(0)->getMatte();
  fixture.actual->getColor(0)->enableMatte(false);
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, fixture.actual),
        "replacing the matte channel refreshes preview");
  fixture.notifications = 0;
  oldMatte->setDefaultValue(0.777);
  fixture.field->update(fixture.frame);
  check(fixture.notifications == 0, "replaced matte observer is detached");
  fixture.actual->getColor(0)->getMatte()->setDefaultValue(0.654321987);
  fixture.field->update(fixture.frame);
  check(equalSpectrum(fixture.preview, fixture.actual),
        "new matte channel remains observed");
}

void legacyTest() {
  Fixture fixture(Mode::Default, false);
  fixture.finish(fixture.byteEdit(0), "200");
  check(fixture.actual->getColor(0)->getValue(0).r == 200,
        "ordinary spectrum retains byte color editing");
  check(fixture.field->findChild<QLineEdit *>("spectrumPreciseRed") == nullptr,
        "ordinary spectrum has no precise channel controls");
}
}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  const auto previousHandler = qInstallMessageHandler(testMessages);
  try {
    require(argc == 2, "expected one test case name");
    const std::string name(argv[1]);
    if (name == "channels")
      channelsTest();
    else if (name == "selection") {
      selectionTest();
      shortAndOutsideDrags();
    } else if (name == "insertion") {
      insertionTest();
      pendingInsertionTest();
      pendingRemovalTest();
      removeOnlyAnimatedStopTest();
    } else if (name == "targets")
      targetsTest();
    else if (name == "history")
      historyTest();
    else if (name == "lifetime") {
      lifetimeTest();
      structuralChangesTest();
    } else if (name == "legacy")
      legacyTest();
    else
      throw std::runtime_error("unknown test case: " + name);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    ++failures;
  }
  qInstallMessageHandler(previousHandler);
  std::cout << checks << " spectrum checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
