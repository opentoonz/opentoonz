

// TnzCore includes
#include "tstencilcontrol.h"
#include "tvectorgl.h"
#include "tvectorrenderdata.h"
#include "tcolorfunctions.h"
#include "tpalette.h"
#include "tropcm.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tcolorstyles.h"
#include "timage_io.h"
#include "tofflinegl.h"
#include "tpixelutils.h"
#include "tstroke.h"
#include "tenv.h"
#include "tregion.h"

// TnzLib includes
#include "toonz/stage2.h"
#include "toonz/stageplayer.h"
#include "toonz/stagevisitor.h"
#include "toonz/txsheet.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/onionskinmask.h"
#include "toonz/dpiscale.h"
#include "toonz/imagemanager.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/preferences.h"
#include "toonz/fill.h"
#include "toonz/levelproperties.h"
#include "toonz/autoclose.h"
#include "toonz/txshleveltypes.h"
#include "imagebuilders.h"

// Qt includes
#include <QImage>
#include <QPainter>
#include <QPolygon>
#include <QThreadStorage>
#include <QMatrix>

#include "toonz/stage.h"

//#define  NUOVO_ONION

//=============================================================================
/*! \namespace Stage
                \brief The Stage namespace provides a set of objects, class and
   method, useful to view images in display.
*/
using namespace Stage;

//=============================================================================

typedef std::vector<Player> PlayerSet;

//=============================================================================

/*! \var Stage::inch
                For historical reasons camera stand is defined in a coordinate
   system in which
                an inch is equal to 'Stage::inch' unit.
                Pay attention: modify this value condition apparent line
   thickness of
                images .pli.
*/
const double Stage::inch        = 53.33333;
const double Stage::standardDpi = 120;

namespace {
void updateOnionSkinSize(const PlayerSet &players) {
  assert(Player::m_onionSkinFrontSize == 0 && Player::m_onionSkinBackSize == 0);
  int i;
  int maxOnionSkinFrontValue = 0, maxOnionSkinBackValue = 0,
      firstBackOnionSkin = 0, lastBackVisibleSkin = 0;
  for (i = 0; i < (int)players.size(); i++) {
    Player player = players[i];
    if (player.m_onionSkinDistance == c_noOnionSkin) continue;
    if (player.m_onionSkinDistance > 0 &&
        maxOnionSkinFrontValue < player.m_onionSkinDistance)
      maxOnionSkinFrontValue = player.m_onionSkinDistance;
    if (player.m_onionSkinDistance < 0 &&
        maxOnionSkinBackValue > player.m_onionSkinDistance)
      maxOnionSkinBackValue = player.m_onionSkinDistance;
    if (firstBackOnionSkin == 0 && player.m_onionSkinDistance < 0)
      firstBackOnionSkin = player.m_onionSkinDistance;
    else if (player.m_onionSkinDistance < 0 &&
             firstBackOnionSkin < player.m_onionSkinDistance)
      firstBackOnionSkin = player.m_onionSkinDistance;
    // Level Editing Mode can send players at a depth beyond what is visible.
    if (player.m_onionSkinDistance < lastBackVisibleSkin &&
        player.m_isVisibleinOSM)
      lastBackVisibleSkin = player.m_onionSkinDistance;
  }
  Player::m_onionSkinFrontSize  = maxOnionSkinFrontValue;
  Player::m_onionSkinBackSize   = maxOnionSkinBackValue;
  Player::m_firstBackOnionSkin  = firstBackOnionSkin;
  Player::m_lastBackVisibleSkin = lastBackVisibleSkin;
}

//-----------------------------------------------------------------------------

bool descending(int i, int j) { return (i > j); }

//----------------------------------------------------------------
}

//=============================================================================
/*! The ZPlacement class preserve camera position informations.
*/
//=============================================================================

class ZPlacement {
public:
  TAffine m_aff;
  double m_z;
  ZPlacement() : m_aff(), m_z(0) {}
  ZPlacement(const TAffine &aff, double z) : m_aff(aff), m_z(z) {}
};

//=============================================================================
/*! The class PlayerLt allows a priority operator for player.
\n	Compare zdepth of two players.
*/
//=============================================================================

class PlayerLt {
public:
  PlayerLt() {}
  inline bool operator()(const Player &a, const Player &b) const {
    if (a.m_bingoOrder < b.m_bingoOrder)
      return true;
    else if (a.m_bingoOrder > b.m_bingoOrder)
      return false;
    return a.m_z < b.m_z;
  }
};

