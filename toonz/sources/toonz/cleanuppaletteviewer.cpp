

// TnzCore includes
#include "tpalette.h"

// ToonzLib includes
#include "toonz/cleanupcolorstyles.h"
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"

// ToonzQt includes
#include "toonzqt/colorfield.h"
#include "toonzqt/dvdialog.h"

// Toonz includes
#include "tapp.h"

// Qt includes
#include <QScrollArea>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

#include "cleanuppaletteviewer.h"

using namespace DVGui;

//********************************************************************************
//    Local stuff
//********************************************************************************

namespace {

int search(TPalette *plt, TCleanupStyle *style) {
  int i, styleCount = plt->getStyleCount();
  for (i = 0; i < styleCount; ++i)
    if (plt->getStyle(i) == style) return i;

  assert(false);
  return -1;
}
}

//********************************************************************************
//    CleanupPaletteViewer implementation
//********************************************************************************

CleanupPaletteViewer::CleanupPaletteViewer(QWidget *parent)
    : QWidget(parent)
    , m_ph(TApp::instance()->getPaletteController()->getCurrentCleanupPalette())
    , m_scrollWidget(0)
    , m_greyMode(true)
    , m_contrastEnabled(true) {
  m_scrollArea = new QScrollArea(this);
  m_add        = new QPushButton("+", this);
  m_remove     = new QPushButton("-", this);

  //-----
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setWidgetResizable(true);
  m_add->setFixedSize(20, 20);
  m_remove->setFixedSize(20, 20);

  buildGUI();

  //----- layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(5);
  mainLayout->setMargin(5);
  {
    mainLayout->addWidget(m_scrollArea, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(7);
    buttonLayout->setMargin(0);
    {
      buttonLayout->addWidget(m_add);
      buttonLayout->addWidget(m_remove);
      buttonLayout->addStretch();
    }
    mainLayout->addLayout(buttonLayout);
  }
  setLayout(mainLayout);

  //----- signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_ph, SIGNAL(paletteSwitched()), SLOT(buildGUI()));
  ret      = ret && connect(m_ph, SIGNAL(paletteChanged()), SLOT(buildGUI()));
  ret      = ret && connect(m_ph, SIGNAL(colorStyleChanged(bool)),
                       SLOT(onColorStyleChanged()));

  ret = ret && connect(m_add, SIGNAL(clicked(bool)), SLOT(onAddClicked(bool)));
  ret = ret &&
        connect(m_remove, SIGNAL(clicked(bool)), SLOT(onRemoveClicked(bool)));
  assert(ret);
}

//---------------------------------------------------------------------------------

void CleanupPaletteViewer::buildGUI() {
  m_colorFields.clear();

  // delete m_scrollWidget;    //Already destroyed by
  // m_scrollWidget->setWidget()
  m_scrollWidget = new QFrame();

  QVBoxLayout *scrollLayout = new QVBoxLayout(m_scrollWidget);
  m_scrollWidget->setLayout(scrollLayout);

  scrollLayout->setSpacing(10);
  scrollLayout->setMargin(0);

  TPalette *palette = TApp::instance()
                          ->getPaletteController()
                          ->getCurrentCleanupPalette()
                          ->getPalette();
  assert(palette);
  bool ret = true;

  int i, stylesCount = palette->getPage(0)->getStyleCount();
  for (i = 0; i < stylesCount; ++i) {
    TCleanupStyle *cs =
        dynamic_cast<TCleanupStyle *>(palette->getPage(0)->getStyle(i));
    if (!cs) continue;

    cs->enableContrast(true);
    CleanupColorField *cf =
        new CleanupColorField(m_scrollWidget, cs, m_ph, m_greyMode);
    cs->enableContrast(m_contrastEnabled);

    ret = ret && connect(cf, SIGNAL(StyleSelected(TCleanupStyle *)),
                         SLOT(onColorStyleSelected(TCleanupStyle *)));

    scrollLayout->addWidget(cf);
    m_colorFields.push_back(cf);

    if (m_greyMode) break;
  }

  scrollLayout->addStretch(1);
  assert(ret);

  // NOTE: setWidget() Must be done after m_scrollWidget's layout has been set
  // (see Qt docs)
  m_scrollArea->setWidget(m_scrollWidget);
  m_scrollArea->ensureWidgetVisible(m_scrollWidget, 0, 0);

  if (m_greyMode) {
    m_scrollArea->setMinimumSize(0, 60);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; }");
  } else {
    m_scrollArea->setMinimumSize(0, 120);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: 1px solid rgb(190, 190, 190); }");
  }

  m_add->setVisible(!m_greyMode);
  m_remove->setVisible(!m_greyMode);

  setContrastEnabled(m_contrastEnabled);
}

//---------------------------------------------------------------

void CleanupPaletteViewer::onColorStyleChanged() {
  int i, colorFieldsCount = m_colorFields.size();
  for (i = 0; i < colorFieldsCount; ++i) m_colorFields[i]->updateColor();
}

//------------------------------------------------------------------------

void CleanupPaletteViewer::onColorStyleSelected(TCleanupStyle *style) {
  m_ph->setStyleIndex(style ? search(m_ph->getPalette(), style) : -1);
}

//---------------------------------------------------------------

void CleanupPaletteViewer::updateColors() {
  int i, colorFieldsCount = m_colorFields.size();
  for (i = 0; i < colorFieldsCount; ++i) m_colorFields[i]->updateColor();
}

//---------------------------------------------------------------

void CleanupPaletteViewer::setMode(bool greyMode) {
  if (m_greyMode == greyMode) return;
  m_greyMode = greyMode;

  buildGUI();
  update();
}

//------------------------------------------------------------------------

void CleanupPaletteViewer::onAddClicked(bool) {
  TPalette *palette = m_ph->getPalette();
  assert(palette);
  palette->getPage(0)->addStyle(new TColorCleanupStyle(TPixel::Red));
  buildGUI();
  update();

  m_ph->notifyPaletteChanged();
}

//------------------------------------------------------------------------------------

void CleanupPaletteViewer::onRemoveClicked(bool) {
  int styleIdx = m_ph->getStyleIndex();
  if (styleIdx < 2) return;

  TPalette *palette = m_ph->getPalette();
  assert(palette);

  QString question;
  question = QObject::tr(
      "Are you sure you want to delete the selected cleanup color?");
  int ret =
      DVGui::MsgBox(question, QObject::tr("Delete"), QObject::tr("Cancel"), 0);
  if (ret == 2 || ret == 0) return;

  TPalette::Page *page = palette->getPage(0);
  int indexInPage      = page->search(m_ph->getStyle());
  page->removeStyle(indexInPage);
  m_ph->setStyleIndex(-1);

  buildGUI();
  update();

  m_ph->notifyPaletteChanged();
}

//------------------------------------------------------------------------

void CleanupPaletteViewer::setContrastEnabled(bool enable) {
  m_contrastEnabled = enable;
  unsigned int i, size = m_colorFields.size();
  for (i = 0; i < size; ++i) m_colorFields[i]->setContrastEnabled(enable);
}
