

#include "dvitemview.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "filebrowser.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tlevel_io.h"
#include "tfiletype.h"
#include "tsystem.h"

// Qt includes
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QAction>
#include <QDate>
#include <QContextMenuEvent>
#include <QMenu>
#include <QScrollBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QTextStream>
#include <qdrawutil.h>

#include <stdint.h>  // for uint64_t

TEnv::IntVar BrowserView("BrowserView", 1);
TEnv::IntVar CastView("CastView", 1);
TEnv::IntVar BrowserFileSizeisVisible("BrowserFileSizeisVisible", 1);
TEnv::IntVar BrowserFrameCountisVisible("BrowserFrameCountisVisible", 1);
TEnv::IntVar BrowserCreationDateisVisible("BrowserCreationDateisVisible", 1);
TEnv::IntVar BrowserModifiedDateisVisible("BrowserModifiedDateisVisible", 1);
TEnv::IntVar BrowserFileTypeisVisible("BrowserFileTypeisVisible", 1);
TEnv::IntVar BrowserVersionControlStatusisVisible(
    "BrowserVersionControlStatusisVisible", 1);

//************************************************************************
//    Local namespace  stuff
//************************************************************************

namespace {

void getFileFids(TFilePath path, std::vector<TFrameId> &fids) {
  QFileInfo info(QString::fromStdWString(path.getWideString()));
  TLevelP level;
  if (info.exists()) {
    if (path.getType() == "tnz") {
      try {
        ToonzScene scene;
        scene.loadNoResources(path);
        int i;
        for (i = 0; i < scene.getFrameCount(); i++)
          fids.push_back(TFrameId(i + 1));
      } catch (...) {
      }
    } else if (TFileType::isViewable(TFileType::getInfo(path))) {
      try {
        TLevelReaderP lr(path);
        level = lr->loadInfo();
      } catch (...) {
      }
    }
  } else if (path.isLevelName())  // for levels johndoe..tif etc.
  {
    try {
      TLevelReaderP lr(path);
      level = lr->loadInfo();
    } catch (...) {
    }
  }
  if (level.getPointer()) {
    for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
      fids.push_back(it->first);
  }
}

//-----------------------------------------------------------------------------

QString hyphenText(const QString &srcText, const QFont &font, int width) {
  QFontMetrics metrics(font);
  int srcWidth = metrics.width(srcText);
  if (srcWidth < width) return srcText;

  int count = double(srcWidth) / double(width);
  int diff  = srcWidth - width * count + 4;  // +4 to keep a margin

  QString text;
  int middleWidth = (double(width) * 0.5);
  int i;
  int hyphenCount = 1;
  for (i = 0; i < srcText.size(); i++) {
    QChar c       = srcText.at(i);
    int cWidth    = metrics.width(c);
    int textWidth = metrics.width(text) + cWidth;
    if ((c.isSpace() && textWidth > (hyphenCount - 1) * width + diff) ||
        (textWidth > hyphenCount * width)) {
      ++hyphenCount;
      --i;
      text += "\n";
    } else
      text += c;
  }
  return text;
}

QPixmap getStatusPixmap(int status) {
  static QPixmap bronzePixmap     = QPixmap(":Resources/bronze.png");
  static QPixmap plusPixmap       = QPixmap(":Resources/plus.png");
  static QPixmap greenPixmap      = QPixmap(":Resources/green.png");
  static QPixmap redPixmap        = QPixmap(":Resources/red.png");
  static QPixmap orangePixmap     = QPixmap(":Resources/orange.png");
  static QPixmap grayPixmap       = QPixmap(":Resources/gray.png");
  static QPixmap halfGreenPixmap  = QPixmap(":Resources/halfGreen.png");
  static QPixmap halfBronzePixmap = QPixmap(":Resources/halfBronze.png");
  static QPixmap halfRedPixmap    = QPixmap(":Resources/halfRed.png");

  // Icon
  if (status == DvItemListModel::VC_Locked)
    return bronzePixmap;
  else if (status == DvItemListModel::VC_Edited)
    return greenPixmap;
  else if (status == DvItemListModel::VC_ToUpdate)
    return orangePixmap;
  else if (status == DvItemListModel::VC_Unversioned)
    return plusPixmap;
  else if (status == DvItemListModel::VC_ReadOnly)
    return grayPixmap;
  else if (status == DvItemListModel::VC_PartialEdited)
    return halfGreenPixmap;
  else if (status == DvItemListModel::VC_PartialLocked)
    return halfBronzePixmap;
  else if (status == DvItemListModel::VC_PartialModified)
    return halfRedPixmap;
  else if (status == DvItemListModel::VC_Modified)
    return redPixmap;
  else
    return QPixmap();
}

}  // namespace

//=============================================================================
//
// DvItemListModel
//
//-----------------------------------------------------------------------------

QString DvItemListModel::getItemDataAsString(int index, DataType dataType) {
  QVariant value = getItemData(index, dataType);
  if (value == QVariant()) return "";
  switch (dataType) {
  case Name:
  case ToolTip:
  case FullPath:
    return value.toString();
  case Thumbnail:
  case Icon:
    return "";
  case CreationDate:
    return value.toDateTime().toString(Qt::SystemLocaleShortDate);
    break;
  case ModifiedDate:
    return value.toDateTime().toString(Qt::SystemLocaleShortDate);
    break;
  case FileSize: {
    if (getItemData(index, IsFolder).toBool()) return QString("");

    uint64_t byteSize = value.toLongLong();

    if (byteSize < 1024) return QString::number(byteSize) + " bytes";

    int size = (byteSize) >> 10;  // divide by 1024

    if (size < 1024)
      return QString::number(size) + " KB";
    else if (size < 1024 * 1024)
      return QString::number((double)size / 1024.0) + " MB";
    else
      return QString::number((double)size / (1024 * 1024)) + " GB";
  } break;
  case FrameCount: {
    int frameCount = value.toInt();
    return frameCount > 0 ? QString::number(frameCount) : "";
  } break;
  case VersionControlStatus: {
    Status s = (Status)value.toInt();
    switch (s) {
    case VC_None:
      return QObject::tr("None");
      break;
    case VC_Edited:
      return QObject::tr("Edited");
      break;
    case VC_ReadOnly:
      return QObject::tr("Normal");
      break;
    case VC_ToUpdate:
      return QObject::tr("To Update");
      break;
    case VC_Modified:
      return QObject::tr("Modified");
      break;
    case VC_Locked:
      return QObject::tr("Locked");
      break;
    case VC_Unversioned:
      return QObject::tr("Unversioned");
      break;
    case VC_Missing:
      return QObject::tr("Missing");
      break;
    case VC_PartialEdited:
      return QObject::tr("Partially Edited");
      break;
    case VC_PartialLocked:
      return QObject::tr("Partially Locked");
      break;
    case VC_PartialModified:
      return QObject::tr("Partially Modified");
      break;
    }
    return QObject::tr("None");
  } break;
  case FileType:
    return value.toString();
  default:
    return "";
  }
}

//-----------------------------------------------------------------------------

QString DvItemListModel::getItemDataIdentifierName(DataType dataType) {
  switch (dataType) {
  case Name:
    return QObject::tr("Name");
  case ToolTip:
    return "";
  case FullPath:
    return QObject::tr("Path");
  case Thumbnail:
    return "";
  case Icon:
    return "";
  case CreationDate:
    return QObject::tr("Date Created");
  case ModifiedDate:
    return QObject::tr("Date Modified");
  case FileSize:
    return QObject::tr("Size");
  case FrameCount:
    return QObject::tr("Frames");
  case VersionControlStatus:
    return QObject::tr("Version Control");
  case FileType:
    return QObject::tr("Type");
  default:
    return "";
  }
}

