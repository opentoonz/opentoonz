#pragma once

#ifndef XDTSIMPORTPOPUP_H
#define XDTSIMPORTPOPUP_H

// Qt includes
#include <QMap>
#include <QStringList>
#include <QCheckBox>

// ToonzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"

// ToonzCore includes
#include "tfilepath.h"

// Forward declarations
namespace DVGui {
class FileField;
}

class ToonzScene;
class QComboBox;

//=============================================================================
// XDTSImportPopup
//-----------------------------------------------------------------------------

/**
 * Dialog for importing XDTS/SXF files with level mapping and conversion
 * options.
 */
class XDTSImportPopup final : public DVGui::Dialog {
  Q_OBJECT

  // UI Elements
  QComboBox* m_tick1Combo          = nullptr;
  QComboBox* m_tick2Combo          = nullptr;
  QComboBox* m_keyFrameCombo       = nullptr;
  QComboBox* m_referenceFrameCombo = nullptr;
  QCheckBox* m_renameCheckBox      = nullptr;
  QComboBox* m_convertCombo        = nullptr;

  // NAA conversion options (visible only for 3rd conversion option)
  QCheckBox* m_paletteCheckBox    = nullptr;
  QComboBox* m_dpiMode            = nullptr;
  DVGui::DoubleLineEdit* m_dpiFld = nullptr;

  // Data members
  QMap<QString, DVGui::FileField*> m_fields;
  QStringList m_pathSuggestedLevels;
  ToonzScene* m_scene = nullptr;
  bool m_isUext = false;  // Whether loading UEXT (unofficial extension) version

public:
  /**
   * Constructs an XDTS/SXF import dialog.
   *
   * @param levelNames List of level names to import.
   * @param scene Pointer to the current Toonz scene.
   * @param scenePath Path to the scene file being imported.
   * @param isUextVersion Whether importing UEXT version.
   * @param isSXF Whether importing SXF format (vs XDTS).
   */
  explicit XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                           TFilePath scenePath, bool isUextVersion,
                           bool isSXF = false);

  /**
   * Gets the file path for a specific level.
   *
   * @param levelName Name of the level.
   * @return The file path for the level, or empty string if not found.
   */
  QString getLevelPath(const QString& levelName) const;

  /**
   * Gets the marker IDs for cell marks.
   *
   * @param tick1Id Output parameter for tick1 symbol ID.
   * @param tick2Id Output parameter for tick2 symbol ID.
   * @param keyFrameId Output parameter for keyframe symbol ID.
   * @param referenceFrameId Output parameter for reference frame symbol ID.
   */
  void getMarkerIds(int& tick1Id, int& tick2Id, int& keyFrameId,
                    int& referenceFrameId) const;

private:
  /**
   * Updates path suggestions based on a sample path.
   *
   * Searches for files matching level names in the directory of the
   * sample path and its subdirectories.
   */
  void updateSuggestions(const QString& samplePath);

  /**
   * Fallback search for path suggestions.
   *
   * Performs a broader search when the initial suggestion method
   * doesn't find matching files.
   */
  void updateSuggestions(const TFilePath& path);

private slots:
  /**
   * Handles path changes in file fields.
   *
   * Triggered when a user modifies a file field. Updates suggestions
   * based on the new path.
   */
  void onPathChanged();

protected:
  /**
   * Handles dialog acceptance.
   *
   * Processes all selected files, performs conversions if requested,
   * and completes the import operation.
   */
  void accept() override;
};

#endif  // XDTSIMPORTPOPUP_H
