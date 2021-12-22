#include "expressionreferencemanager.h"

#include "tapp.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheetexpr.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"
#include "toonz/tstageobject.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/toonzscene.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobjecttree.h"

// TnzBase includes
#include "tdoubleparam.h"
#include "texpression.h"
#include "tdoublekeyframe.h"
#include "tfx.h"

#include "tmsgcore.h"

#include <QList>

#include <boost/xpressive/xpressive_static.hpp>
#include <boost/xpressive/regex_actions.hpp>

namespace {
// reference : columncommand.cpp
bool canRemoveFx(const std::set<TFx*>& leaves, TFx* fx) {
  bool removeFx = false;
  for (int i = 0; i < fx->getInputPortCount(); i++) {
    TFx* inputFx = fx->getInputPort(i)->getFx();
    if (!inputFx) continue;
    if (leaves.count(inputFx) > 0) {
      removeFx = true;
      continue;
    }
    if (!canRemoveFx(leaves, inputFx)) return false;
    removeFx = true;
  }
  return removeFx;
}

void gatherXsheets(TXsheet* xsheet, QSet<TXsheet*>& ret) {
  // return if it is already registered
  if (ret.contains(xsheet)) return;

  ret.insert(xsheet);

  // trace xsheet and recursively find sub-xsheets in it
  for (int c = 0; c < xsheet->getColumnCount(); c++) {
    if (xsheet->isColumnEmpty(c)) continue;
    TXshLevelColumn* levelColumn = xsheet->getColumn(c)->getLevelColumn();
    if (!levelColumn) continue;

    int start, end;
    levelColumn->getRange(start, end);
    for (int r = start; r <= end; r++) {
      int r0, r1;
      if (!levelColumn->getLevelRange(r, r0, r1)) continue;

      TXshChildLevel* childLevel =
          levelColumn->getCell(r).m_level->getChildLevel();
      if (childLevel) {
        gatherXsheets(childLevel->getXsheet(), ret);
      }

      r = r1;
    }
  }
}

QSet<TXsheet*> getAllXsheets() {
  QSet<TXsheet*> ret;
  TXsheet* topXsheet =
      TApp::instance()->getCurrentScene()->getScene()->getTopXsheet();
  gatherXsheets(topXsheet, ret);
  return ret;
}

static QList<QList<std::string>> objExprPhrases = {
    {"table", "tab"},   // Table
    {"col"},            // Column
    {"cam", "camera"},  // Camera
    {"peg", "pegbar"},  // Pegbar
    {}                  // Spline and others
};

int getObjTypeIndex(TStageObjectId id) {
  if (id.isTable())
    return 0;
  else if (id.isColumn())
    return 1;
  else if (id.isCamera())
    return 2;
  else if (id.isPegbar())
    return 3;
  else
    return 4;
}

int getPhraseCount(TStageObjectId id) {
  return objExprPhrases[getObjTypeIndex(id)].count();
}

std::string getPhrase(TStageObjectId id, int index = 0) {
  if (getPhraseCount(id) <= index) index = 0;

  std::string indexStr =
      (id.isTable()) ? "" : std::to_string(id.getIndex() + 1);
  // including period to avoid misunderstanding "col10" as "col1"
  return objExprPhrases[getObjTypeIndex(id)][index] + indexStr + ".";
}

std::string getPhrase(TFx* fx) {
  QString fxIdStr = QString::fromStdWString(toLower(fx->getFxId()));
  return "fx." + fxIdStr.toStdString() + ".";
}

}  // namespace

//----------------------------------------------------------------------------
ExpressionReferenceMonitor* ExpressionReferenceManager::currentMonitor() {
  return TApp::instance()->getCurrentXsheet()->getXsheet()->getExpRefMonitor();
}

QMap<TDoubleParam*, ExpressionReferenceMonitorInfo>&
ExpressionReferenceManager::info(TXsheet* xsh) {
  if (xsh)
    return xsh->getExpRefMonitor()->info();
  else
    return currentMonitor()->info();
}
//-----------------------------------------------------------------------------

ExpressionReferenceMonitorInfo& ExpressionReferenceManager::touchInfo(
    TDoubleParam* param, TXsheet* xsh) {
  if (!info(xsh).contains(param)) {
    ExpressionReferenceMonitorInfo newInfo;
    info(xsh).insert(param, newInfo);
    param->addObserver(this);
  }
  return info(xsh)[param];
}

//-----------------------------------------------------------------------------

