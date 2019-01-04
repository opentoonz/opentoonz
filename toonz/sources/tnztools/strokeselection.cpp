

#include "tools/strokeselection.h"

// TnzTools includes
#include "tools/imagegrouping.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "tools/toolutils.h"

// TnzQt includes
#include "toonzqt/selectioncommandids.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/strokesdata.h"
#include "toonzqt/rasterimagedata.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"

#include "toonz/tcenterlinevectorizer.h"
#include "toonz/stage.h"
#include "toonz/tstageobject.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobject.h"

// TnzCore includes
#include "tthreadmessage.h"
#include "tundo.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tcolorstyles.h"
#include "tpalette.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//=============================================================================
namespace {

void vectorizeToonzImageData(const TVectorImageP &image,
                             const ToonzImageData *tiData,
                             std::set<int> &indexes, TPalette *palette,
                             const VectorizerConfiguration &config) {
  if (!tiData) return;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  TRasterP ras;
  std::vector<TRectD> rects;
  std::vector<TStroke> strokes;
  std::vector<TStroke> originalStrokes;
  TAffine affine;
  double dpiX, dpiY;
  tiData->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes, affine,
                  image->getPalette());
  TRasterCM32P rasCM = ras;
  TToonzImageP ti(rasCM, rasCM->getBounds());
  VectorizerCore vc;
  TVectorImageP vi = vc.vectorize(ti, config, palette);
  assert(vi);
  vi->setPalette(palette);

  TScale sc(dpiX / Stage::inch, dpiY / Stage::inch);
  int i;
  TRectD selectionBounds;
  for (i = 0; i < (int)rects.size(); i++) selectionBounds += rects[i];
  for (i = 0; i < (int)strokes.size(); i++)
    selectionBounds += strokes[i].getBBox();
  TTranslation tr(selectionBounds.getP00());

  for (i = 0; i < (int)vi->getStrokeCount(); i++) {
    TStroke *stroke = vi->getStroke(i);
    stroke->transform(sc.inv() * affine * tr, true);
  }

  UINT oldImageSize = image->getStrokeCount();
  image->mergeImage(vi, TAffine());
  UINT newImageSize = image->getStrokeCount();
  indexes.clear();
  for (UINT sI = oldImageSize; sI < newImageSize; sI++) indexes.insert(sI);
  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------

void copyStrokesWithoutUndo(TVectorImageP image, std::set<int> &indexes) {
  QClipboard *clipboard = QApplication::clipboard();
  StrokesData *data     = new StrokesData();
  data->setImage(image, indexes);
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

bool pasteStrokesWithoutUndo(TVectorImageP image, std::set<int> &outIndexes,
                             TSceneHandle *sceneHandle, bool insert = true) {
  QMutexLocker lock(image->getMutex());
  QClipboard *clipboard = QApplication::clipboard();
  const StrokesData *stData =
      dynamic_cast<const StrokesData *>(clipboard->mimeData());
  const ToonzImageData *tiData =
      dynamic_cast<const ToonzImageData *>(clipboard->mimeData());
  const FullColorImageData *fciData =
      dynamic_cast<const FullColorImageData *>(clipboard->mimeData());
  std::set<int> indexes = outIndexes;
  if (stData)
    stData->getImage(image, indexes, insert);
  else if (tiData) {
    ToonzScene *scene = sceneHandle->getScene();
    assert(scene);

    const VectorizerParameters *vParams =
        scene->getProperties()->getVectorizerParameters();
    assert(vParams);

    std::unique_ptr<VectorizerConfiguration> config(
        vParams->getCurrentConfiguration(0.0));
    vectorizeToonzImageData(image, tiData, indexes, image->getPalette(),
                            *config);
  } else if (fciData) {
    DVGui::error(QObject::tr(
        "The copied selection cannot be pasted in the current drawing."));
    return false;
  } else
    return false;

  StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
  outIndexes = indexes;  // outIndexes is a reference  to current  selection, so
                         // the notifyImageChanged could reset it!
  return true;
}

//-----------------------------------------------------------------------------

void deleteStrokesWithoutUndo(TVectorImageP image, std::set<int> &indexes) {
  QMutexLocker lock(image->getMutex());
  std::vector<int> indexesV(indexes.begin(), indexes.end());
  TRectD bbox;
  UINT i = 0;
  for (; i < indexesV.size(); i++)
    bbox += image->getStroke(indexesV[i])->getBBox();

  std::vector<TFilledRegionInf> regions;
  ImageUtils::getFillingInformationOverlappingArea(image, regions, bbox);

  TVectorImageP other = image->splitImage(indexesV, true);

  indexes.clear();

  TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
}

//-----------------------------------------------------------------------------

void cutStrokesWithoutUndo(TVectorImageP image, std::set<int> &indexes) {
  copyStrokesWithoutUndo(image, indexes);
  deleteStrokesWithoutUndo(image, indexes);
}

//=============================================================================
// CopyStrokesUndo
//-----------------------------------------------------------------------------

class CopyStrokesUndo final : public TUndo {
  QMimeData *m_oldData;
  QMimeData *m_newData;

public:
  CopyStrokesUndo(QMimeData *oldData, QMimeData *newData)
      : m_oldData(oldData), m_newData(newData) {}

  void undo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);
  }

