

#include "custompaneleditorpopup.h"

// Tnz includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "shortcutpopup.h"
#include "custompanelmanager.h"
#include "commandbarpopup.h"
#include "toolpresetcommandmanager.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"

// ToonzLib
#include "toonz/toonzfolders.h"

// ToonzCore
#include "tsystem.h"
#include "tfilepath.h"

// Qt includes
#include <QMainWindow>
#include <QHeaderView>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QUiLoader>
#include <QColor>
#include <QDomDocument>
#include <QTextStream>
#include <QBuffer>
#include <QToolButton>
#include <QRegularExpression>
#include <QInputDialog>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <algorithm>

namespace {
const TFilePath CustomPanelTemplateFolderName("custom panel templates");

TFilePath customPaneTemplateFolderPath() {
  return ToonzFolder::getLibraryFolder() + CustomPanelTemplateFolderName;
}

const TFilePath CustomPanelFolderName("custompanels");

TFilePath customPaneFolderPath() {
  return ToonzFolder::getMyModuleDir() + CustomPanelFolderName;
}

// Mirror up to 3 nested subfolders under the template / custom panel roots.
const int kMaxTemplateFolderDepth = 3;

// Accent already defined by each theme (no theme-file edits): same fill as
// Tool Properties toggles / checked tool buttons / tree selection.
QColor themeHighlightAccent() {
  const QString styleSheet = qApp->styleSheet();
  auto colorFromRule = [&styleSheet](const QString& selector) -> QColor {
    const QRegularExpression re(
        QStringLiteral(R"(%1\s*\{[^}]*?background-color:\s*([^;]+);)")
            .arg(selector),
        QRegularExpression::DotMatchesEverythingOption |
            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(styleSheet);
    if (!m.hasMatch()) return QColor();
    return QColor(m.captured(1).trimmed());
  };

  // Prefer QToolButton:checked — exact square used by Tool Properties headers
  QColor accent = colorFromRule(QStringLiteral(R"(QToolButton:checked)"));
  if (accent.isValid()) return accent;

  accent = colorFromRule(QStringLiteral(R"(QTreeView::item:selected)"));
  if (accent.isValid()) return accent;

  return qApp->palette().color(QPalette::Highlight);
}

// Tool Properties style: accent square + white triangle (not a tinted triangle).
QByteArray openBranchBadgeSvg(const QColor& accent) {
  return QStringLiteral(
             "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' "
             "viewBox='0 0 16 16'>"
             "<rect width='16' height='16' fill='%1'/>"
             "<path fill='#ffffff' d='M 4,5 L 12,5 L 8,11 Z'/>"
             "</svg>")
      .arg(accent.name(QColor::HexRgb))
      .toUtf8();
}

bool writeTempSvg(const QString& fileName, const QByteArray& svg) {
  const QString path = QDir::temp().filePath(fileName);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  file.write(svg);
  return true;
}

QString cssUrlForTempFile(const QString& fileName) {
  QString path = QDir::temp().filePath(fileName);
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  return path;
}

// Classic yellow folder icon (colored asset).
QIcon templateFolderIcon() { return createQIcon(QStringLiteral("folder")); }

void applyFolderIconRecursive(QTreeWidgetItem* item, const QIcon& folderIcon) {
  if (!item) return;
  if (item->data(0, Qt::UserRole).toString().isEmpty())
    item->setIcon(0, folderIcon);
  for (int i = 0; i < item->childCount(); ++i)
    applyFolderIconRecursive(item->child(i), folderIcon);
}

// Expanded folders: accent square around a white triangle (like Tool Properties).
// Collapsed folders keep the theme's default white branch indicators.
void applyTemplateTreeBranchAccent(QTreeWidget* tree) {
  if (!tree) return;

  const QColor accent = themeHighlightAccent();
  if (!accent.isValid()) return;

  const QString hex = accent.name(QColor::HexRgb).mid(1);
  const QString openName =
      QStringLiteral("opentoonz_cp_treebranch_badge_%1.svg").arg(hex);
  if (!writeTempSvg(openName, openBranchBadgeSvg(accent))) return;

  const QString openUrl = cssUrlForTempFile(openName);
  tree->setStyleSheet(QStringLiteral(
      "QTreeView::branch:has-children:open,"
      "QTreeView::branch:has-children:has-siblings:open {"
      "  background: url(\"%1\") no-repeat center center;"
      "  border-image: none;"
      "  image: none;"
      "}")
                          .arg(openUrl));
}

// Same as ToolPropertiesPanel collapsibles: checked → QSS @hl-color square,
// arrow painted white via QToolButton:checked { color: white; }.
void applyTemplateToggleAccent(QToolButton* toggle, bool browserOpen) {
  if (!toggle) return;
  toggle->setCheckable(true);
  toggle->setIcon(QIcon());
  toggle->setChecked(browserOpen);
  toggle->setArrowType(browserOpen ? Qt::DownArrow : Qt::RightArrow);
}

QPoint relativePos(QWidget* child, QWidget* refParent) {
  if (child->parentWidget() == refParent) {
    return child->pos();
  } else {
    return child->pos() + relativePos(child->parentWidget(), refParent);
  }
}

QString relativePanelId(const TFilePath& file, const TFilePath& rootFolder) {
  TFilePath relative = file - rootFolder;
  QString id         = relative.withType("").getQString();
  id.replace('\\', '/');
  return id;
}

void collectUiPanelIds(const TFilePath& folder, const TFilePath& rootFolder,
                       int depth, QStringList& outIds) {
  if (!TSystem::doesExistFileOrLevel(folder)) return;

  TFilePathSet entries =
      TSystem::readDirectory(folder, false, false, false);

  QList<TFilePath> dirs;
  QList<TFilePath> files;
  for (const auto& entry : entries) {
    if (TFileStatus(entry).isDirectory())
      dirs.append(entry);
    else if (entry.getType() == "ui")
      files.append(entry);
  }

  std::sort(dirs.begin(), dirs.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(a.getQString(), b.getQString(),
                                     Qt::CaseInsensitive) < 0;
            });
  std::sort(files.begin(), files.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });

  if (depth < kMaxTemplateFolderDepth) {
    for (const auto& dir : dirs)
      collectUiPanelIds(dir, rootFolder, depth + 1, outIds);
  }

  for (const auto& file : files)
    outIds.append(relativePanelId(file, rootFolder));
}

}  // anonymous namespace

//=============================================================================
// CustomPanelUIField
//-----------------------------------------------------------------------------

CustomPanelUIField::CustomPanelUIField(int objId, const QString& objectName,
                                       QWidget* parent, bool isFirst)
    : QLabel(tr("Drag and set command"), parent), m_id(objId) {
  QFont fnt = font();
  fnt.setPointSize(12);
  setFont(fnt);
  setStyleSheet("background-color: rgb(255, 255, 128); color: black;");
  setAcceptDrops(true);
  // Original sizing = QLabel sizeHint; Fixed policy only prevents layout stretch
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  // objName may be a commandId
  if (setCommand(objectName)) {
    return;
  }

  if (objectName.startsWith("HScroller") ||
      objectName.startsWith("VScroller")) {
    QStringList ids = objectName.split("__");
    if (ids.size() == 3) {
      setCommand(isFirst ? ids[1] : ids[2]);
    }
  }
}

