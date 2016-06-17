

#ifndef __LP64__

#include <vector>
#include <iostream>
#include <string>
#include "tmathutil.h"
#include "tdebugmessage.h"
#include "tfont.h"
#include "tstroke.h"
#include "tcurves.h"
#include "tconvert.h"
#include "tvectorimage.h"

using namespace std;

//----------------------------------------- structures
//-------------------------------------------------------------------

typedef struct {
  Float32Point origin;  // The origin of the current glyph
  Boolean first;        // Keeps track of which segment is first in a glyph
  Float32Point
      current;  // The current pen position (used to filter degenerate cases)

  float adv;
  TVectorImageP m_image;
  std::vector<TThickPoint> m_points;

} MyCurveCallbackData;

typedef struct {
  ATSGlyphRef glyphID;  // The glyphID.  This is simply an index into a table in
                        // the font.
  Float32Point relativeOrigin;  // The origin of this glyph -- relative to the
                                // origin of the line.
} MyGlyphRecord;

//-----------------------------------------
//callback---------------------------------------------------------------------

OSStatus MyQuadraticLineProc(const Float32Point *pt1, const Float32Point *pt2,
                             void *callBackDataPtr) {
  /*
// Adjust the points according to the glyph origin
float x1 = ((MyCurveCallbackData *)callBackDataPtr)->origin.x + pt1->x;
float y1 = ((MyCurveCallbackData *)callBackDataPtr)->origin.y + pt1->y;
float x2 = ((MyCurveCallbackData *)callBackDataPtr)->origin.x + pt2->x;
float y2 = ((MyCurveCallbackData *)callBackDataPtr)->origin.y + pt2->y;
*/

  MyCurveCallbackData *data = (MyCurveCallbackData *)callBackDataPtr;

  if (data->m_points.empty())
    data->m_points.push_back(TThickPoint(pt1->x, pt1->y, 0));
  // else
  // assert(isAlmostEqual(pt1 e back)

  TThickPoint lastPoint = TThickPoint(pt2->x, pt2->y, 0);
  data->m_points.push_back((data->m_points.back() + lastPoint) * 0.5);
  data->m_points.push_back(lastPoint);

  return noErr;
}

OSStatus MyQuadraticCurveProc(const Float32Point *pt1,
                              const Float32Point *controlPt,
                              const Float32Point *pt2, void *callBackDataPtr) {
  /*
// Adjust the points according to the glyph origin
float x1 = ((MyCurveCallbackData *)callBackDataPtr)->origin.x + pt1->x;
float y1 = ((MyCurveCallbackData *)callBackDataPtr)->origin.y + pt1->y;
float x2 = ((MyCurveCallbackData *)callBackDataPtr)->origin.x + pt2->x;
float y2 = ((MyCurveCallbackData *)callBackDataPtr)->origin.y + pt2->y;
float cpx = ((MyCurveCallbackData *)callBackDataPtr)->origin.x + controlPt->x;
float cpy = ((MyCurveCallbackData *)callBackDataPtr)->origin.y + controlPt->y;
*/
  MyCurveCallbackData *data = (MyCurveCallbackData *)callBackDataPtr;

  if (data->m_points.empty())
    data->m_points.push_back(TThickPoint(pt1->x, pt1->y, 0));
  // else
  // assert(isAlmostEqual(pt1 e back)

  data->m_points.push_back(TThickPoint(controlPt->x, controlPt->y, 0));
  data->m_points.push_back(TThickPoint(pt2->x, pt2->y, 0));

  return noErr;
}

OSStatus MyQuadraticNewPathProc(void *callBackDataPtr) {
  assert(((MyCurveCallbackData *)callBackDataPtr)->m_points.empty());
  return noErr;
}

OSStatus MyQuadraticClosePathProc(void *callBackDataPtr) {
  MyCurveCallbackData *data = (MyCurveCallbackData *)callBackDataPtr;

  assert(
      data->m_points.size() >= 3 &&
      data->m_points.size() &
          1);  // il numero di punti di controllo devono essere dispari e >= 3

  TStroke *stroke = new TStroke(data->m_points);
  stroke->setSelfLoop(true);

  data->m_points.clear();
  data->m_image->addStroke(stroke);

  return noErr;
}

