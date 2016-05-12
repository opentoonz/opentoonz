

#include "palettecmd.h"
#include "tpalette.h"
#include "tcolorstyles.h"
#include "tundo.h"
#include "tapp.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonzqt/tselectionhandle.h"
#include "styleselection.h"
#include "tconvert.h"
#include "tcolorutils.h"
#include "toonzutil.h"

#include "tlevel_io.h"
#include "trasterimage.h"

#include "toonz/txshlevel.h"
#include "toonz/toonzscene.h"
#include "toonz/toonzimageutils.h"

using namespace DVGui;

//===================================================================
//
// arrangeStyles
// srcPage : {a0,a1,...an } ==> dstPage : {b,b+1,...b+n-1}
//
//-------------------------------------------------------------------

/*! \namespace PaletteCmd
		\brief Provides a collection of methods to manage \b TPalette.
*/

namespace
{

class ArrangeStylesUndo : public TUndo
{
	TPaletteP m_palette;
	int m_dstPageIndex;
	int m_dstIndexInPage;
	int m_srcPageIndex;
	std::set<int> m_srcIndicesInPage;

  public:
	ArrangeStylesUndo(TPalette *palette, int dstPageIndex, int dstIndexInPage, int srcPageIndex,
					  const std::set<int> &srcIndicesInPage)
		: m_palette(palette), m_dstPageIndex(dstPageIndex), m_dstIndexInPage(dstIndexInPage),
		  m_srcPageIndex(srcPageIndex), m_srcIndicesInPage(srcIndicesInPage)
	{
		assert(m_palette);
		assert(0 <= dstPageIndex && dstPageIndex < m_palette->getPageCount());
		assert(0 <= srcPageIndex && srcPageIndex < m_palette->getPageCount());
		TPalette::Page *dstPage = m_palette->getPage(dstPageIndex);
		assert(dstPage);
		assert(0 <= dstIndexInPage && dstIndexInPage <= dstPage->getStyleCount());
		assert(!srcIndicesInPage.empty());
		TPalette::Page *srcPage = m_palette->getPage(srcPageIndex);
		assert(srcPage);
		assert(0 <= *srcIndicesInPage.begin() &&
			   *srcIndicesInPage.rbegin() < srcPage->getStyleCount());
	}
	void undo() const
	{
		TPalette::Page *srcPage = m_palette->getPage(m_srcPageIndex);
		assert(srcPage);
		TPalette::Page *dstPage = m_palette->getPage(m_dstPageIndex);
		assert(dstPage);
		std::vector<int> styles;
		int count = m_srcIndicesInPage.size();
		int h = m_dstIndexInPage;
		std::set<int>::const_iterator i;
		for (i = m_srcIndicesInPage.begin(); i != m_srcIndicesInPage.end(); ++i) {
			if (*i < h)
				h--;
			else
				break;
		}
		assert(h + count <= dstPage->getStyleCount());
		int k;
		for (k = 0; k < count; k++) {
			styles.push_back(dstPage->getStyleId(h));
			dstPage->removeStyle(h);
		}
		k = 0;
		for (i = m_srcIndicesInPage.begin(); i != m_srcIndicesInPage.end(); ++i, ++k)
			srcPage->insertStyle(*i, styles[k]);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	void redo() const
	{
		TPalette::Page *srcPage = m_palette->getPage(m_srcPageIndex);
		assert(srcPage);
		TPalette::Page *dstPage = m_palette->getPage(m_dstPageIndex);
		assert(dstPage);

		std::vector<int> styles;
		std::set<int>::const_reverse_iterator i;
		std::vector<int>::reverse_iterator j;
		int k = m_dstIndexInPage;
		for (i = m_srcIndicesInPage.rbegin(); i != m_srcIndicesInPage.rend(); ++i) {
			int index = *i;
			if (m_dstPageIndex == m_srcPageIndex && index < k)
				k--;
			styles.push_back(srcPage->getStyleId(index));
			srcPage->removeStyle(index);
		}
		for (j = styles.rbegin(); j != styles.rend(); ++j)
			dstPage->insertStyle(k, *j);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();

		/*
	if(TStyleSelection *sl = dynamic_cast<TStyleSelection *>(TSelection::getCurrent()))
	  {
	   sl->selectNone();
	   for(int h = 0; h<(int)styles.size(); h++)
		 sl->select(m_palette, m_dstPageIndex, k+h, true);
	  }
	  */
	}
	int getSize() const { return sizeof *this + m_srcIndicesInPage.size() * sizeof(int); }
};

} // namespace

//-----------------------------------------------------------------------------

void PaletteCmd::arrangeStyles(TPalette *palette, int dstPageIndex, int dstIndexInPage,
							   int srcPageIndex, const std::set<int> &srcIndicesInPage)
{
	ArrangeStylesUndo *undo = new ArrangeStylesUndo(palette, dstPageIndex, dstIndexInPage,
													srcPageIndex, srcIndicesInPage);
	undo->redo();
	TUndoManager::manager()->add(undo);
}

//=============================================================================
// CreateStyle
//-----------------------------------------------------------------------------
namespace
{
//-----------------------------------------------------------------------------

class CreateStyleUndo : public TUndo
{
	TPaletteP m_palette;
	int m_pageIndex;
	int m_styleId;

