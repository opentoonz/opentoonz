

#include "toonzqt/hexcolornames.h"

// TnzLib includes
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "tconvert.h"
#include "tfiletype.h"
#include "tsystem.h"
#include "tcolorstyles.h"
#include "tpixel.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"

// Qt includes
#include <QCoreApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QToolTip>
#include <QTabWidget>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringListModel>
#include <QScopedValueRollback>

using namespace DVGui;

//-----------------------------------------------------------------------------

#define COLORNAMES_FILE "colornames.txt"

QHash<QString, QString> HexColorNames::s_maincolornames;
QHash<QString, QString> HexColorNames::s_usercolornames;
QHash<QString, QString> HexColorNames::s_tempcolornames;

HexColorNames *HexColorNames::instance() {
  static HexColorNames _instance;
  return &_instance;
}

HexColorNames::HexColorNames() {}

//-----------------------------------------------------------------------------

bool HexColorNames::loadColorTableXML(QHash<QString, QString> &table,
                                      const TFilePath &fp) {
  table.clear();

  if (!TFileStatus(fp).doesExist()) {
    qWarning("Color names file not found: %s",
             fp.getQString().toUtf8().constData());
    return false;
  }

  TIStream is(fp);
  if (!is) {
    qWarning("Cannot read color names file: %s",
             fp.getQString().toUtf8().constData());
    return false;
  }

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "colors") {
    qWarning("Not a valid color names file: %s",
             fp.getQString().toUtf8().constData());
    return false;
  }

  bool success = true;
  while (!is.matchEndTag()) {
    if (!is.matchTag(tagName)) {
      qWarning("Expected tag in color names file");
      success = false;
      break;
    }

    if (tagName == "color") {
      QString name = QString::fromStdString(is.getTagAttribute("name"));
      std::string hexs;
      is >> hexs;
      QString hex = QString::fromStdString(hexs);

      if (!name.isEmpty() && !hex.isEmpty()) {
        table.insert(name.toLower(), hex);
      } else {
        qWarning("Empty name or hex value in color names file");
        success = false;
      }

      if (!is.matchEndTag()) {
        qWarning("Expected end tag </color> in color names file");
        success = false;
        break;
      }
    } else {
      qWarning("Unexpected tag <%s> in color names file", tagName.c_str());
      success = false;
      break;
    }
  }
  return success;
}

//-----------------------------------------------------------------------------