//------------------------------------------------------------------------------------------------------------------

void GetGlyphIDsAndPositions(ATSUTextLayout iLayout, UniCharArrayOffset iStart,
                             UniCharCount iLength,
                             MyGlyphRecord **oGlyphRecordArray,
                             ItemCount *oNumGlyphs) {
  // This block of code uses the new Direct Access APIs, which are only
  // available on Mac OS X 10.2 and later systems
  //

  ATSLayoutRecord *layoutRecords;
  ItemCount numRecords;
  Fixed *deltaYs;
  ItemCount numDeltaYs;
  unsigned int i;
  OSStatus status;

  // Get the arrays of glyph information
  status = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(
      iLayout, iStart, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
      (void **)&layoutRecords, &numRecords);
  assert(status == noErr);

  status = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(
      iLayout, iStart, kATSUDirectDataBaselineDeltaFixedArray,
      (void **)&deltaYs, &numDeltaYs);
  assert(status == noErr);

  // Build the array of MyGlyphRecords
  *oGlyphRecordArray =
      (MyGlyphRecord *)malloc(numRecords * sizeof(MyGlyphRecord));
  *oNumGlyphs = numRecords;

  for (i = 0; i < *oNumGlyphs; i++) {
    // Fill in the glyphID
    (*oGlyphRecordArray)[i].glyphID = layoutRecords[i].glyphID;

    // Set up the relative origin of the glyph
    //
    // The real position is the x coordinate of the glyph, relative to the
    // beginning of the line
    // The baseline delta (deltaY), if any, is the y coordinate of the glyph,
    // relative to the baseline
    //
    (*oGlyphRecordArray)[i].relativeOrigin.x = Fix2X(layoutRecords[i].realPos);

    if (deltaYs == NULL) {
      (*oGlyphRecordArray)[i].relativeOrigin.y = 0.0;
    } else {
      (*oGlyphRecordArray)[i].relativeOrigin.y = 0.0 - Fix2X(deltaYs[i]);
    }
  }

  // Free the arrays of glyph information
  if (deltaYs != NULL) {
    status = ATSUDirectReleaseLayoutDataArrayPtr(
        NULL, kATSUDirectDataBaselineDeltaFixedArray, (void **)&deltaYs);
    assert(status == noErr);
  }
  status = ATSUDirectReleaseLayoutDataArrayPtr(
      NULL, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
      (void **)&layoutRecords);
  assert(status == noErr);
}

void drawQuadratics(ATSUTextLayout iLayout, ATSUStyle iStyle,
                    UniCharArrayOffset start, UniCharCount length,
                    MyCurveCallbackData &data) {
  // boh ----------------
  Fixed penX = 0;
  Fixed penY = 0;
  // -------------------

  MyGlyphRecord *glyphRecordArray;
  ItemCount numGlyphs;
  ATSQuadraticNewPathUPP newPathProc;
  ATSQuadraticLineUPP lineProc;
  ATSQuadraticCurveUPP curveProc;
  ATSQuadraticClosePathUPP closePathProc;

  OSStatus status;
  unsigned int i;

  // Create the Quadratic callbacks
  newPathProc   = NewATSQuadraticNewPathUPP(MyQuadraticNewPathProc);
  lineProc      = NewATSQuadraticLineUPP(MyQuadraticLineProc);
  curveProc     = NewATSQuadraticCurveUPP(MyQuadraticCurveProc);
  closePathProc = NewATSQuadraticClosePathUPP(MyQuadraticClosePathProc);

  // Get the array of glyph information
  GetGlyphIDsAndPositions(iLayout, start, length, &glyphRecordArray,
                          &numGlyphs);

  // Loop over all the glyphs
  for (i = 0; i < numGlyphs; i++) {
    // Set up the absolute origin of the glyph
    data.origin.x = Fix2X(penX) + glyphRecordArray[i].relativeOrigin.x;
    data.origin.y = Fix2X(penY) + glyphRecordArray[i].relativeOrigin.y;

    // Reset state for quadratic drawing (the callbacks only do a MoveTo on the
    // very first segment)
    data.first = true;

    // If this is a deleted glyph (-1), don't draw it.  Otherwise, go ahead.
    if (glyphRecordArray[i].glyphID != kATSDeletedGlyphcode) {
      status = ATSUGlyphGetQuadraticPaths(iStyle, glyphRecordArray[i].glyphID,
                                          newPathProc, lineProc, curveProc,
                                          closePathProc, &data, &status);
      assert(status == noErr);
    }
  }
  // Free the array of glyph information
  free(glyphRecordArray);

  // Dispose of the Quadratic callbacks

  DisposeATSQuadraticNewPathUPP(newPathProc);
  DisposeATSQuadraticLineUPP(lineProc);
  DisposeATSQuadraticCurveUPP(curveProc);
  DisposeATSQuadraticClosePathUPP(closePathProc);
}

