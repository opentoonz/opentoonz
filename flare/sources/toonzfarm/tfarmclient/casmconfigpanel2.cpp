

#include "casmconfigpanel2.h"
#include "filebrowserpopup.h"
#include "application.h"

#include "tw/mainshell.h"
#include "tw/label.h"
#include "tw/optionmenu.h"
#include "tw/button.h"
#include "tw/checkbox.h"
#include "tw/keycodes.h"

#include "tconvert.h"
#include "util.h"
#include "tsystem.h"

#include "tw/message.h"

using namespace TwConsts;

//==============================================================================

FilePathField::FilePathField(TWidget *parent, string name)
    : TTextField(parent, name) {
  m_page = dynamic_cast<CasmConfigPanel2 *>(parent);
}

//------------------------------------------------------------------------------

void FilePathField::onFocusChange(bool on) {
  if (!on) close();
  TWidget::onFocusChange(on);
}

//------------------------------------------------------------------------------

void FilePathField::close() {
  static string oldText;
  if (oldText != toString(m_text)) {
    TFilePath fp(m_text);
    try {
      string uncString;
      try {
        uncString = convertToUncString(fp);
        m_text    = toWideString(uncString);
        m_page->loadSetup(TFilePath(uncString));
      } catch (TException &) {
        TFileStatus fs(fp);
        if (fs.doesExist() && !fs.isDirectory())
          m_page->loadSetup(TFilePath(toString(fp.getWideString())));
        else {
          CasmTask2 *task = dynamic_cast<CasmTask2 *>(m_page->getTask());
          if (task) task->m_setupFile = toString(m_text);
        }
      }
      oldText = toString(m_text);
    } catch (...) {
      TMessage::error("boh");
    }
  }
  invalidate();
}

//------------------------------------------------------------------------------

