

#include "psdsettingspopup.h"

// Tnz6 includes
#include "tapp.h"

// TnzQt includes
#include "toonzqt/checkbox.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"

// TnzCore includes
#include "tconvert.h"

// Qt includes
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QSize>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QMainWindow>

using namespace DVGui;
#define REF_LAYER_BY_NAME

QStringList modesDescription;

// Per adesso non appare
// Costruisce la stringa delle info della psd da caricare che comparirà nel
// popup:
// Path, Dimensioni, numero di livelli, ecc..
static void doPSDInfo(TFilePath psdpath, QTreeWidget *psdTree) {
  psdTree->clear();
  try {
    TPSDReader *psdreader = new TPSDReader(psdpath);

    TPSDHeaderInfo header = psdreader->getPSDHeaderInfo();
    int width             = header.cols;
    int height            = header.rows;
    int depth             = header.depth;
    int channels          = header.channels;
    int layersCount       = header.layersCount;
    QString filename =
        QString::fromStdString(psdpath.getName() + psdpath.getDottedType());
    QString parentDir =
        QString::fromStdWString(psdpath.getParentDir().getWideString());
    QString dimension = QString::number(width) + "x" + QString::number(height);
    QList<QTreeWidgetItem *> items;
    items.append(new QTreeWidgetItem(
        (QTreeWidget *)0, QStringList(QString("Filename: %1").arg(filename))));
    items.append(new QTreeWidgetItem(
        (QTreeWidget *)0,
        QStringList(QString("Parent dir: %1").arg(parentDir))));
    items.append(new QTreeWidgetItem(
        (QTreeWidget *)0,
        QStringList(QString("Dimension: %1").arg(dimension))));
    items.append(new QTreeWidgetItem(
        (QTreeWidget *)0,
        QStringList(QString("Depth: %1").arg(QString::number(depth)))));
    items.append(new QTreeWidgetItem(
        (QTreeWidget *)0,
        QStringList(QString("Channels: %1").arg(QString::number(channels)))));
    QTreeWidgetItem *layersItem = new QTreeWidgetItem((QTreeWidget *)0);
    int count                   = 0;
    QList<QTreeWidgetItem *> layersItemChildren;
    layersItemChildren.append(layersItem);
    int scavenge = 0;
    for (int i = layersCount - 1; i >= 0; i--) {
      TPSDLayerInfo *li = psdreader->getLayerInfo(i);
      int width         = li->right - li->left;
      int height        = li->bottom - li->top;
      QString layerName = li->name;
      if (strcmp(li->name, "</Layer group>") == 0 ||
          strcmp(li->name, "</Layer set>") == 0) {
        scavenge--;
      } else if (li->section > 0 && li->section <= 3) {
        QTreeWidgetItem *child = new QTreeWidgetItem((QTreeWidget *)0);
        child->setText(0, layerName);
        layersItemChildren[scavenge]->addChild(child);
        layersItemChildren.append(child);
        scavenge++;
      } else if (width > 0 && height > 0) {
        if (scavenge >= 0) {
          layersItemChildren[scavenge]->addChild(new QTreeWidgetItem(
              (QTreeWidget *)0, QStringList(QString("%1").arg(layerName))));
          count++;
        }
      }
    }
    QString layerItemText =
        "Layers: " +
        QString::number(count);  //+" ("+QString::number(layersCount)+")";
    layersItem->setText(0, layerItemText);
    items.append(layersItem);

    psdTree->insertTopLevelItems(0, items);
  } catch (TImageException &e) {
    error(QString::fromStdString(::to_string(e.getMessage())));
    return;
  }
}

//=============================================================================
// PsdSettingsPopup
//-----------------------------------------------------------------------------

