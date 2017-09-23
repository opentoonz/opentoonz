

#include "cleanuppopup.h"

// Toonz includes
#include "tapp.h"
#include "imageviewer.h"
#include "cleanupsettingsmodel.h"
#include "cellselection.h"
#include "columnselection.h"
#include "mainwindow.h"

// ToonzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/icongenerator.h"

// ToonzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/imagemanager.h"
#include "toonz/levelupdater.h"
#include "toonz/tcleanupper.h"
#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "tsystem.h"
#include "tlevel_io.h"
#include "timageinfo.h"
#include "tstream.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QMainWindow>
#include <QGroupBox>

// boost includes
#include <boost/bind.hpp>
#include <boost/functional.hpp>
#include <boost/mem_fn.hpp>

// STL includes
#include <set>
#include <map>
#include <numeric>

//*****************************************************************************
//    Local namespace stuff
//*****************************************************************************

namespace {

enum Resolution {
  NO_RESOLUTION,  //!< No required resolution.
  CANCEL,         //!< Validation was canceled.
  OVERWRITE,   //!< Does not delete old cleanupped levels, but overwrites found
               //! frames.
  WRITE_NEW,   //!< Like above, but does not overwrite. Just adds not cleanupped
               //! frames.
  REPLACE,     //!< Destroy the old level and build one anew.
  ADD_SUFFIX,  //!< Add a suffix to the output path.
  NOPAINT_ONLY  //!< overwrite the result only in "nopaint" folder
};

//-----------------------------------------------------------------------------

static const std::wstring unpaintedStr = L"-unpainted";

//-----------------------------------------------------------------------------

inline QString suffix(int num) { return QString("_") + QString::number(num); }

//-----------------------------------------------------------------------------

inline TFilePath withSuffix(const TFilePath &fp, int num) {
  return fp.withName(fp.getWideName() + suffix(num).toStdWString());
}

//-----------------------------------------------------------------------------

inline bool exists(const TFilePath &fp) {
  return TSystem::doesExistFileOrLevel(
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(fp));
}

//-----------------------------------------------------------------------------

inline bool exists(const TFilePath &fp, int num) {
  return exists(withSuffix(fp, num));
}

//-----------------------------------------------------------------------------

void loadCleanupParams(CleanupParameters *params, TXshSimpleLevel *sl) {
  params->assign(CleanupSettingsModel::instance()->getCurrentParameters());
  CleanupSettingsModel::loadSettings(params,
                                     CleanupSettingsModel::getClnPath(sl));
}

//-----------------------------------------------------------------------------

double getBestFactor(QSize viewSize, QSize imageSize) {
  if (abs(viewSize.width() - imageSize.width()) >
      abs(viewSize.height() - imageSize.height())) {
    if (viewSize.width() > imageSize.width())
      return double(imageSize.width()) / double(viewSize.width());
    else
      return double(viewSize.width()) / double(imageSize.width());
  } else {
    if (viewSize.height() > imageSize.height())
      return double(imageSize.height()) / double(viewSize.height());
    else
      return double(viewSize.height()) / double(imageSize.height());
  }

  return 0;
}

//-----------------------------------------------------------------------------
/*! cleanup後のファイルlevelPathに対してUnpaintedファイルを作る。
        Cleanup後のUnpaintedの保存先を1階層下げる（nopaintフォルダ内に入れ、
        "A_np.tlv"のように"_np"を付ける。"_unpainted"は長いので）
        Paletteをキープするかどうかのフラグを追加
*/
void saveUnpaintedLevel(const TFilePath &levelPath, TXshSimpleLevel *sl,
                        std::vector<TFrameId> fids, bool keepOriginalPalette) {
  try {
    /*---nopaintフォルダの作成---*/
    TFilePath nopaintDir = levelPath.getParentDir() + "nopaint";
    if (!TFileStatus(nopaintDir).doesExist()) {
      try {
        TSystem::mkDir(nopaintDir);
      } catch (...) {
        return;
      }
    }

    TFilePath unpaintedLevelPath =
        levelPath.getParentDir() + "nopaint\\" +
        TFilePath(levelPath.getName() + "_np." + levelPath.getType());

    if (!TSystem::doesExistFileOrLevel(unpaintedLevelPath)) {
      // No unpainted level exists. So, just copy the output file.
      TSystem::copyFile(unpaintedLevelPath, levelPath);
      if (keepOriginalPalette) return;

      TFilePath levelPalettePath(levelPath.withType("tpl"));
      TFilePath unpaintedLevelPalettePath =
          levelPalettePath.getParentDir() + "nopaint\\" +
          TFilePath(levelPalettePath.getName() + "_np." +
                    levelPalettePath.getType());

      TSystem::copyFile(unpaintedLevelPalettePath, levelPalettePath);

      return;
    }

    TLevelWriterP lw(unpaintedLevelPath);

    if (keepOriginalPalette) lw->setOverwritePaletteFlag(false);

    int i, fidsCount = fids.size();
    for (i = 0; i < fidsCount; ++i) {
      const TFrameId &fid = fids[i];

      TToonzImageP ti = sl->getFrame(fid, false);
      if (!ti) continue;

      lw->getFrameWriter(fid)->save(ti);
    }
  } catch (...) {
  }
}

//------------------------------------------------------------------------------
/*! Cleanup後のデフォルトPaletteを追加する。
TODO:
Cleanup後にデフォルトPaletteの内容を追加する仕様、Preferencesでオプション化
2016/1/16 shun_iwasawa
*/
void addCleanupDefaultPalette(TXshSimpleLevelP sl) {
  /*--- CleanupデフォルトパレットはStudioPaletteフォルダ内に入れる ---*/
  TFilePath palettePath =
      ToonzFolder::getStudioPaletteFolder() + "cleanup_default.tpl";
  TFileStatus pfs(palettePath);

  if (!pfs.doesExist() || !pfs.isReadable()) {
    DVGui::warning(
        QString("CleanupDefaultPalette file: %1 is not found!")
            .arg(QString::fromStdWString(palettePath.getWideString())));
    return;
  }

  TIStream is(palettePath);
  if (!is) {
    DVGui::warning(
        QString("CleanupDefaultPalette file: failed to get TIStream"));
    return;
  }

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "palette") {
    DVGui::warning(
        QString("CleanupDefaultPalette file: This is not palette file"));
    return;
  }

  std::string gname;
  is.getTagParam("name", gname);
  TPalette *defaultPalette = new TPalette();
  defaultPalette->loadData(is);

  sl->getPalette()->setIsCleanupPalette(false);

  TPalette::Page *dstPage = sl->getPalette()->getPage(0);
  TPalette::Page *srcPage = defaultPalette->getPage(0);

