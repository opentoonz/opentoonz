#pragma once

#ifndef XSHEET_INCLUDED
#define XSHEET_INCLUDED

#include <memory>

// TnzCore includes
#include "traster.h"
#include "tpersist.h"
#include "tsound.h"

// TnzLib includes
#include "toonz/txshcolumn.h"

#include "cellposition.h"

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//    Forward declarations

class TXshLevel;
class TXshCell;
class TAffine;
class TStageObjectId;
class TStageObject;
class TStageObjectTree;
class TStroke;
class TVectorImageP;
class FxDag;
class TControlPointObserver;
class TOfflineGL;
class ColumnFan;
class ToonzScene;
class TXshSoundColumn;
class TXshNoteSet;
class TFrameId;
class Orientation;

//=============================================================================

//****************************************************************************************
//    TXsheet  declaration
//****************************************************************************************

//! This is the base class for an xsheet. An Xsheet is composed of colums of
//! frames.
/*!
Inherits \b TSmartObject and \b TPersist.

        The class provides a collection of functions that returns xsheet
elements, defined in
        struct \b TXsheetImp, and enables manipulation of these. Most important
xsheet elements
        are: a \b column \b set \b TColumnSetT, a \b pegbar \b tree \b
TStageObjectTree, a \b fx
        \b dag \b FxDag, a \b sound \b track \b TSoundTrack and a \b scene \b
ToonzScene.

                For purposes of this class, a Column is a graphics layer, and a
row is a frame number.
                (Whether horizontal or vertial is a matter of displaying).

                A \b column \b set contains all xsheet columns. A collection of
functions, concerning column
        set, allows to manage xsheet column. It's possible to know xsheet column
count,
        getColumnCount(), know first non empty column index,
getFirstFreeColumnIndex(),
        or know if column in datum index is empty, isColumnEmpty(). You can work
on single xsheet
        column, getColumn(), using its indexes: insert and remove a column with
insertColumn()
        and removeColumn() or move column from an index to another using
moveColumn().

        You can manage also column visualization in xsheet, using the xsheet
object \b ColumnFan
        getColumnFan(), and find column icon getColumnIcon().

                cell positions will be identified by a pair of row+column index,
which is a separate class.

        It's possible work on xsheet cells directly, getCell() and setCell() or
getCells() and
        setCells(); cells are identified in xsheet by two index one for row and
one for column.
        You can insert, remove or clear cells using insertCells(), removeCells()
or clearCells();
        the difference between the remove and the clear function is the shift of
remains cells.
        Also there are functions to manipulate cells reverseCells(),
swingCells(),
        incrementCells(), duplicateCells(), int upTo, stepCells(), eachCells().

        About \b pegbar \b tree \b TStageObjectTree, it's possible to manage it
through the stage object's
        related functions.

        The \b fx \b dag \b FxDag getFxDag() is not managed with direct
functions but is always
        up to date; in fact same functions update it. For example
insertColumn(), if necessary,
        insert column index in fx dag, the same happen in setCells().

        The \b sound \b track \b TSoundTrack contain a mixed sound, computed
using makeSound(),
        of all \b TXshSoundColumn present in xsheet.

        It's possible take and know the \b scene \b ToonzScene to which the
xsheet refers using
        getScene() and setScene().
*/

class DVAPI TXsheet final : public TSmartObject, public TPersist {
  PERSIST_DECLARATION(TXsheet)

public:
  class DVAPI SoundProperties {
  public:
    int m_fromFrame;
    int m_toFrame;
    int m_frameRate;
    bool m_isPreview;

    SoundProperties();
    ~SoundProperties();

    inline bool operator==(const SoundProperties &c) const;
    inline bool operator!=(const SoundProperties &c) const;
  };

private:
  TSoundOutputDevice *m_player;

  /*!	\struct TXsheet::TXsheetImp
  The TXsheetImp struct provides all objects necessary to define the \b TXsheet
  class.
*/
  struct TXsheetImp;
  std::unique_ptr<TXsheetImp> m_imp;
  TXshNoteSet *m_notes;
  SoundProperties *m_soundProperties;

  DECLARE_CLASS_CODE

public:
  TXsheet();
  ~TXsheet();

  //! Returns a unique identifier associated with the xsheet instance.
  unsigned long id() const;

