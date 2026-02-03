

// TnzBase includes
#include "tfxattributes.h"

// TnzLib includes
#include "toonz/tcolumnfx.h"
#include "toonz/txsheet.h"
#include "toonz/txshzeraryfxcolumn.h"

#include "fxdata.h"

//******************************************************************
//    Local namespace  stuff
//******************************************************************

namespace {

void linkFxs(const QMap<TFx *, TFx *> &clonedFxs,
             const QList<Link> &selectedLinks) {
  int i;
  for (i = 0; i < selectedLinks.size(); i++) {
    TFx *outFx = selectedLinks[i].m_outputFx.getPointer();

    TZeraryColumnFx *zerayFx = dynamic_cast<TZeraryColumnFx *>(outFx);
    if (zerayFx) outFx       = zerayFx->getZeraryFx();

    TFx *inFx         = selectedLinks[i].m_inputFx.getPointer();
    zerayFx           = dynamic_cast<TZeraryColumnFx *>(inFx);
    if (zerayFx) inFx = zerayFx->getZeraryFx();

    if (!clonedFxs.contains(outFx) || !clonedFxs.contains(inFx)) continue;

    TFx *clonedOutFx = clonedFxs[outFx];
    TFx *clonedInFx  = clonedFxs[inFx];
    assert(clonedOutFx && clonedInFx);

    clonedOutFx->getInputPort(selectedLinks[i].m_index)->setFx(clonedInFx);
  }
}

//-------------------------------------------------------

void linkFxs(const QMap<TFx *, TFx *> &clonedFxs) {
  QMap<TFx *, TFx *>::const_iterator it;
  for (it = clonedFxs.begin(); it != clonedFxs.end(); it++) {
    TFx *fx = it.key();
    int j, portCount = fx->getInputPortCount();
    for (j = 0; j < portCount; j++) {
      TFx *inputFx = fx->getInputPort(j)->getFx();
      if (!clonedFxs.contains(inputFx)) continue;

      TFx *clonedFx      = clonedFxs[fx];
      TFx *inputClonedFx = clonedFxs[inputFx];
      assert(clonedFx && inputClonedFx);
      clonedFx->getInputPort(j)->setFx(inputClonedFx);
    }
  }
}

//-------------------------------------------------------

bool canCopyFx(TFx *fx) {
  TLevelColumnFx *lcFx  = dynamic_cast<TLevelColumnFx *>(fx);
  TPaletteColumnFx *pfx = dynamic_cast<TPaletteColumnFx *>(fx);
  TXsheetFx *xfx        = dynamic_cast<TXsheetFx *>(fx);
  TOutputFx *ofx        = dynamic_cast<TOutputFx *>(fx);

  return (!lcFx && !pfx && !xfx && !ofx);
}

}  // namespace

//******************************************************************
//    FxsData  implementation
//******************************************************************

FxsData::FxsData() : m_connected(false) {}

//-------------------------------------------------------

void FxsData::setFxs(const QList<TFxP> &selectedFxs,
                     const QList<Link> &selectedLinks,
                     const QList<int> &columnIndexes, TXsheet *xsh) {
  // fx->clonedFx
  QMap<TFx *, TFx *> clonedFxs;
  for (int i = 0; i < selectedFxs.size(); i++) {
    TFx *fx = selectedFxs[i].getPointer();
    if (!canCopyFx(fx)) continue;
    TZeraryColumnFx *zerayFx = dynamic_cast<TZeraryColumnFx *>(fx);
    if (zerayFx) fx          = zerayFx->getZeraryFx();
    TFx *clonedFx            = fx->clone(false);
    TPointD pos;
    if (zerayFx)
      pos = zerayFx->getAttributes()->getDagNodePos();
    else
      pos = fx->getAttributes()->getDagNodePos();
    clonedFx->getAttributes()->setDagNodePos(pos);
    m_fxs.append(clonedFx);
    if (zerayFx)
      m_zeraryFxColumnSize[clonedFx] = zerayFx->getColumn()->getRowCount();
    m_visitedFxs[clonedFx]           = false;
    clonedFxs[fx]                    = clonedFx;

    TFx *linkedFx = fx->getLinkedFx();
    if (linkedFx && clonedFxs.contains(linkedFx))
      clonedFx->linkParams(clonedFxs[linkedFx]);
  }

  QList<int>::const_iterator it;
  for (it = columnIndexes.begin(); it != columnIndexes.end(); it++) {
    TXshColumn *col    = xsh->getColumn(*it);
    TXshColumn *newCol = col->clone();
    newCol->getFx()->getAttributes()->setDagNodePos(
        col->getFx()->getAttributes()->getDagNodePos());
    m_columns.append(newCol);
    clonedFxs[col->getFx()] = newCol->getFx();
  }

  linkFxs(clonedFxs, selectedLinks);
  checkConnectivity();
}

