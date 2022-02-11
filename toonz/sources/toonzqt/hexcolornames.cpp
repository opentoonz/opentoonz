#include "toonzqt/hexcolornames.h"

// TnzLib includes
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "tconvert.h"
#include "tfiletype.h"
#include "tsystem.h"
#include "tcolorstyles.h"
#include "tpalette.h"
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

using namespace DVGui;

//-----------------------------------------------------------------------------

#define COLORNAMES_FILE "colornames.txt"

QMap<QString, QString> HexColorNames::s_maincolornames;
QMap<QString, QString> HexColorNames::s_usercolornames;
QMap<QString, QString> HexColorNames::s_tempcolornames;

HexColorNames *HexColorNames::instance() {
  static HexColorNames _instance;
  return &_instance;
}

HexColorNames::HexColorNames() {}

void HexColorNames::loadColorTableXML(QMap<QString, QString> &table,
                                    const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) throw TException("File not found");

  TIStream is(fp);
  if (!is) throw TException("Can't read color names");

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "colors")
    throw TException("Not a color names file");

  while (!is.matchEndTag()) {
    if (!is.matchTag(tagName)) throw TException("Expected tag");
    if (tagName == "color") {
      QString name, hex;
      name = QString::fromStdString(is.getTagAttribute("name"));
      std::string hexs;
      is >> hexs;
      hex = QString::fromStdString(hexs);
      if (name.size() != 0 && hex.size() != 0)
        table.insert(name.toLower(), hex);
      if (!is.matchEndTag()) throw TException("Expected end tag");
    } else
      throw TException("unexpected tag /" + tagName + "/");
  }
}

//-----------------------------------------------------------------------------

void HexColorNames::saveColorTableXML(QMap<QString, QString> &table,
                                      const TFilePath &fp) {
  TOStream os(fp);
  if (!os) throw TException("Can't write color names");

  os.openChild("colors");

  QMap<QString, QString>::const_iterator it;
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
}

//-----------------------------------------------------------------------------