  /*! Returns max frame number used in xsheet.
          \sa getMaxFrame()
  */
  int getFrameCount() const;

  /*! Returns true if all cells in rect delimited by first frame \b \e
     pos0.frame,
     last frame \b \e pos1.frame and first layer \b \e pos0.layer, last layer \b
     \e pos1.layer
         are empty; otherwise, false.
  */
  bool isRectEmpty(const CellPosition &pos0, const CellPosition &pos1) const;

  /*! Returns the \b TXshCell cell in row identified by index \b \e row and
     column identified by index \b \e col. If column \b TXshColumnP in \b \e col
     is empty return
     an empty cell.
          \sa setCell(), getCells(), setCells()
  */
  const TXshCell &getCell(int row, int col) const;

  const TXshCell &getCell(const CellPosition &pos) const;

  bool setCell(int row, int col, const TXshCell &cell);
  /*! Set \b \e cells[] to \b \e rowCount cells of column identified by index \b
     \e col starting from row identified by index \b \e row. If column is empty
     or is not a \b
     TXshCellColumn set \b \e cells[] to \b \e rowCount empty cells.
          \sa getCells(), setCells(), getCell()
*/
  void getCells(int row, int col, int rowCount, TXshCell cells[]) const;
  /*! If column identified by index \b \e col is a \b TXshCellColumn or is empty
    and is not
    locked, this method sets to \b \e cells[] the given \b \e rowCount cells of
    column \b \e col starting from
    row \b \e row. If column in \b \e col is empty it creates a new column
    recalling
    \b TColumnSetT::touchColumn() and sets the new cells to \b \e cells[], and
    on creating a new
    column it adds it to fx dag \b FxDag.
    If cells in \b \e row and \b \e col are not empty recalls \b
    TXshCellColumn::setCells(),
    insert the new cells \b \e cells[] in \b \e row \b \e col and shift old
    cells.
    If xsheet change it updates xsheet's frame count. Return false if cannot set
    cells.
    \sa getCells(), setCell(), getCell()
  */
  bool setCells(int row, int col, int rowCount, const TXshCell cells[]);
  /*! If column identified by index \b \e col is not empty, is a \b \e
     TXshCellColumn and is not locked this method inserts in row identified by
     index \b \e row \b \e
     rowCount empty cells, it calls TXshCellColumn::insertEmptyCells(). An
     update of xsheet's frame count
     is performed.
          \sa setCells(), removeCells()
  */
  void insertCells(int row, int col, int rowCount = 1);

  /*! If column identified by index \b \e col is not empty, is a \b
     TXshCellColumn and is not locked, this method removes \b \e rowCount cells
     starting from \b \e
     row, it calls TXshCellColumn::removeCells(). It removes cells without shift
     remaining cells. An update of xsheet's frame count is performed.
          \sa  clearCells(), insertCells()
  */
  void removeCells(int row, int col, int rowCount = 1);

