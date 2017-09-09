

#include "toonzqt/fxschematicnode.h"

// TnzQt includes
#include "toonzqt/fxschematicscene.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/fxselection.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/fxiconmanager.h"

// TnzLib includes
#include "toonz/tcolumnfx.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txsheet.h"
#include "toonz/fxdag.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/fxcommand.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"

#include "tw/stringtable.h"

// TnzBase includes
#include "tfx.h"
#include "tfxattributes.h"
#include "tmacrofx.h"
#include "trasterfx.h"
#include "tpassivecachemanager.h"

// TnzCore includes
#include "tconst.h"

// Qt includes
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QGraphicsSceneMouseEvent>
#include <QDesktopWidget>

//********************************************************************************
//    Local namespace
//********************************************************************************

namespace {
bool isAInnerMacroFx(TFx *fx, TXsheet *xsh) {
  if (!fx) return false;
  TColumnFx *cfx      = dynamic_cast<TColumnFx *>(fx);
  TXsheetFx *xfx      = dynamic_cast<TXsheetFx *>(fx);
  TOutputFx *ofx      = dynamic_cast<TOutputFx *>(fx);
  TFxSet *internalFxs = xsh->getFxDag()->getInternalFxs();
  return !cfx && !xfx && !ofx && !internalFxs->containsFx(fx);
}

//-----------------------------------------------------

void drawCachedFxFlap(QPainter *painter, const QPointF &pos) {
  painter->save();
  painter->setPen(QColor(0, 0, 0, 255));
  painter->setBrush(QColor(120, 120, 120, 255));

  QPainterPath path(pos);
  path.lineTo(pos.x(), pos.y() - 10.0);
  path.lineTo(pos.x() - 10.0, pos.y());
  path.lineTo(pos);

  painter->drawPath(path);
  painter->setBrush(QColor(255, 255, 255, 255));

  QPointF newPos = pos + QPointF(-10.0, -10.0);
  path           = QPainterPath(newPos);
  path.lineTo(newPos.x(), newPos.y() + 10.0);
  path.lineTo(newPos.x() + 10.0, newPos.y());
  path.lineTo(newPos);

  painter->drawPath(path);
  painter->restore();
}

//-----------------------------------------------------

int getIndex(TFxPort *port, const std::vector<TFxPort *> &ports) {
  std::vector<TFxPort *>::const_iterator it =
      std::find(ports.begin(), ports.end(), port);
  if (it == ports.end()) return -1;
  return std::distance(ports.begin(), it);
}

//-----------------------------------------------------

int getInputPortIndex(TFxPort *port, TFx *fx) {
  int count = fx->getInputPortCount();
  int i;
  for (i = 0; i < count; i++) {
    if (port == fx->getInputPort(i)) return i;
  }
  return -1;
}
}  // namespace

//*****************************************************
//
// FxColumnPainter
//
//*****************************************************

FxColumnPainter::FxColumnPainter(FxSchematicColumnNode *parent, double width,
                                 double height, const QString &name)
    : QGraphicsItem(parent)
    , m_parent(parent)
    , m_name(name)
    , m_width(width)
    , m_height(height) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  connect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
          SLOT(onIconGenerated()));

  TLevelColumnFx *lcfx = dynamic_cast<TLevelColumnFx *>(parent->getFx());
  if (lcfx) {
    int index  = lcfx->getColumnIndex();
    QString id = QString("Col");
    setToolTip(id + QString::number(index + 1));

    FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
    if (!fxScene) return;

    TXsheet *xsh = fxScene->getXsheet();
    int r0, r1;
    xsh->getCellRange(index, r0, r1);
    if (r0 > r1) return;
    TXshCell firstCell = xsh->getCell(r0, index);
    m_type             = firstCell.m_level->getType();
  }
}

//-----------------------------------------------------

FxColumnPainter::~FxColumnPainter() {}

//-----------------------------------------------------

