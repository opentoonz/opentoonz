#include "boardsettingspopup.h"

// Tnz6 includes
#include "tapp.h"
#include "filebrowser.h"

// TnzQt includes
#include "toonzqt/filefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/intfield.h"

// TnzLib includes
#include "toutputproperties.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/boardsettings.h"
#include "toonz/toonzfolders.h"

// TnzBase includes
#include "trasterfx.h"

// TnzCore includes
#include "tsystem.h"
#include "tlevel_io.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QRectF>
#include <QListWidget>
#include <QMap>
#include <QPixmap>
#include <QImageReader>
#include <QMouseEvent>

BoardItem* BoardSettingsPopup::currentBoardItem = nullptr;

namespace {
QMap<BoardItem::Type, QString> stringByItemType;

BoardItem* currentBoardItem() { return BoardSettingsPopup::currentBoardItem; }
void setCurrentBoardItem(BoardItem* item) {
  BoardSettingsPopup::currentBoardItem = item;
}

void editListWidgetItem(QListWidgetItem* listItem, BoardItem* item) {
  QString itemText = item->getName() + "\n(" +
                     stringByItemType.value(item->getType(), "Unknown type") +
                     ")";
  listItem->setText(itemText);

  if (item->getType() == BoardItem::Image) {
    ToonzScene* scene        = TApp::instance()->getCurrentScene()->getScene();
    TFilePath decodedImgPath = scene->decodeFilePath(item->getImgPath());
    QPixmap iconPm           = QPixmap::fromImage(
        QImage(decodedImgPath.getQString()).scaled(QSize(20, 20)));
    listItem->setIcon(QIcon(iconPm));
  } else {
    QPixmap iconPm(20, 20);
    iconPm.fill(item->getColor());
    listItem->setIcon(QIcon(iconPm));
  }
}
}

//=============================================================================

BoardView::BoardView(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
}

void BoardView::paintEvent(QPaintEvent* event) {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  QPainter p(this);

  p.fillRect(rect(), Qt::black);

  // Duration must be more than 0 to save the clapperboard.
  // Scene file can maintain compatibility with older versions if you set the
  // duration to 0.
  if (boardSettings->getDuration() == 0) {
    p.setPen(Qt::white);
    QFont font = p.font();
    font.setPixelSize(30);
    p.setFont(font);
    p.drawText(
        rect().adjusted(5, 5, -5, -5), Qt::AlignCenter | Qt::TextWordWrap,
        tr("Please set the duration more than 0 frame first, or the "
           "clapperboard settings will not be saved in the scene at all!"));

    p.setPen(QPen(Qt::red, 2));
    p.drawLine(5, 5, 150, 5);
    p.drawLine(5, 10, 150, 10);

    return;
  }

  if (!m_valid) {
    int shrinkX          = outProp->getRenderSettings().m_shrinkX,
        shrinkY          = outProp->getRenderSettings().m_shrinkY;
    TDimension frameSize = scene->getCurrentCamera()->getRes();
    TDimension cameraRes(frameSize.lx / shrinkX, frameSize.ly / shrinkY);

    m_boardImg = boardSettings->getBoardImage(cameraRes, shrinkX, scene);
    m_valid    = true;
  }

  p.drawImage(m_boardImgRect, m_boardImg);

  p.translate(m_boardImgRect.topLeft());

  p.setBrush(Qt::NoBrush);
  for (int i = 0; i < boardSettings->getItemCount(); i++) {
    BoardItem* item = &(boardSettings->getItem(i));
    if (item == currentBoardItem()) {
      p.setPen(QPen(QColor(255, 100, 0), 2));
    } else {
      p.setPen(QPen(QColor(64, 64, 255), 1, Qt::DashLine));
    }
    p.drawRect(item->getItemRect(m_boardImgRect.size().toSize()));
  }
}

void BoardView::resizeEvent(QResizeEvent* event) {
  QSize boardRes;
  if (m_valid) {
    boardRes = m_boardImg.size();
  } else {
    ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
    TOutputProperties* outProp = scene->getProperties()->getOutputProperties();
    int shrinkX                = outProp->getRenderSettings().m_shrinkX,
        shrinkY                = outProp->getRenderSettings().m_shrinkY;
    TDimension frameSize       = scene->getCurrentCamera()->getRes();
    boardRes = QSize(frameSize.lx / shrinkX, frameSize.ly / shrinkY);
  }

  float ratio = std::min((float)width() / (float)boardRes.width(),
                         (float)height() / (float)boardRes.height());
  QSizeF imgSize = QSizeF(boardRes) * ratio;
  QPointF imgTopLeft(((float)width() - imgSize.width()) * 0.5,
                     ((float)height() - imgSize.height()) * 0.5);

  m_boardImgRect = QRectF(imgTopLeft, imgSize);
}

