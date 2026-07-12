// toonzpreview.cpp
#include "toonzpreview.h"
#include "tiio.h"
#include "tiio_std.h"
#include "tnzimage.h"

#include "toonzqt/icongenerator.h"
#include "timagecache.h"
#include "tfilepath.h"
#include "traster.h"
#include "tpixel.h"

// Qt Includes
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <shlwapi.h>
#include <propkey.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <codecvt>
#include <locale>
#include <mutex>
#include "tenv.h"

#pragma comment(lib, "shlwapi.lib")
// Ensure we link against the standard UUIDs
#pragma comment(lib, "uuid.lib")

// Global Variables
long g_cDllRef        = 0;
HINSTANCE g_hInst     = NULL;
const wchar_t *exts[] = {L".tnz", L".tlv", L".pli", L".tga"};
const wchar_t *progId = L"OpenToonz.ToonzPreview";
const int EXT_COUNT   = 4;

// CONSTANTS FOR REGISTRY
// This is the Standard Windows Interface ID for IThumbnailProvider.
// DO NOT CHANGE THIS. It must match system expectations.
const wchar_t *SZ_ITHUMBNAILPROVIDER_IID =
    L"{e357fccd-a995-4576-b01f-234630154e96}";

#ifdef TOONZPREVIW_DEBUG
void DebugLog(const std::string &message) {
  std::string fullMessage = "[ToonzPreview] " + message + "\n";
  OutputDebugStringA(fullMessage.c_str());

  WCHAR dllPath[MAX_PATH];
  if (GetModuleFileNameW(g_hInst, dllPath, MAX_PATH)) {
    WCHAR logFilePath[MAX_PATH];
    wcscpy_s(logFilePath, MAX_PATH, dllPath);
    PathRemoveFileSpecW(logFilePath);
    wcscat_s(logFilePath, MAX_PATH, L"\\toonz_preview.log");

    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string narrowLogPath = converter.to_bytes(logFilePath);

    std::ofstream logfile(narrowLogPath, std::ios::app);
    if (logfile.is_open()) {
      logfile << "[" << GetCurrentProcessId() << ":" << GetCurrentThreadId()
              << "] " << message << std::endl;
    }
  }
}

#else
void DebugLog(const std::string &message) {
  // No operation in release builds
}
#endif  // TOONZPREVIW_DEBUG

TFilePath getPaletteIconPath() {
  return TEnv::getConfigDir() + "icon" + "paletteIcon.ico";
}

std::string WideToNarrow(const std::wstring &wideStr) {
  if (wideStr.empty()) return "";
  int size_needed = WideCharToMultiByte(
      CP_UTF8, 0, wideStr.c_str(), (int)wideStr.size(), NULL, 0, NULL, NULL);
  if (size_needed == 0) return "";
  std::string narrowStr(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), (int)wideStr.size(),
                      &narrowStr[0], size_needed, NULL, NULL);
  return narrowStr;
}

// ---------------------------------------------------------------------------
// Process Isolation Implementation
// ---------------------------------------------------------------------------

bool SetDisableProcessIsolation() {
  std::wstring keyPath = L"CLSID\\";
  keyPath += SZ_CLSID_TOONZPREVIEW;
  // Correction: Do not append "\\InprocServer32".
  // DisableProcessIsolation is a direct property of CLSID, not the DLL.

  HKEY hKey = NULL;
  // Attempt to open the CLSID\{GUID} key
  LSTATUS status = RegOpenKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0,
                                 KEY_SET_VALUE, &hKey);

  if (status != ERROR_SUCCESS) {
    DebugLog("Failed to open CLSID key for DisableProcessIsolation.");
    return false;
  }

  DWORD dwData = 1;
  // Set the DisableProcessIsolation key value (DWORD = 1)
  status = RegSetValueExW(hKey, L"DisableProcessIsolation", 0, REG_DWORD,
                          (const BYTE *)&dwData, sizeof(dwData));

  RegCloseKey(hKey);

  if (status != ERROR_SUCCESS) {
    DebugLog("Failed to set DisableProcessIsolation value.");
    return false;
  }

  DebugLog("Successfully set DisableProcessIsolation=1.");
  return true;
}

/**
 * @brief Clears the DisableProcessIsolation value from the registry.
 */
