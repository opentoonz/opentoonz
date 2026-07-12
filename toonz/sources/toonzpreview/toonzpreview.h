#pragma once

#ifndef TOONZPREVIEW_H
#define TOONZPREVIEW_H

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <unknwn.h>  // For IUnknown
#include <windows.h>

// COM Headers
#include <objidl.h>      // For IPersist, IPersistFile
#include <shlguid.h>     // Defines standard Shell IIDs
#include <shlobj.h>      // For IInitializeWithFile
#include <shlwapi.h>     // For QITAB
#include <thumbcache.h>  // For IThumbnailProvider, WTS_ALPHATYPE

// C++
#include <atomic>
#include <string>

// toonz
#include "traster.h"

// {A20E2270-0FE1-421D-A34E-98C26C13F9DB}
static const GUID CLSID_ToonzThumbnailProvider
    = {0xa20e2270,
       0xfe1,
       0x421d,
       {0xa3, 0x4e, 0x98, 0xc2, 0x6c, 0x13, 0xf9, 0xdb}};

const wchar_t *SZ_CLSID_TOONZPREVIEW =
    L"{A20E2270-0FE1-421D-A34E-98C26C13F9DB}";
class __declspec(uuid(
    "A20E2270-0FE1-421D-A34E-98C26C13F9DB")) CToonzThumbnailProvider;

// DLL Export Macro
#ifdef TOONZPREVIEW_EXPORTS
#define PREVIEW_API __declspec(dllexport)
#else
#define PREVIEW_API __declspec(dllimport)
#endif

// Forward Declarations
class CClassFactory;

// ---------------------------------------------------------------------------
// CToonzThumbnailProvider
// Implements IThumbnailProvider and IInitializeWithFile
// ---------------------------------------------------------------------------
class CToonzThumbnailProvider : public IInitializeWithStream,
                                public IInitializeWithFile,
                                public IPersistFile,
                                public IThumbnailProvider {
public:
  CToonzThumbnailProvider();
  virtual ~CToonzThumbnailProvider();

  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
  IFACEMETHODIMP_(ULONG) AddRef();
  IFACEMETHODIMP_(ULONG) Release();

  // IInitializeWithFile
  IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode);
  // IInitializeWithStream
  IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

  // IThumbnailProvider
  IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

  // IPersist
  IFACEMETHODIMP GetClassID(CLSID *pClassID);

  // IPersistFile
  IFACEMETHODIMP IsDirty();
  IFACEMETHODIMP Load(LPCWSTR pszFileName, DWORD dwMode);
  IFACEMETHODIMP Save(LPCWSTR pszFileName, BOOL fRemember);
  IFACEMETHODIMP SaveCompleted(LPCWSTR pszFileName);
  IFACEMETHODIMP GetCurFile(LPWSTR *ppszFileName);

private:
  long m_cRef;
  std::wstring m_filePath;
};

// ---------------------------------------------------------------------------
// CClassFactory
// Standard COM Class Factory
// ---------------------------------------------------------------------------
class CClassFactory : public IClassFactory {
public:
  CClassFactory();
  virtual ~CClassFactory();

  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
  IFACEMETHODIMP_(ULONG) AddRef();
  IFACEMETHODIMP_(ULONG) Release();

  // IClassFactory
  IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
  IFACEMETHODIMP LockServer(BOOL fLock);

private:
  long m_cRef;
};

// Globals
extern long g_cDllRef;
extern HINSTANCE g_hInst;

// Helpers
void DebugLog(const std::string &message);
std::string WideToNarrow(const std::wstring &wideStr);
void EnsureQtApplication();
HBITMAP RasterToHBITMAP(const TRaster32P &ras);

#endif  // TOONZPREVIEW_H