//=============================================================================

struct TFont::Impl {
  bool m_hasKerning;
  int m_hasVertical;

  //  KerningPairs m_kerningPairs;

  ATSUStyle m_style;
  ATSUFontID m_fontId;
  ATSUTextLayout m_layout;
  Fixed m_size;
  int m_ascender;
  int m_descender;

  Impl(ATSUFontID fontId, int size);
  ~Impl();

  // void getChar();
};

//-----------------------------------------------------------------------------

TFont::TFont(ATSUFontID fontId, int size) { m_pimpl = new Impl(fontId, size); }

//-----------------------------------------------------------------------------

TFont::~TFont() { delete m_pimpl; }

//-----------------------------------------------------------------------------

TFont::Impl::Impl(ATSUFontID fontId, int size)
    : m_fontId(fontId), m_size(Long2Fix(size)) {
  OSStatus status;

  long response;
  status = Gestalt(gestaltATSUFeatures, &response);
  assert(response & gestaltATSUDirectAccess);

  status = ATSUCreateStyle(&m_style);
  assert(status == noErr);

  ATSUAttributeTag tags[2];
  ByteCount sizes[2];
  ATSUAttributeValuePtr values[2];

  tags[0]   = kATSUFontTag;
  sizes[0]  = sizeof(ATSUFontID);
  values[0] = &fontId;

  tags[1]   = kATSUSizeTag;
  sizes[1]  = sizeof(Fixed);
  values[1] = &m_size;

  status = ATSUSetAttributes(m_style, 2, tags, sizes, values);
  // assert(status==noErr);

  UniChar dummyStr[]  = {'H', 'e', 'l', 'l', 'o'};
  UniCharCount length = sizeof(dummyStr) / sizeof(UniChar);

  status = ATSUCreateTextLayoutWithTextPtr(dummyStr, kATSUFromTextBeginning,
                                           kATSUToTextEnd, length, 1, &length,
                                           &m_style, &m_layout);
  // assert(status==noErr);

  ATSTrapezoid glyphBounds;
  status =
      ATSUGetGlyphBounds(m_layout, 0, 0, kATSUFromTextBeginning, kATSUToTextEnd,
                         kATSUseFractionalOrigins, 1, &glyphBounds, NULL);

  m_ascender = -FixedToInt(glyphBounds.upperLeft.y);
  assert(m_ascender > 0);
  m_descender = -FixedToInt(glyphBounds.lowerLeft.y);
  assert(m_descender < 0);
}

//-----------------------------------------------------------------------------

