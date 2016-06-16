

#include "submitpage.h"
#include "dependedlist.h"
#include "tfilepath.h"
#include "tconvert.h"
#include "tsystem.h"

#include "tw/mainshell.h"
#include "tw/textfield.h"
#include "tw/label.h"
#include "tw/optionmenu.h"
#include "tw/button.h"
#include "tw/checkbox.h"
#include "tw/keycodes.h"
#include "tw/event.h"

#include "filebrowserpopup.h"
#include "casmconfigpanel2.h"
#include "scriptconfigpanel.h"
#include "casmfileinfo.h"
#include "tfarmcontroller.h"
#include "application.h"
#include "util.h"

#include "tw/message.h"

#ifdef ZCOMP
#include "zrenderconfigpanel.h"
#include "zrendertask.h"
#endif

#include <map>
using namespace std;

#ifdef __sgi
#define TERMINAL "xwsh"
#else
#define TERMINAL "xterm"
#endif

using namespace TwConsts;

//==============================================================================

class TextFieldChanger : public TTextField::Action {
  SubmitPage *m_submit;
  bool m_isName;

public:
  TextFieldChanger(SubmitPage *submit, bool isName)
      : m_submit(submit), m_isName(isName) {}

  void sendCommand(wstring name) {
    m_submit->onTextField(toString(name), m_isName);
  }
};

//==============================================================================

class SubmitPage::Data {
  class FilePathField : public TTextField {
    SubmitPage::Data *m_data;

  public:
    FilePathField(SubmitPage::Data *data, TWidget *parent, string name = "")
        : TTextField(parent, name), m_data(data) {}

    void onFocusChange(bool on) {
      if (!on) close();
      TWidget::onFocusChange(on);
    }

    void close() {
      static wstring oldText;
      if (oldText != m_text) {
        TFilePath fp(m_text);
        try {
          wstring uncString;
          try {
            uncString = toWideString(convertToUncString(fp));
            m_text    = uncString;
            m_data->load(fp);
          } catch (TException &) {
            TFileStatus fs(fp);
            if (fs.doesExist() && !fs.isDirectory())
              m_data->load(fp);
            else if (m_data->m_task)
              m_data->m_task->setFilePath(toString(m_text));
          }
          oldText = m_text;
        } catch (...) {
        }
      }
      invalidate();
    }

    void keyDown(int key, unsigned long flags, const TPoint &p) {
      if (key == TK_Return)
        close();
      else
        TTextField::keyDown(key, flags, p);
    }
  };

public:
  Data(SubmitPage *page);
  ~Data();

  void configureNotify(const TDimension &size);

  void browseCasmFiles();

  void submit();
  void submitCasm(CasmTask2 *casm);
  void submitScript(ScriptTask *script);

  void load(const TFilePath &fp);

  void onTaskType(string type);

  SubmitPage *m_page;

  TLabel *m_taskTypeLbl;
  TOptionMenu *m_taskType;

  TLabel *m_taskNameLbl;
  TTextField *m_taskNameTextField;
  TCheckBox *m_submitAsSuspended;
  TButton *m_submit;

  TLabel *m_filepathLbl;
  FilePathField *m_filepathTextField;
  TButton *m_casmBrowseBtn;

  TLabel *m_priorityLbl;
  TNumField *m_priority;

  TLabel *m_depListLbl;
  DependedList *m_depList;

  SubmitPageTask *m_task;
  std::map<string, SubmitPageTask *> m_tasks;

  TaskConfigPanel *m_configPanel;
  std::map<string, TaskConfigPanel *> m_configPanels;
};

//------------------------------------------------------------------------------

