#pragma once

#ifndef TFXSET_INCLUDED
#define TFXSET_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <set>
#include <map>
#include <string>

class TFx;
class TXsheetFx;
class TOStream;
class TIStream;

class DVAPI TFxSet {
protected:
  std::set<TFx *> m_fxs;
  // TXsheetFx *m_xsheetFx;

public:
  TFxSet();
  virtual ~TFxSet();

  void addFx(TFx *fx);
  bool removeFx(TFx *fx);
  bool containsFx(TFx *fx) { return m_fxs.count(fx) > 0; }

  // n.b. aggiunge m_fxs a fxs senza fare clear di quest'ultimo
  void getFxs(std::set<TFx *> &fxs);

  int getFxCount() const;
  TFx *getFx(int index) const;
  TFx *getFx(const std::string &id) const;

  // TXsheetFx *getXsheetFx() const {
  //  return m_xsheetFx;
  //}

  void clear();

  virtual void saveData(TOStream &os, int occupiedColumnCount);
  virtual void loadData(TIStream &os);

private:
  // not implemented
  TFxSet(const TFxSet &);
  TFxSet &operator=(const TFxSet &);
};

// helper functions
DVAPI TFx *searchFx(const std::map<TFx *, TFx *> &table, TFx *fx);
void DVAPI updateFxLinks(const std::map<TFx *, TFx *> &table);

#endif