  for (int srcIndexInPage = 0; srcIndexInPage < srcPage->getStyleCount();
       srcIndexInPage++) {
    int id = srcPage->getStyleId(srcIndexInPage);

    bool isUsedInCleanupPalette;
    isUsedInCleanupPalette = false;

    for (int dstIndexInPage = 0; dstIndexInPage < dstPage->getStyleCount();
         dstIndexInPage++) {
      if (dstPage->getStyleId(dstIndexInPage) == id) {
        isUsedInCleanupPalette = true;
        break;
      }
    }

    if (isUsedInCleanupPalette)
      continue;

    else {
      int addedId = sl->getPalette()->addStyle(
          srcPage->getStyle(srcIndexInPage)->clone());
      dstPage->addStyle(addedId);
      /*---
       * StudioPalette由来のDefaultPaletteの場合、GrobalName(リンク)を消去する
       * ---*/
      sl->getPalette()->getStyle(addedId)->setGlobalName(L"");
      sl->getPalette()->getStyle(addedId)->setOriginalName(L"");
    }
  }
  delete defaultPalette;
}

}  // namespace

//*****************************************************************************
//    CleanupLevel  definition
//*****************************************************************************

struct CleanupPopup::CleanupLevel {
  TXshSimpleLevel *m_sl;           //!< Level to be cleanupped.
  TFilePath m_outputPath;          //!< Output path for the cleanupped level.
  std::vector<TFrameId> m_frames;  //!< Frames to cleanup.
  Resolution m_resolution;         //!< Resolution for verified file conflicts.

public:
  CleanupLevel(TXshSimpleLevel *sl, const CleanupParameters &params)
      : m_sl(sl)
      , m_outputPath(CleanupSettingsModel::getOutputPath(m_sl, &params))
      , m_resolution(NO_RESOLUTION) {}

  bool empty() const { return m_frames.empty(); }
};

//*****************************************************************************
//    CleanupPopup implementation
//*****************************************************************************

CleanupPopup::CleanupPopup()
    : QDialog(TApp::instance()->getMainWindow())
    , m_params(new CleanupParameters)
    , m_updater(new LevelUpdater)
    , m_originalLevelPath()
    , m_originalPalette(0)
    , m_firstLevelFrame(true) {
  setWindowTitle(tr("Cleanup"));
  // Progress Bar
  m_progressLabel = new QLabel(tr("Cleanup in progress"));
  m_progressBar   = new QProgressBar;
  // Text
  m_cleanupQuestionLabel = new QLabel(tr("Do you want to cleanup this frame?"));

  m_imageViewer = new ImageViewer(0, 0, false);

  // Buttons
  m_cleanupButton           = new QPushButton(tr("Cleanup"));
  m_skipButton              = new QPushButton(tr("Skip"));
  m_cleanupAllButton        = new QPushButton(tr("Cleanup All"));
  QPushButton *cancelButton = new QPushButton(tr("Cancel"));
  m_imgViewBox              = new QGroupBox(tr("View"), this);

  m_imgViewBox->setCheckable(true);
  m_imgViewBox->setChecked(false);
  m_imageViewer->setVisible(false);
  m_imageViewer->resize(406, 306);

  //---layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);
  {
    mainLayout->addWidget(m_progressLabel, 0);
    mainLayout->addWidget(m_progressBar, 0);

    mainLayout->addWidget(m_cleanupQuestionLabel);

    QVBoxLayout *imgBoxLay = new QVBoxLayout();
    imgBoxLay->setMargin(5);
    { imgBoxLay->addWidget(m_imageViewer); }
    m_imgViewBox->setLayout(imgBoxLay);
    mainLayout->addWidget(m_imgViewBox, 1);

    QHBoxLayout *buttonLay = new QHBoxLayout();
    buttonLay->setMargin(0);
    buttonLay->setSpacing(5);
    {
      buttonLay->addWidget(m_cleanupButton);
      buttonLay->addWidget(m_skipButton);
      buttonLay->addWidget(m_cleanupAllButton);

      buttonLay->addWidget(cancelButton);
    }
    mainLayout->addLayout(buttonLay);
    mainLayout->addStretch();
  }
  setLayout(mainLayout);

  //--- signal-slot connections

  bool ret = true;
  ret      = ret && connect(m_progressBar, SIGNAL(valueChanged(int)), this,
                       SLOT(onValueChanged(int)));

  // NOTE: On MAC it seems that QAbstractButton's pressed() signal is reemitted
  // at
  // every mouseMoveEvent when the button is pressed...
  // This is why clicked() substitutes pressed() below.

  ret = ret && connect(m_cleanupButton, SIGNAL(clicked()), this,
                       SLOT(onCleanupFrame()));
  ret = ret &&
        connect(m_skipButton, SIGNAL(clicked()), this, SLOT(onSkipFrame()));
  ret = ret && connect(m_cleanupAllButton, SIGNAL(clicked()), this,
                       SLOT(onCleanupAllFrame()));
  ret = ret &&
        connect(cancelButton, SIGNAL(clicked()), this, SLOT(onCancelCleanup()));
  ret = ret && connect(m_imgViewBox, SIGNAL(toggled(bool)), this,
                       SLOT(onImgViewBoxToggled(bool)));

  assert(ret);

  reset();  // Initialize remaining variables

  resize(450, 400);
}

//-----------------------------------------------------------------------------

CleanupPopup::~CleanupPopup() {}

//-----------------------------------------------------------------------------

void CleanupPopup::closeEvent(QCloseEvent *ce) { reset(); }

//-----------------------------------------------------------------------------

void CleanupPopup::reset() {
  closeLevel();

  m_idx = m_completion = std::pair<int, int>(-1, -1);
  m_cleanupLevels.clear();

  m_imageViewer->setImage(TImageP());

  TCleanupper::instance()->setParameters(
      CleanupSettingsModel::instance()->getCurrentParameters());

  m_cleanupQuestionLabel->show();

  m_levelAlreadyExists.clear();

  /*---タイトルバーを元の表示に戻す---*/
  MainWindow *mainWin =
      qobject_cast<MainWindow *>(TApp::instance()->getMainWindow());
  if (mainWin) mainWin->changeWindowTitle();
}

//-----------------------------------------------------------------------------