bool ClearDisableProcessIsolation() {
  std::wstring keyPath = L"CLSID\\";
  keyPath += SZ_CLSID_TOONZPREVIEW;
  // Correction: Do not append "\\InprocServer32".

  HKEY hKey      = NULL;
  LSTATUS status = RegOpenKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0,
                                 KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);

  if (status != ERROR_SUCCESS) {
    // If the key itself doesn't exist, no cleanup is needed, return true
    return true;
  }

  // Delete the DisableProcessIsolation value
  status = RegDeleteValueW(hKey, L"DisableProcessIsolation");

  RegCloseKey(hKey);

  // ERROR_FILE_NOT_FOUND means the value did not exist in the first place,
  // which is also a success
  if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
    DebugLog("Failed to delete DisableProcessIsolation value.");
    return false;
  }

  DebugLog("Successfully cleared DisableProcessIsolation.");
  return true;
}

void EnsureQtApplication() {
  static std::once_flag qtInitFlag;
  std::call_once(qtInitFlag, []() {
    if (QCoreApplication::instance()) {
      DebugLog("Qt application already exists");
    } else {
      DebugLog("Starting Qt application initialization...");

      // Get the path of our DLL
      WCHAR dllPath[MAX_PATH];
      if (GetModuleFileNameW(g_hInst, dllPath, MAX_PATH)) {
        DebugLog("DLL path: " + WideToNarrow(dllPath));

        // Get the DLL's directory (e.g., E:\)
        WCHAR dllDir[MAX_PATH];
        wcscpy_s(dllDir, MAX_PATH, dllPath);
        PathRemoveFileSpecW(dllDir);
        std::string dllDirNarrow = WideToNarrow(dllDir);
        DebugLog("DLL directory: " + dllDirNarrow);

        QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);

        DebugLog("Creating QCoreApplication...");

        try {
          static int argc     = 1;
          static char *argv[] = {(char *)"toonzpreview_host", nullptr};
          static QCoreApplication app(argc, argv);

          QCoreApplication::setApplicationName("ToonzPreview");
          QCoreApplication::setOrganizationName("OpenToonz");

          DebugLog("QCoreApplication created successfully");

          QString baseDir = QString::fromStdWString(dllDir);

          QCoreApplication::addLibraryPath(baseDir);
          DebugLog("Added base directory to library paths: " +
                   baseDir.toStdString());

          QStringList pluginSubdirs = {
              "audio",            // qtaudio_windows.dll
              "bearer",           // qgenericbearer.dll
              "iconengines",      // qsvgicon.dll
              "imageformats",     // qjpeg.dll, qsvg.dll..
              "mediaservice",     // dsengine.dll..
              "platforms",        // qwindows.dll
              "playlistformats",  // qtmultimedia_m3u.dll
              "printsupport",     // windowsprintersupport.dll
              "styles"            // qwindowsvistastyle.dll
          };

          for (const QString &subdir : pluginSubdirs) {
            QString fullPath = baseDir + "/" + subdir;
            if (QDir(fullPath).exists()) {
              QCoreApplication::addLibraryPath(fullPath);
              DebugLog("Added plugin directory: " + fullPath.toStdString());
            } else {
              DebugLog("WARNING: Plugin directory not found: " +
                       fullPath.toStdString());
            }
          }

          QStringList libraryPaths = QCoreApplication::libraryPaths();
          DebugLog("Final Qt library paths (" +
                   QString::number(libraryPaths.size()).toStdString() + "):");
          for (const QString &path : libraryPaths) {
            DebugLog("  " + path.toStdString());
          }

        } catch (const std::exception &e) {
          DebugLog("Exception during QCoreApplication creation: " +
                   std::string(e.what()));
          return;
        } catch (...) {
          DebugLog("Unknown exception during QCoreApplication creation");
          return;
        }

      } else {
        DebugLog("Error: Could not get DLL path.");
      }
      DebugLog(
          "Qt application initialization completed successfully without "
          "environment variables");
    }
  });
}
HBITMAP RasterToHBITMAP(const TRaster32P &ras) {
  if (!ras || !ras->getRawData()) {
    DebugLog("RasterToHBITMAP: Invalid raster data");
    return NULL;
  }

  int w = ras->getLx();
  int h = ras->getLy();

  if (w <= 0 || h <= 0) return NULL;

  BITMAPINFO bmi              = {0};
  bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth       = w;
  bmi.bmiHeader.biHeight      = h;
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biBitCount    = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage   = w * h * 4;

  void *pBits  = NULL;
  HDC hdc      = GetDC(NULL);
  HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
  ReleaseDC(NULL, hdc);

  if (!hBmp || !pBits) return NULL;

  ras->lock();
  const unsigned char *srcPixels = ras->getRawData();
  if (srcPixels) {
    memcpy(pBits, srcPixels, w * h * 4);
  } else {
    DeleteObject(hBmp);
    hBmp = NULL;
  }
  ras->unlock();
  return hBmp;
}

