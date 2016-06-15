#pragma once

#ifndef SCRIPTCONFIGPANEL_H
#define SCRIPTCONFIGPANEL_H

#include "submitpage.h"

#include "tw/tw.h"
#include "tw/textfield.h"

// forward declarations
class TLabel;
class TButton;
class ScriptConfigPanel;

//==============================================================================

class PathFileField : public TTextField {
  ScriptConfigPanel *m_page;

public:
  PathFileField(TWidget *parent, string name = "");

  void onFocusChange(bool on);
  void close();
  void keyDown(int key, unsigned long flags, const TPoint &p);
};

//==============================================================================

class ScriptConfigPanel : public TaskConfigPanel {
public:
  ScriptConfigPanel(TWidget *parent);

  void configureNotify(const TDimension &d);

  void browseFiles();
  void setTask(SubmitPageTask *task);
  SubmitPageTask *getTask() const;

  void loadScript(const TFilePath &fp);
  void onTextField(string value, int type);

private:
  ScriptTask *m_task;

  TLabel *m_fileLbl;
  PathFileField *m_file;
  TButton *m_browseBtn;

  TLabel *m_arg2Lbl;
  TTextField *m_arg2;

  TLabel *m_arg3Lbl;
  TTextField *m_arg3;

  TLabel *m_arg4Lbl;
  TTextField *m_arg4;

  TLabel *m_arg5Lbl;
  TTextField *m_arg5;
};
#endif