  /*! If column identified by index \b \e col is not empty, is a \b TXshCellColumn and is not
    locked, clear \b \e rowCount cells starting from \b \e row and it recalls TXshCellColumn::clearCells().
    Clears cells and it shifts remaining cells. Xsheet's frame count is updated.
    \sa removeCells(), insertCells()
*/ void
  clearCells(int row, int col, int rowCount = 1);
  /*! Clears xsheet. It sets to default values all xsheet elements contained in
   * struct \b TXsheetImp.
  */
  void clearAll();
  /*! Returns cell range of column identified by index \b \e col and set \b \e
     r0 and \b \e r1 respectively to first and last not empty cell, it then
     recalls \b
     TXshCellColumn::getRange(). If column is empty or is not a \b
     TXshCellColumn this method returns
     zero and sets
          \b \e r0 to 0 and \b \e r1 to -1.
  */
  int getCellRange(int col, int &r0, int &r1) const;
  /*! Returns the max frame number of xsheet column identified by \b \e col and
     calls \b TXshColumn::getMaxFrame().
          \sa getFrameCount()
  */
  int getMaxFrame(int col) const;
  /*! Returns true if xsheet column identified by \b \e col is empty, it calls
          \b TXshColumn::isEmpty(), otherwise returns false.
  */
  bool isColumnEmpty(int col) const;
  /*! Sets the level set \b \e levels with all current xsheet levels used, it
     includes
          in \b \e levels sub-xsheet levels too.
          \sa isLevelUsed()
  */
  void getUsedLevels(std::set<TXshLevel *> &levels) const;
  /*! Returns true if \b \e level is used in current xsheet or in sub-xsheet,
     otherwise returns
          false. It verifies if \b \e level is contained in level set \b
     getUsedLevels().
  */
  bool isLevelUsed(TXshLevel *) const;
  /*! The method retrieves a pegbar \b TStageObject with the passed \b \e id \b
     TStageObjectId
          specialization, calling TStageObjectTree::getStageObject().
          \sa getStageObjectTree()
  */
  TStageObject *getStageObject(const TStageObjectId &id) const;
  /*! The method retrieves the pegbar tree \b TStageObjectTree contained in
     struct \b TXsheetImp.
          \sa getStageObject()
  */
  TStageObjectTree *getStageObjectTree() const;
  /*! Returns transformation matrix \b TAffine related to pegbar \b TStageObject
     with \b \e id
          \b TStageObjectId specialization in frame \b \e frame, it calls \b
     TStageObject::getPlacement().
          \sa getParentPlacement()
  */
  TAffine getPlacement(const TStageObjectId &id, int frame) const;
  /*! Returns z value related to pegbar \b TStageObject with \b \e id
     specialization \b TStageObjectId
          in frame \b \e frame, it calls \b TStageObject::getZ().
  */
  double getZ(const TStageObjectId &id, int frame) const;

  double getNoScaleZ(const TStageObjectId &id) const;
  /*! Returns transformation matrix \b TAffine related to parent of pegbar \b
     TStageObject with
          \b \e id \b TStageObjectId specialization in frame \b \e frame, it
     calls \b TStageObject::getParentPlacement().
          \sa getPlacement()
  */
  TAffine getParentPlacement(const TStageObjectId &id, int frame) const;
  /*! Returns the center related to pegbar \b TStageObject with \b \e id
     TStageObjectId specialization
          in frame \b \e frame; it calls \b TStageObject::getCenter().
          \sa setCenter()
  */
  TPointD getCenter(const TStageObjectId &id, int frame) const;
  /*! Sets the center of pegbar \b TStageObject with \b \e id TStageObjectId
     specialization in frame
          \b \e frame, to \b \e center; it calls \b TStageObject::setCenter().
          \sa getCenter()
  */
  void setCenter(const TStageObjectId &id, int frame, const TPointD &center);
  /*! Returns parent related to pegbar \b TStageObject with \b \e id
     TStageObjectId specialization;
          it calls \b TStageObject::getParent().
          \sa setStageObjectParent()
  */
  TStageObjectId getStageObjectParent(const TStageObjectId &id);
  /*! Sets parent of pegbar \b TStageObject with \b \e id TStageObjectId
     specialization to \b \e parentId;
          it calls \b TStageObject::setParent().
          \sa getStageObjectParent()
  */
  void setStageObjectParent(const TStageObjectId &id,
                            const TStageObjectId &parentId);
  /*! Returns true if pegbar \b TStageObject with \b \e id TStageObjectId
     specialization has children,
          that is if pegbar is a parent of another pegbar; it calls \b
     TStageObject::hasChildren().
          \sa getStageObjectParent() and setStageObjectParent()
  */
  bool hasChildren(const TStageObjectId &id) const;
  /*! Returns current camera's transformation matrix \b TAffine related to frame
     \b \e frame,
          inclusive of zdepth. It Doesn't take care of transformation matrix
     related to pegbar
          linked to camera.
          \note Used only in tab code, "the Tab" has just one camera while
     "Toonz 5.2 Harlequin"
          manages several camera.
  */
  TAffine getCameraAff(int frame) const;