ExpressionReferenceManager::ExpressionReferenceManager()
    : m_model(new FunctionTreeModel()), m_blockParamChange(false) {}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::init() {
  connect(TApp::instance()->getCurrentScene(),
          SIGNAL(preferenceChanged(const QString&)), this,
          SLOT(onPreferenceChanged(const QString&)));
  onPreferenceChanged("modifyExpressionOnMovingReferences");
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::onPreferenceChanged(const QString& prefName) {
  if (prefName != "modifyExpressionOnMovingReferences") return;

  TXsheetHandle* xshHandle  = TApp::instance()->getCurrentXsheet();
  TSceneHandle* sceneHandle = TApp::instance()->getCurrentScene();
  bool on =
      Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled();
  if (on) {
    // when the scene switched, refresh the all list
    connect(sceneHandle, SIGNAL(sceneSwitched()), this,
            SLOT(onSceneSwitched()));
    connect(xshHandle, SIGNAL(xsheetSwitched()), this,
            SLOT(onXsheetSwitched()));
    connect(xshHandle, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
    onSceneSwitched();
  } else {
    // when the scene switched, refresh the all list
    disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
               SLOT(onSceneSwitched()));
    disconnect(xshHandle, SIGNAL(xsheetSwitched()), this,
               SLOT(onXsheetSwitched()));
    disconnect(xshHandle, SIGNAL(xsheetChanged()), this,
               SLOT(onXsheetChanged()));

    // clear all monitor info
    QSet<TXsheet*> allXsheets = getAllXsheets();
    for (auto xsh : allXsheets) {
      for (auto curve : xsh->getExpRefMonitor()->info().keys())
        curve->removeObserver(this);
      xsh->getExpRefMonitor()->clearAll();
      xsh->setObserver(nullptr);
    }
  }
}

//-----------------------------------------------------------------------------

ExpressionReferenceManager* ExpressionReferenceManager::instance() {
  static ExpressionReferenceManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

bool ExpressionReferenceManager::refreshParamsRef(TDoubleParam* curve,
                                                  TXsheet* xsh) {
  QSet<int> colRef;
  QSet<TDoubleParam*> paramsRef;
  for (int k = 0; k < curve->getKeyframeCount(); k++) {
    TDoubleKeyframe keyframe = curve->getKeyframe(k);

    if (keyframe.m_type != TDoubleKeyframe::Expression &&
        keyframe.m_type != TDoubleKeyframe::SimilarShape)
      continue;

    TExpression expr;
    expr.setGrammar(curve->getGrammar());
    expr.setText(keyframe.m_expressionText);

    QSet<int> tmpColRef;
    QSet<TDoubleParam*> tmpParamsRef;
    referenceParams(expr, tmpColRef, tmpParamsRef);
    colRef += tmpColRef;
    paramsRef += tmpParamsRef;
  }
  // replace the indices
  bool hasRef = !colRef.isEmpty() || !paramsRef.isEmpty();
  if (hasRef) {
    touchInfo(curve, xsh).colRefMap()   = colRef;
    touchInfo(curve, xsh).paramRefMap() = paramsRef;
  } else {
    info(xsh).remove(curve);
  }

  return hasRef;
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::checkRef(TreeModel::Item* item, TXsheet* xsh) {
  if (FunctionTreeModel::Channel* channel =
          dynamic_cast<FunctionTreeModel::Channel*>(item)) {
    TDoubleParam* curve = channel->getParam();
    bool hasRef         = refreshParamsRef(curve, xsh);
    if (hasRef) touchInfo(curve, xsh).name() = channel->getLongName();
  } else
    for (int i = 0; i < item->getChildCount(); i++)
      checkRef(item->getChild(i), xsh);
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel* ExpressionReferenceManager::findChannel(
    TDoubleParam* param, TreeModel::Item* item) {
  if (FunctionTreeModel::Channel* channel =
          dynamic_cast<FunctionTreeModel::Channel*>(item)) {
    if (channel->getParam() == param) return channel;
  } else {
    for (int i = 0; i < item->getChildCount(); i++) {
      FunctionTreeModel::Channel* ret = findChannel(param, item->getChild(i));
      if (ret) return ret;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::gatherParams(TreeModel::Item* item,
                                              QList<TDoubleParam*>& paramSet) {
  if (FunctionTreeModel::Channel* channel =
          dynamic_cast<FunctionTreeModel::Channel*>(item)) {
    paramSet.append(channel->getParam());
  } else
    for (int i = 0; i < item->getChildCount(); i++)
      gatherParams(item->getChild(i), paramSet);
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::onSceneSwitched() {
  QSet<TXsheet*> allXsheets = getAllXsheets();
  for (auto xsh : allXsheets) {
    xsh->setObserver(this);

    m_model->refreshData(xsh);
    xsh->getExpRefMonitor()->clearAll();

    for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
      checkRef(m_model->getStageObjectChannel(i), xsh);
    }
    for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
      checkRef(m_model->getFxChannel(i), xsh);
    }
  }
  onXsheetSwitched();
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::refreshXsheetRefInfo(TXsheet* xsh) {
  xsh->setObserver(this);
  m_model->refreshData(xsh);
  xsh->getExpRefMonitor()->clearAll();
  for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
    checkRef(m_model->getStageObjectChannel(i), xsh);
  }
  for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
    checkRef(m_model->getFxChannel(i), xsh);
  }
  onXsheetSwitched();
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::onXsheetSwitched() {
  TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  xsh->setObserver(this);
  m_model->refreshData(xsh);
}

//----------------------------------------------------------------------------

void ExpressionReferenceManager::onXsheetChanged() {
  TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_model->refreshData(xsh);
  // remove deleted parameters
  QList<TDoubleParam*> paramSet;
  for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
    gatherParams(m_model->getStageObjectChannel(i), paramSet);
  }
  for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
    gatherParams(m_model->getFxChannel(i), paramSet);
  }

  // remove deleted parameter from reference map
  for (auto itr = info(xsh).begin(); itr != info(xsh).end();) {
    if (!paramSet.contains(itr.key()))
      itr = info(xsh).erase(itr);
    else {
      // check if the referenced parameters are deleted
      if (!itr.value().ignored()) {
        for (auto refParam : itr.value().paramRefMap()) {
          if (!paramSet.contains(refParam)) {
            // ignore the parameter if the reference does not exist anymore
            itr.value().ignored() = true;
            break;
          }
        }
      }
      ++itr;
    }
  }
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::onChange(const TXsheetColumnChange& change) {
  TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  QMap<TStageObjectId, TStageObjectId> idTable;

  auto setIds = [&](int from, int to) {
    idTable.insert(TStageObjectId::ColumnId(from),
                   TStageObjectId::ColumnId(to));
  };

  switch (change.m_type) {
  case TXsheetColumnChange::Insert: {
    for (int c = xsh->getColumnCount() - 2; c >= change.m_index1; c--) {
      setIds(c, c + 1);
    }
  } break;
  case TXsheetColumnChange::Remove: {
    // update ignore info
    for (auto it = info().begin(); it != info().end(); it++) {
      if (it.value().colRefMap().contains(change.m_index1))
        it.value().ignored() = true;
    }
    for (int c = change.m_index1; c < xsh->getColumnCount(); c++) {
      setIds(c + 1, c);
    }
  } break;
  case TXsheetColumnChange::Move: {
    if (change.m_index1 < change.m_index2) {
      setIds(change.m_index1, change.m_index2);
      for (int c = change.m_index1 + 1; c <= change.m_index2; c++) {
        setIds(c, c - 1);
      }
    } else {
      setIds(change.m_index1, change.m_index2);
      for (int c = change.m_index2; c < change.m_index1; c++) {
        setIds(c, c + 1);
      }
    }
  } break;
  }

  // use empty map since the fxs does not transfer
  QMap<TFx*, TFx*> fxTable;
  transferReference(xsh, xsh, idTable, fxTable);
}

void ExpressionReferenceManager::onFxAdded(const std::vector<TFx*>& fxs) {
  for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
    FxChannelGroup* fcg =
        dynamic_cast<FxChannelGroup*>(m_model->getFxChannel(i));
    if (fcg && fxs.end() != std::find(fxs.begin(), fxs.end(), fcg->getFx()))
      checkRef(fcg);
  }
}

void ExpressionReferenceManager::onStageObjectAdded(
    const TStageObjectId objId) {
  for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
    StageObjectChannelGroup* socg = dynamic_cast<StageObjectChannelGroup*>(
        m_model->getStageObjectChannel(i));
    if (socg && objId == socg->getStageObject()->getId()) checkRef(socg);
  }
}

bool ExpressionReferenceManager::isIgnored(TDoubleParam* param) {
  return touchInfo(param).ignored();
}

//-----------------------------------------------------------------------------
// TParamObserver implementation
void ExpressionReferenceManager::onChange(const TParamChange& change) {
  // do nothing if the change is due to this manager itself
  if (m_blockParamChange) return;
  // do nothing if keyframe does not change or while dragging
  if (!change.m_keyframeChanged || change.m_dragging) return;
  TDoubleParam* curve = dynamic_cast<TDoubleParam*>(change.m_param);
  if (!curve) return;
  bool hasRef = refreshParamsRef(curve, nullptr);
  if (hasRef) {
    FunctionTreeModel::Channel* channel = nullptr;
    for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
      channel = findChannel(curve, m_model->getStageObjectChannel(i));
      if (channel) break;
    }
    if (!channel) {
      for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
        channel = findChannel(curve, m_model->getFxChannel(i));
        if (channel) break;
      }
    }
    if (channel) {
      touchInfo(curve).name() = channel->getLongName();
    }

    if (touchInfo(curve).ignored()) {
      DVGui::info(tr("Expression monitoring restarted: \"%1\"")
                      .arg(touchInfo(curve).name()));
      touchInfo(curve).ignored() = false;
    }
  }
}

//-----------------------------------------------------------------------------

void ExpressionReferenceManager::replaceExpressionTexts(
    TDoubleParam* curve, const std::map<std::string, std::string> replaceMap,
    TXsheet* xsh) {
  if (touchInfo(curve, xsh).ignored() || replaceMap.empty()) {
    for (int kIndex = 0; kIndex < curve->getKeyframeCount(); kIndex++) {
      TDoubleKeyframe keyframe = curve->getKeyframe(kIndex);
      if (keyframe.m_type != TDoubleKeyframe::Expression &&
          keyframe.m_type != TDoubleKeyframe::SimilarShape)
        continue;

      // check circular reference
      TExpression expr;
      expr.setGrammar(curve->getGrammar());
      expr.setText(keyframe.m_expressionText);
      // put "?" marks on both ends of the expression text when the circular
      // reference is detected in order to avoid crash
      if (dependsOn(expr, curve))
        keyframe.m_expressionText = "?" + keyframe.m_expressionText + "?";

      m_blockParamChange = true;
      KeyframeSetter setter(curve, kIndex, false);
      if (keyframe.m_type == TDoubleKeyframe::Expression)
        setter.setExpression(keyframe.m_expressionText);
      else  // SimilarShape case
        setter.setSimilarShape(keyframe.m_expressionText,
                               keyframe.m_similarShapeOffset);
      m_blockParamChange = false;
    }

    return;
  }

  boost::xpressive::local<std::string const*> pstr;
  const boost::xpressive::sregex rx =
      (boost::xpressive::a1 = replaceMap)[pstr = &boost::xpressive::a1];

  for (int kIndex = 0; kIndex < curve->getKeyframeCount(); kIndex++) {
    TDoubleKeyframe keyframe = curve->getKeyframe(kIndex);

    if (keyframe.m_type != TDoubleKeyframe::Expression &&
        keyframe.m_type != TDoubleKeyframe::SimilarShape)
      continue;

    // replace expression
    QString expr       = QString::fromStdString(keyframe.m_expressionText);
    QStringList list   = expr.split('"');
    bool isStringToken = false;
    for (QString& partialExp : list) {
      if (isStringToken) continue;
      isStringToken = !isStringToken;

      std::string partialExpStr = partialExp.toStdString();
      std::string replacedStr =
          boost::xpressive::regex_replace(partialExpStr, rx, *pstr);
      partialExp = QString::fromStdString(replacedStr);
    }

    QString newExpr = list.join('"');

    m_blockParamChange = true;
    KeyframeSetter setter(curve, kIndex, false);
    if (keyframe.m_type == TDoubleKeyframe::Expression)
      setter.setExpression(newExpr.toStdString());
    else  // SimilarShape case
      setter.setSimilarShape(newExpr.toStdString(),
                             keyframe.m_similarShapeOffset);
    m_blockParamChange = false;

    if (newExpr != expr) {
      DVGui::info(tr("Expression modified: \"%1\" key at frame %2, %3 -> %4")
                      .arg(touchInfo(curve, xsh).name())
                      .arg(keyframe.m_frame + 1)
                      .arg(expr)
                      .arg(newExpr));
    }
  }
}

//-----------------------------------------------------------------------------
bool ExpressionReferenceManager::doCheckReferenceDeletion(
    const QSet<int>& columnIdsToBeDeleted, const QSet<TFx*>& fxsToBeDeleted,
    const QList<TStageObjectId>& objectIdsToBeDeleted,
    const QList<TStageObjectId>& objIdsToBeDuplicated, bool checkInvert) {
  QList<TDoubleParam*> paramsToBeDeleted;
  QList<TDoubleParam*> invParamsToBeDeleted;
  // gather Fx parameters to be deleted
  for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
    FxChannelGroup* fcg =
        dynamic_cast<FxChannelGroup*>(m_model->getFxChannel(i));
    if (!fcg) continue;
    if (fxsToBeDeleted.contains(fcg->getFx()))
      gatherParams(fcg, paramsToBeDeleted);
    else if (checkInvert)
      gatherParams(fcg, invParamsToBeDeleted);
  }
  // gather stage objects parameters to be deleted
  for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
    StageObjectChannelGroup* socg = dynamic_cast<StageObjectChannelGroup*>(
        m_model->getStageObjectChannel(i));
    if (!socg) continue;
    TStageObjectId id = socg->getStageObject()->getId();
    if (objectIdsToBeDeleted.contains(id))
      gatherParams(socg, paramsToBeDeleted);
    // objects to be duplicated in the sub xsheet will not lose referenced from
    // either xsheet
    else if (checkInvert && !objIdsToBeDuplicated.contains(id))
      gatherParams(socg, invParamsToBeDeleted);
  }

  // gather parameters which refers to the parameters to be deleted
  QSet<TDoubleParam*> cautionParams;
  QSet<TDoubleParam*> invCautionParams;

  for (auto itr = info().begin(); itr != info().end(); itr++) {
    // find params containing columnId to be deleted
    for (auto refColId : itr.value().colRefMap()) {
      if (columnIdsToBeDeleted.contains(refColId))
        cautionParams.insert(itr.key());
      else if (checkInvert)
        invCautionParams.insert(itr.key());
    }
    // find params containing fx/stage params to be deleted as well
    for (auto refParam : itr.value().paramRefMap()) {
      if (paramsToBeDeleted.contains(refParam)) cautionParams.insert(itr.key());
      if (checkInvert && invParamsToBeDeleted.contains(refParam))
        invCautionParams.insert(itr.key());
    }
  }

  // remove parameters from the list which itself will be deleted
  for (auto it = cautionParams.begin(); it != cautionParams.end();)
    if (paramsToBeDeleted.contains(*it))
      it = cautionParams.erase(it);
    else
      ++it;
  for (auto it = invCautionParams.begin(); it != invCautionParams.end();)
    if (invParamsToBeDeleted.contains(*it))
      it = invCautionParams.erase(it);
    else
      ++it;

  // return true if there is no parameters which will lose references
  if (cautionParams.isEmpty() && invCautionParams.isEmpty()) return true;

  // open warning popup
  QString warningTxt =
      tr("Following parameters will lose reference in expressions:");
  for (auto param : cautionParams) {
    warningTxt += "\n  " + touchInfo(param).name();
  }
  for (auto param : invCautionParams) {
    warningTxt += "\n  " + touchInfo(param).name() + "  " +
                  tr("(To be in the sub xsheet)");
  }
  warningTxt += "\n" + tr("Do you want to continue the operation anyway ?");

  int ret = DVGui::MsgBox(warningTxt, QObject::tr("Continue"),
                          QObject::tr("Cancel"), 0);
  if (ret == 0 || ret == 2) return false;

  return true;
}

