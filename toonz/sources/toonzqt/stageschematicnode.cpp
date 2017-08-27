

// TnzCore includes
#include "tofflinegl.h"
#include "tstroke.h"
#include "tconvert.h"
#include "tvectorimage.h"
#include "tundo.h"
#include "tconst.h"

// TnzLib includes
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/fxcommand.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tframehandle.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/hook.h"
#include "toonz/preferences.h"
#include "toonz/txshsimplelevel.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/stageschematicscene.h"
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QPixmap>
#include <QRegExpValidator>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QTimer>
#include <QMenu>
#include <QTextCursor>
#include <QSet>

#include "toonzqt/stageschematicnode.h"

namespace {
void drawCamera(QPainter *painter, const QColor &color, const QPen &pen,
                double width, double height) {
  QPointF points[3];
  points[0] = QPointF(width, height);
  points[1] = QPointF(width, height * 0.5);
  points[2] = QPointF(width - (height * 2 / 3), height * 0.75);
  painter->setBrush(color);
  painter->drawPolygon(points, 3);
  painter->drawRect(
      QRectF(width * 0.25, height * 0.5, width * 0.5, height * 0.5));
  QRectF rect(0, 0, height * 0.49, height * 0.49);
  rect.moveCenter(QPointF(width * 0.20, height * 0.4));
  painter->drawEllipse(rect);
  rect.moveCenter(QPointF((width * 3 / 5) + 1, height * 0.25));
  painter->drawEllipse(rect);
  painter->drawLine(0, height * 0.75, width * 0.25, height * 0.75);
  painter->drawLine(width, height * 0.75, width + 5, height * 0.75);
  painter->setBrush(QColor(235, 235, 235, 255));
  painter->drawRect(0, -14, width, 14);
}
}  // namespace

//========================================================
//
// ColumnPainter
//
//========================================================

ColumnPainter::ColumnPainter(StageSchematicColumnNode *parent, double width,
                             double height, const QString &name)
    : QGraphicsItem(parent)
    , m_parent(parent)
    , m_width(width)
    , m_height(height)
    , m_name(name) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  assert(parent->getStageObject()->getId().isColumn());
  connect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
          SLOT(onIconGenerated()));

  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;

  TXsheet *xsh = stageScene->getXsheet();
  int r0, r1, index = parent->getStageObject()->getId().getIndex();
  xsh->getCellRange(index, r0, r1);
  if (r0 > r1) return;
  TXshCell firstCell = xsh->getCell(r0, index);
  if (!firstCell.isEmpty())
    m_type = firstCell.m_level->getType();
  else
    m_type = NO_XSHLEVEL;
}

//--------------------------------------------------------

ColumnPainter::~ColumnPainter() {}

//--------------------------------------------------------

QRectF ColumnPainter::boundingRect() const {
  // iwasawa
  if (m_parent->isOpened())
    return QRectF(-5, -54, m_width + 10, m_height + 59);
  else
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void ColumnPainter::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  // obtain level name and type
  int levelType;
  QString levelName;
  m_parent->getLevelTypeAndName(levelType, levelName);

  QLinearGradient linearGrad = getGradientByLevelType(m_type);

  painter->setBrush(QBrush(linearGrad));
  painter->setPen(Qt::NoPen);
  if (levelType == PLT_XSHLEVEL)
    painter->drawRoundRect(0, 0, m_width, m_height, 32, 99);
  else
    painter->drawRect(0, 0, m_width, m_height);

  // draw palette icon for palette node
  if (levelType == PLT_XSHLEVEL) {
    QPixmap palettePm = QPixmap(":Resources/schematic_palette.png");
    QRect paletteRect(-3, -1, 20, 16);
    painter->drawPixmap(paletteRect, palettePm);
  }

  if (m_parent->isOpened()) {
    // Draw the pixmap
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QColor(0, 0, 0, 255));
    QPixmap pixmap = scalePixmapKeepingAspectRatio(
        m_parent->getPixmap(), QSize(m_width, 49), Qt::transparent);
    if (!pixmap.isNull()) {
      painter->drawPixmap(QPointF(0, -pixmap.height()), pixmap);
    } else {
      painter->setBrush(QColor(255, 255, 255, 255));
      painter->drawRect(0, -pixmap.height(), m_width, pixmap.height());
    }
  }
  painter->setPen(Qt::white);
  painter->setBrush(Qt::NoBrush);

  //! draw the name only if it is not editing
  if (!m_parent->isNameEditing()) {
    StageSchematicScene *stageScene =
        dynamic_cast<StageSchematicScene *>(scene());
    if (!stageScene) return;

    // if this is current object
    if (stageScene->getCurrentObject() == m_parent->getStageObject()->getId())
      painter->setPen(Qt::yellow);
    QRectF columnNameRect(18, 2, 54, 14);
    QString elidedName =
        elideText(m_name, painter->font(), columnNameRect.width());
    painter->drawText(columnNameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      elidedName);
  }

  // level names
  QRectF levelNameRect(18, 16, 54, 14);
  QString elidedName =
      elideText(levelName, painter->font(), levelNameRect.width());
  painter->drawText(levelNameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    elidedName);
}

//--------------------------------------------------------

QLinearGradient ColumnPainter::getGradientByLevelType(int type) {
  QColor col1, col2, col3, col4, col5;
  switch (type) {
  case TZI_XSHLEVEL:
  case OVL_XSHLEVEL:
    col1 = QColor(209, 232, 234);
    col2 = QColor(121, 171, 181);
    col3 = QColor(98, 143, 165);
    col4 = QColor(33, 90, 118);
    col5 = QColor(122, 172, 173);
    break;
  case PLI_XSHLEVEL:
    col1 = QColor(236, 226, 182);
    col2 = QColor(199, 187, 95);
    col3 = QColor(180, 180, 67);
    col4 = QColor(130, 125, 15);
    col5 = QColor(147, 150, 28);
    break;
  case TZP_XSHLEVEL:
    col1 = QColor(196, 245, 196);
    col2 = QColor(111, 192, 105);
    col3 = QColor(63, 146, 99);
    col4 = QColor(32, 113, 86);
    col5 = QColor(117, 187, 166);
    break;
  case ZERARYFX_XSHLEVEL:
    col1 = QColor(232, 245, 196);
    col2 = QColor(130, 129, 93);
    col3 = QColor(113, 115, 81);
    col4 = QColor(55, 59, 25);
    col5 = QColor(144, 154, 111);
    break;
  case CHILD_XSHLEVEL:
    col1 = QColor(247, 208, 241);
    col2 = QColor(214, 154, 219);
    col3 = QColor(170, 123, 169);
    col4 = QColor(92, 52, 98);
    col5 = QColor(132, 111, 154);
    break;
  case PLT_XSHLEVEL:
    col1 = QColor(42, 171, 154);
    col2 = QColor(28, 116, 105);
    col3 = QColor(15, 62, 56);
    col4 = QColor(15, 62, 56);
    col5 = QColor(33, 95, 90);
    break;
  case MESH_XSHLEVEL:
    col1 = QColor(210, 140, 255);
    col2 = QColor(200, 130, 255);
    col3 = QColor(150, 80, 180);
    col4 = QColor(150, 80, 180);
    col5 = QColor(180, 120, 220);
    break;
  case UNKNOWN_XSHLEVEL:
  case NO_XSHLEVEL:
  default:
    col1 = QColor(227, 227, 227);
    col2 = QColor(174, 174, 174);
    col3 = QColor(123, 123, 123);
    col4 = QColor(61, 61, 61);
    col5 = QColor(127, 138, 137);
  }

  QLinearGradient linearGrad(QPointF(0, 0), QPointF(0, 32));
  linearGrad.setColorAt(0, col1);
  linearGrad.setColorAt(0.08, col2);
  linearGrad.setColorAt(0.20, col3);
  linearGrad.setColorAt(0.23, col4);
  linearGrad.setColorAt(0.9, col4);
  linearGrad.setColorAt(1, col5);
  return linearGrad;
}

//--------------------------------------------------------

void ColumnPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QMenu menu(stageScene->views()[0]);

  QAction *resetCenter = new QAction(tr("&Reset Center"), &menu);
  connect(resetCenter, SIGNAL(triggered()), stageScene, SLOT(onResetCenter()));
  QAction *collapse   = CommandManager::instance()->getAction("MI_Collapse");
  QAction *openSubxsh = new QAction(tr("&Open Subxsheet"), &menu);
  QAction *explodeChild =
      CommandManager::instance()->getAction("MI_ExplodeChild");
  connect(openSubxsh, SIGNAL(triggered()), stageScene, SLOT(onOpenSubxsheet()));
  QAction *group = CommandManager::instance()->getAction("MI_Group");

  QAction *clear = CommandManager::instance()->getAction("MI_Clear");
  QAction *copy  = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut   = CommandManager::instance()->getAction("MI_Cut");
  QAction *paste = CommandManager::instance()->getAction("MI_Paste");

  menu.addAction(resetCenter);
  menu.addSeparator();
  menu.addAction(collapse);
  TFrameHandle *frameHandle = stageScene->getFrameHandle();
  if (frameHandle->getFrameType() == TFrameHandle::SceneFrame) {
    int col       = m_parent->getStageObject()->getId().getIndex();
    int fr        = frameHandle->getFrame();
    TXsheet *xsh  = stageScene->getXsheet();
    TXshCell cell = xsh->getCell(fr, col);
    if (dynamic_cast<TXshChildLevel *>(cell.m_level.getPointer())) {
      menu.addAction(openSubxsh);
      menu.addAction(explodeChild);
    }
  }
  menu.addSeparator();

  menu.addAction(clear);
  menu.addAction(copy);
  menu.addAction(cut);
  menu.addAction(paste);
  menu.addSeparator();

  menu.addAction(group);
  menu.exec(cme->screenPos());
}

