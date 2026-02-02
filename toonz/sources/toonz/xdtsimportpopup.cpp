

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

// Helper function to create color chip icon for cell marks
QIcon createColorChipIcon(TPixel32 color) {
  QPixmap pixmap(15, 15);
  pixmap.fill(QColor(color.r, color.g, color.b));
  return QIcon(pixmap);
}

// Check if filename matches sequence pattern (digits before extension)
bool matchesSequencePattern(const TFilePath& path) {
  static const QRegularExpression pattern(
      R"(.*?\d+\.(png|jpg|jpeg|bmp|tga|tiff)$)",
      QRegularExpression::CaseInsensitiveOption);

  return pattern.match(QString::fromStdString(path.getLevelName())).hasMatch();
}

// Check if two filenames share same base name (without digits)
bool sharesSameBaseName(const QString& name1, const QString& name2) {
  std::wstring str1 = name1.toStdWString();
  std::wstring str2 = name2.toStdWString();

  // Remove digits from both strings
  str1.erase(std::remove_if(str1.begin(), str1.end(), ::iswdigit), str1.end());
  str2.erase(std::remove_if(str2.begin(), str2.end(), ::iswdigit), str2.end());

  return str1 == str2;
}

// Fetch sequence files and add them to resource data list
void fetchSequenceFiles(
    std::vector<IoCmd::LoadResourceArguments::ResourceData>& resources) {
  for (size_t i = 0; i < resources.size(); ++i) {
    const TFilePath currentPath = resources[i].m_path;
    if (!matchesSequencePattern(currentPath)) continue;

    const TFilePath parentDir = currentPath.getParentDir();
    QDir directory(parentDir.getQString());

    const QString searchPattern =
        "*" + QString::fromStdString(currentPath.getDottedType());
    const QStringList foundFiles =
        directory.entryList({searchPattern}, QDir::Files);

    if (foundFiles.isEmpty()) continue;

    const QString currentFile = currentPath.withoutParentDir().getQString();

    for (const QString& foundFile : foundFiles) {
      if (currentFile == foundFile) continue;

      if (sharesSameBaseName(currentFile, foundFile)) {
        IoCmd::LoadResourceArguments::ResourceData newData;
        newData.m_path = parentDir + foundFile.toStdWString();
        resources.insert(resources.begin() + (++i), newData);
      }
    }
  }
}

}  // namespace

//=============================================================================
// XDTSImportPopup implementation
//-----------------------------------------------------------------------------

