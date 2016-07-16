

// TnzCore includes
#include "timage_io.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcamera.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheethandle.h"
#include "toonz/sceneproperties.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"

// Qt includes
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QApplication>

static void printCurrentFrame() {
  QPrinter printer;
  QPrintDialog dialog(&printer, 0);
  if (!dialog.exec()) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  int frame         = TApp::instance()->getCurrentFrame()->getFrame();
  int lx            = TApp::instance()
               ->getCurrentScene()
               ->getScene()
               ->getCurrentCamera()
               ->getRes()
               .lx;
  int ly = TApp::instance()
               ->getCurrentScene()
               ->getScene()
               ->getCurrentCamera()
               ->getRes()
               .ly;
  TRaster32P raster(lx, ly);
  if (scene->getFrameCount() <= 0) {
    // Ricordarsi di usare DvMsgBox !! (se si decommenta questo codice :) )
    // QMessageBox::warning(0,"Print",tr("It is not possible to generate an
    // animation\nbecause the scene is empty.", "WARNING"));
    return;
  }
  raster->fill(scene->getProperties()->getBgColor());
  scene->renderFrame(raster, frame,
                     TApp::instance()->getCurrentXsheet()->getXsheet());
  QImage img = rasterToQImage(raster);
  QPainter painter(&printer);
  QRect rect = painter.viewport();
  QSize size = img.size();
  size.scale(rect.size(), Qt::KeepAspectRatio);
  painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
  painter.setWindow(img.rect());
  painter.drawImage(0, 0, img);
}

//=============================================================================

class PrintCommand final : public MenuItemHandler {
public:
  PrintCommand() : MenuItemHandler(MI_Print) {}
  void execute() override {
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    qApp->processEvents();
    printCurrentFrame();
    qApp->restoreOverrideCursor();
  }
} printCommand;
