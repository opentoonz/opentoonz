

#include "meshifypopup.h"

// Toonz includes
#include "tapp.h"
#include "cellselection.h"
#include "columnselection.h"
#include "selectionutils.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/levelproperties.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshmeshcolumn.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/stage.h"
#include "toonz/namebuilder.h"
#include "toonz/levelset.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/stagevisitor.h"

// TnzExt includes
#include "ext/meshbuilder.h"
#include "ext/meshutils.h"

// TnzCore includes
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tmsgcore.h"
#include "tsystem.h"
#include "tundo.h"

// Qt includes
#include <QStringList>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QShowEvent>
#include <QHideEvent>
#include <QMainWindow>

// Tunable parameters

#define RENDERED_IMAGES_MAX_LATERAL_RESOLUTION 2048
#define MESH_TARGET_MAX_VERTICES_COUNT 1000

//********************************************************************************************
//    Local classes
//********************************************************************************************

namespace {

struct MeshifyOptions  //!  Available options for the meshification process.
{
  double m_edgesLength;  //!< The desired mesh edges length, in standard world
                         //! coordinates
  double m_rasterizationDpi;  //!< Dpi value used to render PLIs and sub-xsheets
  int m_margin;               //!< Margin to the original shape (in pixels)
};

}  // namespace

//********************************************************************************************
//    Local functions
//********************************************************************************************

