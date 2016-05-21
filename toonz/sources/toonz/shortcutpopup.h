#pragma once

#ifndef SHORTCUTPOPUP_H
#define SHORTCUTPOPUP_H

#include <QDialog>
#include <QTreeWidget>

#include "toonzqt/dvdialog.h"

// forward declaration
class QPushButton;
class ShortcutItem;

//=============================================================================
// ShortcutViewer
// --------------
// E' l'editor dello shortcut associato all'azione corrente
// Visualizza lo shortcut e permette di cambiarlo digitando direttamente
// la nuova sequenza di tasti
// Per cancellarlo bisogna chiamare removeShortcut()
//-----------------------------------------------------------------------------

class ShortcutViewer : public QWidget
{
	Q_OBJECT
	QAction *m_action;

public:
	ShortcutViewer(QWidget *parent);
	~ShortcutViewer();

protected:
	void paintEvent(QPaintEvent *);
	bool event(QEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);

public slots:
	void setAction(QAction *action);
	void removeShortcut();

signals:
	void shortcutChanged();
};

//=============================================================================
// ShortcutTree
// ------------
// Visualizza tutti le QAction (con gli eventuali shortcut assegnati)
// Serve per selezionare la QAction corrente
//-----------------------------------------------------------------------------

class ShortcutTree : public QTreeWidget
{
	Q_OBJECT
	std::vector<ShortcutItem *> m_items;

public:
	ShortcutTree(QWidget *parent = 0);
	~ShortcutTree();

protected:
	// aggiunge un blocco di QAction. commandType e' un CommandType::MenubarCommandType
	void addFolder(const QString &title, int commandType, QTreeWidgetItem *folder = 0);

public slots:
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onShortcutChanged();

signals:
	void actionSelected(QAction *);
};

//=============================================================================
// ShortcutPopup
// -------------
// Questo e' il popup che l'utente utilizza per modificare gli shortcut
//-----------------------------------------------------------------------------

class ShortcutPopup : public DVGui::Dialog
{
	Q_OBJECT
	QPushButton *m_removeBtn;
	ShortcutViewer *m_sViewer;
	ShortcutTree *m_list;

public:
	ShortcutPopup();
	~ShortcutPopup();
};

#endif //  SHORTCUTPOPUP_H