QRectF FxColumnPainter::boundingRect() const {
  if (m_parent->isOpened() && m_parent->isLargeScaled())
    return QRectF(-5, -54, m_width + 10, m_height + 59);
  else
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxColumnPainter::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
  int levelType;
  QString levelName;
  m_parent->getLevelTypeAndName(levelType, levelName);

  QLinearGradient linearGrad = getGradientByLevelType(levelType);

  if (!m_parent->isLargeScaled()) linearGrad.setFinalStop(0, 50);

  painter->setBrush(QBrush(linearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(0, 0, m_width, m_height);

  if (m_parent->isOpened() && m_parent->isLargeScaled()) {
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

  QRectF columnNameRect;
  QRectF levelNameRect;
  if (m_parent->isLargeScaled()) {
    columnNameRect = QRect(18, 2, 54, 14);
    levelNameRect  = QRectF(18, 16, 54, 14);
  } else {
    columnNameRect = QRect(4, 2, 78, 22);
    levelNameRect  = QRectF(4, 26, 78, 22);

    QFont fnt = painter->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    painter->setFont(fnt);
  }

  // column name
  if (!m_parent->isNameEditing()) {
    FxSchematicScene *sceneFx = dynamic_cast<FxSchematicScene *>(scene());
    if (!sceneFx) return;

    // if this is a current object
    if (sceneFx->getCurrentFx() == m_parent->getFx())
      painter->setPen(Qt::yellow);
    QString elidedName =
        elideText(m_name, painter->font(), columnNameRect.width());
    painter->drawText(columnNameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      elidedName);
  }

  // level name
  QString elidedName =
      elideText(levelName, painter->font(), levelNameRect.width());
  painter->drawText(levelNameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    elidedName);
}

//-----------------------------------------------------

QLinearGradient FxColumnPainter::getGradientByLevelType(int type) {
  QColor col1, col2, col3, col4, col5;
  int alpha = 200;
  switch (type) {
  case TZI_XSHLEVEL:
  case OVL_XSHLEVEL:
    col1 = QColor(209, 232, 234, alpha);
    col2 = QColor(121, 171, 181, alpha);
    col3 = QColor(98, 143, 165, alpha);
    col4 = QColor(33, 90, 118, alpha);
    col5 = QColor(122, 172, 173, alpha);
    break;
  case PLI_XSHLEVEL:
    col1 = QColor(236, 226, 182, alpha);
    col2 = QColor(199, 187, 95, alpha);
    col3 = QColor(180, 180, 67, alpha);
    col4 = QColor(130, 125, 15, alpha);
    col5 = QColor(147, 150, 28, alpha);
    break;
  case TZP_XSHLEVEL:
    col1 = QColor(196, 245, 196, alpha);
    col2 = QColor(111, 192, 105, alpha);
    col3 = QColor(63, 146, 99, alpha);
    col4 = QColor(32, 113, 86, alpha);
    col5 = QColor(117, 187, 166, alpha);
    break;
  case ZERARYFX_XSHLEVEL:
    col1 = QColor(232, 245, 196, alpha);
    col2 = QColor(130, 129, 93, alpha);
    col3 = QColor(113, 115, 81, alpha);
    col4 = QColor(55, 59, 25, alpha);
    col5 = QColor(144, 154, 111, alpha);
    break;
  case CHILD_XSHLEVEL:
    col1 = QColor(247, 208, 241, alpha);
    col2 = QColor(214, 154, 219, alpha);
    col3 = QColor(170, 123, 169, alpha);
    col4 = QColor(92, 52, 98, alpha);
    col5 = QColor(132, 111, 154, alpha);
    break;
  case MESH_XSHLEVEL:
    col1 = QColor(210, 140, 255, alpha);
    col2 = QColor(200, 130, 255, alpha);
    col3 = QColor(150, 80, 180, alpha);
    col4 = QColor(150, 80, 180, alpha);
    col5 = QColor(180, 120, 220, alpha);
    break;
  case UNKNOWN_XSHLEVEL:
  case NO_XSHLEVEL:
  default:
    col1 = QColor(227, 227, 227, alpha);
    col2 = QColor(174, 174, 174, alpha);
    col3 = QColor(123, 123, 123, alpha);
    col4 = QColor(61, 61, 61, alpha);
    col5 = QColor(127, 138, 137, alpha);
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

//-----------------------------------------------------
void FxColumnPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  QMenu menu(fxScene->views()[0]);

  QAction *copy = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut  = CommandManager::instance()->getAction("MI_Cut");

  bool enebleInsertAction =
      !m_parent->getFx()->getAttributes()->isGrouped() ||
      m_parent->getFx()->getAttributes()->isGroupEditing();
  if (enebleInsertAction) {
    // repeat the latest action
    if (cme->modifiers() & Qt::ControlModifier) {
      menu.addAction(fxScene->getAgainAction(AddFxContextMenu::Add |
                                             AddFxContextMenu::Insert));
      if (!menu.actions().isEmpty()) {
        menu.exec(cme->screenPos());
        return;
      }
    }
  }

  QMenu *insertMenu = fxScene->getInsertFxMenu();
  fxScene->initCursorScenePos();
  QMenu *addMenu = fxScene->getAddFxMenu();

  QAction *disconnectFromXSheet =
      new QAction(tr("&Disconnect from Xsheet"), &menu);
  connect(disconnectFromXSheet, SIGNAL(triggered()), fxScene,
          SLOT(onDisconnectFromXSheet()));

  QAction *connectToXSheet = new QAction(tr("&Connect to Xsheet"), &menu);
  connect(connectToXSheet, SIGNAL(triggered()), fxScene,
          SLOT(onConnectToXSheet()));

  QAction *addOutputFx =
      CommandManager::instance()->getAction("MI_NewOutputFx");

  QAction *addPaste = new QAction(tr("&Paste Add"), &menu);
  connect(addPaste, SIGNAL(triggered()), fxScene, SLOT(onAddPaste()));

  QAction *preview = new QAction(tr("&Preview"), &menu);
  connect(preview, SIGNAL(triggered()), fxScene, SLOT(onPreview()));

  bool cacheEnabled =
      TPassiveCacheManager::instance()->cacheEnabled(m_parent->getFx());

  QAction *cacheFx =
      new QAction(cacheEnabled ? tr("&Uncache Fx") : tr("&Cache FX"), &menu);
  if (cacheEnabled)
    connect(cacheFx, SIGNAL(triggered()), fxScene, SLOT(onUncacheFx()));
  else
    connect(cacheFx, SIGNAL(triggered()), fxScene, SLOT(onCacheFx()));

  QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");

  QAction *openSubxsh = new QAction(tr("&Open Subxsheet"), &menu);
  connect(openSubxsh, SIGNAL(triggered()), fxScene, SLOT(onOpenSubxsheet()));

  QAction *explodeChild =
      CommandManager::instance()->getAction("MI_ExplodeChild");

  QAction *group = CommandManager::instance()->getAction("MI_Group");

  menu.addMenu(insertMenu);
  menu.addMenu(addMenu);
  menu.addSeparator();
  if (!m_parent->getFx()->getAttributes()->isGrouped()) {
    menu.addAction(copy);
    menu.addAction(cut);
    menu.addAction(addPaste);
  }
  menu.addSeparator();
  if (fxScene->getXsheet()->getFxDag()->getTerminalFxs()->containsFx(
          m_parent->getFx()))
    menu.addAction(disconnectFromXSheet);
  else
    menu.addAction(connectToXSheet);
  if (!m_parent->getFx()->getAttributes()->isGrouped())
    menu.addAction(addOutputFx);
  menu.addAction(preview);
  menu.addAction(cacheFx);
  menu.addSeparator();
  if (enebleInsertAction) {
    menu.addAction(collapse);
  }

  TFrameHandle *frameHandle = fxScene->getFrameHandle();
  if (frameHandle->getFrameType() == TFrameHandle::SceneFrame) {
    TLevelColumnFx *colFx = dynamic_cast<TLevelColumnFx *>(m_parent->getFx());
    assert(colFx);
    int col       = colFx->getColumnIndex();
    int fr        = frameHandle->getFrame();
    TXsheet *xsh  = fxScene->getXsheet();
    TXshCell cell = xsh->getCell(fr, col);
    if (dynamic_cast<TXshChildLevel *>(cell.m_level.getPointer())) {
      menu.addAction(openSubxsh);
      menu.addAction(explodeChild);
    }
  }
  menu.addSeparator();
  menu.addAction(group);

  if (m_type == OVL_XSHLEVEL || m_type == TZP_XSHLEVEL ||
      m_type == PLI_XSHLEVEL) {
    QAction *viewFileCommand =
        CommandManager::instance()->getAction("MI_ViewFile");
    menu.addSeparator();
    menu.addAction(viewFileCommand);
    QAction *levelSettings =
        CommandManager::instance()->getAction("MI_LevelSettings");
    menu.addAction(levelSettings);
  }

  menu.exec(cme->screenPos());
}

//-----------------------------------------------------

void FxColumnPainter::onIconGenerated() {
  if (m_type != OVL_XSHLEVEL) return;

  TLevelColumnFx *lcfx = dynamic_cast<TLevelColumnFx *>(m_parent->getFx());
  if (lcfx) {
    int index  = lcfx->getColumnIndex();
    QString id = QString("Col");
    setToolTip(id + QString::number(index + 1));

    FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
    if (!fxScene) return;

    TXsheet *xsh = fxScene->getXsheet();
    int r0, r1;
    xsh->getCellRange(index, r0, r1);
    if (r0 > r1) return;
    TXshCell firstCell = xsh->getCell(r0, index);
    int type           = firstCell.m_level->getType();
    if (m_type != type) {
      m_type = type;
      update();
    }
  }
}

//*****************************************************
//
// FxPalettePainter
//
//*****************************************************

FxPalettePainter::FxPalettePainter(FxSchematicPaletteNode *parent, double width,
                                   double height, const QString &name)
    : QGraphicsItem(parent)
    , m_parent(parent)
    , m_name(name)
    , m_width(width)
    , m_height(height) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  TLevelColumnFx *lcfx = dynamic_cast<TLevelColumnFx *>(parent->getFx());
  if (lcfx) {
    int index  = lcfx->getColumnIndex() + 1;
    QString id = QString("Col");
    setToolTip(id + QString::number(index));
  }
}

//-----------------------------------------------------

FxPalettePainter::~FxPalettePainter() {}

//-----------------------------------------------------

QRectF FxPalettePainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxPalettePainter::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
  QPixmap palettePm = QPixmap(":Resources/schematic_palette.png");

  int alpha = 200;

  QLinearGradient paletteLinearGrad(
      QPointF(0, 0), QPointF(0, (m_parent->isLargeScaled()) ? 32 : 50));
  paletteLinearGrad.setColorAt(0, QColor(42, 171, 154, alpha));
  paletteLinearGrad.setColorAt(0.2, QColor(15, 62, 56, alpha));
  paletteLinearGrad.setColorAt(0.9, QColor(15, 62, 56, alpha));
  paletteLinearGrad.setColorAt(1, QColor(33, 95, 90, alpha));

  painter->setBrush(QBrush(paletteLinearGrad));

  painter->setPen(Qt::NoPen);

  if (m_parent->isLargeScaled())
    painter->drawRoundRect(QRectF(0, 0, m_width, m_height), 35, 99);
  else
    painter->drawRoundRect(QRectF(0, 0, m_width, m_height), 10, 30);

  // draw icon
  QRect paletteRect;
  QRectF idRect;
  QRectF palNameRect;
  if (m_parent->isLargeScaled()) {
    paletteRect = QRect(-3, -1, 20, 16);
    idRect      = QRectF(18, 2, 54, 14);
    palNameRect = QRectF(18, 16, 54, 14);
  } else {
    paletteRect = QRect(4, -6, 35, 28);
    idRect      = QRectF(25, 2, 49, 22);
    palNameRect = QRectF(4, 26, 78, 22);

    QFont fnt = painter->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    painter->setFont(fnt);
  }

  painter->drawPixmap(paletteRect, palettePm);

  //! draw the name only if it is not editing
  painter->setPen(Qt::white);

  if (!m_parent->isNameEditing()) {
    FxSchematicScene *sceneFx = dynamic_cast<FxSchematicScene *>(scene());
    if (!sceneFx) return;
    if (sceneFx->getCurrentFx() == m_parent->getFx())
      painter->setPen(Qt::yellow);

    int w = idRect.width();

    if (m_parent->isLargeScaled()) {
      QString elidedName = elideText(m_name, painter->font(), w);
      painter->drawText(idRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
    } else
      painter->drawText(idRect, Qt::AlignRight | Qt::AlignVCenter,
                        QString::number(m_parent->getColumnIndex() + 1));
  }

  // level name
  QString paletteName = m_parent->getPaletteName();
  QString elidedName =
      elideText(paletteName, painter->font(), palNameRect.width());
  painter->drawText(palNameRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
}

//-----------------------------------------------------

void FxPalettePainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  QMenu menu(fxScene->views()[0]);

  QAction *disconnectFromXSheet =
      new QAction(tr("&Disconnect from Xsheet"), &menu);
  connect(disconnectFromXSheet, SIGNAL(triggered()), fxScene,
          SLOT(onDisconnectFromXSheet()));

  QAction *connectToXSheet = new QAction(tr("&Connect to Xsheet"), &menu);
  connect(connectToXSheet, SIGNAL(triggered()), fxScene,
          SLOT(onConnectToXSheet()));

  QAction *preview = new QAction(tr("&Preview"), &menu);
  connect(preview, SIGNAL(triggered()), fxScene, SLOT(onPreview()));

  QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");

  QAction *group = CommandManager::instance()->getAction("MI_Group");

  bool enebleInsertAction =
      !m_parent->getFx()->getAttributes()->isGrouped() ||
      m_parent->getFx()->getAttributes()->isGroupEditing();

  if (enebleInsertAction) {
    if (fxScene->getXsheet()->getFxDag()->getTerminalFxs()->containsFx(
            m_parent->getFx()))
      menu.addAction(disconnectFromXSheet);
    else
      menu.addAction(connectToXSheet);
    menu.addAction(preview);
    menu.addSeparator();
    menu.addAction(collapse);
    menu.addSeparator();
  }
  menu.addAction(group);

  menu.exec(cme->screenPos());
}

//*****************************************************
//
// FxPainter
//
//*****************************************************

FxPainter::FxPainter(FxSchematicNode *parent, double width, double height,
                     const QString &name, eFxType type, std::string fxType)
    : QGraphicsItem(parent)
    , m_parent(parent)
    , m_name(name)
    , m_width(width)
    , m_height(height)
    , m_type(type)
    , m_fxType(fxType) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);

  switch (m_type) {
  case eNormalFx:
  case eMacroFx:
  case eNormalLayerBlendingFx:
  case eNormalMatteFx:
  case eNormalImageAdjustFx:
    m_label = QString::fromStdWString(
        TStringTable::translate(parent->getFx()->getFxType()));
    setToolTip(QString::fromStdWString(parent->getFx()->getFxId()));
    break;
  case eZeraryFx: {
    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(parent->getFx());
    if (zfx) {
      TFx *zeraryFx = zfx->getZeraryFx();
      if (zeraryFx) {
        m_label = QString::fromStdWString(
            TStringTable::translate(zeraryFx->getFxType()));
        setToolTip(QString::fromStdWString(zeraryFx->getFxId()));
      }
    }
    break;
  }

  case eGroupedFx:
    m_label = QString("Group ") +
              QString::number(parent->getFx()->getAttributes()->getGroupId());
    setToolTip(m_label);
    break;
  }
}

//-----------------------------------------------------

FxPainter::~FxPainter() {}

//-----------------------------------------------------

QRectF FxPainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxPainter::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget) {
  // if the scale is small, display with fx icons
  if (!m_parent->isLargeScaled()) {
    paint_small(painter);
    return;
  }

  if (m_type == eGroupedFx) {
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
  }

  // draw base rect
  QLinearGradient linearGrad = getGradientByLevelType(m_type);

  painter->setBrush(QBrush(linearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(0, 0, m_width, m_height);

  // draw diagonal line for disabled fx
  bool is_enabled = m_parent->isEnabled();
  if (!is_enabled) {
    painter->save();
    painter->setPen(QColor(255, 0, 0, 255));
    painter->drawLine(QPointF(0, m_height), QPointF(m_width, 0));
    painter->drawLine(QPointF(0, 0), QPointF(m_width, m_height));
    painter->restore();
  }

  QFont columnFont(painter->font());
  columnFont.setPixelSize(columnFont.pixelSize() - 1);
  painter->setFont(columnFont);

  // draw fxId in the bottom part
  painter->setPen(Qt::white);
  if (m_type != eGroupedFx) {
    // for zerary fx
    QString label;
    if (m_type == eZeraryFx) {
      TZeraryColumnFx *zcFx =
          dynamic_cast<TZeraryColumnFx *>(m_parent->getFx());
      if (!zcFx) return;
      label = QString::fromStdWString(zcFx->getZeraryFx()->getFxId());
    } else
      label = QString::fromStdWString(m_parent->getFx()->getFxId());
    label   = elideText(label, painter->font(), m_width - 21);
    painter->drawText(QRectF(3, 16, m_width - 21, 14),
                      Qt::AlignLeft | Qt::AlignVCenter, label);
  }

  // draw user-defined fx name in the upper part
  //! draw the name only if it is not editing
  if (!m_parent->isNameEditing()) {
    FxSchematicScene *sceneFx = dynamic_cast<FxSchematicScene *>(scene());
    if (!sceneFx) return;
    if (sceneFx->getCurrentFx() == m_parent->getFx())
      painter->setPen(Qt::yellow);
    QRectF rect(3, 2, m_width - 21, 14);
    int w              = rect.width();
    QString elidedName = elideText(m_name, painter->font(), w);
    painter->drawText(rect, elidedName);
  }
}
//-----------------------------------------------------
/*! return the gradient pattern according to the type of Fx
*/
QLinearGradient FxPainter::getGradientByLevelType(eFxType type) {
  QColor col1, col2, col3, col4, col5;
  int alpha = 200;
  switch (type) {
  case eNormalFx:
    col1 = QColor(129, 162, 188, alpha);
    col2 = QColor(109, 138, 166, alpha);
    col3 = QColor(94, 120, 150, alpha);
    col4 = QColor(94, 120, 150, alpha);
    col5 = QColor(52, 63, 104, alpha);
    break;
  case eZeraryFx:
    col1 = QColor(232, 245, 196, alpha);
    col2 = QColor(130, 129, 93, alpha);
    col3 = QColor(113, 115, 81, alpha);
    col4 = QColor(55, 59, 25, alpha);
    col5 = QColor(144, 154, 111, alpha);
    break;
  case eMacroFx:
    col1 = QColor(165, 117, 161, alpha);
    col2 = QColor(146, 99, 139, alpha);
    col3 = QColor(132, 86, 123, alpha);
    col4 = QColor(132, 86, 123, alpha);
    col5 = QColor(89, 46, 92, alpha);
    break;
  case eGroupedFx:
    col1 = QColor(104, 191, 211, alpha);
    col2 = QColor(91, 168, 192, alpha);
    col3 = QColor(76, 148, 177, alpha);
    col4 = QColor(76, 148, 177, alpha);
    col5 = QColor(43, 91, 139, alpha);
    break;
  case eNormalImageAdjustFx:
    col1 = QColor(97, 95, 143, alpha);
    col2 = QColor(95, 92, 140, alpha);
    col3 = QColor(88, 84, 131, alpha);
    col4 = QColor(88, 84, 131, alpha);
    col5 = QColor(69, 56, 100, alpha);
    break;
  case eNormalLayerBlendingFx:
    col1 = QColor(75, 127, 133, alpha);
    col2 = QColor(65, 112, 122, alpha);
    col3 = QColor(60, 108, 118, alpha);
    col4 = QColor(60, 108, 118, alpha);
    col5 = QColor(38, 71, 91, alpha);
    break;
  case eNormalMatteFx:
    col1 = QColor(195, 117, 116, alpha);
    col2 = QColor(188, 111, 109, alpha);
    col3 = QColor(181, 103, 103, alpha);
    col4 = QColor(181, 103, 103, alpha);
    col5 = QColor(161, 86, 84, alpha);
    break;
  default:
    break;
  }

  QLinearGradient linearGrad(QPointF(0, 0), QPointF(0, 32));
  linearGrad.setColorAt(0, col1);
  linearGrad.setColorAt(0.08, col2);
  linearGrad.setColorAt(0.20, col3);
  linearGrad.setColorAt(0.23, col4);
  linearGrad.setColorAt(0.9, col4);
  linearGrad.setColorAt(1, col5);

  if (type == eGroupedFx) {
    linearGrad.setColorAt(0.6, col4);
    linearGrad.setColorAt(0.699, col5);
    linearGrad.setColorAt(0.7, col4);
    linearGrad.setColorAt(0.799, col5);
    linearGrad.setColorAt(0.8, col4);
    linearGrad.setColorAt(0.899, col5);
  }
  return linearGrad;
}

//-----------------------------------------------------

void FxPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  QMenu menu(scene()->views()[0]);

  TFx *fx = m_parent->getFx();
  bool isInternalFx =
      fxScene->getXsheet()->getFxDag()->getInternalFxs()->containsFx(fx) ||
      m_type == eGroupedFx;
  bool enableGroupAction = m_parent->isA(eZeraryFx) || isInternalFx;
  bool enableInsertAction =
      enableGroupAction && (fx->getAttributes()->isGroupEditing() ||
                            !fx->getAttributes()->isGrouped());
  if (enableInsertAction) {
    // repeat the last command
    if (cme->modifiers() & Qt::ControlModifier) {
      int commands = (m_type != eGroupedFx)
                         ? AddFxContextMenu::Add | AddFxContextMenu::Insert |
                               AddFxContextMenu::Replace
                         : AddFxContextMenu::Add | AddFxContextMenu::Insert;
      menu.addAction(fxScene->getAgainAction(commands));
      if (!menu.actions().isEmpty()) {
        menu.exec(cme->screenPos());
        return;
      }
    }
  }

  QMenu *insertMenu    = fxScene->getInsertFxMenu();
  QMenu *replacefxMenu = fxScene->getReplaceFxMenu();
  fxScene->initCursorScenePos();
  QMenu *addMenu = fxScene->getAddFxMenu();

  QAction *fxEditorPopup =
      CommandManager::instance()->getAction("MI_FxParamEditor");
  QAction *copy    = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut     = CommandManager::instance()->getAction("MI_Cut");
  QAction *group   = CommandManager::instance()->getAction("MI_Group");
  QAction *ungroup = CommandManager::instance()->getAction("MI_Ungroup");

  QAction *editGroup = new QAction(tr("&Open Group"), &menu);
  connect(editGroup, SIGNAL(triggered()), fxScene, SLOT(onEditGroup()));

  QAction *replacePaste = new QAction(tr("&Paste Replace"), &menu);
  connect(replacePaste, SIGNAL(triggered()), fxScene, SLOT(onReplacePaste()));

  QAction *addPaste = new QAction(tr("&Paste Add"), &menu);
  connect(addPaste, SIGNAL(triggered()), fxScene, SLOT(onAddPaste()));

  QAction *addOutputFx =
      CommandManager::instance()->getAction("MI_NewOutputFx");

  QAction *deleteFx = new QAction(tr("&Delete"), &menu);
  connect(deleteFx, SIGNAL(triggered()), fxScene, SLOT(onDeleteFx()));

  QAction *disconnectFromXSheet =
      new QAction(tr("&Disconnect from Xsheet"), &menu);
  connect(disconnectFromXSheet, SIGNAL(triggered()), fxScene,
          SLOT(onDisconnectFromXSheet()));

  QAction *connectToXSheet = new QAction(tr("&Connect to Xsheet"), &menu);
  connect(connectToXSheet, SIGNAL(triggered()), fxScene,
          SLOT(onConnectToXSheet()));

  QAction *duplicateFx = new QAction(tr("&Create Linked FX"), &menu);
  connect(duplicateFx, SIGNAL(triggered()), fxScene, SLOT(onDuplicateFx()));

  QAction *unlinkFx = new QAction(tr("&Unlink"), &menu);
  connect(unlinkFx, SIGNAL(triggered()), fxScene, SLOT(onUnlinkFx()));

  QAction *macroFx = new QAction(tr("&Make Macro FX"), &menu);
  connect(macroFx, SIGNAL(triggered()), fxScene, SLOT(onMacroFx()));

  QAction *explodeMacroFx = new QAction(tr("&Explode Macro FX"), &menu);
  connect(explodeMacroFx, SIGNAL(triggered()), fxScene,
          SLOT(onExplodeMacroFx()));

  QAction *openMacroFx = new QAction(tr("&Open Macro FX"), &menu);
  connect(openMacroFx, SIGNAL(triggered()), fxScene, SLOT(onOpenMacroFx()));

  QAction *savePresetFx = new QAction(tr("&Save As Preset..."), &menu);
  connect(savePresetFx, SIGNAL(triggered()), fxScene, SLOT(onSavePresetFx()));

  QAction *preview = new QAction(tr("&Preview"), &menu);
  connect(preview, SIGNAL(triggered()), fxScene, SLOT(onPreview()));

  bool cacheEnabled = m_parent->isCached();

  QAction *cacheFx =
      new QAction(cacheEnabled ? tr("&Uncache FX") : tr("&Cache FX"), &menu);
  if (cacheEnabled)
    connect(cacheFx, SIGNAL(triggered()), fxScene, SLOT(onUncacheFx()));
  else
    connect(cacheFx, SIGNAL(triggered()), fxScene, SLOT(onCacheFx()));

  QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");

  TZeraryColumnFx *zsrc = dynamic_cast<TZeraryColumnFx *>(fx);
  // if(m_type != eGroupedFx)
  // {
  if (enableInsertAction) {
    menu.addMenu(insertMenu);
    menu.addMenu(addMenu);
    if (m_type != eGroupedFx) menu.addMenu(replacefxMenu);
  }
  if (m_type != eGroupedFx) menu.addAction(fxEditorPopup);
  if (enableInsertAction) {
    menu.addSeparator();
    menu.addAction(copy);
    menu.addAction(cut);
    if (m_type != eGroupedFx && !fx->getAttributes()->isGrouped()) {
      menu.addAction(replacePaste);
      menu.addAction(addPaste);
    }
    menu.addAction(deleteFx);

    menu.addSeparator();
    if (fxScene->getXsheetHandle()
            ->getXsheet()
            ->getFxDag()
            ->getTerminalFxs()
            ->containsFx(m_parent->getFx()))
      menu.addAction(disconnectFromXSheet);
    else
      menu.addAction(connectToXSheet);
    menu.addAction(duplicateFx);
    if (zsrc && zsrc->getZeraryFx() &&
            zsrc->getZeraryFx()->getLinkedFx() != zsrc->getZeraryFx() ||
        fx->getLinkedFx() != fx)
      menu.addAction(unlinkFx);
  }
  menu.addSeparator();
  //}
  if (!fx->getAttributes()->isGrouped()) menu.addAction(addOutputFx);
  menu.addAction(preview);
  if (enableGroupAction) menu.addAction(cacheFx);
  menu.addSeparator();
  if (m_type != eGroupedFx && enableInsertAction) {
    menu.addAction(macroFx);
    if (scene()->selectedItems().size() == 1 && m_parent->isA(eMacroFx)) {
      menu.addAction(explodeMacroFx);
      menu.addAction(openMacroFx);
    }
    menu.addAction(savePresetFx);
    if (zsrc) {
      menu.addSeparator();
      menu.addAction(collapse);
    }
    menu.addSeparator();
  }
  if (enableGroupAction) {
    menu.addAction(group);
    if (m_type == eGroupedFx) {
      menu.addAction(ungroup);
      menu.addAction(editGroup);
    }
  }
  menu.exec(cme->screenPos());
}

//------------------------------------------------------------------------
/*! for small-scaled display
*/
void FxPainter::paint_small(QPainter *painter) {
  if (m_type == eGroupedFx) {
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
  }

  QLinearGradient linearGrad = getGradientByLevelType(m_type);

  linearGrad.setFinalStop(QPointF(0, 50));

  painter->setBrush(QBrush(linearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(0, 0, m_width, m_height);

  if (m_type == eGroupedFx || m_type == eZeraryFx) {
    QFont fnt = painter->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    painter->setFont(fnt);
    painter->setPen(Qt::white);
    FxSchematicScene *sceneFx = dynamic_cast<FxSchematicScene *>(scene());
    if (!sceneFx) return;
    if (sceneFx->getCurrentFx() == m_parent->getFx())
      painter->setPen(Qt::yellow);
  }

  if (m_type == eGroupedFx) {
    if (!m_parent->isNameEditing()) {
      QRectF rect(14, 2, 68, 22);
      int w              = rect.width();
      QString elidedName = elideText(m_name, painter->font(), w);
      painter->drawText(rect, elidedName);
    }
  } else {
    painter->drawPixmap(16, 6, 38, 38,
                        FxIconPixmapManager::instance()->getFxIconPm(m_fxType));
  }

  // show column number on the right side of icon
  if (m_type == eZeraryFx) {
    FxSchematicZeraryNode *zeraryNode =
        dynamic_cast<FxSchematicZeraryNode *>(m_parent);
    if (zeraryNode) {
      QRect idRect(30, 10, 46, 38);
      painter->drawText(idRect, Qt::AlignRight | Qt::AlignBottom,
                        QString::number(zeraryNode->getColumnIndex() + 1));
    }
  }

  // diagonal line for disabled fx
  bool is_enabled = false;
  if (TZeraryColumnFx *zcFx =
          dynamic_cast<TZeraryColumnFx *>(m_parent->getFx()))
    is_enabled = zcFx->getColumn()->isPreviewVisible();
  else
    is_enabled = m_parent->getFx()->getAttributes()->isEnabled();
  if (!is_enabled) {
    painter->save();
    painter->setPen(QColor(255, 0, 0, 255));
    painter->drawLine(QPointF(10, m_height), QPointF(m_width - 10, 0));
    painter->drawLine(QPointF(10, 0), QPointF(m_width - 10, m_height));
    painter->restore();
  }
}

//*****************************************************
//
// FxXSheetPainter
//
//*****************************************************

FxXSheetPainter::FxXSheetPainter(FxSchematicXSheetNode *parent, double width,
                                 double height)
    : QGraphicsItem(parent)
    , m_width(width)
    , m_height(height)
    , m_parent(parent) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
}

//-----------------------------------------------------

FxXSheetPainter::~FxXSheetPainter() {}

//-----------------------------------------------------

QRectF FxXSheetPainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxXSheetPainter::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
  int alpha = 200;
  QLinearGradient xsheetLinearGrad(
      QPointF(0, 0), QPointF(0, (m_parent->isLargeScaled()) ? 18 : 36));
  xsheetLinearGrad.setColorAt(0, QColor(152, 146, 188, alpha));
  xsheetLinearGrad.setColorAt(0.14, QColor(107, 106, 148, alpha));
  xsheetLinearGrad.setColorAt(0.35, QColor(96, 96, 138, alpha));
  xsheetLinearGrad.setColorAt(0.4, QColor(63, 67, 99, alpha));
  xsheetLinearGrad.setColorAt(0.8, QColor(63, 67, 99, alpha));
  xsheetLinearGrad.setColorAt(1, QColor(101, 105, 143, alpha));

  painter->setBrush(QBrush(xsheetLinearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(QRectF(0, 0, m_width, m_height));

  painter->setPen(Qt::white);
  if (m_parent->isLargeScaled()) {
    // Draw the name
    QRectF rect(18, 0, 54, 18);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
                      QString("XSheet"));
  }
  // small scaled
  else {
    QRectF rect(28, 4, 32, 32);
    QFont fnt = painter->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    painter->setFont(fnt);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, QString("X"));
  }
}

//-----------------------------------------------------

void FxXSheetPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  QMenu menu(fxScene->views()[0]);

  if (cme->modifiers() & Qt::ControlModifier) {
    menu.addAction(fxScene->getAgainAction(AddFxContextMenu::Add |
                                           AddFxContextMenu::Insert));
    if (!menu.actions().isEmpty()) {
      menu.exec(cme->screenPos());
      return;
    }
  }

  QMenu *insertMenu = fxScene->getInsertFxMenu();
  fxScene->initCursorScenePos();
  QMenu *addMenu = fxScene->getAddFxMenu();

  QAction *addOutputFx =
      CommandManager::instance()->getAction("MI_NewOutputFx");

  QAction *addPaste = new QAction(tr("&Paste Add"), &menu);
  connect(addPaste, SIGNAL(triggered()), fxScene, SLOT(onAddPaste()));

  QAction *preview = new QAction(tr("&Preview"), &menu);
  connect(preview, SIGNAL(triggered()), fxScene, SLOT(onPreview()));

  menu.addMenu(insertMenu);
  menu.addMenu(addMenu);
  menu.addSeparator();
  menu.addAction(addPaste);
  menu.addAction(addOutputFx);
  menu.addAction(preview);
  menu.exec(cme->screenPos());
}

