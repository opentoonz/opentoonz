

#include "versioncontroltimeline.h"

// Tnz6 includes
#include "tapp.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"

// TnzCore includes
#include "tfiletype.h"
#include "tfilepath.h"
#include "tsystem.h"

// Qt includes
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QScrollBar>
#include <QDate>
#include <QTimer>
#include <QDir>
#include <QMainWindow>

using namespace DVGui;

#define ICON_WIDTH 60
#define ICON_HEIGHT 60

//=============================================================================
// TimelineWidget
//-----------------------------------------------------------------------------

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);

  QLabel *label = new QLabel(tr("Recent Version"));
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  label->setAlignment(Qt::AlignHCenter);
  label->setStyleSheet(
      "background-color: rgb(220,220,220); border: 1px solid "
      "rgb(150,150,150);");

  mainLayout->addWidget(label);

  m_listWidget = new QListWidget;
  // m_listWidget->setItemDelegate(new ItemDelegate(m_listWidget));
  m_listWidget->setFlow(QListView::TopToBottom);
  m_listWidget->setViewMode(QListView::ListMode);
  m_listWidget->setMovement(QListView::Static);
  m_listWidget->setWrapping(false);
  m_listWidget->setAlternatingRowColors(true);
  m_listWidget->setIconSize(QSize(ICON_WIDTH, ICON_HEIGHT));
  m_listWidget->setMinimumWidth(400);
  m_listWidget->setMinimumHeight(300);

  mainLayout->addWidget(m_listWidget);

  label = new QLabel(tr("Older Version"));
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  label->setAlignment(Qt::AlignHCenter);
  label->setStyleSheet(
      "background-color: rgb(220,220,220); border: 1px solid "
      "rgb(150,150,150); ");

  mainLayout->addWidget(label);

  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

int TimelineWidget::getCurrentIndex() const {
  return m_listWidget->currentIndex().row();
}

//=============================================================================
// SVNTimeline
//-----------------------------------------------------------------------------

SVNTimeline::SVNTimeline(QWidget *parent, const QString &workingDir,
                         const QString &fileName, const QStringList &auxFiles)
    : Dialog(TApp::instance()->getMainWindow(), true, true)
    , m_workingDir(workingDir)
    , m_auxFiles(auxFiles)
    , m_fileName(fileName)
    , m_currentExportIndex(0)
    , m_currentAuxExportIndex(0) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Timeline ") + fileName);

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel(tr("Getting file history..."));

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  m_timelineWidget = new TimelineWidget;
  m_timelineWidget->setStyleSheet(
      "QListWidget { background-color: white; } QListWidget:item { margin: "
      "5px; }");
  m_timelineWidget->hide();
  connect(m_timelineWidget->getListWidget(), SIGNAL(itemSelectionChanged()),
          this, SLOT(onSelectionChanged()));

  QHBoxLayout *checkBoxLayout = new QHBoxLayout;
  checkBoxLayout->setMargin(0);
  m_sceneContentsCheckBox = new QCheckBox(this);
  m_sceneContentsCheckBox->setVisible(m_fileName.endsWith(".tnz"));
  connect(m_sceneContentsCheckBox, SIGNAL(toggled(bool)), this,
          SLOT(onSceneContentsToggled(bool)));
  m_sceneContentsCheckBox->setChecked(false);
  m_sceneContentsCheckBox->setText(tr("Get Scene Contents"));
  checkBoxLayout->addStretch();
  checkBoxLayout->addWidget(m_sceneContentsCheckBox);
  checkBoxLayout->addStretch();

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setMargin(0);
  mainLayout->addLayout(hLayout);
  mainLayout->addWidget(m_timelineWidget);
  mainLayout->addSpacing(5);
  mainLayout->addLayout(checkBoxLayout);

  QWidget *container = new QWidget;
  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_updateButton = new QPushButton(tr("Get Last Revision"));
  connect(m_updateButton, SIGNAL(clicked()), this,
          SLOT(onUpdateButtonClicked()));

  m_updateToRevisionButton = new QPushButton(tr("Get Selected Revision"));
  connect(m_updateToRevisionButton, SIGNAL(clicked()), this,
          SLOT(onUpdateToRevisionButtonClicked()));
  m_updateToRevisionButton->setEnabled(false);

  m_closeButton = new QPushButton(tr("Close"));
  connect(m_closeButton, SIGNAL(clicked()), this, SLOT(close()));

  addButtonBarWidget(m_updateButton, m_updateToRevisionButton, m_closeButton);

  // 0. Connect for svn errors (that may occurs everythings)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. get the log (history) of fileName
  QStringList args;
  args << "log" << m_fileName << "--xml";
  connect(&m_thread, SIGNAL(done(const QString &)),
          SLOT(onLogDone(const QString &)));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