SubmitPage::Data::Data(SubmitPage *page) : m_page(page), m_task(0) {
  m_taskTypeLbl = new TLabel(page);
  m_taskTypeLbl->setText("Task type:");
  m_taskType = new TOptionMenu(page);
  m_taskType->addOption("Casm");
  m_taskType->addOption("Script");
  m_taskType->setAction(new TOptionMenuAction<SubmitPage::Data>(
      this, &SubmitPage::Data::onTaskType));

  m_configPanel = new CasmConfigPanel2(page);
  m_configPanels.insert(make_pair(string("Casm"), m_configPanel));
  m_configPanels.insert(
      make_pair(string("Script"), new ScriptConfigPanel(page)));
  std::map<string, TaskConfigPanel *>::iterator it = m_configPanels.begin();
  for (; it != m_configPanels.end(); ++it) {
    TaskConfigPanel *configPanel = it->second;
    if (configPanel != m_configPanel) configPanel->hide();
  }

  m_taskNameLbl = new TLabel(page);
  m_taskNameLbl->setText("Task Name:");
  m_taskNameTextField = new TTextField(page);
  m_taskNameTextField->addAction(new TextFieldChanger(page, true));

  m_submitAsSuspended = new TCheckBox(page, "Submit as Suspended");

  m_filepathLbl = new TLabel(page);
  m_filepathLbl->setText("File Path:");

  m_filepathTextField = new FilePathField(this, page);
  // m_filepathTextField->addAction(new TextFieldChanger(page, false));
  m_casmBrowseBtn = new TButton(page);
  m_casmBrowseBtn->setTitle("...");
  tconnect(*m_casmBrowseBtn, this, &SubmitPage::Data::browseCasmFiles);

  m_submit = new TButton(page);
  m_submit->setTitle("Submit");
  tconnect(*m_submit, this, &SubmitPage::Data::submit);

  m_priorityLbl = new TLabel(page);
  m_priorityLbl->setText("Priority:");
  m_priority = new TNumField(page);
  m_priority->setIsInteger(true);
  m_priority->setRange(1, 100);
  m_priority->setValue(50);

  m_depListLbl = new TLabel(page);
  m_depListLbl->setText("Task Dependencies:");
  m_depList = new DependedList(page);
}

//------------------------------------------------------------------------------

SubmitPage::Data::~Data() {
  std::map<string, SubmitPageTask *>::iterator it = m_tasks.begin();
  while (it != m_tasks.end()) {
    delete it->second;
    ++it;
  }
  std::map<string, TaskConfigPanel *>::iterator it1 = m_configPanels.begin();
  while (it1 != m_configPanels.end()) {
    delete it1->second;
    ++it1;
  }
}

//------------------------------------------------------------------------------

void SubmitPage::Data::configureNotify(const TDimension &size) {
  int x0 = 20;
  int y  = size.ly - 1 - 10;
  int x  = x0;

  m_taskTypeLbl->setGeometry(x, y - 15, x + 100, y);
  m_taskType->setGeometry(x + 104, y - 15, x + 200, y);

  x = x0;
  y -= 40;

  m_taskNameLbl->setGeometry(x, y - 15, x + 100, y);
  m_taskNameTextField->setGeometry(x + 104, y - 15, x + 250, y);
  x += 280;
  m_priorityLbl->setGeometry(x, y - 15, x + 50, y);
  m_priority->setGeometry(x + 54, y - 15, x + 100, y);
  m_submitAsSuspended->setGeometry(x + 130, y - 15, x + 280, y);

  x += 310;
  m_submit->setGeometry(x, y - 15, x + 80, y);

  x = x0;
  y -= 40;
  m_filepathLbl->setGeometry(x, y - 15, x + 100, y);
  m_filepathTextField->setGeometry(x + 104, y - 15, x + 650, y);
  m_casmBrowseBtn->setGeometry(x + 660, y - 15, x + 680, y);

  y -= 40;
  x = x0;
  m_depListLbl->setGeometry(x, y - 15, x + 160, y);
  y -= 20;
  m_depList->setGeometry(x + 10, y - 80, size.lx - 1, y);

  x = x0;
  y -= 90;
  // m_configPanel->setGeometry(0,0,size.lx-1,y);
  std::map<string, TaskConfigPanel *>::iterator it;
  it = m_configPanels.begin();
  for (; it != m_configPanels.end(); ++it)
    it->second->setGeometry(0, 0, size.lx - 1, y);
}

//------------------------------------------------------------------------------

void SubmitPage::Data::browseCasmFiles() {
  static FileBrowserPopup *puCasm   = 0L;
  static FileBrowserPopup *puScript = 0L;
  FileBrowserPopup *popup;

  if (m_taskType->getText() == "Casm") {
    if (!puCasm) {
      vector<string> fileTypes;
      fileTypes.push_back("casm");
      puCasm = new FileBrowserPopup(m_page, fileTypes);
      puCasm->setOkAction(new TFileBrowserPopupAction<SubmitPage::Data>(
          this, &SubmitPage::Data::load));
    }
    popup = puCasm;
  } else if (m_taskType->getText() == "Script") {
    if (!puScript) {
      vector<string> fileTypes;
#ifdef WIN32
      fileTypes.push_back("bat");
#else
      fileTypes.push_back("csh");
      fileTypes.push_back("sh");
      fileTypes.push_back("tcsh");
#endif
      puScript = new FileBrowserPopup(m_page, fileTypes);
      puScript->setOkAction(new TFileBrowserPopupAction<SubmitPage::Data>(
          this, &SubmitPage::Data::load));
    }
    popup = puScript;
  }

  if (!popup) return;

  TFilePath fp = Application::instance()->getCurrentFolder();

  /*
E'stato necessario fare questo controllo perche' il popup non e' in grado
di impostare opportunamente le cose
*/
  wstring path = fp.getWideString();
  if (fp != TFilePath() && (path[0] != '\\')) popup->setCurrentDir(fp);

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
  popup->setCaption("Load " + m_taskType->getText());
}

