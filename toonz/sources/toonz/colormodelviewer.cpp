

// Tnz6 includes
#include "colormodelviewer.h"
#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "tapp.h"
#include "pane.h"
#include "colormodelbehaviorpopup.h"

// TnzTools includes
#include "tools/cursormanager.h"
#include "tools/cursors.h"
#include "tools/stylepicker.h"
#include "tools/toolcommandids.h"
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "../tnztools/stylepickertool.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"

// TnzLib includes
#include "toonz/palettecmd.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"

// TnzCore includes
#include "tsystem.h"
#include "ttoonzimage.h"

// Qt includes
#include <QMimeData>
#include <QMouseEvent>
#include <QUrl>
#include <QMenu>

#define LINES "Lines"
#define AREAS "Areas"
#define ALL "Lines & Areas"

using namespace DVGui;

namespace {
TPaletteHandle *getPaletteHandle() {
  return TApp::instance()->getPaletteController()->getCurrentLevelPalette();
}

}  // namespace

//=============================================================================
/*! \class ColorModelViewer
                \brief The ColorModelViewer class is a flip book used to manage
   color model.

                Inherits \b FlipBook.

                This object show the reference image linked to current level
   palette.
*/
/*!	\fn void ColorModelViewer::resetImageViewer()
                Set current level to TLevelP() and image to "0".
*/
ColorModelViewer::ColorModelViewer(QWidget *parent)
    : FlipBook(parent, QString(tr("Color Model")),
               FlipConsole::cFullConsole &
                   (~(FlipConsole::eFilterRgbm | FlipConsole::cFilterGRgb |
                      FlipConsole::eRate | FlipConsole::eSound |
                      FlipConsole::eSaveImg | FlipConsole::eHisto |
                      FlipConsole::eCompare | FlipConsole::eCustomize |
                      FlipConsole::eSave | FlipConsole::eFilledRaster |
                      FlipConsole::eDefineLoadBox | FlipConsole::eUseLoadBox |
                      FlipConsole::eDefineSubCamera | FlipConsole::eLocator)),
               eDontKeepFilesOpened, true)
    , m_mode(0)
    , m_currentRefImgPath(TFilePath()) {
  setObjectName("colormodel");

  setToolCursor(m_imageViewer, ToolCursor::PickerCursor);

  // Do not call the special procedure for flipbook closures...
  disconnect(parentWidget(), SIGNAL(closeButtonPressed()), this,
             SLOT(onCloseButtonPressed()));

  bool ret = connect(this, SIGNAL(refImageNotFound()), this,
                     SLOT(onRefImageNotFound()), Qt::QueuedConnection);
  assert(ret);

  m_imageViewer->setMouseTracking(true);
}
//-----------------------------------------------------------------------------

ColorModelViewer::~ColorModelViewer() {}