SVNTimeline::~SVNTimeline() {
  removeTempFiles();
  // Delete, if exist the sceneIcons folder
  QDir d;
  d.rmdir("sceneIcons");
}

//-----------------------------------------------------------------------------

void SVNTimeline::removeTempFiles() {
  int tempFileCount = m_tempFiles.count();
  for (int i = 0; i < tempFileCount; i++) {
    QTemporaryFile *file = m_tempFiles.at(i);
    file->close();
    QFileInfo fi(*file);
    QFile::remove(fi.absoluteFilePath());
    delete file;
  }
  m_tempFiles.clear();

  tempFileCount = m_auxTempFiles.count();
  for (int i = 0; i < tempFileCount; i++) {
    QTemporaryFile *file = m_auxTempFiles.at(i);
    file->close();
    QFileInfo fi(*file);
    QFile::remove(fi.absoluteFilePath());
    delete file;
  }
  m_auxTempFiles.clear();
}

//-----------------------------------------------------------------------------

void SVNTimeline::onLogDone(const QString &xmlResponse) {
  m_textLabel->hide();
  m_waitingLabel->hide();
  // qDebug(xmlResponse.toAscii());

  SVNLogReader lr(xmlResponse);
  m_log = lr.getLog();

  int logCount = m_log.size();
  if (logCount == 0) return;

  QString fileNameType =
      QString::fromStdString(TFilePath(m_fileName.toStdWString()).getType());

  for (int i = 0; i < logCount; i++) {
    SVNLog log = m_log.at(i);

    QDate d      = QDate::fromString(log.m_date.left(10), "yyyy-MM-dd");
    int dayCount = d.daysTo(QDate::currentDate());
    QString text = "\n" + tr("Date") + ": " + d.toString("MM-dd-yyyy");

    if (dayCount == 0) {
      // text += "Today\nby " + log.m_author;
      QString timeString = log.m_date.split("T").at(1);
      timeString         = timeString.left(5);

      // Convert the current time, to UTC
      QDateTime currentTime = QDateTime::currentDateTime().toUTC();

      QTime t     = QTime::fromString(timeString, "hh:mm");
      QTime now   = QTime::fromString(currentTime.toString("hh:mm"), "hh:mm");
      int seconds = t.secsTo(now);
      int minute  = seconds / 60;
      text += " ( " + QString::number(minute) + " minutes ago )\n" +
              tr("Author") + ": " + log.m_author + "\n" + tr("Comment") + ": " +
              log.m_msg;
    } else
      text += " ( " + QString::number(dayCount) + " days ago )\n" +
              tr("Author") + ": " + log.m_author + "\n" + tr("Comment") + ": " +
              log.m_msg;

    QString tooltip = "<b>" + tr("Revision") + "</b>: " + log.m_revision +
                      "<br><b>" + tr("Author") + "</b>: " + log.m_author +
                      "<br><b>" + tr("Date") + "</b>: " + log.m_date +
                      "<br><b>" + tr("Comment") + "</b>: " + log.m_msg;

    QPixmap pixmap(ICON_WIDTH, ICON_HEIGHT);
    pixmap.fill(Qt::lightGray);

    QListWidgetItem *lwi = new QListWidgetItem(
        QIcon(pixmap), text, m_timelineWidget->getListWidget());
    lwi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    lwi->setToolTip(tooltip);
    lwi->setTextAlignment(Qt::AlignLeft);
    m_tempFiles.append(new QTemporaryFile("svn_temp_img_" + QString::number(i) +
                                          "." + fileNameType));

    // Add also auxiliary files
    TFilePath fp(TFilePath(m_workingDir.toStdWString()) +
                 m_fileName.toStdWString());
    if (fp.getDots() == ".." || fp.getType() == "tlv" ||
        fp.getType() == "pli") {
      TFilePathSet fpset;
      TXshSimpleLevel::getFiles(fp, fpset);

      TFilePathSet::iterator it;
      for (it = fpset.begin(); it != fpset.end(); ++it) {
        TFilePath fp = *it;
        if (fp.getType() == "tpl")
          m_auxTempFiles.append(
              new QTemporaryFile("svn_temp_img_" + QString::number(i) + "." +
                                 QString::fromStdString(fp.getType())));
      }
    }
    // Add sceneIcon (only for scene files)
    else if (fp.getType() == "tnz") {
      TFilePath iconPath = ToonzScene::getIconPath(fp);
      if (TFileStatus(iconPath).doesExist()) {
        QDir dir(toQString(fp.getParentDir()));

        // Create the sceneIcons folder
        QDir d;
        d.mkdir("sceneIcons");
        m_auxTempFiles.append(
            new QTemporaryFile(QDir::tempPath() + QString("/sceneicons/") +
                               "svn_temp_img_" + QString::number(i) + " .png"));
      }
    }

    m_listWidgetitems.append(lwi);
  }

  m_timelineWidget->getListWidget()->update();
  m_timelineWidget->show();

  // Export to temporary files
  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onExportDone()));
  exportToTemporaryFile(m_currentExportIndex);
}