namespace {

TRaster32P render(const TVectorImageP &vi, double &rasDpi, int margin,
                  TPointD &worldOriginPos) {
  // Extract vi's bbox
  TRectD bboxD(vi->getBBox());
  if (bboxD.x0 >= bboxD.x1 || bboxD.y0 >= bboxD.y1) return TRaster32P();

  // Build the scale factor from scene's to specified dpi
  double scale = rasDpi / Stage::inch;

  // Ensure that the maximum lateral resolution is respected
  if (scale * bboxD.getLx() > RENDERED_IMAGES_MAX_LATERAL_RESOLUTION ||
      scale * bboxD.getLy() > RENDERED_IMAGES_MAX_LATERAL_RESOLUTION) {
    scale = std::min(RENDERED_IMAGES_MAX_LATERAL_RESOLUTION / bboxD.getLx(),
                     RENDERED_IMAGES_MAX_LATERAL_RESOLUTION / bboxD.getLy());
  }

  bboxD = TScale(scale) * bboxD;

  // Enlarge by margin+1 pix. This is necessary since:
  //  1. We'd better be sure about drawing. This accounts for 1-pix enlargement
  //  2. A margin enlargement is enforced by the MeshBuilder.
  bboxD = bboxD.enlarge(margin + 1);

  TRect bbox(tfloor(bboxD.x0), tfloor(bboxD.y0), tceil(bboxD.x1) - 1,
             tceil(bboxD.y1) - 1);

  worldOriginPos = -convert(bbox.getP00());
  rasDpi         = scale * Stage::inch;

  // Initialize a corresponding OpenGL context
  std::unique_ptr<TOfflineGL> offlineGlContext(new TOfflineGL(bbox.getSize()));
  offlineGlContext->makeCurrent();

  // Draw the image
  TVectorRenderData rd(TTranslation(worldOriginPos) * TScale(scale),
                       TRect(bbox.getSize()), vi->getPalette(), 0, true, true);
  offlineGlContext->draw(vi, rd, false);

  return offlineGlContext->getRaster();
}

//-----------------------------------------------------------------------

TRaster32P render(const TXsheet *xsh, int row, double &rasDpi, int margin,
                  TPointD &worldOriginPos) {
  // Extract xsh's bbox
  TRectD bbox(xsh->getBBox(row));
  if (bbox.x0 >= bbox.x1 || bbox.y0 >= bbox.y1) return TRaster32P();

  // Since xsh represents a sub-xsheet, its camera affine must be applied
  const TAffine &cameraAff =
      xsh->getPlacement(xsh->getStageObjectTree()->getCurrentCameraId(), row);
  bbox = cameraAff.inv() * bbox;

  // Build the scale factor from scene's to specified dpi
  double scale = rasDpi / Stage::inch;

  // Ensure that the maximum lateral resolution is respected
  if (scale * bbox.getLx() > RENDERED_IMAGES_MAX_LATERAL_RESOLUTION ||
      scale * bbox.getLy() > RENDERED_IMAGES_MAX_LATERAL_RESOLUTION) {
    scale = std::min(RENDERED_IMAGES_MAX_LATERAL_RESOLUTION / bbox.getLx(),
                     RENDERED_IMAGES_MAX_LATERAL_RESOLUTION / bbox.getLy());
  }

  bbox = TScale(scale) * bbox;

  // Enlarge by margin+1 pix. This is necessary since:
  //  1. We'd better be sure about drawing. This accounts for 1-pix enlargement
  //  2. A margin enlargement is enforced by the MeshBuilder.
  bbox = bbox.enlarge(margin + 1);

  bbox =
      TRectD(tfloor(bbox.x0), tfloor(bbox.y0), tceil(bbox.x1), tceil(bbox.y1));

  worldOriginPos = -bbox.getP00();
  rasDpi         = scale * Stage::inch;

  // Draw the xsheet
  TRaster32P ras(tround(bbox.getLx()), tround(bbox.getLy()));

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->renderFrame(ras, row, xsh, bbox, TScale(scale));

  return ras;
}

//-----------------------------------------------------------------------

inline void xshPos(int &r, int &c) {
  TApp *app = TApp::instance();

  r = app->getCurrentFrame()->getFrame();
  c = app->getCurrentColumn()->getColumnIndex();
}

//-----------------------------------------------------------------------

inline const TXshCell &xshCell() {
  TApp *app = TApp::instance();
  return app->getCurrentXsheet()->getXsheet()->getCell(
      app->getCurrentFrame()->getFrame(),
      app->getCurrentColumn()->getColumnIndex());
}

//-----------------------------------------------------------------------

// Creates a suitable mesh level from a given one. Observe that the output
// mesh level has "levelName..mesh" format.
TXshSimpleLevel *createMeshLevel(TXshLevel *texturesLevel) {
  struct locals {
    static void copyLevelProperties(TXshLevel *src, TXshSimpleLevel *dstSl) {
      LevelProperties *dstProp = dstSl->getProperties();
      dstProp->setDpiPolicy(LevelProperties::DP_ImageDpi);
      dstProp->setDpi(TPointD(Stage::inch, Stage::inch));

      TXshSimpleLevel *srcSl = dynamic_cast<TXshSimpleLevel *>(src);
      if (srcSl && srcSl->getType() != PLI_XSHLEVEL) {
        LevelProperties *srcProp = srcSl->getProperties();

        // Assign the original dpi ONLY if it proves to be not null
        const TPointD &srcDpi = srcProp->getDpi();
        if (srcDpi.x != 0.0 && srcDpi.y != 0.0) {
          dstProp->setDpiPolicy(srcProp->getDpiPolicy());
          dstProp->setDpi(srcProp->getDpi());
        }
      }
    }
  };  // locals

  TXshSimpleLevel *result = 0;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  // Build a suitable name (avoid duplicate names from the xsheet)
  std::wstring levelName = texturesLevel->getName() + L"_mesh";
  {
    NameBuilder *nameBuilder = NameBuilder::getBuilder(levelName);

    while (scene->getLevelSet()->getLevel(levelName))
      levelName = nameBuilder->getNext();

    delete nameBuilder;
  }

  // Build a suitable path (avoid duplicate names from the file system AND
  // scene's level set)
  TFilePath origPath, dstPath, codedOrigPath, codedDstPath;
  TXshSimpleLevel *origSl = 0;

  {
    std::wstring pathName = texturesLevel->getName();

    codedOrigPath = texturesLevel->getPath();
    if (dynamic_cast<TXshChildLevel *>(
            texturesLevel))  // Sub-xsheets do not have a suitable
      codedOrigPath = TFilePath(
          "+drawings/a");  // parent directory. Store them in "+drawings".

    codedOrigPath = codedDstPath =
        codedOrigPath.withName(pathName).withType("mesh").withFrame(
            TFrameId::EMPTY_FRAME);

    origPath = dstPath = scene->decodeFilePath(codedOrigPath);

    {
      NameBuilder *nameBuilder = NameBuilder::getBuilder(pathName);

      while (TSystem::doesExistFileOrLevel(dstPath) ||
             scene->getLevelSet()->hasLevel(*scene, codedDstPath)) {
        pathName = nameBuilder->getNext();

        codedDstPath = origPath.withName(pathName).withType("mesh").withFrame(
            TFrameId::EMPTY_FRAME);
        dstPath = scene->decodeFilePath(codedDstPath);
      }

      delete nameBuilder;
    }

    origSl = dynamic_cast<TXshSimpleLevel *>(
        scene->getLevelSet()->getLevel(*scene, codedOrigPath));
  }

  // In case the preferred mesh path is already occupied, ask the user what to
  // do
  if (codedOrigPath != codedDstPath) {
    // Prompt for an answer
    enum { CANCEL = 0, DELETE_OLD, OVERWRITE_OLD, KEEP_OLD };

    int answer = -1;
    {
      QString question(
          MeshifyPopup::tr("A level with the preferred path \"%1\" already "
                           "exists.\nWhat do you want to do?")
              .arg(QString::fromStdWString(codedOrigPath.getWideString())));

      QStringList optionsList;
      optionsList << MeshifyPopup::tr("Delete the old level entirely")
                  << MeshifyPopup::tr(
                         "Keep the old level and overwrite processed frames")
                  << MeshifyPopup::tr("Choose a different path (%1)")
                         .arg(QString::fromStdWString(
                             codedDstPath.getWideString()));

      answer = DVGui::RadioButtonMsgBox(DVGui::QUESTION, question, optionsList,
                                        TApp::instance()->getMainWindow());
    }

    // Apply decision
    switch (answer) {
    case CANCEL:
      return result;

    case DELETE_OLD:
      // Remove the level on disk
      TSystem::removeFileOrLevel(origPath);
      if (origSl) {
        removeIcons(origSl, origSl->getFids());
        origSl->clearFrames();
        origSl->setDirtyFlag(true);

        locals::copyLevelProperties(texturesLevel, origSl);
      }

      codedDstPath = codedOrigPath;
      dstPath      = origPath;
      break;

    case OVERWRITE_OLD:
      if (origSl) {
        removeIcons(origSl, origSl->getFids());  // Invalidate the levels' icons
        origSl->setDirtyFlag(true);
      }

      codedDstPath = codedOrigPath;
      dstPath      = origPath;
      break;
    }
  }

  // Build the TXshSimpleLevel instance
  if (codedDstPath == codedOrigPath && origSl) {
    // Return an already existing level
    result = origSl;
    assert(result);
  } else {
    // Create a new level
    result = dynamic_cast<TXshSimpleLevel *>(
        scene->createNewLevel(MESH_XSHLEVEL, levelName));
    assert(result);

    result->setPath(codedDstPath);

    locals::copyLevelProperties(texturesLevel, result);
  }

  return result;
}

//-----------------------------------------------------------------------

void makeMeshColumn(TXsheet &xsh, int texColId, int meshColIdx) {
  TXshMeshColumn *meshColumn = new TXshMeshColumn;
  xsh.insertColumn(meshColIdx, meshColumn);

  TStageObject *colObj = xsh.getStageObject(TStageObjectId::ColumnId(texColId));
  TStageObject *meshColObj =
      xsh.getStageObject(TStageObjectId::ColumnId(meshColIdx));

  const std::string &meshColName = colObj->getName() + "_mesh";

  meshColObj->setParent(colObj->getParent());
  meshColObj->setParentHandle(colObj->getParentHandle());
  meshColObj->setName(meshColName);

  colObj->setParent(TStageObjectId::ColumnId(meshColIdx));
}

//-----------------------------------------------------------------------

TStageObjectId firstChildLevelColumn(const TXsheet &xsh,
                                     const TStageObject &obj) {
  TStageObjectId result;

  const std::list<TStageObject *> &children = obj.getChildren();

  std::list<TStageObject *>::const_iterator ct, cEnd(children.end());
  for (ct = children.begin(); ct != cEnd; ++ct) {
    const TStageObjectId &id = (*ct)->getId();
    if (!id.isColumn() ||
        xsh.getColumn(id.getIndex())->getColumnType() != TXshColumn::eLevelType)
      continue;

    if (!result.isColumn() || id < result) result = id;
  }

  return result;
}

//-----------------------------------------------------------------------

TXshLevel *levelToMeshify() {
  TXshLevel *result = 0;

  int r, c;
  ::xshPos(r, c);

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  TXshColumn *col = xsh->getColumn(c);
  if (col) {
    TXshMeshColumn *meshCol = col->getMeshColumn();
    if (meshCol) {
      TStageObject *meshObj = xsh->getStageObject(TStageObjectId::ColumnId(c));

      // Retrieve the mesh cell
      const TXshCell &cell = xsh->getCell(r, c);

      TXshSimpleLevel *ml = cell.getSimpleLevel();
      if (ml && ml->getType() == MESH_XSHLEVEL) {
        // Retrieve the associated texture
        const TStageObjectId &childId = firstChildLevelColumn(*xsh, *meshObj);

        const TXshCell &texCell = xsh->getCell(r, childId.getIndex());

        TXshLevel *xl = texCell.m_level.getPointer();
        if (xl &&
            (xl->getType() & LEVELCOLUMN_XSHLEVEL))  // Includes sub-xsheet type
          result = xl;
      }
    } else {
      const TXshCell &cell = xsh->getCell(r, c);

      TXshLevel *xl = cell.m_level.getPointer();
      if (xl && (xl->getType() & LEVELCOLUMN_XSHLEVEL)) result = xl;
    }
  }

  return result;
}

//-----------------------------------------------------------------------

TMeshImageP meshify(const TXshCell &cell, const MeshifyOptions &options);
TMeshImageP meshify(const TRasterP &ras, const TPointD &rasDpi,
                    const TPointD &worldOriginPos, const TPointD &meshDpi,
                    const MeshBuilderOptions &options);

bool meshifySelection(const MeshifyOptions &options);

}  // namespace