  public:
	CreateStyleUndo(const TPaletteP &palette, int pageIndex, int styleId)
		: m_palette(palette), m_pageIndex(pageIndex), m_styleId(styleId)
	{
		assert(m_palette);
		assert(0 <= pageIndex && pageIndex < m_palette->getPageCount());
		assert(0 <= styleId && styleId < m_palette->getStyleCount());
	}
	void undo() const
	{
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		int indexInPage = page->search(m_styleId);
		assert(indexInPage >= 0);
		page->removeStyle(indexInPage);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	void redo() const
	{
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		assert(m_palette->getStylePage(m_styleId) == 0);
		page->addStyle(m_styleId);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	int getSize() const { return sizeof *this; }
};

} // namespace

//-----------------------------------------------------------------------------

void PaletteCmd::createStyle(TPalette *palette, TPalette::Page *page)
{
	int index = TApp::instance()->getCurrentPalette()->getStyleIndex();
	int newIndex;
	if (index == -1)
		newIndex = page->addStyle(TPixel32::Black);
	else {
		TColorStyle *style = palette->getStyle(index);
		newIndex = page->addStyle(style->getMainColor());
	}
	TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
	if (!palette->isCleanupPalette() && level)
		level->setPaletteDirtyFlag(true);
	else
		TApp::instance()->getCurrentScene()->setDirtyFlag(true);

	TStyleSelection *styleSelection =
		dynamic_cast<TStyleSelection *>(TApp::instance()->getCurrentSelection()->getSelection());
	if (!styleSelection)
		styleSelection = new TStyleSelection();
	styleSelection->select(palette, page->getIndex(), newIndex, true);
	TApp::instance()->getCurrentPalette()->setStyleIndex(page->getStyleId(newIndex));
	TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	TUndoManager::manager()->add(
		new CreateStyleUndo(palette, page->getIndex(), page->getStyleId(newIndex)));
}

//=============================================================================
// addPage
//-----------------------------------------------------------------------------

namespace
{

class AddPageUndo : public TUndo
{
	TPaletteP m_palette;
	int m_pageIndex;
	wstring m_pageName;
	std::vector<std::pair<TColorStyle *, int>> m_styles;