//--------------------------------------------------------

void ColumnPainter::onIconGenerated() {
  if (m_type != OVL_XSHLEVEL) return;
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;

  TXsheet *xsh = stageScene->getXsheet();
  int r0, r1, index = m_parent->getStageObject()->getId().getIndex();
  xsh->getCellRange(index, r0, r1);
  if (r0 > r1) return;
  TXshCell firstCell = xsh->getCell(r0, index);
  int type           = firstCell.m_level->getType();
  if (m_type != type) {
    m_type = type;
    update();
  }
}

//========================================================
//
// GroupPainter
//
//========================================================

GroupPainter::GroupPainter(StageSchematicGroupNode *parent, double width,
                           double height, const QString &name)
    : QGraphicsItem(parent)
    , m_parent(parent)
    , m_width(width)
    , m_height(height)
    , m_name(name) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
}

//--------------------------------------------------------

GroupPainter::~GroupPainter() {}

//--------------------------------------------------------

QRectF GroupPainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void GroupPainter::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;

  painter->save();
  QPen pen;
  if (m_parent->isSelected()) {
    painter->setBrush(QColor(0, 0, 0, 0));
    pen.setColor(QColor(255, 255, 255, 255));
    pen.setWidth(4.0);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->drawRect(-2, -2, m_width + 4, m_height + 4);
  }
  painter->restore();

  {
    QLinearGradient groupLinearGrad(QPointF(0, 0), QPointF(0, 18));
    groupLinearGrad.setColorAt(0, QColor(115, 184, 200));
    groupLinearGrad.setColorAt(0.14, QColor(65, 118, 150));
    groupLinearGrad.setColorAt(0.35, QColor(57, 107, 158));
    groupLinearGrad.setColorAt(0.4, QColor(12, 60, 120));
    groupLinearGrad.setColorAt(0.8, QColor(12, 60, 120));
    groupLinearGrad.setColorAt(1, QColor(85, 91, 110));
    painter->setBrush(QBrush(groupLinearGrad));
    painter->setPen(Qt::NoPen);
    painter->drawRect(QRectF(0, 0, m_width, m_height));
  }

  //! draw the name only if it is not editing
  if (!m_parent->isNameEditing()) {
    QFont font("Verdana", 8);
    painter->setFont(font);
    if (stageScene->getCurrentObject() == m_parent->getStageObject()->getId())
      painter->setPen(QColor(255, 0, 0, 255));
    else
      painter->setPen(Qt::white);

    QRectF rect(18, 0, 54, 18);
    QString elidedName = elideText(m_name, painter->font(), rect.width());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
  }
}

//--------------------------------------------------------

void GroupPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QMenu menu(stageScene->views()[0]);

  QAction *group   = CommandManager::instance()->getAction("MI_Group");
  QAction *ungroup = CommandManager::instance()->getAction("MI_Ungroup");

  QAction *editGroup = new QAction(tr("&Open Group"), &menu);
  connect(editGroup, SIGNAL(triggered()), stageScene, SLOT(onEditGroup()));

  menu.addAction(group);
  menu.addAction(ungroup);
  menu.addAction(editGroup);
  menu.exec(cme->screenPos());
}

//========================================================
//
// PegbarPainter
//
//========================================================

PegbarPainter::PegbarPainter(StageSchematicPegbarNode *parent, double width,
                             double height, const QString &name)
    : QGraphicsItem(parent)
    , m_width(width)
    , m_height(height)
    , m_parent(parent)
    , m_name(name) {}

//--------------------------------------------------------

PegbarPainter::~PegbarPainter() {}

//--------------------------------------------------------

QRectF PegbarPainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void PegbarPainter::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  QLinearGradient pegLinearGrad(QPointF(0, 0), QPointF(0, 18));
  pegLinearGrad.setColorAt(0, QColor(223, 184, 115));
  pegLinearGrad.setColorAt(0.14, QColor(165, 118, 65));
  pegLinearGrad.setColorAt(0.35, QColor(157, 107, 58));
  pegLinearGrad.setColorAt(0.4, QColor(112, 60, 27));
  pegLinearGrad.setColorAt(0.8, QColor(112, 60, 27));
  pegLinearGrad.setColorAt(1, QColor(113, 91, 85));

  painter->setBrush(QBrush(pegLinearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(QRectF(0, 0, m_width, m_height));

  if (!m_parent->isNameEditing()) {
    StageSchematicScene *stageScene =
        dynamic_cast<StageSchematicScene *>(scene());
    if (!stageScene) return;
    if (stageScene->getCurrentObject() == m_parent->getStageObject()->getId())
      painter->setPen(Qt::yellow);
    else
      painter->setPen(Qt::white);
    // Draw the name
    QRectF rect(18, 0, 54, 18);
    QString elidedName = elideText(m_name, painter->font(), rect.width());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
  }
}

//--------------------------------------------------------

void PegbarPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QMenu menu(stageScene->views()[0]);

  QAction *resetCenter = new QAction(tr("&Reset Center"), &menu);
  connect(resetCenter, SIGNAL(triggered()), stageScene, SLOT(onResetCenter()));

  QAction *group = CommandManager::instance()->getAction("MI_Group");

  QAction *clear = CommandManager::instance()->getAction("MI_Clear");
  QAction *copy  = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut   = CommandManager::instance()->getAction("MI_Cut");
  QAction *paste = CommandManager::instance()->getAction("MI_Paste");

  menu.addAction(resetCenter);
  menu.addSeparator();
  menu.addAction(clear);
  menu.addAction(copy);
  menu.addAction(cut);
  menu.addAction(paste);
  menu.addSeparator();
  menu.addAction(group);
  menu.exec(cme->screenPos());
}

//========================================================
//
// CameraPainter
//
//========================================================

CameraPainter::CameraPainter(StageSchematicCameraNode *parent, double width,
                             double height, const QString &name)
    : QGraphicsItem(parent)
    , m_width(width)
    , m_height(height)
    , m_parent(parent)
    , m_name(name) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  TXsheet *xsh = stageScene->getXsheet();
  m_isActive   = xsh->getStageObjectTree()->getCurrentCameraId() ==
               parent->getStageObject()->getId();
}

//--------------------------------------------------------

CameraPainter::~CameraPainter() {}

//--------------------------------------------------------

QRectF CameraPainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void CameraPainter::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  QLinearGradient camLinearGrad(QPointF(0, 0), QPointF(0, 18));
  if (m_isActive) {
    camLinearGrad.setColorAt(0, QColor(115, 190, 224));
    camLinearGrad.setColorAt(0.14, QColor(51, 132, 208));
    camLinearGrad.setColorAt(0.35, QColor(39, 118, 196));
    camLinearGrad.setColorAt(0.4, QColor(18, 82, 153));
    camLinearGrad.setColorAt(0.8, QColor(18, 82, 153));
    camLinearGrad.setColorAt(1, QColor(68, 119, 169));
  } else {
    camLinearGrad.setColorAt(0, QColor(183, 197, 196));
    camLinearGrad.setColorAt(0.14, QColor(138, 157, 160));
    camLinearGrad.setColorAt(0.35, QColor(125, 144, 146));
    camLinearGrad.setColorAt(0.4, QColor(80, 94, 97));
    camLinearGrad.setColorAt(0.8, QColor(80, 94, 97));
    camLinearGrad.setColorAt(1, QColor(128, 140, 142));
  }

  painter->setBrush(QBrush(camLinearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(QRectF(0, 0, m_width, m_height));

  if (!m_parent->isNameEditing()) {
    StageSchematicScene *stageScene =
        dynamic_cast<StageSchematicScene *>(scene());
    if (!stageScene) return;
    if (stageScene->getCurrentObject() == m_parent->getStageObject()->getId())
      painter->setPen(Qt::yellow);
    else
      painter->setPen(Qt::white);
    // Draw the name
    QRectF rect(18, 0, 54, 18);
    QString elidedName = elideText(m_name, painter->font(), rect.width());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
  }
}

//--------------------------------------------------------

void CameraPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QMenu menu(stageScene->views()[0]);

  QAction *cameraSettings =
      CommandManager::instance()->getAction("MI_CameraStage");

  QAction *resetCenter = new QAction(tr("&Reset Center"), &menu);
  connect(resetCenter, SIGNAL(triggered()), stageScene, SLOT(onResetCenter()));

  QAction *activate = new QAction(tr("&Activate"), &menu);
  connect(activate, SIGNAL(triggered()), stageScene, SLOT(onCameraActivate()));

  QAction *clear = CommandManager::instance()->getAction("MI_Clear");
  QAction *copy  = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut   = CommandManager::instance()->getAction("MI_Cut");
  QAction *paste = CommandManager::instance()->getAction("MI_Paste");

  bool isDeactivated =
      m_parent->getStageObject()->getId() !=
      stageScene->getXsheet()->getStageObjectTree()->getCurrentCameraId();

  if (isDeactivated)
    menu.addAction(activate);
  else
    menu.addAction(cameraSettings);
  menu.addAction(resetCenter);
  menu.addSeparator();
  if (isDeactivated) menu.addAction(clear);
  menu.addAction(copy);
  if (isDeactivated) menu.addAction(cut);
  menu.addAction(paste);
  menu.exec(cme->screenPos());
}

