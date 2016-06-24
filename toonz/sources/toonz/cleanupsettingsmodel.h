#pragma once

#ifndef CLEANUPSETTINGSMODEL_H
#define CLEANUPSETTINGSMODEL_H

// TnzCore includes
#include "trasterimage.h"

// ToonzLib includes
#include "toonz/cleanupparameters.h"

// Qt includes
#include <QObject>

//================================================

//  Forward declarations

class TXshLevel;
class TXshSimpleLevel;
class CleanupSettingsPopup;

//================================================

//********************************************************************
//    CleanupSettingsModel declaration
//********************************************************************

//! The CleanupSettingsModel is the class responsible for concrete management of
//! cleanup settings in Toonz.

/*!
  In Toonz, the interface to cleanup settings follows the model/view
architecture,
  where each Cleanup Settings panel is a view to a singleton model of this
class.
\n\n
  The CleanupSettingsModel is responsible for handling signals that involve
cleanup
  changes, such as user changes in the parameters coming from views, settings
reset or
  switches from commands, and so on.
\n\n
  A cleanup operation is roughly split in 2 parts: main processing and
post-processing.
\n\n
  The main processing part takes the full original image and applies geometric
transforms
  and other full-image analysis and processing. This is typically slow, and is
thus
  performed by this class once for all views.
\n\n
  Post-processing performs further local or pixel-per-pixel adjustments that are
sufficiently
  fast to be carried out by single views on their preview area. This includes
<I> colors recognition,
  colors adjustment, despeckling <\I> and \a antialiasing-related effects.

\note This class also acts as a wrapper to CleanupParamaters' palette, attaching
to the
      "current cleanup palette handle" accessible through the
TPaletteController.
*/
class CleanupSettingsModel final : public QObject {
  Q_OBJECT

  int m_listenersCount;    //!< Number of attached clients
  int m_previewersCount;   //!< Number of attached *active* previewers
  int m_cameraTestsCount;  //!< Number of attached camera tests

  CleanupParameters
      m_backupParams;    //!< Stores a copy of current configuration.
                         //!< It is used to track parameter changes.
  int m_allowedActions;  //!< Actions allowed on changes commit.
  int m_action;          //!< Cumulative parameter changes action.

  int m_c;                //!< Current xsheet position
  TXshSimpleLevel *m_sl;  //!< Currently referenced level
  TFrameId m_fid;         //!< Frame Id of currently referenced image
  TFilePath m_clnPath;  //!< Path of the cln file associated with current level
                        //!(if any)

  TRasterImageP m_original;  //!< The original preview/camera test image
  TRasterImageP m_previewTransformed;     //!< The preview-transformed image
  TRasterImageP m_cameraTestTransformed;  //!< The camera test-transformed image
  TAffine m_transform;  //!< The preview transform (no camera test for now)

public:
  enum { LISTENER = 0x1, PREVIEWER = 0x2, CAMERATEST = 0x4 };
  enum CommitMask { NONE = 0, INTERFACE, POSTPROCESS, FULLPROCESS };

public:
  CleanupSettingsModel();
  ~CleanupSettingsModel();

  static CleanupSettingsModel *instance();

  CleanupParameters *getCurrentParameters();
  void getCleanupFrame(TXshSimpleLevel *&sl, TFrameId &fid);

  const TFilePath &clnPath() const { return m_clnPath; }

  //! Attaches an object to the model. It is not necessary to supply the object
  //! itself, but a description of its role
  //! is required. The object could be any combination of the LISTENER,
  //! PREVIEWER or CAMERATEST flags. The role information
  //! is used to activate centralized preview or camera test processing.
  void attach(int objectFlag, bool doPreview = true);

  //! The inverse of attach(), it's used to deactivate the specified role
  //! activities when no more attached objects to that role
  //! remain.
  void detach(int objectFlag);

  //! Returns the interesting info about a cleanup preview process.
  //! Specifically, we have both the original and
  //! affine-transformed images, and the applied transform.
  void getPreviewData(TRasterImageP &original, TRasterImageP &transformed,
                      TAffine &transform) {
    original    = m_original;
    transformed = m_previewTransformed;
    transform   = m_transform;
  }

  //! Returns the interesting info about a camera test process. This includes
  //! both the original and affine-transformed
  //! image.
  void getCameraTestData(TRasterImageP &original, TRasterImageP &transformed) {
    original    = m_original;
    transformed = m_cameraTestTransformed;
  }

  //! Specifies the model behavior upon a cleanup parameters change commit. Once
  //! the model receives a commitChanges()
  //! notification, it tracks the changed parameters and decides how deep the
  //! image under cleanup focus must be
  //! re-processed to stick to the changes. The commit mask can be specified to
  //! force the model to limit the performed
  //! cleanup up to the specified stage. This is especially useful in case a
  //! parameter changes frequently enough
  //! that only the last of a long sequence of updates should be processed (e.g.
  //! when dragging a parameter slider).
  void setCommitMask(CommitMask allowedProcessing);

  void saveSettings(const TFilePath &clnPath);
  bool loadSettings(const TFilePath &clnPath);

  //! Prompts to save current settings in case they have been modified. Returns
  //! false if the
  //! user decided to keep the settings and stop (cancel) any action affecting
  //! them.
  bool saveSettingsIfNeeded();

public:
  // NOTE: These should be moved to TCleanupper as soon as I get the chance

  static TFilePath getInputPath(TXshSimpleLevel *sl);  //!< Returns the \b
                                                       //! encoded input cleanup
  //! path for the specified
  //! level.
  static TFilePath getOutputPath(TXshSimpleLevel *sl,
                                 const CleanupParameters *params);  //!< Returns
                                                                    //! the \b
  //! encoded
  //! output
  //! cleanup
  //! path for
  //! the
  //! specified
  //! level.

  static TFilePath getClnPath(TXshSimpleLevel *sl);  //!< Returns the \b decoded
                                                     //! path for the level's \a
  //! cln file.
  static void saveSettings(CleanupParameters *params, const TFilePath &clnPath);
  static bool loadSettings(CleanupParameters *params, const TFilePath &clnPath);

public slots:

  //! Informs the model that current settings have changed. Settings analysis
  //! and cleanup refreshes ensue. The modelChanged() signal is then emitted to
  //! inform connected views of said change.
  void commitChanges();

  void promptSave();  //!< Prompts to save current settings
  void promptLoad();  //!< Prompts to load settings

  void restoreGlobalSettings();  //!< Reloads the project's cleanup settings (no
                                 //! prompts)

signals:

  //! Emitted whenever the model has finished processing a cleanup settings
  //! change.
  //! This happens after an explicit commitChange() has been invoked from a
  //! view,
  //! or other command-related settings change occur.
  void modelChanged(bool needsPostProcess);

  //! Emitted every time the cleanup focus switches to a new image.
  void imageSwitched();

  //! Emitted when the data retrievable with getPreviewData() and
  //! getCameraTestData()
  //! have been updated.
  void previewDataChanged();

  //! Emitted when the model loads (or unloads) a cln settings file. It is also
  //! invoked
  //! when a cln is saved, since saving should put us in the same situation we
  //! have after
  //! a cln has been loaded.
  void clnLoaded();

private:
  void commitChanges(int action);

  void connectSignals();
  void disconnectSignals();

  void rebuildPreview();
  void processFrame(TXshSimpleLevel *sl, TFrameId fid);

private slots:

  void onSceneSwitched();
  void onCellChanged();
  void onPaletteChanged();
};

#endif  // CLEANUPSETTINGSMODEL_H