//-----------------------------------------------------------------------------
// check on deleting columns
bool ExpressionReferenceManager::checkReferenceDeletion(
    const QSet<int>& columnIdsToBeDeleted, const QSet<TFx*>& fxsToBeDeleted,
    const QList<TStageObjectId>& objIdsToBeDuplicated, bool checkInvert) {
  bool on =
      Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled();
  if (!on) return true;
  QList<TStageObjectId> objectIdsToBeDeleted;
  for (auto colId : columnIdsToBeDeleted)
    objectIdsToBeDeleted.append(TStageObjectId::ColumnId(colId));

  return doCheckReferenceDeletion(columnIdsToBeDeleted, fxsToBeDeleted,
                                  objectIdsToBeDeleted, objIdsToBeDuplicated,
                                  checkInvert);
}

//-----------------------------------------------------------------------------
// check on deleting stage objects
bool ExpressionReferenceManager::checkReferenceDeletion(
    const QList<TStageObjectId>& objectIdsToBeDeleted) {
  bool on =
      Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled();
  if (!on) return true;
  QSet<int> columnIdsToBeDeleted;
  QSet<TFx*> fxsToBeDeleted;

  TApp* app    = TApp::instance();
  TXsheet* xsh = app->getCurrentXsheet()->getXsheet();
  std::set<TFx*> leaves;
  // fx references should be checked when deleting columns
  for (const auto& objId : objectIdsToBeDeleted) {
    if (objId.isColumn()) {
      int index = objId.getIndex();
      if (index < 0) continue;
      TXshColumn* column = xsh->getColumn(index);
      if (!column) continue;
      columnIdsToBeDeleted.insert(index);
      TFx* fx = column->getFx();
      if (fx) {
        leaves.insert(fx);
        TZeraryColumnFx* zcfx = dynamic_cast<TZeraryColumnFx*>(fx);
        if (zcfx) fxsToBeDeleted.insert(zcfx->getZeraryFx());
      }
    }
  }
  // store fx to be deleted along with columns
  TFxSet* fxSet = xsh->getFxDag()->getInternalFxs();
  for (int i = 0; i < fxSet->getFxCount(); i++) {
    TFx* fx = fxSet->getFx(i);
    if (canRemoveFx(leaves, fx)) fxsToBeDeleted.insert(fx);
  }
  QList<TStageObjectId> dummy;

  return doCheckReferenceDeletion(columnIdsToBeDeleted, fxsToBeDeleted,
                                  objectIdsToBeDeleted, dummy);
}