void CleanupPopup::buildCleanupList() {
  struct locals {
    static inline bool supportsCleanup(TXshSimpleLevel *sl) {
      return (
          sl->getType() & FULLCOLOR_TYPE ||
          (sl->getType() == TZP_XSHLEVEL && !sl->getScannedPath().isEmpty()));
    }
  };  // locals

  typedef std::set<TFrameId> FramesList;

  /*--- これからCleanupするLevel/FrameIdのリスト ---*/
  std::map<TXshSimpleLevel *, FramesList>
      cleanupList;  // List of frames to be cleanupped
  std::vector<TXshSimpleLevel *>
      levelsList;  // List of levels in the cleanup list,
                   // ordered as found in the xsheet.
  m_cleanupLevels.clear();

  // Retrieve current selection
  TCellSelection *selection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  TColumnSelection *columnSel =
      dynamic_cast<TColumnSelection *>(TSelection::getCurrent());
  /*--- セル選択でも、カラム選択でも無い場合はCleanup自体を無効にする ---*/
  if (!selection && !columnSel) return;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  /*--- セル選択の場合 ---*/
  if (selection) {
    if (selection->isEmpty()) return;
    // Store frames in the specified selection
    int r0, c0, r1, c1;
    selection->getSelectedCells(r0, c0, r1, c1);

    int r, c;
    for (c = c0; c <= c1; ++c) {
      for (r = r0; r <= r1; ++r) {
        /*---選択範囲内にLevelが無い場合はcontinue---*/
        const TXshCell &cell = xsh->getCell(r, c);
        if (cell.isEmpty()) continue;
        TXshSimpleLevel *sl = cell.getSimpleLevel();
        if (!(sl && locals::supportsCleanup(sl))) continue;
        /*---もし新しいLevelなら、Levelのリストに登録---*/
        std::map<TXshSimpleLevel *, FramesList>::iterator it =
            cleanupList.find(sl);
        if (it == cleanupList.end()) {
          it = cleanupList.insert(std::make_pair(sl, FramesList())).first;
          levelsList.push_back(sl);
        }
        /*---TFrameIdを登録---*/
        it->second.insert(cell.getFrameId()).second;
      }
    }
  }
  /*--- カラム選択の場合 ---*/
  else {
    int frameCount = xsh->getFrameCount();
    if (columnSel->isEmpty() || frameCount <= 0) return;
    /*--- 選択された各カラムについて ---*/
    std::set<int>::const_iterator it = columnSel->getIndices().begin();
    for (; it != columnSel->getIndices().end(); ++it) {
      int c = (*it);
      for (int r = 0; r < frameCount; r++) {
        /*--- 選択範囲内にLevelが無い場合はcontinue ---*/
        const TXshCell &cell = xsh->getCell(r, c);
        if (cell.isEmpty()) continue;
        TXshSimpleLevel *sl = cell.getSimpleLevel();
        if (!sl && locals::supportsCleanup(sl)) continue;
        /*---もし新しいLevelなら、Levelのリストに登録---*/
        std::map<TXshSimpleLevel *, FramesList>::iterator it =
            cleanupList.find(sl);
        if (it == cleanupList.end()) {
          it = cleanupList.insert(std::make_pair(sl, FramesList())).first;
          levelsList.push_back(sl);
        }
        /*---TFrameIdを登録---*/
        it->second.insert(cell.getFrameId()).second;
      }
    }
  }

  // Finally, copy the retrieved data to the sorted output vector
  std::vector<TXshSimpleLevel *>::iterator lt, lEnd = levelsList.end();
  for (lt = levelsList.begin(); lt != lEnd; ++lt) {
    loadCleanupParams(
        m_params.get(),
        *lt);  // Load cleanup parameters associated with current level.
    // This is necessary since the output path is specified among them.
    m_cleanupLevels.push_back(CleanupLevel(*lt, *m_params.get()));
    CleanupLevel &cl = m_cleanupLevels.back();

    FramesList &framesList = cleanupList[cl.m_sl];
    cl.m_frames.assign(framesList.begin(), framesList.end());
  }
}

//-----------------------------------------------------------------------------

