#pragma once

#ifndef STAGESCHEMATICNODE_H
#define STAGESCHEMATICNODE_H

#include "schematicnode.h"

// forward declarations
class StageSchematicScene;
class StageSchematicNode;
class StageSchematicColumnNode;
class StageSchematicNodeDock;
class TStageObject;
class QPixmap;
class QRegExpValidator;
class TStageObjectId;
class TStageObjectSpline;
class StageSchematicSplineDock;
class StageSchematicColumnNode;
class StageSchematicPegbarNode;
class StageSchematicSplineNode;
class StageSchematicCameraNode;
class StageSchematicTableNode;
class StageSchematicGroupNode;
class QTimer;

enum eStageSchematicPortType {
  eStageSplinePort      = 100,
  eStageParentPort      = 101,
  eStageChildPort       = 102,
  eStageSplineGroupPort = 103,
  eStageParentGroupPort = 104,
  eStageChildGroupPort  = 105
};

//========================================================
//
// ColumnPainter
//
//========================================================

class ColumnPainter final : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  StageSchematicColumnNode *m_parent;
  double m_width, m_height;
  QString m_name;
  int m_type;
  bool m_isReference = false;

public:
  ColumnPainter(StageSchematicColumnNode *parent, double width, double height,
                const QString &name);
  ~ColumnPainter();
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setName(const QString &name) { m_name = name; }
  void setIsReference(bool ref = true) { m_isReference = ref; }

  QLinearGradient getGradientByLevelType(int type);

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;

public slots:
  void onIconGenerated();
};

//========================================================
//
// GroupPainter
//
//========================================================

class GroupPainter final : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  StageSchematicGroupNode *m_parent;
  double m_width, m_height;
  QString m_name;

public:
  GroupPainter(StageSchematicGroupNode *parent, double width, double height,
               const QString &name);
  ~GroupPainter();
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setName(const QString &name) { m_name = name; }

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
};

//========================================================
//
// PegbarPainter
//
//========================================================

class PegbarPainter final : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  StageSchematicPegbarNode *m_parent;
  double m_width, m_height;
  QString m_name;

public:
  PegbarPainter(StageSchematicPegbarNode *parent, double width, double height,
                const QString &name);
  ~PegbarPainter();
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setName(const QString &name) { m_name = name; }

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
};

//========================================================
//
// CameraPainter
//
//========================================================

class CameraPainter final : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  StageSchematicCameraNode *m_parent;
  double m_width, m_height;
  QString m_name;
  bool m_isActive;

public:
  CameraPainter(StageSchematicCameraNode *parent, double width, double height,
                const QString &name);
  ~CameraPainter();
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setName(const QString &name) { m_name = name; }

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
};

//========================================================
//
// TablePainter
//
//========================================================

class TablePainter final : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  StageSchematicTableNode *m_parent;
  double m_width, m_height;

public:
  TablePainter(StageSchematicTableNode *parent, double width, double height);
  ~TablePainter();
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
};

//========================================================
//
// SplinePainter
//
//========================================================

class SplinePainter final : public QObject, public QGraphicsItem {
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  StageSchematicSplineNode *m_parent;
  double m_width, m_height;
  QString m_name;

public:
  SplinePainter(StageSchematicSplineNode *parent, double width, double height,
                const QString &name);
  ~SplinePainter();
  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setName(const QString &name) { m_name = name; }

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
};

//========================================================
//
// StageSchematicNodePort
//
//========================================================

class StageSchematicNodePort final : public SchematicPort {
  QString m_handle;

public:
  StageSchematicNodePort(StageSchematicNodeDock *parent, int type);
  ~StageSchematicNodePort();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setHandle(const QString &value) { m_handle = value; }
  QString getHandle() { return m_handle; }

  bool linkTo(SchematicPort *port, bool checkOnly = false) override;

private:
  SchematicPort *searchPort(const QPointF &scenePos) override;
  void hideSnappedLinks(SchematicPort *linkingPort) override;
  void showSnappedLinks(SchematicPort *linkingPort) override;
};

//========================================================
//
// StageSchematicSplinePort
//
//========================================================

