

#include "casmsubmitpage.h"
#include "tfilepath.h"
#include "tconvert.h"
#include "tsystem.h"

#include "tw/mainshell.h"
#include "tw/textfield.h"
#include "tw/label.h"
#include "tw/optionmenu.h"
#include "tw/button.h"
#include "tw/checkbox.h"

#include "filebrowserpopup.h"
#include "casmconfigpanel.h"
#include "casmfileinfo.h"
#include "tfarmcontroller.h"
#include "application.h"
#include "util.h"

#include "tw/message.h"

//==============================================================================

class CasmSubmitPage::Data {
public:
  Data(CasmSubmitPage *cspage);
  ~Data() { delete m_casmTask; }

  void configureNotify(const TDimension &size);

  void browseCasmFiles();
  void submit();
  void loadCasm(const TFilePath &fp);

  CasmSubmitPage *m_cspage;

  TLabel *m_taskNameLabel;
  TTextField *m_taskNameTextField;
  TCheckBox *m_submitAsSuspended;
  TButton *m_submit;

  TLabel *m_filepathLabel;
  TTextField *m_filepathTextField;

  TButton *m_casmBrowseBtn;

  CasmConfigPanel *m_configPanel;

  CasmTask *m_casmTask;
};

//------------------------------------------------------------------------------

CasmSubmitPage::Data::Data(CasmSubmitPage *cspage)
    : m_cspage(cspage), m_casmTask(0) {
  m_taskNameLabel = new TLabel(cspage);
  m_taskNameLabel->setText("Task Name:");

  m_submitAsSuspended = new TCheckBox(cspage, "Submit as Suspended");

  m_taskNameTextField = new TTextField(cspage);

  m_filepathLabel = new TLabel(cspage);
  m_filepathLabel->setText("File Path:");

  m_filepathTextField = new TTextField(cspage);
  m_casmBrowseBtn     = new TButton(cspage);
  m_casmBrowseBtn->setTitle("...");
  tconnect(*m_casmBrowseBtn, this, &CasmSubmitPage::Data::browseCasmFiles);

  m_submit = new TButton(cspage);
  m_submit->setTitle("Submit");
  tconnect(*m_submit, this, &CasmSubmitPage::Data::submit);

  m_configPanel = new CasmConfigPanel(cspage);
}

//------------------------------------------------------------------------------

void CasmSubmitPage::Data::configureNotify(const TDimension &size) {
  int x0 = 20;
  int y  = size.ly - 1 - 10;
  int x  = x0;

  m_taskNameLabel->setGeometry(x, y - 15, x + 100, y);
  m_taskNameTextField->setGeometry(x + 104, y - 15, x + 250, y);
  x += 280;
  m_submitAsSuspended->setGeometry(x, y - 15, x + 120, y);
  x += 150;
  m_submit->setGeometry(x, y - 15, x + 80, y);
  x = x0;
  y -= 40;

  m_filepathLabel->setGeometry(x, y - 15, x + 100, y);
  m_filepathTextField->setGeometry(x + 104, y - 15, x + 650, y);
  m_casmBrowseBtn->setGeometry(x + 660, y - 15, x + 680, y);
  x = x0;
  y -= 40;

  m_configPanel->setGeometry(0, 0, size.lx - 1, y);
}

//------------------------------------------------------------------------------

void CasmSubmitPage::Data::browseCasmFiles() {
  static FileBrowserPopup *popup = 0;

  if (!popup) {
    vector<string> fileTypes;
    fileTypes.push_back("casm");
    popup = new FileBrowserPopup(m_cspage, fileTypes);

    popup->setOkAction(new TFileBrowserPopupAction<CasmSubmitPage::Data>(
        this, &CasmSubmitPage::Data::loadCasm));
  }

  if (!popup) return;

  TFilePath fp = Application::instance()->getCurrentFolder();
  if (fp != TFilePath()) popup->setCurrentDir(fp);

  TDimension d = TMainshell::getMainshell()->getSize();
#ifdef WIN32
  HDC hdc = GetDC(0);
  d.lx    = GetDeviceCaps(hdc, HORZRES);
  d.ly    = GetDeviceCaps(hdc, VERTRES);
  ReleaseDC(0, hdc);
#endif

  d -= popup->getSize();
  // TDimension d = TMainshell::getMainshell()->getSize() - popup->getSize();
  popup->popup(TPoint(d.lx / 2, d.ly / 2));
  popup->setCaption("Load Casm");
}

