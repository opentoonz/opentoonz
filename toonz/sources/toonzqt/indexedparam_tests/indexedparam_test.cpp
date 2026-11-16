#include "toonzqt/fxsettings.h"
#include "toonzqt/doublefield.h"
#include "tfxparam.h"
#include "tenv.h"
#include "tstream.h"
#include "tundo.h"

#include <QApplication>
#include <QComboBox>
#include <QTemporaryFile>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

class IndexedTestFx final : public TRasterFx {
  FX_DECLARATION(IndexedTestFx)

public:
  TDoubleParamP count      = new TDoubleParam(1);
  TDoubleParamP groupCount = new TDoubleParam(1);
  TPixelParamP first       = new TPixelParam(TPixel32::Red);
  TPixelParamP second      = new TPixelParam(TPixel32::Blue);

  IndexedTestFx() {
    count->setValueRange(0, 2);
    groupCount->setValueRange(0, 2);
    bindParam(this, "count", count);
    bindParam(this, "groupCount", groupCount);
    bindParam(this, "first", first);
    bindParam(this, "second", second);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool canHandle(const TRenderSettings &, double) override { return true; }
  std::string getPluginId() const override { return "TEST"; }
  bool doGetBBox(double, TRectD &box, const TRenderSettings &) override {
    box = TRectD(0, 0, 1, 1);
    return true;
  }
  void doCompute(TTile &tile, double, const TRenderSettings &) override {
    tile.getRaster()->clear();
  }
};
FX_IDENTIFIER(IndexedTestFx, "indexedParamRegression")

namespace {
int checks   = 0;
int warnings = 0;

void require(bool condition, const char *message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}

void messages(QtMsgType type, const QMessageLogContext &,
              const QString &message) {
  // The existing ParamViewer constructor attaches its nested layout twice.
  if (type == QtWarningMsg &&
      message ==
          "QLayout: Attempting to add QLayout \"\" to ParamViewer \"\", "
          "which already has a layout")
    return;
  if (type == QtWarningMsg || type == QtCriticalMsg) ++warnings;
  std::cerr << message.toStdString() << '\n';
}

struct Fixture {
  TFxP actual  = new IndexedTestFx;
  TFxP preview = new IndexedTestFx;
  ParamViewer viewer;
  ParamsPage page{nullptr, &viewer};
  TFxHandle handle;