//------------------------------------------------------------------------------

void SubmitPage::Data::load(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) {
    string msg = toString(fp.getWideString()) + "\n";
    msg += "File not found\n";
    msg += "Please verify that the path and file name are correct";
    m_filepathTextField->setText(toString(fp.getWideString()));
    TMessage::error(msg);
    return;
  }

  string uncString = toString(fp.getWideString());
  if (toString(m_filepathTextField->getText()) != uncString) try {
      uncString = convertToUncString(fp);
    } catch (TException &e) {
      if (!TFileStatus(fp).doesExist()) {
        TMessage::error(toString(e.getMessage()));
        return;
      }
      uncString = toString(fp.getWideString());
    }

  Application::instance()->setCurrentFolder(fp.getParentDir());

  m_filepathTextField->setText(uncString);
  m_taskNameTextField->setText(TFilePath(uncString).getName());

  string type = m_taskType->getText();
#ifdef WIN32
  if (type == "Casm") {
    int x = uncString.find("\\", 2);
    while (x != string::npos) {
      uncString.replace(x, 1, "/", 0, 1);
      x = uncString.find("\\", x + 1);
    }
  }
#endif

  if (type == "Casm")
    m_task = new CasmTask2;
  else if (type == "Script")
    m_task = new ScriptTask;
  else
    assert(false);

  std::map<string, SubmitPageTask *>::iterator it = m_tasks.find(type);
  if (it != m_tasks.end()) {
    delete it->second;
    it->second = m_task;
  } else
    m_tasks.insert(std::make_pair(type, m_task));

  m_task->setFileArg(uncString);
  m_depList->clearAll();

  m_configPanel->setTask(m_task);
}

//------------------------------------------------------------------------------

void SubmitPage::Data::submit() {
  if (toString(m_filepathTextField->getText()) == "") {
    TMessage::error("You must load a file");
    return;
  }

  try {
    CasmTask2 *casm    = dynamic_cast<CasmTask2 *>(m_task);
    ScriptTask *script = dynamic_cast<ScriptTask *>(m_task);

#ifdef ZCOMP
    ZRenderTask *zrender = dynamic_cast<ZRenderTask *>(m_task);
#endif

    if (casm) {
      casm->checkNameUnc();
      submitCasm(casm);
    } else if (script) {
      script->checkNameUnc();
      submitScript(script);
    }
#ifdef ZCOMP
    else if (zrender) {
      zrender->checkNameUnc();
      submitZRender(zrender);
    }
#endif
    else {
      TMessage::error("Verify if " + toString(m_filepathTextField->getText()) +
                      " is correct");
      return;
    }
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
  }
}

//------------------------------------------------------------------------------

