#pragma once

#ifndef FXCOMMAND_INCLUDED
#define FXCOMMAND_INCLUDED

// TnzCore includes
#include "tcommon.h"

// TnzBase includes
#include "tfx.h"

// TnzLib includes
#include "toonz/tapplication.h"
#include "toonz/txshcolumn.h"

// Qt includes
#include <QPair>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================

//    Forward declarations

class TXsheetHandle;
class TFxHandle;
class TXsheet;
class TFx;
class TMacroFx;
class TXshColumn;

//=================================================

namespace TFxCommand {
class DVAPI Link {
public:
  TFxP m_inputFx, m_outputFx;
  int m_index;  //!< m_outputFx's input port index

  Link() : m_index(-1) {}
  Link(const TFxP &inputFx, const TFxP &outputFx, int index)
      : m_inputFx(inputFx), m_outputFx(outputFx), m_index(index) {}

  bool operator==(const Link &link) const {
    return m_inputFx == link.m_inputFx && m_outputFx == link.m_outputFx &&
           m_index == link.m_index;
  }

  bool operator!=(const Link &link) const { return !operator==(link); }
  bool operator<(const Link &link) const { return m_index < link.m_index; }
};

//======================================================================================

DVAPI void insertFx(TFx *newFx, const QList<TFxP> &fxs,
                    const QList<Link> &links, TApplication *app, int colunIndex,
                    int rowIndex);
DVAPI void addFx(TFx *newFx, const QList<TFxP> &fxs, TApplication *app,
                 int colunIndex, int rowIndex);
DVAPI void replaceFx(TFx *newFx, const QList<TFxP> &fxs,
                     TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void duplicateFx(TFx *src, TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void unlinkFx(TFx *fx, TFxHandle *fxHandle, TXsheetHandle *xshHandle);
DVAPI void makeMacroFx(const std::vector<TFxP> &fxs, TApplication *app);
DVAPI void explodeMacroFx(TMacroFx *macroFx, TApplication *app);
DVAPI void createOutputFx(TXsheetHandle *xshHandle, TFx *currentFx);
DVAPI void makeOutputFxCurrent(TFx *fx, TXsheetHandle *xshHandle);
DVAPI void disconnectNodesFromXsheet(const std::list<TFxP> &fxs,
                                     TXsheetHandle *xshHandle);
DVAPI void disconnectFxs(const std::list<TFxP> &fxs, TXsheetHandle *xshHandle,
                         const QList<QPair<TFxP, TPointD>> &fxPos);
DVAPI void connectFxs(const Link &link, const std::list<TFxP> &fxs,
                      TXsheetHandle *xshHandle,
                      const QList<QPair<TFxP, TPointD>> &fxPos);
DVAPI void setParent(TFx *fx, TFx *parentFx, int parentFxPort,
                     TXsheetHandle *xshHandle);
DVAPI void connectNodesToXsheet(const std::list<TFxP> &fxs,
                                TXsheetHandle *xshHandle);
DVAPI void deleteSelection(const std::list<TFxP> &fxs,
                           const std::list<Link> &links,
                           const std::list<int> &columnIndexes,
                           TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void pasteFxs(const std::list<TFxP> &fxs,
                    const std::map<TFx *, int> &zeraryFxColumnSize,
                    const std::list<TXshColumnP> &columns, const TPointD &pos,
                    TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void insertPasteFxs(const Link &link, const std::list<TFxP> &fxs,
                          const std::map<TFx *, int> &zeraryFxColumnSize,
                          const std::list<TXshColumnP> &columns,
                          TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void addPasteFxs(TFx *inFx, const std::list<TFxP> &fxs,
                       const std::map<TFx *, int> &zeraryFxColumnSize,
                       const std::list<TXshColumnP> &columns,
                       TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void replacePasteFxs(TFx *inFx, const std::list<TFxP> &fxs,
                           const std::map<TFx *, int> &zeraryFxColumnSize,
                           const std::list<TXshColumnP> &columns,
                           TXsheetHandle *xshHandle, TFxHandle *fxHandle);
DVAPI void renameFx(TFx *fx, const std::wstring &newName,
                    TXsheetHandle *xshHandle);
DVAPI void groupFxs(const std::list<TFxP> &fxs, TXsheetHandle *xshHandle);
DVAPI void ungroupFxs(int groupId, TXsheetHandle *xshHandle);
DVAPI void renameGroup(const std::list<TFxP> &fxs, const std::wstring &name,
                       bool fromEditor, TXsheetHandle *xshHandle);

}  // namespace TFxCommand

#endif  // FXCOMMAND_INCLUDED
