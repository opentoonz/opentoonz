

#ifndef PALETTECMD_INCLUDED
#define PALETTECMD_INCLUDED

#include <set>
#include <vector>
#include "tpalette.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TPaletteHandle;
class TXsheet;
class TXshSimpleLevel;
class ToonzScene;

/*!Find all level with palette \b palette. Set \b row and \b column with first
	 cell which contain a simil level.*/
DVAPI void findPaletteLevels(std::set<TXshSimpleLevel *> &levels,
							 int &rowIndex, int &columnIndex,
							 TPalette *palette, TXsheet *xsheet);

DVAPI bool isStyleUsed(const TImageP image, int styleId);
DVAPI bool areStylesUsed(const std::set<TXshSimpleLevel *> levels, const std::vector<int> styleIds);

namespace PaletteCmd
{

DVAPI void arrangeStyles(TPaletteHandle *paletteHandle,
						 int dstPageIndex, int dstIndexInPage,
						 int srcPageIndex, const std::set<int> &srcIndicesInPage);

DVAPI void createStyle(TPaletteHandle *paletteHandle, TPalette::Page *page);

DVAPI void addStyles(TPaletteHandle *paletteHandle, int pageIndex,
					 int indexInPage, const std::vector<TColorStyle *> &styles);

/*!
  \brief    Erases <I>image entities</I> whose style is any of the specified ones.
  \details  This function applies to <I>all objects contained in the images of a level</I>;
            for example, all strokes and regions of the <TT>TVectorImage</TT>s in a level.

  \remark   This command does not provide an undo for level types whose images are raster-based
            (specifically, fullcolor and TLV images), due to memory occupation concerns.
*/

DVAPI void eraseStyles(const std::set<TXshSimpleLevel *> &levels, const std::vector<int> &styleIds);

// se name == L"" viene generato un nome univoco ('page N')
DVAPI void addPage(TPaletteHandle *paletteHandle, std::wstring name = L"", bool withUndo = true);

DVAPI void destroyPage(TPaletteHandle *paletteHandle, int pageIndex);

DVAPI int loadReferenceImage(TPaletteHandle *paletteHandle, bool replace,
							 const TFilePath &_fp, int &frame, ToonzScene *scene,
							 const std::vector<int> &frames = std::vector<int>());

DVAPI void removeReferenceImage(TPaletteHandle *paletteHandle);

DVAPI void movePalettePage(TPaletteHandle *paletteHandle, int srcIndex, int dstIndex);

DVAPI void renamePalettePage(TPaletteHandle *paletteHandle, int pageIndex, const std::wstring &newName);

DVAPI void renamePaletteStyle(TPaletteHandle *paletteHandle, const std::wstring &newName);

} // namespace

#endif
