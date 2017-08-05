

// TnzCore includes
#include "tstream.h"

// TnzBase includes
#include "toutputproperties.h"
#include "tfx.h"
#include "tparamcontainer.h"
#include "tparamset.h"
#include "tfxattributes.h"

// TnzLib includes
#include "toonz/fxdag.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/observer.h"
#include "toonz/controlpointobserver.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/columnfan.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshnoteset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage.h"
#include "toonz/textureutils.h"
#include "xshhandlemanager.h"
#include "orientation.h"

#include "toonz/txsheet.h"

using namespace std;

DEFINE_CLASS_CODE(TXsheet, 18)

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

string getColumnDefaultName(TXsheet *xsh, int col, QString oldName) {
  TXshColumn *column = xsh->getColumn(col);
  if (column) {
    TXshLevelColumn *lc = column->getLevelColumn();
    if (lc) {
      int r0, r1;
      if (lc->getRange(r0, r1)) {
        TXshCell cell = lc->getCell(r0);
        assert(!cell.isEmpty());
        TXshLevel *level = cell.m_level.getPointer();
        if (level) {
          bool isNumber = true;
          oldName.right(oldName.size() - 3).toInt(&isNumber);
          if (oldName.left(3) == "Col" && isNumber)
            return ::to_string(level->getName());
          else
            return "";
        }
      }
    }
  }
  return "Col" + std::to_string(col + 1);
}

//-----------------------------------------------------------------------------

void setColumnName(TXsheet *xsh, int col) {
  TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(col));
  QString oldName   = QString::fromStdString(obj->getName());
  string name       = getColumnDefaultName(xsh, col, oldName);
  if (!name.empty()) obj->setName(name);
}

}  // namespace

//=============================================================================
// TXsheetImp

struct TXsheet::TXsheetImp {
  unsigned long m_id;  //!< The xsheet instance's unique identifier

  TColumnSetT<TXshColumn> m_columnSet;
  TStageObjectTree *m_pegTree;
  FxDag *m_fxDag;

  int m_frameCount;
  int m_soloColumn;
  int m_viewColumn;

  TSoundTrackP m_mixedSound;
  ColumnFan m_columnFans[Orientations::COUNT];
  XshHandleManager *m_handleManager;
  ToonzScene *m_scene;

public:
  TXsheetImp();
  ~TXsheetImp();

  static inline unsigned long newIdentifier() {
    static unsigned long currentId = 0;
    return ++currentId;
  }

  void copyFoldedState();

private:
  void initColumnFans();

  // not implemented
  TXsheetImp(const TXsheetImp &);
  TXsheetImp &operator=(const TXsheetImp &);
};

//-----------------------------------------------------------------------------

TXsheet::SoundProperties::SoundProperties()
    : m_fromFrame(-1), m_toFrame(-1), m_frameRate(-1), m_isPreview(false) {}

//-----------------------------------------------------------------------------

TXsheet::SoundProperties::~SoundProperties() {}

//-----------------------------------------------------------------------------

inline bool TXsheet::SoundProperties::operator==(
    const SoundProperties &c) const {
  return m_fromFrame == c.m_fromFrame && m_toFrame == c.m_toFrame &&
         m_frameRate == c.m_frameRate && m_isPreview == c.m_isPreview;
}

inline bool TXsheet::SoundProperties::operator!=(
    const SoundProperties &c) const {
  return !(*this == c);
}

//-----------------------------------------------------------------------------

TXsheet::TXsheetImp::TXsheetImp()
    : m_id(newIdentifier())
    , m_pegTree(new TStageObjectTree)
    , m_handleManager(0)
    , m_fxDag(new FxDag())
    , m_frameCount(0)
    , m_soloColumn(-1)
    , m_viewColumn(-1)
    , m_mixedSound(0)
    , m_scene(0) {
  initColumnFans();
}

//-----------------------------------------------------------------------------

TXsheet::TXsheetImp::~TXsheetImp() {
  assert(m_pegTree);
  assert(m_fxDag);
  assert(m_handleManager);
  delete m_pegTree;
  delete m_fxDag;
  delete m_handleManager;
}

//-----------------------------------------------------------------------------

void TXsheet::TXsheetImp::initColumnFans() {
  for (auto o : Orientations::all()) {
    int index = o->dimension(PredefinedDimension::INDEX);
    m_columnFans[index].setDimension(o->dimension(PredefinedDimension::LAYER));
  }
}

void TXsheet::TXsheetImp::copyFoldedState() {
  for (int i = 1; i < Orientations::COUNT; i++)
    m_columnFans[i].copyFoldedStateFrom(m_columnFans[0]);
}

//=============================================================================
// TXsheet

TXsheet::TXsheet()
    : TSmartObject(m_classCode)
    , m_player(0)
    , m_imp(new TXsheet::TXsheetImp)
    , m_notes(new TXshNoteSet()) {
  // extern TSyntax::Grammar *createXsheetGrammar(TXsheet*);
  m_soundProperties      = new TXsheet::SoundProperties();
  m_imp->m_handleManager = new XshHandleManager(this);
  m_imp->m_pegTree->setHandleManager(m_imp->m_handleManager);
  m_imp->m_pegTree->createGrammar(this);
}

//-----------------------------------------------------------------------------

TXsheet::~TXsheet() {
  texture_utils::invalidateTextures(this);

  assert(m_imp);
  if (m_notes) delete m_notes;
  if (m_soundProperties) delete m_soundProperties;
}

//-----------------------------------------------------------------------------

unsigned long TXsheet::id() const { return m_imp->m_id; }

//-----------------------------------------------------------------------------

int TXsheet::getFrameCount() const { return m_imp->m_frameCount; }

//-----------------------------------------------------------------------------

const TXshCell &TXsheet::getCell(int row, int col) const {
  return getCell(CellPosition(row, col));
}

const TXshCell &TXsheet::getCell(const CellPosition &pos) const {
  static const TXshCell emptyCell;

  TXshColumnP column = m_imp->m_columnSet.getColumn(pos.layer());
  if (!column) return emptyCell;
  TXshCellColumn *xshColumn = column->getCellColumn();
  if (!xshColumn) return emptyCell;
  return xshColumn->getCell(pos.frame());
}

//-----------------------------------------------------------------------------