bool CustomPanelUIField::setCommand(const QString& commandId) {
  if (m_commandId == commandId) {
    return false;
  }

  if (commandId.isEmpty()) {
    m_commandId = commandId;
    setText(tr("Drag and set command"));
    setStyleSheet("background-color: rgb(255, 255, 128); color: black;");
    return true;
  }

  QAction* action =
      CommandManager::instance()->getAction(commandId.toStdString().c_str());
  if (!action) {
    return false;
  }

  m_commandId      = commandId;
  QString tempText = action->text();

  // Remove accelerator key indicator
  QRegularExpression re("&([^& ])");
  tempText = tempText.replace(re, "\\1");

  // Remove doubled &s
  tempText = tempText.replace("&&", "&");

  setText(tempText);
  setStyleSheet("background-color: rgb(230, 230, 230); color: black;");
  return true;
}

void CustomPanelUIField::enterEvent(QEvent* event) {
  Q_UNUSED(event);
  emit highlight(m_id);
}

void CustomPanelUIField::leaveEvent(QEvent* event) {
  Q_UNUSED(event);
  emit highlight(-1);
}

void CustomPanelUIField::dragEnterEvent(QDragEnterEvent* event) {
  QString txt = event->mimeData()->text();
  if (CommandManager::instance()->getAction(txt.toStdString().c_str())) {
    event->setDropAction(Qt::CopyAction);
    event->accept();
  }
  emit highlight(m_id);
}

void CustomPanelUIField::dragLeaveEvent(QDragLeaveEvent* event) {
  Q_UNUSED(event);
  emit highlight(-1);
}

void CustomPanelUIField::dropEvent(QDropEvent* event) {
  QString oldCommandId = m_commandId;
  QString commandId    = event->mimeData()->text();

  if (setCommand(commandId)) {
    // If dragged from the command tree, command can be duplicated
    if (event->dropAction() == Qt::CopyAction) {
      emit commandChanged(QString(), QString());
    } else {
      emit commandChanged(oldCommandId, m_commandId);
    }
  }
}

void CustomPanelUIField::mousePressEvent(QMouseEvent* event) {
  if (m_commandId.isEmpty()) {
    return;
  }

  QMimeData* mimeData = new QMimeData;
  mimeData->setText(m_commandId);

  QString dragPixmapTxt = text();
  QFontMetrics fm(QApplication::font());
  QPixmap pix(fm.boundingRect(dragPixmapTxt).adjusted(-2, -2, 2, 2).size());

  {
    QPainter painter(&pix);
    painter.fillRect(pix.rect(), Qt::white);
    painter.setPen(Qt::black);
    painter.drawText(pix.rect(), Qt::AlignCenter, dragPixmapTxt);
  }

  QDrag* drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->setPixmap(pix);

  drag->exec(Qt::MoveAction);
}

//=============================================================================
// UiPreviewWidget
//-----------------------------------------------------------------------------

UiPreviewWidget::UiPreviewWidget(const QPixmap& uiPixmap,
                                 const QList<UiEntry>& uiEntries,
                                 QWidget* parent)
    : QWidget(parent), m_highlightUiId(-1), m_uiPixmap(uiPixmap) {
  for (const auto& entry : uiEntries) {
    m_rectTable.append(entry.rect);
  }
  setFixedSize(m_uiPixmap.size());
  setAcceptDrops(true);
  setMouseTracking(true);
}

void UiPreviewWidget::onViewerResize(const QSize& size) {
  if (m_uiPixmap.isNull()) {
    return;
  }

  setFixedSize(std::max(size.width(), m_uiPixmap.width()),
               std::max(size.height(), m_uiPixmap.height()));
}

void UiPreviewWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter p(this);
  p.translate((width() - m_uiPixmap.width()) / 2,
              (height() - m_uiPixmap.height()) / 2);
  p.drawPixmap(0, 0, m_uiPixmap);

  for (int id = 0; id < m_rectTable.count(); id++) {
    QRect uiRect = m_rectTable.at(id);
    if (id == m_highlightUiId) {
      p.setBrush(QColor(0, 255, 255, 64));
    } else {
      p.setBrush(Qt::NoBrush);
    }
    p.setPen(Qt::cyan);
    p.drawRect(uiRect);
  }
}

void UiPreviewWidget::highlightUi(int objId) {
  m_highlightUiId = objId;
  update();
}

void UiPreviewWidget::mousePressEvent(QMouseEvent* event) {
  if (m_highlightUiId >= 0) {
    emit clicked(m_highlightUiId);
  }
}

void UiPreviewWidget::onMove(const QPoint& pos) {
  QPoint offset((width() - m_uiPixmap.width()) / 2,
                (height() - m_uiPixmap.height()) / 2);

  for (int id = 0; id < m_rectTable.size(); id++) {
    if (m_rectTable.at(id).contains(pos - offset)) {
      highlightUi(id);
      return;
    }
  }
  highlightUi(-1);
}

void UiPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
  onMove(event->pos());
}

void UiPreviewWidget::dragEnterEvent(QDragEnterEvent* event) {
  QString txt = event->mimeData()->text();
  if (CommandManager::instance()->getAction(txt.toStdString().c_str())) {
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }
}

void UiPreviewWidget::dragMoveEvent(QDragMoveEvent* event) {
  onMove(event->pos());

  if (m_highlightUiId < 0) {
    event->ignore();
    return;
  }

  QString txt = event->mimeData()->text();
  if (CommandManager::instance()->getAction(txt.toStdString().c_str())) {
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }
}

void UiPreviewWidget::dropEvent(QDropEvent* event) {
  QString commandId      = event->mimeData()->text();
  bool isDraggedFromTree = (event->dropAction() == Qt::CopyAction);

  emit dropped(m_highlightUiId, commandId, isDraggedFromTree);
}

//-----------------------------------------------------------------------------

UiPreviewArea::UiPreviewArea(QWidget* parent) : QScrollArea(parent) {}

void UiPreviewArea::resizeEvent(QResizeEvent* event) {
  if (widget()) {
    UiPreviewWidget* previewWidget = qobject_cast<UiPreviewWidget*>(widget());
    if (previewWidget) {
      previewWidget->onViewerResize(event->size());
    }
  }

  QScrollArea::resizeEvent(event);
}