//*****************************************************
//
// FxOutputPainter
//
//*****************************************************

FxOutputPainter::FxOutputPainter(FxSchematicOutputNode *parent, double width,
                                 double height)
    : QGraphicsItem(parent)
    , m_width(width)
    , m_height(height)
    , m_parent(parent) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFx *currentOutFx = fxScene->getXsheet()->getFxDag()->getCurrentOutputFx();
  m_isActive        = currentOutFx == parent->getFx();
}

//-----------------------------------------------------

FxOutputPainter::~FxOutputPainter() {}

//-----------------------------------------------------

QRectF FxOutputPainter::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxOutputPainter::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
  int alpha = 200;
  QLinearGradient outputLinearGrad(
      QPointF(0, 0), QPointF(0, (m_parent->isLargeScaled()) ? 18 : 36));
  if (m_isActive) {
    outputLinearGrad.setColorAt(0, QColor(115, 190, 224, alpha));
    outputLinearGrad.setColorAt(0.14, QColor(51, 132, 208, alpha));
    outputLinearGrad.setColorAt(0.35, QColor(39, 118, 196, alpha));
    outputLinearGrad.setColorAt(0.4, QColor(18, 82, 153, alpha));
    outputLinearGrad.setColorAt(0.8, QColor(18, 82, 153, alpha));
    outputLinearGrad.setColorAt(1, QColor(68, 119, 169, alpha));
  } else {
    outputLinearGrad.setColorAt(0, QColor(183, 197, 196, alpha));
    outputLinearGrad.setColorAt(0.14, QColor(138, 157, 160, alpha));
    outputLinearGrad.setColorAt(0.35, QColor(125, 144, 146, alpha));
    outputLinearGrad.setColorAt(0.4, QColor(80, 94, 97, alpha));
    outputLinearGrad.setColorAt(0.8, QColor(80, 94, 97, alpha));
    outputLinearGrad.setColorAt(1, QColor(128, 140, 142, alpha));
  }

  painter->setBrush(QBrush(outputLinearGrad));
  painter->setPen(Qt::NoPen);
  painter->drawRect(QRectF(0, 0, m_width, m_height));

  painter->setPen(Qt::white);
  if (m_parent->isLargeScaled()) {
    // Draw the name
    QRectF rect(18, 0, 72, 18);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
                      QString("Output"));
  }
  // small view
  else {
    QRectF rect(16, 0, 50, 36);
    QFont fnt = painter->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    painter->setFont(fnt);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, QString("Out"));
  }
}

//-----------------------------------------------------

void FxOutputPainter::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  QMenu menu(fxScene->views()[0]);
  if (fxScene->getXsheet()->getFxDag()->getOutputFxCount() > 1) {
    QAction *removeOutput = new QAction(tr("&Delete"), &menu);
    connect(removeOutput, SIGNAL(triggered()), fxScene, SLOT(onRemoveOutput()));

    QAction *activateOutput = new QAction(tr("&Activate"), &menu);
    connect(activateOutput, SIGNAL(triggered()), fxScene,
            SLOT(onActivateOutput()));

    TFx *currentOutFx = fxScene->getXsheet()->getFxDag()->getCurrentOutputFx();
    if (currentOutFx != m_parent->getFx()) menu.addAction(activateOutput);
    menu.addAction(removeOutput);
  } else {
    QAction *addOutputFx =
        CommandManager::instance()->getAction("MI_NewOutputFx");
    menu.addAction(addOutputFx);
  }
  menu.exec(cme->screenPos());
}

//*****************************************************
//
// FxSchematicLink
//
//*****************************************************

FxSchematicLink::FxSchematicLink(QGraphicsItem *parent, QGraphicsScene *scene)
    : SchematicLink(parent, scene) {}

//-----------------------------------------------------

FxSchematicLink::~FxSchematicLink() {}

//-----------------------------------------------------

void FxSchematicLink::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  TFxCommand::Link link     = fxScene->getFxSelection()->getBoundingFxs(this);
  if (link == TFxCommand::Link()) return;

  QMenu menu(fxScene->views()[0]);

  if (cme->modifiers() & Qt::ControlModifier) {
    menu.addAction(fxScene->getAgainAction(AddFxContextMenu::Insert));
    if (!menu.actions().isEmpty()) {
      menu.exec(cme->screenPos());
      return;
    }
  }

  QAction *deleteFx = new QAction(tr("&Delete"), &menu);
  connect(deleteFx, SIGNAL(triggered()), fxScene, SLOT(onDeleteFx()));

  QAction *insertPaste = new QAction(tr("&Paste Insert"), &menu);
  connect(insertPaste, SIGNAL(triggered()), fxScene, SLOT(onInsertPaste()));

  menu.addMenu(fxScene->getInsertFxMenu());
  menu.addSeparator();
  menu.addAction(insertPaste);
  menu.addAction(deleteFx);

  menu.exec(cme->screenPos());
}

//*****************************************************
//
// FxSchematicPort
//
//*****************************************************

FxSchematicPort::FxSchematicPort(FxSchematicDock *parent, int type)
    : SchematicPort(parent, parent->getNode(), type), m_currentTargetPort(0) {
  QRectF rect = boundingRect();
  if (getType() == eFxInputPort || getType() == eFxGroupedInPort)
    m_hook = QPointF(rect.left(), (rect.top() + rect.bottom()) * 0.5);
  else if (getType() == eFxOutputPort || getType() == eFxGroupedOutPort)
    m_hook = QPointF(rect.right(), (rect.top() + rect.bottom()) * 0.5);
  else  // link port
    m_hook  = QPointF(rect.right(), (rect.top() + rect.bottom()) * 0.5);
  m_ownerFx = getOwnerFx();
  TZeraryColumnFx *colFx = dynamic_cast<TZeraryColumnFx *>(m_ownerFx);
  if (colFx) m_ownerFx   = colFx->getZeraryFx();
}

//-----------------------------------------------------

FxSchematicPort::~FxSchematicPort() {}

//-----------------------------------------------------