//-----------------------------------------------------------------------------
/*! Accept event if current palette is not empty, event data has urls and each
                urls type are different from "src" and "tpl" .
*/
void ColorModelViewer::dragEnterEvent(QDragEnterEvent *event) {
  TPalette *palette = getPaletteHandle()->getPalette();
  if (!palette) return;
  const QMimeData *mimeData = event->mimeData();
  if (!acceptResourceDrop(mimeData->urls())) return;

  foreach (QUrl url, mimeData->urls()) {
    TFilePath fp(url.toLocalFile().toStdWString());
    std::string type = fp.getType();
    if (type == "scr" || type == "tpl") return;
  }
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------
/*! If event data has urls, convert each urls in path and set view and current
    palette reference image (recall loadImage() and setLevel()).
*/
void ColorModelViewer::dropEvent(QDropEvent *event) {
  const QMimeData *mimeData = event->mimeData();
  if (mimeData->hasUrls()) {
    foreach (QUrl url, mimeData->urls()) {
      TFilePath fp(url.toLocalFile().toStdWString());
      loadImage(fp);
      setLevel(fp);
    }
    event->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------
/*! Set current palette reference image to \b fp recall:
                \b PaletteCmd::loadReferenceImage().
*/
// This function will be called when drag & drop the file into the color model
// viewer

void ColorModelViewer::loadImage(const TFilePath &fp) {
  if (fp.isEmpty()) return;

  TPaletteHandle *paletteHandle = getPaletteHandle();
  TPalette *palette             = paletteHandle->getPalette();

  if (!palette || palette->isCleanupPalette()) {
    DVGui::error(QObject::tr("Cannot load Color Model in current palette."));
    return;
  }

  PaletteCmd::ColorModelLoadingConfiguration config;

  // if the palette is locked, replace the color model's palette with the
  // destination
  if (palette->isLocked()) {
    // do nothing as config will use behavior = ReplaceColorModelPlt by default
    // config.behavior = PaletteCmd::ReplaceColorModelPlt;
  } else {
    std::set<TFilePath> fpSet;
    fpSet.insert(fp);
    ColorModelBehaviorPopup popup(fpSet, 0);
    int ret = popup.exec();
    if (ret == QDialog::Rejected) return;
    popup.getLoadingConfiguration(config);
  }

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  int isLoaded =
      PaletteCmd::loadReferenceImage(paletteHandle, config, fp, scene);

  if (0 != isLoaded) {
    std::cout << "loadReferenceImage Failed for some reason." << std::endl;
    return;
  }

  // no changes in the icon with replace (Keep the destination palette) option
  if (config.behavior != PaletteCmd::ReplaceColorModelPlt) {
    TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
    if (!level) return;
    std::vector<TFrameId> fids;
    level->getFids(fids);
    invalidateIcons(level, fids);
  }
}

//-----------------------------------------------------------------------------
/*! Create and open the Right-click menu color model viewer.
*/
void ColorModelViewer::contextMenuEvent(QContextMenuEvent *event) {
  /*-- Levelが取得できない場合はMenuを出さない --*/
  TApp *app     = TApp::instance();
  TXshLevel *xl = app->getCurrentLevel()->getLevel();
  if (!xl) return;
  TXshSimpleLevel *sl = xl->getSimpleLevel();
  if (!sl) return;
  TPalette *currentPalette =
      app->getPaletteController()->getCurrentLevelPalette()->getPalette();
  if (!currentPalette) return;

  QMenu menu(this);

  menu.addAction(CommandManager::instance()->getAction(MI_LoadColorModel));

  QAction *loadCurrentFrame =
      new QAction(QString(tr("Use Current Frame")), this);
  connect(loadCurrentFrame, SIGNAL(triggered()), SLOT(loadCurrentFrame()));
  menu.addAction(loadCurrentFrame);

  if (!m_imageViewer->getImage()) {
    menu.exec(event->globalPos());
    return;
  }

  QAction *removeColorModel =
      new QAction(QString(tr("Remove Color Model")), this);
  connect(removeColorModel, SIGNAL(triggered()), SLOT(removeColorModel()));
  menu.addAction(removeColorModel);

  /* If there is at least one style with "picked pos" parameter, then enable
   * repick command */
  TRasterImageP ri = m_imageViewer->getImage();
  if (ri && currentPalette->hasPickedPosStyle()) {
    menu.addSeparator();
    QAction *repickFromColorModelAct = new QAction(
        QString(tr("Update Colors by Using Picked Positions")), this);
    connect(repickFromColorModelAct, SIGNAL(triggered()),
            SLOT(repickFromColorModel()));
    menu.addAction(repickFromColorModelAct);
  }

  menu.addSeparator();

  QString shortcut = QString::fromStdString(
      CommandManager::instance()->getShortcutFromId(V_ZoomReset));
  QAction *reset = menu.addAction(tr("Reset View") + "\t " + shortcut);
  connect(reset, SIGNAL(triggered()), m_imageViewer, SLOT(resetView()));

  shortcut = QString::fromStdString(
      CommandManager::instance()->getShortcutFromId(V_ZoomFit));
  QAction *fit = menu.addAction(tr("Fit to Window") + "\t" + shortcut);
  connect(fit, SIGNAL(triggered()), m_imageViewer, SLOT(fitView()));

  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------
/*! If left button is pressed recall \b pick() in event pos.
*/
void ColorModelViewer::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) pick(event->pos());
}

//-----------------------------------------------------------------------------
/*! If left button is moved recall \b pick() in event pos.
*/
void ColorModelViewer::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) pick(event->pos());
}