  void reverseCells(int r0, int c0, int r1, int c1);
  /*! The cells, contained in rect delimited by first row \b \e r0, last row \b
     \e r1 and
          first column \b \e c0, are appended at the end of the rect in a
     reversed order.
          The last cell of rect will not be repeated.
  */
  void swingCells(int r0, int c0, int r1, int c1);
  /*! If cells is increasingly numbered and belong from same level return true;
          otherwise return false.
  \n	If returns true, the cells, contained in rect delimited by first row
  \b \e r0, last row
          \b \e r1 and first column \b \e c0, are repeated as if filling the
  numbering gap between
          two subsequence cell. For example if the command is applied to two
  cell with
          \b TFrameId number equal to 2 and 5, the result will be four cells: a
  cell with
          \b TFrameId number equal to 2 repeated three times and one cell with
  \b TFrameId
          number equal to 5.
  \n	This method inserts in \b \e forUndo vector all the inserted cells, it
  is useful for the undo process.
  */
  bool incrementCells(int r0, int c0, int r1, int c1,
                      std::vector<std::pair<TRect, TXshCell>> &forUndo);
  /*! A copy of cells, contained in rect delimited by first row \b \e r0, last
     row \b \e r1
          and first column \b \e c0, is made in row cells from \b r1+1 to \b \e
     upTo. The duplicated
          cells will be inserted by shifting the other down.
  */
  void duplicateCells(int r0, int c0, int r1, int c1, int upTo);
  /*! The cells, contained in rect delimited by first row \b \e r0, last row \b
     \e r1 and
          first column \b \e c0, are repeated in order to have a step \b \e
     type. The duplicated
          cells will be inserted by shifting the other down.
  */
  void stepCells(int r0, int c0, int r1, int c1, int type);
  /*! For each sequenze of frame with same number, contained in rect delimited
     by first row \b \e r0, last row \b \e r1 and
          first column \b \e c0, a frame with same number is inserted.
*/
  void increaseStepCells(int r0, int c0, int &r1, int c1);
  /*!
For each sequenze of frame with same number, contained in rect delimited by
first row \b \e r0, last row \b \e r1 and
          first column \b \e c0, a frame with same number is removed.
*/
  void decreaseStepCells(int r0, int c0, int &r1, int c1);
  /*!
The cells, contained in rect delimited by first row \b \e r0, last row \b \e r1
and
          first column \b \e c0, are resetted in order to have no sequential
frame duplication.
*/
  void resetStepCells(int r0, int c0, int r1, int c1);
  /*! Only one cell each step \b \e type, of the cells, contained in rect
     delimited by
          first row \b \e r0, last row \b \e r1 and first column \b \e c0, are
     preserved,  the
          others are deleted.
  */
  void eachCells(int r0, int c0, int r1, int c1, int type);
  void rollupCells(int r0, int c0, int r1, int c1);
  void rolldownCells(int r0, int c0, int r1, int c1);
  void timeStretch(int r0, int c0, int r1, int c1, int nr);

  // force cells order in n-steps. returns the row amount after process
  int reframeCells(int r0, int r1, int col, int type);

  /*! Exposes level \b \e xl in xsheet starting from cell identified by \b \e
     row and \b \e col.
          Returns the number of the inserted cells.
  */
  int exposeLevel(int row, int col, TXshLevel *xl, bool overwrite = false);

  // cutomized exposseLevel used from LoadLevel command
  int exposeLevel(int row, int col, TXshLevel *xl, std::vector<TFrameId> &fIds_,
                  int xFrom = -1, int xTo = -1, int step = -1, int inc = -1,
                  int frameCount = -1, bool doesFileActuallyExist = true);

  /*! Exposes level \b \e xl \b \e fids in xsheet starting from cell identified
   * by \b \e row and \b \e col.
  */
  void exposeLevel(int row, int col, TXshLevel *xl, std::vector<TFrameId> fids,
                   bool overwrite);
  /*! Updates xsheet frame count, find max frame count between all
          xsheet column, using getMaxFrame().
  */
  void updateFrameCount();
  /*! Clears xsheet calling \b clearAll() and sets all xsheet elements,
     contained
          in \b TXsheetImp, to information contained in \b TIStream \b \e is.
          \sa saveData()
  */
  void loadData(TIStream &is) override;
  /*! Save all xsheet elements information, contained in \b TXsheetImp, in \b
     TOStream \b \e os.
          \sa loadData()
  */
  void saveData(TOStream &os) override;
  /*! Inserts an empty column in \b \e index calling \b insertColumn().
  */
  void insertColumn(int index,
                    TXshColumn::ColumnType type = TXshColumn::eLevelType);
  /*! Insert \b \e column in column \b \e index. Insert column in the column
     set, in the
          pegbarTree \b TStageObjectTree contained in TXsheetImp and if column
     fx \b TFx is
          not empty in fx dag, calling FxDag::addToXsheet().
          \sa removeColumn()
  */
  void insertColumn(int index, TXshColumn *column);  // becomes owner
  /*! Removes column identified by \b \e index from xsheet column. If column in
\b \e index
is not empty and it has a \b TFx fx, removes column from fx dag, calls
FxDag::removeFromXsheet() and disconnects fx column from all output fx \b TFx,
than removes column from column set and from pegbarTree \b TStageObjectTree
contained
in TXsheetImp.
\sa insertColumn()
*/
  void removeColumn(int index);
  /*! Moves column from xsheet column index \b \e srcIndex to \b \e dstIndex,
                  columns between \b \e srcIndex+1 and \b \e dstIndex (included)
     are shifted of -1.
          \sa insertColumn() and removeColumn()
  */
  void moveColumn(int srcIndex, int dstIndex);