bool CleanupPopup::analyzeCleanupList() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  bool shownOverwriteDialog = false, shownWritingOnSourceFile = false;

  /*--- 消されるLevel名の確認ダイアログを出すため ---*/
  QList<TFilePath> filePathsToBeDeleted;
  /*	Cleanup前に既存のLevelを消す場合、セルのStatusをScannedに変更するために、
          TXshSimpleLevelポインタを格納しておく
  */
  QList<TXshSimpleLevel *> levelsToBeDeleted;

  // Traverse the cleanup list
  bc::vector<CleanupLevel>::iterator clt, clEnd = m_cleanupLevels.end();
  for (clt = m_cleanupLevels.begin(); clt != clEnd; ++clt) {
    TXshSimpleLevel *sl = clt->m_sl;

    /*--- Cleanup対象LevelのCleanupSettingを取得 ---*/
    loadCleanupParams(m_params.get(),
                      sl);  // Needed to retrieve output level resolution

    // Check level existence
    /*--- Cleanup後に得られるであろうTLVのパス ---*/
    TFilePath outputPath = scene->decodeFilePath(clt->m_outputPath);
    {
      /*-- 出力先にTLVファイルが無ければ問題なし(このLevelはCleanupする) --*/
      if (!TSystem::doesExistFileOrLevel(outputPath)) {
        m_levelAlreadyExists[sl] = false;
        continue;  // Newly cleaned up level. No problem.
      } else
        m_levelAlreadyExists[sl] = true;

      // Check whether output file == input file
      /*--- 入力となるScanned（TIFなど）のパスを得る ---*/
      const TFilePath &inputPath =
          scene->decodeFilePath(CleanupSettingsModel::getInputPath(sl));

      if (!shownWritingOnSourceFile && inputPath == outputPath) {
        shownWritingOnSourceFile = true;

        int ret = DVGui::MsgBox(tr("Selected drawings will overwrite the "
                                   "original files after the cleanup process.\n"
                                   "Do you want to continue?"),
                                tr("Ok"), tr("Cancel"));

        if (ret != 1) return false;
      }

      /*---一度も上書き確認のダイアログを出していなかったら、ここで出す---*/
      // Reset the overwrite dialog
      if (!shownOverwriteDialog) {
        shownOverwriteDialog = true;

        if (!m_overwriteDialog)
          m_overwriteDialog.reset(new OverwriteDialog);
        else
          m_overwriteDialog->reset();
      }

      // Prompt user for file conflict resolution
      switch (clt->m_resolution =
                  Resolution(m_overwriteDialog->execute(&clt->m_outputPath))) {
      case CANCEL:
        return false;

      case NO_RESOLUTION:
        assert(false);
        break;
      case REPLACE:
        /*--- 既存のTLVを消すオプションの場合、消されるファイルのリストを作る
         * ---*/
        break;
      case ADD_SUFFIX:
        continue;
      case NOPAINT_ONLY:
        /*--- NOPAINT_ONLY の場合は、nopaintのみを変更。
                ただし、nopaintのLevelは消さず、処理したフレームを Overwrite
        する
        ---*/
        outputPath =
            outputPath.getParentDir() + "nopaint\\" +
            TFilePath(outputPath.getName() + "_np." + outputPath.getType());
        /*--- nopaintの有無を確かめる。無ければ次のLevelへ
         * (このLevelはCleanupする) ---*/
        if (!TSystem::doesExistFileOrLevel(outputPath)) {
          m_levelAlreadyExists[sl] = false;
          continue;
        }
      }

      TLevelP level(0);  // Current level info. Yeah the init is a shame... :(
      /*--- 元のLevelと新しいCleanup結果が混合する場合。REPLACE以外 ---*/
      if (clt->m_resolution == OVERWRITE || clt->m_resolution == WRITE_NEW ||
          clt->m_resolution == NOPAINT_ONLY) {
        // Check output resolution consistency
        // Retrieve file resolution
        /*---現在在るTLVのサイズと、CleanupSettingsのサイズが一致しているかチェック---*/
        TDimension oldRes(0, 0);
        try {
          TLevelReaderP lr(outputPath);
          level = lr->loadInfo();

          if (const TImageInfo *imageInfo = lr->getImageInfo())
            oldRes = TDimension(imageInfo->m_lx, imageInfo->m_ly);
          else
            throw TException();
        } catch (...) {
          // There was a problem reading the existing level data.
          // Thus, the conservative approach is not feasible.

          // Inform the user and abort cleanup
          DVGui::warning(
              tr("There were errors opening the existing level "
                 "\"%1\".\n\nPlease choose to delete the existing level and "
                 "create a new one\nwhen running the cleanup process.")
                  .arg(QString::fromStdWString(outputPath.getLevelNameW())));

          return false;
        }

        // Retrieve output resolution
        TDimension outRes(0, 0);
        TPointD outDpi;
        m_params->getOutputImageInfo(outRes, outDpi.x, outDpi.y);

        if (oldRes != outRes) {
          DVGui::warning(
              tr("The resulting resolution of level \"%1\"\ndoes not match "
                 "with that of previously cleaned up level drawings.\n\nPlease "
                 "set the right camera resolution and closest field, or choose "
                 "to delete\nthe existing level and create a new one when "
                 "running the cleanup process.")
                  .arg(QString::fromStdWString(outputPath.getLevelNameW())));

          return false;
        }
      }
      /*--- REPLACEの場合、消されるファイルパスのリストを作る ---*/
      else if (clt->m_resolution == REPLACE) {
        filePathsToBeDeleted.push_back(outputPath);

        levelsToBeDeleted.push_back(sl);

        /*--- パレットファイルも、あれば消す ---*/
        TFilePath palettePath =
            (outputPath.getParentDir() + outputPath.getName()).withType("tpl");
        if (TSystem::doesExistFileOrLevel(palettePath))
          filePathsToBeDeleted.push_back(palettePath);
        /*--- つぎに、nopaintのTLV。これは、REPLACE、NOPAINT_ONLY 両方で消す
         * ---*/
        TFilePath unpaintedLevelPath =
            outputPath.getParentDir() + "nopaint\\" +
            TFilePath(outputPath.getName() + "_np." + outputPath.getType());
        if (TSystem::doesExistFileOrLevel(unpaintedLevelPath)) {
          filePathsToBeDeleted.push_back(unpaintedLevelPath);
          filePathsToBeDeleted.push_back(
              (unpaintedLevelPath.getParentDir() + unpaintedLevelPath.getName())
                  .withType("tpl"));
        }
      }

      // Finally, apply resolution to individual frames.
      /*--- WRITE_NEW は、「未Cleanupのフレームだけ処理する」オプション ---*/
      if (clt->m_resolution == WRITE_NEW) {
        const TLevel::Table *table = level->getTable();

        clt->m_frames.erase(
            std::remove_if(clt->m_frames.begin(), clt->m_frames.end(),
                           [table](TLevel::Table::key_type const &key) {
                             return table->count(key);
                           }),
            clt->m_frames.end());
      }
    }
  }

  /*--- ファイル消去の確認ダイアログを表示 ---*/
  if (!filePathsToBeDeleted.isEmpty()) {
    QString question = QObject::tr(
        "Delete and Re-cleanup : The following files will be deleted.\n\n");
    for (int i = 0; i < filePathsToBeDeleted.size(); i++) {
      question +=
          "   " +
          QString::fromStdWString(filePathsToBeDeleted[i].getWideString()) +
          "\n";
    }
    question += QObject::tr("\nAre you sure ?");

    int ret = DVGui::MsgBox(question, QObject::tr("Delete"),
                            QObject::tr("Cancel"), 0);
    if (ret == 0 || ret == 2) {
      return false;
    } else if (ret == 1) {
      /*--- 先にCleanup処理で出力先となるファイルを消す ---*/
      try {
        for (int i = 0; i < filePathsToBeDeleted.size(); i++) {
          TSystem::removeFileOrLevel_throw(filePathsToBeDeleted[i]);
        }
      } catch (...) {
        return false;
      }

      // Reset level status
      for (int i = 0; i < levelsToBeDeleted.size(); i++) {
        TXshSimpleLevel *lev = levelsToBeDeleted.at(i);
        /*--- TLVだった場合、Scanned(TIFレベル)に戻す ---*/
        TFilePath scannedPath = lev->getScannedPath();
        if (scannedPath != TFilePath()) {
          lev->setScannedPath(TFilePath());
          lev->setPath(scannedPath, true);
          lev->clearFrames();
          lev->setType(OVL_XSHLEVEL);  // OVL_XSHLEVEL
          lev->setPalette(0);
          if (lev == TApp::instance()->getCurrentLevel()->getLevel())
            TApp::instance()
                ->getPaletteController()
                ->getCurrentLevelPalette()
                ->setPalette(0);

          lev->load();
          int i, frameCount = lev->getFrameCount();
          for (i = 0; i < frameCount; i++) {
            TFrameId id = lev->index2fid(i);
            IconGenerator::instance()->invalidate(lev, id);
          }
          IconGenerator::instance()->invalidateSceneIcon();
        }
      }
    }
  }

  // Before returning, erase levels whose frames list is empty
  /*--- Cleanup対象フレームが無くなったLevelを対象から外す ---*/
  m_cleanupLevels.erase(
      std::remove_if(m_cleanupLevels.begin(), m_cleanupLevels.end(),
                     boost::mem_fn(&CleanupLevel::empty)),
      m_cleanupLevels.end());

  return true;
}

//-----------------------------------------------------------------------------

bool CleanupPopup::isValidPosition(const std::pair<int, int> &pos) const {
  if (pos.first < 0 || int(m_cleanupLevels.size()) <= pos.first) return false;

  const CleanupLevel &cl = m_cleanupLevels[pos.first];

  if (pos.second < 0 || int(cl.m_frames.size()) <= pos.second) return false;

  return true;
}

//-----------------------------------------------------------------------------