class StageSchematicSplinePort final : public SchematicPort {
  StageSchematicSplineDock *m_parent;
  QPixmap m_squarePixmap, m_rhombPixmap;

public:
  StageSchematicSplinePort(StageSchematicSplineDock *parent, int type);
  ~StageSchematicSplinePort();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  bool linkTo(SchematicPort *port, bool checkOnly = false) override;

private:
  SchematicPort *searchPort(const QPointF &scenePos) override;
  void hideSnappedLinks(SchematicPort *) override;
  void showSnappedLinks(SchematicPort *) override;
};

//========================================================
//
// SplineAimChanger
//
//========================================================

class SplineAimChanger final : public SchematicHandleSpinBox {
  bool m_aim;

public:
  SplineAimChanger(QGraphicsItem *parent);
  ~SplineAimChanger();
  void setAim(bool aim) { m_aim = aim; }
  bool getAim() { return m_aim; }

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent *me) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *me) override;
};

//========================================================
//
// StageSchematicNodeDock
//
//========================================================

class StageSchematicNodeDock final : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif

  StageSchematicNode *m_parent;
  StageSchematicNodePort *m_port;
  SchematicHandleSpinBox *m_handleSpinBox;

  bool m_isParentPort;
  QTimer *m_timer;

public:
  StageSchematicNodeDock(StageSchematicNode *parent, bool isParentPort,
                         eStageSchematicPortType type);
  ~StageSchematicNodeDock();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

  StageSchematicNodePort *getPort() { return m_port; }
  StageSchematicNode *getNode() { return m_parent; }
  bool isParentPort() { return m_isParentPort; }

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent *he) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *he) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent *he) override;

private:
  // void updateHandle(bool increase);
  // void moveZValueLinks(int value);

signals:
  void sceneChanged();

protected slots:
  void onPortClicked();
  void onPortReleased(const QPointF &);
  void onTimeOut();
  void onModifyHandle(int);
};

//========================================================
//
// StageSchematicSplineDock
//
//========================================================

class StageSchematicSplineDock final : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif
  SchematicNode *m_parent;
  StageSchematicSplinePort *m_port;
  bool m_isParentPort;

public:
  StageSchematicSplineDock(SchematicNode *parent, bool isParentPort,
                           eStageSchematicPortType type);
  ~StageSchematicSplineDock();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

  StageSchematicSplinePort *getPort() { return m_port; }
  SchematicNode *getNode() { return m_parent; }
  bool isParentPort() { return m_isParentPort; }

signals:
  void sceneChanged();
};

//========================================================
//
// class StageSchematicNode
//
//========================================================

class StageSchematicNode : public SchematicNode {
  Q_OBJECT

protected:
  TStageObject *m_stageObject;
  StageSchematicNodeDock *m_parentDock;
  QList<StageSchematicNodeDock *> m_childDocks;
  StageSchematicSplineDock *m_splineDock;
  SchematicToggle *m_pathToggle, *m_cpToggle;
  bool m_isGroup;
  QString m_name;
  SchematicName *m_nameItem;

public:
  StageSchematicNode(StageSchematicScene *scene, TStageObject *obj, int width,
                     int height, bool isGroup = false);
  ~StageSchematicNode();

  void setWidth(const qreal &width) { m_width = width; }
  void setHeight(const qreal &height) { m_height = height; }
  void setSchematicNodePos(const QPointF &pos) const override;
  bool isNameEditing() { return m_nameItem->isVisible(); }
  void onClicked() override;

  int getChildCount() { return m_childDocks.size(); }
  StageSchematicNodePort *getChildPort(int i) {
    return m_childDocks[i]->getPort();
  }
  StageSchematicNodePort *getParentPort() { return m_parentDock->getPort(); }

  TStageObject *getStageObject() { return m_stageObject; }
  StageSchematicNodePort *makeChildPort(const QString &label);
  StageSchematicNodePort *makeParentPort(const QString &label);
  virtual void updateChildDockPositions();  // TODO: commento! doxygen
  void setPosition(const QPointF &newPos) override;

signals:
  void currentObjectChanged(const TStageObjectId &id, bool isSpline);
  void currentColumnChanged(int index);
  void editObject();

protected slots:
  void onHandleReleased();
};

//========================================================
//
// class StageSchematicPegbarNode
//
//========================================================

