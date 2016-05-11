

// character_manager.cpp: implementation of the TFont class.
//
//////////////////////////////////////////////////////////////////////

#include "tpixelgr.h"
#include "tfont.h"
#include "tstroke.h"
//#include "tcurves.h"
#include "traster.h"
#include <vector>
#include <iostream>
#include <string>
//#include <tstring.h>
#include <tmathutil.h>
//#include <tdebugmessage.h>
#include "tvectorimage.h"
using namespace std;

//=============================================================================

typedef map<wstring, LOGFONTW> WindowsFontTable;
typedef map<pair<unsigned short, unsigned short>, int> KerningPairs;

//=============================================================================

struct TFont::Impl {

	bool m_hasKerning;
	bool m_hasVertical;
	HFONT m_font;
	HDC m_hdc;
	TEXTMETRICW m_metrics;
	KerningPairs m_kerningPairs;

	Impl(const LOGFONTW &font, HDC hdc);
	~Impl();
};

//-----------------------------------------------------------------------------

TFont::TFont(const LOGFONTW &font, HDC hdc)
{
	m_pimpl = new Impl(font, hdc);
}

//-----------------------------------------------------------------------------

TFont::~TFont()
{
	delete m_pimpl;
}

//-----------------------------------------------------------------------------

TFont::Impl::Impl(const LOGFONTW &logfont, HDC hdc)
	: m_hdc(hdc)
{
	m_font = CreateFontIndirectW(&logfont);

	if (!m_font)
		throw TFontCreationError();

	HGDIOBJ hObj = SelectObject(hdc, m_font);
	if (!hObj || hObj == HGDI_ERROR)
		throw TFontCreationError();

	if (!GetTextMetricsW(hdc, &m_metrics))
		throw TFontCreationError();

	DWORD pairsCount = GetKerningPairsW(hdc, 0, 0);
	if (pairsCount) {
		m_hasKerning = true;
		std::unique_ptr<KERNINGPAIR[]> tempKernPairs(new KERNINGPAIR[pairsCount]);
		GetKerningPairsW(hdc, pairsCount, tempKernPairs.get());
		for (UINT i = 0; i < pairsCount; i++) {
			pair<unsigned short, unsigned short> key = make_pair(tempKernPairs[i].wFirst, tempKernPairs[i].wSecond);
			m_kerningPairs[key] = tempKernPairs[i].iKernAmount;
		}
	} else
		m_hasKerning = false;

	m_hasVertical = (logfont.lfFaceName)[0] == '@';
}

//-----------------------------------------------------------------------------

TFont::Impl::~Impl()
{
	//delete m_advances;
	DeleteObject(m_font);
}

//-----------------------------------------------------------------------------

namespace
{
inline TThickPoint toThickPoint(POINTFX point)
{
	double app1 = point.x.value + ((double)point.x.fract) / (std::numeric_limits<WORD>::max)();
	double app2 = point.y.value + ((double)point.y.fract) / (std::numeric_limits<WORD>::max)();
	return TThickPoint(app1, app2, 0);
}
}

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TVectorImageP &image, wchar_t charcode, wchar_t nextCharCode) const