//-------------------------------------------------------

void FxsData::getFxs(QList<TFxP> &fxs, QMap<TFx *, int> &zeraryFxColumnSize,
                     QList<TXshColumnP> &columns) const {
  QMap<TFx *, TFx *> clonedFxs;
  for (int i = 0; i < m_fxs.size(); i++) {
    TFx *clonedFx = m_fxs[i]->clone(false);
    TPointD pos   = m_fxs[i]->getAttributes()->getDagNodePos();
    clonedFx->getAttributes()->setDagNodePos(pos);
    clonedFx->getAttributes()->removeFromAllGroup();
    fxs.append(clonedFx);
    if (m_fxs[i]->isZerary())
      zeraryFxColumnSize[clonedFx] =
          m_zeraryFxColumnSize[m_fxs[i].getPointer()];
    clonedFxs[m_fxs[i].getPointer()] = clonedFx;

    TFx *linkedFx = m_fxs[i]->getLinkedFx();
    if (linkedFx && clonedFxs.contains(linkedFx))
      clonedFx->linkParams(clonedFxs[linkedFx]);
  }

  QList<TXshColumnP>::const_iterator it;
  for (it = m_columns.begin(); it != m_columns.end(); it++) {
    TXshColumn *col    = it->getPointer();
    TXshColumn *newCol = col->clone();
    newCol->getFx()->getAttributes()->setDagNodePos(
        col->getFx()->getAttributes()->getDagNodePos());
    columns.append(newCol);
    clonedFxs[col->getFx()] = newCol->getFx();
  }

  linkFxs(clonedFxs);
}

//-------------------------------------------------------

FxsData *FxsData::clone() const {
  FxsData *data = new FxsData;
  getFxs(data->m_fxs, data->m_zeraryFxColumnSize, data->m_columns);

  return data;
}

//-------------------------------------------------------

void FxsData::checkConnectivity() {
  if (m_fxs.isEmpty()) return;
  visitFx(m_fxs.at(0).getPointer());
  m_connected = true;
  QMap<TFx *, bool>::const_iterator it;
  for (it = m_visitedFxs.begin(); it != m_visitedFxs.end(); it++)
    m_connected = m_connected && it.value();
}

//-------------------------------------------------------

void FxsData::visitFx(TFx *fx) {
  if (m_visitedFxs.value(fx)) return;
  m_visitedFxs[fx] = true;
  int i;
  for (i = 0; i < fx->getInputPortCount(); i++) {
    TFx *inputFx = fx->getInputPort(i)->getFx();
    if (m_visitedFxs.contains(inputFx) && areLinked(fx, inputFx))
      visitFx(inputFx);
  }
  for (i = 0; i < fx->getOutputConnectionCount(); i++) {
    TFx *outputFx = fx->getOutputConnection(i)->getOwnerFx();
    if (m_visitedFxs.contains(outputFx) && areLinked(outputFx, fx))
      visitFx(outputFx);
  }
}

//---------------------------------------------------------

bool FxsData::areLinked(TFx *outFx, TFx *inFx) {
  for (int i = 0; i < outFx->getInputPortCount(); i++) {
    TFx *inputFx = outFx->getInputPort(i)->getFx();
    if (inFx == inputFx) return true;
  }
  return false;
}