QRectF FxSchematicPort::boundingRect() const {
  // large scaled
  if (getDock()->getNode()->isLargeScaled()) {
    switch (getType()) {
    case eFxInputPort:
    case eFxGroupedInPort:
      return QRectF(0, 0, 14, 14);
      break;

    case eFxOutputPort:
    case eFxGroupedOutPort:
      return QRectF(0, 0, 18, 18);
      break;

    case eFxLinkPort:  // LinkPort
    default:
      return QRectF(0, 0, 18, 7);
    }
  }
  // small scaled
  else {
    switch (getType()) {
    case eFxInputPort: {
      FxSchematicNode *node = getDock()->getNode();
      float nodeHeight =
          node->boundingRect().height() - 10.0f;  // 10.0 is margin

      TFx *fx = 0;
      FxSchematicZeraryNode *zeraryNode =
          dynamic_cast<FxSchematicZeraryNode *>(node);
      if (zeraryNode) {
        TZeraryColumnFx *zcfx =
            dynamic_cast<TZeraryColumnFx *>(zeraryNode->getFx());
        fx = zcfx->getZeraryFx();
      } else {
        fx = node->getFx();
      }

      if (fx && fx->getInputPortCount()) {
        // ex. particles fx etc. For fxs of which amount of ports may change
        if (fx->hasDynamicPortGroups())
          nodeHeight =
              (nodeHeight - (float)(2 * (fx->getInputPortCount() - 1))) /
              (float)fx->getInputPortCount();
        else
          nodeHeight =
              (nodeHeight - (float)(4 * (fx->getInputPortCount() - 1))) /
              (float)fx->getInputPortCount();
      }

      return QRectF(0, 0, 10, nodeHeight);
    } break;

    case eFxGroupedInPort:
    case eFxOutputPort:
    case eFxGroupedOutPort: {
      FxSchematicNode *node = getDock()->getNode();
      float nodeHeight =
          node->boundingRect().height() - 10.0f;  // 10.0 is margin
      return QRectF(0, 0, 10, nodeHeight);
    } break;

    case eFxLinkPort:  // LinkPort
    default:
      return QRectF(0, 0, 10, 5);
      break;
    }
  }
  return QRectF();
}

//-----------------------------------------------------

void FxSchematicPort::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
  // large scaled
  if (getDock()->getNode()->isLargeScaled()) {
    switch (getType()) {
    case eFxInputPort:
    case eFxGroupedInPort: {
      QRect sourceRect =
          scene()->views()[0]->matrix().mapRect(boundingRect()).toRect();
      static QIcon fxPortRedIcon(":Resources/fxport_red.svg");
      QPixmap redPm = fxPortRedIcon.pixmap(sourceRect.size());
      sourceRect    = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                         sourceRect.height() * getDevPixRatio());
      painter->drawPixmap(boundingRect(), redPm, sourceRect);
    } break;

    case eFxOutputPort:
    case eFxGroupedOutPort: {
      QRect sourceRect =
          scene()->views()[0]->matrix().mapRect(boundingRect()).toRect();
      static QIcon fxPortBlueIcon(":Resources/fxport_blue.svg");
      QPixmap bluePm = fxPortBlueIcon.pixmap(sourceRect.size());
      sourceRect     = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                         sourceRect.height() * getDevPixRatio());
      painter->drawPixmap(boundingRect(), bluePm, sourceRect);
      FxSchematicDock *parentDock =
          dynamic_cast<FxSchematicDock *>(parentItem());
      if (parentDock) {
        FxSchematicNode *parentFxNode =
            dynamic_cast<FxSchematicNode *>(parentDock->parentItem());
        if (parentFxNode) {
          bool isCached = parentFxNode->isCached();
          if (isCached) {
            QPixmap cachePm = QPixmap(":Resources/cachefx.png");
            painter->drawPixmap(QRectF(0, 0, 18, 36), cachePm,
                                QRectF(0, 0, 50, 100));
          }
        }
      }
    } break;

    case eFxLinkPort:  // LinkPort
    default: {         //ここから！！！
      QRect sourceRect =
          scene()->views()[0]->matrix().mapRect(boundingRect()).toRect();
      QPixmap linkPm =
          QIcon(":Resources/schematic_link.svg").pixmap(sourceRect.size());
      sourceRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                         sourceRect.height() * getDevPixRatio());
      painter->drawPixmap(boundingRect(), linkPm, sourceRect);
    } break;
    }
  }
  // small scaled
  else {
    painter->setPen(Qt::NoPen);
    switch (getType()) {
    case eFxInputPort:
    case eFxGroupedInPort: {
      painter->setBrush(QColor(223, 100, 100, 255));
    } break;

    case eFxOutputPort:
    case eFxGroupedOutPort: {
      painter->setBrush(QColor(100, 100, 223, 255));
    } break;

    case eFxLinkPort:  // LinkPort
    default: { painter->setBrush(QColor(192, 192, 192, 255)); } break;
    }
    painter->drawRect(boundingRect());
  }
}

//-----------------------------------------------------

FxSchematicDock *FxSchematicPort::getDock() const {
  return dynamic_cast<FxSchematicDock *>(parentItem());
}

//-----------------------------------------------------

SchematicLink *FxSchematicPort::makeLink(SchematicPort *port) {
  if (isLinkedTo(port) || !port) return 0;
  FxSchematicLink *link = new FxSchematicLink(0, scene());
  if (getType() == eFxLinkPort && port->getType() == eFxLinkPort)
    link->setLineShaped(true);

  link->setStartPort(this);
  link->setEndPort(port);
  addLink(link);
  port->addLink(link);
  link->updatePath();
  return link;
}

//-----------------------------------------------------

bool FxSchematicPort::linkTo(SchematicPort *port, bool checkOnly) {
  if (port == this) return false;

  FxSchematicNode *dstNode = dynamic_cast<FxSchematicNode *>(port->getNode());
  FxSchematicNode *srcNode = dynamic_cast<FxSchematicNode *>(getNode());
  if (dstNode == srcNode) return false;

  TFx *inputFx = 0, *fx = 0;
  int portIndex = 0;
  if (getType() == eFxInputPort && port->getType() == eFxOutputPort) {
    inputFx   = dstNode->getFx();
    fx        = srcNode->getFx();
    portIndex = srcNode->getInputDockId(this->getDock());
  } else if (getType() == eFxOutputPort && port->getType() == eFxInputPort) {
    inputFx   = srcNode->getFx();
    fx        = dstNode->getFx();
    portIndex = dstNode->getInputDockId(
        (dynamic_cast<FxSchematicPort *>(port))->getDock());
  } else
    return false;
  if (inputFx->getAttributes()->isGrouped() &&
      fx->getAttributes()->isGrouped() &&
      inputFx->getAttributes()->getEditingGroupId() !=
          fx->getAttributes()->getEditingGroupId())
    return false;

  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return false;
  FxDag *fxDag = fxScene->getXsheet()->getFxDag();
  if (fxDag->checkLoop(inputFx, fx)) return false;
  if (!checkOnly) linkEffects(inputFx, fx, portIndex);
  return true;
}

//-----------------------------------------------------

void FxSchematicPort::linkEffects(TFx *inputFx, TFx *fx, int inputId) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFxCommand::setParent(inputFx, fx, inputId, fxScene->getXsheetHandle());
}

//-----------------------------------------------------

void FxSchematicPort::hideSnappedLinks() {
  if (!m_linkingTo) return;
  if (m_linkingTo->getType() == eFxInputPort &&
      m_linkingTo->getLinkCount() == 1) {
    FxSchematicXSheetNode *xsheetNode =
        dynamic_cast<FxSchematicXSheetNode *>(m_linkingTo->getNode());
    if (!xsheetNode) m_linkingTo->getLink(0)->hide();
  }
  if (m_linkingTo->getType() == eFxOutputPort &&
      m_linkingTo->getLinkCount() >= 1) {
    int i;
    for (i = 0; i < m_linkingTo->getLinkCount(); i++) {
      SchematicLink *link               = m_linkingTo->getLink(i);
      FxSchematicXSheetNode *xsheetNode = dynamic_cast<FxSchematicXSheetNode *>(
          link->getOtherNode(m_linkingTo->getNode()));
      if (xsheetNode) link->hide();
    }
  }
  if (getType() == eFxInputPort && getLinkCount() == 1) {
    FxSchematicXSheetNode *xsheetNode =
        dynamic_cast<FxSchematicXSheetNode *>(getNode());
    if (!xsheetNode) getLink(0)->hide();
  }
  if (getType() == eFxOutputPort && getLinkCount() == 1) {
    FxSchematicXSheetNode *xsheetNode = dynamic_cast<FxSchematicXSheetNode *>(
        getLink(0)->getOtherNode(this->getNode()));
    if (xsheetNode) getLink(0)->hide();
  }
}

//-----------------------------------------------------

void FxSchematicPort::showSnappedLinks() {
  if (!m_linkingTo) return;
  if (m_linkingTo->getType() == eFxInputPort &&
      m_linkingTo->getLinkCount() == 1) {
    FxSchematicXSheetNode *xsheetNode =
        dynamic_cast<FxSchematicXSheetNode *>(m_linkingTo->getNode());
    if (!xsheetNode) m_linkingTo->getLink(0)->show();
  }
  if (m_linkingTo->getType() == eFxOutputPort &&
      m_linkingTo->getLinkCount() >= 1) {
    int i;
    for (i = 0; i < m_linkingTo->getLinkCount(); i++) {
      SchematicLink *link               = m_linkingTo->getLink(i);
      FxSchematicXSheetNode *xsheetNode = dynamic_cast<FxSchematicXSheetNode *>(
          link->getOtherNode(m_linkingTo->getNode()));
      if (xsheetNode) link->show();
    }
  }
  if (getType() == eFxInputPort && getLinkCount() == 1) {
    FxSchematicXSheetNode *xsheetNode =
        dynamic_cast<FxSchematicXSheetNode *>(getNode());
    if (!xsheetNode) getLink(0)->show();
  }
  if (getType() == eFxOutputPort && getLinkCount() == 1) {
    FxSchematicXSheetNode *xsheetNode = dynamic_cast<FxSchematicXSheetNode *>(
        getLink(0)->getOtherNode(this->getNode()));
    if (xsheetNode) getLink(0)->show();
  }
}

//-----------------------------------------------------

SchematicPort *FxSchematicPort::searchPort(const QPointF &scenePos) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  assert(fxScene);
  TXsheet *xsh                 = fxScene->getXsheet();
  QList<QGraphicsItem *> items = scene()->items(scenePos);
  int i;
  for (i = 0; i < items.size(); i++) {
    FxSchematicPort *linkingTo = dynamic_cast<FxSchematicPort *>(items[i]);
    if (linkingTo && linkingTo->getType() != eFxLinkPort) {
      TFx *fx = linkingTo->getDock()->getNode()->getFx();
      assert(fx);
      if ((!fx->getAttributes()->isGrouped() ||
           fx->getAttributes()->isGroupEditing()) &&
          !isAInnerMacroFx(fx, xsh))
        return linkingTo;
    }
    FxSchematicDock *linkingDock = dynamic_cast<FxSchematicDock *>(items[i]);
    if (linkingDock && linkingDock->getPort()->getType() != eFxLinkPort) {
      TFx *fx = linkingDock->getNode()->getFx();
      assert(fx);
      if ((!fx->getAttributes()->isGrouped() ||
           fx->getAttributes()->isGroupEditing()) &&
          !isAInnerMacroFx(fx, xsh))
        return linkingDock->getPort();
    }
    FxSchematicXSheetNode *linkingNode =
        dynamic_cast<FxSchematicXSheetNode *>(items[i]);
    if (linkingNode && getType() == eFxOutputPort)
      return linkingNode->getInputPort(0);
    if (linkingNode && getType() == eFxInputPort)
      return linkingNode->getOutputPort();
    FxSchematicOutputNode *outFx =
        dynamic_cast<FxSchematicOutputNode *>(items[i]);
    if (outFx && getType() == eFxOutputPort) return outFx->getInputPort(0);
  }
  return 0;
}
//-----------------------------------------------------

void FxSchematicPort::handleSnappedLinksOnDynamicPortFx(
    const std::vector<TFxPort *> &groupedPorts, int targetIndex,
    int startIndex) {
  FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(getNode());
  if (!m_ownerFx->hasDynamicPortGroups() || !node) return;
  int groupedPortCount           = groupedPorts.size();
  if (startIndex < 0) startIndex = groupedPortCount - 1;
  if (startIndex >= groupedPortCount || targetIndex >= groupedPortCount) return;
  int i;

  // hide existing links
  QMap<int, SchematicPort *> linkedPorts;
  int minIndex = std::min(targetIndex, startIndex);
  int maxIndex = std::max(targetIndex, startIndex);
  for (i = minIndex; i <= maxIndex; i++) {
    TFxPort *currentPort = groupedPorts[i];
    int portId = getInputPortIndex(currentPort, currentPort->getOwnerFx());
    if (portId == -1) continue;
    FxSchematicPort *port = node->getInputPort(portId);
    SchematicLink *link   = port->getLink(0);
    if (link) {
      linkedPorts[i] = link->getOtherPort(port);
      link->hide();
      m_hiddenLinks.append(link);
    }
  }

  // create ghost links
  if (targetIndex < startIndex) {
    for (i = targetIndex + 1; i <= startIndex; i++) {
      TFxPort *currentPort = groupedPorts[i];
      int portId = getInputPortIndex(currentPort, currentPort->getOwnerFx());
      if (portId == -1) continue;
      FxSchematicPort *port = node->getInputPort(portId);
      SchematicLink *link   = port->makeLink(linkedPorts[i - 1]);
      if (link) m_ghostLinks.append(link);
    }
  } else {
    for (i = startIndex; i < targetIndex; i++) {
      TFxPort *currentPort = groupedPorts[i];
      int portId = getInputPortIndex(currentPort, currentPort->getOwnerFx());
      if (portId == -1) continue;
      FxSchematicPort *port = node->getInputPort(portId);
      SchematicLink *link   = port->makeLink(linkedPorts[i + 1]);
      if (link) m_ghostLinks.append(link);
    }
  }
}

//-----------------------------------------------------

void FxSchematicPort::resetSnappedLinksOnDynamicPortFx() {
  int i;
  for (i = 0; i < m_hiddenLinks.size(); i++) m_hiddenLinks.at(i)->show();
  m_hiddenLinks.clear();
  for (i = 0; i < m_ghostLinks.size(); i++) {
    SchematicLink *link = m_ghostLinks.at(i);
    link->getStartPort()->removeLink(link);
    link->getEndPort()->removeLink(link);
    scene()->removeItem(link);
    delete link;
  }
  m_ghostLinks.clear();
}

//-----------------------------------------------------

void FxSchematicPort::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  QMenu menu(fxScene->views()[0]);

  TFx *fx = getDock()->getNode()->getFx();
  bool enableInsertAction =
      fxScene->getXsheet()->getFxDag()->getInternalFxs()->containsFx(fx) &&
      (!fx->getAttributes()->isGrouped() ||
       fx->getAttributes()->isGroupEditing());

  if ((getType() == eFxOutputPort || getType() == eFxGroupedOutPort) &&
      enableInsertAction) {
    fxScene->initCursorScenePos();
    if (cme->modifiers() & Qt::ControlModifier) {
      menu.addAction(fxScene->getAgainAction(AddFxContextMenu::Add));
      if (!menu.actions().isEmpty()) {
        menu.exec(cme->screenPos());
        return;
      }
    }

    QAction *disconnectFromXSheet =
        new QAction(tr("&Disconnect from Xsheet"), &menu);
    connect(disconnectFromXSheet, SIGNAL(triggered()), fxScene,
            SLOT(onDisconnectFromXSheet()));

    QAction *connectToXSheet = new QAction(tr("&Connect to Xsheet"), &menu);
    connect(connectToXSheet, SIGNAL(triggered()), fxScene,
            SLOT(onConnectToXSheet()));

    QAction *fxEditorPopup =
        CommandManager::instance()->getAction("MI_FxParamEditor");

    menu.addMenu(fxScene->getAddFxMenu());
    menu.addAction(fxEditorPopup);
    if (fxScene->getXsheet()->getFxDag()->getTerminalFxs()->containsFx(
            getDock()->getNode()->getFx()))
      menu.addAction(disconnectFromXSheet);
    else
      menu.addAction(connectToXSheet);
  }
  menu.exec(cme->screenPos());
}

