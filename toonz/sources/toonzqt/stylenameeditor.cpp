#include "toonzqt/stylenameeditor.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/palettecmd.h"
#include "toonz/tpalettehandle.h"

// TnzCore includes
#include "tsystem.h"
#include "tcommon.h"
#include "tcolorstyles.h"
#include "tpalette.h"

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QMessageBox>
#include <QMenu>
#include <QContextMenuEvent>
#include <QFrame>

//------------------------------------------------------------
namespace {
const int areaColCount[WORD_COLUMN_AMOUNT] = {2, 2, 1};

int indexToRow(int index, int columnId) {
  return index / areaColCount[columnId];
}
int indexToCol(int index, int columnId) {
  return index % areaColCount[columnId];
}
};  // namespace
//------------------------------------------------------------

NewWordDialog::NewWordDialog(QWidget* parent) {
  setModal(true);
  m_lineEdit             = new QLineEdit(this);
  QPushButton* okBtn     = new QPushButton(tr("OK"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  // layout
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(5);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(new QLabel(tr("Enter new word"), this), 0,
                       Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_lineEdit, 0);
    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setMargin(3);
    buttonsLay->setSpacing(20);
    {
      buttonsLay->addSpacing(1);
      buttonsLay->addWidget(okBtn, 0);
      buttonsLay->addWidget(cancelBtn, 0);
      buttonsLay->addSpacing(1);
    }
    mainLay->addLayout(buttonsLay);
  }
  setLayout(mainLay);

  // signal-slot connections
  bool ret = true;
  ret      = ret && connect(okBtn, SIGNAL(clicked(bool)), this, SLOT(accept()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked(bool)), this, SLOT(reject()));
}

//-------

QString NewWordDialog::getName() { return m_lineEdit->text(); }

//------------------------------------------------------------

WordButton::WordButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {
  setFixedHeight(23);
  setMinimumWidth(75);
  setObjectName("WordButton");
  setToolTip(text);

  bool ret = connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked()));
  assert(ret);
}

//-------

void WordButton::onClicked() { emit clicked(text()); }

//-------

void WordButton::onRemove() { emit removeWord(text()); }

//-------

void WordButton::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  QAction* removeAct = new QAction(tr("Remove %1").arg(text()), &menu);
  bool ret = connect(removeAct, SIGNAL(triggered()), this, SLOT(onRemove()));
  menu.addAction(removeAct);
  menu.exec(event->globalPos());
}

//------------------------------------------------------------

AddWordButton::AddWordButton(const int col, QWidget* parent)
    : WordButton(tr("New"), parent), m_column(col) {
  static QString columnLabel[WORD_COLUMN_AMOUNT] = {tr("Character"), tr("Part"),
                                                    tr("Suffix")};
  // setFixedSize(23, 23);
  setIcon(QIcon(":Resources/plus.svg"));
  setIconSize(QSize(16, 16));
  setToolTip(tr("Add New Word for %1").arg(columnLabel[col]));
}

//-------

void AddWordButton::onClicked() { emit clicked(m_column); }

//------------------------------------------------------------

//-------
// load word list from user's settings

void EasyInputArea::loadList() {
  TFilePath fp(ToonzFolder::getMyModuleDir() +
               TFilePath(styleNameEasyInputWordsFileName));
  if (!TFileStatus(fp).doesExist()) return;
  QSettings wordsSettings(toQString(fp), QSettings::IniFormat);
  for (int a = 0; a < WORD_COLUMN_AMOUNT; a++) {
    int size = wordsSettings.beginReadArray(QString::number(a));
    if (size == 0) continue;
    for (int i = 0; i < size; ++i) {
      wordsSettings.setArrayIndex(i);
      m_wordList[a].append(wordsSettings.value("word").toString());
    }
    wordsSettings.endArray();
  }
}

//-------
// save word list to user's settings

void EasyInputArea::saveList() {
  TFilePath fp(ToonzFolder::getMyModuleDir() +
               TFilePath(styleNameEasyInputWordsFileName));
  QSettings wordsSettings(toQString(fp), QSettings::IniFormat);
  wordsSettings.clear();
  for (int a = 0; a < WORD_COLUMN_AMOUNT; a++) {
    wordsSettings.beginWriteArray(QString::number(a));
    for (int i = 0; i < m_wordList[a].count(); ++i) {
      wordsSettings.setArrayIndex(i);
      wordsSettings.setValue("word", m_wordList[a].at(i));
    }
    wordsSettings.endArray();
  }
}

//------

