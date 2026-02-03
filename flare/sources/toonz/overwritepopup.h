#pragma once

#ifndef OVERWRITEPOPUP_H
#define OVERWRITEPOPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"

//===========================================================

//    Forward declarations

class ToonzScene;
class TFilePath;

namespace DVGui {
class CheckBox;
class LineEdit;
}

class QLabel;
class QRadioButton;
class QPushButton;

//===========================================================

/*!
  This dialog offers a standard 'overwrite file' resolution Dialog.

  Sometimes, the system attempts to write to the file path of an
  already existing file. This dialog presents a standard interface
  to let users choose the conflict resolution manually.

  There are 3 possible resolution choices:

  \li \b Overwrite the existing file
  \li \b Keep the existing file (aborting the write operation)
  \li \b Rename the file to write adding a suffix

  Additionally, the dialog could be \t CANCELED, either by closing it
  or pressing the "Cancel" button.
*/
class OverwriteDialog final : public DVGui::Dialog {
  Q_OBJECT

public:
  enum Resolution {
    CANCELED        = 0x0,
    KEEP_OLD        = 0x1,
    OVERWRITE       = 0x2,
    RENAME          = 0x4,
    ALL_RESOLUTIONS = KEEP_OLD | OVERWRITE | RENAME
  };
  enum Flags { NO_FLAG = 0x0, APPLY_TO_ALL_FLAG = 0x1 };

public:
  struct ExistsFunc {
    virtual QString conflictString(const TFilePath &fp) const = 0;
    virtual bool operator()(const TFilePath &fp) const        = 0;
  };

  struct DecodeFileExistsFunc final : public ExistsFunc {
    ToonzScene *m_scene;
    DecodeFileExistsFunc(ToonzScene *scene) : m_scene(scene) {}

    QString conflictString(const TFilePath &fp) const override;
    bool operator()(const TFilePath &fp) const override;
  };

public:
  OverwriteDialog();

  bool cancelPressed() const { return m_cancelPressed; }

  Resolution execute(TFilePath &filePath, const ExistsFunc &exists,
                     Resolution acceptedRes = ALL_RESOLUTIONS,
                     Flags flags            = NO_FLAG);

  //--------------------- Legacy Functions ------------------------

  // The following functions are deprecated and retained for backward
  // compatibility only

  //! Returns the resolution type chosen by the user
  Resolution getChoice() const { return m_choice; }

  //! Returns the suffix to be added at the base file name when the chosen
  //! resolution type is \t KEEP_OLD.
  std::wstring getSuffix();

  //! Resets state variables
  void reset();

  /*! this method has to be called for each filepath to be imported. Only if
necessary, it opens a popup.
put parameter multiLoad to true only if you are importing more then one level
(so that the button 'apply to all' appears in the dialog)*/
  std::wstring execute(ToonzScene *scene, const TFilePath &levelPath,
                       bool multiLoad);

protected slots:

  void applyToAll();
  void cancel();
  void onButtonClicked(int);

private:
  bool m_applyToAll;
  bool m_cancelPressed;

  Resolution m_choice;

  QLabel *m_label;
  QRadioButton *m_overwrite;
  QRadioButton *m_keep;
  QRadioButton *m_rename;
  DVGui::LineEdit *m_suffix;
  QPushButton *m_okBtn, *m_okToAllBtn, *m_cancelBtn;

private:
  TFilePath addSuffix(const TFilePath &src) const;
};

#endif  // OVERWRITEPOPUP_H
