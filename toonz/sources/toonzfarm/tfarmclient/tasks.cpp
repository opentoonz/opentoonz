

#include "tasks.h"
#include "tconvert.h"
#include "casmfileinfo.h"
#include "util.h"
#include "texception.h"
#include "tsystem.h"

#define DDR_OUTPUTSCRIPT_FILENAME "D@D@R"

//==============================================================================

string SubmitPageTask::getFilePath() { return m_filePath; }

//------------------------------------------------------------------------------

void SubmitPageTask::setFilePath(const string &filePath) {
  m_filePath = filePath;
}

//------------------------------------------------------------------------------

string SubmitPageTask::getName() { return m_name; }

//------------------------------------------------------------------------------

void SubmitPageTask::setName(const string &name) { m_name = name; }

//------------------------------------------------------------------------------

map<string, string> SubmitPageTask::getDependencies() { return m_depTasks; }

//------------------------------------------------------------------------------

void SubmitPageTask::setDependencies(const map<string, string> &tasks) {
  m_depTasks = tasks;
}

//------------------------------------------------------------------------------

void SubmitPageTask::checkNameUnc() {
  if (m_filePath == "") return;

  TFilePath fp(m_filePath);
  string uncString = "";
  try {
    uncString = convertToUncString(fp);
  } catch (TException &) {
    if (!TFileStatus(fp).doesExist()) {
      string msg = toString(fp.getWideString()) + "\n";
      msg += "File not found\n";
      msg += "Please verify that the path is correct";
      throw TException(msg);
    }
  }
}

//==============================================================================

CasmTask2::CasmTask2() { setDefaultValue(); }

//------------------------------------------------------------------------------

CasmTask2::CasmTask2(const string &s, const string &setupFilePath)
    : m_setupFile(setupFilePath) {
  setCommandLine(s);
}

//------------------------------------------------------------------------------

void CasmTask2::setDefaultValue() {
  m_taskChunksize        = 1;
  m_start                = 0;
  m_end                  = 9999;
  m_step                 = 1;
  m_filePath             = "";
  m_name                 = "";
  m_outname              = "";
  m_firstFrame           = 0;
  m_lastFrame            = 0;
  m_reduction            = 1;
  m_moveType             = M_BG_FRACMOVE;
  m_prec                 = 32;
  m_renderTile           = 1.;
  m_memChunk             = 4.;
  m_multimedia           = false;
  m_logfile              = false;
  m_cf                   = false;
  m_restype              = RES_TYPE_STANDARD;
  m_noOverwrite          = false;
  m_clap                 = false;
  m_mprocess             = false;
  m_numColumn            = -1;
  m_lineart              = -1;
  m_edgeAliasing         = -1;
  m_gamma                = -1.;
  m_bcScript             = "";
  m_acScript             = "";
  m_outputScript         = "";
  m_outputScriptArgument = "";
  m_setupFile            = "";
  xsize                  = 0;
  ysize                  = 0;
}

//------------------------------------------------------------------------------

void CasmTask2::setFileArg(const string &fp) {
  TFilePath filePath = TFilePath(fp);
  CasmFileInfo casmFileInfo(filePath);
  int start, end;
  bool interlaced;
  casmFileInfo.getFrameRange(start, end, interlaced);

  m_filePath = fp;
  m_start    = start;
  m_end      = end;
  m_step     = 1;

  setName(TFilePath(fp).getName());
}

//------------------------------------------------------------------------------

