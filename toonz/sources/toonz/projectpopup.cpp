

#include "projectpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "filebrowser.h"
#include "iocommand.h"
#include "filebrowsermodel.h"
#include "dvdirtreeview.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"

// TnzLic includes
#ifdef LINETEST
#include "tnzcamera.h"
#endif

// TnzCore includes
#include "tsystem.h"
#include "tenv.h"

// Qt includes
#include <QPushButton>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMainWindow>
#include <QComboBox>

using namespace DVGui;

//=============================================================================
// ProjectDvDirModelProjectNode
//-----------------------------------------------------------------------------

QPixmap ProjectDvDirModelProjectNode::getPixmap(bool isOpen) const {
  static QPixmap openProjectPixmap(
      svgToPixmap(":Resources/browser_project_open.svg"));
  static QPixmap closeProjectPixmap(
      svgToPixmap(":Resources/browser_project_close.svg"));
  return isOpen ? openProjectPixmap : closeProjectPixmap;
}

//=============================================================================
// ProjectDvDirModelFileFolderNode [Root]
//-----------------------------------------------------------------------------

DvDirModelNode *ProjectDvDirModelFileFolderNode::makeChild(std::wstring name) {
  return createNode(this, m_path + name);
}

//-----------------------------------------------------------------------------

DvDirModelFileFolderNode *ProjectDvDirModelFileFolderNode::createNode(
    DvDirModelNode *parent, const TFilePath &path) {
  DvDirModelFileFolderNode *node;
  if (TProjectManager::instance()->isProject(path))
    node = new ProjectDvDirModelProjectNode(parent, path);
  else
    node = new ProjectDvDirModelFileFolderNode(parent, path);
  return node;
}

//=============================================================================
// ProjectDvDirModelSpecialFileFolderNode
//-----------------------------------------------------------------------------

//=============================================================================
// ProjectDvDirModelRootNode [Root]
//-----------------------------------------------------------------------------

ProjectDvDirModelRootNode::ProjectDvDirModelRootNode()
    : DvDirModelNode(0, L"Root") {
  m_nodeType = "Root";
}

//-----------------------------------------------------------------------------

void ProjectDvDirModelRootNode::refreshChildren() {
  m_childrenValid = true;
  if (m_children.empty()) {
    TProjectManager *pm = TProjectManager::instance();
    std::vector<TFilePath> projectRoots;
    pm->getProjectRoots(projectRoots);

    int i;
    for (i = 0; i < (int)projectRoots.size(); i++) {
      TFilePath projectRoot = projectRoots[i];
      std::wstring rootDir  = projectRoot.getWideString();
      ProjectDvDirModelSpecialFileFolderNode *projectRootNode =
          new ProjectDvDirModelSpecialFileFolderNode(
              this, L"Project root (" + rootDir + L")", projectRoot);
      projectRootNode->setPixmap(svgToPixmap(":Resources/projects.svg"));
      addChild(projectRootNode);
    }

    // SVN Repository
    QList<SVNRepository> repositories =
        VersionControl::instance()->getRepositories();
    int count = repositories.size();
    for (int i = 0; i < count; i++) {
      SVNRepository repo = repositories.at(i);

      ProjectDvDirModelSpecialFileFolderNode *node =
          new ProjectDvDirModelSpecialFileFolderNode(
              this, repo.m_name.toStdWString(),
              TFilePath(repo.m_localPath.toStdWString()));
      node->setPixmap(svgToPixmap(":Resources/vcroot.svg"));
      addChild(node);
    }
  }
}

//=============================================================================
// ProjectDirModel
//-----------------------------------------------------------------------------

ProjectDirModel::ProjectDirModel() {
  m_root = new ProjectDvDirModelRootNode();
  m_root->refreshChildren();
}

//-----------------------------------------------------------------------------

ProjectDirModel::~ProjectDirModel() { delete m_root; }

//-----------------------------------------------------------------------------

DvDirModelNode *ProjectDirModel::getNode(const QModelIndex &index) const {
  if (index.isValid())
    return static_cast<DvDirModelNode *>(index.internalPointer());
  else
    return m_root;
}

