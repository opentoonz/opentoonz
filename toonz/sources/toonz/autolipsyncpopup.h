#pragma once

#ifndef AUTOLIPSYNCPOPUP_H
#define AUTOLIPSYNCPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"
#include "toonz/txshlevel.h"
#include "toonzqt/intfield.h"
#include "toonzqt/gutil.h"
#include "toonz/txshsoundcolumn.h"

#include <QProcess>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QMediaPlayer>
#include <QTimer>

// Forward declarations
class QLabel;
class TXshSimpleLevel;
class TXshChildLevel;
class TFrameId;
class QComboBox;
class QTextEdit;
class QIcon;
class QProcess;
class QGroupBox;
class QFrame;
class QShowEvent;
class QHideEvent;
class QPaintEvent;

//=============================================================================
// AutoLipSyncPopup
//-----------------------------------------------------------------------------

class AutoLipSyncPopup final : public DVGui::Dialog {
  Q_OBJECT

  // UI Elements
  QLabel *m_aiLabel;
  QLabel *m_oLabel;
  QLabel *m_eLabel;
  QLabel *m_uLabel;
  QLabel *m_lLabel;
  QLabel *m_wqLabel;
  QLabel *m_mbpLabel;
  QLabel *m_fvLabel;
  QLabel *m_restLabel;
  QLabel *m_otherLabel;
  QComboBox *m_soundLevels;
  QTextEdit *m_scriptEdit;
  QIcon m_playIcon, m_stopIcon;

  QLabel *m_imageLabels[10];
  QLabel *m_textLabels[10];
  QPushButton *m_navButtons[20];
  QPixmap m_pixmaps[10];
  QPushButton *m_applyButton;
  QPushButton *m_playButton;

  // Level and frame data
  std::vector<TFrameId> m_levelFrameIds;
  std::vector<TFrameId> m_activeFrameIds;
  DVGui::FileField *m_audioFile;
  TXshSimpleLevel *m_sl;
  TXshChildLevel *m_cl;
  TXshLevelP m_childLevel;
  DVGui::IntLineEdit *m_startAt;
  int m_col;
  int m_startFrame = -1;
  bool m_valid     = false;
  bool m_isEditingLevel;
  QStringList m_textLines;
  QCheckBox *m_restToEnd;

  // Audio and processing
  QString m_audioPath;
  TFilePath m_datPath;
  QMediaPlayer *m_player;
  QLabel *m_scriptLabel;
  QLabel *m_columnLabel;
  QLabel *m_insertAtLabel;
  bool m_deleteFile = false;
  DVGui::ProgressDialog *m_progressDialog;
  QProcess *m_rhubarb;
  QString m_rhubarbPath;
  QFrame *m_audioFrame;

  QTimer m_audioTimeout;
  TXshSoundColumn *m_playingSound;

  // Process management
  bool m_processRunning;  // Tracks if Rhubarb process is running
  bool m_userCancelled;   // Tracks if user cancelled the process

public:
  AutoLipSyncPopup();
  ~AutoLipSyncPopup() override;  // Added destructor

protected:
  // Event handlers
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

  // UI helpers
  void refreshSoundLevels();
  void generateThumbnails();
  void updateThumbnail(int index);

  // Audio processing
  void saveAudio();
  void runRhubarb();

  // Process management
  void cleanupAfterProcess();    // Cleans up temporary files and resets state
  void processRhubarbResults();  // Processes successful Rhubarb output

public slots:
  // Main actions
  void onApplyButton();
  void playSound();
  void stopAllSound();
  bool setAudioFile();

  // UI interactions
  void imageNavClicked(int id);
  void onStartValueChanged();
  void onLevelChanged(int index);

  // Media control
  void onMediaStateChanged(QMediaPlayer::State state);
  void onAudioTimeout();

  // Rhubarb process handling
  void onOutputReady();
  void onRhubarbFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onRhubarbError(QProcess::ProcessError error);
  void onAnalysisCancelled();  // Called when user cancels analysis

private slots:
  // Internal slot for icon generation updates
  void onIconGenerated();  // Called when IconGenerator finishes generating
                           // thumbnails
};

#endif  // AUTOLIPSYNCPOPUP_H