void SetupDependencyPath(HMODULE hModule) {
  WCHAR dllPath[MAX_PATH];
  if (GetModuleFileNameW(hModule, dllPath, MAX_PATH)) {
    PathRemoveFileSpecW(dllPath);
    SetDllDirectoryW(dllPath);
  }
}
void LoadDependencies() {
  static HMODULE hToonzQt = NULL;
  static HMODULE hTnzCore = NULL;
  static HMODULE hImage   = NULL;

  // Keep these DLLs resident in memory
  WCHAR dllDir[MAX_PATH];
  GetModuleFileNameW(g_hInst, dllDir, MAX_PATH);
  PathRemoveFileSpecW(dllDir);

  auto LoadDep = [&](const wchar_t *name) -> HMODULE {
    WCHAR path[MAX_PATH];
    wcscpy_s(path, dllDir);
    wcscat_s(path, L"\\");
    wcscat_s(path, name);
    return LoadLibraryW(path);
  };

  hToonzQt = LoadDep(L"toonzqt.dll");
  hTnzCore = LoadDep(L"tnzcore.dll");
  hImage   = LoadDep(L"image.dll");
}
// ---------------------------------------------------------------------------
// DLL Entry Point
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  static HMODULE hToonzQt = NULL;
  static HMODULE hTnzCore = NULL;
  static HMODULE hImage   = NULL;

  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH: {
    g_hInst = hModule;

    SetupDependencyPath(hModule);

    DisableThreadLibraryCalls(hModule);
    DebugLog("DLL_PROCESS_ATTACH - Path Set & ToonzPreview loaded");
    break;
  }
  case DLL_PROCESS_DETACH:

    DebugLog("DLL_PROCESS_DETACH");
    break;
  }
  return TRUE;
}

// ---------------------------------------------------------------------------
// DLL Exports
// ---------------------------------------------------------------------------
STDAPI DllCanUnloadNow() { return (g_cDllRef > 0) ? S_FALSE : S_OK; }

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
  DebugLog("DllGetClassObject called");
  if (!IsEqualCLSID(rclsid, CLSID_ToonzThumbnailProvider)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }
  CClassFactory *pFactory = new CClassFactory();
  if (!pFactory) return E_OUTOFMEMORY;

  HRESULT hr = pFactory->QueryInterface(riid, ppv);
  pFactory->Release();
  return hr;
}