//-----------------------------------------------------------------------------

QModelIndex ProjectDirModel::index(int row, int column,
                                   const QModelIndex &parent) const {
  if (column != 0) return QModelIndex();
  DvDirModelNode *parentNode       = m_root;
  if (parent.isValid()) parentNode = getNode(parent);
  if (row < 0 || row >= parentNode->getChildCount()) return QModelIndex();
  DvDirModelNode *node = parentNode->getChild(row);
  return createIndex(row, column, node);
}

//-----------------------------------------------------------------------------

QModelIndex ProjectDirModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) return QModelIndex();
  DvDirModelNode *node       = getNode(index);
  DvDirModelNode *parentNode = node->getParent();
  if (!parentNode || parentNode == m_root)
    return QModelIndex();
  else
    return createIndex(parentNode->getRow(), 0, parentNode);
}

//-----------------------------------------------------------------------------

QModelIndex ProjectDirModel::childByName(const QModelIndex &parent,
                                         const std::wstring &name) const {
  if (!parent.isValid()) return QModelIndex();
  DvDirModelNode *parentNode = getNode(parent);
  if (!parentNode) return QModelIndex();
  int row = parentNode->rowByName(name);
  if (row < 0 || row >= parentNode->getChildCount()) return QModelIndex();
  DvDirModelNode *childNode = parentNode->getChild(row);
  return createIndex(row, 0, childNode);
}

//-----------------------------------------------------------------------------

int ProjectDirModel::rowCount(const QModelIndex &parent) const {
  DvDirModelNode *node = getNode(parent);
  int childCount       = node->getChildCount();
  return childCount;
}

//-----------------------------------------------------------------------------

QVariant ProjectDirModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();
  DvDirModelNode *node = getNode(index);
  if (role == Qt::DisplayRole || role == Qt::EditRole)
    return QString::fromStdWString(node->getName());
  else if (role == Qt::DecorationRole) {
    return QVariant();
  } else if (role == Qt::ForegroundRole) {
    if (!node || !node->isRenameEnabled())
      return QBrush(Qt::blue);
    else
      return QVariant();
  } else
    return QVariant();
}

//-----------------------------------------------------------------------------

Qt::ItemFlags ProjectDirModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags flags = QAbstractItemModel::flags(index);
  if (index.isValid()) {
    DvDirModelNode *node = getNode(index);
    if (node && node->isRenameEnabled()) flags |= Qt::ItemIsEditable;
  }
  return flags;
}

//-----------------------------------------------------------------------------

bool ProjectDirModel::setData(const QModelIndex &index, const QVariant &value,
                              int role) {
  if (!index.isValid()) return false;
  DvDirModelNode *node = getNode(index);
  if (!node || !node->isRenameEnabled()) return false;
  QString newName = value.toString();
  if (newName == "") return false;
  if (!node->setName(newName.toStdWString())) return false;
  emit dataChanged(index, index);
  return true;
}

//-----------------------------------------------------------------------------

bool ProjectDirModel::hasChildren(const QModelIndex &parent) const {
  DvDirModelNode *node = getNode(parent);
  return node->hasChildren();
}

//-----------------------------------------------------------------------------

void ProjectDirModel::refresh(const QModelIndex &index) {
  if (!index.isValid()) return;
  DvDirModelNode *node = getNode(index);
  if (!node) return;
  emit layoutAboutToBeChanged();
  emit beginRemoveRows(index, 0, node->getChildCount());
  node->refreshChildren();
  emit endRemoveRows();
  emit layoutChanged();
}

//-----------------------------------------------------------------------------

void ProjectDirModel::refreshFolderChild(const QModelIndex &i) {
  DvDirModelNode *node = getNode(i);
  if (!node || !node->areChildrenValid()) return;

  if (node->isFolder() || dynamic_cast<DvDirModelMyComputerNode *>(node))
    refresh(i);
  int count = rowCount(i);
  int r;
  for (r = 0; r < count; r++) refreshFolderChild(index(r, 0, i));
}

//-----------------------------------------------------------------------------