//=============================================================================

class StackingOrder {
public:
  StackingOrder() {}
  inline bool operator()(const std::pair<double, int> &a,
                         const std::pair<double, int> &b) const {
    return a.first < b.first;
  }
};

//=============================================================================
/*! The StageBuilder class finds and provides data for frame visualization.
\n	The class contains a \b PlayerSet, a vector of player, of all
necessary information.
*/
// Tutto cio che riguarda una "colonna maschera" non e' utilizzato in TOONZ ma
// in TAB Pro.
//=============================================================================

class StageBuilder {
public:
  struct SubXSheet {
    ZPlacement m_camera;
    TAffine m_aff, m_zaff;
    double m_z;
  };

public:
  enum ShiftTraceGhostId { NO_GHOST, FIRST_GHOST, SECOND_GHOST, TRACED };

public:
  PlayerSet m_players;
  std::vector<PlayerSet *> m_maskPool;

  std::vector<ZPlacement> m_placementStack;
  ZPlacement m_cameraPlacement;

  std::vector<SubXSheet> m_subXSheetStack;

  std::vector<int> m_masks;
  int m_onionSkinDistance;
  double m_fade;

  ShiftTraceGhostId m_shiftTraceGhostId;

  bool m_camera3d;
  OnionSkinMask m_onionSkinMask;

  int m_currentColumnIndex;   // application's current column index
  int m_ancestorColumnIndex;  // index of the column that is the deepest
                              // ancestor of the column being processed
  int m_currentXsheetLevel;   // level of the current xsheet, see: editInPlace
  int m_xsheetLevel;          // xsheet-level of the column being processed

  // for guided drawing
  TFrameId m_currentFrameId;
  int m_isGuidedDrawingEnabled;
  std::vector<TXshColumn *> m_ancestors;

  const ImagePainter::VisualSettings *m_vs;

public:
  StageBuilder();
  virtual ~StageBuilder();

  /*! Add in \b players vector informations about specify cell.
  \n	Analyze cell in row \b row and column \b col of xsheet \b xsh. If level
                  containing this cell is a simple level, \b TXshSimpleLevel,
  create a Pleyer
                  object with informations of the cell. Else if level is a child
  level,
                  \b TXshChildLevel, recall \b addFrame().
  */
  void addCell(PlayerSet &players, ToonzScene *scene, TXsheet *xsh, int row,
               int col, int level);

  /*! Verify if onion-skin is active and recall \b addCell().
  \n	Compute the distance between each cell with active onion-skin and
  current
                  cell and recall \b addCell(). If onion-skin is not active
  recall \b addCell()
                  with argument current cell.
  */
  void addCellWithOnionSkin(PlayerSet &players, ToonzScene *scene, TXsheet *xsh,
                            int row, int col, int level);

  /*!Recall \b addCellWithOnionSkin() for each cell of row \b row.*/
  void addFrame(PlayerSet &players, ToonzScene *scene, TXsheet *xsh, int row,
                int level, bool includeUnvisible, bool checkPreviewVisibility);

  /*! Add in \b players vector informations about \b level cell with \b TFrameId
  \b fid.
  \n	Compute information for all cell with active onion-skin, if onion-skin
  is not active
                  compute information only for current cell.
  */
  void addSimpleLevelFrame(PlayerSet &players, TXshSimpleLevel *level,
                           const TFrameId &fid);

  /*! Recall \b visitor.onImage(player) for each \b player contained in \b
   * players vector.
  */
  void visit(PlayerSet &players, Visitor &visitor, bool isPlaying = false);

// debug!
#ifdef _DEBUG
  void dumpPlayerSet(PlayerSet &players, std::ostream &out);
  void dumpAll(std::ostream &out);
#endif
};

//-----------------------------------------------------------------------------

StageBuilder::StageBuilder()
    : m_onionSkinDistance(c_noOnionSkin)
    , m_camera3d(false)
    , m_currentColumnIndex(-1)
    , m_ancestorColumnIndex(-1)
    , m_fade(0)
    , m_shiftTraceGhostId(NO_GHOST)
    , m_currentXsheetLevel(0)
    , m_xsheetLevel(0) {
  m_placementStack.push_back(ZPlacement());
}

