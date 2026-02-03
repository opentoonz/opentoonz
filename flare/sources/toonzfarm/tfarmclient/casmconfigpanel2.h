#pragma once

#ifndef CASMCONFIGPANEL2_H
#define CASMCONFIGPANEL2_H

#include "submitpage.h"

#include "tw/tw.h"
#include "tw/textfield.h"

// forward declarations
class TLabel;
class TNumField;
class TOptionMenu;
class TButton;
class TFilePath;
class TCheckBox;
class CasmTask2;
class CasmConfigPanel2;

//==============================================================================

class FilePathField : public TTextField {
  CasmConfigPanel2 *m_page;

public:
  FilePathField(TWidget *parent, string name = "");

  void onFocusChange(bool on);
  void close();
  void keyDown(int key, unsigned long flags, const TPoint &p);
};

//==============================================================================

class CasmConfigPanel2 : public TaskConfigPanel {
public:
  CasmConfigPanel2(TWidget *parent);

  void configureNotify(const TDimension &d);

  void setTask(SubmitPageTask *task);
  SubmitPageTask *getTask() const;

  void loadSetup(const TFilePath &fp);
  void onNumField(double v, int type);
  void onColumnField(string value, int type);

private:
  CasmTask2 *m_task;

  TLabel *m_setupLabel;
  FilePathField *m_setupTextField;
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
