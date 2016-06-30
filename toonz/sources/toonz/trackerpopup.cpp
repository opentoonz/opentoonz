

#include "trackerpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "filmstripselection.h"
#include "cellselection.h"
#include "menubarcommandids.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/tool.h"

// TnzQt includes
#include "toonzqt/doublefield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"

// TnzLib includes
#include "toonz/hook.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tcamera.h"

// TnzCore includes
#include "tlevel_io.h"
#include "tpalette.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "tropcm.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tsystem.h"

// Qt includes
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMainWindow>

using namespace DVGui;
using namespace std;

//=============================================================================
namespace {
//=============================================================================
// helper function

TFrameId getFidFromFrame(int frame) {
  TApp *app = TApp::instance();
  if (app->getCurrentFrame()->isEditingLevel()) {
    TXshLevel *level = app->getCurrentLevel()->getLevel();
    assert(level);
    if (level->getFrameCount() < frame)
      return TFrameId();
    else
      return TFrameId(frame);
  }
  int columnIndex = app->getCurrentColumn()->getColumnIndex();
  TXsheet *xsh    = app->getCurrentXsheet()->getXsheet();
  assert(xsh);
  TXshColumn *column = xsh->getColumn(columnIndex);
  assert(column);
  TXshCellColumn *cellColumn = column->getCellColumn();
  if (frame > cellColumn->getRowCount()) return TFrameId();
  TXshCell cell = cellColumn->getCell(frame - 1);
  return cell.getFrameId();
}

//-----------------------------------------------------------------------------

TRaster32P resampleRaster(TRaster32P raster, const TAffine &affine) {
  if (affine.isIdentity()) return raster;
  TRect rasRect  = raster->getBounds();
  TRectD newRect = affine * convert(rasRect);
  TRaster32P app(TDimension(newRect.getLx(), newRect.getLy()));
  app->clear();
  TRop::resample(app, raster, affine);
  return app;
}

//-----------------------------------------------------------------------------

TRaster32P loadFrame(int frame, const TAffine &affine) {
  TFrameId id = getFidFromFrame(frame);
  if (id.isNoFrame()) return (TRaster32P)0;

  TImageP img = TApp::instance()->getCurrentLevel()->getSimpleLevel()->getFrame(
      id, false);
  if (!img) return (TRaster32P)0;
  TPaletteP plt    = img->getPalette();
  TRasterImageP ri = img;
  TVectorImageP vi = img;
  TToonzImageP ti  = img;
  if (ri) {
    TRaster32P raster = TRaster32P(ri->getRaster());
    return resampleRaster(raster, affine);
  } else if (vi) {
    assert(plt);
    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    if (!camera) return TRaster32P();
    TDimension size = camera->getRes();

    TVectorImageP vimage = vi->clone();
    vimage->transform(TTranslation((size.lx - 1) * 0.5, (size.ly - 1) * 0.5),
                      true);

    TVectorRenderData rd(TVectorRenderData::ProductionSettings(), TAffine(),
                         size, plt.getPointer());

    TOfflineGL offlineContext(size);
    offlineContext.makeCurrent();
    offlineContext.clear(TPixel32(0, 0, 0, 0));
    offlineContext.draw(vimage, rd);
    TRaster32P raster = offlineContext.getRaster();

    assert(raster->getSize() == size);
    return resampleRaster(raster, affine);
  } else if (ti) {
    assert(plt);
    TRasterCM32P CMras = ti->getCMapped();
    TRaster32P raster  = TRaster32P(CMras->getSize());
    TRop::convert(raster, CMras, plt);
    return resampleRaster(raster, affine);
  } else
    return TRaster32P();
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TrackerPopup
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
TrackerPopup::TrackerPopup(QWidget *parent, Qt::WindowFlags flags)
#else
TrackerPopup::TrackerPopup(QWidget *parent, Qt::WFlags flags)
#endif
    : Dialog(TApp::instance()->getMainWindow(), true, true, "Tracker") {
  // Su MAC i dialog modali non hanno bottoni di chiusura nella titleBar
  setModal(false);
  setWindowTitle(tr("Tracking Settings"));

  beginVLayout();

  // IntField per i parametri threshold, accuracy, smoothness
  m_threshold = new DoubleField();  // W_Threshold
  m_threshold->setFixedHeight(WidgetHeight);
  m_threshold->setRange(0, 1);
  m_threshold->setValue(0.2);
  addWidget(tr("Threshold:"), m_threshold);

  m_sensitivity = new DoubleField();  // W_Accuracy
  m_sensitivity->setFixedHeight(WidgetHeight);
  m_sensitivity->setRange(0, 1000);
  m_sensitivity->setValue(10);
  addWidget(tr("Sensitivity:"), m_sensitivity);

  m_variationWindow = new CheckBox(tr("Variable Region Size"));
  m_variationWindow->setFixedHeight(WidgetHeight);
  addWidget(m_variationWindow);

  m_activeBackground = new CheckBox(tr("Include Background"));
  m_activeBackground->setFixedHeight(WidgetHeight);
  addWidget(m_activeBackground);

  endVLayout();

  // Bottone Convert
  m_trackBtn = new QPushButton(QString(tr("Track")), this);  // W_Vectorize
  connect(m_trackBtn, SIGNAL(clicked()), this, SLOT(onTrack()));

  addButtonBarWidget(m_trackBtn);
}

//-----------------------------------------------------------------------------

bool TrackerPopup::apply() {
  float threshold      = m_threshold->getValue();
  float sensitivity    = m_sensitivity->getValue() / 1000;
  int activeBackground = m_activeBackground->isChecked();
  int manageOcclusion  = 0;
  int variationWindow  = m_variationWindow->isChecked();

  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
  TFilmstripSelection *filmstripSelection =
      dynamic_cast<TFilmstripSelection *>(selection);

  int frameStart   = 0;
  int framesNumber = 0;

  if (cellSelection) {
    TCellSelection::Range range = cellSelection->getSelectedCells();
    if (range.getColCount() == 1) {
      frameStart   = range.m_r0 + 1;
      framesNumber = range.m_r1 - range.m_r0 + 1;
    }
  } else if (filmstripSelection) {
    set<TFrameId> frameIds = filmstripSelection->getSelectedFids();
    if (!frameIds.empty()) {
      frameStart   = frameIds.begin()->getNumber();
      framesNumber = frameIds.size();
    }
  } else
    return false;

  if (framesNumber <= 0) return false;

  m_tracker =
      new Tracker(threshold, sensitivity, activeBackground, manageOcclusion,
                  variationWindow, frameStart, framesNumber);
  if (m_tracker->getLastError() != 0) {
    DVGui::warning(m_tracker->getLastError());
    return false;
  }

  m_myThread = new MyThread(m_tracker);
  connect(m_myThread, SIGNAL(finished()), this, SLOT(startNewThread()));
  m_progressBarIndex = 0;
  m_stoppedProcess   = false;
  m_progress =
      new ProgressDialog(tr("Processing..."), tr("Cancel"), m_progressBarIndex,
                         m_tracker->getFramesNumber() - 1);
  //	QPushButton* cancelButton = new QPushButton(QString("Cancel"),this);
  //  m_progress->setCancelButton(cancelButton);
  bool ret =
      connect(m_progress, SIGNAL(canceled()), this, SLOT(stopProcessing()));
  assert(ret);
  //	m_progress->setWindowModality(Qt::WindowModal);
  m_progress->show();
  m_myThread->start();

  return true;
}
void TrackerPopup::startNewThread() {
  // Interruzione del processing da parte dell'utente
  if (m_stoppedProcess) return;
  QString lastError = m_tracker->getLastError();
  if (lastError != "") {
    QMessageBox::critical(this, "Tracking Error", lastError);
    m_progress->close();
    return;
  }
  int progressBarMaxValue = m_tracker->getFramesNumber() - 1;
  m_progressBarIndex++;
  m_progress->setValue(m_progressBarIndex);
  if (m_progressBarIndex < progressBarMaxValue) {
    m_myThread->start();
  } else {
    m_progress->close();
    close();
  }
}
void TrackerPopup::stopProcessing() {
  m_myThread->quit();
  m_stoppedProcess = true;
  m_myThread->wait();
}
//-----------------------------------------------------------------------------

void TrackerPopup::onTrack() { apply(); }

//=============================================================================
// TrackerPopup
//-----------------------------------------------------------------------------

Tracker::Tracker(double threshold, double sensitivity, int activeBackground,
                 int manageOcclusion, int variationWindow, int frameStart,
                 int framesNumber)
    : m_affine() {
  m_threshold        = threshold;
  m_sensitivity      = sensitivity;
  m_activeBackground = activeBackground;
  m_manageOcclusion  = manageOcclusion;
  m_variationWindow  = variationWindow;
  m_indexFrameStart  = frameStart;
  m_framesNumber     = framesNumber;
  m_processor        = new DummyProcessor();

  m_processor->setActive(false);
  m_lastErrorCode = 0;
  setup();
}

//-----------------------------------------------------------------------------

Tracker::~Tracker() {
  int i = 0;
  for (i = 0; i < m_trackerCount; i++) delete m_pObjectTracker[i];
}

//-----------------------------------------------------------------------------

bool Tracker::setup() {
  ToolHandle *toolH = TApp::instance()->getCurrentTool();
  if (toolH && toolH->getTool()) toolH->getTool()->reset();
  TTool *tool   = toolH->getTool();
  TXshLevel *xl = TApp::instance()->getCurrentLevel()->getLevel();
  if (!xl) {
    m_lastErrorCode = 13;
    return false;
  }
  // setto il frame di partenza
  TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
  int columnIndex = TApp::instance()->getCurrentColumn()->getColumnIndex();
  TFrameId frameStartId =
      xsh->getCell(m_indexFrameStart - 1, columnIndex).getFrameId();
  if (frameStartId.isNoFrame()) {
    m_lastErrorCode = 11;
    return false;
  }
  int startFrameGlobalIndex = frameStartId.getNumber() - 1;

  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  if (!fh) {
    m_lastErrorCode = 1;
    return false;
  }
  HookSet *hookSet = xl->getHookSet();
  if (!hookSet) {
    m_lastErrorCode = 2;
    return false;
  }
  m_trackerObjectsSet = hookSet->getTrackerObjectsSet();
  if (!m_trackerObjectsSet ||
      m_trackerObjectsSet->getTrackerObjectsCount() == 0) {
    m_lastErrorCode = 2;
    return false;
  }
  TXshSimpleLevel *simpleLevel = xl->getSimpleLevel();
  if (!simpleLevel) {
    m_lastErrorCode = 13;
    return false;
  }
  TFilePath fp1 = xl->getSimpleLevel()->getPath();
  if (fp1.isEmpty()) {
    m_lastErrorCode = 12;
    return false;
  }
  TFilePath fp(xl->getSimpleLevel()->getScene()->decodeFilePath(fp1));
  if (fp.isEmpty() || !TSystem::doesExistFileOrLevel(fp)) {
    m_lastErrorCode = 14;
    return false;
  }
  TLevelReaderP levelReader;
  try {
    levelReader = TLevelReaderP(fp);
  } catch (...) {
    m_lastErrorCode = 1;
    return false;
  }
  if (!levelReader) {
    m_lastErrorCode = 1;
    return false;
  }
  m_level = levelReader->loadInfo();
  if (!m_level) {
    m_lastErrorCode = 1;
    return false;
  }

  // # elements to track
  m_trackerCount = 0;
  int k          = 0;
  for (k = 0; k < (int)m_trackerObjectsSet->getTrackerObjectsCount(); k++) {
    m_trackerCount =
        m_trackerCount +
        m_trackerObjectsSet->getObjectFromIndex(k)->getHooksCount();
  }
  if (m_trackerCount <= 0 || m_trackerCount > 30) {
    m_lastErrorCode = 3;
    return false;
  }

  // ID object
  std::unique_ptr<short[]> id(new short[m_trackerCount]);
  if (!id) {
    m_lastErrorCode = 1;
    return false;
  }

  //(x,y) coordinate object
  std::unique_ptr<short[]> x(new short[m_trackerCount]);
  if (!x) {
    m_lastErrorCode = 1;
    return false;
  }

  std::unique_ptr<short[]> y(new short[m_trackerCount]);
  if (!y) {
    m_lastErrorCode = 1;
    return false;
  }

  // Width and Height of object box
  std::unique_ptr<short[]> Width(new short[m_trackerCount]);
  if (!Width) {
    m_lastErrorCode = 1;
    return false;
  }

  std::unique_ptr<short[]> Height(new short[m_trackerCount]);
  if (!Height) {
    m_lastErrorCode = 1;
    return false;
  }

  //# start frame
  std::unique_ptr<int[]> m_numstart(new int[m_trackerCount]);
  if (!m_numstart) {
    m_lastErrorCode = 1;
    return false;
  }

  TImageP imagep = tool->getImage(false);
  if (!imagep) {
    m_lastErrorCode = 1;
    return false;
  }

  TRasterImageP ri = imagep;
  TVectorImageP vi = imagep;
  TToonzImageP ti  = imagep;

  TPaletteP plt = m_level->getPalette();
  if (ri)
    m_raster = ri->getRaster();
  else if (vi) {
    assert(plt);
    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    if (!camera) {
      m_lastErrorCode = 1;
      return false;
    }
    TDimension size = camera->getRes();

    TVectorImageP vimage = vi->clone();
    vimage->transform(TTranslation((size.lx - 1) * 0.5, (size.ly - 1) * 0.5),
                      true);

    TVectorRenderData rd(TVectorRenderData::ProductionSettings(), TAffine(),
                         size, plt.getPointer());

    TOfflineGL offlineContext(size);
    offlineContext.makeCurrent();
    offlineContext.clear(TPixel32(0, 0, 0, 0));
    offlineContext.draw(vimage, rd);
    m_raster = offlineContext.getRaster();
    assert(m_raster->getSize() == size);
  } else if (ti) {
    assert(plt);
    TRasterCM32P CMras = ti->getCMapped();
    m_raster           = TRaster32P(CMras->getSize());
    TRop::convert(m_raster, CMras, plt);
  } else {
    m_lastErrorCode = 1;
    return false;
  }

  TDimension size = m_raster->getSize();
  if (size.lx > 1024 || size.ly > 768) {
    double scaleX = 1024.0 / (double)size.lx;
    double scaleY = 768.0 / (double)size.ly;
    double factor = scaleX > scaleY ? scaleY : scaleX;
    m_affine      = TScale(factor);
    m_raster      = resampleRaster(m_raster, m_affine);
    size          = m_raster->getSize();
  }

  TFrameId fid   = toolH->getTool()->getCurrentFid();
  m_rasterOrigin = m_raster->getCenterD() - TPointD(size.lx, 0);
  // characteristics objects
  int posInitObject   = 0;
  int totalFrameCount = m_level->getFrameCount();
  int i               = 0;
  for (i = 0; i < (int)m_trackerObjectsSet->getTrackerObjectsCount(); i++) {
    int j = 0;
    for (j = 0; j < m_trackerObjectsSet->getObjectFromIndex(i)->getHooksCount();
         j++) {
      id[posInitObject + j] =
          m_trackerObjectsSet->getObjectFromIndex(i)->getId();
      Hook *hook      = m_trackerObjectsSet->getObjectFromIndex(i)->getHook(j);
      TPointD hookPos = m_affine * hook->getPos(fid);
      assert(hook);
      double globalx       = hookPos.x;
      double localx        = globalx - m_rasterOrigin.x;
      x[posInitObject + j] = localx;
      if (x[posInitObject + j] < 0 || x[posInitObject + j] > size.lx - 1) {
        m_lastErrorCode = 4;
        return false;
      }
      double globaly       = hookPos.y;
      double localy        = globaly - m_rasterOrigin.y;
      y[posInitObject + j] = -localy;
      if (y[posInitObject + j] < 0 || y[posInitObject + j] > size.ly - 1) {
        m_lastErrorCode = 4;
        return false;
      }
      TRectD trackerRect(TDimensionD(hook->getTrackerRegionWidth(),
                                     hook->getTrackerRegionHeight()));
      trackerRect              = m_affine * trackerRect;
      Width[posInitObject + j] = trackerRect.getLx();
      // controllo che le dimensioni (larghezza) della regioni siano contenute
      // nell'immagine
      if (Width[posInitObject + j] <= 0 ||
          x[posInitObject + j] - 0.5 * Width[posInitObject + j] < 0 ||
          x[posInitObject + j] + 0.5 * Width[posInitObject + j] > size.lx) {
        m_lastErrorCode = 5;
        return false;
      }
      Height[posInitObject + j] = trackerRect.getLy();
      // controllo che le dimensioni (altezza) della regioni siano contenute
      // nell'immagine
      if (Height[posInitObject + j] <= 0 ||
          y[posInitObject + j] - 0.5 * Height[posInitObject + j] < 0 ||
          y[posInitObject + j] + 0.5 * Height[posInitObject + j] > size.ly) {
        m_lastErrorCode = 6;
        return false;
      }
      m_numstart[posInitObject + j] = startFrameGlobalIndex;
      if (m_numstart[posInitObject + j] < 0 ||
          m_numstart[posInitObject + j] >= totalFrameCount - 1) {
        m_lastErrorCode = 12;
        return false;
      }
    }
    posInitObject +=
        m_trackerObjectsSet->getObjectFromIndex(i)->getHooksCount();
  }
  int numframe = m_framesNumber;
  if (numframe < 1 || (fh->getMaxFrameIndex() - m_indexFrameStart + 2) < 0) {
    m_lastErrorCode = 8;
    return false;
  }
  if (numframe >= (fh->getMaxFrameIndex() - m_indexFrameStart) + 2) {
    numframe       = (fh->getMaxFrameIndex() - m_indexFrameStart) + 2;
    m_framesNumber = numframe;
  }

  // dimension searce template area
  short dimtemp = 8;
  // variation dimension window
  short vardim = m_variationWindow;
  // threshold lose object (null =0)
  float threshold_dist = m_threshold;

  if ((threshold_dist < 0.0) || (threshold_dist > 1.0)) {
    m_lastErrorCode = 9;
    return false;
  }
  float threshold_distB = 0.6 * threshold_dist;

  if ((m_sensitivity < 0) || (m_sensitivity > 1)) {
    m_lastErrorCode = 10;
    return false;
  }

  m_currentFrame = m_indexFrameStart;
  // update current frame
  m_raster = loadFrame(m_currentFrame, m_affine);
  if (!m_raster) {
    m_lastErrorCode = 1;
    return false;
  }
  m_processor->setActive(false);
  m_processor->process(m_raster);

  // Store the old value of m_processor
  bool status = m_processor->isActive();

  // Set processing false
  m_processor->setActive(false);

  bool image_c          = true;
  bool image_background = false;
  bool occl             = m_manageOcclusion;

  // costruct trackers

  for (i = 0; i < m_trackerCount; i++) {
    // costruct
    m_pObjectTracker[i] =
        new CObjectTracker(size.lx, size.ly, image_c, image_background, occl);

    // initialization
    m_pObjectTracker[i]->ObjectTrackerInitObjectParameters(
        id[i], x[i], y[i], Width[i], Height[i], dimtemp, vardim, threshold_dist,
        threshold_distB);
  }

  m_numobjactive = 0;

  for (i = 0; i < m_trackerCount; i++) {
    if (m_numstart[0] == m_numstart[i])
      m_numobjactive++;
    else
      break;
  }

  m_raster_template = new TRaster32P[m_trackerCount];

  for (i = 0; (i < m_numobjactive); i++) {
    // tracking	object first frame
    m_pObjectTracker[i]->ObjeckTrackerHandlerByUser(&m_raster);
    m_raster_template[i] = m_raster;
  }

  m_num = m_numstart[0] + 1;
  ++m_currentFrame;
  m_trackerRegionIndex = 0;
  m_oldObjectId        = 0;

  return true;
}

//-----------------------------------------------------------------------------

bool Tracker::trackCurrentFrame() {
  m_raster_prev = m_raster;

  // update Current Frame;
  m_raster = loadFrame(m_currentFrame, m_affine);
  if (!m_raster) {
    m_lastErrorCode = 1;
    return false;
  }
  m_processor->process(m_raster);
  short app1 = 0;
  app1       = m_numobjactive;

  // control active object
  int i = 0;
  for (i = m_numobjactive; (i < m_trackerCount); i++) {
    if (m_num == m_numstart[m_numobjactive])
      m_numobjactive++;
    else
      break;
  }

  for (i = 0; i < app1; i++) {
    // tracking old objects
    if (m_pObjectTracker[i]->track)
      m_pObjectTracker[i]->ObjeckTrackerHandlerByUser(&m_raster);
    float dist_temp;
    dist_temp = m_pObjectTracker[i]->Matching(&m_raster, &m_raster_template[i]);
    if ((dist_temp < m_sensitivity) && (m_pObjectTracker[i]->track)) {
      m_raster_template[i] = m_raster;
      m_pObjectTracker[i]->updateTemp();
    }
  }
  // update neighbours
  for (i = 0; i < app1; i++) {
    m_pObjectTracker[i]->DistanceReset();
    for (int k = 0; k < app1; k++) {
      if ((m_pObjectTracker[k]->track) && (i != k) &&
          (m_pObjectTracker[i]->objID == m_pObjectTracker[k]->objID)) {
        NEIGHBOUR position = m_pObjectTracker[k]->GetPosition();
        m_pObjectTracker[i]->DistanceAndUpdate(position);
        if (!m_pObjectTracker[i]->GetInit()) {
          m_pObjectTracker[i]->SetInitials(m_pObjectTracker[i]->GetPosition());
        }
      }
    }
    m_pObjectTracker[i]->SetInit(true);
  }
  Predict3D::Point current[30], initials[30];
  bool visible[30];
  // datiPredict<<endl<<"-----------------------------"<<endl<<endl;
  for (i = 0; i < app1; i++) {
    visible[i]         = m_pObjectTracker[i]->track;
    initials[i]        = m_pObjectTracker[i]->GetInitials();
    NEIGHBOUR position = m_pObjectTracker[i]->GetPosition();
    current[i].x       = position.X;
    current[i].y       = position.Y;
  }
  int k_dist;
  for (i = 0; i < app1; i++) {
    if (m_pObjectTracker[i]->GetVisibility() == "WARNING") {
      if (Predict3D::Predict(app1, initials, current, visible)) {
        k_dist             = m_pObjectTracker[i]->GetKDist();
        NEIGHBOUR position = m_pObjectTracker[i]->GetPosition();
        m_pObjectTracker[i]->SetPosition(
            (k_dist * (current[i].x) + (1 - k_dist) * (position.X)),
            (k_dist * (current[i].y) + (1 - k_dist) * (position.Y)));
      }
    }
    // Set position by neighbours
    if (!m_pObjectTracker[i]->track) {
      if (Predict3D::Predict(app1, initials, current, visible)) {
        m_pObjectTracker[i]->SetPosition(current[i].x, current[i].y);
      } else {
        m_pObjectTracker[i]->SetVisibility("LOST");
      }
    }
    short objectId = m_pObjectTracker[i]->getId();
    if (objectId != m_oldObjectId) {
      m_trackerRegionIndex = 0;
      m_oldObjectId        = objectId;
    }

    double globalx = m_rasterOrigin.x + m_pObjectTracker[i]->GetPosition().X;
    double globaly = m_rasterOrigin.y - m_pObjectTracker[i]->GetPosition().Y;

    Hook *hook =
        m_trackerObjectsSet->getObject(objectId)->getHook(m_trackerRegionIndex);
    hook->setAPos(getFidFromFrame(m_currentFrame),
                  m_affine.inv() * TPointD(globalx, globaly));
    m_trackerRegionIndex++;
    if (m_trackerRegionIndex >=
        m_trackerObjectsSet->getObject(objectId)->getHooksCount()) {
      m_trackerRegionIndex = 0;
    }
  }

  // tracking new objects
  for (i = app1; i < m_numobjactive; i++) {
    m_pObjectTracker[i]->ObjeckTrackerHandlerByUser(&m_raster);
    m_raster_template[i] = m_raster;
  }

  ++m_currentFrame;
  ++m_num;
  return true;
}

//-----------------------------------------------------------------------------

QString Tracker::getErrorMessage(int errorCode) {
  QString errorMessage;
  switch (errorCode) {
  case 0:  // No Error
    errorMessage = QObject::tr("");
    break;
  case 1:
    errorMessage = QObject::tr(
        "It is not possible to track the level:\nallocation error.");
    break;
  case 2:
    errorMessage = QObject::tr(
        "It is not possible to track the level:\nno region defined.");
    break;
  case 3:
    errorMessage = QObject::tr(
        "It is not possible to track specified regions:\nmore than 30 regions "
        "defined.");
    break;
  case 4:
    errorMessage = QObject::tr(
        "It is not possible to track specified regions:\ndefined regions are "
        "not valid.");
    break;
  case 5:
    errorMessage = QObject::tr(
        "It is not possible to track specified regions:\nsome regions are too "
        "wide.");
    break;
  case 6:
    errorMessage = QObject::tr(
        "It is not possible to track specified regions:\nsome regions are too "
        "high.");
    break;
  case 7:
    errorMessage = QObject::tr("Frame Start Error");
    break;
  case 8:
    errorMessage = QObject::tr("Frame End Error");
    break;
  case 9:
    errorMessage = QObject::tr("Threshold Distance Error");
    break;
  case 10:
    errorMessage = QObject::tr("Sensitivity Error");
    break;
  case 11:
    errorMessage = QObject::tr("No Frame Found");
    break;
  case 12:
    errorMessage = QObject::tr(
        "It is not possible to track specified regions:\nthe selected level is "
        "not valid.");
    break;
  case 13:
    errorMessage = QObject::tr(
        "It is not possible to track the level:\nno level selected.");
    break;
  case 14:
    errorMessage = QObject::tr(
        "It is not possible to track specified regions:\nthe level has to be "
        "saved first.");
    break;
  default:
    errorMessage =
        QObject::tr("It is not possible to track the level:\nundefined error.");
  }
  return errorMessage;
}

//-----------------------------------------------------------------------------

MyThread::MyThread(Tracker *tracker) { m_tracker = tracker; }

//-----------------------------------------------------------------------------

void MyThread::run() { m_tracker->trackCurrentFrame(); }

//=============================================================================
// TrackerPopupCommand
//-----------------------------------------------------------------------------

OpenPopupCommandHandler<TrackerPopup> openTrackerPopup(MI_Tracking);
