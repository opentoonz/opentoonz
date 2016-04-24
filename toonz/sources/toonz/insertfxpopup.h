

#ifndef INSERTFXPOPUP_H
#define INSERTFXPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include "tstream.h"

// forward declaration
class QTreeWidget;
class QTreeWidgetItem;
class TFx;

#include <QIcon>

//=============================================================================
// InsertFxPopup
//-----------------------------------------------------------------------------

class InsertFxPopup : public DVGui::Dialog
{
	Q_OBJECT

	QTreeWidget *m_fxTree;

	TIStream *m_is;
	TFilePath m_presetFolder;

	QIcon m_folderIcon;
	QIcon m_presetIcon;
	QIcon m_fxIcon;

public:
	InsertFxPopup();

private:
	TFx *createFx();

	void makeItem(QTreeWidgetItem *parent, std::string fxid);

	void loadFolder(QTreeWidgetItem *parent);
	/*!Return true if preset is loaded.*/
	bool loadPreset(QTreeWidgetItem *item);

	bool loadFx(TFilePath fp);
	void loadMacro();

public slots:
	void onItemDoubleClicked(QTreeWidgetItem *w, int c);
	void onInsert();
	void onReplace();
	void onAdd();

protected:
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);
	void contextMenuEvent(QContextMenuEvent *);

protected slots:
	void updatePresets();
	void removePreset();
};

#endif // INSERTFXPOPUP_H