TFont::Impl::~Impl() {}

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TVectorImageP &image, wchar_t charcode,
                       wchar_t nextCharCode) const {
  OSStatus status;

  UniChar subString[2];
  subString[0]        = charcode;
  subString[1]        = 0 /*nextCharCode*/;
  UniCharCount length = sizeof(subString) / sizeof(UniChar);

  status = ATSUCreateTextLayoutWithTextPtr(
      subString, kATSUFromTextBeginning, kATSUToTextEnd, length, 1, &length,
      &(m_pimpl->m_style), &(m_pimpl->m_layout));
  assert(status == noErr);

  MyCurveCallbackData data;
  data.m_image = image;

  drawQuadratics(m_pimpl->m_layout, m_pimpl->m_style, kATSUFromTextBeginning,
                 kATSUToTextEnd, data);
  image->transform(TScale(1, -1));

  image->group(0, image->getStrokeCount());

  return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------
namespace {

void appDrawChar(TRasterGR8P &outImage, TFont::Impl *pimpl, wchar_t charcode) {
  OSStatus status;
  UniChar subString[2];
  subString[0]        = charcode;
  subString[1]        = 0;
  UniCharCount length = sizeof(subString) / sizeof(UniChar);

  status = ATSUCreateTextLayoutWithTextPtr(
      subString, kATSUFromTextBeginning, kATSUToTextEnd, length, 1, &length,
      &(pimpl->m_style), &(pimpl->m_layout));
  assert(status == noErr);

  ATSTrapezoid glyphBounds;
  status = ATSUGetGlyphBounds(pimpl->m_layout, 0, 0, kATSUFromTextBeginning,
                              kATSUToTextEnd, kATSUseFractionalOrigins, 1,
                              &glyphBounds, NULL);

  int height =
      FixedToInt(glyphBounds.lowerLeft.y) - FixedToInt(glyphBounds.upperLeft.y);
  int width = tmax(FixedToInt(glyphBounds.lowerRight.x),
                   FixedToInt(glyphBounds.upperRight.x)) -
              tmin(FixedToInt(glyphBounds.lowerLeft.x),
                   FixedToInt(glyphBounds.upperLeft.x));

  outImage = TRasterGR8P(width, height);
  TPixelGR8 bgp;
  bgp.value = 255;
  outImage->fill(bgp);
  void *data = outImage->getRawData();

  CGColorSpaceRef grayColorSpace =
      CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
  CGContextRef gContext = CGBitmapContextCreate(
      data, width, height, 8, width, grayColorSpace, kCGImageAlphaNone);

#if defined(DEBUG) || defined(_DEBUG)

  int bpc = CGBitmapContextGetBitsPerComponent(gContext);
  if (bpc != 8) std::cout << "BitsPerComponent: " << bpc << std::endl;

  int bpp = CGBitmapContextGetBitsPerPixel(gContext);
  if (bpp != 8) std::cout << "BitsPerPixel: " << bpp << std::endl;

  int bytesPerRow = CGBitmapContextGetBytesPerRow(gContext);
  int newWidth    = CGBitmapContextGetWidth(gContext);
  if (bytesPerRow != width || newWidth != width)
    std::cout << "BytesPerRow: " << bytesPerRow << " Old width= " << width
              << " New width= " << newWidth << std::endl;

  int newHeight = CGBitmapContextGetHeight(gContext);
  if (newHeight != height)
    std::cout << " Old height= " << height << " New height= " << newHeight
              << std::endl;

  assert(CGBitmapContextGetColorSpace(gContext) == grayColorSpace);

#endif

  ATSUAttributeTag tags[1];
  ByteCount sizes[1];
  ATSUAttributeValuePtr values[1];

  tags[0]   = kATSUCGContextTag;
  sizes[0]  = sizeof(CGContextRef);
  values[0] = &gContext;
  status    = ATSUSetLayoutControls(pimpl->m_layout, 1, tags, sizes, values);
  assert(status == noErr);

  ATSUDrawText(pimpl->m_layout, kATSUFromTextBeginning, kATSUToTextEnd, 0,
               glyphBounds.lowerLeft.y);
}
}
//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TRasterGR8P &outImage, TPoint &unused, wchar_t charcode,
                       wchar_t nextCharCode) const {
  appDrawChar(outImage, m_pimpl, charcode);
  outImage->yMirror();
  return getDistance(charcode, nextCharCode);
}

