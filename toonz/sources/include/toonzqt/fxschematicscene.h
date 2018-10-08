#pragma once

#ifndef FXSCHEMATIC_H
#define FXSCHEMATIC_H

#include "toonzqt/addfxcontextmenu.h"
#include "schematicviewer.h"
#include "tgeometry.h"
#include <QMap>
#include <tfx.h>
#include <set>

// forward declaration
class FxSchematicNode;
class TFxHandle;
class FxSelection;
class FxSchematicNode;
class TXsheet;
class TSceneHandle;
class QMenu;
class SchematicLink;
class FxGroupNode;
class FxSchematicGroupEditor;
class FxSchematicMacroEditor;
class TMacroFx;

//==================================================================
//
// FXSchematic
//
//==================================================================

class FxSchematicScene final : public SchematicScene {
  Q_OBJECT

  //----------------------------------------------------

  class SupportLinks {
    QList<SchematicLink *> m_bridges;
    QList<SchematicLink *> m_inputs;
    QList<SchematicLink *> m_outputs;

  public:
    SupportLinks() {}

    void addBridgeLink(SchematicLink *link);
    void addInputLink(SchematicLink *link);
    void addOutputLink(SchematicLink *link);

    QList<SchematicLink *> getBridgeLinks() { return m_bridges; }
    QList<SchematicLink *> getInputLinks() { return m_inputs; }
    QList<SchematicLink *> getOutputLinks() { return m_outputs; }

    void hideBridgeLinks();
    void hideInputLinks();
    void hideOutputLinks();

    void showBridgeLinks();
    void showInputLinks();
    void showOutputLinks();

    void removeBridgeLinks(bool deleteLink = false);
    void removeInputLinks(bool deleteLink = false);
    void removeOutputLinks(bool deleteLink = false);

    bool isABridgeLink(SchematicLink *link) { return m_bridges.contains(link); }
    bool isAnInputLink(SchematicLink *link) { return m_inputs.contains(link); }
    bool isAnOutputLink(SchematicLink *link) {
      return m_outputs.contains(link);
    }

    void clearAll();
    int size();
  };

  //----------------------------------------------------

  TApplication *m_app;
  TXsheetHandle *m_xshHandle;
  TFxHandle *m_fxHandle;
  TFrameHandle *m_frameHandle;
  TColumnHandle *m_columnHandle;

  QPointF m_firstPoint;
  QMap<TFx *, FxSchematicNode *> m_table;
  QMap<int, FxGroupNode *> m_groupedTable;
  QMap<int, FxSchematicGroupEditor *> m_groupEditorTable;
  QMap<TMacroFx *, FxSchematicMacroEditor *> m_macroEditorTable;
  FxSelection *m_selection;
  AddFxContextMenu m_addFxContextMenu;
  SupportLinks m_disconnectionLinks, m_connectionLinks;
  bool m_isConnected;
  bool m_linkUnlinkSimulation;
  bool m_altPressed;
  QPointF m_lastPos;
  QList<QPair<TFxP, TPointD>> m_selectionOldPos;
  QList<TFx *> m_placedFxs;
  FxSchematicNode *m_currentFxNode;
  int m_gridDimension;

  bool m_isNormalIconView;

  QMap<TFx *, QList<FxSchematicNode *>> m_nodesToPlace;

  SchematicViewer *m_viewer;

public:
  FxSchematicScene(QWidget *parent);
  ~FxSchematicScene();

  void updateScene() override;
  QGraphicsItem *getCurrentNode() override;
  void reorderScene() override;
  TXsheet *getXsheet();

  void setApplication(TApplication *app);

  TApplication *getApplication() const { return m_app; }
  TXsheetHandle *getXsheetHandle() const { return m_xshHandle; }
  TFxHandle *getFxHandle() const { return m_fxHandle; }
  TFrameHandle *getFrameHandle() const { return m_frameHandle; }
  TColumnHandle *getColumnHandle() const { return m_columnHandle; }
  TFx *getCurrentFx();

  QMenu *getInsertFxMenu() { return m_addFxContextMenu.getInsertMenu(); }
  QMenu *getAddFxMenu() { return m_addFxContextMenu.getAddMenu(); }
  QMenu *getReplaceFxMenu() { return m_addFxContextMenu.getReplaceMenu(); }

