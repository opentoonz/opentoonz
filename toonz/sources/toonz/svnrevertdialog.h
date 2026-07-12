#pragma once

#ifndef SVN_REVERT_DIALOG_H
#define SVN_REVERT_DIALOG_H

// Qt includes
#include <QList>
#include <QStringList>

// ToonzQt includes
#include "toonzqt/dvdialog.h"

// Version control includes
#include "versioncontrol.h"

// Forward declarations
class QPushButton;
class QTreeWidget;
class QCheckBox;
class QLabel;

//-----------------------------------------------------------------------------
/**
 * @brief Dialog for reverting changes in version control system.
 *
 * This dialog allows users to revert changes made to files in a Subversion
 * repository. It can handle both individual files and folders, and includes
 * special handling for scene files and their linked resources.
 */
class SVNRevertDialog final : public DVGui::Dialog {
  Q_OBJECT

  // UI Elements
  QLabel* m_waitingLabel                   = nullptr;
  QLabel* m_textLabel                      = nullptr;
  QPushButton* m_revertButton              = nullptr;
  QPushButton* m_cancelButton              = nullptr;
  QTreeWidget* m_treeWidget                = nullptr;
  QCheckBox* m_revertSceneContentsCheckBox = nullptr;

  // Data members
  QString m_workingDir;
  QStringList m_files;
  QStringList m_filesToRevert;
  QStringList m_sceneResources;
  QList<SVNStatus> m_status;
  VersionControlThread m_thread;

  // Configuration
  bool m_folderOnly    = false;
  int m_sceneIconAdded = 0;

public:
  /**
   * @brief Constructs a revert dialog.
   *
   * @param parent Parent widget.
   * @param workingDir The working directory for SVN operations.
   * @param files List of files to consider for reversion.
   * @param folderOnly Whether the operation is for folders only.
   * @param sceneIconAdded Number of scene icons added (for display
   * adjustments).
   */
  explicit SVNRevertDialog(QWidget* parent, const QString& workingDir,
                           const QStringList& files, bool folderOnly = false,
                           int sceneIconAdded = 0);

  /**
   * @brief Checks which files need to be reverted based on SVN status.
   */
  void checkFiles();

private:
  /**
   * @brief Switches the dialog to close mode.
   *
   * Changes the revert button to a close button and updates the UI
   * to indicate completion.
   */
  void switchToCloseButton();

  /**
   * @brief Initiates the revert process for selected files.
   */
  void revertFiles();

  /**
   * @brief Initializes the tree widget with files to revert.
   *
   * Groups linked files (like scene resources) under parent items
   * for better visualization.
   */
  void initTreeWidget();

private slots:
  /**
   * @brief Handles revert button click event.
   */
  void onRevertButtonClicked();

  /**
   * @brief Handles completion of revert operation.
   */
  void onRevertDone();

  /**
   * @brief Handles SVN errors.
   *
   * @param errorString The error message from SVN.
   */
  void onError(const QString& errorString);

  /**
   * @brief Handles retrieval of SVN status.
   *
   * @param xmlResponse The XML response from SVN status command.
   */
  void onStatusRetrieved(const QString& xmlResponse);

  /**
   * @brief Handles toggle of revert scene contents checkbox.
   *
   * @param checked Whether the checkbox is checked.
   */
  void onRevertSceneContentsToggled(bool checked);

signals:
  /**
   * @brief Signal emitted when revert operation is complete.
   *
   * @param files List of files that were reverted.
   */
  void done(const QStringList& files);
};

//-----------------------------------------------------------------------------
/**
 * @brief Dialog for reverting frame range changes in version control system.
 *
 * Specialized dialog for reverting changes to frame ranges in animation files.
 * Handles the special case where multiple frame files need to be reverted.
 */
class SVNRevertFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  // UI Elements
  QLabel* m_waitingLabel      = nullptr;
  QLabel* m_textLabel         = nullptr;
  QPushButton* m_revertButton = nullptr;
  QPushButton* m_cancelButton = nullptr;

  // Data members
  QString m_workingDir;
  QString m_file;
  QString m_tempFileName;
  QStringList m_files;
  QStringList m_filesToRevert;
  QList<SVNStatus> m_status;
  VersionControlThread m_thread;

public:
  /**
   * @brief Constructs a frame range revert dialog.
   *
   * @param parent Parent widget.
   * @param workingDir The working directory for SVN operations.
   * @param file The main file to revert.
   * @param tempFileName Temporary file name used during the operation.
   */
  explicit SVNRevertFrameRangeDialog(QWidget* parent, const QString& workingDir,
                                     const QString& file,
                                     const QString& tempFileName);

private:
  /**
   * @brief Switches the dialog to close mode.
   */
  void switchToCloseButton();

  /**
   * @brief Initiates the revert process for frame range files.
   */
  void revertFiles();

  /**
   * @brief Checks which frame files need to be reverted.
   */
  void checkFiles();

private slots:
  /**
   * @brief Handles revert button click event.
   */
  void onRevertButtonClicked();

  /**
   * @brief Handles SVN errors.
   *
   * @param errorString The error message from SVN.
   */
  void onError(const QString& errorString);

  /**
   * @brief Handles retrieval of SVN status for frame files.
   *
   * @param xmlResponse The XML response from SVN status command.
   */
  void onStatusRetrieved(const QString& xmlResponse);

  /**
   * @brief Handles completion of revert operation.
   */
  void onRevertDone();

signals:
  /**
   * @brief Signal emitted when revert operation is complete.
   *
   * @param files List of files that were reverted.
   */
  void done(const QStringList& files);
};

#endif  // SVN_REVERT_DIALOG_H
