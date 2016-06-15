

#include "scriptconfigpanel.h"
#include "filebrowserpopup.h"
#include "application.h"
#include "util.h"

#include "tsystem.h"

#include "tw/mainshell.h"
#include "tw/label.h"
#include "tw/button.h"
#include "tw/keycodes.h"

#include "tw/message.h"

using namespace TwConsts;

//==============================================================================

PathFileField::PathFileField(TWidget *parent, string name)
    : TTextField(parent, name) {
  m_page = dynamic_cast<ScriptConfigPanel *>(parent);
}

//------------------------------------------------------------------------------

void PathFileField::onFocusChange(bool on) {
  if (!on) close();
  TWidget::onFocusChange(on);
}

//------------------------------------------------------------------------------

void PathFileField::close() {
  static wstring oldText;
  if (oldText != m_text) {
    TFilePath fp(m_text);
    try {
      wstring uncString;
      try {
        uncString = toWideString(convertToUncString(fp));
        m_text    = uncString;
        m_page->loadScript(TFilePath(uncString));
      } catch (TException &) {
        TFileStatus fs(fp);
        if (fs.doesExist() && !fs.isDirectory())
          m_page->loadScript(fp);
        else {
          ScriptTask *task = dynamic_cast<ScriptTask *>(m_page->getTask());
          if (task) task->m_arg1 = toString(m_text);
        }
      }
      oldText = m_text;
    } catch (...) {
      TMessage::error("boh");
    }
  }
  invalidate();
}

//------------------------------------------------------------------------------

void PathFileField::keyDown(int key, unsigned long flags, const TPoint &p) {
  if (key == TK_Return)
    close();
  else
    TTextField::keyDown(key, flags, p);
}

//==============================================================================

enum textFieldType { M_ARG1 = 0, M_ARG2, M_ARG3, M_ARG4, M_ARG5 };

class ArgumentChanger : public TTextField::Action {
  ScriptConfigPanel *m_scp;
  textFieldType m_typeField;

public:
  ArgumentChanger(ScriptConfigPanel *scp, textFieldType type)
      : m_scp(scp), m_typeField(type) {}

  void sendCommand(std::wstring value) {
    m_scp->onTextField(toString(value), m_typeField);
  }
};

//==============================================================================

ScriptConfigPanel::ScriptConfigPanel(TWidget *parent)
    : TaskConfigPanel(parent), m_task(new ScriptTask()) {
  m_fileLbl = new TLabel(this);
  m_fileLbl->setText("Arg #1:");
  m_file      = new PathFileField(this);
  m_browseBtn = new TButton(this);
  m_browseBtn->setTitle("...");
  tconnect(*m_browseBtn, this, &ScriptConfigPanel::browseFiles);
  // m_file->addAction(new ArgumentChanger(this, M_ARG1));

  m_arg2Lbl = new TLabel(this);
  m_arg2Lbl->setText("Arg #2:");
  m_arg2 = new TTextField(this);
  m_arg2->addAction(new ArgumentChanger(this, M_ARG2));

  m_arg3Lbl = new TLabel(this);
  m_arg3Lbl->setText("Arg #3:");
  m_arg3 = new TTextField(this);
  m_arg3->addAction(new ArgumentChanger(this, M_ARG3));

  m_arg4Lbl = new TLabel(this);
  m_arg4Lbl->setText("Arg #4:");
  m_arg4 = new TTextField(this);
  m_arg4->addAction(new ArgumentChanger(this, M_ARG4));

  m_arg5Lbl = new TLabel(this);
  m_arg5Lbl->setText("Arg #5:");
  m_arg5 = new TTextField(this);
  m_arg5->addAction(new ArgumentChanger(this, M_ARG5));
}

//------------------------------------------------------------------------------

void ScriptConfigPanel::configureNotify(const TDimension &d) {
  int x0 = 20;
  int y  = d.ly - 1 - 10;

  int x = x0;

  m_fileLbl->setGeometry(x, y - 15, x + 100, y);
  m_file->setGeometry(x + 104, y - 15, x + 650, y);
  m_browseBtn->setGeometry(x + 660, y - 15, x + 680, y);

  y -= 40;
  m_arg2Lbl->setGeometry(x, y - 15, x + 100, y);
  m_arg2->setGeometry(x + 104, y - 15, x + 650, y);

  y -= 40;
  m_arg3Lbl->setGeometry(x, y - 15, x + 100, y);
  m_arg3->setGeometry(x + 104, y - 15, x + 650, y);

  y -= 40;
  m_arg4Lbl->setGeometry(x, y - 15, x + 100, y);
  m_arg4->setGeometry(x + 104, y - 15, x + 650, y);

  y -= 40;
  m_arg5Lbl->setGeometry(x, y - 15, x + 100, y);
  m_arg5->setGeometry(x + 104, y - 15, x + 650, y);
}

//------------------------------------------------------------------------------

void ScriptConfigPanel::setTask(SubmitPageTask *task) {
  m_task = dynamic_cast<ScriptTask *>(task);
  assert(m_task);

  m_file->setText(m_task->m_arg1);
  m_arg2->setText(m_task->m_arg2);
  m_arg3->setText(m_task->m_arg3);
  m_arg4->setText(m_task->m_arg4);
  m_arg5->setText(m_task->m_arg5);
}

//------------------------------------------------------------------------------

SubmitPageTask *ScriptConfigPanel::getTask() const { return m_task; }

//------------------------------------------------------------------------------

void ScriptConfigPanel::browseFiles() {
  static FileBrowserPopup *popup = 0;

  if (!popup) {
    ScriptConfigPanel *csp = this;
    vector<string> fileTypes;

#ifdef WIN32
    fileTypes.push_back("bat");
#else
    fileTypes.push_back("csh");
#endif
    popup = new FileBrowserPopup(csp, fileTypes);

    popup->setOkAction(new TFileBrowserPopupAction<ScriptConfigPanel>(
        csp, &ScriptConfigPanel::loadScript));
  }

  if (!popup) return;

  TFilePath fp = Application::instance()->getCurrentFolder();
  /*
E'stato necessario fare questo controllo perche' il popup non e' in grado
di impostare opportunamente le cose
*/
  string path = toString(fp.getWideString()).c_str();
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
  popup->setCaption("Load Script");
}

//------------------------------------------------------------------------------

void ScriptConfigPanel::loadScript(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) {
    string msg = toString(fp.getWideString()) + "\n";
    msg += "File not found\n";
    msg += "Please verify that the path and file name are correct";
    m_file->setText("");
    TMessage::error(msg);
    return;
  }

  wstring uncString = fp.getWideString();
  if (m_file->getText() != uncString) try {
      uncString = toWideString(convertToUncString(fp));
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
      return;
    }

  Application::instance()->setCurrentFolder(fp.getParentDir());

  m_file->setText(uncString);

  /*
#ifdef WIN32
int x = uncString.find("\\",2);
while (x != string::npos)
{
uncString.replace(x,1,"/",0,1);
x = uncString.find("\\",x+1);
}
#endif
*/
  m_task->m_arg1 = toString(uncString);
}

//------------------------------------------------------------------------------

void ScriptConfigPanel::onTextField(string value, int type) {
  if (!m_task) return;

  switch (type) {
  case M_ARG1:
    m_task->m_arg1 = value;
    break;
  case M_ARG2:
    m_task->m_arg2 = value;
    break;
  case M_ARG3:
    m_task->m_arg3 = value;
    break;
  case M_ARG4:
    m_task->m_arg4 = value;
    break;
  case M_ARG5:
    m_task->m_arg5 = value;
    break;
  }
}