class StageSchematicPegbarNode final : public StageSchematicNode {
  Q_OBJECT

  PegbarPainter *m_pegbarPainter;

public:
  StageSchematicPegbarNode(StageSchematicScene *scene, TStageObject *pegbar);
  ~StageSchematicPegbarNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *me) override;

protected slots:
  void onNameChanged();
};

//========================================================
//
// class SchematicTableNode
//
//========================================================

class StageSchematicTableNode final : public StageSchematicNode {
  TablePainter *m_tablePainter;

public:
  StageSchematicTableNode(StageSchematicScene *scene, TStageObject *pegbar);
  ~StageSchematicTableNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
};

//========================================================
//
// class SchematicColumnNode
//
//========================================================

class StageSchematicColumnNode final : public StageSchematicNode {
  Q_OBJECT

  SchematicThumbnailToggle *m_resizeItem;
  SchematicToggle *m_renderToggle, *m_cameraStandToggle;
  ColumnPainter *m_columnPainter;
  bool m_isOpened;

public:
  StageSchematicColumnNode(StageSchematicScene *scene, TStageObject *pegbar);
  ~StageSchematicColumnNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  QPixmap getPixmap();
  bool isOpened() { return m_isOpened; }
  void resize(bool maximized);

  void getLevelTypeAndName(int &, QString &);

private:
  void updatePortsPosition();

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *me) override;

protected slots:
  void onNameChanged();
  void onChangedSize(bool expand);
  void onRenderToggleClicked(bool isActive);
  void onCameraStandToggleClicked(int state);
};

//========================================================
//
// class SchematicCameraNode
//
//========================================================

class StageSchematicCameraNode final : public StageSchematicNode {
  Q_OBJECT

  CameraPainter *m_cameraPainter;

public:
  StageSchematicCameraNode(StageSchematicScene *scene, TStageObject *pegbar);
  ~StageSchematicCameraNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *me) override;

protected slots:
  void onNameChanged();
};

//========================================================
//
// class StageSchematicSplineNode
//
//========================================================

class StageSchematicSplineNode final : public SchematicNode {
  Q_OBJECT

  TStageObjectSpline *m_spline;
  QString m_splineName;
  SchematicName *m_nameItem;
  SchematicThumbnailToggle *m_resizeItem;
  StageSchematicSplineDock *m_dock;
  SplinePainter *m_splinePainter;
  bool m_isOpened;

public:
  StageSchematicSplineNode(StageSchematicScene *scene,
                           TStageObjectSpline *spline);
  ~StageSchematicSplineNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void onClicked() override;

  void setSchematicNodePos(const QPointF &pos) const override;
  TStageObjectSpline *getSpline() { return m_spline; }
  bool isNameEditing() { return m_nameItem->isVisible(); }
  bool isOpened() { return m_isOpened; }
  QPixmap getPixmap();
  StageSchematicSplinePort *getParentPort() { return m_dock->getPort(); }
  void setPosition(const QPointF &newPos) override { setPos(newPos); }
  void resize(bool maximized);

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *me) override;

signals:
  void currentObjectChanged(const TStageObjectId &id, bool isSpline);

protected slots:
  void onNameChanged();
  void onChangedSize(bool expanded);
};

//========================================================
//
// class StageSchematicSplineNode
//
//========================================================

class StageSchematicGroupNode final : public StageSchematicNode {
  Q_OBJECT

  GroupPainter *m_painter;
  QList<TStageObject *> m_groupedObj;
  TStageObject *m_root;
  bool m_isOpened;

public:
  StageSchematicGroupNode(StageSchematicScene *scene, TStageObject *root,
                          const QList<TStageObject *> groupedObj);
  ~StageSchematicGroupNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  QList<TStageObject *> getGroupedObjects() { return m_groupedObj; }
  int getGroupId();
  TStageObject *getRoot() { return m_root; }
  void updateObjsDagPosition(const TPointD &pos) const;
  bool isOpened() { return m_isOpened; }
  void updatePortsPosition();
  void resize(bool maximized);

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *me) override;

protected slots:
  void onNameChanged();
  void onChangedSize(bool expanded);
};

#endif  // STAGESCHEMATICNODE_H
