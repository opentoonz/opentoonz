#pragma once

#ifndef CASTSELECTION_H
#define CASTSELECTION_H

#include "dvitemview.h"
#include <QMimeData>

class CastBrowser;
class TXshPaletteLevel;
class TXshSoundLevel;
class TXshLevel;

//=============================================================================
//
// CastSelection
//
//-----------------------------------------------------------------------------

class CastSelection : public DvItemSelection
{
	CastBrowser *m_browser;

public:
	CastSelection();
	~CastSelection();

	void setBrowser(CastBrowser *browser) { m_browser = browser; }

	void getSelectedLevels(std::vector<TXshLevel *> &levels);

	// commands
	void enableCommands();
};

//=============================================================================
//
// CastItems
//
//-----------------------------------------------------------------------------
// TODO: spostare in un altro file

class TXshLevel;
class TXshSimpleLevel;

class CastItem
{
public:
	CastItem() {}
	virtual ~CastItem() {}
	virtual QString getName() const = 0;
	virtual QString getToolTip() const = 0;
	virtual int getFrameCount() const { return 0; }
	virtual QPixmap getPixmap(bool isSelected) const = 0;
	virtual TXshSimpleLevel *getSimpleLevel() const { return 0; }
	virtual TXshSoundLevel *getSoundLevel() const { return 0; }
	virtual TXshPaletteLevel *getPaletteLevel() const { return 0; }
	virtual CastItem *clone() const = 0;
	virtual bool exists() const = 0;
};
//-----------------------------------------------------------------------------

class LevelCastItem : public CastItem
{
	TXshLevel *m_level;
	QSize m_itemPixmapSize;

public:
	LevelCastItem(TXshLevel *level, QSize itemPixmapSize)
		: m_level(level), m_itemPixmapSize(itemPixmapSize) {}
	TXshLevel *getLevel() const { return m_level; }
	QString getName() const;
	QString getToolTip() const;
	int getFrameCount() const;
	QPixmap getPixmap(bool isSelected = false) const;
	TXshSimpleLevel *getSimpleLevel() const;
	CastItem *clone() const { return new LevelCastItem(m_level, m_itemPixmapSize); }
	bool exists() const;
};
//-----------------------------------------------------------------------------

class SoundCastItem : public CastItem
{
	TXshSoundLevel *m_soundLevel;
	QSize m_itemPixmapSize;

public:
	SoundCastItem(TXshSoundLevel *soundLevel, QSize itemPixmapSize)
		: m_soundLevel(soundLevel), m_itemPixmapSize(itemPixmapSize) {}
	TXshSoundLevel *getSoundLevel() const { return m_soundLevel; }
	QString getName() const;
	QString getToolTip() const;
	int getFrameCount() const;
	QPixmap getPixmap(bool isSelected = false) const;
	CastItem *clone() const { return new SoundCastItem(m_soundLevel, m_itemPixmapSize); }
	bool exists() const;
};

//-----------------------------------------------------------------------------

class PaletteCastItem : public CastItem
{
	TXshPaletteLevel *m_paletteLevel;
	QSize m_itemPixmapSize;

public:
	PaletteCastItem(TXshPaletteLevel *paletteLevel, QSize itemPixmapSize)
		: m_paletteLevel(paletteLevel), m_itemPixmapSize(itemPixmapSize) {}
	TXshPaletteLevel *getPaletteLevel() const { return m_paletteLevel; }
	QString getName() const;
	QString getToolTip() const;
	int getFrameCount() const;
	QPixmap getPixmap(bool isSelected = false) const;
	CastItem *clone() const { return new PaletteCastItem(m_paletteLevel, m_itemPixmapSize); }
	bool exists() const;
};
//-----------------------------------------------------------------------------

class CastItems : public QMimeData
{
	std::vector<CastItem *> m_items;

public:
	CastItems();
	~CastItems();

	void clear();
	void addItem(CastItem *item);

	int getItemCount() const { return m_items.size(); }
	CastItem *getItem(int index) const;

	void swapItem(int index1, int index2)
	{
		std::swap(m_items[index1], m_items[index2]);
	}

	QStringList formats() const;
	bool hasFormat(const QString &mimeType) const;
	static QString getMimeFormat();

	CastItems *getSelectedItems(const std::set<int> &indices) const;
};

#endif //FILESELECTION_H
