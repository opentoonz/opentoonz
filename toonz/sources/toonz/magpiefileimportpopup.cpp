

#include "magpiefileimportpopup.h"
#include "tapp.h"
#include "tfilepath_io.h"
#include "tsystem.h"
#include "iocommand.h"
#include "flipbook.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/gutil.h"

#include <QLabel>
#include <QPushButton>
#include <QTextStream>
#include <QMainWindow>

//=============================================================================
// MagpieInfo
//-----------------------------------------------------------------------------

MagpieInfo::MagpieInfo(TFilePath path)
    : m_fileName(QString::fromStdWString(path.getWideName())) {
  QFile file(QString::fromStdWString(path.getWideString()));
  if (!file.open(QFile::ReadOnly)) return;
  QTextStream textStream(&file);
  QString line;
  do {
    line = textStream.readLine();
    // E' la prima riga
    if (line == QString("Toonz")) continue;
    if (!line.contains(L'|')) {
      if (!line.isEmpty()) m_actsIdentifier.append(line);
      continue;
    }
    QStringList list = line.split(QString("|"));
    assert(list.size() == 3);
    m_actorActs.append(list.at(1));
    m_comments.append(list.at(2));
  } while (!line.isNull());
}

//=============================================================================
// MagpieFileImportPopup
//-----------------------------------------------------------------------------

