#pragma once

#ifndef SCHEMATICGROUPEDITOR_H
#define SCHEMATICGROUPEDITOR_H

#include "tcommon.h"
#include <QGraphicsItem>
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QGraphicsScene;
class SchematicNode;
class FxSchematicNode;
class SchematicScene;
class TFx;
class TMacroFx;
class SchematicName;

//=====================================================
//
// SchematicGroupEditor
//
//=====================================================

class DVAPI SchematicWindowEditor : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  QPointF m_lastPos;

protected:
  QList<SchematicNode *> m_groupedNode;
  QString m_groupName;
  SchematicScene *m_scene;
  SchematicName *m_nameItem;
  bool m_isMacroEditor;
  Qt::MouseButton m_button;

public:
  SchematicWindowEditor(const QList<SchematicNode *> &groupedNode,
                        SchematicScene *scene);
  ~SchematicWindowEditor();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  virtual QRectF boundingSceneRect() const      = 0;
  virtual void setGroupedNodeZValue(int zValue) = 0;
  bool contains(SchematicNode *node) const {
    return m_groupedNode.contains(node);
  }
  void resizeNodes(bool maximizeNodes);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override;

protected slots:
  virtual void closeEditor()   = 0;
  virtual void onNameChanged() = 0;

private:
  //! retrieve the group name and the group Id
  virtual void initializeEditor()                = 0;
  virtual void doResizeNodes(bool maximizeNodes) = 0;
};

//=====================================================
//
// FxSchematicGroupEditor
//
//=====================================================

class DVAPI FxSchematicGroupEditor final : public SchematicWindowEditor {
  Q_OBJECT

  int m_groupId;

public:
  FxSchematicGroupEditor(int groupId, const QList<SchematicNode *> &groupedNode,
                         SchematicScene *scene);
  ~FxSchematicGroupEditor();

  QRectF boundingSceneRect() const override;
  void setGroupedNodeZValue(int zValue) override;

protected slots:
  void closeEditor() override;
  void onNameChanged() override;

private:
  void initializeEditor() override;
  void doResizeNodes(bool maximizeNodes) override;
};

//=====================================================
//
// FxSchematicMacroEditor
//
//=====================================================

class DVAPI FxSchematicMacroEditor final : public SchematicWindowEditor {
  Q_OBJECT

  TMacroFx *m_macro;

public:
  FxSchematicMacroEditor(TMacroFx *macro,
                         const QList<SchematicNode *> &groupedNode,
                         SchematicScene *scene);
  ~FxSchematicMacroEditor();

  QRectF boundingSceneRect() const override;
  void setGroupedNodeZValue(int zValue) override;

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;

protected slots:
  void closeEditor() override;
  void onNameChanged() override;

private:
  void initializeEditor() override;
  void doResizeNodes(bool maximizeNodes) override;
};

//=====================================================
//
// StageSchematicGroupEditor
//
//=====================================================

class DVAPI StageSchematicGroupEditor final : public SchematicWindowEditor {
  Q_OBJECT

  int m_groupId;

public:
  StageSchematicGroupEditor(int groupId,
                            const QList<SchematicNode *> &groupedNode,
                            SchematicScene *scene);
  ~StageSchematicGroupEditor();

  QRectF boundingSceneRect() const override;
  void setGroupedNodeZValue(int zValue) override;

protected slots:
  void closeEditor() override;
  void onNameChanged() override;

private:
  void initializeEditor() override;
  void doResizeNodes(bool maximizeNodes) override;
};

#endif
