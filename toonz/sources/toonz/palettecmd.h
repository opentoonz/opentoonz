#pragma once

#ifndef PALETTECMD_INCLUDED
#define PALETTECMD_INCLUDED

#include <set>
#include "tpalette.h"

namespace PaletteCmd
{

void arrangeStyles(
	TPalette *palette,
	int dstPageIndex, int dstIndexInPage,
	int srcPageIndex, const std::set<int> &srcIndicesInPage);

void createStyle(
	TPalette *palette,
	TPalette::Page *page);

// se name == L"" viene generato un nome univoco ('page N')
void addPage(
	TPalette *palette,
	wstring name = L"");

void destroyPage(
	TPalette *palette,
	int pageIndex);

void addStyles(
	const TPaletteP &palette,
	int pageIndex,
	int indexInPage,
	const std::vector<TColorStyle *> &styles);

int loadReferenceImage(
	const TFilePath &_fp,
	int &index);

int loadReferenceImageInStdPlt(
	const TFilePath &_fp,
	int &index);

void removeReferenceImage();

} // namespace

#endif
