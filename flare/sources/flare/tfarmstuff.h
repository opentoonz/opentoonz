#pragma once

#include "tw/tw.h"
#include "tw/popup.h"
#include "tw/textlist.h"
#include "tw/textfield.h"

#include "tfilepath.h"
#include "tfarmcontroller.h"
//#include "texception.h"

class TFarmController;
class TPopupMenuItem;
class TPopupMenu;
class TTextField;
class TLabel;
class TButton;

namespace TFarmStuff {

//------------------------------------------------------------------------------

class TFarmPage : public TWidget {
public:
  TFarmPage(TWidget *parent, const std::string &name) : TWidget(parent, name) {}
  virtual ~TFarmPage() {}

  virtual void onActivate() {}
  virtual void onDeactivate() {}

  virtual void update() {}
};

//------------------------------------------------------------------------------

class TaskInfoPage final : public TFarmPage {
public:
  TaskInfoPage(TWidget *parent);
  ~TaskInfoPage();

  void configureNotify(const TDimension &size) override;
  void rightButtonDown(const TMouseEvent &e) override;

  void onActivate() override;
  void onDeactivate() override;
  void update() override;

  void showTaskInfo(const std::string &id);

private:
  class Data;
  Data *m_data;
};

//------------------------------------------------------------------------------

class SubmitPageTask {
public:
  virtual ~SubmitPageTask() {}

  /*
virtual void setFileArg(const std::string &fp) = 0;
virtual void setCommandLine(const std::string &cmdLine) = 0;
virtual void setDefaultValue() = 0;
*/

  virtual std::string getCommandLine() const = 0;

  virtual std::string getFilePath() { return m_filePath; }

  virtual void setFilePath(const std::string &filePath) {
    m_filePath = filePath;
  }

  virtual std::string getName() { return m_name; }

  virtual void setName(const std::string &name) { m_name = name; }

  /*
virtual map<std::string, string> getDependencies();
virtual void setDependencies(const map<std::string, string> &tasks);
*/
  static SubmitPageTask *create(const std::string &type);

protected:
  std::string m_filePath;
  std::string m_name;
  std::map<std::string, std::string> m_depTasks;
};

//------------------------------------------------------------------------------

class SubmitPage final : public TFarmPage {
public:
  SubmitPage(TWidget *parent);
  ~SubmitPage();

  void configureNotify(const TDimension &size) override;

  void onActivate() override;
  void onDeactivate() override;

  SubmitPageTask *getTask() const;
  void setTask(SubmitPageTask *task);
  void onTextField(const std::string &name, bool isName);

  class Data;
  Data *m_data;
};

//------------------------------------------------------------------------------

class TaskConfigPanel final : public TWidget {
public:
  TaskConfigPanel(TWidget *parent) : TWidget(parent) {}

  virtual void setTask(SubmitPageTask *task) = 0;
  virtual SubmitPageTask *getTask() const    = 0;
};

//------------------------------------------------------------------------------

class SubmitRenderPopup final : public TModalPopup {
public:
  SubmitRenderPopup(TWidget *parent, std::string name);
  ~SubmitRenderPopup();

  void onOk();

  TDimension getPreferredSize() const override;

  void configureNotify(const TDimension &d) override;
  bool onNcPaint(bool is_active, const TDimension &size,
                 const TRect &titlebar) override;

  void popup(const TPoint &p, const TFilePath &fp);
  void onIntFieldChange(const TNumField::Event &e);
  void draw() override;

private:
  class Data;
  Data *m_data;
};

//------------------------------------------------------------------------------

class SubmitCleanupPopup final : public TModalPopup {
public:
  SubmitCleanupPopup(TWidget *parent, std::string name);
  ~SubmitCleanupPopup();

  void onOk();

  TDimension getPreferredSize() const override;

  void configureNotify(const TDimension &d) override;
  bool onNcPaint(bool is_active, const TDimension &size,
                 const TRect &titlebar) override;

  void popup(const TPoint &p, const TFilePath &fp);
  void onIntFieldChange(const TNumField::Event &e);
  void draw() override;

private:
  class Data;
  Data *m_data;
};
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class GRootEnvVarPopup final : public TModalPopup {
public:
  GRootEnvVarPopup(TWidget *parent, std::string name);
  ~GRootEnvVarPopup();

  void onOk();

  TDimension getPreferredSize() const override;

  void configureNotify(const TDimension &d) override;
  void draw() override;

  // bool onNcPaint(bool is_active, const TDimension &size, const TRect
  // &titlebar);

  void popup(const TPoint &p, bool missingvar);

  bool canceled() { return m_canceled; }

  TLabel *m_grootVarLabel;
  TTextField *m_grootVar;
  TButton *m_okBtn;
  TButton *m_cancelBtn;

  bool m_canceled;
  bool m_missingvar;
};

//------------------------------------------------------------------------------

TFilePath getGlobalRoot();
void setGlobalRoot(const TFilePath &fp);

TFarmController *getTFarmController();

bool testConnection(const QString &ipaddr, int port);
bool testConnectionToController();

void getControllerData(QString &hostName, QString &ipAddr, int &port);

//------------------------------------------------------------------------------

class TMissingGRootFolder final : public TException {
public:
  TMissingGRootFolder() : TException() {}
};

//------------------------------------------------------------------------------

class TMissingGRootEnvironmentVariable final : public TException {
public:
  TMissingGRootEnvironmentVariable() : TException() {}
};

}  // namespace TFarmStuff