HRESULT RegisterRegistryKeys() {
  WCHAR modulePath[MAX_PATH];
  if (!GetModuleFileNameW(g_hInst, modulePath, MAX_PATH)) return E_FAIL;

  // Key: HKLM\SOFTWARE\Classes\CLSID\{SZ_CLSID_TOONZPREVIEW}
  std::wstring clsidKey =
      std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + SZ_CLSID_TOONZPREVIEW;
  HKEY hKey;
  LONG lResult =
      RegCreateKeyExW(HKEY_LOCAL_MACHINE, clsidKey.c_str(), 0, NULL,
                      REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
  if (lResult == ERROR_SUCCESS) {
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)L"Toonz Thumbnail Provider",
                   (wcslen(L"Toonz Thumbnail Provider") + 1) * sizeof(WCHAR));

    // AppID
    RegSetValueExW(hKey, L"AppID", 0, REG_SZ, (BYTE *)SZ_CLSID_TOONZPREVIEW,
                   (wcslen(SZ_CLSID_TOONZPREVIEW) + 1) * sizeof(WCHAR));

    HKEY hInProc;
    if (RegCreateKeyExW(hKey, L"InProcServer32", 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hInProc,
                        NULL) == ERROR_SUCCESS) {
      RegSetValueExW(hInProc, NULL, 0, REG_SZ, (BYTE *)modulePath,
                     (wcslen(modulePath) + 1) * sizeof(WCHAR));
      RegSetValueExW(hInProc, L"ThreadingModel", 0, REG_SZ,
                     (BYTE *)L"Apartment",
                     (wcslen(L"Apartment") + 1) * sizeof(WCHAR));
      RegCloseKey(hInProc);
    }
    RegCloseKey(hKey);
  }

  // Key: HKLM\SOFTWARE\Classes\{progId}
  std::wstring typeKey = std::wstring(L"SOFTWARE\\Classes\\") + progId;
  if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, typeKey.c_str(), 0, NULL,
                      REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)progId,
                   (wcslen(progId) + 1) * sizeof(WCHAR));
    RegCloseKey(hKey);

    bool installTGA = true;
    for (int i = 0; i < EXT_COUNT; i++) {
      if (wcscmp(exts[i], L".tga") == 0) {
        int result = MessageBoxW(
            NULL,
            L"Do you want to install thumbnail handler for .tga file type?",
            L"Confirm Installation", MB_YESNO | MB_ICONQUESTION);

        if (result != IDYES) {
          installTGA = false;
          DebugLog("User canceled TGA file type registration");
          continue;
        }
      }
    }

    // Key: HKLM\SOFTWARE\Classes\{progId}\ShellEx\{SZ_ITHUMBNAILPROVIDER_IID}
    std::wstring shellExKey = std::wstring(L"SOFTWARE\\Classes\\") + progId +
                              L"\\ShellEx\\" + SZ_ITHUMBNAILPROVIDER_IID;

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, shellExKey.c_str(), 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey,
                        NULL) == ERROR_SUCCESS) {
      RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)SZ_CLSID_TOONZPREVIEW,
                     (wcslen(SZ_CLSID_TOONZPREVIEW) + 1) * sizeof(WCHAR));
      RegCloseKey(hKey);
      DebugLog("Registered thumbnail handler for " +
               WideToNarrow(exts[EXT_COUNT - 1]));
    }

    for (int i = 0; i < EXT_COUNT; i++) {
      if (wcscmp(exts[i], L".tga") == 0 && !installTGA) {
        continue;
      }

      // Key:
      // HKLM\SOFTWARE\Classes\SystemFileAssociations\{exts[i]}\ShellEx\{SZ_ITHUMBNAILPROVIDER_IID}
      std::wstring sysKeyPath =
          std::wstring(L"SOFTWARE\\Classes\\SystemFileAssociations\\") +
          exts[i] + L"\\ShellEx\\" + SZ_ITHUMBNAILPROVIDER_IID;

      if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, sysKeyPath.c_str(), 0, NULL,
                          REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey,
                          NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)SZ_CLSID_TOONZPREVIEW,
                       (wcslen(SZ_CLSID_TOONZPREVIEW) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
        DebugLog("Registered SystemFileAssociations for " +
                 WideToNarrow(exts[i]));
      }
    }
  }

  // Key: HKLM\SOFTWARE\Classes\.tpl\DefaultIcon
  std::wstring tplIconKey = L"SOFTWARE\\Classes\\.tpl\\DefaultIcon";
  if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, tplIconKey.c_str(), 0, NULL,
                      REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    TFilePath paletteIconPath = getPaletteIconPath();
    std::wstring iconPathW    = paletteIconPath.getWideString();

    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)iconPathW.c_str(),
                   (iconPathW.length() + 1) * sizeof(WCHAR));

    RegCloseKey(hKey);
    DebugLog("Registered DefaultIcon for .tpl: " + WideToNarrow(iconPathW));
  }

  return S_OK;
}

STDAPI DllRegisterServer() {
  DebugLog("DllRegisterServer called");
  HRESULT hr = RegisterRegistryKeys();

  // Critical Step: After successful COM registration, set to disable process
  // isolation
  if (SUCCEEDED(hr)) {
    if (!SetDisableProcessIsolation()) {
      DebugLog(
          "WARNING: Failed to set DisableProcessIsolation=1. "
          "IInitializeWithFile may not be called.");
      // Do not return E_FAIL, allow registration to continue but log a warning
    }
  }

  SHChangeNotify(SHCNE_UPDATEIMAGE, SHCNF_DWORD | SHCNF_FLUSH, NULL, NULL);
  system("ie4uinit.exe -show");
  return hr;
}

