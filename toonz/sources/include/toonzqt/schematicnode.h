#pragma once

#ifndef SCHEMATICNODE_H
#define SCHEMATICNODE_H

#include <QGraphicsItem>
#include "schematicviewer.h"

// forward declarations
class SchematicPort;

//========================================================
//
//  StageSchematicName.
//
//========================================================

class SchematicName final : public QGraphicsTextItem {
  Q_OBJECT
  double m_width;
  double m_height;

public:
  SchematicName(QGraphicsItem *parent, double width, double height);
  ~SchematicName();

  bool eventFilter(QObject *object, QEvent *event) override;

  void setName(const QString &name);

protected:
  void focusInEvent(QFocusEvent *fe) override;
  void focusOutEvent(QFocusEvent *fe) override;

  void keyPressEvent(QKeyEvent *ke) override;

signals:
  void focusOut();

protected slots:
  void onContentsChanged();
};

//========================================================
//
// SchematicThumbnailToggle
//
//========================================================

class SchematicThumbnailToggle final : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif

  bool m_isDown;

public:
  SchematicThumbnailToggle(SchematicNode *parent, bool isOpened);
  ~SchematicThumbnailToggle();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;
  void setIsDown(bool value);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;

signals:
  void toggled(bool isOpened);
};

//========================================================
//
// SchematicToggle
//
//========================================================

class SchematicToggle : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif
protected:
  QIcon m_imageOn, m_imageOn2, m_imageOff;
  QColor m_colorOn, m_colorOff;
  int m_state;
  int m_flags;
  int m_width, m_height;

public:
  enum { eIsParentColumn = 0x01, eEnableNullState = 0x02 };

  SchematicToggle(SchematicNode *parent, const QIcon &imageOn, QColor colorOn,
                  int flags, bool isNormalIconView = true);

  SchematicToggle(SchematicNode *parent, const QIcon &imageOn, QColor colorOn,
                  const QIcon &imageOff, QColor colorOff, int flags,
                  bool isNormalIconView = true);

  //! the schematic toggle can be a 3-state or a 2-state toggle!
  SchematicToggle(SchematicNode *parent, const QIcon &imageOn,
                  const QIcon &imageOn2, QColor colorOn, int flags,
                  bool isNormalIconView = true);

  SchematicToggle(SchematicNode *parent, const QIcon &imageOn,
                  const QIcon &imageOn2, QColor colorOn, const QIcon &imageOff,
                  QColor colorOff, int flags, bool isNormalIconView = true);

  ~SchematicToggle();

  QRectF boundingRect() const override;
  // reimplemeted in SchematicToggle_SplineOptions
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

  //! this is used for a 2-state toggle;
  void setIsActive(bool value) { m_state = value ? 1 : 0; }

  //! this is used for a 3-state toggle;
  void setState(int state) { m_state = state; }

  void setSize(int width, int height) {
    m_width  = width;
    m_height = height;
    update();
  }

protected:
  // reimplemeted in SchematicToggle_SplineOptions
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) override;
signals:
  //! this is triggered for a 2-state toggle;
  void toggled(bool isChecked);

  //! this is triggered for a 3-state toggle;
  void stateChanged(int state);
};

//========================================================

class SchematicToggle_SplineOptions final : public SchematicToggle {
  Q_OBJECT
public:
  SchematicToggle_SplineOptions(SchematicNode *parent, const QPixmap &pixmap,
                                int flags)
      : SchematicToggle(parent, QIcon(pixmap), QColor(0, 0, 0, 0), flags) {}
  SchematicToggle_SplineOptions(SchematicNode *parent, const QPixmap &pixmap1,
                                const QPixmap &pixmap2, int flags)
      : SchematicToggle(parent, QIcon(pixmap1), QIcon(pixmap2),
                        QColor(0, 0, 0, 0), flags) {}

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
};

//========================================================
//
// SchematicHandleSpinBox
//
//========================================================

class SchematicHandleSpinBox : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif

protected:
  Qt::MouseButton m_buttonState;
  int m_delta;
  QPixmap m_pixmap;

public:
  SchematicHandleSpinBox(QGraphicsItem *parent);
  ~SchematicHandleSpinBox();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

