#include "historypane.h"

#include "tundo.h"
#include "historytypes.h"

#include <QApplication>
#include <QPainter>
#include <QScrollBar>
#include <QFrame>
#include <QPaintEvent>
#include <QVBoxLayout>

#include <QMutex>

const int HISTORY_ITEM_HEIGHT = 20;

namespace {

const struct {
  int historyType;
  const char *pixmapFilename;
} historyTypeInfo[] = {
    {HistoryType::Unidentified, "history_normal"},
    {HistoryType::BrushTool, "history_brush"},
    {HistoryType::EraserTool, "history_eraser"},
    {HistoryType::FillTool, "history_fill"},
    {HistoryType::PaintBrushTool, "history_paintbrush"},
    {HistoryType::AutocloseTool, "history_autoclose"},
    {HistoryType::GeometricTool, "history_geometric"},
    {HistoryType::ControlPointEditorTool, "history_controlpointeditor"},
    {HistoryType::EditTool_Move, "history_move"},
    {HistoryType::FingerTool, "history_finger"},
    //{HistoryType::PickTool,			"history_pick"},
    {HistoryType::Palette, "history_palette"},
    {HistoryType::Fx, "history_fx"},
    {HistoryType::Schematic, "history_schematic"},
    {HistoryType::Xsheet, "history_xsheet"},
    {HistoryType::FilmStrip, "history_filmstrip"},

    {0, 0}};
};

class HistoryPixmapManager {  // singleton

  std::map<int, QPixmap> m_pms;

  HistoryPixmapManager() {}

public:
  static HistoryPixmapManager *instance() {
    static HistoryPixmapManager _instance;
    return &_instance;
  }

  const QPixmap &getHistoryPm(int type) {
    std::map<int, QPixmap>::iterator it;
    it = m_pms.find(type);
    if (it != m_pms.end()) return it->second;

    int i;
    for (i = 0; historyTypeInfo[i].pixmapFilename; i++)
      if (type == historyTypeInfo[i].historyType) {
        QString path =
            QString(":Resources/") + historyTypeInfo[i].pixmapFilename + ".png";
        it = m_pms.insert(std::make_pair(type, QPixmap(path))).first;
        return it->second;
      }

    static const QPixmap standardCursorPixmap("history_unidentified.png");
    it = m_pms.insert(std::make_pair(type, standardCursorPixmap)).first;
    return it->second;
  }
};
//-----------------------------------------------------------------------------
#if QT_VERSION >= 0x050500
HistoryField::HistoryField(QScrollArea *parent, Qt::WindowFlags flags)
#else
HistoryField::HistoryField(QScrollArea *parent, Qt::WFlags flags)
#endif
    : QFrame(parent, flags), m_scrollArea(parent) {
  setObjectName("filmStripFrames");
  setFrameStyle(QFrame::StyledPanel);

  setFocusPolicy(Qt::StrongFocus); /*-- KeyboadでもTabキーでもFocusされる --*/

  setFixedHeight(parentWidget()->height());
  setMinimumWidth(std::max(parentWidget()->width(), 600));
}
//-----------------------------------------------------------------------------

void HistoryField::updateContentHeight(int minimumHeight) {
  if (minimumHeight < 0)
    minimumHeight = visibleRegion().boundingRect().bottom();
  int contentHeight =
      TUndoManager::manager()->getHistoryCount() * HISTORY_ITEM_HEIGHT;
  if (contentHeight < minimumHeight) contentHeight = minimumHeight;
  int parentHeight                                 = parentWidget()->height();
  if (contentHeight < parentHeight) contentHeight  = parentHeight;
  if (contentHeight != height()) setFixedHeight(contentHeight);
}

//-----------------------------------------------------------------------------

