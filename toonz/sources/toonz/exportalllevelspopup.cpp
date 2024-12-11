

#include "exportalllevelspopup.h"

// Tnz6 includes
#include "tapp.h"
#include "filebrowser.h"
#include "columnselection.h"
#include "menubarcommandids.h"
#include "iocommand.h"
#include "exportlevelcommand.h"
#include "formatsettingspopups.h"

// TnzQt includes
#include "toonzqt/checkbox.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/framenavigator.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tcamera.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tproject.h"
#include "toonz/stage.h"
#include "toonz/preferences.h"
#include "toonz/tstageobjectcmd.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshleveltypes.h"
#include "toutputproperties.h"

// TnzCore includes
#include "tiio.h"
#include "tproperty.h"

// Qt includes
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QTabBar>
#include <QStackedWidget>
#include <QSplitter>
#include <QToolBar>
#include <QString>
#include <QMessageBox>


//********************************************************************************
//    Export callbacks  definition
//********************************************************************************

namespace {

struct MultiExportOverwriteCB final : public IoCmd::OverwriteCallbacks {
  bool m_yesToAll;
  bool m_stopped;

  MultiExportOverwriteCB() : m_yesToAll(false), m_stopped(false) {}
  bool overwriteRequest(const TFilePath &fp) override {
    if (m_yesToAll) return true;
    if (m_stopped) return false;

    int ret = DVGui::MsgBox(
        QObject::tr("Warning: file %1 already exists.").arg(toQString(fp)),
        QObject::tr("Continue Exporting"), QObject::tr("Continue to All"),
        QObject::tr("Stop Exporting"), 1);

    m_yesToAll = (ret == 2);
    m_stopped  = (ret == 0) || (ret == 3);
    return !m_stopped;
  }
};

//=============================================================================

struct MultiExportProgressCB final : public IoCmd::ProgressCallbacks {
  QString m_processedName;
  DVGui::ProgressDialog m_pb;

public:
  MultiExportProgressCB() : m_pb("", QObject::tr("Cancel"), 0, 0) {
    m_pb.show();
  }
  static QString msg() {
    return QObject::tr("Exporting level of %1 frames in %2");
  }

  void setProcessedName(const QString &name) override {
    m_processedName = name;
  }
  void setRange(int min, int max) override {
    m_pb.setMaximum(max);
    buildString();
  }
  void setValue(int val) override { m_pb.setValue(val); }
  bool canceled() const override { return m_pb.wasCanceled(); }
  void buildString() {
    m_pb.setLabelText(
        msg().arg(QString::number(m_pb.maximum())).arg(m_processedName));
  }
};

}  // namespace

//********************************************************************************
//    ExportAllLevelsPopup  implementation
//********************************************************************************

ExportAllLevelsPopup::ExportAllLevelsPopup(){
  setWindowModality(Qt::ApplicationModal);
  setWindowTitle(QString("Exporting All Levels..."));
  m_browser->setFolder(TApp::instance()->getCurrentScene()->getScene()->getProject()->getProjectFolder());

  // Export Options Tab
  m_exportOptions->m_createlevelfolder->setChecked(true);
  m_exportOptions->m_noAntialias->setChecked(true);

    // Export All checkbox
  m_exportall = new DVGui::CheckBox(tr("Export All"), this);
  m_exportall->setChecked(true);
  m_nameField->setDisabled(true);
  m_nameField->setText("");
  isexport_all = true;

  // Skip Button
  m_skipButton = new QPushButton(tr("Skip"), this);
  m_skipButton->setDisabled(true);

  // Layout
  QLayout *fileFormatLayout =
      layout()
          ->itemAt(2)
          ->layout()
          ->itemAt(2)
          ->widget()
          ->layout();  // Add below checkbox No Antialias
  fileFormatLayout->addWidget(m_exportall);
  fileFormatLayout->addWidget(m_skipButton);

  // Establish connections
  bool ret = true;

  ret = connect(m_skipButton, SIGNAL(clicked()), this, SLOT(skip())) && ret;
  ret = connect(m_exportall, SIGNAL(toggled(bool)), this,
                SLOT(onExportAll(bool))) &&
        ret;
  }

ExportAllLevelsPopup::~ExportAllLevelsPopup() { outputLevels.clear(); }

//------------------------------------

void ExportAllLevelsPopup::showEvent(QShowEvent *se) {
    
    if (Preferences::instance()->getPixelsOnly()) {
      m_exportOptions->m_widthFld->hide();
      m_exportOptions->m_heightFld->hide();
      m_exportOptions->m_widthLabel->hide();
      m_exportOptions->m_heightLabel->hide();
      m_exportOptions->m_dpiLabel->hide();
    } else {
      m_exportOptions->m_widthFld->show();
      m_exportOptions->m_heightFld->show();
      m_exportOptions->m_widthLabel->show();
      m_exportOptions->m_heightLabel->show();
      m_exportOptions->m_dpiLabel->show();
    }

    // reset map
    level_to_foldername.clear();

    GetSelectedSimpLevels();//also init level_to_foldername,and sort them

    if (outputLevels.empty()) {
        QTimer::singleShot(1000, this, &ExportAllLevelsPopup::hide);
        DVGui::error(tr("No Level in camera stand or null Level !"));
        return;
    }

    onExportAll(isexport_all);
    updateOnSelection();
}

