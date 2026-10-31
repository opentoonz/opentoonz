#ifdef _WIN32

#include "tscannerwia.h"

#include <windows.h>
#include <wia.h>
#include <sti.h>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

namespace {

class ScopedCom {
  HRESULT m_hr;
  bool m_uninit;

public:
  ScopedCom()
      : m_hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))
      , m_uninit(SUCCEEDED(m_hr)) {}
  ~ScopedCom() {
    if (m_uninit) CoUninitialize();
  }
  bool isReady() const { return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE; }
};

class ScopedBusy {
  bool &m_busy;

public:
  explicit ScopedBusy(bool &busy) : m_busy(busy) { m_busy = true; }
  ~ScopedBusy() { m_busy = false; }
};

IWiaDevMgr2 *createWiaManager() {
  IWiaDevMgr2 *manager = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WiaDevMgr2, nullptr, CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(&manager));
  return SUCCEEDED(hr) ? manager : nullptr;
}

TRasterImageP loadRasterImage(const QString &path) {
  QImage image(path);
  if (image.isNull()) return TRasterImageP();

  image = image.convertToFormat(QImage::Format_ARGB32);
  TRaster32P raster(image.width(), image.height());
  raster->lock();
  for (int y = 0; y < image.height(); ++y) {
    const QRgb *src = reinterpret_cast<const QRgb *>(image.constScanLine(y));
    TPixel32 *dst   = raster->pixels(image.height() - 1 - y);
    for (int x = 0; x < image.width(); ++x)
      dst[x] =
          TPixel32(qRed(src[x]), qGreen(src[x]), qBlue(src[x]), qAlpha(src[x]));
  }
  raster->unlock();

  TRasterImageP result(raster);
  const double xdpi = image.dotsPerMeterX() * 0.0254;
  const double ydpi = image.dotsPerMeterY() * 0.0254;
  if (xdpi > 0.0 && ydpi > 0.0) result->setDpi(xdpi, ydpi);
  return result;
}

}  // namespace

TScannerWia::TScannerWia() : m_busy(false) {
  setName(QStringLiteral("Windows WIA"));
}

TScannerWia::~TScannerWia() = default;

bool TScannerWia::isDeviceAvailable() {
  ScopedCom com;
  if (!com.isReady()) return false;
  IWiaDevMgr2 *manager = createWiaManager();
  if (!manager) return false;
  manager->Release();
  return true;
}

bool TScannerWia::isDeviceSelected() { return !m_deviceId.empty(); }

void TScannerWia::selectDevice() {
  if (m_busy) return;
  ScopedBusy busy(m_busy);
  m_deviceId.clear();
  setName(QStringLiteral("Windows WIA"));

  ScopedCom com;
  if (!com.isReady()) return;

  IWiaDevMgr2 *manager = createWiaManager();
  if (!manager) return;

  BSTR deviceId = nullptr;
  HRESULT hr    = manager->SelectDeviceDlgID(
         nullptr, StiDeviceTypeScanner, WIA_SELECT_DEVICE_NODEFAULT, &deviceId);
  if (SUCCEEDED(hr) && deviceId) {
    try {
      m_deviceId.assign(deviceId, SysStringLen(deviceId));
      setName(QString::fromWCharArray(deviceId));
    } catch (...) {
      SysFreeString(deviceId);
      manager->Release();
      throw;
    }
  }

  if (deviceId) SysFreeString(deviceId);
  manager->Release();
}

void TScannerWia::updateParameters(TScannerParameters &parameters) {
  // The first WIA implementation intentionally lets the Windows/vendor dialog
  // own scanner settings. Keep OpenToonz scan controls non-authoritative until
  // acquisition is proven across real WIA devices.
  parameters.setSupportedTypes(false, false, true);
  parameters.setMaxPaperSize(1000.0, 1000.0);
}

void TScannerWia::acquire(const TScannerParameters &, int paperCount) {
  if (m_busy || m_deviceId.empty()) {
    notifyError();
    return;
  }
  ScopedBusy busy(m_busy);

  ScopedCom com;
  if (!com.isReady()) {
    notifyError();
    return;
  }

  QTemporaryDir tempDir(QDir::tempPath() +
                        QStringLiteral("/opentoonz-wia-XXXXXX"));
  if (!tempDir.isValid()) {
    notifyError();
    return;
  }

  const QString tempPath = tempDir.path();
  IWiaDevMgr2 *manager   = createWiaManager();
  if (!manager) {
    notifyError();
    return;
  }

  BSTR deviceId = SysAllocString(m_deviceId.c_str());
  BSTR folder =
      SysAllocString(reinterpret_cast<const wchar_t *>(tempPath.utf16()));
  BSTR filename   = SysAllocString(L"scan");
  LONG fileCount  = 0;
  BSTR *filePaths = nullptr;
  IWiaItem2 *item = nullptr;

  auto cleanup = [&]() {
    if (item) {
      item->Release();
      item = nullptr;
    }
    if (deviceId) {
      SysFreeString(deviceId);
      deviceId = nullptr;
    }
    if (folder) {
      SysFreeString(folder);
      folder = nullptr;
    }
    if (filename) {
      SysFreeString(filename);
      filename = nullptr;
    }
    if (filePaths) {
      for (LONG i = 0; i < fileCount; ++i) {
        if (!filePaths[i]) continue;
        QFile::remove(QString::fromWCharArray(filePaths[i]));
        SysFreeString(filePaths[i]);
        filePaths[i] = nullptr;
      }
      CoTaskMemFree(filePaths);
      filePaths = nullptr;
    }
    manager->Release();
    manager = nullptr;
  };

  if (!deviceId || !folder || !filename) {
    cleanup();
    notifyError();
    return;
  }

  HRESULT hr =
      manager->GetImageDlg(WIA_DEVICE_DIALOG_USE_COMMON_UI, deviceId, nullptr,
                           folder, filename, &fileCount, &filePaths, &item);

  bool delivered = false;
  bool canceled  = false;
  try {
    if (SUCCEEDED(hr) && fileCount > 0 && filePaths) {
      setPaperLeftCount(paperCount > 0 ? paperCount : fileCount);
      for (LONG i = 0; i < fileCount && getPaperLeftCount() > 0; ++i) {
        if (isScanningCanceled()) {
          canceled = true;
          break;
        }
        if (!filePaths[i]) continue;
        QString path        = QString::fromWCharArray(filePaths[i]);
        TRasterImageP image = loadRasterImage(path);
        if (image) {
          notifyImageDone(image);
          delivered = true;
          if (getPaperLeftCount() > 0) decrementPaperLeftCount();
        }
      }
    }
  } catch (...) {
    cleanup();
    throw;
  }

  cleanup();
  if (FAILED(hr) || (!delivered && !canceled && hr != S_FALSE)) notifyError();
}

#endif
