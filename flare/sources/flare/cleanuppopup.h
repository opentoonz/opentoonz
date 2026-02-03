#pragma once

#ifndef CLEANUPPOPUP_H
#define CLEANUPPOPUP_H

#include <memory>

// TnzQt includes
#include "toonzqt/validatedchoicedialog.h"

// TnzCore includes
#include "tfilepath.h"
#include "timage.h"

#include <QMap>

//================================================

//  Forward declarations

class TXshSimpleLevel;
class ImageViewer;
class CleanupParameters;
class LevelUpdater;

namespace DVGui {
class LineEdit;
}

class QProgressBar;
class QLabel;
class QPushButton;
class QHideEvent;
class QGroupBox;

//================================================

//*****************************************************************************
//    CleanupPopup declaration
//*****************************************************************************

class CleanupPopup final : public QDialog {
  Q_OBJECT

public:
  CleanupPopup();
  ~CleanupPopup();

  void execute();

protected:
  void closeEvent(QCloseEvent *) override;

private:
  class OverwriteDialog;
  struct CleanupLevel;

private:
  QProgressBar *m_progressBar;
  QLabel *m_progressLabel;
  QLabel *m_cleanupQuestionLabel;
  ImageViewer *m_imageViewer;
  QGroupBox *m_imgViewBox;

  /*---ビジー時にボタンをUnableするため---*/
  QPushButton *m_cleanupAllButton;
  QPushButton *m_cleanupButton;
  QPushButton *m_skipButton;

  std::unique_ptr<LevelUpdater> m_updater;  //!< The cleanup level updater.

  std::vector<CleanupLevel>
      m_cleanupLevels;        //!< List of levels to be cleanupped.
  std::pair<int, int> m_idx,  //!< Current cleanup list position.
      m_completion;        //!< Count of completed frames versus total frames.
  bool m_firstLevelFrame;  //!< Whether this is the first cleanupped frame on
                           //!  current level. Used in some autoadjust cases.

  std::vector<TFrameId> m_cleanuppedLevelFrames;  //!< Current level's list of
                                                  //! cleanupped frames. Used
  //!  to selectively build the level's unpainted backup.
  std::unique_ptr<CleanupParameters>
      m_params;  //!< Cleanup params used to cleanup.

  std::unique_ptr<OverwriteDialog>
      m_overwriteDialog;  //!< Dialog about level overwriting options.

  /*	Palette上書きの判断をするために、保存先Levelが既に存在するかどうかのフラグ
          ただし、REPLACE又はNOPAINT_ONLYの場合はこのフラグは無視して必ずPaletteを上書きする
  */
  QMap<TXshSimpleLevel *, bool> m_levelAlreadyExists;
  /*--- 再Cleanupの場合、Xsheetを再現するためにパスを取っておく ---*/
  TFilePath m_originalLevelPath;
  /*	Cleanup後、Paletteを更新するかどうかのフラグ。再Cleanupの場合は、
          m_actionがREPLACE_LEVELの場合以外は元のPaletteを保持する。
  */
  bool m_keepOriginalPalette;
  TPalette *m_originalPalette;

private:
  void reset();
  void buildCleanupList();
  bool
  analyzeCleanupList();  //!< Checks m_cleanupFrames for existing output levels
  //!  and updates it depending on user choices.  \return  Whether cleanup can
  //!  take place.
  bool isValidPosition(const std::pair<int, int> &pos)
      const;  //!< Returns whether specified cleanup position is valid.
  QString currentString() const;
  TImageP currentImage() const;

  QString setupLevel();  //!< Prepares level for cleanup.  \return  An eventual
                         //! failure message.
  QString resetLevel();  //!< Erases existing output for the cleanup operation.
                         //!\return  An eventual failure message.
  void closeLevel();

  void cleanupFrame();
  void advanceFrame();

  /*--- 進捗をタスクバーから確認するため、MainWindowのタイトルバーに表示する
   * ---*/
  void updateTitleString();

private slots:

  void onCleanupFrame();
  void onSkipFrame();
  void onCleanupAllFrame();
  void onCancelCleanup();
  void onValueChanged(int);
  void onImgViewBoxToggled(bool);
};

//*****************************************************************************
//    CleanupPopup::OverwriteDialog declaration (private)
//*****************************************************************************

class CleanupPopup::OverwriteDialog final
    : public DVGui::ValidatedChoiceDialog {
  Q_OBJECT

public:
  OverwriteDialog();

  void reset() override;
  void enableOptions(bool writingOnSource);

private:
  DVGui::LineEdit *m_suffix;
  QString m_suffixText;

private:
  QString acceptResolution(void *obj, int resolution, bool applyToAll) override;
  void initializeUserInteraction(const void *obj) override;

private slots:

  void onButtonClicked(int buttonId);
};

#endif  // CLEANUPPOPUP_H