void BoardView::mouseMoveEvent(QMouseEvent* event) {
  // mouse move - change drag item
  if (!(event->buttons() & Qt::LeftButton)) {
    if (!currentBoardItem()) {
      m_dragItem = None;
      setCursor(Qt::ArrowCursor);
      return;
    }

    QPointF posOnImg = QPointF(event->pos()) - m_boardImgRect.topLeft();

    QRectF itemRect =
        currentBoardItem()->getItemRect(m_boardImgRect.size().toSize());

    qreal distance = 10.0;

    if ((itemRect.topLeft() - posOnImg).manhattanLength() < distance) {
      m_dragItem = TopLeftCorner;
      setCursor(Qt::SizeFDiagCursor);
    } else if ((itemRect.topRight() - posOnImg).manhattanLength() < distance) {
      m_dragItem = TopRightCorner;
      setCursor(Qt::SizeBDiagCursor);
    } else if ((itemRect.bottomRight() - posOnImg).manhattanLength() <
               distance) {
      m_dragItem = BottomRightCorner;
      setCursor(Qt::SizeFDiagCursor);
    } else if ((itemRect.bottomLeft() - posOnImg).manhattanLength() <
               distance) {
      m_dragItem = BottomLeftCorner;
      setCursor(Qt::SizeBDiagCursor);
    } else if (std::abs(itemRect.top() - posOnImg.y()) < distance &&
               itemRect.left() < posOnImg.x() &&
               itemRect.right() > posOnImg.x()) {
      m_dragItem = TopEdge;
      setCursor(Qt::SizeVerCursor);
    } else if (std::abs(itemRect.right() - posOnImg.x()) < distance &&
               itemRect.top() < posOnImg.y() &&
               itemRect.bottom() > posOnImg.y()) {
      m_dragItem = RightEdge;
      setCursor(Qt::SizeHorCursor);
    } else if (std::abs(itemRect.bottom() - posOnImg.y()) < distance &&
               itemRect.left() < posOnImg.x() &&
               itemRect.right() > posOnImg.x()) {
      m_dragItem = BottomEdge;
      setCursor(Qt::SizeVerCursor);
    } else if (std::abs(itemRect.left() - posOnImg.x()) < distance &&
               itemRect.top() < posOnImg.y() &&
               itemRect.bottom() > posOnImg.y()) {
      m_dragItem = LeftEdge;
      setCursor(Qt::SizeHorCursor);
    } else if (itemRect.contains(posOnImg)) {
      m_dragItem = Translate;
      setCursor(Qt::SizeAllCursor);
    } else {
      m_dragItem = None;
      setCursor(Qt::ArrowCursor);
    }

    return;
  }

  // left mouse drag
  if (m_dragItem == None) return;

  if (m_dragStartItemRect.isNull()) return;

  QPointF posOnImg = QPointF(event->pos()) - m_boardImgRect.topLeft();
  QPointF ratioPos = QPointF(posOnImg.x() / m_boardImgRect.width(),
                             posOnImg.y() / m_boardImgRect.height());

  // with alt : center resize
  bool altPressed = event->modifiers() & Qt::AltModifier;
  // with shift : align to discrete position
  bool shiftPressed = event->modifiers() & Qt::ShiftModifier;

  auto adjVal = [&](double p) {
    double step = 0.05;
    if (!shiftPressed) return p;
    return std::round(p / step) * step;
  };
  auto adjPos = [&](QPointF p) {
    if (!shiftPressed) return p;
    return QPointF(adjVal(p.x()), adjVal(p.y()));
  };

  QRectF newRect = m_dragStartItemRect;
  QPointF d      = ratioPos - m_dragStartPos;
  switch (m_dragItem) {
  case Translate:
    newRect.translate(ratioPos - m_dragStartPos);
    if (shiftPressed) {
      newRect.setTopLeft(adjPos(newRect.topLeft()));
      newRect.setBottomRight(adjPos(newRect.bottomRight()));
    }
    break;

  case TopLeftCorner:
    newRect.setTopLeft(adjPos(newRect.topLeft() + d));
    if (altPressed) newRect.setBottomRight(adjPos(newRect.bottomRight() - d));
    break;

  case TopRightCorner:
    newRect.setTopRight(adjPos(newRect.topRight() + d));
    if (altPressed) newRect.setBottomLeft(adjPos(newRect.bottomLeft() - d));
    break;

  case BottomRightCorner:
    newRect.setBottomRight(adjPos(newRect.bottomRight() + d));
    if (altPressed) newRect.setTopLeft(adjPos(newRect.topLeft() - d));
    break;

  case BottomLeftCorner:
    newRect.setBottomLeft(adjPos(newRect.bottomLeft() + d));
    if (altPressed) newRect.setTopRight(adjPos(newRect.topRight() - d));
    break;

  case TopEdge:
    newRect.setTop(adjVal(newRect.top() + d.y()));
    if (altPressed) newRect.setBottom(adjVal(newRect.bottom() - d.y()));
    break;

  case RightEdge:
    newRect.setRight(adjVal(newRect.right() + d.x()));
    if (altPressed) newRect.setLeft(adjVal(newRect.left() - d.x()));
    break;

  case BottomEdge:
    newRect.setBottom(adjVal(newRect.bottom() + d.y()));
    if (altPressed) newRect.setTop(adjVal(newRect.top() - d.y()));
    break;

  case LeftEdge:
    newRect.setLeft(adjVal(newRect.left() + d.x()));
    if (altPressed) newRect.setRight(adjVal(newRect.right() - d.x()));
    break;
  default:
    break;
  }

  currentBoardItem()->setRatioRect(newRect.normalized());
  invalidate();
  update();
}