  void redo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_newData), QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// PasteStrokesUndo
//-----------------------------------------------------------------------------

class PasteStrokesUndo final : public ToolUtils::TToolUndo {
  std::set<int> m_indexes;
  QMimeData *m_oldData;
  TSceneHandle *m_sceneHandle;

public:
  PasteStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                   std::set<int> &indexes, TPaletteP oldPalette,
                   TSceneHandle *sceneHandle, bool createdFrame,
                   bool createdLevel)
      : TToolUndo(level, frameId, createdFrame, createdLevel, oldPalette)
      , m_indexes(indexes)
      , m_sceneHandle(sceneHandle) {
    QClipboard *clipboard = QApplication::clipboard();
    m_oldData             = cloneData(clipboard->mimeData());
  }

  ~PasteStrokesUndo() { delete m_oldData; }

  void undo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);

    // Se la selezione corrente e' la stroke selection devo svuotarla,
    // altrimenti puo' rimanere selezionato uno stroke che non esiste piu'.
    StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
        TTool::getApplication()->getCurrentSelection()->getSelection());
    if (selection) selection->selectNone();

    std::set<int> indexes = m_indexes;
    deleteStrokesWithoutUndo(image, indexes);

    removeLevelAndFrameIfNeeded();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    std::set<int> indexes = m_indexes;

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *data       = cloneData(clipboard->mimeData());

    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);

    pasteStrokesWithoutUndo(image, indexes, m_sceneHandle);
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();

    clipboard->setMimeData(data, QClipboard::Clipboard);

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }
};

//--------------------------------------------------------------------

class RemoveEndpointsUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  TFrameId m_frameId;
  std::vector<std::pair<int, TStroke *>> m_strokes;

public:
  RemoveEndpointsUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                      std::vector<std::pair<int, TStroke *>> strokes)
      : m_level(level)
      , m_frameId(frameId)
      , m_strokes(strokes)

  {}

  ~RemoveEndpointsUndo() {
    int i;
    for (i = 0; i < (int)m_strokes.size(); i++) delete m_strokes[i].second;
  }

  void undo() const override {
    TVectorImageP vi = m_level->getFrame(m_frameId, true);
    int i;
    for (i = 0; i < (int)m_strokes.size(); i++) {
      TStroke *newS = new TStroke(*(m_strokes[i].second));
      newS->setId(m_strokes[i].second->getId());
      vi->restoreEndpoints(m_strokes[i].first, newS);
    }
    StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
        TTool::getApplication()->getCurrentSelection()->getSelection());
    if (selection) selection->selectNone();

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  void redo() const override {
    int i;
    TVectorImageP vi = m_level->getFrame(m_frameId, true);
    for (i = 0; i < (int)m_strokes.size(); i++) {
      TStroke *s = vi->removeEndpoints(m_strokes[i].first);
      delete s;
      // assert(s==m_strokes[i].second);
    }
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }
};
//=============================================================================
// DeleteFramesUndo
//-----------------------------------------------------------------------------