//********************************************************************************************
//    Cell to image locals
//********************************************************************************************

namespace {

void getRaster(const TImageP &img, TPointD &imgDpi, TRasterP &ras,
               TPointD &rasDpi, TPointD &worldOriginPos, int margin) {
  TRasterImageP ri(img);
  if (ri) {
    ras = ri->getRaster();

    worldOriginPos = ras->getCenterD();

    ri->getDpi(imgDpi.x, imgDpi.y);
    rasDpi = imgDpi;
  }

  TToonzImageP ti(img);
  if (ti) {
    ras = ti->getRaster();

    worldOriginPos = ras->getCenterD();

    ti->getDpi(imgDpi.x, imgDpi.y);
    rasDpi = imgDpi;
  }

  TVectorImageP vi(img);
  if (vi) {
    ras      = render(vi, rasDpi.x, margin, worldOriginPos);
    rasDpi.y = rasDpi.x;
    imgDpi   = TPointD(Stage::inch, Stage::inch);
  }
}

//-----------------------------------------------------------------------

void getRaster(const TXsheet *xsh, int row, TRasterP &ras, TPointD &rasDpi,
               TPointD &worldOriginPos, int margin) {
  ras      = render(xsh, row, rasDpi.x, margin, worldOriginPos);
  rasDpi.y = rasDpi.x;
}

}  // namespace

//********************************************************************************************
//    MeshifyPopup::Swatch  definition
//********************************************************************************************

class MeshifyPopup::Swatch final : public PlaneViewer {
public:
  TImageP m_img;  //!< The eventual image to be meshified

  TXsheetP m_xsh;  //!< The eventual sub-xsheet to be meshified...
  int m_row;       //!< ... at this row (frame)