MagpieFileImportPopup::MagpieFileImportPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "MagPieFileImport")
    , m_levelField(0)
    , m_fromField(0)
    , m_toField(0)
    , m_flipbook(0)
    , m_levelPath() {
  setWindowTitle(tr("Import Toonz Lip Sync File"));

  beginVLayout();

  setLabelWidth(60);

  addSeparator(tr("Frame Range"));

  QWidget *fromToWidget = new QWidget(this);
  fromToWidget->setFixedHeight(DVGui::WidgetHeight);
  fromToWidget->setFixedSize(210, DVGui::WidgetHeight);
  QHBoxLayout *fromToLayout = new QHBoxLayout(fromToWidget);
  fromToLayout->setMargin(0);
  fromToLayout->setSpacing(0);
  m_fromField = new DVGui::IntLineEdit(fromToWidget, 1, 1, 1);
  fromToLayout->addWidget(m_fromField, 0, Qt::AlignLeft);
  m_toField       = new DVGui::IntLineEdit(fromToWidget, 1, 1, 1);
  QLabel *toLabel = new QLabel(tr("To: "));
  toLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  toLabel->setFixedSize(20, DVGui::WidgetHeight);
  fromToLayout->addWidget(toLabel, 0, Qt::AlignRight);
  fromToLayout->addWidget(m_toField, 0, Qt::AlignLeft);
  fromToWidget->setLayout(fromToLayout);
  addWidget(tr("From:"), fromToWidget);

  addSeparator(tr("Animation Level"));

  m_levelField = new DVGui::FileField(this);
  m_levelField->setFileMode(QFileDialog::AnyFile);
  m_levelField->setFixedWidth(200);
  bool ret =
      connect(m_levelField, SIGNAL(pathChanged()), SLOT(onLevelPathChanged()));

  addWidget(tr("Level:"), m_levelField);

  QLabel *frameLabel = new QLabel(" Frame", this);
  frameLabel->setFixedHeight(DVGui::WidgetHeight);
  frameLabel->setAlignment(Qt::AlignVCenter);
  addWidget(tr("Phoneme"), frameLabel);
  int i;
  for (i = 0; i < 9; i++) {
    DVGui::IntLineEdit *field = new DVGui::IntLineEdit(this, 1, 1);
    field->setFixedSize(54, DVGui::WidgetHeight);
    QLabel *label = new QLabel("", this);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setFixedSize(getLabelWidth(), DVGui::WidgetHeight);
    m_actFields.append(QPair<QLabel *, DVGui::IntLineEdit *>(label, field));
    addWidgets(label, field);
  }

  endVLayout();

  QFrame *frame = new QFrame(this);
  frame->setFrameStyle(QFrame::StyledPanel);
  frame->setObjectName("LipSynkViewer");
  frame->setStyleSheet(
      "#LipSynkViewer { border: 1px solid rgb(150,150,150); }");
  QVBoxLayout *frameLayout = new QVBoxLayout(frame);
  frameLayout->setMargin(0);
  frameLayout->setSpacing(0);
  std::vector<int> buttonMask = {FlipConsole::eRate,
                                 FlipConsole::eSound,
                                 FlipConsole::eSaveImg,
                                 FlipConsole::eHisto,
                                 FlipConsole::eCompare,
                                 FlipConsole::eCustomize,
                                 FlipConsole::eSave,
                                 FlipConsole::eBegin,
                                 FlipConsole::eEnd,
                                 FlipConsole::eFirst,
                                 FlipConsole::eNext,
                                 FlipConsole::ePause,
                                 FlipConsole::ePlay,
                                 FlipConsole::ePrev,
                                 FlipConsole::eRate,
                                 FlipConsole::eWhiteBg,
                                 FlipConsole::eCheckBg,
                                 FlipConsole::eBlackBg,
                                 FlipConsole::eNext,
                                 FlipConsole::eLast,
                                 FlipConsole::eLoop,
                                 FlipConsole::eGRed,
                                 FlipConsole::eGGreen,
                                 FlipConsole::eGBlue,
                                 FlipConsole::eRed,
                                 FlipConsole::eGreen,
                                 FlipConsole::eBlue,
                                 FlipConsole::eMatte,
                                 FlipConsole::eDefineSubCamera,
                                 FlipConsole::eDefineLoadBox,
                                 FlipConsole::eUseLoadBox,
                                 FlipConsole::eFilledRaster,
                                 FlipConsole::eLocator,
                                 FlipConsole::eZoomIn,
                                 FlipConsole::eZoomOut,
                                 FlipConsole::eFlipHorizontal,
                                 FlipConsole::eFlipVertical,
                                 FlipConsole::eResetView};
  m_flipbook = new FlipBook(this, tr("Import Toonz Lip Sync File"), buttonMask);
  m_flipbook->setFixedHeight(250);
  frameLayout->addWidget(m_flipbook);
  frame->setLayout(frameLayout);
  addWidget(frame);

  QPushButton *okBtn = new QPushButton(tr("Import"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  ret = ret && connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkPressed()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  assert(ret);

  addButtonBarWidget(okBtn, cancelBtn);
}

//-----------------------------------------------------------------------------

void MagpieFileImportPopup::setFilePath(TFilePath path) {
  m_info = new MagpieInfo(path);
}

//-----------------------------------------------------------------------------

void MagpieFileImportPopup::showEvent(QShowEvent *) {
  if (m_info == 0) return;

  int frameCount = m_info->getFrameCount();
  m_fromField->setRange(1, frameCount);
  m_fromField->setValue(1);
  m_toField->setRange(1, frameCount);
  m_toField->setValue(frameCount);

  int i;
  QList<QString> actsIdentifier = m_info->getActsIdentifier();
  for (i = 0; i < m_actFields.size(); i++) {
    DVGui::IntLineEdit *field = m_actFields.at(i).second;
    QLabel *label             = m_actFields.at(i).first;
    if (i >= actsIdentifier.size()) {
      field->hide();
      label->hide();
      continue;
    }
    QString act = actsIdentifier.at(i);
    field->setProperty("act", QVariant(act));
    field->show();
    label->setText(act + ":");
    label->show();
  }
  QString oldLevelPath = m_levelField->getPath();
  TFilePath oldFilePath(oldLevelPath.toStdWString());
  TFilePath perntDir = oldFilePath.getParentDir();
  m_levelField->setPath(QString::fromStdWString(perntDir.getWideString()));
}

//-----------------------------------------------------------------------------

void MagpieFileImportPopup::hideEvent(QHideEvent *) {
  // Devo svuotare il flibook
  emit closeButtonPressed();
}

//-----------------------------------------------------------------------------

void MagpieFileImportPopup::onLevelPathChanged() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath levelPath(m_levelField->getPath().toStdWString());
  levelPath = scene->decodeFilePath(levelPath);
  if (levelPath.isEmpty() || levelPath.getUndottedType().empty() ||
      !TSystem::doesExistFileOrLevel(levelPath)) {
    DVGui::error(tr("The file path is missing."));
    return;
  }
  m_levelPath = levelPath;

  std::string format = m_levelPath.getType();
  if (format == "tzp" || format == "tzu") {
    std::wstring name                       = m_levelPath.getWideName();
    IoCmd::ConvertingPopup *convertingPopup = new IoCmd::ConvertingPopup(
        TApp::instance()->getMainWindow(),
        QString::fromStdWString(name) +
            QString::fromStdString(m_levelPath.getDottedType()));
    convertingPopup->show();

    bool ok = scene->convertLevelIfNeeded(m_levelPath);
    convertingPopup->hide();
    if (!ok) return;
  }
  m_flipbook->setLevel(m_levelPath);
}

//-----------------------------------------------------------------------------

void MagpieFileImportPopup::onOkPressed() {
  if (m_levelPath.isEmpty() || m_levelPath.getUndottedType().empty() ||
      !TSystem::doesExistFileOrLevel(m_levelPath)) {
    DVGui::error(tr("The file path is missing."));
    return;
  }

  QList<QString> actorActs = m_info->getActorActs();
  QList<QString> comments  = m_info->getComments();
  int from                 = m_fromField->getValue() - 1;
  int to                   = m_toField->getValue() - 1;
  assert(to < m_info->getFrameCount());

  QList<TFrameId> frameList;
  QList<QString> commentList;
  int commentCount   = comments.size();
  int actorActsCount = actorActs.size();
  for (int i = from; i <= to; i++) {
    if (commentCount <= i || actorActsCount <= i) continue;
    commentList.append(comments.at(i));
    QString actorAct = actorActs.at(i);
    if (actorAct == QString("<none>")) {
      frameList.push_back(TFrameId());
      continue;
    }
    for (int j = 0; j < m_actFields.size(); j++) {
      DVGui::IntLineEdit *field = m_actFields.at(j).second;
      QString act               = field->property("act").toString();
      if (actorAct != act) continue;
      frameList.push_back(TFrameId(field->getValue()));
      break;
    }
  }

  bool ret = IoCmd::importLipSync(m_levelPath, frameList, commentList,
                                  m_info->getFileName());
  if (!ret) return;
  accept();
}