//-----------------------------------------------------------------------------

StageBuilder::~StageBuilder() { clearPointerContainer(m_maskPool); }

//-----------------------------------------------------------------------------

#ifdef _DEBUG
void StageBuilder::dumpPlayerSet(PlayerSet &players, std::ostream &out) {
  out << "[";
  int m = players.size();
  for (int i = 0; i < m; i++) {
    if (i > 0) out << " ";
    const Player &player = players[i];
    out << "img";
    if (!player.m_masks.empty()) {
      out << "(" << player.m_masks[0];
      for (unsigned int j = 1; j < player.m_masks.size(); j++)
        out << "," << player.m_masks[j];
      out << ")";
    }
  }
  out << "]";
}

//-----------------------------------------------------------------------------

void StageBuilder::dumpAll(std::ostream &out) {
  dumpPlayerSet(m_players, out);
  for (unsigned int i = 0; i < m_maskPool.size(); i++) {
    if (i > 0) out << " ";
    out << " mask:";
    dumpPlayerSet(*m_maskPool[i], out);
  }
}
#endif

//-----------------------------------------------------------------------------

void StageBuilder::addCell(PlayerSet &players, ToonzScene *scene, TXsheet *xsh,
                           int row, int col, int level) {
  // Local functions
  struct locals {
    static inline bool isMeshDeformed(TXsheet *xsh, TStageObject *obj,
                                      const ImagePainter::VisualSettings *vs) {
      const TStageObjectId &parentId = obj->getParent();
      if (parentId.isColumn() && obj->getParentHandle()[0] != 'H') {
        TStageObject *parentObj = xsh->getStageObject(parentId);
        if (!parentObj->getPlasticSkeletonDeformation()) return false;

        TXshColumn *parentCol = xsh->getColumn(parentId.getIndex());
        return (parentCol->getColumnType() == TXshColumn::eMeshType) &&
               (parentCol != vs->m_plasticVisualSettings.m_showOriginalColumn);
      }

      return false;
    }

    //-----------------------------------------------------------------------------

    static inline bool applyPlasticDeform(
        TXshColumn *col, const ImagePainter::VisualSettings *vs) {
      const PlasticVisualSettings &pvs = vs->m_plasticVisualSettings;
      return pvs.m_applyPlasticDeformation && (col != pvs.m_showOriginalColumn);
    }
  };  // locals

  TXshColumnP column = xsh->getColumn(col);
  assert(column);

  TStageObject *pegbar = xsh->getStageObject(TStageObjectId::ColumnId(col));

  // Build affine placements

  TAffine columnAff     = pegbar->getPlacement(row);
  double columnZ        = pegbar->getZ(row);
  double columnNoScaleZ = pegbar->getGlobalNoScaleZ();

  TXshCell cell = xsh->getCell(row, col);
  TXshLevel *xl = cell.m_level.getPointer();
  if (!xl) return;

  ZPlacement cameraPlacement;
  if (m_subXSheetStack.empty())
    cameraPlacement = m_cameraPlacement;
  else
    cameraPlacement = m_subXSheetStack.back().m_camera;
  TAffine columnZaff;
  bool columnBehindCamera = TStageObject::perspective(
      columnZaff, cameraPlacement.m_aff, cameraPlacement.m_z, columnAff,
      columnZ, columnNoScaleZ);

  if (!columnBehindCamera) return;

  TXshSimpleLevel *sl = xl->getSimpleLevel();

  bool storePlayer =
      sl || (xl->getChildLevel() &&
             locals::applyPlasticDeform(column.getPointer(), m_vs) &&
             locals::isMeshDeformed(xsh, pegbar, m_vs));

  if (storePlayer) {
    // Build and store a player

    Player player;
    player.m_sl                     = sl;
    player.m_fid                    = cell.m_frameId;
    player.m_xsh                    = xsh;
    player.m_column                 = col;
    player.m_frame                  = row;
    player.m_currentFrameId         = m_currentFrameId;
    player.m_isGuidedDrawingEnabled = m_isGuidedDrawingEnabled;
    player.m_dpiAff = sl ? getDpiAffine(sl, cell.m_frameId) : TAffine();
    player.m_onionSkinDistance   = m_onionSkinDistance;
    player.m_isCurrentColumn     = (m_currentColumnIndex == col);
    player.m_ancestorColumnIndex = m_ancestorColumnIndex;
    player.m_masks               = m_masks;
    player.m_opacity             = column->getOpacity();
    player.m_filterColor         = column->getFilterColor();

    if (m_subXSheetStack.empty()) {
      player.m_z         = columnZ;
      player.m_placement = m_camera3d ? columnAff : columnZaff;
    } else {
      const SubXSheet &parent = m_subXSheetStack.back();

      player.m_z = parent.m_z;
      player.m_placement =
          (m_camera3d ? parent.m_aff : parent.m_zaff) * columnZaff;
    }

    if (m_shiftTraceGhostId != NO_GHOST) {
      // if F1, F2 or F3 key is pressed, then draw only the corresponding ghost
      int flipKey = m_onionSkinMask.getGhostFlipKey();
      if (Qt::Key_F1 <= flipKey && flipKey <= Qt::Key_F3) {
        if (m_shiftTraceGhostId == TRACED && flipKey == Qt::Key_F2) {
          players.push_back(player);
        } else if (m_shiftTraceGhostId == FIRST_GHOST &&
                   flipKey == Qt::Key_F1) {
          player.m_placement =
              m_onionSkinMask.getShiftTraceGhostAff(0) * player.m_placement;
          players.push_back(player);
        } else if (m_shiftTraceGhostId == SECOND_GHOST &&
                   flipKey == Qt::Key_F3) {
          player.m_placement =
              m_onionSkinMask.getShiftTraceGhostAff(1) * player.m_placement;
          players.push_back(player);
        }
        return;
      }

      else {
        int opacity         = player.m_opacity;
        player.m_bingoOrder = 10;
        if (m_onionSkinMask.getShiftTraceStatus() !=
            OnionSkinMask::ENABLED_WITHOUT_GHOST_MOVEMENTS) {
          if (m_shiftTraceGhostId == FIRST_GHOST) {
            player.m_opacity = 30;
            players.push_back(player);
            player.m_opacity           = opacity;
            player.m_onionSkinDistance = -1;
            player.m_placement =
                m_onionSkinMask.getShiftTraceGhostAff(0) * player.m_placement;
          } else if (m_shiftTraceGhostId == SECOND_GHOST) {
            player.m_opacity = 30;
            players.push_back(player);
            player.m_opacity           = opacity;
            player.m_onionSkinDistance = 1;
            player.m_placement =
                m_onionSkinMask.getShiftTraceGhostAff(1) * player.m_placement;
          }
        }
      }
    }

    players.push_back(player);
  } else if (TXshChildLevel *cl = xl->getChildLevel()) {
    int childRow         = cell.m_frameId.getNumber() - 1;
    TXsheet *childXsheet = cl->getXsheet();
    TStageObjectId childCameraId =
        childXsheet->getStageObjectTree()->getCurrentCameraId();
    TStageObject *childCamera = childXsheet->getStageObject(childCameraId);
    TAffine childCameraAff    = childCamera->getPlacement(childRow);
    double childCameraZ       = childCamera->getZ(childRow);

    std::vector<UCHAR> originalOpacity(childXsheet->getColumnCount());

    for (int c = 0; c < childXsheet->getColumnCount(); c++) {
      originalOpacity[c] = childXsheet->getColumn(c)->getOpacity();
      childXsheet->getColumn(c)->setOpacity(column->getOpacity());
    }

    SubXSheet subXSheet;
    subXSheet.m_camera = ZPlacement(childCameraAff, childCameraZ);
    subXSheet.m_z      = columnZ;

    TAffine childCameraZaff =
        childCameraAff *
        TScale((1000 + childCameraZ) / 1000);  // TODO: put in some lib
    TAffine invChildCameraZaff = childCameraZaff.inv();

    subXSheet.m_aff  = columnAff * invChildCameraZaff;
    subXSheet.m_zaff = columnZaff * invChildCameraZaff;

    if (!m_subXSheetStack.empty()) {
      const SubXSheet &parentXSheet = m_subXSheetStack.back();

      subXSheet.m_z =
          m_subXSheetStack.front().m_z;  // Top-level xsheet cell's z
      subXSheet.m_aff  = parentXSheet.m_aff * subXSheet.m_aff;
      subXSheet.m_zaff = parentXSheet.m_zaff * subXSheet.m_zaff;
    }

    m_subXSheetStack.push_back(subXSheet);

    ++m_currentXsheetLevel;
    addFrame(players, scene, childXsheet, childRow, level + 1, false, false);
    --m_currentXsheetLevel;

    m_subXSheetStack.pop_back();

    for (int c = 0; c < childXsheet->getColumnCount(); c++)
      childXsheet->getColumn(c)->setOpacity(originalOpacity[c]);
  }
}