void BoardView::mousePressEvent(QMouseEvent* event) {
  // only the left button counts
  if (event->button() != Qt::LeftButton) return;
  // store the current item rect and mouse pos, relative to the image size
  m_dragStartItemRect = currentBoardItem()->getRatioRect();
  QPointF posOnImg    = QPointF(event->pos()) - m_boardImgRect.topLeft();
  m_dragStartPos      = QPointF(posOnImg.x() / m_boardImgRect.width(),
                           posOnImg.y() / m_boardImgRect.height());
}

void BoardView::mouseReleaseEvent(QMouseEvent* event) {
  m_dragStartItemRect = QRectF();
  m_dragStartPos      = QPointF();
}

//=============================================================================

ItemInfoView::ItemInfoView(QWidget* parent) : QStackedWidget(parent) {
  m_nameEdit        = new QLineEdit(this);
  m_maxFontSizeEdit = new DVGui::IntLineEdit(this, 1, 1);
  m_typeCombo       = new QComboBox(this);
  m_textEdit        = new QTextEdit(this);
  m_imgPathField    = new DVGui::FileField(this);
  m_fontCombo       = new QFontComboBox(this);
  m_boldButton =
      new QPushButton(QIcon(":Resources/bold_on.png"), tr("Bold"), this);
  m_italicButton =
      new QPushButton(QIcon(":Resources/italic_on.png"), tr("Italic"), this);
  m_fontColorField =
      new DVGui::ColorField(this, true, TPixel32(0, 0, 0, 255), 25, false, 54);
  m_imgARModeCombo = new QComboBox(this);

  m_fontPropBox = new QWidget(this);
  m_imgPropBox  = new QWidget(this);

  for (int i = 0; i < BoardItem::TypeCount; i++) {
    m_typeCombo->addItem(stringByItemType[(BoardItem::Type)i]);
  }

  m_textEdit->setAcceptRichText(false);
  m_textEdit->setStyleSheet(
      "background:white;\ncolor:black;\nborder:1 solid black;");

  m_boldButton->setIconSize(QSize(30, 30));
  m_boldButton->setFixedHeight(25);
  m_boldButton->setCheckable(true);
  m_italicButton->setIconSize(QSize(30, 30));
  m_italicButton->setFixedHeight(25);
  m_italicButton->setCheckable(true);

  QStringList filters;
  for (QByteArray& format : QImageReader::supportedImageFormats())
    filters += format;
  m_imgPathField->setFilters(filters);
  m_imgPathField->setFileMode(QFileDialog::ExistingFile);

  m_imgARModeCombo->addItem(tr("Ignore"), Qt::IgnoreAspectRatio);
  m_imgARModeCombo->addItem(tr("Keep"), Qt::KeepAspectRatio);

  //----- layout

  QGridLayout* mainLay = new QGridLayout();
  mainLay->setMargin(5);
  mainLay->setHorizontalSpacing(3);
  mainLay->setVerticalSpacing(10);
  {
    mainLay->addWidget(new QLabel(tr("Name:"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_nameEdit, 0, 1);

    mainLay->addWidget(new QLabel(tr("Type:"), this), 1, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_typeCombo, 1, 1);

    QVBoxLayout* extraInfoLay = new QVBoxLayout();
    extraInfoLay->setMargin(0);
    extraInfoLay->setSpacing(0);
    {
      extraInfoLay->addWidget(m_textEdit, 1);

      QGridLayout* imgPropLay = new QGridLayout();
      imgPropLay->setMargin(0);
      imgPropLay->setHorizontalSpacing(3);
      imgPropLay->setVerticalSpacing(10);
      {
        imgPropLay->addWidget(new QLabel(tr("Path:"), this), 0, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
        imgPropLay->addWidget(m_imgPathField, 0, 1, 1, 2);

        imgPropLay->addWidget(new QLabel(tr("Aspect Ratio:"), this), 1, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
        imgPropLay->addWidget(m_imgARModeCombo, 1, 1);
      }
      imgPropLay->setColumnStretch(0, 0);
      imgPropLay->setColumnStretch(1, 0);
      imgPropLay->setColumnStretch(2, 1);
      m_imgPropBox->setLayout(imgPropLay);
      extraInfoLay->addWidget(m_imgPropBox, 0);

      extraInfoLay->addSpacing(5);

      QGridLayout* fontPropLay = new QGridLayout();
      fontPropLay->setMargin(0);
      fontPropLay->setHorizontalSpacing(3);
      fontPropLay->setVerticalSpacing(10);
      {
        fontPropLay->addWidget(new QLabel(tr("Font:"), this), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        fontPropLay->addWidget(m_fontCombo, 0, 1, 1, 4);

        fontPropLay->addWidget(new QLabel(tr("Max Size:"), this), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        fontPropLay->addWidget(m_maxFontSizeEdit, 1, 1);

        fontPropLay->addWidget(m_boldButton, 1, 2);
        fontPropLay->addWidget(m_italicButton, 1, 3);

        fontPropLay->addWidget(m_fontColorField, 2, 0, 1, 5);
      }
      fontPropLay->setColumnStretch(0, 0);
      fontPropLay->setColumnStretch(1, 0);
      fontPropLay->setColumnStretch(2, 0);
      fontPropLay->setColumnStretch(3, 0);
      fontPropLay->setColumnStretch(4, 1);
      m_fontPropBox->setLayout(fontPropLay);
      extraInfoLay->addWidget(m_fontPropBox, 0);
    }
    mainLay->addLayout(extraInfoLay, 2, 0, 1, 2);
  }
  mainLay->setColumnStretch(0, 0);
  mainLay->setColumnStretch(1, 1);
  mainLay->setRowStretch(0, 0);
  mainLay->setRowStretch(1, 0);
  mainLay->setRowStretch(2, 1);

  QWidget* mainWidget = new QWidget(this);
  mainWidget->setLayout(mainLay);
  addWidget(mainWidget);

  addWidget(new QLabel(tr("No item selected."), this));

  bool ret = true;
  ret      = ret && connect(m_nameEdit, SIGNAL(editingFinished()), this,
                       SLOT(onNameEdited()));
  ret = ret && connect(m_maxFontSizeEdit, SIGNAL(editingFinished()), this,
                       SLOT(onMaxFontSizeEdited()));
  ret = ret && connect(m_typeCombo, SIGNAL(activated(int)), this,
                       SLOT(onTypeComboActivated(int)));
  ret = ret && connect(m_textEdit, SIGNAL(textChanged()), this,
                       SLOT(onFreeTextChanged()));
  ret = ret && connect(m_imgPathField, SIGNAL(pathChanged()), this,
                       SLOT(onImgPathChanged()));
  ret = ret && connect(m_fontCombo, SIGNAL(currentFontChanged(const QFont&)),
                       this, SLOT(onFontComboChanged(const QFont&)));
  ret = ret && connect(m_boldButton, SIGNAL(clicked(bool)), this,
                       SLOT(onBoldButtonClicked(bool)));
  ret = ret && connect(m_italicButton, SIGNAL(clicked(bool)), this,
                       SLOT(onItalicButtonClicked(bool)));
  ret = ret &&
        connect(m_fontColorField, SIGNAL(colorChanged(const TPixel32&, bool)),
                this, SLOT(onFontColorChanged(const TPixel32&, bool)));
  ret = ret && connect(m_imgARModeCombo, SIGNAL(activated(int)), this,
                       SLOT(onImgARModeComboActivated()));
  assert(ret);
}

// called on switching the current item
void ItemInfoView::setCurrentItem(int index) {
  if (index == -1) {
    if (!currentBoardItem()) return;
    setCurrentBoardItem(nullptr);
    setCurrentIndex(1);  // set to "no item seleceted" page.
    return;
  }

  setCurrentIndex(0);  // set to normal page.

  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  if (index >= boardSettings->getItemCount()) return;

  BoardItem* newItem = &(boardSettings->getItem(index));
  if (currentBoardItem() == newItem) return;

  setCurrentBoardItem(newItem);

  // sync
  m_nameEdit->setText(newItem->getName());
  m_maxFontSizeEdit->setValue(newItem->getMaximumFontSize());
  m_typeCombo->setCurrentIndex((int)newItem->getType());
  m_textEdit->setText(newItem->getFreeText());
  m_imgPathField->setPath(newItem->getImgPath().getQString());
  m_fontCombo->setCurrentFont(newItem->font());
  m_boldButton->setChecked(newItem->font().bold());
  m_italicButton->setChecked(newItem->font().italic());
  QColor col = newItem->getColor();
  m_fontColorField->setColor(
      TPixel32(col.red(), col.green(), col.blue(), col.alpha()));
  int ARModeIndex = m_imgARModeCombo->findData(newItem->getImgARMode());
  if (ARModeIndex >= 0) m_imgARModeCombo->setCurrentIndex(ARModeIndex);

  switch (newItem->getType()) {
  case BoardItem::FreeText:
    m_textEdit->show();
    m_imgPropBox->hide();
    m_fontPropBox->show();
    break;
  case BoardItem::Image:
    m_textEdit->hide();
    m_imgPropBox->show();
    m_fontPropBox->hide();
    break;
  default:
    m_textEdit->hide();
    m_imgPropBox->hide();
    m_fontPropBox->show();
    break;
  }
}

void ItemInfoView::onNameEdited() {
  assert(currentBoardItem());

  QString newName = m_nameEdit->text();

  if (newName.isEmpty()) {
    newName = tr("Item");
    m_nameEdit->setText(newName);
  }

  if (currentBoardItem()->getName() == newName) return;

  currentBoardItem()->setName(newName);

  emit itemPropertyChanged(true);
}

void ItemInfoView::onMaxFontSizeEdited() {
  assert(currentBoardItem());

  int maxFontSize = m_maxFontSizeEdit->getValue();

  if (currentBoardItem()->getMaximumFontSize() == maxFontSize) return;

  currentBoardItem()->setMaximumFontSize(maxFontSize);

  emit itemPropertyChanged(false);
}

void ItemInfoView::onTypeComboActivated(int) {
  assert(currentBoardItem());

  BoardItem::Type newType = (BoardItem::Type)m_typeCombo->currentIndex();

  if (currentBoardItem()->getType() == newType) return;

  currentBoardItem()->setType(newType);

  switch (newType) {
  case BoardItem::FreeText:
    m_textEdit->show();
    m_imgPropBox->hide();
    m_fontPropBox->show();
    break;
  case BoardItem::Image:
    m_textEdit->hide();
    m_imgPropBox->show();
    m_fontPropBox->hide();
    break;
  default:
    m_textEdit->hide();
    m_imgPropBox->hide();
    m_fontPropBox->show();
    break;
  }

  emit itemPropertyChanged(true);
}

void ItemInfoView::onFreeTextChanged() {
  assert(currentBoardItem());

  currentBoardItem()->setFreeText(m_textEdit->toPlainText());

  emit itemPropertyChanged(false);
}

void ItemInfoView::onImgPathChanged() {
  assert(currentBoardItem());
  TFilePath fp(m_imgPathField->getPath());
  if (fp.isLevelName()) {
    ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
    TLevelReaderP lr(scene->decodeFilePath(fp));
    TLevelP level;
    if (lr) level = lr->loadInfo();
    if (level.getPointer() && level->getTable()->size() > 0) {
      TFrameId firstFrame = level->begin()->first;
      fp                  = fp.withFrame(firstFrame);
      m_imgPathField->setPath(fp.getQString());
    }
  }
  currentBoardItem()->setImgPath(fp);

  emit itemPropertyChanged(true);
}

void ItemInfoView::onFontComboChanged(const QFont& newFont) {
  assert(currentBoardItem());

  currentBoardItem()->font().setFamily(newFont.family());

  emit itemPropertyChanged(false);
}

void ItemInfoView::onBoldButtonClicked(bool on) {
  assert(currentBoardItem());

  currentBoardItem()->font().setBold(on);

  emit itemPropertyChanged(false);
}

void ItemInfoView::onItalicButtonClicked(bool on) {
  assert(currentBoardItem());

  currentBoardItem()->font().setItalic(on);

  emit itemPropertyChanged(false);
}

void ItemInfoView::onFontColorChanged(const TPixel32& color, bool isDragging) {
  assert(currentBoardItem());

  if (isDragging) return;

  QColor newColor((int)color.r, (int)color.g, (int)color.b, (int)color.m);
  if (currentBoardItem()->getColor() == newColor) return;
  currentBoardItem()->setColor(newColor);

  emit itemPropertyChanged(true);
}

void ItemInfoView::onImgARModeComboActivated() {
  assert(currentBoardItem());

  currentBoardItem()->setImgARMode(
      (Qt::AspectRatioMode)m_imgARModeCombo->currentData().toInt());

  emit itemPropertyChanged(false);
}

//=============================================================================

ItemListView::ItemListView(QWidget* parent) : QWidget(parent) {
  QPushButton* newItemBtn =
      new QPushButton(QIcon(":Resources/plus.svg"), tr("Add"), this);
  m_deleteItemBtn =
      new QPushButton(QIcon(":Resources/delete_on.svg"), tr("Remove"), this);
  m_moveUpBtn =
      new QPushButton(QIcon(":Resources/up.svg"), tr("Move Up"), this);
  m_moveDownBtn =
      new QPushButton(QIcon(":Resources/down.svg"), tr("Move Down"), this);

  m_list = new QListWidget(this);

  QSize iconSize(20, 20);

  newItemBtn->setIconSize(iconSize);
  m_deleteItemBtn->setIconSize(iconSize);
  m_moveUpBtn->setIconSize(iconSize);
  m_moveDownBtn->setIconSize(iconSize);

  m_list->setIconSize(QSize(20, 20));
  m_list->setAlternatingRowColors(true);
  m_list->setMaximumWidth(225);

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(5);
  mainLay->setSpacing(5);
  {
    QVBoxLayout* buttonsLay = new QVBoxLayout();
    buttonsLay->setMargin(0);
    buttonsLay->setSpacing(3);
    {
      buttonsLay->addWidget(newItemBtn, 0);
      buttonsLay->addWidget(m_deleteItemBtn, 0);
      buttonsLay->addWidget(m_moveUpBtn, 0);
      buttonsLay->addWidget(m_moveDownBtn, 0);
      buttonsLay->addStretch(1);
    }
    mainLay->addLayout(buttonsLay, 0);

    mainLay->addWidget(m_list, 1);
  }
  setLayout(mainLay);

  bool ret = true;
  ret      = ret && connect(m_list, SIGNAL(currentRowChanged(int)), this,
                       SLOT(onCurrentItemSwitched(int)));
  ret = ret && connect(newItemBtn, SIGNAL(clicked(bool)), this,
                       SLOT(onNewItemButtonClicked()));
  ret = ret && connect(m_deleteItemBtn, SIGNAL(clicked(bool)), this,
                       SLOT(onDeleteItemButtonClicked()));
  ret = ret && connect(m_moveUpBtn, SIGNAL(clicked(bool)), this,
                       SLOT(onMoveUpButtonClicked()));
  ret = ret && connect(m_moveDownBtn, SIGNAL(clicked(bool)), this,
                       SLOT(onMoveDownButtonClicked()));
  assert(ret);
}

void ItemListView::initialize() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  // first, clear all the list items
  m_list->clear();

  if (boardSettings->getItemCount() == 0) return;

  for (int i = 0; i < boardSettings->getItemCount(); i++) {
    BoardItem* item           = &(boardSettings->getItem(i));
    QListWidgetItem* listItem = new QListWidgetItem(m_list);

    editListWidgetItem(listItem, item);
  }

  // this will update information view
  m_list->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
}

// called when the infoView is edited
void ItemListView::updateCurrentItem() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  int currentItemIndex = m_list->currentRow();

  if (currentItemIndex < 0 || currentItemIndex >= boardSettings->getItemCount())
    return;

  BoardItem* item = &(boardSettings->getItem(currentItemIndex));
  editListWidgetItem(m_list->item(currentItemIndex), item);
}

void ItemListView::onCurrentItemSwitched(int currentRow) {
  if (currentRow == -1) {
    m_moveUpBtn->setEnabled(false);
    m_moveDownBtn->setEnabled(false);
  } else {
    m_moveUpBtn->setEnabled(currentRow != 0);
    m_moveDownBtn->setEnabled(currentRow != m_list->count() - 1);
  }
  emit currentItemSwitched(currentRow);
}

void ItemListView::onNewItemButtonClicked() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  boardSettings->addNewItem();

  QListWidgetItem* listItem = new QListWidgetItem();
  BoardItem* item           = &(boardSettings->getItem(0));

  editListWidgetItem(listItem, item);

  m_list->insertItem(0, listItem);
  m_list->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);

  m_deleteItemBtn->setEnabled(true);

  emit itemAddedOrDeleted();
}

void ItemListView::onDeleteItemButtonClicked() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  int currentRow = m_list->currentRow();

  assert(currentRow != -1);

  boardSettings->removeItem(currentRow);

  if (m_list->count() == 1) {
    m_deleteItemBtn->setEnabled(false);
    m_list->setCurrentRow(-1, QItemSelectionModel::ClearAndSelect);
  } else if (currentRow == m_list->count() - 1)
    m_list->setCurrentRow(m_list->count() - 2,
                          QItemSelectionModel::ClearAndSelect);

  delete m_list->takeItem(currentRow);

  emit itemAddedOrDeleted();
}

void ItemListView::onMoveUpButtonClicked() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  int currentRow = m_list->currentRow();
  assert(currentRow > 0);

  boardSettings->swapItems(currentRow - 1, currentRow);

  m_list->insertItem(currentRow - 1, m_list->takeItem(currentRow));
  m_list->setCurrentRow(currentRow - 1);

  emit itemAddedOrDeleted();
}