  QAction *getAgainAction(int commands) {
    return m_addFxContextMenu.getAgainCommand(commands);
  }

  FxSelection *getFxSelection() const { return m_selection; }
  //! Disconnects or connects selected item from the rest of the graph.
  //! Selection must be a connected subgraph. If \b disconnect is true, the
  //! selection is disconnected; connected otherwise.
  void simulateDisconnectSelection(bool disconnect);

  //! Updates all Group Editors containing fx.
  //! Each fx is only in one group, but each gruop can contains othe group. All
  //! nested Groups must be updated.
  void updateNestedGroupEditors(FxSchematicNode *node, const QPointF &newPos);
  void closeInnerMacroEditor(int groupId);
  void resizeNodes(bool maximizedNode);

  void initCursorScenePos() {
    m_addFxContextMenu.setCurrentCursorScenePos(QPointF(0, 0));
  }
  void selectNodes(QList<TFxP> &fxs);
  void toggleNormalIconView() { m_isNormalIconView = !m_isNormalIconView; }
  bool isNormalIconView() { return m_isNormalIconView; }

  SchematicViewer *getSchematicViewer() { return m_viewer; }

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *me) override;
  bool event(QEvent *e) override;

private:
  FxSchematicNode *addFxSchematicNode(TFx *fx);
  FxSchematicNode *addGroupedFxSchematicNode(int groupId,
                                             const QList<TFxP> &groupedFxs);
  FxSchematicGroupEditor *addEditedGroupedFxSchematicNode(
      int groupId, const QList<SchematicNode *> &groupedFxs);
  FxSchematicMacroEditor *addEditedMacroFxSchematicNode(
      TMacroFx *macro, const QList<SchematicNode *> &groupedFxs);
  FxSchematicNode *createFxSchematicNode(TFx *fx);
  void placeNode(FxSchematicNode *node);
  void updateLink();
  void updateDuplcatedNodesLink();
  void updateEditedGroups(const QMap<int, QList<SchematicNode *>> &editedGroup);
  void updateEditedMacros(
      const QMap<TMacroFx *, QList<SchematicNode *>> &editedMacro);
  FxSchematicNode *getFxNodeFromPosition(const QPointF &pos);
  void placeNodeAndParents(TFx *fx, double x, double &maxX, double &maxY);

  QPointF nearestPoint(const QPointF &point);
  void highlightLinks(FxSchematicNode *node, bool value);
  void updatePosition(FxSchematicNode *node, const TPointD &pos);
  void simulateInsertSelection(SchematicLink *link, bool connect);
  void updatePositionOnResize(TFx *fx, bool maximizedNode);
  void removeRetroLinks(TFx *fx, double &maxX);

signals:
  void showPreview(TFxP);
  void cacheFx(TFxP);
  void doCollapse(const QList<TFxP> &);
  void doExplodeChild(const QList<TFxP> &);
  void editObject();

protected slots:
  void onSelectionSwitched(TSelection *oldSel, TSelection *newSel) override;
  void onSelectionChanged();

  // void onOutputFxAdded();
  void onDisconnectFromXSheet();
  void onConnectToXSheet();
  void onDeleteFx();
  void onDuplicateFx();
  void onUnlinkFx();
  void onMacroFx();
  void onExplodeMacroFx();
  void onOpenMacroFx();
  void onSavePresetFx();
  void onRemoveOutput();
  void onActivateOutput();
  void onPreview();
  void onCacheFx();
  void onUncacheFx();
  void onCollapse(const QList<TFxP> &);

  void onXsheetChanged();
  void onSceneChanged();
  void onSwitchCurrentFx(TFx *);
  void onCurrentFxSwitched();
  void onCurrentColumnChanged(int);

  void onFxNodeDoubleClicked();

  void onInsertPaste();
  void onAddPaste();
  void onReplacePaste();
  void onAltModifierChanged(bool);
  void onEditGroup();

  void onIconifyNodesToggled(bool iconified);

private:
  void setEnableCache(bool toggle);

  // not implemented
  FxSchematicScene(const FxSchematicScene &);
  const FxSchematicScene &operator=(const FxSchematicScene &);
};

#endif  // FXSCHEMATIC_H