QString CleanupPopup::currentString() const {
  if (!isValidPosition(m_idx)) return QString();

  const CleanupLevel &cl = m_cleanupLevels[m_idx.first];

  TXshSimpleLevel *sl = cl.m_sl;
  const TFrameId &fid = cl.m_frames[m_idx.second];

  TFilePath scannedPath(sl->getScannedPath());
  if (scannedPath.isEmpty()) scannedPath = sl->getPath();

  TFilePath levelName(scannedPath.getLevelNameW());
  QString imageName = toQString(levelName.withFrame(fid));

  return tr("Cleanup in progress: ") + imageName + " " +
         QString::number(m_completion.first) + "/" +
         QString::number(m_completion.second);
}

//-----------------------------------------------------------------------------
/*--- これからCleanupするフレームを取得 ---*/
TImageP CleanupPopup::currentImage() const {
  if (!isValidPosition(m_idx)) return TImageP();

  const CleanupLevel &cl = m_cleanupLevels[m_idx.first];
  return cl.m_sl->getFrameToCleanup(cl.m_frames[m_idx.second]);
}

//-----------------------------------------------------------------------------

void CleanupPopup::execute() {
  struct locals {
    static inline int addFrames(int a, const CleanupLevel &cl) {
      return a + int(cl.m_frames.size());
    }
  };  // locals

  // Re-initialize the list of frames to be cleanupped
  /*--- Cleanup対象のリストを作る ---*/
  buildCleanupList();

  // In case some cleaned up level already exists, let the user decide what to
  // do
  if (!analyzeCleanupList()) {
    reset();
    return;
  }

  // Initialize completion variable
  m_completion = std::pair<int, int>(
      0, std::accumulate(m_cleanupLevels.begin(), m_cleanupLevels.end(), 0,
                         locals::addFrames));

  // If there are no (more) frames to cleanup, warn and quit
  int framesCount = m_completion.second;
  if (!framesCount) {
    DVGui::info(
        tr("It is not possible to cleanup: the cleanup list is empty."));

    reset();
    return;
  }

  // Initialize the cleanup process and show the popup

  m_idx = std::pair<int, int>(0, 0);

  // Reset progress bar
  m_progressLabel->setText(currentString());

  m_progressBar->setRange(0, framesCount);
  m_progressBar->setValue(0);

  // show the progress to the main window's title bar
  updateTitleString();

  TImageP image = currentImage();
  if (image) {
    m_imageViewer->setImage(image);

    // Set the zoom factor depending on image and viewer sizes, so that the
    // image fits
    // the preview area.

    QSize viewSize  = m_imageViewer->size();
    QSize imageSize = QSize(image->getBBox().getLx(), image->getBBox().getLy());
    double factor   = getBestFactor(viewSize, imageSize);
    TPointD delta(0, 0);
    TAffine viewAff =
        TTranslation(delta) * TScale(factor) * TTranslation(-delta);
    m_imageViewer->setViewAff(viewAff);
  }

  show();
}

//-----------------------------------------------------------------------------

QString CleanupPopup::setupLevel() {
  // Level's pre-cleanup stuff initialization.
  // Invoked right before the cleanupFrame() of the first level frame.

  assert(isValidPosition(m_idx));

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  /*--- これからCleanupするLevel ---*/
  CleanupLevel &cl    = m_cleanupLevels[m_idx.first];
  TXshSimpleLevel *sl = cl.m_sl;

  /*---  保存先のTLVが既に存在する、かつ、REPLACE でも
          NOPAINT_ONLY でもない場合、Paletteを変更せず維持する ---*/
  if (cl.m_resolution != REPLACE && cl.m_resolution != NOPAINT_ONLY &&
      m_levelAlreadyExists[sl] == true)
    m_keepOriginalPalette = true;
  else
    m_keepOriginalPalette = false;

  // Update cleanup parameters, loading a cln if necessary
  TCleanupper *cleanupper = TCleanupper::instance();

  /*--- CleanupSettingsを読み込み、TCleanupperに渡す ---*/
  loadCleanupParams(m_params.get(), sl);
  cleanupper->setParameters(m_params.get());

  // Touch the output parent directory
  TFilePath &outputPath = cl.m_outputPath;

  /*--- 保存先PathをFull Pathにする ---*/
  TFilePath decodedPath(scene->decodeFilePath(outputPath));
  if (!TSystem::touchParentDir(decodedPath))
    return tr("Couldn't create directory \"%1\"").arg(decodedPath.getQString());

  /*---
   * 上書きオプションが選択されているとき、既存のLevel,Palette,Nopaintファイルを消す
   * ---*/
  // If the user decided to remove any existing level, do so now.
  if (cl.m_resolution == REPLACE) {
    const QString &err = resetLevel();

    if (!err.isEmpty()) return err;
  }
  /*--- Nopaintのみ上書きのオプションが選択されているとき（再Clenaupの場合）
     ---*/
  else if (cl.m_resolution == NOPAINT_ONLY) {
    m_originalLevelPath = outputPath;
    /*--- 必要なら、nopaintフォルダを作成 ---*/
    TFilePath nopaintDir = decodedPath.getParentDir() + "nopaint";
    if (!TFileStatus(nopaintDir).doesExist()) {
      try {
        TSystem::mkDir(nopaintDir);
      } catch (...) {
        return NULL;
      }
    }
    /*--- 保存先のパスをnopaintの方に変更 ---*/
    outputPath =
        outputPath.getParentDir() + "nopaint\\" +
        TFilePath(outputPath.getName() + "_np." + outputPath.getType());
    decodedPath = scene->decodeFilePath(outputPath);
  }

  // Frames are cleaned-up at full-sampling. Thus, clear subsampling on the
  // level.
  if (sl->getProperties()->getSubsampling() != 1) {
    sl->getProperties()->setSubsampling(1);
    sl->invalidateFrames();
  }

  bool lineProcessing   = (m_params->m_lineProcessingMode != lpNone),
       notLineProcessed = (sl->getType() != TZP_XSHLEVEL);

  if (lineProcessing) {
    if (notLineProcessed) {
      /*-- Type, Pathを切り替えてTLVにする --*/
      // The level type changes to TLV
      sl->makeTlv(outputPath);

      // Remap all current images under the IM control to Scanned status.
      int f, fCount = sl->getFrameCount();

      for (f = 0; f != fCount; ++f) {
        const TFrameId &fid = sl->getFrameId(f);

        /*--- 「スキャン済み」のステータスにし、画像、アイコンのIDを切り替える
         * ---*/
        sl->setFrameStatus(fid, TXshSimpleLevel::Scanned);
        ImageManager::instance()->rebind(
            sl->getImageId(fid, 0),
            sl->getImageId(fid, TXshSimpleLevel::Scanned));

        const std::string &oldIconId = sl->getIconId(fid, 0);
        const std::string &newIconId =
            sl->getIconId(fid, TXshSimpleLevel::Scanned);

        IconGenerator::instance()->remap(newIconId, oldIconId);
      }
    }
    /*--- 対象が既にTLVファイルで、出力先パスが違う場合、切り替える ---*/
    else if (outputPath != sl->getPath()) {
      // Just wants to be written to a different destination path. Update it.
      sl->setPath(outputPath, false);
    }

    /*-- Cleanup用のパレットを作る --*/
    // Update the level palette
    TPaletteP palette =
        TCleanupper::instance()->createToonzPaletteFromCleanupPalette();

    /*--- Cleanup後にPaletteを元に戻すため、Paletteを保持しておく ---*/
    if (m_keepOriginalPalette) {
      if ((sl->getType() == TZP_XSHLEVEL || sl->getType() == TZI_XSHLEVEL) &&
          sl->getPalette() != NULL)
        m_originalPalette = sl->getPalette()->clone();
      else /*--- 既にCleanup済みだが、再びTIFファイルからCleanupを行う場合 ---*/
      {
        /*--- Cleanup先のPaletteをロードして取っておく ---*/
        TFilePath targetPalettePath = outputPath.getParentDir() +
                                      TFilePath(outputPath.getName() + ".tpl");
        TFileStatus pfs(targetPalettePath);
        if (pfs.doesExist() && pfs.isReadable()) {
          TIStream is(targetPalettePath);
          std::string tagName;
          if (!is.matchTag(tagName) || tagName != "palette") {
            DVGui::warning(QString(
                "CleanupDefaultPalette file: This is not palette file"));
            return NULL;
          }
          m_originalPalette = new TPalette();
          m_originalPalette->loadData(is);
        } else
          m_originalPalette = 0;
      }
    }

    sl->setPalette(palette.getPointer());

    /*--- カレントPaletteを切り替える ---*/
    if (sl == app->getCurrentLevel()->getLevel())
      app->getPaletteController()->getCurrentLevelPalette()->setPalette(
          palette.getPointer());

    // Notify the xsheet that the level has changed visual type informations
    // (either the level type,
    // cleanup status, etc)
    app->getCurrentXsheet()->notifyXsheetChanged();
  } else if (!m_params->getPath(scene)
                  .isEmpty())  // Should never be empty, AFAIK...
  {
    // No line processing

    if (notLineProcessed) {
      // Just change paths
      if (sl->getScannedPath().isEmpty()) sl->setScannedPath(sl->getPath());

      sl->setPath(outputPath, false);  // Reload frames from the result, too
    } else {
      // Return to scan level type
      sl->clearFrames();
      sl->setType(OVL_XSHLEVEL);
      sl->setPath(outputPath);
    }
  }

  // Finally, open the LevelUpdater on the level.
  assert(!m_updater->opened());
  try {
    m_updater->open(sl);
  } catch (...) {
    return tr("Couldn't open \"%1\" for write").arg(outputPath.getQString());
  }

  m_updater->getLevelWriter()->setOverwritePaletteFlag(!m_keepOriginalPalette);

  return QString();
}