PsdSettingsPopup::PsdSettingsPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "PsdSettings")
    , m_mode(FLAT) {
  bool ret = true;

  setWindowTitle(tr("Load PSD File"));

  if (modesDescription.isEmpty()) {
    modesDescription
        << tr("Flatten visible document layers into a single image. Layer "
              "styles are maintained.")
        << tr("Load document layers as frames into a single xsheet column.")
        << tr("Load document layers as xhseet columns.");
  }

  m_filename = new QLabel(tr(""));
  m_filename->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_filename->setFixedHeight(WidgetHeight);
  m_parentDir = new QLabel(tr(""));
  m_parentDir->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_parentDir->setFixedHeight(WidgetHeight);
  QLabel *nmLbl = new QLabel(tr("Name:"));
  nmLbl->setObjectName("TitleTxtLabel");
  QLabel *ptLbl = new QLabel(tr("Path:"));
  ptLbl->setObjectName("TitleTxtLabel");
  QGridLayout *grid = new QGridLayout();
  grid->setColumnMinimumWidth(0, 65);
  grid->addWidget(nmLbl, 0, 0, Qt::AlignRight);
  grid->addWidget(m_filename, 0, 1, Qt::AlignLeft);
  grid->addWidget(ptLbl, 1, 0, Qt::AlignRight);
  grid->addWidget(m_parentDir, 1, 1, Qt::AlignLeft);
  QHBoxLayout *layinfo = new QHBoxLayout;
  layinfo->addLayout(grid);
  layinfo->addStretch();
  addLayout(layinfo, false);

  /*
          m_psdTree = new QTreeWidget();
          m_psdTree->setIconSize(QSize(21,17));
          m_psdTree->setColumnCount(1);
          m_psdTree->setMaximumHeight(7*WidgetHeight);
          m_psdTree->setHeaderLabel("PSD Info");
          addWidget(m_psdTree);			 */

  addSeparator();

  m_loadMode = new QComboBox();
  m_loadMode->addItem(tr("Single Image"), QString("Single Image"));
  m_loadMode->addItem(tr("Frames"), QString("Frames"));
  m_loadMode->addItem(tr("Columns"), QString("Columns"));
  m_loadMode->setFixedHeight(WidgetHeight);
  m_loadMode->setFixedWidth(120);

  m_modeDescription = new QTextEdit(modesDescription[0]);
  m_modeDescription->setFixedHeight(40);
  m_modeDescription->setMinimumWidth(250);
  m_modeDescription->setReadOnly(true);
  m_createSubXSheet = new CheckBox(tr("Expose in a Sub-xsheet"));
  m_createSubXSheet->setMaximumHeight(WidgetHeight);
  m_createSubXSheet->setEnabled(false);

  m_levelNameType = new QComboBox();
  QStringList types;
  types << tr("FileName#LayerName") << tr("LayerName");
  m_levelNameType->addItems(types);
  m_levelNameType->setFixedHeight(WidgetHeight);
  m_levelNameType->setEnabled(false);

  QLabel *modeLbl = new QLabel(tr("Load As:"));
  modeLbl->setObjectName("TitleTxtLabel");

  QLabel *levelNameLbl = new QLabel(tr("Level Name:"));
  levelNameLbl->setObjectName("TitleTxtLabel");

  QGridLayout *gridMode = new QGridLayout();
  gridMode->setColumnMinimumWidth(0, 65);
  gridMode->setMargin(0);
  gridMode->addWidget(modeLbl, 0, 0, Qt::AlignRight);
  gridMode->addWidget(m_loadMode, 0, 1, Qt::AlignLeft);
  gridMode->addWidget(m_modeDescription, 1, 1, Qt::AlignLeft);
  gridMode->addWidget(m_createSubXSheet, 2, 1, Qt::AlignLeft);
  gridMode->addWidget(levelNameLbl, 3, 0, Qt::AlignRight);
  gridMode->addWidget(m_levelNameType, 3, 1, Qt::AlignLeft);
  QHBoxLayout *modeLayout = new QHBoxLayout;
  modeLayout->addLayout(gridMode);
  modeLayout->addStretch();
  addLayout(modeLayout, false);

  addSeparator(tr("Group Option"));
  m_psdFolderOptions = new QButtonGroup(this);
  QList<QString> folderOptionsList;
  folderOptionsList << tr("Ignore groups");
  folderOptionsList << tr(
      "Expose layers in a group as columns in a sub-xsheet");
  folderOptionsList << tr("Expose layers in a group as frames in a column");

  QGridLayout *gridButton = new QGridLayout();
  gridButton->setColumnMinimumWidth(0, 70);
  int i;
  for (i = 0; i < folderOptionsList.count(); i++) {
    QRadioButton *radioButton = new QRadioButton(folderOptionsList.at(i));
    if (i == 0) radioButton->setChecked(true);

    radioButton->setMaximumHeight(WidgetHeight);
    if (m_mode != COLUMNS) {
      radioButton->setEnabled(false);
    }
    m_psdFolderOptions->addButton(radioButton);
    m_psdFolderOptions->setId(radioButton, i);
    gridButton->addWidget(radioButton, i, 1);
  }
  QHBoxLayout *folderOptLayout = new QHBoxLayout;
  folderOptLayout->addLayout(gridButton);
  folderOptLayout->addStretch();
  addLayout(folderOptLayout, false);

  ret = ret && connect(m_loadMode, SIGNAL(currentIndexChanged(const QString &)),
                       SLOT(onModeChanged()));
  assert(ret);
  ret = ret && connect(m_psdFolderOptions, SIGNAL(buttonClicked(int)), this,
                       SLOT(onFolderOptionChange(int)));
  assert(ret);
  m_okBtn     = new QPushButton(tr("OK"), this);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(onOk()));
  connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(close()));
  addButtonBarWidget(m_okBtn, m_cancelBtn);
}