//========================================================
//
// TablePainter
//
//========================================================

TablePainter::TablePainter(StageSchematicTableNode *parent, double width,
                           double height)
    : QGraphicsItem(parent)
    , m_width(width)
    , m_height(height)
    , m_parent(parent) {}

//--------------------------------------------------------

TablePainter::~TablePainter() {}

//--------------------------------------------------------

QRectF TablePainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void TablePainter::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget) {
  QPixmap tablePm = QPixmap(":Resources/schematic_tablenode.png");

  QLinearGradient tableLinearGrad(QPointF(0, 0), QPointF(0, 18));
  tableLinearGrad.setColorAt(0, QColor(152, 146, 188));
  tableLinearGrad.setColorAt(0.14, QColor(107, 106, 148));
  tableLinearGrad.setColorAt(0.35, QColor(96, 96, 138));
  tableLinearGrad.setColorAt(0.4, QColor(63, 67, 99));
  tableLinearGrad.setColorAt(0.8, QColor(63, 67, 99));
  tableLinearGrad.setColorAt(1, QColor(101, 105, 143));

  painter->setBrush(QBrush(tableLinearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(QRectF(0, 0, m_width, m_height));

  QRect imgRect(3, -3, 24, 24);

  painter->drawPixmap(imgRect, tablePm);

  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  if (stageScene->getCurrentObject() == m_parent->getStageObject()->getId())
    painter->setPen(Qt::yellow);
  else
    painter->setPen(Qt::white);

  // Draw the name
  QRectF rect(30, 0, 42, 18);
  painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, QString("Table"));
}

//--------------------------------------------------------

void TablePainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QMenu menu(stageScene->views()[0]);

  QAction *resetCenter = new QAction(tr("&Reset Center"), &menu);
  connect(resetCenter, SIGNAL(triggered()), stageScene, SLOT(onResetCenter()));

  menu.addAction(resetCenter);
  menu.exec(cme->screenPos());
}

//========================================================
//
// SplinePainter
//
//========================================================

SplinePainter::SplinePainter(StageSchematicSplineNode *parent, double width,
                             double height, const QString &name)
    : QGraphicsItem(parent)
    , m_parent(parent)
    , m_width(width)
    , m_height(height)
    , m_name(name) {}

//--------------------------------------------------------

SplinePainter::~SplinePainter() {}

//--------------------------------------------------------

QRectF SplinePainter::boundingRect() const {
  if (m_parent->isOpened())
    return QRectF(-5, -5, m_width + 10, m_height + 59);
  else
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void SplinePainter::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  QLinearGradient pegLinearGrad(QPointF(0, 0), QPointF(0, 18));
  pegLinearGrad.setColorAt(0, QColor(157, 255, 82));
  pegLinearGrad.setColorAt(0.14, QColor(127, 207, 42));
  pegLinearGrad.setColorAt(0.35, QColor(128, 201, 37));
  pegLinearGrad.setColorAt(0.4, QColor(100, 148, 8));
  pegLinearGrad.setColorAt(0.8, QColor(100, 148, 8));
  pegLinearGrad.setColorAt(1, QColor(120, 178, 73));

  painter->setBrush(QBrush(pegLinearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRoundRect(QRectF(0, 0, m_width, m_height), 20, 99);
  if (m_parent->isOpened()) {
    // Draw the pixmap
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QColor(0, 0, 0, 255));
    QPixmap pixmap = scalePixmapKeepingAspectRatio(
        m_parent->getPixmap(), QSize(m_width, 49), Qt::transparent);
    if (!pixmap.isNull()) {
      painter->drawPixmap(QPointF(0, 18), pixmap);
    } else {
      painter->setBrush(QColor(255, 255, 255, 255));
      painter->drawRect(0, 18, m_width, 49);
    }
  }

  //! draw the name only if it is not editing
  if (!m_parent->isNameEditing()) {
    StageSchematicScene *stageScene =
        dynamic_cast<StageSchematicScene *>(scene());
    if (!stageScene) return;
    painter->setPen(Qt::white);
    QRectF rect(18, 0, 72, 18);
    QString elidedName = elideText(m_name, painter->font(), rect.width());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
  }
}

//--------------------------------------------------------

void SplinePainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QMenu menu(stageScene->views()[0]);

  QAction *removeSpline = new QAction(tr("&Delete"), &menu);
  connect(removeSpline, SIGNAL(triggered()), stageScene,
          SLOT(onRemoveSpline()));
  QAction *saveSpline = new QAction(tr("&Save Motion Path..."), &menu);
  connect(saveSpline, SIGNAL(triggered()), stageScene, SLOT(onSaveSpline()));
  QAction *loadSpline = new QAction(tr("&Load Motion Path..."), &menu);
  connect(loadSpline, SIGNAL(triggered()), stageScene, SLOT(onLoadSpline()));

  QAction *copy  = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut   = CommandManager::instance()->getAction("MI_Cut");
  QAction *paste = CommandManager::instance()->getAction("MI_Paste");

  menu.addAction(saveSpline);
  menu.addAction(loadSpline);
  menu.addSeparator();
  menu.addAction(removeSpline);
  menu.addAction(copy);
  menu.addAction(cut);
  menu.addAction(paste);
  menu.exec(cme->screenPos());
}

//========================================================
//
// StageSchematicNodePort
//
//========================================================

StageSchematicNodePort::StageSchematicNodePort(StageSchematicNodeDock *parent,
                                               int type)
    : SchematicPort(parent, parent->getNode(), type), m_handle("") {
  QRectF rect = boundingRect();
  if (parent->isParentPort())
    m_hook = QPointF(rect.left(), (rect.top() + rect.bottom()) * 0.5);
  else
    m_hook = QPointF(rect.right(), (rect.top() + rect.bottom()) * 0.5);
}

//--------------------------------------------------------

StageSchematicNodePort::~StageSchematicNodePort() {}

//--------------------------------------------------------

QRectF StageSchematicNodePort::boundingRect() const {
  return QRectF(0, 0, 18, 18);
}

//--------------------------------------------------------

void StageSchematicNodePort::paint(QPainter *painter,
                                   const QStyleOptionGraphicsItem *option,
                                   QWidget *widget) {
  // StageSchematicNode *node = dynamic_cast<StageSchematicNode *>(getNode());
  StageSchematicScene *scene =
      dynamic_cast<StageSchematicScene *>(this->scene());
  if (scene && scene->isShowLetterOnPortFlagEnabled()) {
    painter->setBrush(QColor(255, 255, 255, 255));
    painter->setPen(QColor(180, 180, 180, 255));
    painter->drawRect(boundingRect());
    if (m_type == 103 || m_type == 104 || m_type == 105) return;
    painter->setPen(QColor(0, 0, 0, 255));
    QFont font("Verdana", 8);
    painter->setFont(font);
    QTextOption textOption(Qt::AlignCenter);
    QString text = m_handle;
    if (text.size() > 1 && text.at(0) == 'H') text.remove("H");
    painter->drawText(boundingRect(), text, textOption);
  } else {
    QRect imgRect(2, 2, 14, 14);
    QRect sourceRect = scene->views()[0]->matrix().mapRect(imgRect);
    QPixmap pixmap;

    if (getType() == eStageParentPort || getType() == eStageParentGroupPort) {
      if (isHighlighted())
        pixmap = QIcon(":Resources/port_blue_highlight.svg")
                     .pixmap(sourceRect.size());
      else
        pixmap = QIcon(":Resources/port_blue.svg").pixmap(sourceRect.size());
    } else {
      if (isHighlighted())
        pixmap = QIcon(":Resources/port_red_highlight.svg")
                     .pixmap(sourceRect.size());
      else
        pixmap = QIcon(":Resources/port_red.svg").pixmap(sourceRect.size());
    }
    sourceRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                       sourceRect.height() * getDevPixRatio());
    painter->drawPixmap(imgRect, pixmap, sourceRect);
  }
}

//--------------------------------------------------------

bool StageSchematicNodePort::linkTo(SchematicPort *port, bool checkOnly) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!port) return false;

  StageSchematicNode *srcNode = dynamic_cast<StageSchematicNode *>(getNode());
  if (this == port && !srcNode->getStageObject()->isGrouped()) {
    if (!checkOnly)
      TStageObjectCmd::setParent(srcNode->getStageObject()->getId(),
                                 TStageObjectId::NoneId, "",
                                 stageScene->getXsheetHandle());
    return true;
  }
  StageSchematicNode *dstNode =
      dynamic_cast<StageSchematicNode *>(port->getNode());
  if (!dstNode) return false;
  if (srcNode == dstNode) return false;

  TStageObject *pegbar = 0, *parentPegbar = 0;
  SchematicPort *childPort, *parentPort;
  if (getType() == eStageParentPort && port->getType() == eStageChildPort) {
    pegbar       = srcNode->getStageObject();
    parentPegbar = dstNode->getStageObject();
    childPort    = this;
    parentPort   = port;
  } else if (getType() == eStageChildPort &&
             port->getType() == eStageParentPort) {
    pegbar       = dstNode->getStageObject();
    parentPegbar = srcNode->getStageObject();
    childPort    = port;
    parentPort   = this;
  } else
    return false;
  if (pegbar->getId().isTable()) return false;
  if (pegbar->getId().isPegbar() && !parentPegbar->getId().isTable() &&
      !parentPegbar->getId().isPegbar() && !parentPegbar->getId().isCamera())
    return false;

  if (parentPegbar == pegbar || parentPegbar->isAncestor(pegbar)) return false;
  if (parentPegbar->isGrouped() || pegbar->isGrouped()) return false;
  if (checkOnly) return true;

  StageSchematicNodePort *dstPort =
      dynamic_cast<StageSchematicNodePort *>(parentPort);
  if (!dstPort) return false;
  TStageObjectCmd::setParent(pegbar->getId(), parentPegbar->getId(),
                             dstPort->getHandle().toStdString(),
                             stageScene->getXsheetHandle());
  return true;
}