//-----------------------------------------------------------------------------
/*	setupLevel()から、 m_overwriteAction == REPLACE のとき呼ばれる。
        選択LevelのCleanup後Levelを消す
*/
QString CleanupPopup::resetLevel() {
  struct locals {
    static bool removeFileOrLevel(const TFilePath &fp) {
      return (!TSystem::doesExistFileOrLevel(fp) ||
              TSystem::removeFileOrLevel(fp));
    }

    static QString decorate(const TFilePath &fp) {
      return tr("Couldn't remove file \"%1\"").arg(fp.getQString());
    }
  };

  // Try to remove the existing level
  const CleanupLevel &cl = m_cleanupLevels[m_idx.first];

  TXshSimpleLevel *sl = cl.m_sl;
  assert(sl);

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  /*--- Cleanup後のTLVファイルパスを得る。すなわちこれから消すファイル ---*/
  // Ensure outputPath != inputPath
  const TFilePath &outputPath = scene->decodeFilePath(cl.m_outputPath),
                  &inputPath  = scene->decodeFilePath(
                      CleanupSettingsModel::getInputPath(sl));

  if (inputPath == outputPath)
    return QString();  // Cannot remove source file - however, it can be
                       // overwritten

  // Remove existing output files
  if (!locals::removeFileOrLevel(outputPath))
    return locals::decorate(outputPath);

  if (m_params->m_lineProcessingMode != lpNone) {
    TFilePath fp;

    // Line processing on - remove palette too
    if (!locals::removeFileOrLevel(fp = outputPath.withType("tpl")))
      return locals::decorate(fp);

    // Also remove unpainted output path if any
    const TFilePath &unpaintedPath(
        outputPath.getParentDir() + "nopaint\\" +
        TFilePath(outputPath.getName() + "_np." + outputPath.getType()));

    if (!locals::removeFileOrLevel(unpaintedPath))
      return locals::decorate(unpaintedPath);

    if (!locals::removeFileOrLevel(fp = unpaintedPath.withType("tpl")))
      return locals::decorate(fp);
  }

  // Reset level status
  sl->setPath(sl->getPath(), false);  // false rebuilds level data
  // NOTE: sl is the INPUT level - so this instruction
  return QString();  //       should take place AFTER output availability
                     //       has been ensured.
}

//-----------------------------------------------------------------------------
/*---
 * 現在処理を行っているLevelの最後のフレームの処理が終わってから、フレームを進めるときに呼ばれる
 * ---*/
void CleanupPopup::closeLevel() {
  if (m_cleanuppedLevelFrames.empty()) {
    if (m_updater->opened()) m_updater->close();
    return;
  }

  // Save the unpainted level if necessary
  const CleanupLevel &cl = m_cleanupLevels[m_idx.first];

  TXshSimpleLevel *sl = cl.m_sl;
  assert(sl);

  /*--- Nopaintのみ上書きの場合、Cleanup前に戻す ---*/
  if (cl.m_resolution == NOPAINT_ONLY && !m_originalLevelPath.isEmpty()) {
    sl->setPath(m_originalLevelPath);
    sl->invalidateFrames();
    std::vector<TFrameId> fIds;
    sl->getFids(fIds);
    invalidateIcons(sl, fIds);
    m_originalLevelPath = TFilePath();
  }
  /*--- Paletteを更新しない場合は、ここで取っておいたPaletteデータを元に戻す
   * ---*/
  if (m_keepOriginalPalette && m_originalPalette) {
    sl->setPalette(m_originalPalette);
    if (sl == TApp::instance()->getCurrentLevel()->getLevel())
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->setPalette(m_originalPalette);
    sl->invalidateFrames();
    std::vector<TFrameId> fIds;
    sl->getFids(fIds);
    invalidateIcons(sl, fIds);
    TApp::instance()->getCurrentPalette()->notifyPaletteSwitched();
  }

  // Close the level updater. Silence any exception (ok, something went bad, old
  // story now).
  try {
    m_updater->close();
  } catch (...) {
  }

  if (sl->getType() == TZP_XSHLEVEL &&
      Preferences::instance()->isSaveUnpaintedInCleanupEnable() &&
      cl.m_resolution !=
          NOPAINT_ONLY) /*--- 再Cleanupの場合は既にNoPaintに上書きしている ---*/
  {
    const TFilePath &outputPath = cl.m_outputPath;

    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    TFilePath decodedPath(scene->decodeFilePath(outputPath));

    if (outputPath.getLevelNameW().find(L"-np.") == std::wstring::npos &&
        TFileStatus(decodedPath).doesExist()) {
      saveUnpaintedLevel(decodedPath, sl, m_cleanuppedLevelFrames,
                         (m_keepOriginalPalette && m_originalPalette));
    }
  }

  m_cleanuppedLevelFrames.clear();
  m_firstLevelFrame = true;
}