//-----------------------------------------------------------------------------
// check on exploding sub xsheet.
// - If removeColumn is true, it means that the sub xsheet column in the parent
// xsheet will be deleted.
// - If columnsOnly is true, it means that all references to the objects other
// than columns in the sub xsheet will be lost.
// - If columnsOnly is false, it means that references to camera not connected
// to the table node in the sub xsheet will be lost.
// - Open warning popup if there is any expression which will lose reference
// after the operation.

bool ExpressionReferenceManager::checkExplode(TXsheet* childXsh, int index,
                                              bool removeColumn,
                                              bool columnsOnly) {
  // return if the preference option is off
  bool on =
      Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled();
  if (!on) return true;

  QSet<TDoubleParam*> mainCautionParams, subCautionParams;
  if (removeColumn) {
    // find params referring to the sub xsheet column to be exploded and removed
    for (auto itr = info().begin(); itr != info().end(); itr++) {
      if (itr.value().colRefMap().contains(index))
        mainCautionParams.insert(itr.key());
    }
  }

  m_model->refreshData(childXsh);
  // find params referring to the stage params to be deleted
  QList<TDoubleParam*> stageParamsToBeDeleted;
  TStageObject* table = childXsh->getStageObject(TStageObjectId::TableId);
  for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
    StageObjectChannelGroup* socg = dynamic_cast<StageObjectChannelGroup*>(
        m_model->getStageObjectChannel(i));
    if (!socg) continue;
    TStageObjectId id = socg->getStageObject()->getId();
    if ((columnsOnly && !id.isColumn()) ||
        (!columnsOnly && !socg->getStageObject()->isAncestor(table)))
      gatherParams(socg, stageParamsToBeDeleted);
  }
  for (auto itr = info(childXsh).begin(); itr != info(childXsh).end(); itr++) {
    for (auto refParam : itr.value().paramRefMap()) {
      if (stageParamsToBeDeleted.contains(refParam)) {
        subCautionParams.insert(itr.key());
        break;
      }
    }
  }

  // remove parameters from the list which itself will be deleted
  for (auto it = subCautionParams.begin(); it != subCautionParams.end();)
    if (stageParamsToBeDeleted.contains(*it))
      it = subCautionParams.erase(it);
    else
      ++it;

  TXsheet* currentXsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_model->refreshData(currentXsh);

  // return true if there is no parameters which will lose references
  if (mainCautionParams.isEmpty() && subCautionParams.isEmpty()) return true;

  // open warning popup
  QString warningTxt =
      tr("Following parameters will lose reference in expressions:");
  for (auto param : mainCautionParams) {
    warningTxt +=
        "\n  " + touchInfo(param).name() + "  " + tr("(In the current xsheet)");
  }
  for (auto param : subCautionParams) {
    warningTxt += "\n  " + touchInfo(param, childXsh).name() + "  " +
                  tr("(To be brought from the subxsheet)");
  }
  warningTxt += tr("\nDo you want to explode anyway ?");

  int ret = DVGui::MsgBox(warningTxt, QObject::tr("Explode"),
                          QObject::tr("Cancel"), 0);
  if (ret == 0 || ret == 2) return false;

  return true;
}