//-----------------------------------------------------------------------------

void SVNTimeline::onExportDone() {
  int itemCount = m_listWidgetitems.count();
  if (m_currentExportIndex < 0 || m_currentExportIndex >= itemCount) {
    m_currentExportIndex++;
    return;
  }
  QListWidgetItem *lwi = m_listWidgetitems.at(m_currentExportIndex);
  if (lwi) {
    QFileInfo fi(*m_tempFiles.at(m_currentExportIndex));
    // qDebug(fi.absoluteFilePath().toAscii());

    // Create the icon if there isn't any auxiliary files associated (palette,
    // sceneIcons..)
    if (m_auxTempFiles.isEmpty())
      lwi->setIcon(createIcon(fi.absoluteFilePath()));

    lwi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }

  m_currentExportIndex++;
  if (m_currentExportIndex < itemCount) {
    exportToTemporaryFile(m_currentExportIndex);
  } else {
    if (!m_auxTempFiles.isEmpty()) {
      m_thread.disconnect(SIGNAL(done(const QString &)));
      connect(&m_thread, SIGNAL(done(const QString &)),
              SLOT(onExportAuxDone()));
      exportToTemporaryFile(m_currentAuxExportIndex, true);
    }
  }
}

//-----------------------------------------------------------------------------

void SVNTimeline::onExportAuxDone() {
  int itemCount = m_listWidgetitems.count();
  if (m_currentAuxExportIndex < 0 || m_currentAuxExportIndex >= itemCount) {
    m_currentAuxExportIndex++;
    return;
  }
  QListWidgetItem *lwi = m_listWidgetitems.at(m_currentAuxExportIndex);
  if (lwi) {
    QFileInfo fi(*m_tempFiles.at(m_currentAuxExportIndex));
    // qDebug(fi.absoluteFilePath().toAscii());
    lwi->setIcon(createIcon(fi.absoluteFilePath()));
  }
  m_currentAuxExportIndex++;
  if (m_currentAuxExportIndex < itemCount) {
    exportToTemporaryFile(m_currentAuxExportIndex, true);
  }
}

//-----------------------------------------------------------------------------