  public:
	// creare DOPO l'inserimento
	AddPageUndo(const TPaletteP &palette, int pageIndex, wstring pageName)
		: m_palette(palette), m_pageIndex(pageIndex), m_pageName(pageName)
	{
		assert(m_palette);
		assert(0 <= m_pageIndex && m_pageIndex < m_palette->getPageCount());
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		for (int i = 0; i < page->getStyleCount(); i++) {
			std::pair<TColorStyle *, int> p;
			p.first = page->getStyle(i)->clone();
			p.second = page->getStyleId(i);
			m_styles.push_back(p);
		}
	}
	~AddPageUndo()
	{
		for (int i = 0; i < (int)m_styles.size(); i++)
			delete m_styles[i].first;
	}
	void undo() const
	{
		m_palette->erasePage(m_pageIndex);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	void redo() const
	{
		TPalette::Page *page = m_palette->addPage(m_pageName);
		assert(page);
		assert(page->getIndex() == m_pageIndex);
		for (int i = 0; i < (int)m_styles.size(); i++) {
			TColorStyle *cs = m_styles[i].first->clone();
			int styleId = m_styles[i].second;
			assert(m_palette->getStylePage(styleId) == 0);
			m_palette->setStyle(styleId, cs);
			page->addStyle(styleId);
		};
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	int getSize() const { return sizeof *this + m_styles.size() * sizeof(TColorStyle); }
};

} // namespace

//-----------------------------------------------------------------------------

void PaletteCmd::addPage(TPalette *palette, wstring name)
{
	if (name == L"")
		name = L"page " + toWideString(palette->getPageCount() + 1);
	TPalette::Page *page = palette->addPage(name);
	TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
	if (!palette->isCleanupPalette() && level)
		level->setPaletteDirtyFlag(true);
	else
		TApp::instance()->getCurrentScene()->setDirtyFlag(true);
	TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	TUndoManager::manager()->add(new AddPageUndo(palette, page->getIndex(), name));
}

//=============================================================================
// destroyPage
//-----------------------------------------------------------------------------

namespace
{

class DestroyPageUndo : public TUndo
{
	TPaletteP m_palette;
	int m_pageIndex;
	wstring m_pageName;
	std::vector<int> m_styles;

  public:
	DestroyPageUndo(const TPaletteP &palette, int pageIndex)
		: m_palette(palette), m_pageIndex(pageIndex)
	{
		assert(m_palette);
		assert(0 <= pageIndex && pageIndex < m_palette->getPageCount());
		assert(m_palette->getPageCount() > 1);
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		m_pageName = page->getName();
		m_styles.resize(page->getStyleCount());
		for (int i = 0; i < page->getStyleCount(); i++)
			m_styles[i] = page->getStyleId(i);
	}
	void undo() const
	{
		TPalette::Page *page = m_palette->addPage(m_pageName);
		m_palette->movePage(page, m_pageIndex);
		for (int i = 0; i < (int)m_styles.size(); i++)
			page->addStyle(m_styles[i]);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	void redo() const
	{
		m_palette->erasePage(m_pageIndex);
		TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
	}
	int getSize() const { return sizeof *this; }
};

} // namespace

//-----------------------------------------------------------------------------

void PaletteCmd::destroyPage(TPalette *palette, int pageIndex)
{
	assert(0 <= pageIndex && pageIndex < palette->getPageCount());
	TUndoManager::manager()->add(new DestroyPageUndo(palette, pageIndex));
	palette->erasePage(pageIndex);
}

//=============================================================================
// addStyles
//-----------------------------------------------------------------------------

namespace
{

//=============================================================================
// AddStylesUndo
//-----------------------------------------------------------------------------

class AddStylesUndo : public TUndo
{
	TPaletteP m_palette;
	int m_pageIndex;
	int m_indexInPage;
	std::vector<std::pair<TColorStyle *, int>> m_styles;