//----------------------------------------------------------------------------

void ExpressionReferenceManager::transferReference(
    TXsheet* fromXsh, TXsheet* toXsh,
    const QMap<TStageObjectId, TStageObjectId>& idTable,
    const QMap<TFx*, TFx*>& fxTable) {
  // return if the preference option is off
  bool on =
      Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled();
  if (!on) return;

  // 1. create 3 tables for replacing; column indices, parameter pointers, and
  // expression texts. Note that moving columns in the same xsheet does not need
  // to replace the parameter pointers since they are swapped along with
  // columns.
  QMap<int, int> colIdReplaceTable;
  QMap<TDoubleParam*, TDoubleParam*> curveReplaceTable;
  std::map<std::string, std::string> exprReplaceTable;

  bool sameXSheet = (fromXsh == toXsh);

  // First, check the stage objects
  for (auto obj_itr = idTable.constBegin(); obj_itr != idTable.constEnd();
       obj_itr++) {
    TStageObjectId fromId = obj_itr.key();
    TStageObjectId toId   = obj_itr.value();

    // register column indices replacement table ( register even if fromId and
    // toId are identical )
    if (fromId.isColumn() && toId.isColumn())
      colIdReplaceTable.insert(fromId.getIndex(), toId.getIndex());
    // register expression texts replacement table ( register only if the
    // phrases will be changed )
    if (fromId != toId) {
      for (int ph = 0; ph < getPhraseCount(fromId); ph++)
        exprReplaceTable[getPhrase(fromId, ph)] = getPhrase(toId, ph);
    }
    if (sameXSheet) {
      // the parameter pointers are already swapped when moving columns in the
      // same xsheet curveReplaceTable will be used just for parameter list to
      // be modified
      TStageObject* toObj =
          toXsh->getStageObjectTree()->getStageObject(toId, false);
      assert(toObj);
      if (toObj) {
        for (int c = 0; c < TStageObject::T_ChannelCount; c++) {
          TDoubleParam* to_p = toObj->getParam((TStageObject::Channel)c);
          curveReplaceTable.insert(to_p, to_p);
        }
      }
    } else {  // for transferring objects over xsheets (i.e. collapse and
              // explode)
      // register to the parameter pointer replacement table
      TStageObject* fromObj =
          fromXsh->getStageObjectTree()->getStageObject(fromId, false);
      TStageObject* toObj =
          toXsh->getStageObjectTree()->getStageObject(toId, false);
      assert(fromObj && toObj);
      if (fromObj && toObj) {
        for (int c = 0; c < TStageObject::T_ChannelCount; c++) {
          TDoubleParam* from_p = fromObj->getParam((TStageObject::Channel)c);
          TDoubleParam* to_p   = toObj->getParam((TStageObject::Channel)c);
          curveReplaceTable.insert(from_p, to_p);
        }
      }
    }
  }

  // Secondly, check the Fxs
  QMap<TFx*, QList<TDoubleParam*>> fromFxParams, toFxParams;
  for (auto fx_itr = fxTable.constBegin(); fx_itr != fxTable.constEnd();
       fx_itr++) {
    TFx* fromFx = fx_itr.key();
    TFx* toFx   = fx_itr.value();
    // skip the case that the Xsheet node is converted to the OverFx when
    // exploding
    if (fromFx->getFxType() == toFx->getFxType()) {
      fromFxParams.insert(fromFx, QList<TDoubleParam*>());
      toFxParams.insert(toFx, QList<TDoubleParam*>());
      // register expression texts replacement table
      if (fromFx->getFxId() != toFx->getFxId())
        exprReplaceTable[getPhrase(fromFx)] = getPhrase(toFx);
    }
  }
  if (!fromFxParams.isEmpty() && !toFxParams.isEmpty()) {
    // gather from-fx parameter pointers
    m_model->refreshData(fromXsh);
    for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
      FxChannelGroup* fcg =
          dynamic_cast<FxChannelGroup*>(m_model->getFxChannel(i));
      if (!fcg) continue;
      if (fromFxParams.contains(fcg->getFx()))
        gatherParams(fcg, fromFxParams[fcg->getFx()]);
    }
    // gather to-fx parameter pointers
    m_model->refreshData(toXsh);
    for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
      FxChannelGroup* fcg =
          dynamic_cast<FxChannelGroup*>(m_model->getFxChannel(i));
      if (!fcg) continue;
      if (toFxParams.contains(fcg->getFx()))
        gatherParams(fcg, toFxParams[fcg->getFx()]);
    }
    // register parameters to the table
    for (auto ffp_itr = fromFxParams.constBegin();
         ffp_itr != fromFxParams.constEnd(); ffp_itr++) {
      TFx* fromFx = ffp_itr.key();
      TFx* toFx   = fxTable.value(fromFx);
      assert(toFx && toFxParams.contains(toFx));
      for (int i = 0; i < ffp_itr.value().size(); i++) {
        curveReplaceTable.insert(ffp_itr.value().at(i),
                                 toFxParams.value(toFx).at(i));
      }
    }
  }

  // 2. transfer reference information from fromXsh to toXsh by using tables
  // QMap<int, int> colIdReplaceTable;
  // QMap<TDoubleParam*, TDoubleParam*> curveReplaceTable;
  // std::map<std::string, std::string> exprReplaceTable;
  QSet<TDoubleParam*> insertedCurves;
  for (auto itr = info(fromXsh).begin(); itr != info(fromXsh).end(); itr++) {
    TDoubleParam* fromParam = itr.key();
    bool ignored            = touchInfo(fromParam, fromXsh).ignored();
    if (sameXSheet) {
      // transfer as-is if the parameter is ignored
      if (!ignored) {
        // converting the column indices.
        QSet<int> convertedColIdSet;
        for (auto fromId : itr.value().colRefMap()) {
          if (colIdReplaceTable.contains(fromId))
            convertedColIdSet.insert(colIdReplaceTable.value(fromId));
          // if there is a index not in the replacement table, transfer it
          // as-is.
          else
            convertedColIdSet.insert(fromId);
        }
        // replacing the info
        itr.value().colRefMap() = convertedColIdSet;
      }
      insertedCurves.insert(fromParam);
    }
    // if the parameter is in the replacement table
    else if (curveReplaceTable.contains(fromParam)) {
      // transfer as-is if the parameter is ignored
      // converting the column indices.
      QSet<int> convertedColIdSet;
      for (auto fromId : itr.value().colRefMap()) {
        if (ignored) break;
        if (colIdReplaceTable.contains(fromId))
          convertedColIdSet.insert(colIdReplaceTable.value(fromId));
        // if there is a index not in the replacement table, the parameter will
        // be ignored
        else
          ignored = true;
      }

      // converting the parameter pointers
      QSet<TDoubleParam*> convertedParamSet;
      for (auto fromRefParam : itr.value().paramRefMap()) {
        if (curveReplaceTable.contains(fromRefParam))
          convertedParamSet.insert(curveReplaceTable.value(fromRefParam));
        // if there is a index not in the replacement table, the parameter will
        // be ignored
        else
          ignored = true;
      }

      // register the converted list to toXsh
      TDoubleParam* toParam = curveReplaceTable.value(fromParam);
      if (ignored) {
        touchInfo(toParam, toXsh).ignored() = true;
        // if the parameter is ignored, transfer the column reference list
        // as-is.
        touchInfo(toParam, toXsh).colRefMap() = itr.value().colRefMap();
      } else
        touchInfo(toParam, toXsh).colRefMap() = convertedColIdSet;

      touchInfo(toParam, toXsh).paramRefMap() = convertedParamSet;

      insertedCurves.insert(toParam);
    }

    // update parameter names
    if (curveReplaceTable.contains(fromParam)) {
      TDoubleParam* toParam               = curveReplaceTable.value(fromParam);
      FunctionTreeModel::Channel* channel = nullptr;
      // here m_model should be refreshed using toXsh
      for (int i = 0; i < m_model->getStageObjectsChannelCount(); i++) {
        channel = findChannel(toParam, m_model->getStageObjectChannel(i));
        if (channel) break;
      }
      if (!channel) {
        for (int i = 0; i < m_model->getFxsChannelCount(); i++) {
          channel = findChannel(toParam, m_model->getFxChannel(i));
          if (channel) break;
        }
      }
      if (channel) {
        touchInfo(toParam, toXsh).name() = channel->getLongName();
      }
    }
  }

  // refresh m_model with the current xsheet
  if (toXsh != TApp::instance()->getCurrentXsheet()->getXsheet())
    m_model->refreshData(TApp::instance()->getCurrentXsheet()->getXsheet());

  // 3. replace the expression texts
  for (auto ic : insertedCurves) replaceExpressionTexts(ic, exprReplaceTable);
}