void ItemListView::onMoveDownButtonClicked() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  int currentRow = m_list->currentRow();
  assert(currentRow < m_list->count() - 1);

  boardSettings->swapItems(currentRow, currentRow + 1);

  QListWidgetItem* item = m_list->takeItem(currentRow);
  m_list->insertItem(currentRow + 1, item);
  m_list->setCurrentRow(currentRow + 1);

  emit itemAddedOrDeleted();
}

//=============================================================================

BoardSettingsPopup::BoardSettingsPopup(QWidget* parent)
    : DVGui::Dialog(parent, true, false, "ClapperboardSettings") {
  setWindowTitle(tr("Clapperboard Settings"));

  initializeItemTypeString();

  m_boardView    = new BoardView(this);
  m_itemInfoView = new ItemInfoView(this);
  m_itemListView = new ItemListView(this);

  m_durationEdit = new DVGui::IntLineEdit(this, 1, 0);

  QPushButton* loadPresetBtn =
      new QPushButton(QIcon(":Resources/load_on.svg"), tr("Load Preset"), this);
  QPushButton* savePresetBtn = new QPushButton(QIcon(":Resources/save_on.svg"),
                                               tr("Save as Preset"), this);

  QPushButton* closeButton = new QPushButton(tr("Close"), this);

  //--- layout

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(10);
  {
    QVBoxLayout* leftLay = new QVBoxLayout();
    leftLay->setMargin(0);
    leftLay->setSpacing(0);
    {
      QHBoxLayout* leftTopLay = new QHBoxLayout();
      leftTopLay->setMargin(5);
      leftTopLay->setSpacing(3);
      {
        leftTopLay->addWidget(new QLabel(tr("Duration (frames):"), this), 0);
        leftTopLay->addWidget(m_durationEdit, 0);

        leftTopLay->addSpacing(10);

        leftTopLay->addWidget(loadPresetBtn, 0);
        leftTopLay->addWidget(savePresetBtn, 0);

        leftTopLay->addStretch(1);
      }
      leftLay->addLayout(leftTopLay, 0);

      leftLay->addWidget(m_boardView, 1);
    }
    mainLay->addLayout(leftLay, 1);

    QVBoxLayout* rightLay = new QVBoxLayout();
    rightLay->setMargin(0);
    rightLay->setSpacing(15);
    {
      rightLay->addWidget(m_itemInfoView, 0);
      rightLay->addWidget(m_itemListView, 1);
    }
    mainLay->addLayout(rightLay, 0);
  }
  m_topLayout->addLayout(mainLay, 1);

  addButtonBarWidget(closeButton);

  bool ret = true;
  ret = ret && connect(m_itemListView, SIGNAL(currentItemSwitched(int)), this,
                       SLOT(onCurrentItemSwitched(int)));
  ret = ret && connect(m_itemListView, SIGNAL(itemAddedOrDeleted()), this,
                       SLOT(onItemAddedOrDeleted()));
  ret = ret && connect(m_itemInfoView, SIGNAL(itemPropertyChanged(bool)), this,
                       SLOT(onItemPropertyChanged(bool)));
  ret = ret && connect(m_durationEdit, SIGNAL(editingFinished()), this,
                       SLOT(onDurationEdited()));
  ret = ret && connect(closeButton, SIGNAL(pressed()), this, SLOT(close()));
  ret = ret &&
        connect(loadPresetBtn, SIGNAL(pressed()), this, SLOT(onLoadPreset()));
  ret = ret &&
        connect(savePresetBtn, SIGNAL(pressed()), this, SLOT(onSavePreset()));

  assert(ret);
}

