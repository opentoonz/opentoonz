#include "tooloptionscontrols.h"
#include "toonzvectorbrushtool.h"
#include "tools/tooloptions.h"
#include "toonz/tapplication.h"
#include "tenv.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTest>
#include <stdexcept>

namespace {

class EmptyApplication final : public TApplication {
public:
  TFrameHandle *getCurrentFrame() const override { return nullptr; }
  TXshLevelHandle *getCurrentLevel() const override { return nullptr; }
  TXsheetHandle *getCurrentXsheet() const override { return nullptr; }
  TObjectHandle *getCurrentObject() const override { return nullptr; }
  TColumnHandle *getCurrentColumn() const override { return nullptr; }
  TSceneHandle *getCurrentScene() const override { return nullptr; }
  ToolHandle *getCurrentTool() const override { return nullptr; }
  TSelectionHandle *getCurrentSelection() const override { return nullptr; }
  TOnionSkinMaskHandle *getCurrentOnionSkin() const override { return nullptr; }
  TPaletteHandle *getCurrentPalette() const override { return nullptr; }
  TFxHandle *getCurrentFx() const override { return nullptr; }
  PaletteController *getPaletteController() const override { return nullptr; }
  TColorStyle *getCurrentLevelStyle() const override { return nullptr; }
  int getCurrentLevelStyleIndex() const override { return 0; }
  void setCurrentLevelStyleIndex(int, bool) override {}
};

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

void selectRange(ToolOptionCombo &range, Qt::Key key, int expected) {
  QTest::keyClick(&range, key);
  QApplication::processEvents();
  require(range.currentIndex() == expected,
          "The toolbar discarded the selected Frame Range");
  require(range.getProperty()->getIndex() == expected,
          "Frame Range did not reach the brush property");
}

void checkRangeSelections(ToonzVectorBrushTool &tool) {
  auto *property = dynamic_cast<TEnumProperty *>(
      tool.getProperties(0)->getProperty("Range:"));
  require(property != nullptr, "Vector Brush is missing Frame Range");
  property->setIndex(0);

  BrushToolOptionsBox panel(nullptr, &tool, nullptr, nullptr);
  auto *range = dynamic_cast<ToolOptionCombo *>(panel.control("Range:"));
  require(range != nullptr && range->isEnabled(), "Frame Range is unavailable");

  for (int index = 1; index < range->count(); ++index)
    selectRange(*range, Qt::Key_Down, index);
  for (int index = range->count() - 2; index >= 0; --index)
    selectRange(*range, Qt::Key_Up, index);

  property->setIndex(2);
  panel.updateStatus();
  require(range->currentIndex() == 2,
          "The toolbar did not reflect a property update");
  selectRange(*range, Qt::Key_Down, 3);
}

}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setStyle("windows");
  const QString stuff = qEnvironmentVariable("TOONZROOT");
  require(!stuff.isEmpty(), "Set TOONZROOT to the checkout's stuff directory");
  TEnv::setArgPathValue("TOONZROOT", stuff.toStdString());
  EmptyApplication context;
  TTool::setApplication(&context);
  ToonzVectorBrushTool tool("T_BrushOptionsTest", TTool::Vectors);
  tool.updateTranslation();

  checkRangeSelections(tool);
  for (const QString &theme : {QString("Default"), QString("Darker")}) {
    require(QDir::setCurrent(stuff + "/config/qss/" + theme),
            "Cannot enter the theme directory");
    QFile stylesheet(theme + ".qss");
    require(stylesheet.open(QIODevice::ReadOnly),
            "Cannot read the theme stylesheet");
    app.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    checkRangeSelections(tool);
  }
}
