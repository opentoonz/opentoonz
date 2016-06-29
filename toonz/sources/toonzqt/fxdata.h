#pragma once

#ifndef FXDATA_H
#define FXDATA_H

// TnzBase includes
#include "tfx.h"

// TnzLib includes
#include "toonz/fxcommand.h"
#include "toonz/txshcolumn.h"
#include "toonz/tcolumnfx.h"

// TnzQt includes
#include "toonzqt/dvmimedata.h"

// Qt includes
#include <QList>
#include <QPair>

using namespace TFxCommand;

//**********************************************************************
//    FxsData  declaration
//**********************************************************************

class FxsData final : public DvMimeData {
  QList<TFxP> m_fxs;
  QMap<TFx *, bool> m_visitedFxs;
  QMap<TFx *, int> m_zeraryFxColumnSize;
  QList<TXshColumnP> m_columns;
  bool m_connected;

public:
  FxsData();

  FxsData *clone() const override;

  //! Set the FxsData. FxsData<-QList<TFxP>
  void setFxs(const QList<TFxP> &selectedFxs, const QList<Link> &selectedLinks,
              const QList<int> &columnIndexes, TXsheet *xsh);

  //! Get the FxsData. FxsData->QList<TFxP>
  void getFxs(QList<TFxP> &selectedFxs, QMap<TFx *, int> &zeraryFxColumnSize,
              QList<TXshColumnP> &columns) const;

  //! Return true if copied fxs makes a connected graph.
  bool isConnected() const { return m_connected; }

private:
  void checkConnectivity();
  void visitFx(TFx *fx);
  bool areLinked(TFx *outFx, TFx *inFx);
};

#endif  // FXDATA_H