//--------------------------------------------------------

SchematicPort *StageSchematicNodePort::searchPort(const QPointF &scenePos) {
  QList<QGraphicsItem *> items = scene()->items(scenePos);
  int i;
  for (i = 0; i < items.size(); i++) {
    StageSchematicNodePort *linkingTo =
        dynamic_cast<StageSchematicNodePort *>(items[i]);
    if (linkingTo) {
      StageSchematicNode *node =
          dynamic_cast<StageSchematicNode *>(linkingTo->getNode());
      if (!node->getStageObject()->isGrouped()) return linkingTo;
    }
  }
  return 0;
}

//--------------------------------------------------------

void StageSchematicNodePort::hideSnappedLinks() {
  if (!m_linkingTo) return;
  if (getType() == eStageChildPort &&
      m_linkingTo->getType() == eStageParentPort &&
      m_linkingTo->getLinkCount() == 1)
    m_linkingTo->getLink(0)->hide();
  if (getType() == eStageParentPort &&
      m_linkingTo->getType() == eStageChildPort && getLinkCount() == 1)
    getLink(0)->hide();
}

//--------------------------------------------------------

void StageSchematicNodePort::showSnappedLinks() {
  if (!m_linkingTo) return;
  if (getType() == eStageChildPort &&
      m_linkingTo->getType() == eStageParentPort &&
      m_linkingTo->getLinkCount() == 1) {
    m_linkingTo->getLink(0)->show();
    m_linkingTo->highLight(true);
    m_linkingTo->update();
  }
  if (getType() == eStageParentPort &&
      m_linkingTo->getType() == eStageChildPort && getLinkCount() == 1) {
    getLink(0)->show();
    m_linkingTo->highLight(true);
    m_linkingTo->update();
  }
}

//========================================================
//
// StageSchematicSplinePort
//
//========================================================

StageSchematicSplinePort::StageSchematicSplinePort(
    StageSchematicSplineDock *parent, int type)
    : SchematicPort(parent, parent->getNode(), type), m_parent(parent) {
  m_squarePixmap = QPixmap(":Resources/schematic_spline_aim_square.svg");
  m_rhombPixmap  = QPixmap(":Resources/schematic_spline_aim_rhomb.svg");
  QRectF rect    = boundingRect();
  if (parent->isParentPort())
    m_hook = QPointF((rect.left() + rect.right()) * 0.5, rect.bottom() - 5);
  else
    m_hook = QPointF((rect.left() + rect.right()) * 0.5, rect.top() + 5);
}

//--------------------------------------------------------

StageSchematicSplinePort::~StageSchematicSplinePort() {}

//--------------------------------------------------------

QRectF StageSchematicSplinePort::boundingRect() const {
  return QRectF(0, 0, 16, 8);
}

//--------------------------------------------------------

void StageSchematicSplinePort::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {
  QRect rect(0, 0, 16, 8);
  QRect sourceRect = scene()->views()[0]->matrix().mapRect(rect);
  QPixmap pixmap;

  if (!m_parent->isParentPort()) {
    if (getLinkCount() > 0) {
      static QIcon splineChildIcon(":Resources/spline_child_port.svg");
      pixmap = splineChildIcon.pixmap(sourceRect.size());
    } else {
      static QIcon splineChildDisconIcon(
          ":Resources/spline_child_port_disconnect.svg");
      pixmap = splineChildDisconIcon.pixmap(sourceRect.size());
    }
  } else {
    static QIcon splineParentIcon(":Resources/spline_parent_port.svg");
    pixmap = splineParentIcon.pixmap(sourceRect.size());
  }
  sourceRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                     sourceRect.height() * getDevPixRatio());
  painter->drawPixmap(rect, pixmap, sourceRect);
}

//--------------------------------------------------------

bool StageSchematicSplinePort::linkTo(SchematicPort *port, bool checkOnly) {
  assert(port != 0);
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  StageSchematicNode *node = dynamic_cast<StageSchematicNode *>(getNode());
  StageSchematicSplineNode *splineNode =
      dynamic_cast<StageSchematicSplineNode *>(getNode());
  if (node) {
    StageSchematicSplineNode *targetSplineNode =
        dynamic_cast<StageSchematicSplineNode *>(port->getNode());
    if (!targetSplineNode && port != this) return false;
    if (!checkOnly && targetSplineNode) {
      TStageObjectSpline *spline = targetSplineNode->getSpline();
      TStageObjectCmd::setSplineParent(spline, node->getStageObject(),
                                       stageScene->getXsheetHandle());
    } else if (!checkOnly && port == this)
      node->getStageObject()->setSpline(0);
    return true;
  } else if (splineNode) {
    StageSchematicNode *targetNode =
        dynamic_cast<StageSchematicNode *>(port->getNode());
    if (!targetNode || port->getType() != eStageSplinePort) return false;
    if (!checkOnly) {
      TStageObjectSpline *spline = splineNode->getSpline();
      TStageObjectCmd::setSplineParent(spline, targetNode->getStageObject(),
                                       stageScene->getXsheetHandle());
    }
    return true;
  }
  return false;
}

//--------------------------------------------------------

SchematicPort *StageSchematicSplinePort::searchPort(const QPointF &scenePos) {
  QList<QGraphicsItem *> items = scene()->items(scenePos);
  int i;
  for (i = 0; i < items.size(); i++) {
    StageSchematicSplinePort *linkingTo =
        dynamic_cast<StageSchematicSplinePort *>(items[i]);
    if (linkingTo) return linkingTo;
  }
  return 0;
}

//--------------------------------------------------------

void StageSchematicSplinePort::hideSnappedLinks() {
  if (!m_linkingTo) return;
  StageSchematicNode *node = dynamic_cast<StageSchematicNode *>(getNode());
  StageSchematicSplineNode *splineNode =
      dynamic_cast<StageSchematicSplineNode *>(getNode());
  if (node && getLinkCount() == 1) getLink(0)->hide();
  if (splineNode && m_linkingTo->getLinkCount() == 1)
    m_linkingTo->getLink(0)->hide();
}

//--------------------------------------------------------

void StageSchematicSplinePort::showSnappedLinks() {
  if (!m_linkingTo) return;
  StageSchematicNode *node = dynamic_cast<StageSchematicNode *>(getNode());
  StageSchematicSplineNode *splineNode =
      dynamic_cast<StageSchematicSplineNode *>(getNode());
  if (node && getLinkCount() == 1) getLink(0)->show();
  if (splineNode && m_linkingTo->getLinkCount() == 1)
    m_linkingTo->getLink(0)->show();
}

//========================================================
//
// class SplineAimChanger
//
//========================================================

SplineAimChanger::SplineAimChanger(QGraphicsItem *parent)
    : SchematicHandleSpinBox(parent), m_aim(false) {}

//--------------------------------------------------------

SplineAimChanger::~SplineAimChanger() {}

//--------------------------------------------------------

void SplineAimChanger::mouseMoveEvent(QGraphicsSceneMouseEvent *me) {
  if (m_buttonState == Qt::LeftButton) {
    bool increase           = false;
    int delta               = me->screenPos().y() - me->lastScreenPos().y();
    if (delta < 0) increase = true;
    m_delta += abs(delta);
    if (m_delta > 15) {
      emit(changeStatus());
      m_delta = 0;
    }
  }
}

//--------------------------------------------------------

void SplineAimChanger::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  SchematicHandleSpinBox::mousePressEvent(me);
}

//--------------------------------------------------------

void SplineAimChanger::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  SchematicHandleSpinBox::mouseReleaseEvent(me);
  m_delta = 0;
}

//========================================================
//
// class StageSchematicNodeDock
//
//========================================================