  /*! Returns a pointer to the \b TXshColumn identified in xsheet by index \b \e
   * index.
  */
  TXshColumn *getColumn(int index) const;
  /*! Returns xsheet column count, i.e the number of xsheet column used, it
     calls
          \b TColumnSetT::getColumnCount().
          \sa getColumn() and getMaxFrame().
  */
  int getColumnCount() const;
  /*! Returns first not empty column index in xsheet.
  */
  int getFirstFreeColumnIndex() const;

  TSoundTrack *makeSound(SoundProperties *properties);
#ifdef BUTTA
  /*! Returns \b TSoundTrack with frame rate \b \e frameRate computed calling
          \b TXshSoundColumn::mixingTogether() of all sound column contained in
          xsheet. If xsheet doesn't have sound column it returns zero.
  */
  TSoundTrack *makeSound(int frameRate, bool isPreview = true);
  /*! Utility Function */
  TSoundTrack *makeSound(int fromFrame, int toFrame, int frameRate = -1);
#endif
  void scrub(int frame, bool isPreview = false);
  void stopScrub();
  void play(TSoundTrackP soundtrack, int s0, int s1, bool loop);

  /*! Returns a pointer to object \b FxDag contained in \b TXsheetImp, this
     object
          allows the user to manage all fx dag, i.e. all element of fx
     schematic.
  */
  FxDag *getFxDag() const;
  /*! Returns a pointer to object \b ColumnFan contained in \b TXsheetImp, this
          object allows the user to manage columns visualization in xsheet.
          TXsheet maintains one column fan per each orientation.
  */
  ColumnFan *getColumnFan(const Orientation *o) const;
  /*! Returns a pointer to \b ToonzScene contained in \b TXsheetImp, that is the
     scene to
          which the xsheet refers.
          \sa setScene()
  */
  ToonzScene *getScene() const;
  /*! Set scene \b ToonzScene contained in \b TXsheetImp to \b scene, change the
          scene to which the xsheet refers.
          \sa setScene()
  */
  void setScene(ToonzScene *scene);

  TXshNoteSet *getNotes() const { return m_notes; }

  //! Returns true if the \b cellCandidate creates a circular reference.
  //! A circular reference is obtained when \b cellCandidate is a subXsheet cell
  //! and contains or matches
  //! with this XSheet.
  bool checkCircularReferences(const TXshCell &cellCandidate);

  //! Returns true if the \b columnCandidate creates a circular reference.
  //! A circular reference is obtained when \b columnCandidate is a subXsheet
  //! column and contains or matches
  //! with this XSheet.
  bool checkCircularReferences(TXshColumn *columnCandidate);

  void invalidateSound();

  //! Returns the xsheet content's \a camstand bbox at the specified row.
  TRectD getBBox(int row) const;

protected:
  bool checkCircularReferences(TXsheet *childCandidate);

private:
  // Not copiable
  TXsheet(const TXsheet &);
  TXsheet &operator=(const TXsheet &);

  /*! Return column in index if exists, overwise create a new column;
if column exist and is empty check \b isSoundColumn and return right type.
*/
  TXshColumn *touchColumn(int index,
                          TXshColumn::ColumnType = TXshColumn::eLevelType);
};

//-----------------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TXsheet>;
#endif
typedef TSmartPointerT<TXsheet> TXsheetP;

#endif  // XSHEET_INCLUDED
