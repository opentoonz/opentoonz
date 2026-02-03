#pragma once

#ifndef FXDAG_INCLUDED
#define FXDAG_INCLUDED

#include "tcommon.h"
#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFxSet;
class TFx;
class TOutputFx;
class TOStream;
class TIStream;

class DVAPI FxDag {
  enum DagGridDimension { eLarge = 0, eSmall = 1 };

  TFxSet *m_internalFxs, *m_terminalFxs;
  TFx *m_xsheetFx;
  std::vector<TOutputFx *> m_outputFxs;
  std::map<std::string, int> m_typeTable;
  std::map<std::wstring, TFx *> m_idTable;
  int m_groupIdCount;
  int m_dagGridDimension;

public:
  FxDag();
  ~FxDag();

  // FxDag *clone() const;
  void assignUniqueId(TFx *fx);
  int getFxTypeCount(TFx *fx);
  void updateFxTypeTable(TFx *fx, int value);
  void updateFxIdTable(TFx *fx);
  int getNewGroupId() { return ++m_groupIdCount; };

  TFxSet *getInternalFxs() const;
  TFxSet *getTerminalFxs() const;

  void addToXsheet(TFx *fx);
  void removeFromXsheet(TFx *fx);

  TFx *getXsheetFx() const { return m_xsheetFx; }

  int getOutputFxCount() const { return m_outputFxs.size(); }
  TOutputFx *getOutputFx(int index) const { return m_outputFxs[index]; }
  TOutputFx *addOutputFx(TOutputFx *outputFx = 0);
  void removeOutputFx(TOutputFx *fx);
  void setCurrentOutputFx(TOutputFx *fx);
  TOutputFx *getCurrentOutputFx() const;
  //! Returns \b true if the link between inputFx and fx creates a loop in the
  //! dag.
  //! inputFx is the effect that is taken in input by fx.
  bool checkLoop(TFx *inputFx, TFx *fx);

  bool isRendered(TFx *fx) const;
  bool isControl(TFx *fx) const;

  void getFxs(std::vector<TFx *> &fxs) const;

  TFx *getFxById(std::wstring id) const;

  void setDagGridDimension(int dim) { m_dagGridDimension = dim; }
  int getDagGridDimension() const { return m_dagGridDimension; }

  void saveData(TOStream &os, int occupiedColumnCount);
  void loadData(TIStream &os);

private:
  // not implemented
  FxDag(const FxDag &);
  FxDag &operator=(const FxDag &);
};

#endif
