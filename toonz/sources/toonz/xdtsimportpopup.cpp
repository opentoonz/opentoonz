#include "xdtsimportpopup.h"

#include "tapp.h"
#include "tsystem.h"
#include "toonzqt/filefield.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"

#include <QMainWindow>
#include <QTableView>
#include <QPushButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>

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
    return pattern.match(QString::fromStdString(path.getLevelName()))
        .hasMatch();
};

bool isSharingSameParam(QString name1,
    QString name2) {
    std::wstring str1 = name1.toStdWString();
    std::wstring str2 = name2.toStdWString();

    str1.erase(std::remove_if(str1.begin(), str1.end(), ::iswdigit),
        str1.end());
    str2.erase(std::remove_if(str2.begin(), str2.end(), ::iswdigit),
        str2.end());

    return str1 == str2;
}

}  // namespace

//=============================================================================

XDTSImportPopup::XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                                 TFilePath scenePath, bool isUextVersion)
    : m_scene(scene)
    , m_isUext(isUextVersion)
    , DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "XDTSImport") {
  setWindowTitle(tr("Importing XDTS file %1")
                     .arg(QString::fromStdString(scenePath.getLevelName())));
  QPushButton* loadButton   = new QPushButton(tr("Load"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

  m_tick1Combo             = new QComboBox(this);
  m_tick2Combo             = new QComboBox(this);
  QList<QComboBox*> combos = {m_tick1Combo, m_tick2Combo};

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
  fieldsLay->setMargin(0);
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

  // 来週 原画／中割参考のコママークのレイアウトから！

  // cell mark area
  QGroupBox* cellMarkGroupBox =
      new QGroupBox(tr("Cell marks for XDTS symbols"));
  QGridLayout* markLay = new QGridLayout();
  markLay->setMargin(10);
  markLay->setVerticalSpacing(10);
  markLay->setHorizontalSpacing(5);
  {
    markLay->addWidget(new QLabel(tr("Inbetween Symbol1 (O):"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    markLay->addWidget(m_tick1Combo, 0, 1);
    markLay->addItem(new QSpacerItem(10, 1), 0, 2);
    markLay->addWidget(new QLabel(tr("Inbetween Symbol2 (*)"), this), 0, 3,
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

  connect(loadButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(loadButton, cancelButton);

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
  bool suggested = false;
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
            suggested = true;
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
  if (!suggested) {
      updateSuggestions(fp);
      return;
  }

  // repaint fields
  fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    if (m_pathSuggestedLevels.contains(fieldsItr.key()))
      fieldsItr.value()->setStyleSheet(
          QString("#SuggestiveFileField "
                  "QLineEdit{border-color:#2255aa;border-width:2px;}"));
    else
      fieldsItr.value()->setStyleSheet(QString(""));
    ++fieldsItr;
  }
}

//-----------------------------------------------------------------------------
// FALL BACK
void XDTSImportPopup::updateSuggestions(const TFilePath &path) {
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
            QString levelName = it.key();
            FileField* fileField = it.value();
            if (fileField->getPath().isEmpty() ||
                m_pathSuggestedLevels.contains(levelName)) {
                QString filePattern = ".*" + levelName + ".*" + ".{3,4}$";
                int index = files.indexOf(QRegExp(filePattern));
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
    QStringList relPaths = suggestFolder.entryList(filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    assignMatchingFiles(relPaths);

    if (!suggestFolderFound) {
        relPaths.clear();
        QStringList subDirs = suggestFolder.entryList(QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot);
        for (const QString& subDirName : subDirs) {
            QDir subDir = suggestFolder;
            if (!subDir.cd(subDirName)) continue;

            QStringList filesInSubDir = subDir.entryList(filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
            for (const QString& file : filesInSubDir) {
                relPaths.append(subDirName + "/" + file);
            }
        }
        assignMatchingFiles(relPaths);
    }

  // repaint fields
  fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    if (m_pathSuggestedLevels.contains(fieldsItr.key()))
      fieldsItr.value()->setStyleSheet(
          QString("#SuggestiveFileField "
                  "QLineEdit{border-color:#2255aa;border-width:2px;}"));
    else
      fieldsItr.value()->setStyleSheet(QString(""));
    ++fieldsItr;
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