

#ifndef EXPORTSCENEPOPUP_H
#define EXPORTSCENEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "tfilepath.h"
#include "filebrowsermodel.h"

#include <QTreeView>
#include <QItemDelegate>

// forward declaration
class QLabel;
class ExportSceneTreeView;
class QRadioButton;

//=============================================================================
// ExportSceneDvDirModelFileFolderNode

class ExportSceneDvDirModelFileFolderNode : public DvDirModelFileFolderNode
{
public:
	ExportSceneDvDirModelFileFolderNode(DvDirModelNode *parent, std::wstring name, const TFilePath &path)
		: DvDirModelFileFolderNode(parent, name, path) {}
	ExportSceneDvDirModelFileFolderNode(DvDirModelNode *parent, const TFilePath &path)
		: DvDirModelFileFolderNode(parent, path) {}

	DvDirModelNode *makeChild(std::wstring name);
	DvDirModelFileFolderNode *createExposeSceneNode(DvDirModelNode *parent, const TFilePath &path);
};

//=============================================================================
// ExportSceneDvDirModelSpecialFileFolderNode

class ExportSceneDvDirModelSpecialFileFolderNode : public ExportSceneDvDirModelFileFolderNode
{
	QPixmap m_pixmap;

public:
	ExportSceneDvDirModelSpecialFileFolderNode(DvDirModelNode *parent, std::wstring name, const TFilePath &path)
		: ExportSceneDvDirModelFileFolderNode(parent, name, path) {}
	QPixmap getPixmap(bool isOpen) const { return m_pixmap; }
	void setPixmap(const QPixmap &pixmap) { m_pixmap = pixmap; }
};

//=============================================================================
// ExportSceneDvDirModelProjectNode

class ExportSceneDvDirModelProjectNode : public ExportSceneDvDirModelFileFolderNode
{
public:
	ExportSceneDvDirModelProjectNode(DvDirModelNode *parent, const TFilePath &path)
		: ExportSceneDvDirModelFileFolderNode(parent, path) {}
	void makeCurrent() {}
	QPixmap getPixmap(bool isOpen) const;
};

//=============================================================================
// ExportSceneDvDirModelRootNode

class ExportSceneDvDirModelRootNode : public DvDirModelNode
{
	std::vector<ExportSceneDvDirModelFileFolderNode *> m_projectRootNodes;
	ExportSceneDvDirModelFileFolderNode *m_sandboxProjectNode;
	void add(std::wstring name, const TFilePath &path);

public:
	ExportSceneDvDirModelRootNode();

	void refreshChildren();
	DvDirModelNode *getNodeByPath(const TFilePath &path);
};

//=============================================================================
// ExportSceneDvDirModel

class ExportSceneDvDirModel : public QAbstractItemModel
{
	DvDirModelNode *m_root;

public:
	ExportSceneDvDirModel();
	~ExportSceneDvDirModel();

	DvDirModelNode *getNode(const QModelIndex &index) const;
	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &index) const;
	QModelIndex childByName(const QModelIndex &parent, const std::wstring &name) const;
	int columnCount(const QModelIndex &parent) const { return 1; }
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	bool hasChildren(const QModelIndex &parent) const;
	void refresh(const QModelIndex &index);
};

//=============================================================================
// ExportSceneTreeViewDelegate

class ExportSceneTreeViewDelegate : public QItemDelegate
{
	Q_OBJECT
	ExportSceneTreeView *m_treeView;

public:
	ExportSceneTreeViewDelegate(ExportSceneTreeView *parent);
	~ExportSceneTreeViewDelegate();
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

//=============================================================================
// ExportSceneTreeView

class ExportSceneTreeView : public QTreeView
{
	Q_OBJECT
	ExportSceneDvDirModel *m_model;

public:
	ExportSceneTreeView(QWidget *parent);
	QSize sizeHint() const;
	DvDirModelNode *getCurrentNode() const;

protected:
	void refresh();
	void showEvent(QShowEvent *);
	void focusInEvent(QFocusEvent *event);

public slots:
	void resizeToConts();

signals:
	void focusIn();
};

//=============================================================================
// ExportScenePopup

class ExportScenePopup : public DVGui::Dialog
{
	Q_OBJECT

	std::vector<TFilePath> m_scenes;
	//  QLabel* m_command;
	QLabel *m_newProjectNameLabel;
	DVGui::LineEdit *m_newProjectName;
	ExportSceneTreeView *m_projectTreeView;
	QRadioButton *m_newProjectButton;
	QRadioButton *m_chooseProjectButton;

	bool m_createNewProject;

public:
	ExportScenePopup(std::vector<TFilePath> scenes);

	void setScenes(std::vector<TFilePath> scenes)
	{
		m_scenes = scenes;
		//    updateCommandLabel();
	}

protected slots:
	void switchMode(int id);
	void onProjectTreeViweFocusIn();
	void onProjectNameFocusIn();
	void onExport();

protected:
	//!Create new project and return new project path.
	TFilePath createNewProject();
	//  void updateCommandLabel();
};

#endif // EXPORTSCENEPOPUP_H
