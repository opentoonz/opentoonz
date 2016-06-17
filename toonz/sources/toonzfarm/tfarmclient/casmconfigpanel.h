#pragma once

#ifndef CASMCONFIGPANEL_H
#define CASMCONFIGPANEL_H

#include "tfilepath.h"

#include "tw/tw.h"

// forward declarations
class TLabel;
class TNumField;
class TOptionMenu;
class TTextField;
class TButton;
class TFilePath;
class TCheckBox;

//==============================================================================

#define DDR_OUTPUTSCRIPT_FILENAME "D@D@R"

class CasmTask {
public:
  CasmTask();
  CasmTask(const string &s, const string &setupFilePath);

  string getCommandLine() const;
  void setCommandLine(const string &cmdLine);
  void setdefaultValue();

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
  string m_casmFile;
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

//==============================================================================

class CasmConfigPanel : public TWidget {
public:
  CasmConfigPanel(TWidget *parent);

  void configureNotify(const TDimension &d);

  void setTask(CasmTask *task);
  CasmTask *getTask() const;

  void loadSetup(const TFilePath &fp);
  void onNumField(double v, int type);
  void onColumnField(std::wstring value);

private:
  CasmTask *m_task;

  TLabel *m_setupLabel;
  TTextField *m_setupTextField;
  TButton *m_setupBrowseBtn;

  TLabel *m_chunkSizeLabel;
  TNumField *m_chunkSize;

  TLabel *m_fromLabel;
  TLabel *m_toLabel;
  TLabel *m_stepLabel;

  TNumField *m_from;
  TNumField *m_to;
  TNumField *m_step;

  TLabel *m_shrinkLabel;
  TLabel *m_renderTileLabel;
  TLabel *m_memChunkLabel;

  TNumField *m_shrink;
  TNumField *m_renderTile;
  TNumField *m_memChunk;

  TLabel *m_subPixelMoveLabel;
  TOptionMenu *m_subPixelMove;

  TLabel *m_processingLabel;
  TOptionMenu *m_processing;

  TLabel *m_resampleQualityLabel;
  TOptionMenu *m_resampleQuality;

  TCheckBox *m_focus;
  TCheckBox *m_noOverwrite;
  TCheckBox *m_multimedia;

  TTextField *m_column;
  TCheckBox *m_lineart;
  TCheckBox *m_edgeAliasing;
  TNumField *m_thickness;

  TCheckBox *m_gamma;
  TNumField *m_gammaValue;

  TCheckBox *m_clap;
  TLabel *m_outputScriptLabel;
  TOptionMenu *m_outputScript;
  TLabel *m_entryPointLabel;
  TTextField *m_entryPoint;

  void browseSetupFiles();
  void onMultimedia(TCheckBox *box, bool on);
  void onLineart(TCheckBox *box, bool on);
  void onEdgeAliasing(TCheckBox *box, bool on);
  void onConstantFocus(TCheckBox *box, bool on);
  void onOverwrite(TCheckBox *box, bool on);
  void onGamma(TCheckBox *box, bool on);
  void onSubPixelMove(string move);
  void onProcessing(string bits);
  void onResampleQuality(string quality);
};

#endif