//-----------------------------------------------------

void FxSchematicPort::mouseMoveEvent(QGraphicsSceneMouseEvent *me) {
  SchematicPort::mouseMoveEvent(me);
  if (m_ghostLink && !m_ghostLink->isVisible()) m_ghostLink->show();
  bool cntr = me->modifiers() == Qt::ControlModifier;
  if (m_currentTargetPort) {
    m_currentTargetPort->resetSnappedLinksOnDynamicPortFx();
    m_currentTargetPort = 0;
  }
  if (!m_linkingTo) return;
  FxSchematicPort *targetPort = dynamic_cast<FxSchematicPort *>(m_linkingTo);
  assert(targetPort);
  m_currentTargetPort    = targetPort;
  TFx *targetFx          = targetPort->getOwnerFx();
  TZeraryColumnFx *colFx = dynamic_cast<TZeraryColumnFx *>(targetFx);
  if (colFx) targetFx    = colFx->getZeraryFx();
  if (targetPort->getType() != eFxInputPort ||
      !targetFx->hasDynamicPortGroups() || targetPort == this)
    return;

  FxSchematicNode *node =
      dynamic_cast<FxSchematicNode *>(targetPort->getNode());
  int id                = node->getInputDockId(targetPort->getDock());
  TFxPort *targetFxPort = targetFx->getInputPort(id);
  int groupId           = targetFxPort->getGroupIndex();

  if (groupId < 0) return;

  std::vector<TFxPort *> groupedPorts =
      targetFx->dynamicPortGroup(groupId)->ports();
  int portId = getIndex(targetFxPort, groupedPorts);
  if (portId == -1) return;
  if (targetFx != m_ownerFx && cntr && getType() == eFxOutputPort)
    targetPort->handleSnappedLinksOnDynamicPortFx(groupedPorts, portId);
  else if (targetFx == m_ownerFx && getType() == eFxInputPort) {
    if (m_ghostLink) m_ghostLink->hide();
    FxSchematicNode *thisNode = dynamic_cast<FxSchematicNode *>(getNode());
    int thisId                = thisNode->getInputDockId(getDock());
    TFxPort *thisFxPort       = targetFx->getInputPort(thisId);
    int thisGroupId           = thisFxPort->getGroupIndex();
    if (thisGroupId != groupId) return;
    int thisPortId = getIndex(thisFxPort, groupedPorts);
    if (thisPortId == -1) return;
    targetPort->handleSnappedLinksOnDynamicPortFx(groupedPorts, portId,
                                                  thisPortId);
    SchematicLink *link = getLink(0);
    assert(link);
    SchematicLink *ghostLink = targetPort->makeLink(link->getOtherPort(this));
    if (ghostLink) {
      targetPort->m_ghostLinks.append(ghostLink);
    }
  }
}

//-----------------------------------------------------

void FxSchematicPort::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  if (m_currentTargetPort)
    m_currentTargetPort->resetSnappedLinksOnDynamicPortFx();
  m_currentTargetPort = 0;
  FxSchematicPort *targetPort =
      dynamic_cast<FxSchematicPort *>(searchPort(me->scenePos()));
  if (!targetPort)  // not over a port: do nothing
  {
    SchematicPort::mouseReleaseEvent(me);
    return;
  }
  TFx *targetOwnerFx       = targetPort->getOwnerFx();
  TZeraryColumnFx *colFx   = dynamic_cast<TZeraryColumnFx *>(targetOwnerFx);
  if (colFx) targetOwnerFx = colFx->getZeraryFx();

  // if the target fx has no dynamic port or has dinamic ports but the tatgert
  // port is not an input port: do nothing!
  if (!targetOwnerFx || !targetOwnerFx->hasDynamicPortGroups() ||
      targetPort->getType() != eFxInputPort) {
    SchematicPort::mouseReleaseEvent(me);
    return;
  }
  FxSchematicNode *targetNode =
      dynamic_cast<FxSchematicNode *>(targetPort->getNode());
  int targetFxId        = targetNode->getInputDockId(targetPort->getDock());
  TFxPort *targetFxPort = targetOwnerFx->getInputPort(targetFxId);
  int targetGroupId     = targetFxPort->getGroupIndex();

  if (targetGroupId < 0) {
    SchematicPort::mouseReleaseEvent(me);
    return;
  }

  std::vector<TFxPort *> groupedPorts =
      targetOwnerFx->dynamicPortGroup(targetGroupId)->ports();
  int groupedPortCount = groupedPorts.size();
  if (targetOwnerFx != m_ownerFx && me->modifiers() == Qt::ControlModifier &&
      linkTo(targetPort, true)) {
    // trying to link different fxs insertin the new link and shifting the
    // others
    int targetIndex = getIndex(targetFxPort, groupedPorts);
    if (targetIndex == -1) {
      SchematicPort::mouseReleaseEvent(me);
      return;
    }

    int i;
    TUndoManager::manager()->beginBlock();
    for (i = groupedPortCount - 1; i > targetIndex; i--) {
      TFxPort *currentPort   = groupedPorts[i];
      TFxPort *precedentPort = groupedPorts[i - 1];
      int currentPortIndex   = getInputPortIndex(currentPort, targetOwnerFx);
      linkEffects(precedentPort->getFx(), colFx ? colFx : targetOwnerFx,
                  currentPortIndex);
    }
    linkTo(targetPort);
    TUndoManager::manager()->endBlock();
    emit sceneChanged();
    emit xsheetChanged();
  } else if (targetOwnerFx == m_ownerFx && m_type == eFxInputPort &&
             getLink(0) && targetPort != this) {
    // reordering of links in input of the same fxs
    /*FxSchematicNode* linkedNode =
dynamic_cast<FxSchematicNode*>(getLinkedNode(0));
assert(linkedNode);
TFx* linkedFx = linkedNode->getFx();
assert(linkedFx);*/

    int thisFxId        = targetNode->getInputDockId(getDock());
    TFxPort *thisFxPort = m_ownerFx->getInputPort(thisFxId);
    TFx *linkedFx       = thisFxPort->getFx();

    int targetIndex = getIndex(targetFxPort, groupedPorts);
    int thisIndex   = getIndex(thisFxPort, groupedPorts);
    if (targetIndex != -1 && thisIndex != -1 && thisIndex != targetIndex) {
      TUndoManager::manager()->beginBlock();

      // linkEffects(0,colFx ? colFx : m_ownerFx,thisFxId);
      if (thisIndex > targetIndex) {
        int i;
        for (i = thisIndex; i > targetIndex; i--) {
          TFxPort *currentPort   = groupedPorts[i];
          TFxPort *precedentPort = groupedPorts[i - 1];
          int currentId          = getInputPortIndex(currentPort, m_ownerFx);
          linkEffects(precedentPort->getFx(), colFx ? colFx : m_ownerFx,
                      currentId);
        }
      } else {
        int i;
        for (i = thisIndex; i < targetIndex; i++) {
          TFxPort *currentPort = groupedPorts[i];
          TFxPort *nextPort    = groupedPorts[i + 1];
          int currentId        = getInputPortIndex(currentPort, m_ownerFx);
          linkEffects(nextPort->getFx(), colFx ? colFx : m_ownerFx, currentId);
        }
      }
      linkEffects(linkedFx, colFx ? colFx : m_ownerFx, targetFxId);
      TUndoManager::manager()->endBlock();
      emit sceneChanged();
      emit xsheetChanged();
    }
  } else
    SchematicPort::mouseReleaseEvent(me);
}

//-----------------------------------------------------

TFx *FxSchematicPort::getOwnerFx() const {
  FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(this->getNode());
  if (!node) return 0;
  return node->getFx();
}

//*****************************************************
//
// FxSchematicDock
//
//*****************************************************

FxSchematicDock::FxSchematicDock(FxSchematicNode *parent, const QString &name,
                                 double width, eFxSchematicPortType type)
    : QGraphicsItem(parent), m_name(name), m_width(width) {
  m_port = new FxSchematicPort(this, type);
  m_port->setPos(0, 0);
  if (parent) {
    TFx *fx       = parent->getFx();
    TFxPort *port = fx->getInputPort(name.toStdString());
    if (port) {
      TFx *inputFx = port->getFx();
      if (inputFx) {
        TLevelColumnFx *levelFx    = dynamic_cast<TLevelColumnFx *>(inputFx);
        TPaletteColumnFx *palettFx = dynamic_cast<TPaletteColumnFx *>(inputFx);
        if (levelFx || palettFx) {
          int index =
              levelFx ? levelFx->getColumnIndex() : palettFx->getColumnIndex();
          TStageObjectId objId         = TStageObjectId::ColumnId(index);
          QGraphicsScene *graphicScene = scene();
          FxSchematicScene *schematicScene =
              dynamic_cast<FxSchematicScene *>(graphicScene);
          if (schematicScene) {
            std::string colName =
                schematicScene->getXsheet()->getStageObject(objId)->getName();
            setToolTip(QString::fromStdString(colName));
          }
        } else {
          TZeraryColumnFx *zeraryFx = dynamic_cast<TZeraryColumnFx *>(inputFx);
          if (zeraryFx) inputFx     = zeraryFx->getZeraryFx();
          setToolTip(QString::fromStdWString(inputFx->getName()));
        }
      }
    }
  }
  connect(m_port, SIGNAL(sceneChanged()), parent, SIGNAL(sceneChanged()));
  connect(m_port, SIGNAL(xsheetChanged()), parent, SIGNAL(xsheetChanged()));
}

//-----------------------------------------------------

FxSchematicDock::~FxSchematicDock() {}

//-----------------------------------------------------

QRectF FxSchematicDock::boundingRect() const {
  return QRectF(0, 0, m_width, m_port->boundingRect().height());
}

//-----------------------------------------------------

void FxSchematicDock::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
  if (m_port->getType() == eFxInputPort ||
      m_port->getType() == eFxGroupedInPort) {
    // do nothing when small scaled
    if (!getNode()->isLargeScaled()) return;

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 192));
    painter->drawRect(boundingRect());

    QFont tempFont(painter->font());
    tempFont.setPixelSize(tempFont.pixelSize() - 1);
    painter->setFont(tempFont);

    painter->setPen(Qt::white);
    painter->drawText(boundingRect().adjusted(18, 0, 0, 0),
                      Qt::AlignLeft | Qt::AlignVCenter, m_name);
  }
}

//-----------------------------------------------------

FxSchematicNode *FxSchematicDock::getNode() {
  FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(parentItem());
  return node;
}

//*****************************************************
//
// FxSchematicNode
//
//*****************************************************

FxSchematicNode::FxSchematicNode(FxSchematicScene *scene, TFx *fx, qreal width,
                                 qreal height, eFxType type)
    : SchematicNode(scene)
    , m_fx(fx)
    , m_type(type)
    , m_isCurrentFxLinked(false)
    , m_isLargeScaled(scene->isLargeScaled()) {
  if (m_type == eGroupedFx)
    m_actualFx = 0;
  else {
    // m_fx could be the zerary wrapper to the actual (user-coded) fx - find and
    // store it.
    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(m_fx.getPointer());
    m_actualFx           = zfx ? zfx->getZeraryFx() : m_fx;
  }

  setWidth(width);
  setHeight(height);
}

//-----------------------------------------------------

void FxSchematicNode::setSchematicNodePos(const QPointF &pos) const {
  TPointD p(pos.x(), pos.y());
  if (!m_fx->getAttributes()->isGrouped() ||
      m_fx->getAttributes()->isGroupEditing()) {
    m_fx->getAttributes()->setDagNodePos(p);
    TMacroFx *macro = dynamic_cast<TMacroFx *>(m_fx.getPointer());
    if (macro) {
      TPointD delta = p - macro->getRoot()->getAttributes()->getDagNodePos();
      std::vector<TFxP> fxs = macro->getFxs();
      int i;
      for (i = 0; i < (int)fxs.size(); i++) {
        TPointD oldPos = fxs[i]->getAttributes()->getDagNodePos();
        fxs[i]->getAttributes()->setDagNodePos(oldPos + delta);
      }
    }
  } else {
    const FxGroupNode *groupNode = dynamic_cast<const FxGroupNode *>(this);
    assert(groupNode);
    groupNode->updateFxsDagPosition(p);
  }
}

//-----------------------------------------------------

int FxSchematicNode::getInputDockId(FxSchematicDock *dock) {
  return m_inDocks.indexOf(dock);
}

//-----------------------------------------------------

void FxSchematicNode::onClicked() {
  emit switchCurrentFx(m_fx.getPointer());
  if (TColumnFx *cfx = dynamic_cast<TColumnFx *>(m_fx.getPointer())) {
    int columnIndex = cfx->getColumnIndex();
    if (columnIndex >= 0) emit currentColumnChanged(columnIndex);
  }
}

//-----------------------------------------------------

void FxSchematicNode::setIsCurrentFxLinked(bool value,
                                           FxSchematicNode *comingNode) {
  m_isCurrentFxLinked = value;
  update();
  if (!m_linkDock) return;
  int i;
  for (i = 0; i < m_linkDock->getPort()->getLinkCount(); i++) {
    SchematicNode *node = m_linkDock->getPort()->getLinkedNode(i);
    if (!node || node == comingNode) continue;
    FxSchematicNode *fxNode = dynamic_cast<FxSchematicNode *>(node);
    assert(fxNode);
    fxNode->setIsCurrentFxLinked(value, this);
  }
}

//-----------------------------------------------------

void FxSchematicNode::setPosition(const QPointF &newPos) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  assert(fxScene);
  fxScene->updateNestedGroupEditors(this, newPos);
}

//-----------------------------------------------------

bool FxSchematicNode::isEnabled() const {
  TZeraryColumnFx *zcFx = dynamic_cast<TZeraryColumnFx *>(m_fx.getPointer());
  return zcFx ? zcFx->getColumn()->isPreviewVisible()
              : m_fx->getAttributes()->isEnabled();
}

