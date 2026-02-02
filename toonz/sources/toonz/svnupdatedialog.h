#pragma once

#ifndef SVN_UPDATE_DIALOG_H
#define SVN_UPDATE_DIALOG_H

// ToonzQt includes
#include "toonzqt/dvdialog.h"

// Version control includes
#include "versioncontrol.h"

// Qt includes
#include <QStringList>

// Forward declarations
class QLabel;
class QPushButton;
class DateChooserWidget;
class ConflictWidget;
class QTextEdit;
class QCheckBox;

//-----------------------------------------------------------------------------
/**
 * @brief Dialog for updating files from version control system.
 *
 * This dialog handles SVN update operations, including normal updates,
 * updates to specific revisions, and conflict resolution. It can also
 * handle scene files and their associated resources.
 */
class SVNUpdateDialog final : public DVGui::Dialog {
  Q_OBJECT

public:
  /**
   * @brief Constructs an update dialog.
   *
   * @param parent Parent widget.
   * @param workingDir The working directory for SVN operations.
   * @param filesToUpdate List of files to update.
   * @param sceneIconsCount Number of scene icons (for display adjustments).
   * @param isFolderOnly Whether the operation is for folders only.
   * @param updateToRevision Whether to update to a specific revision.
   * @param nonRecursive Whether to perform a non-recursive update.
   */
  SVNUpdateDialog(QWidget* parent, const QString& workingDir,
                  const QStringList& filesToUpdate, int sceneIconsCount,
                  bool isFolderOnly, bool updateToRevision, bool nonRecursive);

private:
  /**
   * @brief Initiates the update process for files.
   */
  void updateFiles();

  /**
   * @brief Checks which files need to be updated based on SVN status.
   */
  void checkFiles();

  /**
   * @brief Switches the dialog to close mode.
   *
   * Changes the UI to show only the close button after operation completion.
   */
  void switchToCloseButton();

private slots:
  /**
   * @brief Handles SVN errors.
   *
   * @param errorString The error message from SVN.
   */
  void onError(const QString& errorString);

  /**
   * @brief Handles completion of update operation.
   *
   * @param text Output text from the update command.
   */
  void onUpdateDone(const QString& text);

  /**
   * @brief Handles completion of conflict resolution.
   */
  void onConflictSetted();

  /**
   * @brief Handles completion of update with "mine-full" option.
   */
  void onUpdateToMineDone();

  /**
   * @brief Handles completion of conflict resolution.
   */
  void onConflictResolved();

  /**
   * @brief Handles update to specific revision button click.
   */
  void onUpdateToRevisionButtonClicked();

  /**
   * @brief Handles normal update button click.
   */
  void onUpdateButtonClicked();

  /**
   * @brief Adds text to the output display.
   *
   * @param text Text to display.
   */
  void addOutputText(const QString& text);

  /**
   * @brief Handles retrieval of SVN status.
   *
   * @param xmlResponse The XML response from SVN status command.
   */
  void onStatusRetrieved(const QString& xmlResponse);

  /**
   * @brief Handles toggle of update scene contents checkbox.
   *
   * @param checked Whether the checkbox is checked.
   */
  void onUpdateSceneContentsToggled(bool checked);

signals:
  /**
   * @brief Signal emitted when update operation is complete.
   *
   * @param files List of files that were updated.
   */
  void done(const QStringList& files);

private:
  // UI Elements
  QPushButton* m_closeButton               = nullptr;
  QPushButton* m_cancelButton              = nullptr;
  QPushButton* m_updateButton              = nullptr;
  QCheckBox* m_updateSceneContentsCheckBox = nullptr;
  QLabel* m_waitingLabel                   = nullptr;
  QLabel* m_textLabel                      = nullptr;
  QTextEdit* m_output                      = nullptr;
  DateChooserWidget* m_dateChooserWidget   = nullptr;
  ConflictWidget* m_conflictWidget         = nullptr;

  // SVN thread for asynchronous operations
  VersionControlThread m_thread;

  // Data members
  QString m_workingDir;
  QStringList m_files;
  QStringList m_filesToUpdate;
  QStringList m_filesWithConflict;
  QStringList m_sceneResources;
  QList<SVNStatus> m_status;

  // Configuration flags
  bool m_updateToRevision   = false;
  bool m_nonRecursive       = false;
  bool m_someSceneIsMissing = false;

  // Counters
  int m_sceneIconsCount = 0;
};

#endif  // SVN_UPDATE_DIALOG_H