//-----------------------------------------------------------------------------

static bool alreadyAdded(TXsheet *xsh, int row, int index,
                         const std::vector<int> &rows, int col) {
  int i;
  for (i = 0; i < index; i++)
    if (xsh->getCell(rows[i], col) == xsh->getCell(rows[index], col))
      return true;
  if (xsh->getCell(rows[index], col) == xsh->getCell(row, col)) return true;
  return false;
}

//-----------------------------------------------------------------------------

void StageBuilder::addCellWithOnionSkin(PlayerSet &players, ToonzScene *scene,
                                        TXsheet *xsh, int row, int col,
                                        int level) {
  struct locals {
    static inline bool hasOnionSkinnedMeshParent(StageBuilder *sb, TXsheet *xsh,
                                                 int col) {
      const TStageObjectId &parentId =
          xsh->getStageObject(TStageObjectId::ColumnId(col))->getParent();

      return parentId.isColumn() &&
             (parentId.getIndex() == sb->m_currentColumnIndex);
    }

    //-----------------------------------------------------------------------------

    static inline bool doStandardOnionSkin(StageBuilder *sb, TXsheet *xsh,
                                           int level, int col) {
      const OnionSkinMask &osm = sb->m_onionSkinMask;

      return level == sb->m_xsheetLevel && osm.isEnabled() && !osm.isEmpty() &&
             (osm.isWholeScene() || col == sb->m_currentColumnIndex ||
              locals::hasOnionSkinnedMeshParent(sb, xsh, col));
    }
  };  // locals

  if (m_onionSkinMask.isShiftTraceEnabled()) {
    if (col == m_currentColumnIndex) {
      TXshCell cell = xsh->getCell(row, col);

      // First Ghost
      int r;
      r = row + m_onionSkinMask.getShiftTraceGhostFrameOffset(0);
      if (r >= 0 && xsh->getCell(r, col) != cell &&
          (cell.getSimpleLevel() == 0 ||
           xsh->getCell(r, col).getSimpleLevel() == cell.getSimpleLevel())) {
        m_shiftTraceGhostId = FIRST_GHOST;
        addCell(players, scene, xsh, r, col, level);
      }

      r = row + m_onionSkinMask.getShiftTraceGhostFrameOffset(1);
      if (r >= 0 && xsh->getCell(r, col) != cell &&
          (cell.getSimpleLevel() == 0 ||
           xsh->getCell(r, col).getSimpleLevel() == cell.getSimpleLevel())) {
        m_shiftTraceGhostId = SECOND_GHOST;
        addCell(players, scene, xsh, r, col, level);
      }

      // draw current working frame
      if (!cell.isEmpty()) {
        m_shiftTraceGhostId = TRACED;
        addCell(players, scene, xsh, row, col, level);
        m_shiftTraceGhostId = NO_GHOST;
      }
    }
    // flip non-current columns as well
    else {
      int flipKey = m_onionSkinMask.getGhostFlipKey();
      if (flipKey == Qt::Key_F1) {
        int r = row + m_onionSkinMask.getShiftTraceGhostFrameOffset(0);
        addCell(players, scene, xsh, r, col, level);
      } else if (flipKey == Qt::Key_F3) {
        int r = row + m_onionSkinMask.getShiftTraceGhostFrameOffset(1);
        addCell(players, scene, xsh, r, col, level);
      } else
        addCell(players, scene, xsh, row, col, level);
    }
  } else if (locals::doStandardOnionSkin(this, xsh, level, col)) {
    std::vector<int> rows;
    m_onionSkinMask.getAll(row, rows);
    std::vector<int>::iterator it = rows.begin();
    while (it != rows.end() && *it < row) it++;
    std::sort(rows.begin(), it, descending);

    int frontPos = 0, backPos = 0;
    for (int i = 0; i < (int)rows.size(); i++) {
      if (rows[i] == row) continue;

#ifdef NUOVO_ONION
      m_onionSkinDistance = rows[i] - row;
#else
      if (!Preferences::instance()->isAnimationSheetEnabled() ||
          !alreadyAdded(xsh, row, i, rows, col)) {
        m_onionSkinDistance = (rows[i] - row) < 0 ? --backPos : ++frontPos;
        addCell(players, scene, xsh, rows[i], col, level);
      }
#endif
    }

    m_onionSkinDistance = 0;
    addCell(players, scene, xsh, row, col, level);

    m_onionSkinDistance = c_noOnionSkin;
  } else
    addCell(players, scene, xsh, row, col, level);
}