//------------------------------------------------------------------------------

void CasmSubmitPage::Data::submit() {
  if (m_filepathTextField->getText() == toWideString("")) {
    TMessage::error("You must load a file");
    return;
  }

  CasmTask *casm = m_configPanel->getTask();

  TFarmController *controller = Application::instance()->getController();

  string nativeCmdLine("runcasm ");
  nativeCmdLine += casm->m_casmFile;
  nativeCmdLine += " ";

  if (casm->m_setupFile != "") {
    nativeCmdLine += "-setup ";
    nativeCmdLine += casm->m_setupFile;
    nativeCmdLine += " ";
  }

  nativeCmdLine += casm->getCommandLine();

  string casmName = TFilePath(casm->m_casmFile).getName();

  int stepCount = casm->m_end - casm->m_start + 1;

  TFarmTaskGroup task(casmName, nativeCmdLine, TSystem::getUserName(),
                      TSystem::getHostName(), stepCount);

  int ra = casm->m_start;

  for (;;) {
    CasmTask subcasm(*casm);

    string cmdLine("runcasm ");
    int rb = tmin(ra + casm->m_taskChunksize - 1, casm->m_end);

    subcasm.m_start = ra;
    subcasm.m_end   = rb;

    cmdLine += subcasm.m_casmFile;
    cmdLine += " ";

    if (subcasm.m_setupFile != "") {
      cmdLine += "-setup ";
      cmdLine += subcasm.m_setupFile;
      cmdLine += " ";
    }

    cmdLine += subcasm.getCommandLine();
    cmdLine += " -nowait ";

    try {
      string name = casmName + " " + toString(ra) + "-" + toString(rb);
      stepCount   = rb - ra + 1;

      task.addTask(new TFarmTask(name, cmdLine, TSystem::getUserName(),
                                 TSystem::getHostName(), stepCount));
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }

    if (rb == casm->m_end) break;
    ra = rb + 1;
  }

  try {
    controller->addTask(task, m_submitAsSuspended->isSelected());
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
  }
}

//------------------------------------------------------------------------------

#ifdef WIN32

void DisplayStruct(LPNETRESOURCE lpnr) {
  string info;
  if (lpnr->lpLocalName) {
    info += lpnr->lpLocalName;
    info += " = ";
  }
  info += lpnr->lpRemoteName;

  MessageBox(NULL, info.c_str(), "Connection info", 0);
}