//-----------------------------------------------------------------------------

void PsdSettingsPopup::setPath(const TFilePath &path) {
  m_path = path;
  // doPSDInfo(path,m_psdTree);
  QString filename =
      QString::fromStdString(path.getName());  //+psdpath.getDottedType());
  QString pathLbl =
      QString::fromStdWString(path.getParentDir().getWideString());
  m_filename->setText(filename);
  m_parentDir->setText(pathLbl);
}

void PsdSettingsPopup::onOk() {
  doPsdParser();
  accept();
}

bool PsdSettingsPopup::subxsheet() {
  return (m_createSubXSheet->isEnabled() && m_createSubXSheet->isChecked());
}

int PsdSettingsPopup::levelNameType() {
  return m_levelNameType->currentIndex();
}

void PsdSettingsPopup::onModeChanged() {
  QString mode = m_loadMode->currentData().toString();
  if (mode == "Single Image") {
    m_mode = FLAT;
    m_modeDescription->setText(modesDescription[0]);
    m_createSubXSheet->setEnabled(false);
    m_levelNameType->setEnabled(false);
    QList<QAbstractButton *> buttons = m_psdFolderOptions->buttons();
    while (!buttons.isEmpty()) {
      QAbstractButton *btn = buttons.takeFirst();
      btn->setEnabled(false);
    }
  } else if (mode == "Frames") {
    m_mode = FRAMES;
    m_modeDescription->setText(modesDescription[1]);
    m_createSubXSheet->setEnabled(false);
    m_levelNameType->setEnabled(false);
    QList<QAbstractButton *> buttons = m_psdFolderOptions->buttons();
    while (!buttons.isEmpty()) {
      QAbstractButton *btn = buttons.takeFirst();
      btn->setEnabled(false);
    }
  } else if (mode == "Columns") {
    if (m_psdFolderOptions->checkedId() == 2 ||
        m_psdFolderOptions->checkedId() == 1)
      m_mode = FOLDER;
    else
      m_mode = COLUMNS;
    m_modeDescription->setText(modesDescription[2]);
    m_createSubXSheet->setEnabled(true);
    m_levelNameType->setEnabled(true);
    QList<QAbstractButton *> buttons = m_psdFolderOptions->buttons();
    while (!buttons.isEmpty()) {
      QAbstractButton *btn = buttons.takeFirst();
      btn->setEnabled(true);
    }
  } else {
    assert(false);
  }
}

void PsdSettingsPopup::onFolderOptionChange(int id) {
  switch (id) {
  case 0:  // Ignore Folder
    m_mode = COLUMNS;
    break;
  case 1:  // Fodler as SUBXSHEET
    m_mode = FOLDER;
    break;
  case 2:  // Frames
    m_mode = FOLDER;
    break;
  default:
    assert(0);
  }
}
int PsdSettingsPopup::getFolderOption() {
  return m_psdFolderOptions->checkedId();
}