  Fixture() {
    QTemporaryFile layout;
    require(layout.open(), "temporary layout opens");
    layout.write(R"(<page>
      <control>count</control><control>groupCount</control>
      <indexedColors count="count" label="Colors">
        <control>first</control><control>second</control>
      </indexedColors>
      <indexedGroups count="groupCount" label="Groups">
        <entry color="first"><control>first</control></entry>
        <entry color="second"><control>second</control></entry>
      </indexedGroups>
    </page>)");
    layout.flush();
    TIStream stream(TFilePath(layout.fileName().toStdString()));
    std::string tag;
    require(stream.matchTag(tag) && tag == "page", "layout page opens");
    page.setPage(stream, actual, false);
    ParamField::setFxHandle(&handle);
    QObject::connect(&handle, &TFxHandle::fxChanged, &page,
                     [&] { page.update(4); });
  }
  ~Fixture() {
    TUndoManager::manager()->reset();
    ParamField::setFxHandle(nullptr);
  }
  IndexedTestFx *saved() {
    return dynamic_cast<IndexedTestFx *>(actual.getPointer());
  }
  IndexedTestFx *current() {
    return dynamic_cast<IndexedTestFx *>(preview.getPointer());
  }
  MeasuredDoubleParamField *field(const char *name) {
    for (auto *field : page.findChildren<MeasuredDoubleParamField *>())
      if (field->getParamName() == name) return field;
    throw std::runtime_error("count field missing");
  }
  QComboBox *selector(int group) {
    return page.findChildren<QComboBox *>("indexedColorSelector").at(group);
  }
  void edit(const char *name, double value) {
    auto *number = field(name)->findChild<DVGui::MeasuredDoubleField *>();
    require(number != nullptr, "numeric count editor exists");
    number->setValue(value);
    require(
        QMetaObject::invokeMethod(number, "valueChanged", Q_ARG(bool, false)),
        "numeric count edit commits");
  }
  void bindAnimated() {
    for (auto count : {saved()->count, saved()->groupCount}) {
      count->setValue(0, 1);
      count->setValue(9, 1);
    }
    preview = actual->clone(false);
    page.setFx(preview, actual, 4);
  }
};

void pendingCounts() {
  Fixture fixture;
  fixture.bindAnimated();
  fixture.edit("count", 2);
  fixture.edit("groupCount", 2);
  require(fixture.current()->count->getValue(4) == 2,
          "pending color count changes preview");
  require(fixture.saved()->count->getValue(4) == 1,
          "pending color count leaves saved curve alone");
  fixture.page.update(4);
  require(fixture.current()->count->getValue(4) == 2,
          "refresh preserves pending color count");
  require(fixture.current()->groupCount->getValue(4) == 2,
          "refresh preserves pending group count");
  auto *number =
      fixture.field("count")->findChild<DVGui::MeasuredDoubleField *>();
  require(number->getValue() == 2, "count editor displays the pending count");
  require(!fixture.selector(0)->itemText(1).contains("inactive"),
          "pending count enables second color");
  fixture.selector(0)->setCurrentIndex(1);
  require(fixture.current()->count->getValue(4) == 2,
          "list navigation leaves preview unchanged");
  fixture.field("count")->findChild<ParamFieldKeyToggle *>()->keyToggled();
  require(fixture.saved()->count->getValue(4) == 2,
          "keying commits the pending count");
  require(TUndoManager::manager()->undo(), "count key undo exists");
  require(fixture.current()->count->getValue(4) == 1,
          "count key undo refreshes the saved result");
  require(fixture.current()->groupCount->getValue(4) == 2,
          "count undo preserves another list's pending edit");
  require(TUndoManager::manager()->redo(), "count key redo exists");
  require(fixture.current()->count->getValue(4) == 2,
          "count key redo restores preview");
}

void defaultAndRebind() {
  Fixture fixture;
  fixture.page.setFx(fixture.preview, fixture.actual, 4);
  fixture.edit("count", 2);
  require(fixture.saved()->count->getDefaultValue() == 2,
          "default count edit commits");
  require(TUndoManager::manager()->undo(), "default count undo exists");
  require(fixture.current()->count->getValue(4) == 1,
          "default count undo removes preview-only key");
  fixture.bindAnimated();
  fixture.edit("count", 2);
  fixture.page.update(5);
  require(!fixture.current()->count->isKeyframe(4),
          "frame change cancels pending count key");
  const TFxP oldActual = fixture.actual;
  fixture.actual       = new IndexedTestFx;
  fixture.preview      = fixture.actual->clone(false);
  fixture.page.setFx(fixture.preview, fixture.actual, 4);
  dynamic_cast<IndexedTestFx *>(oldActual.getPointer())->count->setValue(4, 2);
  fixture.page.update(4);
  require(fixture.current()->count->getValue(4) == 1,
          "rebind detaches the previous count curve");
}

void swatchesAndLifetime() {
  TFxP retained;
  {
    Fixture fixture;
    fixture.bindAnimated();
    retained = fixture.actual;
    const TPixelD color(1e100, -1e100, 0.25, 1);
    fixture.current()->first->setValueD(4, color);
    fixture.field("count")->currentParamChanged();
    const QImage swatch =
        fixture.selector(0)->itemIcon(0).pixmap(16, 16).toImage();
    require(swatch.pixelColor(8, 8) == QColor(255, 0, 64),
            "indexed swatches clamp only the display copy of HDR channels");
    require(fixture.current()->first->getValueD(4) == color,
            "indexed swatch conversion retains precise preview values");
  }
  dynamic_cast<IndexedTestFx *>(retained.getPointer())->count->setValue(4, 2);
  require(true, "destroyed indexed page detaches count observers");
}
}  // namespace

int main(int argc, char **argv) {
  QApplication application(argc, argv);
  QTemporaryDir runtime;
  require(runtime.isValid(), "temporary runtime opens");
  TEnv::setStuffDir(TFilePath(runtime.path().toStdString()));
  TEnv::setArgPathValue(TEnv::getRootVarName(), runtime.path().toStdString());
  TEnv::setArgPathValue(TEnv::getSystemVarPrefix() + "PROFILES",
                        runtime.path().toStdString());
  TEnv::setArgPathValue(TEnv::getSystemVarPrefix() + "CONFIG",
                        runtime.path().toStdString());
  qInstallMessageHandler(messages);
  TThread::init();
  try {
    pendingCounts();
    defaultAndRebind();
    swatchesAndLifetime();
    require(warnings == 0, "unexpected Qt warnings");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  TThread::shutdown();
  std::cout << checks << " indexed parameter checks passed\n";
}
