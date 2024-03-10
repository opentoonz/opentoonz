#include "custompaneleditorpopup.h"

// Tnz includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "shortcutpopup.h"
#include "custompanelmanager.h"
#include "commandbarpopup.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// ToonzLib
#include "toonz/toonzfolders.h"

// ToonzCore
#include "tsystem.h"

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
#include <QComboBox>
#include <QPushButton>
#include <QUiLoader>
#include <QColor>
#include <QDomDocument>
#include <QTextStream>
#include <QBuffer>
#include <QToolButton>

namespace {
const TFilePath CustomPanelTemplateFolderName("custom panel templates");
const TFilePath customPaneTemplateFolderPath() {
  return ToonzFolder::getLibraryFolder() + CustomPanelTemplateFolderName;
}
const TFilePath CustomPanelFolderName("custompanels");
const TFilePath customPaneFolderPath() {
  return ToonzFolder::getMyModuleDir() + CustomPanelFolderName;
}

QPoint relativePos(QWidget* child, QWidget* refParent) {
  if (child->parentWidget() == refParent)
    return child->pos();
  else
    return child->pos() + relativePos(child->parentWidget(), refParent);
}

}  // namespace

//=============================================================================
// CustomPanelUIField
//-----------------------------------------------------------------------------

CustomPanelUIField::CustomPanelUIField(const int objId,
                                       const QString objectName,
                                       QWidget* parent, bool isFirst)
    : QLabel(tr("Drag and set command"), parent), m_id(objId) {
  QFont fnt = font();
  fnt.setPointSize(12);
  setFont(fnt);
  setStyleSheet("background-color: rgb(255, 255, 128); color: black;");
  setAcceptDrops(true);

  // objName may be a commandId
  if (setCommand(objectName)) return;

  if (objectName.startsWith("HScroller") ||
      objectName.startsWith("VScroller")) {
    QStringList ids = objectName.split("__");
    if (ids.size() == 3) {
      setCommand((isFirst) ? ids[1] : ids[2]);
    }
  }
}

bool CustomPanelUIField::setCommand(QString commandId) {
  if (m_commandId == commandId) return false;
  if (commandId.isEmpty()) {
    m_commandId = commandId;
    setText(tr("Drag and set command"));
    setStyleSheet("background-color: rgb(255, 255, 128); color: black;");
    return true;
  }

  QAction* action =
      CommandManager::instance()->getAction(commandId.toStdString().c_str());
  if (!action) return false;

  m_commandId      = commandId;
  QString tempText = action->text();
  // removing accelerator key indicator
  tempText = tempText.replace(QRegExp("&([^& ])"), "\\1");
  // removing doubled &s
  tempText = tempText.replace("&&", "&");
  setText(tempText);
  setStyleSheet("background-color: rgb(230, 230, 230); color: black;");
  return true;
}

void CustomPanelUIField::enterEvent(QEvent* event) { emit highlight(m_id); }
void CustomPanelUIField::leaveEvent(QEvent* event) { emit highlight(-1); }
void CustomPanelUIField::dragEnterEvent(QDragEnterEvent* event) {
  QString txt = event->mimeData()->text();
  if (CommandManager::instance()->getAction(txt.toStdString().c_str())) {
    event->setDropAction(Qt::CopyAction);
    event->accept();
  }
  emit highlight(m_id);
}

void CustomPanelUIField::dragLeaveEvent(QDragLeaveEvent* event) {
  emit highlight(-1);
}

void CustomPanelUIField::dropEvent(QDropEvent* event) {
  QString oldCommandId = m_commandId;
  QString commandId    = event->mimeData()->text();
  if (setCommand(commandId)) {
    // if dragged from the command tree, command can be duplicated
    if (event->dropAction() == Qt::CopyAction)
      emit commandChanged(QString(), QString());
    else
      emit commandChanged(oldCommandId, m_commandId);
  }
}