signals:
  void modifyHandle(int);
  void changeStatus();
  void sceneChanged();
  void handleReleased();

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent *me) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *me) override;
};

//========================================================
//
// class SchematicLink
//
//========================================================

/*!
        \brief The class provides method to draw links between two SchematicPort

        A link can has a cubic shape or a line shape and is drawing calling the
   updatePath() method. The class also provides
        methods to retrieve the start SchematicPort and the end SchematicPort of
   the link and a method to remove the
        link from these two SchematicPort.

        \see SchematicPort, SchematicNode.
*/
class SchematicLink : public QObject, public QGraphicsItem {
  Q_OBJECT
  SchematicPort *m_startPort, *m_endPort;
  QPainterPath m_path, m_hitPath;
  bool m_lineShaped;
  bool m_highlighted;

public:
  SchematicLink(QGraphicsItem *parent, QGraphicsScene *scene);
  ~SchematicLink();

  //! Reimplements the pure virtual QGraphicsItem::boundingRect() method.
  QRectF boundingRect() const override;
  //! Reimplements the virtual QGraphicsItem::shape() method.
  QPainterPath shape() const override;
  //! Reimplements the pure virtual QGraphicsItem::paint() method.
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

  /*! Update the link path.\n
    The link is has a cubic shape starting from \b startPos and ending to \b
    endPos.
    If a link path  already exists, the path is updating otherwise a path is
    created.*/
  void updatePath(const QPointF &startPos, const QPointF &endPos);
  //! Update the link path.\n
  //! Call the updatePath(const QPointF &startPos, const QPointF &endPos)
  //! method. The \b start pos and the \b endPos
  //! are taken from the the given \b startPort and \b endPort.
  void updatePath(SchematicPort *startPort, SchematicPort *endPort);
  void updatePath() { updatePath(m_startPort, m_endPort); }

  void updateEndPos(const QPointF &endPos);

  //! Sets the start SchematicPort of the link to \b startPort.
  void setStartPort(SchematicPort *startPort) { m_startPort = startPort; }
  //! Sets the start SchematicPort of the link to \b startPort.
  void setEndPort(SchematicPort *endPort) { m_endPort = endPort; }
  //! Returns the start SchematicPort of the link.
  SchematicPort *getStartPort() const { return m_startPort; }
  //! Returns the end SchematicPort of the link.
  SchematicPort *getEndPort() const { return m_endPort; }

  /*! Returns the other SchematicPort linked to the specified \b port.\n
    Returns 0 if \b port isn't neither the start SchematicPort eihter the end
    SchematicPort of this link.*/
  SchematicPort *getOtherPort(const SchematicPort *port) const;
  /*! Returns the other SchematicNode linked to the specified \b node.\n
    Returns 0 if \b node isn't neither the parent node of the start
    SchematicPort
    eihter the parent node of the end SchematicPort of the link.*/
  SchematicNode *getOtherNode(const SchematicNode *node) const;

  //! Returns true if the link is line shaped.
  bool isLineShaped() { return m_lineShaped; }
  void setLineShaped(bool value) { m_lineShaped = value; }

  bool isHighlighted() { return m_highlighted; }
  void setHighlighted(bool value) { m_highlighted = value; }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *me) override;
};

//========================================================
//
// class SchematicPort
//
//========================================================

/*!
        \brief The class provides method to draw and andle a SchematicPort.

        A SchematicPort is a child af a SchematicNode and is used to link a
  parent node to other nodes. It can be an input port,
        or better , a port used to accept link coming from other node. A port
  that isn't an input port cannot accept
        links but can begin to draw links.\n
  A SchematicPort has got a hook thet is a position where links starts or
  ends.\n
        A SchematicPort can be linked to an arbitriary number of links.
        A SchematicPort handles a container of all links to retrieve all linked
  node; each link is indexed using a
        progressive number assigned when the link is inserted to the container.
        \see SchematicNode, SchematicLink.
*/
class SchematicPort : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif

protected:
  Qt::MouseButton m_buttonState;
  SchematicNode *m_node;
  QPointF m_hook;
  bool m_highlighted;
  QList<SchematicLink *> m_ghostLinks;
  SchematicPort *m_linkingTo;
  QList<SchematicLink *> m_links;
  int m_type;

