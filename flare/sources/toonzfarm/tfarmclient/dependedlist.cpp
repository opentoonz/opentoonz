

#include "tfarmcontroller.h"
#include "application.h"
#include "dependedlist.h"
#include "submitpage.h"

#include "tw/message.h"

#include <vector>

using namespace std;

DependedList::DependedList(TWidget *parent) : TWidget(parent) {
  m_depList = new TTextList(this);

  m_add = new TButton(this, "Add");
  tconnect<DependedList>(*(m_add), this, onAdd);

  m_remove = new TButton(this, "Remove");
  tconnect<DependedList>(*(m_remove), this, onRemove);
}

//------------------------------------------------------------------------------

void DependedList::configureNotify(const TDimension &size) {
  const int leftSize = size.lx / 3;
  m_depList->setGeometry(0, 0, leftSize, size.ly - 1);

  TDimension buttonSize(60, 15);
  int x  = leftSize + 30;
  int y0 = size.ly - 10;

  m_add->setGeometry(x, y0 - buttonSize.ly, x + buttonSize.lx, y0);
  y0 -= (buttonSize.ly + 10);
  m_remove->setGeometry(x, y0 - buttonSize.ly, x + buttonSize.lx, y0);
}

//------------------------------------------------------------------------------

void DependedList::onRemove() {
  if (!m_depList->getSelectedItem(0)) return;

  int count = m_depList->getSelectedItemCount();
  int i     = 0;
  for (; i < count; ++i) {
    string id = m_depList->getSelectedItemId(i);
    m_tasks.erase(m_tasks.find(id));
    m_depList->removeItem(id);
  }

  m_depList->invalidate();

  SubmitPage *submitPage = dynamic_cast<SubmitPage *>(getParent());
  if (submitPage) {
    SubmitPageTask *task = submitPage->getTask();
    if (task) task->setDependencies(m_tasks);
  }
}

//------------------------------------------------------------------------------

void DependedList::clearAll() {
  m_depList->clearAll();
  m_tasks.erase(m_tasks.begin(), m_tasks.end());
}

//------------------------------------------------------------------------------

void DependedList::onAdd() {
  static DependedPopup *popup = 0;

  if (!popup) {
    popup = new DependedPopup(this);

    popup->setOkAction(
        new TDependedPopupAction<DependedList>(this, &DependedList::AddItems));
  }

  if (!popup) return;

  TFarmController *controller = Application::instance()->getController();
  vector<TaskShortInfo> tasks;
  try {
    controller->getTasks("", tasks);
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
    return;
  }

  popup->setList(tasks);

  TDimension d = TMainshell::getMainshell()->getSize();
#ifdef WIN32
  HDC hdc = GetDC(0);
  d.lx    = GetDeviceCaps(hdc, HORZRES);
  d.ly    = GetDeviceCaps(hdc, VERTRES);
  ReleaseDC(0, hdc);
#endif

  d -= popup->getSize();
  popup->popup(TPoint(d.lx / 2, d.ly / 2));
  popup->setCaption("Tasks Submitted");
}

//------------------------------------------------------------------------------

void DependedList::AddItems(const vector<string> &tasksId) {
  TFarmController *controller       = Application::instance()->getController();
  vector<string>::const_iterator it = tasksId.begin();
  for (; it != tasksId.end(); ++it)
    if (m_tasks.end() == m_tasks.find(*it)) {
      try {
        string parentId, name;
        TaskState status;
        controller->queryTaskShortInfo(*it, parentId, name, status);
        string label = "<" + *it + "> " + name;
        m_depList->addItem(new TTextListItem(*it, label));
        m_tasks.insert(make_pair(*it, label));
      } catch (TException &e) {
        TMessage::error(toString(e.getMessage()));
      }
    }
  m_depList->invalidate();

  SubmitPage *submitPage = dynamic_cast<SubmitPage *>(getParent());
  if (submitPage) {
    SubmitPageTask *task = submitPage->getTask();
    if (task) task->setDependencies(m_tasks);
  }
}

//------------------------------------------------------------------------------

void DependedList::setList(const std::map<string, string> &tasks) {
  m_depList->clearAll();
  std::map<string, string>::const_iterator it = tasks.begin();
  for (; it != tasks.end(); ++it)
    m_depList->addItem(new TTextListItem((*it).first, (*it).second));
  m_tasks = tasks;
  invalidate();
}

//==============================================================================

DependedPopup::DependedPopup(TWidget *parent)
    : TModalPopup(parent, "DependedList"), m_okAction(0) {
  m_submitList = new TTextList(this);

  m_ok = new TButton(this, "Ok");
  tconnect<DependedPopup>(*(m_ok), this, onOk);

  m_cancel = new TButton(this, "Cancel");
  tconnect<DependedPopup>(*(m_cancel), this, TPopup::close);
}

//------------------------------------------------------------------------------

void DependedPopup::configureNotify(const TDimension &size) {
  const int bottomSize = 35;
  m_submitList->setGeometry(0, bottomSize, size.lx - 1, size.ly - 1);

  TDimension buttonSize(60, 15);
  int x1 = 30;
  int x2 = size.lx - x1;
  int y0 = bottomSize - 10;

  m_ok->setGeometry(x1, y0 - buttonSize.ly, x1 + buttonSize.lx, y0);
  m_cancel->setGeometry(x2 - buttonSize.lx, y0 - buttonSize.ly, x2, y0);
}

//------------------------------------------------------------------------------

TDimension DependedPopup::getPreferredSize() const {
  return TDimension(400, 300);
}

//------------------------------------------------------------------------------

void DependedPopup::onOk() {
  assert(m_okAction);
  vector<string> tasks;
  if (m_submitList->getSelectedItem(0)) {
    int count = m_submitList->getSelectedItemCount();
    int i     = 0;
    for (; i < count; ++i) tasks.push_back(m_submitList->getSelectedItemId(i));
  }
  try {
    m_okAction->sendCommand(tasks);
    close();
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
  }
}

//------------------------------------------------------------------------------

void DependedPopup::setList(const vector<TaskShortInfo> &tasks) {
  m_submitList->clearAll();

  vector<TaskShortInfo>::const_iterator it = tasks.begin();
  for (; it != tasks.end(); ++it) {
    string label = "<" + (*it).m_id + "> " + (*it).m_name;
    m_submitList->addItem(new TTextListItem((*it).m_id, label));
  }
}

//------------------------------------------------------------------------------

void DependedPopup::setOkAction(TGenericDependedPopupAction *action) {
  m_okAction = action;
}