//-----------------------------------------------------

bool FxSchematicNode::isCached() const {
  return TPassiveCacheManager::instance()->cacheEnabled(m_fx.getPointer());
}

//-----------------------------------------------------

void FxSchematicNode::checkDynamicInputPortSize() const {
  assert(m_actualFx);

  if (!m_actualFx->hasDynamicPortGroups()) return;

  // Shift holes in each dynamic port group links toward the end of the group
  shiftLinks();

  // Treat each dynamic port group independently
  int g, gCount = m_actualFx->dynamicPortGroupsCount();
  for (g = 0; g != gCount; ++g) {
    const TFxPortDG *group = m_actualFx->dynamicPortGroup(g);
    int gSize              = group->ports().size();

    int minPortsCount = group->minPortsCount();

    if (gSize < minPortsCount) {
      // Add empty ports up to the group's minimal required quota
      for (; gSize != minPortsCount; ++gSize) addDynamicInputPort(g);
    }

    // Must remove all unlinked ports beyond the minimal - however, we'll be
    // iterating
    // on the ports container, so it's easier if we avoid removing WHILE
    // iterating

    // So, fill in a list of all the fx's unlinked ports first
    QList<std::string> unlinkedPorts;

    int p, portCount = m_actualFx->getInputPortCount();
    for (p = 0; p != portCount; ++p) {
      TFxPort *port = m_actualFx->getInputPort(p);
      assert(port);

      if ((port->getGroupIndex() == g) && !port->isConnected())
        unlinkedPorts.append(m_actualFx->getInputPortName(p));
    }

    // Remove excess empty ports save for 1 (needed to let the user insert new
    // links)
    if (unlinkedPorts.empty())
      addDynamicInputPort(g);
    else {
      while (unlinkedPorts.size() > 1 &&
             m_actualFx->getInputPortCount() > minPortsCount) {
        removeDynamicInputPort(unlinkedPorts.last());
        unlinkedPorts.pop_back();
      }
    }
  }
}

//-----------------------------------------------------

void FxSchematicNode::addDynamicInputPort(int groupIdx) const {
  assert(m_actualFx);
  assert(groupIdx < m_actualFx->dynamicPortGroupsCount());

  TFxPort *port = new TRasterFxPort;

  // Add the port with a suitable port name
  const TFxPortDG *group = m_actualFx->dynamicPortGroup(groupIdx);

  for (int n =
           group->ports().size() + 1;  // +1 since ports start from <prefix>1
       !m_actualFx->addInputPort(
           group->portsPrefix() + QString::number(n).toStdString(), port,
           groupIdx);
       ++n)
    ;
}

//-----------------------------------------------------

bool FxSchematicNode::removeDynamicInputPort(const std::string &name) const {
  assert(m_actualFx);

  TFxPort *port = m_actualFx->getInputPort(
      name);  // Must retrieve the port to access its infos
  if (!port || port->getFx() || port->getGroupIndex() < 0) {
    assert(port && !port->isConnected() && port->getGroupIndex() >= 0);
    return false;
  }

  m_actualFx->removeInputPort(name);
  // delete port;                                                // TFxPortDG
  // owns it - so it would be wrong

  return true;
}

//-----------------------------------------------------

void FxSchematicNode::shiftLinks() const {
  struct locals {
    // Advances p along the ports array, stopping at the first port in the group
    // with the specified
    // connection state. Returns whether the port index is in the ports array
    // range.
    static inline bool moveToNext(const TFxPortDG &group, bool connected,
                                  int &p) {
      int pCount = group.ports().size();
      while (p < pCount && (group.ports()[p]->isConnected() != connected)) ++p;

      return (p < pCount);
    }

  };  // locals

  assert(m_actualFx);

  // Treat each dynamic port group individually
  int g, gCount = m_actualFx->dynamicPortGroupsCount();
  for (g = 0; g != gCount; ++g) {
    const TFxPortDG *group = m_actualFx->dynamicPortGroup(g);

    // Traverse fx ports, moving all the group links to the front

    // First, couple empty and not-empty ports manually
    int e = 0;
    locals::moveToNext(*group, false, e);  // manual advance

    int ne = e + 1;

    if (locals::moveToNext(*group, true, ne))  // manual advance
    {
      do {
        // Swap links between the empty and not-empty ports
        TFxPort *ePort  = group->ports()[e];
        TFxPort *nePort = group->ports()[ne];

        ePort->setFx(nePort->getFx());
        nePort->setFx(0);

        ++e, ++ne;

        // Then, advance coupled from there
      } while (locals::moveToNext(*group, false, e) &&
               locals::moveToNext(*group, true, ne));
    }
  }
}

//-----------------------------------------------------

void FxSchematicNode::updateOutputDockToolTips(const QString &name) {
  FxSchematicPort *outPort = getOutputPort();
  int i;
  for (i = 0; i < outPort->getLinkCount(); i++) {
    SchematicLink *link = outPort->getLink(i);
    if (!link) continue;
    SchematicPort *linkedPort = link->getOtherPort(outPort);
    assert(linkedPort);
    FxSchematicPort *linkedFxPort = dynamic_cast<FxSchematicPort *>(linkedPort);
    FxSchematicDock *linkedFxDock = linkedFxPort->getDock();
    linkedFxDock->setToolTip(name);
  }
}

//-----------------------------------------------------

void FxSchematicNode::updatePortsPosition() {
  struct locals {
    struct PositionAssigner {
      double m_lastYPos;
      bool isLarge;

      inline void assign(FxSchematicDock *dock) {
        dock->setPos(0, m_lastYPos);

        if (isLarge)
          m_lastYPos += dock->boundingRect().height();
        else
          m_lastYPos += dock->boundingRect().height() + 4;  // 4 is margin
      }
    };

  };  // locals

  locals::PositionAssigner positioner;
  if (m_isLargeScaled) {
    positioner.m_lastYPos = m_height;
    positioner.isLarge    = true;
  } else {
    positioner.m_lastYPos = 0;
    positioner.isLarge    = false;
  }

  if (!(m_actualFx && m_actualFx->hasDynamicPortGroups())) {
    // 'Fake' or common schematic fx nodes can just list the prepared m_inDocks
    // container incrementally
    for (int i = 0; i != m_inDocks.size(); ++i) positioner.assign(m_inDocks[i]);
  } else {
    // Otherwise, port groups must be treated
    assert(m_inDocks.size() == m_actualFx->getInputPortCount());

    int incrementalGroupIndex = -1;  // We'll assume that groups are encountered
    // incrementally (requires not ill-formed fxs).
    int p, pCount = m_actualFx->getInputPortCount();
    for (p = 0; p != pCount; ++p) {
      int g = m_actualFx->getInputPort(p)->getGroupIndex();
      if (g < 0)
        positioner.assign(
            m_inDocks[p]);  // The port is in no group - just add it
      else if (g > incrementalGroupIndex) {
        // Position the whole group
        incrementalGroupIndex = g;

        for (int gp = p; gp != pCount; ++gp)
          if (m_actualFx->getInputPort(gp)->getGroupIndex() == g)
            positioner.assign(m_inDocks[gp]);
      }
    }
  }
}

//*****************************************************
//
// FxSchematicOutputNode
//
//*****************************************************

FxSchematicOutputNode::FxSchematicOutputNode(FxSchematicScene *scene,
                                             TOutputFx *fx)
    : FxSchematicNode(scene, fx, 67, 18, eOutpuFx) {
  // resize if small scaled
  if (!m_isLargeScaled) {
    setWidth(60);
    setHeight(36);
  }

  m_linkedNode            = 0;
  m_outDock               = 0;
  m_linkDock              = 0;
  FxSchematicDock *inDock = new FxSchematicDock(this, "", 0, eFxInputPort);
  if (m_isLargeScaled)
    inDock->setPos(0, 2);
  else
    inDock->setPos(0, 0);
  inDock->setZValue(2);
  m_inDocks.push_back(inDock);
  addPort(0, inDock->getPort());
  m_outputPainter = new FxOutputPainter(this, m_width, m_height);
  m_outputPainter->setZValue(1);
}

//-----------------------------------------------------

FxSchematicOutputNode::~FxSchematicOutputNode() {}

//-----------------------------------------------------

QRectF FxSchematicOutputNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxSchematicOutputNode::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
  FxSchematicNode::paint(painter, option, widget);
}

//-----------------------------------------------------

void FxSchematicOutputNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QAction *outputSettingsPopup =
      CommandManager::instance()->getAction("MI_OutputSettings");
  outputSettingsPopup->trigger();
}

//*****************************************************
//
// FxSchematicXSheetNode
//
//*****************************************************

FxSchematicXSheetNode::FxSchematicXSheetNode(FxSchematicScene *scene,
                                             TXsheetFx *fx)
    : FxSchematicNode(scene, fx, 90, 18, eXSheetFx) {
  if (!m_isLargeScaled) {
    setWidth(70);
    setHeight(36);
  }

  m_linkedNode = 0;
  m_linkDock   = 0;

  m_outDock               = new FxSchematicDock(this, "", 0, eFxOutputPort);
  FxSchematicDock *inDock = new FxSchematicDock(this, "", 0, eFxInputPort);
  m_xsheetPainter         = new FxXSheetPainter(this, m_width, m_height);

  addPort(0, m_outDock->getPort());
  addPort(1, inDock->getPort());

  m_inDocks.push_back(inDock);

  if (m_isLargeScaled) {
    m_outDock->setPos(72, 0);
    inDock->setPos(0, 2);
  } else {
    m_outDock->setPos(60, 0);
    inDock->setPos(0, 0);
  }

  m_outDock->setZValue(2);
  inDock->setZValue(2);
  m_xsheetPainter->setZValue(1);
}

//-----------------------------------------------------

FxSchematicXSheetNode::~FxSchematicXSheetNode() {}

//-----------------------------------------------------

QRectF FxSchematicXSheetNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxSchematicXSheetNode::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
  FxSchematicNode::paint(painter, option, widget);
}

//-----------------------------------------------------

void FxSchematicXSheetNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QAction *sceneSettingsPopup =
      CommandManager::instance()->getAction("MI_SceneSettings");
  sceneSettingsPopup->trigger();
}

//*****************************************************
//
// FxSchematicNormalFxNode
//
//*****************************************************

// TODO: Fxの分類、各Fxに自己申告させるべき 2016/1/8 shun_iwasawa
namespace {
bool isImageAdjustFx(std::string id) {
  if (id == "STD_toneCurveFx" || id == "STD_inoChannelSelectorFx" ||
      id == "STD_inoDensityFx" || id == "STD_inohlsAddFx" ||
      id == "STD_inohlsAdjustFx" || id == "STD_inohsvAddFx" ||
      id == "STD_inohsvAdjustFx" || id == "STD_inoLevelAutoFx" ||
      id == "STD_inoLevelMasterFx" || id == "STD_inoLevelrgbaFx" ||
      id == "STD_inoNegateFx" || id == "STD_localTransparencyFx" ||
      id == "STD_multiToneFx" || id == "STD_premultiplyFx" ||
      id == "STD_rgbmCutFx" || id == "STD_rgbmFadeFx" ||
      id == "STD_rgbmScaleFx" || id == "STD_sharpenFx" || id == "STD_fadeFx")
    return true;
  else
    return false;
}

bool isLayerBlendingFx(std::string id) {
  if (id == "STD_inoOverFx" || id == "STD_inoCrossDissolveFx" ||
      id == "STD_inoDarkenFx" || id == "STD_inoMultiplyFx" ||
      id == "STD_inoColorBurnFx" || id == "STD_inoLinearBurnFx" ||
      id == "STD_inoDarkerColorFx" || id == "STD_inoAddFx" ||
      id == "STD_inoLightenFx" || id == "STD_inoScreenFx" ||
      id == "STD_inoColorDodgeFx" || id == "STD_inoLinearDodgeFx" ||
      id == "STD_inoLighterColorFx" || id == "STD_inoOverlayFx" ||
      id == "STD_inoSoftLightFx" || id == "STD_inoHardLightFx" ||
      id == "STD_inoVividLightFx" || id == "STD_inoLinearLightFx" ||
      id == "STD_inoPinLightFx" || id == "STD_inoHardMixFx" ||
      id == "STD_inoDivideFx" || id == "STD_inoSubtractFx")
    return true;
  else
    return false;
}

bool isMatteFx(std::string id) {
  if (id == "STD_hsvKeyFx" || id == "inFx" || id == "outFx" ||
      id == "STD_rgbKeyFx" || id == "atopFx")
    return true;
  else
    return false;
}
};

