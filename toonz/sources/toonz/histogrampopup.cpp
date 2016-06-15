

#include "histogrampopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "previewer.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/combohistogram.h"

// TnzLib includes
#include "toonz/tframehandle.h"

// TnzCore includes
#include "trasterimage.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"

// Qt includes
#include <QTimer>
#include <QMainWindow>

using namespace DVGui;

//=============================================================================
/*! \class HistogramPopup
                \brief The HistogramPopup class provides a dialog to show an
   histogram \b Histogram

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

HistogramPopup::HistogramPopup(QString title)
    : QDialog(TApp::instance()->getMainWindow()) {
  setTitle(title);

  m_histogram = new ComboHistogram(this);

  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(0);
  { mainLay->addWidget(m_histogram); }
  setLayout(mainLay);
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

//-----------------------------------------------------------------------------

void HistogramPopup::setTitle(QString title) { setWindowTitle(title); }

//-----------------------------------------------------------------------------

void HistogramPopup::setImage(TImageP image) {
  TRasterImageP rimg = (TRasterImageP)image;
  TVectorImageP vimg = (TVectorImageP)image;
  TToonzImageP timg  = (TToonzImageP)image;

  TPaletteP palette;

  TRasterP ras;
  if (rimg)
    ras = rimg->getRaster();
  else if (timg) {
    ras     = timg->getRaster();
    palette = timg->getPalette();
  } else if (vimg)
    ras = vimg->render(false);

  m_histogram->setRaster(ras, palette);
}
//-----------------------------------------------------------------------------
/*! show the picked color
*/
void HistogramPopup::updateInfo(const TPixel32 &pix, const TPointD &imagePos) {
  m_histogram->updateInfo(pix, imagePos);
}

//-----------------------------------------------------------------------------
/*! show the average-picked color
*/
void HistogramPopup::updateAverageColor(const TPixel32 &pix) {
  m_histogram->updateAverageColor(pix);
}

//=============================================================================
/*! \class ViewerHistogramPopup
                \brief The ViewerHistogramPopup show the histogram pertain to
   current viewer.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

ViewerHistogramPopup::ViewerHistogramPopup()
    : HistogramPopup(tr("Viewer Histogram")) {}

//-----------------------------------------------------------------------------

void ViewerHistogramPopup::showEvent(QShowEvent *e) {
  connect(TApp::instance()->getCurrentFrame(), SIGNAL(frameSwitched()),
          SLOT(setCurrentRaster()));
  connect(Previewer::instance(), SIGNAL(activedChanged()),
          SLOT(setCurrentRaster()));

  setCurrentRaster();
}

//-----------------------------------------------------------------------------

void ViewerHistogramPopup::hideEvent(QHideEvent *e) {
  disconnect(TApp::instance()->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(setCurrentRaster()));
  disconnect(Previewer::instance(), SIGNAL(activedChanged()), this,
             SLOT(setCurrentRaster()));
}

//-----------------------------------------------------------------------------

void ViewerHistogramPopup::setCurrentRaster() {
  TApp *app            = TApp::instance();
  Previewer *previewer = Previewer::instance();
  TRasterP ras         = 0;
  if (previewer->isActive()) {
    int currentFrame = app->getCurrentFrame()->getFrameIndex();
    // Se il preview del frame non e' pronto richiamo questo metodo dopo un
    // intervallo di 10 ms
    if (!previewer->isFrameReady(currentFrame)) {
      QTimer::singleShot(10, this, SLOT(setCurrentRaster()));
      return;
    }
    ras = previewer->getRaster(currentFrame);
  }
  m_histogram->setRaster(ras);
  update();
}

//=============================================================================

OpenPopupCommandHandler<ViewerHistogramPopup> openHistogramPopup(MI_Histogram);