//-----------------------------------------------------------------------------
/*-- 各フレームの処理 --*/
void CleanupPopup::cleanupFrame() {
  assert(isValidPosition(m_idx));

  TRasterImageP original(currentImage());
  if (!original) return;

  CleanupLevel &cl    = m_cleanupLevels[m_idx.first];
  TXshSimpleLevel *sl = cl.m_sl;
  const TFrameId &fid = cl.m_frames[m_idx.second];

  assert(sl);

  // Perform image cleanup
  {
    TCleanupper *cl                 = TCleanupper::instance();
    const CleanupParameters *params = cl->getParameters();

    if (params->m_lineProcessingMode == lpNone) {
      // No line processing

      TRasterImageP ri(original);
      if (params->m_autocenterType != CleanupTypes::AUTOCENTER_NONE) {
        bool autocentered;
        ri = cl->autocenterOnly(original, false, autocentered);
        if (!autocentered)
          DVGui::warning(
              QObject::tr("The autocentering failed on the current drawing."));
      }

      sl->setFrame(fid, ri);

      // Update the associated file. In case the operation throws, oh well the
      // image gets skipped.
      try {
        m_updater->update(fid, ri);
      } catch (...) {
      }

      IconGenerator::instance()->invalidate(sl, fid);
    } else {
      // Perform main processing

      // Obtain the source dpi. Changed it to be done once at the first frame of
      // each level in order to avoid the following problem:
      // If the original raster level has no dpi (such as TGA images), obtaining
      // dpi in every frame causes dpi mismatch between the first frame and the
      // following frames, since the value
      // TXshSimpleLevel::m_properties->getDpi() will be changed to the
      // dpi of cleanup camera (= TLV's dpi) after finishing the first frame.
      if (m_firstLevelFrame) {
        TPointD dpi;
        original->getDpi(dpi.x, dpi.y);
        if (dpi.x == 0 && dpi.y == 0) dpi = sl->getProperties()->getDpi();
        cl->setSourceDpi(dpi);
      }

      CleanupPreprocessedImage *cpi;
      {
        TRasterImageP resampledRaster;
        cpi = cl->process(original, m_firstLevelFrame, resampledRaster);
      }

      if (!cpi) return;

      // Perform post-processing
      TToonzImageP ti(cl->finalize(cpi));

      /*--- Cleanup Default Paletteを作成、適用 ---*/
      if (m_firstLevelFrame) {
        addCleanupDefaultPalette(sl);
        sl->getPalette()->setPaletteName(sl->getName());
      }

      ti->setPalette(sl->getPalette());  // Assigned to sl in setupLevel()
      assert(sl->getPalette());

      // Update the level data about the cleanupped frame
      sl->setFrameStatus(fid,
                         sl->getFrameStatus(fid) | TXshSimpleLevel::Cleanupped);
      sl->setFrame(fid, TImageP());  // Invalidate the old image data

      // Output the cleanupped image to disk
      try {
        m_updater->update(fid, ti);
      }  // The file image data will be reloaded upon request
      catch (...) {
      }

      // Invalidate icons
      IconGenerator::instance()->invalidate(sl, fid);

      int autocenterType = params->m_autocenterType;
      if (autocenterType == CleanupTypes::AUTOCENTER_FDG &&
          !cpi->m_autocentered)
        DVGui::warning(
            QObject::tr("The autocentering failed on the current drawing."));

      delete cpi;

      if (m_firstLevelFrame) {
        // Update result-dependent level data
        TPointD dpi(0, 0);
        ti->getDpi(dpi.x, dpi.y);
        if (dpi.x != 0 && dpi.y != 0) sl->getProperties()->setDpi(dpi);

        sl->getProperties()->setImageRes(ti->getSize());
        sl->getProperties()->setBpp(32);
      }
    }
  }

  // this enables to view the level during cleanup by another user. this
  // behavior may abort Toonz.
  /*
try { m_updater->flush(); }                                           // Release
the opened level from writing
catch(...) {}                                                       // It is
required to have it open for read
                                                                  // when
rebuilding icons... (still dangerous though)
  */
  m_firstLevelFrame = false;
  m_cleanuppedLevelFrames.push_back(fid);

  TApp *app = TApp::instance();
  app->getCurrentLevel()->notifyLevelChange();
  app->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void CleanupPopup::advanceFrame() {
  assert(isValidPosition(m_idx));

  std::pair<int, int> newIdx = std::make_pair(m_idx.first, m_idx.second + 1);

  // In case the level was completely processed, close down the old level
  if (!isValidPosition(newIdx)) {
    if (!m_cleanuppedLevelFrames.empty()) closeLevel();

    newIdx = std::make_pair(m_idx.first + 1, 0);
  }

  // Advance in the cleanup list
  m_idx = newIdx;

  if (m_imgViewBox->isChecked()) {
    TImageP image = currentImage();
    if (image) m_imageViewer->setImage(image);
  }

  // show the progress in the mainwindow's title bar
  updateTitleString();

  // Update the progress bar
  m_progressBar->setValue(++m_completion.first);
}

//-----------------------------------------------------------------------------

void CleanupPopup::onValueChanged(int value) {
  if (value == m_progressBar->maximum()) {
    close();
    return;
  }

  m_progressLabel->setText(currentString());
}

//-----------------------------------------------------------------------------

void CleanupPopup::onCleanupFrame() {
  /*--- Busy時にボタンをUnableする ---*/
  m_cleanupAllButton->setEnabled(false);
  m_cleanupButton->setEnabled(false);
  m_skipButton->setEnabled(false);

  /*--- 新しいLevelに取り掛かり始めたとき ---*/
  if (m_cleanuppedLevelFrames.empty()) {
    const QString &err = setupLevel();

    if (!err.isEmpty()) {
      DVGui::error(err);
      return;
    }
  }

  cleanupFrame();
  advanceFrame();

  /*--- ボタンを元に戻す---*/
  m_cleanupAllButton->setEnabled(true);
  m_cleanupButton->setEnabled(true);
  m_skipButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void CleanupPopup::onSkipFrame() { advanceFrame(); }

//-----------------------------------------------------------------------------

void CleanupPopup::onCleanupAllFrame() {
  m_cleanupQuestionLabel->hide();

  /*--- Busy時にボタンをUnableする ---*/
  m_cleanupAllButton->setEnabled(false);
  m_cleanupButton->setEnabled(false);
  m_skipButton->setEnabled(false);

  while (isValidPosition(m_idx)) {
    if (m_cleanuppedLevelFrames.empty()) {
      const QString &err = setupLevel();

      if (!err.isEmpty()) {
        DVGui::error(err);
        return;
      }
    }

    cleanupFrame();
    advanceFrame();

    QCoreApplication::processEvents();  // Allow cancels to be received
  }
  /*--- ボタンを元に戻す---*/
  m_cleanupAllButton->setEnabled(true);
  m_cleanupButton->setEnabled(true);
  m_skipButton->setEnabled(true);

  close();
}

//-----------------------------------------------------------------------------

void CleanupPopup::onCancelCleanup() { close(); }

//*****************************************************************************
//    CleanupPopup::OverwriteDialog  implementation
//*****************************************************************************

CleanupPopup::OverwriteDialog::OverwriteDialog()
    : DVGui::ValidatedChoiceDialog(TApp::instance()->getMainWindow()) {
  setWindowTitle(tr("Warning!"));

  bool ret = connect(m_buttonGroup, SIGNAL(buttonClicked(int)),
                     SLOT(onButtonClicked(int)));
  assert(ret);

  // Option 1: OVERWRITE
  QRadioButton *radioButton = new QRadioButton;
  radioButton->setText(
      tr("Cleanup all selected drawings overwriting those previously cleaned "
         "up.*"));
  radioButton->setFixedHeight(20);
  radioButton->setChecked(true);  // initial option: OVERWRITE

  m_buttonGroup->addButton(radioButton, OVERWRITE);
  addWidget(radioButton);

  // Option 2: WRITE_NEW
  radioButton = new QRadioButton;
  radioButton->setText(
      tr("Cleanup only non-cleaned up drawings and keep those previously "
         "cleaned up.*"));
  radioButton->setFixedHeight(20);

  m_buttonGroup->addButton(radioButton, WRITE_NEW);
  addWidget(radioButton);

  // Option 3: REPLACE
  radioButton = new QRadioButton;
  radioButton->setText(
      tr("Delete existing level and create a new level with selected drawings "
         "only."));
  radioButton->setFixedHeight(20);

  m_buttonGroup->addButton(radioButton, REPLACE);
  addWidget(radioButton);

  // Option 4: ADD_SUFFIX
  QHBoxLayout *suffixLayout = new QHBoxLayout;
  {
    radioButton = new QRadioButton;
    radioButton->setText(tr("Rename the new level adding the suffix "));
    radioButton->setFixedHeight(20);

    m_buttonGroup->addButton(radioButton, ADD_SUFFIX);
    suffixLayout->addWidget(radioButton);

    m_suffix = new DVGui::LineEdit;
    m_suffix->setEnabled(false);

    suffixLayout->addWidget(m_suffix);
  }
  addLayout(suffixLayout);  // Couldnt' place it right after allocation,
                            // DVGui::Dialog::addLayout() crashed...
  // Option 5: NOPAINT_ONLY
  radioButton = new QRadioButton(this);
  radioButton->setText(
      tr("This is Re-Cleanup. Overwrite only to the no-paint files."));
  radioButton->setFixedHeight(20);
  m_buttonGroup->addButton(radioButton, NOPAINT_ONLY);
  addWidget(radioButton);

  QLabel *note = new QLabel(tr("* Palette will not be changed."), this);
  note->setStyleSheet("font-size: 10px; font: italic;");
  addWidget(note);

  endVLayout();

  layout()->setSizeConstraint(QLayout::SetFixedSize);
}

//-----------------------------------------------------------------------------

void CleanupPopup::OverwriteDialog::reset() {
  ValidatedChoiceDialog::reset();
  m_suffixText.clear();
}

//-----------------------------------------------------------------------------

QString CleanupPopup::OverwriteDialog::acceptResolution(void *obj,
                                                        int resolution,
                                                        bool applyToAll) {
  struct locals {
    static inline QString existsStr(const TFilePath &fp) {
      return tr("File \"%1\" already exists.\nWhat do you want to do?")
          .arg(fp.getQString());
    }
  };  // locals

  assert(obj);

  TFilePath &fp = *static_cast<TFilePath *>(obj);

  QString error;

  switch (resolution) {
  case NO_RESOLUTION:
    // fp was already found to be invalid
    assert(::exists(fp));
    error = locals::existsStr(fp);

    // Restore previous apply-to-all options if necessary
    if (!error.isEmpty() && appliedToAll()) {
      assert(!m_suffixText.isEmpty());
      m_suffix->setText(m_suffixText);
    }
    break;

  case ADD_SUFFIX:
    // Save resolution options if necessary
    if (applyToAll) m_suffixText = m_suffix->text();

    // Test produced file path
    const TFilePath &fp_suf =
        fp.withName(fp.getWideName() + m_suffix->text().toStdWString());

    if (::exists(fp_suf))
      error = locals::existsStr(fp_suf);
    else
      fp = fp_suf;

    break;
  }

  return error;
}

//-----------------------------------------------------------------------------

void CleanupPopup::OverwriteDialog::initializeUserInteraction(const void *obj) {
  const TFilePath &fp = *static_cast<const TFilePath *>(obj);

  // Generate a suitable initial suffix
  int num = 1;
  for (; ::exists(fp, num); ++num)
    ;

  m_suffix->setText(::suffix(num));
}

//-----------------------------------------------------------------------------

void CleanupPopup::OverwriteDialog::onButtonClicked(int buttonId) {
  m_suffix->setEnabled(buttonId == ADD_SUFFIX);
}

//-----------------------------------------------------------------------------

void CleanupPopup::onImgViewBoxToggled(bool on) {
  m_imageViewer->setVisible(on);
}

//-----------------------------------------------------------------------------
/*!	Show the progress in the mainwindow's title bar
*/
void CleanupPopup::updateTitleString() {
  if (!TApp::instance()->getMainWindow()) return;
  MainWindow *mainWin =
      qobject_cast<MainWindow *>(TApp::instance()->getMainWindow());
  if (!mainWin) return;

  QString str = QString::number(m_completion.first) + "/" +
                QString::number(m_completion.second) +
                tr(" : Cleanup in progress");

  mainWin->changeWindowTitle(str);
}

//*****************************************************************************
//    CleanupCommand  definition
//*****************************************************************************

class CleanupCommand final : public MenuItemHandler {
public:
  CleanupCommand() : MenuItemHandler("MI_Cleanup") {}

  void execute() override {
    static CleanupPopup *popup = new CleanupPopup;
    popup->execute();
  }

} CleanupCommand;
