

#ifndef FILEBROWSERMODEL_INCLUDED
#define FILEBROWSERMODEL_INCLUDED

#include <QAbstractItemModel>
#include <QPixmap>
#include <QMap>
#include "tfilepath.h"
#include "toonz/toonzfolders.h"

#include "versioncontrol.h"

class FileBrowser;
class DvDirVersionControlRootNode;

//-----------------------------------------------------------------------------

class DvDirModelNode
{
	DvDirModelNode *m_parent;
	int m_row;
	bool m_renameEnabled;

protected:
	std::wstring m_name;
	std::wstring m_oldName; // Used for temporary Name
	std::string m_nodeType;
	std::vector<DvDirModelNode *> m_children;
	bool m_childrenValid;

public:
	DvDirModelNode(DvDirModelNode *parent, std::wstring name);
	virtual ~DvDirModelNode();

	DvDirModelNode *getParent() const { return m_parent; }
	std::wstring getName() const { return m_name; }
	virtual bool setName(std::wstring newName) { return false; }

	void setTemporaryName(const std::wstring &newName);
	void restoreName();

	int getRow() const { return m_row; }
	void setRow(int row) { m_row = row; }

	void removeChildren(int row, int count);
	void addChild(DvDirModelNode *child);
	virtual void visualizeContent(FileBrowser *browser) {} // ?????????????

	virtual QPixmap getPixmap(bool isOpen) const;

	int getChildCount();
	DvDirModelNode *getChild(int index);

	virtual void refreshChildren() = 0;
	virtual bool hasChildren();

	virtual int rowByName(const std::wstring &name) const { return -1; } // ?????????????

	void enableRename(bool enabled) { m_renameEnabled = enabled; }
	bool isRenameEnabled() const { return m_renameEnabled; }

	virtual bool isFolder(const TFilePath &folderPath) const { return false; } // ?????????????
	virtual bool isFolder() const { return false; }

	bool areChildrenValid() const { return m_childrenValid; } // ?????????????

	std::string getNodeType() const { return m_nodeType; }

	virtual DvDirModelNode *getNodeByPath(const TFilePath &path) { return 0; }

	virtual DvDirVersionControlRootNode *getVersionControlRootNode() { return 0; }
};

//-----------------------------------------------------------------------------

class DvDirModelFileFolderNode : public DvDirModelNode
{
protected:
	TFilePath m_path;				//!< The folder path
	bool m_isProjectFolder;			//!< Whether this is a project folder
	bool m_existsChecked, m_exists; //!< Whether the folder exists on the file system (a node could exist
									//!< on a remote version control repository, without an actual copy on disk).
	bool m_hasChildren;				//!< Cached info about sub-folders existence
	bool m_peeks;					//!< Whether this folder allows peeking (typically
									//!< to gather additional data on sub-folders existence)
public:
	DvDirModelFileFolderNode(DvDirModelNode *parent, std::wstring name, const TFilePath &path);
	DvDirModelFileFolderNode(DvDirModelNode *parent, const TFilePath &path);

	QPixmap getPixmap(bool isOpen) const;
	TFilePath getPath() const { return m_path; }
	void visualizeContent(FileBrowser *browser); //?????????????

	virtual void refreshChildren();
	virtual bool hasChildren();
	virtual void getChildrenNames(std::vector<std::wstring> &names) const;
	virtual DvDirModelNode *makeChild(std::wstring name);
	int rowByName(const std::wstring &name) const; //?????

	bool setName(std::wstring newName); // chiamarlo rename ????
	bool isFolder(const TFilePath &folderPath) const { return folderPath == m_path; }
	bool isFolder() const { return !m_path.isEmpty(); }

	static DvDirModelFileFolderNode *createNode(DvDirModelNode *parent, const TFilePath &path);

	DvDirModelNode *getNodeByPath(const TFilePath &path);

	bool isProjectFolder() const { return m_isProjectFolder; }
	void setIsProjectFolder(bool yes) { m_isProjectFolder = yes; }

	bool exists();
	void setExists(bool yes) { m_existsChecked = true, m_exists = yes; }