void BoardSettingsPopup::initialize() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  m_durationEdit->setValue(boardSettings->getDuration());

  m_itemListView->initialize();

  m_boardView->invalidate();
  m_boardView->update();
}

void BoardSettingsPopup::initializeItemTypeString() {
  if (!stringByItemType.isEmpty()) return;

  stringByItemType[BoardItem::FreeText]    = tr("Text");  // free text (m_text)
  stringByItemType[BoardItem::ProjectName] = tr("Project name");
  stringByItemType[BoardItem::SceneName]   = tr("Scene name");
  stringByItemType[BoardItem::Duration_Frame]    = tr("Duration : Frame");
  stringByItemType[BoardItem::Duration_SecFrame] = tr("Duration : Sec + Frame");
  stringByItemType[BoardItem::Duration_HHMMSSFF] = tr("Duration : HH:MM:SS:FF");
  stringByItemType[BoardItem::CurrentDate]       = tr("Current date");
  stringByItemType[BoardItem::CurrentDateTime]   = tr("Current date and time");
  stringByItemType[BoardItem::UserName]          = tr("User name");
  stringByItemType[BoardItem::ScenePath_Aliased] =
      tr("Scene location : Aliased path");
  stringByItemType[BoardItem::ScenePath_Full] =
      tr("Scene location : Full path");
  stringByItemType[BoardItem::MoviePath_Aliased] =
      tr("Output location : Aliased path");
  stringByItemType[BoardItem::MoviePath_Full] =
      tr("Output location : Full path");
  stringByItemType[BoardItem::Image] = tr("Image");  // m_imgPath
}