class DeleteStrokesUndo : public TUndo {
protected:
  TXshSimpleLevelP m_level;
  TFrameId m_frameId;
  std::set<int> m_indexes;
  QMimeData *m_data;
  TSceneHandle *m_sceneHandle;

public:
  DeleteStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                    std::set<int> indexes, QMimeData *data,
                    TSceneHandle *sceneHandle)
      : m_level(level)
      , m_frameId(frameId)
      , m_indexes(indexes)
      , m_data(data)
      , m_sceneHandle(sceneHandle) {}

  ~DeleteStrokesUndo() { delete m_data; }

  void undo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *oldData    = cloneData(clipboard->mimeData());

    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);
    std::set<int> indexes = m_indexes;
    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    pasteStrokesWithoutUndo(image, indexes, m_sceneHandle, false);
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();

    clipboard->setMimeData(oldData, QClipboard::Clipboard);
  }

  void redo() const override {
    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    std::set<int> indexes = m_indexes;
    deleteStrokesWithoutUndo(image, indexes);
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// CutStrokesUndo
//-----------------------------------------------------------------------------

class CutStrokesUndo final : public DeleteStrokesUndo {
public:
  CutStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                 std::set<int> indexes, QMimeData *data,
                 TSceneHandle *sceneHandle)
      : DeleteStrokesUndo(level, frameId, indexes, data, sceneHandle) {}

  ~CutStrokesUndo() {}

  void redo() const override {
    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    std::set<int> indexes = m_indexes;
    cutStrokesWithoutUndo(image, indexes);
  }
};

}  // namespace

//=============================================================================
//
// StrokeSelection ctor/dtor
//
//-----------------------------------------------------------------------------

StrokeSelection::StrokeSelection()
    : m_groupCommand(new TGroupCommand())
    , m_sceneHandle()
    , m_updateSelectionBBox(false) {
  m_groupCommand->setSelection(this);
}

//-----------------------------------------------------------------------------

StrokeSelection::~StrokeSelection() {}

//-----------------------------------------------------------------------------

StrokeSelection::StrokeSelection(const StrokeSelection &other)
    : m_vi(other.m_vi)
    , m_indexes(other.m_indexes)
    , m_groupCommand(new TGroupCommand())
    , m_sceneHandle(other.m_sceneHandle)
    , m_updateSelectionBBox(other.m_updateSelectionBBox) {
  m_groupCommand->setSelection(this);
}

//-----------------------------------------------------------------------------

StrokeSelection &StrokeSelection::operator=(const StrokeSelection &other) {
  m_vi                  = other.m_vi;
  m_indexes             = other.m_indexes;
  m_sceneHandle         = other.m_sceneHandle;
  m_updateSelectionBBox = other.m_updateSelectionBBox;

  return *this;
}

//-----------------------------------------------------------------------------

void StrokeSelection::select(int index, bool on) {
  if (on)
    m_indexes.insert(index);
  else
    m_indexes.erase(index);
}

//-----------------------------------------------------------------------------

void StrokeSelection::toggle(int index) {
  std::set<int>::iterator it = m_indexes.find(index);
  if (it == m_indexes.end())
    m_indexes.insert(index);
  else
    m_indexes.erase(it);
}

//=============================================================================
//
// removeEndpoints
//
//-----------------------------------------------------------------------------

void StrokeSelection::removeEndpoints() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;

  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be updated. It is not editable."));
    return;
  }

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();
  for (auto const &e : m_indexes) {
    TStroke *s = m_vi->removeEndpoints(e);
    if (s) undoData.push_back(std::pair<int, TStroke *>(e, s));
  }
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  if (!undoData.empty())
    TUndoManager::manager()->add(
        new RemoveEndpointsUndo(level, tool->getCurrentFid(), undoData));

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//=============================================================================
//
// selectAll
//
//-----------------------------------------------------------------------------

void StrokeSelection::selectAll() {
  if (!m_vi) return;

  int sCount = int(m_vi->getStrokeCount());

  for (int s = 0; s < sCount; ++s) {
    m_indexes.insert(s);
  }

  StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
}

//=============================================================================
//
// deleteStrokes
//
//-----------------------------------------------------------------------------