  public:
	// creare DOPO l'inserimento
	AddStylesUndo(const TPaletteP &palette, int pageIndex, int indexInPage, int count)
		: m_palette(palette), m_pageIndex(pageIndex), m_indexInPage(indexInPage)
	{
		assert(m_palette);
		assert(0 <= m_pageIndex && m_pageIndex < m_palette->getPageCount());
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		assert(0 <= indexInPage && indexInPage + count <= page->getStyleCount());
		for (int i = 0; i < count; i++) {
			std::pair<TColorStyle *, int> p;
			p.second = page->getStyleId(m_indexInPage + i);
			p.first = m_palette->getStyle(p.second)->clone();
			m_styles.push_back(p);
		}
	}
	~AddStylesUndo()
	{
		for (int i = 0; i < (int)m_styles.size(); i++)
			delete m_styles[i].first;
	}
	void undo() const
	{
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		int count = m_styles.size();
		for (int i = 0; i < count; i++)
			page->removeStyle(m_indexInPage + i);
	}
	void redo() const
	{
		TPalette::Page *page = m_palette->getPage(m_pageIndex);
		assert(page);
		for (int i = 0; i < (int)m_styles.size(); i++) {
			TColorStyle *cs = m_styles[i].first->clone();
			int styleId = m_styles[i].second;
			assert(m_palette->getStylePage(styleId) == 0);
			m_palette->setStyle(styleId, cs);
			page->insertStyle(m_indexInPage + i, styleId);
		}
	}
	int getSize() const { return sizeof *this + m_styles.size() * sizeof(TColorStyle); }
};

} // namespace

//-----------------------------------------------------------------------------

void PaletteCmd::addStyles(const TPaletteP &palette, int pageIndex, int indexInPage,
						   const std::vector<TColorStyle *> &styles)
{
	assert(0 <= pageIndex && pageIndex < palette->getPageCount());
	TPalette::Page *page = palette->getPage(pageIndex);
	assert(page);
	assert(0 <= indexInPage && indexInPage <= page->getStyleCount());
	int count = styles.size();
	for (int i = 0; i < count; i++)
		page->insertStyle(indexInPage + i, styles[i]->clone());
	TUndoManager::manager()->add(new AddStylesUndo(palette, pageIndex, indexInPage, count));
}

//===================================================================
// ReferenceImage
//-------------------------------------------------------------------

namespace
{

//===================================================================
// SetReferenceImageUndo
//-------------------------------------------------------------------

class SetReferenceImageUndo : public TUndo
{
	TPaletteP m_palette;
	TPaletteP m_oldPalette, m_newPalette;

