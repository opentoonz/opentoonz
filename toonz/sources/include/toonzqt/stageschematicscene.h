#pragma once

#ifndef STAGESCHEMATIC_H
#define STAGESCHEMATIC_H

#include "schematicviewer.h"
#include "tgeometry.h"
#include <QMap>

// forward declaration
class TStageObject;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TSceneHandle;
class TFrameHandle;
class TXsheet;
class StageSchematicNode;
class StageSchematicSplineNode;
class TStageObjectSpline;
class StageObjectSelection;
class SchematicLink;
class StageSchematicGroupNode;
class StageSchematicGroupEditor;

//==================================================================
//
// TreeStageNode
//
//==================================================================
//! Provide a structure representing a node af a tree.
//! This class mantains information about a node of the stage schematic and its
//! children.
//! The tree is generated using the TStageObjectTree.
//! The generated tree is used to oreder stage schematic nodes in the window.
class TreeStageNode {
  //! The node to replace in the window
  StageSchematicNode *m_node;
  //! The childre of the node
  std::vector<TreeStageNode *> m_cildren;

public:
  TreeStageNode(StageSchematicNode *node) : m_node(node) {}

  ~TreeStageNode() {
    int i;
    for (i = 0; i < (int)m_cildren.size(); i++) {
      delete m_cildren[i];
    }
  }
  //--------------------------------------

  void setSchematicNode(StageSchematicNode *node) { m_node = node; }
  void addChild(TreeStageNode *child) { m_cildren.push_back(child); }

  StageSchematicNode *getNode() { return m_node; }
  int getChildrenCount() { return m_cildren.size(); }
  TreeStageNode *getChildTreeNode(int i) { return m_cildren[i]; }
  //! Sort the children container.
  //! The sorting follows this rules:\n
  //! \li TableNode < CameraNode < PegbarNode < ColumnNode
  //! \li if two nodes are of the same type, they are odered using indexes
  void sortChildren(int startIndex, int lastIndex);
  void reverseChildren() { std::reverse(m_cildren.begin(), m_cildren.end()); }
};

//==================================================================
//
// StageSchematic
//
//==================================================================

//! It is the implementation of the SchematicScene representig the
//! QGraphicsScene for the Stage Schematic.
//! All StageSchematicNode,  StageSchematicSplineNode and link are pleced and
//! handled in this scene.
//! The scene mantains two mapping:
//! one for StageSchematicNode and one for StageSchematicSplineNode. These
//! mapping can be used to retrieve
//! nodes and spline.\n
//! To regenearte the scene use the updateScene().\n
//! To oreder nodes in the scene use the reorderScene().

class StageSchematicScene final : public SchematicScene {
  Q_OBJECT
  TXsheetHandle *m_xshHandle;
  TObjectHandle *m_objHandle;
  TColumnHandle *m_colHandle;
  TSceneHandle *m_sceneHandle;
  TFrameHandle *m_frameHandle;
  TPointD m_nextNodePos;
  TPointD m_firstPos;
  QMap<TStageObjectId, StageSchematicNode *> m_nodeTable;
  QMap<int, StageSchematicGroupNode *> m_GroupTable;
  QMap<int, StageSchematicGroupEditor *> m_groupEditorTable;
  QMap<TStageObjectSpline *, StageSchematicSplineNode *> m_splineTable;
  StageObjectSelection *m_selection;
  int m_gridDimension;

  QMap<TStageObjectId, QList<StageSchematicNode *>> m_nodesToPlace;

  bool m_showLetterOnPortFlag;

  SchematicViewer *m_viewer;

public:
  StageSchematicScene(QWidget *parent);
  ~StageSchematicScene();

  //! Clear all item an regenerate the Stage Schematic graph.
  void updateScene() override;

  //! Reurns the current node.
  QGraphicsItem *getCurrentNode() override;

  //! Replace all nodes in the scene.
  void reorderScene() override;
  TXsheet *getXsheet();
  TXsheetHandle *getXsheetHandle() { return m_xshHandle; }