bool TXsheet::setCell(int row, int col, const TXshCell &cell) {
  if (row < 0 || col < 0) return false;

  bool wasColumnEmpty = isColumnEmpty(col);
  TXshCellColumn *cellColumn;

  if (!cell.isEmpty()) {
    TXshLevel *level = cell.m_level.getPointer();
    assert(level);

    int levelType               = level->getType();
    TXshColumn::ColumnType type = TXshColumn::eLevelType;

    if (levelType == SND_XSHLEVEL)
      type = TXshColumn::eSoundType;
    else if (levelType == SND_TXT_XSHLEVEL)
      type = TXshColumn::eSoundTextType;
    else if (levelType == PLT_XSHLEVEL)
      type = TXshColumn::ePaletteType;
    else if (levelType == ZERARYFX_XSHLEVEL)
      type = TXshColumn::eZeraryFxType;
    else if (levelType == MESH_XSHLEVEL)
      type = TXshColumn::eMeshType;

    cellColumn = touchColumn(col, type)->getCellColumn();
  } else {
    TXshColumn *column = getColumn(col);
    cellColumn         = column ? column->getCellColumn() : 0;
  }

  if (!cellColumn || cellColumn->isLocked()) return false;

  cellColumn->setXsheet(this);

  if (!cellColumn->setCell(row, cell)) {
    if (wasColumnEmpty) {
      removeColumn(col);
      insertColumn(col);
    }

    return false;
  }

  TFx *fx = cellColumn->getFx();
  if (wasColumnEmpty && fx && fx->getOutputConnectionCount() == 0 &&
      cellColumn->getPaletteColumn() == 0)
    getFxDag()->addToXsheet(fx);

  if (cell.isEmpty())
    updateFrameCount();
  else if (row >= m_imp->m_frameCount)
    m_imp->m_frameCount = row + 1;

  TNotifier::instance()->notify(TXsheetChange());

  return true;
}

//-----------------------------------------------------------------------------

void TXsheet::getCells(int row, int col, int rowCount, TXshCell cells[]) const {
  static const TXshCell emptyCell;
  int i;
  TXshColumnP column = m_imp->m_columnSet.getColumn(col);
  if (!column) {
    for (i = 0; i < rowCount; i++) cells[i] = emptyCell;
    return;
  }
  TXshCellColumn *xshColumn = column->getCellColumn();
  if (!xshColumn) {
    for (i = 0; i < rowCount; i++) cells[i] = emptyCell;
    return;
  }
  xshColumn->getCells(row, rowCount, cells);
}

//-----------------------------------------------------------------------------

bool TXsheet::setCells(int row, int col, int rowCount, const TXshCell cells[]) {
  static const TXshCell emptyCell;
  int i = 0;
  while (i < rowCount && cells[i].isEmpty()) i++;

  // inserito da Elisa verso novembre 2009.
  // cosi' ha il difetto che se assegno celle vuote non fa nulla
  // per ora lo commento. bisogna indagare se questo rompe qualcosa

  // ho modificato il seguito per gestire il caso in cui i>=rowCount
  // => niente livelli dentro cells

  // if(i>=rowCount)
  //  return false;

  TXshColumn::ColumnType type = TXshColumn::eLevelType;
  if (i < rowCount) {
    TXshLevel *level = cells[i].m_level.getPointer();
    int levelType    = level->getType();

    if (levelType == SND_XSHLEVEL)
      type = TXshColumn::eSoundType;
    else if (levelType == SND_TXT_XSHLEVEL)
      type = TXshColumn::eSoundTextType;
    else if (levelType == PLT_XSHLEVEL)
      type = TXshColumn::ePaletteType;
    else if (levelType == ZERARYFX_XSHLEVEL)
      type = TXshColumn::eZeraryFxType;
    else if (levelType == MESH_XSHLEVEL)
      type = TXshColumn::eMeshType;
  }
  bool wasColumnEmpty    = isColumnEmpty(col);
  TXshCellColumn *column = touchColumn(col, type)->getCellColumn();
  if (!column) return false;

  int oldColRowCount = column->getMaxFrame() + 1;
  bool ret           = column->setCells(row, rowCount, cells);
  if (!ret || column->isLocked()) {
    if (wasColumnEmpty) {
      removeColumn(col);
      insertColumn(col);
    }
    return false;
  }
  int newColRowCount = column->getMaxFrame() + 1;

  TFx *fx = column->getFx();
  if (wasColumnEmpty && fx && fx->getOutputConnectionCount() == 0)
    getFxDag()->addToXsheet(fx);
  column->setXsheet(this);

  if (newColRowCount > m_imp->m_frameCount)
    m_imp->m_frameCount = newColRowCount;
  else {
    if (oldColRowCount == m_imp->m_frameCount &&
        newColRowCount < m_imp->m_frameCount)
      updateFrameCount();
  }

  return true;
}

//-----------------------------------------------------------------------------

void TXsheet::insertCells(int row, int col, int rowCount) {
  TXshColumnP column = m_imp->m_columnSet.getColumn(col);
  if (!column || column->isLocked()) return;
  TXshCellColumn *xshColumn = column->getCellColumn();
  if (!xshColumn) return;
  xshColumn->insertEmptyCells(row, rowCount);
  // aggiorno il frame count
  int fc = xshColumn->getMaxFrame() + 1;
  if (fc > m_imp->m_frameCount) m_imp->m_frameCount = fc;
}

//-----------------------------------------------------------------------------

void TXsheet::removeCells(int row, int col, int rowCount) {
  TXshColumnP column = m_imp->m_columnSet.getColumn(col);
  if (!column || column->isLocked()) return;

  TXshCellColumn *xshCellColumn = column->getCellColumn();
  if (!xshCellColumn) return;

  int oldColRowCount = xshCellColumn->getMaxFrame() + 1;
  xshCellColumn->removeCells(row, rowCount);

  // aggiornamento framecount
  if (oldColRowCount == m_imp->m_frameCount) updateFrameCount();

  TNotifier::instance()->notify(TXsheetChange());
}

//-----------------------------------------------------------------------------

void TXsheet::clearCells(int row, int col, int rowCount) {
  const TXshColumnP &column = m_imp->m_columnSet.getColumn(col);
  if (!column || column->isLocked()) return;

  TXshCellColumn *xshCellColumn = column->getCellColumn();
  if (!xshCellColumn) return;

  int oldColRowCount = xshCellColumn->getMaxFrame() + 1;
  xshCellColumn->clearCells(row, rowCount);

  // aggiornamento framecount
  if (oldColRowCount == m_imp->m_frameCount) updateFrameCount();
}

//-----------------------------------------------------------------------------

void TXsheet::clearAll() {
  int c0 = 0, c1 = m_imp->m_columnSet.getColumnCount() - 1;
  int r0 = 0, r1 = getFrameCount() - 1;
  m_imp->m_columnSet.clear();

  if (m_imp->m_pegTree) {
    delete m_imp->m_pegTree;
    m_imp->m_pegTree = new TStageObjectTree();
    m_imp->m_pegTree->setHandleManager(m_imp->m_handleManager);
    m_imp->m_pegTree->createGrammar(this);
  }

  if (m_imp->m_fxDag) {
    delete m_imp->m_fxDag;
    m_imp->m_fxDag = new FxDag();
  }

  m_imp->m_frameCount = 0;
  m_imp->m_mixedSound = 0;
}