void StrokeSelection::deleteStrokes() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be deleted. It is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  TUndo *undo;
  if (isSpline)
    undo = new ToolUtils::UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());

  StrokesData *data = new StrokesData();
  data->setImage(m_vi, m_indexes);
  std::set<int> oldIndexes = m_indexes;
  deleteStrokesWithoutUndo(m_vi, m_indexes);

  if (!isSpline) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(new DeleteStrokesUndo(
        level, tool->getCurrentFid(), oldIndexes, data, m_sceneHandle));
  } else {
    assert(undo);
    if (undo) TUndoManager::manager()->add(undo);
  }
}

//=============================================================================
//
// StrokeSelection::copy()
//
//-----------------------------------------------------------------------------

void StrokeSelection::copy() {
  if (m_indexes.empty()) return;
  QClipboard *clipboard = QApplication::clipboard();
  QMimeData *oldData    = cloneData(clipboard->mimeData());
  copyStrokesWithoutUndo(m_vi, m_indexes);
  QMimeData *newData = cloneData(clipboard->mimeData());

  // TUndoManager::manager()->add(new CopyStrokesUndo(oldData, newData));
}

//=============================================================================
//
// StrokeSelection::paste()
//
//-----------------------------------------------------------------------------

void StrokeSelection::paste() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be pasted. It is not editable."));
    return;
  }

  if (TTool::getApplication()->getCurrentObject()->isSpline()) {
    const StrokesData *stData = dynamic_cast<const StrokesData *>(
        QApplication::clipboard()->mimeData());
    if (!stData) return;
    TVectorImageP splineImg = tool->getImage(true);
    TVectorImageP img       = stData->m_image;
    if (!splineImg || !img) return;

    QMutexLocker lock(splineImg->getMutex());
    TUndo *undo = new ToolUtils::UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
    while (splineImg->getStrokeCount() > 0) splineImg->deleteStroke(0);

    TStroke *stroke = img->getStroke(0);
    splineImg->addStroke(new TStroke(*stroke), false);
    TUndoManager::manager()->add(undo);
    tool->notifyImageChanged();
    tool->invalidate();
    return;
  }

  TVectorImageP tarImg = tool->touchImage();
  if (!tarImg) return;
  TPaletteP palette       = tarImg->getPalette();
  TPaletteP oldPalette    = new TPalette();
  if (palette) oldPalette = palette->clone();
  bool isPaste = pasteStrokesWithoutUndo(tarImg, m_indexes, m_sceneHandle);
  if (isPaste) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(new PasteStrokesUndo(
        level, tool->getCurrentFid(), m_indexes, oldPalette, m_sceneHandle,
        tool->m_isFrameCreated, tool->m_isLevelCreated));
    m_updateSelectionBBox = isPaste;
  }
  tool->notifyImageChanged();
  tool->getApplication()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->notifyPaletteChanged();
  m_updateSelectionBBox = false;
  tool->invalidate();
}

//=============================================================================
//
// StrokeSelection::cut()
//
//-----------------------------------------------------------------------------

void StrokeSelection::cut() {
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be deleted. It is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  TUndo *undo;
  if (isSpline)
    undo = new ToolUtils::UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());

  StrokesData *data = new StrokesData();
  data->setImage(m_vi, m_indexes);
  std::set<int> oldIndexes = m_indexes;
  cutStrokesWithoutUndo(m_vi, m_indexes);
  if (!isSpline) {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(new CutStrokesUndo(
        level, tool->getCurrentFid(), oldIndexes, data, m_sceneHandle));
  } else {
    assert(undo);
    if (undo) TUndoManager::manager()->add(undo);
  }
}

//=============================================================================
//
// enableCommands
//
//-----------------------------------------------------------------------------

void StrokeSelection::enableCommands() {
  enableCommand(this, MI_Clear, &StrokeSelection::deleteStrokes);
  enableCommand(this, MI_Cut, &StrokeSelection::cut);
  enableCommand(this, MI_Copy, &StrokeSelection::copy);
  enableCommand(this, MI_Paste, &StrokeSelection::paste);

  enableCommand(m_groupCommand.get(), MI_Group, &TGroupCommand::group);
  enableCommand(m_groupCommand.get(), MI_Ungroup, &TGroupCommand::ungroup);
  enableCommand(m_groupCommand.get(), MI_BringToFront, &TGroupCommand::front);
  enableCommand(m_groupCommand.get(), MI_BringForward, &TGroupCommand::forward);
  enableCommand(m_groupCommand.get(), MI_SendBack, &TGroupCommand::back);
  enableCommand(m_groupCommand.get(), MI_SendBackward,
                &TGroupCommand::backward);
  enableCommand(m_groupCommand.get(), MI_EnterGroup,
                &TGroupCommand::enterGroup);
  enableCommand(m_groupCommand.get(), MI_ExitGroup, &TGroupCommand::exitGroup);

  enableCommand(this, MI_RemoveEndpoints, &StrokeSelection::removeEndpoints);
  enableCommand(this, MI_SelectAll, &StrokeSelection::selectAll);
}