string CasmTask2::getCommandLine() const {
  string qualifiers, prgname, dirname, biname;

  if (m_reduction != 1) qualifiers += " -shrink " + toString(m_reduction) + " ";

  if (m_moveType != moveType::M_BG_FRACMOVE)
    qualifiers +=
        (m_moveType == moveType::M_FRACMOVE) ? " -fracmove " : " -intmove ";
  if (m_prec == 64)
    qualifiers += " -64bit";
  else
    qualifiers += " -32bit";

  qualifiers += " -tile " + toString(m_renderTile) + " ";

  qualifiers += " -mem " + toString(m_memChunk) + " ";

  if (m_multimedia) {
    if (m_numColumn < 0)
      qualifiers += " -mm ";
    else
      qualifiers += " -mmsingle " + toString(m_numColumn) + " ";
  }

  if (m_logfile) qualifiers += " -logfile +output/casm_batches.log ";

  if (m_lineart > -1) qualifiers += " -lineart " + toString(m_lineart) + " ";
  if (m_edgeAliasing > -1)
    qualifiers += " -edgealiasing " + toString(m_edgeAliasing) + " ";
  if (m_gamma > -1.0) qualifiers += " -gamma " + toString(m_gamma) + " ";

  switch (m_restype) {
    CASE RES_TYPE_STANDARD : qualifiers += " -sq ";
    CASE RES_TYPE_IMPROVED : qualifiers += " -iq ";
    CASE RES_TYPE_HIGH : qualifiers += " -hq ";
  }

  if (m_cf) qualifiers += " -cf ";

  if (m_noOverwrite) qualifiers += " -no_overwrite ";

  if (m_clap) qualifiers += " -clap ";

  if (m_bcScript != "") qualifiers += " -bc " + m_bcScript + " ";

  if (m_acScript != "" && m_outputScript == "")
    qualifiers += " -ac " + m_acScript + " ";

  if (m_outputScript != "") {
    bool outputToDdr = (m_outputScript == DDR_OUTPUTSCRIPT_FILENAME);

    if (!outputToDdr) qualifiers += " -ac " + m_outputScript + " ";
    if (m_outputScriptArgument != "") {
      string entry_point = m_outputScriptArgument;
      string parity      = "odd";
      int i;
      i = entry_point.size();
      if (i > 0 && entry_point[i - 1] == '+') {
        parity             = "even";
        entry_point[i - 1] = '\0';
      }
      // convert_timecode_to_frame (entry_point);//??????
      if (outputToDdr)
        qualifiers += " -ddr " + entry_point + " ";
      else {
        string app;
#ifdef WIN32
        app = " -ac_args \"$filename $count " + entry_point;
        app += " " + parity;
        app += " $total\" ";
        qualifiers += app;
#else
        app = " -ac_args '$filename $count " + entry_point;
        app += " " + parity;
        app += " $total' ";
        qualifiers += " app;
#endif
      }
    } else {
      if (outputToDdr)
        qualifiers += " -ddr " + toString(0) + " ";
      else
#ifdef WIN32
        qualifiers += " -ac_args \"$filename $count $total\" ";
#else
        qualifiers += " -ac_args '$filename $count $total' ";
#endif
    }
  }

  qualifiers += "  -step " + toString(m_step);

  if (m_start > m_firstFrame || m_end < m_lastFrame)
    qualifiers += " -range " + toString(m_start) + " " + toString(m_end) + " ";

  // qualifiers += " -chunk "+toString(m_taskChunksize)+" ";

  return qualifiers;
}

//------------------------------------------------------------------------------

static bool is_num(string word) {
  string::iterator it = word.begin();
  while (it != word.end()) {
    if ((*it < '0' || *it > '9') && *it != '.') return false;
    it++;
  }
  return true;
}

//------------------------------------------------------------------------------

static string cpy_word(string &word, string s) {
  /* salto gli spazi bianchi iniziali */
  string::iterator it  = s.begin();
  word                 = "";
  string::iterator it2 = word.begin();
  while (s[0] == ' ' || s[0] == '\t') it++;
  if (s[0] == '\'' || s[0] == '\"') {
    /* argomento fra virgolette */
    char quote = *it++;
    while (*it && *it != '\n') {
      if (*it == '\\') {
        it++;
        if (*it == '\0' || *it == '\n') break;
        switch (*it) {
          CASE '\\' : *it2++ = '\\';
        DEFAULT:
          *it2++ = *it;
        }
        it++;
      } else if (*it == quote) {
        it++;
        break;
      } else
        *it2++ = *it++;
    }
  } else {
    /* argomento senza virgolette */
    while (*it && *it != ' ' && *it != '\t' && *it != '\n') *it2++ = *it++;
  }
  //*word = '\0';

  /* salto gli spazi bianchi finali */
  while (*it == ' ' || *it == '\t') it++;
  return s.erase(s.begin(), it);
}

