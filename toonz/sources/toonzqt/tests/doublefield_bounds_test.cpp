#include "toonzqt/doublefield.h"
#include "toonzqt/paramfield.h"
#include "tundo.h"

#include <QApplication>
#include <QSlider>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

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

bool inside(double value, double minimum, double maximum) {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

void enterValue(DVGui::DoubleValueField &field, double value) {
  auto *edit = field.findChild<DVGui::DoubleValueLineEdit *>();
  require(edit != nullptr, "numeric entry missing");
  edit->setText(QString::number(value, 'g', 9));
  require(
      QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection),
      "cannot finish numeric entry");
}

struct Bounds {
  double minimum, maximum, sliderMinimum, sliderMaximum;
  int precision = 3;
};

struct MeasuredFixture {
  TDoubleParamP actual;
  TDoubleParamP preview;
  TFxHandle handle;
  std::unique_ptr<MeasuredDoubleParamField> field;
  DVGui::MeasuredDoubleField *numeric;
  QSlider *slider;

  MeasuredFixture(double value, const Bounds &bounds)
      : actual(new TDoubleParam(value)) {
    actual->setValueRange(bounds.minimum, bounds.maximum);
    actual->setUISliderRange(bounds.sliderMinimum, bounds.sliderMaximum);
    preview = TParamP(actual->clone());
    ParamField::setFxHandle(&handle);
    field =
        std::make_unique<MeasuredDoubleParamField>(nullptr, "Bounds", actual);
    numeric = field->findChild<DVGui::MeasuredDoubleField *>();
    slider  = field->findChild<QSlider *>();
    require(numeric && slider,
            "measured field did not create numeric entry and slider");
    check(numeric->getValue() == value,
          "constructor preserves initial parameter value");
    field->setParam(preview, actual, 0);
    if (bounds.precision != 3) field->setPrecision(bounds.precision);
  }

  ~MeasuredFixture() {
    TUndoManager::manager()->reset();
    ParamField::setFxHandle(nullptr);
  }

  void verifyValue(double expected) {
    check(numeric->getValue() == expected, "numeric field retains full value");
    check(actual->getValue(0) == expected,
          "actual parameter receives full value");
    check(preview->getValue(0) == expected,
          "preview parameter receives full value");
  }

  void sweep(const Bounds &bounds) {
    require(slider->isEnabled(), "representable slider is disabled");
    const int minimum = slider->minimum(), maximum = slider->maximum();
    require(static_cast<long long>(maximum) - minimum < 5000,
            "test slider unexpectedly has too many positions");
    slider->setValue(maximum);
    for (int position = minimum; position <= maximum; ++position) {
      slider->setValue(position);
      require(QMetaObject::invokeMethod(slider, "sliderReleased",
                                        Qt::DirectConnection),
              "cannot release measured slider");
      check(inside(actual->getValue(0), bounds.minimum, bounds.maximum),
            "slider commit stays inside actual hard bounds");
      check(inside(preview->getValue(0), bounds.minimum, bounds.maximum),
            "slider commit stays inside preview hard bounds");
      check(inside(actual->getValue(0), bounds.sliderMinimum,
                   bounds.sliderMaximum),
            "slider commit stays inside requested optional bounds");
    }
  }
};

void wideMeasured(bool negative) {
  const double sign = negative ? -1.0 : 1.0;
  const Bounds bounds =
      negative ? Bounds{-1e7, 0, -10, 0} : Bounds{0, 1e7, 0, 10};
  MeasuredFixture fixture(sign * 7e6, bounds);
  fixture.verifyValue(sign * 7e6);
  double minimum, maximum;
  fixture.numeric->getRange(minimum, maximum);
  check(minimum == bounds.minimum && maximum == bounds.maximum,
        "full numeric bounds survive optional slider setup");
  check(fixture.numeric->getSliderRange(minimum, maximum),
        "wide measured parameter applies optional slider range");
  check(minimum == bounds.sliderMinimum && maximum == bounds.sliderMaximum,
        "wide measured parameter uses requested slider window");
  enterValue(*fixture.numeric, sign * 1e7);
  fixture.verifyValue(sign * 1e7);
  fixture.field->setPrecision(6);
  fixture.verifyValue(sign * 1e7);
  fixture.actual = TParamP(fixture.actual->clone());
  fixture.actual->setDefaultValue(sign * 8e6);
  fixture.preview = TParamP(fixture.actual->clone());
  fixture.field->setParam(fixture.preview, fixture.actual, 0);
  fixture.verifyValue(sign * 8e6);
  enterValue(*fixture.numeric, sign * 9e6);
  fixture.verifyValue(sign * 9e6);
  enterValue(*fixture.numeric, sign * 10);
  fixture.verifyValue(sign * 10);
}

