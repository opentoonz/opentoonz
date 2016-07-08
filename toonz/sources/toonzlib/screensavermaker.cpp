#include "toonz/screensavermaker.h"

#include "texception.h"
#include "tsystem.h"

#include <memory>

#ifdef _WIN32
#pragma warning(disable : 4996)

#include <windows.h>
#include <shellapi.h>
//#include <iostream.h>
#include <sys/stat.h>
#include <stdio.h>

#include <QUrl>
#include <QDesktopServices>

void makeScreenSaver(TFilePath scrFn, TFilePath swfFn,
                     std::string screenSaverName) {
  struct _stat results;
  if (_wstat(swfFn.getWideString().c_str(), &results) != 0)
    throw TException(L"Can't stat file " + swfFn.getWideString());

  int swfSize = results.st_size;
  std::unique_ptr<char> swf(new char[swfSize]);
  FILE *chan = _wfopen(swfFn.getWideString().c_str(), L"rb");
  if (!chan) throw TException(L"fopen failed on " + swfFn.getWideString());
  fread(swf.get(), swfSize, 1, chan);
  fclose(chan);

  TFilePath svscrn = TSystem::getBinDir() + "screensaver.dat";
  if (!TFileStatus(svscrn).doesExist()) {
    throw TException(std::wstring(L"Screensaver template not found: ") +
                     svscrn.getWideString());
  }
  TSystem::copyFile(scrFn, svscrn);
  HANDLE hUpdateRes =
      BeginUpdateResourceW(scrFn.getWideString().c_str(), FALSE);
  if (hUpdateRes == NULL)
    throw TException(L"can't write " + scrFn.getWideString());

  BOOL result = UpdateResource(hUpdateRes, "FLASHFILE", MAKEINTRESOURCE(101),
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                               swf.get(), swfSize);
  if (result == FALSE)
    throw TException(L"can't add resource to " + scrFn.getWideString());
  /*
result = UpdateResource(
hUpdateRes,
RT_STRING,
MAKEINTRESOURCE(1),
MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
(void*)screenSaverName.c_str(),
screenSaverName.size());

if (result == FALSE)
throw TException(L"can't add name to "+scrFn.getWideString());
*/

  if (!EndUpdateResource(hUpdateRes, FALSE))
    throw TException(L"Couldn't write " + scrFn.getWideString());
}

void previewScreenSaver(TFilePath scr) {
  try {
    TSystem::showDocument(scr);
  } catch (...) {
  }
}

void installScreenSaver(TFilePath scr) {
  std::wstring cmd = L"desk.cpl,InstallScreenSaver " + scr.getWideString();
  int ret = (int)ShellExecuteW(0, L"open", L"rundll32.exe", cmd.c_str(), 0,
                               SW_SHOWNORMAL);
  if (ret <= 32) throw;
}

#else

void makeScreenSaver(TFilePath scrFn, TFilePath swfFn,
                     std::string screenSaverName) {}
/*
void previewScreenSaver(TFilePath scr)
{
}
*/

void installScreenSaver(TFilePath scr) {}

#endif