//----------------------------------------------------------------------------
// open warning popup if there is any parameters which is ignored (i.e. the
// reference is lost and user hasn't touch yet)
bool ExpressionReferenceManager::askIfParamIsIgnoredOnSave(bool saveSubXsheet) {
  // return if the preference option is off
  bool on =
      Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled();
  if (!on) return true;
  QSet<TXsheet*> xsheetSet;
  TXsheet* parentXsh;
  if (saveSubXsheet)  // check only inside the current xsheet
    parentXsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  else  // check whole xsheets from the top
    parentXsh = TApp::instance()->getCurrentScene()->getScene()->getTopXsheet();

  gatherXsheets(parentXsh, xsheetSet);

  // gather the ignored parameter names
  QStringList ignoredParamNames;
  for (auto xsh : xsheetSet) {
    bool isParent = (xsh == parentXsh);
    for (auto itr = info(xsh).begin(); itr != info(xsh).end(); itr++) {
      if (!itr.value().ignored()) continue;
      QString paramName = itr.value().name();
      if (!isParent) paramName += "  " + tr("(In a sub xsheet)");
      ignoredParamNames.append(paramName);
    }
  }

  // return if there is not ignored parameters
  if (ignoredParamNames.isEmpty()) return true;

  // open warning popup
  QString warningTxt =
      tr("Following parameters may contain broken references in expressions:");
  warningTxt += "\n  " + ignoredParamNames.join("\n  ");
  warningTxt += "\n" + tr("Do you want to save the scene anyway ?");

  int ret =
      DVGui::MsgBox(warningTxt, QObject::tr("Save"), QObject::tr("Cancel"), 0);
  if (ret == 0 || ret == 2) return false;

  return true;
}