//-----------------------------------------------------------------------------
/*! Pick color from image and set it as current style.
*/
void ColorModelViewer::pick(const QPoint &p) {
  TImageP img = m_imageViewer->getImage();
  if (!img) return;

  TPaletteHandle *ph =
      TApp::instance()->getPaletteController()->getCurrentLevelPalette();
  TPalette *currentPalette = ph->getPalette();
  if (!currentPalette) return;

  /*- 画面外ではPickできない -*/
  if (!m_imageViewer->rect().contains(p)) return;

  StylePicker picker(img, currentPalette);

  QPoint viewP = m_imageViewer->mapFrom(this, p);
  TPointD pos  = m_imageViewer->getViewAff().inv() *
                TPointD(viewP.x() - m_imageViewer->width() / 2,
                        -viewP.y() + m_imageViewer->height() / 2);

  /*---
          カレントToolに合わせてPickモードを変更
          0=Area, 1=Line, 2=Line&Areas(default)
  ---*/
  int styleIndex = picker.pickStyleId(pos, 1, m_mode);

  if (styleIndex < 0) return;

  /*-- pickLineモードのとき、取得Styleが0の場合 /
   * PurePaintの部分をクリックした場合 はカレントStyleを変えない --*/
  if (m_mode == 1) {
    if (styleIndex == 0) return;
    TToonzImageP ti = img;
    if (ti && picker.pickTone(pos) == 255) return;
  }
  /*- Paletteに存在しない色が取れた場合はreturn -*/
  TPalette::Page *page = currentPalette->getStylePage(styleIndex);
  if (!page) return;

  /*-- Styleを選択している場合は選択を解除する --*/
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  if (selection) {
    TStyleSelection *styleSelection =
        dynamic_cast<TStyleSelection *>(selection);
    if (styleSelection) styleSelection->selectNone();
  }

  /*
    if the Style Picker tool is current and "organize palette" is activated,
    then move the picked style to the first page of the palette.
  */
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool->getName() == "T_StylePicker") {
    StylePickerTool *spTool = dynamic_cast<StylePickerTool *>(tool);
    if (spTool && spTool->isOrganizePaletteActive()) {
      TPoint point = picker.getRasterPoint(pos);
      int frame    = m_flipConsole->getCurrentFrame() - 1;
      PaletteCmd::organizePaletteStyle(
          ph, styleIndex, TColorStyle::PickedPosition(point, frame));
    }
  }

  ph->setStyleIndex(styleIndex);
}

//-----------------------------------------------------------------------------

void ColorModelViewer::hideEvent(QHideEvent *e) {
  TPaletteHandle *paletteHandle = getPaletteHandle();
  TXshLevelHandle *levelHandle  = TApp::instance()->getCurrentLevel();
  ToolHandle *toolHandle        = TApp::instance()->getCurrentTool();
  disconnect(paletteHandle, SIGNAL(paletteSwitched()), this,
             SLOT(showCurrentImage()));
  disconnect(paletteHandle, SIGNAL(paletteChanged()), this,
             SLOT(showCurrentImage()));
  disconnect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
             SLOT(showCurrentImage()));

  disconnect(toolHandle, SIGNAL(toolSwitched()), this, SLOT(changePickType()));
  disconnect(toolHandle, SIGNAL(toolChanged()), this, SLOT(changePickType()));

  disconnect(levelHandle, SIGNAL(xshLevelViewChanged()), this,
             SLOT(updateViewer()));
}

//-----------------------------------------------------------------------------

void ColorModelViewer::showEvent(QShowEvent *e) {
  TPaletteHandle *paletteHandle = getPaletteHandle();
  TXshLevelHandle *levelHandle  = TApp::instance()->getCurrentLevel();
  ToolHandle *toolHandle        = TApp::instance()->getCurrentTool();
  bool ret = connect(paletteHandle, SIGNAL(paletteSwitched()), this,
                     SLOT(showCurrentImage()));
  ret = ret && connect(paletteHandle, SIGNAL(paletteChanged()), this,
                       SLOT(showCurrentImage()));
  ret = ret && connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
                       SLOT(showCurrentImage()));
  /*- ツールのTypeに合わせてPickのタイプも変え、カーソルも切り替える -*/
  ret = ret && connect(toolHandle, SIGNAL(toolSwitched()), this,
                       SLOT(changePickType()));
  ret = ret && connect(toolHandle, SIGNAL(toolChanged()), this,
                       SLOT(changePickType()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelViewChanged()), this,
                       SLOT(updateViewer()));
  assert(ret);
  changePickType();
  showCurrentImage();
}

