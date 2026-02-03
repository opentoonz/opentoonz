#include "layerheaderpanel.h"

#include <QPainter>
#include <QToolTip>
#include <QToolButton>

#include "xsheetviewer.h"
#include "xshcolumnviewer.h"

#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"

#include "toonz/preferences.h"
#include "../include/toonzqt/gutil.h"

using XsheetGUI::ColumnArea;

LayerHeaderPanel::LayerHeaderPanel(XsheetViewer *viewer, QWidget *parent,
                                   Qt::WindowFlags flags)
    : QWidget(parent, flags), m_viewer(viewer) {
  QHBoxLayout *layout = new QHBoxLayout(this);
  setObjectName("layerHeaderPanel");
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_previewButton  = new QToolButton(this);
  m_camstandButton = new QToolButton(this);
  m_lockButton     = new QToolButton(this);

  // Define style and icon size for all buttons
  QString buttonStyleSheet =
      "QToolButton { padding: 0px; margin: 0px; }";
  QSize iconSize(16, 16);
  QSize buttonSize(20, 20);

  QList<QToolButton *> buttonList = {m_previewButton, m_camstandButton,
                                     m_lockButton};

  for (auto &button : buttonList) {
    button->setStyleSheet(buttonStyleSheet);
    button->setIconSize(iconSize);
    button->setFixedSize(buttonSize);
  }
  
  m_previewButton->setIcon(createQIcon("preview"));
  m_camstandButton->setIcon(createQIcon("table"));
  m_lockButton->setIcon(createQIcon("lock_on"));

  connect(m_previewButton, &QToolButton::clicked, this,
          &LayerHeaderPanel::onPreviewClicked);
  connect(m_camstandButton, &QToolButton::clicked, this,
          &LayerHeaderPanel::onCamstandClicked);
  connect(m_lockButton, &QToolButton::clicked, this,
          &LayerHeaderPanel::onLockClicked);

  // Add widgets to the layout
  layout->addWidget(m_previewButton);
  layout->addWidget(m_camstandButton);
  layout->addWidget(m_lockButton);
  layout->addStretch();

  setLayout(layout);
}

LayerHeaderPanel::~LayerHeaderPanel() {}

//-----------------------------------------------------------------------------

void LayerHeaderPanel::toggleColumnVisibility(int visibilityFlag) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = m_viewer->getXsheet();
  int col, totcols = xsh->getColumnCount();
  bool sound_changed = false;

  if (totcols > 0) {
    int startCol =
        Preferences::instance()->isXsheetCameraColumnVisible() ? -1 : 0;
    for (col = startCol; col < totcols; col++) {
      if (startCol < 0 || !xsh->isColumnEmpty(col)) {
        TXshColumn *column = xsh->getColumn(col);

        if (visibilityFlag == ToggleAllPreviewVisible) {
          column->setPreviewVisible(!column->isPreviewVisible());
        } else if (visibilityFlag == ToggleAllTransparency) {
          column->setCamstandVisible(!column->isCamstandVisible());
          if (column->getSoundColumn()) sound_changed = true;
        } else if (visibilityFlag == ToggleAllLock) {
          column->lock(!column->isLocked());
        }
      }
    }

    if (sound_changed) {
      app->getCurrentXsheet()->notifyXsheetSoundChanged();
    }

    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
  m_viewer->updateColumnArea();
  update();
}

//-----------------------------------------------------------------------------

void LayerHeaderPanel::onPreviewClicked() {
  toggleColumnVisibility(ToggleAllPreviewVisible);
}

void LayerHeaderPanel::onCamstandClicked() {
  toggleColumnVisibility(ToggleAllTransparency);
}

void LayerHeaderPanel::onLockClicked() {
  toggleColumnVisibility(ToggleAllLock);
}

//-----------------------------------------------------------------------------

void LayerHeaderPanel::showOrHide(const Orientation *o) {
  QRect rect = o->rect(PredefinedRect::LAYER_HEADER_PANEL);
  if (rect.isEmpty())
    hide();
  else
    show();
}