//-----------------------------------------------------------------------------

TPoint TFont::drawChar(TRasterCM32P &outImage, TPoint &unused, int inkId,
                       wchar_t charcode, wchar_t nextCharCode) const {
  TRasterGR8P grayAppImage;
  appDrawChar(grayAppImage, m_pimpl, charcode);

  int lx = grayAppImage->getLx();
  int ly = grayAppImage->getLy();

  outImage = TRasterCM32P(lx, ly);

  assert(TPixelCM32::getMaxTone() == 255);
  TPixelCM32 bgColor(0, 0, TPixelCM32::getMaxTone());
  grayAppImage->lock();
  outImage->lock();
  int ty = 0;
  for (int gy = ly - 1; gy >= 0; --gy, ++ty) {
    TPixelGR8 *srcPix  = grayAppImage->pixels(gy);
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

TPoint TFont::getDistance(wchar_t firstChar, wchar_t secondChar) const {
  OSStatus status;
  UniChar subString[2];
  subString[0]        = firstChar;
  subString[1]        = secondChar;
  UniCharCount length = sizeof(subString) / sizeof(UniChar);

  status = ATSUCreateTextLayoutWithTextPtr(
      subString, kATSUFromTextBeginning, kATSUToTextEnd, length, 1, &length,
      &(m_pimpl->m_style), &(m_pimpl->m_layout));
  assert(status == noErr);

  MyGlyphRecord *glyphRecordArray;
  ItemCount numGlyphs;

  // Get the array of glyph information
  GetGlyphIDsAndPositions(m_pimpl->m_layout, kATSUFromTextBeginning,
                          kATSUToTextEnd, &glyphRecordArray, &numGlyphs);

  assert(numGlyphs >= 2);

  assert(glyphRecordArray[0].relativeOrigin.x == 0);
  int advance = (int)(glyphRecordArray[1].relativeOrigin.x -
                      glyphRecordArray[0].relativeOrigin.x);
  if (advance == 0) {
    subString[1] = 0;
    status       = ATSUCreateTextLayoutWithTextPtr(
        subString, kATSUFromTextBeginning, kATSUToTextEnd, length, 1, &length,
        &(m_pimpl->m_style), &(m_pimpl->m_layout));

    GetGlyphIDsAndPositions(m_pimpl->m_layout, kATSUFromTextBeginning,
                            kATSUToTextEnd, &glyphRecordArray, &numGlyphs);
    advance = (int)(glyphRecordArray[1].relativeOrigin.x -
                    glyphRecordArray[0].relativeOrigin.x);
  }
  return TPoint(advance, 0);
}

//-----------------------------------------------------------------------------

int TFont::getMaxHeight() const {
  return m_pimpl->m_ascender - m_pimpl->m_descender;
}

//-----------------------------------------------------------------------------

int TFont::getMaxWidth() const {
  assert(!"not implemented yet");
  return 100;
}
//-----------------------------------------------------------------------------

int TFont::getLineAscender() const { return m_pimpl->m_ascender; }

//-----------------------------------------------------------------------------

int TFont::getLineDescender() const { return m_pimpl->m_descender; }

//-----------------------------------------------------------------------------

bool TFont::hasKerning() const { return true; }

//-----------------------------------------------------------------------------

bool TFont::hasVertical() const { return false; }

//-----------------------------------------------------------------------------

#include <string>
#include <map>

typedef std::map<std::string, ATSUFontID> FontFamily;

typedef std::map<std::string, FontFamily> FamilyMap;

struct TFontManager::Impl {
  FamilyMap m_families;
  bool m_loaded;
  ATSUFontID m_currentAtsuFontId;
  TFont *m_currentFont;
  wstring m_currentFamily;
  wstring m_currentTypeface;
  int m_size;

  Impl()
      : m_currentAtsuFontId(0), m_currentFont(0), m_loaded(false), m_size(70) {}

  bool setFontName(ATSUFontID fontId, int platform, int script, int lang);
  bool addFont(ATSUFontID);
  void loadFontNames();
  bool setFont(std::wstring family, std::wstring style);
};

using namespace std;

bool TFontManager::Impl::setFontName(ATSUFontID fontId, int platform,
                                     int script, int lang) {
  ByteCount oActualNameLength;
  ItemCount oFontCount;
  OSStatus status;

  char *buffer  = 0;
  char *buffer2 = 0;

  // chiedo la lunhezza del Full Family Name per allocare il buffer
  status = ATSUFindFontName(fontId, kFontFullName, platform, script, lang, 0, 0,
                            &oActualNameLength, 0);

  if (status != noErr || oActualNameLength <= 1) return false;

  buffer = new char[oActualNameLength + 1];
  // chiedo il Full Family Name
  status = ATSUFindFontName(fontId, kFontFullName, platform, script, lang,
                            oActualNameLength, buffer, &oActualNameLength,
                            &oFontCount);

  if (status != noErr || oActualNameLength <= 1 || buffer[0] == '\0') {
    delete[] buffer;
    return false;
  }
  buffer[oActualNameLength] = '\0';

  //-------------------

  // chiedo la lunhezza del Typeface Name per allocare il buffer
  status = ATSUFindFontName(fontId, kFontStyleName, platform, script, lang, 0,
                            0, &oActualNameLength, 0);

  if (status != noErr || oActualNameLength <= 1) {
    delete[] buffer;
    return false;
  }
  buffer2 = new char[oActualNameLength + 1];

  // chiedo il Typeface Name
  status = ATSUFindFontName(fontId, kFontStyleName, platform, script, lang,
                            oActualNameLength, buffer2, &oActualNameLength,
                            &oFontCount);

  if (status != noErr || oActualNameLength <= 1 || buffer2[0] == '\0') {
    delete[] buffer;
    delete[] buffer2;
    return false;
  } else
    buffer2[oActualNameLength] = '\0';

  string s_family(buffer);
  FontFamily &family      = m_families[s_family];
  family[string(buffer2)] = fontId;
  delete[] buffer;
  delete[] buffer2;
  return true;
}

bool TFontManager::Impl::addFont(ATSUFontID fontId) {
  int platform, script, lang;

  // per ottimizzare, ciclo solo sui valori
  // piu' comuni
  for (lang = -1; lang <= 0; lang++)
    for (platform = -1; platform <= 1; platform++)
      for (script = -1; script <= 0; script++)
        if (setFontName(fontId, platform, script, lang)) return true;

  // poi li provo tutti
  for (lang = -1; lang <= 139; lang++)
    for (script = -1; script <= 32; script++)
      for (platform = -1; platform <= 4; platform++) {
        // escludo quelli nel tri-ciclo for precedente.
        // Purtoppo si deve fare cosi:
        // non si puo' fare partendo con indici piu' alti nei cicli for!
        if (-1 <= lang && lang <= 0 && -1 <= script && script <= 0 &&
            -1 <= platform && platform <= 1)
          continue;

        if (setFontName(fontId, platform, script, lang)) return true;
      }

  return false;
}

void TFontManager::Impl::loadFontNames() {
  if (m_loaded) return;

  ItemCount oFontCount, fontCount;
  ATSUFontCount(&oFontCount);
  fontCount            = oFontCount;
  ATSUFontID *oFontIDs = new ATSUFontID[fontCount];
  ATSUGetFontIDs(oFontIDs, fontCount, &oFontCount);
  assert(fontCount == oFontCount);

  for (unsigned int i = 0; i < fontCount; i++) addFont(oFontIDs[i]);

  delete[] oFontIDs;
  m_loaded = true;
}

bool TFontManager::Impl::setFont(std::wstring family, std::wstring typeface) {
  if (family == m_currentFamily &&
      (typeface == m_currentTypeface || typeface == L""))
    return false;

  FamilyMap::iterator family_it = m_families.find(toString(family));
  if (family_it == m_families.end()) throw TFontCreationError();

  m_currentFamily = family;
  FontFamily::iterator style_it;
  if (typeface == L"") {
    style_it = ((*family_it).second).find(toString(m_currentTypeface));
    if (style_it == (*family_it).second.end())
      style_it = ((*family_it).second).begin();

    typeface = toWideString(style_it->first);
  } else
    style_it = ((*family_it).second).find(toString(typeface));

  if (style_it == (*family_it).second.end()) throw TFontCreationError();

  m_currentTypeface   = typeface;
  m_currentAtsuFontId = (*style_it).second;
  return true;
}

//---------------------------------------------------------

TFontManager::TFontManager() { m_pimpl = new TFontManager::Impl(); }

//---------------------------------------------------------

TFontManager::~TFontManager() { delete m_pimpl; }

//---------------------------------------------------------

TFontManager *TFontManager::instance() {
  static TFontManager theManager;
  return &theManager;
}

//---------------------------------------------------------

void TFontManager::loadFontNames() { m_pimpl->loadFontNames(); }

//---------------------------------------------------------

void TFontManager::setFamily(const wstring family) {
  bool changed = m_pimpl->setFont(family, L"");
  if (changed) {
    delete m_pimpl->m_currentFont;
    m_pimpl->m_currentFont =
        new TFont(m_pimpl->m_currentAtsuFontId, m_pimpl->m_size);
  }
}

//---------------------------------------------------------

void TFontManager::setTypeface(const wstring typeface) {
  bool changed = m_pimpl->setFont(m_pimpl->m_currentFamily, typeface);
  if (changed) {
    delete m_pimpl->m_currentFont;
    m_pimpl->m_currentFont =
        new TFont(m_pimpl->m_currentAtsuFontId, m_pimpl->m_size);
  }
}

//---------------------------------------------------------

void TFontManager::setSize(int size) {
  if (m_pimpl->m_size != size) {
    m_pimpl->m_size = size;
    delete m_pimpl->m_currentFont;
    m_pimpl->m_currentFont =
        new TFont(m_pimpl->m_currentAtsuFontId, m_pimpl->m_size);
  }
}

//---------------------------------------------------------

wstring TFontManager::getCurrentFamily() const {
  return m_pimpl->m_currentFamily;
}

//---------------------------------------------------------

wstring TFontManager::getCurrentTypeface() const {
  return m_pimpl->m_currentTypeface;
}

//---------------------------------------------------------

TFont *TFontManager::getCurrentFont() {
  if (m_pimpl->m_currentFont) return m_pimpl->m_currentFont;

  if (!m_pimpl->m_currentFont) loadFontNames();

  assert(!m_pimpl->m_families.empty());
  setFamily(toWideString(m_pimpl->m_families.begin()->first));

  return m_pimpl->m_currentFont;
}

//---------------------------------------------------------

void TFontManager::getAllFamilies(vector<wstring> &families) const {
  families.clear();
  families.reserve(m_pimpl->m_families.size());

  FamilyMap::iterator it = m_pimpl->m_families.begin();
  for (; it != m_pimpl->m_families.end(); ++it) {
    families.push_back(toWideString(it->first));
  }
}

//---------------------------------------------------------

void TFontManager::getAllTypefaces(vector<wstring> &typefaces) const {
  typefaces.clear();
  FamilyMap::iterator it_family =
      m_pimpl->m_families.find(toString(m_pimpl->m_currentFamily));
  if (it_family == m_pimpl->m_families.end()) return;
  FontFamily &typefaceSet = it_family->second;

  typefaces.reserve(typefaceSet.size());
  FontFamily::iterator it_typeface = typefaceSet.begin();
  for (; it_typeface != typefaceSet.end(); ++it_typeface) {
    typefaces.push_back(toWideString(it_typeface->first));
  }
}

//---------------------------------------------------------

void TFontManager::setVertical(bool vertical) {}

//---------------------------------------------------------

#endif
