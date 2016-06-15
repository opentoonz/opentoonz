#pragma once

#ifndef TFONT_H
#define TFONT_H

// Toonz includes
#include "trastercm.h"
#include "texception.h"

// STL includes
#include <string>

#undef DVAPI
#undef DVVAR
#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-----------------------------------------------------

//  Forward declarations
class TVectorImageP;
class TFont;

//-----------------------------------------------------

// TFont declaration. The class is currently not directly available under 64-bit
// MAC OSX.

#if !defined(__LP64__) || !defined(MACOSX)

#ifdef MACOSX
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#endif

//=================================================================================================

//! Class to manage loading and rendering of fonts.

class DVAPI TFont {
public:
  struct Impl;

private:
  friend class TFontManager;
  Impl *m_pimpl;

#ifdef _WIN32
  TFont(const LOGFONTW &, HDC hdc);
#elif defined(MACOSX)
  TFont(ATSUFontID, int size);
#else
  TFont(const std::wstring family, const std::wstring face, int size);
#endif

public:
  ~TFont();

  TPoint drawChar(TVectorImageP &outImage, wchar_t charcode,
                  wchar_t nextCode = 0) const;
  TPoint drawChar(TRasterGR8P &outImage, TPoint &glyphOrigin, wchar_t charcode,
                  wchar_t nextCode = 0) const;
  TPoint drawChar(TRasterCM32P &outImage, TPoint &glyphOrigin, int inkId,
                  wchar_t charcode, wchar_t nextCode = 0) const;

  // brief  get kerning distance between two characters
  TPoint getDistance(wchar_t firstChar, wchar_t secondChar) const;

  int getMaxHeight() const;
  int getMaxWidth() const;
  // void disableKerning();
  // void enableKerning();
  bool hasKerning() const;
  int getLineAscender() const;
  int getLineDescender() const;
  bool hasVertical() const;
  std::wstring getFamily() const;
  std::wstring getTypeface() const;
  // bool isValidCode( wchar_t code ) const;

private:
  // not implemented
  TFont(const TFont &);
  TFont &operator=(const TFont &);
};

#endif  //!__LP64__ || !MACOSX

//-----------------------------------------------------------------------------

class TFontCreationError : public TException {
public:
  TFontCreationError() : TException("E_CanNotUseSelectedFont") {}
};

//-----------------------------------------------------------------------------

class TFontLibraryLoadingError : public TException {
public:
  TFontLibraryLoadingError() : TException("E_CanNotLoadFonts") {}
};

//-----------------------------------------------------------------------------

// singleton
class DVAPI TFontManager {
  struct Impl;
  Impl *m_pimpl;

  TFontManager();
  ~TFontManager();

public:
  static TFontManager *instance();

  TFont *getCurrentFont();

  //! throws TFontLibraryLoadingError if can not load fonts
  void loadFontNames();

  //! if can not create font, throws TFontCreationError and leave as current the
  //! old one
  void setFamily(const std::wstring family);

  //! if can not create font, throws TFontCreationError and leave as current the
  //! old one
  void setTypeface(const std::wstring typeface);

  std::wstring getCurrentFamily() const;
  std::wstring getCurrentTypeface() const;
  void getAllFamilies(std::vector<std::wstring> &families) const;
  void getAllTypefaces(std::vector<std::wstring> &typefaces) const;
  void setVertical(bool vertical);
  void setSize(int size);

// --------- TFont methods  called on curren font -----------

#if !defined(__LP64__) || defined(LINUX)

  TPoint drawChar(TVectorImageP &outImage, wchar_t charcode,
                  wchar_t nextCode = 0) {
    return getCurrentFont()->drawChar(outImage, charcode, nextCode);
  }

  TPoint drawChar(TRasterGR8P &outImage, TPoint &glyphOrigin, wchar_t charcode,
                  wchar_t nextCode = 0) {
    return getCurrentFont()->drawChar(outImage, glyphOrigin, charcode,
                                      nextCode);
  }

  TPoint drawChar(TRasterCM32P &outImage, TPoint &glyphOrigin, int inkId,
                  wchar_t charcode, wchar_t nextCode = 0) {
    return getCurrentFont()->drawChar(outImage, glyphOrigin, inkId, charcode,
                                      nextCode);
  }

  TPoint getDistance(wchar_t firstChar, wchar_t secondChar) {
    return getCurrentFont()->getDistance(firstChar, secondChar);
  }

  int getMaxHeight() { return getCurrentFont()->getMaxHeight(); }
  int getMaxWidth() { return getCurrentFont()->getMaxWidth(); }
  bool hasKerning() { return getCurrentFont()->hasKerning(); }
  int getLineAscender() { return getCurrentFont()->getLineAscender(); }
  int getLineDescender() { return getCurrentFont()->getLineDescender(); }
  bool hasVertical() { return getCurrentFont()->hasVertical(); }

#else  //__LP64__

  TPoint drawChar(TVectorImageP &outImage, wchar_t charcode,
                  wchar_t nextCode = 0);
  TPoint drawChar(TRasterGR8P &outImage, TPoint &glyphOrigin, wchar_t charcode,
                  wchar_t nextCode = 0);
  TPoint drawChar(TRasterCM32P &outImage, TPoint &glyphOrigin, int inkId,
                  wchar_t charcode, wchar_t nextCode = 0);

  TPoint getDistance(wchar_t firstChar, wchar_t secondChar);

  int getMaxHeight();
  int getMaxWidth();
  bool hasKerning();
  int getLineAscender();
  int getLineDescender();
  bool hasVertical();

#endif  //__LP64__
};

//-----------------------------------------------------------------------------

#endif