void CustomPanelUIField::mousePressEvent(QMouseEvent* event) {
  if (m_commandId.isEmpty()) return;
  QMimeData* mimeData = new QMimeData;
  mimeData->setText(m_commandId);

  QString dragPixmapTxt = text();
  QFontMetrics fm(QApplication::font());
  QPixmap pix(fm.boundingRect(dragPixmapTxt).adjusted(-2, -2, 2, 2).size());
  QPainter painter(&pix);
  painter.fillRect(pix.rect(), Qt::white);
  painter.setPen(Qt::black);
  painter.drawText(pix.rect(), Qt::AlignCenter, dragPixmapTxt);

  QDrag* drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->setPixmap(pix);

  drag->exec(Qt::MoveAction);
}

//=============================================================================
// UiPreviewWidget
//-----------------------------------------------------------------------------

UiPreviewWidget::UiPreviewWidget(QPixmap uiPixmap, QList<UiEntry>& uiEntries,
                                 QWidget* parent)
    : QWidget(parent), m_highlightUiId(-1), m_uiPixmap(uiPixmap) {
  for (auto entry : uiEntries) m_rectTable.append(entry.rect);
  setFixedSize(m_uiPixmap.size());
  setAcceptDrops(true);
  setMouseTracking(true);
}

void UiPreviewWidget::onViewerResize(QSize size) {
  if (m_uiPixmap.isNull()) return;
  setFixedSize(std::max(size.width(), m_uiPixmap.width()),
               std::max(size.height(), m_uiPixmap.height()));
}

void UiPreviewWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.translate((width() - m_uiPixmap.width()) / 2,
              (height() - m_uiPixmap.height()) / 2);
  p.drawPixmap(0, 0, m_uiPixmap);

  for (int id = 0; id < m_rectTable.count(); id++) {
    QRect uiRect = m_rectTable.at(id);
    if (id == m_highlightUiId)
      p.setBrush(QColor(0, 255, 255, 64));
    else
      p.setBrush(Qt::NoBrush);
    p.setPen(Qt::cyan);
    p.drawRect(uiRect);
  }
}

void UiPreviewWidget::highlightUi(const int objId) {
  m_highlightUiId = objId;
  update();
}

void UiPreviewWidget::mousePressEvent(QMouseEvent* event) {
  if (m_highlightUiId >= 0) emit clicked(m_highlightUiId);
}

void UiPreviewWidget::onMove(const QPoint pos) {
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
  QString commandId     = event->mimeData()->text();
  bool isDraggdFromTree = (event->dropAction() == Qt::CopyAction);

  emit dropped(m_highlightUiId, commandId, isDraggdFromTree);
}

//-----------------------------------------------------------------------------

UiPreviewArea::UiPreviewArea(QWidget* parent) : QScrollArea(parent) {}

void UiPreviewArea::resizeEvent(QResizeEvent* event) {
  if (widget()) {
    UiPreviewWidget* previewWidget = dynamic_cast<UiPreviewWidget*>(widget());
    if (previewWidget) {
      previewWidget->onViewerResize(event->size());
    }
  }

  QScrollArea::resizeEvent(event);
}

//=============================================================================
// CustomPanelEditorPopup
//-----------------------------------------------------------------------------