bool HexColorNames::parseHexInternal(QString text, TPixel &outPixel) {
  bool ok;
  uint parsedValue = text.toUInt(&ok, 16);
  if (!ok) return false;

  switch (text.length()) {
  case 8:  // #RRGGBBAA
    outPixel.r = parsedValue >> 24;
    outPixel.g = parsedValue >> 16;
    outPixel.b = parsedValue >> 8;
    outPixel.m = parsedValue;
    break;
  case 6:  // #RRGGBB
    outPixel.r = parsedValue >> 16;
    outPixel.g = parsedValue >> 8;
    outPixel.b = parsedValue;
    outPixel.m = 255;
    break;
  case 4:  // #RGBA
    outPixel.r = (parsedValue >> 12) & 15;
    outPixel.r |= outPixel.r << 4;
    outPixel.g = (parsedValue >> 8) & 15;
    outPixel.g |= outPixel.g << 4;
    outPixel.b = (parsedValue >> 4) & 15;
    outPixel.b |= outPixel.b << 4;
    outPixel.m = parsedValue & 15;
    outPixel.m |= outPixel.m << 4;
    break;
  case 3:  // #RGB
    outPixel.r = (parsedValue >> 8) & 15;
    outPixel.r |= outPixel.r << 4;
    outPixel.g = (parsedValue >> 4) & 15;
    outPixel.g |= outPixel.g << 4;
    outPixel.b = parsedValue & 15;
    outPixel.b |= outPixel.b << 4;
    outPixel.m = 255;
    break;
  case 2:  // #VV (non-standard)
    outPixel.r = parsedValue;
    outPixel.g = outPixel.r;
    outPixel.b = outPixel.r;
    outPixel.m = 255;
    break;
  case 1:  // #V (non-standard)
    outPixel.r = parsedValue & 15;
    outPixel.r |= outPixel.r << 4;
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

  // Load main color names
  try {
    if (reload || s_maincolornames.size() == 0) {
      s_maincolornames.clear();
      loadColorTableXML(s_maincolornames, mainCTFp);
    }
  } catch (...) {
    return false;
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

  // Load user color names (if exists...)
  if (TFileStatus(userCTFp).doesExist()) {
    try {
      if (reload || s_usercolornames.size() == 0) {
        s_usercolornames.clear();
        loadColorTableXML(s_usercolornames, userCTFp);
      }
    } catch (...) {
      return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::loadTempFile(const TFilePath &fp) {
  if (TFileStatus(fp).doesExist()) {
    try {
      s_tempcolornames.clear();
      loadColorTableXML(s_tempcolornames, fp);
    } catch (...) {
      return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::saveUserFile() {
  TFilePath userCTFp = ToonzFolder::getMyModuleDir() + COLORNAMES_FILE;

  try {
    saveColorTableXML(s_usercolornames, userCTFp);
  } catch (...) {
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

bool HexColorNames::saveTempFile(const TFilePath &fp) {
  try {
    saveColorTableXML(s_tempcolornames, fp);
  } catch (...) {
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

void HexColorNames::clearUserEntries() { s_usercolornames.clear(); }

void HexColorNames::clearTempEntries() { s_tempcolornames.clear(); }

//-----------------------------------------------------------------------------

void HexColorNames::setUserEntry(const QString &name, const QString &hex) {
  s_usercolornames.insert(name, hex);
}

//-----------------------------------------------------------------------------

void HexColorNames::setTempEntry(const QString &name, const QString &hex) {
  s_tempcolornames.insert(name, hex);
}

//-----------------------------------------------------------------------------

bool HexColorNames::parseText(QString text, TPixel &outPixel) {
  static QRegExp space("\\s");
  text.remove(space);
  if (text.size() == 0) return false;
  if (text[0] == "#") {
    text.remove(0, 1);
    return parseHexInternal(text, outPixel);
  }
  text = text.toLower();  // table names are lowercase

  // Find color from tables, user takes priority
  QMap<QString, QString>::const_iterator it;
  it = s_usercolornames.constFind(text);
  if (it == s_usercolornames.constEnd()) {
    it = s_maincolornames.constFind(text);
    if (it == s_maincolornames.constEnd()) return false;
  }

  QString hexText = it.value();
  hexText.remove(space);
  if (hexText[0] == "#") {
    hexText.remove(0, 1);
    return parseHexInternal(hexText, outPixel);
  }
  return false;
}

//-----------------------------------------------------------------------------

bool HexColorNames::parseHex(QString text, TPixel &outPixel) {
  static QRegExp space("\\s");
  text.remove(space);
  if (text.size() == 0) return false;
  if (text[0] == "#") {
    text.remove(0, 1);
  }
  return parseHexInternal(text, outPixel);
}

//-----------------------------------------------------------------------------

QString HexColorNames::generateHex(TPixel pixel) {
  if (pixel.m == 255) {
    // Opaque, omit alpha
    return QString("#%1%2%3")
        .arg(pixel.r, 2, 16, QLatin1Char('0'))
        .arg(pixel.g, 2, 16, QLatin1Char('0'))
        .arg(pixel.b, 2, 16, QLatin1Char('0'))
        .toUpper();
  } else {
    return QString("#%1%2%3%4")
        .arg(pixel.r, 2, 16, QLatin1Char('0'))
        .arg(pixel.g, 2, 16, QLatin1Char('0'))
        .arg(pixel.b, 2, 16, QLatin1Char('0'))
        .arg(pixel.m, 2, 16, QLatin1Char('0'))
        .toUpper();
  }
}

//-----------------------------------------------------------------------------

//*****************************************************************************
//  Hex line widget
//*****************************************************************************

TEnv::IntVar HexLineEditAutoComplete("HexLineEditAutoComplete", 1);

HexLineEdit::HexLineEdit(const QString &contents, QWidget *parent)
    : QLineEdit(contents, parent)
    , m_editing(false)
    , m_color(0, 0, 0)
    , m_completer(nullptr) {
  HexColorNames::loadMainFile(false);
  HexColorNames::loadUserFile(false);

  if (HexLineEditAutoComplete != 0) onAutoCompleteChanged(true);

  bool ret = true;

  ret = ret &&
        connect(HexColorNames::instance(), SIGNAL(autoCompleteChanged(bool)),
                this, SLOT(onAutoCompleteChanged(bool)));
  ret = ret && connect(HexColorNames::instance(), SIGNAL(colorsChanged()), this,
          SLOT(onColorsChanged()));
  assert(ret);
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
  deselect();
  m_editing = false;
}

//-----------------------------------------------------------------------------

void HexLineEdit::showEvent(QShowEvent *event) {
  QLineEdit::showEvent(event);
  updateColor();
}

//-----------------------------------------------------------------------------

QCompleter *HexLineEdit::getCompleter() {
  QStringList autolist;

  // Build words list from all color names tables
  HexColorNames::iterator it;
  for (it = HexColorNames::beginMain(); it != HexColorNames::endMain(); ++it) {
    autolist.append(it.name());
  }
  for (it = HexColorNames::beginUser(); it != HexColorNames::endUser(); ++it) {
    autolist.append(it.name());
  }

  QCompleter *completer = new QCompleter(autolist);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  return completer;
}

//-----------------------------------------------------------------------------

void HexLineEdit::onAutoCompleteChanged(bool enable) {
  if (m_completer) {
    m_completer->deleteLater();
    setCompleter(nullptr);
    m_completer = nullptr;
  }
  if (enable) {
    m_completer = getCompleter();
    setCompleter(m_completer);
  }
}

void HexLineEdit::onColorsChanged() { onAutoCompleteChanged(true); }

//-----------------------------------------------------------------------------

//*****************************************************************************
//  Hex color names editor
//*****************************************************************************

HexColorNamesEditor::HexColorNamesEditor(QWidget *parent)
    : DVGui::Dialog(parent, true, false, "HexColorNamesEditor")
    , m_selectedItem(nullptr)
    , m_newEntry(false) {
  setWindowTitle(tr("Hex Color Names Editor"));
  setModal(false); // user may want to access main style editor and palettes

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
  QWidget *mainTab = new QWidget();
  mainTab->setLayout(mainLay);

  m_mainTreeWidget = new QTreeWidget();
  m_mainTreeWidget->setColumnCount(2);
  m_mainTreeWidget->setColumnWidth(0, 175);
  m_mainTreeWidget->setColumnWidth(1, 50);
  m_mainTreeWidget->setHeaderLabels(QStringList() << "Name"
                                                  << "Hex value");
  mainLay->addWidget(m_mainTreeWidget, 0, 0);

  // User defined color names
  QGridLayout *userLay = new QGridLayout();
  QWidget *userTab     = new QWidget();
  userTab->setLayout(userLay);

  m_userTreeWidget = new QTreeWidget();
  m_userTreeWidget->setColumnCount(2);
  m_userTreeWidget->setColumnWidth(0, 175);
  m_userTreeWidget->setColumnWidth(1, 50);
  m_userTreeWidget->setHeaderLabels(QStringList() << "Name"
                                                  << "Hex value");
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
  m_autoCompleteCb  = new QCheckBox(tr("Enable Auto-Complete"));
  m_autoCompleteCb->setChecked(HexLineEditAutoComplete != 0);
  m_autoCompleteCb->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
  m_importButton = new QPushButton(tr("Import"));
  m_exportButton = new QPushButton(tr("Export"));
  bottomLay->setMargin(8);
  bottomLay->setSpacing(5);
  bottomLay->addWidget(m_autoCompleteCb);
  bottomLay->addWidget(m_importButton);
  bottomLay->addWidget(m_exportButton);

  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->addWidget(tab);
  m_topLayout->addLayout(bottomLay);

  addButtonBarWidget(okButton, applyButton, closeButton);

  bool ret = true;

  ret = ret && connect(m_userEditingDelegate,
                       SIGNAL(editingStarted(const QModelIndex &)), this,
                       SLOT(onEditingStarted(const QModelIndex &)));
  ret = ret && connect(m_userEditingDelegate,
                       SIGNAL(editingFinished(const QModelIndex &)), this,
                       SLOT(onEditingFinished(const QModelIndex &)));
  ret = ret && connect(m_userEditingDelegate, SIGNAL(editingClosed()), this,
                       SLOT(onEditingClosed()));

  ret =
      ret &&
      connect(m_userTreeWidget,
              SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
              this,
              SLOT(onCurrentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
  ret =
      ret && connect(m_colorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
                     this, SLOT(onColorFieldChanged(const TPixel32 &, bool)));
  ret = ret && connect(m_hexLineEdit, SIGNAL(editingFinished()), this,
                       SLOT(onHexChanged()));

  ret = ret &&
        connect(m_unsColorButton, SIGNAL(pressed()), this, SLOT(onDeselect()));
  ret = ret &&
        connect(m_addColorButton, SIGNAL(pressed()), this, SLOT(onAddColor()));
  ret = ret &&
        connect(m_delColorButton, SIGNAL(pressed()), this, SLOT(onDelColor()));
  ret =
      ret && connect(m_importButton, SIGNAL(pressed()), this, SLOT(onImport()));
  ret =
      ret && connect(m_exportButton, SIGNAL(pressed()), this, SLOT(onExport()));

  ret = ret && connect(okButton, SIGNAL(pressed()), this, SLOT(onOK()));
  ret = ret && connect(applyButton, SIGNAL(pressed()), this, SLOT(onApply()));
  ret = ret && connect(closeButton, SIGNAL(pressed()), this, SLOT(close()));
  assert(ret);
}

//-----------------------------------------------------------------------------

QTreeWidgetItem *HexColorNamesEditor::addEntry(QTreeWidget *tree,
                                               const QString &name,
                                               const QString &hex,
                                               bool editable) {
  TPixel pixel = TPixel(0, 0, 0);
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

bool HexColorNamesEditor::nameValid(const QString& name) {
  if (name.isEmpty()) return false;
  return name.count(QRegExp("[\\\\#<>\"']")) == 0;
}

//-----------------------------------------------------------------------------

bool HexColorNamesEditor::nameExists(const QString &name,
                                     QTreeWidgetItem *self) {
  for (int i = 0; i < m_userTreeWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_userTreeWidget->topLevelItem(i);
    if (item == self) continue;
    if (name.compare(item->text(0), Qt::CaseInsensitive) == 0) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::deselectItem(bool treeFocus) {
  if (m_newEntry) return;

  m_userTreeWidget->setCurrentItem(nullptr);
  if (treeFocus) m_userTreeWidget->setFocus();
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::deleteCurrentItem(bool deselect) {
  if (m_newEntry) return;

  QTreeWidgetItem *treeItem = m_userTreeWidget->currentItem();
  if (treeItem) delete treeItem;
  m_selectedItem = nullptr;
  if (deselect) m_userTreeWidget->setCurrentItem(nullptr);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::populateMainList(bool reload) {
  HexColorNames::loadMainFile(reload);

  HexColorNames::iterator it;
  m_mainTreeWidget->clear();
  for (it = HexColorNames::beginMain(); it != HexColorNames::endMain(); ++it) {
    addEntry(m_mainTreeWidget, it.name(), it.value(), false);
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::populateUserList(bool reload) {
  HexColorNames::loadUserFile(reload);

  HexColorNames::iterator it;
  m_userTreeWidget->clear();
  for (it = HexColorNames::beginUser(); it != HexColorNames::endUser(); ++it) {
    if (!nameExists(it.name(), nullptr))
      addEntry(m_userTreeWidget, it.name(), it.value(), true);
  }

  m_userTreeWidget->sortItems(0, Qt::SortOrder::AscendingOrder);
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

//--------------------------------------------------------

void HexColorNamesEditor::focusOutEvent(QFocusEvent *e) {
  QWidget::focusOutEvent(e);
  qApp->removeEventFilter(this);
}

//-----------------------------------------------------------------------------

bool HexColorNamesEditor::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() == QEvent::Shortcut ||
      e->type() == QEvent::ShortcutOverride) {
      e->accept();
      return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onCurrentItemChanged(QTreeWidgetItem *current,
                                               QTreeWidgetItem *previous) {
  m_selectedItem = current;
  m_delColorButton->setEnabled(current);
  m_unsColorButton->setEnabled(current);

  if (!current) return;
  m_selectedName = current->text(0);
  m_selectedHex  = current->text(1);
  m_selectedColn = 0;

  // Modify color field
  TPixel pixel(0, 0, 0);
  if (HexColorNames::parseHex(m_selectedHex, pixel)) {
    m_colorField->setColor(pixel);
    m_hexLineEdit->setColor(pixel);
  }
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onEditingStarted(const QModelIndex &modelIndex) {
  QTreeWidgetItem *item = (QTreeWidgetItem *)modelIndex.internalPointer();
  int column            = modelIndex.column();
  onItemStarted(item, column);
}

void HexColorNamesEditor::onEditingFinished(const QModelIndex &modelIndex) {
  QTreeWidgetItem *item = (QTreeWidgetItem *)modelIndex.internalPointer();
  int column            = modelIndex.column();
  onItemFinished(item, column);
}

void HexColorNamesEditor::onEditingClosed() {
  onItemFinished(m_selectedItem, m_selectedColn);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onItemStarted(QTreeWidgetItem *item, int column) {
  m_selectedName = item->text(0);
  m_selectedHex  = item->text(1);
  m_selectedColn = 0;
  m_selectedItem = item;
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onItemFinished(QTreeWidgetItem *item, int column) {
  if (!m_selectedItem || !item) return;
  m_delColorButton->setEnabled(true);
  m_unsColorButton->setEnabled(true);

  try {
    if (m_selectedItem == item) {
      QString text = item->text(column);
      if (column == 0) {
        // Edit Name
        static QRegExp space("\\s");
        text.remove(space);
        text = text.toLower();
        if (text.isEmpty()) throw "";
        if (!nameValid(text))
          throw "Color name is not valid.\nFollowing characters can't be used: \\ # < > \" '";
        if (nameExists(text, item)) throw "Color name already exists.\nPlease use another name.";
        item->setText(0, text);
        m_userTreeWidget->sortItems(0, Qt::SortOrder::AscendingOrder);
      } else {
        // Edit Hex
        TPixel pixel;
        if (HexColorNames::parseHex(text, pixel)) {
          m_colorField->setColor(pixel);
          m_hexLineEdit->setColor(pixel);
          updateUserHexEntry(item, pixel);
        } else {
          item->setText(1, m_selectedHex);
        }
      }
    }
  } catch (const char *reason) {
    m_selectedItem = nullptr;
    if (m_selectedName.isEmpty()) {
      delete item;
    } else {
      item->setText(0, m_selectedName);
    }
    if (strlen(reason)) DVGui::warning(tr(reason));
  } catch (...) {
    m_selectedItem = nullptr;
    delete item;
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

  TPixel pixel = m_colorField->getColor();
  QString hex = HexColorNames::generateHex(pixel);
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
      this, tr("Hex Color Names Import"), tr("Do you want to merge with existing entries?"),
      QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No |
                                   QMessageBox::Cancel));
  if (ret == QMessageBox::Cancel) return;

  if (!HexColorNames::loadTempFile(TFilePath(fileName))) {
    DVGui::warning(tr("Error importing color names XML"));
  }
  HexColorNames::iterator it;
  if (ret == QMessageBox::No) m_userTreeWidget->clear();
  for (it = HexColorNames::beginTemp(); it != HexColorNames::endTemp(); ++it) {
    if (!nameExists(it.name(), nullptr))
      addEntry(m_userTreeWidget, it.name(), it.value(), true);
  }
  HexColorNames::clearTempEntries();
  m_userTreeWidget->sortItems(0, Qt::SortOrder::AscendingOrder);
}

//-----------------------------------------------------------------------------

void HexColorNamesEditor::onExport() {
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Color Names"), QString(),
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
};

void HexColorNamesEditor::onApply() {
  HexColorNames::clearUserEntries();
  for (int i = 0; i < m_userTreeWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_userTreeWidget->topLevelItem(i);
    HexColorNames::setUserEntry(item->text(0), item->text(1));
  }
  HexColorNames::saveUserFile();
  HexColorNames::instance()->emitChanged();

  bool oldAutoCompState = (HexLineEditAutoComplete != 0);
  bool newAutoCompState = m_autoCompleteCb->isChecked();
  if (oldAutoCompState != newAutoCompState) {
    HexLineEditAutoComplete = newAutoCompState ? 1 : 0;
    HexColorNames::instance()->emitAutoComplete(newAutoCompState);
  }
};

//-----------------------------------------------------------------------------
