#include "toonzqt/doublefield.h"
#include "toonzqt/paramfield.h"
#include "tdoublekeyframe.h"
#include "tundo.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLayout>
#include <QSlider>

#include <iostream>
#include <memory>

namespace {
int failures = 0;

void check(bool passed, const char* message) {
  if (passed) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void sliderEndpoint(double initial, double endpoint) {
  DVGui::DoubleField field(nullptr);
  field.setRange(-1000.0, 1000.0);
  check(field.setSliderRange(-10.0, 10.0), "set optional slider window");
  field.setValue(initial);
  field.layout()->setSizeConstraint(QLayout::SetNoConstraint);
  field.show();
  QApplication::processEvents();

  double committed  = initial;
  int notifications = 0;
  QObject::connect(&field, &DVGui::DoubleValueField::valueChanged,
                   [&](bool dragging) {
                     check(!dragging, "typed endpoint is a committed edit");
                     committed = field.getValue();
                     ++notifications;
                   });
  auto* edit = field.findChild<DVGui::DoubleLineEdit*>();
  check(edit != nullptr, "numeric entry exists");
  if (!edit) return;
  edit->setText(QString::number(endpoint));
  QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
  check(committed == endpoint, "typed endpoint reaches parameter consumer");
  check(notifications == 1, "typed endpoint produces one commit notification");
}

struct ChoiceFixture {
  TDoubleParamP actual = new TDoubleParam(0.0);
  TDoubleParamP preview;
  TFxHandle handle;
  std::unique_ptr<AnimatedChoiceParamField> field;
  int frame                = 9;
  int previewNotifications = 0;

  explicit ChoiceFixture(bool checkbox = false) {
    actual->setUIChoices({"Off", "On"}, checkbox);
    TDoubleKeyframe first(0, 0.0), last(9, 1.0);
    first.m_type = last.m_type = TDoubleKeyframe::Constant;
    actual->setKeyframe(first);
    actual->setKeyframe(last);
    preview = TParamP(actual->clone());
    ParamField::setFxHandle(&handle);
    field =
        std::make_unique<AnimatedChoiceParamField>(nullptr, "Choice", actual);
    field->setParam(preview, actual, frame);
    QObject::connect(&handle, &TFxHandle::fxChanged, field.get(),
                     [this] { field->setParam(preview, actual, frame); });
    QObject::connect(field.get(), &ParamField::currentParamChanged, field.get(),
                     [this] { ++previewNotifications; });
  }

  ~ChoiceFixture() {
    TUndoManager::manager()->reset();
    ParamField::setFxHandle(nullptr);
  }

  void toggleKey() {
    auto* toggle = field->findChild<ParamFieldKeyToggle*>();
    check(toggle != nullptr, "choice key button exists");
    if (toggle) toggle->keyToggled();
  }

  void verify(double expected, ParamFieldKeyToggle::Status status) {
    check(preview->getValue(frame) == expected, "preview has expected value");
    auto* combo = field->findChild<QComboBox*>("animatedChoice");
    auto* box   = field->findChild<QCheckBox*>("animatedCheckbox");
    check(combo ? combo->currentIndex() == expected
                : box->isChecked() == (expected != 0),
          "choice control agrees with preview");
    auto* toggle = field->findChild<ParamFieldKeyToggle*>();
    check(toggle->getStatus() == status, "key indicator has expected state");
  }
};

void deleteAndRestoreChoice(bool checkbox) {
  ChoiceFixture fixture(checkbox);
  fixture.toggleKey();
  check(!fixture.actual->isKeyframe(9), "actual key was deleted");
  check(!fixture.preview->isKeyframe(9),
        "deleted key removed from preview curve");
  fixture.verify(0, ParamFieldKeyToggle::NOT_KEYFRAME);

  check(TUndoManager::manager()->undo(), "key deletion can be undone");
  check(fixture.actual->isKeyframe(9), "undo restores actual key");
  fixture.verify(1, ParamFieldKeyToggle::KEYFRAME);
  check(TUndoManager::manager()->redo(), "key deletion can be redone");
  check(!fixture.preview->isKeyframe(9), "redo removes preview key");
  fixture.verify(0, ParamFieldKeyToggle::NOT_KEYFRAME);
}

void historyAfterPreviewReplacement() {
  ChoiceFixture fixture;
  fixture.toggleKey();
  fixture.preview = TParamP(fixture.actual->clone());
  fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
  fixture.previewNotifications = 0;
  QObject::connect(
      fixture.field.get(), &ParamField::currentParamChanged,
      fixture.field.get(), [&fixture] {
        check(fixture.preview->getValue(9) == fixture.actual->getValue(9),
              "history signal observes synchronized value");
        check(fixture.preview->getKeyframeCount() ==
                  fixture.actual->getKeyframeCount(),
              "history signal observes synchronized curve");
      });
  check(TUndoManager::manager()->undo(), "undo after preview replacement");
  fixture.verify(1, ParamFieldKeyToggle::KEYFRAME);
  check(TUndoManager::manager()->redo(), "redo after preview replacement");
  fixture.verify(0, ParamFieldKeyToggle::NOT_KEYFRAME);
  check(fixture.previewNotifications >= 2,
        "history refresh notifies preview consumers");
}

void preserveUncommittedChoice(bool checkbox) {
  ChoiceFixture fixture(checkbox);
  fixture.frame = 4;
  fixture.field->update(fixture.frame);
  if (checkbox) {
    fixture.field->findChild<QCheckBox*>("animatedCheckbox")->click();
  } else {
    auto* combo = fixture.field->findChild<QComboBox*>("animatedChoice");
    combo->setCurrentIndex(1);
    QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection,
                              Q_ARG(int, 1));
  }
  fixture.field->update(fixture.frame);
  check(fixture.actual->getValue(4) == 0,
        "between-key edit leaves actual value unchanged");
  check(!fixture.actual->isKeyframe(4), "between-key edit does not save a key");
  fixture.verify(1, ParamFieldKeyToggle::MODIFIED);
  check(TUndoManager::manager()->getHistoryCount() == 0,
        "preview edit adds no undo item");
  fixture.toggleKey();
  check(fixture.actual->getValue(4) == 1, "key toggle commits preview choice");
  fixture.verify(1, ParamFieldKeyToggle::KEYFRAME);
}

void externalCurveEdit() {
  ChoiceFixture fixture;
  fixture.actual->deleteKeyframe(9);
  fixture.field->update(9);
  check(!fixture.preview->isKeyframe(9),
        "external deletion updates complete preview curve");
  fixture.verify(0, ParamFieldKeyToggle::NOT_KEYFRAME);
  check(fixture.previewNotifications == 1,
        "external edit notifies preview consumer");
}

void rebindActualCurve() {
  ChoiceFixture fixture;
  const TDoubleParamP oldActual = fixture.actual;
  fixture.actual                = TParamP(oldActual->clone());
  fixture.preview               = TParamP(fixture.actual->clone());
  fixture.field->setParam(fixture.preview, fixture.actual, fixture.frame);
  oldActual->deleteKeyframe(9);
  fixture.field->update(9);
  fixture.verify(1, ParamFieldKeyToggle::KEYFRAME);
  check(fixture.previewNotifications == 0,
        "old actual no longer invalidates preview");
  fixture.actual->deleteKeyframe(9);
  fixture.field->update(9);
  fixture.verify(0, ParamFieldKeyToggle::NOT_KEYFRAME);
  check(fixture.previewNotifications == 1,
        "replacement actual invalidates preview");
  fixture.field.reset();
  fixture.actual->setValue(9, 1.0);
}
}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  sliderEndpoint(50.0, 10.0);
  sliderEndpoint(-50.0, -10.0);
  deleteAndRestoreChoice(false);
  deleteAndRestoreChoice(true);
  historyAfterPreviewReplacement();
  preserveUncommittedChoice(false);
  preserveUncommittedChoice(true);
  externalCurveEdit();
  rebindActualCurve();
  if (failures) {
    std::cerr << failures << " failed checks\n";
    return 1;
  }
  std::cout << "All parameter field regression checks passed\n";
  return 0;
}