void ExportAllLevelsPopup::updateOnSelection() {
    TXshSimpleLevel *sl = outputLevels.back()->getSimpleLevel();
    assert(sl);

    /*   Implement this Feature in the feature
    // Enable tlv output in case all inputs are pli or tlv
    int tlvIdx = m_format->findText("tlv");
        //export all mode
    if (isexport_all) {
      bool allPliTlvs = true;
      for (auto sl : outputLevels) {
        allPliTlvs = (sl && (sl->getType() == PLI_XSHLEVEL ||
                             sl->getType() == TZP_XSHLEVEL)) &&
                     allPliTlvs;
      }
      if (allPliTlvs) {
        if (tlvIdx < 0) m_format->addItem("tlv");
      } else {
        if (tlvIdx > 0) m_format->removeItem(tlvIdx);
      }
      
    } 
        //export one by one mode
    else {
      if (sl->getType() == PLI_XSHLEVEL || sl->getType() == TZP_XSHLEVEL) {
        if (tlvIdx < 0) m_format->addItem("tlv");
      } else {
        if (tlvIdx > 0) m_format->removeItem(tlvIdx);
      }
    }
    */

    //whether be abel to set PliOptions
    if (sl->getType() == PLI_XSHLEVEL) {
        m_exportOptions->pliOtionsVisable = true;  // used when exportOptions be shown
        m_exportOptions->m_pliOptions->setEnabled(true); 
    } else {
        m_exportOptions->pliOtionsVisable = false;
        m_exportOptions->m_pliOptions->setEnabled(false); 
    }
    updatePreview();
    return;
}

void ExportAllLevelsPopup::GetSelectedSimpLevels() {
  // get output Levels
  outputLevels.clear();
  TXsheetHandle *xshHandle = TApp::instance()->getCurrentXsheet();
  TXsheet *xsh  = xshHandle->getXsheet();
  int col_count = xsh->getColumnCount();
  int c, r0, r1;  // columnindex,cellstartpoint
  TXshSimpleLevel *sl;
  std::vector<int> cols;
  TStageObject *pegbar;
  TStageObjectId id;
  std::string colname;
  std::wstring levelname;

  for (int i = 1; i <= col_count; ++i) {
    TXshColumn *col = xsh->getColumn(i - 1);
    assert(col);
    if (!col->isCamstandVisible()) continue;
    if (!col->getColumnType()) {  // eLevelType
      c = col->getIndex();
      if (col->getLevelRange(c, r0, r1)) {  // Not empty
        sl = xsh->getCell(r0, c).getSimpleLevel();
        assert(sl);
        outputLevels.push_back(sl);

        pegbar = xsh->getStageObject(TStageObjectId::ColumnId(c));
        colname = pegbar->getName();
        levelname = sl->getName();
        level_to_foldername[sl->getName()] = backfoldername(colname,levelname);
      } else
        continue;
    }
  }

  // sort
  auto *p = &level_to_foldername;
  std::sort(outputLevels.begin(), outputLevels.end(),
            [p](TXshLevel *lhs, TXshLevel *rhs) {
              if (lhs->getType() ==
                  rhs->getType()) {  // vector first,biggest level name second
                return p->find(lhs->getName())->second <
                       p->find(rhs->getName())->second;
              } else
                return lhs->getType() > rhs->getType();
            });
}


void ExportAllLevelsPopup::onExportAll(bool toggled) {
  if (toggled) {
    this->setWindowTitle(QString("Exporting All Levels"));
    m_skipButton->setDisabled(true);
    m_nameField->setDisabled(true);
    m_nameField->setText("");
    isexport_all = true;
  } 
  
  else {
    this->setWindowTitle(QString("Exporting ") +
                         getLevelTypeName(outputLevels.back()->getType()) +
                         QString(" Level ") +
                         QString::fromStdWString(outputLevels.back()->getName()));
    m_skipButton->setEnabled(true);
    m_nameField->setEnabled(true);
    
    isexport_all = false;
    m_nameField->setText(QString::fromStdWString(
        level_to_foldername.find(outputLevels.back()->getName())->second));
  }
}