void roundedMeasured() {
  const Bounds cases[] = {
      {0.04, 1.04, 0.04, 1.04, 1},
      {-1.04, -0.04, -1.04, -0.04, 1},
      {0.0004, 1, 0.0004, 0.01},
      {0, 1.0006, 0.99, 1.0006},
      {-1.0006, 0, -1.0006, -0.99},
      {-1, -0.0004, -0.01, -0.0004},
      {-1, 1, 0.04, 0.26, 1},
      {-1, 1, -0.26, -0.04, 1},
      {0.04, 0.3, 0.04, 0.3, 1},
      {-0.3, -0.04, -0.3, -0.04, 1},
      {std::nextafter(0.1, 1.0), 0.4, std::nextafter(0.1, 1.0), 0.4, 1}};
  for (const auto &bounds : cases) {
    MeasuredFixture fixture((bounds.minimum + bounds.maximum) / 2, bounds);
    fixture.sweep(bounds);
  }
  MeasuredFixture decimal(0.2, {0, 1, 0.14, 0.58, 2});
  double minimum, maximum;
  check(decimal.numeric->getSliderRange(minimum, maximum) && minimum == 0.14 &&
            maximum == 0.58,
        "representable decimal endpoints are retained");
  decimal.sweep({0, 1, 0.14, 0.58, 2});
}

void wideBeforeAndAfterSetup() {
  DVGui::DoubleField field(nullptr, true, 3);
  field.setRange(-1e7, 1e7);
  for (double value : {8e6, -8e6}) {
    enterValue(field, value);
    check(field.getValue() == value,
          "wide typed value survives before optional setup");
  }
  require(field.setSliderRange(-10, 10), "cannot install narrow slider window");
  check(field.getValue() == -8e6,
        "optional setup preserves existing wide value");
  for (double value : {9e6, -9e6}) {
    enterValue(field, value);
    check(field.getValue() == value,
          "wide typed value survives optional setup");
  }
  field.setRange(-1e7, 1e7);
  check(field.getValue() == -9e6,
        "resetting slider range preserves wide value");
  enterValue(field, 7e6);
  check(field.getValue() == 7e6, "wide typed value survives range reset");
}

void invalidWindows() {
  DVGui::DoubleField field(nullptr, true, 1);
  field.setRange(-1, 1);
  require(field.setSliderRange(-0.5, 0.5),
          "cannot install initial valid window");
  const double nan        = std::numeric_limits<double>::quiet_NaN();
  const double infinity   = std::numeric_limits<double>::infinity();
  const double cases[][2] = {
      {0.041, 0.049}, {-0.049, -0.041}, {0.04, 0.14},
      {0.1, 0.1},     {0.5, -0.5},      {-2, 0},
      {0, 2},         {nan, 0.5},       {-0.5, infinity}};
  for (const auto &bounds : cases) {
    check(!field.setSliderRange(bounds[0], bounds[1]),
          "invalid or sub-quantum optional range is rejected");
    double minimum, maximum;
    check(field.getSliderRange(minimum, maximum) && minimum == -0.5 &&
              maximum == 0.5,
          "invalid range leaves previous optional window intact");
  }
  for (const auto &bounds : {Bounds{0.041, 0.049, 0.041, 0.049, 1},
                             Bounds{-0.049, -0.041, -0.049, -0.041, 1}}) {
    MeasuredFixture fixture((bounds.minimum + bounds.maximum) / 2, bounds);
    check(!fixture.slider->isEnabled(),
          "hard range without any slider tick disables dragging");
    check(inside(fixture.actual->getValue(0), bounds.minimum, bounds.maximum),
          "unrepresentable slider setup preserves parameter value");
    const double value = bounds.minimum < 0 ? -0.046 : 0.046;
    enterValue(*fixture.numeric, value);
    fixture.verifyValue(value);
    auto *roller = fixture.numeric->findChild<DVGui::RollerField *>();
    require(roller != nullptr, "measured roller missing");
    roller->setValue(bounds.maximum - 0.002);
    require(QMetaObject::invokeMethod(roller, "valueChanged",
                                      Qt::DirectConnection, Q_ARG(bool, false)),
            "cannot finish roller edit");
    fixture.verifyValue(bounds.maximum - 0.002);
  }
  DVGui::DoubleField wide(nullptr, true, 3);
  wide.setRange(-1e7, 1e7);
  require(wide.setSliderRange(-10, 10),
          "cannot install valid window on wide bounds");
  for (const auto &bounds :
       {Bounds{-1e7, 1e7, -1e7, 1e7}, Bounds{-1e7, 1e7, -2e6, 2e6}}) {
    check(!wide.setSliderRange(bounds.sliderMinimum, bounds.sliderMaximum),
          "unrepresentable integer bounds or span are rejected");
    double minimum, maximum;
    check(wide.getSliderRange(minimum, maximum) && minimum == -10 &&
              maximum == 10,
          "unrepresentable integer range leaves previous window intact");
  }
}

