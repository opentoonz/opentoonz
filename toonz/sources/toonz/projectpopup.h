#pragma once

#ifndef PROJECTPOPUP_H
#define PROJECTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/tproject.h"
#include "filebrowsermodel.h"
#include <QList>

// forward declaration
namespace DVGui {
class FileField;
class LineEdit;
class CheckBox;
}

class DvDirTreeView;
class QComboBox;

//=============================================================================
// ProjectDvDirModelRootNode

class ProjectDvDirModelRootNode final : public DvDirModelNode {
public:
  ProjectDvDirModelRootNode();
  void refreshChildren() override;
};

//=============================================================================
// ProjectDvDirModelFileFolderNode

class ProjectDvDirModelFileFolderNode : public DvDirModelFileFolderNode {
public:
  ProjectDvDirModelFileFolderNode(DvDirModelNode *parent, std::wstring name,
                                  const TFilePath &path)
      : DvDirModelFileFolderNode(parent, name, path) {}
  ProjectDvDirModelFileFolderNode(DvDirModelNode *parent, const TFilePath &path)
      : DvDirModelFileFolderNode(parent, path) {}
  DvDirModelNode *makeChild(std::wstring name) override;
  DvDirModelFileFolderNode *createNode(DvDirModelNode *parent,
                                       const TFilePath &path);
};

//=============================================================================
// ProjectDvDirModelSpecialFileFolderNode

class ProjectDvDirModelSpecialFileFolderNode final
    : public ProjectDvDirModelFileFolderNode {
  QPixmap m_pixmap;

public:
  ProjectDvDirModelSpecialFileFolderNode(DvDirModelNode *parent,
                                         std::wstring name,
                                         const TFilePath &path)
      : ProjectDvDirModelFileFolderNode(parent, name, path) {}
  QPixmap getPixmap(bool isOpen) const override { return m_pixmap; }
  void setPixmap(const QPixmap &pixmap) { m_pixmap = pixmap; }
};

//=============================================================================
// ProjectDvDirModelProjectNode

class ProjectDvDirModelProjectNode final
    : public ProjectDvDirModelFileFolderNode {
public:
  ProjectDvDirModelProjectNode(DvDirModelNode *parent, const TFilePath &path)
      : ProjectDvDirModelFileFolderNode(parent, path) {}
  void makeCurrent() {}
  QPixmap getPixmap(bool isOpen) const override;
};

//=============================================================================
// ProjectDirModel

class ProjectDirModel final : public QAbstractItemModel {
  DvDirModelNode *m_root;

public:
  ProjectDirModel();
  ~ProjectDirModel();

  DvDirModelNode *getNode(const QModelIndex &index) const;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  QModelIndex childByName(const QModelIndex &parent,
                          const std::wstring &name) const;
  int columnCount(const QModelIndex &parent) const override { return 1; }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  bool hasChildren(const QModelIndex &parent) const override;
  void refresh(const QModelIndex &index);
  void refreshFolderChild(const QModelIndex &i = QModelIndex());
  QModelIndex getIndexByNode(DvDirModelNode *node) const;
};

//=============================================================================
// ProjectPopup
//-----------------------------------------------------------------------------

class ProjectPopup : public DVGui::Dialog, public TProjectManager::Listener {
  Q_OBJECT

protected:
  ProjectDirModel *m_model;
  DvDirTreeView *m_treeView;
  DVGui::LineEdit *m_nameFld;
  QList<QPair<std::string, DVGui::FileField *>> m_folderFlds;
  QList<QPair<std::string, DVGui::CheckBox *>> m_useScenePathCbs;

  QLabel *m_prjNameLabel;
  QLabel *m_choosePrjLabel;
  QComboBox *m_chooseProjectCombo;
  QList<TFilePath> m_projectPaths;

public:
  ProjectPopup(bool isModal);
  // da TProjectManager::Listener
  void onProjectSwitched() override;
  // da TProjectManager::Listener
  void onProjectChanged() override {}

  void updateProjectFromFields(TProject *);
  void updateFieldsFromProject(TProject *);

  void updateChooseProjectCombo();

protected:
  void showEvent(QShowEvent *) override;
};

//=============================================================================
// ProjectSettingsPopup
//-----------------------------------------------------------------------------

class ProjectSettingsPopup final : public ProjectPopup {
  Q_OBJECT

public:
  ProjectSettingsPopup();

public slots:
  void onFolderChanged();
  void onUseSceneChekboxChanged(int);

  void onChooseProjectChanged(int);
};

//=============================================================================
// ProjectCreatePopup
//-----------------------------------------------------------------------------

class ProjectCreatePopup final : public ProjectPopup {
  Q_OBJECT

public:
  ProjectCreatePopup();

public slots:
  void createProject();

protected:
  void showEvent(QShowEvent *) override;
};

#endif  // PROJECTPOPUP_H