STDAPI DllUnregisterServer() {
  DebugLog("DllUnregisterServer called");

  ClearDisableProcessIsolation();

  std::wstring clsidKey =
      std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + SZ_CLSID_TOONZPREVIEW;
  HKEY hTestKey;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, clsidKey.c_str(), 0, KEY_READ,
                    &hTestKey) == ERROR_SUCCESS) {
    RegCloseKey(hTestKey);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKey.c_str());
  }

  std::wstring shellExKey = std::wstring(L"SOFTWARE\\Classes\\") + progId +
                            L"\\ShellEx\\" + SZ_ITHUMBNAILPROVIDER_IID;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, shellExKey.c_str(), 0, KEY_READ,
                    &hTestKey) == ERROR_SUCCESS) {
    RegCloseKey(hTestKey);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, shellExKey.c_str());
  }

  std::wstring progIDPath = L"SOFTWARE\\Classes\\" + std::wstring(progId);
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, progIDPath.c_str(), 0, KEY_READ,
                    &hTestKey) == ERROR_SUCCESS) {
    RegCloseKey(hTestKey);
    WCHAR existingValue[100];
    DWORD dataSize = sizeof(existingValue);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, progIDPath.c_str(), NULL,
                     RRF_RT_REG_SZ, NULL, existingValue,
                     &dataSize) == ERROR_SUCCESS) {
      if (wcscmp(existingValue, progId) == 0) {
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, progIDPath.c_str());
      }
    }
  }

  for (int i = 0; i < EXT_COUNT; i++) {
    std::wstring sysKeyPath =
        std::wstring(L"SOFTWARE\\Classes\\SystemFileAssociations\\") + exts[i] +
        L"\\ShellEx\\" + SZ_ITHUMBNAILPROVIDER_IID;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sysKeyPath.c_str(), 0, KEY_READ,
                      &hTestKey) == ERROR_SUCCESS) {
      RegCloseKey(hTestKey);
      WCHAR existingClsid[100];
      DWORD dataSize = sizeof(existingClsid);
      if (RegGetValueW(HKEY_LOCAL_MACHINE, sysKeyPath.c_str(), NULL,
                       RRF_RT_REG_SZ, NULL, existingClsid,
                       &dataSize) == ERROR_SUCCESS) {
        if (wcscmp(existingClsid, SZ_CLSID_TOONZPREVIEW) == 0) {
          RegDeleteKeyW(HKEY_LOCAL_MACHINE, sysKeyPath.c_str());
        }
      }
    }

    std::wstring extPath = L"SOFTWARE\\Classes\\" + std::wstring(exts[i]);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, extPath.c_str(), 0, KEY_READ,
                      &hTestKey) == ERROR_SUCCESS) {
      RegCloseKey(hTestKey);
      WCHAR existingValue[100];
      DWORD dataSize = sizeof(existingValue);
      if (RegGetValueW(HKEY_LOCAL_MACHINE, extPath.c_str(), NULL, RRF_RT_REG_SZ,
                       NULL, existingValue, &dataSize) == ERROR_SUCCESS) {
        if (wcscmp(existingValue, progId) == 0) {
          RegDeleteKeyW(HKEY_LOCAL_MACHINE, extPath.c_str());
        }
      }
    }
  }

  std::wstring tplIconKey = L"SOFTWARE\\Classes\\.tpl\\DefaultIcon";
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, tplIconKey.c_str(), 0, KEY_READ,
                    &hTestKey) == ERROR_SUCCESS) {
    RegCloseKey(hTestKey);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, tplIconKey.c_str());
  }

  SHChangeNotify(SHCNE_UPDATEIMAGE, SHCNF_DWORD | SHCNF_FLUSH, NULL, NULL);
  system("ie4uinit.exe -show");

  return S_OK;
}

// ---------------------------------------------------------------------------
// CToonzThumbnailProvider Implementation
// ---------------------------------------------------------------------------
CToonzThumbnailProvider::CToonzThumbnailProvider() : m_cRef(1) {
  InterlockedIncrement(&g_cDllRef);
  DebugLog("CToonzThumbnailProvider constructor called");
}

CToonzThumbnailProvider::~CToonzThumbnailProvider() {
  InterlockedDecrement(&g_cDllRef);
  DebugLog("CToonzThumbnailProvider destructor called");
}