bool ExportAllLevelsPopup::execute() {
  // Get Folder 
    TFilePath fp = TFilePath(m_browser->getFolder().getQString());

  // Do Path Checks
  if (fp.isEmpty()) {
    DVGui::error(tr("Please select a Folder!"));
    return false;
  }

  // ReBuild export options
  const std::string &ext                = m_format->currentText().toStdString();
  const IoCmd::ExportLevelOptions &opts = getOptions(ext);

  // Start to export
  bool ret = true;

  // export rest at one time
  if (isexport_all) {
    TXshSimpleLevel *sl;
    std::wstring folderName;
    MultiExportProgressCB progressCB;
    MultiExportOverwriteCB overwriteCB;

    
    while (!outputLevels.empty()) {
      sl  = outputLevels.back()->getSimpleLevel();
      folderName = m_exportOptions->m_createlevelfolder->isChecked()
                       ? level_to_foldername.find(sl->getName())->second
                       : std::wstring();
      ret = IoCmd::exportLevel(
                TFilePath(fp.getWideString() + L"\\" +
                          level_to_foldername.find(sl->getName())->second +
                          L"." + to_wstring(ext)),
                          sl,
                               opts, &overwriteCB, &progressCB,
                folderName) &&
            ret;
      if (ret) {
        ++level_exported;  // count exported levels
      };
      outputLevels.pop_back();
    }
  }

  // Export one by one
  else {
    // File Name Check
    if (m_nameField->text().isEmpty()) {
      m_nameField->setText(QString::fromStdWString(
          level_to_foldername.find(outputLevels.back()->getName())->second));
      return false;
    }
    if (!isValidFileName(QString::fromStdString(fp.getName()))) {
      DVGui::error(
          tr("The file name cannot be empty or contain any of the following "
             "characters:(new line)  \\ / : * ? \"  |"));
      return false;
    }
    if (isReservedFileName_message(QString::fromStdString(fp.getName())))
      return false;

    TXshSimpleLevel *sl = outputLevels.back()->getSimpleLevel();
    ret = IoCmd::exportLevel(TFilePath(fp.getWideString() + L"\\" +
                                       m_nameField->text().toStdWString() +
                                       L"." + to_wstring(ext)),
              sl,
              opts, 0, 0, (m_exportOptions->m_createlevelfolder->isChecked() ?
                    m_nameField->text().toStdWString() : std::wstring())) &&
          ret;

    // count levels exported
    if (ret) {
      ++level_exported;
      outputLevels.pop_back();
    } else
      return false;
  }

  // Is The Export All Action Done?
  if (isAllLevelsExported()) {
    return true;
  }
  else {
    updateOnSelection();
    return false;
  }
}

std::wstring ExportAllLevelsPopup::backfoldername(std::string colname,
                                                  std::wstring levelname) {
  if (levelname == to_wstring(colname) || colname.substr(0, 3) == "Col")
    return levelname;
  else {
    std::wstring foldername;
    colname.erase(std::remove(colname.begin(), colname.end(), L'#'),
                  colname.end());

    int pos = colname.find(L'1');
    if (pos != std::wstring::npos) {
      foldername += to_wstring("_1gen");
      colname.erase(pos);
    } else {
      pos = colname.find(L'2');
      if (!pos == std::wstring::npos) {
        foldername += to_wstring("_2gen");
        colname.erase(pos);
      }
    }

    pos = colname.find(L'_');    //  '_'
    if (pos != std::wstring::npos) {
      foldername.insert(0,to_wstring(colname.substr(0, pos)));
    } else {
      foldername.insert(0,to_wstring(colname));
    }
    return foldername;
  }
}

bool ExportAllLevelsPopup::isAllLevelsExported() {
  if (outputLevels.empty()) {  // no more level to export
    QMessageBox::information(
        nullptr,
        "Export All Levels",                                   // title
        QString::number(level_exported) + " Levels Exported",  // content
        QMessageBox::Ok);
    level_exported = 0;
    return true;
  } else {  // move to next level
    m_nameField->setText(QString::fromStdWString(
        level_to_foldername.find(outputLevels.back()->getName())->second));
    this->setWindowTitle(
        QString("Exporting ") +
        getLevelTypeName(outputLevels.back()->getType()) + QString(" Level ") +
        QString::fromStdWString(outputLevels.back()->getName()));
    return false;
  }
}

void ExportAllLevelsPopup::skip() {
  outputLevels.pop_back();
  // Is The Export All Action Done?
  if (isAllLevelsExported()) {
    close();
  } else {
    updateOnSelection();
  }
}

QString ExportAllLevelsPopup::getLevelTypeName(int type) { 
    switch (type){
  case PLI_XSHLEVEL:
     return QString("ToonzVector");
  case TZP_XSHLEVEL:
     return QString("ToonzRaster");
  case OVL_XSHLEVEL:
     return QString("Raster");
  case LEVELCOLUMN_XSHLEVEL:
     return QString("Picture");
  default:
      return QString("");
    }
}
    
//********************************************************************************
//    Export All Levels Command  instantiation
//********************************************************************************

OpenPopupCommandHandler<ExportAllLevelsPopup> exportAllLevelsPopupCommand(MI_ExportAllLevels);