void EasyInputArea::updatePanelSize(int columnId) {
  int itemCount = m_wordList[columnId].size() + 1;
  int rowCount  = tceil((double)itemCount / (double)areaColCount[columnId]);

  QWidget* widget = m_scrollArea[columnId]->widget();
  widget->setFixedSize(m_scrollArea[columnId]->width(), rowCount * 26 + 3);
}

//------

EasyInputArea::EasyInputArea(QWidget* parent) : QWidget(parent) {
  loadList();

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(3);
  for (int a = 0; a < WORD_COLUMN_AMOUNT; a++) {
    m_scrollArea[a] = new QScrollArea(this);
    m_scrollArea[a]->setObjectName("SolidLineFrame");

    QFrame* wordPanel       = new QFrame(this);
    QGridLayout* buttonsLay = new QGridLayout();
    buttonsLay->setMargin(3);
    buttonsLay->setSpacing(3);
    {
      int row = 0;
      int col = 0;
      // store word buttons
      for (int s = 0; s < m_wordList[a].size(); s++) {
        WordButton* button = new WordButton(m_wordList[a].at(s), this);
        button->setFocusPolicy(Qt::NoFocus);
        buttonsLay->addWidget(button, row, col);
        connect(button, SIGNAL(clicked(const QString&)), this,
                SIGNAL(wordClicked(const QString&)));
        connect(button, SIGNAL(removeWord(const QString&)), this,
                SLOT(onRemoveWord(const QString&)));
        col++;
        if (col == areaColCount[a]) {
          col = 0;
          row++;
        }
      }
      // add button
      AddWordButton* addWordButton = new AddWordButton(a, this);
      addWordButton->setFocusPolicy(Qt::NoFocus);
      buttonsLay->addWidget(addWordButton, row, col);
      connect(addWordButton, SIGNAL(clicked(const int)), this,
              SLOT(addWordButtonClicked(const int)));
    }
    for (int c = 0; c < areaColCount[a]; c++)
      buttonsLay->setColumnStretch(c, 1);
    wordPanel->setLayout(buttonsLay);
    m_wordLayout[a] = buttonsLay;

    m_scrollArea[a]->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea[a]->setMinimumWidth(areaColCount[a] * 78 + 3);
    m_scrollArea[a]->setWidget(wordPanel);
    mainLay->addWidget(m_scrollArea[a], areaColCount[a]);
  }
  setLayout(mainLay);
}

//-------

void EasyInputArea::addWordButtonClicked(const int columnId) {
  NewWordDialog dialog(this);
  if (dialog.exec() == QDialog::Rejected) return;

  QString word = dialog.getName();
  if (word.isEmpty()) return;

  bool found = false;
  for (int i = 0; i < WORD_COLUMN_AMOUNT; i++) {
    found = m_wordList[i].contains(word);
    if (found) break;
  }
  if (found) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("%1 is already registered").arg(word));
    return;
  }

  // Append word to the list
  m_wordList[columnId].append(word);
  // Append new WordButton
  WordButton* button = new WordButton(word, this);
  connect(button, SIGNAL(clicked(const QString&)), this,
          SIGNAL(wordClicked(const QString&)));
  connect(button, SIGNAL(removeWord(const QString&)), this,
          SLOT(onRemoveWord(const QString&)));
  button->setFocusPolicy(Qt::NoFocus);

  int wordCount   = m_wordList[columnId].count();
  int row         = indexToRow(wordCount - 1, columnId);
  int col         = indexToCol(wordCount - 1, columnId);
  QWidget* addBtn = m_wordLayout[columnId]->itemAtPosition(row, col)->widget();
  m_wordLayout[columnId]->addWidget(button, row, col);
  // Move add button to the next index
  col++;
  if (col == areaColCount[columnId]) {
    col = 0;
    row++;
  }
  m_wordLayout[columnId]->addWidget(addBtn, row, col);

  updatePanelSize(columnId);

  saveList();
}

//------------------------------------------------------------

