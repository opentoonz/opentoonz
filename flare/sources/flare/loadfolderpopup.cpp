

#include "loadfolderpopup.h"

// Tnz6 includes
#include "dvdirtreeview.h"
#include "iocommand.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QPushButton>

//**********************************************************************************
//    LoadFolderPopup  definition
//**********************************************************************************

LoadFolderPopup::LoadFolderPopup(QWidget *parent)
    : DVGui::Dialog(parent, true, false, tr("Load Folder")) {
  // Tree view
  m_treeView = new DvDirTreeView(this);
  addWidget(m_treeView);

  // Buttons bar
  QPushButton *okBtn = new QPushButton(QString(QObject::tr("Ok")), this);
  addButtonBarWidget(okBtn);

  QPushButton *cancelBtn =
      new QPushButton(QString(QObject::tr("Cancel")), this);
  addButtonBarWidget(cancelBtn);

  // Connect signals/slots
  bool ret = true;

  ret = ret && connect(okBtn, SIGNAL(clicked()), SLOT(onOk()));
  ret = ret && connect(okBtn, SIGNAL(clicked()), SLOT(close()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked()), SLOT(close()));

  assert(ret);
}

//------------------------------------------------------------------------

TFilePath LoadFolderPopup::getFolder() const {
  assert(m_treeView->getCurrentNode()->isFolder());
  return m_treeView->getCurrentPath();
}

//------------------------------------------------------------------------

void LoadFolderPopup::onOk() {
  const TFilePath &folder = getFolder();
  if (folder.isEmpty()) return;

  IoCmd::LoadResourceArguments args(folder);
  IoCmd::loadResourceFolders(args);
}