//-----------------------------------------------------------------------------

int TXsheet::getCellRange(int col, int &r0, int &r1) const {
  r0                 = 0;
  r1                 = -1;
  TXshColumnP column = m_imp->m_columnSet.getColumn(col);
  if (!column) return 0;
  TXshCellColumn *cellColumn = column->getCellColumn();
  if (!cellColumn) return 0;
  return cellColumn->getRange(r0, r1);
}

//-----------------------------------------------------------------------------

int TXsheet::getMaxFrame(int col) const {
  TXshColumnP column = m_imp->m_columnSet.getColumn(col);
  if (!column) return 0;
  return column->getMaxFrame();
}

//-----------------------------------------------------------------------------

bool TXsheet::isColumnEmpty(int col) const {
  TXshColumnP column = m_imp->m_columnSet.getColumn(col);
  return column ? column->isEmpty() : true;
}

//-----------------------------------------------------------------------------

void TXsheet::getUsedLevels(set<TXshLevel *> &levels) const {
  set<const TXsheet *> visitedXshs;
  vector<const TXsheet *> todoXshs;

  visitedXshs.insert(this);
  todoXshs.push_back(this);

  while (!todoXshs.empty()) {
    const TXsheet *xsh = todoXshs.back();
    todoXshs.pop_back();

    int c0 = 0, c1 = xsh->getColumnCount() - 1;
    for (int c = c0; c <= c1; ++c) {
      TXshColumnP column = const_cast<TXsheet *>(xsh)->getColumn(c);
      if (!column) continue;

      TXshCellColumn *cellColumn = column->getCellColumn();
      if (!cellColumn) continue;

      int r0, r1;
      if (!cellColumn->getRange(r0, r1)) continue;

      TXshLevel *level = 0;
      for (int r = r0; r <= r1; r++) {
        TXshCell cell = cellColumn->getCell(r);
        if (cell.isEmpty() || !cell.m_level) continue;

        if (level != cell.m_level.getPointer()) {
          level = cell.m_level.getPointer();
          levels.insert(level);
          if (level->getChildLevel()) {
            TXsheet *childXsh = level->getChildLevel()->getXsheet();
            if (visitedXshs.count(childXsh) == 0) {
              visitedXshs.insert(childXsh);
              todoXshs.push_back(childXsh);
            }
          }
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

bool TXsheet::isLevelUsed(TXshLevel *level) const {
  set<TXshLevel *> levels;
  getUsedLevels(levels);
  return levels.count(level) > 0;
}

//-----------------------------------------------------------------------------

TStageObject *TXsheet::getStageObject(const TStageObjectId &id) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id);
}

//-----------------------------------------------------------------------------

TStageObjectTree *TXsheet::getStageObjectTree() const {
  return m_imp->m_pegTree;
}

//-----------------------------------------------------------------------------

TAffine TXsheet::getPlacement(const TStageObjectId &id, int frame) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->getPlacement(frame);
}

//-----------------------------------------------------------------------------

double TXsheet::getZ(const TStageObjectId &id, int frame) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->getZ(frame);
}

//-----------------------------------------------------------------------------

double TXsheet::getNoScaleZ(const TStageObjectId &id) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->getNoScaleZ();
}

//-----------------------------------------------------------------------------

TAffine TXsheet::getParentPlacement(const TStageObjectId &id, int frame) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->getParentPlacement(frame);
}

//-----------------------------------------------------------------------------

TPointD TXsheet::getCenter(const TStageObjectId &id, int frame) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->getCenter(frame);
}

//-----------------------------------------------------------------------------

void TXsheet::setCenter(const TStageObjectId &id, int frame,
                        const TPointD &center) {
  assert(id != TStageObjectId::NoneId);
  m_imp->m_pegTree->getStageObject(id)->setCenter(frame, center);
}

//-----------------------------------------------------------------------------

TStageObjectId TXsheet::getStageObjectParent(const TStageObjectId &id) {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->getParent();
}

//-----------------------------------------------------------------------------

void TXsheet::setStageObjectParent(const TStageObjectId &id,
                                   const TStageObjectId &parentId) {
  assert(id != TStageObjectId::NoneId);
  m_imp->m_pegTree->getStageObject(id)->setParent(parentId);
}

//-----------------------------------------------------------------------------

bool TXsheet::hasChildren(const TStageObjectId &id) const {
  assert(id != TStageObjectId::NoneId);
  return m_imp->m_pegTree->getStageObject(id)->hasChildren();
}

//-----------------------------------------------------------------------------

TAffine TXsheet::getCameraAff(int frame) const {
  TStageObjectId cameraId = getStageObjectTree()->getCurrentCameraId();
  TAffine cameraAff       = getPlacement(cameraId, frame);
  double cameraZ          = getZ(cameraId, frame);
  TAffine aff             = cameraAff * TScale((1000 + cameraZ) / 1000);
  return aff;
}

//-----------------------------------------------------------------------------

void TXsheet::reverseCells(int r0, int c0, int r1, int c1) {
  int rowCount = r1 - r0;
  if (rowCount < 0 || c1 - c0 < 0) return;

  for (int j = c0; j <= c1; j++) {
    int i1, i2;
    for (i1 = r0, i2 = r1; i1 < i2; i1++, i2--) {
      TXshCell app1 = getCell(CellPosition(i1, j));
      TXshCell app2 = getCell(CellPosition(i2, j));
      setCell(i1, j, app2);
      setCell(i2, j, app1);
    }
  }
}

//-----------------------------------------------------------------------------

void TXsheet::swingCells(int r0, int c0, int r1, int c1) {
  int rowCount = r1 - r0;
  if (rowCount < 0 || c1 - c0 < 0) return;
  int r0Mod = r1 + 1;
  for (int c = c0; c <= c1; ++c) insertCells(r0Mod, c, rowCount);

  for (int j = c0; j <= c1; j++) {
    for (int i1 = r0Mod, i2 = r1 - 1; i2 >= r0; i1++, i2--) {
      TXshCell cell = getCell(CellPosition(i2, j));
      setCell(i1, j, cell);
    }
  }
}

//-----------------------------------------------------------------------------

