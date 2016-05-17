#pragma once

#ifndef DVDIRTREEVIEW_H
#define DVDIRTREEVIEW_H

// Tnz6 includes
#include "dvitemview.h"
#include "versioncontrol.h"
#include "filebrowsermodel.h"

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/lineedit.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QTreeView>
#include <QItemDelegate>
#include <QWidget>

//==============================================================

//    Forward declarations

class DvDirModel;
class DvDirModelNode;
class DvDirVersionControlNode;
class DvDirTreeView;

//==============================================================

//**********************************************************************************
//    DvDirTreeView  declaration
//**********************************************************************************

class DvDirTreeView : public QTreeView, public TSelection
{
	Q_OBJECT

	QColor m_textColor;				  //text color (black)
	QColor m_selectedTextColor;		  //selected item text color (white)
	QColor m_folderTextColor;		  //folder item text color (blue)
	QColor m_selectedFolderTextColor; //selected folder item text color (yellow)
	QColor m_selectedItemBackground;  //selected item background color (0,0,128)

	Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)
	Q_PROPERTY(QColor SelectedTextColor READ getSelectedTextColor WRITE setSelectedTextColor)
	Q_PROPERTY(QColor FolderTextColor READ getFolderTextColor WRITE setFolderTextColor)
	Q_PROPERTY(QColor SelectedFolderTextColor READ getSelectedFolderTextColor WRITE setSelectedFolderTextColor)
	Q_PROPERTY(QColor SelectedItemBackground READ getSelectedItemBackground WRITE setSelectedItemBackground)

public:
	DvDirTreeView(QWidget *parent = 0);

	TFilePath getCurrentPath() const; //!< Returns the path of currently selected file or folder,
									  //!  or an empty one in case it couldn't be extracted.
	DvDirModelNode *getCurrentNode() const;
	DvDirModelNode *getCurrentDropNode() const
	{
		return m_currentDropItem;
	}

	// From TSelection
	bool isEmpty() const { return false; }
	void selectNone() {}
	void enableCommands();

	void enableGlobalSelection(bool enabled)
	{
		m_globalSelectionEnabled = enabled;
	}

	void refreshVersionControl(DvDirVersionControlNode *node,
							   const QStringList &files = QStringList());

	void setRefreshVersionControlEnabled(bool value)
	{
		m_refreshVersionControlEnabled = value;
	}
	bool refreshVersionControlEnabled() const
	{
		return m_refreshVersionControlEnabled;
	}

	DvItemListModel::Status getItemVersionControlStatus(
		DvDirVersionControlNode *node,
		const TFilePath &fp);

	void setTextColor(const QColor &color) { m_textColor = color; }
	QColor getTextColor() const { return m_textColor; }
	void setSelectedTextColor(const QColor &color) { m_selectedTextColor = color; }
	QColor getSelectedTextColor() const { return m_selectedTextColor; }
	void setFolderTextColor(const QColor &color) { m_folderTextColor = color; }
	QColor getFolderTextColor() const { return m_folderTextColor; }
	void setSelectedFolderTextColor(const QColor &color) { m_selectedFolderTextColor = color; }
	QColor getSelectedFolderTextColor() const { return m_selectedFolderTextColor; }
	void setSelectedItemBackground(const QColor &color) { m_selectedItemBackground = color; }
	QColor getSelectedItemBackground() const { return m_selectedItemBackground; }

signals:

	void currentNodeChanged(); //!< Emitted when user selectes a different node.

public slots:

	void deleteFolder(); //!< Deletes the selected folder.

	void setCurrentNode(DvDirModelNode *node); //!< Sets the current node, make it visible.
	void setCurrentNode(const TFilePath &fp,
						bool expandNode = false);
	void resizeToConts(void);

	void cleanupVersionControl(DvDirVersionControlNode *node);
	void purgeVersionControl(DvDirVersionControlNode *node);
	void deleteVersionControl(DvDirVersionControlNode *node);
	void putVersionControl(DvDirVersionControlNode *node);
	void updateVersionControl(DvDirVersionControlNode *node);

	void listVersionControl(DvDirVersionControlNode *lastExistingNode,
							const QString &relativePath);

	// Only for tnz "special folder" and project folder (working in this case on project settings xml file)
	void editCurrentVersionControlNode();
	void unlockCurrentVersionControlNode();
	void revertCurrentVersionControlNode();

	void updateCurrentVersionControlNode();
	void putCurrentVersionControlNode();
	void deleteCurrentVersionControlNode();
	void refreshCurrentVersionControlNode();
	void cleanupCurrentVersionControlNode();
	void purgeCurrentVersionControlNode();

	void onInfoDone(const QString &);
	void onListDone(const QString &);

	void onCheckPartialLockError(const QString &);
	void onCheckPartialLockDone(const QString &);

	void onCheckOutError(const QString &);
	void onCheckOutDone(const QString &);

	void onRefreshStatusDone(const QString &);
	void onRefreshStatusError(const QString &);

protected:
	QSize sizeHint() const;

	void currentChanged(const QModelIndex &current, const QModelIndex &previous);
	bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *ev);
	void resizeEvent(QResizeEvent *);

	void dragEnterEvent(QDragEnterEvent *event);
	void dragLeaveEvent(QDragLeaveEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);

	void contextMenuEvent(QContextMenuEvent *event);

	void createMenuAction(QMenu &menu, QString name, const char *slot, bool enable = true);

	void checkPartialLock(const QString &workingDir, const QStringList &files);

private:
	bool m_globalSelectionEnabled;
	DvDirModelNode *m_currentDropItem;
	VersionControlThread m_thread;

	bool m_refreshVersionControlEnabled;

	// Temporary variable used while retrieving list of missing directories
	QString m_getSVNListRelativePath;

	// Using for version control node refreshing
	DvDirVersionControlNode *m_currentRefreshedNode;
};

//**********************************************************************************
//    DvDirTreeViewDelegate  declaration
//**********************************************************************************

class DvDirTreeViewDelegate : public QItemDelegate
{
	Q_OBJECT

public:
	DvDirTreeViewDelegate(DvDirTreeView *parent);
	~DvDirTreeViewDelegate();

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;

	bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

	void setEditorData(QWidget *editor, const QModelIndex &index) const;
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
	DvDirTreeView *m_treeView;

private slots:

	void commitAndCloseEditor();
};

//**********************************************************************************
//    NodeEditor  declaration
//**********************************************************************************

class NodeEditor : public QWidget
{
	Q_OBJECT

public:
	NodeEditor(QWidget *parent = 0, QRect rect = QRect(), int leftMargin = 0);

	void setText(QString value) { m_lineEdit->setText(value); }
	QString getText() const { return m_lineEdit->text(); }

signals:

	void editingFinished();

protected:
	void focusInEvent(QFocusEvent *event);

protected slots:

	void emitFinished();

private:
	DVGui::LineEdit *m_lineEdit;
};

#endif // DVDIRTREEVIEW_H