//------------------------------------------------------------------------------

void CasmTask2::setCommandLine(const string &cmdLine) {
  if (cmdLine.empty()) return;

  setDefaultValue();
  string s            = cmdLine;
  string::iterator it = s.begin();
  string word;
  while (it != s.end()) {
    s = cpy_word(word, s);

    if (word == "-64bit")
      m_prec = 64;
    else if (word == "-32bit")
      m_prec = 32;
    else if (word == "-sq")
      m_restype = RES_TYPE_STANDARD;
    else if (word == "-iq")
      m_restype = RES_TYPE_IMPROVED;
    else if (word == "-hq")
      m_restype = RES_TYPE_HIGH;
    else if (word == "-step") {
      s                        = cpy_word(word, s);
      if (is_num(word)) m_step = atoi(word.c_str());
    } else if (word == "-range") {
      s                         = cpy_word(word, s);
      if (is_num(word)) m_start = atoi(word.c_str());
      s                         = cpy_word(word, s);
      if (is_num(word)) m_end   = atoi(word.c_str());
    } else if (word == "-shrink") {
      s                             = cpy_word(word, s);
      if (is_num(word)) m_reduction = atoi(word.c_str());
    } else if (word == "-fracmove") {
      m_moveType = M_FRACMOVE;
    } else if (word == "-intmove") {
      m_moveType = M_INTMOVE;
    }
    /*     else
if (STR_EQ(word, "-64bit"))
task->prec = 64;
else
if (STR_EQ(word, "-32bit"))
task->prec = 32;*/
    else if (word == "-tile") {
      s                              = cpy_word(word, s);
      if (is_num(word)) m_renderTile = atof(word.c_str());
    } else if (word == "-mem") {
      s                                 = cpy_word(word, s);
      if (is_num(word)) m_taskChunksize = atoi(word.c_str());
    } else if (word == "-chunk") {
      s                            = cpy_word(word, s);
      if (is_num(word)) m_memChunk = atof(word.c_str());
    } else if (word == "-mm") {
      m_multimedia = true;
      m_numColumn  = -1;
    } else if (word == "-mmsingle") {
      m_multimedia                  = true;
      s                             = cpy_word(word, s);
      if (is_num(word)) m_numColumn = atoi(word.c_str());
    } else if (word == "-lineart") {
      s                           = cpy_word(word, s);
      if (is_num(word)) m_lineart = atoi(word.c_str());
    } else if (word == "-edgealiasing") {
      s                                = cpy_word(word, s);
      if (is_num(word)) m_edgeAliasing = atoi(word.c_str());
    } else if (word == "-gamma") {
      s                         = cpy_word(word, s);
      if (is_num(word)) m_gamma = atof(word.c_str());
    } else if (word == "-cf") {
      m_cf = true;
    } else if (word == "-no_overwrite") {
      m_noOverwrite = true;
    } else if (word == "-clap") {
      m_clap = true;
    } else if (word == "-bc") {
      s          = cpy_word(word, s);
      m_bcScript = word;
    } else if (word == "-ac") {
      s              = cpy_word(word, s);
      m_outputScript = word;
    } else if (word == "-ac_args") { /*
char output_script_argument[1024];
s = cpy_word(word, s);
if(parse_ac_args(output_script_argument, word))
task->output_script_argument = strsave(output_script_argument);*/
    } else if (word == "-logfile") {
      s         = cpy_word(word, s);
      m_logfile = true;
    }
    /*     else
if (!word.compare("-range"))
{
s = cpy_word(word, s);
if (is_num(word))
task->start = atoi(word);
else
{
delete_job((TASK *)task);
return NULL;
}
s = cpy_word(word, s);
if (is_num(word))
task->end = atoi(word);
else
{
delete_job((TASK *)task);
return NULL;
}
}*/
    /*     else
if (*word!='\n' && *word!='\0')
{
t = tim_get_type(word);
if (*t == '.') t++;
if (FILESTR_NE(t, "casm"))
{
delete_job((TASK *)task);
//           return NULL;
}
else
{
TCALLOC(task->casm_file, strlen(word)+1);
strcpy(task->casm_file, word);
compute_casm_range(task->casm_file, &(task->first_frame), &(task->last_frame),
&dummy);
if (task->start<task->first_frame)
  task->start=task->first_frame;
if (task->end>task->last_frame)
  task->end=task->last_frame;
task->xsize = task->ysize = 0;
casm_camera_size(task->casm_file, &task->xsize, &task->ysize);


}
}*/
    it = s.begin();
  }
}