StageSchematicNodeDock::StageSchematicNodeDock(StageSchematicNode *parent,
                                               bool isParentPort,
                                               eStageSchematicPortType type)
    : QGraphicsItem(parent), m_parent(parent), m_isParentPort(isParentPort) {
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  m_port = new StageSchematicNodePort(this, type);

  m_handleSpinBox = new SchematicHandleSpinBox(this);

  m_port->setPos(0, 0);
  if (isParentPort) {
    StageSchematicScene *stageScene =
        dynamic_cast<StageSchematicScene *>(scene());
    if (stageScene && stageScene->isShowLetterOnPortFlagEnabled())
      m_port->setPos(m_handleSpinBox->boundingRect().width(), 0);
    m_handleSpinBox->setPos(0, 1);
  } else
    m_handleSpinBox->setPos(m_port->boundingRect().width(), 1);

  m_handleSpinBox->hide();
  connect(m_handleSpinBox, SIGNAL(modifyHandle(int)), this,
          SLOT(onModifyHandle(int)));
  connect(m_handleSpinBox, SIGNAL(sceneChanged()), parent,
          SIGNAL(sceneChanged()));
  connect(m_handleSpinBox, SIGNAL(handleReleased()), parent,
          SLOT(onHandleReleased()));

  connect(this, SIGNAL(sceneChanged()), parent, SIGNAL(sceneChanged()));

  connect(m_port, SIGNAL(isClicked()), this, SLOT(onPortClicked()));
  connect(m_port, SIGNAL(isReleased(const QPointF &)), this,
          SLOT(onPortReleased(const QPointF &)));

  m_timer = new QTimer(this);
  m_timer->setInterval(200);
  m_timer->setSingleShot(true);
  connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeOut()));

  connect(m_port, SIGNAL(sceneChanged()), parent, SIGNAL(sceneChanged()));
  connect(m_port, SIGNAL(xsheetChanged()), parent, SIGNAL(xsheetChanged()));

  setZValue(1.5);
#if QT_VERSION >= 0x050000
  setAcceptHoverEvents(true);
#else
  setAcceptsHoverEvents(true);
#endif
}

//--------------------------------------------------------

StageSchematicNodeDock::~StageSchematicNodeDock() {}

//--------------------------------------------------------

QRectF StageSchematicNodeDock::boundingRect() const {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  QRectF portRect = m_port->boundingRect();
  portRect.moveTopLeft(QPointF(0, 0));
  if (stageScene && stageScene->isShowLetterOnPortFlagEnabled()) {
    QRectF handleRect = m_handleSpinBox->boundingRect();
    handleRect.moveTopLeft(QPointF(portRect.width(), handleRect.topLeft().y()));
#if QT_VERSION >= 0x050000
    portRect = portRect.united(handleRect);
#else
    portRect = portRect.unite(handleRect);
#endif
  }
  return portRect;
}

//--------------------------------------------------------

void StageSchematicNodeDock::paint(QPainter *painter,
                                   const QStyleOptionGraphicsItem *option,
                                   QWidget *widget) {}

//--------------------------------------------------------

void StageSchematicNodeDock::onModifyHandle(int increase) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  std::string handle(getPort()->getHandle().toStdString());

  int index;
  if (handle[0] == 'H' && handle.length() > 1)
    index = -(std::stoi(handle.substr(1)));
  else
    index = handle[0] - 'A';
  index += -increase;  //==1 ? -1 : 1;

  int min = (getNode()->getStageObject()->getId().isColumn())
                ? -HookSet::maxHooksCount
                : 0;
  index = tcrop(index, min, 25);

  if (index >= 0)
    handle = std::string(1, (char)('A' + index));
  else
    handle = "H" + std::to_string(-index);

  if (m_isParentPort)
    TStageObjectCmd::setHandle(getNode()->getStageObject()->getId(), handle,
                               stageScene->getXsheetHandle());
  else {
    int i, linkCount = getPort()->getLinkCount();
    std::vector<TStageObjectId> ids;
    for (i = 0; i < linkCount; i++) {
      StageSchematicNode *node =
          dynamic_cast<StageSchematicNode *>(getPort()->getLinkedNode(i));
      if (node) ids.push_back(node->getStageObject()->getId());
    }
    TStageObjectCmd::setParentHandle(ids, handle,
                                     stageScene->getXsheetHandle());
  }
  getPort()->setHandle(QString::fromStdString(handle));
  getPort()->update();
}

//--------------------------------------------------------

void StageSchematicNodeDock::hoverEnterEvent(QGraphicsSceneHoverEvent *he) {
  QList<QGraphicsItem *> items = scene()->items(he->scenePos());
  eStageSchematicPortType type = (eStageSchematicPortType)getPort()->getType();
  if (items.contains(m_port) && type != 103 && type != 104 && type != 105) {
    m_port->highLight(true);
    m_timer->start();
  }
  QGraphicsItem::hoverEnterEvent(he);
}

//--------------------------------------------------------

void StageSchematicNodeDock::hoverLeaveEvent(QGraphicsSceneHoverEvent *he) {
  m_port->highLight(false);
  m_timer->stop();
  m_handleSpinBox->hide();
  QGraphicsItem::hoverLeaveEvent(he);
  int i;
  for (i = 0; i < m_port->getLinkCount(); i++) m_port->getLink(i)->updatePath();
}

//--------------------------------------------------------

void StageSchematicNodeDock::hoverMoveEvent(QGraphicsSceneHoverEvent *he) {
  QList<QGraphicsItem *> items = scene()->items(he->scenePos());
  if (!m_port->isHighlighted() && items.contains(m_port)) {
    m_port->highLight(true);
    eStageSchematicPortType type =
        (eStageSchematicPortType)getPort()->getType();
    if (!m_timer->isActive() && type != 103 && type != 104 && type != 105)
      m_timer->start();
  }
  QGraphicsItem::hoverMoveEvent(he);
  update();
}

//--------------------------------------------------------

void StageSchematicNodeDock::onPortClicked() {
  m_handleSpinBox->hide();
  m_timer->stop();
  int i;
  for (i = 0; i < m_port->getLinkCount(); i++) m_port->getLink(i)->updatePath();
}

//--------------------------------------------------------

void StageSchematicNodeDock::onPortReleased(const QPointF &pos) {
  QRectF portRect = m_port->boundingRect();
  portRect.moveTopLeft(m_port->scenePos());
  if (portRect.contains(pos)) m_timer->start();
}

//--------------------------------------------------------

void StageSchematicNodeDock::onTimeOut() {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (stageScene && stageScene->isShowLetterOnPortFlagEnabled())
    m_handleSpinBox->show();
  StageSchematicNodePort *port = getPort();
  int i;
  for (i = 0; i < port->getLinkCount(); i++) {
    SchematicLink *link      = port->getLink(i);
    SchematicPort *startPort = link->getStartPort();
    SchematicPort *endPort   = link->getEndPort();
    QPointF startPos         = startPort->getLinkEndPoint();
    QPointF endPos           = endPort->getLinkEndPoint();
    if (startPort == port) {
      if (port->getType() == 101 && startPos.x() > endPos.x())
        startPos = mapToScene(0, boundingRect().height() * 0.5);
      if (port->getType() == 102 && startPos.x() < endPos.x())
        startPos =
            mapToScene(boundingRect().width(), boundingRect().height() * 0.5);
      link->updatePath(startPos, endPos);
    } else {
      if (port->getType() == 101 && startPos.x() < endPos.x())
        endPos = mapToScene(0, boundingRect().height() * 0.5);
      if (port->getType() == 102 && startPos.x() > endPos.x())
        endPos =
            mapToScene(boundingRect().width(), boundingRect().height() * 0.5);
      link->updatePath(startPos, endPos);
    }
  }
}

//========================================================
//
// class StageSchematicSplineDock
//
//========================================================

StageSchematicSplineDock::StageSchematicSplineDock(SchematicNode *parent,
                                                   bool isParentPort,
                                                   eStageSchematicPortType type)
    : QGraphicsItem(parent), m_parent(parent), m_isParentPort(isParentPort) {
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  m_port = new StageSchematicSplinePort(this, type);
#if QT_VERSION >= 0x050000
  setAcceptHoverEvents(true);
#else
  setAcceptsHoverEvents(true);
#endif

  connect(m_port, SIGNAL(sceneChanged()), parent, SIGNAL(sceneChanged()));
  connect(m_port, SIGNAL(xsheetChanged()), parent, SIGNAL(xsheetChanged()));
  connect(this, SIGNAL(sceneChanged()), parent, SIGNAL(sceneChanged()));
}

//--------------------------------------------------------

StageSchematicSplineDock::~StageSchematicSplineDock() {}

//--------------------------------------------------------

QRectF StageSchematicSplineDock::boundingRect() const {
  QRectF portRect = m_port->boundingRect();
  portRect.moveTopLeft(QPointF(0, 0));
  return portRect;
}

//--------------------------------------------------------

void StageSchematicSplineDock::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {}

//========================================================
//
// class StageSchematicNode
//
//========================================================