  public:
	SetReferenceImageUndo(TPaletteP palette) : m_palette(palette), m_oldPalette(palette->clone()) {}
	void onAdd() { m_newPalette = m_palette->clone(); }
	void undo() const { m_palette->assign(m_oldPalette.getPointer()); }
	void redo() const { m_palette->assign(m_newPalette.getPointer()); }
	int getSize() const { return sizeof(*this) + sizeof(TPalette) * 2; }
};

//===================================================================
// loadRefImage
//-------------------------------------------------------------------

int loadRefImage(TPaletteP levelPalette, const TFilePath &_fp, int &index)
{
	bool paletteAlreadyLoaded = false;
	TFilePath &fp = (TFilePath &)_fp;
	if (_fp == TFilePath()) {
		paletteAlreadyLoaded = true;
		fp = levelPalette->getRefImgPath();
	}

	TImageP img;
	try {
		TLevelReaderP lr(fp);
		if (fp != TFilePath() && lr) {
			TLevelP level = lr->loadInfo();
			if (level && level->getFrameCount() > 0) {
				TLevel::Iterator it;
				if (!paletteAlreadyLoaded) {
					std::vector<TFrameId> fids;
					for (it = level->begin(); it != level->end(); ++it) {
						if (it->first == -1 || it->first == -2) {
							assert(level->getFrameCount() == 1);
							fids.push_back(0);
							break;
						}
						fids.push_back(it->first);
					}
					levelPalette->setRefLevelFids(fids);
				}
				if (index < 0)
					index = 0;
				else if (index >= level->getFrameCount())
					index = level->getFrameCount() - 1;
				it = level->begin();
				advance(it, index);
				img = lr->getFrameReader(it->first)->load();
				if (img && img->getPalette() == 0) {
					if (paletteAlreadyLoaded || level->getPalette() != 0)
						img->setPalette(level->getPalette());
					else if ((fp.getType() == "tzp" || fp.getType() == "tzu"))
						img->setPalette(
							ToonzImageUtils::loadTzPalette(fp.withType("plt").withNoFrame()));
				}
			}
		} else
			img = levelPalette->getRefImg();
	} catch (...) {
	}
	if (!img)
		return 1;

	if (paletteAlreadyLoaded) {
		img->setPalette(0);
		levelPalette->setRefImg(img);
		levelPalette->setRefImgPath(fp);
		return 0;
	}

	TUndo *undo = new SetReferenceImageUndo(levelPalette);

	QString question;
	question = QObject::tr("Replace palette?");
	int ret = MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
	if (ret == 1) {
		TPaletteP imagePalette;
		if (TRasterImageP ri = img) {
			TRaster32P raster = ri->getRaster();
			if (raster) {
				std::set<TPixel32> colors;
				int colorCount = 256;
				TColorUtils::buildPalette(colors, raster, colorCount);
				colors.erase(TPixel::Black); // il nero viene messo dal costruttore della TPalette
				imagePalette = new TPalette();
				std::set<TPixel32>::const_iterator it = colors.begin();
				for (; it != colors.end(); ++it)
					imagePalette->getPage(0)->addStyle(*it);
			}
		} else
			imagePalette = img->getPalette();

		if (imagePalette) {
			// voglio evitare di sostituire una palette con pochi colori ad una con
			// tanti colori
			while (imagePalette->getStyleCount() < levelPalette->getStyleCount()) {
				int index = imagePalette->getStyleCount();
				assert(index < levelPalette->getStyleCount());
				TColorStyle *style = levelPalette->getStyle(index)->clone();
				imagePalette->addStyle(style);
			}
			levelPalette->assign(imagePalette.getPointer());
		}
	}

	img->setPalette(0);

	levelPalette->setRefImg(img);
	levelPalette->setRefImgPath(fp);

	// DAFARE ovviamente non e' la notifica corretta,
	//			 ma senza non aggiorna il palette viewer della studio palette e crash!
	TApp *app = TApp::instance();
	app->getCurrentPalette()->notifyPaletteChanged();

	TUndoManager::manager()->add(undo);
	return 0;
}
//-------------------------------------------------------------------
} // namespace
//-------------------------------------------------------------------

//===================================================================
// loadReferenceImage
//-------------------------------------------------------------------

int PaletteCmd::loadReferenceImage(const TFilePath &_fp, int &index)
{
	TPaletteP levelPalette = TApp::instance()
								 ->getCurrentPalette()
								 ->getPalette(); // ColorController::instance()->getLevelPalette();
	if (!levelPalette)
		return 2;

	int ret = loadRefImage(levelPalette, _fp, index);
	if (ret != 0)
		return ret;

	TApp *app = TApp::instance();
	app->getCurrentPalette()->notifyPaletteChanged();
	if (app->getCurrentLevel()->getLevel())
		app->getCurrentLevel()->getLevel()->setDirtyFlag(true);

	return 0;
}

//===================================================================
// loadReferenceImageInStdPlt
//-------------------------------------------------------------------

// DA FARE: togli una volta sistemata la studiopalette corrente.
#include "studiopaletteviewer.h"
#include "toonz/studiopalette.h"
int PaletteCmd::loadReferenceImageInStdPlt(const TFilePath &_fp, int &index)
{
	// DAFARE
	TPaletteP levelPalette = DAFARE::
		getCurrentStudioPalette(); // TApp::instance()->getCurrentPalette()->getPalette();//ColorController::instance()->getLevelPalette();
	if (!levelPalette)
		return 2;

	int ret = loadRefImage(levelPalette, _fp, index);
	if (ret != 0)
		return ret;

	wstring name = levelPalette->getGlobalName();
	StudioPalette::instance()->save(StudioPalette::instance()->getPalettePath(name),
									levelPalette.getPointer());

	return 0;
}

//===================================================================
// removeReferenceImage
//-------------------------------------------------------------------

void PaletteCmd::removeReferenceImage()
{
	TPaletteP levelPalette = TApp::instance()
								 ->getCurrentPalette()
								 ->getPalette(); // ColorController::instance()->getLevelPalette();
	if (!levelPalette)
		return;
	TUndo *undo = new SetReferenceImageUndo(levelPalette);

	levelPalette->setRefImg(TImageP());
	levelPalette->setRefImgPath(TFilePath());

	TApp *app = TApp::instance();
	app->getCurrentPalette()->notifyPaletteChanged();
	if (app->getCurrentLevel()->getLevel())
		app->getCurrentLevel()->getLevel()->setDirtyFlag(true);

	TUndoManager::manager()->add(undo);
}