//-----------------------------------------------------------------------------

void StageBuilder::addFrame(PlayerSet &players, ToonzScene *scene, TXsheet *xsh,
                            int row, int level, bool includeUnvisible,
                            bool checkPreviewVisibility) {
  int columnCount        = xsh->getColumnCount();
  unsigned int maskCount = m_masks.size();

  std::vector<std::pair<double, int>> shuffle;
  for (int c = 0; c < columnCount; c++) {
    TXshColumnP column = xsh->getColumn(c);
    assert(column);
    TStageObject *pegbar = xsh->getStageObject(TStageObjectId::ColumnId(c));
    double columnSO      = pegbar->getSO(row);
    shuffle.push_back(std::make_pair(columnSO, c));
  }
  std::stable_sort(shuffle.begin(), shuffle.end(), StackingOrder());

  for (int i = 0; i < columnCount; i++) {
    int c = shuffle[i].second;
    if (CameraTestCheck::instance()->isEnabled() && c != m_currentColumnIndex)
      continue;
    if (level == 0) {
      // m_isCurrentColumn = (c == m_currentColumnIndex);
      m_ancestorColumnIndex = c;
    }
    TXshColumn *column = xsh->getColumn(c);
    bool isMask        = false;
    if (column && !column->isEmpty()) {
      if (!column->isPreviewVisible() && checkPreviewVisibility) continue;
      if (column->isCamstandVisible() ||
          includeUnvisible)  // se l'"occhietto" non e' chiuso
      {
        if (column->isMask())  // se e' una maschera (usate solo in tab pro)
        {
          isMask = true;
          std::vector<int> saveMasks;
          saveMasks.swap(m_masks);
          int maskIndex   = m_maskPool.size();
          PlayerSet *mask = new PlayerSet();
          m_maskPool.push_back(mask);
          addCellWithOnionSkin(*mask, scene, xsh, row, c, level);
          std::stable_sort(mask->begin(), mask->end(), PlayerLt());
          saveMasks.swap(m_masks);
          m_masks.push_back(maskIndex);
        } else
          addCellWithOnionSkin(players, scene, xsh, row, c, level);
      }
    }
    if (!isMask) {
      while (m_masks.size() > maskCount) m_masks.pop_back();
    }
  }
  if (level == 0) std::stable_sort(players.begin(), players.end(), PlayerLt());
}