//-----------------------------------------------------------------------------
/*- ツールのTypeに合わせてPickのタイプも変え、カーソルも切り替える -*/
void ColorModelViewer::changePickType() {
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool->getName() == T_StylePicker) {
    StylePickerTool *stylePickerTool = dynamic_cast<StylePickerTool *>(tool);
    if (stylePickerTool->isOrganizePaletteActive()) {
      setToolCursor(m_imageViewer, ToolCursor::PickerCursorOrganize);
      return;
    }
  }

  TPropertyGroup *propGroup = tool->getProperties(0);
  /*- Propertyの無いツールの場合 -*/
  if (!propGroup) {
    m_mode = 2;
    setToolCursor(m_imageViewer, ToolCursor::PickerCursor);
    return;
  }

  /*- Mode: の無いツールの場合は0が返る -*/
  TProperty *modeProp = propGroup->getProperty("Mode:");
  if (!modeProp) {
    m_mode = 2;
    setToolCursor(m_imageViewer, ToolCursor::PickerCursor);
    return;
  }

  else {
    std::string var = modeProp->getValueAsString();
    if (var == LINES) {
      m_mode = 1;
      setToolCursor(m_imageViewer, ToolCursor::PickerCursorLine);
    } else if (var == AREAS) {
      m_mode = 0;
      setToolCursor(m_imageViewer, ToolCursor::PickerCursorArea);
    } else  // Line & Areas
    {
      m_mode = 2;
      setToolCursor(m_imageViewer, ToolCursor::PickerCursor);
    }
  }
}

//-----------------------------------------------------------------------------
/*! If current palette level exists reset image viewer and set current viewer
    to refences image path level.
*/

void ColorModelViewer::updateViewer() { getImageViewer()->repaint(); }

//------------------------------------------------------------------------------

void ColorModelViewer::showCurrentImage() {
  TPalette *palette = getPaletteHandle()->getPalette();
  if (!palette) {
    resetImageViewer();
    return;
  }
  TApp *app     = TApp::instance();
  TXshLevel *xl = app->getCurrentLevel()->getLevel();
  if (!xl) {
    resetImageViewer();
    return;
  }
  /*-- UseCurrentFrameの場合、paletteを差し替える --*/
  if (palette->getRefImgPath() == xl->getPath()) {
    TXshSimpleLevel *sl = xl->getSimpleLevel();
    if (!sl) {
      resetImageViewer();
      return;
    }
    /*- 同じTFrameIdだった場合、パレットを差し替える -*/
    if (m_currentRefImgPath == xl->getPath()) {
      TImageP refImg = m_imageViewer->getImage();
      if (!refImg) {
        resetImageViewer();
        return;
      }

      TImageP refImg_clone = refImg->cloneImage();
      refImg_clone->setPalette(palette);
      m_imageViewer->setImage(refImg_clone);
      return;
    }
    /*- UseCurrentFrameのLevelに移動してきた場合、Levelを入れ直す -*/
    else {
      reloadCurrentFrame();
      return;
    }
  }

  /*- 以下、通常のColorModelが読み込まれている場合 -*/
  resetImageViewer();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp      = scene->decodeFilePath(palette->getRefImgPath());
  if (TSystem::doesExistFileOrLevel(fp)) {
    setLevel(fp, palette);
  } else if (!fp.isEmpty())
    emit refImageNotFound();
}

