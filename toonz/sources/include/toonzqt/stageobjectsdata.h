#pragma once

#ifndef STAGEOBJECT_DATA_H
#define STAGEOBJECT_DATA_H

// TnzCore includes
#include "tconst.h"

// TnzLib includes
#include "toonz/tstageobjectid.h"
#include "toonz/txshcolumn.h"
#include "toonz/tcamera.h"

// TnzQt includes
#include "toonzqt/dvmimedata.h"

// STL include
#include <set>

// Qt includes
#include <QList>
#include <QMap>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================

//  Forward declarations

class TStageObject;
class TStageObjectParams;
class TStageObjectDataElement;
class TSplineDataElement;
class TXsheet;
class TFx;
class TFxSet;

//=================================================

//********************************************************************************
//    StageObjectsData  declaration
//********************************************************************************

//! StageObjectsData is the class used to store multiple schematic stage
//! object's relational data.
class DVAPI StageObjectsData final : public DvMimeData {
  QList<TStageObjectDataElement *>
      m_elements;  //!< The collection of single stage object
                   //!< relational data (owned)
  QList<TSplineDataElement *>
      m_splines;  //!< The collection of single spline (owned)

  // Fxs are not assumed to be stage objects. So, they must be stored
  // independently.

  std::set<TFx *> m_fxs;  //!< Supplied Fx objects (cloned, shared ownership)
  std::set<TFx *> m_originalColumnFxs;  //!< Column Fxs of the supplied column
                                        //! objects (originals, 'owned' due to
  //! m_elements)
  std::set<TFx *> m_terminalFxs;  //!< Selection-terminal fxs (from both the
                                  //! above) (cloned, shared ownership)

  std::map<TFx *, TFx *> m_fxTable;  //!< Original fxs/Cloned fxs pairings

public:
  enum FxFlags {
    eDoClone = 0x1,  // Clones sensible stored data
    eResetFxDagPositions =
        0x2  // Accepts default positions for newly created FX objects
  };

public:
  StageObjectsData();
  ~StageObjectsData();

  //! Clones this object.
  //! \note Clones stored fxs, columns, etc. too.
  StageObjectsData *clone() const override;

  bool isEmpty() const { return m_elements.isEmpty() && m_splines.isEmpty(); }

  //! Stores the xsheet's specified stage objects (be them cameras, columns,
  //! pegbars or else).
  //! Additional flags support TXshColumn cloning for columns.
  void storeObjects(const std::vector<TStageObjectId> &ids, TXsheet *xsheet,
                    int fxFlags);

  //! Overloaded function, stores the specified columns in the supplied xsheet
  void storeColumns(const std::set<int> &columnIndexes, TXsheet *xsheet,
                    int fxFlags);

  //! Stores the specified fxs from the xsheet.
  void storeFxs(const std::set<TFx *> &fxs, TXsheet *xsheet, int fxFlags);

  //! Stores the column fxs \it{and their upstream fxs} associated to the
  //! specified column indices,
  //! \b without storing the columns stage objects.
  void storeColumnFxs(const std::set<int> &columnIndexes, TXsheet *xsh,
                      int fxFlags);

  //! Stores the splines identified by the \b splineIds
  //! NOTE: a spline will be stored only if no TStageObject is connected to it,
  //! or if no TStageObject connected is stored
  //! by this StageObjectsData.
  void storeSplines(const std::list<int> &splineIds, TXsheet *xsh, int fxFlags);

  //! Scans the stored column elements to verify if the specified xsheet is a
  //! sub-xsheet
  //! in of them (thus creating an impossible '\it{circular reference}').
  bool checkCircularReferences(TXsheet *xsheet) const;

  //! Restores the stored objects in the supplied xsheet, inserting columns with
  //! the specified
  //! indices. In case there are more stored columns than specified column
  //! indices, the remaining
  //! columns are inserted right after the last specified index.
  //! Returns the inserted stage object identifiers and updates the
  //! columnIndices with all
  //! the newly inserted column indices.
  //! Additional flags support TXshColumn cloning before insertion.
  std::vector<TStageObjectId> restoreObjects(
      std::set<int> &columnIndices, std::list<int> &restoredSplinIds,
      TXsheet *xsheet, int fxFlags, const TPointD &pos = TConst::nowhere) const;

  std::vector<TStageObjectId> restoreObjects(
      std::set<int> &columnIndices, std::list<int> &restoredSplinIds,
      TXsheet *xsheet, int fxFlags,
      QMap<TStageObjectId, TStageObjectId> &idTable,
      QMap<TFx *, TFx *> &fxTable, const TPointD &pos = TConst::nowhere) const;
};

#endif  // STAGEOBJECT_DATA_H