//-----------------------------------------------------------------------------

void StageBuilder::addSimpleLevelFrame(PlayerSet &players,
                                       TXshSimpleLevel *level,
                                       const TFrameId &fid) {
  auto addGhost = [&](int ghostIndex, int ghostRow, bool fullOpac = false) {
    const TFrameId &ghostFid = level->index2fid(ghostRow);

    Player player;
    player.m_sl                     = level;
    player.m_frame                  = level->guessIndex(ghostFid);
    player.m_fid                    = ghostFid;
    player.m_isCurrentColumn        = true;
    player.m_isCurrentXsheetLevel   = true;
    player.m_isEditingLevel         = true;
    player.m_currentFrameId         = m_currentFrameId;
    player.m_isGuidedDrawingEnabled = m_isGuidedDrawingEnabled;
    player.m_isVisibleinOSM         = ghostRow >= 0;
    player.m_onionSkinDistance      = m_onionSkinDistance;
    player.m_dpiAff                 = getDpiAffine(level, ghostFid);
    player.m_ancestorColumnIndex    = -1;

    if (fullOpac) {
      player.m_placement = m_onionSkinMask.getShiftTraceGhostAff(ghostIndex) *
                           player.m_placement;
      players.push_back(player);
      return;
    }

    int opacity         = player.m_opacity;
    player.m_bingoOrder = 10;
    if (m_onionSkinMask.getShiftTraceStatus() !=
        OnionSkinMask::ENABLED_WITHOUT_GHOST_MOVEMENTS) {
      player.m_opacity = 30;
      players.push_back(player);
      player.m_opacity           = opacity;
      player.m_onionSkinDistance = (ghostIndex == 0) ? -1 : 1;
      player.m_placement = m_onionSkinMask.getShiftTraceGhostAff(ghostIndex) *
                           player.m_placement;
    }
    players.push_back(player);
  };

  int index = -1;

  int row = level->guessIndex(fid);

  // Shift & Trace
  if (m_onionSkinMask.isShiftTraceEnabled()) {
    int previousOffset = m_onionSkinMask.getShiftTraceGhostFrameOffset(0);
    int forwardOffset  = m_onionSkinMask.getShiftTraceGhostFrameOffset(1);

    // If F1, F2 or F3 key is pressed, then only
    // display the corresponding ghost
    int flipKey = m_onionSkinMask.getGhostFlipKey();
    if (Qt::Key_F1 <= flipKey && flipKey <= Qt::Key_F3) {
      if (flipKey == Qt::Key_F1 && previousOffset != 0) {
        addGhost(0, row + previousOffset, true);
        return;
      } else if (flipKey == Qt::Key_F3 && forwardOffset != 0) {
        addGhost(1, row + forwardOffset, true);
        return;
      }
    }

    else {
      // draw the first ghost
      if (previousOffset != 0) addGhost(0, row + previousOffset);
      if (forwardOffset != 0) addGhost(1, row + forwardOffset);
    }
  }
  // Onion Skin
  else if (!m_onionSkinMask.isEmpty() && m_onionSkinMask.isEnabled()) {
    std::vector<int> rows;
    m_onionSkinMask.getAll(row, rows);

    std::vector<int>::iterator it = rows.begin();
    while (it != rows.end() && *it < row) ++it;
    std::sort(rows.begin(), it, descending);
    m_onionSkinDistance = 0;
    int frontPos = 0, backPos = 0;

    for (int i = 0; i < (int)rows.size(); i++) {
      const TFrameId &fid2 = level->index2fid(rows[i]);
      if (fid2 == fid) continue;
      players.push_back(Player());
      Player &player                  = players.back();
      player.m_sl                     = level;
      player.m_frame                  = level->guessIndex(fid);
      player.m_fid                    = fid2;
      player.m_isCurrentColumn        = true;
      player.m_isCurrentXsheetLevel   = true;
      player.m_isEditingLevel         = true;
      player.m_currentFrameId         = m_currentFrameId;
      player.m_isGuidedDrawingEnabled = m_isGuidedDrawingEnabled;
      player.m_isVisibleinOSM         = rows[i] >= 0;
#ifdef NUOVO_ONION
      player.m_onionSkinDistance = rows[i] - row;
#else
      player.m_onionSkinDistance = rows[i] - row < 0 ? --backPos : ++frontPos;
#endif
      player.m_dpiAff = getDpiAffine(level, fid2);
    }
  }
  players.push_back(Player());
  Player &player = players.back();
  player.m_sl    = level;
  player.m_frame = level->guessIndex(fid);
  player.m_fid   = fid;
  if (!m_onionSkinMask.isEmpty() && m_onionSkinMask.isEnabled())
    player.m_onionSkinDistance  = 0;
  player.m_isCurrentColumn      = true;
  player.m_isCurrentXsheetLevel = true;
  player.m_isEditingLevel       = true;
  player.m_ancestorColumnIndex  = -1;
  player.m_dpiAff               = getDpiAffine(level, fid);
}