bool TXsheet::incrementCells(int r0, int c0, int r1, int c1,
                             vector<std::pair<TRect, TXshCell>> &forUndo) {
  for (int j = c0; j <= c1; j++) {
    int i = r0;
    while (getCell(CellPosition(i, j)).isEmpty() && i <= r1 - 1) i++;

    for (; i <= r1 - 1; i++) {
      if (getCell(CellPosition(i + 1, j)).isEmpty()) break;
      const TXshCell &ce1 = getCell(CellPosition(i, j)),
                     &ce2 = getCell(CellPosition(i + 1, j));
      if (ce2.getSimpleLevel() != ce1.getSimpleLevel() ||
          ce2.getFrameId().getNumber() < ce1.getFrameId().getNumber())
        return false;
    }
    i = r0;
    while (getCell(CellPosition(i, j)).isEmpty() && i <= r1 - 1) i++;
    int count;
    for (; i <= r1 - 1; i++) {
      count = 1;
      if (getCell(CellPosition(i + 1, j)).isEmpty()) continue;

      int frame1 = getCell(CellPosition(i, j)).getFrameId().getNumber();
      if (frame1 == -1) break;
      while (!getCell(CellPosition(i + 1, j)).isEmpty() &&
             getCell(CellPosition(i + 1, j)).getFrameId().getNumber() ==
                 getCell(CellPosition(i, j)).getFrameId().getNumber())
        i++, count++;

      int frame2 = getCell(CellPosition(i + 1, j)).getFrameId().getNumber();
      if (frame2 == -1) break;

      if (frame1 + count == frame2)
        continue;
      else if (frame1 + count < frame2)  // add
      {
        int numCells = frame2 - frame1 - count;
        insertCells(i + 1, j, numCells);
        forUndo.push_back(std::pair<TRect, TXshCell>(
            TRect(i + 1, j, i + 1 + numCells - 1, j), TXshCell()));
        for (int k = 1; k <= numCells; k++)
          setCell(i + k, j, getCell(CellPosition(i, j)));
        i += numCells;
        r1 += numCells;
      } else  // remove
      {
        int numCells = count - frame2 + frame1;
        i            = i - numCells;
        forUndo.push_back(
            std::pair<TRect, TXshCell>(TRect(i + 1, j, i + 1 + numCells - 1, j),
                                       getCell(CellPosition(i + 1, j))));
        removeCells(i + 1, j, numCells);
        r1 -= numCells;
      }
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

void TXsheet::duplicateCells(int r0, int c0, int r1, int c1, int upTo) {
  assert(upTo >= r1 + 1);
  int chunk = r1 - r0 + 1;

  for (int j = c0; j <= c1; j++) {
    insertCells(r1 + 1, j, upTo - (r1 + 1) + 1);
    for (int i = r1 + 1; i <= upTo; i++)
      setCell(i, j, getCell(CellPosition(r0 + ((i - (r1 + 1)) % chunk), j)));
  }
}

//-----------------------------------------------------------------------------

void TXsheet::stepCells(int r0, int c0, int r1, int c1, int type) {
  int nr = r1 - r0 + 1;
  int nc = c1 - c0 + 1;
  if (nr < 1 || nc <= 0) return;
  int size = nr * nc;
  std::unique_ptr<TXshCell[]> cells(new TXshCell[size]);
  if (!cells) return;
  // salvo il contenuto delle celle in cells
  int k = 0;
  for (int r = r0; r <= r1; r++)
    for (int c = c0; c <= c1; c++) {
      cells[k++] = getCell(CellPosition(r, c));
    }

  int nrows = nr * (type - 1);

  for (int c = c0; c <= c1; ++c) insertCells(r1 + 1, c, nrows);

  for (int j = c0; j <= c1; j++) {
    int i, k;
    for (i = r0, k = j - c0; k < size; k += nc) {
      for (int i1 = 0; i1 < type; i1++) {
        if (cells[k].isEmpty())
          clearCells(i + i1, j);
        else
          setCell(i + i1, j, cells[k]);
      }
      i += type;  // dipende dal tipo di step (2 o 3 per ora)
    }
  }
}

//-----------------------------------------------------------------------------

void TXsheet::increaseStepCells(int r0, int c0, int &r1, int c1) {
  int c, size = r1 - r0 + 1;
  QList<int> ends;
  for (c = c0; c <= c1; c++) {
    int r = r0, i = 0, rEnd = r1;
    while (r <= rEnd) {
      TXshCell cell = getCell(CellPosition(r, c));
      if (!cell.isEmpty()) {
        insertCells(r, c);
        setCell(r, c, cell);
        rEnd++;
        r++;
        while (cell == getCell(CellPosition(r, c)) && r <= rEnd) r++;
      } else
        r++;
      i++;
    }
    ends.append(rEnd);
  }
  if (ends.isEmpty()) return;
  // controllo se devo cambiare la selezione
  bool allIncreaseIsEqual = true;
  for (c = 0; c < ends.size() - 1 && allIncreaseIsEqual; c++)
    allIncreaseIsEqual       = allIncreaseIsEqual && ends[c] == ends[c + 1];
  if (allIncreaseIsEqual) r1 = ends[0];
}

//-----------------------------------------------------------------------------

void TXsheet::decreaseStepCells(int r0, int c0, int &r1, int c1) {
  int c, size = r1 - r0 + 1;
  QList<int> ends;
  for (c = c0; c <= c1; c++) {
    int r = r0, i = 0, rEnd = r1;
    while (r <= rEnd) {
      TXshCell cell = getCell(CellPosition(r, c));
      if (!cell.isEmpty()) {
        r++;
        bool removed = false;
        while (cell == getCell(CellPosition(r, c)) && r <= rEnd) {
          if (!removed) {
            removed = true;
            removeCells(r, c);
            rEnd--;
          } else
            r++;
        }
      } else
        r++;
      i++;
    }
    ends.append(rEnd);
  }
  if (ends.isEmpty()) return;
  // controllo se devo cambiare la selezione
  bool allDecreaseIsEqual = true;
  for (c = 0; c < ends.size() - 1 && allDecreaseIsEqual; c++)
    allDecreaseIsEqual       = allDecreaseIsEqual && ends[c] == ends[c + 1];
  if (allDecreaseIsEqual) r1 = ends[0];
}

//-----------------------------------------------------------------------------

void TXsheet::eachCells(int r0, int c0, int r1, int c1, int type) {
  int nr = r1 - r0 + 1;
  int nc = c1 - c0 + 1;
  if (nr < type || nc <= 0) return;

  int newRows = nr % type ? nr / type + 1 : nr / type;

  int size = newRows * nc;
  assert(size > 0);
  std::unique_ptr<TXshCell[]> cells(new TXshCell[size]);
  assert(cells);

  int i, j, k;
  for (j = r0, i = 0; i < size;
       j += type)  // in cells copio il contenuto delle celle che mi interessano
  {
    for (k = c0; k <= c1; k++, i++) cells[i] = getCell(CellPosition(j, k));
  }

  int c;
  for (c = c0; c <= c1; ++c) removeCells(r0 + newRows, c, nr - newRows);

  for (i = r0, k = 0; i < r0 + newRows && k < size; i++)
    for (j = c0; j <= c1; j++) {
      //----110523 iwasawa
      // Eachでできた空きセルに、操作前のセルの中身が残ってしまう不具合を修正
      if (cells[k].isEmpty())
        clearCells(i, j);
      else
        setCell(i, j, cells[k]);
      k++;
    }
}

//-----------------------------------------------------------------------------
/*! force cells order in n-steps. returns the row amount after process
 */
int TXsheet::reframeCells(int r0, int r1, int col, int type) {
  // Row amount in the selection
  int nr = r1 - r0 + 1;

  if (nr < 1) return 0;

  QVector<TXshCell> cells;

  cells.clear();

  for (int r = r0; r <= r1; r++) {
    if (cells.size() == 0 || cells.last() != getCell(CellPosition(r, col)))
      cells.push_back(getCell(CellPosition(r, col)));
  }

  if (cells.empty()) return 0;

  // row amount after n-step
  int nrows = cells.size() * type;

  // if needed, insert cells
  if (nr < nrows) {
    insertCells(r1 + 1, col, nrows - nr);
  }
  // if needed, remove cells
  else if (nr > nrows) {
    removeCells(r0 + nrows, col, nr - nrows);
  }

  for (int i = r0, k = 0; i < r0 + nrows; k++) {
    for (int i1 = 0; i1 < type; i1++) {
      if (cells[k].isEmpty())
        clearCells(i + i1, col);
      else
        setCell(i + i1, col, cells[k]);
    }
    i += type;  // dipende dal tipo di step (2 o 3 per ora)
  }

  return nrows;  // return row amount after process
}

//-----------------------------------------------------------------------------

void TXsheet::resetStepCells(int r0, int c0, int r1, int c1) {
  int c, size = r1 - r0 + 1;
  for (c = c0; c <= c1; c++) {
    int r = r0, i = 0;
    TXshCell *cells = new TXshCell[size];
    while (r <= r1) {
      // mi prendo le celle che mi servono
      cells[i] = getCell(CellPosition(r, c));
      r++;
      while (cells[i] == getCell(CellPosition(r, c)) && r <= r1) r++;
      i++;
    }

    size = i;
    removeCells(r0, c, r1 - r0 + 1);
    insertCells(r0, c, i);
    i = 0;
    r = r0;
    for (i = 0; i < size; i++, r++) setCell(r, c, cells[i]);
  }
}

//-----------------------------------------------------------------------------
/*! Roll first cells of rect r0,c0,r1,c1. Move cells contained in first row to
 * last row.
*/
void TXsheet::rollupCells(int r0, int c0, int r1, int c1) {
  int nc   = c1 - c0 + 1;
  int size = 1 * nc;
  assert(size > 0);
  std::unique_ptr<TXshCell[]> cells(new TXshCell[size]);
  assert(cells);

  // in cells copio il contenuto delle celle che mi interessano
  int k;
  for (k = c0; k <= c1; k++) cells[k - c0] = getCell(CellPosition(r0, k));

  for (k = c0; k <= c1; k++) removeCells(r0, k, 1);

  for (k = c0; k <= c1; k++) {
    insertCells(r1, k, 1);
    setCell(r1, k, cells[k - c0]);  // setto le celle
  }
}

//-----------------------------------------------------------------------------
/*! Roll last cells of rect r0,c0,r1,c1. Move cells contained in last row to
 * first row.
*/
void TXsheet::rolldownCells(int r0, int c0, int r1, int c1) {
  int nc   = c1 - c0 + 1;
  int size = 1 * nc;
  assert(size > 0);
  std::unique_ptr<TXshCell[]> cells(new TXshCell[size]);
  assert(cells);

  // in cells copio il contenuto delle celle che mi interessano
  int k;
  for (k = c0; k <= c1; k++) cells[k - c0] = getCell(CellPosition(r1, k));

  for (k = c0; k <= c1; k++) removeCells(r1, k, 1);

  for (k = c0; k <= c1; k++) {
    insertCells(r0, k, 1);
    setCell(r0, k, cells[k - c0]);  // setto le celle
  }
}

//-----------------------------------------------------------------------------
/*! Stretch cells contained in rect r0,c0,r1,c1, from r1-r0+1 to nr.
                If nr>r1-r0+1 add cells, overwise remove cells. */
void TXsheet::timeStretch(int r0, int c0, int r1, int c1, int nr) {
  int oldNr = r1 - r0 + 1;
  if (nr > oldNr) /* ingrandisce */
  {
    int c;
    for (c = c0; c <= c1; c++) {
      int dn = nr - oldNr;
      assert(oldNr > 0);
      std::unique_ptr<TXshCell[]> cells(new TXshCell[oldNr]);
      assert(cells);
      getCells(r0, c, oldNr, cells.get());
      insertCells(r0 + 1, c, dn);
      int i;
      for (i = nr - 1; i >= 0; i--) {
        int j = i * double(oldNr) / double(nr);
        if (j < i) setCell(i + r0, c, cells[j]);
      }
    }
  } else /* rimpicciolisce */
  {
    int c;
    for (c = c0; c <= c1; c++) {
      int dn = oldNr - nr;
      std::unique_ptr<TXshCell[]> cells(new TXshCell[oldNr]);
      assert(cells);
      getCells(r0, c, oldNr, cells.get());
      int i;
      for (i = 0; i < nr; i++) {
        int j = i * double(oldNr) / double(nr);
        if (j > i) setCell(i + r0, c, cells[j]);
      }
      removeCells(r1 - dn + 1, c, dn);
    }
  }
}

//-----------------------------------------------------------------------------

int TXsheet::exposeLevel(int row, int col, TXshLevel *xl, bool overwrite) {
  if (!xl) return 0;
  std::vector<TFrameId> fids;
  xl->getFids(fids);
  int frameCount = 1;
  if (fids.empty()) {
    setCell(row, col, TXshCell(xl, TFrameId(1)));
    updateFrameCount();
    return frameCount;
  }
  exposeLevel(row, col, xl, fids, overwrite);
  return (int)fids.size();
}

//-----------------------------------------------------------------------------
// customized version for load level popup
int TXsheet::exposeLevel(int row, int col, TXshLevel *xl,
                         std::vector<TFrameId> &fIds_, int xFrom, int xTo,
                         int step, int inc, int frameCount,
                         bool doesFileActuallyExist) {
  if (!xl) return 0;
  std::vector<TFrameId> fids;

  if (doesFileActuallyExist)
    xl->getFids(fids);
  else {
    for (int i = 0; i < (int)fIds_.size(); i++) {
      fids.push_back(fIds_[i]);
    }
  }

  // multiple exposing
  if (frameCount < 0 || xFrom < 0 || xTo < 0 || step < 0 || inc < 0) {
    insertCells(row, col, xl->getFrameCount());

    frameCount = 1;
    if (fids.empty())
      setCell(row, col, TXshCell(xl, TFrameId(1)));
    else {
      frameCount = (int)fids.size();
      insertCells(row, col, frameCount);
      std::vector<TFrameId>::iterator it;
      for (it = fids.begin(); it != fids.end(); ++it)
        setCell(row++, col, TXshCell(xl, *it));
    }
    updateFrameCount();
    return frameCount;
  }

  // single exposing

  insertCells(row, col, frameCount);

  if (fids.empty()) {
    setCell(row, col, TXshCell(xl, TFrameId(1)));
  } else {
    if (inc == 0)  // inc = Auto
    {
      std::vector<TFrameId>::iterator it;
      it = fids.begin();
      while (it->getNumber() < xFrom) it++;

      if (step == 0)  // Step = Auto
      {
        std::vector<TFrameId>::iterator next_it;
        next_it = it;
        next_it++;

        int startFrame = it->getNumber();

        for (int f = startFrame; f < startFrame + frameCount; f++) {
          if (next_it != fids.end() && f >= next_it->getNumber()) {
            it++;
            next_it++;
          }
          setCell(row++, col, TXshCell(xl, *it));
        }
      }

      else  // Step != Auto
      {
        int loopCount = frameCount / step;
        for (int loop = 0; loop < loopCount; loop++) {
          for (int s = 0; s < step; s++) {
            setCell(row++, col, TXshCell(xl, *it));
          }
          it++;
        }
      }

    } else  // inc != Auto
    {
      int loopCount;
      if (step == 0)  // Step = Auto
        step = inc;

      loopCount = frameCount / step;

      for (int loop = 0; loop < loopCount; loop++) {
        TFrameId id(xFrom + loop * inc, fids.begin()->getLetter());
        for (int s = 0; s < step; s++) {
          setCell(row++, col, TXshCell(xl, id));
        }
      }
    }
  }
  updateFrameCount();
  return frameCount;
}

//-----------------------------------------------------------------------------

void TXsheet::exposeLevel(int row, int col, TXshLevel *xl,
                          std::vector<TFrameId> fids, bool overwrite) {
  int frameCount = (int)fids.size();
  if (!overwrite) insertCells(row, col, frameCount);
  std::vector<TFrameId>::iterator it;
  for (it = fids.begin(); it != fids.end(); ++it)
    setCell(row++, col, TXshCell(xl, *it));
  updateFrameCount();
}

//-----------------------------------------------------------------------------

void TXsheet::updateFrameCount() {
  m_imp->m_frameCount = 0;
  for (int i = 0; i < m_imp->m_columnSet.getColumnCount(); ++i) {
    TXshColumnP cc = m_imp->m_columnSet.getColumn(i);
    if (cc && !cc->isEmpty())
      m_imp->m_frameCount =
          std::max(m_imp->m_frameCount, cc->getMaxFrame() + 1);
  }
}

//-----------------------------------------------------------------------------

void TXsheet::loadData(TIStream &is) {
  clearAll();
  TStageObjectId cameraId   = TStageObjectId::CameraId(0);
  TStageObject *firstCamera = getStageObject(cameraId);
  m_imp->m_pegTree->removeStageObject(cameraId);

  int col = 0;
  string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "columns") {
      while (!is.eos()) {
        TPersist *p = 0;
        is >> p;
        TXshColumn *column = dynamic_cast<TXshColumn *>(p);
        if (!column) throw TException("expected xsheet column");
        m_imp->m_columnSet.insertColumn(col++, column);
        column->setXsheet(this);
        if (TXshZeraryFxColumn *zc =
                dynamic_cast<TXshZeraryFxColumn *>(column)) {
          TFx *fx         = zc->getZeraryColumnFx()->getZeraryFx();
          int fxTypeCount = m_imp->m_fxDag->getFxTypeCount(fx);
          int maxFxTypeId = std::max(fxTypeCount, fx->getAttributes()->getId());
          m_imp->m_fxDag->updateFxTypeTable(fx, maxFxTypeId);
          m_imp->m_fxDag->updateFxIdTable(fx);
          for (int j = 0; j < fx->getParams()->getParamCount(); j++) {
            TParam *param = fx->getParams()->getParam(j);
            if (TDoubleParam *dp = dynamic_cast<TDoubleParam *>(param))
              getStageObjectTree()->setGrammar(dp);
            else if (dynamic_cast<TPointParam *>(param) ||
                     dynamic_cast<TRangeParam *>(param) ||
                     dynamic_cast<TPixelParam *>(param)) {
              TParamSet *paramSet = dynamic_cast<TParamSet *>(param);
              assert(paramSet);
              int f;
              for (f = 0; f < paramSet->getParamCount(); f++) {
                TDoubleParam *dp = dynamic_cast<TDoubleParam *>(
                    paramSet->getParam(f).getPointer());
                if (!dp) continue;
                getStageObjectTree()->setGrammar(dp);
              }
            }
          }
        }
      }
    } else if (tagName == "pegbars") {
      TPersist *p = m_imp->m_pegTree;
      is >> *p;
    } else if (tagName == "fxnodes") {
      m_imp->m_fxDag->loadData(is);
      std::vector<TFx *> fxs;
      m_imp->m_fxDag->getFxs(fxs);
      for (int i = 0; i < (int)fxs.size(); i++) {
        TFx *fx = fxs[i];
        for (int j = 0; j < fx->getParams()->getParamCount(); j++) {
          TParam *param = fx->getParams()->getParam(j);
          if (TDoubleParam *dp = dynamic_cast<TDoubleParam *>(param))
            getStageObjectTree()->setGrammar(dp);
          else if (dynamic_cast<TPointParam *>(param) ||
                   dynamic_cast<TRangeParam *>(param) ||
                   dynamic_cast<TPixelParam *>(param)) {
            TParamSet *paramSet = dynamic_cast<TParamSet *>(param);
            assert(paramSet);
            int f;
            for (f = 0; f < paramSet->getParamCount(); f++) {
              TDoubleParam *dp = dynamic_cast<TDoubleParam *>(
                  paramSet->getParam(f).getPointer());
              if (!dp) continue;
              getStageObjectTree()->setGrammar(dp);
            }
          }
        }
      }

      if (is.matchEndTag()) continue;

      // was ist dass?
      TFxSet fxSet;
      fxSet.loadData(is);
    } else if (tagName == "columnFan") {
      m_imp->m_columnFans[0].loadData(is);
      m_imp->copyFoldedState();
    } else if (tagName == "noteSet") {
      m_notes->loadData(is);
    } else {
      throw TException("xsheet, unknown tag: " + tagName);
    }
    is.closeChild();
  }
  updateFrameCount();
}

//-----------------------------------------------------------------------------

void TXsheet::saveData(TOStream &os) {
  os.openChild("columns");
  for (int c = 0; c < m_imp->m_columnSet.getColumnCount(); ++c) {
    TXshColumnP column = m_imp->m_columnSet.getColumn(c);
    if (column && c < getFirstFreeColumnIndex()) os << column.getPointer();
  }
  os.closeChild();
  os.openChild("pegbars");
  m_imp->m_pegTree->saveData(os, getFirstFreeColumnIndex());
  // os << *(m_imp->m_pegTree);
  os.closeChild();

  FxDag *fxDag = getFxDag();
  os.openChild("fxnodes");
  fxDag->saveData(os, getFirstFreeColumnIndex());
  os.closeChild();

  // does not matter which Orientation to take, as all fans share folded data
  ColumnFan *columnFan = getColumnFan(Orientations::topToBottom());
  if (!columnFan->isEmpty()) {
    os.openChild("columnFan");
    columnFan->saveData(os);
    os.closeChild();
  }

  TXshNoteSet *notes = getNotes();
  if (notes->getCount() > 0) {
    os.openChild("noteSet");
    notes->saveData(os);
    os.closeChild();
  }
}

//-----------------------------------------------------------------------------

PERSIST_IDENTIFIER(TXsheet, "xsheet")

//-----------------------------------------------------------------------------

void TXsheet::insertColumn(int col, TXshColumn::ColumnType type) {
  insertColumn(col, TXshColumn::createEmpty(type));
}

//-----------------------------------------------------------------------------

void TXsheet::insertColumn(int col, TXshColumn *column) {
  column->setXsheet(this);
  m_imp->m_columnSet.insertColumn(col, column);
  m_imp->m_pegTree->insertColumn(col);
  if (column->getPaletteColumn() ==
      0)  // palette column are not connected to the xsheet fx node
  {
    TFx *fx = column->getFx();
    if (fx) getFxDag()->addToXsheet(fx);
  }
}

//-----------------------------------------------------------------------------

void TXsheet::removeColumn(int col) {
  TXshColumn *column = getColumn(col);
  if (column) {
    TFx *fx = column->getFx();
    if (fx) {
      getFxDag()->removeFromXsheet(fx);

      // disconnetto dal columnFx tutti gli effetti connessi in uscita
      TFxPort *outPort = 0;
      while ((outPort = fx->getOutputConnection(0))) outPort->setFx(0);
    }
  }
  m_imp->m_columnSet.removeColumn(col);
  m_imp->m_pegTree->removeColumn(col);
}

//-----------------------------------------------------------------------------

void TXsheet::moveColumn(int srcIndex, int dstIndex) {
  if (srcIndex == dstIndex) return;
  assert(srcIndex >= 0);
  assert(dstIndex >= 0);
  int col = std::max(srcIndex, dstIndex);
  if (col >= m_imp->m_columnSet.getColumnCount()) {
    int n = m_imp->m_columnSet.getColumnCount();
    touchColumn(col, TXshColumn::eLevelType);
    while (n <= col) {
      TXshColumn *column = getColumn(n);
      assert(column);
      column->setXsheet(this);
      n++;
    }
  }
  assert(m_imp->m_columnSet.getColumnCount() > srcIndex);
  assert(m_imp->m_columnSet.getColumnCount() > dstIndex);

  if (srcIndex < dstIndex) {
    int c0 = srcIndex;
    int c1 = dstIndex;
    assert(c0 < c1);
    m_imp->m_columnSet.rollLeft(c0, c1 - c0 + 1);
    for (int c = c0; c < c1; ++c) m_imp->m_pegTree->swapColumns(c, c + 1);
  } else {
    int c0 = dstIndex;
    int c1 = srcIndex;
    assert(c0 < c1);
    m_imp->m_columnSet.rollRight(c0, c1 - c0 + 1);
    for (int c = c1 - 1; c >= c0; --c) m_imp->m_pegTree->swapColumns(c, c + 1);
  }
}

//-----------------------------------------------------------------------------

TXshColumn *TXsheet::getColumn(int col) const {
  return m_imp->m_columnSet.getColumn(col).getPointer();
}

//-----------------------------------------------------------------------------

int TXsheet::getColumnCount() const {
  return m_imp->m_columnSet.getColumnCount();
}

//-----------------------------------------------------------------------------

int TXsheet::getFirstFreeColumnIndex() const {
  int i = getColumnCount();
  while (i > 0 && isColumnEmpty(i - 1)) --i;
  return i;
}

//-----------------------------------------------------------------------------

TXshColumn *TXsheet::touchColumn(int index, TXshColumn::ColumnType type) {
  TXshColumn *column = m_imp->m_columnSet.touchColumn(index, type).getPointer();
  if (!column) return 0;

  // NOTE (Daniele): The following && should be a bug... but I fear I'd break
  // something changing it.
  // Observe that the implied behavior is that of REPLACING AN EXISTING
  // LEGITIMATE COLUMN!
  // Please, Inquire further if you're not upon release!

  if (column->isEmpty() && column->getColumnType() != type) {
    removeColumn(index);
    insertColumn(index, type);
    column = getColumn(index);
  }

  return column;
}

//=============================================================================
namespace {  // Utility function
//-----------------------------------------------------------------------------

void searchAudioColumn(TXsheet *xsh, std::vector<TXshSoundColumn *> &sounds,
                       bool isPreview = true) {
  int i       = 0;
  int columns = xsh->getColumnCount();

  for (; i < columns; ++i) {
    TXshColumn *column = xsh->getColumn(i);
    if (column) {
      TXshSoundColumn *soundCol = column->getSoundColumn();
      if (soundCol && ((isPreview && soundCol->isPreviewVisible()) ||
                       (!isPreview && soundCol->isCamstandVisible()))) {
        sounds.push_back(soundCol);
        continue;
      }
    }
  }
}
//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

TSoundTrack *TXsheet::makeSound(SoundProperties *properties) {
  std::vector<TXshSoundColumn *> sounds;
  searchAudioColumn(this, sounds, properties->m_isPreview);
  if (!m_imp->m_mixedSound || *properties != *m_soundProperties) {
    if (!sounds.empty() && properties->m_fromFrame <= properties->m_toFrame)
      m_imp->m_mixedSound = sounds[0]->mixingTogether(
          sounds, properties->m_fromFrame, properties->m_toFrame,
          properties->m_frameRate);
    else
      m_imp->m_mixedSound = 0;
    delete m_soundProperties;
    m_soundProperties = properties;
  } else
    delete properties;
  return m_imp->m_mixedSound.getPointer();
}

//-----------------------------------------------------------------------------

void TXsheet::scrub(int frame, bool isPreview) {
  double fps =
      getScene()->getProperties()->getOutputProperties()->getFrameRate();

  TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
  prop->m_isPreview              = isPreview;

  TSoundTrack *st = makeSound(prop);  // Absorbs prop's ownership
  if (!st) return;

  double samplePerFrame = st->getSampleRate() / fps;

  double s0 = frame * samplePerFrame, s1 = s0 + samplePerFrame;

  play(st, s0, s1, false);
}

//-----------------------------------------------------------------------------

void TXsheet::stopScrub() {
  if (m_player) m_player->stop();
}

//-----------------------------------------------------------------------------

void TXsheet::play(TSoundTrackP soundtrack, int s0, int s1, bool loop) {
  if (!TSoundOutputDevice::installed()) return;

  if (!m_player) m_player = new TSoundOutputDevice();

  if (m_player) {
    try {
      m_player->play(soundtrack, s0, s1, loop);
    } catch (TSoundDeviceException &) {
    }
  }
}

//-----------------------------------------------------------------------------

FxDag *TXsheet::getFxDag() const { return m_imp->m_fxDag; }

//-----------------------------------------------------------------------------

ColumnFan *TXsheet::getColumnFan(const Orientation *o) const {
  int index = o->dimension(PredefinedDimension::INDEX);
  return &m_imp->m_columnFans[index];
}

//-----------------------------------------------------------------------------

ToonzScene *TXsheet::getScene() const { return m_imp->m_scene; }

//-----------------------------------------------------------------------------

void TXsheet::setScene(ToonzScene *scene) { m_imp->m_scene = scene; }

//-----------------------------------------------------------------------------

bool TXsheet::checkCircularReferences(const TXshCell &cellCandidate) {
  if (cellCandidate.isEmpty() || !cellCandidate.m_level->getChildLevel())
    return false;
  TXsheet *childCandidate = cellCandidate.m_level->getChildLevel()->getXsheet();
  return checkCircularReferences(childCandidate);
}

//-----------------------------------------------------------------------------

bool TXsheet::checkCircularReferences(TXshColumn *columnCandidate) {
  if (!columnCandidate || !columnCandidate->getLevelColumn()) return false;
  TXshLevelColumn *lc = columnCandidate->getLevelColumn();
  int r0 = 0, r1 = -1;
  if (lc->getRange(r0, r1) <= 0) return false;
  int r;
  TXshCell oldCell;
  for (r = r0; r <= r1; r++) {
    TXshCell cell = lc->getCell(r);
    // to speed up:
    if (cell.m_level.getPointer() == oldCell.m_level.getPointer()) continue;
    if (checkCircularReferences(cell)) return true;
    oldCell = cell;
  }
  return false;
}

//-----------------------------------------------------------------------------

void TXsheet::invalidateSound() { m_imp->m_mixedSound = TSoundTrackP(); }

//-----------------------------------------------------------------------------

bool TXsheet::checkCircularReferences(TXsheet *childCandidate) {
  if (this == childCandidate) return true;
  if (childCandidate == 0) return false;
  int i;
  for (i = 0; i < childCandidate->getColumnCount(); i++)
    if (checkCircularReferences(childCandidate->getColumn(i))) return true;
  return false;
}

//-----------------------------------------------------------------------------

// Builds the camstand bbox associated to the specified xsheet
TRectD TXsheet::getBBox(int r) const {
  static const double maxDouble = (std::numeric_limits<double>::max)();
  static const TRectD voidRect(maxDouble, maxDouble, -maxDouble, -maxDouble);

  //-----------------------------------------------------------------------

  struct locals {
    static TRectD getBBox(const TXsheet *xsh, int r, int c) {
      // Discriminate cell content
      const TXshCell &cell = xsh->getCell(CellPosition(r, c));
      if (cell.isEmpty()) return voidRect;

      if (TXshChildLevel *cl = cell.getChildLevel())
        return cl->getXsheet()->getBBox(cell.getFrameId().getNumber() - 1);

      TXshSimpleLevel *sl = cell.getSimpleLevel();
      if (!sl ||
          !(sl->getType() &
            LEVELCOLUMN_XSHLEVEL))  // Avoid other mesh levels - which could
        return voidRect;            // be deformed too...

      // Retrieve column affine
      TAffine columnZaff;
      {
        TStageObject *colObj = xsh->getStageObject(TStageObjectId::ColumnId(c));

        const TAffine &columnAff = colObj->getPlacement(r);  // ...
        double columnZ           = colObj->getZ(r);          // ...
        double columnNoScaleZ    = colObj->getGlobalNoScaleZ();

        TStageObjectId cameraId =
            xsh->getStageObjectTree()->getCurrentCameraId();
        TStageObject *camera = xsh->getStageObject(cameraId);

        const TAffine &cameraAff = camera->getPlacement(r);  // ...
        double cameraZ           = camera->getZ(r);          // ...

        if (!TStageObject::perspective(columnZaff, cameraAff, cameraZ,
                                       columnAff, columnZ, columnNoScaleZ))
          return voidRect;
      }

      const TRectD &bbox = sl->getBBox(cell.getFrameId());
      if (bbox.getLx() <= 0.0 || bbox.getLy() <= 0.0) return voidRect;

      return columnZaff * TScale(Stage::inch, Stage::inch) * bbox;
    }
  };

  //-----------------------------------------------------------------------

  // Initialize a union-neutral rect
  TRectD bbox(voidRect);

  // Traverse the xsheet's columns, adding the bbox of each
  int c, cCount = getColumnCount();
  for (c = 0; c != cCount; ++c) {
    // Skip empty or invisible columns
    TXshColumn *column = getColumn(c);
    if (column->isEmpty() || !column->isCamstandVisible()) continue;

    const TRectD &colBBox = locals::getBBox(this, r, c);

    // Make the union
    bbox.x0 = std::min(bbox.x0, colBBox.x0);
    bbox.y0 = std::min(bbox.y0, colBBox.y0);
    bbox.x1 = std::max(bbox.x1, colBBox.x1);
    bbox.y1 = std::max(bbox.y1, colBBox.y1);
  }

  return bbox;
}

//-----------------------------------------------------------------------

bool TXsheet::isRectEmpty(const CellPosition &pos0,
                          const CellPosition &pos1) const {
  for (int frame = pos0.frame(); frame <= pos1.frame(); frame++)
    for (int layer = pos0.layer(); layer <= pos1.layer(); layer++)
      if (!getCell(CellPosition(frame, layer)).isEmpty()) return false;
  return true;
}