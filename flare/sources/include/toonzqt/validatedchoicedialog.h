#pragma once

#ifndef VALIDATEDCHOICEDIALOG_H
#define VALIDATEDCHOICEDIALOG_H

// TnzCore includes
#include "tcommon.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class QButtonGroup;

//====================================================

//************************************************************************
//    ValidatedChoiceDialog  declaration
//************************************************************************

namespace DVGui {

/*!
  \brief    Base class for validated multi-choice user dialogs.

  \warning  For correct usage, observe the guidelines described in the
            ValidatedChoiceDialog() constructor.
*/

class DVAPI ValidatedChoiceDialog : public DVGui::Dialog {
  Q_OBJECT

public:
  enum Options         //!  Construction options
  { NO_OPTIONS = 0x0,  //!< No special options.
    APPLY_TO_ALL =
        0x1  //!< Has the "Apply to All" button. An <I>applied to all</I>
             //!  resolution persists as the default without-prompt resolution
             //!  until the next call to reset().
  };

  enum DefaultResolution  //!  Basic resolutions supported by the dialog.
  { NO_REQUIRED_RESOLUTION =
        0,       //!< An object supplied for validation turns out to be valid,
                 //!  no need for any resolution.
    CANCEL = 1,  //!< Resolution selection was canceled. Stop processing.

    DEFAULT_RESOLUTIONS_COUNT };

public:
  /*! \warning  Due to compatibility with DVGui::Dialog's interface, this class
reserves
          invocation of \p Dialog::beginVLayout() during class construction -
          however, <B>\p Dialog::endVLayout() invocation is responsibility of
          derived classes</B> after resolution choices have been manually added
          to the layout.

\warning  Users are required to respect the default resolution ids when adding
          buttons into \p m_buttonGroup. See the \p DefaultResolution enum for a
          list of all default resolutions. */

  ValidatedChoiceDialog(QWidget *parent,
                        Options opts = NO_OPTIONS);  //!< Constructs the dialog
                                                     //! with no default
  //! resolutions.
  //!\param parent  Parent
  //! top-level widget.
  //!\param Opts
  //! Construction options.

  /*! \param    obj  The object to resolve.
\return        The accepted resolution. */

  int execute(
      void *obj);  //!< Prompts the dialog asking for user input, until a
                   //!  valid resolution for the specified object is selected or
                   //!  the action is canceled.
  virtual void
  reset();  //!< Clears accepted resolutions - should be used together
            //!  with option \p APPLY_TO_ALL.
protected:
  QButtonGroup *m_buttonGroup;  //!< Group of abstract buttons representing all
                                //!  available resolutions.
protected:
  /*! \details  Resolution \p NO_REQUIRED_RESOLUTION is always tested before
other
          proposed resolutions.

\return   An error message upon failure. */

  virtual QString acceptResolution(
      void *obj,  //!< The type-erased object to be resolved. May be modified.
      int resolution,  //!< The <I>button id</I> associated to the selected
                       //! resolution.
      bool applyToAll  //!< Whether user selected the resolution for all
                       //! successive prompts.
      ) = 0;           //!< Attempts enforcement of the selected resolution.

  /*! \details  This function can be used to initialize the widget as soon as
          user interaction is required. Use \p showEvent() in case the
          initialization is needed at \a every interaction time. */

  virtual void initializeUserInteraction(const void *obj) {
  }  //!< Invoked at max \a once per \p execute(), immediately
     //!  before the dialog is shown.
  bool appliedToAll() const {
    return m_appliedToAll;
  }  //!< Returns whether the "Apply To All" button has been hit.
  int appliedToAllResolution() const {
    return m_appliedToAllRes;
  }  //!< Returns the resolution associated to the "Apply To All"
     //!  button.
private:
  QLabel *m_label;  //!< Label showing a description of the validation problem.

  int m_appliedToAllRes;  //!< Resolution applied in the \p APPLY_TO_ALL case.
  bool m_appliedToAll,    //!< Whether the \p APPLY_TO_ALL option has previously
                          //! been selected.
      m_applyToAll;  //!< Whether the \p APPLY_TO_ALL option has been selected
                     //! in
  //!  \a current user interaction.
private slots:

  void onApplyToAll();
};

}  // namespace DVGui

#endif  // VALIDATEDCHOICEDIALOG_H