QModelIndex ProjectDirModel::getIndexByNode(DvDirModelNode *node) const {
  if (!node) return QModelIndex();
  return createIndex(node->getRow(), 0, node);
}

//=============================================================================
// ProjectPopup
//-----------------------------------------------------------------------------

ProjectPopup::ProjectPopup(bool isModal)
    : Dialog(TApp::instance()->getMainWindow(), isModal, false, "Project") {
  TProjectManager *pm = TProjectManager::instance();

  m_choosePrjLabel     = new QLabel(tr("Project:"), this);
  m_chooseProjectCombo = new QComboBox();
  m_prjNameLabel       = new QLabel(tr("Project Name:"), this);
  m_nameFld            = new LineEdit();
  m_model              = new ProjectDirModel;
  m_treeView           = new DvDirTreeView(this);

  m_nameFld->setMaximumHeight(WidgetHeight);
  m_treeView->setModel(m_model);
  m_treeView->setStyleSheet("border:1px solid rgb(120,120,120);");

  //----layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(10);
  {
    m_topLayout->addWidget(m_treeView, 0);

    QGridLayout *upperLayout = new QGridLayout();
    upperLayout->setMargin(5);
    upperLayout->setHorizontalSpacing(5);
    upperLayout->setVerticalSpacing(10);
    {
      upperLayout->addWidget(m_choosePrjLabel, 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_chooseProjectCombo, 0, 1);

      upperLayout->addWidget(m_prjNameLabel, 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_nameFld, 1, 1);
    }
    upperLayout->setColumnStretch(0, 0);
    upperLayout->setColumnStretch(1, 1);

    std::vector<std::string> folderNames;
    pm->getFolderNames(folderNames);
    int i;
    for (i = 0; i < (int)folderNames.size(); i++) {
      std::string name = folderNames[i];
      QString qName    = QString::fromStdString(name);
      FileField *ff    = new FileField(0, qName);
      m_folderFlds.append(qMakePair(name, ff));
      upperLayout->addWidget(new QLabel("+" + qName, this), i + 2, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(ff, i + 2, 1);
    }
    struct {
      QString name;
      std::string folderName;
    } cbs[] = {{tr("Append $scenepath to +drawings"), TProject::Drawings},
               {tr("Append $scenepath to +inputs"), TProject::Inputs},
               {tr("Append $scenepath to +extras"), TProject::Extras}};
    int currentRow = upperLayout->rowCount();

    for (i = 0; i < tArrayCount(cbs); i++) {
      CheckBox *cb = new CheckBox(cbs[i].name);
      cb->setMaximumHeight(WidgetHeight);
      upperLayout->addWidget(cb, currentRow + i, 1);
      m_useScenePathCbs.append(qMakePair(cbs[i].folderName, cb));
    }
    m_topLayout->addLayout(upperLayout);
  }

  pm->addListener(this);
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateChooseProjectCombo() {
  m_projectPaths.clear();
  m_chooseProjectCombo->clear();

  TFilePath sandboxFp = TProjectManager::instance()->getSandboxProjectFolder() +
                        "sandbox_otprj.xml";
  m_projectPaths.push_back(sandboxFp);
  m_chooseProjectCombo->addItem("sandbox");

  std::vector<TFilePath> prjRoots;
  TProjectManager::instance()->getProjectRoots(prjRoots);
  for (int i = 0; i < prjRoots.size(); i++) {
    TFilePathSet fps;
    TSystem::readDirectory_Dir_ReadExe(fps, prjRoots[i]);

    TFilePathSet::iterator it;
    for (it = fps.begin(); it != fps.end(); ++it) {
      TFilePath fp(*it);
      if (TProjectManager::instance()->isProject(fp)) {
        m_projectPaths.push_back(
            TProjectManager::instance()->projectFolderToProjectPath(fp));
        m_chooseProjectCombo->addItem(QString::fromStdString(fp.getName()));
      }
    }
  }

  for (int i = 0; i < m_projectPaths.size(); i++) {
    if (TProjectManager::instance()->getCurrentProjectPath() ==
        m_projectPaths[i]) {
      m_chooseProjectCombo->setCurrentIndex(i);
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateFieldsFromProject(TProject *project) {
  m_nameFld->setText(toQString(project->getName()));
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    std::string folderName = m_folderFlds[i].first;
    TFilePath path         = project->getFolder(folderName);
    m_folderFlds[i].second->setPath(toQString(path));
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    std::string folderName = m_useScenePathCbs[i].first;
    Qt::CheckState cbState =
        project->getUseScenePath(folderName) ? Qt::Checked : Qt::Unchecked;
    CheckBox *cb                = m_useScenePathCbs[i].second;
    bool signalesAlreadyBlocked = cb->blockSignals(true);
    cb->setCheckState(cbState);
    cb->blockSignals(signalesAlreadyBlocked);
  }
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateProjectFromFields(TProject *project) {
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    std::string folderName = m_folderFlds[i].first;
    TFilePath folderPath(m_folderFlds[i].second->getPath().toStdWString());
    project->setFolder(folderName, folderPath);
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    std::string folderName = m_useScenePathCbs[i].first;
    int cbState            = m_useScenePathCbs[i].second->checkState();
    bool useScenePath      = cbState == Qt::Checked;
    project->setUseScenePath(folderName, cbState);
  }
  TProjectManager::instance()->notifyProjectChanged();
}

//-----------------------------------------------------------------------------

void ProjectPopup::onProjectSwitched() {
  TProjectP currentProject = TProjectManager::instance()->getCurrentProject();
  updateFieldsFromProject(currentProject.getPointer());
}

//-----------------------------------------------------------------------------

void ProjectPopup::showEvent(QShowEvent *) {
  // Must refresh the tree.
  DvDirModelNode *rootNode = m_model->getNode(QModelIndex());
  QModelIndex index        = m_model->getIndexByNode(rootNode);
  m_model->refreshFolderChild(index);
  TProjectP currentProject = TProjectManager::instance()->getCurrentProject();
  updateFieldsFromProject(currentProject.getPointer());
  updateChooseProjectCombo();
}

//=============================================================================
/*! \class ProjectSettingsPopup
                \brief The ProjectSettingsPopup class provides a dialog to
   change current project settings.

                Inherits \b ProjectPopup.
*/
//-----------------------------------------------------------------------------

ProjectSettingsPopup::ProjectSettingsPopup() : ProjectPopup(false) {
  setWindowTitle(tr("Project Settings"));

  m_prjNameLabel->hide();
  m_nameFld->hide();
  m_choosePrjLabel->show();
  m_chooseProjectCombo->show();

  m_treeView->hide();
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    FileField *ff = m_folderFlds[i].second;
    connect(ff, SIGNAL(pathChanged()), this, SLOT(onFolderChanged()));
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    CheckBox *cb = m_useScenePathCbs[i].second;
    connect(cb, SIGNAL(stateChanged(int)), this,
            SLOT(onUseSceneChekboxChanged(int)));
  }

  connect(m_chooseProjectCombo, SIGNAL(activated(int)), this,
          SLOT(onChooseProjectChanged(int)));
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::onChooseProjectChanged(int index) {
  TFilePath projectFp = m_projectPaths[index];

  TProjectManager::instance()->setCurrentProjectPath(projectFp);
  TProject *projectP =
      TProjectManager::instance()->getCurrentProject().getPointer();
  updateFieldsFromProject(projectP);
  IoCmd::saveSceneIfNeeded("Change project");
  IoCmd::newScene();
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::onFolderChanged() {
  TProjectP project = TProjectManager::instance()->getCurrentProject();
  updateProjectFromFields(project.getPointer());
  try {
    project->save();
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }
  DvDirModel::instance()->refreshFolder(project->getProjectFolder());
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::onUseSceneChekboxChanged(int) {
  TProjectP project = TProjectManager::instance()->getCurrentProject();
  updateProjectFromFields(project.getPointer());
  try {
    project->save();
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }
  DvDirModel::instance()->refreshFolder(project->getProjectFolder());
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ProjectSettingsPopup> openProjectSettingsPopup(
    MI_ProjectSettings);

//=============================================================================
/*! \class ProjectCreatePopup
                \brief The ProjectCreatePopup class provides a modal dialog to
   create a new project.

                Inherits \b ProjectPopup.
*/
//-----------------------------------------------------------------------------

ProjectCreatePopup::ProjectCreatePopup() : ProjectPopup(true) {
  setWindowTitle(tr("New Project"));

  QPushButton *okBtn = new QPushButton(tr("OK"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(createProject()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(20);
  {
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(okBtn);
    m_buttonLayout->addWidget(cancelBtn);
  }

  m_prjNameLabel->show();
  m_nameFld->show();
  m_choosePrjLabel->hide();
  m_chooseProjectCombo->hide();
}

//-----------------------------------------------------------------------------

void ProjectCreatePopup::createProject() {
  if (!IoCmd::saveSceneIfNeeded(QObject::tr("Create project"))) return;

#ifdef LINETEST
  TnzCamera *camera = TnzCamera::instance();
  if (camera->isCameraConnected()) camera->cameraDisconnect();
#endif

  QFileInfo fi(m_nameFld->text());

  if (!isValidFileName(fi.baseName())) {
    error(
        tr("Project Name cannot be empty or contain any of the following "
           "characters:\n \\ / : * ? \" < > |"));
    return;
  }

  TProjectManager *pm   = TProjectManager::instance();
  TFilePath projectName = TFilePath(m_nameFld->text().toStdWString());
  if (projectName == TFilePath()) {
    return;
  }

  if (projectName.isAbsolute()) {
    error(tr("Bad project name: '%1' looks like an absolute file path")
              .arg(m_nameFld->text()));
    return;
  }

  if (pm->getProjectPathByName(projectName) != TFilePath()) {
    error(tr("Project '%1' already exists").arg(m_nameFld->text()));
    // project already exists
    return;
  }

  TFilePath currentProjectRoot;
  DvDirModelFileFolderNode *node =
      dynamic_cast<DvDirModelFileFolderNode *>(m_treeView->getCurrentNode());
  if (node)
    currentProjectRoot = node->getPath();
  else
    currentProjectRoot = pm->getCurrentProjectRoot();

  TFilePath projectFolder = currentProjectRoot + projectName;
  TFilePath projectPath   = pm->projectFolderToProjectPath(projectFolder);
  TProject *project       = new TProject();
  updateProjectFromFields(project);
  TProjectP currentProject = pm->getCurrentProject();
  project->setSceneProperties(currentProject->getSceneProperties());
  try {
    bool isSaved = project->save(projectPath);
    if (!isSaved)
      DVGui::error(tr("It is not possible to create the %1 project.")
                       .arg(toQString(projectPath)));
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }
  pm->setCurrentProjectPath(projectPath);
  IoCmd::newScene();
  DvDirModel::instance()->refreshFolder(projectFolder.getParentDir());
  accept();
}

//-----------------------------------------------------------------------------

void ProjectCreatePopup::showEvent(QShowEvent *) {
  int i;
  QString fldName;
  for (i = 0; i < m_folderFlds.size(); i++) {
    fldName = QString::fromStdString(m_folderFlds[i].first);
    m_folderFlds[i].second->setPath(fldName);
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    CheckBox *cb                = m_useScenePathCbs[i].second;
    bool signalesAlreadyBlocked = cb->blockSignals(true);
    cb->setChecked(false);
    cb->blockSignals(signalesAlreadyBlocked);
  }

  m_nameFld->setText("");
  // Must refresh the tree.
  DvDirModelNode *rootNode = m_model->getNode(QModelIndex());
  QModelIndex index        = m_model->getIndexByNode(rootNode);
  m_model->refreshFolderChild(index);
  // Select the first Item in the treeView
  QItemSelectionModel *selection = new QItemSelectionModel(m_model);
  index                          = m_model->index(0, 0, QModelIndex());
  selection->select(index, QItemSelectionModel::Select);
  m_treeView->setSelectionModel(selection);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ProjectCreatePopup> openProjectCreatePopup(
    MI_NewProject);