void SubmitPage::Data::submitCasm(CasmTask2 *casm) {
  TFarmController *controller = Application::instance()->getController();

  string nativeCmdLine("runcasm ");
  nativeCmdLine += casm->getFilePath();
  nativeCmdLine += " ";

  if (casm->m_setupFile != "") {
    nativeCmdLine += "-setup ";
    nativeCmdLine += casm->m_setupFile;
    nativeCmdLine += " ";
  }

  nativeCmdLine += casm->getCommandLine();

  string casmName = casm->getName();

  string user   = TSystem::getUserName();
  string host   = TSystem::getHostName();
  int stepCount = casm->m_end - casm->m_start + 1;

  TFarmTaskGroup task(casmName, nativeCmdLine, user, host, stepCount,
                      (int)m_priority->getValue());

  map<string, string> dep             = casm->getDependencies();
  map<string, string>::iterator itDep = dep.begin();
  for (; itDep != dep.end(); ++itDep) task.m_dependencies->add((*itDep).first);

  int ra = casm->m_start;

  int subCount = casm->m_end / casm->m_taskChunksize;
  if (casm->m_end % casm->m_taskChunksize) subCount++;

  if (subCount != 1)
    for (;;) {
      CasmTask2 subcasm(*casm);

      string cmdLine("runcasm ");
      int rb = tmin(ra + casm->m_taskChunksize - 1, casm->m_end);

      subcasm.m_start = ra;
      subcasm.m_end   = rb;

      cmdLine += subcasm.getFilePath();
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

        TFarmTask *subTask = new TFarmTask(name, cmdLine, user, host, stepCount,
                                           (int)m_priority->getValue());
        subTask->m_dependencies =
            new TFarmTask::Dependencies(*task.m_dependencies);
        task.addTask(subTask);
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

void SubmitPage::Data::submitScript(ScriptTask *script) {
  TFarmController *controller = Application::instance()->getController();

  string nativeCmdLine = script->getCommandLine();

  string scriptName = script->getName();
  string user       = TSystem::getUserName();
  string host       = TSystem::getHostName();
  TFarmTask task(scriptName, nativeCmdLine, user, host, 1,
                 (int)m_priority->getValue());
  map<string, string> dep             = script->getDependencies();
  map<string, string>::iterator itDep = dep.begin();
  for (; itDep != dep.end(); ++itDep) task.m_dependencies->add((*itDep).first);

#ifdef WIN32
  task.m_platform = Windows;
#else
  task.m_platform = Irix;
#endif

  try {
    controller->addTask(task, m_submitAsSuspended->isSelected());
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
  }
}

//------------------------------------------------------------------------------

void SubmitPage::Data::onTaskType(string type) {
  std::map<string, TaskConfigPanel *>::iterator it;
  it = m_configPanels.find(type);
  if (it != m_configPanels.end()) {
    m_configPanel->hide();
    TaskConfigPanel *configPanel = it->second;
    m_configPanel                = configPanel;
    m_configPanel->show();
    std::map<string, SubmitPageTask *>::iterator it1 = m_tasks.find(type);
    if (it1 != m_tasks.end()) {
      m_task = it1->second;
      m_depList->setList(m_task->getDependencies());
      m_filepathTextField->setText(m_task->getFilePath());
      m_taskNameTextField->setText(m_task->getName());
      m_configPanel->setTask(m_task);
    } else {
      std::map<string, string> tasks;
      m_depList->setList(tasks);
      m_taskNameTextField->setText("");
      m_filepathTextField->setText("");
    }
  }
}

//==============================================================================

SubmitPage::SubmitPage(TWidget *parent) : TabPage(parent, "Submit Task") {
  m_data = new SubmitPage::Data(this);
}

//------------------------------------------------------------------------------

SubmitPage::~SubmitPage() { delete m_data; }

//------------------------------------------------------------------------------

void SubmitPage::configureNotify(const TDimension &size) {
  m_data->configureNotify(size);
}

//------------------------------------------------------------------------------

void SubmitPage::onActivate() {}

//------------------------------------------------------------------------------

void SubmitPage::onDeactivate() {}

//------------------------------------------------------------------------------

SubmitPageTask *SubmitPage::getTask() const { return m_data->m_task; }

//------------------------------------------------------------------------------

void SubmitPage::setTask(SubmitPageTask *task) {
  string type        = "Casm";
  CasmTask2 *casm    = dynamic_cast<CasmTask2 *>(task);
  ScriptTask *script = dynamic_cast<ScriptTask *>(task);

#ifdef ZCOMP
  ZRenderTask *zrender = dynamic_cast<ZRenderTask *>(task);
#endif

  if (casm)
    m_data->m_task = casm;
  else if (script) {
    m_data->m_task = script;
    type           = "Script";
  }
#ifdef ZCOMP
  else if (zrender) {
    m_data->m_task = zrender;
    type           = "ZComp";
  }
#endif
  else {
    assert(false);
  }

  std::map<string, SubmitPageTask *>::iterator it = m_data->m_tasks.find(type);
  if (it == m_data->m_tasks.end()) {
    m_data->m_tasks.insert(std::make_pair(type, m_data->m_task));
  } else
    it->second = m_data->m_task;

  std::map<string, TaskConfigPanel *>::iterator it1 =
      m_data->m_configPanels.find(type);
  if (it1 != m_data->m_configPanels.end()) {
    it1->second->setTask(task);
    m_data->m_configPanel = it1->second;
  }
}

//------------------------------------------------------------------------------

void SubmitPage::onTextField(const string &name, bool isName) {
  if (!m_data->m_task) return;

  if (isName)
    m_data->m_task->setName(name);
  else
    m_data->m_task->setFilePath(name);
}