XDTSImportPopup::XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                                 TFilePath scenePath, bool isUextVersion,
                                 bool isSXF)
    : m_scene(scene)
    , m_isUext(isUextVersion)
    , Dialog(TApp::instance()->getMainWindow(), true, false, "XDTSImport") {
  // Set dialog properties
  const QString fileType = isSXF ? "SXF" : "XDTS";
  setWindowTitle(
      tr("Importing %1 file %2")
          .arg(fileType, QString::fromStdString(scenePath.getLevelName())));

  // Create buttons
  auto loadButton   = new QPushButton(tr("Load"), this);
  auto cancelButton = new QPushButton(tr("Cancel"), this);

  // Create combo boxes for cell marks
  m_tick1Combo                 = new QComboBox(this);
  m_tick2Combo                 = new QComboBox(this);
  QList<QComboBox*> comboBoxes = {m_tick1Combo, m_tick2Combo};

  // Create rename checkbox
  m_renameCheckBox = new QCheckBox(tr("Rename Image Sequence"), this);
  m_renameCheckBox->setChecked(true);

  // Create conversion combo box
  m_convertCombo             = new QComboBox(this);
  QStringList convertOptions = {
      tr("Do not Convert"), tr("Convert Every Level with Settings Popup"),
      tr("Convert Unpainted Aliasing Raster(Inks Only)")};
  m_convertCombo->addItems(convertOptions);
  m_convertCombo->setCurrentIndex(0);

  // Create NAA conversion widget (only visible for 3rd option)
  auto convertNAAWidget = new QWidget(this);
  convertNAAWidget->setVisible(false);
  auto convertNAALayout = new QHBoxLayout;

  m_paletteCheckBox = new QCheckBox(tr("Append Default Palette"), this);
  m_paletteCheckBox->setChecked(true);
  m_paletteCheckBox->setToolTip(
      tr("When activated, styles of the default "
         "palette\n($TOONZSTUDIOPALETTE\\Global Palettes\\Default "
         "Palettes\\Cleanup_Palette.tpl) will \nbe "
         "appended to the palette after conversion"));

  QStringList dpiModes = {tr("Image DPI"), tr("Current Camera DPI"),
                          tr("Custom DPI")};

  m_dpiMode = new QComboBox(this);
  m_dpiMode->addItems(dpiModes);

  m_dpiFld = new DoubleLineEdit(this);
  m_dpiFld->setValue(120.0);
  m_dpiFld->setDisabled(true);

  // Connect DPI mode change
  connect(m_dpiMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            m_dpiFld->setEnabled(index == 2);  // Custom DPI
          });

  convertNAALayout->addWidget(m_paletteCheckBox);
  convertNAALayout->addStretch();
  convertNAALayout->addWidget(new QLabel(tr("DPI:")));
  convertNAALayout->addWidget(m_dpiMode);
  convertNAALayout->addWidget(m_dpiFld);
  convertNAAWidget->setLayout(convertNAALayout);

  // Connect conversion mode change
  connect(m_convertCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [=](int index) {
            convertNAAWidget->setVisible(index == 2);
            adjustSize();
          });

  // Create additional combo boxes for UEXT version
  if (m_isUext) {
    m_keyFrameCombo       = new QComboBox(this);
    m_referenceFrameCombo = new QComboBox(this);
    comboBoxes << m_keyFrameCombo << m_referenceFrameCombo;
  }

  // Populate combo boxes with cell marks
  const auto cellMarks = TApp::instance()
                             ->getCurrentScene()
                             ->getScene()
                             ->getProperties()
                             ->getCellMarks();

  for (auto comboBox : comboBoxes) {
    comboBox->addItem(tr("None"), -1);
    int currentId = 0;

    for (const auto& mark : cellMarks) {
      const QString label = QString("%1: %2").arg(currentId).arg(mark.name);
      comboBox->addItem(createColorChipIcon(mark.color), label, currentId);
      ++currentId;
    }
  }

  // Set default selections
  m_tick1Combo->setCurrentIndex(m_tick1Combo->findData(6));
  m_tick2Combo->setCurrentIndex(m_tick2Combo->findData(8));

  if (m_isUext) {
    m_keyFrameCombo->setCurrentIndex(m_keyFrameCombo->findData(0));
    m_referenceFrameCombo->setCurrentIndex(m_referenceFrameCombo->findData(4));
  }

  // Create description label
  const QString description =
      tr("Please specify the level locations. Suggested paths "
         "are input in the fields with blue border.");

  m_topLayout->addWidget(new QLabel(description, this));
  m_topLayout->addSpacing(15);

  // Create scroll area for file fields
  auto fieldsArea = new QScrollArea(this);
  fieldsArea->setWidgetResizable(true);

  auto fieldsWidget = new QWidget(this);
  auto fieldsLayout = new QGridLayout();
  fieldsLayout->setContentsMargins(0, 0, 0, 0);
  fieldsLayout->setHorizontalSpacing(10);
  fieldsLayout->setVerticalSpacing(10);

  // Add column headers
  fieldsLayout->addWidget(new QLabel(tr("Level Name"), this), 0, 0,
                          Qt::AlignLeft | Qt::AlignVCenter);
  fieldsLayout->addWidget(new QLabel(tr("Level Path"), this), 0, 1,
                          Qt::AlignLeft | Qt::AlignVCenter);

  // Create file field for each level name
  for (const QString& levelName : levelNames) {
    const int row = fieldsLayout->rowCount();
    fieldsLayout->addWidget(new QLabel(levelName, this), row, 0,
                            Qt::AlignRight | Qt::AlignVCenter);

    auto fileField = new FileField(this);
    fieldsLayout->addWidget(fileField, row, 1);

    m_fields.insert(levelName, fileField);
    fileField->setFileMode(QFileDialog::AnyFile);
    fileField->setObjectName("SuggestiveFileField");

    // Connect path changed signal
    connect(fileField, &FileField::pathChanged, this,
            &XDTSImportPopup::onPathChanged);
  }

  fieldsLayout->setColumnStretch(1, 1);
  fieldsLayout->setRowStretch(fieldsLayout->rowCount(), 1);

  fieldsWidget->setLayout(fieldsLayout);
  fieldsArea->setWidget(fieldsWidget);
  m_topLayout->addWidget(fieldsArea, 1);

  // Create cell mark group box
  auto cellMarkGroupBox =
      new QGroupBox(tr("Cell marks for %1 symbols").arg(fileType));

  auto markLayout = new QGridLayout();
  markLayout->setContentsMargins(10, 10, 10, 10);
  markLayout->setVerticalSpacing(10);
  markLayout->setHorizontalSpacing(5);

  {
    // Symbols for SXF: ○ (0x25CB) and ● (0x25CF)
    // Symbols for XDTS: '0' and '*'
    const QChar symbol1 = isSXF ? QChar(0x25CB) : QChar('0');
    const QChar symbol2 = isSXF ? QChar(0x25CF) : QChar('*');

    markLayout->addWidget(
        new QLabel(tr("Inbetween Symbol1 (%1):").arg(symbol1), this), 0, 0,
        Qt::AlignRight | Qt::AlignVCenter);
    markLayout->addWidget(m_tick1Combo, 0, 1);
    markLayout->addItem(new QSpacerItem(10, 1), 0, 2);
    markLayout->addWidget(
        new QLabel(tr("Inbetween Symbol2 (%1):").arg(symbol2), this), 0, 3,
        Qt::AlignRight | Qt::AlignVCenter);
    markLayout->addWidget(m_tick2Combo, 0, 4);

    if (m_isUext) {
      markLayout->addWidget(new QLabel(tr("Keyframe Symbol:"), this), 1, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      markLayout->addWidget(m_keyFrameCombo, 1, 1);
      markLayout->addWidget(new QLabel(tr("Reference Frame Symbol:"), this), 1,
                            3, Qt::AlignRight | Qt::AlignVCenter);
      markLayout->addWidget(m_referenceFrameCombo, 1, 4);
    }
  }

  cellMarkGroupBox->setLayout(markLayout);
  m_topLayout->addWidget(cellMarkGroupBox, 0, Qt::AlignRight);

  // Create conversion options layout
  auto checkBoxLayout = new QHBoxLayout;
  checkBoxLayout->addWidget(new QLabel(tr("Convert Raster to TLV : ")));
  checkBoxLayout->addWidget(m_convertCombo);
  checkBoxLayout->addStretch();
  checkBoxLayout->addWidget(m_renameCheckBox);

  m_topLayout->addLayout(checkBoxLayout);
  m_topLayout->addWidget(convertNAAWidget);

  // Connect buttons using modern signal/slot syntax
  connect(loadButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  addButtonBarWidget(loadButton, cancelButton);
  adjustSize();
  updateSuggestions(scenePath.getQString());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::onPathChanged() {
  auto fileField = qobject_cast<FileField*>(sender());
  if (!fileField) return;

  const QString levelName = m_fields.key(fileField);

  // Remove level from suggested list since user changed it
  m_pathSuggestedLevels.removeAll(levelName);

  // Check if path is in sub-folder with same name as level
  // (common in CSP exports like foo/A/A.tlv)
  const TFilePath currentPath =
      m_scene->decodeFilePath(TFilePath(fileField->getPath()));
  const TFilePath parentDir = currentPath.getParentDir();

  QDir directory(parentDir.getQString());
  if (directory.dirName() == levelName) {
    updateSuggestions(parentDir.getQString());
  }

  updateSuggestions(fileField->getPath());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::updateSuggestions(const QString& samplePath) {
  TFilePath filePath(samplePath);
  filePath = m_scene->decodeFilePath(filePath).getParentDir();

  QDir suggestFolder(filePath.getQString());

  // Supported image file extensions
  static const QStringList filters = {"*.bmp",  "*.jpg",  "*.jpeg", "*.nol",
                                      "*.pic",  "*.pict", "*.pct",  "*.png",
                                      "*.rgb",  "*.sgi",  "*.tga",  "*.tif",
                                      "*.tiff", "*.tlv",  "*.pli",  "*.psd"};

  suggestFolder.setNameFilters(filters);
  suggestFolder.setFilter(QDir::Files);

  TFilePathSet pathSet;
  try {
    TSystem::readDirectory(pathSet, suggestFolder, true);
  } catch (...) {
    return;
  }

  auto fieldsItr = m_fields.begin();
  bool suggested = false;

  while (fieldsItr != m_fields.end()) {
    const QString levelName = fieldsItr.key();
    FileField* fileField    = fieldsItr.value();

    // Check if field can be filled with suggestion
    if (fileField->getPath().isEmpty() ||
        m_pathSuggestedLevels.contains(levelName)) {
      bool found = false;

      // Look for file with same name as level in current folder
      for (const auto& path : pathSet) {
        if (path.getName() == levelName.toStdString()) {
          const TFilePath codedPath = m_scene->codeFilePath(path);
          fileField->setPath(codedPath.getQString());

          if (!m_pathSuggestedLevels.contains(levelName)) {
            m_pathSuggestedLevels.append(levelName);
          }

          found = true;
          break;
        }
      }

      // If not found, check sub-folder with same name as level
      if (!found && suggestFolder.cd(levelName)) {
        TFilePathSet subPathSet;
        try {
          TSystem::readDirectory(subPathSet, suggestFolder, true);
        } catch (...) {
          return;
        }

        for (const auto& path : subPathSet) {
          if (path.getName() == levelName.toStdString()) {
            suggested                 = true;
            const TFilePath codedPath = m_scene->codeFilePath(path);
            fileField->setPath(codedPath.getQString());

            if (!m_pathSuggestedLevels.contains(levelName)) {
              m_pathSuggestedLevels.append(levelName);
            }

            break;
          }
        }

        suggestFolder.cdUp();  // Return to parent folder
      }

      if (found) suggested = true;
    }

    ++fieldsItr;
  }

  // Fallback search if no suggestions found
  if (!suggested) {
    updateSuggestions(filePath);
  }

  // Update field styles based on suggestion status
  fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    FileField* field = fieldsItr.value();

    if (m_pathSuggestedLevels.contains(fieldsItr.key())) {
      // Highlight suggested fields with blue border
      field->setStyleSheet(
          "#SuggestiveFileField QLineEdit {"
          "border-color: #2255aa;"
          "border-width: 2px;"
          "}");
    } else {
      // Clear styling for non-suggested fields
      field->setStyleSheet("");

      // Set default path but clear displayed text
      if (field->getPath().isEmpty()) {
        field->setPath(m_scene->getScenePath().getParentDir().getQString());
        field->getField()->clear();
      }
    }

    ++fieldsItr;
  }
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::updateSuggestions(const TFilePath& path) {
  static const QStringList filters = {
      "*.bmp", "*.jpg", "*.jpeg", "*.nol", "*.pic", "*.pict", "*.pct",
      "*.png", "*.rgb", "*.sgi",  "*.tga", "*.tif", "*.tiff"};

  bool suggestFolderFound = false;
  QDir suggestFolder(path.getQString());

  auto assignMatchingFiles = [&](QStringList& files) {
    auto it = m_fields.begin();
    while (it != m_fields.end()) {
      const QString levelName = it.key();
      FileField* fileField    = it.value();

      if (fileField->getPath().isEmpty() ||
          m_pathSuggestedLevels.contains(levelName)) {
        // Create regex pattern to match level name with any extension
        const QString filePattern =
            QString(".*%1.*\\.[a-zA-Z0-9]{3,4}$").arg(levelName);
        const QRegularExpression regex(filePattern);

        const int index = files.indexOf(regex);
        if (index != -1) {
          suggestFolderFound = true;
          const TFilePath foundPath(suggestFolder.filePath(files[index]));
          const TFilePath codedPath = m_scene->codeFilePath(foundPath);

          fileField->setPath(codedPath.getQString());
          files.removeAt(index);
          m_pathSuggestedLevels.append(levelName);
        }
      }
      ++it;
    }
  };

  // Search in relative paths
  QStringList relativePaths = suggestFolder.entryList(
      filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);

  assignMatchingFiles(relativePaths);

  if (!suggestFolderFound) {
    relativePaths.clear();

    // Search in subdirectories
    const QStringList subDirs = suggestFolder.entryList(
        QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot);

    for (const QString& subDirName : subDirs) {
      QDir subDir = suggestFolder;
      if (!subDir.cd(subDirName)) continue;

      const QStringList filesInSubDir = subDir.entryList(
          filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);

      for (const QString& file : filesInSubDir) {
        relativePaths.append(subDirName + "/" + file);
      }
    }

    assignMatchingFiles(relativePaths);
  }
}

//-----------------------------------------------------------------------------

QString XDTSImportPopup::getLevelPath(const QString& levelName) const {
  auto field = m_fields.value(levelName);
  return field ? field->getPath() : QString();
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::getMarkerIds(int& tick1Id, int& tick2Id, int& keyFrameId,
                                   int& referenceFrameId) const {
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

//-----------------------------------------------------------------------------

void XDTSImportPopup::accept() {
  QDialog::accept();

  std::vector<IoCmd::LoadResourceArguments::ResourceData> resourceData;

  // Collect resource data from fields
  for (auto it = m_fields.constBegin(); it != m_fields.constEnd(); ++it) {
    IoCmd::LoadResourceArguments::ResourceData data;
    data.m_path = TFilePath(it.value()->getPath());
    resourceData.push_back(data);
  }

  // Fetch sequence files if available
  fetchSequenceFiles(resourceData);

  // Rename resources if checkbox is checked
  if (m_renameCheckBox->isChecked()) {
    IoCmd::renameResources(resourceData, false);
  }

  // Handle conversion based on selected option
  const int conversionOption = m_convertCombo->currentIndex();

  if (conversionOption == 1) {
    // Convert each level individually with settings popup
    ConvertPopup popup;

    for (auto& data : resourceData) {
      const TFileType::Type fileType = TFileType::getInfo(data.m_path);
      if (!TFileType::isFullColor(fileType)) continue;

      popup.setWindowModality(Qt::ApplicationModal);
      popup.setFiles({data.m_path});
      popup.setFormat("tlv");
      popup.adjustSize();
      popup.show();

      // Wait for conversion to complete
      while (popup.isVisible() || popup.isConverting()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents |
                                        QEventLoop::WaitForMoreEvents);
      }

      const TFilePath convertedPath = popup.getConvertedPath(data.m_path);
      if (!convertedPath.isEmpty()) {
        data.m_path = convertedPath;
      }
    }
  } else if (conversionOption == 2) {
    // Convert NAA Unpainted raster to TLV
    double dpi         = 0.0;
    const int dpiIndex = m_dpiMode->currentIndex();

    if (dpiIndex == 1) {
      // Use current camera DPI
      if (const auto camera = TApp::instance()
                                  ->getCurrentScene()
                                  ->getScene()
                                  ->getCurrentCamera()) {
        dpi = camera->getDpi().x;
      }
    } else if (dpiIndex == 2) {
      // Use custom DPI
      dpi = m_dpiFld->getValue();
    }

    IoCmd::convertNAARaster2TLV(
        resourceData,
        false,  // not generating paints, only inks
        dpi,
        m_paletteCheckBox->isChecked()  // append default palette
    );
  }

  // Update fields with potentially converted paths
  int index = 0;
  for (auto it = m_fields.begin(); it != m_fields.end(); ++it, ++index) {
    if (index < static_cast<int>(resourceData.size())) {
      it.value()->setPath(resourceData[index].m_path.getQString());
    }
  }
}