{

	GLYPHMETRICS gm;
	MAT2 mat2;
	mat2.eM11.fract = 0;
	mat2.eM12.fract = 0;
	mat2.eM21.fract = 0;
	mat2.eM22.fract = 0;
	mat2.eM11.value = 1;
	mat2.eM12.value = 0;
	mat2.eM21.value = 0;
	mat2.eM22.value = 1;

	vector<TThickPoint> points;

	UINT j = 0;

	DWORD charMemorySize = GetGlyphOutlineW(m_pimpl->m_hdc, charcode, GGO_NATIVE, &gm, 0, 0, &mat2);
	if (charMemorySize == GDI_ERROR) {
		assert(0);
		return TPoint();
	}

	std::unique_ptr<char[]> lpvBuffer(new char[charMemorySize]);

	charMemorySize = GetGlyphOutlineW(m_pimpl->m_hdc, charcode, GGO_NATIVE, &gm, charMemorySize, lpvBuffer.get(), &mat2);
	if (charMemorySize == GDI_ERROR) {
		assert(0);
		return TPoint();
	}

	TTPOLYGONHEADER *header = (TTPOLYGONHEADER *)lpvBuffer.get();

	while ((char *)header < (char *)lpvBuffer.get() + charMemorySize) {
		points.clear();
		TThickPoint startPoint = toThickPoint(header->pfxStart);
		points.push_back(startPoint);

		if (header->dwType != TT_POLYGON_TYPE) {
			assert(0);
		}
		int memorySize = header->cb;

		TTPOLYCURVE *curve = (TTPOLYCURVE *)(header + 1);

		while ((char *)curve < (char *)header + memorySize) {
			switch (curve->wType) {
			case TT_PRIM_LINE:

				for (j = 0; j < curve->cpfx; j++) {
					TThickPoint p0 = points.back();
					TThickPoint p1 = toThickPoint(((*curve).apfx[j]));
					points.push_back((p0 + p1) * 0.5);
					points.push_back(p1);
				}

				break;

			case TT_PRIM_QSPLINE:

				for (j = 0; (int)j + 2 < curve->cpfx; j++) {
					TThickPoint p1 = toThickPoint(((*curve).apfx[j]));
					TThickPoint p2 = toThickPoint(((*curve).apfx[j + 1]));

					points.push_back(p1);
					points.push_back((p1 + p2) * 0.5);
				}
				points.push_back(toThickPoint(((*curve).apfx[j++])));
				points.push_back(toThickPoint(((*curve).apfx[j++])));

				break;
			case TT_PRIM_CSPLINE:
				assert(0);
				break;
			default:
				assert(0);
			}

			curve = (TTPOLYCURVE *)(&(curve->apfx)[j]);
		}

		TThickPoint p0 = points.back();
		if (!isAlmostZero(p0.x - startPoint.x) || !isAlmostZero(p0.y - startPoint.y)) {
			points.push_back((p0 + startPoint) * 0.5);
			points.push_back(startPoint);
		}

		TStroke *stroke = new TStroke();
		stroke->reshape(&(points[0]), points.size());
		stroke->setSelfLoop(true);
		image->addStroke(stroke);

		header = (TTPOLYGONHEADER *)curve;
	}

	image->group(0, image->getStrokeCount());

	return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------

namespace
{

TPoint appDrawChar(TRasterGR8P &outImage, HDC hdc, wchar_t charcode)
{
	GLYPHMETRICS gm;
	MAT2 mat2;
	mat2.eM11.fract = 0;
	mat2.eM12.fract = 0;
	mat2.eM21.fract = 0;
	mat2.eM22.fract = 0;
	mat2.eM11.value = 1;
	mat2.eM12.value = 0;
	mat2.eM21.value = 0;
	mat2.eM22.value = 1;

	DWORD charMemorySize = GetGlyphOutlineW(hdc, charcode, GGO_GRAY8_BITMAP, &gm, 0, 0, &mat2);

	if (charMemorySize == GDI_ERROR) {
		assert(0);
	}

	int lx = gm.gmBlackBoxX;
	int ly = gm.gmBlackBoxY;

	int wrap = ((lx + 3) >> 2) << 2;

	TRasterGR8P appImage = TRasterGR8P(wrap, ly);
	appImage->clear();
	outImage = appImage->extract(0, 0, lx - 1, ly - 1);
	outImage->lock();
	GetGlyphOutlineW(hdc, charcode, GGO_GRAY8_BITMAP, &gm, wrap * ly, outImage->getRawData(), &mat2);
	outImage->unlock();
	TPoint glyphOrig;
	glyphOrig.x = gm.gmptGlyphOrigin.x;
	glyphOrig.y = gm.gmptGlyphOrigin.y;
	return glyphOrig;
}
}

//-----------------------------------------------------------------------------
//valori compresi tra 0 e 64 (si si, proprio 64 e non 63: sono 65 valori)

TPoint TFont::drawChar(TRasterGR8P &outImage, TPoint &glyphOrigin, wchar_t charcode, wchar_t nextCharCode) const
{
	TRasterGR8P grayAppImage;

	TPoint glyphOrig = appDrawChar(grayAppImage, m_pimpl->m_hdc, charcode);

	if (glyphOrig.x < 0) {
		glyphOrigin.x = glyphOrig.x;
		glyphOrig.x = 0;
	} else
		glyphOrigin.x = 0;

	if (glyphOrig.y < 0) {
		glyphOrigin.y = glyphOrig.y;
		glyphOrig.y = 0;
	} else
		glyphOrigin.y = 0;

	int srcLx = grayAppImage->getLx();
	int srcLy = grayAppImage->getLy();

	int dstLx = srcLx + glyphOrig.x;
	int dstLy = getMaxHeight();

	outImage = TRasterGR8P(dstLx, dstLy);
	outImage->clear();

	int ty = m_pimpl->m_metrics.tmDescent - 1 + glyphOrig.y;
	assert(ty < dstLy);
	assert(ty >= srcLy - 1);
	grayAppImage->lock();
	outImage->lock();

	for (int sy = 0; sy < srcLy; ++sy, --ty) {
		TPixelGR8 *srcPix = grayAppImage->pixels(sy);
		TPixelGR8 *tarPix = outImage->pixels(ty) + glyphOrig.x;
		for (int x = 0; x < srcLx; ++x) {
			assert(srcPix->value < 65);

			switch (srcPix->value) {
			case 0:
				tarPix->value = 0;
				break;
			default:
				tarPix->value = (srcPix->value << 2) - 1;
				break;
			}
			++srcPix;
			++tarPix;
		}
	}
	grayAppImage->unlock();
	outImage->unlock();

	return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TRasterCM32P &outImage, TPoint &glyphOrigin, int inkId, wchar_t charcode, wchar_t nextCharCode) const
{
	TRasterGR8P grayAppImage;
	TPoint glyphOrig = appDrawChar(grayAppImage, m_pimpl->m_hdc, charcode);

	if (glyphOrig.x < 0) {
		glyphOrigin.x = glyphOrig.x;
		glyphOrig.x = 0;
	} else
		glyphOrigin.x = 0;

	if (glyphOrig.y < 0) {
		glyphOrigin.y = glyphOrig.y;
		glyphOrig.y = 0;
	} else
		glyphOrigin.y = 0;

	int srcLx = grayAppImage->getLx();
	int srcLy = grayAppImage->getLy();

	int dstLx = srcLx + glyphOrig.x;
	int dstLy = getMaxHeight();

	outImage = TRasterCM32P(dstLx, dstLy);
	outImage->clear();

	assert(TPixelCM32::getMaxTone() == 255);
	// TPixelCM32 bgColor(BackgroundStyle,BackgroundStyle,TPixelCM32::getMaxTone());
	TPixelCM32 bgColor;

	int ty = m_pimpl->m_metrics.tmDescent - 1 + glyphOrig.y;
	assert(ty < dstLy);
	assert(ty >= srcLy - 1);
	grayAppImage->lock();
	outImage->lock();

	for (int sy = 0; sy < srcLy; ++sy, --ty) {
		TPixelGR8 *srcPix = grayAppImage->pixels(sy);
		TPixelCM32 *tarPix = outImage->pixels(ty) + glyphOrig.x;
		for (int x = 0; x < srcLx; ++x) {
			int tone = 256 - (srcPix->value << 2);

			// grayScale  ToonzImage tone   Meaning
			//         0  255               Bg = PurePaint
			//         1  252
			//                 ...
			//        63    4
			//        64    0               Fg = Pure Ink

			if (tone < 0)
				tone = 0;

			if (tone >= 255)
				*tarPix = bgColor;
			else
				*tarPix = TPixelCM32(inkId, 0, tone); // BackgroundStyle,tone);

			++srcPix;
			++tarPix;
		}
	}
	grayAppImage->unlock();
	outImage->unlock();

	return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------

TPoint TFont::getDistance(wchar_t firstChar, wchar_t secondChar) const
{
	int advance;
	BOOL result = GetCharWidth32W(m_pimpl->m_hdc, firstChar, firstChar, &advance);
	assert(result);
	if (m_pimpl->m_hasKerning && secondChar) {
		KerningPairs::iterator it = m_pimpl->m_kerningPairs.find(make_pair(firstChar, secondChar));
		if (it != m_pimpl->m_kerningPairs.end()) {
			advance += it->second;
		}
	}
	return TPoint(advance, 0);
}

//-----------------------------------------------------------------------------

int TFont::getMaxHeight() const
{
	return m_pimpl->m_metrics.tmHeight;
}

//-----------------------------------------------------------------------------

int TFont::getMaxWidth() const
{
	return m_pimpl->m_metrics.tmMaxCharWidth;
}
//-----------------------------------------------------------------------------

int TFont::getLineAscender() const
{
	return m_pimpl->m_metrics.tmAscent;
}

//-----------------------------------------------------------------------------

int TFont::getLineDescender() const
{
	return -m_pimpl->m_metrics.tmDescent;
}

//-----------------------------------------------------------------------------

bool TFont::hasKerning() const
{
	return m_pimpl->m_hasKerning;
}

//-----------------------------------------------------------------------------

bool TFont::hasVertical() const
{
	return m_pimpl->m_hasVertical;
}

//=============================================================================
//====================      TFontManager  =====================================
//=============================================================================

//---------------------------------------------------------

struct TFontManager::Impl {
	WindowsFontTable m_families;
	bool m_loaded;

	LOGFONTW m_currentLogFont;
	TFont *m_currentFont;

	// this option is set by library user when he wants to write vertically.
	// In this implementation, if m_vertical is true and the font
	// has the @-version, the library use it.
	bool m_vertical;

	TFontManager::Impl()
		: m_loaded(false), m_currentFont(0), m_vertical(false)
	{
	}
};

//---------------------------------------------------------

TFontManager::TFontManager()
{
	m_pimpl = new TFontManager::Impl();
}

//---------------------------------------------------------

TFontManager::~TFontManager()
{
	delete m_pimpl;
}

//---------------------------------------------------------

TFontManager *TFontManager::instance()
{
	static TFontManager theManager;
	return &theManager;
}

//---------------------------------------------------------

namespace
{
BOOL CALLBACK EnumFamCallBack(
	CONST LOGFONTW *lplf,
	CONST TEXTMETRICW *,
	DWORD FontType,
	LPARAM data)
{
	if (FontType & TRUETYPE_FONTTYPE) {
		LOGFONTW newLplf = *lplf;
		newLplf.lfHeight = 200;
		newLplf.lfWidth = 0;
		WindowsFontTable &table = *(WindowsFontTable *)data;
		table[lplf->lfFaceName] = newLplf;
		return TRUE;
	}
	return TRUE;
}
}

//---------------------------------------------------------

void TFontManager::loadFontNames()
{
	if (m_pimpl->m_loaded)
		return;

	HDC hdc = CreateCompatibleDC(NULL);
	if (!hdc)
		throw TFontLibraryLoadingError();
	EnumFontFamiliesW(hdc,
					  (LPCWSTR)NULL,
					  (FONTENUMPROCW)EnumFamCallBack,
					  (LPARAM) & (m_pimpl->m_families));
	DeleteDC(hdc);
	hdc = 0;
	if (m_pimpl->m_families.empty())
		throw TFontLibraryLoadingError();

	m_pimpl->m_loaded = true;
}

//---------------------------------------------------------

void TFontManager::setFamily(const wstring family)
{

	wstring userFamilyName = ((family.c_str())[0] == L'@') ? wstring(family.c_str() + 1) : family;
	wstring realFamilyName = (m_pimpl->m_vertical && (family.c_str())[0] != L'@') ? L"@" + family : family;

	wstring currentFamilyName = wstring(m_pimpl->m_currentLogFont.lfFaceName);

	if (currentFamilyName == realFamilyName)
		return;

	LOGFONTW logfont;
	if (m_pimpl->m_vertical) {
		WindowsFontTable::iterator it = m_pimpl->m_families.find(realFamilyName);
		if (it != m_pimpl->m_families.end())
			logfont = it->second;
		else {
			it = m_pimpl->m_families.find(userFamilyName);
			assert(it != m_pimpl->m_families.end());
			if (it != m_pimpl->m_families.end())
				logfont = it->second;
			else
				throw TFontCreationError();
		}
	} else {
		WindowsFontTable::iterator it = m_pimpl->m_families.find(userFamilyName);
		assert(it != m_pimpl->m_families.end());
		if (it != m_pimpl->m_families.end())
			logfont = it->second;
		else
			throw TFontCreationError();
	}

	if (m_pimpl->m_currentFont) {
		logfont.lfHeight = m_pimpl->m_currentLogFont.lfHeight;
		logfont.lfItalic = m_pimpl->m_currentLogFont.lfItalic;
		logfont.lfWeight = m_pimpl->m_currentLogFont.lfWeight;
	} else
		logfont.lfHeight = 200;

	try {
		HDC hdc = CreateCompatibleDC(NULL);
		TFont *newfont = new TFont(logfont, hdc);
		delete m_pimpl->m_currentFont;
		m_pimpl->m_currentFont = newfont;
		m_pimpl->m_currentLogFont = logfont;
	} catch (TException &) {
		throw TFontCreationError();
	}
}

//---------------------------------------------------------

void TFontManager::setTypeface(const wstring typeface)
{
	LOGFONTW logfont = m_pimpl->m_currentLogFont;
	logfont.lfItalic = (typeface == L"Italic" ||
						typeface == L"Bold Italic")
						   ? TRUE
						   : FALSE;
	logfont.lfWeight = (typeface == L"Bold" ||
						typeface == L"Bold Italic")
						   ? 700
						   : 400;

	try {
		HDC hdc = CreateCompatibleDC(NULL);
		TFont *newfont = new TFont(logfont, hdc);
		delete m_pimpl->m_currentFont;
		m_pimpl->m_currentFont = newfont;
		m_pimpl->m_currentLogFont = logfont;
	} catch (TException &) {
		throw TFontCreationError();
	}
}

//---------------------------------------------------------

void TFontManager::setSize(int size)
{
	LOGFONTW logfont = m_pimpl->m_currentLogFont;
	logfont.lfHeight = size;
	try {
		HDC hdc = CreateCompatibleDC(NULL);
		TFont *newfont = new TFont(logfont, hdc);
		delete m_pimpl->m_currentFont;
		m_pimpl->m_currentFont = newfont;
		m_pimpl->m_currentLogFont = logfont;
	} catch (TException &) {
		throw TFontCreationError();
	}
}

//---------------------------------------------------------

wstring TFontManager::getCurrentFamily() const
{
	wstring currentFamilyName = (m_pimpl->m_currentLogFont.lfFaceName[0] == L'@') ? wstring(m_pimpl->m_currentLogFont.lfFaceName + 1) : wstring(m_pimpl->m_currentLogFont.lfFaceName);
	return currentFamilyName;
}

//---------------------------------------------------------

wstring TFontManager::getCurrentTypeface() const
{
	if (m_pimpl->m_currentLogFont.lfItalic) {
		if (m_pimpl->m_currentLogFont.lfWeight == 700)
			return L"Bold Italic";
		else
			return L"Italic";
	} else {
		if (m_pimpl->m_currentLogFont.lfWeight == 700)
			return L"Bold";
		else
			return L"Regular";
	}
}

//---------------------------------------------------------

TFont *TFontManager::getCurrentFont()
{
	if (m_pimpl->m_currentFont)
		return m_pimpl->m_currentFont;

	if (!m_pimpl->m_currentFont)
		loadFontNames();

	assert(!m_pimpl->m_families.empty());
	setFamily(m_pimpl->m_families.begin()->first);

	return m_pimpl->m_currentFont;
}

//---------------------------------------------------------

void TFontManager::getAllFamilies(vector<wstring> &families) const
{
	families.clear();
	families.reserve(m_pimpl->m_families.size());
	WindowsFontTable::iterator it = m_pimpl->m_families.begin();
	for (; it != m_pimpl->m_families.end(); ++it) {
		if ((it->first)[0] != L'@')
			families.push_back(it->first);
	}
}

//---------------------------------------------------------

void TFontManager::getAllTypefaces(vector<wstring> &typefaces) const
{
	typefaces.resize(4);
	typefaces[0] = L"Regular";
	typefaces[1] = L"Italic";
	typefaces[2] = L"Bold";
	typefaces[3] = L"Bold Italic";
}

//---------------------------------------------------------

void TFontManager::setVertical(bool vertical)
{
	if (m_pimpl->m_vertical == vertical)
		return;
	m_pimpl->m_vertical = vertical;

	wstring currentFamilyName = (m_pimpl->m_currentLogFont.lfFaceName[0] == L'@') ? wstring(m_pimpl->m_currentLogFont.lfFaceName + 1) : wstring(m_pimpl->m_currentLogFont.lfFaceName);
	if (vertical)
		currentFamilyName = L'@' + currentFamilyName;
	setFamily(currentFamilyName);
}