IFACEMETHODIMP CToonzThumbnailProvider::QueryInterface(REFIID riid,
                                                       void **ppv) {
  if (!ppv) return E_INVALIDARG;
  *ppv = NULL;

  DebugLog("=== QueryInterface Start ===");

  LPOLESTR riidStr = NULL;
  StringFromIID(riid, &riidStr);
  std::wstring wriid(riidStr);
  DebugLog("Requested IID: " + WideToNarrow(wriid));
  CoTaskMemFree(riidStr);

  // Completely manual interface query
  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_IThumbnailProvider) ||
      IsEqualIID(riid, IID_IInitializeWithFile) ||
      IsEqualIID(riid, IID_IPersistFile) || IsEqualIID(riid, IID_IPersist)) {
    //|| IsEqualIID(riid, IID_IInitializeWithStream)) {
    // Return the correct pointer based on the requested interface
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IThumbnailProvider)) {
      DebugLog("Returning IThumbnailProvider interface");
      *ppv = static_cast<IThumbnailProvider *>(this);
    } else if (IsEqualIID(riid, IID_IInitializeWithFile)) {
      DebugLog("Returning IInitializeWithFile interface");
      *ppv = static_cast<IInitializeWithFile *>(this);
    } else if (IsEqualIID(riid, IID_IPersistFile)) {
      DebugLog("Returning IPersistFile interface");
      *ppv = static_cast<IPersistFile *>(this);
    } else if (IsEqualIID(riid, IID_IPersist)) {
      DebugLog("Returning IPersist interface");
      *ppv = static_cast<IPersist *>(this);
    } else if (IsEqualIID(riid, IID_IInitializeWithStream)) {
      DebugLog("Returning IInitializeWithStream interface");
      *ppv = static_cast<IInitializeWithStream *>(this);
    }

    if (*ppv) {
      AddRef();
      DebugLog("QueryInterface SUCCEEDED");
      DebugLog("=== QueryInterface End ===");
      return S_OK;
    }
  }

  DebugLog("Interface not supported");
  DebugLog("=== QueryInterface End ===");
  return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) CToonzThumbnailProvider::AddRef() {
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) CToonzThumbnailProvider::Release() {
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (0 == cRef) delete this;
  return cRef;
}

IFACEMETHODIMP CToonzThumbnailProvider::Initialize(LPCWSTR pszFilePath,
                                                   DWORD grfMode) {
  DebugLog("IInitializeWithFile::Initialize called");

  if (!pszFilePath) {
    DebugLog("IInitializeWithFile::Initialize: pszFilePath is NULL");
    return E_INVALIDARG;
  }

  m_filePath = pszFilePath;
  DebugLog("IInitializeWithFile::Initialize: File path set to: " +
           WideToNarrow(m_filePath));
  DebugLog("IInitializeWithFile::Initialize: Mode: " + std::to_string(grfMode));

  return S_OK;
}

/**
 * @brief Attempts to extract the full path of the file from an IStream object.
 * * This is a standard method to get the source file path in
 * IInitializeWithStream::Initialize. It first tries IStream::Stat, then
 * attempts the IPropertyStore interface.
 *
 * @param pStream The input IStream pointer.
 * @return The extracted full file path; an empty string if unsuccessful.
 */
std::wstring GetFullPathFromStream(IStream *pStream) {
  if (!pStream) {
    DebugLog("GetFullPathFromStream: pStream is NULL.");
    return L"";
  }

  std::wstring filePath;

  // --- Method 1: Try to get path from STATSTG ---
  STATSTG stat = {0};
  HRESULT hr   = pStream->Stat(&stat, STATFLAG_DEFAULT);

  if (SUCCEEDED(hr) && stat.pwcsName) {
    filePath = stat.pwcsName;
    DebugLog("Method 1: Got name from STATSTG: " + WideToNarrow(filePath));

    // Check if it's a full path (contains drive letter ':' or starts with root
    // '\')
    bool isFullPath = (filePath.find(L':') != std::wstring::npos) ||
                      (filePath.find(L'\\') == 0);

    // If it's a full path, success, free memory and return
    if (isFullPath) {
      DebugLog("STATSTG path appears complete.");
      CoTaskMemFree(stat.pwcsName);
      return filePath;
    }

    // If it's not a full path (only filename), proceed to Method 2
    DebugLog("STATSTG path is incomplete, attempting Method 2...");
    CoTaskMemFree(stat.pwcsName);  // Free memory allocated for stat.pwcsName
  } else {
    DebugLog(
        "Method 1: Could not get STATSTG information or pwcsName is NULL.");
  }

  // --- Method 2: Try to get PKEY_ItemPathDisplay via IPropertyStore ---
  IPropertyStore *pPropertyStore = nullptr;
  HRESULT propHr = pStream->QueryInterface(IID_PPV_ARGS(&pPropertyStore));

  if (SUCCEEDED(propHr) && pPropertyStore) {
    PROPVARIANT propVar;
    PropVariantInit(&propVar);

    // Get the full file path property
    propHr = pPropertyStore->GetValue(PKEY_ItemPathDisplay, &propVar);

    if (SUCCEEDED(propHr) && propVar.vt == VT_LPWSTR && propVar.pwszVal) {
      filePath = propVar.pwszVal;
      DebugLog("Method 2: Got full path from IPropertyStore: " +
               WideToNarrow(filePath));
    } else {
      DebugLog(
          "Method 2: Failed to get PKEY_ItemPathDisplay or property type is "
          "wrong.");
    }

    PropVariantClear(&propVar);
    pPropertyStore->Release();
  } else {
    DebugLog("GetFullPathFromStream: Failed to query IPropertyStore.");
  }

  return filePath;
}