void EasyInputArea::onRemoveWord(const QString& word) {
  int a = -1;
  int index;
  for (int i = 0; i < WORD_COLUMN_AMOUNT; i++) {
    index = m_wordList[i].indexOf(word);
    if (index >= 0) {
      a = i;
      break;
    }
  }
  if (a < 0 || a >= WORD_COLUMN_AMOUNT) return;

  // delete button
  int row                = indexToRow(index, a);
  int col                = indexToCol(index, a);
  WordButton* wordButton = qobject_cast<WordButton*>(
      m_wordLayout[a]->itemAtPosition(row, col)->widget());
  if (!wordButton) return;
  bool ret = true;
  ret = ret && disconnect(wordButton, SIGNAL(clicked(const QString&)), this,
                          SIGNAL(wordClicked(const QString&)));
  ret = ret && disconnect(wordButton, SIGNAL(removeWord(const QString&)), this,
                          SLOT(onRemoveWord(const QString&)));
  assert(ret);
  m_wordLayout[a]->removeWidget(wordButton);
  wordButton->deleteLater();
  // move the following buttons
  for (int i = index + 1; i <= m_wordList[a].count(); i++) {
    int row         = indexToRow(i, a);
    int col         = indexToCol(i, a);
    QWidget* button = m_wordLayout[a]->itemAtPosition(row, col)->widget();
    col--;
    if (col < 0) {
      row--;
      col = areaColCount[a] - 1;
    }
    m_wordLayout[a]->addWidget(button, row, col);
  }

  // remove word from the list
  m_wordList[a].removeAt(index);

  updatePanelSize(a);
}

//------------------------------------------------------------

void EasyInputArea::enterEvent(QEvent*) { emit mouseEnter(); }

//------------------------------------------------------------

void EasyInputArea::resizeEvent(QResizeEvent*) {
  for (int i = 0; i < WORD_COLUMN_AMOUNT; i++) {
    updatePanelSize(i);
  }
}

//------------------------------------------------------------

StyleNameEditor::StyleNameEditor(QWidget* parent)
    : Dialog(parent, false, false, "StyleNameEditor")
    , m_paletteHandle(0)
    , m_selectionStart(-1)
    , m_selectionLength(0) {
  QString columnLabel[WORD_COLUMN_AMOUNT] = {AddWordButton::tr("Character"),
                                             AddWordButton::tr("Part"),
                                             AddWordButton::tr("Suffix")};

  setWindowTitle(tr("Name Editor"));

  m_styleName                  = new QLineEdit(this);
  m_okButton                   = new QPushButton(tr("OK"), this);
  m_cancelButton               = new QPushButton(tr("Cancel"), this);
  m_applyButton                = new QPushButton(tr("Apply and Next"), this);
  EasyInputArea* easyInputArea = new EasyInputArea(this);

  QLabel* panelLabel[WORD_COLUMN_AMOUNT];
  for (int i = 0; i < WORD_COLUMN_AMOUNT; i++) {
    panelLabel[i] = new QLabel(columnLabel[i], this);
    panelLabel[i]->setStyleSheet("font-size: 10px; font: italic;");
  }

  setFocusProxy(m_styleName);

  m_styleName->setEnabled(false);
  m_okButton->setEnabled(false);
  m_okButton->setFocusPolicy(Qt::NoFocus);
  m_applyButton->setEnabled(false);
  m_applyButton->setFocusPolicy(Qt::NoFocus);
  m_cancelButton->setFocusPolicy(Qt::NoFocus);

  m_styleName->setObjectName("LargeSizedText");

  easyInputArea->setFocusPolicy(Qt::NoFocus);

  // QVBoxLayout* mainLayout = new QVBoxLayout();
  m_topLayout->setMargin(10);
  m_topLayout->setSpacing(5);
  {
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setMargin(0);
    inputLayout->setSpacing(3);
    {
      inputLayout->addWidget(new QLabel(tr("Style Name"), this), 0);
      inputLayout->addWidget(m_styleName, 1);
    }
    m_topLayout->addLayout(inputLayout, 0);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setMargin(0);
    buttonLayout->setSpacing(3);
    {
      buttonLayout->addWidget(m_okButton);
      buttonLayout->addWidget(m_applyButton);
      buttonLayout->addWidget(m_cancelButton);
    }
    m_topLayout->addLayout(buttonLayout, 0);

    m_topLayout->addSpacing(5);

    QHBoxLayout* labelLay = new QHBoxLayout();
    labelLay->setMargin(0);
    labelLay->setSpacing(3);
    {
      labelLay->addWidget(new QLabel(tr("Easy Inputs"), this), 1,
                          Qt::AlignLeft);
      for (int i = 0; i < WORD_COLUMN_AMOUNT; i++) {
        labelLay->addWidget(panelLabel[i],
                            (i == 0) ? areaColCount[i] - 1 : areaColCount[i],
                            Qt::AlignRight | Qt::AlignBottom);
      }
    }
    m_topLayout->addLayout(labelLay, 0);

    m_topLayout->addWidget(easyInputArea, 1);
  }

  bool ret = true;
  ret =
      ret && connect(m_okButton, SIGNAL(pressed()), this, SLOT(onOkPressed()));
  ret = ret && connect(m_cancelButton, SIGNAL(pressed()), this,
                       SLOT(onCancelPressed()));
  ret = ret &&
        connect(m_applyButton, SIGNAL(pressed()), this, SLOT(onApplyPressed()));
  ret = ret && connect(easyInputArea, SIGNAL(wordClicked(const QString&)), this,
                       SLOT(onWordClicked(const QString&)));
  ret = ret && connect(easyInputArea, SIGNAL(mouseEnter()), this,
                       SLOT(storeSelectionInfo()));
  assert(ret);
}