FxSchematicNormalFxNode::FxSchematicNormalFxNode(FxSchematicScene *scene,
                                                 TFx *fx)
    : FxSchematicNode(scene, fx, 90, 32, eNormalFx) {
  checkDynamicInputPortSize();

  // resize if small scaled
  if (!m_isLargeScaled) {
    setWidth(70);
    setHeight(50);
  }

  TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx);
  if (macroFx) {
    m_type                = eMacroFx;
    std::vector<TFxP> fxs = macroFx->getFxs();
    bool enable           = false;
    int i;
    for (i = 0; i < (int)fxs.size(); i++)
      enable = enable || fxs[i]->getAttributes()->isEnabled();
    macroFx->getAttributes()->enable(enable);
  }
  m_name       = QString::fromStdWString(fx->getName());
  m_linkedNode = 0;

  // set fx type
  std::string id = fx->getFxType();
  if (isImageAdjustFx(id))
    m_type = eNormalImageAdjustFx;
  else if (isLayerBlendingFx(id))
    m_type = eNormalLayerBlendingFx;
  else if (isMatteFx(id))
    m_type = eNormalMatteFx;

  m_nameItem = new SchematicName(this, 72, 20);  // for rename
  m_outDock  = new FxSchematicDock(this, "", 0, eFxOutputPort);
  m_linkDock = new FxSchematicDock(this, "", 0, eFxLinkPort);

  m_renderToggle = new SchematicToggle(
      this, QPixmap(":Resources/schematic_prev_eye.png"), 0);

  m_painter =
      new FxPainter(this, m_width, m_height, m_name, m_type, fx->getFxType());

  m_linkedNode = 0;
  //-----
  m_nameItem->setName(m_name);
  m_renderToggle->setIsActive(m_fx->getAttributes()->isEnabled());

  addPort(0, m_outDock->getPort());
  addPort(-1, m_linkDock->getPort());

  if (m_isLargeScaled) {
    m_nameItem->setPos(1, -1);
    m_outDock->setPos(72, 14);
    m_linkDock->setPos(72, 7);
    m_renderToggle->setPos(72, 0);
  } else {
    QFont fnt = m_nameItem->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    m_nameItem->setFont(fnt);

    m_nameItem->setPos(-1, 0);
    m_outDock->setPos(60, 0);
    m_linkDock->setPos(60, -5);
    m_renderToggle->setPos(35, -5);
  }

  m_nameItem->setZValue(3);
  m_outDock->setZValue(2);
  m_linkDock->setZValue(2);
  m_renderToggle->setZValue(2);
  m_painter->setZValue(1);

  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  connect(m_renderToggle, SIGNAL(toggled(bool)), this,
          SLOT(onRenderToggleClicked(bool)));
  m_nameItem->hide();

  int i, inputPorts = fx->getInputPortCount();
  double lastPosY = (m_isLargeScaled) ? m_height : 0;
  for (i = 0; i < inputPorts; i++) {
    std::string portName = fx->getInputPortName(i);
    QString qPortName    = QString::fromStdString(portName);
    QString toolTip      = "";
    if (isA(eMacroFx)) {
      // Add a Tool Tip to the Dock showing the name of the port and the
      // original fx id.
      int firstIndex  = qPortName.indexOf("_");
      int secondIndex = qPortName.indexOf("_", firstIndex + 1);
      qPortName.remove(qPortName.indexOf("_"), secondIndex - firstIndex);
      toolTip = qPortName;
      toolTip.replace("_", "(");
      toolTip.append(")");
      QString qInMacroFxId = qPortName;
      qInMacroFxId.remove(0, qInMacroFxId.indexOf("_") + 1);
      std::vector<TFxP> macroFxs = macroFx->getFxs();
      int j;
      for (j = 0; j < (int)macroFxs.size(); j++) {
        TFx *inMacroFx      = macroFxs[j].getPointer();
        std::wstring fxName = inMacroFx->getName();
        QString qFxName     = QString::fromStdWString(fxName);
        if (inMacroFx->getFxId() == qInMacroFxId.toStdWString()) {
          int count = inMacroFx->getInputPortCount();
          if (count == 1)
            qPortName = qFxName;
          else {
            qPortName.remove(1, qPortName.size());
            qPortName += ". " + qFxName;
          }
        }
      }
    }

    FxSchematicDock *inDock;

    if (m_isLargeScaled) {
      inDock = new FxSchematicDock(this, qPortName, m_width - 18, eFxInputPort);
      inDock->setPos(0, lastPosY);
      lastPosY += inDock->boundingRect().height();
    } else {
      inDock = new FxSchematicDock(this, qPortName, 10, eFxInputPort);
      inDock->setPos(0, lastPosY);
      lastPosY += inDock->boundingRect().height() + 4;  // 4 is margin
    }

    inDock->setZValue(2);
    m_inDocks.push_back(inDock);
    addPort(i + 1, inDock->getPort());
    if (toolTip != "") inDock->setToolTip(toolTip);
  }
}

//-----------------------------------------------------

FxSchematicNormalFxNode::~FxSchematicNormalFxNode() {}

//-----------------------------------------------------

QRectF FxSchematicNormalFxNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxSchematicNormalFxNode::paint(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    QWidget *widget) {
  FxSchematicNode::paint(painter, option, widget);
}

//-----------------------------------------------------

void FxSchematicNormalFxNode::onNameChanged() {
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_painter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFxCommand::renameFx(m_fx.getPointer(), m_name.toStdWString(),
                       fxScene->getXsheetHandle());
  updateOutputDockToolTips(m_name);
  emit sceneChanged();
  update();
}

//-----------------------------------------------------

void FxSchematicNormalFxNode::onRenderToggleClicked(bool value) {
  m_fx->getAttributes()->enable(value);
  TMacroFx *macro = dynamic_cast<TMacroFx *>(m_fx.getPointer());
  if (macro) {
    std::vector<TFxP> fxs = macro->getFxs();
    int i;
    for (i = 0; i < (int)fxs.size(); i++)
      fxs[i]->getAttributes()->enable(value);
  }
  update();
  emit sceneChanged();
  emit xsheetChanged();
}

//-----------------------------------------------------

void FxSchematicNormalFxNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(0, 0, m_width, 14);
  if (nameArea.contains(me->pos()) && me->modifiers() == Qt::ControlModifier) {
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  } else {
    QAction *fxEitorPopup =
        CommandManager::instance()->getAction("MI_FxParamEditor");
    fxEitorPopup->trigger();
    // this signal cause the update the contents of the FxSettings
    emit fxNodeDoubleClicked();
  }
}

//-----------------------------------------------------

void FxSchematicNormalFxNode::resize(bool maximized) {}

//*****************************************************
//
// FxSchematicZeraryNode
//
//*****************************************************

FxSchematicZeraryNode::FxSchematicZeraryNode(FxSchematicScene *scene,
                                             TZeraryColumnFx *fx)
    : FxSchematicNode(scene, fx, 90, 32, eZeraryFx) {
  checkDynamicInputPortSize();

  if (!m_isLargeScaled) {
    setWidth(90);
    setHeight(50);
  }

  m_columnIndex = fx->getColumnIndex();

  TFx *zeraryFx     = fx->getZeraryFx();
  TStageObjectId id = TStageObjectId::ColumnId(m_columnIndex);
  std::string name  = scene->getXsheet()->getStageObject(id)->getName();
  m_name            = QString::fromStdString(name);

  m_nameItem = new SchematicName(this, 72, 20);  // for rename
  m_outDock  = new FxSchematicDock(this, "", 0, eFxOutputPort);
  m_linkDock = new FxSchematicDock(this, "", 0, eFxLinkPort);
  m_renderToggle =
      new SchematicToggle(this, QPixmap(":Resources/schematic_prev_eye.png"),
                          SchematicToggle::eIsParentColumn, m_isLargeScaled);

  // get the fx icons according to the fx type
  m_painter = new FxPainter(this, m_width, m_height, m_name, m_type,
                            zeraryFx->getFxType());

  m_linkedNode = 0;

  //---
  m_nameItem->setName(m_name);
  m_nameItem->hide();

  addPort(0, m_outDock->getPort());
  addPort(-1, m_linkDock->getPort());

  TXshColumn *column = scene->getXsheet()->getColumn(m_columnIndex);
  if (column) m_renderToggle->setIsActive(column->isPreviewVisible());

  // define positions
  if (m_isLargeScaled) {
    m_nameItem->setPos(1, -1);
    m_outDock->setPos(72, 14);
    m_linkDock->setPos(72, m_height);
    m_renderToggle->setPos(72, 0);
  } else {
    QFont fnt = m_nameItem->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    m_nameItem->setFont(fnt);

    m_nameItem->setPos(-1, 0);
    m_outDock->setPos(80, 0);
    m_linkDock->setPos(80, -5);
    m_renderToggle->setPos(55, -5);
  }

  m_nameItem->setZValue(3);
  m_outDock->setZValue(2);
  m_renderToggle->setZValue(2);
  m_painter->setZValue(1);

  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  connect(m_renderToggle, SIGNAL(toggled(bool)), this,
          SLOT(onRenderToggleClicked(bool)));

  if (zeraryFx) {
    int i, inputPorts = zeraryFx->getInputPortCount();
    double lastPosY = (m_isLargeScaled) ? m_height : 0;

    for (i = 0; i < inputPorts; i++) {
      FxSchematicDock *inDock;
      if (m_isLargeScaled) {
        inDock = new FxSchematicDock(
            this, QString::fromStdString(zeraryFx->getInputPortName(i)),
            m_width - 18, eFxInputPort);
        inDock->setPos(0, lastPosY);
        lastPosY += inDock->boundingRect().height();
      } else {
        inDock = new FxSchematicDock(
            this, QString::fromStdString(zeraryFx->getInputPortName(i)), 10,
            eFxInputPort);
        inDock->setPos(0, lastPosY);
        lastPosY += inDock->boundingRect().height() + 4;  // 4 is margin
      }

      m_inDocks.push_back(inDock);
      addPort(i + 1, inDock->getPort());
    }
  }

  updatePortsPosition();
  updateLinksGeometry();
}

//-----------------------------------------------------

FxSchematicZeraryNode::~FxSchematicZeraryNode() {}

//-----------------------------------------------------

QRectF FxSchematicZeraryNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxSchematicZeraryNode::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
  FxSchematicNode::paint(painter, option, widget);
}

//-----------------------------------------------------

void FxSchematicZeraryNode::onRenderToggleClicked(bool toggled) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TXshColumn *column = fxScene->getXsheet()->getColumn(m_columnIndex);
  if (column) {
    column->setPreviewVisible(toggled);
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//-----------------------------------------------------

bool FxSchematicZeraryNode::isCached() const {
  TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(m_fx.getPointer());
  if (!zfx)
    return TPassiveCacheManager::instance()->cacheEnabled(m_fx.getPointer());
  TFx *zeraryFx = zfx->getZeraryFx();

  if (zeraryFx)
    return TPassiveCacheManager::instance()->cacheEnabled(zeraryFx);
  else
    return TPassiveCacheManager::instance()->cacheEnabled(m_fx.getPointer());
}

//-----------------------------------------------------

void FxSchematicZeraryNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QAction *fxEditorPopup =
      CommandManager::instance()->getAction("MI_FxParamEditor");
  fxEditorPopup->trigger();

  // this signal cause the update the contents of the FxSettings
  emit fxNodeDoubleClicked();
}

//-----------------------------------------------------

void FxSchematicZeraryNode::onNameChanged() {
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_painter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFxCommand::renameFx(m_fx.getPointer(), m_name.toStdWString(),
                       fxScene->getXsheetHandle());
  updateOutputDockToolTips(m_name);
  emit sceneChanged();
  update();
}

//-----------------------------------------------------

void FxSchematicZeraryNode::resize(bool maximized) {}

//*****************************************************
//
// FxSchematicColumnNode
//
//*****************************************************

FxSchematicColumnNode::FxSchematicColumnNode(FxSchematicScene *scene,
                                             TLevelColumnFx *fx)
    : FxSchematicNode(scene, fx, 90, 32, eColumnFx)
    , m_isOpened(false)  // iwasawa
{
  if (!m_isLargeScaled) {
    setWidth(90);
    setHeight(50);
  }
  m_columnIndex     = fx->getColumnIndex();
  TStageObjectId id = TStageObjectId::ColumnId(m_columnIndex);
  std::string name  = scene->getXsheet()->getStageObject(id)->getName();
  m_name            = QString::fromStdString(name);

  m_resizeItem = new SchematicThumbnailToggle(
      this, fx->getAttributes()->isOpened());    //サムネイル矢印
  m_nameItem = new SchematicName(this, 54, 20);  //リネーム部分
  m_outDock  = new FxSchematicDock(this, "", 0, eFxOutputPort);  // Outポート
  m_renderToggle =
      new SchematicToggle(this, QPixmap(":Resources/schematic_prev_eye.png"),
                          SchematicToggle::eIsParentColumn, m_isLargeScaled);
  m_cameraStandToggle = new SchematicToggle(
      this, QPixmap(":Resources/schematic_table_view.png"),
      QPixmap(":Resources/schematic_table_view_transp.png"),
      SchematicToggle::eIsParentColumn | SchematicToggle::eEnableNullState,
      m_isLargeScaled);
  m_columnPainter = new FxColumnPainter(this, m_width, m_height, m_name);

  // no link port
  m_linkedNode = 0;
  m_linkDock   = 0;

  //-----
  m_nameItem->setName(m_name);
  setToolTip(m_name);

  addPort(0, m_outDock->getPort());

  m_nameItem->hide();
  TXshColumn *column = scene->getXsheet()->getColumn(m_columnIndex);
  if (column) {
    m_renderToggle->setIsActive(column->isPreviewVisible());
    m_cameraStandToggle->setState(
        column->isCamstandVisible() ? (column->getOpacity() < 255 ? 2 : 1) : 0);
  }

  // set geometry
  if (m_isLargeScaled) {
    m_resizeItem->setPos(2, 0);
    m_nameItem->setPos(16, -1);
    m_outDock->setPos(72, 14);
    m_renderToggle->setPos(72, 0);
    m_cameraStandToggle->setPos(72, 7);
  } else {
    m_resizeItem->hide();
    m_nameItem->setPos(0, 0);
    m_outDock->setPos(80, 0);
    m_renderToggle->setPos(60, -5);
    m_cameraStandToggle->setPos(30, -5);
  }

  m_resizeItem->setZValue(2);
  m_nameItem->setZValue(2);
  m_outDock->setZValue(2);
  m_renderToggle->setZValue(2);
  m_cameraStandToggle->setZValue(2);
  m_columnPainter->setZValue(1);

  bool ret = true;
  ret      = ret && connect(m_resizeItem, SIGNAL(toggled(bool)), this,
                       SLOT(onChangedSize(bool)));
  ret = ret &&
        connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  ret = ret && connect(m_renderToggle, SIGNAL(toggled(bool)), this,
                       SLOT(onRenderToggleClicked(bool)));
  ret = ret && connect(m_cameraStandToggle, SIGNAL(stateChanged(int)), this,
                       SLOT(onCameraStandToggleClicked(int)));

  assert(ret);

  onChangedSize(fx->getAttributes()->isOpened());
}

//-----------------------------------------------------

FxSchematicColumnNode::~FxSchematicColumnNode() {}

//-----------------------------------------------------

QRectF FxSchematicColumnNode::boundingRect() const {
  if (m_isOpened && m_isLargeScaled)
    return QRectF(-5, -54, m_width + 10, m_height + 59);
  else
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxSchematicColumnNode::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
  FxSchematicNode::paint(painter, option, widget);
}

//-----------------------------------------------------

void FxSchematicColumnNode::onRenderToggleClicked(bool toggled) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TXshColumn *column = fxScene->getXsheet()->getColumn(m_columnIndex);
  if (column) {
    column->setPreviewVisible(toggled);
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//-----------------------------------------------------

void FxSchematicColumnNode::onCameraStandToggleClicked(int state) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TXshColumn *column = fxScene->getXsheet()->getColumn(m_columnIndex);
  if (column) {
    column->setCamstandVisible(!column->isCamstandVisible());
    // column->setCamstandVisible(toggled);
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//-----------------------------------------------------

QPixmap FxSchematicColumnNode::getPixmap() {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return QPixmap();
  TXsheet *xsh = fxScene->getXsheet();
  if (xsh && !xsh->isColumnEmpty(m_columnIndex)) {
    int r0, r1;
    xsh->getCellRange(m_columnIndex, r0, r1);
    if (r1 >= r0) {
      TXshCell cell = xsh->getCell(r0, m_columnIndex);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl)
        return IconGenerator::instance()->getIcon(xl, cell.m_frameId, false);
    }
  }
  return QPixmap();
}

//--------------------------------------------------------

void FxSchematicColumnNode::getLevelTypeAndName(int &ltype,
                                                QString &levelName) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) {
    ltype     = NO_XSHLEVEL;
    levelName = QString();
    return;
  }

  TXsheet *xsh = fxScene->getXsheet();
  if (xsh && !xsh->isColumnEmpty(m_columnIndex)) {
    int r0, r1;
    xsh->getCellRange(m_columnIndex, r0, r1);
    if (r1 >= r0) {
      TXshCell cell = xsh->getCell(r0, m_columnIndex);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl) {
        ltype = cell.m_level->getType();

        levelName = QString::fromStdWString(xl->getName());
        return;
      }
    }
  }

  ltype     = NO_XSHLEVEL;
  levelName = QString();
  return;
}