public:
  SchematicPort(QGraphicsItem *parent, SchematicNode *node, int type);
  ~SchematicPort();

  SchematicNode *getNode() const { return m_node; }

  QRectF boundingRect() const override { return QRectF(0, 0, 1, 1); };

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override{};

  //! Add the \b link to the links container.
  void addLink(SchematicLink *link) { m_links.push_back(link); }
  //! Returns the number of the link contained in the links container or, it is
  //! the same, the number of nodes
  //! linked to this SchematicPort
  int getLinkCount() const { return m_links.size(); }
  //! Removes the link form the links container.\n
  //! It doesn't remove the link from the scene and it doesn't delete the link!
  void removeLink(SchematicLink *link);

  //! Removes \b link from the scene and the m_links list; delete it .
  void eraseLink(SchematicLink *link);
  //! Removes all links from the scene and the m_links list; delete them .
  void eraseAllLinks();

  //! Returns the link indexed with \b index.\n
  //!\note A link is indexed with a progressive number when is inserted in the
  //! container.
  SchematicLink *getLink(int index) const {
    return (index < m_links.size() && index >= 0) ? m_links[index] : 0;
  }
  //! Returns the node linked with the link \b index.
  SchematicNode *getLinkedNode(int index) const {
    return m_links[index] ? m_links[index]->getOtherNode(m_node) : 0;
  }

  //! Make a link from this port to the given port.
  virtual SchematicLink *makeLink(SchematicPort *port);
  //! Check and make a connection between the data Objects.
  //! Returns true if it is possible to have a connection between the data
  //! Object represented by this SchematicPort
  //! and that represented by \b port. If check only is false no connections is
  //! created!
  //! \see TFxPort, TStageObject.
  virtual bool linkTo(SchematicPort *port, bool checkOnly = false) = 0;

  // !Return the hook poeistion of the port.
  QPointF getHook() const { return m_hook; }

  //! Returns true if this SchematicPort is linked to \b port. Otherwise returns
  //! false.
  bool isLinkedTo(SchematicPort *port) const;

  void highLight(bool value) { m_highlighted = value; }
  bool isHighlighted() const { return m_highlighted; }

  //! Updates all links of the ports.
  void updateLinksGeometry();

  //! Returns the scene position of the link end
  QPointF getLinkEndPoint() const;

  //! Returns the type of the port. \see eFxSchematicPortType,
  //! eStageSchematicPortType
  int getType() const { return m_type; }
  //! Set the type of the port.
  void setType(int type) { m_type = type; }

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent *me) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *me) override;

private:
  virtual SchematicPort *searchPort(const QPointF &scenePos) = 0;

  // linkingPort is used only for stage schematic port -
  // in order to enable to connect from multiple node at the same time.
  virtual void hideSnappedLinks(SchematicPort *linkingPort) = 0;
  virtual void showSnappedLinks(SchematicPort *linkingPort) = 0;

signals:
  void isClicked();
  void isReleased(const QPointF &);
  void sceneChanged();
  void xsheetChanged();
};

//========================================================
//
// class SchematicNode
//
//========================================================

class SchematicNode : public QObject, public QGraphicsItem {
  Q_OBJECT
#ifndef MACOSX
  Q_INTERFACES(QGraphicsItem)
#endif

protected:
  SchematicScene *m_scene;
  qreal m_width, m_height;
  Qt::MouseButton m_buttonState;
  QMap<int, SchematicPort *> m_ports;

public:
  SchematicNode(SchematicScene *scene);
  ~SchematicNode();

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = 0) override;

  SchematicPort *addPort(int portId, SchematicPort *port);
  void erasePort(int portId);

  SchematicPort *getPort(int portId) const;
  QList<SchematicNode *> getLinkedNodes(int portId) const;

  virtual void setSchematicNodePos(const QPointF &pos) const = 0;
  virtual void setPosition(const QPointF &newPos)            = 0;

  void updateLinksGeometry();
  virtual void onClicked(){};

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent *me) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *me) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *me) override;

signals:
  void sceneChanged();
  void xsheetChanged();
};

#endif  // SCHEMATICNODE_H