void BoardSettingsPopup::hideEvent(QHideEvent* event) {
  setCurrentBoardItem(nullptr);
}

// called on changing the current row of ItemListView
void BoardSettingsPopup::onCurrentItemSwitched(int index) {
  // updat Info
  m_itemInfoView->setCurrentItem(index);

  m_boardView->update();
}

void BoardSettingsPopup::onItemAddedOrDeleted() {
  m_boardView->invalidate();
  m_boardView->update();
}

// called on modifying the infoView
void BoardSettingsPopup::onItemPropertyChanged(bool updateListView) {
  m_boardView->invalidate();
  m_boardView->update();

  if (updateListView) m_itemListView->updateCurrentItem();
}

void BoardSettingsPopup::onDurationEdited() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* outProp   = scene->getProperties()->getOutputProperties();
  BoardSettings* boardSettings = outProp->getBoardSettings();

  boardSettings->setDuration(m_durationEdit->getValue());

  m_boardView->update();
}

void BoardSettingsPopup::onLoadPreset() {
  LoadBoardPresetFilePopup popup;

  TFilePath fp = popup.getPath();
  if (fp.isEmpty()) return;

  TIStream is(fp);
  if (!is) throw TException(fp.getWideString() + L": Can't open file");
  try {
    std::string tagName = "";
    if (!is.matchTag(tagName)) throw TException("Bad file format");
    if (tagName == "clapperboardSettingsPreset") {
      ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
      if (!scene) return;
      TOutputProperties* outProp =
          scene->getProperties()->getOutputProperties();
      BoardSettings* boardSettings = outProp->getBoardSettings();

      boardSettings->loadData(is);
    } else
      throw TException("Bad file format");
  } catch (const TSystemException& se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
  } catch (...) {
    DVGui::error(QObject::tr("Couldn't load %1").arg(fp.getQString()));
  }

  initialize();
}