//-----------------------------------------------------

void FxSchematicColumnNode::onChangedSize(bool expand) {
  prepareGeometryChange();
  m_isOpened = expand;
  m_fx->getAttributes()->setIsOpened(m_isOpened);
  m_height = (m_isLargeScaled) ? 32 : 50;
  updateLinksGeometry();
  update();
}

//-----------------------------------------------------

void FxSchematicColumnNode::onNameChanged() {
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_columnPainter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  TStageObjectId id = TStageObjectId::ColumnId(m_columnIndex);
  renameObject(id, m_name.toStdString());
  updateOutputDockToolTips(m_name);
  emit sceneChanged();
  update();
}

//-----------------------------------------------------

void FxSchematicColumnNode::resize(bool maximized) {
  m_resizeItem->setIsDown(!maximized);
}

//-----------------------------------------------------

void FxSchematicColumnNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(14, 0, m_width - 15, 14);
  if (nameArea.contains(me->pos()) && me->modifiers() == Qt::ControlModifier) {
    TStageObjectId id         = TStageObjectId::ColumnId(m_columnIndex);
    FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
    if (!fxScene) return;
    TStageObject *pegbar = fxScene->getXsheet()->getStageObject(id);
    if (!pegbar) return;
    m_name = QString::fromStdString(pegbar->getName());
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  }
}

//-----------------------------------------------------

void FxSchematicColumnNode::renameObject(const TStageObjectId &id,
                                         std::string name) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TStageObjectCmd::rename(id, name, fxScene->getXsheetHandle());
}

//*****************************************************
//
// FxSchematicPaletteNode
//
//*****************************************************

FxSchematicPaletteNode::FxSchematicPaletteNode(FxSchematicScene *scene,
                                               TPaletteColumnFx *fx)
    : FxSchematicNode(scene, fx, 90, 32, eColumnFx) {
  if (!m_isLargeScaled) {
    setWidth(90);
    setHeight(50);
  }
  m_columnIndex     = fx->getColumnIndex();
  TStageObjectId id = TStageObjectId::ColumnId(m_columnIndex);
  std::string name  = scene->getXsheet()->getStageObject(id)->getFullName();
  m_name            = QString::fromStdString(name);

  m_linkedNode = 0;
  m_linkDock   = 0;
  m_nameItem   = new SchematicName(this, 54, 20);  // for rename
  m_outDock    = new FxSchematicDock(this, "", 0, eFxOutputPort);
  m_renderToggle =
      new SchematicToggle(this, QPixmap(":Resources/schematic_prev_eye.png"),
                          SchematicToggle::eIsParentColumn, m_isLargeScaled);
  m_palettePainter = new FxPalettePainter(this, m_width, m_height, m_name);

  //----
  QString paletteName = getPaletteName();
  setToolTip(QString("%1 : %2").arg(m_name, paletteName));

  m_nameItem->setName(m_name);

  addPort(0, m_outDock->getPort());

  TXshColumn *column = scene->getXsheet()->getColumn(m_columnIndex);
  if (column) m_renderToggle->setIsActive(column->isPreviewVisible());

  // set geometry
  if (m_isLargeScaled) {
    m_nameItem->setPos(19, -1);
    m_outDock->setPos(72, 14);
    m_renderToggle->setPos(72, 0);
  } else {
    QFont fnt = m_nameItem->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    m_nameItem->setFont(fnt);

    m_nameItem->setPos(-1, 0);
    m_outDock->setPos(80, 0);
    m_renderToggle->setPos(60, -5);
  }

  m_nameItem->setZValue(2);
  m_outDock->setZValue(2);
  m_renderToggle->setZValue(2);
  m_palettePainter->setZValue(1);

  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  connect(m_renderToggle, SIGNAL(toggled(bool)), this,
          SLOT(onRenderToggleClicked(bool)));

  m_nameItem->hide();
  prepareGeometryChange();
  m_fx->getAttributes()->setIsOpened(false);
}

//-----------------------------------------------------

QString FxSchematicPaletteNode::getPaletteName() {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) {
    return QString();
  }

  TXsheet *xsh = fxScene->getXsheet();
  if (xsh && !xsh->isColumnEmpty(m_columnIndex)) {
    int r0, r1;
    xsh->getCellRange(m_columnIndex, r0, r1);
    if (r1 >= r0) {
      TXshCell cell = xsh->getCell(r0, m_columnIndex);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl) {
        return QString::fromStdWString(xl->getName());
      }
    }
  }

  return QString();
}

//-----------------------------------------------------

FxSchematicPaletteNode::~FxSchematicPaletteNode() {}

//-----------------------------------------------------

QRectF FxSchematicPaletteNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxSchematicPaletteNode::paint(QPainter *painter,
                                   const QStyleOptionGraphicsItem *option,
                                   QWidget *widget) {
  FxSchematicNode::paint(painter, option, widget);
}

//-----------------------------------------------------

void FxSchematicPaletteNode::onRenderToggleClicked(bool toggled) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TXshColumn *column = fxScene->getXsheet()->getColumn(m_columnIndex);
  if (column) {
    column->setPreviewVisible(toggled);
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//-----------------------------------------------------

QPixmap FxSchematicPaletteNode::getPixmap() { return QPixmap(); }

//-----------------------------------------------------

void FxSchematicPaletteNode::onNameChanged() {
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_palettePainter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  TStageObjectId id = TStageObjectId::ColumnId(m_columnIndex);
  renameObject(id, m_name.toStdString());
  updateOutputDockToolTips(m_name);
  emit sceneChanged();
  update();
}

//-----------------------------------------------------

void FxSchematicPaletteNode::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(18, 2, 54, 14);
  if (nameArea.contains(me->pos()) && me->modifiers() == Qt::ControlModifier) {
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  } else {
    QAction *fxEitorPopup =
        CommandManager::instance()->getAction("MI_FxParamEditor");
    fxEitorPopup->trigger();
  }
}

//-----------------------------------------------------

void FxSchematicPaletteNode::renameObject(const TStageObjectId &id,
                                          std::string name) {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TStageObjectCmd::rename(id, name, fxScene->getXsheetHandle());
}

//*****************************************************
//
// FxGroupNode
//
//*****************************************************

FxGroupNode::FxGroupNode(FxSchematicScene *scene, const QList<TFxP> &groupedFx,
                         const QList<TFxP> &roots, int groupId,
                         const std::wstring &groupName)
    : FxSchematicNode(scene, roots[0].getPointer(), 90, 32, eGroupedFx)
    , m_groupId(groupId)
    , m_groupedFxs(groupedFx) {
  if (!m_isLargeScaled) {
    setWidth(90);
    setHeight(50);
  }

  m_name  = QString::fromStdWString(groupName);
  m_roots = roots;

  m_nameItem = new SchematicName(this, 72, 20);  // for rename
  m_renderToggle =
      new SchematicToggle(this, QPixmap(":Resources/schematic_prev_eye.png"),
                          SchematicToggle::eIsParentColumn, m_isLargeScaled);
  m_outDock               = new FxSchematicDock(this, "", 0, eFxGroupedOutPort);
  FxSchematicDock *inDock = new FxSchematicDock(
      this, "Source", (m_isLargeScaled) ? m_width - 18 : 10, eFxGroupedInPort);

  m_painter = new FxPainter(this, m_width, m_height, m_name, m_type,
                            roots[0]->getFxType());

  m_linkedNode = 0;
  m_linkDock   = 0;

  //-----
  m_nameItem->setName(m_name);
  m_renderToggle->setIsActive(m_fx->getAttributes()->isEnabled());

  addPort(0, m_outDock->getPort());
  addPort(1, inDock->getPort());
  m_inDocks.push_back(inDock);

  // set geometry
  if (m_isLargeScaled) {
    m_nameItem->setPos(1, -1);
    m_renderToggle->setPos(72, 0);
    m_outDock->setPos(72, 14);
    inDock->setPos(0, m_height);
  } else {
    QFont fnt = m_nameItem->font();
    fnt.setPixelSize(fnt.pixelSize() * 2);
    m_nameItem->setFont(fnt);

    m_nameItem->setPos(-1, 0);
    m_renderToggle->setPos(60, -5);
    m_outDock->setPos(80, 0);
    inDock->setPos(0, 0);
  }

  m_nameItem->setZValue(3);
  m_renderToggle->setZValue(2);
  m_outDock->setZValue(2);
  inDock->setZValue(2);
  m_painter->setZValue(1);

  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
  connect(m_renderToggle, SIGNAL(toggled(bool)), this,
          SLOT(onRenderToggleClicked(bool)));
  m_nameItem->hide();

  setPos(computePos());
}

//-----------------------------------------------------

FxGroupNode::~FxGroupNode() {}

//-----------------------------------------------------

QRectF FxGroupNode::boundingRect() const {
  return QRectF(-5, -5, m_width + 10, m_height + 10);
}

//-----------------------------------------------------

void FxGroupNode::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *widget) {
  // FxSchematicNode::paint(painter,option,widget);
}

//-----------------------------------------------------

void FxGroupNode::updateFxsDagPosition(const TPointD &pos) const {
  QPointF qOldPos = computePos();
  TPointD oldPos(qOldPos.x(), qOldPos.y());
  TPointD delta = pos - oldPos;
  int i;
  for (i = 0; i < m_groupedFxs.size(); i++) {
    // If the node position is unidentified, then leave the placement of it to
    // placeNode() function.
    // if (m_groupedFxs[i]->getAttributes()->getDagNodePos() != TConst::nowhere)
    {
      m_groupedFxs[i]->getAttributes()->setDagNodePos(
          m_groupedFxs[i]->getAttributes()->getDagNodePos() + delta);
      TMacroFx *macro = dynamic_cast<TMacroFx *>(m_groupedFxs[i].getPointer());
      if (macro) {
        std::vector<TFxP> fxs = macro->getFxs();
        int i;
        for (i = 0; i < (int)fxs.size(); i++) {
          TPointD oldP = fxs[i]->getAttributes()->getDagNodePos();
          // if (oldP != TConst::nowhere)
          fxs[i]->getAttributes()->setDagNodePos(oldP + delta);
        }
      }
    }
  }
}

//-----------------------------------------------------

void FxGroupNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *me) {
  QRectF nameArea(0, 0, m_width, 14);
  if (nameArea.contains(me->pos())) {
    m_nameItem->setPlainText(m_name);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  }
}

//-----------------------------------------------------

QPointF FxGroupNode::computePos() const {
  int i, notCounted = 0, fxCount = m_groupedFxs.size();
  TPointD pos;
  for (i = 0; i < fxCount; i++) {
    TFx *fx       = m_groupedFxs[i].getPointer();
    TPointD fxPos = fx->getAttributes()->getDagNodePos();
    if (fxPos != TConst::nowhere)
      pos += fxPos;
    else
      notCounted++;
  }
  fxCount -= notCounted;
  if (fxCount > 0)
    return QPointF(pos.x / fxCount, pos.y / fxCount);
  else if (fxCount == 0 && pos != TPointD())
    return QPointF(pos.x, pos.y);
  return QPointF(25000, 25000);  // Qualcosa e' andato male... posiziono nel
                                 // cebntro della scena per non far danni
}

//-----------------------------------------------------

void FxGroupNode::onNameChanged() {
  m_nameItem->hide();
  m_name = m_nameItem->toPlainText();
  m_painter->setName(m_name);
  setToolTip(m_name);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFxCommand::renameGroup(m_groupedFxs.toStdList(), m_name.toStdWString(),
                          false, fxScene->getXsheetHandle());
  update();
}

//-----------------------------------------------------

void FxGroupNode::onRenderToggleClicked(bool value) {
  int i;
  for (i = 0; i < m_groupedFxs.size(); i++) {
    TFxP fx = m_groupedFxs[i];
    if (TLevelColumnFx *lcFx = dynamic_cast<TLevelColumnFx *>(fx.getPointer()))
      lcFx->getColumn()->setPreviewVisible(value);
    else
      fx->getAttributes()->enable(value);
  }
  update();
  emit sceneChanged();
  emit xsheetChanged();
}

//-----------------------------------------------------

void FxGroupNode::resize(bool maximized) {}

//-----------------------------------------------------

bool FxGroupNode::contains(TFxP fx) {
  int i;
  for (i = 0; i < m_groupedFxs.size(); i++) {
    if (m_groupedFxs[i].getPointer() == fx.getPointer()) return true;
  }
  return false;
}

//-----------------------------------------------------

int FxGroupNode::getOutputConnectionsCount() const {
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  assert(fxScene);
  TXsheet *xsh = fxScene->getXsheet();
  assert(xsh);

  int i, count = 0;
  for (i = 0; i < m_roots.size(); i++) {
    TFx *fx = m_roots[i].getPointer();
    count += fx->getOutputConnectionCount();

    if (xsh->getFxDag()->getTerminalFxs()->containsFx(fx)) count++;
  }
  return count;
}

//-----------------------------------------------------

bool FxGroupNode::isEnabled() const {
  int i;
  bool isEnabled = true;
  for (i = 0; i < m_roots.size(); i++) {
    TFx *fx = m_roots[i].getPointer();
    if (TZeraryColumnFx *zcFx = dynamic_cast<TZeraryColumnFx *>(fx))
      isEnabled = isEnabled && zcFx->getColumn()->isPreviewVisible();
    else
      isEnabled = isEnabled && fx->getAttributes()->isEnabled();
  }
  return isEnabled;
}

//-----------------------------------------------------

bool FxGroupNode::isCached() const {
  int i;
  bool isCached = true;
  for (i = 0; i < m_roots.size(); i++) {
    TFx *fx = m_roots[i].getPointer();
    if (TZeraryColumnFx *zcFx = dynamic_cast<TZeraryColumnFx *>(fx))
      isCached =
          isCached &&
          TPassiveCacheManager::instance()->cacheEnabled(zcFx->getZeraryFx());
    else
      isCached = isCached && TPassiveCacheManager::instance()->cacheEnabled(fx);
  }
  return isCached;
}
