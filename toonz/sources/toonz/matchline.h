#pragma once

// TnzCore includes
#include "tfilepath.h"
#include "tgeometry.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include "toonzqt/styleindexlineedit.h"
#include "toonzqt/filefield.h"

// STD includes
#include <set>

//==================================================

//    Forward declarations

class QRadioButton;
class TPaletteHandle;
class TXshSimpleLevel;
class TXsheet;

//==================================================

class MergeCmappedDialog final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::FileField *m_saveInFileFld;
  DVGui::LineEdit *m_fileNameFld;
  TFilePath &m_levelPath;

public:
  MergeCmappedDialog(TFilePath &);

  void accept() override;

protected slots:

  void onPathChanged() {}
  void onNameChanged() {}
};

//----------------------------------------------------------------

class MatchlinesDialog final : public DVGui::Dialog {
  Q_OBJECT

  QRadioButton *m_button1, *m_button2, *m_button3;
  DVGui::StyleIndexLineEdit *m_inkIndex;
  DVGui::IntField *m_inkPrevalence;
  TPaletteHandle *m_pltHandle;

  QPushButton *m_lup_noGapButton;
  QPushButton *m_rup_noGapButton;
  QPushButton *m_lup_gapButton;
  QPushButton *m_rup_gapButton;
  TXsheet *m_currentXsheet;

protected slots:

  void onChooseInkClicked(bool value);
  void onLineStackButtonPressed(int id);
  void onInkPrevalenceChanged(bool isDragging);
  void accept() override;

protected:
  void showEvent(QShowEvent *e) override;

public:
  MatchlinesDialog();

  int exec(TPalette *plt);
  int getInkIndex();
  int getInkPrevalence();
};

//----------------------------------------------------------------

class DeleteInkDialog final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_inkIndex;
  DVGui::LineEdit *m_frames;

public:
  DeleteInkDialog(const QString &str, int inkIndex);

  void setRange(const QString &str);

  std::vector<int> getInkIndexes();
  std::vector<TFrameId> getFrames();
};

//==========================================================================

void deleteMatchlines(TXshSimpleLevel *sl, const std::set<TFrameId> &fids);
void deleteInk(TXshSimpleLevel *sl, const std::set<TFrameId> &fids);
void mergeColumns(int dstColumn, int srcColumn, bool isRedo);
void mergeColumns(const std::set<int> &columns);
void doMatchlines(int column, int mColumn, int index, int inkPrevalence,
                  int MatchlineSessionId = 0);
void mergeCmapped(int dstColumn, int srcColumn, const QString &fullpath,
                  bool isRedo);
std::vector<int> string2Indexes(const QString &values);