void BoardSettingsPopup::onSavePreset() {
  SaveBoardPresetFilePopup popup;

  TFilePath fp = popup.getPath();
  if (fp.isEmpty()) return;

  try {
    TFileStatus fs(fp);
    if (fs.doesExist() && !fs.isWritable()) {
      throw TSystemException(
          fp, "The preset cannot be saved: it is a read only file.");
    }

    TOStream os(fp, false);
    if (!os.checkStatus()) throw TException("Could not open file");

    ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    TOutputProperties* outProp = scene->getProperties()->getOutputProperties();
    BoardSettings* boardSettings = outProp->getBoardSettings();

    os.openChild("clapperboardSettingsPreset");
    boardSettings->saveData(os, true);
    os.closeChild();

    bool status = os.checkStatus();
    if (!status) throw TException("Could not complete the save preset");

  } catch (const TSystemException& se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
  } catch (...) {
    DVGui::error(QObject::tr("Couldn't save %1").arg(fp.getQString()));
  }
}

//=============================================================================

SaveBoardPresetFilePopup::SaveBoardPresetFilePopup()
    : GenericSaveFilePopup(tr("Save Clapperboard Settings As Preset")) {
  m_browser->enableGlobalSelection(false);
  setFilterTypes(QStringList("clapperboard"));
}

void SaveBoardPresetFilePopup::showEvent(QShowEvent* e) {
  FileBrowserPopup::showEvent(e);
  setFolder(ToonzFolder::getLibraryFolder() + "clapperboards");
}

//=============================================================================

LoadBoardPresetFilePopup::LoadBoardPresetFilePopup()
    : GenericLoadFilePopup(tr("Load Clapperboard Settings Preset")) {
  m_browser->enableGlobalSelection(false);
  setFilterTypes(QStringList("clapperboard"));
}

void LoadBoardPresetFilePopup::showEvent(QShowEvent* e) {
  FileBrowserPopup::showEvent(e);
  setFolder(ToonzFolder::getLibraryFolder() + "clapperboards");
}