StageSchematicNode::StageSchematicNode(StageSchematicScene *scene,
                                       TStageObject *obj, int width, int height,
                                       bool isGroup)
    : SchematicNode(scene), m_stageObject(obj), m_isGroup(isGroup) {
  m_stageObject->addRef();
  m_width  = width;
  m_height = height;

  // aggiunge le porte
  qreal y = m_height * 0.5 - 5;
  qreal x = m_width * 0.5 - 8;

  if (m_isGroup)
    m_splineDock =
        new StageSchematicSplineDock(this, false, eStageSplineGroupPort);
  else
    m_splineDock = new StageSchematicSplineDock(this, false, eStageSplinePort);
  addPort(-1, m_splineDock->getPort());
  m_splineDock->setPos(x, m_height);

  m_pathToggle = new SchematicToggle_SplineOptions(
      this, QPixmap(":Resources/schematic_spline_aim_rhomb.svg"),
      QPixmap(":Resources/schematic_spline_aim_square.svg"), 0);
  m_cpToggle = new SchematicToggle_SplineOptions(
      this, QPixmap(":Resources/schematic_spline_cp.svg"), 0);
  m_pathToggle->setSize(7, 7);
  m_cpToggle->setSize(7, 7);
  m_cpToggle->setPos(m_splineDock->pos() - QPointF(7, 0));
  m_pathToggle->setPos(m_cpToggle->pos() - QPointF(7, 0));

  if (m_stageObject->isPathEnabled()) {
    if (m_stageObject->isAimEnabled())
      m_pathToggle->setState(1);
    else
      m_pathToggle->setState(2);
  }
  m_cpToggle->setIsActive(m_stageObject->isUppkEnabled());

  connect(m_pathToggle, SIGNAL(stateChanged(int)), scene,
          SLOT(onPathToggled(int)));
  connect(m_cpToggle, SIGNAL(toggled(bool)), scene, SLOT(onCpToggled(bool)));

  if (!m_stageObject->getSpline()) {
    m_pathToggle->hide();
    m_cpToggle->hide();
  }

  if (m_isGroup)
    m_parentDock =
        new StageSchematicNodeDock(this, true, eStageParentGroupPort);
  else
    m_parentDock = new StageSchematicNodeDock(this, true, eStageParentPort);
  addPort(0, m_parentDock->getPort());
  if (scene->isShowLetterOnPortFlagEnabled())
    m_parentDock->setPos(-m_parentDock->boundingRect().width(), m_height - 15);
  else
    m_parentDock->setPos(0, 0);
  m_parentDock->getPort()->setHandle("B");

  StageSchematicNodeDock *childDock;
  if (m_isGroup)
    childDock = new StageSchematicNodeDock(this, false, eStageChildGroupPort);
  else
    childDock = new StageSchematicNodeDock(this, false, eStageChildPort);
  addPort(1, childDock->getPort());
  m_childDocks.append(childDock);
  if (scene->isShowLetterOnPortFlagEnabled())
    childDock->setPos(m_width, m_height - 15);
  else
    childDock->setPos(m_width - 18, 0);
  childDock->getPort()->setHandle("B");
}

//--------------------------------------------------------

StageSchematicNode::~StageSchematicNode() { m_stageObject->release(); }

//--------------------------------------------------------

void StageSchematicNode::setSchematicNodePos(const QPointF &pos) const {
  if (!m_stageObject->isGrouped() || m_stageObject->isGroupEditing())
    m_stageObject->setDagNodePos(TPointD(pos.x(), pos.y()));
  else {
    const StageSchematicGroupNode *groupNode =
        dynamic_cast<const StageSchematicGroupNode *>(this);
    assert(groupNode);
    groupNode->updateObjsDagPosition(TPointD(pos.x(), pos.y()));
  }
}

//--------------------------------------------------------

StageSchematicNodePort *StageSchematicNode::makeChildPort(
    const QString &label) {
  int i, n = m_childDocks.size();
  assert(n > 0);
  for (i = 0; i < n - 1; i++)
    if (m_childDocks[i]->getPort()->getHandle() == label)
      return m_childDocks[i]->getPort();
  StageSchematicNodeDock *dock = m_childDocks.back();
  dock->getPort()->setHandle(label);
  int k;
  QString newPortName;
  for (k = 0;; k++) {
    newPortName = QString(1, (char)('A' + k));
    int j;
    for (j = 0; j < n; j++) {
      QString name = m_childDocks[j]->getPort()->getHandle();
      if (name == newPortName) break;
    }
    if (j == n) break;
  }
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (stageScene && stageScene->isShowLetterOnPortFlagEnabled()) {
    StageSchematicNodeDock *newDock;
    if (m_isGroup)
      newDock = new StageSchematicNodeDock(this, false, eStageChildGroupPort);
    else
      newDock = new StageSchematicNodeDock(this, false, eStageChildPort);
    addPort(m_childDocks.size() + 1, newDock->getPort());
    m_childDocks.append(newDock);
    newDock->getPort()->setHandle(newPortName);
  }
  updateChildDockPositions();
  return dock->getPort();
}

//--------------------------------------------------------

StageSchematicNodePort *StageSchematicNode::makeParentPort(
    const QString &label) {
  m_parentDock->getPort()->setHandle(label);
  return m_parentDock->getPort();
}

//--------------------------------------------------------

void StageSchematicNode::setPosition(const QPointF &newPos) {
  if (m_stageObject->isGrouped() && m_stageObject->getEditingGroupId() != -1) {
    StageSchematicScene *stageScene =
        dynamic_cast<StageSchematicScene *>(scene());
    assert(stageScene);
    stageScene->updateNestedGroupEditors(this, newPos);
  } else
    setPos(newPos);
}

//--------------------------------------------------------

void StageSchematicNode::updateChildDockPositions() {
  int i, size = m_childDocks.size();
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (stageScene && stageScene->isShowLetterOnPortFlagEnabled()) {
    double portHeight = m_childDocks.at(0)->getPort()->boundingRect().height();
    double height     = size * portHeight;
    double y          = (m_height - 15 - portHeight * 0.5) + height * 0.5;
    for (i = 0; i < size; i++) {
      m_childDocks[i]->setPos(m_width, y);
      m_childDocks[i]->getPort()->updateLinksGeometry();
      y -= portHeight;
    }
  } else {
    double y = 18;
    for (i = 0; i < size; i++) {
      m_childDocks[i]->setPos(m_width - 18, m_height - y);
      m_childDocks[i]->getPort()->updateLinksGeometry();
      y += m_childDocks.at(0)->getPort()->boundingRect().height();
    }
  }
}

//--------------------------------------------------------

void StageSchematicNode::onClicked() {
  TStageObjectId id = m_stageObject->getId();
  emit currentObjectChanged(id, false);
  if (id.isColumn())
    emit currentColumnChanged(id.getIndex());
  else if (id.isCamera() || id.isPegbar() || id.isTable())
    emit editObject();
}

//--------------------------------------------------------

void StageSchematicNode::onHandleReleased() {
  int i;
  bool updated = false;
  for (i = m_childDocks.size() - 1; i >= 0; i--) {
    StageSchematicNodePort *port = m_childDocks[i]->getPort();
    assert(port);
    if (!port) continue;
    QString label = port->getHandle();
    int j;
    for (j = 0; j < m_childDocks.size(); j++) {
      StageSchematicNodePort *refPort = m_childDocks[j]->getPort();
      assert(refPort);
      if (!refPort) continue;
      if (refPort->getHandle() == label && i != j) {
        StageSchematicNodeDock *dock = m_childDocks[i];
        int k;
        if (port->getLinkCount() == 0 || refPort->getLinkCount() == 0) {
          updated = true;
          break;
        }
        for (k = port->getLinkCount() - 1; k >= 0; k--) {
          SchematicLink *link = port->getLink(k);
          assert(link);
          if (!link) continue;
          if (link->getStartPort() == port)
            link->setStartPort(refPort);
          else if (link->getEndPort() == port)
            link->setEndPort(refPort);
          port->removeLink(link);
          refPort->addLink(link);
        }
        m_childDocks.removeAt(i);
        delete dock;
        updateChildDockPositions();
        update();
        updated = true;
        break;
      }
    }
    if (updated) break;
  }

  if (updated) {
    QSet<QString> labels;
    StageSchematicNodePort *freePort = 0;
    for (i = 0; i < m_childDocks.size(); i++) {
      StageSchematicNodePort *port = m_childDocks[i]->getPort();
      assert(port);
      if (!port) continue;
      if (port->getLinkCount() == 0)
        freePort = port;
      else
        labels.insert(port->getHandle());
    }

    i = 0;
    while (labels.contains(QString(1, (char)('A' + i)))) i++;
    freePort->setHandle(QString(1, (char)('A' + i)));
    freePort->update();
  }
}

//========================================================
//
// class StageSchematicPegbarNode
//
//========================================================

StageSchematicPegbarNode::StageSchematicPegbarNode(StageSchematicScene *scene,
                                                   TStageObject *pegbar)
    : StageSchematicNode(scene, pegbar, 90, 18) {
  std::string name = m_stageObject->getFullName();
  std::string id   = m_stageObject->getId().toString();
  m_name           = QString::fromStdString(name);
  m_nameItem       = new SchematicName(this, 72, 20);
  m_nameItem->setName(m_name);
  m_nameItem->setPos(16, -1);
  m_nameItem->setZValue(2);
  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  m_nameItem->hide();

  m_pegbarPainter = new PegbarPainter(this, m_width, m_height, m_name);
  m_pegbarPainter->setZValue(1);

  QString toolTip =
      name == id ? m_name : m_name + " (" + QString::fromStdString(id) + ")";
  setToolTip(toolTip);
}

//--------------------------------------------------------

StageSchematicPegbarNode::~StageSchematicPegbarNode() {}

//--------------------------------------------------------

QRectF StageSchematicPegbarNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void StageSchematicPegbarNode::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {
  StageSchematicNode::paint(painter, option, widget);
}

//--------------------------------------------------------

void StageSchematicPegbarNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(18, 0, m_width - 36, 14);
  if (nameArea.contains(me->pos())) {
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    return;
  }
}

//--------------------------------------------------------

