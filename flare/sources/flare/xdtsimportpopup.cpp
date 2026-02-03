#include "xdtsimportpopup.h"

#include "tapp.h"
#include "tsystem.h"
#include "iocommand.h"
#include "toonzqt/filefield.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "convertpopup.h"
#include "tfiletype.h"

#include <QMainWindow>
#include <QTableView>
#include <QPushButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QCoreApplication>

using namespace DVGui;

namespace {
QIcon getColorChipIcon(TPixel32 color) {
  QPixmap pm(15, 15);
  pm.fill(QColor(color.r, color.g, color.b));
  return QIcon(pm);
}

bool matchSequencePattern(const TFilePath& path) {
  QRegularExpression pattern(
      R"(
  ^                           # Match the start of the string
  .*?                         # Optional prefix
  \d+                         # allow aFilePrefix<number>.ext
  \.                          # Match a dot (.)
  (png|jpg|jpeg|bmp|tga|tiff) # Image extensions
  $                           # Match the end of the string
)",
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::ExtendedPatternSyntaxOption);
  return pattern.match(QString::fromStdString(path.getLevelName())).hasMatch();
};

bool isSharingSameParam(QString name1, QString name2) {
  std::wstring str1 = name1.toStdWString();
  std::wstring str2 = name2.toStdWString();

  str1.erase(std::remove_if(str1.begin(), str1.end(), ::iswdigit), str1.end());
  str2.erase(std::remove_if(str2.begin(), str2.end(), ::iswdigit), str2.end());

  return str1 == str2;
}

void fetchSequence(
    std::vector<IoCmd::LoadResourceArguments::ResourceData>& rds) {
  for (size_t i = 0; i < rds.size(); ++i) {
    TFilePath currentPath = rds[i].m_path;
    if (!matchSequencePattern(currentPath)) continue;

    TFilePath thisFolder = currentPath.getParentDir();
    QDir dir(thisFolder.getQString());

    QString searchPattern =
        "*" + QString::fromStdString(currentPath.getDottedType());
    QStringList foundFiles =
        dir.entryList(QStringList(searchPattern), QDir::Files);
    if (foundFiles.isEmpty()) continue;
    QString thisFile = currentPath.withoutParentDir().getQString();
    for (const QString& foundFile : foundFiles) {
      if (thisFile == foundFile) continue;

      if (isSharingSameParam(thisFile, foundFile)) {
        IoCmd::LoadResourceArguments::ResourceData newData;
        newData.m_path = thisFolder + foundFile.toStdWString();
        rds.insert(rds.begin() + (++i), newData);
      }
    }
  }
}
}  // namespace

//=============================================================================

