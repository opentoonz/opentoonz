#pragma once

#ifndef STAGEOBJECTSELECTION_H
#define STAGEOBJECTSELECTION_H

#include "toonzqt/selection.h"
#include "toonz/tstageobjectid.h"
#include "toonz/txshcolumn.h"
#include "tgeometry.h"
#include <QList>
#include <QPair>

// forward declaration
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TFxHandle;
class SchematicLink;
class SchematicPort;

//=========================================================
//
// FxSelection
//
//---------------------------------------------------------

class StageObjectSelection final : public QObject, public TSelection {
  Q_OBJECT

  QList<QPair<TStageObjectId, TStageObjectId>> m_selectedLinks;
  QList<TStageObjectId> m_selectedObjects;
  QList<int> m_selectedSplines;
  TXsheetHandle *m_xshHandle;
  TObjectHandle *m_objHandle;
  TColumnHandle *m_colHandle;
  TFxHandle *m_fxHandle;
  TPointD m_pastePosition;

public:
  StageObjectSelection();
  StageObjectSelection(const StageObjectSelection &src);
  ~StageObjectSelection();

  void enableCommands() override;

  bool isEmpty() const override {
    return m_selectedObjects.empty() && m_selectedLinks.empty() &&
           m_selectedSplines.empty();
  }
  void setPastePosition(const TPointD &pos) { m_pastePosition = pos; };

  void selectNone() override {
    m_selectedObjects.clear();
    m_selectedLinks.clear();
    m_selectedSplines.clear();
  }
  void deleteSelection();
  void groupSelection();
  void ungroupSelection();
  void collapseSelection();
  void explodeChild();
  void copySelection();
  void pasteSelection();
  void cutSelection();

  void select(const TStageObjectId &id);
  void unselect(const TStageObjectId &id);
  void select(int id);
  void unselect(int id);
  void select(SchematicLink *link);
  void unselect(SchematicLink *link);

  bool isSelected(const TStageObjectId &id) const;
  bool isSelected(SchematicLink *link);

  const QList<TStageObjectId> &getObjects() const { return m_selectedObjects; }
  const QList<QPair<TStageObjectId, TStageObjectId>> &getLinks() const {
    return m_selectedLinks;
  }
  const QList<int> &getSplines() const { return m_selectedSplines; }

  void setXsheetHandle(TXsheetHandle *xshHandle) { m_xshHandle = xshHandle; }
  void setObjectHandle(TObjectHandle *objHandle) { m_objHandle = objHandle; }
  void setColumnHandle(TColumnHandle *colHandle) { m_colHandle = colHandle; }
  void setFxHandle(TFxHandle *fxHandle) { m_fxHandle = fxHandle; }

  // return true if objects in m_selectedObjects makes a connected graph;
  bool isConnected() const;

private:
  // not implemented
  StageObjectSelection &operator=(const StageObjectSelection &);

  QPair<TStageObjectId, TStageObjectId> getBoundingObjects(SchematicLink *link);
  QPair<TStageObjectId, TStageObjectId> getBoundingObjects(
      SchematicPort *inputPort, SchematicPort *outputPort);

signals:
  void doCollapse(QList<TStageObjectId>);
  void doExplodeChild(QList<TStageObjectId>);
  void doDelete();
  void columnPasted(const QList<TXshColumnP> &);
};

#endif