  TMeshImageP m_meshImg;  //!< The mesh image preview

public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent), m_row(-1) {}

  void clear() {
    m_img = TImageP(), m_xsh = TXsheetP(), m_row = -1,
    m_meshImg = TMeshImageP();
  }

  void paintGL() override {
    drawBackground();

    // Draw original
    if (m_img) {
      pushGLWorldCoordinates();

      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // Note GL_ONE instead of GL_SRC_ALPHA: it's needed since the input
      // image is supposedly premultiplied - and it works because the
      // viewer's background is opaque.
      // See tpixelutils.h's overPixT function for comparison.

      draw(m_img);

      glDisable(GL_BLEND);

      popGLCoordinates();
    } else if (m_xsh) {
      // Build reference change affines

      // EXPLANATION: RasterPainter receives an affine specifiying the reference
      // change
      // from world coordinates to the OpenGL viewport, where (0,0) corresponds
      // to the
      // viewport center.

      const TAffine &cameraAff = m_xsh->getPlacement(
          m_xsh->getStageObjectTree()->getCurrentCameraId(), m_row);

      TTranslation centeredWidgetToWidgetAff(0.5 * width(), 0.5 * height());
      const TAffine &worldToCenteredWigetAff =
          centeredWidgetToWidgetAff.inv() * viewAff() * cameraAff.inv();

      glPushMatrix();
      tglMultMatrix(centeredWidgetToWidgetAff);

      assert(m_row >= 0);

      ImagePainter::VisualSettings vs;

      TDimension viewerSize(width(), height());
      Stage::RasterPainter painter(viewerSize, worldToCenteredWigetAff,
                                   TRect(viewerSize), vs, false);

      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      Stage::visit(painter, scene, m_xsh.getPointer(), m_row);

      painter.flushRasterImages();
      glFlush();

      glPopMatrix();
    }

    // Draw mesh preview
    if (m_meshImg) {
      glEnable(GL_BLEND);
      glEnable(GL_LINE_SMOOTH);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // Retrieve mesh dpi
      TPointD meshDpi;
      m_meshImg->getDpi(meshDpi.x, meshDpi.y);

      // Push mesh-to-world coordinates change
      pushGLWorldCoordinates();
      glScaled(Stage::inch / meshDpi.x, Stage::inch / meshDpi.y, 1.0);

      glColor4f(0.0, 1.0, 0.0, 0.7);  // Translucent green
      tglDrawEdges(*m_meshImg);

      popGLCoordinates();

      glDisable(GL_LINE_SMOOTH);
      glDisable(GL_BLEND);
    }
  }
};

//********************************************************************************************
//    Meshify Popup  implementation
//********************************************************************************************

MeshifyPopup::MeshifyPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "MeshifyPopup")
    , m_r(-1)
    , m_c(-1) {
  setWindowTitle(tr("Create Mesh"));
  setLabelWidth(0);
  setModal(false);

  setTopMargin(0);
  setTopSpacing(0);

  beginVLayout();

  QSplitter *splitter = new QSplitter(Qt::Vertical);
  splitter->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                      QSizePolicy::MinimumExpanding));
  addWidget(splitter);

  endVLayout();

  //------------------------- Top Layout --------------------------

  QScrollArea *scrollArea = new QScrollArea(splitter);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setMinimumWidth(450);
  splitter->addWidget(scrollArea);
  splitter->setStretchFactor(0, 1);

  QFrame *topWidget = new QFrame(scrollArea);
  scrollArea->setWidget(topWidget);

  QGridLayout *topLayout =
      new QGridLayout(topWidget);  // Needed to justify at top
  topWidget->setLayout(topLayout);

  //------------------------- Parameters --------------------------

  int row = 0;

  QLabel *edgesLengthLabel = new QLabel(tr("Mesh Edges Length:"));
  topLayout->addWidget(edgesLengthLabel, row, 0,
                       Qt::AlignRight | Qt::AlignVCenter);

  m_edgesLength = new DVGui::MeasuredDoubleField;
  m_edgesLength->setMeasure("length.x");
  m_edgesLength->setRange(0.0, 1.0);  // In inches (standard unit)
  m_edgesLength->setValue(0.2);

  topLayout->addWidget(m_edgesLength, row++, 1);

  QLabel *rasterizationDpiLabel = new QLabel(tr("Rasterization DPI:"));
  topLayout->addWidget(rasterizationDpiLabel, row, 0,
                       Qt::AlignRight | Qt::AlignVCenter);

  m_rasterizationDpi = new DVGui::DoubleLineEdit;
  m_rasterizationDpi->setRange(1.0, (std::numeric_limits<double>::max)());
  m_rasterizationDpi->setValue(300.0);

  topLayout->addWidget(m_rasterizationDpi, row++, 1);

  QLabel *shapeMarginLabel = new QLabel(tr("Mesh Margin (pixels):"));
  topLayout->addWidget(shapeMarginLabel, row, 0,
                       Qt::AlignRight | Qt::AlignVCenter);

  m_margin = new DVGui::IntLineEdit;
  m_margin->setRange(2, (std::numeric_limits<int>::max)());
  m_margin->setValue(5);

  topLayout->addWidget(m_margin, row++, 1);

  connect(m_edgesLength, SIGNAL(valueChanged(bool)), this,
          SLOT(onParamsChanged(bool)));
  connect(m_rasterizationDpi, SIGNAL(editingFinished()), this,
          SLOT(onParamsChanged()));
  connect(m_margin, SIGNAL(editingFinished()), this, SLOT(onParamsChanged()));

  topLayout->setRowStretch(row, 1);

  //------------------------- View Widget -------------------------

  // NOTE: It's IMPORTANT that parent widget is supplied. It's somewhat
  // used by QSplitter to decide the initial widget sizes...

  m_viewer = new Swatch(splitter);
  m_viewer->setMinimumHeight(150);
  m_viewer->setFocusPolicy(Qt::WheelFocus);

  splitter->addWidget(m_viewer);

  //--------------------------- Buttons ---------------------------

  m_okBtn = new QPushButton(tr("Apply"));
  addButtonBarWidget(m_okBtn);

  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply()));

  // Finally, acquire current selection
  onCellSwitched();

  m_viewer->resize(600, 350);
  resize(600, 700);
}