	bool peeks() const { return m_peeks; }
	void setPeeking(bool yes) { m_peeks = yes; }
};

//-----------------------------------------------------------------------------

class DvDirModelSceneFolderNode : public DvDirModelFileFolderNode
{
	std::map<std::wstring, TFilePath> m_folders;

public:
	DvDirModelSceneFolderNode(DvDirModelNode *parent, std::wstring name, const TFilePath &scenePath);
	DvDirModelSceneFolderNode(DvDirModelNode *parent, const TFilePath &path);
	~DvDirModelSceneFolderNode();
	bool setName(std::wstring newName);
	QPixmap getPixmap(bool isOpen) const;
	DvDirModelNode *makeChild(std::wstring name);
	void getChildrenNames(std::vector<std::wstring> &names) const;
	void refreshChildren();
	static DvDirModelFileFolderNode *createNode(DvDirModelNode *parent, const TFilePath &path);
	int rowByName(const std::wstring &name);
};

//-----------------------------------------------------------------------------

class DvDirModelSpecialFileFolderNode : public DvDirModelFileFolderNode
{
	QPixmap m_pixmap;

public:
	DvDirModelSpecialFileFolderNode(DvDirModelNode *parent, std::wstring name, const TFilePath &localPath);
	QPixmap getPixmap(bool isOpen) const;
	void setPixmap(const QPixmap &pixmap);
};

//-----------------------------------------------------------------------------

class DvDirVersionControlNode : public DvDirModelFileFolderNode
{
private:
	QPixmap m_pixmap;
	bool m_isSynched;
	bool m_isUnversioned;
	bool m_isUnderVersionControl;

	// items contained in the folder: Filename, Version Control Status
	QMap<QString, SVNStatus> m_statusMap;

	std::wstring m_oldName;

public:
	DvDirVersionControlNode(DvDirModelNode *parent, std::wstring name, const TFilePath &path);

	DvDirModelNode *makeChild(std::wstring name);
	QPixmap getPixmap(bool isOpen) const;

	void getChildrenNames(std::vector<std::wstring> &names) const;

	QList<TFilePath> getMissingFiles() const;
	QStringList getMissingFiles(const QRegExp &filter) const;
	QList<TFilePath> getMissingFolders() const;

	void insertVersionControlStatus(const QString &fileName, SVNStatus status);
	SVNStatus getVersionControlStatus(const QString &fileName);

	// Refresh the status map
	// Return the list of files that needs to be check against partialLock properties
	// Update the synched and isUnderVersionControl boolean values
	QStringList refreshVersionControl(const QList<SVNStatus> &status);

	// Also enable/disable rename
	void setIsUnderVersionControl(bool value);
	bool isUnderVersionControl() const { return m_isUnderVersionControl; }

	void setIsSynched(bool value) { m_isSynched = value; }
	bool isSynched() const { return m_isSynched; }

	void setIsUnversioned(bool value) { m_isUnversioned = value; }
	bool isUnversioned() const { return m_isUnversioned; }

	DvDirVersionControlRootNode *getVersionControlRootNode();
};

//-----------------------------------------------------------------------------

class DvDirVersionControlRootNode : public DvDirVersionControlNode
{
	QPixmap m_pixmap;
	std::wstring m_repositoryPath;
	std::wstring m_localPath;
	std::wstring m_userName;
	std::wstring m_password;

public:
	DvDirVersionControlRootNode(DvDirModelNode *parent, std::wstring name, const TFilePath &localPath);
	void refreshChildren();

	QPixmap getPixmap(bool isOpen) const { return m_pixmap; }
	void setPixmap(const QPixmap &pixmap) { m_pixmap = pixmap; }

	void setLocalPath(const std::wstring &localPath) { m_localPath = localPath; }
	std::wstring getLocalPath() { return m_localPath; }
	void setRepositoryPath(const std::wstring &repoPath) { m_repositoryPath = repoPath; }
	std::wstring getRepositoryPath() { return m_repositoryPath; }

	void setUserName(const std::wstring &userName) { m_userName = userName; }
	std::wstring getUserName() const { return m_userName; }

	void setPassword(const std::wstring &password) { m_password = password; }
	std::wstring getPassword() const { return m_password; }

