

// character_manager.cpp: implementation of the TFont class
// for FreeType 2.
//
//////////////////////////////////////////////////////////////////////

//#include <ft2build.h>
//#include FT_FREETYPE_H


#include <QStringList>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QImage>
#include <QPainterPath>
#include <QRawFont>


#include <vector>
#include <iostream>
#include <string>

#include "tpixelgr.h"
#include "tfont.h"
#include "tstroke.h"
#include "tcurves.h"
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

struct TFont::Impl {
	bool m_hasKerning;
	int m_hasVertical;
	QFont m_font;
	//XXX:cache a QFontMetrics m_metrics; ?
	//XXX:cache a QRawFont m_raw; ?

	//  KerningPairs m_kerningPairs;

/*
	ATSUStyle m_style;
	ATSUFontID m_fontId;
	ATSUTextLayout m_layout;
	Fixed m_size;
	int m_ascender;
	int m_descender;
*/

	Impl(const QString &family, const QString &style, int size);
	~Impl();

	//void getChar();
};

//-----------------------------------------------------------------------------

TFont::TFont(const wstring family, const wstring face, int size)
{
	m_pimpl = new Impl(QString::fromStdWString(family),
		QString::fromStdWString(face), size);
}

//-----------------------------------------------------------------------------

TFont::~TFont()
{
	delete m_pimpl;
}

//-----------------------------------------------------------------------------

TFont::Impl::Impl(const QString &family, const QString &style, int size)
{
	m_font = QFont(family, size);
	m_font.setStyleName(style);

}

//-----------------------------------------------------------------------------