XDTSImportPopup::XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                                 TFilePath scenePath, bool isUextVersion,
                                 bool isSXF)
    : m_scene(scene)
    , m_isUext(isUextVersion)
    , DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "XDTSImport") {
  QString type = isSXF ? "SXF" : "XDTS";
  setWindowTitle(tr("Importing %1 file %2")
                     .arg(type)
                     .arg(QString::fromStdString(scenePath.getLevelName())));
  QPushButton* loadButton   = new QPushButton(tr("Load"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

  m_tick1Combo             = new QComboBox(this);
  m_tick2Combo             = new QComboBox(this);
  QList<QComboBox*> combos = {m_tick1Combo, m_tick2Combo};

  m_renameCheckBox = new QCheckBox("Rename Image Sequence", this);
  m_renameCheckBox->setChecked(true);
  m_convertCombo             = new QComboBox(this);
  QStringList convertOptions = {
      tr("Do not Convert"), tr("Convert Every Level with Settings Popup"),
      tr("Convert Unpainted Aliasing Raster(Inks Only)")};
  m_convertCombo->addItems(convertOptions);
  m_convertCombo->setCurrentIndex(0);

  // Only works for 3rd Convert Option: NAA Unpainted
  QWidget* convertNAAWidget = new QWidget(this);
  convertNAAWidget->setVisible(false);
  QHBoxLayout* convertNAALayout = new QHBoxLayout;
  m_paletteCheckBox             = new QCheckBox("Append Default Palette", this);
  m_paletteCheckBox->setChecked(true);
  m_paletteCheckBox->setToolTip(
      tr("When activated, styles of the default "
         "palette\n($TOONZSTUDIOPALETTE\\Global Palettes\\Default "
         "Palettes\\Cleanup_Palette.tpl) will \nbe "
         "appended to the palette after conversion"));
  QStringList dpiModes;
  dpiModes << tr("Image DPI") << tr("Current Camera DPI") << tr("Custom DPI");
  m_dpiMode = new QComboBox(this);
  m_dpiMode->addItems(dpiModes);
  m_dpiFld = new DVGui::DoubleLineEdit(this);
  m_dpiFld->setValue(120);
  m_dpiFld->setDisabled(true);
  connect(m_dpiMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [=](int index) {
            m_dpiFld->setEnabled(index == 2);  // Custom DPI
          });

  convertNAALayout->addWidget(m_paletteCheckBox);
  convertNAALayout->addStretch();
  convertNAALayout->addWidget(new QLabel(tr("DPI:")));
  convertNAALayout->addWidget(m_dpiMode);
  convertNAALayout->addWidget(m_dpiFld);
  convertNAAWidget->setLayout(convertNAALayout);
  connect(m_convertCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [=](int index) {
            convertNAAWidget->setVisible(index == 2);
            this->adjustSize();
          });

  if (m_isUext) {
    m_keyFrameCombo       = new QComboBox(this);
    m_referenceFrameCombo = new QComboBox(this);
    combos << m_keyFrameCombo << m_referenceFrameCombo;
  }

  QList<TSceneProperties::CellMark> marks = TApp::instance()
                                                ->getCurrentScene()
                                                ->getScene()
                                                ->getProperties()
                                                ->getCellMarks();
  for (auto combo : combos) {
    combo->addItem(tr("None"), -1);
    int curId = 0;
    for (auto mark : marks) {
      QString label = QString("%1: %2").arg(curId).arg(mark.name);
      combo->addItem(getColorChipIcon(mark.color), label, curId);
      curId++;
    }
  }
  m_tick1Combo->setCurrentIndex(m_tick1Combo->findData(6));
  m_tick2Combo->setCurrentIndex(m_tick2Combo->findData(8));
  if (m_isUext) {
    m_keyFrameCombo->setCurrentIndex(m_keyFrameCombo->findData(0));
    m_referenceFrameCombo->setCurrentIndex(m_referenceFrameCombo->findData(4));
  }

  QString description =
      tr("Please specify the level locations. Suggested paths "
         "are input in the fields with blue border.");

  m_topLayout->addWidget(new QLabel(description, this), 0);
  m_topLayout->addSpacing(15);

  QScrollArea* fieldsArea = new QScrollArea(this);
  fieldsArea->setWidgetResizable(true);

  QWidget* fieldsWidget = new QWidget(this);

  QGridLayout* fieldsLay = new QGridLayout();
  fieldsLay->setContentsMargins(0, 0, 0, 0);
  fieldsLay->setHorizontalSpacing(10);
  fieldsLay->setVerticalSpacing(10);
  fieldsLay->addWidget(new QLabel(tr("Level Name"), this), 0, 0,
                       Qt::AlignLeft | Qt::AlignVCenter);
  fieldsLay->addWidget(new QLabel(tr("Level Path"), this), 0, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);
  for (QString& levelName : levelNames) {
    int row = fieldsLay->rowCount();
    fieldsLay->addWidget(new QLabel(levelName, this), row, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
    FileField* fileField = new FileField(this);
    fieldsLay->addWidget(fileField, row, 1);
    m_fields.insert(levelName, fileField);
    fileField->setFileMode(QFileDialog::AnyFile);
    fileField->setObjectName("SuggestiveFileField");
    connect(fileField, SIGNAL(pathChanged()), this, SLOT(onPathChanged()));
  }
  fieldsLay->setColumnStretch(1, 1);
  fieldsLay->setRowStretch(fieldsLay->rowCount(), 1);

  fieldsWidget->setLayout(fieldsLay);
  fieldsArea->setWidget(fieldsWidget);
  m_topLayout->addWidget(fieldsArea, 1);

  // —ˆT Œ´‰æ^’†Š„ŽQl‚ÌƒRƒ}ƒ}[ƒN‚ÌƒŒƒCƒAƒEƒg‚©‚çI

  // cell mark area
  QGroupBox* cellMarkGroupBox = new QGroupBox(
      tr("Cell marks for %1 symbols").arg(isSXF ? "SXF" : "XDTS"));
  QGridLayout* markLay = new QGridLayout();
  markLay->setContentsMargins(10, 10, 10, 10);
  markLay->setVerticalSpacing(10);
  markLay->setHorizontalSpacing(5);
  {
    // 0x25CB = ¡ð, 0x25CF = ¡ñ
    QChar symbol1 = isSXF ? QChar(0x25CB) : QChar('0');
    QChar symbol2 = isSXF ? QChar(0x25CF) : QChar('*');
    markLay->addWidget(
        new QLabel(tr("Inbetween Symbol1 (%1):").arg(symbol1), this), 0, 0,
        Qt::AlignRight | Qt::AlignVCenter);
    markLay->addWidget(m_tick1Combo, 0, 1);
    markLay->addItem(new QSpacerItem(10, 1), 0, 2);
    markLay->addWidget(
        new QLabel(tr("Inbetween Symbol2 (%1):").arg(symbol2), this), 0, 3,
        Qt::AlignRight | Qt::AlignVCenter);
    markLay->addWidget(m_tick2Combo, 0, 4);

    if (m_isUext) {
      markLay->addWidget(new QLabel(QObject::tr("Keyframe Symbol:")), 1, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      markLay->addWidget(m_keyFrameCombo, 1, 1);
      markLay->addWidget(new QLabel(QObject::tr("Reference Frame Symbol:")), 1,
                         3, Qt::AlignRight | Qt::AlignVCenter);
      markLay->addWidget(m_referenceFrameCombo, 1, 4);
    }
  }
  cellMarkGroupBox->setLayout(markLay);

  m_topLayout->addWidget(cellMarkGroupBox, 0, Qt::AlignRight);

  QHBoxLayout* checkBoxLayout = new QHBoxLayout;
  checkBoxLayout->addWidget(
      new QLabel(QObject::tr("Convert Raster to TLV : ")));
  checkBoxLayout->addWidget(m_convertCombo);
  checkBoxLayout->addStretch();
  checkBoxLayout->addWidget(m_renameCheckBox);

  m_topLayout->addLayout(checkBoxLayout);
  m_topLayout->addWidget(convertNAAWidget);
  connect(loadButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(loadButton, cancelButton);
  this->adjustSize();
  updateSuggestions(scenePath.getQString());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::onPathChanged() {
  FileField* fileField = dynamic_cast<FileField*>(sender());
  if (!fileField) return;
  QString levelName = m_fields.key(fileField);
  // make the fields non-suggestive
  m_pathSuggestedLevels.removeAll(levelName);

  // if the path is specified under the sub-folder with the same name as the
  // level, then try to make suggestions from the parent folder of it
  TFilePath fp =
      m_scene->decodeFilePath(TFilePath(fileField->getPath())).getParentDir();
  if (QDir(fp.getQString()).dirName() == levelName)
    updateSuggestions(fp.getQString());

  updateSuggestions(fileField->getPath());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::updateSuggestions(const QString samplePath) {
  TFilePath fp(samplePath);
  fp = m_scene->decodeFilePath(fp).getParentDir();
  QDir suggestFolder(fp.getQString());
  QStringList filters;
  filters << "*.bmp"
          << "*.jpg"
          << "*.jpeg"
          << "*.nol"
          << "*.pic"
          << "*.pict"
          << "*.pct"
          << "*.png"
          << "*.rgb"
          << "*.sgi"
          << "*.tga"
          << "*.tif"
          << "*.tiff"
          << "*.tlv"
          << "*.pli"
          << "*.psd";
  suggestFolder.setNameFilters(filters);
  suggestFolder.setFilter(QDir::Files);
  TFilePathSet pathSet;
  try {
    TSystem::readDirectory(pathSet, suggestFolder, true);
  } catch (...) {
    return;
  }

  QMap<QString, FileField*>::iterator fieldsItr = m_fields.begin();
  bool suggested                                = false;
  while (fieldsItr != m_fields.end()) {
    QString levelName    = fieldsItr.key();
    FileField* fileField = fieldsItr.value();
    // check if the field can be filled with suggestion
    if (fileField->getPath().isEmpty() ||
        m_pathSuggestedLevels.contains(levelName)) {
      // input suggestion if there is a file with the same level name
      bool found = false;
      for (TFilePath path : pathSet) {
        if (path.getName() == levelName.toStdString()) {
          TFilePath codedPath = m_scene->codeFilePath(path);
          fileField->setPath(codedPath.getQString());
          if (!m_pathSuggestedLevels.contains(levelName))
            m_pathSuggestedLevels.append(levelName);
          found = true;
          break;
        }
      }
      // Not found in the current folder.
      // Then check if there is a sub-folder with the same name as the level
      // (like foo/A/A.tlv), as CSP exports levels like that.
      if (!found && suggestFolder.cd(levelName)) {
        TFilePathSet subPathSet;
        try {
          TSystem::readDirectory(subPathSet, suggestFolder, true);
        } catch (...) {
          return;
        }
        for (TFilePath path : subPathSet) {
          if (path.getName() == levelName.toStdString()) {
            suggested           = true;
            TFilePath codedPath = m_scene->codeFilePath(path);
            fileField->setPath(codedPath.getQString());
            if (!m_pathSuggestedLevels.contains(levelName))
              m_pathSuggestedLevels.append(levelName);
            break;
          }
        }
        // back to parent folder
        suggestFolder.cdUp();
      }
      if (found) suggested = true;
    }
    ++fieldsItr;
  }

  // fallBack search
  if (!suggested) updateSuggestions(fp);

  // Repaint fields:
  //   - Suggested fields: highlight with a blue border
  //   - Non-suggested fields:
  //       if empty, set their internal path to the scene's parent directory,
  //       but clear the text so the user does not see it
  fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    if (m_pathSuggestedLevels.contains(fieldsItr.key()))
      fieldsItr.value()->setStyleSheet(
          QString("#SuggestiveFileField "
                  "QLineEdit{border-color:#2255aa;border-width:2px;}"));
    else {
      fieldsItr.value()->setStyleSheet(QString(""));
      if (fieldsItr.value()->getPath().isEmpty()) {
        fieldsItr.value()->setPath(
            m_scene->getScenePath().getParentDir().getQString());
        fieldsItr.value()->getField()->clear();
      }
    }
    ++fieldsItr;
  }
}

//-----------------------------------------------------------------------------
// FALL BACK
void XDTSImportPopup::updateSuggestions(const TFilePath& path) {
  QMap<QString, FileField*>::iterator fieldsItr = m_fields.begin();
  QStringList filters;
  filters << "*.bmp"
          << "*.jpg"
          << "*.jpeg"
          << "*.nol"
          << "*.pic"
          << "*.pict"
          << "*.pct"
          << "*.png"
          << "*.rgb"
          << "*.sgi"
          << "*.tga"
          << "*.tif"
          << "*.tiff";
  bool suggestFolderFound = false;
  QDir suggestFolder(path.getQString());
  auto assignMatchingFiles = [&](QStringList& files) {
    QMap<QString, FileField*>::iterator it = m_fields.begin();
    while (it != m_fields.end()) {
      QString levelName    = it.key();
      FileField* fileField = it.value();
      if (fileField->getPath().isEmpty() ||
          m_pathSuggestedLevels.contains(levelName)) {
        QString filePattern = ".*" + levelName + ".*" + ".{3,4}$";
        int index           = files.indexOf(QRegExp(filePattern));
        if (index != -1) {
          suggestFolderFound = true;
          TFilePath foundPath(suggestFolder.filePath(files[index]));
          TFilePath codedPath = m_scene->codeFilePath(foundPath);
          fileField->setPath(codedPath.getQString());
          files.removeAt(index);
          m_pathSuggestedLevels.append(levelName);
        }
      }
      ++it;
    }
  };

  // Relative Paths
  QStringList relPaths = suggestFolder.entryList(
      filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
  assignMatchingFiles(relPaths);

  if (!suggestFolderFound) {
    relPaths.clear();
    QStringList subDirs = suggestFolder.entryList(QDir::Dirs | QDir::Readable |
                                                  QDir::NoDotAndDotDot);
    for (const QString& subDirName : subDirs) {
      QDir subDir = suggestFolder;
      if (!subDir.cd(subDirName)) continue;

      QStringList filesInSubDir = subDir.entryList(
          filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
      for (const QString& file : filesInSubDir) {
        relPaths.append(subDirName + "/" + file);
      }
    }
    assignMatchingFiles(relPaths);
  }
}

//-----------------------------------------------------------------------------

QString XDTSImportPopup::getLevelPath(QString levelName) {
  FileField* field = m_fields.value(levelName);
  if (!field) return QString();
  return field->getPath();
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::getMarkerIds(int& tick1Id, int& tick2Id, int& keyFrameId,
                                   int& referenceFrameId) {
  tick1Id = m_tick1Combo->currentData().toInt();
  tick2Id = m_tick2Combo->currentData().toInt();
  if (m_isUext) {
    keyFrameId       = m_keyFrameCombo->currentData().toInt();
    referenceFrameId = m_referenceFrameCombo->currentData().toInt();
  } else {
    keyFrameId       = -1;
    referenceFrameId = -1;
  }
}

void XDTSImportPopup::accept() {
  QDialog::accept();
  std::vector<IoCmd::LoadResourceArguments::ResourceData> rds;
  for (auto it = m_fields.constBegin(); it != m_fields.constEnd(); ++it) {
    auto name  = it.key();
    auto field = it.value();
  }
  IoCmd::LoadResourceArguments::ResourceData d;
  for (auto field : m_fields) rds.push_back(TFilePath(field->getPath()));
  fetchSequence(rds);
  if (m_renameCheckBox->isChecked()) IoCmd::renameResources(rds, false);
  if (int ret = m_convertCombo->currentIndex()) {
    if (ret == 1) {
      // Convert One by One
      ConvertPopup popup;
      for (auto& rd : rds) {
        popup.setWindowModality(Qt::ApplicationModal);
        TFileType::Type type = TFileType::getInfo(rd.m_path);
        if (!TFileType::isFullColor(type)) continue;
        popup.setFiles({rd.m_path});
        popup.show();
        popup.setFormat("tlv");
        popup.adjustSize();
        while (popup.isVisible() || popup.isConverting())
          QCoreApplication::processEvents(QEventLoop::AllEvents |
                                          QEventLoop::WaitForMoreEvents);
        TFilePath convertedPath = popup.getConvertedPath(rd.m_path);
        if (!convertedPath.isEmpty()) rd.m_path = convertedPath;
      }
    } else {
      // Convert NAA Unpainted
      int index  = m_dpiMode->currentIndex();
      double dpi = 0;
      if (index == 1) {
        TCamera* camera =
            TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
        if (camera) dpi = camera->getDpi().x;
      } else if (index == 2)
        dpi = m_dpiFld->getValue();
      IoCmd::convertNAARaster2TLV(
          rds, false, dpi,
          m_paletteCheckBox->isChecked());  // Only Generate Inks
    }
  }
  int i = 0;
  for (auto field : m_fields) field->setPath(rds[i++].m_path.getQString());
}