IFACEMETHODIMP CToonzThumbnailProvider::Initialize(IStream *pStream,
                                                   DWORD grfMode) {
  DebugLog("IInitializeWithStream::Initialize called");

  if (!pStream) {
    DebugLog("IInitializeWithStream::Initialize: pStream is NULL");
    return E_INVALIDARG;
  }

  // Method 1: Try to get the full path from STATSTG
  STATSTG stat;
  HRESULT hr = pStream->Stat(&stat, STATFLAG_DEFAULT);
  m_filePath = GetFullPathFromStream(pStream);

  // If still no full path, log a warning
  if (m_filePath.find(L':') == std::wstring::npos &&
      m_filePath.find(L'\\') != 0) {
    DebugLog(
        "IInitializeWithStream::Initialize: WARNING - Only have filename, "
        "palette files may not be found: " +
        WideToNarrow(m_filePath));
  }

  DebugLog("IInitializeWithStream::Initialize: Mode: " +
           std::to_string(grfMode));
  return S_OK;
}
static int g_requestCount         = 0;
static const int CLEANUP_INTERVAL = 10;  // Clean up every 10 requests
IFACEMETHODIMP CToonzThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp,
                                                     WTS_ALPHATYPE *pdwAlpha) {
  DebugLog("GetThumbnail called size=" + std::to_string(cx));

  g_requestCount++;
  if (g_requestCount >= CLEANUP_INTERVAL) {
    DebugLog("Performing periodic cache cleanup - request count: " +
             std::to_string(g_requestCount));
    TImageCache::instance()->clear();
    g_requestCount = 0;  // Reset count
  }

  if (!phbmp || !pdwAlpha) {
    DebugLog("GetThumbnail: Invalid arguments");
    return E_INVALIDARG;
  }

  *phbmp    = NULL;
  *pdwAlpha = WTSAT_UNKNOWN;

  if (m_filePath.empty()) {
    DebugLog("GetThumbnail: No file path set");
    return E_FAIL;
  }

  DebugLog("GetThumbnail: Processing file: " + WideToNarrow(m_filePath));

  try {
    // 1. Initialize Qt Environment (Required for TOfflineGL used by
    // IconGenerator)
    EnsureQtApplication();

    static std::once_flag ioInitFlag;
    std::call_once(ioInitFlag, []() {
      LoadDependencies();
      Tiio::defineStd();
      initImageIo();
      TThread::init();
      DebugLog("Tiio and ImageIo initialized successfully.");
    });

    TFilePath path(m_filePath);
    TDimension size(cx, cx);

    // 2. Normalize extension to lowercase for comparison
    std::string type = path.getType();
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);

    DebugLog("GetThumbnail: File type: " + type);

    TRaster32P ras;
    static TDimension oldSize(0, 0);
    if (size.lx > oldSize.lx || size.ly > oldSize.ly) {
      IconGenerator::setFilmstripIconSize(size);
      oldSize = size;
    }

    // 3. Route to the specific static generator based on file type
    if (type == "tnz") {
      DebugLog("GetThumbnail: Generating Scene (TNZ) icon");
      ras = IconGenerator::generateSceneFileIcon(path, size, -5);
    } else if (type == "pli") {
      DebugLog("GetThumbnail: Generating Vector Level (PLI) icon");
      // NO_FRAME lets the reader pick the first frame in the level
      ras =
          IconGenerator::generateVectorFileIcon(path, size, TFrameId::NO_FRAME);
    } else if (type == "tlv" || type == "tga") {
      DebugLog("GetThumbnail: Generating Raster Level (TLV/TGA) icon");
      if (type == "tlv")
        ras = IconGenerator::generateRasterFileIcon(path, size,
                                                    TFrameId::NO_FRAME);
      else
        ras = IconGenerator::generateRasterFileIcon(path, size,
                                                    path.analyzePath().fId);
    } else if (type == "tpl") {
      DebugLog(
          "GetThumbnail: TPL generation not fully implemented for Shell "
          "Preview.");
    } else {
      DebugLog("GetThumbnail: Unsupported file type: " + type);
    }

    // 4. Validate Result
    if (!ras.getPointer() || !ras->getRawData()) {
      // TODO : return image broken notify infomation
      DebugLog("GetThumbnail: No raster generated or raster is empty");
      return E_FAIL;
    }

    DebugLog(
        "GetThumbnail: Raster generated successfully, converting to HBITMAP");

    // 5. Convert Toonz Raster to Windows HBITMAP
    *phbmp = RasterToHBITMAP(ras);

    if (*phbmp) {
      *pdwAlpha = WTSAT_ARGB;  // Indicate the bitmap has an alpha channel
      DebugLog("GetThumbnail: Success - HBITMAP created");
      return S_OK;
    } else {
      DebugLog("GetThumbnail: Failed to create HBITMAP from Raster");
    }

  } catch (const std::exception &e) {
    DebugLog("GetThumbnail: Exception: " + std::string(e.what()));
  } catch (...) {
    DebugLog("GetThumbnail: Unknown exception");
  }

  return E_FAIL;
}