TFont::Impl::~Impl()
{
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TVectorImageP &image, wchar_t charcode, wchar_t nextCharCode) const
{
	QRawFont raw(QRawFont::fromFont(m_pimpl->m_font));

	QChar chars[2] = { charcode, nextCharCode };
	quint32 indices[2];
	int count = 2;

	if (!raw.glyphIndexesForChars(chars, 2, indices, &count) || count < 1)
		return TPoint(0, 0);
	QPainterPath path = raw.pathForGlyph(indices[0]);

	// empty glyph, nothing to do
	if (path.elementCount() < 1)
		return getDistance(charcode, nextCharCode);

	// force closing the last path
	if (path.elementAt(path.elementCount()-1).type != QPainterPath::MoveToElement)
		path.moveTo(0.0, 0.0);

	int i, n = path.elementCount();
	int strokes = 0;
	std::vector<TThickPoint> points;


	TThickPoint pts[4];
	int nCubicPts = 0;

	for (i = 0; i < n; i++) {
		QPainterPath::Element e = path.elementAt(i);
		e.y = -e.y;


		switch (e.type) {
		case QPainterPath::MoveToElement:
			if (!points.empty()) {

				if (points.back() != points.front()) {
					points.push_back(0.5 * (points.back() + points.front()));
					points.push_back(points.front());
				}

				TStroke *stroke = new TStroke(points);
				stroke->setSelfLoop(true);
				image->addStroke(stroke);
				strokes++;
				points.clear();
			}
			points.push_back(TThickPoint(e.x, e.y, 0));
			break;
		case QPainterPath::LineToElement:
		{
			TThickPoint p0 = points.back();
			TThickPoint p1 = TThickPoint(e.x, e.y, 0);
			points.push_back((p0 + p1) * 0.5);
			points.push_back(p1);
			break;
		}
		case QPainterPath::CurveToElement:
			pts[0] = points.back();
			pts[1] = TThickPoint(e.x, e.y, 0);
			nCubicPts = 2;
			break;
		case QPainterPath::CurveToDataElement:
			pts[nCubicPts++] = TThickPoint(e.x, e.y, 0);
			if (nCubicPts == 4) {
				vector<TThickQuadratic *> chunkArray;
				computeQuadraticsFromCubic(pts[0],pts[1],pts[2],pts[3], 0.09,
					chunkArray);

				for (int j = 0; j < chunkArray.size(); j++) {
					points.push_back(chunkArray[j]->getP1());
					points.push_back(chunkArray[j]->getP2());
				}
				nCubicPts = 0;
			}
			break;
		}
	}

	if (strokes > 1)
		image->group(0, strokes);

	return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TRasterGR8P &outImage, TPoint &unused, wchar_t charcode, wchar_t nextCharCode) const
{
	QRawFont raw(QRawFont::fromFont(m_pimpl->m_font));

	QChar chars[2] = { charcode, nextCharCode };
	quint32 indices[2];
	int count = 2;

	if (!raw.glyphIndexesForChars(chars, 2, indices, &count) || count < 1)
		return TPoint(0, 0);

	QImage image(raw.alphaMapForGlyph(indices[0], QRawFont::PixelAntialiasing));

	if (image.format() != QImage::Format_Indexed8)
		throw TException(L"bad QImage format " + image.format());

	int height = image.height();
	int width = image.width();

	outImage = TRasterGR8P(width, height);

/*
	TPixelGR8 bgp;
	bgp.value = 255;
	outImage->fill(bgp);
*/
	void *data = outImage->getRawData();

	memcpy(data, image.bits(), image.byteCount());

	return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TRasterCM32P &outImage, TPoint &unused, int inkId, wchar_t charcode, wchar_t nextCharCode) const
{
	TRasterGR8P grayAppImage;
	this->drawChar(grayAppImage, unused, charcode, nextCharCode);

	int lx = grayAppImage->getLx();
	int ly = grayAppImage->getLy();

	outImage = TRasterCM32P(lx, ly);

	assert(TPixelCM32::getMaxTone() == 255);
	TPixelCM32 bgColor(0, 0, TPixelCM32::getMaxTone());
	grayAppImage->lock();
	outImage->lock();
	int ty = 0;
	for (int gy = ly - 1; gy >= 0; --gy, ++ty) {
		TPixelGR8 *srcPix = grayAppImage->pixels(gy);
		TPixelCM32 *tarPix = outImage->pixels(ty);
		for (int x = 0; x < lx; ++x) {
			int tone = srcPix->value;

			if (tone == 255)
				*tarPix = bgColor;
			else
				*tarPix = TPixelCM32(inkId, 0, tone);

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
	QRawFont raw(QRawFont::fromFont(m_pimpl->m_font));
	QChar chars[2] = { firstChar, secondChar };
	quint32 indices[2];
	QPointF advances[2];
	int count = 2;

	if (!raw.glyphIndexesForChars(chars, 2, indices, &count) || count != 2)
		return TPoint(0, 0);

	if (!raw.advancesForGlyphIndexes(indices, advances, 2, QRawFont::KernedAdvances))
		return TPoint(0, 0);

	int advance = (int)(advances[0].x());

	return TPoint(advance, 0);
}

//-----------------------------------------------------------------------------

int TFont::getMaxHeight() const
{
	QFontMetrics metrics(m_pimpl->m_font);
	return metrics.ascent() - metrics.descent();
}

//-----------------------------------------------------------------------------

int TFont::getMaxWidth() const
{
	QFontMetrics metrics(m_pimpl->m_font);
	return metrics.maxWidth();
}
//-----------------------------------------------------------------------------

int TFont::getLineAscender() const
{
	QFontMetrics metrics(m_pimpl->m_font);
	return metrics.ascent();
}

//-----------------------------------------------------------------------------

int TFont::getLineDescender() const
{
	QFontMetrics metrics(m_pimpl->m_font);
	return metrics.descent();
}

//-----------------------------------------------------------------------------

bool TFont::hasKerning() const
{
	return m_pimpl->m_font.kerning();
}

//-----------------------------------------------------------------------------

bool TFont::hasVertical() const
{
	//FIXME
	return false;
}

//-----------------------------------------------------------------------------

//=============================================================================
//====================      TFontManager  =====================================
//=============================================================================

//---------------------------------------------------------

struct TFontManager::Impl {
	QFontDatabase *m_qfontdb;
	bool m_loaded;

	TFont *m_currentFont;
	wstring m_currentFamily;
	wstring m_currentTypeface;
	int m_size;


	// this option is set by library user when he wants to write vertically.
	// In this implementation, if m_vertical is true and the font
	// has the @-version, the library use it.
	bool m_vertical;

	Impl()
		: m_qfontdb(NULL), m_loaded(false), m_currentFont(0), m_size(0),
		  m_vertical(false)
	{
	}
	~Impl()
	{
		delete m_qfontdb;
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

void TFontManager::loadFontNames()
{
	if (m_pimpl->m_loaded)
		return;

	m_pimpl->m_qfontdb = new QFontDatabase;

	if (m_pimpl->m_qfontdb->families().empty())
		throw TFontLibraryLoadingError();

	m_pimpl->m_loaded = true;
}

//---------------------------------------------------------

void TFontManager::setFamily(const wstring family)
{
	if (m_pimpl->m_currentFamily == family)
		return;

	QString qFamily = QString::fromStdWString(family);
	QStringList families = m_pimpl->m_qfontdb->families();
	if (!families.contains(qFamily))
		throw TFontCreationError();

	m_pimpl->m_currentFamily = family;

	// XXX: if current style is not valid for family, reset it?
	// doing so asserts when chosing a font in the GUI
/*
	QStringList styles = m_pimpl->m_qfontdb->styles(qFamily);
	if (styles.contains(QString::fromStdWString(m_pimpl->m_currentTypeface)))
		m_pimpl->m_currentTypeface = L"";
*/
	delete m_pimpl->m_currentFont;
	m_pimpl->m_currentFont = new TFont(m_pimpl->m_currentFamily,
		m_pimpl->m_currentTypeface, m_pimpl->m_size);
}

//---------------------------------------------------------

void TFontManager::setTypeface(const wstring typeface)
{
	if (m_pimpl->m_currentTypeface == typeface)
		return;

	QString qTypeface = QString::fromStdWString(typeface);
	QStringList styles = m_pimpl->m_qfontdb->styles(QString::fromStdWString(m_pimpl->m_currentFamily));
	if (!styles.contains(qTypeface))
		throw TFontCreationError();

	m_pimpl->m_currentTypeface = typeface;

	delete m_pimpl->m_currentFont;
	m_pimpl->m_currentFont = new TFont(m_pimpl->m_currentFamily,
		m_pimpl->m_currentTypeface, m_pimpl->m_size);
}

//---------------------------------------------------------

void TFontManager::setSize(int size)
{
	if (m_pimpl->m_size == size)
		return;
	m_pimpl->m_size = size;
	delete m_pimpl->m_currentFont;
	m_pimpl->m_currentFont = new TFont(m_pimpl->m_currentFamily,
		m_pimpl->m_currentTypeface, m_pimpl->m_size);
}

//---------------------------------------------------------

wstring TFontManager::getCurrentFamily() const
{
	return m_pimpl->m_currentFamily;
}

//---------------------------------------------------------

wstring TFontManager::getCurrentTypeface() const
{
	return m_pimpl->m_currentTypeface;
}

//---------------------------------------------------------

TFont *TFontManager::getCurrentFont()
{
	if (m_pimpl->m_currentFont)
		return m_pimpl->m_currentFont;

	if (!m_pimpl->m_currentFont)
		loadFontNames();

	assert(!m_pimpl->m_qfontdb->families().empty());
	setFamily(m_pimpl->m_qfontdb->families().first().toStdWString());

	return m_pimpl->m_currentFont;
}

//---------------------------------------------------------

void TFontManager::getAllFamilies(vector<wstring> &families) const
{
	QStringList qFamilies = m_pimpl->m_qfontdb->families();

	families.clear();
	families.reserve(qFamilies.count());

	QStringList::const_iterator it = qFamilies.begin();
	for (; it != qFamilies.end(); ++it) {
		families.push_back(it->toStdWString());
	}
}

//---------------------------------------------------------

void TFontManager::getAllTypefaces(vector<wstring> &typefaces) const
{
	typefaces.clear();

	QStringList qStyles = m_pimpl->m_qfontdb->styles(QString::fromStdWString(m_pimpl->m_currentFamily));

	if (qStyles.empty())
		return;

	typefaces.reserve(qStyles.count());
	QStringList::const_iterator it_typeface = qStyles.begin();
	for (; it_typeface != qStyles.end(); ++it_typeface) {
		typefaces.push_back(it_typeface->toStdWString());
	}
}

//---------------------------------------------------------

void TFontManager::setVertical(bool vertical) {}

//---------------------------------------------------------


