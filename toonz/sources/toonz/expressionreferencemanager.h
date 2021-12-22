#pragma once

#ifndef EXPRESSIONREFERENCEMANAGER_H
#define EXPRESSIONREFERENCEMANAGER_H

#include "tparamchange.h"
#include "toonzqt/treemodel.h"
#include "toonzqt/functiontreeviewer.h"
#include "toonz/txsheetcolumnchange.h"
#include "toonz/tstageobjectid.h"
#include "toonz/expressionreferencemonitor.h"
#include <QObject>
#include <QMap>
#include <QSet>
#include <QList>

class TDoubleParam;
class TFx;

class ExpressionReferenceManager : public QObject,
                                   public TXsheetColumnChangeObserver,
                                   public TParamObserver {  // singleton
  Q_OBJECT

  FunctionTreeModel* m_model;
  // block to run onChange() due to keyframe change caused by itself
  bool m_blockParamChange;

  ExpressionReferenceMonitor* currentMonitor();

  QMap<TDoubleParam*, ExpressionReferenceMonitorInfo>& info(
      TXsheet* xsh = nullptr);

  ExpressionReferenceMonitorInfo& touchInfo(TDoubleParam* param,
                                            TXsheet* xsh = nullptr);

  ExpressionReferenceManager();

  void checkRef(TreeModel::Item* item, TXsheet* xsh = nullptr);
  FunctionTreeModel::Channel* findChannel(TDoubleParam* param,
                                          TreeModel::Item* item);

  void gatherParams(TreeModel::Item* item, QList<TDoubleParam*>&);
  bool refreshParamsRef(TDoubleParam* curve, TXsheet* xsh = nullptr);

  void replaceExpressionTexts(
      TDoubleParam* curve, const std::map<std::string, std::string> replaceMap,
      TXsheet* xsh = nullptr);

  bool doCheckReferenceDeletion(
      const QSet<int>& columnIdsToBeDeleted, const QSet<TFx*>& fxsToBeDeleted,
      const QList<TStageObjectId>& objectIdsToBeDeleted,
      const QList<TStageObjectId>& objIdsToBeDuplicated,
      bool checkInvert = false);

public:
  static ExpressionReferenceManager* instance();
  void onChange(const TXsheetColumnChange&) override;
  void onFxAdded(const std::vector<TFx*>&) override;
  void onStageObjectAdded(const TStageObjectId) override;
  bool isIgnored(TDoubleParam*) override;

  void onChange(const TParamChange&) override;

  void init();

  bool checkReferenceDeletion(const QSet<int>& columnIdsToBeDeleted,
                              const QSet<TFx*>& fxsToBeDeleted,
                              const QList<TStageObjectId>& objIdsToBeDuplicated,
                              bool checkInvert);
  bool checkReferenceDeletion(
      const QList<TStageObjectId>& objectIdsToBeDeleted);
  bool checkExplode(TXsheet* childXsh, int index, bool removeColumn,
                    bool columnsOnly);

  void transferReference(TXsheet* fromXsh, TXsheet* toXsh,
                         const QMap<TStageObjectId, TStageObjectId>& idTable,
                         const QMap<TFx*, TFx*>& fxTable);

  bool askIfParamIsIgnoredOnSave(bool saveSubXsheet);

  void refreshXsheetRefInfo(TXsheet* xsh);
protected slots:
  void onSceneSwitched();
  void onXsheetSwitched();
  void onXsheetChanged();
  void onPreferenceChanged(const QString& prefName);
};

#endif