void HistoryField::paintEvent(QPaintEvent *evt) {
  QPainter p(this);

  QRect clipRect = evt->rect();

  int i0 = y2index(clipRect.top());
  int i1 = y2index(clipRect.bottom());

  int currentHistoryIndex = TUndoManager::manager()->getCurrentHistoryIndex();
  int currentHistoryCount = TUndoManager::manager()->getHistoryCount();

  QRect undoChipRect(0, 0, width(), HISTORY_ITEM_HEIGHT);
  QRect undoIconRect(7, 2, 17, 17);

  for (int i = i0; i <= i1; i++) {
    if (0 <= i && i <= currentHistoryCount) {
      QRect tmpRect = undoChipRect.translated(0, HISTORY_ITEM_HEIGHT * (i - 1));

      bool isCurrent = (i == currentHistoryIndex);
      bool isFuture  = (i > currentHistoryIndex);

      p.setPen(QColor(64, 64, 64));
      p.setBrush((isFuture) ? QColor(150, 150, 150) : QColor(192, 192, 192));
      if (isCurrent) p.setBrush(QColor(0, 0, 128));
      p.drawRect(tmpRect);

      // draw text
      QFont fon = p.font();
      fon.setItalic(isFuture);
      p.setFont(fon);

      p.setPen((isFuture) ? QColor(64, 64, 64) : Qt::black);
      if (isCurrent) p.setPen(Qt::white);

      tmpRect.adjust(30, 0, 0, 0);

      TUndo *tmpUndo = TUndoManager::manager()->getUndoItem(i);
      p.drawText(tmpRect, Qt::AlignLeft | Qt::AlignVCenter,
                 tmpUndo->getHistoryString());

      QRect tmpIconRect = undoIconRect.translated(0, 20 * (i - 1));
      p.drawPixmap(tmpIconRect, HistoryPixmapManager::instance()->getHistoryPm(
                                    tmpUndo->getHistoryType()));
    }
  }
}

//-----------------------------------------------------------------------------

int HistoryField::y2index(int y) const { return y / HISTORY_ITEM_HEIGHT + 1; }

//-----------------------------------------------------------------------------

int HistoryField::index2y(int index) const {
  return (index - 1) * HISTORY_ITEM_HEIGHT;
}

//-----------------------------------------------------------------------------

void HistoryField::mousePressEvent(QMouseEvent *event) {
  int index        = y2index(event->pos().y());
  int historyCount = TUndoManager::manager()->getHistoryCount();

  if (index > historyCount || index <= 0) return;

  int currentIndex = TUndoManager::manager()->getCurrentHistoryIndex();

  if (index == currentIndex) return;

  if (index < currentIndex)  // undo
  {
    for (int i = 0; i < (currentIndex - index); i++)
      TUndoManager::manager()->undo();
  } else  // redo
  {
    for (int i = 0; i < (index - currentIndex); i++)
      TUndoManager::manager()->redo();
  }
}

//-----------------------------------------------------------------------------

void HistoryField::exposeCurrent() {
  int currentIndex = TUndoManager::manager()->getCurrentHistoryIndex();

  m_scrollArea->ensureVisible(0, index2y(currentIndex) + 10, 50, 10);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#if QT_VERSION >= 0x050500
HistoryPane::HistoryPane(QWidget *parent, Qt::WindowFlags flags)
#else
HistoryPane::HistoryPane(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent) {
  m_frameArea = new QScrollArea(this);
  m_field     = new HistoryField(m_frameArea);

  m_frameArea->setObjectName("filmScrollArea");
  m_frameArea->setFrameStyle(QFrame::StyledPanel);
  m_frameArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_frameArea->verticalScrollBar()->setObjectName("LevelStripScrollBar");

  m_frameArea->setWidget(m_field);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  { mainLayout->addWidget(m_frameArea, 1); }
  setLayout(mainLayout);

  setFocusProxy(m_field);
}
//-----------------------------------------------------------------------------

void HistoryPane::onHistoryChanged() {
  m_field->updateContentHeight();
  m_field->exposeCurrent();
  update();
}

//-----------------------------------------------------------------------------

void HistoryPane::resizeEvent(QResizeEvent *e) {
  m_field->updateContentHeight();
  m_field->setFixedWidth(std::max(width(), 600));
}

//-----------------------------------------------------------------------------

void HistoryPane::showEvent(QShowEvent *) {
  connect(TUndoManager::manager(), SIGNAL(historyChanged()), this,
          SLOT(onHistoryChanged()));
  onHistoryChanged();
}

//-----------------------------------------------------------------------------

void HistoryPane::hideEvent(QHideEvent *) {
  disconnect(TUndoManager::manager(), SIGNAL(historyChanged()), this,
             SLOT(onHistoryChanged()));
}