void StageSchematicPegbarNode::onNameChanged() {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_pegbarPainter->setName(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  TStageObjectId id = m_stageObject->getId();
  std::string strId = id.toString();
  std::string name  = m_name.toStdString();
  QString toolTip   = name == strId
                        ? m_name
                        : m_name + " (" + QString::fromStdString(strId) + ")";
  setToolTip(toolTip);
  if (id.isPegbar())
    TStageObjectCmd::rename(id, m_name.toStdString(),
                            stageScene->getXsheetHandle());
  update();
}

//========================================================
//
// class StageSchematicTableNode
//
//========================================================

StageSchematicTableNode::StageSchematicTableNode(StageSchematicScene *scene,
                                                 TStageObject *pegbar)
    : StageSchematicNode(scene, pegbar, 90, 18) {
  m_parentDock->hide();
  updateChildDockPositions();

  m_tablePainter = new TablePainter(this, m_width, m_height);
  m_tablePainter->setZValue(1);
}

//--------------------------------------------------------

StageSchematicTableNode::~StageSchematicTableNode() {}

//--------------------------------------------------------

QRectF StageSchematicTableNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void StageSchematicTableNode::paint(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    QWidget *widget) {
  StageSchematicNode::paint(painter, option, widget);
}

//--------------------------------------------------------

//========================================================
//
// class StageSchematicColumnNode
//
//========================================================

StageSchematicColumnNode::StageSchematicColumnNode(StageSchematicScene *scene,
                                                   TStageObject *pegbar)
    : StageSchematicNode(scene, pegbar, 90, 32), m_isOpened(true) {
  bool ret = true;

  assert(pegbar && pegbar->getId().isColumn());
  std::string name = m_stageObject->getName();

  m_name       = QString::fromStdString(name);
  m_resizeItem = new SchematicThumbnailToggle(this, m_stageObject->isOpened());
  m_resizeItem->setPos(2, 0);
  m_resizeItem->setZValue(2);
  ret = ret && connect(m_resizeItem, SIGNAL(toggled(bool)), this,
                       SLOT(onChangedSize(bool)));

  m_nameItem = new SchematicName(this, 54, 20);
  m_nameItem->setName(m_name);
  m_nameItem->setPos(16, -1);
  m_nameItem->setZValue(2);
  ret = ret &&
        connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  m_nameItem->hide();

  m_renderToggle =
      new SchematicToggle(this, QPixmap(":Resources/schematic_prev_eye.png"),
                          SchematicToggle::eIsParentColumn);
  ret = ret && connect(m_renderToggle, SIGNAL(toggled(bool)), this,
                       SLOT(onRenderToggleClicked(bool)));
  if (scene) {
    TXshColumn *column =
        scene->getXsheet()->getColumn(pegbar->getId().getIndex());
    if (column) m_renderToggle->setIsActive(column->isPreviewVisible());

    m_renderToggle->setPos(72, 0);
    m_renderToggle->setZValue(2);

    m_cameraStandToggle = new SchematicToggle(
        this, QPixmap(":Resources/schematic_table_view.png"),
        QPixmap(":Resources/schematic_table_view_transp.png"),
        SchematicToggle::eIsParentColumn | SchematicToggle::eEnableNullState);
    ret = ret && connect(m_cameraStandToggle, SIGNAL(stateChanged(int)), this,
                         SLOT(onCameraStandToggleClicked(int)));
    if (column)
      m_cameraStandToggle->setState(column->isCamstandVisible()
                                        ? (column->getOpacity() < 255 ? 2 : 1)
                                        : 0);

    m_cameraStandToggle->setPos(72, 7);
    m_cameraStandToggle->setZValue(2);
  }

  qreal y = 14;
  m_parentDock->setY(y);
  m_childDocks[0]->setY(y);

  m_columnPainter = new ColumnPainter(this, m_width, m_height, m_name);
  m_columnPainter->setZValue(1);

  int levelType;
  QString levelName;
  getLevelTypeAndName(levelType, levelName);
  setToolTip(QString("%1 : %2").arg(m_name, levelName));

  onChangedSize(m_stageObject->isOpened());
  assert(ret);

  if (levelType == ZERARYFX_XSHLEVEL || levelType == PLT_XSHLEVEL)
    m_resizeItem->hide();
}

//--------------------------------------------------------

StageSchematicColumnNode::~StageSchematicColumnNode() {}

//--------------------------------------------------------

QRectF StageSchematicColumnNode::boundingRect() const {
  // iwasawa
  if (m_isOpened)
    return QRectF(-5, -54, m_width + 10, m_height + 59);
  else
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void StageSchematicColumnNode::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {
  StageSchematicNode::paint(painter, option, widget);
  TStageObjectId id = m_stageObject->getId();
  assert(id.isColumn());
  QString colNumber = QString::number(id.getIndex() + 1);
  QFont font("Verdana", 8);
  painter->setFont(font);
  StageSchematicScene *scene = dynamic_cast<StageSchematicScene *>(m_scene);
  if (scene && scene->getCurrentObject() == id) painter->setPen(Qt::red);
  QFontMetrics metrix(font);
  int srcWidth  = metrix.width(colNumber);
  int srcHeight = metrix.height();
  QPointF pos(m_cameraStandToggle->pos() -
              QPointF(srcWidth + 1, -srcHeight + 3));
  painter->drawText(pos, colNumber);
}

//--------------------------------------------------------

QPixmap StageSchematicColumnNode::getPixmap() {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return QPixmap();
  int columnIndex = m_stageObject->getId().getIndex();
  TXsheet *xsh    = stageScene->getXsheet();
  if (xsh && !xsh->isColumnEmpty(columnIndex)) {
    int r0, r1;
    xsh->getCellRange(columnIndex, r0, r1);
    if (r1 >= r0) {
      TXshCell cell = xsh->getCell(r0, columnIndex);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl) {
        bool onDemand = false;
        if (Preferences::instance()->getColumnIconLoadingPolicy() ==
            Preferences::LoadOnDemand)
          onDemand =
              stageScene->getColumnHandle()->getColumnIndex() != columnIndex;
        return IconGenerator::instance()->getIcon(xl, cell.m_frameId, false,
                                                  onDemand);
      }
    }
  }
  return QPixmap();
}

//--------------------------------------------------------

void StageSchematicColumnNode::resize(bool maximized) {
  m_resizeItem->setIsDown(!maximized);
}

//--------------------------------------------------------

void StageSchematicColumnNode::getLevelTypeAndName(int &ltype,
                                                   QString &levelName) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) {
    ltype     = NO_XSHLEVEL;
    levelName = QString();
    return;
  }

  int columnIndex = m_stageObject->getId().getIndex();
  TXsheet *xsh    = stageScene->getXsheet();
  if (xsh && !xsh->isColumnEmpty(columnIndex)) {
    int r0, r1;
    xsh->getCellRange(columnIndex, r0, r1);
    if (r1 >= r0) {
      TXshCell cell = xsh->getCell(r0, columnIndex);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl) {
        ltype = cell.m_level->getType();

        // for Zerary Fx, display FxId
        if (ltype == ZERARYFX_XSHLEVEL) {
          TXshZeraryFxColumn *zColumn =
              dynamic_cast<TXshZeraryFxColumn *>(xsh->getColumn(columnIndex));
          if (zColumn) {
            TFx *fx   = zColumn->getZeraryColumnFx()->getZeraryFx();
            levelName = QString::fromStdWString(fx->getFxId());
            return;
          }
        }

        levelName = QString::fromStdWString(xl->getName());
        return;
      }
    }
  }

  ltype     = NO_XSHLEVEL;
  levelName = QString();
  return;
}
//--------------------------------------------------------

void StageSchematicColumnNode::onChangedSize(bool expand) {
  prepareGeometryChange();
  m_isOpened = expand;
  m_stageObject->setIsOpened(m_isOpened);
  m_height = 32;
  updatePortsPosition();
  updateLinksGeometry();
  update();
}

//--------------------------------------------------------

void StageSchematicColumnNode::updatePortsPosition() {
  qreal x = m_width * 0.5 - 5;
  m_splineDock->setPos(x, m_height);
  updateChildDockPositions();
}

//--------------------------------------------------------

void StageSchematicColumnNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;

  // do nothing for double-clicking a zerary Fx node
  TStageObjectId id  = m_stageObject->getId();
  TXshColumn *column = stageScene->getXsheet()->getColumn(id.getIndex());
  TXshZeraryFxColumn *fxColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (fxColumn) {
    return;
  }

  QRectF nameArea(14, 0, m_width - 15, 14);
  if (nameArea.contains(me->pos())) {
    m_name = QString::fromStdString(m_stageObject->getName());
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  }
}

//--------------------------------------------------------

void StageSchematicColumnNode::onNameChanged() {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_columnPainter->setName(m_name);

  int levelType;
  QString levelName;
  getLevelTypeAndName(levelType, levelName);
  setToolTip(QString("%1 : %2").arg(m_name, levelName));

  setFlag(QGraphicsItem::ItemIsSelectable, true);

  TStageObjectId id = m_stageObject->getId();
  if (!id.isColumn()) return;
  TXshColumn *column = stageScene->getXsheet()->getColumn(id.getIndex());
  TXshZeraryFxColumn *fxColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (fxColumn)
    TFxCommand::renameFx(fxColumn->getZeraryColumnFx(), m_name.toStdWString(),
                         stageScene->getXsheetHandle());
  else {
    TStageObjectCmd::rename(id, m_name.toStdString(),
                            stageScene->getXsheetHandle());
    update();
  }
}

//--------------------------------------------------------