//-----------------------------------------------------------------------------
/*! Clone current image and set it in viewer.
*/
void ColorModelViewer::loadCurrentFrame() {
  TApp *app     = TApp::instance();
  TXshLevel *xl = app->getCurrentLevel()->getLevel();
  if (!xl) return;
  TXshSimpleLevel *sl = xl->getSimpleLevel();
  if (!sl) return;
  /*- カレントフレームのFIdの取得 -*/
  TFrameId fid;
  if (app->getCurrentFrame()->isEditingLevel())
    fid = app->getCurrentFrame()->getFid();
  else if (app->getCurrentFrame()->isEditingScene()) {
    int columnIndex = app->getCurrentColumn()->getColumnIndex();
    TXsheet *xsh    = app->getCurrentXsheet()->getXsheet();
    if (!xsh) return;
    TXshColumn *column = xsh->getColumn(columnIndex);
    if (!column) return;
    int frame = app->getCurrentFrame()->getFrame();
    fid       = column->getCellColumn()->getCell(frame).getFrameId();
  }
  TImageP img = sl->getFrame(fid, true);

  TPalette *currentPalette =
      app->getPaletteController()->getCurrentLevelPalette()->getPalette();
  if (!img || !currentPalette) return;
  TImageP refImg = img->cloneImage();
  currentPalette->setRefImg(TImageP());
  /*--onPaletteSwitchedでRefImagePathを見て、自分自身のパスを参照していたらUpdateのルーチンを切り替える--*/
  currentPalette->setRefImgPath(xl->getPath());

  std::vector<TFrameId> fids;
  fids.push_back(fid);
  currentPalette->setRefLevelFids(fids);

  m_currentRefImgPath = xl->getPath();

  m_levels.clear();

  m_levelNames.clear();
  m_framesCount = 1;
  m_palette     = 0;
  m_lr          = TLevelReaderP();
  m_dim         = TDimension(0, 0);
  m_loadbox     = TRect();
  m_loadboxes.clear();
  m_flipConsole->enableProgressBar(false);
  m_flipConsole->setProgressBarStatus(0);
  m_flipConsole->setFrameRange(1, 1, 1);
  m_title1 = m_viewerTitle + " :: " +
             m_currentRefImgPath.withoutParentDir().withFrame(fid);
  m_title = "  ::  <Use Current Frame>";

  m_imageViewer->setImage(refImg);
}

//-----------------------------------------------------------------------------
/*--
 * UseCurrentFrameのLevelに移動してきたときに、改めてCurrentFrameを格納しなおす
 * --*/
void ColorModelViewer::reloadCurrentFrame() {
  TApp *app     = TApp::instance();
  TXshLevel *xl = app->getCurrentLevel()->getLevel();
  if (!xl) return;
  TXshSimpleLevel *sl = xl->getSimpleLevel();
  if (!sl) return;

  TPalette *currentPalette =
      app->getPaletteController()->getCurrentLevelPalette()->getPalette();
  if (!currentPalette) return;

  std::vector<TFrameId> fids = currentPalette->getRefLevelFids();
  /*- CurrentFrameなので、1Frameのみのはず -*/
  if (fids.size() != 1) return;

  TImageP img = sl->getFrame(fids[0], true);
  if (!img) return;

  TImageP refImg = img->cloneImage();

  currentPalette->setRefImg(TImageP());

  currentPalette->setRefImgPath(xl->getPath());

  /*- currentRefImgPathの更新 -*/
  m_currentRefImgPath = xl->getPath();

  m_levels.clear();

  m_levelNames.clear();
  m_framesCount = 1;
  m_palette     = 0;
  m_lr          = TLevelReaderP();
  m_dim         = TDimension(0, 0);
  m_loadbox     = TRect();
  m_loadboxes.clear();
  m_flipConsole->enableProgressBar(false);
  m_flipConsole->setProgressBarStatus(0);
  m_flipConsole->setFrameRange(1, 1, 1);
  m_title1 = m_viewerTitle + " :: " +
             m_currentRefImgPath.withoutParentDir().withFrame(fids[0]);
  m_title = "  ::  <Use Current Frame>";

  m_imageViewer->setImage(refImg);
}

//-----------------------------------------------------------------------------
/*! Remove reference image from current palette using
                \b PaletteCmd::removeReferenceImage() and reset image viewer.
*/
void ColorModelViewer::removeColorModel() {
  PaletteCmd::removeReferenceImage(getPaletteHandle());
  resetImageViewer();
  m_currentRefImgPath = TFilePath();
}

//-----------------------------------------------------------------------------

void ColorModelViewer::onRefImageNotFound() {
  DVGui::info(
      tr("It is not possible to retrieve the color model set for the current "
         "level."));
}

//-----------------------------------------------------------------------------

void ColorModelViewer::repickFromColorModel() {
  TImageP img = m_imageViewer->getImage();
  if (!img) return;
  TPaletteHandle *ph =
      TApp::instance()->getPaletteController()->getCurrentLevelPalette();

  PaletteCmd::pickColorByUsingPickedPosition(
      ph, img, m_flipConsole->getCurrentFrame() - 1);
}

//=============================================================================

OpenFloatingPanel openColorModelCommand(MI_OpenColorModel, "ColorModel",
                                        QObject::tr("Color Model"));