bool CustomPanelEditorPopup::loadTemplateList() {
  // library / custom panel templatesの中をチェック
  TFilePath customPanelTemplateFolder = customPaneTemplateFolderPath();
  if (!TSystem::doesExistFileOrLevel(customPanelTemplateFolder)) {
    DVGui::warning(tr("Template folder %1 not found.")
                       .arg(customPanelTemplateFolder.getQString()));
    return false;
  }
  TFilePathSet fileList =
      TSystem::readDirectory(customPanelTemplateFolder, false, true, false);
  if (fileList.empty()) {
    DVGui::warning(tr("Template files not found."));
    return false;
  }
  m_templateCombo->clear();
  QList<QString> fileNames;
  for (auto file : fileList) {
    // accept only .ui files
    if (file.getType() != "ui") continue;
    m_templateCombo->addItem(QString::fromStdString(file.getName()),
                             file.getQString());
  }

  int templateCount = m_templateCombo->count();

  if (templateCount == 0) {
    DVGui::warning(tr("Template files not found."));
    return false;
  }

  // then, insert user custom panel
  TFilePath customPanelsFolder = customPaneFolderPath();
  if (TSystem::doesExistFileOrLevel(customPanelsFolder)) {
    TFilePathSet fileList2 =
        TSystem::readDirectory(customPanelsFolder, false, true, false);
    for (auto file : fileList2) {
      // accept only .ui files
      if (file.getType() != "ui") continue;
      m_templateCombo->addItem(
          tr("%1 (Edit)").arg(QString::fromStdString(file.getName())),
          file.getQString());
    }
  }

  if (m_templateCombo->count() > templateCount)
    m_templateCombo->insertSeparator(templateCount);

  return true;
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::createFields() {
  QList<QWidget*> widgets = m_UiFieldsContainer->findChildren<QWidget*>();
  foreach (QWidget* child, widgets) {
    delete child;
  }

  QGridLayout* gridLay;
  if (m_UiFieldsContainer->layout())
    gridLay = dynamic_cast<QGridLayout*>(m_UiFieldsContainer->layout());
  else {
    gridLay = new QGridLayout();
    gridLay->setMargin(15);
    gridLay->setHorizontalSpacing(10);
    gridLay->setVerticalSpacing(15);
    gridLay->setColumnStretch(0, 0);
    gridLay->setColumnStretch(1, 1);
    gridLay->setColumnStretch(2, 1);
    m_UiFieldsContainer->setLayout(gridLay);
  }

  // for each entry
  int uiCounts[2]     = {0, 0};
  QString labelStr[2] = {tr("Button"), tr("Scroller")};
  int id              = 0;
  int row             = 0;
  for (auto& entry : m_uiEntries) {
    if (entry.type == Button || entry.type == Scroller_Back) {
      entry.field = new CustomPanelUIField(id, entry.objectName, this);
      QString label =
          labelStr[(int)entry.type] + QString::number(uiCounts[entry.type] + 1);
      uiCounts[entry.type]++;
      gridLay->addWidget(new QLabel(label, this), row, 0, Qt::AlignRight);
      gridLay->addWidget(entry.field, row, 1);
    } else {  // Scroller_Fore
      entry.field = new CustomPanelUIField(id, entry.objectName, this, false);
      gridLay->addWidget(entry.field, row, 2);
    }
    if (entry.type == Button || entry.type == Scroller_Fore) row++;

    connect(entry.field, SIGNAL(highlight(int)), this, SLOT(onHighlight(int)));
    connect(entry.field, SIGNAL(commandChanged(QString, QString)), this,
            SLOT(onCommandChanged(QString, QString)));
    id++;
  }
}

//-----------------------------------------------------------------------------

// create entries from a widget just loaded from .ui file
void CustomPanelEditorPopup::buildEntries(QWidget* customWidget) {
  m_uiEntries.clear();

  // this will define child widgets positions
  customWidget->grab();

  QList<QWidget*> allWidgets = customWidget->findChildren<QWidget*>();
  std::sort(allWidgets.begin(), allWidgets.end(),
            [](const QWidget* a, const QWidget* b) -> bool {
              return (a->pos().y() == b->pos().y())
                         ? (a->pos().x() < b->pos().x())
                         : (a->pos().y() < b->pos().y());
            });

  for (auto widget : allWidgets) {
    if (widget->objectName().isEmpty()) continue;
    if (widget->layout() != nullptr) continue;
    UiEntry entry;

    entry.type = (dynamic_cast<QAbstractButton*>(widget))
                     ? (UiType)Button
                     : (UiType)Scroller_Back;

    entry.objectName = widget->objectName();
    if (entry.type == Button)
      entry.rect = QRect(relativePos(widget, customWidget), widget->size());
    else {  // Sroller_Back
      entry.orientation =
          (widget->width() > widget->height()) ? Qt::Horizontal : Qt::Vertical;
      if (entry.orientation == Qt::Horizontal)
        entry.rect = QRect(relativePos(widget, customWidget),
                           QSize(widget->width() / 2, widget->height()));
      else
        entry.rect = QRect(relativePos(widget, customWidget),
                           QSize(widget->width(), widget->height() / 2));
    }
    m_uiEntries.append(entry);

    // register Scroller_Fore
    if (entry.type == Scroller_Back) {
      entry.type = (UiType)Scroller_Fore;
      if (entry.orientation == Qt::Horizontal)
        entry.rect.translate(widget->width() / 2, 0);
      else
        entry.rect.translate(0, widget->height() / 2);

      m_uiEntries.append(entry);
    }
  }
}

//-----------------------------------------------------------------------------

// update widget using the current entries
void CustomPanelEditorPopup::updateControls(QWidget* customWidget) {
  QList<QWidget*> allWidgets = customWidget->findChildren<QWidget*>();
  for (auto widget : allWidgets) {
    QList<int> entryIds = entryIdByObjName(widget->objectName());
    if (entryIds.isEmpty()) continue;
    UiEntry entry = m_uiEntries.at(entryIds.at(0));
    if (entry.type == Button) {
      QString commandId = entry.field->commandId();
      QAction* action   = CommandManager::instance()->getAction(
          commandId.toStdString().c_str());
      if (!action) continue;
      QAbstractButton* button = dynamic_cast<QAbstractButton*>(widget);
      QToolButton* tb         = dynamic_cast<QToolButton*>(widget);
      CommandManager::instance()->enlargeIcon(commandId.toStdString().c_str(),
                                              button->iconSize());
      if (tb)
        tb->setDefaultAction(action);
      else if (button)
        button->setIcon(action->icon());
    }
  }
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onTemplateSwitched() {
  QUiLoader loader;
  QString fp = m_templateCombo->currentData().toString();
  QFile file(fp);

  file.open(QFile::ReadOnly);
  QWidget* customWidget = loader.load(&file, 0);
  file.close();

  // create entries from a widget just loaded from .ui file
  buildEntries(customWidget);

  // objectName of each widget will be overwritten in this function
  CustomPanelManager::instance()->initializeControl(customWidget);

  // create UI fields
  createFields();

  UiPreviewWidget* previewWidget =
      new UiPreviewWidget(customWidget->grab(), m_uiEntries, this);

  if (m_previewArea->widget()) {
    UiPreviewWidget* oldPreview =
        dynamic_cast<UiPreviewWidget*>(m_previewArea->widget());
    if (oldPreview) {
      disconnect(oldPreview, SIGNAL(clicked(int)), this,
                 SLOT(onPreviewClicked(int)));
      disconnect(oldPreview, SIGNAL(dropped(int, QString, bool)), this,
                 SLOT(onPreviewDropped(int, QString, bool)));
    }
    delete m_previewArea->widget();
  }

  connect(previewWidget, SIGNAL(clicked(int)), this,
          SLOT(onPreviewClicked(int)));
  connect(previewWidget, SIGNAL(dropped(int, QString, bool)), this,
          SLOT(onPreviewDropped(int, QString, bool)));

  m_previewArea->setWidget(previewWidget);
  previewWidget->onViewerResize(m_previewArea->size());

  delete customWidget;

  if (customPaneFolderPath().isAncestorOf(TFilePath(fp)))
    m_panelNameEdit->setText(m_templateCombo->currentText().chopped(7));

  updateGeometry();
}
//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onHighlight(int id) {
  if (!m_previewArea->widget()) return;
  UiPreviewWidget* previewWidget =
      dynamic_cast<UiPreviewWidget*>(m_previewArea->widget());
  if (!previewWidget) return;
  previewWidget->highlightUi(id);
}

//-----------------------------------------------------------------------------
// set pixmap of updated ui to the preview
void CustomPanelEditorPopup::onCommandChanged(QString oldCmdId,
                                              QString newCmdId) {
  // If the command is dragged from another field, then swap the commands.
  if (!newCmdId.isEmpty()) {
    CustomPanelUIField* senderField =
        dynamic_cast<CustomPanelUIField*>(sender());
    for (auto entry : m_uiEntries) {
      if (!entry.field || entry.field == senderField) continue;
      if (entry.field->commandId() == newCmdId) {
        entry.field->setCommand(oldCmdId);
        break;
      }
    }
  }

  QString fp = m_templateCombo->currentData().toString();
  QFile tmplFile(fp);
  tmplFile.open(QFile::ReadOnly);

  QUiLoader loader;
  QWidget* customWidget = loader.load(&tmplFile, 0);
  tmplFile.close();

  updateControls(customWidget);

  UiPreviewWidget* previewWidget =
      dynamic_cast<UiPreviewWidget*>(m_previewArea->widget());
  previewWidget->setUiPixmap(customWidget->grab());
  delete customWidget;
}

void CustomPanelEditorPopup::onPreviewClicked(int id) {
  CustomPanelUIField* field = m_uiEntries.at(id).field;
  if (!field) return;
  QString commandId = field->commandId();
  if (commandId.isEmpty()) return;
  QMimeData* mimeData = new QMimeData;
  mimeData->setText(commandId);

  QString dragPixmapTxt = field->text();
  QFontMetrics fm(QApplication::font());
  QPixmap pix(fm.boundingRect(dragPixmapTxt).adjusted(-2, -2, 2, 2).size());
  QPainter painter(&pix);
  painter.fillRect(pix.rect(), Qt::white);
  painter.setPen(Qt::black);
  painter.drawText(pix.rect(), Qt::AlignCenter, dragPixmapTxt);

  QDrag* drag = new QDrag(sender());
  drag->setMimeData(mimeData);
  drag->setPixmap(pix);

  drag->exec(Qt::MoveAction);
}

void CustomPanelEditorPopup::onPreviewDropped(int id, QString cmdId,
                                              bool fromTree) {
  CustomPanelUIField* field = m_uiEntries.at(id).field;
  if (!field) return;

  QString oldCommandId = field->commandId();
  if (field->setCommand(cmdId)) {
    if (fromTree)
      field->notifyCommandChanged(QString(), QString());
    else
      field->notifyCommandChanged(oldCommandId, cmdId);
  }
}

//-----------------------------------------------------------------------------

QList<int> CustomPanelEditorPopup::entryIdByObjName(const QString objName) {
  QList<int> ret;
  for (int i = 0; i < m_uiEntries.size(); i++) {
    if (m_uiEntries[i].objectName == objName) ret.append(i);
  }
  return ret;
}

void CustomPanelEditorPopup::replaceObjectNames(QDomElement& element) {
  QDomNode n = element.firstChild();
  while (!n.isNull()) {
    if (n.isElement()) {
      QDomElement e = n.toElement();
      // 自分自身をチェック
      if (e.tagName() == "widget" && e.hasAttribute("name")) {
        QString objName     = e.attribute("name");
        QList<int> entryIds = entryIdByObjName(objName);
        if (!entryIds.isEmpty()) {
          UiEntry entry = m_uiEntries.at(entryIds[0]);

          if (entry.type == Button)
            e.setAttribute("name", entry.field->commandId());
          else {  // Scroller
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
      // check recursively
      replaceObjectNames(e);
    }
    n = n.nextSibling();
  }
}

void CustomPanelEditorPopup::onRegister() {
  QString panelName = m_panelNameEdit->text();
  if (panelName.isEmpty()) {
    DVGui::warning(tr("Please input the panel name."));
    return;
  }
  // overwrite confirmation
  TFilePath customPanelPath =
      customPaneFolderPath() + TFilePath(panelName + ".ui");
  if (TSystem::doesExistFileOrLevel(customPanelPath)) {
    QString question =
        tr("The custom panel %1 already exists. Do you want to overwrite?")
            .arg(panelName);
    int ret = DVGui::MsgBox(question, tr("Overwrite"), tr("Cancel"), 0);
    if (ret == 0 || ret == 2) {
      return;
    }
  }

  // create folder if not exist
  if (!TSystem::touchParentDir(customPanelPath)) {
    DVGui::warning(tr("Failed to create folder."));
    return;
  }

  // base template file
  QDomDocument doc(panelName);
  QFile tmplFile(m_templateCombo->currentData().toString());
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

CustomPanelEditorPopup::CustomPanelEditorPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "CustomPanelEditorPopup") {
  setWindowTitle(tr("Custom Panel Editor"));

  m_commandListTree =
      new CommandListTree(tr("a control in the panel"), this, false);

  QLabel* commandItemListLabel = new QLabel(tr("Command List"), this);
  QFont f("Arial", 15, QFont::Bold);
  commandItemListLabel->setFont(f);

  QLineEdit* searchEdit = new QLineEdit(this);

  m_previewArea       = new UiPreviewArea(this);
  m_UiFieldsContainer = new QWidget(this);
  m_templateCombo     = new QComboBox(this);
  m_panelNameEdit     = new QLineEdit("My Custom Panel", this);

  QPushButton* registerButton = new QPushButton(tr("Register"), this);
  QPushButton* cancelButton   = new QPushButton(tr("Cancel"), this);

  m_previewArea->setStyleSheet("background-color: black;");

  beginHLayout();

  QVBoxLayout* leftLay = new QVBoxLayout();
  leftLay->setMargin(0);
  leftLay->setSpacing(10);
  {
    QHBoxLayout* templateLay = new QHBoxLayout();
    templateLay->setMargin(0);
    templateLay->setSpacing(5);
    {
      templateLay->addWidget(new QLabel(tr("Template:"), this), 0);
      templateLay->addWidget(m_templateCombo, 1, Qt::AlignLeft);
    }
    leftLay->addLayout(templateLay, 0);
    leftLay->addWidget(m_UiFieldsContainer, 0);
    leftLay->addWidget(m_previewArea, 1);
  }
  addLayout(leftLay);

  QVBoxLayout* rightLay = new QVBoxLayout();
  rightLay->setMargin(0);
  rightLay->setSpacing(10);
  {
    rightLay->addWidget(commandItemListLabel, 0);
    QHBoxLayout* searchLay = new QHBoxLayout();
    searchLay->setMargin(0);
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
  nameLay->setMargin(0);
  nameLay->setSpacing(3);
  {
    nameLay->addWidget(new QLabel(tr("Panel name:"), this), 0);
    nameLay->addWidget(m_panelNameEdit, 1);
  }
  m_buttonLayout->addLayout(nameLay, 0);
  m_buttonLayout->addWidget(registerButton, 0);
  m_buttonLayout->addSpacing(10);
  m_buttonLayout->addWidget(cancelButton, 0);

  bool ret = true;
  ret = ret && connect(cancelButton, SIGNAL(clicked()), this, SLOT(close()));
  ret = ret && connect(m_templateCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onTemplateSwitched()));
  ret = ret &&
        connect(registerButton, SIGNAL(clicked()), this, SLOT(onRegister()));
  ret = ret && connect(searchEdit, SIGNAL(textChanged(const QString&)), this,
                       SLOT(onSearchTextChanged(const QString&)));
  assert(ret);

  // load template
  bool ok = loadTemplateList();
  if (!ok) {
    // show some warning?
  }
}

//-----------------------------------------------------------------------------

void CustomPanelEditorPopup::onSearchTextChanged(const QString& text) {
  static bool busy = false;
  if (busy) return;
  busy = true;
  m_commandListTree->searchItems(text);
  busy = false;
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<CustomPanelEditorPopup> openCustomPanelEditorPopup(
    MI_CustomPanelEditor);