//-------
void StyleNameEditor::setPaletteHandle(TPaletteHandle* ph) {
  m_paletteHandle = ph;
  connect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this,
          SLOT(onStyleSwitched()));
  connect(m_paletteHandle, SIGNAL(paletteSwitched()), this,
          SLOT(onStyleSwitched()));
  m_styleName->setEnabled(true);
  m_okButton->setEnabled(true);
  m_applyButton->setEnabled(true);
}

//-------
void StyleNameEditor::showEvent(QShowEvent* e) {
  if (m_paletteHandle) {
    disconnect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this,
               SLOT(onStyleSwitched()));
    disconnect(m_paletteHandle, SIGNAL(paletteSwitched()), this,
               SLOT(onStyleSwitched()));
    connect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this,
            SLOT(onStyleSwitched()));
    connect(m_paletteHandle, SIGNAL(paletteSwitched()), this,
            SLOT(onStyleSwitched()));
  }
  // update view
  onStyleSwitched();
}

//-------disconnection
void StyleNameEditor::hideEvent(QHideEvent* e) {
  disconnect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this,
             SLOT(onStyleSwitched()));
  disconnect(m_paletteHandle, SIGNAL(paletteSwitched()), this,
             SLOT(onStyleSwitched()));
}

//-----update display when the current style is switched
void StyleNameEditor::onStyleSwitched() {
  if (!m_paletteHandle || !m_paletteHandle->getStyle()) return;

  std::wstring styleName = m_paletteHandle->getStyle()->getName();
  m_styleName->setText(QString::fromStdWString(styleName));
  m_styleName->selectAll();
  m_styleName->setFocus();

  int styleId = m_paletteHandle->getStyleIndex();
  setWindowTitle(tr("Name Editor: # %1").arg(styleId));
}

//-------
void StyleNameEditor::onOkPressed() {
  onApplyPressed();
  close();
}

//-------
void StyleNameEditor::onApplyPressed() {
  if (!m_paletteHandle || !m_paletteHandle->getStyle()) return;
  if (m_styleName->text() == "") return;

  std::wstring newName = m_styleName->text().toStdWString();

  PaletteCmd::renamePaletteStyle(m_paletteHandle, newName);

  // move the current style to the next
  TPalette* palette    = m_paletteHandle->getPalette();
  int currentIndex     = m_paletteHandle->getStyleIndex();
  TPalette::Page* page = palette->getStylePage(currentIndex);
  int indexInPage      = page->search(currentIndex);

  // If indexInPage is at the end of the page, then move to the top of the next
  // page
  if (indexInPage == page->getStyleCount() - 1) {
    int pageIndex = page->getIndex();
    // if the current page is the last one, then move to the first page
    while (1) {
      pageIndex++;
      if (pageIndex == palette->getPageCount()) pageIndex = 0;
      page = palette->getPage(pageIndex);
      if (page->getStyleCount() > 0) break;
    }
    currentIndex = page->getStyleId(0);
  } else
    currentIndex = page->getStyleId(indexInPage + 1);
  // change the current index
  m_paletteHandle->setStyleIndex(currentIndex);
}

//-------
void StyleNameEditor::onCancelPressed() { close(); }

//-------
// focus when the mouse enters
void StyleNameEditor::enterEvent(QEvent* e) {
  activateWindow();
  m_styleName->setFocus();
}

//-------
void StyleNameEditor::onWordClicked(const QString& word) {
  if (m_selectionLength != 0)
    m_styleName->setSelection(m_selectionStart, m_selectionLength);
  else
    m_styleName->setCursorPosition(m_selectionStart);

  m_styleName->insert(word);
  m_styleName->setFocus();

  storeSelectionInfo();
}

//-------
// remember the selection of m_stylename when mouse entered in EasyInputArea
void StyleNameEditor::storeSelectionInfo() {
  if (m_styleName->hasSelectedText()) {
    m_selectionStart  = m_styleName->selectionStart();
    m_selectionLength = m_styleName->selectedText().length();
  } else {
    m_selectionStart  = m_styleName->cursorPosition();
    m_selectionLength = 0;
  }
}