void SVNTimeline::exportToTemporaryFile(int index, bool isAuxFile) {
  QFileInfo fi;
  QString fileName = m_fileName;
  fileName.chop(4);

  if (isAuxFile)
    fi = QFileInfo(*m_auxTempFiles.at(index));
  else
    fi = QFileInfo(*m_tempFiles.at(index));

  QString extension = fi.completeSuffix();

  QStringList args;
  args << "export";

  // SceneIcons (pay attention to add a space at the fileName end)
  if (isAuxFile && fi.completeSuffix() == "png") {
    args << "sceneIcons/" + fileName + " ." + extension + "@" +
                m_log.at(index).m_revision;
    args << fi.absoluteFilePath();
  } else {
    args << fileName + "." + fi.completeSuffix() + "@" +
                m_log.at(index).m_revision;
    args << fi.absoluteFilePath();
  }

  // Export to temporary file...
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNTimeline::onError(const QString &text) {
  m_waitingLabel->hide();
  m_sceneContentsCheckBox->hide();
  m_textLabel->setText(text);
  m_textLabel->show();
  m_timelineWidget->hide();
  m_closeButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void SVNTimeline::onSelectionChanged() {
  int selectedItemCount =
      m_timelineWidget->getListWidget()->selectedItems().count();
  m_updateToRevisionButton->setEnabled(selectedItemCount > 0);
}

//-----------------------------------------------------------------------------

QIcon SVNTimeline::createIcon(const QString &fileName) {
  TDimension iconSize(60, 60);

  TFilePath path(fileName.toStdWString());
  TRaster32P iconRaster;
  std::string type(path.getType());

  QPixmap filePixmap(fileName);

  if (filePixmap.isNull()) {
    if (type == "tnz" || type == "tab")
      filePixmap = rasterToQPixmap(
          IconGenerator::generateSceneFileIcon(path, iconSize, 0));
    else if (type == "pli")
      filePixmap = rasterToQPixmap(
          IconGenerator::generateVectorFileIcon(path, iconSize, 1));
    else if (type == "tpl")
      filePixmap = QPixmap(":Resources/paletteicon.svg");
    else if (type == "tzp")
      filePixmap = QPixmap(":Resources/tzpicon.png");
    else if (type == "tzu")
      filePixmap = QPixmap(":Resources/tzuicon.png");
    else if (TFileType::getInfo(path) == TFileType::AUDIO_LEVEL)
      filePixmap = QPixmap(svgToPixmap(":Resources/audio.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
    else if (type == "scr")
      filePixmap = QPixmap(":Resources/savescreen.png");
    else if (type == "psd")
      filePixmap = QPixmap(svgToPixmap(":Resources/psd.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
    else if (TFileType::isViewable(TFileType::getInfo(path)) || type == "tlv")
      filePixmap = rasterToQPixmap(
          IconGenerator::generateRasterFileIcon(path, iconSize, 1));
    else if (type == "mpath")
      filePixmap = QPixmap(svgToPixmap(":Resources/motionpath_fileicon.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
    else if (type == "curve")
      filePixmap = QPixmap(svgToPixmap(":Resources/curve.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
    else if (type == "cln")
      filePixmap = QPixmap(svgToPixmap(":Resources/cleanup.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
    else if (type == "tnzbat")
      filePixmap = QPixmap(svgToPixmap(":Resources/tasklist.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
    else if (type == "js")
      filePixmap = QPixmap(":Resources/scripticon.png");
    else
      filePixmap = QPixmap(svgToPixmap(":Resources/unknown.svg",
                                       QSize(iconSize.lx, iconSize.ly),
                                       Qt::KeepAspectRatio));
  }

  if (filePixmap.isNull()) return QIcon();

  if (filePixmap.size() != QSize(ICON_WIDTH, ICON_HEIGHT))
    filePixmap = filePixmap.scaled(ICON_WIDTH, ICON_HEIGHT, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);

  if (filePixmap.size() == QSize(ICON_WIDTH, ICON_HEIGHT))
    return QIcon(filePixmap);

  QPixmap iconPixmap(ICON_WIDTH, ICON_HEIGHT);
  iconPixmap.fill(Qt::white);

  int y = (int)((iconPixmap.height() - filePixmap.height()) * 0.5);

  QPainter p(&iconPixmap);
  p.drawPixmap(0, y, filePixmap);
  p.end();

  QIcon icon;
  icon.addPixmap(iconPixmap, QIcon::Normal, QIcon::On);
  icon.addPixmap(iconPixmap, QIcon::Selected, QIcon::Off);
  return icon;
}

//-----------------------------------------------------------------------------

void SVNTimeline::onUpdateButtonClicked() {
  m_updateButton->hide();
  m_sceneContentsCheckBox->hide();
  m_updateToRevisionButton->hide();
  m_timelineWidget->hide();
  m_waitingLabel->show();
  if (m_sceneResources.isEmpty())
    m_textLabel->setText(tr("Getting the status for %1...").arg(m_fileName));
  else
    m_textLabel->setText(tr("Getting repository status..."));
  m_textLabel->show();

  QStringList files;
  files.append(m_fileName);
  files.append(m_sceneResources);
  files.append(m_auxFiles);

  // Getting status to control if an update is needed
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir, files, true);
}

//-----------------------------------------------------------------------------

void SVNTimeline::onUpdateToRevisionButtonClicked() {
  int index = m_timelineWidget->getCurrentIndex();
  if (index == -1) return;

  SVNLog log = m_log.at(index);

  m_updateButton->hide();
  m_updateToRevisionButton->hide();
  m_timelineWidget->hide();
  m_sceneContentsCheckBox->hide();
  m_waitingLabel->show();
  if (m_sceneResources.isEmpty())
    m_textLabel->setText(
        tr("Getting %1 to revision %2...").arg(m_fileName).arg(log.m_revision));
  else
    m_textLabel->setText(tr("Getting %1 items to revision %2...")
                             .arg(m_sceneResources.size() + 1)
                             .arg(log.m_revision));
  m_textLabel->show();

  QStringList args;
  args << "update" << m_fileName;

  int auxFilesSize = m_auxFiles.count();
  for (int i = 0; i < auxFilesSize; i++) args << m_auxFiles.at(i);

  int sceneResourcesSize = m_sceneResources.count();
  for (int i = 0; i < sceneResourcesSize; i++) args << m_sceneResources.at(i);

  args << QString("-r").append(log.m_revision);

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onUpdateDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNTimeline::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  QList<SVNStatus> list = sr.getStatus();

  if (!list.empty()) {
    SVNStatus s = list.at(0);

    if (s.m_item == "none" || s.m_item == "missing" ||
        s.m_repoStatus == "modified") {
      if (list.size() == 1)
        m_textLabel->setText(tr("Getting %1...").arg(m_fileName));
      else
        m_textLabel->setText(
            tr("Getting %1 items...").arg(m_sceneResources.size() + 1));

      m_sceneContentsCheckBox->hide();

      QStringList args;
      args << "update" << m_fileName;

      for (int i = 1; i < list.size(); i++) {
        s = list.at(i);
        if (s.m_item == "none" || s.m_item == "missing" ||
            s.m_repoStatus == "modified")
          args << s.m_path;
      }

      m_thread.disconnect(SIGNAL(done(const QString &)));
      connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onUpdateDone()));
      m_thread.executeCommand(m_workingDir, "svn", args, true);
      return;
    }
  }

  // Do nothing (restore the GUI)
  m_updateButton->show();
  m_updateToRevisionButton->show();
  m_timelineWidget->show();
  if (m_fileName.endsWith(".tnz")) m_sceneContentsCheckBox->show();
  m_waitingLabel->hide();
  m_textLabel->hide();
}

//-----------------------------------------------------------------------------

void SVNTimeline::onUpdateDone() {
  // Get the new log ...
  m_log.clear();
  m_timelineWidget->getListWidget()->clear();
  m_listWidgetitems.clear();
  removeTempFiles();
  m_currentExportIndex    = 0;
  m_currentAuxExportIndex = 0;

  // Update the filebrowser
  QStringList files;
  files.append(m_fileName);
  files.append(m_sceneResources);
  files.append(m_auxFiles);
  emit commandDone(files);

  m_updateButton->show();
  m_updateToRevisionButton->show();
  if (m_fileName.endsWith(".tnz")) m_sceneContentsCheckBox->show();

  QStringList args;
  args << "log" << m_fileName << "--xml";
  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)),
          SLOT(onLogDone(const QString &)));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNTimeline::onSceneContentsToggled(bool checked) {
  if (!checked)
    m_sceneResources.clear();
  else {
    VersionControl *vc = VersionControl::instance();
    m_sceneResources.append(vc->getSceneContents(m_workingDir, m_fileName));
  }
}