//------------------------------------------------------------------------------

void CasmTask2::checkNameUnc() {
  SubmitPageTask::checkNameUnc();
  if (m_setupFile == "") return;

  TFilePath fp(m_setupFile);
  string uncString = "";
  try {
    uncString = convertToUncString(fp);
  } catch (TException &) {
    if (!TFileStatus(fp).doesExist()) {
      string msg = toString(fp.getWideString()) + "\n";
      msg += "File not found\n";
      msg += "Please verify that the path is correct";
      throw TException(msg);
    }
  }
}

//==============================================================================

ScriptTask::ScriptTask() { setDefaultValue(); }

//------------------------------------------------------------------------------

ScriptTask::ScriptTask(const string &s, const string &setupFilePath)
    : m_setupFile(setupFilePath) {
  setCommandLine(s);
}

//------------------------------------------------------------------------------

void ScriptTask::setFileArg(const string &fp) {
  m_filePath = fp;
  m_name     = TFilePath(fp).getName();
}

//------------------------------------------------------------------------------

string ScriptTask::getCommandLine() const {
  string cmdLine;
  if (m_filePath == "") {
    assert(false);
    return "";
  }
#ifdef WIN32
  /*
if (is_fg)
cmdLine = m_filePath;
else
*/
  // viene sempre fatto girare in background
  cmdLine = "CALL " + m_filePath + " ";
#else
  cmdLine = m_filePath;
#endif
  cmdLine += "   ";

  if (m_arg1 != "") cmdLine += m_arg1 + "   ";
  if (m_arg2 != "") cmdLine += m_arg2 + "   ";
  if (m_arg3 != "") cmdLine += m_arg3 + "   ";
  if (m_arg4 != "") cmdLine += m_arg4 + "   ";
  if (m_arg5 != "") cmdLine += m_arg5 + "   ";
  return cmdLine;
}

//------------------------------------------------------------------------------

void ScriptTask::setCommandLine(const string &cmdLine) {
  setDefaultValue();
  string s = cmdLine;
  string word;
  if (s == "") {
    s = cpy_word(word, s);
    // if (word == "")
    m_arg1 = word;
    // else
    // m_arg1 = "";
  }
  if (s == "") {
    s = cpy_word(word, s);
    // if (word == "")
    m_arg2 = word;
  }
  if (s == "") {
    s = cpy_word(word, s);
    // if (word == "")
    m_arg3 = word;
  }
  if (s == "") {
    s = cpy_word(word, s);
    // if (word == "")
    m_arg4 = word;
  }
  if (s == "") {
    s = cpy_word(word, s);
    // if (word == "")
    m_arg5 = word;
  }
}

//------------------------------------------------------------------------------

void ScriptTask::setDefaultValue() {
  m_name      = "";
  m_filePath  = "";
  m_setupFile = "";
  m_arg1      = "";
  m_arg2      = "";
  m_arg3      = "";
  m_arg4      = "";
  m_arg5      = "";
}

//------------------------------------------------------------------------------

void ScriptTask::checkNameUnc() {
  SubmitPageTask::checkNameUnc();
  if (m_arg1 == "") return;

  TFilePath fp(m_arg1);
  string uncString = "";
  try {
    uncString = convertToUncString(fp);
  } catch (TException &) {
    if (!TFileStatus(fp).doesExist()) {
      string msg = toString(fp.getWideString()) + "\n";
      msg += "File not found\n";
      msg += "Please verify that the path is correct";
      throw TException(msg);
    }
  }
}