//------------------------------------------------------------------------------

void MeshifyPopup::showEvent(QShowEvent *se) {
  TFrameHandle *frameHandle   = TApp::instance()->getCurrentFrame();
  TColumnHandle *columnHandle = TApp::instance()->getCurrentColumn();

  connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(onCellSwitched()));
  connect(columnHandle, SIGNAL(columnIndexSwitched()), this,
          SLOT(onCellSwitched()));

  onCellSwitched();
}

//------------------------------------------------------------------------------

void MeshifyPopup::hideEvent(QHideEvent *he) {
  Dialog::hideEvent(he);

  TFrameHandle *frameHandle   = TApp::instance()->getCurrentFrame();
  TColumnHandle *columnHandle = TApp::instance()->getCurrentColumn();

  disconnect(frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(onCellSwitched()));
  disconnect(columnHandle, SIGNAL(columnIndexSwitched()), this,
             SLOT(onCellSwitched()));

  m_viewer->clear();
  m_r = -1, m_c = -1, m_cell = TXshCell();
}

//------------------------------------------------------------------------------

void MeshifyPopup::acquirePreview() {
  m_viewer->clear();

  // Assign preview input to the viewer
  bool enabled = false;

  ::xshPos(m_r, m_c);
  m_cell = TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(m_r, m_c);

  // Redirect mesh case to texture
  TXshSimpleLevel *sl = m_cell.getSimpleLevel();
  if (sl && sl->getType() == MESH_XSHLEVEL) {
    // Mesh image case
    TXsheet *xsh          = TApp::instance()->getCurrentXsheet()->getXsheet();
    TStageObject *meshObj = xsh->getStageObject(TStageObjectId::ColumnId(m_c));

    const TStageObjectId &childId = ::firstChildLevelColumn(*xsh, *meshObj);
    if (childId.isColumn())
      // Retrieved the associated texture cell - redirect acquisition there
      m_cell = xsh->getCell(m_r, childId.getIndex());
  }

  if ((sl = m_cell.getSimpleLevel())) {
    // Standard image case
    m_viewer->m_img = sl->getFullsampledFrame(m_cell.getFrameId(),
                                              ImageManager::dontPutInCache);

    enabled = true;
  } else if (TXshChildLevel *cl = m_cell.getChildLevel()) {
    // Sub-xsheet case
    TXsheet *xsh = cl->getXsheet();
    int row      = m_cell.getFrameId().getNumber() - 1;

    m_viewer->m_xsh = xsh, m_viewer->m_row = row;

    enabled = true;
  }

  m_okBtn->setEnabled(enabled);

  // Update the corresponding processed image in the viewer
  updateMeshPreview();
}

//------------------------------------------------------------------------------

void MeshifyPopup::updateMeshPreview() {
  // Meshify
  MeshifyOptions options = {m_edgesLength->getValue(),
                            m_rasterizationDpi->getValue(),
                            m_margin->getValue()};
  TMeshImageP meshImg(::meshify(m_cell, options));

  // Update the swatch
  m_viewer->m_meshImg = meshImg;
  m_viewer->update();
}

//------------------------------------------------------------------------------

void MeshifyPopup::onCellSwitched() {
  // In case current cell level is not of the suitable type, disable the
  // rasterization parameter
  {
    TXshLevel *level = ::levelToMeshify();

    int type = level ? level->getType() : UNKNOWN_XSHLEVEL;
    m_rasterizationDpi->setEnabled(type == PLI_XSHLEVEL ||
                                   type == CHILD_XSHLEVEL);
  }

  acquirePreview();
}

//------------------------------------------------------------------------------

void MeshifyPopup::onParamsChanged(bool dragging) {
  if (dragging) return;

  updateMeshPreview();
}

//------------------------------------------------------------------------------

void MeshifyPopup::apply() {
  MeshifyOptions options = {m_edgesLength->getValue(),
                            m_rasterizationDpi->getValue(),
                            m_margin->getValue()};

  if (meshifySelection(options))  // Meshify invocation
  {
    TUndoManager::manager()
        ->reset();  // Since this operation CURRENTLY HAS NO UNDO, and yet
    close();        // it modifies the xsheet, it CLEARS THE UNDOS stack.
  }
}

//********************************************************************************************
//    Meshifying  functions
//********************************************************************************************