//=============================================================================
// CustomPanelEditorPopup
//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::populateTemplateFolder(QTreeWidgetItem* parent,
                                                    const TFilePath& folder,
                                                    int depth,
                                                    bool isEditSection,
                                                    const QIcon& folderIcon) {
  if (!TSystem::doesExistFileOrLevel(folder)) return;

  TFilePathSet entries =
      TSystem::readDirectory(folder, false, false, false);

  QList<TFilePath> dirs;
  QList<TFilePath> files;
  for (const auto& entry : entries) {
    if (TFileStatus(entry).isDirectory())
      dirs.append(entry);
    else if (entry.getType() == "ui")
      files.append(entry);
  }

  std::sort(dirs.begin(), dirs.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });
  std::sort(files.begin(), files.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });

  if (depth < kMaxTemplateFolderDepth) {
    for (const auto& dir : dirs) {
      QTreeWidgetItem* folderItem = new QTreeWidgetItem(
          QStringList(QString::fromStdString(dir.getName())));
      folderItem->setIcon(0, folderIcon);
      folderItem->setFlags(Qt::ItemIsEnabled);
      folderItem->setData(0, Qt::UserRole, QString());
      populateTemplateFolder(folderItem, dir, depth + 1, isEditSection,
                             folderIcon);
      if (folderItem->childCount() == 0) {
        delete folderItem;
        continue;
      }
      if (parent)
        parent->addChild(folderItem);
      else
        m_templateTree->addTopLevelItem(folderItem);
    }
  }

  for (const auto& file : files) {
    QString name = QString::fromStdString(file.getName());
    if (isEditSection) name = tr("%1 (Edit)").arg(name);
    QTreeWidgetItem* fileItem = new QTreeWidgetItem(QStringList(name));
    fileItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    fileItem->setData(0, Qt::UserRole, file.getQString());
    if (parent)
      parent->addChild(fileItem);
    else
      m_templateTree->addTopLevelItem(fileItem);
  }
}

//-----------------------------------------------------------------------------