//-----------------------------------------------------------------------------

int DvItemListModel::compareData(DataType dataType, int firstIndex,
                                 int secondIndex) {
  QVariant firstValue  = getItemData(firstIndex, dataType);
  QVariant secondValue = getItemData(secondIndex, dataType);

  switch (dataType) {
  case Name:
  case FileType:
    return QString::localeAwareCompare(firstValue.toString(),
                                       secondValue.toString());

  case CreationDate:
  case ModifiedDate: {
    if (firstValue.toDateTime() < secondValue.toDateTime()) return 1;
    if (firstValue.toDateTime() == secondValue.toDateTime()) return 0;
    if (firstValue.toDateTime() > secondValue.toDateTime()) return -1;
  }

  case FileSize:
    return firstValue.toLongLong() - secondValue.toLongLong();

  case FrameCount:
    return firstValue.toInt() - secondValue.toInt();

  case VersionControlStatus:
    return firstValue.toInt() < secondValue.toInt();
  }

  return 0;
}

//=============================================================================
//
// DvItemSelection
//
//-----------------------------------------------------------------------------

DvItemSelection::DvItemSelection() : m_model(0) {}

//-----------------------------------------------------------------------------

void DvItemSelection::select(int index, bool on) {
  if (on)
    m_selectedIndices.insert(index);
  else
    m_selectedIndices.erase(index);
  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::select(int *indices, int indicesCount) {
  m_selectedIndices.clear();
  m_selectedIndices.insert(indices, indices + indicesCount);

  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::selectNone() {
  m_selectedIndices.clear();
  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::selectAll() {
  m_selectedIndices.clear();
  int i = 0;
  for (i = 0; i < m_model->getItemCount(); i++) m_selectedIndices.insert(i);
  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::setModel(DvItemListModel *model) { m_model = model; }

//-----------------------------------------------------------------------------

void DvItemSelection::enableCommands() {
  if (m_model) m_model->enableSelectionCommands(this);
}

//=============================================================================
//
// ItemViewPlayWidget::PlayManager
//
//-----------------------------------------------------------------------------

ItemViewPlayWidget::PlayManager::PlayManager()
    : m_path(TFilePath())
    , m_currentFidIndex(0)
    , m_pixmap(QPixmap())
    , m_iconSize(QSize()) {}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::PlayManager::reset() {
  int i;
  for (i = 1; i < m_fids.size(); i++)
    IconGenerator::instance()->remove(m_path, m_fids[i]);
  m_fids.clear();
  m_path            = TFilePath();
  m_currentFidIndex = 0;
  m_pixmap          = QPixmap();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::PlayManager::setInfo(DvItemListModel *model,
                                              int index) {
  assert(!!model && index >= 0);
  QString string =
      model->getItemData(index, DvItemListModel::FullPath).toString();
  TFilePath path = TFilePath(string.toStdWString());
  if (!m_path.isEmpty() && !m_fids.empty() &&
      path == m_path)  // Ho gia' il path e i frameId settati correttamente
  {
    m_currentFidIndex = 0;
    m_pixmap          = QPixmap();
    return;
  }

  reset();
  m_pixmap =
      model->getItemData(index, DvItemListModel::Thumbnail).value<QPixmap>();
  if (!m_pixmap.isNull()) m_iconSize = m_pixmap.size();
  m_path                             = path;
  getFileFids(m_path, m_fids);
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::increaseCurrentFrame() {
  QPixmap pixmap;
  if (m_currentFidIndex == 0)  // Il primo fid deve essere calcolato senza tener
                               // conto del frameId (dovrebbe esistere)
    pixmap = (m_pixmap.isNull()) ? IconGenerator::instance()->getIcon(m_path)
                                 : m_pixmap;
  else
    pixmap =
        IconGenerator::instance()->getIcon(m_path, m_fids[m_currentFidIndex]);
  if (pixmap.isNull())
    return false;  // Se non ha ancora finito di calcolare l'icona ritorno
  assert(!m_iconSize.isEmpty());
  m_pixmap = scalePixmapKeepingAspectRatio(pixmap, m_iconSize, Qt::transparent);
  ++m_currentFidIndex;
  return true;
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::getCurrentFrame() {
  QPixmap pixmap;
  if (m_currentFidIndex == 0)  // Il primo fid deve essere calcolato senza tener
                               // conto del frameId (dovrebbe esistere)
    pixmap = IconGenerator::instance()->getIcon(m_path);
  else
    pixmap =
        IconGenerator::instance()->getIcon(m_path, m_fids[m_currentFidIndex]);
  if (pixmap.isNull())
    return false;  // Se non ha ancora finito di calcolare l'icona ritorno
  assert(!m_iconSize.isEmpty());
  m_pixmap = scalePixmapKeepingAspectRatio(pixmap, m_iconSize, Qt::transparent);
  return true;
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::isFrameIndexInRange() {
  return (m_currentFidIndex >= 0 && m_currentFidIndex < m_fids.size());
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::setCurrentFrameIndexFromXValue(
    int xValue, int length) {
  if (m_fids.size() == 0) return false;
  double d     = (double)length / (double)(m_fids.size() - 1);
  int newIndex = tround((double)xValue / d);
  if (newIndex == m_currentFidIndex) return false;
  m_currentFidIndex = newIndex;
  assert(isFrameIndexInRange());
  return true;
}

//-----------------------------------------------------------------------------

double ItemViewPlayWidget::PlayManager::currentFrameIndexToXValue(int length) {
  if (m_fids.size() == 0) return false;
  double d = (double)length / (double)(m_fids.size() - 1);
  return d * m_currentFidIndex;
}

//-----------------------------------------------------------------------------

QPixmap ItemViewPlayWidget::PlayManager::getCurrentPixmap() { return m_pixmap; }

//=============================================================================
//
// ItemViewPlayWidget
//
//-----------------------------------------------------------------------------

ItemViewPlayWidget::ItemViewPlayWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentItemIndex(-1)
    , m_timerId(0)
    , m_isSliderMode(false) {
  m_playManager = new PlayManager();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::play() { m_timerId = startTimer(100); }

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::stop() {
  if (m_timerId != 0) {
    killTimer(m_timerId);
    m_timerId = 0;
  }
  if (!m_isSliderMode) m_currentItemIndex = -1;
}
//-----------------------------------------------------------------------------

void ItemViewPlayWidget::clear() {
  m_isSliderMode = false;
  if (m_currentItemIndex != -1) stop();
  m_playManager->reset();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::setIsPlaying(DvItemListModel *model, int index) {
  if (isIndexPlaying(index) &&
      !m_isSliderMode)  // Devo fare stop prima di inizializzare un nuovo play
  {
    stop();
    return;
  } else if (m_isSliderMode) {
    m_isSliderMode = false;
  }

  if (m_currentItemIndex == -1) {
    m_currentItemIndex = index;
    m_playManager->setInfo(model, index);
  }
  play();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::setIsPlayingCurrentFrameIndex(DvItemListModel *model,
                                                       int index, int xValue,
                                                       int length) {
  m_isSliderMode = true;

  if (m_currentItemIndex == -1) {
    m_currentItemIndex = index;
    m_playManager->setInfo(model, index);
  }
  if (!m_playManager->setCurrentFrameIndexFromXValue(xValue, length)) return;
  stop();  // Devo fare stop prima di cambiare il frame corrente
  play();
}

//-----------------------------------------------------------------------------

int ItemViewPlayWidget::getCurrentFramePosition(int length) {
  if (m_playManager->isFrameIndexInRange())
    return m_playManager->currentFrameIndexToXValue(length);
  return 0;
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::paint(QPainter *painter, QRect rect) {
  QPixmap pixmap = m_playManager->getCurrentPixmap();
  if (pixmap.isNull()) return;
  painter->drawPixmap(rect.adjusted(2, 2, -1, -1), pixmap);
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::timerEvent(QTimerEvent *event) {
  if (!m_playManager->isFrameIndexInRange()) {
    stop();
    parentWidget()->update();
    return;
  }
  if (m_isSliderMode)  // Modalita' slider
  {
    if (!m_playManager->getCurrentFrame()) return;
    parentWidget()->update();
    stop();
    return;
  }
  // Modalita' play
  if (!m_playManager->increaseCurrentFrame())
    return;
  else
    parentWidget()->update();
}

//=============================================================================
// DVItemViewPlayDelegate
//-----------------------------------------------------------------------------

DVItemViewPlayDelegate::DVItemViewPlayDelegate(QWidget *parent)
    : QObject(parent) {
  m_itemViewPlay = new ItemViewPlayWidget(parent);
}

//-----------------------------------------------------------------------------

bool DVItemViewPlayDelegate::setPlayWidget(DvItemListModel *model, int index,
                                           QRect rect, QPoint pos) {
  bool isPlaying = getPlayButtonRect(rect).contains(pos);
  if (!isPlaying && !getPlaySliderRect(rect).contains(pos)) return false;
  if (isPlaying)
    m_itemViewPlay->setIsPlaying(model, index);
  else
    m_itemViewPlay->setIsPlayingCurrentFrameIndex(
        model, index, pos.x() - getPlaySliderRect(rect).left(),
        getPlaySliderRect(rect).width());
  return true;
}

//-----------------------------------------------------------------------------

void DVItemViewPlayDelegate::resetPlayWidget() { m_itemViewPlay->clear(); }

//-----------------------------------------------------------------------------

void DVItemViewPlayDelegate::paint(QPainter *painter, QRect rect, int index) {
  if (m_itemViewPlay->isIndexPlaying(index) &&
      !m_itemViewPlay->isSliderMode())  // Modalita' play
  {
    QSize iconSize(10, 11);
    static QIcon playIcon = createQIconPNG("iconpause");
    QPixmap pixmap        = playIcon.pixmap(iconSize);
    if (m_itemViewPlay->isIndexPlaying(index))
      m_itemViewPlay->paint(painter, rect);
    painter->drawPixmap(getPlayButtonRect(rect), pixmap);
  } else {
    QSize iconSize(6, 11);
    static QIcon playIcon = createQIconPNG("iconplay");
    QPixmap pixmap        = playIcon.pixmap(iconSize);
    if (m_itemViewPlay->isIndexPlaying(
            index))  // Puo' servire in modalita' slider
      m_itemViewPlay->paint(painter, rect);
    painter->drawPixmap(getPlayButtonRect(rect).adjusted(2, 0, -2, 0), pixmap);
  }
  QRect sliderRect = getPlaySliderRect(rect);
  double xSliderValue =
      sliderRect.left() +
      m_itemViewPlay->getCurrentFramePosition(sliderRect.width()) - 1;
  QRect indicatorRect(xSliderValue, sliderRect.top() + 2, 2, 6);
  sliderRect = sliderRect.adjusted(0, 4, 0, -4);
  QColor sliderColor(171, 206, 255);
  painter->setPen(Qt::black);
  painter->fillRect(sliderRect, QBrush(sliderColor));
  painter->drawRect(sliderRect);
  painter->fillRect(indicatorRect, QBrush(sliderColor));
  painter->drawRect(indicatorRect);
}

//-----------------------------------------------------------------------------

QRect DVItemViewPlayDelegate::getPlayButtonRect(QRect rect) {
  QPoint iconSize = QPoint(10, 11);
  QPoint point    = rect.bottomRight() - iconSize;
  QRect playButtonRect(point.x(), point.y(), iconSize.x(), iconSize.y());
  return playButtonRect;
}

//-----------------------------------------------------------------------------

QRect DVItemViewPlayDelegate::getPlaySliderRect(QRect rect) {
  QPoint point = rect.bottomLeft() + QPoint(5, -10);
  QRect playSliderRect(point.x(), point.y(), rect.width() - 20, 10);
  return playSliderRect;
}

//=============================================================================
//
// DvItemViewerPanel
//
//-----------------------------------------------------------------------------

DvItemViewerPanel::DvItemViewerPanel(DvItemViewer *viewer, bool noContextMenu,
                                     bool multiSelectionEnabled,
                                     QWidget *parent)
    : QFrame(parent)
    , m_selection(0)
    , m_viewer(viewer)
    , m_viewType(viewer->m_windowType == DvItemViewer::Cast
                     ? to_enum(CastView)
                     : to_enum(BrowserView))
    , m_xMargin(0)
    , m_yMargin(0)
    , m_itemSpacing(0)
    , m_itemPerRow(0)
    , m_itemSize(0, 0)
    , m_iconSize(80, 60)
    , m_currentIndex(-1)
    , m_singleColumnEnabled(false)
    , m_centerAligned(false)
    , m_noContextMenu(noContextMenu)
    , m_isPlayDelegateDisable(true)
    , m_globalSelectionEnabled(true)
    , m_multiSelectionEnabled(multiSelectionEnabled)
    , m_missingColor(Qt::gray) {
  setFrameStyle(QFrame::StyledPanel);
  setFocusPolicy(Qt::StrongFocus);
  // setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
  QSizePolicy sizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
  sizePolicy.setHeightForWidth(true);
  setSizePolicy(sizePolicy);
  m_selection = new DvItemSelection();
  m_selection->setView(this);
  m_selection->setModel(m_viewer->getModel());
  connect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
          SLOT(update()));

  m_editFld = new DVGui::LineEdit(this);
  m_editFld->hide();
  connect(m_editFld, SIGNAL(editingFinished()), this, SLOT(rename()));

  m_columns.push_back(
      std::make_pair(DvItemListModel::Name, std::make_pair(200, 1)));
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setItemViewPlayDelegate(
    DVItemViewPlayDelegate *playDelegate) {
  m_isPlayDelegateDisable = false;
  m_itemViewPlayDelegate  = playDelegate;
}

//-----------------------------------------------------------------------------

DVItemViewPlayDelegate *DvItemViewerPanel::getItemViewPlayDelegate() {
  if (m_isPlayDelegateDisable) return 0;
  return m_itemViewPlayDelegate;
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::addColumn(DvItemListModel::DataType dataType,
                                  int width) {
  bool val;
  switch (dataType) {
  case DvItemListModel::FileSize:
    val = (bool)BrowserFileSizeisVisible;
    break;
  case DvItemListModel::FrameCount:
    val = (bool)BrowserFrameCountisVisible;
    break;
  case DvItemListModel::CreationDate:
    val = (bool)BrowserCreationDateisVisible;
    break;
  case DvItemListModel::ModifiedDate:
    val = (bool)BrowserModifiedDateisVisible;
    break;
  case DvItemListModel::FileType:
    val = (bool)BrowserFileTypeisVisible;
    break;
  case DvItemListModel::VersionControlStatus:
    val = (bool)BrowserVersionControlStatusisVisible;
    break;
  default:
    val = true;
  }
  m_columns.push_back(std::make_pair(dataType, std::make_pair(width, val)));
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setColumnWidth(DvItemListModel::DataType dataType,
                                       int width) {
  int i;
  for (i = 0; i < m_columns.size(); i++) {
    if (m_columns[i].first != dataType)
      continue;
    else
      m_columns[i].second.first = width;
  }
  update();
}

//-----------------------------------------------------------------------------

bool DvItemViewerPanel::getVisibility(DvItemListModel::DataType dataType) {
  int i;
  for (i = 0; i < m_columns.size(); i++) {
    if (m_columns[i].first != dataType)
      continue;
    else
      return m_columns[i].second.second;
  }
  return 0;
}
//-----------------------------------------------------------------------------

void DvItemViewerPanel::setVisibility(DvItemListModel::DataType dataType,
                                      bool value) {
  int i;
  for (i = 0; i < m_columns.size(); i++) {
    if (m_columns[i].first != dataType)
      continue;
    else
      m_columns[i].second.second = value;
  }
  switch (dataType) {
  case DvItemListModel::FileSize:
    BrowserFileSizeisVisible = value;
    break;
  case DvItemListModel::FrameCount:
    BrowserFrameCountisVisible = value;
    break;
  case DvItemListModel::CreationDate:
    BrowserCreationDateisVisible = value;
    break;
  case DvItemListModel::ModifiedDate:
    BrowserModifiedDateisVisible = value;
    break;
  case DvItemListModel::FileType:
    BrowserFileTypeisVisible = value;
    break;
  case DvItemListModel::VersionControlStatus:
    BrowserVersionControlStatusisVisible = value;
    break;
  }
}

//-----------------------------------------------------------------------------

DvItemListModel *DvItemViewerPanel::getModel() const {
  return m_viewer->getModel();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setSelection(DvItemSelection *selection) {
  if (m_selection == selection) return;
  delete m_selection;
  m_selection = selection;
  m_selection->setModel(getModel());
}

//-----------------------------------------------------------------------------

const std::set<int> &DvItemViewerPanel::getSelectedIndices() const {
  return m_selection->getSelectedIndices();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::updateViewParameters(int panelWidth) {
  m_itemPerRow  = 1;
  m_xMargin     = 5;
  m_yMargin     = 5;
  m_itemSpacing = 5;
  int w;

  switch (m_viewType) {
  case ListView:
    m_itemSize    = QSize(panelWidth, fontMetrics().height());
    m_itemSpacing = 0;
    break;
  case TableView: {
    m_itemSize    = QSize(panelWidth, fontMetrics().height() + 7);
    m_itemSpacing = 0;
    m_xMargin     = 0;
    m_yMargin     = 0;
    break;
  }
  case ThumbnailView:
    m_itemSize = QSize(m_iconSize.width() + 10, m_iconSize.height() + 30);
    if (!m_singleColumnEnabled) {
      int w        = panelWidth - m_xMargin * 2 + m_itemSpacing;
      int iw       = m_itemSize.width() + m_itemSpacing;
      m_itemPerRow = w / iw;
      if (m_itemPerRow < 1) m_itemPerRow = 1;
    }
    w = (panelWidth + m_itemSpacing -
         m_itemPerRow * (m_itemSize.width() + m_itemSpacing)) /
        2;
    if (w > m_xMargin) m_xMargin = w;
    break;
  }
  if (m_centerAligned) {
    int rowCount = (getItemCount() + m_itemPerRow - 1) / m_itemPerRow;
    int contentHeight =
        rowCount * (m_itemSize.height() + m_itemSpacing) - m_itemSpacing;
    int parentHeight = parentWidget()->height();
    if (contentHeight + 2 * m_yMargin < parentHeight)
      m_yMargin = (parentHeight - contentHeight) / 2;
  }
}

//-----------------------------------------------------------------------------

int DvItemViewerPanel::pos2index(const QPoint &pos) const {
  int xDist = (pos.x() - m_xMargin);
  int col   = (xDist < 0) ? -1 : xDist / (m_itemSize.width() + m_itemSpacing);
  int yDist = (pos.y() - m_yMargin);
  int row   = (yDist < 0) ? -1 : yDist / (m_itemSize.height() + m_itemSpacing);
  return row * m_itemPerRow + col;
}

//-----------------------------------------------------------------------------

QRect DvItemViewerPanel::index2pos(int index) const {
  int row = index / m_itemPerRow;
  int col = index - row * m_itemPerRow;
  QPoint pos(m_xMargin + (m_itemSize.width() + m_itemSpacing) * col,
             m_yMargin + (m_itemSize.height() + m_itemSpacing) * row);
  return QRect(pos, m_itemSize);
}

//-----------------------------------------------------------------------------

QRect DvItemViewerPanel::getCaptionRect(int index) const {
  QRect itemRect = index2pos(index);
  //  TDimension m_iconSize(80,60);// =
  //  IconGenerator::instance()->getIconSize();
  int y = itemRect.top() + m_iconSize.height();
  QRect textRect(itemRect.left(), y, itemRect.width(), itemRect.bottom() - y);

  return textRect;
}

//-----------------------------------------------------------------------------

int DvItemViewerPanel::getContentMinimumWidth() {
  switch (m_viewType) {
  case ListView:
    return 200;
    break;
  case TableView:
    return 600;
    break;
  case ThumbnailView:
    return 120;
    break;
  default:
    return 120;
    break;
  }
}

//-----------------------------------------------------------------------------

int DvItemViewerPanel::getContentHeight(int width) {
  updateViewParameters(width);
  int itemCount = getItemCount();
  QRect rect    = index2pos(itemCount - 1);
  return rect.bottom() + m_yMargin;
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::paintEvent(QPaintEvent *) {
  QPainter p(this);
  int i, n = getItemCount();
  updateViewParameters(width());
  switch (m_viewType) {
  case ListView:
    for (i = 0; i < n; i++) paintListItem(p, i);
    break;
  case TableView:
    for (i = 0; i < n; i++) paintTableItem(p, i);
    break;
  case ThumbnailView:
    for (i = 0; i < n; i++) paintThumbnailItem(p, i);
    break;
  }

  /*

p.setPen(Qt::green);
for(i=0;i<n;i++)
p.drawRect(index2pos(i));

int y = getContentHeight(width());
p.drawLine(0,y,width(),y);

p.setPen(Qt::magenta);
p.drawRect(0,0,width()-1,height()-1);
*/

  m_viewer->draw(p);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setMissingTextColor(const QColor &color) {
  m_missingColor = color;
}

//-----------------------------------------------

void DvItemViewerPanel::paintThumbnailItem(QPainter &p, int index) {
  // Get Version Control Status
  int status = getModel()
                   ->getItemData(index, DvItemListModel::VersionControlStatus)
                   .toInt();

  bool isSelected = m_selection->isSelected(index);
  QRect rect      = index2pos(index);
  if (!visibleRegion().intersects(rect)) return;
  if (!getModel()) return;

  QRect iconRect(rect.left() + (rect.width() - m_iconSize.width()) / 2,
                 rect.top(), m_iconSize.width(), m_iconSize.height());
  QRect textRect(iconRect.left(), iconRect.bottom(), iconRect.width(),
                 rect.bottom() - iconRect.bottom());

  // Draw Selection
  if (isSelected) {
    p.setPen(Qt::NoPen);
    p.fillRect(iconRect.adjusted(-2, -2, 2, 2), getSelectedItemBackground());
    p.fillRect(textRect.adjusted(-2, 3, 2, 0), getSelectedItemBackground());
  }

  // Draw Pixmap
  // if(status != DvItemListModel::VC_Missing)
  //{

  QPixmap thumbnail =
      getModel()
          ->getItemData(index, DvItemListModel::Thumbnail, isSelected)
          .value<QPixmap>();
  if (!thumbnail.isNull()) p.drawPixmap(iconRect.topLeft(), thumbnail);
  //}
  else {
    static QPixmap missingPixmap = QPixmap(":Resources/missing.svg");
    QRect pixmapRect(rect.left() + (rect.width() - missingPixmap.width()) / 2,
                     rect.top(), missingPixmap.width(), missingPixmap.height());
    p.drawPixmap(pixmapRect.topLeft(), missingPixmap);
  }

  // Draw Text
  if (status == DvItemListModel::VC_Missing)
    p.setPen(m_missingColor);
  else
    p.setPen((isSelected) ? getSelectedTextColor() : getTextColor());

  QString name =
      getModel()->getItemData(index, DvItemListModel::Name).toString();
  int frameCount =
      getModel()->getItemData(index, DvItemListModel::FrameCount).toInt();
  if (frameCount > 0) {
    QString num;
    name += QString(" [") + num.number(frameCount) + QString("]");
  }
  QString elideName = elideText(name, p.font(), 2 * textRect.width());
  p.drawText(textRect, Qt::AlignCenter,
             hyphenText(elideName, p.font(), textRect.width()));

  if (isSelected) {
    if (!m_isPlayDelegateDisable &&
        getModel()->getItemData(index, DvItemListModel::PlayAvailable).toBool())
      m_itemViewPlayDelegate->paint(&p, iconRect.adjusted(-2, -2, 1, 1), index);
  }

  // Draw Scene rect
  if (getModel()->isSceneItem(index)) {
    QRect r(iconRect.left(), iconRect.top(), iconRect.width(), 4);
    p.setPen(Qt::black);
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.fillRect(r.adjusted(1, 1, -1, -1), Qt::white);
    p.setPen(isSelected ? Qt::blue : Qt::black);
    int x;
    for (x = r.left() + 5; x + 5 < r.right(); x += 10) {
      int y = r.top() + 1;
      p.drawLine(x, y, x + 4, y);
      y++;
      x++;
      p.drawLine(x, y, x + 4, y);
    }
  }

  if (status != DvItemListModel::VC_None &&
      status != DvItemListModel::VC_Missing) {
    QPoint iconSize = QPoint(18, 18);
    QPoint point    = rect.topLeft() - QPoint(1, 4);
    QRect pixmapRect(point.x(), point.y(), iconSize.x(), iconSize.y());

    QPixmap statusPixmap = getStatusPixmap(status);
    p.drawPixmap(pixmapRect, statusPixmap);
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::paintListItem(QPainter &p, int index) {
  QRect rect = index2pos(index);
  if (!visibleRegion().intersects(rect)) return;
  if (!getModel()) return;
  QPixmap icon =
      getModel()->getItemData(index, DvItemListModel::Icon).value<QPixmap>();
  QString name =
      getModel()->getItemData(index, DvItemListModel::Name).toString();
  bool isSelected = m_selection->isSelected(index);
  if (isSelected) p.fillRect(rect, QColor(171, 206, 255));
  p.drawPixmap(rect.topLeft(), icon);
  rect.adjust(30, 0, 0, 0);
  p.setPen(Qt::black);
  p.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
             elideText(name, p.font(), rect.width()));
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::paintTableItem(QPainter &p, int index) {
  QRect rect = index2pos(index);
  if (!visibleRegion().intersects(rect)) return;
  if (!getModel()) return;
  bool isSelected = m_selection->isSelected(index);
  if (isSelected)
    p.fillRect(rect, getSelectedItemBackground());
  else if (index % 2 == 0)
    p.fillRect(rect, getAlternateBackground());  // 160,160,160

  DvItemListModel::Status status =
      (DvItemListModel::Status)getModel()
          ->getItemData(index, DvItemListModel::VersionControlStatus)
          .toInt();

  if (getModel()->getItemData(index, DvItemListModel::IsFolder).toBool())
    p.setPen(getFolderTextColor());
  else {
    if (status == DvItemListModel::VC_Missing)
      p.setPen(m_missingColor);
    else
      p.setPen((isSelected) ? getSelectedTextColor() : getTextColor());
  }

  int h  = 0;  // fontMetrics().descent();
  int y  = rect.top();
  int ly = rect.height();
  int x  = rect.left();

  int i, n = (int)m_columns.size();

  // Version Control status pixmap
  QPixmap statusPixmap = getStatusPixmap(status);
  if (!statusPixmap.isNull()) {
    p.drawPixmap(x + 1, y + 1, statusPixmap.scaled(15, 15, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
    x += 15;
  }

  for (i = 0; i < n; i++) {
    if (!(m_columns[i].second.second)) continue;
    DvItemListModel::DataType dataType = m_columns[i].first;
    QString value = getModel()->getItemDataAsString(index, dataType);
    int lx        = m_columns[i].second.first;
    p.drawText(QRect(x + 4, y + 1, lx - 4, ly - 1),
               Qt::AlignLeft | Qt::AlignVCenter,
               elideText(value, p.font(), lx));
    x += lx;
  }
  if (n > 1) {
    p.setPen(QColor(0, 0, 0, 100));  // column line
    if ((m_columns[0].second.second))
      x = rect.left() + m_columns[0].second.first;
    else
      x = rect.left();
    for (i = 1; i < n; i++) {
      if (!(m_columns[i].second.second)) continue;
      p.drawLine(x - 1, y, x - 1, y + ly);
      x += m_columns[i].second.first;
    }
    p.drawLine(x - 1, y, x - 1, y + ly);
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mousePressEvent(QMouseEvent *event) {
  updateViewParameters(width());
  int index       = pos2index(event->pos());
  bool isSelected = m_selection->isSelected(index);
  if (event->button() == Qt::RightButton) {
    // when a folder item is right-clicked, do nothing
    if (getModel()->getItemData(index, DvItemListModel::IsFolder).toBool())
      return;

    if (!isSelected) {
      m_selection->selectNone();
      if (!m_isPlayDelegateDisable) m_itemViewPlayDelegate->resetPlayWidget();
      if (0 <= index && index < getItemCount()) m_selection->select(index);
      if (m_globalSelectionEnabled) m_selection->makeCurrent();
      update();
    }
    return;
  } else if (event->button() == Qt::MidButton) {
    m_lastMousePos = event->globalPos();
    event->accept();
    return;
  }
  if (!m_isPlayDelegateDisable) {
    QRect rect = index2pos(index);
    QRect iconRect(rect.left() + (rect.width() - m_iconSize.width()) / 2,
                   rect.top(), m_iconSize.width(), m_iconSize.height());
    DvItemListModel *model = getModel();
    if (model->getItemData(index, DvItemListModel::PlayAvailable).toBool() &&
        isSelected) {
      if (m_itemViewPlayDelegate->setPlayWidget(getModel(), index, iconRect,
                                                event->pos())) {
        update();
        return;
      }
    } else
      m_itemViewPlayDelegate->resetPlayWidget();
  }

  // without any modifier, clear the selection
  if (!m_multiSelectionEnabled ||
      (0 == (event->modifiers() & Qt::ControlModifier) &&
       0 == (event->modifiers() & Qt::ShiftModifier) &&
       !m_selection->isSelected(index)))
    m_selection->selectNone();

  // if click something
  if (0 <= index && index < getItemCount()) {
    if (0 != (event->modifiers() & Qt::ControlModifier)) {
      // ctrl-click
      m_selection->select(index, !m_selection->isSelected(index));
    } else if (0 != (event->modifiers() & Qt::ShiftModifier)) {
      // shift-click
      if (!isSelected) {
        int a = index, b = index;
        while (a > 0 && !m_selection->isSelected(a - 1)) a--;
        if (a == 0) a = index;
        int k         = getItemCount();
        while (b < k && !m_selection->isSelected(b + 1)) b++;
        if (b == k) b = index;
        int i;
        for (i = a; i <= b; i++) {
          // select except folder items
          if (!getModel()->getItemData(i, DvItemListModel::IsFolder).toBool())
            m_selection->select(i);
        }
      }
    } else {
      m_selection->selectNone();
      m_selection->select(index);
    }
  }
  if (m_globalSelectionEnabled) m_selection->makeCurrent();
  m_currentIndex = index;
  if (m_viewer) m_viewer->notifyClick(index);
  m_startDragPosition = event->pos();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() == Qt::MidButton) {
    QPoint d       = event->globalPos() - m_lastMousePos;
    m_lastMousePos = event->globalPos();
    if (m_viewer) {
      QScrollBar *scb = m_viewer->verticalScrollBar();
      scb->setValue(scb->value() - d.y());
    }
    return;
  }
  // continuo solo se il bottone sinistro e' premuto.
  else if (!(event->buttons() & Qt::LeftButton))
    return;

  if (!m_isPlayDelegateDisable) {
    int index       = pos2index(event->pos());
    bool isSelected = m_selection->isSelected(index);
    QRect rect      = index2pos(index);
    QRect iconRect(rect.left() + (rect.width() - m_iconSize.width()) / 2,
                   rect.top(), m_iconSize.width(), m_iconSize.height());
    DvItemListModel *model = getModel();
    if (model->getItemData(index, DvItemListModel::PlayAvailable).toBool() &&
        isSelected) {
      if (m_itemViewPlayDelegate->setPlayWidget(getModel(), index, iconRect,
                                                event->pos())) {
        update();
        return;
      }
    } else
      m_itemViewPlayDelegate->resetPlayWidget();
  }

  // faccio partire il drag&drop solo se mi sono mosso di una certa quantita'
  if ((event->pos() - m_startDragPosition).manhattanLength() < 20) return;
  // e se c'e' una selezione non vuota
  if (m_currentIndex < 0 || m_currentIndex >= getItemCount()) return;

  assert(getModel());
  getModel()->startDragDrop();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mouseReleaseEvent(QMouseEvent *) {}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mouseDoubleClickEvent(QMouseEvent *event) {
  int index = pos2index(event->pos());
  if (index < 0 || index >= getItemCount()) return;
  if (!getModel()->canRenameItem(index)) return;
  QRect captionRect = getCaptionRect(index);
  if (!captionRect.contains(event->pos())) return;
  m_currentIndex = index;

  DVGui::LineEdit *fld = m_editFld;
  // getModel()->refreshData();
  QString name =
      getModel()->getItemData(index, DvItemListModel::Name).toString();
  fld->setText(name);
  fld->setGeometry(captionRect);
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::rename() {
  QString newName = m_editFld->text();
  m_editFld->hide();
  if (getModel() && 0 <= m_currentIndex && m_currentIndex < getItemCount()) {
    getModel()->renameItem(m_currentIndex, newName);
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::contextMenuEvent(QContextMenuEvent *event) {
  if (!getModel()) return;
  if (m_noContextMenu) return;

  int index   = pos2index(event->pos());
  QMenu *menu = getModel()->getContextMenu(this, index);
  if (menu) {
    menu->exec(event->globalPos());
    delete menu;
  }
}

//-----------------------------------------------------------------------------

bool DvItemViewerPanel::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    // getModel()->refreshData();
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    int index             = pos2index(helpEvent->pos());
    if (0 <= index && index < getItemCount()) {
      QRect rect      = index2pos(index);
      QPoint iconSize = QPoint(18, 18);
      QPoint point    = rect.topLeft() - QPoint(1, 4);
      QRect pixmapRect(point.x(), point.y(), iconSize.x(), iconSize.y());
      if (pixmapRect.contains(helpEvent->pos()))
        QToolTip::showText(helpEvent->globalPos(),
                           getModel()->getItemDataAsString(
                               index, DvItemListModel::VersionControlStatus));
      else {
        QVariant data =
            getModel()->getItemData(index, DvItemListModel::ToolTip);
        if (data == QVariant())
          QToolTip::hideText();
        else
          QToolTip::showText(helpEvent->globalPos(), data.toString());
      }
    } else
      QToolTip::hideText();
  }
  return QWidget::event(event);
}
//-----------------------------------------------------------------------------

void DvItemViewerPanel::setListView() {
  m_viewType                                                 = ListView;
  m_viewer->m_windowType == DvItemViewer::Cast ? CastView    = ListView
                                               : BrowserView = ListView;
  emit viewTypeChange(m_viewType);
  m_viewer->updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setTableView() {
  m_viewType                                                 = TableView;
  m_viewer->m_windowType == DvItemViewer::Cast ? CastView    = TableView
                                               : BrowserView = TableView;
  emit viewTypeChange(m_viewType);
  m_viewer->updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setThumbnailsView() {
  m_viewType                                                 = ThumbnailView;
  m_viewer->m_windowType == DvItemViewer::Cast ? CastView    = ThumbnailView
                                               : BrowserView = ThumbnailView;
  emit viewTypeChange(m_viewType);
  m_viewer->updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::exportFileList() {
  TProject *project =
      TProjectManager::instance()->getCurrentProject().getPointer();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp;
  if (scene) fp = scene->decodeFilePath(project->getFolder(TProject::Extras));

  QString initialPath;
  if (fp.isEmpty())
    initialPath = QString();
  else
    initialPath = toQString(fp);

  QString fileName = QFileDialog::getSaveFileName(
      0, tr("Save File List"), initialPath, tr("File List (*.csv)"));

  if (fileName.isEmpty()) return;

  QFile data(fileName);
  if (data.open(QFile::WriteOnly)) {
    QTextStream out(&data);

    for (int index = 0; index < getItemCount(); index++) {
      if (getModel()->getItemData(index, DvItemListModel::IsFolder).toBool())
        continue;

      for (int i = 0; i < (int)m_columns.size(); i++) {
        DvItemListModel::DataType dataType = m_columns[i].first;

        if (dataType != DvItemListModel::Name &&
            dataType != DvItemListModel::FrameCount)
          continue;

        QString value = getModel()->getItemDataAsString(index, dataType);

        out << value;
        if (dataType == DvItemListModel::Name) out << ",";
      }
      out << ('\n');
    }
  }
  data.close();
}

//=============================================================================
//
// DvItemViewer
//
//-----------------------------------------------------------------------------

DvItemViewer::DvItemViewer(QWidget *parent, bool noContextMenu,
                           bool multiSelectionEnabled,
                           DvItemViewer::WindowType windowType)
    : QScrollArea(parent), m_model(0) {
  m_windowType = windowType;
  m_panel =
      new DvItemViewerPanel(this, noContextMenu, multiSelectionEnabled, 0);
  setObjectName("BrowserTreeView");
  setStyleSheet("#BrowserTreeView {qproperty-autoFillBackground: true;}");

  setWidget(m_panel);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setAcceptDrops(true);
}

//-----------------------------------------------------------------------------

void DvItemViewer::setModel(DvItemListModel *model) {
  if (model == m_model) return;
  delete m_model;
  m_model = model;
  m_panel->getSelection()->setModel(model);
}

//-----------------------------------------------------------------------------

void DvItemViewer::updateContentSize() {
  int w              = m_panel->getContentMinimumWidth();
  if (w < width()) w = width();
  int h              = m_panel->getContentHeight(w) +
          20;  // 20 is margin for showing the empty area
  if (h < height()) h = height();
  m_panel->resize(w, h);
}

//-----------------------------------------------------------------------------

void DvItemViewer::resizeEvent(QResizeEvent *event) {
  updateContentSize();
  QScrollArea::resizeEvent(event);
}
//-----------------------------------------------------------------------------

void DvItemViewer::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Home)
    QScrollArea::verticalScrollBar()->setValue(0);
  if (event->key() == Qt::Key_End)
    QScrollArea::verticalScrollBar()->setValue(
        QScrollArea::verticalScrollBar()->maximum());

  QScrollArea::keyPressEvent(event);
}
//-----------------------------------------------------------------------------

void DvItemViewer::resetVerticalScrollBar() {
  QScrollArea::verticalScrollBar()->setValue(0);
}
//-----------------------------------------------------------------------------

void DvItemViewer::dragEnterEvent(QDragEnterEvent *event) {
  if (m_model && m_model->acceptDrop(event->mimeData()))
    event->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void DvItemViewer::dropEvent(QDropEvent *event) {
  if (m_model && m_model->drop(event->mimeData()))
    event->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void DvItemViewer::refresh() {
  updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewer::selectNone() {
  if (m_panel->getSelection()) m_panel->getSelection()->selectNone();
}

//=============================================================================
//
// DvItemViewTitleBar
//
//-----------------------------------------------------------------------------

DvItemViewerTitleBar::DvItemViewerTitleBar(DvItemViewer *itemViewer,
                                           QWidget *parent, bool isInteractive)
    : QWidget(parent)
    , m_itemViewer(itemViewer)
    , m_isInteractive(isInteractive)
    , m_pos(QPoint(0, 0))
    , m_dragColumnIndex(-1) {
  setMinimumHeight(22);

  bool ret = connect(m_itemViewer->getPanel(),
                     SIGNAL(viewTypeChange(DvItemViewerPanel::ViewType)), this,
                     SLOT(onViewTypeChanged(DvItemViewerPanel::ViewType)));

  assert(ret);

  setMouseTracking(true);
  if ((itemViewer->m_windowType == DvItemViewer::Browser &&
       BrowserView == DvItemViewerPanel::TableView) ||
      (itemViewer->m_windowType == DvItemViewer::Cast &&
       CastView == DvItemViewerPanel::TableView))
    show();
  else
    hide();
}

//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::onViewTypeChanged(
    DvItemViewerPanel::ViewType viewType) {
  if ((m_itemViewer->m_windowType == DvItemViewer::Browser &&
       BrowserView == DvItemViewerPanel::TableView) ||
      (m_itemViewer->m_windowType == DvItemViewer::Cast &&
       CastView == DvItemViewerPanel::TableView))
    show();
  else
    hide();
}

//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::mouseMoveEvent(QMouseEvent *event) {
  QPoint pos = event->pos();
  std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
      columns;
  m_itemViewer->getPanel()->getColumns(columns);
  DvItemListModel *model = m_itemViewer->getModel();

  if (event->buttons() == Qt::NoButton) {
    int i, n = (int)columns.size();
    int x  = 0;
    int ly = height();
    for (i = 0; i < n; i++) {
      if (!(columns[i].second.second)) continue;
      int lx = columns[i].second.first;
      x += lx;
      if (abs(x - pos.x()) > 1) continue;
      m_dragColumnIndex = i;
      setCursor(Qt::SplitHCursor);
      return;
    }
    m_dragColumnIndex = -1;
    setCursor(Qt::ArrowCursor);
  } else if (event->buttons() == Qt::LeftButton && m_dragColumnIndex >= 0) {
    int delta       = pos.x() - m_pos.x();
    int columnWidth = columns[m_dragColumnIndex].second.first;
    if (columnWidth + delta < 20) return;
    m_itemViewer->getPanel()->setColumnWidth(columns[m_dragColumnIndex].first,
                                             columnWidth + delta);
    update();
    m_pos = pos;
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::mousePressEvent(QMouseEvent *event) {
  QPoint pos = event->pos();
  if (event->button() == Qt::LeftButton) {
    if (m_dragColumnIndex >= 0) {
      m_pos = pos;
      return;
    } else
      m_pos = QPoint(0, 0);
    if (!m_isInteractive) return;
    std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
        columns;
    m_itemViewer->getPanel()->getColumns(columns);
    DvItemListModel *model = m_itemViewer->getModel();
    int i, n = (int)columns.size();
    int x  = 0;
    int ly = height();
    for (i = 0; i < n; i++) {
      if (!(columns[i].second.second)) continue;
      int lx = columns[i].second.first;
      QRect columnRect(x, 0, lx, ly - 1);
      x += lx;
      if (!columnRect.contains(pos)) continue;
      DvItemListModel::DataType dataType = columns[i].first;
      model->sortByDataModel(dataType, !model->isDiscendentOrder());
      update();
    }
    return;
  }
  if (event->button() == Qt::RightButton) {
    openContextMenu(event);
  }
}
//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::openContextMenu(QMouseEvent *event) {
  // QAction setNameAction          (QObject::tr("Name"),0);
  // setNameAction.setCheckable(true);
  // setNameAction.setChecked(m_itemViewer->getPanel()->getVisibility(DvItemListModel::Name));
  QAction setSizeAction(QObject::tr("Size"), 0);
  setSizeAction.setCheckable(true);
  setSizeAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileSize));
  QAction setFramesAction(QObject::tr("Frames"), 0);
  setFramesAction.setCheckable(true);
  setFramesAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::FrameCount));
  QAction setDateCreatedAction(QObject::tr("Date Created"), 0);
  setDateCreatedAction.setCheckable(true);
  setDateCreatedAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::CreationDate));
  QAction setDateModifiedAction(QObject::tr("Date Modified"), 0);
  setDateModifiedAction.setCheckable(true);
  setDateModifiedAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::ModifiedDate));
  QAction setTypeAction(QObject::tr("Type"), 0);
  setTypeAction.setCheckable(true);
  setTypeAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileType));
  QAction setVersionControlAction(QObject::tr("Version Control"), 0);
  setVersionControlAction.setCheckable(true);
  setVersionControlAction.setChecked(m_itemViewer->getPanel()->getVisibility(
      DvItemListModel::VersionControlStatus));
  QMenu menu(0);
  // menu.addAction(&setNameAction);
  menu.addAction(&setFramesAction);
  if (m_itemViewer->m_windowType == DvItemViewer::Browser) {
    menu.addAction(&setSizeAction);
    menu.addAction(&setDateCreatedAction);
    menu.addAction(&setDateModifiedAction);
    menu.addAction(&setTypeAction);
    menu.addAction(&setVersionControlAction);
  }

  QAction *action = menu.exec(event->globalPos());  // QCursor::pos());
  // if(action==&setNameAction)
  // m_itemViewer->getPanel()->setVisibility(DvItemListModel::Name,!m_itemViewer->getPanel()->getVisibility(DvItemListModel::Name));
  if (action == &setSizeAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::FileSize,
        !m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileSize));
  if (action == &setFramesAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::FrameCount,
        !m_itemViewer->getPanel()->getVisibility(DvItemListModel::FrameCount));
  if (action == &setDateCreatedAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::CreationDate, !m_itemViewer->getPanel()->getVisibility(
                                           DvItemListModel::CreationDate));
  if (action == &setDateModifiedAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::ModifiedDate, !m_itemViewer->getPanel()->getVisibility(
                                           DvItemListModel::ModifiedDate));
  if (action == &setTypeAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::FileType,
        !m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileType));
  if (action == &setVersionControlAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::VersionControlStatus,
        !m_itemViewer->getPanel()->getVisibility(
            DvItemListModel::VersionControlStatus));

  m_itemViewer->getPanel()->update();
}
//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::paintEvent(QPaintEvent *) {
  QPainter p(this);
  std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
      columns;
  m_itemViewer->getPanel()->getColumns(columns);
  QRect rect(0, 0, width(), height());

  QBrush nb = QBrush(Qt::NoBrush);
  QPalette pal =
      QPalette(nb, nb, QBrush(QColor(255, 255, 255, 30)), QBrush(QColor(0, 0, 0, 110)),
               QBrush(QColor(Qt::gray)), nb, nb, nb, nb);

  p.fillRect(rect, QColor(0, 0, 0, 90));  // bg color

  p.setPen(QColor(200, 200, 200, 255));  // text color
  int h  = 0;  // fontMetrics().descent();
  int y  = rect.top();
  int ly = rect.height();
  int lx = rect.width();
  int x  = rect.left();

  DvItemListModel *model = m_itemViewer->getModel();
  int i, n = (int)columns.size();
  for (i = 0; i < n; i++) {
    if (!(columns[i].second.second)) continue;
    DvItemListModel::DataType dataType = columns[i].first;
    int columnLx                       = columns[i].second.first;

    // paint background
    QColor bgColor;
    if (dataType == model->getCurrentOrderType())
      bgColor = QColor(255, 255, 255, 30);
    else
      bgColor = QColor(0, 0, 0, 0);

    QRect typeRect(x, y, columnLx, ly);
    QBrush brush(bgColor);
    qDrawShadePanel(&p, typeRect, pal, false, 1, &brush);

    // draw ordering arrow
    if (m_isInteractive && dataType == model->getCurrentOrderType()) {
      QIcon arrowIcon;
      if (model->isDiscendentOrder())
        arrowIcon = createQIconPNG("arrow_up");
      else
        arrowIcon = createQIconPNG("arrow_down");
      p.drawPixmap(QRect(x + columnLx - 10, y + 6, 8, 8),
                   arrowIcon.pixmap(8, 8));
    }

    // draw text
    QString value = model->getItemDataIdentifierName(dataType);
    p.drawText(QRect(x + 4, y + 1, columnLx - 4, ly - 1),
               Qt::AlignLeft | Qt::AlignVCenter,
               elideText(value, p.font(), columnLx - 4));

    x += columnLx;
  }
}

//=============================================================================
//
// DvItemViewButtonBar
//
//-----------------------------------------------------------------------------

DvItemViewerButtonBar::DvItemViewerButtonBar(DvItemViewer *itemViewer,
                                             QWidget *parent)
    : QToolBar(parent) {
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setIconSize(QSize(17, 17));
  setObjectName("buttonBar");
  // buttonBar->setIconSize(QSize(10,10));

  QString backButtonEnable  = QString(":Resources/fb_history_back_enable.svg");
  QString backButtonDisable = QString(":Resources/fb_history_back_disable.svg");
  QString fwdButtonEnable   = QString(":Resources/fb_history_fwd_enable.svg");
  QString fwdButtonDisable  = QString(":Resources/fb_history_fwd_disable.svg");

  QIcon backButtonIcon, fwdButtonIcon;
  backButtonIcon.addFile(backButtonEnable, QSize(), QIcon::Normal);
  backButtonIcon.addFile(backButtonDisable, QSize(), QIcon::Disabled);
  fwdButtonIcon.addFile(fwdButtonEnable, QSize(), QIcon::Normal);
  fwdButtonIcon.addFile(fwdButtonDisable, QSize(), QIcon::Disabled);

  m_folderBack = new QAction(backButtonIcon, tr("Back"), this);
  m_folderBack->setIconText("");
  addAction(m_folderBack);
  m_folderFwd = new QAction(fwdButtonIcon, tr("Forward"), this);
  m_folderFwd->setIconText("");
  addAction(m_folderFwd);

  QIcon folderUpIcon = createQIcon("folderup");
  QAction *folderUp  = new QAction(folderUpIcon, tr("Up One Level"), this);
  folderUp->setIconText(tr("Up"));
  addAction(folderUp);
  addSeparator();

  QIcon newFolderIcon = createQIcon("newfolder");
  QAction *newFolder  = new QAction(newFolderIcon, tr("New Folder"), this);
  newFolder->setIconText(tr("New"));
  addAction(newFolder);
  addSeparator();

  // view mode
  QActionGroup *actions = new QActionGroup(this);
  actions->setExclusive(true);

  QIcon thumbViewIcon = createQIconOnOff("viewicon");
  QAction *thumbView  = new QAction(thumbViewIcon, tr("Icons View"), this);
  thumbView->setCheckable(true);
  thumbView->setIconText(tr("Icon"));
  thumbView->setChecked((itemViewer->m_windowType == DvItemViewer::Browser &&
                         DvItemViewerPanel::ThumbnailView == BrowserView) ||
                        (itemViewer->m_windowType == DvItemViewer::Cast &&
                         DvItemViewerPanel::ThumbnailView == CastView));
  actions->addAction(thumbView);
  addAction(thumbView);

  QIcon listViewIcon = createQIconOnOff("viewlist");
  QAction *listView  = new QAction(listViewIcon, tr("List View"), this);
  listView->setCheckable(true);
  listView->setIconText(tr("List"));
  listView->setChecked((itemViewer->m_windowType == DvItemViewer::Browser &&
                        DvItemViewerPanel::TableView == BrowserView) ||
                       (itemViewer->m_windowType == DvItemViewer::Cast &&
                        DvItemViewerPanel::TableView == CastView));
  actions->addAction(listView);
  addAction(listView);

  //	QIcon tableViewIcon = createQIconOnOffPNG("viewtable");
  //  QAction* tableView = new QAction(tableViewIcon, tr("Table View"), this);
  //  tableView->setCheckable(true);
  //  actions->addAction(tableView);
  //  addAction(tableView);
  addSeparator();

  // button to export file list to csv
  QAction *exportFileListAction = new QAction(tr("Export File List"), this);
  addAction(exportFileListAction);
  addSeparator();

  if (itemViewer->m_windowType == DvItemViewer::Browser &&
      !Preferences::instance()->isWatchFileSystemEnabled()) {
    addAction(CommandManager::instance()->getAction("MI_RefreshTree"));
    addSeparator();
  }

  connect(exportFileListAction, SIGNAL(triggered()), itemViewer->getPanel(),
          SLOT(exportFileList()));

  connect(folderUp, SIGNAL(triggered()), SIGNAL(folderUp()));
  connect(newFolder, SIGNAL(triggered()), SIGNAL(newFolder()));
  connect(thumbView, SIGNAL(triggered()), itemViewer->getPanel(),
          SLOT(setThumbnailsView()));
  connect(listView, SIGNAL(triggered()), itemViewer->getPanel(),
          SLOT(setTableView()));
  //	connect(listView      , SIGNAL(triggered()), itemViewer->getPanel(),
  // SLOT(setListView()));
  //	connect(tableView     , SIGNAL(triggered()), itemViewer->getPanel(),
  // SLOT(setTableView()));

  connect(m_folderBack, SIGNAL(triggered()), SIGNAL(folderBack()));
  connect(m_folderFwd, SIGNAL(triggered()), SIGNAL(folderFwd()));

  if (itemViewer->m_windowType == DvItemViewer::Browser) {
    connect(TApp::instance()->getCurrentScene(),
            SIGNAL(preferenceChanged(const QString &)), this,
            SLOT(onPreferenceChanged(const QString &)));
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onHistoryChanged(bool backEnable, bool fwdEnable) {
  if (backEnable)
    m_folderBack->setEnabled(true);
  else
    m_folderBack->setEnabled(false);

  if (fwdEnable)
    m_folderFwd->setEnabled(true);
  else
    m_folderFwd->setEnabled(false);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onPreferenceChanged(const QString &prefName) {
  // react only when the related preference is changed
  if (prefName != "WatchFileSystem") return;

  QAction *refreshAct = CommandManager::instance()->getAction("MI_RefreshTree");
  if (Preferences::instance()->isWatchFileSystemEnabled()) {
    removeAction(refreshAct);
    removeAction(actions().last());  // remove separator
  } else {
    addAction(refreshAct);
    addSeparator();
  }
}