#pragma once

#ifndef LOADFOLDERPOPUP_H
#define LOADFOLDERPOPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"

//==============================================================

//    Forward declarations

class TFilePath;
class DvDirTreeView;

//==============================================================

//**********************************************************************************
//    LoadFolderPopup  declaration
//**********************************************************************************

/*!
  \brief    Popup widget used to invoke the <I>Load Folder</I> command.
*/

class LoadFolderPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  LoadFolderPopup(QWidget *parent = 0);  //!< Constructs the popup as child of
                                         //! an optional parent widget.

  /*! \details  The path returned by getFolder() could be empty in case
          there is no selected folder, or the selection is not a
          valid folder.                                                       */

  TFilePath getFolder() const;  //!< Returns currently selected folder path.
  //!  \return  Currently selected folder path, <I>if any</I>.
private:
  DvDirTreeView
      *m_treeView;  //!< [\p child] The folder tree view used for selection.

private slots:

  void onOk();  //!< Invokes IoCmd::loadResourceFolder() appropriately.
};

#endif  // LOADFOLDERPOPUP_H
