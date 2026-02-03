#pragma once

#ifndef TASKS_H
#define TASKS_H

#ifdef WIN32
#pragma warning(disable : 4786)
#pragma warning(disable : 4146)
#endif

#include <string>
#include <map>
using namespace std;

//------------------------------------------------------------------------------

class SubmitPageTask {
public:
  virtual ~SubmitPageTask() {}

  virtual void setFileArg(const string &fp)          = 0;
  virtual string getCommandLine() const              = 0;
  virtual void setCommandLine(const string &cmdLine) = 0;
  virtual void setDefaultValue()                     = 0;

  virtual string getFilePath();
  virtual void setFilePath(const string &filePath);
  virtual string getName();
  virtual void setName(const string &name);
  virtual map<string, string> getDependencies();
  virtual void setDependencies(const map<string, string> &tasks);

  virtual void checkNameUnc();

  static SubmitPageTask *create(const string &type);

protected:
  string m_filePath;
  string m_name;
  map<string, string> m_depTasks;
};

//------------------------------------------------------------------------------

class CasmTask2 : public SubmitPageTask {
public:
  CasmTask2();
  CasmTask2(const string &s, const string &setupFilePath);

  void setFileArg(const string &fp);

  string getCommandLine() const;
  void setCommandLine(const string &cmdLine);

  void checkNameUnc();

  void setDefaultValue();

  enum moveType { M_NONE = 0, M_BG_FRACMOVE, M_INTMOVE, M_FRACMOVE };

  enum resType {
    RES_TYPE_NONE = 0,
    RES_TYPE_STANDARD,
    RES_TYPE_IMPROVED,
    RES_TYPE_HIGH
  };

  int m_taskChunksize;
  int m_start;
  int m_end;
  int m_step;
  string m_outname;
  int m_firstFrame;
  int m_lastFrame;
  int m_reduction;
  moveType m_moveType;
  int m_prec;
  double m_renderTile;
  double m_memChunk;
  bool m_multimedia;
  bool m_logfile;
  bool m_cf;
  resType m_restype;
  bool m_noOverwrite;
  bool m_clap;
  bool m_mprocess;
  int m_numColumn;
  int m_lineart;
  int m_edgeAliasing;
  double m_gamma;
  string m_bcScript;
  string m_acScript;
  string m_outputScript;
  string m_outputScriptArgument;
  string m_setupFile;
  int xsize, ysize;
};

//------------------------------------------------------------------------------

class ScriptTask : public SubmitPageTask {
public:
  ScriptTask();
  ScriptTask(const string &s, const string &setupFilePath);

  void setFileArg(const string &fp);
  string getCommandLine() const;
  void setCommandLine(const string &cmdLine);

  void checkNameUnc();

  void setDefaultValue();

  string m_setupFile;
  string m_arg1;
  string m_arg2;
  string m_arg3;
  string m_arg4;
  string m_arg5;
};

#endif