void ordinaryRangeChanges() {
  const double cases[][4] = {
      {50, -10, 10, 10}, {-50, -10, 10, -10}, {0, 3, 3, 3}, {50, 0, 0, 0}};
  for (const auto &values : cases) {
    DVGui::DoubleField field(nullptr, false, 2);
    field.setValue(values[0]);
    int notifications = 0;
    QObject::connect(&field, &DVGui::DoubleValueField::valueChanged,
                     [&notifications](bool) { ++notifications; });
    field.setRange(values[1], values[2]);
    check(field.getValue() == values[3],
          "ordinary range change clamps numeric value");
    auto *roller = field.findChild<DVGui::RollerField *>();
    require(roller != nullptr, "ordinary roller missing");
    check(std::abs(roller->getValue() - values[3]) < 1e-12,
          "ordinary range change clamps roller value");
    check(notifications == 1,
          "ordinary range change emits its existing notification");
  }
}

void precisionBeforeBinding() {
  TDoubleParamP actual = new TDoubleParam(0.75);
  actual->setValueRange(-1, 1);
  actual->setUISliderRange(0.00004, 0.00006);
  MeasuredDoubleParamField field(nullptr, "Fine window", actual);
  auto *numeric = field.findChild<DVGui::MeasuredDoubleField *>();
  auto *slider  = field.findChild<QSlider *>();
  require(numeric && slider, "fine measured field missing");
  check(numeric->getValue() == 0.75,
        "unrepresentable initial window preserves value");
  check(!slider->isEnabled(), "initial precision cannot represent fine window");
  for (int precision : {5, 1, 5}) {
    field.setPrecision(precision);
    check(numeric->getValue() == 0.75,
          "precision change preserves initial value before binding");
    if (precision == 1) {
      check(!slider->isEnabled(),
            "coarser precision disables unrepresentable window");
    } else {
      double minimum, maximum;
      check(numeric->getSliderRange(minimum, maximum) && minimum == 0.00004 &&
                maximum == 0.00006 && slider->isEnabled(),
            "finer precision restores declared optional window before binding");
    }
  }
  TDoubleParamP preview = TParamP(actual->clone());
  field.setParam(preview, actual, 0);
  check(numeric->getValue() == 0.75 && actual->getValue(0) == 0.75 &&
            preview->getValue(0) == 0.75,
        "binding retains value outside fine optional window");
}

class NonlinearField final : public DVGui::DoubleField {
public:
  NonlinearField() : DoubleField(nullptr, true, 2) { setLinearSlider(false); }
};

void ordinaryAndNonlinear() {
  MeasuredFixture ordinary(0, {-10, 10, -1, 1, 1});
  ordinary.sweep({-10, 10, -1, 1, 1});
  enterValue(*ordinary.numeric, 8);
  ordinary.verifyValue(8);
  NonlinearField field;
  field.setRange(-1000, 1000);
  require(field.setSliderRange(-10, 10), "cannot install nonlinear window");
  auto *slider = field.findChild<QSlider *>();
  require(slider != nullptr, "nonlinear slider missing");
  field.setValue(500);
  check(field.getValue() == 500 && slider->value() == slider->maximum(),
        "nonlinear thumb clamps without changing numeric value");
  slider->setValue(slider->minimum());
  check(std::abs(field.getValue() + 10) < 1e-12,
        "nonlinear minimum is preserved");
  slider->setValue((slider->minimum() + slider->maximum()) / 2);
  check(std::abs(field.getValue() + 9.6) < 1e-12,
        "nonlinear midpoint mapping is preserved");
  slider->setValue(slider->maximum());
  check(std::abs(field.getValue() - 10) < 1e-12,
        "nonlinear maximum is preserved");
  field.setRange(-1e7, 1e7);
  field.setValue(-9e6);
  check(field.getValue() == -9e6, "nonlinear wide value remains numeric");
  require(field.setSliderRange(-10, 10), "cannot reinstall nonlinear window");
  check(field.getValue() == -9e6,
        "nonlinear reconfiguration preserves wide value");
}
}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    require(argc == 2, "supply one bounds test scenario");
    const std::string scenario = argv[1];
    const struct {
      const char *name;
      void (*run)();
    } scenarios[] = {{"positive", [] { wideMeasured(false); }},
                     {"negative", [] { wideMeasured(true); }},
                     {"rounding", roundedMeasured},
                     {"wide_typing", wideBeforeAndAfterSetup},
                     {"invalid", invalidWindows},
                     {"ordinary_nonlinear", ordinaryAndNonlinear},
                     {"ordinary_range", ordinaryRangeChanges},
                     {"precision", precisionBeforeBinding}};
    bool found    = false;
    for (const auto &test : scenarios) {
      if (scenario != test.name) continue;
      test.run();
      found = true;
      break;
    }
    require(found, "unknown bounds test scenario");
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  if (failures) return 1;
  std::cout << "Double field bounds checks passed\n";
  return 0;
}