	DvDirVersionControlRootNode *getVersionControlRootNode() { return this; }
};

//-----------------------------------------------------------------------------

class DvDirVersionControlProjectNode : public DvDirVersionControlNode
{

public:
	DvDirVersionControlProjectNode(DvDirModelNode *parent, std::wstring name, const TFilePath &path);
	TFilePath getProjectPath() const;
	bool isCurrent() const;
	void makeCurrent();
	QPixmap getPixmap(bool isOpen) const;
	void refreshChildren();
	void getChildrenNames(std::vector<std::wstring> &names) const;
	//DvDirModelNode *makeChild(std::wstring name);
};

//-----------------------------------------------------------------------------

class DvDirModelProjectNode : public DvDirModelFileFolderNode
{
public:
	DvDirModelProjectNode(DvDirModelNode *parent, const TFilePath &path);
	TFilePath getProjectPath() const;
	bool isCurrent() const;
	void makeCurrent();
	QPixmap getPixmap(bool isOpen) const;
	void refreshChildren();
	void getChildrenNames(std::vector<std::wstring> &names) const;
	DvDirModelNode *makeChild(std::wstring name);
};

//-----------------------------------------------------------------------------

class DvDirModelDayNode : public DvDirModelNode
{
	std::string m_dayDateString;

public:
	DvDirModelDayNode(DvDirModelNode *parent, std::wstring dayDateString);
	void refreshChildren() {}
	void visualizeContent(FileBrowser *browser); //??????????????????
	QPixmap getPixmap(bool isOpen) const;
};

//-----------------------------------------------------------------------------

class DvDirModelHistoryNode : public DvDirModelNode
{

public:
	DvDirModelHistoryNode(DvDirModelNode *parent);
	void refreshChildren();
	QPixmap getPixmap(bool isOpen) const;
};

//-----------------------------------------------------------------------------

class DvDirModelMyComputerNode : public DvDirModelNode
{
public:
	DvDirModelMyComputerNode(DvDirModelNode *parent);
	void refreshChildren();
	QPixmap getPixmap(bool isOpen) const;
	bool isFolder() const { return true; }
};

//-----------------------------------------------------------------------------

class DvDirModelNetworkNode : public DvDirModelNode
{
public:
	DvDirModelNetworkNode(DvDirModelNode *parent);
	void refreshChildren();
	QPixmap getPixmap(bool isOpen) const;
	bool isFolder() const { return true; }
};

//-----------------------------------------------------------------------------

class DvDirModelRootNode : public DvDirModelNode
{

	std::vector<DvDirModelFileFolderNode *> m_projectRootNodes;
	std::vector<DvDirModelFileFolderNode *> m_versionControlNodes;
	DvDirModelMyComputerNode *m_myComputerNode;
	DvDirModelNetworkNode *m_networkNode;
	DvDirModelProjectNode *m_sandboxProjectNode;

	void add(std::wstring name, const TFilePath &path);

public:
	DvDirModelRootNode();
	void refreshChildren();

	DvDirModelNode *getNodeByPath(const TFilePath &path);
	// QPixmap getPixmap(bool isOpen) const;
};

//=============================================================================

// singleton
class DvDirModel : public QAbstractItemModel,
				   public FolderListenerManager::Listener
{
	DvDirModelNode *m_root;

public:
	DvDirModel();
	~DvDirModel();

	static DvDirModel *instance();

	void onFolderChanged(const TFilePath &path);

	DvDirModelNode *getNode(const QModelIndex &index) const;

	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &index) const;

	QModelIndex getIndexByPath(const TFilePath &path) const;
	QModelIndex getIndexByNode(DvDirModelNode *node) const;

	QModelIndex childByName(const QModelIndex &parent, const std::wstring &name) const;

	int columnCount(const QModelIndex &parent) const { return 1; }
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
	void refresh(const QModelIndex &index);
	void refreshFolder(const TFilePath &folderPath, const QModelIndex &i = QModelIndex());
	void refreshFolderChild(const QModelIndex &i = QModelIndex());
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());

	QModelIndex getCurrentProjectIndex() const;
};

#endif