void PsdSettingsPopup::doPsdParser() {
  TFilePath psdpath =
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(m_path);
  std::string mode = "";
  switch (m_mode) {
  case FLAT: {
    break;
  }
  case FRAMES: {
    mode = "#frames";
    std::string name =
        psdpath.getName() + "#1" + mode + psdpath.getDottedType();
    psdpath = psdpath.getParentDir() + TFilePath(name);
    break;
  }
  case COLUMNS: {
    std::string name = psdpath.getName() + "#1" + psdpath.getDottedType();
    psdpath          = psdpath.getParentDir() + TFilePath(name);
    break;
  }
  case FOLDER: {
    mode = "#group";
    std::string name =
        psdpath.getName() + "#1" + mode + psdpath.getDottedType();
    psdpath = psdpath.getParentDir() + TFilePath(name);
    break;
  }
  default: {
    assert(false);
    return;
  }
  }
  try {
    m_psdparser = new TPSDParser(psdpath);
    m_psdLevelPaths.clear();
    for (int i = 0; i < m_psdparser->getLevelsCount(); i++) {
      int layerId      = m_psdparser->getLevelId(i);
      std::string name = m_path.getName();
      if (layerId > 0 && m_mode != FRAMES) {
        if (m_levelNameType->currentIndex() == 0)  // FileName#LevelName
          name += "#" + std::to_string(layerId);
        else  // LevelName
          name += "##" + std::to_string(layerId);
      }
      if (mode != "") name += mode;
      name += m_path.getDottedType();
      TFilePath psdpath = m_path.getParentDir() + TFilePath(name);
      m_psdLevelPaths.push_back(psdpath);
    }
  } catch (TImageException &e) {
    error(QString::fromStdString(::to_string(e.getMessage())));
    return;
  }
}
TFilePath PsdSettingsPopup::getPsdPath(int levelIndex) {
  assert(levelIndex >= 0 && levelIndex < m_psdLevelPaths.size());
  return m_psdLevelPaths[levelIndex];
}
TFilePath PsdSettingsPopup::getPsdFramePath(int levelIndex, int frameIndex) {
  int layerId      = m_psdparser->getLevelId(levelIndex);
  int frameId      = m_psdparser->getFrameId(layerId, frameIndex);
  std::string name = m_path.getName();
  if (frameId > 0) name += "#" + std::to_string(frameId);
  name += m_path.getDottedType();
  TFilePath psdpath = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->decodeFilePath(m_path)
                          .getParentDir() +
                      TFilePath(name);
  return psdpath;
}
int PsdSettingsPopup::getFramesCount(int levelIndex) {
  // assert(levelIndex>=0 && levelIndex<m_levels.size());
  // return m_levels[levelIndex].framesCount;
  int levelId = m_psdparser->getLevelId(levelIndex);
  return m_psdparser->getFramesCount(levelId);
}
bool PsdSettingsPopup::isFolder(int levelIndex) {
  // assert(levelIndex>=0 && levelIndex<m_levels.size());
  // return m_levels[levelIndex].isFolder;
  return m_psdparser->isFolder(levelIndex);
}
bool PsdSettingsPopup::isSubFolder(int levelIndex, int frameIndex) {
  return m_psdparser->isSubFolder(levelIndex, frameIndex);
}
int PsdSettingsPopup::getSubfolderLevelIndex(int psdLevelIndex,
                                             int frameIndex) {
  int levelId        = m_psdparser->getLevelId(psdLevelIndex);
  int frameId        = m_psdparser->getFrameId(levelId, frameIndex);
  int subFolderIndex = m_psdparser->getLevelIndexById(frameId);
  return subFolderIndex;
}

//-----------------------------------------------------------------------------

//=============================================================================

// OpenPopupCommandHandler<PsdSettingsPopup>
// openPsdSettingsPopup(MI_SceneSettings);