//-----------------------------------------------------------------------------

void StageBuilder::visit(PlayerSet &players, Visitor &visitor, bool isPlaying) {
  std::vector<int> masks;
  int m = players.size();
  int h = 0;
  for (; h < m; h++) {
    Player &player = players[h];
    unsigned int i = 0;
    // vale solo per TAB pro
    for (; i < masks.size() && i < player.m_masks.size(); i++)
      if (masks[i] != player.m_masks[i]) break;
    // vale solo per TAB pro
    if (i < masks.size() || i < player.m_masks.size()) {
      while (i < masks.size()) {
        masks.pop_back();
        visitor.disableMask();
      }
      while (i < player.m_masks.size()) {
        int maskIndex = player.m_masks[i];
        visitor.beginMask();
        visit(*m_maskPool[maskIndex], visitor, isPlaying);
        visitor.endMask();
        masks.push_back(maskIndex);
        visitor.enableMask();
        i++;
      }
    }
    player.m_isPlaying = isPlaying;
    visitor.onImage(player);
  }
  // vale solo per TAB pro
  for (h = 0; h < (int)masks.size(); h++) visitor.disableMask();
}

//=============================================================================

//=============================================================================
/*! Declare a \b StageBuilder object and recall \b StageBuilder::addFrame() and
                \b StageBuilder::visit().
*/
//=============================================================================