void StageSchematicColumnNode::onRenderToggleClicked(bool isActive) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  TXshColumn *column =
      stageScene->getXsheet()->getColumn(m_stageObject->getId().getIndex());
  if (column) {
    column->setPreviewVisible(isActive);
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//--------------------------------------------------------

void StageSchematicColumnNode::onCameraStandToggleClicked(int state) {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  TXshColumn *column =
      stageScene->getXsheet()->getColumn(m_stageObject->getId().getIndex());
  if (column) {
    // column->setCamstandNextState();
    column->setCamstandVisible(!column->isCamstandVisible());
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//========================================================
//
// class StageSchematicCameraNode
//
//========================================================

StageSchematicCameraNode::StageSchematicCameraNode(StageSchematicScene *scene,
                                                   TStageObject *pegbar)
    : StageSchematicNode(scene, pegbar, 90, 18) {
  std::string name = m_stageObject->getFullName();
  m_name           = QString::fromStdString(name);

  m_nameItem = new SchematicName(this, 54, 20);
  m_nameItem->setName(m_name);

  m_nameItem->setPos(16, -2);
  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  m_nameItem->hide();
  m_nameItem->setZValue(2);

  m_cameraPainter = new CameraPainter(this, m_width, m_height, m_name);
  m_cameraPainter->setZValue(1);

  setToolTip(m_name);
}

//--------------------------------------------------------

StageSchematicCameraNode::~StageSchematicCameraNode() {}

//--------------------------------------------------------

QRectF StageSchematicCameraNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void StageSchematicCameraNode::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {
  StageSchematicNode::paint(painter, option, widget);
}

//--------------------------------------------------------

void StageSchematicCameraNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(0, -14, m_width, 14);
  if (nameArea.contains(me->pos())) {
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  } else {
    QAction *cameraSettingsPopup =
        CommandManager::instance()->getAction("MI_CameraStage");
    cameraSettingsPopup->trigger();
  }
}

//--------------------------------------------------------

void StageSchematicCameraNode::onNameChanged() {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_cameraPainter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  TStageObjectId id = m_stageObject->getId();
  if (id.isCamera())
    TStageObjectCmd::rename(id, m_name.toStdString(),
                            stageScene->getXsheetHandle());
  update();
}

//========================================================
//
// class StageSchematicSplineNode
//
//========================================================

StageSchematicSplineNode::StageSchematicSplineNode(StageSchematicScene *scene,
                                                   TStageObjectSpline *spline)
    : SchematicNode(scene), m_spline(spline), m_isOpened(false) {
  m_width  = 90;
  m_height = 18;
  assert(spline);

  m_dock = new StageSchematicSplineDock(this, true, eStageSplinePort);
  addPort(-1, m_dock->getPort());
  QRectF rect = m_dock->getPort()->boundingRect();
  m_dock->setPos(m_width * 0.5 - rect.width() * 0.5, -rect.height());

  m_resizeItem = new SchematicThumbnailToggle(this, m_spline->isOpened());
  m_resizeItem->setPos(2, 2);
  m_resizeItem->setZValue(2);
  connect(m_resizeItem, SIGNAL(toggled(bool)), this, SLOT(onChangedSize(bool)));

  std::string name = m_spline->getName();
  m_splineName     = QString::fromStdString(name);
  m_nameItem       = new SchematicName(this, 72, 20);
  m_nameItem->setName(m_splineName);
  m_nameItem->setPos(16, -1);
  m_nameItem->setZValue(2);
  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  m_nameItem->hide();

  m_splinePainter = new SplinePainter(this, m_width, m_height, m_splineName);
  m_splinePainter->setZValue(1);

  setToolTip(m_splineName);
  onChangedSize(m_spline->isOpened());
}

//--------------------------------------------------------

StageSchematicSplineNode::~StageSchematicSplineNode() {}

//--------------------------------------------------------

QRectF StageSchematicSplineNode::boundingRect() const {
  if (m_isOpened)
    return QRectF(-5, -5, m_width + 10, m_height + 59);
  else
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void StageSchematicSplineNode::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {
  SchematicNode::paint(painter, option, widget);
}

//--------------------------------------------------------

void StageSchematicSplineNode::onClicked() {
  if (m_dock->getPort()->getLinkCount() > 0) {
    StageSchematicNode *parentNode =
        dynamic_cast<StageSchematicNode *>(m_dock->getPort()->getLinkedNode(0));
    TStageObjectId parentId = parentNode->getStageObject()->getId();
    emit currentObjectChanged(parentId, true);
  }
}

//--------------------------------------------------------

void StageSchematicSplineNode::setSchematicNodePos(const QPointF &pos) const {
  m_spline->setDagNodePos(TPointD(pos.x(), pos.y()));
}

//--------------------------------------------------------

QPixmap StageSchematicSplineNode::getPixmap() {
  return IconGenerator::instance()->getIcon(m_spline);
  return QPixmap();
}

//--------------------------------------------------------

void StageSchematicSplineNode::resize(bool maximized) {
  m_resizeItem->setIsDown(!maximized);
}

//--------------------------------------------------------

void StageSchematicSplineNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(14, 0, m_width - 15, 14);
  if (nameArea.contains(me->pos())) {
    m_nameItem->setPlainText(m_splineName);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  }
}

//--------------------------------------------------------

void StageSchematicSplineNode::onNameChanged() {
  m_nameItem->hide();
  m_splineName = m_nameItem->toPlainText();
  m_splinePainter->setName(m_splineName);
  setToolTip(m_splineName);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  m_spline->setName(m_splineName.toStdString());
  update();
}

//--------------------------------------------------------

void StageSchematicSplineNode::onChangedSize(bool expanded) {
  prepareGeometryChange();
  m_isOpened = expanded;
  m_spline->setIsOpened(m_isOpened);
  m_height = 18;
  updateLinksGeometry();
  update();
}

//========================================================
//
// class StageSchematicSplineNode
//
//========================================================

StageSchematicGroupNode::StageSchematicGroupNode(
    StageSchematicScene *scene, TStageObject *root,
    const QList<TStageObject *> groupedObj)
    : StageSchematicNode(scene, root, 90, 18, true)
    , m_root(root)
    , m_groupedObj(groupedObj) {
  int i;
  for (i   = 0; i < m_groupedObj.size(); i++) m_groupedObj[i]->addRef();
  bool ret = true;
  std::wstring name = m_stageObject->getGroupName(false);
  m_name            = QString::fromStdWString(name);

  m_nameItem = new SchematicName(this, 72, 20);
  m_nameItem->setName(m_name);
  m_nameItem->setPos(16, -1);
  m_nameItem->setZValue(2);
  ret = ret &&
        connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  m_nameItem->hide();

  // m_childDocks[0]->setPos(m_width - 18, 0);
  // m_parentDock->setPos(0, 0);

  m_painter = new GroupPainter(this, m_width, m_height, m_name);
  m_painter->setZValue(1);

  setToolTip(m_name);

  assert(ret);
}

//--------------------------------------------------------

StageSchematicGroupNode::~StageSchematicGroupNode() {
  int i;
  for (i = 0; i < m_groupedObj.size(); i++) m_groupedObj[i]->release();
}

//--------------------------------------------------------

QRectF StageSchematicGroupNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//--------------------------------------------------------

void StageSchematicGroupNode::paint(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    QWidget *widget) {}

//--------------------------------------------------------

void StageSchematicGroupNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(14, 0, m_width - 15, 14);
  if (nameArea.contains(me->pos())) {
    m_name = QString::fromStdWString(m_root->getGroupName(false));
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  }
}

//--------------------------------------------------------

void StageSchematicGroupNode::onNameChanged() {
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_painter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  TStageObjectCmd::renameGroup(m_groupedObj, m_name.toStdWString(), false,
                               stageScene->getXsheetHandle());
  update();
}

//--------------------------------------------------------

int StageSchematicGroupNode::getGroupId() { return m_root->getGroupId(); }

//--------------------------------------------------------

void StageSchematicGroupNode::onChangedSize(bool expanded) {
  prepareGeometryChange();
  m_isOpened = expanded;
  int i;
  for (i = 0; i < m_groupedObj.size(); i++)
    m_groupedObj[i]->setIsOpened(m_isOpened);
  if (m_isOpened) {
    m_height = 59;
    m_nameItem->setPos(4, -6);
  } else {
    m_height = 14;
    m_nameItem->setPos(8, -2);
  }
  updatePortsPosition();
  updateLinksGeometry();
  update();
}

//--------------------------------------------------------

void StageSchematicGroupNode::updateObjsDagPosition(const TPointD &pos) const {
  TPointD oldPos = m_root->getDagNodePos();
  TPointD delta  = pos - oldPos;
  int i;
  for (i = 0; i < m_groupedObj.size(); i++) {
    // If the node position is unidentified, then leave the placement of it to
    // placeNode() function.
    if (m_groupedObj[i]->getDagNodePos() != TConst::nowhere)
      m_groupedObj[i]->setDagNodePos(m_groupedObj[i]->getDagNodePos() + delta);
  }
}

//--------------------------------------------------------

void StageSchematicGroupNode::updatePortsPosition() {
  qreal middleY = m_isOpened ? (14 + m_height) * 0.5 : m_height * 0.5;
  qreal y = middleY - (m_parentDock->getPort()->boundingRect().height() * 0.5);

  m_parentDock->setPos(-m_parentDock->boundingRect().width(), y);
  updateChildDockPositions();
}

//--------------------------------------------------------

void StageSchematicGroupNode::resize(bool maximized) {}
