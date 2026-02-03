

#include "casmconfigpanel.h"
#include "filebrowserpopup.h"
#include "application.h"

#include "tw/mainshell.h"
#include "tw/textfield.h"
#include "tw/label.h"
#include "tw/optionmenu.h"
#include "tw/button.h"
#include "tw/checkbox.h"

#include "tconvert.h"
#include "util.h"
#include "tsystem.h"

#include "tw/message.h"

//==============================================================================

CasmTask::CasmTask() { setdefaultValue(); }

//------------------------------------------------------------------------------

CasmTask::CasmTask(const string &s, const string &setupFilePath)
    : m_setupFile(setupFilePath) {
  setCommandLine(s);
}

//------------------------------------------------------------------------------

void CasmTask::setdefaultValue() {
  m_taskChunksize        = 1;
  m_start                = 0;
  m_end                  = 9999;
  m_step                 = 1;
  m_casmFile             = "";
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

string CasmTask::getCommandLine() const {
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

void CasmTask::setCommandLine(const string &cmdLine) {
  if (cmdLine.empty()) return;

  setdefaultValue();
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

//==============================================================================
enum numFieldType {
  M_FROM = 0,
  M_TO,
  M_STEP,
  M_SHRINK,
  M_RTILE,
  M_MCHUNK,
  M_COLUMN,
  M_THICK,
  M_GAMMA,
  M_CHUNKSIZE
};

class NumFieldChanger : public TNumField::Action {
  CasmConfigPanel *m_ccp;
  numFieldType m_typeField;

public:
  NumFieldChanger(CasmConfigPanel *ccp, numFieldType type)
      : m_ccp(ccp), m_typeField(type) {}

  void sendCommand(const TNumField::Event &ev) {
    m_ccp->onNumField(ev.m_value, m_typeField);
  }
};

class ColumnFieldChanger : public TTextField::Action {
  CasmConfigPanel *m_ccp;

public:
  ColumnFieldChanger(CasmConfigPanel *ccp) : m_ccp(ccp) {}

  void sendCommand(std::wstring value) { m_ccp->onColumnField(value); }
};

class NumFieldChanger2 : public TNumField::Action {
public:
  NumFieldChanger2(CasmConfigPanel *ccp, double CasmTask::*) {}

  void sendCommand(double value) {
    //*m_member = value;
  }

  double CasmTask::*m_member;
};

//==============================================================================

CasmConfigPanel::CasmConfigPanel(TWidget *parent) : TWidget(parent) {
  m_task = new CasmTask();

  m_chunkSizeLabel = new TLabel(this);
  m_chunkSizeLabel->setText("Task Chunk Size:");
  m_chunkSize = new TNumField(this);
  m_chunkSize->setIsInteger(true);
  m_chunkSize->setRange(1, std::numeric_limits<int>::max());
  m_chunkSize->addAction(new NumFieldChanger(this, M_CHUNKSIZE));

  m_setupLabel = new TLabel(this);
  m_setupLabel->setText("Setup Path:");
  m_setupTextField = new TTextField(this);
  m_setupBrowseBtn = new TButton(this);
  m_setupBrowseBtn->setTitle("...");
  tconnect(*m_setupBrowseBtn, this, &CasmConfigPanel::browseSetupFiles);

  m_fromLabel = new TLabel(this);
  m_fromLabel->setText("From:");
  m_from = new TNumField(this);
  m_from->setIsInteger(true);
  m_from->addAction(new NumFieldChanger(this, M_FROM));

  m_toLabel = new TLabel(this);
  m_toLabel->setText("To:");
  m_to = new TNumField(this);
  m_to->setIsInteger(true);
  m_to->addAction(new NumFieldChanger(this, M_TO));

  m_stepLabel = new TLabel(this);
  m_stepLabel->setText("Step:");
  m_step = new TNumField(this);
  m_step->setIsInteger(true);
  m_step->addAction(new NumFieldChanger(this, M_STEP));

  m_shrinkLabel = new TLabel(this);
  m_shrinkLabel->setText("Shrink:");
  m_shrink = new TNumField(this);
  m_shrink->setIsInteger(true);
  m_shrink->addAction(new NumFieldChanger(this, M_SHRINK));

  m_renderTileLabel = new TLabel(this);
  m_renderTileLabel->setText("Render Tile:");
  m_renderTile = new TNumField(this);
  m_renderTile->setIsInteger(true);
  m_renderTile->addAction(new NumFieldChanger(this, M_RTILE));

  m_memChunkLabel = new TLabel(this);
  m_memChunkLabel->setText("Mem Chunk:");
  m_memChunk = new TNumField(this);
  m_memChunk->setIsInteger(true);
  m_memChunk->addAction(new NumFieldChanger(this, M_MCHUNK));

  m_subPixelMoveLabel = new TLabel(this);
  m_subPixelMoveLabel->setText("Subpixel Move:");
  m_subPixelMove = new TOptionMenu(this);
  m_subPixelMove->addOption("None");
  m_subPixelMove->addOption("RGB");
  m_subPixelMove->addOption("All");
  m_subPixelMove->setAction(new TOptionMenuAction<CasmConfigPanel>(
      this, &CasmConfigPanel::onSubPixelMove));

  m_processingLabel = new TLabel(this);
  m_processingLabel->setText("Processing:");
  m_processing = new TOptionMenu(this);
  m_processing->addOption("32 Bit");
  m_processing->addOption("64 Bit");
  m_processing->setAction(new TOptionMenuAction<CasmConfigPanel>(
      this, &CasmConfigPanel::onProcessing));

  m_resampleQualityLabel = new TLabel(this);
  m_resampleQualityLabel->setText("Resample Quality:");
  m_resampleQuality = new TOptionMenu(this);
  m_resampleQuality->addOption("Standard");
  m_resampleQuality->addOption("Improved");
  m_resampleQuality->addOption("High");
  m_resampleQuality->setAction(new TOptionMenuAction<CasmConfigPanel>(
      this, &CasmConfigPanel::onResampleQuality));

  m_focus = new TCheckBox(this, "Constant Focus");
  tconnect<CasmConfigPanel>(*m_focus, this, onConstantFocus);
  m_noOverwrite = new TCheckBox(this, "No Overwrite");
  tconnect<CasmConfigPanel>(*m_noOverwrite, this, onOverwrite);
  m_multimedia = new TCheckBox(this, "Multimedia");
  tconnect<CasmConfigPanel>(*m_multimedia, this, onMultimedia);

  m_column = new TTextField(this);
  m_column->setText("All");
  m_column->disable();
  m_column->addAction(new ColumnFieldChanger(this));
  m_lineart = new TCheckBox(this, "Lineart");
  m_lineart->disable();
  tconnect<CasmConfigPanel>(*m_lineart, this, onLineart);
  m_edgeAliasing = new TCheckBox(this, "Edge Aliasing");
  m_edgeAliasing->disable();
  tconnect<CasmConfigPanel>(*m_edgeAliasing, this, onEdgeAliasing);
  m_thickness = new TNumField(this);
  m_thickness->setIsInteger(true);
  m_thickness->setRange(0, 15);
  m_thickness->setValue(8);
  m_thickness->disable();
  m_thickness->addAction(new NumFieldChanger(this, M_THICK));

  m_gamma = new TCheckBox(this, "Gamma");
  tconnect<CasmConfigPanel>(*m_gamma, this, onGamma);
  m_gammaValue = new TNumField(this);
  m_gammaValue->setValue(0);
  m_gammaValue->disable();
  m_gammaValue->addAction(new NumFieldChanger(this, M_GAMMA));

  m_clap = new TCheckBox(this, "Add Clap");
  // tconnect<CasmConfigPanel>(*m_clap, this, onClap);
  /*
m_outputScriptLabel = new TLabel(this);
m_outputScriptLabel->setText("Output Script:");
m_outputScript = new TOptionMenu(this);
m_outputScript->addOption("None");
//m_subPixelMove->setAction(new TOptionMenuAction<CasmConfigPanel>(this,
&CasmConfigPanel::onOutputScript));
m_entryPointLabel = new TLabel(this);
m_entryPointLabel->setText("Entry Point:");
m_entryPoint = new TTextField(this);
m_entryPoint->setText("");
*/
}

//------------------------------------------------------------------------------

void CasmConfigPanel::configureNotify(const TDimension &d) {
  int x0 = 20;
  int y  = d.ly - 1 - 10;

  int x = x0;

  m_setupLabel->setGeometry(x, y - 15, x + 100, y);
  m_setupTextField->setGeometry(x + 104, y - 15, x + 650, y);
  m_setupBrowseBtn->setGeometry(x + 660, y - 15, x + 680, y);

  y -= 60;

  m_chunkSizeLabel->setGeometry(x, y - 15, x + 100, y);
  m_chunkSize->setGeometry(x + 110, y - 15, x + 156, y);

  y -= 40;

  m_fromLabel->setGeometry(x, y - 15, x + 50, y);
  m_from->setGeometry(x + 54, y - 15, x + 100, y);
  x += 120;

  m_toLabel->setGeometry(x, y - 15, x + 80, y);
  m_to->setGeometry(x + 84, y - 15, x + 130, y);
  x += 150;

  m_stepLabel->setGeometry(x, y - 15, x + 80, y);
  m_step->setGeometry(x + 84, y - 15, x + 130, y);

  ///
  x = x0;
  y -= 40;

  m_shrinkLabel->setGeometry(x, y - 15, x + 50, y);
  m_shrink->setGeometry(x + 54, y - 15, x + 100, y);
  x += 120;

  m_renderTileLabel->setGeometry(x, y - 15, x + 80, y);
  m_renderTile->setGeometry(x + 84, y - 15, x + 130, y);
  x += 150;

  m_memChunkLabel->setGeometry(x, y - 15, x + 80, y);
  m_memChunk->setGeometry(x + 84, y - 15, x + 130, y);

  x = x0;
  y -= 40;

  m_subPixelMoveLabel->setGeometry(x, y - 15, x + 94, y);
  m_subPixelMove->setGeometry(x + 98, y - 15, x + 160, y);
  x += 180;

  m_processingLabel->setGeometry(x, y - 15, x + 74, y);
  m_processing->setGeometry(x + 78, y - 15, x + 140, y);
  x += 160;

  m_resampleQualityLabel->setGeometry(x, y - 15, x + 114, y);
  m_resampleQuality->setGeometry(x + 118, y - 15, x + 200, y);

  x = x0;
  y -= 40;

  m_focus->setGeometry(x, y - 15, x + 90, y);
  x += 120;
  m_noOverwrite->setGeometry(x, y - 15, x + 80, y);
  x += 110;
  m_multimedia->setGeometry(x, y - 15, x + 80, y);
  x += 90;
  m_column->setGeometry(x, y - 15, x + 20, y);
  x += 30;
  m_lineart->setGeometry(x, y - 15, x + 60, y);
  x += 65;
  m_edgeAliasing->setGeometry(x, y - 15, x + 80, y);
  x += 90;
  m_thickness->setGeometry(x, y - 15, x + 30, y);
  x += 50;
  m_gamma->setGeometry(x, y - 15, x + 60, y);
  x += 70;
  m_gammaValue->setGeometry(x, y - 15, x + 30, y);

  x = x0;
  y -= 40;

  m_clap->setGeometry(x, y - 15, x + 90, y);
  /*
x += 120;
m_outputScriptLabel->setGeometry(x, y-15, x+94, y);
m_outputScript->setGeometry(x+98, y-15, x+160, y);
x +=180;

m_entryPointLabel->setGeometry(x, y-15, x+94, y);
m_entryPoint->setGeometry(x+98, y-15, x+160, y);
*/
}

//------------------------------------------------------------------------------

void CasmConfigPanel::setTask(CasmTask *task) {
  m_task = task;

  m_chunkSize->setValue(m_task->m_taskChunksize);
  m_from->setValue(m_task->m_start);
  m_to->setValue(m_task->m_end);
  m_step->setValue(m_task->m_step);

  m_shrink->setValue(m_task->m_reduction);
  m_renderTile->setValue(m_task->m_renderTile);
  m_memChunk->setValue(m_task->m_memChunk);

  string move;
  if (m_task->m_moveType == CasmTask::M_INTMOVE)
    move = "None";
  else if (m_task->m_moveType == CasmTask::M_BG_FRACMOVE)
    move = "RGB";
  else
    move = "All";
  m_subPixelMove->setText(move);

  string bits;
  if (m_task->m_prec == 32)
    bits = "32 Bit";
  else
    bits = "64 Bit";
  m_processing->setText(bits);

  string quality = "";
  if (m_task->m_restype == CasmTask::RES_TYPE_STANDARD)
    quality = "Standard";
  else if (m_task->m_restype == CasmTask::RES_TYPE_IMPROVED)
    quality = "Improved";
  else if (m_task->m_restype == CasmTask::RES_TYPE_HIGH)
    quality = "High";
  m_resampleQuality->setText(quality);

  m_focus->select(m_task->m_cf);
  m_noOverwrite->select(m_task->m_noOverwrite);
  m_multimedia->select(m_task->m_multimedia);

  if (m_task->m_numColumn < 0)
    m_column->setText("All");
  else
    m_column->setText(toString(m_task->m_numColumn));

  if (m_task->m_lineart < 0) {
    m_lineart->disable();
    m_lineart->select(false);
  } else {
    m_lineart->enable();
    m_lineart->select(true);
  }

  if (m_task->m_edgeAliasing < 0) {
    m_edgeAliasing->disable();
    m_edgeAliasing->select(false);
  } else {
    m_edgeAliasing->enable();
    m_edgeAliasing->select(true);
  }
  if (m_task->m_edgeAliasing >= 0 || m_task->m_lineart >= 0)
    m_thickness->setValue(tmax(m_task->m_edgeAliasing, m_task->m_lineart));
  else
    m_thickness->setValue(8);

  if (m_task->m_gamma < 0.) {
    m_gamma->select(false);
    m_gammaValue->setValue(0.0);
    m_gammaValue->disable();
  } else {
    m_gamma->select(true);
    m_gammaValue->setValue(m_task->m_gamma);
    m_gammaValue->enable();
  }
}

//------------------------------------------------------------------------------

CasmTask *CasmConfigPanel::getTask() const { return m_task; }

//------------------------------------------------------------------------------

void CasmConfigPanel::onNumField(double v, int type) {
  switch (type) {
  case M_FROM:
    m_task->m_start = (int)v;
    break;
  case M_TO:
    m_task->m_end = (int)v;
    break;
  case M_STEP:
    m_task->m_step = (int)v;
    break;
  case M_SHRINK:
    m_task->m_reduction = (int)v;
    break;
  case M_RTILE:
    m_task->m_renderTile = (int)v;
    break;
  case M_MCHUNK:
    m_task->m_memChunk = (int)v;
    break;
  case M_THICK:
    if (m_lineart->isSelected()) m_task->m_lineart           = (int)v;
    if (m_edgeAliasing->isSelected()) m_task->m_edgeAliasing = (int)v;
    break;
  case M_GAMMA:
    m_task->m_gamma = v;
    break;
  case M_CHUNKSIZE:
    m_task->m_taskChunksize = (int)v;
    break;
  }
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onColumnField(std::wstring value) {
  if (value == toWideString("") || value == toWideString("ALL") ||
      value == toWideString("All") || value == toWideString("all") ||
      value == toWideString("AL") || value == toWideString("Al") ||
      value == toWideString("a") || value == toWideString("A"))
    m_task->m_numColumn = -1;
  else if (is_num(toString(value)))
    m_task->m_numColumn = atoi(toString(value).c_str());
  else {
    m_task->m_numColumn = -1;
    m_column->setText("All");
    TMessage::error("Only \"All\" or a number is a valid argument");
  }
}

//------------------------------------------------------------------------------

void CasmConfigPanel::browseSetupFiles() {
  static FileBrowserPopup *popup = 0;

  if (!popup) {
    CasmConfigPanel *csp = this;
    vector<string> fileTypes;
    fileTypes.push_back("setup");
    popup = new FileBrowserPopup(csp, fileTypes);

    popup->setOkAction(new TFileBrowserPopupAction<CasmConfigPanel>(
        csp, &CasmConfigPanel::loadSetup));
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
  popup->setCaption("Load Setup");
}

//------------------------------------------------------------------------------

void CasmConfigPanel::loadSetup(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) {
    string msg = toString(fp.getWideString()) + "\n";
    msg += "File not found\n";
    msg += "Please verify that the path and file name are correct";
    m_setupTextField->setText("");
    TMessage::error(msg);
    return;
  }
  Application::instance()->setCurrentFolder(fp.getParentDir());

  string uncString = convertToUncString(fp);

  m_setupTextField->setText(uncString);

#ifdef WIN32
  int x = uncString.find("\\", 2);
  while (x != string::npos) {
    uncString.replace(x, 1, "/", 0, 1);
    x = uncString.find("\\", x + 1);
  }
#endif

  m_task->m_setupFile = uncString;
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onMultimedia(TCheckBox *box, bool on) {
  if (on) {
    m_task->m_multimedia = true;
    m_column->enable();
    m_lineart->enable();
    m_edgeAliasing->enable();
    if (m_lineart->isSelected() || m_edgeAliasing->isSelected())
      m_thickness->enable();
    else
      m_thickness->disable();
  } else {
    m_task->m_multimedia = false;
    m_column->disable();
    m_lineart->disable();
    m_lineart->select(false);
    m_edgeAliasing->disable();
    m_edgeAliasing->select(false);
    m_thickness->setValue(8);
    m_thickness->disable();
    m_task->m_lineart = m_task->m_edgeAliasing = -1;
  }
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onLineart(TCheckBox *box, bool on) {
  if (on) {
    m_task->m_lineart      = (int)m_thickness->getValue();
    m_task->m_edgeAliasing = -1;
    m_lineart->enable();
    m_edgeAliasing->disable();
    m_thickness->enable();
  } else {
    m_task->m_lineart = -1;
    m_lineart->enable();
    m_edgeAliasing->enable();
    m_thickness->disable();
  }
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onEdgeAliasing(TCheckBox *box, bool on) {
  if (on) {
    m_task->m_edgeAliasing = (int)m_thickness->getValue();
    m_task->m_lineart      = -1;
    m_lineart->disable();
    m_edgeAliasing->enable();
    m_thickness->enable();
  } else {
    m_task->m_edgeAliasing = -1;
    m_lineart->enable();
    m_edgeAliasing->enable();
    m_thickness->disable();
  }
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onConstantFocus(TCheckBox *box, bool on) {
  m_task->m_cf = on;
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onOverwrite(TCheckBox *box, bool on) {
  m_task->m_noOverwrite = on;
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onGamma(TCheckBox *box, bool on) {
  if (on)
    m_task->m_gamma = m_gammaValue->getValue();
  else
    m_task->m_gamma = -1.;
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onSubPixelMove(string move) {
  if (move == "None")
    m_task->m_moveType = CasmTask::M_INTMOVE;
  else if (move == "RGB")
    m_task->m_moveType = CasmTask::M_BG_FRACMOVE;
  else
    m_task->m_moveType = CasmTask::M_FRACMOVE;
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onProcessing(string bits) {
  if (bits == "32 Bit")
    m_task->m_prec = 32;
  else
    m_task->m_prec = 64;
}

//------------------------------------------------------------------------------

void CasmConfigPanel::onResampleQuality(string quality) {
  if (quality == "Standard")
    m_task->m_restype = CasmTask::RES_TYPE_STANDARD;
  else if (quality == "Improved")
    m_task->m_restype = CasmTask::RES_TYPE_IMPROVED;
  else if (quality == "High")
    m_task->m_restype = CasmTask::RES_TYPE_HIGH;
  else
    m_task->m_restype = CasmTask::RES_TYPE_NONE;
}