//===================================================================
namespace {
//===================================================================

class UndoSetStrokeStyle final : public TUndo {
  TVectorImageP m_image;
  std::vector<int> m_strokeIndexes;
  std::vector<int> m_oldStyles;
  int m_newStyle;

public:
  UndoSetStrokeStyle(TVectorImageP image, int newStyle)
      : m_image(image), m_newStyle(newStyle) {}

  void addStroke(TStroke *stroke) {
    m_strokeIndexes.push_back(m_image->getStrokeIndex(stroke));
    m_oldStyles.push_back(stroke->getStyle());
  }

  void undo() const override {
    UINT size = m_strokeIndexes.size();
    assert(size == m_oldStyles.size());

    for (UINT i = 0; i != size; i++) {
      int index = m_strokeIndexes[i];
      if (index != -1 && index < (int)m_image->getStrokeCount())
        m_image->getStroke(index)->setStyle(m_oldStyles[i]);
    }

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  void redo() const override {
    UINT size = m_strokeIndexes.size();
    assert(size == m_oldStyles.size());

    for (UINT i = 0; i != size; i++) {
      int index = m_strokeIndexes[i];
      if (index != -1 && index < (int)m_image->getStrokeCount())
        m_image->getStroke(index)->setStyle(m_newStyle);
    }

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           m_strokeIndexes.capacity() * sizeof(m_strokeIndexes[0]) +
           m_oldStyles.capacity() * sizeof(m_oldStyles[0]);
  }
};

}  // namespace

//=============================================================================
//
// changeColorStyle
//
//-----------------------------------------------------------------------------

void StrokeSelection::changeColorStyle(int styleIndex) {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  TVectorImageP img(tool->getImage(true));
  if (!img) return;
  TPalette *palette = img->getPalette();
  TColorStyle *cs   = palette->getStyle(styleIndex);
  if (!cs->isStrokeStyle()) return;
  if (m_indexes.empty()) return;

  UndoSetStrokeStyle *undo = new UndoSetStrokeStyle(img, styleIndex);
  std::set<int>::iterator it;
  for (it = m_indexes.begin(); it != m_indexes.end(); ++it) {
    int index = *it;
    assert(0 <= index && index < (int)img->getStrokeCount());
    TStroke *stroke = img->getStroke(index);
    undo->addStroke(stroke);
    stroke->setStyle(styleIndex);
  }

  tool->notifyImageChanged();
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

bool StrokeSelection::isEditable() {
  TTool::Application *app = TTool::getApplication();
  TXshSimpleLevel *level  = app->getCurrentLevel()->getSimpleLevel();

  TFrameHandle *frame = app->getCurrentFrame();
  bool filmstrip      = frame->isEditingLevel();

  if (level) {
    if (level->isReadOnly()) return false;

    TFrameId frameId = app->getCurrentTool()->getTool()->getCurrentFid();
    if (level->isFrameReadOnly(frameId)) return false;
  }

  if (!filmstrip) {
    int colIndex = app->getCurrentTool()->getTool()->getColumnIndex();
    int rowIndex = frame->getFrame();
    if (app->getCurrentTool()->getTool()->isColumnLocked(colIndex))
      return false;

    TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(colIndex));
    // Test for Mesh-deformed levels
    const TStageObjectId &parentId = obj->getParent();
    if (parentId.isColumn() && obj->getParentHandle()[0] != 'H') {
      TXshSimpleLevel *parentSl =
          xsh->getCell(rowIndex, parentId.getIndex()).getSimpleLevel();
      if (parentSl && parentSl->getType() == MESH_XSHLEVEL) return false;
    }
  }

  return true;
}