void FilePathField::keyDown(int key, unsigned long flags, const TPoint &p) {
  if (key == TK_Return)
    close();
  else
    TTextField::keyDown(key, flags, p);
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

enum textFieldType { M_TCOLUMN = 0, M_TSETUP, M_TENTRYPOINT };

namespace {

class NumFieldChanger : public TNumField::Action {
  CasmConfigPanel2 *m_ccp;
  numFieldType m_type;

public:
  NumFieldChanger(CasmConfigPanel2 *ccp, numFieldType type)
      : m_ccp(ccp), m_type(type) {}

  void sendCommand(const TNumField::Event &ev);
};

void NumFieldChanger::sendCommand(const TNumField::Event &ev) {
  m_ccp->onNumField(ev.m_value, m_type);
}

class ColumnFieldChanger : public TTextField::Action {
  CasmConfigPanel2 *m_ccp;
  textFieldType m_type;

public:
  ColumnFieldChanger(CasmConfigPanel2 *ccp, textFieldType type)
      : m_ccp(ccp), m_type(type) {}

  void sendCommand(std::wstring value) {
    m_ccp->onColumnField(toString(value), m_type);
  }
};
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

//==============================================================================

CasmConfigPanel2::CasmConfigPanel2(TWidget *parent)
    : TaskConfigPanel(parent), m_task(0) {
  m_chunkSizeLabel = new TLabel(this);
  m_chunkSizeLabel->setText("Task Chunk Size:");
  m_chunkSize = new TNumField(this);
  m_chunkSize->setIsInteger(true);
  m_chunkSize->setRange(1, std::numeric_limits<int>::max());
  m_chunkSize->addAction(new NumFieldChanger(this, M_CHUNKSIZE));

  m_setupLabel = new TLabel(this);
  m_setupLabel->setText("Setup Path:");
  m_setupTextField = new FilePathField(this);
  // m_setupTextField->addAction(new ColumnFieldChanger(this, M_TSETUP));
  m_setupBrowseBtn = new TButton(this);
  m_setupBrowseBtn->setTitle("...");
  tconnect(*m_setupBrowseBtn, this, &CasmConfigPanel2::browseSetupFiles);

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
  m_subPixelMove->setAction(new TOptionMenuAction<CasmConfigPanel2>(
      this, &CasmConfigPanel2::onSubPixelMove));

  m_processingLabel = new TLabel(this);
  m_processingLabel->setText("Processing:");
  m_processing = new TOptionMenu(this);
  m_processing->addOption("32 Bit");
  m_processing->addOption("64 Bit");
  m_processing->setAction(new TOptionMenuAction<CasmConfigPanel2>(
      this, &CasmConfigPanel2::onProcessing));

  m_resampleQualityLabel = new TLabel(this);
  m_resampleQualityLabel->setText("Resample Quality:");
  m_resampleQuality = new TOptionMenu(this);
  m_resampleQuality->addOption("Standard");
  m_resampleQuality->addOption("Improved");
  m_resampleQuality->addOption("High");
  m_resampleQuality->setAction(new TOptionMenuAction<CasmConfigPanel2>(
      this, &CasmConfigPanel2::onResampleQuality));

  m_focus = new TCheckBox(this, "Constant Focus");
  tconnect<CasmConfigPanel2>(*m_focus, this, onConstantFocus);
  m_noOverwrite = new TCheckBox(this, "No Overwrite");
  tconnect<CasmConfigPanel2>(*m_noOverwrite, this, onOverwrite);
  m_multimedia = new TCheckBox(this, "Multimedia");
  tconnect<CasmConfigPanel2>(*m_multimedia, this, onMultimedia);

  m_column = new TTextField(this);
  m_column->setText("All");
  m_column->disable();
  m_column->addAction(new ColumnFieldChanger(this, M_TCOLUMN));
  m_lineart = new TCheckBox(this, "Lineart");
  m_lineart->disable();
  tconnect<CasmConfigPanel2>(*m_lineart, this, onLineart);
  m_edgeAliasing = new TCheckBox(this, "Edge Aliasing");
  m_edgeAliasing->disable();
  tconnect<CasmConfigPanel2>(*m_edgeAliasing, this, onEdgeAliasing);
  m_thickness = new TNumField(this);
  m_thickness->setIsInteger(true);
  m_thickness->setRange(0, 15);
  m_thickness->setValue(8);
  m_thickness->disable();
  m_thickness->addAction(new NumFieldChanger(this, M_THICK));

  m_gamma = new TCheckBox(this, "Gamma");
  tconnect<CasmConfigPanel2>(*m_gamma, this, onGamma);
  m_gammaValue = new TNumField(this);
  m_gammaValue->setValue(0);
  m_gammaValue->disable();
  m_gammaValue->addAction(new NumFieldChanger(this, M_GAMMA));

  m_clap = new TCheckBox(this, "Add Clap");
  // tconnect<CasmConfigPanel2>(*m_clap, this, onClap);
  /*
m_outputScriptLabel = new TLabel(this);
m_outputScriptLabel->setText("Output Script:");
m_outputScript = new TOptionMenu(this);
m_outputScript->addOption("None");
//m_subPixelMove->setAction(new TOptionMenuAction<CasmConfigPanel2>(this,
&CasmConfigPanel2::onOutputScript));
m_entryPointLabel = new TLabel(this);
m_entryPointLabel->setText("Entry Point:");
m_entryPoint = new TTextField(this);
m_entryPoint->setText("");
m_entryPoint->addAction(new ColumnFieldChanger(this, M_TENTRYPOINT));
*/
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::configureNotify(const TDimension &d) {
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

  x += 8;
  m_focus->setGeometry(x, y - 15, x + 90, y);
  x += 120;
  m_noOverwrite->setGeometry(x, y - 15, x + 80, y);
  x += 110;
  m_gamma->setGeometry(x, y - 15, x + 60, y);
  x += 70;
  m_gammaValue->setGeometry(x, y - 15, x + 30, y);
  x += 60;
  m_clap->setGeometry(x, y - 15, x + 90, y);

  x = x0;
  y -= 40;

  x += 8;
  m_multimedia->setGeometry(x, y - 15, x + 80, y);
  x += 90;
  m_column->setGeometry(x, y - 15, x + 20, y);
  x += 30;
  m_lineart->setGeometry(x, y - 15, x + 60, y);
  x += 65;
  m_edgeAliasing->setGeometry(x, y - 15, x + 80, y);
  x += 90;
  m_thickness->setGeometry(x, y - 15, x + 30, y);
  /*
x = x0;
y -=40;
m_outputScriptLabel->setGeometry(x, y-15, x+94, y);
m_outputScript->setGeometry(x+98, y-15, x+160, y);
x +=180;

m_entryPointLabel->setGeometry(x, y-15, x+94, y);
m_entryPoint->setGeometry(x+98, y-15, x+160, y);
*/
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::setTask(SubmitPageTask *task) {
  m_task = dynamic_cast<CasmTask2 *>(task);
  assert(m_task);

  m_setupTextField->setText(m_task->m_setupFile);
  m_chunkSize->setValue(m_task->m_taskChunksize);
  m_from->setValue(m_task->m_start);
  m_to->setValue(m_task->m_end);
  m_step->setValue(m_task->m_step);

  m_shrink->setValue(m_task->m_reduction);
  m_renderTile->setValue(m_task->m_renderTile);
  m_memChunk->setValue(m_task->m_memChunk);

  string move;
  if (m_task->m_moveType == CasmTask2::M_INTMOVE)
    move = "None";
  else if (m_task->m_moveType == CasmTask2::M_BG_FRACMOVE)
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
  if (m_task->m_restype == CasmTask2::RES_TYPE_STANDARD)
    quality = "Standard";
  else if (m_task->m_restype == CasmTask2::RES_TYPE_IMPROVED)
    quality = "Improved";
  else if (m_task->m_restype == CasmTask2::RES_TYPE_HIGH)
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

SubmitPageTask *CasmConfigPanel2::getTask() const { return m_task; }

//------------------------------------------------------------------------------

void CasmConfigPanel2::onNumField(double v, int type) {
  if (!m_task) return;

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

void CasmConfigPanel2::onColumnField(string value, int type) {
  if (!m_task) return;

  switch (type) {
  case M_TCOLUMN:
    if (value == "" || value == "ALL" || value == "All" || value == "all" ||
        value == "AL" || value == "Al" || value == "a" || value == "A")
      m_task->m_numColumn = -1;
    else if (is_num(value))
      m_task->m_numColumn = atoi(value.c_str());
    else {
      m_task->m_numColumn = -1;
      m_column->setText("All");
      TMessage::error("Only \"All\" or a number is a valid argument");
    }
    break;
  case M_TSETUP:
    m_task->m_setupFile = value;
    break;
  default:
    break;
  }
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::browseSetupFiles() {
  static FileBrowserPopup *popup = 0;

  if (!popup) {
    CasmConfigPanel2 *csp = this;
    vector<string> fileTypes;
    fileTypes.push_back("setup");
    popup = new FileBrowserPopup(csp, fileTypes);

    popup->setOkAction(new TFileBrowserPopupAction<CasmConfigPanel2>(
        csp, &CasmConfigPanel2::loadSetup));
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
  popup->setCaption("Load Setup");
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::loadSetup(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) {
    string msg = toString(fp.getWideString()) + "\n";
    msg += "File not found\n";
    msg += "Please verify that the path and file name are correct";
    m_setupTextField->setText("");
    TMessage::error(msg);
    return;
  }

  string uncString = toString(fp.getWideString());
  if (toString(m_setupTextField->getText()) != uncString) try {
      uncString = convertToUncString(fp);
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
      return;
    }

  Application::instance()->setCurrentFolder(fp.getParentDir());

  m_setupTextField->setText(uncString);

#ifdef WIN32
  int x = uncString.find("\\", 2);
  while (x != string::npos) {
    uncString.replace(x, 1, "/", 0, 1);
    x = uncString.find("\\", x + 1);
  }
#endif

  if (m_task) m_task->m_setupFile = uncString;
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::onMultimedia(TCheckBox *box, bool on) {
  if (!m_task) return;

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

void CasmConfigPanel2::onLineart(TCheckBox *box, bool on) {
  if (!m_task) return;

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

void CasmConfigPanel2::onEdgeAliasing(TCheckBox *box, bool on) {
  if (!m_task) return;

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

void CasmConfigPanel2::onConstantFocus(TCheckBox *box, bool on) {
  if (!m_task) return;
  m_task->m_cf = on;
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::onOverwrite(TCheckBox *box, bool on) {
  if (!m_task) return;

  m_task->m_noOverwrite = on;
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::onGamma(TCheckBox *box, bool on) {
  if (!m_task) return;

  if (on)
    m_task->m_gamma = m_gammaValue->getValue();
  else
    m_task->m_gamma = -1.;
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::onSubPixelMove(string move) {
  if (!m_task) return;

  if (move == "None")
    m_task->m_moveType = CasmTask2::M_INTMOVE;
  else if (move == "RGB")
    m_task->m_moveType = CasmTask2::M_BG_FRACMOVE;
  else
    m_task->m_moveType = CasmTask2::M_FRACMOVE;
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::onProcessing(string bits) {
  if (!m_task) return;

  if (bits == "32 Bit")
    m_task->m_prec = 32;
  else
    m_task->m_prec = 64;
}

//------------------------------------------------------------------------------

void CasmConfigPanel2::onResampleQuality(string quality) {
  if (!m_task) return;

  if (quality == "Standard")
    m_task->m_restype = CasmTask2::RES_TYPE_STANDARD;
  else if (quality == "Improved")
    m_task->m_restype = CasmTask2::RES_TYPE_IMPROVED;
  else if (quality == "High")
    m_task->m_restype = CasmTask2::RES_TYPE_HIGH;
  else
    m_task->m_restype = CasmTask2::RES_TYPE_NONE;
}