IFACEMETHODIMP CToonzThumbnailProvider::GetClassID(CLSID *pClassID) {
  DebugLog("IPersist::GetClassID called");
  if (!pClassID) return E_INVALIDARG;
  *pClassID = CLSID_ToonzThumbnailProvider;
  return S_OK;
}

// IPersistFile
IFACEMETHODIMP CToonzThumbnailProvider::IsDirty() { return S_FALSE; }
IFACEMETHODIMP CToonzThumbnailProvider::Save(LPCWSTR pszFileName,
                                             BOOL fRemember) {
  return E_NOTIMPL;
}
IFACEMETHODIMP CToonzThumbnailProvider::SaveCompleted(LPCWSTR pszFileName) {
  return E_NOTIMPL;
}
IFACEMETHODIMP CToonzThumbnailProvider::GetCurFile(LPWSTR *ppszFileName) {
  return E_NOTIMPL;
}

IFACEMETHODIMP CToonzThumbnailProvider::Load(LPCWSTR pszFileName,
                                             DWORD dwMode) {
  DebugLog("IPersistFile::Load called");

  if (!pszFileName) {
    DebugLog("IPersistFile::Load: pszFileName is NULL");
    return E_INVALIDARG;
  }

  m_filePath = pszFileName;
  DebugLog("IPersistFile::Load: File path set to: " + WideToNarrow(m_filePath));
  DebugLog("IPersistFile::Load: Mode: " + std::to_string(dwMode));

  return S_OK;
}

// ---------------------------------------------------------------------------
// CClassFactory Implementation
// ---------------------------------------------------------------------------
CClassFactory::CClassFactory() : m_cRef(1) { InterlockedIncrement(&g_cDllRef); }

CClassFactory::~CClassFactory() { InterlockedDecrement(&g_cDllRef); }

IFACEMETHODIMP CClassFactory::QueryInterface(REFIID riid, void **ppv) {
  if (!ppv) return E_INVALIDARG;
  *ppv = NULL;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
    *ppv = static_cast<IClassFactory *>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) CClassFactory::AddRef() {
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) CClassFactory::Release() {
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (0 == cRef) delete this;
  return cRef;
}

IFACEMETHODIMP CClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid,
                                             void **ppv) {
  if (pUnkOuter) return CLASS_E_NOAGGREGATION;

  CToonzThumbnailProvider *pObj = new CToonzThumbnailProvider();
  if (!pObj) return E_OUTOFMEMORY;

  HRESULT hr = pObj->QueryInterface(riid, ppv);
  pObj->Release();
  return hr;
}

IFACEMETHODIMP CClassFactory::LockServer(BOOL fLock) {
  if (fLock)
    InterlockedIncrement(&g_cDllRef);
  else
    InterlockedDecrement(&g_cDllRef);
  return S_OK;
}

STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine) {
  if (bInstall) {
    DllRegisterServer();
  } else {
    DllUnregisterServer();
  }
  return S_OK;
}