bool HexColorNames::saveColorTableXML(QHash<QString, QString> &table,
                                      const TFilePath &fp) {
  TOStream os(fp);
  if (!os) {
    qWarning("Cannot write to color names file: %s",
             fp.getQString().toUtf8().constData());
    return false;
  }

  os.openChild("colors");

  QHash<QString, QString>::const_iterator it;
  std::map<std::string, std::string> attrs;
  for (it = table.cbegin(); it != table.cend(); ++it) {
    std::string nameStd = it.key().toStdString();
    attrs.clear();
    attrs.insert({"name", nameStd});
    os.openChild("color", attrs);
    os << it.value();
    os.closeChild();
  }

  os.closeChild();
  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::parseHexInternal(const QString &text, TPixel &outPixel) {
  static QRegularExpression validHex("^[0-9a-fA-F]+$");
  if (!validHex.match(text).hasMatch()) return false;

  bool ok;
  uint parsedValue = text.toUInt(&ok, 16);
  if (!ok) return false;

  switch (text.length()) {
  case 8:  // #RRGGBBAA
    outPixel.r = (parsedValue >> 24) & 0xFF;
    outPixel.g = (parsedValue >> 16) & 0xFF;
    outPixel.b = (parsedValue >> 8) & 0xFF;
    outPixel.m = parsedValue & 0xFF;
    break;
  case 6:  // #RRGGBB
    outPixel.r = (parsedValue >> 16) & 0xFF;
    outPixel.g = (parsedValue >> 8) & 0xFF;
    outPixel.b = parsedValue & 0xFF;
    outPixel.m = 255;
    break;
  case 4:  // #RGBA
    outPixel.r = ((parsedValue >> 12) & 15) * 17;
    outPixel.g = ((parsedValue >> 8) & 15) * 17;
    outPixel.b = ((parsedValue >> 4) & 15) * 17;
    outPixel.m = (parsedValue & 15) * 17;
    break;
  case 3:  // #RGB
    outPixel.r = ((parsedValue >> 8) & 15) * 17;
    outPixel.g = ((parsedValue >> 4) & 15) * 17;
    outPixel.b = (parsedValue & 15) * 17;
    outPixel.m = 255;
    break;
  case 2:  // #VV (non-standard grayscale)
    outPixel.r = parsedValue & 0xFF;
    outPixel.g = outPixel.r;
    outPixel.b = outPixel.r;
    outPixel.m = 255;
    break;
  case 1:  // #V (non-standard grayscale)
    outPixel.r = (parsedValue & 15) * 17;
    outPixel.g = outPixel.r;
    outPixel.b = outPixel.r;
    outPixel.m = 255;
    break;
  default:
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::loadMainFile(bool reload) {
  TFilePath mainCTFp = TEnv::getConfigDir() + COLORNAMES_FILE;
  if (reload || s_maincolornames.isEmpty()) {
    return loadColorTableXML(s_maincolornames, mainCTFp);
  }
  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::hasUserFile() {
  TFilePath userCTFp = ToonzFolder::getMyModuleDir() + COLORNAMES_FILE;
  return TFileStatus(userCTFp).doesExist();
}

//-----------------------------------------------------------------------------

bool HexColorNames::loadUserFile(bool reload) {
  TFilePath userCTFp = ToonzFolder::getMyModuleDir() + COLORNAMES_FILE;
  if (TFileStatus(userCTFp).doesExist()) {
    if (reload || s_usercolornames.isEmpty()) {
      return loadColorTableXML(s_usercolornames, userCTFp);
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::loadTempFile(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) return false;
  s_tempcolornames.clear();
  return loadColorTableXML(s_tempcolornames, fp);
}

//-----------------------------------------------------------------------------

bool HexColorNames::saveUserFile() {
  TFilePath userCTFp = ToonzFolder::getMyModuleDir() + COLORNAMES_FILE;
  return saveColorTableXML(s_usercolornames, userCTFp);
}

//-----------------------------------------------------------------------------

bool HexColorNames::saveTempFile(const TFilePath &fp) {
  return saveColorTableXML(s_tempcolornames, fp);
}

//-----------------------------------------------------------------------------

void HexColorNames::clearUserEntries() { s_usercolornames.clear(); }

void HexColorNames::clearTempEntries() { s_tempcolornames.clear(); }

//-----------------------------------------------------------------------------

void HexColorNames::setUserEntry(const QString &name, const QString &hex) {
  if (!name.isEmpty() && !hex.isEmpty()) {
    s_usercolornames.insert(name.toLower(), hex);
  }
}

//-----------------------------------------------------------------------------

void HexColorNames::setTempEntry(const QString &name, const QString &hex) {
  if (!name.isEmpty() && !hex.isEmpty()) {
    s_tempcolornames.insert(name.toLower(), hex);
  }
}

//-----------------------------------------------------------------------------

bool HexColorNames::parseText(const QString &input, TPixel &outPixel) {
  QString text = input;
  text.remove(QRegularExpression("\\s"));

  if (text.isEmpty()) return false;

  QString lower = text.toLower();

  // First, try to find it as a NAME (user + main)
  auto it = s_usercolornames.constFind(lower);
  if (it == s_usercolornames.constEnd()) {
    it = s_maincolornames.constFind(lower);
  }
  if (it != s_maincolornames.constEnd()) {
    QString hexText = it.value();
    hexText.remove(QRegularExpression("\\s"));
    if (hexText.startsWith('#')) hexText.remove(0, 1);
    return parseHexInternal(hexText, outPixel);
  }

  // Only then try to interpret it as a pure hexadecimal
  if (text.startsWith('#')) {
    text.remove(0, 1);
  }
  static QRegularExpression hexOnly("^[0-9a-fA-F]{1,8}$");
  if (hexOnly.match(text).hasMatch()) {
    return parseHexInternal(text, outPixel);
  }

  return false;
}

//-----------------------------------------------------------------------------

bool HexColorNames::parseHex(const QString &input, TPixel &outPixel) {
  QString text = input;
  static QRegularExpression space("\\s");
  text.remove(space);

  if (text.startsWith(QLatin1Char('#'))) text.remove(0, 1);

  return !text.isEmpty() && parseHexInternal(text, outPixel);
}

//-----------------------------------------------------------------------------

QString HexColorNames::generateHex(TPixel pixel) {
  if (pixel.m == 255) {
    return QStringLiteral("#%1%2%3")
        .arg(pixel.r, 2, 16, QLatin1Char('0'))
        .arg(pixel.g, 2, 16, QLatin1Char('0'))
        .arg(pixel.b, 2, 16, QLatin1Char('0'))
        .toUpper();
  } else {
    return QStringLiteral("#%1%2%3%4")
        .arg(pixel.r, 2, 16, QLatin1Char('0'))
        .arg(pixel.g, 2, 16, QLatin1Char('0'))
        .arg(pixel.b, 2, 16, QLatin1Char('0'))
        .arg(pixel.m, 2, 16, QLatin1Char('0'))
        .toUpper();
  }
}

//*****************************************************************************
//  Hex line widget
//*****************************************************************************

TEnv::IntVar HexLineEditAutoComplete("HexLineEditAutoComplete", 1);

HexLineEdit::HexLineEdit(const QString &contents, QWidget *parent)
    : QLineEdit(contents, parent)
    , m_editing(false)
    , m_color(0, 0, 0)
    , m_completer(nullptr)
    , m_completerModel(new QStringListModel(this)) {
  HexColorNames::loadMainFile(false);
  HexColorNames::loadUserFile(false);

  if (HexLineEditAutoComplete != 0) onAutoCompleteChanged(true);

  connect(HexColorNames::instance(), &HexColorNames::autoCompleteChanged, this,
          &HexLineEdit::onAutoCompleteChanged);
  connect(HexColorNames::instance(), &HexColorNames::colorsChanged, this,
          &HexLineEdit::onColorsChanged);
}

//-----------------------------------------------------------------------------

void HexLineEdit::updateColor() {
  setText(HexColorNames::generateHex(m_color));
}

//-----------------------------------------------------------------------------

void HexLineEdit::setStyle(TColorStyle &style, int index) {
  setColor(style.getColorParamValue(index));
}

//-----------------------------------------------------------------------------

void HexLineEdit::setColor(TPixel color) {
  if (m_color != color) {
    m_color = color;
    if (isVisible()) updateColor();
  }
}

//-----------------------------------------------------------------------------

bool HexLineEdit::fromText(QString text) {
  bool res = HexColorNames::parseText(text, m_color);
  if (res) updateColor();
  return res;
}

//-----------------------------------------------------------------------------

bool HexLineEdit::fromHex(QString text) {
  bool res = HexColorNames::parseHex(text, m_color);
  if (res) updateColor();
  return res;
}

//-----------------------------------------------------------------------------

void HexLineEdit::mousePressEvent(QMouseEvent *event) {
  QLineEdit::mousePressEvent(event);
  // Make Ctrl key disable select all so the user can click a specific character
  // after a focus-in, this likely will fall into a hidden feature thought.
  bool ctrlDown = event->modifiers() & Qt::ControlModifier;
  if (!m_editing && !ctrlDown) selectAll();
  m_editing = true;
}

//-----------------------------------------------------------------------------

void HexLineEdit::focusOutEvent(QFocusEvent *event) {
  QLineEdit::focusOutEvent(event);
  if (!m_editing) {
    deselect();
  }
  m_editing = false;
}

//-----------------------------------------------------------------------------

void HexLineEdit::showEvent(QShowEvent *event) {
  QLineEdit::showEvent(event);
  updateColor();
}

//-----------------------------------------------------------------------------

void HexLineEdit::updateCompleterList() {
  QStringList wordList;

  for (auto it = HexColorNames::beginMain(); it != HexColorNames::endMain();
       ++it)
    wordList.append(it.key());

  for (auto it = HexColorNames::beginUser(); it != HexColorNames::endUser();
       ++it)
    wordList.append(it.key());

  m_completerModel->setStringList(wordList);
}

//-----------------------------------------------------------------------------

void HexLineEdit::onAutoCompleteChanged(bool enable) {
  if (!enable) {
    if (m_completer) {
      m_completer->deleteLater();
      m_completer = nullptr;
    }
    setCompleter(nullptr);
    return;
  }

  if (!m_completer) {
    m_completer = new QCompleter(m_completerModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(m_completer);
  }

  updateCompleterList();
}

//-----------------------------------------------------------------------------

void HexLineEdit::onColorsChanged() {
  if (m_completer && HexLineEditAutoComplete != 0) {
    updateCompleterList();
  }
}

//*****************************************************************************
//  Hex color names editor
//*****************************************************************************

HexColorNamesEditor::HexColorNamesEditor(QWidget *parent)
    : DVGui::Dialog(parent, true, false, "HexColorNamesEditor")
    , m_selectedItem(nullptr)
    , m_newEntry(false) {
  setWindowTitle(tr("Hex Color Names Editor"));
  setModal(false);

  QPushButton *okButton    = new QPushButton(tr("OK"), this);
  QPushButton *applyButton = new QPushButton(tr("Apply"), this);
  QPushButton *closeButton = new QPushButton(tr("Close"), this);

  m_unsColorButton = new QPushButton(tr("Unselect"));
  m_addColorButton = new QPushButton(tr("Add Color"));
  m_delColorButton = new QPushButton(tr("Delete Color"));
  m_hexLineEdit    = new HexLineEdit("", this);
  m_hexLineEdit->setObjectName("HexLineEdit");
  m_hexLineEdit->setFixedWidth(75);

  // Main default color names
  QGridLayout *mainLay = new QGridLayout();
  QWidget *mainTab     = new QWidget();
  mainTab->setLayout(mainLay);

  m_mainTreeWidget = new QTreeWidget();
  m_mainTreeWidget->setColumnCount(2);
  m_mainTreeWidget->setColumnWidth(0, 175);
  m_mainTreeWidget->setColumnWidth(1, 50);
  m_mainTreeWidget->setHeaderLabels(QStringList() << "Name" << "Hex value");
  mainLay->addWidget(m_mainTreeWidget, 0, 0);

  // User defined color names
  QGridLayout *userLay = new QGridLayout();
  QWidget *userTab     = new QWidget();
  userTab->setLayout(userLay);

  m_userTreeWidget = new QTreeWidget();
  m_userTreeWidget->setColumnCount(2);
  m_userTreeWidget->setColumnWidth(0, 175);
  m_userTreeWidget->setColumnWidth(1, 50);
  m_userTreeWidget->setHeaderLabels(QStringList() << "Name" << "Hex value");
  m_colorField = new ColorField(this, true);
  userLay->addWidget(m_userTreeWidget, 0, 0, 1, 4);
  userLay->addWidget(m_unsColorButton, 1, 0);
  userLay->addWidget(m_addColorButton, 1, 1);
  userLay->addWidget(m_delColorButton, 1, 2);
  userLay->addWidget(m_hexLineEdit, 1, 3);
  userLay->addWidget(m_colorField, 2, 0, 1, 4);

  // Set delegate
  m_userEditingDelegate = new HexColorNamesEditingDelegate(m_userTreeWidget);
  m_mainTreeWidget->setItemDelegate(m_userEditingDelegate);
  m_userTreeWidget->setItemDelegate(m_userEditingDelegate);
  populateMainList(false);

  // Tabs
  QTabWidget *tab = new QTabWidget();
  tab->addTab(userTab, tr("User Defined Colors"));
  tab->addTab(mainTab, tr("Default Main Colors"));
  tab->setObjectName("hexTabWidget");

  // Bottom widgets
  QHBoxLayout *bottomLay = new QHBoxLayout();
  m_autoCompleteCb       = new QCheckBox(tr("Enable Auto-Complete"));
  m_autoCompleteCb->setChecked(HexLineEditAutoComplete != 0);
  m_autoCompleteCb->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
  m_importButton = new QPushButton(tr("Import"));
  m_exportButton = new QPushButton(tr("Export"));
  bottomLay->setContentsMargins(8, 8, 8, 8);
  bottomLay->setSpacing(5);
  bottomLay->addWidget(m_autoCompleteCb);
  bottomLay->addWidget(m_importButton);
  bottomLay->addWidget(m_exportButton);

  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->addWidget(tab);
  m_topLayout->addLayout(bottomLay);

  addButtonBarWidget(okButton, applyButton, closeButton);

  connect(m_userEditingDelegate, &HexColorNamesEditingDelegate::editingStarted,
          this, &HexColorNamesEditor::onEditingStarted);
  connect(m_userEditingDelegate, &HexColorNamesEditingDelegate::editingFinished,
          this, &HexColorNamesEditor::onEditingFinished);
  connect(m_userEditingDelegate, &HexColorNamesEditingDelegate::editingClosed,
          this, &HexColorNamesEditor::onEditingClosed);

  connect(m_userTreeWidget, &QTreeWidget::currentItemChanged, this,
          &HexColorNamesEditor::onCurrentItemChanged);
  connect(m_colorField, &ColorField::colorChanged, this,
          &HexColorNamesEditor::onColorFieldChanged);
  connect(m_hexLineEdit, &HexLineEdit::editingFinished, this,
          &HexColorNamesEditor::onHexChanged);

  connect(m_unsColorButton, &QPushButton::pressed, this,
          &HexColorNamesEditor::onDeselect);
  connect(m_addColorButton, &QPushButton::pressed, this,
          &HexColorNamesEditor::onAddColor);
  connect(m_delColorButton, &QPushButton::pressed, this,
          &HexColorNamesEditor::onDelColor);
  connect(m_importButton, &QPushButton::pressed, this,
          &HexColorNamesEditor::onImport);
  connect(m_exportButton, &QPushButton::pressed, this,
          &HexColorNamesEditor::onExport);

  connect(okButton, &QPushButton::pressed, this, &HexColorNamesEditor::onOK);
  connect(applyButton, &QPushButton::pressed, this,
          &HexColorNamesEditor::onApply);
  connect(closeButton, &QPushButton::pressed, this, &QWidget::close);
}

//-----------------------------------------------------------------------------

QTreeWidgetItem *HexColorNamesEditor::addEntry(QTreeWidget *tree,
                                               const QString &name,
                                               const QString &hex,
                                               bool editable) {
  TPixel pixel(0, 0, 0);
  HexColorNames::parseHex(hex, pixel);

  QPixmap pixm(16, 16);
  pixm.fill(QColor(pixel.r, pixel.g, pixel.b, pixel.m));

  QTreeWidgetItem *treeItem = new QTreeWidgetItem(tree);
  treeItem->setText(0, name);
  treeItem->setIcon(1, QIcon(pixm));
  treeItem->setText(1, hex);

  if (!editable)
    treeItem->setFlags(treeItem->flags() & ~Qt::ItemIsSelectable);
  else
    treeItem->setFlags(treeItem->flags() | Qt::ItemIsEditable);

  return treeItem;
}

//-----------------------------------------------------------------------------

bool HexColorNamesEditor::updateUserHexEntry(QTreeWidgetItem *treeItem,
                                             const TPixel32 &color) {
  if (treeItem) {
    QPixmap pixm(16, 16);
    pixm.fill(QColor(color.r, color.g, color.b, color.m));

    treeItem->setIcon(1, QIcon(pixm));
    treeItem->setText(1, HexColorNames::generateHex(color));
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool HexColorNamesEditor::nameValid(const QString &name) {
  if (name.isEmpty()) return false;

  static QRegularExpression invalidChars("[\\\\#<>\"']");
  return !name.contains(invalidChars);
}

//-----------------------------------------------------------------------------

bool HexColorNamesEditor::nameExists(const QString &name,
                                     QTreeWidgetItem *self) {
  for (int i = 0; i < m_userTreeWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_userTreeWidget->topLevelItem(i);
    if (item == self) continue;
    if (name.compare(item->text(0), Qt::CaseInsensitive) == 0) return true;
  }

  for (auto it = HexColorNames::beginMain(); it != HexColorNames::endMain();
       ++it) {
    if (name.compare(it.key(), Qt::CaseInsensitive) == 0) return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::deselectItem(bool treeFocus) {
  if (m_newEntry) return;

  m_userTreeWidget->setCurrentItem(nullptr);
  m_selectedItem = nullptr;
  m_selectedColn = 0;

  if (treeFocus) m_userTreeWidget->setFocus();
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::deleteCurrentItem(bool deselect) {
  if (m_newEntry) return;

  QTreeWidgetItem *treeItem = m_userTreeWidget->currentItem();
  if (!treeItem) return;

  int row = m_userTreeWidget->indexOfTopLevelItem(treeItem);
  delete treeItem;

  m_selectedItem = nullptr;
  m_selectedColn = 0;

  if (deselect) {
    m_userTreeWidget->clearSelection();

    int count = m_userTreeWidget->topLevelItemCount();
    if (count > 0) {
      int newRow = (row >= count) ? count - 1 : row;
      m_userTreeWidget->setCurrentItem(m_userTreeWidget->topLevelItem(newRow));
    }
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::populateMainList(bool reload) {
  HexColorNames::loadMainFile(reload);

  m_mainTreeWidget->clear();

  for (auto it = HexColorNames::beginMain(); it != HexColorNames::endMain();
       ++it) {
    addEntry(m_mainTreeWidget, it.key(), it.value(), false);
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::populateUserList(bool reload) {
  HexColorNames::loadUserFile(reload);

  m_userTreeWidget->clear();

  for (auto it = HexColorNames::beginUser(); it != HexColorNames::endUser();
       ++it) {
    if (!nameExists(it.key(), nullptr))
      addEntry(m_userTreeWidget, it.key(), it.value(), true);
  }

  m_userTreeWidget->sortItems(0, Qt::AscendingOrder);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::showEvent(QShowEvent *e) {
  populateUserList(false);

  deselectItem(false);
  m_delColorButton->setEnabled(false);
  m_unsColorButton->setEnabled(false);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::keyPressEvent(QKeyEvent *event) {
  // Blocking dialog default actions is actually desirable
  // for example when user press Esc or Enter twice while
  // editing it might close the dialog by accident.

  if (!m_userTreeWidget->hasFocus()) return;

  switch (event->key()) {
  case Qt::Key_F5:
    populateMainList(true);
    populateUserList(true);
    m_mainTreeWidget->update();
    m_userTreeWidget->update();
    break;
  case Qt::Key_Escape:
    deselectItem(true);
    break;
  case Qt::Key_Delete:
    deleteCurrentItem(false);
    break;
  case Qt::Key_Insert:
    onAddColor();
    break;
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::focusInEvent(QFocusEvent *e) {
  QWidget::focusInEvent(e);
  qApp->installEventFilter(this);
}

//-----------------------------------------------------------------------------
void HexColorNamesEditor::focusOutEvent(QFocusEvent *e) {
  QWidget::focusOutEvent(e);
  qApp->removeEventFilter(this);
}

//-----------------------------------------------------------------------------

bool HexColorNamesEditor::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() == QEvent::Shortcut || e->type() == QEvent::ShortcutOverride) {
    e->accept();
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onCurrentItemChanged(QTreeWidgetItem *current,
                                               QTreeWidgetItem *previous) {
  m_selectedItem = current;
  m_delColorButton->setEnabled(current != nullptr);
  m_unsColorButton->setEnabled(current != nullptr);
  m_selectedColn = 0;

  if (!current) return;

  m_selectedName = current->text(0);
  m_selectedHex  = current->text(1);

  // Modify color field
  TPixel pixel(0, 0, 0);
  if (HexColorNames::parseHex(m_selectedHex, pixel)) {
    m_colorField->setColor(pixel);
    m_hexLineEdit->setColor(pixel);
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onEditingStarted(const QModelIndex &modelIndex) {
  QTreeWidgetItem *item =
      static_cast<QTreeWidgetItem *>(modelIndex.internalPointer());
  int column = modelIndex.column();
  onItemStarted(item, column);
}

void HexColorNamesEditor::onEditingFinished(const QModelIndex &modelIndex) {
  QTreeWidgetItem *item =
      static_cast<QTreeWidgetItem *>(modelIndex.internalPointer());
  int column = modelIndex.column();
  onItemFinished(item, column);
}

void HexColorNamesEditor::onEditingClosed() {
  onItemFinished(m_selectedItem, m_selectedColn);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onItemStarted(QTreeWidgetItem *item, int column) {
  if (!item) return;
  m_selectedName = item->text(0);
  m_selectedHex  = item->text(1);
  m_selectedColn = column;
  m_selectedItem = item;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onItemFinished(QTreeWidgetItem *item, int column) {
  if (m_processingItemFinished) return;

  QScopedValueRollback<bool> guard(m_processingItemFinished, true);

  if (!m_selectedItem || !item) return;

  m_delColorButton->setEnabled(true);
  m_unsColorButton->setEnabled(true);

  QString text = item->text(column);

  if (column == 0) {
    static QRegularExpression space("\\s");
    text.remove(space);
    text = text.toLower();

    if (text.isEmpty()) {
      QTreeWidget *tree = item->treeWidget();
      int index         = tree->indexOfTopLevelItem(item);

      m_selectedItem = nullptr;
      m_newEntry     = false;

      QMetaObject::invokeMethod(
          tree,
          [tree, index]() {
            if (index >= 0) delete tree->takeTopLevelItem(index);
          },
          Qt::QueuedConnection);

      return;
    }

    if (!nameValid(text)) {
      DVGui::warning(
          tr("Color name is not valid.\nThe following characters cannot be "
             "used: \\ # < > \" '"));
      item->setText(0, m_selectedName);
      return;
    }

    if (nameExists(text, item)) {
      DVGui::warning(
          tr("Color name already exists.\nPlease use another name."));
      item->setText(0, m_selectedName);
      return;
    }

    item->setText(0, text);
    m_userTreeWidget->sortItems(0, Qt::AscendingOrder);

  } else {
    // Edit Hex
    TPixel pixel;
    if (HexColorNames::parseHex(text, pixel)) {
      m_colorField->setColor(pixel);
      m_hexLineEdit->setColor(pixel);
      updateUserHexEntry(item, pixel);
    } else {
      item->setText(1, m_selectedHex);
      DVGui::warning(tr("Invalid hex color format."));
    }
  }

  m_newEntry = false;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onColorFieldChanged(const TPixel32 &color,
                                              bool isDragging) {
  QTreeWidgetItem *treeItem = m_userTreeWidget->currentItem();
  if (updateUserHexEntry(treeItem, color)) {
    m_userTreeWidget->setCurrentItem(treeItem);
  }

  m_hexLineEdit->setColor(color);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onHexChanged() {
  QTreeWidgetItem *treeItem = m_userTreeWidget->currentItem();
  if (m_hexLineEdit->fromText(m_hexLineEdit->text())) {
    TPixel pixel = m_hexLineEdit->getColor();
    updateUserHexEntry(treeItem, pixel);
    m_colorField->setColor(pixel);
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onAddColor() {
  if (m_newEntry) return;

  TPixel pixel              = m_colorField->getColor();
  QString hex               = HexColorNames::generateHex(pixel);
  QTreeWidgetItem *treeItem = addEntry(m_userTreeWidget, "", hex, true);

  m_userTreeWidget->setCurrentItem(treeItem);
  onItemStarted(treeItem, 0);
  m_newEntry = true;
  m_userTreeWidget->editItem(treeItem, 0);

  m_delColorButton->setEnabled(false);
  m_unsColorButton->setEnabled(false);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onDelColor() {
  if (m_newEntry) return;

  deleteCurrentItem(true);
  m_userTreeWidget->setFocus();
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onDeselect() { deselectItem(false); }

//-----------------------------------------------------------------------------
void HexColorNamesEditor::onImport() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Open Color Names"), QString(),
      tr("Text or XML (*.txt *.xml);;Text files (*.txt);;XML files (*.xml)"));

  if (fileName.isEmpty()) return;

  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Hex Color Names Import"),
      tr("Do you want to merge with existing entries?"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if (ret == QMessageBox::Cancel) return;

  try {
    if (!HexColorNames::loadTempFile(TFilePath(fileName))) {
      DVGui::warning(
          tr("Error importing color names XML:\nFile not found or invalid "
             "format"));
      return;
    }
  } catch (const TException &e) {
    DVGui::warning(
        tr("Error importing color names XML:\nThe file may be corrupt or have "
           "invalid format."));

    return;
  } catch (...) {
    DVGui::warning(tr("An unexpected error occurred while importing colors."));
    return;
  }

  if (ret == QMessageBox::No) {
    m_userTreeWidget->clear();
  }

  for (auto it = HexColorNames::beginTemp(); it != HexColorNames::endTemp();
       ++it) {
    if (!nameExists(it.key(), nullptr))
      addEntry(m_userTreeWidget, it.key(), it.value(), true);
  }

  HexColorNames::clearTempEntries();
  m_userTreeWidget->sortItems(0, Qt::AscendingOrder);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onExport() {
  QString fileName =
      QFileDialog::getSaveFileName(this, tr("Save Color Names"), QString(),
                                   tr("XML files (*.xml);;Text files (*.txt)"));

  if (fileName.isEmpty()) return;

  HexColorNames::clearTempEntries();
  for (int i = 0; i < m_userTreeWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_userTreeWidget->topLevelItem(i);
    HexColorNames::setTempEntry(item->text(0), item->text(1));
  }

  if (!HexColorNames::saveTempFile(TFilePath(fileName))) {
    DVGui::warning(tr("Error exporting color names XML"));
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onOK() {
  onApply();
  close();
}

void HexColorNamesEditor::onApply() {
  HexColorNames::clearUserEntries();
  for (int i = 0; i < m_userTreeWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_userTreeWidget->topLevelItem(i);
    HexColorNames::setUserEntry(item->text(0), item->text(1));
  }

  if (!HexColorNames::saveUserFile()) {
    DVGui::warning(tr("Failed to save user color names file."));
  }

  HexColorNames::instance()->emitChanged();

  bool oldAutoCompState = (HexLineEditAutoComplete != 0);
  bool newAutoCompState = m_autoCompleteCb->isChecked();
  if (oldAutoCompState != newAutoCompState) {
    HexLineEditAutoComplete = newAutoCompState ? 1 : 0;
    HexColorNames::instance()->emitAutoComplete(newAutoCompState);
  }
}