  void setXsheetHandle(TXsheetHandle *xshHandle);
  void setObjectHandle(TObjectHandle *objHandle);
  void setColumnHandle(TColumnHandle *colHandle);
  void setFxHandle(TFxHandle *fxHandle);
  void setSceneHandle(TSceneHandle *sceneHandle) {
    m_sceneHandle = sceneHandle;
  }
  void setFrameHandle(TFrameHandle *frameHandle) {
    m_frameHandle = frameHandle;
  }
  TStageObjectId getCurrentObject();
  TFrameHandle *getFrameHandle() { return m_frameHandle; }
  TColumnHandle *getColumnHandle() { return m_colHandle; }
  void updateNestedGroupEditors(StageSchematicNode *node,
                                const QPointF &newPos);
  void resizeNodes(bool maximizedNode);

  bool isShowLetterOnPortFlagEnabled() { return m_showLetterOnPortFlag; }

  SchematicViewer *getSchematicViewer() { return m_viewer; }

private:
  StageSchematicNode *addStageSchematicNode(TStageObject *pegbar);
  StageSchematicGroupNode *addStageGroupNode(QList<TStageObject *> objs);
  StageSchematicSplineNode *addSchematicSplineNode(TStageObjectSpline *spline);
  StageSchematicGroupEditor *addEditedGroupedStageSchematicNode(
      int groupId, const QList<SchematicNode *> &groupedObjs);
  StageSchematicNode *createStageSchematicNode(StageSchematicScene *scene,
                                               TStageObject *pegbar);

  //! Places all nodes in the window
  void placeNodes();

  //! Search nodes in the TStageObjectTree that are roots of a tree.
  //! Roots are placed in the container \b roots
  void findRoots(std::vector<TreeStageNode *> &roots);

  //! Build the tree starting from \b treenode.
  //! The tree is build recursively on each children of \b treeNode.
  void makeTree(TreeStageNode *treeNode);

  //! Give the right position to the \b treeNode children.
  //! All nodes of the stage schematic are replaced recursively on each children
  //! starting from a root.
  //! \b xPos and \b yPos are update from the method.
  //! \sa findRoots(vector<TreeNode*> &roots)
  void placeChildren(TreeStageNode *treeNode, double &xPos, double &yPos,
                     bool isCameraTree = false);

  //! Place a StageSchematicNode in the window.
  //! It is usually used to place new nodes that have not position.
  void placeNode(StageSchematicNode *node);

  //! Place a StageSchematicSplineNode in the window.
  //! It is usually used to place new splines that have not position.
  void placeSplineNode(StageSchematicSplineNode *splineNode);

  SchematicNode *getNodeFromPosition(const QPointF &pos);

  void updatePosition(StageSchematicNode *node, const TPointD &pos);
  void highlightLinks(StageSchematicNode *node, bool value);
  void updateEditedGroups(const QMap<int, QList<SchematicNode *>> &editedGroup);
  void updatePositionOnResize(TStageObject *obj, bool maximizedNode);
  void updateSplinePositionOnResize(TStageObjectSpline *spl,
                                    bool maximizedNode);

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;

signals:
  void editObject();
  void doCollapse(QList<TStageObjectId>);
  void doExplodeChild(QList<TStageObjectId>);

protected slots:
  void onSelectionSwitched(TSelection *oldSel, TSelection *newSel) override;

  void onPegbarAdded();
  void onSplineAdded();
  void onCameraAdded();
  void onResetCenter();
  void onCameraActivate();
  void onRemoveSpline();
  void onSaveSpline();
  void onLoadSpline();

  void onPathToggled(int state);
  void onCpToggled(bool toggled);

  void onXsheetChanged();
  void onSceneChanged();
  void onCurrentObjectChanged(const TStageObjectId &, bool);
  void onCurrentColumnChanged(int);
  void onSelectionChanged();
  void onCollapse(QList<TStageObjectId>);
  void onEditGroup();

  void onSwitchPortModeToggled(bool withLetter);
};

#endif  // STAGESCHEMATIC_H
