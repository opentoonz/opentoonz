

#include "testpanel.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "toonzqt/doublepairfield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/spectrumfield.h"
#include "tapp.h"
#include "mainwindow.h"

#include "tlogger.h"

#include <QFrame>
#include <QVBoxLayout>

using namespace DVGui;

//=============================================================================
/*! \class TestPanel
                \brief The TestPanel class is a pane for test.

                Inherits \b TPanel.

                ATTENZIONE Ho modificato anche il file style.qss:

                TPanel #testPanel
                {
                        border: 0px;
                        margin: 1px;
                }
*/
TestPanel::TestPanel(QWidget *parent, Qt::WindowFlags flags) : TPanel(parent) {
  setPanelType("Test");
  setIsMaximizable(false);
  setWindowTitle("Test");

  QFrame *box = new QFrame(this);
  box->setFrameStyle(QFrame::StyledPanel);
  box->setObjectName("testPanel");

  QVBoxLayout *layout = new QVBoxLayout(box);

  DoublePairField *doublePairFld = new DoublePairField(box);
  doublePairFld->setRange(0, 10);
  doublePairFld->setValues(std::make_pair(2.5, 6.5));
  doublePairFld->setLeftText(tr("Left:"));
  doublePairFld->setRightText(tr("Right:"));
  connect(doublePairFld, SIGNAL(valuesChanged(bool)),
          SLOT(onDoubleValuesChanged(bool)));
  layout->addWidget(doublePairFld, 1);

  DoubleField *doubleFld = new DoubleField();
  doubleFld->setRange(-100, 100);
  doubleFld->setValue(50.55);
  connect(doubleFld, SIGNAL(valueChanged(bool)),
          SLOT(onDoubleValueChanged(bool)));
  layout->addWidget(doubleFld, 1);

  IntField *intFld = new IntField();
  intFld->setRange(0, 100);
  intFld->setValue(50);
  connect(intFld, SIGNAL(valueChanged(bool)), SLOT(onIntValueChanged(bool)));
  layout->addWidget(intFld, 1);

  ColorField *colorFld = new ColorField(0, true, TPixel32(0, 0, 255, 255), 50);
  connect(colorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
          SLOT(onColorValueChanged(const TPixel32 &, bool)));
  layout->addWidget(colorFld, 1, Qt::AlignCenter);

  SpectrumField *spectrumFld = new SpectrumField(0, TPixel32(0, 0, 255));
  layout->addWidget(spectrumFld, 1, Qt::AlignCenter);

  box->setLayout(layout);

  setWidget(box);
}

//-----------------------------------------------------------------------------

TestPanel::~TestPanel() {}

//-----------------------------------------------------------------------------

void TestPanel::onDoubleValuesChanged(bool isDragging) {
  TLogger::debug() << "DoublePairField signal emitted.";
}

//-----------------------------------------------------------------------------

void TestPanel::onDoubleValueChanged(bool isDragging) {
  TLogger::debug() << "DoubleField signal emitted.";
}

//-----------------------------------------------------------------------------

void TestPanel::onIntValueChanged(bool) {
  TLogger::debug() << "IntField signal emitted.";
}

//-----------------------------------------------------------------------------

void TestPanel::onColorValueChanged(const TPixel32 &, bool isDragging) {
  TLogger::debug() << "ColorField signal emitted.";
}

//=============================================================================
// OpenFloatingTestPanel

class OpenFloatingTestPanel final : public MenuItemHandler {
public:
  OpenFloatingTestPanel() : MenuItemHandler(MI_OpenTest) {}
  void execute() override {
    TMainWindow *currentRoom = TApp::instance()->getCurrentRoom();
    if (currentRoom) {
      QList<TPanel *> list = currentRoom->findChildren<TPanel *>();
      for (int i = 0; i < list.size(); i++) {
        TPanel *pane = list.at(i);
        // If the pane is hidden, floating and have the same name
        if (pane->isHidden() && pane->isFloating() &&
            pane->getPanelType() == "Test") {
          pane->show();
          pane->raise();
          return;
        }
      }

      TPanel *pane = TPanelFactory::createPanel(currentRoom, "Test");

      pane->setFloating(true);
      pane->show();
      pane->raise();
    }
  }
} openFloatingTestPanelCommand;