QTreeWidgetItem* CustomPanelEditorPopup::findFirstTemplateItem(
    QTreeWidgetItem* parent) const {
  if (!parent) {
    for (int i = 0; i < m_templateTree->topLevelItemCount(); ++i) {
      QTreeWidgetItem* found =
          findFirstTemplateItem(m_templateTree->topLevelItem(i));
      if (found) return found;
    }
    return nullptr;
  }

  if (!parent->data(0, Qt::UserRole).toString().isEmpty()) return parent;

  for (int i = 0; i < parent->childCount(); ++i) {
    QTreeWidgetItem* found = findFirstTemplateItem(parent->child(i));
    if (found) return found;
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

QString CustomPanelEditorPopup::currentTemplatePath() const {
  return m_currentTemplatePath;
}

//-----------------------------------------------------------------------------

QString CustomPanelEditorPopup::templateDisplayNameForItem(
    QTreeWidgetItem* item) const {
  if (!item) return QString();
  QStringList parts;
  for (QTreeWidgetItem* p = item; p; p = p->parent())
    parts.prepend(p->text(0));
  return parts.join(QStringLiteral(" / "));
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::updateTemplateValueLabel() {
  if (!m_templateValueLabel) return;
  QTreeWidgetItem* item = findTemplateItemByPath(m_currentTemplatePath);
  if (item)
    m_templateValueLabel->setText(templateDisplayNameForItem(item));
  else if (!m_currentTemplatePath.isEmpty())
    m_templateValueLabel->setText(
        QFileInfo(m_currentTemplatePath).completeBaseName());
  else
    m_templateValueLabel->setText(tr("(none)"));
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::setTemplateBrowserVisible(bool visible) {
  if (!m_templateBrowser) return;

  m_templateBrowser->setVisible(visible);
  applyTemplateToggleAccent(m_templateToggle, visible);

  // Do not clear search / rewrite expand state — user controls the tree
  if (visible && m_templateSearchEdit) m_templateSearchEdit->setFocus();
}

//-----------------------------------------------------------------------------

QString CustomPanelEditorPopup::templateTreeItemKey(QTreeWidgetItem* item) {
  if (!item) return QString();
  QStringList parts;
  for (QTreeWidgetItem* p = item; p; p = p->parent())
    parts.prepend(p->text(0));
  return parts.join(QLatin1Char('/'));
}

void CustomPanelEditorPopup::saveTemplateTreeExpandState() {
  m_templateExpandStateBeforeSearch.clear();
  if (!m_templateTree) return;
  const QList<QTreeWidgetItem*> allItems = m_templateTree->findItems(
      QString(), Qt::MatchContains | Qt::MatchRecursive);
  for (QTreeWidgetItem* item : allItems) {
    if (item->data(0, Qt::UserRole).toString().isEmpty() && item->isExpanded())
      m_templateExpandStateBeforeSearch.insert(templateTreeItemKey(item));
  }
}

void CustomPanelEditorPopup::restoreTemplateTreeExpandState() {
  if (!m_templateTree) return;
  const QList<QTreeWidgetItem*> allItems = m_templateTree->findItems(
      QString(), Qt::MatchContains | Qt::MatchRecursive);
  for (QTreeWidgetItem* item : allItems) {
    if (item->data(0, Qt::UserRole).toString().isEmpty())
      item->setExpanded(
          m_templateExpandStateBeforeSearch.contains(templateTreeItemKey(item)));
  }
}

void CustomPanelEditorPopup::expandTemplateTreeAncestors(QTreeWidgetItem* item) {
  for (QTreeWidgetItem* p = item ? item->parent() : nullptr; p; p = p->parent())
    p->setExpanded(true);
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::filterTemplateTree(const QString& searchWord) {
  if (!m_templateTree) return;

  QList<QTreeWidgetItem*> allItems = m_templateTree->findItems(
      QString(), Qt::MatchContains | Qt::MatchRecursive);

  if (searchWord.isEmpty()) {
    for (QTreeWidgetItem* item : allItems) item->setHidden(false);
    if (m_templateSearchActive) {
      restoreTemplateTreeExpandState();
      m_templateSearchActive = false;
    }
    return;
  }

  if (!m_templateSearchActive) {
    saveTemplateTreeExpandState();
    m_templateSearchActive = true;
  }

  // Hide all, then reveal matches and their ancestors
  for (QTreeWidgetItem* item : allItems) item->setHidden(true);

  QList<QTreeWidgetItem*> found = m_templateTree->findItems(
      searchWord, Qt::MatchContains | Qt::MatchRecursive);
  for (QTreeWidgetItem* item : found) {
    for (QTreeWidgetItem* p = item; p; p = p->parent()) {
      p->setHidden(false);
      p->setExpanded(true);
    }
  }
}

//-----------------------------------------------------------------------------

QTreeWidgetItem* CustomPanelEditorPopup::findTemplateItemByPath(
    const QString& path) const {
  if (path.isEmpty() || !m_templateTree) return nullptr;
  QList<QTreeWidgetItem*> items = m_templateTree->findItems(
      QString(), Qt::MatchContains | Qt::MatchRecursive);
  for (QTreeWidgetItem* item : items) {
    if (item->data(0, Qt::UserRole).toString() == path) return item;
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

bool CustomPanelEditorPopup::loadTemplateList() {
  TFilePath customPanelTemplateFolder = customPaneTemplateFolderPath();
  if (!TSystem::doesExistFileOrLevel(customPanelTemplateFolder)) {
    DVGui::warning(tr("Template folder %1 not found.")
                       .arg(customPanelTemplateFolder.getQString()));
    return false;
  }

  const QString previousPath = m_currentTemplatePath;

  m_templateTree->blockSignals(true);
  m_templateTree->clear();

  const QIcon folderIcon = templateFolderIcon();

  // Library templates — folders on disk become collapsible tree nodes
  populateTemplateFolder(nullptr, customPanelTemplateFolder, 0, false,
                         folderIcon);

  const int templateCount = m_templateTree->topLevelItemCount();
  if (templateCount == 0) {
    m_templateTree->blockSignals(false);
    DVGui::warning(tr("Template files not found."));
    return false;
  }

  // Registered user panels (editable), including subfolders
  TFilePath customPanelsFolder = customPaneFolderPath();
  if (TSystem::doesExistFileOrLevel(customPanelsFolder)) {
    QTreeWidgetItem* editRoot =
        new QTreeWidgetItem(QStringList(tr("Registered Panels")));
    editRoot->setIcon(0, folderIcon);
    editRoot->setFlags(Qt::ItemIsEnabled);
    editRoot->setData(0, Qt::UserRole, QString());
    populateTemplateFolder(editRoot, customPanelsFolder, 0, true, folderIcon);
    if (editRoot->childCount() == 0)
      delete editRoot;
    else
      m_templateTree->addTopLevelItem(editRoot);
  }

  m_templateSearchActive = false;
  m_templateExpandStateBeforeSearch.clear();

  QTreeWidgetItem* toSelect = nullptr;
  if (!previousPath.isEmpty()) toSelect = findTemplateItemByPath(previousPath);
  if (!toSelect) toSelect = findFirstTemplateItem(nullptr);

  if (toSelect) {
    m_currentTemplatePath = toSelect->data(0, Qt::UserRole).toString();
    m_templateTree->setCurrentItem(toSelect);
    // Only open the path to the current template — leave other folders closed
    expandTemplateTreeAncestors(toSelect);
  } else {
    m_currentTemplatePath.clear();
  }
  m_templateTree->blockSignals(false);

  updateTemplateValueLabel();
  if (!m_currentTemplatePath.isEmpty()) onTemplateSwitched();

  return true;
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::createFields() {
  // Clear existing fields
  if (QLayout* oldLay = m_UiFieldsContainer->layout()) {
    QLayoutItem* item;
    while ((item = oldLay->takeAt(0)) != nullptr) {
      if (item->widget()) delete item->widget();
      delete item;
    }
    delete oldLay;
  }

  auto* gridLay = new QGridLayout();
  gridLay->setContentsMargins(15, 15, 15, 15);
  gridLay->setHorizontalSpacing(10);
  gridLay->setVerticalSpacing(15);
  gridLay->setColumnStretch(0, 0);
  gridLay->setColumnStretch(1, 1);
  gridLay->setColumnStretch(2, 1);
  m_UiFieldsContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  m_UiFieldsContainer->setLayout(gridLay);

  // For each entry
  int uiCounts[2]     = {0, 0};
  QString labelStr[2] = {tr("Button"), tr("Scroller")};
  int id              = 0;
  int row             = 0;

  for (auto& entry : m_uiEntries) {
    if (entry.type == Button || entry.type == Scroller_Back) {
      entry.field   = new CustomPanelUIField(id, entry.objectName, this);
      QString label = labelStr[static_cast<int>(entry.type)] +
                      QString::number(uiCounts[entry.type] + 1);
      uiCounts[entry.type]++;
      gridLay->addWidget(new QLabel(label, this), row, 0, Qt::AlignRight);
      gridLay->addWidget(entry.field, row, 1);
    } else {  // Scroller_Fore
      entry.field = new CustomPanelUIField(id, entry.objectName, this, false);
      gridLay->addWidget(entry.field, row, 2);
    }

    if (entry.type == Button || entry.type == Scroller_Fore) {
      row++;
    }

    connect(entry.field, &CustomPanelUIField::highlight, this,
            &CustomPanelEditorPopup::onHighlight);
    connect(entry.field, &CustomPanelUIField::commandChanged, this,
            &CustomPanelEditorPopup::onCommandChanged);
    id++;
  }

  m_UiFieldsContainer->adjustSize();

  // Scroll only when there are too many rows (e.g. 25-button templates).
  // Short templates keep natural height — no scrollbar.
  if (m_fieldsScrollArea) {
    static const int kScrollAfterRows = 12;
    if (row > kScrollAfterRows) {
      m_fieldsScrollArea->setMinimumHeight(80);
      m_fieldsScrollArea->setMaximumHeight(220);
      m_fieldsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
      m_fieldsScrollArea->setMinimumHeight(0);
      m_fieldsScrollArea->setMaximumHeight(QWIDGETSIZE_MAX);
      m_fieldsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
  }
}

//-----------------------------------------------------------------------------

// Create entries from a widget just loaded from .ui file
void CustomPanelEditorPopup::buildEntries(QWidget* customWidget) {
  m_uiEntries.clear();

  // This will define child widgets positions
  customWidget->grab();

  QList<QWidget*> allWidgets = customWidget->findChildren<QWidget*>();
  std::sort(allWidgets.begin(), allWidgets.end(),
            [](const QWidget* a, const QWidget* b) -> bool {
              return (a->pos().y() == b->pos().y())
                         ? (a->pos().x() < b->pos().x())
                         : (a->pos().y() < b->pos().y());
            });

  for (auto widget : allWidgets) {
    if (widget->objectName().isEmpty()) {
      continue;
    }
    if (widget->layout() != nullptr) {
      continue;
    }

    UiEntry entry;
    entry.type = (qobject_cast<QAbstractButton*>(widget))
                     ? static_cast<UiType>(Button)
                     : static_cast<UiType>(Scroller_Back);

    entry.objectName = widget->objectName();
    if (entry.type == Button) {
      entry.rect = QRect(relativePos(widget, customWidget), widget->size());
    } else {  // Scroller_Back
      entry.orientation =
          (widget->width() > widget->height()) ? Qt::Horizontal : Qt::Vertical;
      if (entry.orientation == Qt::Horizontal) {
        entry.rect = QRect(relativePos(widget, customWidget),
                           QSize(widget->width() / 2, widget->height()));
      } else {
        entry.rect = QRect(relativePos(widget, customWidget),
                           QSize(widget->width(), widget->height() / 2));
      }
    }
    m_uiEntries.append(entry);

    // Register Scroller_Fore
    if (entry.type == Scroller_Back) {
      UiEntry foreEntry = entry;
      foreEntry.type    = static_cast<UiType>(Scroller_Fore);
      if (foreEntry.orientation == Qt::Horizontal) {
        foreEntry.rect.translate(widget->width() / 2, 0);
      } else {
        foreEntry.rect.translate(0, widget->height() / 2);
      }
      m_uiEntries.append(foreEntry);
    }
  }
}

//-----------------------------------------------------------------------------

// Update widget using the current entries
void CustomPanelEditorPopup::updateControls(QWidget* customWidget) {
  QList<QWidget*> allWidgets = customWidget->findChildren<QWidget*>();
  for (auto widget : allWidgets) {
    QList<int> entryIds = entryIdByObjName(widget->objectName());
    if (entryIds.isEmpty()) {
      continue;
    }

    UiEntry entry = m_uiEntries.at(entryIds.at(0));
    if (entry.type == Button) {
      QString commandId = entry.field->commandId();
      QAction* action   = CommandManager::instance()->getAction(
          commandId.toStdString().c_str());
      if (!action) {
        continue;
      }

      QAbstractButton* button = qobject_cast<QAbstractButton*>(widget);
      QToolButton* tb         = qobject_cast<QToolButton*>(widget);

      CommandManager::instance()->enlargeIcon(commandId.toStdString().c_str(),
                                              button->iconSize());
      if (tb) {
        tb->setDefaultAction(action);
      } else if (button) {
        button->setIcon(action->icon());
      }
    }
  }
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onTemplateSwitched() {
  QString fp = currentTemplatePath();
  if (fp.isEmpty()) return;

  QUiLoader loader;
  QFile file(fp);

  if (!file.open(QFile::ReadOnly)) {
    return;
  }

  QWidget* customWidget = loader.load(&file, nullptr);
  file.close();

  // Create entries from a widget just loaded from .ui file
  buildEntries(customWidget);

  // ObjectName of each widget will be overwritten in this function
  CustomPanelManager::instance()->initializeControl(customWidget);

  // Create UI fields
  createFields();

  UiPreviewWidget* previewWidget =
      new UiPreviewWidget(customWidget->grab(), m_uiEntries, this);

  if (m_previewArea->widget()) {
    UiPreviewWidget* oldPreview =
        qobject_cast<UiPreviewWidget*>(m_previewArea->widget());
    if (oldPreview) {
      disconnect(oldPreview, &UiPreviewWidget::clicked, this,
                 &CustomPanelEditorPopup::onPreviewClicked);
      disconnect(oldPreview, &UiPreviewWidget::dropped, this,
                 &CustomPanelEditorPopup::onPreviewDropped);
    }
    delete m_previewArea->widget();
  }

  connect(previewWidget, &UiPreviewWidget::clicked, this,
          &CustomPanelEditorPopup::onPreviewClicked);
  connect(previewWidget, &UiPreviewWidget::dropped, this,
          &CustomPanelEditorPopup::onPreviewDropped);

  m_previewArea->setWidget(previewWidget);
  previewWidget->onViewerResize(m_previewArea->size());

  delete customWidget;

  TFilePath templateFp(fp);
  if (customPaneFolderPath().isAncestorOf(templateFp)) {
    m_panelNameEdit->setText(relativePanelId(templateFp, customPaneFolderPath()));
  }

  updateTemplateValueLabel();
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onTemplateHeaderClicked() {
  setTemplateBrowserVisible(!m_templateBrowser || m_templateBrowser->isHidden());
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onTemplateSearchTextChanged(const QString& text) {
  filterTemplateTree(text.trimmed());
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onTemplateItemClicked(QTreeWidgetItem* item,
                                                   int column) {
  Q_UNUSED(column);
  if (!item) return;

  const QString path = item->data(0, Qt::UserRole).toString();
  if (path.isEmpty()) {
    // Folder row: expand/collapse is handled by the branch triangle only
    return;
  }

  const bool pathChanged = (path != m_currentTemplatePath);
  m_currentTemplatePath  = path;
  m_templateTree->setCurrentItem(item);
  updateTemplateValueLabel();
  // Keep the browser open so the user can try several templates without
  // reopening the tree or losing their expand/collapse layout.

  if (pathChanged) onTemplateSwitched();
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onHighlight(int id) {
  if (!m_previewArea->widget()) {
    return;
  }

  UiPreviewWidget* previewWidget =
      qobject_cast<UiPreviewWidget*>(m_previewArea->widget());
  if (!previewWidget) {
    return;
  }

  previewWidget->highlightUi(id);
}

//-----------------------------------------------------------------------------
// Set pixmap of updated ui to the preview
void CustomPanelEditorPopup::onCommandChanged(const QString& oldCmdId,
                                              const QString& newCmdId) {
  // If the command is dragged from another field, then swap the commands.
  if (!newCmdId.isEmpty()) {
    CustomPanelUIField* senderField =
        qobject_cast<CustomPanelUIField*>(sender());
    for (auto& entry : m_uiEntries) {
      if (!entry.field || entry.field == senderField) {
        continue;
      }
      if (entry.field->commandId() == newCmdId) {
        entry.field->setCommand(oldCmdId);
        break;
      }
    }
  }

  QString fp = currentTemplatePath();
  QFile tmplFile(fp);
  if (!tmplFile.open(QFile::ReadOnly)) {
    return;
  }

  QUiLoader loader;
  QWidget* customWidget = loader.load(&tmplFile, nullptr);
  tmplFile.close();

  updateControls(customWidget);

  UiPreviewWidget* previewWidget =
      qobject_cast<UiPreviewWidget*>(m_previewArea->widget());
  if (previewWidget) {
    previewWidget->setUiPixmap(customWidget->grab());
  }

  delete customWidget;
}

void CustomPanelEditorPopup::onPreviewClicked(int id) {
  CustomPanelUIField* field = m_uiEntries.at(id).field;
  if (!field) {
    return;
  }

  QString commandId = field->commandId();
  if (commandId.isEmpty()) {
    return;
  }

  QMimeData* mimeData = new QMimeData;
  mimeData->setText(commandId);

  QString dragPixmapTxt = field->text();
  QFontMetrics fm(QApplication::font());
  QPixmap pix(fm.boundingRect(dragPixmapTxt).adjusted(-2, -2, 2, 2).size());

  {
    QPainter painter(&pix);
    painter.fillRect(pix.rect(), Qt::white);
    painter.setPen(Qt::black);
    painter.drawText(pix.rect(), Qt::AlignCenter, dragPixmapTxt);
  }

  QDrag* drag = new QDrag(sender());
  drag->setMimeData(mimeData);
  drag->setPixmap(pix);

  drag->exec(Qt::MoveAction);
}

void CustomPanelEditorPopup::onPreviewDropped(int id, const QString& cmdId,
                                              bool fromTree) {
  CustomPanelUIField* field = m_uiEntries.at(id).field;
  if (!field) {
    return;
  }

  QString oldCommandId = field->commandId();
  if (field->setCommand(cmdId)) {
    if (fromTree) {
      emit field->commandChanged(QString(), QString());
    } else {
      emit field->commandChanged(oldCommandId, cmdId);
    }
  }
}

//-----------------------------------------------------------------------------

QList<int> CustomPanelEditorPopup::entryIdByObjName(const QString& objName) {
  QList<int> ret;
  for (int i = 0; i < m_uiEntries.size(); i++) {
    if (m_uiEntries[i].objectName == objName) {
      ret.append(i);
    }
  }
  return ret;
}

void CustomPanelEditorPopup::replaceObjectNames(QDomElement& element) {
  QDomNode n = element.firstChild();
  while (!n.isNull()) {
    if (n.isElement()) {
      QDomElement e = n.toElement();
      // Check self
      if (e.tagName() == "widget" && e.hasAttribute("name")) {
        QString objName     = e.attribute("name");
        QList<int> entryIds = entryIdByObjName(objName);
        if (!entryIds.isEmpty()) {
          UiEntry entry = m_uiEntries.at(entryIds[0]);

          if (entry.type == Button) {
            e.setAttribute("name", entry.field->commandId());
          } else {  // Scroller
            UiEntry entryFore = m_uiEntries.at(entryIds[1]);
            QStringList newNameList;
            newNameList.append((entry.orientation == Qt::Horizontal)
                                   ? "HScroller"
                                   : "VScroller");
            newNameList.append(entry.field->commandId());
            newNameList.append(entryFore.field->commandId());
            e.setAttribute("name", newNameList.join("__"));
          }
        }
      }
      // Check recursively
      replaceObjectNames(e);
    }
    n = n.nextSibling();
  }
}

void CustomPanelEditorPopup::onRegister() {
  QString panelName = m_panelNameEdit->text().trimmed();
  panelName.replace('\\', '/');
  while (panelName.startsWith('/')) panelName.remove(0, 1);
  if (panelName.isEmpty()) {
    DVGui::warning(tr("Please input the panel name."));
    return;
  }
  if (panelName.contains("..")) {
    DVGui::warning(tr("Invalid panel name."));
    return;
  }

  // Allow optional subfolders in the panel name (e.g. Characters/Eyes)
  TFilePath customPanelPath = customPaneFolderPath();
  const QStringList parts   = panelName.split('/', Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    DVGui::warning(tr("Please input the panel name."));
    return;
  }
  for (int i = 0; i < parts.size(); ++i) {
    if (i == parts.size() - 1)
      customPanelPath = customPanelPath + TFilePath(parts[i] + ".ui");
    else
      customPanelPath = customPanelPath + TFilePath(parts[i]);
  }

  // Overwrite confirmation
  if (TSystem::doesExistFileOrLevel(customPanelPath)) {
    QString question =
        tr("The custom panel %1 already exists. Do you want to overwrite?")
            .arg(panelName);
    int ret = DVGui::MsgBox(question, tr("Overwrite"), tr("Cancel"), 0);
    if (ret == 0 || ret == 2) {
      return;
    }
  }

  // Create folder if not exist
  if (!TSystem::touchParentDir(customPanelPath)) {
    DVGui::warning(tr("Failed to create folder."));
    return;
  }

  // Base template file
  QDomDocument doc(parts.last());
  QFile tmplFile(currentTemplatePath());
  if (!tmplFile.open(QIODevice::ReadOnly)) {
    DVGui::warning(tr("Failed to open the template."));
    return;
  }

  if (!doc.setContent(&tmplFile)) {
    tmplFile.close();
    return;
  }
  tmplFile.close();

  QDomElement docElem = doc.documentElement();
  replaceObjectNames(docElem);

  QFile file(customPanelPath.getQString());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    DVGui::warning(tr("Failed to open the file for writing."));
    return;
  }

  QTextStream stream(&file);
  stream.setCodec("UTF-8");
  stream << doc.toString();
  file.close();

  CustomPanelManager::instance()->loadCustomPanelEntries();
  close();
}

//-----------------------------------------------------------------------------

// Custom dialog for panel removal with collapsible multi-selection.
// Top section stays anchored; collapsible section expands downward only.
class RemoveCustomPanelDialog : public QDialog {
  QLineEdit* m_mainPanelField;
  QString m_mainPanelName;
  QWidget* m_collapsibleWidget;
  QList<QCheckBox*> m_checkboxes;
  QStringList m_allPanels;
  QPushButton* m_toggleButton;
  
public:
  RemoveCustomPanelDialog(const QStringList& panels, const QString& currentPanel, QWidget* parent)
      : QDialog(parent), m_allPanels(panels), m_mainPanelName(currentPanel) {
    setWindowTitle(tr("Remove Custom Panels"));
    setModal(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(4);
    
    // --- Top section: anchored, never moves ---
    QLabel* mainLabel = new QLabel(tr("Selected Panel to Remove:"), this);
    mainLayout->addWidget(mainLabel);
    
    // Read-only field with dark background for visibility
    m_mainPanelField = new QLineEdit(currentPanel, this);
    m_mainPanelField->setReadOnly(true);
    m_mainPanelField->setFrame(true);
    m_mainPanelField->setStyleSheet(
        "QLineEdit { background-color: #2a2a2a; color: #e0e0e0; "
        "border: 1px solid #555; padding: 3px; }");
    mainLayout->addWidget(m_mainPanelField);
    
    // Collapsible toggle (immediately below, minimal gap)
    m_toggleButton = new QPushButton(this);
    m_toggleButton->setFlat(true);
    m_toggleButton->setText(QString::fromUtf8("\xe2\x96\xb6") + tr(" Other custom panels"));
    m_toggleButton->setStyleSheet("text-align: left; padding: 4px 0px;");
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(m_toggleButton);
    
    // Collapsible container (starts hidden, zero size policy when hidden)
    m_collapsibleWidget = new QWidget(this);
    m_collapsibleWidget->setVisible(false);
    m_collapsibleWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout* collapsibleLayout = new QVBoxLayout(m_collapsibleWidget);
    collapsibleLayout->setContentsMargins(16, 2, 0, 2);
    collapsibleLayout->setSpacing(2);
    
    // Scroll area for the list of other panels
    QScrollArea* scrollArea = new QScrollArea(m_collapsibleWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumHeight(180);
    
    QWidget* scrollWidget = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(2);
    
    for (const QString& panel : panels) {
      if (panel != currentPanel) {
        QCheckBox* cb = new QCheckBox(panel, scrollWidget);
        scrollLayout->addWidget(cb);
        m_checkboxes.append(cb);
      }
    }
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    collapsibleLayout->addWidget(scrollArea);
    mainLayout->addWidget(m_collapsibleWidget);
    
    // Stretch between content and buttons so buttons stay at bottom
    mainLayout->addStretch(1);
    
    // Bottom buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);
    QPushButton* okBtn = new QPushButton(tr("Remove"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
    
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    // Toggle logic: show/hide collapsible widget, resize downward
    connect(m_toggleButton, &QPushButton::clicked, [this]() {
      bool wasVisible = m_collapsibleWidget->isVisible();
      m_collapsibleWidget->setVisible(!wasVisible);
      if (wasVisible) {
        m_toggleButton->setText(QString::fromUtf8("\xe2\x96\xb6") + tr(" Other custom panels"));
      } else {
        m_toggleButton->setText(QString::fromUtf8("\xe2\x96\xbc") + tr(" Other custom panels"));
      }
      // Resize dialog to fit content, expanding downward only
      QApplication::processEvents();
      resize(width(), sizeHint().height());
    });
    
    // Start compact
    resize(320, sizeHint().height());
  }
  
  QStringList getSelectedPanels() const {
    QStringList selected;
    // Always include main panel
    selected.append(m_mainPanelName);
    // Add checked panels from collapsible section
    for (QCheckBox* cb : m_checkboxes) {
      if (cb->isChecked() && !selected.contains(cb->text())) {
        selected.append(cb->text());
      }
    }
    return selected;
  }
};

void CustomPanelEditorPopup::onRemove() {
  // List all existing custom panels (including subfolders)
  TFilePath customPanelsFolder = customPaneFolderPath();
  if (!TSystem::doesExistFileOrLevel(customPanelsFolder)) {
    DVGui::warning(tr("No custom panels found."));
    return;
  }

  QStringList panelNames;
  collectUiPanelIds(customPanelsFolder, customPanelsFolder, 0, panelNames);

  if (panelNames.isEmpty()) {
    DVGui::warning(tr("No custom panels found."));
    return;
  }

  // Determine current panel (use panel name edit field if it matches an existing panel)
  QString currentPanel = m_panelNameEdit->text().trimmed();
  currentPanel.replace('\\', '/');
  if (!panelNames.contains(currentPanel)) {
    currentPanel = panelNames.first();  // Fallback to first panel
  }

  // Show custom dialog
  RemoveCustomPanelDialog dialog(panelNames, currentPanel, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;  // User cancelled
  }

  QStringList selectedPanels = dialog.getSelectedPanels();
  if (selectedPanels.isEmpty()) {
    return;
  }

  // Confirmation dialog
  QString question;
  if (selectedPanels.size() == 1) {
    question = tr("Are you sure you want to permanently remove the custom panel '%1'?\n\n"
                  "This will delete the panel file and unregister its command.")
                   .arg(selectedPanels.first());
  } else {
    question = tr("Are you sure you want to permanently remove %1 custom panels?\n\n"
                  "This will delete the panel files and unregister their commands.")
                   .arg(selectedPanels.size());
  }

  int ret = DVGui::MsgBox(question, tr("Remove"), tr("Cancel"), 0);
  if (ret == 0 || ret == 2) {
    return;  // User cancelled
  }

  // Delete the .ui files and mark commands as invisible
  int successCount = 0;
  QStringList deletedPanels;

  for (const QString& panelName : selectedPanels) {
    TFilePath panelPath = customPanelsFolder;
    const QStringList parts = panelName.split('/', Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
      if (i == parts.size() - 1)
        panelPath = panelPath + TFilePath(parts[i] + ".ui");
      else
        panelPath = panelPath + TFilePath(parts[i]);
    }
    if (TSystem::doesExistFileOrLevel(panelPath)) {
      try {
        TSystem::deleteFile(panelPath);
        deletedPanels.append(panelName);
        successCount++;

        // IMMEDIATELY mark the command action as invisible
        // to hide it from Configure Shortcuts during current session
        QString commandId = "MI_CustomPanel_" +
                            QString(panelName).replace('/', "__");
        QAction* action = CommandManager::instance()->getAction(
            commandId.toStdString().c_str(), false);
        if (action) {
          action->setVisible(false);
        }
      } catch (...) {
        DVGui::warning(tr("Failed to delete panel: %1").arg(panelName));
      }
    }
  }

  if (successCount > 0) {
    // Reload custom panel entries
    // This rebuilds m_registeredPanelIds to exclude deleted panels
    // and updates the Custom Panels menu
    CustomPanelManager::instance()->loadCustomPanelEntries();

    // Refresh the template tree in this editor to reflect changes
    loadTemplateList();

    QString message = (successCount == 1)
                          ? tr("Custom panel has been successfully removed.")
                          : tr("%1 custom panels have been successfully removed.")
                                .arg(successCount);
    DVGui::info(message);
  }
}

//-----------------------------------------------------------------------------

// Static instance pointer for refreshCommandTreeIfOpen()
CustomPanelEditorPopup* CustomPanelEditorPopup::s_instance = nullptr;

CustomPanelEditorPopup::~CustomPanelEditorPopup() {
  if (s_instance == this) s_instance = nullptr;
}

void CustomPanelEditorPopup::refreshCommandTreeIfOpen() {
  if (s_instance && s_instance->m_commandListTree) {
    s_instance->m_commandListTree->refreshTree();
    s_instance->m_commandListTree->searchItems();
  }
}

CustomPanelEditorPopup::CustomPanelEditorPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "CustomPanelEditorPopup") {
  s_instance = this;
  setWindowTitle(tr("Custom Panel Editor"));

  m_commandListTree =
      new CommandListTree(tr("a control in the panel"), this, false);

  QLabel* commandItemListLabel = new QLabel(tr("Command List"), this);
  QFont f("Arial", 15, QFont::Bold);
  commandItemListLabel->setFont(f);

  QLineEdit* searchEdit = new QLineEdit(this);

  m_previewArea       = new UiPreviewArea(this);
  m_UiFieldsContainer = new QWidget(this);
  m_panelNameEdit     = new QLineEdit("My Custom Panel", this);

  // Hybrid template selector: one-line header + inline collapsible browser
  m_templateHeader = new QWidget(this);
  m_templateHeader->setCursor(Qt::PointingHandCursor);
  m_templateToggle = new QToolButton(m_templateHeader);
  // Match Tool Properties collapsible headers (checked → accent square)
  m_templateToggle->setFixedSize(16, 16);
  m_templateToggle->setStyleSheet("QToolButton { border: none; }");
  applyTemplateToggleAccent(m_templateToggle, false);

  QLabel* templateLabel = new QLabel(tr("Template:"), m_templateHeader);
  m_templateValueLabel  = new QLabel(tr("(none)"), m_templateHeader);
  m_templateValueLabel->setObjectName("valueLabel");
  {
    QFont italicFont = m_templateValueLabel->font();
    italicFont.setItalic(true);
    m_templateValueLabel->setFont(italicFont);
    QPalette pal = m_templateValueLabel->palette();
    pal.setColor(QPalette::WindowText,
                 palette().color(QPalette::Disabled, QPalette::WindowText));
    m_templateValueLabel->setPalette(pal);
  }

  QHBoxLayout* headerLay = new QHBoxLayout(m_templateHeader);
  headerLay->setContentsMargins(0, 0, 0, 0);
  headerLay->setSpacing(5);
  headerLay->addWidget(m_templateToggle, 0);
  headerLay->addWidget(templateLabel, 0);
  headerLay->addWidget(m_templateValueLabel, 1);

  // Inline browser (not Qt::Popup) so open/close via triangle is reliable
  m_templateBrowser = new QFrame(this);
  m_templateBrowser->setObjectName("TemplateSelectorBrowser");
  m_templateBrowser->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
  m_templateBrowser->setVisible(false);
  m_templateBrowser->setMaximumHeight(240);

  m_templateSearchEdit = new QLineEdit(m_templateBrowser);
  m_templateSearchEdit->setPlaceholderText(tr("Search templates..."));
  m_templateSearchEdit->setClearButtonEnabled(true);

  m_templateTree = new QTreeWidget(m_templateBrowser);
  // Original OT tree style (readable zebra) — same as Command List
  m_templateTree->setObjectName("SolidLineFrame");
  m_templateTree->setAlternatingRowColors(true);
  m_templateTree->setHeaderHidden(true);
  m_templateTree->setRootIsDecorated(true);
  m_templateTree->setAnimated(false);
  m_templateTree->setUniformRowHeights(true);
  m_templateTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_templateTree->setIconSize(QSize(21, 18));
  m_templateTree->header()->close();
  applyTemplateTreeBranchAccent(m_templateTree);

  QVBoxLayout* browserLay = new QVBoxLayout(m_templateBrowser);
  browserLay->setContentsMargins(4, 4, 4, 4);
  browserLay->setSpacing(4);
  {
    QHBoxLayout* searchLay = new QHBoxLayout();
    searchLay->setContentsMargins(0, 0, 0, 0);
    searchLay->setSpacing(4);
    searchLay->addWidget(new QLabel(tr("Search:"), m_templateBrowser), 0);
    searchLay->addWidget(m_templateSearchEdit, 1);
    browserLay->addLayout(searchLay, 0);
    browserLay->addWidget(m_templateTree, 1);
  }

  // Command fields wrapper — scrollbar enabled in createFields() only if needed
  m_fieldsScrollArea = new QScrollArea(this);
  m_fieldsScrollArea->setWidgetResizable(true);
  m_fieldsScrollArea->setFrameShape(QFrame::NoFrame);
  m_fieldsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_fieldsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // Do not vertically expand and stretch the yellow fields
  m_fieldsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  m_fieldsScrollArea->setWidget(m_UiFieldsContainer);

  QPushButton* registerButton = new QPushButton(tr("Register"), this);
  QPushButton* removeButton   = new QPushButton(tr("Remove"), this);
  QPushButton* cancelButton   = new QPushButton(tr("Cancel"), this);

  m_previewArea->setStyleSheet("background-color: black;");

  beginHLayout();

  QVBoxLayout* leftLay = new QVBoxLayout();
  leftLay->setContentsMargins(0, 0, 0, 0);
  leftLay->setSpacing(10);
  {
    leftLay->addWidget(m_templateHeader, 0);
    leftLay->addWidget(m_templateBrowser, 0);
    leftLay->addWidget(m_fieldsScrollArea, 0);
    leftLay->addWidget(m_previewArea, 1);
  }
  addLayout(leftLay);

  QVBoxLayout* rightLay = new QVBoxLayout();
  rightLay->setContentsMargins(0, 0, 0, 0);
  rightLay->setSpacing(10);
  {
    rightLay->addWidget(commandItemListLabel, 0);
    QHBoxLayout* searchLay = new QHBoxLayout();
    searchLay->setContentsMargins(0, 0, 0, 0);
    searchLay->setSpacing(5);
    {
      searchLay->addWidget(new QLabel(tr("Search:"), this), 0);
      searchLay->addWidget(searchEdit);
    }
    rightLay->addLayout(searchLay, 0);
    rightLay->addWidget(m_commandListTree, 1);
  }
  addLayout(rightLay);

  endHLayout();

  m_buttonLayout->addStretch(1);
  QHBoxLayout* nameLay = new QHBoxLayout();
  nameLay->setContentsMargins(0, 0, 0, 0);
  nameLay->setSpacing(3);
  {
    nameLay->addWidget(new QLabel(tr("Panel name:"), this), 0);
    nameLay->addWidget(m_panelNameEdit, 1);
  }
  m_buttonLayout->addLayout(nameLay, 0);
  m_buttonLayout->addWidget(registerButton, 0);
  m_buttonLayout->addWidget(removeButton, 0);
  m_buttonLayout->addSpacing(10);
  m_buttonLayout->addWidget(cancelButton, 0);

  connect(cancelButton, &QPushButton::clicked, this,
          &CustomPanelEditorPopup::close);
  connect(m_templateTree, &QTreeWidget::itemClicked, this,
          &CustomPanelEditorPopup::onTemplateItemClicked);
  connect(m_templateSearchEdit, &QLineEdit::textChanged, this,
          &CustomPanelEditorPopup::onTemplateSearchTextChanged);
  connect(registerButton, &QPushButton::clicked, this,
          &CustomPanelEditorPopup::onRegister);
  connect(removeButton, &QPushButton::clicked, this,
          &CustomPanelEditorPopup::onRemove);
  connect(searchEdit, &QLineEdit::textChanged, this,
          &CustomPanelEditorPopup::onSearchTextChanged);

  m_templateHeader->installEventFilter(this);
  m_templateToggle->installEventFilter(this);

  // Load template
  if (!loadTemplateList()) {
    // Handle template loading failure if needed
  }
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::showEvent(QShowEvent* event) {
  // Refresh brush preset and size commands to ensure they're up-to-date
  ToolPresetCommandManager::instance()->refreshPresetCommands();
  ToolPresetCommandManager::instance()->refreshSizeCommands();

  // Refresh the command list tree to show new commands
  m_commandListTree->refreshTree();
  m_commandListTree->searchItems();  // Clear any search filter

  // Theme may have been ready after construction — refresh accents
  if (m_templateTree) {
    applyTemplateTreeBranchAccent(m_templateTree);
    applyTemplateToggleAccent(m_templateToggle,
                              m_templateBrowser && m_templateBrowser->isVisible());
    const QIcon folderIcon = templateFolderIcon();
    for (int i = 0; i < m_templateTree->topLevelItemCount(); ++i)
      applyFolderIconRecursive(m_templateTree->topLevelItem(i), folderIcon);
  }

  Dialog::showEvent(event);
}

//-----------------------------------------------------------------------------

bool CustomPanelEditorPopup::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::MouseButtonPress) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() != Qt::LeftButton)
      return QObject::eventFilter(watched, event);

    // Whole header row toggles the browser (triangle included)
    if (watched == m_templateHeader || watched == m_templateToggle) {
      onTemplateHeaderClicked();
      return true;
    }
  }
  return QObject::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onSearchTextChanged(const QString& text) {
  static bool busy = false;
  if (busy) {
    return;
  }

  busy = true;
  m_commandListTree->searchItems(text);
  busy = false;
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<CustomPanelEditorPopup> openCustomPanelEditorPopup(
    MI_CustomPanelEditor);