BOOL WINAPI EnumerateFunc(LPNETRESOURCE lpnr) {
  DWORD dwResult, dwResultEnum;
  HANDLE hEnum;
  DWORD cbBuffer = 16384;   // 16K is a good size
  DWORD cEntries = -1;      // enumerate all possible entries
  LPNETRESOURCE lpnrLocal;  // pointer to enumerated structures
  DWORD i;
  //
  // Call the WNetOpenEnum function to begin the enumeration.
  //
  dwResult = WNetOpenEnum(
      RESOURCE_CONNECTED /*RESOURCE_GLOBALNET*/,  // all network resources
      RESOURCETYPE_ANY,                           // all resources
      0,                                          // enumerate all resources
      lpnr,     // NULL first time the function is called
      &hEnum);  // handle to the resource

  if (dwResult != NO_ERROR) {
    //
    // Process errors with an application-defined error handler.
    //
    // NetErrorHandler(hwnd, dwResult, (LPSTR)"WNetOpenEnum");
    return FALSE;
  }
  //
  // Call the GlobalAlloc function to allocate resources.
  //
  lpnrLocal = (LPNETRESOURCE)GlobalAlloc(GPTR, cbBuffer);

  do {
    //
    // Initialize the buffer.
    //
    ZeroMemory(lpnrLocal, cbBuffer);
    //
    // Call the WNetEnumResource function to continue
    //  the enumeration.
    //
    dwResultEnum = WNetEnumResource(hEnum,       // resource handle
                                    &cEntries,   // defined locally as -1
                                    lpnrLocal,   // LPNETRESOURCE
                                    &cbBuffer);  // buffer size
    //
    // If the call succeeds, loop through the structures.
    //
    if (dwResultEnum == NO_ERROR) {
      for (i = 0; i < cEntries; i++) {
        // Call an application-defined function to
        //  display the contents of the NETRESOURCE structures.
        //
        DisplayStruct(&lpnrLocal[i]);

        // If the NETRESOURCE structure represents a container resource,
        //  call the EnumerateFunc function recursively.

        if (RESOURCEUSAGE_CONTAINER ==
            (lpnrLocal[i].dwUsage & RESOURCEUSAGE_CONTAINER))
          if (!EnumerateFunc(&lpnrLocal[i])) return FALSE;
        // TextOut(hdc, 10, 10, "EnumerateFunc returned FALSE.", 29);
      }
    }
    // Process errors.
    //
    else if (dwResultEnum != ERROR_NO_MORE_ITEMS) {
      // NetErrorHandler(hwnd, dwResultEnum, (LPSTR)"WNetEnumResource");
      break;
    }
  }
  //
  // End do.
  //
  while (dwResultEnum != ERROR_NO_MORE_ITEMS);
  //
  // Call the GlobalFree function to free the memory.
  //
  GlobalFree((HGLOBAL)lpnrLocal);
  //
  // Call WNetCloseEnum to end the enumeration.
  //
  dwResult = WNetCloseEnum(hEnum);

  if (dwResult != NO_ERROR) {
    //
    // Process errors.
    //
    // NetErrorHandler(hwnd, dwResult, (LPSTR)"WNetCloseEnum");
    return FALSE;
  }

  return TRUE;
}

#endif

//------------------------------------------------------------------------------

void CasmSubmitPage::Data::loadCasm(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) {
    string msg = toString(fp.getWideString()) + "\n";
    msg += "File not found\n";
    msg += "Please verify that the path and file name are correct";
    m_filepathTextField->setText("");
    TMessage::error(msg);
    return;
  }

  Application::instance()->setCurrentFolder(fp.getParentDir());

  string uncString = convertToUncString(fp);

  m_filepathTextField->setText(uncString);
  m_taskNameTextField->setText(TFilePath(uncString).getName());

#ifdef WIN32
  int x = uncString.find("\\", 2);
  while (x != string::npos) {
    uncString.replace(x, 1, "/", 0, 1);
    x = uncString.find("\\", x + 1);
  }
#endif

  TFilePath uncFilePath = TFilePath(uncString);
  CasmFileInfo casmFileInfo(uncFilePath);
  int start, end;
  bool interlaced;
  casmFileInfo.getFrameRange(start, end, interlaced);

  if (m_casmTask) delete m_casmTask;
  m_casmTask = new CasmTask;

  m_casmTask->m_casmFile = uncString;
  m_casmTask->m_start    = start;
  m_casmTask->m_end      = end;
  m_casmTask->m_step     = 1;

  m_configPanel->setTask(m_casmTask);
}

//==============================================================================

CasmSubmitPage::CasmSubmitPage(TWidget *parent)
    : TabPage(parent, "SubmitCasm") {
  m_data = new CasmSubmitPage::Data(this);
}

//------------------------------------------------------------------------------

CasmSubmitPage::~CasmSubmitPage() {}

//------------------------------------------------------------------------------

void CasmSubmitPage::configureNotify(const TDimension &size) {
  m_data->configureNotify(size);
}

//------------------------------------------------------------------------------

void CasmSubmitPage::onActivate() {}

//------------------------------------------------------------------------------

void CasmSubmitPage::onDeactivate() {}