namespace {

TMeshImageP meshify(const TXshCell &cell, const MeshifyOptions &options) {
  struct locals {
    static inline void checkEmptyDpi(TPointD &dpi) {
      if (dpi.x == 0.0 || dpi.y == 0.0) dpi.x = dpi.y = Stage::inch;
    }
  };  // locals

  TRasterP ras;
  TPointD worldOriginPos;
  TPointD imageDpi, rasDpi, slDpi, shownDpi;

  MeshBuilderOptions opts;
  opts.m_margin = options.m_margin;

  rasDpi = TPointD(options.m_rasterizationDpi, options.m_rasterizationDpi);

  if (TXshSimpleLevel *sl = cell.getSimpleLevel()) {
    TImageP img(sl->getFullsampledFrame(cell.getFrameId(),
                                        ImageManager::dontPutInCache));

    ::getRaster(img, imageDpi, ras, rasDpi, worldOriginPos, opts.m_margin);
    locals::checkEmptyDpi(imageDpi), locals::checkEmptyDpi(rasDpi);

    // Get the level dpi
    slDpi = sl->getDpi();
    locals::checkEmptyDpi(slDpi);

    // Due to a Toonz bug when loading a PLI, slDpi may actually aquire the
    // camera dpi -
    // but it's always shown to be at the STANDARD world DPI, Stage::inch -
    // plus, the
    // image to be meshed is rendered at imageDpi anyway.
    shownDpi = (sl->getType() == PLI_XSHLEVEL) ? rasDpi : slDpi;

    opts.m_transparentColor = sl->getProperties()->whiteTransp()
                                  ? TPixel64::White
                                  : TPixel64::Transparent;
  } else if (TXshChildLevel *cl = cell.getChildLevel()) {
    TXsheet *xsh = cl->getXsheet();
    int row      = cell.getFrameId().getNumber() - 1;

    ::getRaster(xsh, row, ras, rasDpi, worldOriginPos, opts.m_margin);

    slDpi.x = slDpi.y = Stage::inch;
    imageDpi = slDpi, shownDpi = rasDpi;

    opts.m_transparentColor = TPixel64::Transparent;
  }

  if (!ras) return TMeshImageP();

  // Build Mesh Builder Options and Meshify
  opts.m_targetEdgeLength       = options.m_edgesLength * shownDpi.x;
  opts.m_targetMaxVerticesCount = MESH_TARGET_MAX_VERTICES_COUNT;

  return meshify(ras, rasDpi, worldOriginPos, imageDpi, opts);
}

//------------------------------------------------------------------------------

TMeshImageP meshify(const TRasterP &ras, const TPointD &rasDpi,
                    const TPointD &worldOriginPos, const TPointD &meshDpi,
                    const MeshBuilderOptions &opts) {
  // Meshify the image
  TMeshImageP meshImg = buildMesh(ras, opts);

  // Transform it to have the correct image reference (origin at center, dpi
  // scale)
  transform(meshImg, TScale(meshDpi.x / rasDpi.x, meshDpi.y / rasDpi.y) *
                         TTranslation(-worldOriginPos));

  meshImg->setDpi(meshDpi.x, meshDpi.y);

  return meshImg;
}

//------------------------------------------------------------------------------

void createMeshifiedLevels(std::map<TXshLevel *, TXshSimpleLevel *> &meshLevels,
                           const MeshifyOptions &options) {
  // Preliminaries
  typedef std::map<TXshLevel *, std::set<TFrameId>> LevelsMap;
  typedef std::map<TXshLevel *, TXshSimpleLevel *> MeshLevelsMap;

  // Build the images set to meshify
  LevelsMap levels;
  getSelectedFrames(levels);

  if (levels.empty()) {
    // Perhaps the selection was empty, or there was no selection at all. Try to
    // access cell at current frame and column
    const TXshCell &cell = ::xshCell();

    TXshLevel *xl       = cell.m_level.getPointer();
    const TFrameId &fid = cell.getFrameId();

    if (xl) levels[xl].insert(fid);
  }

  // Prepare a progress dialog
  std::unique_ptr<DVGui::ProgressDialog> progressDialog(
      new DVGui::ProgressDialog(
          MeshifyPopup::tr("Mesh Creation in progress..."), QString(), 0, 0));
  {
    progressDialog->setWindowTitle("Create Mesh");
    progressDialog->setModal(true);

    // Build the number of objects to be meshified
    int count = 0;

    LevelsMap::const_iterator lt, lEnd(levels.end());
    for (lt = levels.begin(); lt != lEnd; ++lt) count += lt->second.size();

    progressDialog->setMaximum(count);
    progressDialog->show();

    progressDialog->setValue(0);  // Processes the show()
  }

  // Process each level independently
  LevelsMap::const_iterator lt, lEnd(levels.end());
  for (lt = levels.begin(); lt != lEnd; ++lt) {
    TXshLevel *xl = lt->first;

    // Create the associated mesh level
    TXshSimpleLevel *ml = ::createMeshLevel(xl);

    if (!ml) {
      progressDialog->setValue(progressDialog->value() + lt->second.size());
      continue;
    }

    meshLevels[xl] = ml;

    // Meshify the associated set of frames, and assign the results
    // to the mesh level
    const std::set<TFrameId> &frames = lt->second;

    // The path format of single image levels must be reproduced. It was not
    // possible to enforce this in ::createMeshLevels(), so we're doing it now.
    if (frames.size() == 1 && *frames.begin() == TFrameId(TFrameId::NO_FRAME))
      ml->setPath(ml->getPath().withFrame(TFrameId::NO_FRAME));

    std::set<TFrameId>::const_iterator ft, fEnd(frames.end());
    for (ft = frames.begin(); ft != fEnd; ++ft) {
      TMeshImageP meshImage = meshify(TXshCell(xl, *ft), options);
      if (meshImage) ml->setFrame(*ft, meshImage);

      progressDialog->setValue(progressDialog->value() + 1);
    }

    ml->setDirtyFlag(true);
  }
}

//------------------------------------------------------------------------------

void createMeshifiedColumns(int r0, int c0, int r1, int c1,
                            const MeshifyOptions &options) {
  struct locals {
    static inline bool isEmpty(const TXsheet &xsh, int r0, int r1, int c) {
      for (int r = r0; r != r1; ++r) {
        if (!xsh.getCell(r, c).isEmpty()) return false;
      }

      return true;
    }

    static inline int meshColumnIdx(const TXsheet &xsh, int r0, int r1, int c) {
      TStageObject *texObj = xsh.getStageObject(TStageObjectId::ColumnId(c));
      assert(texObj);

      const TStageObjectId &parentId = texObj->getParent();
      if (!parentId.isColumn()) return -1;

      int meshColIdx = parentId.getIndex();

      const TXshMeshColumn *meshCol =
          xsh.getColumn(meshColIdx)->getMeshColumn();
      if (!meshCol) return -1;

      return isEmpty(xsh, r0, r1, meshColIdx) ? meshColIdx : -1;
    }

  };  // locals

  // Retrieve selection data
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  // Meshify levels
  std::map<TXshLevel *, TXshSimpleLevel *> meshLevels;
  createMeshifiedLevels(meshLevels, options);
  if (meshLevels.empty()) return;

  // Create corresponding columns and assign them the meshified levels
  int meshColIdx;

  int r, c;
  for (c = c1; c >= c0;
       --c)  // Reverse iteration since we're interleaving the results
  {
    TXshLevelColumn *column =
        dynamic_cast<TXshLevelColumn *>(xsh->getColumn(c));
    if (!column || column->isEmpty())  // Skip empty/innocent columns
      continue;

    meshColIdx = -1;

    // Deal with cells contents
    for (r = r0; r <= r1; ++r) {
      // Set meshCol's cells accordingly
      const TXshCell &srcCell = xsh->getCell(r, c);
      if (srcCell.isEmpty()) continue;

      std::map<TXshLevel *, TXshSimpleLevel *>::iterator lt(
          meshLevels.find(srcCell.m_level.getPointer()));

      if (lt != meshLevels.end()) {
        if (meshColIdx < 0) {
          meshColIdx = locals::meshColumnIdx(
              *xsh, r0, r1, c);  // Attempt retrieval of an existing mesh column
          if (meshColIdx < 0)    // first - if not found, then make a new one
            ::makeMeshColumn(*xsh, c,
                             meshColIdx = c + 1);  // right after current one.
        }

        TXshCell dstCell(lt->second, srcCell.m_frameId);
        xsh->setCell(r, meshColIdx, dstCell);
      }
    }
  }

  // Set current column to meshColIdx - this allows the users to skip selecting
  // it
  // in case they want to start editing meshes immediately
  assert(meshColIdx >= 0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(meshColIdx);
}

//------------------------------------------------------------------------------

void updateMeshifiedColumns(int r0, int c0, int r1, int c1,
                            const MeshifyOptions &options) {
  typedef std::map<std::pair<TXshLevel *, TFrameId>, TXshCell> MeshesMap;

  // Build a map of the mesh images to be updated
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  assert(xsh);

  MeshesMap meshToUpdate;

  for (int c = c0; c <= c1; ++c) {
    TXshColumn *col = xsh->getColumn(c);
    if (!col) continue;

    TXshMeshColumn *meshCol = col->getMeshColumn();
    if (!meshCol) continue;

    TStageObject *meshObj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    assert(meshObj);

    for (int r = r0; r <= r1; ++r) {
      // Retrieve the mesh cell
      const TXshCell &cell = xsh->getCell(r, c);

      TXshSimpleLevel *ml = cell.getSimpleLevel();
      if (!ml || ml->getType() != MESH_XSHLEVEL) continue;

      // Retrieve the associated texture
      const TStageObjectId &childId = firstChildLevelColumn(*xsh, *meshObj);

      const TXshCell &texCell = xsh->getCell(r, childId.getIndex());

      TXshLevel *xl = texCell.m_level.getPointer();
      if (!(xl && (xl->getType() & LEVELCOLUMN_XSHLEVEL))) continue;

      // Found a match - insert it in the map.
      // NOTE: The same mesh cell could be found multiple times. In this case,
      // subsequent ones
      // will be ignored.
      MeshesMap::key_type meshPair(cell.m_level.getPointer(), cell.m_frameId);

      if (meshToUpdate.find(meshPair) == meshToUpdate.end())
        meshToUpdate.insert(std::make_pair(meshPair, texCell));
    }
  }

  // Now, we have to re-meshify the map's former mesh references against the
  // latter textures

  MeshesMap::iterator ct, cEnd(meshToUpdate.end());
  for (ct = meshToUpdate.begin(); ct != cEnd; ++ct) {
    TXshSimpleLevel *ml  = ct->first.first->getSimpleLevel();
    const TFrameId &mFid = ct->first.second;

    TMeshImageP meshImg = meshify(ct->second, options);

    ml->setFrame(mFid, meshImg);
    ml->setDirtyFlag(true);
  }

  // Update icons. Okay, I'm doing this in a separate cycle, because I suspect
  // it's
  // not safe to re-render icons as we're manipulating the level frames map. It
  // *could* be
  // that the TThread::Executor waits until the control loop returns before
  // starting the
  // icon threads... well - better safe than sorry :)
  for (ct = meshToUpdate.begin(); ct != cEnd; ++ct)
    IconGenerator::instance()->invalidate(
        ct->first.first, ct->first.second);  // Yep, this is still manual...
}

//------------------------------------------------------------------------------

template <typename Func>
void meshifySelection(Func func, TSelection *selection,
                      const MeshifyOptions &options) {
  bool emptySelection = selection ? selection->isEmpty() : true;

  if (TCellSelection *cellSelection =
          emptySelection ? (TCellSelection *)0
                         : dynamic_cast<TCellSelection *>(selection)) {
    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);

    (*func)(r0, c0, r1, c1, options);
    return;
  }

  if (TColumnSelection *colSelection =
          emptySelection ? (TColumnSelection *)0
                         : dynamic_cast<TColumnSelection *>(selection)) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    const std::set<int> &indices = colSelection->getIndices();

    int c0 = *indices.begin(), c1 = *indices.rbegin();
    int r0 = (std::numeric_limits<int>::max)(), r1 = -r0;

    std::set<int>::const_iterator it, iEnd(indices.end());
    for (it = indices.begin(); it != iEnd; ++it) {
      TXshCellColumn *column =
          dynamic_cast<TXshCellColumn *>(xsh->getColumn(*it));
      if (!column || column->isEmpty()) continue;

      r0 = std::min(r0, column->getFirstRow());
      r1 = std::max(r1, column->getMaxFrame());
    }

    (*func)(r0, c0, r1, c1, options);
    return;
  }