void Stage::visit(Visitor &visitor, const VisitArgs &args) {
  ToonzScene *scene        = args.m_scene;
  TXsheet *xsh             = args.m_xsh;
  int row                  = args.m_row;
  int col                  = args.m_col;
  const OnionSkinMask *osm = args.m_osm;
  bool camera3d            = args.m_camera3d;
  int xsheetLevel          = args.m_xsheetLevel;
  bool isPlaying           = args.m_isPlaying;

  StageBuilder sb;
  sb.m_vs                     = &visitor.m_vs;
  TStageObjectId cameraId     = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera        = xsh->getStageObject(cameraId);
  TAffine cameraAff           = camera->getPlacement(row);
  double z                    = camera->getZ(row);
  sb.m_cameraPlacement        = ZPlacement(cameraAff, z);
  sb.m_camera3d               = camera3d;
  sb.m_currentColumnIndex     = col;
  sb.m_xsheetLevel            = xsheetLevel;
  sb.m_onionSkinMask          = *osm;
  sb.m_currentFrameId         = args.m_currentFrameId;
  sb.m_isGuidedDrawingEnabled = args.m_isGuidedDrawingEnabled;
  Player::m_onionSkinFrontSize     = 0;
  Player::m_onionSkinBackSize      = 0;
  Player::m_firstBackOnionSkin     = 0;
  Player::m_lastBackVisibleSkin    = 0;
  Player::m_isShiftAndTraceEnabled = osm->isShiftTraceEnabled();
  sb.addFrame(sb.m_players, scene, xsh, row, 0, args.m_onlyVisible,
              args.m_checkPreviewVisibility);

  updateOnionSkinSize(sb.m_players);

  sb.visit(sb.m_players, visitor, isPlaying);
}

//-----------------------------------------------------------------------------

void Stage::visit(Visitor &visitor, ToonzScene *scene, TXsheet *xsh, int row) {
  Stage::VisitArgs args;
  args.m_scene = scene;
  args.m_xsh   = xsh;
  args.m_row   = row;
  args.m_col   = -1;
  OnionSkinMask osm;
  args.m_osm = &osm;

  visit(visitor, args);  // scene, xsh, row, -1, OnionSkinMask(), false, 0);
}

//-----------------------------------------------------------------------------
/*! Declare a \b StageBuilder object and recall \b
   StageBuilder::addSimpleLevelFrame()
                and \b StageBuilder::visit().
*/
void Stage::visit(Visitor &visitor, TXshSimpleLevel *level, const TFrameId &fid,
                  const OnionSkinMask &osm, bool isPlaying,
                  int isGuidedDrawingEnabled) {
  StageBuilder sb;
  sb.m_vs                          = &visitor.m_vs;
  sb.m_onionSkinMask               = osm;
  sb.m_currentFrameId              = fid;
  sb.m_isGuidedDrawingEnabled      = isGuidedDrawingEnabled;
  Player::m_onionSkinFrontSize     = 0;
  Player::m_onionSkinBackSize      = 0;
  Player::m_firstBackOnionSkin     = 0;
  Player::m_lastBackVisibleSkin    = 0;
  Player::m_isShiftAndTraceEnabled = osm.isShiftTraceEnabled();
  sb.addSimpleLevelFrame(sb.m_players, level, fid);
  updateOnionSkinSize(sb.m_players);
  sb.visit(sb.m_players, visitor, isPlaying);
}

//-----------------------------------------------------------------------------

void Stage::visit(Visitor &visitor, TXshLevel *level, const TFrameId &fid,
                  const OnionSkinMask &osm, bool isPlaying,
                  double isGuidedDrawingEnabled) {
  if (level && level->getSimpleLevel())
    visit(visitor, level->getSimpleLevel(), fid, osm, isPlaying,
          (int)isGuidedDrawingEnabled);
}