  // Meshify the sole cell at current frame and column positions
  int r = TApp::instance()->getCurrentFrame()->getFrame();
  int c = TApp::instance()->getCurrentColumn()->getColumnIndex();

  (*func)(r, c, r, c, options);
}

//------------------------------------------------------------------------------

enum ColumnEnum { HAS_LEVEL_COLUMNS = 0x1, HAS_MESH_COLUMNS = 0x2 };

//------------------------------------------------------------------------------

// Ensure that the specified range has homogeneous columns with respect to
// meshification
int columnTypes(TXsheet *xsh, int c0, int c1) {
  int columnBits = 0x0;

  for (int c = c0; c <= c1; ++c) {
    TXshColumn *xc = xsh->getColumn(c);
    if (!xc) continue;

    if (xc->getLevelColumn()) columnBits |= HAS_LEVEL_COLUMNS;

    if (xc->getMeshColumn()) columnBits |= HAS_MESH_COLUMNS;
  }

  return columnBits;
}

//------------------------------------------------------------------------------

// Ensure that the specified range has homogeneous columns with respect to
// meshification
int columnTypes(TXsheet *xsh, const std::set<int> &indices) {
  int columnBits = 0x0;

  std::set<int>::const_iterator it, iEnd(indices.end());
  for (it = indices.begin(); it != iEnd; ++it) {
    TXshColumn *xc = xsh->getColumn(*it);

    if (xc->getLevelColumn()) columnBits |= HAS_LEVEL_COLUMNS;

    if (xc->getMeshColumn()) columnBits |= HAS_MESH_COLUMNS;
  }

  return columnBits;
}

//------------------------------------------------------------------------------

bool meshifySelection(const MeshifyOptions &options) {
  TApp *app = TApp::instance();

  // Build selection data
  TSelection *selection = TSelection::getCurrent();
  bool emptySelection   = selection ? selection->isEmpty() : true;

  TCellSelection *cellSelection =
      emptySelection ? (TCellSelection *)0
                     : dynamic_cast<TCellSelection *>(selection);
  TColumnSelection *colSelection =
      emptySelection ? (TColumnSelection *)0
                     : dynamic_cast<TColumnSelection *>(selection);

  int r0, c0, r1, c1;
  if (cellSelection)
    cellSelection->getSelectedCells(r0, c0, r1, c1);
  else {
    // Meshify a single frame (the one at current frame and current column)
    r0 = r1 = app->getCurrentFrame()->getFrame();
    c0 = c1 = app->getCurrentColumn()->getColumnIndex();
  }

  // Decide action. Dismiss mixed selections containing both mesh AND simple
  // levels.
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  int cTypes = colSelection ? columnTypes(xsh, colSelection->getIndices())
                            : columnTypes(xsh, c0, c1);

  switch (cTypes) {
  case HAS_LEVEL_COLUMNS:
    // Create new mesh columns corresponding to specified selection
    meshifySelection(&createMeshifiedColumns, selection, options);
    break;
  case HAS_MESH_COLUMNS:
    // Check parental relationship - if specified columns have level column
    // children,
    // update related meshes
    meshifySelection(&updateMeshifiedColumns, selection, options);
    break;
  case HAS_LEVEL_COLUMNS | HAS_MESH_COLUMNS:
    // Error message
    DVGui::error(MeshifyPopup::tr(
        "Current selection contains mixed image and mesh level types"));
    return false;
  default:
    // Error message
    DVGui::error(MeshifyPopup::tr(
        "Current selection contains no image or mesh level types"));
    return false;
  }

  // Notify xsheet change
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentScene()->notifyCastChange();

  return true;
}

}  // namespace

//********************************************************************************************
//    Meshify Command  definition
//********************************************************************************************

class MeshifyCommand final : public MenuItemHandler {
public:
  MeshifyCommand() : MenuItemHandler("A_ToolOption_Meshify") {}

  void execute() override {
    static MeshifyPopup *thePopup = 0;
    if (!thePopup) thePopup       = new MeshifyPopup;

    thePopup->raise();
    thePopup->show();
  }

} meshifyCommand;
