

#include "xshcellviewer.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "keyframeselection.h"
#include "filmstripselection.h"
#include "filmstripcommand.h"
#include "xsheetdragtool.h"
#include "iocommand.h"
#include "castselection.h"
#include "cellkeyframeselection.h"
#include "keyframedata.h"
#include "columnselection.h"
#include "orientation.h"

// TnzTools includes
#include "tools/cursormanager.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/tselectionhandle.h"
#include "historytypes.h"
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/sceneproperties.h"
#include "toonz/columnfan.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshnoteset.h"
#include "toonz/txshcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/txshsoundtextlevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tdoublekeyframe.h"

// TnzCore includes
#include "tconvert.h"
#include "tundo.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QUrl>
#include <QToolTip>
#include <QApplication>
#include <QClipboard>

namespace {

bool selectionContainTlvImage(TCellSelection *selection, TXsheet *xsheet,
                              bool onlyTlv = false) {
  int r0, r1, c0, c1;
  selection->getSelectedCells(r0, c0, r1, c1);

  // Verifico se almeno un livello della selezione e' un tlv
  int c, r;
  for (c = c0; c <= c1; c++)
    for (r = r0; r <= r1; r++) {
      TXshCell cell = xsheet->getCell(r, c);
      TXshSimpleLevel *level =
          cell.isEmpty() ? (TXshSimpleLevel *)0 : cell.getSimpleLevel();
      if (!level) continue;

      if (level->getType() != TZP_XSHLEVEL && onlyTlv) return false;
      if (level->getType() == TZP_XSHLEVEL && !onlyTlv) return true;
    }

  return false;
}

//-----------------------------------------------------------------------------

bool selectionContainRasterImage(TCellSelection *selection, TXsheet *xsheet,
                                 bool onlyRaster = false) {
  int r0, r1, c0, c1;
  selection->getSelectedCells(r0, c0, r1, c1);

  // Verifico se almeno un livello della selezione e' un tlv
  int c, r;
  for (c = c0; c <= c1; c++)
    for (r = r0; r <= r1; r++) {
      TXshCell cell = xsheet->getCell(r, c);
      TXshSimpleLevel *level =
          cell.isEmpty() ? (TXshSimpleLevel *)0 : cell.getSimpleLevel();
      if (!level) continue;

      if (level->getType() != OVL_XSHLEVEL && onlyRaster) return false;
      if (level->getType() == OVL_XSHLEVEL && !onlyRaster) return true;
    }

  return false;
}

//-----------------------------------------------------------------------------

bool selectionContainLevelImage(TCellSelection *selection, TXsheet *xsheet) {
  int r0, r1, c0, c1;
  selection->getSelectedCells(r0, c0, r1, c1);
  // Verifico se almeno un livello della selezione e' un tlv, un pli o un
  // fullcolor (!= .psd)
  int c, r;
  for (c = c0; c <= c1; c++)
    for (r = r0; r <= r1; r++) {
      TXshCell cell = xsheet->getCell(r, c);
      TXshSimpleLevel *level =
          cell.isEmpty() ? (TXshSimpleLevel *)0 : cell.getSimpleLevel();
      if (!level) continue;

      std::string ext = level->getPath().getType();
      int type        = level->getType();
      if (type == TZP_XSHLEVEL || type == PLI_XSHLEVEL ||
          (type == OVL_XSHLEVEL && ext != "psd"))
        return true;
    }

  return false;
}

//-----------------------------------------------------------------------------
/*! convert the last one digit of the frame number to alphabet
    Ex.  12 -> 1B    21 -> 2A   30 -> 3
*/
void parse_with_letter(const QString &text, std::wstring &levelName,
                       TFrameId &fid) {
  QRegExp spaces("\\t|\\s");
  QRegExp numbers("\\d+");
  QRegExp caracters("[^\\d+]");
  QString str = text;

  // remove final spaces
  int size = str.size();
  while (str.lastIndexOf(spaces) == size - 1 && size > 0)
    str.remove(str.size() - 1, 1);
  if (str.isEmpty()) {
    levelName = L"";
    fid       = TFrameId::NO_FRAME;
    return;
  }

  int lastSpaceIndex = str.lastIndexOf(spaces);
  // if input only digits / alphabet
  if (lastSpaceIndex == -1) {
    // in case of only level name
    if (str.contains(caracters) && !str.contains(numbers)) {
      levelName = text.toStdWString();
      fid       = TFrameId::NO_FRAME;
    }
    // in case of input frame number + alphabet
    else if (str.contains(numbers) && str.contains(caracters)) {
      levelName = L"";

      QString appendix = str.right(1);

      int appendNum = 0;

      if (appendix == QString('A') || appendix == QString('a'))
        appendNum = 1;
      else if (appendix == QString('B') || appendix == QString('b'))
        appendNum = 2;
      else if (appendix == QString('C') || appendix == QString('c'))
        appendNum = 3;
      else if (appendix == QString('D') || appendix == QString('d'))
        appendNum = 4;
      else if (appendix == QString('E') || appendix == QString('e'))
        appendNum = 5;
      else if (appendix == QString('F') || appendix == QString('f'))
        appendNum = 6;
      else if (appendix == QString('G') || appendix == QString('g'))
        appendNum = 7;
      else if (appendix == QString('H') || appendix == QString('h'))
        appendNum = 8;
      else if (appendix == QString('I') || appendix == QString('i'))
        appendNum = 9;

      str.chop(1);

      if (str.contains(caracters) || !str.contains(numbers)) {
        levelName = text.toStdWString();
        fid       = TFrameId::NO_FRAME;
        return;
      }

      fid = TFrameId(str.toInt() * 10 + appendNum);
    }
    // in case of input only frame number
    else if (str.contains(numbers)) {
      levelName = L"";
      fid       = TFrameId(str.toInt() * 10);
    }
  }
  // if input both the level name and frame number
  else {
    QString lastString = str.right(str.size() - 1 - lastSpaceIndex);
    if (lastString.contains(numbers)) {
      // level name
      QString firstString = str.left(lastSpaceIndex);
      levelName           = firstString.toStdWString();

      // frame number + alphabet
      if (lastString.contains(caracters)) {
        QString appendix = lastString.right(1);

        int appendNum = 0;

        if (appendix == QString('A') || appendix == QString('a'))
          appendNum = 1;
        else if (appendix == QString('B') || appendix == QString('b'))
          appendNum = 2;
        else if (appendix == QString('C') || appendix == QString('c'))
          appendNum = 3;
        else if (appendix == QString('D') || appendix == QString('d'))
          appendNum = 4;
        else if (appendix == QString('E') || appendix == QString('e'))
          appendNum = 5;
        else if (appendix == QString('F') || appendix == QString('f'))
          appendNum = 6;
        else if (appendix == QString('G') || appendix == QString('g'))
          appendNum = 7;
        else if (appendix == QString('H') || appendix == QString('h'))
          appendNum = 8;
        else if (appendix == QString('I') || appendix == QString('i'))
          appendNum = 9;

        lastString.chop(1);

        if (lastString.contains(caracters) || !lastString.contains(numbers)) {
          levelName = text.toStdWString();
          fid       = TFrameId::NO_FRAME;
          return;
        }

        fid = TFrameId(lastString.toInt() * 10 + appendNum);
      }
      // only frame number
      else if (lastString.contains(numbers)) {
        fid = TFrameId(lastString.toInt() * 10);
      }

    } else {
      levelName = text.toStdWString();
      fid       = TFrameId::NO_FRAME;
    }
  }
}

//-----------------------------------------------------------------------------

void parse(const QString &text, std::wstring &levelName, TFrameId &fid) {
  QRegExp spaces("\\t|\\s");
  QRegExp numbers("\\d+");
  QRegExp caracters("[^\\d+]");
  QString str = text;

  // remove final spaces
  int size = str.size();
  while (str.lastIndexOf(spaces) == size - 1 && size > 0)
    str.remove(str.size() - 1, 1);
  if (str.isEmpty()) {
    levelName = L"";
    fid       = TFrameId::NO_FRAME;
    return;
  }
  int lastSpaceIndex = str.lastIndexOf(spaces);
  if (lastSpaceIndex == -1) {
    if (str.contains(numbers) && !str.contains(caracters)) {
      levelName = L"";
      fid       = TFrameId(str.toInt());
    } else if (str.contains(caracters)) {
      levelName = text.toStdWString();
      fid       = TFrameId::NO_FRAME;
    }
  } else {
    QString lastString = str.right(str.size() - 1 - lastSpaceIndex);
    if (lastString.contains(numbers) && !lastString.contains(caracters)) {
      QString firstString = str.left(lastSpaceIndex);
      levelName           = firstString.toStdWString();
      fid                 = TFrameId(lastString.toInt());
    } else if (lastString.contains(caracters)) {
      levelName = text.toStdWString();
      fid       = TFrameId::NO_FRAME;
    }
  }
}

//-----------------------------------------------------------------------------

bool isGlobalKeyFrameWithSameTypeDiffFromLinear(TStageObject *stageObject,
                                                int frame) {
  if (!stageObject->isFullKeyframe(frame)) return false;
  TDoubleKeyframe::Type type =
      stageObject->getParam(TStageObject::T_Angle)->getKeyframeAt(frame).m_type;
  if (type == TDoubleKeyframe::Linear) return false;
  if (type !=
          stageObject->getParam(TStageObject::T_X)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_Y)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_Z)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_SO)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_ScaleX)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_ScaleY)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_Scale)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_Path)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_ShearX)
              ->getKeyframeAt(frame)
              .m_type ||
      type !=
          stageObject->getParam(TStageObject::T_ShearY)
              ->getKeyframeAt(frame)
              .m_type)
    return false;
  return true;
}

//-----------------------------------------------------------------------------

bool rectContainsPos(QList<QRect> rects, QPoint pos) {
  int i;
  for (i = 0; i < rects.size(); i++)
    if (rects.at(i).contains(pos)) return true;
  return false;
}

//-----------------------------------------------------------------------------

bool isGlobalKeyFrameWithSamePrevTypeDiffFromLinear(TStageObject *stageObject,
                                                    int frame) {
  if (!stageObject->isFullKeyframe(frame)) return false;
  TDoubleKeyframe::Type type = stageObject->getParam(TStageObject::T_Angle)
                                   ->getKeyframeAt(frame)
                                   .m_prevType;
  if (type == TDoubleKeyframe::Linear) return false;
  if (type !=
          stageObject->getParam(TStageObject::T_X)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_Y)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_Z)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_SO)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_ScaleX)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_ScaleY)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_Scale)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_Path)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_ShearX)
              ->getKeyframeAt(frame)
              .m_prevType ||
      type !=
          stageObject->getParam(TStageObject::T_ShearY)
              ->getKeyframeAt(frame)
              .m_prevType)
    return false;
  return true;
}

//-----------------------------------------------------------------------------

#ifdef LINETEST

int getParamStep(TStageObject *stageObject, int frame) {
  TDoubleKeyframe keyFrame =
      stageObject->getParam(TStageObject::T_Angle)->getKeyframeAt(frame);
  return keyFrame.m_step;
}

//-----------------------------------------------------------------------------

void setParamStep(int indexKeyframe, int step, TDoubleParam *param) {
  KeyframeSetter setter(param, indexKeyframe);
  setter.setStep(step);
}

#endif
//=============================================================================
// RenameCellUndo
//-----------------------------------------------------------------------------

class RenameCellUndo final : public TUndo {
  int m_row;
  int m_col;
  const TXshCell m_oldCell;
  const TXshCell m_newCell;

public:
  // indices sono le colonne inserite
  RenameCellUndo(int row, int col, TXshCell oldCell, TXshCell newCell)
      : m_row(row), m_col(col), m_oldCell(oldCell), m_newCell(newCell) {}

  void setcell(const TXshCell cell) const {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    assert(xsh);
    if (cell.isEmpty())
      xsh->clearCells(m_row, m_col);
    else
      xsh->setCell(m_row, m_col, cell);
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void undo() const override { setcell(m_oldCell); }

  void redo() const override { setcell(m_newCell); }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Rename Cell  at Column %1  Frame %2")
        .arg(QString::number(m_col + 1))
        .arg(QString::number(m_row + 1));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

class RenameTextCellUndo final : public TUndo {
  int m_row;
  int m_col;
  const TXshCell m_oldCell;
  const TXshCell m_newCell;
  QString m_oldText, m_newText;
  TXshSoundTextLevel *m_level;

public:
  RenameTextCellUndo(int row, int col, TXshCell oldCell, TXshCell newCell,
                     QString oldText, QString newText,
                     TXshSoundTextLevel *level)
      : m_row(row)
      , m_col(col)
      , m_oldCell(oldCell)
      , m_newCell(newCell)
      , m_oldText(oldText)
      , m_newText(newText)
      , m_level(level) {}

  void setcell(const TXshCell cell, QString text = "") const {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    assert(xsh);
    if (cell.isEmpty())
      xsh->clearCells(m_row, m_col);
    else {
      xsh->setCell(m_row, m_col, cell);
      m_level->setFrameText(cell.getFrameId().getNumber() - 1, text);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void undo() const override { setcell(m_oldCell, m_oldText); }

  void redo() const override { setcell(m_newCell, m_newText); }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Change Text at Column %1  Frame %2")
        .arg(QString::number(m_col + 1))
        .arg(QString::number(m_row + 1));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

// display upper-directional smart tab only when pressing ctrl key
bool isCtrlPressed = false;

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// RenameCellField
//-----------------------------------------------------------------------------

RenameCellField::RenameCellField(QWidget *parent, XsheetViewer *viewer)
    : QLineEdit(parent), m_viewer(viewer), m_row(-1), m_col(-1) {
  connect(this, SIGNAL(returnPressed()), SLOT(onReturnPressed()));
  setContextMenuPolicy(Qt::PreventContextMenu);
  setObjectName("RenameCellField");
  setAttribute(Qt::WA_TranslucentBackground, true);
  installEventFilter(this);
}

//-----------------------------------------------------------------------------

void RenameCellField::showInRowCol(int row, int col, bool multiColumnSelected) {
  const Orientation *o = m_viewer->orientation();

  m_viewer->scrollTo(row, col);

  m_row = row;
  m_col = col;

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  setFont(font);
  setAlignment(Qt::AlignRight | Qt::AlignBottom);

  // Se la cella non e' vuota setto il testo
  TXsheet *xsh = m_viewer->getXsheet();

  // adjust text position
  int padding = 3;
  if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
    TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
    int r0, r1;
    if (pegbar && pegbar->getKeyframeRange(r0, r1)) padding += 9;
  }

  // make the field semi-transparent
  QColor bgColor        = m_viewer->getColumnHeadPastelizer();
  QString styleSheetStr = QString(
                              "#RenameCellField { padding-right:%1px; "
                              "background-color:rgba(%2,%3,%4,75); color:%5;}")
                              .arg(padding)
                              .arg(bgColor.red())
                              .arg(bgColor.green())
                              .arg(bgColor.blue())
                              .arg(m_viewer->getTextColor().name());
  setStyleSheet(styleSheetStr);

  TXshCell cell = xsh->getCell(row, col);
  QPoint xy     = m_viewer->positionToXY(CellPosition(row, col)) - QPoint(1, 2);
  if (!cell.isEmpty()) {
    // Do not cover left side of the cell in order to enable grabbing the drag
    // handle
    if (o->isVerticalTimeline()) {
      int dragHandleWidth = o->rect(PredefinedRect::DRAG_HANDLE_CORNER).width();
      setFixedSize(o->cellWidth() - dragHandleWidth, o->cellHeight() + 2);
      move(xy + QPoint(1 + dragHandleWidth, 1));
    } else {
      setFixedSize(o->cellWidth(), o->cellHeight() + 2);
      move(xy + QPoint(1, 1));
    }

    TFrameId fid           = cell.getFrameId();
    std::wstring levelName = cell.m_level->getName();

    // convert the last one digit of the frame number to alphabet
    // Ex.  12 -> 1B    21 -> 2A   30 -> 3
    if (Preferences::instance()->isShowFrameNumberWithLettersEnabled() &&
        cell.m_level->getType() != TXshLevelType::SND_TXT_XSHLEVEL)
      setText(
          (fid.isEmptyFrame() || fid.isNoFrame())
              ? QString::fromStdWString(levelName)
              : (multiColumnSelected)
                    ? m_viewer->getFrameNumberWithLetters(fid.getNumber())
                    : QString::fromStdWString(levelName) + QString(" ") +
                          m_viewer->getFrameNumberWithLetters(fid.getNumber()));
    else {
      std::string frameNumber("");
      if (fid.getNumber() > 0) frameNumber = std::to_string(fid.getNumber());
      if (fid.getLetter() != 0) frameNumber.append(1, fid.getLetter());

      // get text from sound text level
      if (cell.m_level->getType() == TXshLevelType::SND_TXT_XSHLEVEL) {
        TXshSoundTextLevelP textLevel = cell.m_level->getSoundTextLevel();
        if (textLevel) {
          std::string frameText =
              textLevel->getFrameText(fid.getNumber() - 1).toStdString();
          setText(textLevel->getFrameText(fid.getNumber() - 1));
        }
      }
      // other level types
      else {
        setText((frameNumber.empty())
                    ? QString::fromStdWString(levelName)
                    : (multiColumnSelected)
                          ? QString::fromStdString(frameNumber)
                          : QString::fromStdWString(levelName) + QString(" ") +
                                QString::fromStdString(frameNumber));
      }
    }
    selectAll();
  }
  // clear the field if the empty cell is clicked
  else {
    setFixedSize(o->cellWidth(), o->cellHeight() + 2);
    move(xy + QPoint(1, 1));

    setText("");
  }
  show();
  raise();
  setFocus();
}

//-----------------------------------------------------------------------------

void RenameCellField::renameSoundTextColumn(TXshSoundTextColumn *sndTextCol,
                                            const QString &s) {
  TXsheet *xsheet  = m_viewer->getXsheet();
  QString oldText  = "changeMe";  // text for undo - changed later
  TXshCell cell    = xsheet->getCell(m_row, m_col);
  TXshCell oldCell = cell;
  // the text index is always one less than the frame number
  int textIndex = cell.getFrameId().getNumber() - 1;
  if (!cell.m_level) {  // cell not part of a level
    oldText       = "";
    int lastFrame = sndTextCol->getMaxFrame();
    TXshSoundTextLevel *sndTextLevel;
    TXshCell lastCell;
    TFrameId newId;
    if (lastFrame < 0) {  // no level on column
      sndTextLevel = new TXshSoundTextLevel();
      sndTextLevel->setType(SND_TXT_XSHLEVEL);
      newId = TFrameId(1);
      cell  = TXshCell(sndTextLevel, newId);
      sndTextCol->setCell(m_row, cell);
      textIndex = 0;
    } else {
      TXshCell lastCell                = xsheet->getCell(lastFrame, m_col);
      TXshSoundTextLevel *sndTextLevel = lastCell.m_level->getSoundTextLevel();
      int textSize                     = sndTextLevel->m_framesText.size();
      textIndex                        = textSize;
      newId                            = TFrameId(textSize + 1);
      cell                             = TXshCell(sndTextLevel, newId);
      sndTextCol->setCell(m_row, cell);
    }
  }

  TXshCell prevCell             = xsheet->getCell(m_row - 1, m_col);
  TXshSoundTextLevel *textLevel = cell.m_level->getSoundTextLevel();
  if (oldText == "changeMe")
    oldText = textLevel->getFrameText(cell.getFrameId().getNumber() - 1);
  if (!prevCell.isEmpty()) {
    // check if the previous cell had the same content as the entered text
    // just extend the frame if so
    if (textLevel->getFrameText(prevCell.getFrameId().getNumber() - 1) == s) {
      sndTextCol->setCell(m_row, prevCell);
      RenameTextCellUndo *undo = new RenameTextCellUndo(
          m_row, m_col, oldCell, prevCell, oldText, s, textLevel);
      TUndoManager::manager()->add(undo);
      return;
    }
    // check if the cell was part of an extended frame, but now has different
    // text
    else if (textLevel->getFrameText(textIndex) ==
                 textLevel->getFrameText(prevCell.getFrameId().getNumber() -
                                         1) &&
             textLevel->getFrameText(textIndex) != s) {
      int textSize   = textLevel->m_framesText.size();
      textIndex      = textSize;
      TFrameId newId = TFrameId(textSize + 1);
      cell           = TXshCell(textLevel, newId);
      sndTextCol->setCell(m_row, cell);
    }
  }
  RenameTextCellUndo *undo = new RenameTextCellUndo(m_row, m_col, oldCell, cell,
                                                    oldText, s, textLevel);
  TUndoManager::manager()->add(undo);
  textLevel->setFrameText(textIndex, s);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void RenameCellField::renameCell() {
  QString s            = text();
  std::wstring newName = s.toStdWString();

  setText("");

  std::wstring levelName;
  TFrameId fid;

  TXsheet *xsheet = m_viewer->getXsheet();

  // renaming note level column
  if (xsheet->getColumn(m_col) &&
      xsheet->getColumn(m_col)->getSoundTextColumn()) {
    TXshSoundTextColumn *sndTextCol =
        xsheet->getColumn(m_col)->getSoundTextColumn();
    renameSoundTextColumn(sndTextCol, s);
    return;
  }

  // convert the last one digit of the frame number to alphabet
  // Ex.  12 -> 1B    21 -> 2A   30 -> 3
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
    parse_with_letter(QString::fromStdWString(newName), levelName, fid);
  else
    parse(QString::fromStdWString(newName), levelName, fid);

  bool animationSheetEnabled =
      Preferences::instance()->isAnimationSheetEnabled();
  bool levelDefined =
      xsheet->getCell(m_row, m_col).getSimpleLevel() != 0 ||
      m_row > 0 && xsheet->getCell(m_row - 1, m_col).getSimpleLevel() != 0;

  if (animationSheetEnabled && levelDefined) {
    TXshCell cell       = xsheet->getCell(m_row, m_col);
    TXshSimpleLevel *sl = cell.getSimpleLevel();
    if (sl) {
      QRegExp fidRe("([0-9]+)([a-z]?)");
      if (fidRe.exactMatch(s)) {
#if QT_VERSION >= 0x050500
        fid = TFrameId(fidRe.cap(1).toInt(),
                       fidRe.cap(2) == "" ? 0 : fidRe.cap(2).toLatin1()[0]);
#else
        fid  = TFrameId(fidRe.cap(1).toInt(),
                       fidRe.cap(2) == "" ? 0 : fidRe.cap(2).toAscii()[0]);
#endif
        FilmstripCmd::renumberDrawing(sl, cell.m_frameId, fid);
      }
    }
    return;
  }

  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (!cellSelection) return;

  QList<TXshCell> cells;

  if (levelName == L"") {
    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    bool changed = false;
    // rename cells for each column in the selection
    for (int c = c0; c <= c1; c++) {
      // if there is no level at the current cell, take the level from the
      // previous frames
      // (when editing not empty column)

      TXshCell cell;
      int tmpRow = m_row;
      while (1) {
        cell = xsheet->getCell(tmpRow, c);
        if (!cell.isEmpty() || tmpRow == 0) break;
        tmpRow--;
      }
      TXshLevel *xl = cell.m_level.getPointer();
      if (!xl || (xl->getType() == OVL_XSHLEVEL &&
                  xl->getPath().getFrame() == TFrameId::NO_FRAME)) {
        cells.append(TXshCell());
        continue;
      }
      // if the next upper cell is empty, then make this cell empty too
      if (fid == TFrameId::NO_FRAME)
        fid = (m_row - tmpRow <= 1) ? cell.m_frameId : TFrameId(0);
      cells.append(TXshCell(xl, fid));
      changed = true;
    }
    if (!changed) return;
  } else {
    ToonzScene *scene   = m_viewer->getXsheet()->getScene();
    TLevelSet *levelSet = scene->getLevelSet();
    TXshLevel *xl       = levelSet->getLevel(levelName);
    if (!xl && fid != TFrameId::NO_FRAME) {
      if (animationSheetEnabled) {
        Preferences *pref   = Preferences::instance();
        int levelType       = pref->getDefLevelType();
        xl                  = scene->createNewLevel(levelType, levelName);
        TXshSimpleLevel *sl = xl->getSimpleLevel();
        if (levelType == TZP_XSHLEVEL || levelType == OVL_XSHLEVEL)
          sl->setFrame(fid, sl->createEmptyFrame());
      } else
        xl = scene->createNewLevel(TZI_XSHLEVEL, levelName);
    }
    if (!xl) return;
    cells.append(TXshCell(xl, fid));
  }

  if (fid.getNumber() == 0) {
    TCellSelection::Range range = cellSelection->getSelectedCells();
    cellSelection->deleteCells();
    // revert cell selection
    cellSelection->selectCells(range.m_r0, range.m_c0, range.m_r1, range.m_c1);
  } else if (cells.size() == 1)
    cellSelection->renameCells(cells[0]);
  else
    cellSelection->renameMultiCells(cells);
}

//-----------------------------------------------------------------------------

void RenameCellField::onReturnPressed() {
  renameCell();

  // move the cell selection
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (!cellSelection) return;
  TCellSelection::Range range = cellSelection->getSelectedCells();
  int offset                  = range.m_r1 - range.m_r0 + 1;
  cellSelection->selectCells(range.m_r0 + offset, range.m_c0,
                             range.m_r1 + offset, range.m_c1);
  showInRowCol(m_row + offset, m_col, range.getColCount() > 1);
  m_viewer->updateCells();
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//-----------------------------------------------------------------------------

void RenameCellField::focusOutEvent(QFocusEvent *e) {
  hide();

  QLineEdit::focusOutEvent(e);
}

//-----------------------------------------------------------------------------
// Override shortcut keys for cell selection commands

bool RenameCellField::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() != QEvent::ShortcutOverride)
    return QLineEdit::eventFilter(obj, e);  // return false;

  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (!cellSelection) return QLineEdit::eventFilter(obj, e);

  QKeyEvent *ke = (QKeyEvent *)e;
  std::string keyStr =
      QKeySequence(ke->key() + ke->modifiers()).toString().toStdString();
  QAction *action = CommandManager::instance()->getActionFromShortcut(keyStr);
  if (!action) return QLineEdit::eventFilter(obj, e);

  std::string actionId = CommandManager::instance()->getIdFromAction(action);

  // These are usally standard ctrl/command strokes for text editing.
  // Default to standard behavior and don't execute OT's action while renaming
  // cell if users prefer to do so.
  // Or, always invoke OT's commands when renaming cell even the standard
  // command strokes for text editing.
  // The latter option is demanded by Japanese animation industry in order to
  // gain efficiency for inputting xsheet.
  if (!Preferences::instance()->isShortcutCommandsWhileRenamingCellEnabled() &&
      (actionId == "MI_Undo" || actionId == "MI_Redo" ||
       actionId == "MI_Clear" || actionId == "MI_Copy" ||
       actionId == "MI_Paste" || actionId == "MI_Cut"))
    return QLineEdit::eventFilter(obj, e);

  return TCellSelection::isEnabledCommand(actionId);
}

//-----------------------------------------------------------------------------

void RenameCellField::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    clearFocus();
    return;
  }

  // move the cell selection
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (!cellSelection) {
    QLineEdit::keyPressEvent(event);
    return;
  }

  int r0, c0, r1, c1;
  CellPosition stride(1, 1);  // stride in row and column axes
  cellSelection->getSelectedCells(r0, c0, r1, c1);
  stride.setFrame(cellSelection->getSelectedCells().getRowCount());

  CellPosition offset;
  switch (int key = event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Down:
    offset = m_viewer->orientation()->arrowShift(key);
    break;
  case Qt::Key_Left:
  case Qt::Key_Right:
    // ctrl+left/right arrow for moving cursor to the end in the field
    if (isCtrlPressed &&
        !Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled()) {
      QLineEdit::keyPressEvent(event);
      return;
    }
    offset = m_viewer->orientation()->arrowShift(key);
    break;
  default:
    QLineEdit::keyPressEvent(event);
    return;
    break;
  }

  if (isCtrlPressed &&
      Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled()) {
    if (r0 == r1 && offset.frame() == -1) return;
    if (c0 == c1 && offset.layer() == -1) return;
    cellSelection->selectCells(r0, c0, r1 + offset.frame(),
                               c1 + offset.layer());
  } else {
    CellPosition offset(offset * stride);
    int movedR0   = std::max(0, r0 + offset.frame());
    int movedC0   = std::max(0, c0 + offset.layer());
    int diffFrame = movedR0 - r0;
    int diffLayer = movedC0 - c0;
    // It needs to be discussed - I made not to rename cell with arrow key.
    // 19/Jan/2017 shun-iwasawa
    // renameCell();
    cellSelection->selectCells(r0 + diffFrame, c0 + diffLayer, r1 + diffFrame,
                               c1 + diffLayer);
    showInRowCol(m_row + offset.frame(), m_col + offset.layer(), c1 - c0 > 0);
  }
  m_viewer->updateCells();
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//-----------------------------------------------------------------------------

void RenameCellField::showEvent(QShowEvent *) {
  bool ret = connect(TApp::instance()->getCurrentXsheet(),
                     SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void RenameCellField::hideEvent(QHideEvent *) {
  disconnect(TApp::instance()->getCurrentXsheet(), SIGNAL(xsheetChanged()),
             this, SLOT(onXsheetChanged()));
}

//-----------------------------------------------------------------------------

void RenameCellField::onXsheetChanged() {
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (!cellSelection) {
    hide();
    return;
  }
  TCellSelection::Range range = cellSelection->getSelectedCells();
  showInRowCol(m_row, m_col, range.getColCount() > 1);
}

//=============================================================================
// CellArea
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
CellArea::CellArea(XsheetViewer *parent, Qt::WindowFlags flags)
#else
CellArea::CellArea(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QWidget(parent, flags)
    , m_viewer(parent)
    , m_levelExtenderRect()
    , m_soundLevelModifyRects()
    , m_isPanning(false)
    , m_isMousePressed(false)
    , m_pos(-1, -1)
    , m_tooltip(tr(""))
    , m_renameCell(new RenameCellField(this, m_viewer)) {
  setAcceptDrops(true);
  setMouseTracking(true);
  m_renameCell->hide();
  setFocusPolicy(Qt::NoFocus);
}

//-----------------------------------------------------------------------------

CellArea::~CellArea() {}

//-----------------------------------------------------------------------------

DragTool *CellArea::getDragTool() const { return m_viewer->getDragTool(); }
void CellArea::setDragTool(DragTool *dragTool) {
  m_viewer->setDragTool(dragTool);
}

//-----------------------------------------------------------------------------

void CellArea::drawCells(QPainter &p, const QRect toBeUpdated) {
  TXsheet *xsh         = m_viewer->getXsheet();
  const Orientation *o = m_viewer->orientation();
  ColumnFan *columnFan = xsh->getColumnFan(o);

  // selected cells range
  TCellSelection *cellSelection = m_viewer->getCellSelection();
  int rS0, cS0, rS1, cS1;
  if (!cellSelection->isEmpty())
    cellSelection->getSelectedCells(rS0, cS0, rS1, cS1);
  // visible cells range
  CellRange visible = m_viewer->xyRectToRange(toBeUpdated);
  int r0, r1, c0, c1;  // range of visible rows and columns
  r0 = visible.from().frame();
  r1 = visible.to().frame();
  c0 = visible.from().layer();
  c1 = visible.to().layer();
  if (!m_viewer->orientation()->isVerticalTimeline()) {
    int colCount = qMax(1, xsh->getColumnCount());
    c1           = qMin(c1, colCount - 1);
  }

  drawNonEmptyBackground(p);

  drawSelectionBackground(p);

  // marker interval every 6 frames
  int distance, offset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, offset);
  if (distance == 0) distance = 6;

  int currentRow = m_viewer->getCurrentRow();
  int col, row;

  int drawLeft  = std::max(1, toBeUpdated.left());
  int drawRight = std::min(width(), toBeUpdated.right());

  int drawTop    = std::max(1, toBeUpdated.top());
  int drawBottom = std::min(height() - 2, toBeUpdated.bottom());
  QRect drawingArea(QPoint(drawLeft, drawTop), QPoint(drawRight, drawBottom));
  NumberRange frameSide = m_viewer->orientation()->frameSide(drawingArea);

  // for each layer
  m_soundLevelModifyRects.clear();
  for (col = c0; col <= c1; col++) {
    // x in vertical timeline / y in horizontal
    int layerAxis = m_viewer->columnToLayerAxis(col);

    if (!columnFan->isActive(col)) {  // folded column
      drawFoldedColumns(p, layerAxis, frameSide);

      if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
          !m_viewer->orientation()->isVerticalTimeline() &&
          Preferences::instance()->isCurrentTimelineIndicatorEnabled()) {
        row       = m_viewer->getCurrentRow();
        QPoint xy = m_viewer->positionToXY(CellPosition(row, col));
        int x     = xy.x();
        int y     = xy.y();
        if (row == 0) {
          if (m_viewer->orientation()->isVerticalTimeline())
            xy.setY(xy.y() + 1);
          else
            xy.setX(xy.x() + 1);
        }
        drawCurrentTimeIndicator(p, xy, true);
      }
      continue;
    }

    TXshColumn *column     = xsh->getColumn(col);
    bool isColumn          = (column) ? true : false;
    bool isSoundColumn     = false;
    bool isPaletteColumn   = false;
    bool isSoundTextColumn = false;
    if (isColumn) {
      isSoundColumn     = column->getSoundColumn() != 0;
      isPaletteColumn   = column->getPaletteColumn() != 0;
      isSoundTextColumn = column->getSoundTextColumn() != 0;
    }
    // check if the column is reference
    bool isReference = true;
    if (column) {  // Verifico se la colonna e' una mask
      if (column->isControl() || column->isRendered() ||
          column->getMeshColumn())
        isReference = false;
    }

    NumberRange layerAxisRange(layerAxis + 1,
                               m_viewer->columnToLayerAxis(col + 1));
    if (!m_viewer->orientation()->isVerticalTimeline()) {
      int adjY       = m_viewer->orientation()->cellHeight() - 1;
      layerAxisRange = NumberRange(layerAxis + 1, layerAxis + adjY);
    }

    // draw vertical line
    if (layerAxis > 0) {
      p.setPen(m_viewer->getVerticalLineColor());
      QLine verticalLine =
          m_viewer->orientation()->verticalLine(layerAxis, frameSide);
      p.drawLine(verticalLine);
    }

    // for each frame
    for (row = r0; row <= r1; row++) {
      // draw horizontal lines
      // hide top-most marker line
      QColor color = ((row - offset) % distance == 0 && row != 0)
                         ? m_viewer->getMarkerLineColor()
                         : m_viewer->getLightLineColor();

      p.setPen(color);
      int frameAxis = m_viewer->rowToFrameAxis(row);
      QLine horizontalLine =
          m_viewer->orientation()->horizontalLine(frameAxis, layerAxisRange);
      p.drawLine(horizontalLine);

      if (!isColumn) {
        if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
            !m_viewer->orientation()->isVerticalTimeline() &&
            row == m_viewer->getCurrentRow() &&
            Preferences::instance()->isCurrentTimelineIndicatorEnabled()) {
          QPoint xy = m_viewer->positionToXY(CellPosition(row, col));
          int x     = xy.x();
          int y     = xy.y();
          if (row == 0) {
            if (m_viewer->orientation()->isVerticalTimeline())
              xy.setY(xy.y() + 1);
            else
              xy.setX(xy.x() + 1);
          }
          drawCurrentTimeIndicator(p, xy);
        }
        continue;
      }
      // Cells appearance depending on the type of column
      if (isSoundColumn)
        drawSoundCell(p, row, col, isReference);
      else if (isPaletteColumn)
        drawPaletteCell(p, row, col, isReference);
      else if (isSoundTextColumn)
        drawSoundTextCell(p, row, col);
      else
        drawLevelCell(p, row, col, isReference);
    }
  }

  drawExtenderHandles(p);
}

// slightly bright background for non-empty rectangular area
void CellArea::drawNonEmptyBackground(QPainter &p) const {
  TXsheet *xsh = m_viewer->getXsheet();

  int totalFrames = xsh->getFrameCount();
  if (!totalFrames) return;

  int lastNonEmptyCol;
  for (lastNonEmptyCol = xsh->getColumnCount() - 1; lastNonEmptyCol >= 0;
       lastNonEmptyCol--) {
    TXshColumn *currentColumn = xsh->getColumn(lastNonEmptyCol);
    if (!currentColumn) continue;
    if (!currentColumn->isEmpty()) break;
  }
  QPoint xyTop, xyBottom;
  const Orientation *o = m_viewer->orientation();
  if (o->isVerticalTimeline()) {
    xyTop = QPoint(1, 0);
    xyBottom =
        m_viewer->positionToXY(CellPosition(totalFrames, lastNonEmptyCol + 1));
  } else {
    xyTop          = m_viewer->positionToXY(CellPosition(0, lastNonEmptyCol));
    xyBottom       = m_viewer->positionToXY(CellPosition(totalFrames, 0));
    ColumnFan *fan = xsh->getColumnFan(o);
    xyBottom.setY(xyBottom.y() +
                  ((fan ? fan->isActive(0) : true) ? o->cellHeight() : 9));
  }
  p.fillRect(xyTop.x(), xyTop.y(), xyBottom.x(), xyBottom.y(),
             QBrush(m_viewer->getNotEmptyColumnColor()));
}

void CellArea::drawFoldedColumns(QPainter &p, int layerAxis,
                                 const NumberRange &frameAxis) const {
  // 3 white bars
  for (int i = 0; i < 3; i++) {
    QRect whiteRect =
        m_viewer->orientation()->foldedRectangle(layerAxis, frameAxis, i);
    p.fillRect(whiteRect, QBrush(Qt::white));
  }

  // 3 dark lines
  p.setPen(m_viewer->getLightLineColor());
  for (int i = 0; i < 3; i++) {
    QLine darkLine =
        m_viewer->orientation()->foldedRectangleLine(layerAxis, frameAxis, i);
    p.drawLine(darkLine);
  }
}

void CellArea::drawSelectionBackground(QPainter &p) const {
  // selected cells range
  TCellSelection *cellSelection = m_viewer->getCellSelection();
  if (cellSelection->isEmpty()) return;

  int selRow0, selCol0, selRow1, selCol1;
  cellSelection->getSelectedCells(selRow0, selCol0, selRow1, selCol1);
  QRect selectionRect;
  const Orientation *o = m_viewer->orientation();
  if (o->isVerticalTimeline())
    selectionRect = m_viewer->rangeToXYRect(
        CellRange(CellPosition(selRow0, selCol0),
                  CellPosition(selRow1 + 1, selCol1 + 1)));
  else {
    int newSelCol0 = qMax(selCol0, selCol1);
    int newSelCol1 = qMin(selCol0, selCol1);
    selectionRect  = m_viewer->rangeToXYRect(
        CellRange(CellPosition(selRow0, newSelCol0),
                  CellPosition(selRow1 + 1, newSelCol1 - 1)));
  }

  p.fillRect(selectionRect, QBrush(m_viewer->getSelectedEmptyCellColor()));
}

void CellArea::drawExtenderHandles(QPainter &p) {
  const Orientation *o = m_viewer->orientation();

  m_levelExtenderRect      = QRect();
  m_upperLevelExtenderRect = QRect();

  // selected cells range
  TCellSelection *cellSelection = m_viewer->getCellSelection();
  if (cellSelection->isEmpty() || m_viewer->areSoundCellsSelected()) return;

  int selRow0, selCol0, selRow1, selCol1;
  cellSelection->getSelectedCells(selRow0, selCol0, selRow1, selCol1);
  QRect selected;
  selected = m_viewer->rangeToXYRect(CellRange(
      CellPosition(selRow0, selCol0), CellPosition(selRow1 + 1, selCol1 + 1)));
  if (!o->isVerticalTimeline()) {
    TXsheet *xsh         = m_viewer->getXsheet();
    ColumnFan *columnFan = xsh->getColumnFan(o);
    int topAdj           = columnFan->isActive(selCol0) ? o->cellHeight() : 9;
    int bottomAdj        = columnFan->isActive(selCol1) ? o->cellHeight() : 9;
    selected.adjust(0, topAdj, 0, bottomAdj);
  }

  int x0, y0, x1, y1;
  x0 = selected.left();
  x1 = selected.right();
  y0 = selected.top();
  y1 = selected.bottom();

  // smart tab
  QPoint smartTabPosOffset =
      Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()
          ? QPoint(0, 0)
          : o->point(PredefinedPoint::KEY_HIDDEN);

  int distance, offset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, offset);
  if (distance == 0) distance = 6;

  QPoint xyRadius = o->point(PredefinedPoint::EXTENDER_XY_RADIUS);

  // bottom / right extender handle
  m_levelExtenderRect =
      o->rect(PredefinedRect::END_EXTENDER)
          .translated(selected.bottomRight() + smartTabPosOffset);
  p.setPen(Qt::black);
  p.setBrush(SmartTabColor);
  p.drawRoundRect(m_levelExtenderRect, xyRadius.x(), xyRadius.y());
  QColor color = ((selRow1 + 1 - offset) % distance != 0)
                     ? m_viewer->getLightLineColor()
                     : m_viewer->getMarkerLineColor();
  p.setPen(color);
  QLine extenderLine = o->line(PredefinedLine::EXTENDER_LINE);
  extenderLine.setP1(extenderLine.p1() + smartTabPosOffset);
  p.drawLine(extenderLine.translated(selected.bottomRight()));

  // up / left extender handle
  if (isCtrlPressed && selRow0 > 0 && !m_viewer->areCellsSelectedEmpty()) {
    QPoint properPoint       = o->topRightCorner(selected);
    m_upperLevelExtenderRect = o->rect(PredefinedRect::BEGIN_EXTENDER)
                                   .translated(properPoint + smartTabPosOffset);
    p.setPen(Qt::black);
    p.setBrush(SmartTabColor);
    p.drawRoundRect(m_upperLevelExtenderRect, xyRadius.x(), xyRadius.y());
    QColor color = ((selRow0 - offset) % distance != 0)
                       ? m_viewer->getLightLineColor()
                       : m_viewer->getMarkerLineColor();
    p.setPen(color);
    p.drawLine(extenderLine.translated(properPoint));
  }

  p.setBrush(Qt::NoBrush);
}

//-----------------------------------------------------------------------------

void CellArea::drawSoundCell(QPainter &p, int row, int col, bool isReference) {
  const Orientation *o = m_viewer->orientation();
  TXshSoundColumn *soundColumn =
      m_viewer->getXsheet()->getColumn(col)->getSoundColumn();
  QPoint xy = m_viewer->positionToXY(CellPosition(row, col));
  int x     = xy.x();
  int y     = xy.y();
  if (row == 0) {
    if (o->isVerticalTimeline())
      xy.setY(xy.y() + 1);
    else
      xy.setX(xy.x() + 1);
  }

  int frameAdj   = m_viewer->getFrameZoomAdjustment();
  int frameZoomF = m_viewer->getFrameZoomFactor();
  QRect cellRect = o->rect(PredefinedRect::CELL).translated(QPoint(x, y));
  cellRect.adjust(0, 0, -frameAdj, 0);
  QRect rect      = cellRect.adjusted(1, 1, 0, 0);
  int maxNumFrame = soundColumn->getMaxFrame() + 1;
  int startFrame  = soundColumn->getFirstRow();
  TXshCell cell   = soundColumn->getCell(row);
  if (soundColumn->isCellEmpty(row) || cell.isEmpty() || row > maxNumFrame ||
      row < startFrame) {
    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);
    return;
  }

  TXshSoundLevelP soundLevel = cell.getSoundLevel();

  int r0, r1;
  if (!soundColumn->getLevelRange(row, r0, r1)) return;
  bool isFirstRow = (row == r0);
  bool isLastRow  = (row == r1);

  TCellSelection *cellSelection     = m_viewer->getCellSelection();
  TColumnSelection *columnSelection = m_viewer->getColumnSelection();
  bool isSelected                   = cellSelection->isCellSelected(row, col) ||
                    columnSelection->isColumnSelected(col);

  // get cell colors
  QColor cellColor, sideColor;
  int levelType;
  if (isReference) {
    cellColor = (isSelected) ? m_viewer->getSelectedReferenceColumnColor()
                             : m_viewer->getReferenceColumnColor();
    sideColor = m_viewer->getReferenceColumnBorderColor();
  } else
    m_viewer->getCellTypeAndColors(levelType, cellColor, sideColor, cell,
                                   isSelected);

  // cells background
  p.fillRect(rect, QBrush(cellColor));

  if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
      !m_viewer->orientation()->isVerticalTimeline() &&
      row == m_viewer->getCurrentRow() &&
      Preferences::instance()->isCurrentTimelineIndicatorEnabled())
    drawCurrentTimeIndicator(p, xy);

  drawDragHandle(p, xy, sideColor);
  drawEndOfDragHandle(p, isLastRow, xy, cellColor);
  drawLockedDottedLine(p, soundColumn->isLocked(), xy, cellColor);

  QRect trackRect = o->rect(PredefinedRect::SOUND_TRACK)
                        .adjusted(0, 0, -frameAdj, 0)
                        .translated(xy);
  QRect previewRect = o->rect(PredefinedRect::PREVIEW_TRACK)
                          .adjusted(0, 0, -frameAdj, 0)
                          .translated(xy);
  NumberRange trackBounds   = o->layerSide(trackRect);
  NumberRange previewBounds = o->layerSide(previewRect);
  NumberRange trackAndPreview(trackBounds.from(), previewBounds.to());

  NumberRange timeBounds = o->frameSide(trackRect);
  int offset =
      row - cell.getFrameId().getNumber();  // rows since start of the clip
  int begin      = timeBounds.from();       // time axis
  int end        = timeBounds.to();
  int soundPixel = o->rowToFrameAxis(row) -
                   o->rowToFrameAxis(offset);  // pixels since start of clip

  int trackWidth = trackBounds.length();
  int lastMin, lastMax;
  DoublePair minmax;
  soundLevel->getValueAtPixel(o, soundPixel, minmax);

  double pmin = minmax.first;
  double pmax = minmax.second;

  int center = trackBounds.middle();
  lastMin    = tcrop((int)pmin, -trackWidth / 2, 0) + center;
  lastMax    = tcrop((int)pmax, 0, trackWidth / 2 - 1) + center;

  bool scrub = m_viewer->isScrubHighlighted(row, col);

  int i;
  int z = 100 / frameZoomF;
  for (i = begin; i <= end; i++) {
    soundLevel->getValueAtPixel(o, soundPixel, minmax);
    soundPixel += z;  // ++;
    int min, max;
    pmin = minmax.first;
    pmax = minmax.second;

    center = trackBounds.middle();
    min    = tcrop((int)pmin, -trackWidth / 2, 0) + center;
    max    = tcrop((int)pmax, 0, trackWidth / 2 - 1) + center;

    if (scrub && i % 2) {
      p.setPen(m_viewer->getSoundColumnHlColor());
      QLine stroke = o->horizontalLine(i, previewBounds.adjusted(-1, -1));
      p.drawLine(stroke);
    } else if (i != begin || !isFirstRow) {
      // preview tool on the right side
      if (i % 2)
        p.setPen(cellColor);
      else
        p.setPen(m_viewer->getSoundColumnTrackColor());
      QLine stroke = o->horizontalLine(i, previewBounds.adjusted(-1, -1));
      p.drawLine(stroke);
    }
    if (i != begin) {
      // "audio track" in the middle of the column
      p.setPen(m_viewer->getSoundColumnTrackColor());
      QLine minLine = o->horizontalLine(i, NumberRange(lastMin, min));
      p.drawLine(minLine);
      QLine maxLine = o->horizontalLine(i, NumberRange(lastMax, max));
      p.drawLine(maxLine);
    }

    lastMin = min;
    lastMax = max;
  }

  // yellow clipped border
  p.setPen(SoundColumnExtenderColor);
  int r0WithoutOff, r1WithoutOff;
  bool ret =
      soundColumn->getLevelRangeWithoutOffset(row, r0WithoutOff, r1WithoutOff);
  assert(ret);

  if (isFirstRow) {
    QRect modifierRect = m_viewer->orientation()
                             ->rect(PredefinedRect::BEGIN_SOUND_EDIT)
                             .adjusted(-frameAdj, 0, -frameAdj, 0)
                             .translated(xy);
    if (r0 != r0WithoutOff) p.fillRect(modifierRect, SoundColumnExtenderColor);
    m_soundLevelModifyRects.append(modifierRect);  // list of clipping rects
  }
  if (isLastRow) {
    QRect modifierRect = m_viewer->orientation()
                             ->rect(PredefinedRect::END_SOUND_EDIT)
                             .adjusted(-frameAdj, 0, -frameAdj, 0)
                             .translated(xy);
    if (r1 != r1WithoutOff) p.fillRect(modifierRect, SoundColumnExtenderColor);
    m_soundLevelModifyRects.append(modifierRect);
  }

  int distance, markerOffset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, markerOffset);
  bool isAfterMarkers =
      (row - markerOffset) % distance == 0 && distance != 0 && row != 0;

  // draw marker interval
  if (isAfterMarkers) {
    p.setPen(m_viewer->getMarkerLineColor());
    p.drawLine(o->line(PredefinedLine::SEE_MARKER_THROUGH).translated(xy));
  }
}

//-----------------------------------------------------------------------------

// paint side bar
void CellArea::drawDragHandle(QPainter &p, const QPoint &xy,
                              const QColor &sideColor) const {
  int frameAdj         = m_viewer->getFrameZoomAdjustment();
  QRect dragHandleRect = m_viewer->orientation()
                             ->rect(PredefinedRect::DRAG_HANDLE_CORNER)
                             .adjusted(0, 0, -frameAdj, 0)
                             .translated(xy);
  p.fillRect(dragHandleRect, QBrush(sideColor));
}

// cut off triangle at the end of drag handle
void CellArea::drawEndOfDragHandle(QPainter &p, bool isEnd, const QPoint &xy,
                                   const QColor &cellColor) const {
  if (!isEnd) return;
  int frameAdj        = m_viewer->getFrameZoomAdjustment();
  QPainterPath corner = m_viewer->orientation()
                            ->path(PredefinedPath::DRAG_HANDLE_CORNER)
                            .translated(xy - QPoint(frameAdj, 0));
  p.fillPath(corner, QBrush(cellColor));
}

// draw dot line if the column is locked
void CellArea::drawLockedDottedLine(QPainter &p, bool isLocked,
                                    const QPoint &xy,
                                    const QColor &cellColor) const {
  if (!isLocked) return;
  p.setPen(QPen(cellColor, 2, Qt::DotLine));
  int frameAdj = m_viewer->getFrameZoomAdjustment();
  QLine dottedLine =
      m_viewer->orientation()->line(PredefinedLine::LOCKED).translated(xy);
  dottedLine.setP2(QPoint(dottedLine.x2() - frameAdj, dottedLine.y2()));
  p.drawLine(dottedLine);
}

void CellArea::drawCurrentTimeIndicator(QPainter &p, const QPoint &xy,
                                        bool isFolded) {
  int frameAdj = m_viewer->getFrameZoomAdjustment();
  QRect cell =
      m_viewer->orientation()->rect(PredefinedRect::CELL).translated(xy);
  cell.adjust(-frameAdj / 2, 0, -frameAdj / 2, 0);

  int cellMid    = cell.left() + (cell.width() / 2) - 1;
  int cellTop    = cell.top();
  int cellBottom = cell.bottom();

  // Adjust left for 1st row
  if (xy.x() <= 1) cellMid -= 1;

  if (isFolded)
    cellBottom = cell.top() + m_viewer->orientation()->foldedCellSize() - 1;

  p.setPen(Qt::red);
  p.drawLine(cellMid, cellTop, cellMid, cellBottom);
}

void CellArea::drawFrameDot(QPainter &p, const QPoint &xy, bool isValid) {
  int frameAdj = m_viewer->getFrameZoomAdjustment();
  QRect dotRect =
      m_viewer->orientation()->rect(PredefinedRect::FRAME_DOT).translated(xy);
  p.setPen(Qt::black);
  p.setBrush(isValid ? QColor(230, 100, 100) : m_viewer->getTextColor());
  dotRect.adjust(-frameAdj / 2, 0, -frameAdj / 2, 0);
  p.drawEllipse(dotRect);
  p.setBrush(Qt::NoBrush);
}

//-----------------------------------------------------------------------------

void CellArea::drawLevelCell(QPainter &p, int row, int col, bool isReference) {
  const Orientation *o = m_viewer->orientation();

  TXsheet *xsh  = m_viewer->getXsheet();
  TXshCell cell = xsh->getCell(row, col);
  TXshCell prevCell;

  TCellSelection *cellSelection     = m_viewer->getCellSelection();
  TColumnSelection *columnSelection = m_viewer->getColumnSelection();
  bool isSelected                   = cellSelection->isCellSelected(row, col) ||
                    columnSelection->isColumnSelected(col);

  if (row > 0) prevCell = xsh->getCell(row - 1, col);  // cell in previous frame

  QPoint xy = m_viewer->positionToXY(CellPosition(row, col));
  int x     = xy.x();
  int y     = xy.y();
  if (row == 0) {
    if (o->isVerticalTimeline())
      xy.setY(xy.y() + 1);
    else
      xy.setX(xy.x() + 1);
  }

  // nothing to draw
  if (cell.isEmpty() && prevCell.isEmpty()) {
    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);
    return;
  }
  TXshCell nextCell;
  nextCell = xsh->getCell(row + 1, col);  // cell in next frame

  int frameAdj   = m_viewer->getFrameZoomAdjustment();
  QRect cellRect = o->rect(PredefinedRect::CELL).translated(QPoint(x, y));
  cellRect.adjust(0, 0, -frameAdj, 0);
  QRect rect = cellRect.adjusted(1, 1, 0, 0);
  if (cell.isEmpty()) {  // it means previous is not empty
    // diagonal cross meaning end of level
    QColor levelEndColor = m_viewer->getTextColor();
    levelEndColor.setAlphaF(0.3);
    p.setPen(levelEndColor);
    p.drawLine(rect.topLeft(), rect.bottomRight());
    p.drawLine(rect.topRight(), rect.bottomLeft());

    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);

    return;
  }

  // get cell colors
  QColor cellColor, sideColor;
  if (isReference) {
    cellColor = (isSelected) ? m_viewer->getSelectedReferenceColumnColor()
                             : m_viewer->getReferenceColumnColor();
    sideColor = m_viewer->getReferenceColumnBorderColor();
  } else {
    int levelType;
    m_viewer->getCellTypeAndColors(levelType, cellColor, sideColor, cell,
                                   isSelected);
  }

  // check if the level is scanned but not cleanupped
  bool yetToCleanupCell = false;
  if (!cell.isEmpty() && cell.getSimpleLevel()) {
    int frameStatus = cell.getSimpleLevel()->getFrameStatus(cell.m_frameId);
    const int mask  = TXshSimpleLevel::Cleanupped | TXshSimpleLevel::Scanned;
    const int yetToCleanupValue = TXshSimpleLevel::Scanned;
    yetToCleanupCell            = (frameStatus & mask) == yetToCleanupValue;
  }

  // paint cell
  p.fillRect(rect, QBrush(cellColor));

  if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
      !m_viewer->orientation()->isVerticalTimeline() &&
      row == m_viewer->getCurrentRow() &&
      Preferences::instance()->isCurrentTimelineIndicatorEnabled())
    drawCurrentTimeIndicator(p, xy);

  drawDragHandle(p, xy, sideColor);

  if (yetToCleanupCell)  // ORIENTATION: what's this?
  {
    if (o->isVerticalTimeline())
      p.fillRect(rect.adjusted(rect.width() / 2, 0, 0, 0),
                 (isSelected) ? m_viewer->getSelectedFullcolorColumnColor()
                              : m_viewer->getFullcolorColumnColor());
    else
      p.fillRect(rect.adjusted(0, rect.height() / 2, 0, 0),
                 (isSelected) ? m_viewer->getSelectedFullcolorColumnColor()
                              : m_viewer->getFullcolorColumnColor());
  }

  bool isLastRow = nextCell.isEmpty() ||
                   cell.m_level.getPointer() != nextCell.m_level.getPointer();
  drawEndOfDragHandle(p, isLastRow, xy, cellColor);

  drawLockedDottedLine(p, xsh->getColumn(col)->isLocked(), xy, cellColor);

  bool sameLevel = prevCell.m_level.getPointer() == cell.m_level.getPointer();

  int distance, offset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, offset);
  bool isAfterMarkers =
      (row - offset) % distance == 0 && distance != 0 && row != 0;

  // draw marker interval
  if (isAfterMarkers) {
    p.setPen(m_viewer->getMarkerLineColor());
    p.drawLine(o->line(PredefinedLine::SEE_MARKER_THROUGH).translated(xy));
  }

  QRect nameRect = o->rect(PredefinedRect::CELL_NAME).translated(QPoint(x, y));

  if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
    TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
    int r0, r1;
    if (pegbar && pegbar->getKeyframeRange(r0, r1))
      nameRect = o->rect(PredefinedRect::CELL_NAME_WITH_KEYFRAME)
                     .translated(QPoint(x, y));
  }

  nameRect.adjust(0, 0, -frameAdj, 0);

  // draw text in red if the file does not exist
  bool isRed                                  = false;
  TXshSimpleLevel *sl                         = cell.getSimpleLevel();
  if (sl && !sl->isFid(cell.m_frameId)) isRed = true;
  TXshChildLevel *cl                          = cell.getChildLevel();
  if (cl && cell.getFrameId().getNumber() - 1 >= cl->getFrameCount())
    isRed = true;
  p.setPen(
      isRed ? QColor(230, 100, 100)  // m_viewer->getSelectedColumnTextColor()
            : m_viewer->getTextColor());

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  p.setFont(font);

  // do not draw frame number under RenameCellField
  if (m_renameCell->isVisible() && m_renameCell->isLocatedAt(row, col)) return;

  // if the same level & same fId with the previous cell,
  // draw continue line
  QString fnum;
  if (sameLevel && prevCell.m_frameId == cell.m_frameId) {
    // not on line marker
    PredefinedLine which =
        Preferences::instance()->isLevelNameOnEachMarkerEnabled()
            ? PredefinedLine::CONTINUE_LEVEL_WITH_NAME
            : PredefinedLine::CONTINUE_LEVEL;

    QLine continueLine = o->line(which).translated(xy);
    continueLine.setP2(QPoint(continueLine.x2() - frameAdj, continueLine.y2()));
    p.drawLine(continueLine);
  }
  // draw frame number
  else {
    if (m_viewer->getFrameZoomFactor() <= 50) {
      drawFrameDot(p, QPoint(x, y), isRed);
      return;
    }

    TFrameId fid = cell.m_frameId;

    // convert the last one digit of the frame number to alphabet
    // Ex.  12 -> 1B    21 -> 2A   30 -> 3
    if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
      fnum = m_viewer->getFrameNumberWithLetters(fid.getNumber());
    else {
      std::string frameNumber("");
      // set number
      if (fid.getNumber() > 0) frameNumber = std::to_string(fid.getNumber());
      // add letter
      if (fid.getLetter() != 0) frameNumber.append(1, fid.getLetter());
      fnum = QString::fromStdString(frameNumber);
    }

    p.drawText(nameRect, Qt::AlignRight | Qt::AlignBottom, fnum);
  }

  // draw level name
  if (!sameLevel ||
      (isAfterMarkers &&
       Preferences::instance()->isLevelNameOnEachMarkerEnabled())) {
    std::wstring levelName = cell.m_level->getName();
    QString text           = QString::fromStdWString(levelName);
    QFontMetrics fm(font);
#if QT_VERSION >= 0x050500
    //    QFontMetrics fm(font);
    QString elidaName =
        elideText(text, fm, nameRect.width() - fm.width(fnum), QString("~"));
#else
    QString elidaName =
        elideText(text, font, nameRect.width() - fm.width(fnum));
#endif
    p.drawText(nameRect, Qt::AlignLeft | Qt::AlignBottom, elidaName);
  }
}

//-----------------------------------------------------------------------------

void CellArea::drawSoundTextCell(QPainter &p, int row, int col) {
  const Orientation *o = m_viewer->orientation();
  TXsheet *xsh         = m_viewer->getXsheet();
  TXshCell cell        = xsh->getCell(row, col);
  TXshCell prevCell;

  TCellSelection *cellSelection     = m_viewer->getCellSelection();
  TColumnSelection *columnSelection = m_viewer->getColumnSelection();
  bool isSelected                   = cellSelection->isCellSelected(row, col) ||
                    columnSelection->isColumnSelected(col);

  if (row > 0) prevCell = xsh->getCell(row - 1, col);  // cell in previous frame
                                                       // nothing to draw
  QPoint xy = m_viewer->positionToXY(CellPosition(row, col));
  int x     = xy.x();
  int y     = xy.y();
  if (row == 0) {
    if (o->isVerticalTimeline())
      xy.setY(xy.y() + 1);
    else
      xy.setX(xy.x() + 1);
  }

  if (cell.isEmpty() && prevCell.isEmpty()) {
    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);

    return;
  }
  TXshCell nextCell;
  nextCell = xsh->getCell(row + 1, col);

  int frameAdj   = m_viewer->getFrameZoomAdjustment();
  QRect cellRect = o->rect(PredefinedRect::CELL).translated(QPoint(x, y));
  cellRect.adjust(0, 0, -frameAdj, 0);
  QRect rect = cellRect.adjusted(1, 1, 0, 0);
  if (cell.isEmpty()) {  // diagonal cross meaning end of level
    QColor levelEndColor = m_viewer->getTextColor();
    levelEndColor.setAlphaF(0.3);
    p.setPen(levelEndColor);
    p.drawLine(rect.topLeft(), rect.bottomRight());
    p.drawLine(rect.topRight(), rect.bottomLeft());

    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);

    return;
  }

  int levelType;
  QColor cellColor, sideColor;
  m_viewer->getCellTypeAndColors(levelType, cellColor, sideColor, cell,
                                 isSelected);

  // paint cell
  p.fillRect(rect, QBrush(cellColor));

  if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
      !m_viewer->orientation()->isVerticalTimeline() &&
      row == m_viewer->getCurrentRow() &&
      Preferences::instance()->isCurrentTimelineIndicatorEnabled())
    drawCurrentTimeIndicator(p, xy);

  drawDragHandle(p, xy, sideColor);

  bool isLastRow = nextCell.isEmpty() ||
                   cell.m_level.getPointer() != nextCell.m_level.getPointer();
  drawEndOfDragHandle(p, isLastRow, xy, cellColor);

  drawLockedDottedLine(p, xsh->getColumn(col)->isLocked(), xy, cellColor);
  bool sameLevel = prevCell.m_level.getPointer() == cell.m_level.getPointer();

  TFrameId fid = cell.m_frameId;
  if (fid.getNumber() - 1 < 0) return;
  int distance, offset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, offset);
  bool isAfterMarkers =
      (row - offset) % distance == 0 && distance != 0 && row != 0;

  // draw marker interval
  if (isAfterMarkers) {
    p.setPen(m_viewer->getMarkerLineColor());
    p.drawLine(o->line(PredefinedLine::SEE_MARKER_THROUGH).translated(xy));
  }

  p.setPen(Qt::black);
  QRect nameRect = o->rect(PredefinedRect::CELL_NAME).translated(QPoint(x, y));
  nameRect.adjust(0, 0, -frameAdj, 0);

  // il nome va scritto se e' diverso dalla cella precedente oppure se
  // siamo su una marker line
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName          = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  p.setFont(font);

  // if the same level & same fId with the previous cell,
  // draw continue line
  if (sameLevel && prevCell.m_frameId == cell.m_frameId) {
    // not on line marker
    PredefinedLine which =
        Preferences::instance()->isLevelNameOnEachMarkerEnabled()
            ? PredefinedLine::CONTINUE_LEVEL_WITH_NAME
            : PredefinedLine::CONTINUE_LEVEL;
    QLine continueLine = o->line(which).translated(xy);
    continueLine.setP2(QPoint(continueLine.x2() - frameAdj, continueLine.y2()));
    p.drawLine(continueLine);
  }

  QString text =
      cell.getSoundTextLevel()->getFrameText(cell.m_frameId.getNumber() - 1);

  QFontMetrics metric(font);

  int charWidth = metric.width(text, 1);
  if ((charWidth * 2) > nameRect.width()) nameRect.adjust(-2, 0, 4, 0);

#if QT_VERSION >= 0x050500
  QString elidaName = elideText(text, metric, nameRect.width(), "~");
#else
  QString elidaName   = elideText(text, font, nameRect.width(), "~");
#endif

  if (!sameLevel || prevCell.m_frameId != cell.m_frameId)
    p.drawText(nameRect, Qt::AlignLeft | Qt::AlignBottom, elidaName);
}

//-----------------------------------------------------------------------------

void CellArea::drawPaletteCell(QPainter &p, int row, int col,
                               bool isReference) {
  const Orientation *o = m_viewer->orientation();

  TXsheet *xsh  = m_viewer->getXsheet();
  TXshCell cell = xsh->getCell(row, col);
  TXshCell prevCell;

  TCellSelection *cellSelection = m_viewer->getCellSelection();
  bool isSelected               = cellSelection->isCellSelected(row, col);

  if (row > 0) prevCell = xsh->getCell(row - 1, col);
  TXshCell nextCell     = xsh->getCell(row + 1, col);

  QPoint xy = m_viewer->positionToXY(CellPosition(row, col));
  int x     = xy.x();
  int y     = xy.y();
  if (row == 0) {
    if (o->isVerticalTimeline())
      xy.setY(xy.y() + 1);
    else
      xy.setX(xy.x() + 1);
  }
  if (cell.isEmpty() && prevCell.isEmpty()) {
    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);

    return;
  }

  int frameAdj   = m_viewer->getFrameZoomAdjustment();
  QRect cellRect = o->rect(PredefinedRect::CELL).translated(QPoint(x, y));
  cellRect.adjust(0, 0, -frameAdj, 0);
  QRect rect = cellRect.adjusted(1, 1, 0, 0);
  if (cell.isEmpty()) {  // this means the former is not empty
    QColor levelEndColor = m_viewer->getTextColor();
    levelEndColor.setAlphaF(0.3);
    p.setPen(levelEndColor);
    p.drawLine(rect.topLeft(), rect.bottomRight());
    p.drawLine(rect.topRight(), rect.bottomLeft());

    if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
        !m_viewer->orientation()->isVerticalTimeline() &&
        row == m_viewer->getCurrentRow() &&
        Preferences::instance()->isCurrentTimelineIndicatorEnabled())
      drawCurrentTimeIndicator(p, xy);

    return;
  }

  QColor cellColor, sideColor;
  if (isReference) {
    cellColor = (isSelected) ? m_viewer->getSelectedReferenceColumnColor()
                             : m_viewer->getReferenceColumnColor();
    sideColor = m_viewer->getReferenceColumnBorderColor();
  } else {
    cellColor = (isSelected) ? m_viewer->getSelectedPaletteColumnColor()
                             : m_viewer->getPaletteColumnColor();
    sideColor = m_viewer->getPaletteColumnBorderColor();
  }

  p.fillRect(rect, QBrush(cellColor));

  if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
      !m_viewer->orientation()->isVerticalTimeline() &&
      row == m_viewer->getCurrentRow() &&
      Preferences::instance()->isCurrentTimelineIndicatorEnabled())
    drawCurrentTimeIndicator(p, xy);

  drawDragHandle(p, xy, sideColor);
  bool isLastRow = nextCell.isEmpty() ||
                   cell.m_level.getPointer() != nextCell.m_level.getPointer();
  drawEndOfDragHandle(p, isLastRow, xy, cellColor);
  drawLockedDottedLine(p, xsh->getColumn(col)->isLocked(), xy, cellColor);

  bool sameLevel = prevCell.m_level.getPointer() == cell.m_level.getPointer();

  int distance, offset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, offset);
  if (distance == 0) distance = 6;
  bool isAfterMarkers         = (row - offset) % distance == 0 && row != 0;

  if (isAfterMarkers) {
    p.setPen(m_viewer->getMarkerLineColor());
    p.drawLine(o->line(PredefinedLine::SEE_MARKER_THROUGH).translated(xy));
  }

  if (sameLevel && prevCell.m_frameId == cell.m_frameId &&
      !isAfterMarkers) {  // cell equal to previous one (not on marker line):
                          // do not write anything and draw a vertical line
    QPen oldPen = p.pen();
    p.setPen(QPen(m_viewer->getTextColor(), 1));
    QLine continueLine = o->line(PredefinedLine::CONTINUE_LEVEL).translated(xy);
    continueLine.setP2(QPoint(continueLine.x2() - frameAdj, continueLine.y2()));
    p.drawLine(continueLine);
    p.setPen(oldPen);
  } else {
    TFrameId fid = cell.m_frameId;

    std::wstring levelName = cell.m_level->getName();
    std::string frameNumber("");
    if (fid.getNumber() > 0) frameNumber = std::to_string(fid.getNumber());
    if (fid.getLetter() != 0) frameNumber.append(1, fid.getLetter());

    QRect nameRect =
        o->rect(PredefinedRect::CELL_NAME).translated(QPoint(x, y));

    if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
      TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
      int r0, r1;
      if (pegbar && pegbar->getKeyframeRange(r0, r1))
        nameRect = o->rect(PredefinedRect::CELL_NAME_WITH_KEYFRAME)
                       .translated(QPoint(x, y));
    }

    nameRect.adjust(0, 0, -frameAdj, 0);
    bool isRed                         = false;
    TXshPaletteLevel *pl               = cell.getPaletteLevel();
    if (pl && !pl->getPalette()) isRed = true;
    p.setPen(
        isRed ? QColor(230, 100, 100)  // m_viewer->getSelectedColumnTextColor()
              : m_viewer->getTextColor());
    // il nome va scritto se e' diverso dalla cella precedente oppure se
    // siamo su una marker line
    QString fontName = Preferences::instance()->getInterfaceFont();
    if (fontName == "") {
#ifdef _WIN32
      fontName = "Arial";
#else
      fontName        = "Helvetica";
#endif
    }
    static QFont font(fontName, -1, QFont::Normal);
    font.setPixelSize(XSHEET_FONT_PX_SIZE);
    p.setFont(font);
    QFontMetrics fm(font);

    // il numero va scritto se e' diverso dal precedente oppure se il livello
    // e' diverso dal precedente
    QString numberStr;
    if (!sameLevel || prevCell.m_frameId != cell.m_frameId) {
      // convert the last one digit of the frame number to alphabet
      // Ex.  12 -> 1B    21 -> 2A   30 -> 3
      if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
        numberStr = m_viewer->getFrameNumberWithLetters(fid.getNumber());
        p.drawText(nameRect, Qt::AlignRight | Qt::AlignBottom, numberStr);
      } else {
        std::string frameNumber("");
        // set number
        if (fid.getNumber() > 0) frameNumber = std::to_string(fid.getNumber());
        // add letter
        if (fid.getLetter() != 0) frameNumber.append(1, fid.getLetter());
        numberStr = QString::fromStdString(frameNumber);
        p.drawText(nameRect, Qt::AlignRight | Qt::AlignBottom, numberStr);
      }
    }

    QString text = QString::fromStdWString(levelName);
#if QT_VERSION >= 0x050500
    QString elidaName = elideText(
        text, fm, nameRect.width() - fm.width(numberStr) - 2, QString("~"));
#else
    QString elidaName = elideText(
        text, font, nameRect.width() - fm.width(numberStr) - 2, QString("~"));
#endif

    if (!sameLevel || isAfterMarkers)
      p.drawText(nameRect, Qt::AlignLeft | Qt::AlignBottom, elidaName);
  }
}

//-----------------------------------------------------------------------------

void CellArea::drawKeyframe(QPainter &p, const QRect toBeUpdated) {
  const Orientation *o = m_viewer->orientation();
  int r0, r1, c0, c1;  // range of visible rows and columns
  CellRange visible = m_viewer->xyRectToRange(toBeUpdated);
  r0                = visible.from().frame();
  r1                = visible.to().frame();
  c0                = visible.from().layer();
  c1                = visible.to().layer();

  static QPixmap selectedKey = svgToPixmap(":Resources/selected_key.svg");
  static QPixmap key         = svgToPixmap(":Resources/key.svg");
  int frameAdj               = m_viewer->getFrameZoomAdjustment();
  const QRect &keyRect       = o->rect(PredefinedRect::KEY_ICON)
                             .adjusted(-frameAdj / 2, 0, -frameAdj / 2, 0);

  TXsheet *xsh         = m_viewer->getXsheet();
  ColumnFan *columnFan = xsh->getColumnFan(o);
  int col;
  for (col = c0; col <= c1; col++) {
    if (!columnFan->isActive(col)) continue;
    int layerAxis = m_viewer->columnToLayerAxis(col);

    TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
    if (!pegbar) return;

    int row0, row1;
    bool emptyKeyframe = !pegbar->getKeyframeRange(row0, row1);
    if (emptyKeyframe) continue;

    bool emptyKeyframeRange = row0 >= row1;
    int row;
    row0 = std::max(row0, r0);
    row1 = std::min(row1, r1);

    /*- first, draw key segments -*/
    p.setPen(m_viewer->getTextColor());
    int line_layerAxis = layerAxis + o->layerSide(keyRect).middle();
    for (row = row0; row <= row1; row++) {
      int segmentRow0, segmentRow1;
      double ease0, ease1;
      if (pegbar->getKeyframeSpan(row, segmentRow0, ease0, segmentRow1,
                                  ease1)) {
        drawKeyframeLine(p, col, NumberRange(segmentRow0, segmentRow1));

        if (segmentRow1 - segmentRow0 >
            4) {  // only show if distance more than 4 frames
          int handleRow0, handleRow1;
          if (getEaseHandles(segmentRow0, segmentRow1, ease0, ease1, handleRow0,
                             handleRow1)) {
            QPoint topLeft =
                m_viewer->positionToXY(CellPosition(handleRow0, col));
            m_viewer->drawPredefinedPath(p, PredefinedPath::BEGIN_EASE_TRIANGLE,
                                         topLeft + QPoint(-frameAdj / 2, 0),
                                         m_viewer->getLightLineColor(),
                                         m_viewer->getTextColor());

            topLeft = m_viewer->positionToXY(CellPosition(handleRow1, col));
            m_viewer->drawPredefinedPath(p, PredefinedPath::END_EASE_TRIANGLE,
                                         topLeft + QPoint(-frameAdj / 2, 0),
                                         m_viewer->getLightLineColor(),
                                         m_viewer->getTextColor());
          }
        }
        // skip to next segment
        row = segmentRow1 - 1;
      } else if (pegbar->isKeyframe(row) && pegbar->isKeyframe(row + 1)) {
        // 2 keyframes in a row - connect with a short line
        drawKeyframeLine(p, col, NumberRange(row, row + 1));
      }
    }

    /*- then draw keyframe pixmaps -*/
    int icon_layerAxis = line_layerAxis - 5;
    for (row = row0; row <= row1; row++) {
      p.setPen(m_viewer->getTextColor());
      if (pegbar->isKeyframe(row)) {
        QPoint target =
            keyRect.translated(m_viewer->positionToXY(CellPosition(row, col)))
                .topLeft();
        if (m_viewer->getKeyframeSelection() &&
            m_viewer->getKeyframeSelection()->isSelected(row, col)) {
          // keyframe selected
          p.drawPixmap(target, selectedKey);
        } else {
          // keyframe not selected
          p.drawPixmap(target, key);
        }
      }
    }

    int icon_frameAxis = m_viewer->rowToFrameAxis(row1 + 1);
    if (!emptyKeyframeRange && row0 <= row1 + 1) {
      // there's just a keyframe
      // drawing loop button
      p.setBrush(Qt::white);
      p.setPen(Qt::black);
      QPoint target = o->frameLayerToXY(icon_frameAxis, icon_layerAxis);
      p.drawRect(QRect(target, QSize(10, 10)));
      p.setBrush(Qt::NoBrush);
      // drawing the bottom edge (rounded)
      p.drawLine(target + QPoint(1, 10), target + QPoint(9, 10));
      p.setPen(Qt::white);
      p.drawLine(target + QPoint(3, 10), target + QPoint(7, 10));
      p.setPen(Qt::black);
      p.drawLine(target + QPoint(3, 11), target + QPoint(7, 11));

      // drawing the arrow
      p.drawArc(QRect(target + QPoint(2, 3), QSize(6, 6)), 180 * 16, 270 * 16);
      p.drawLine(target + QPoint(5, 2), target + QPoint(5, 5));
      p.drawLine(target + QPoint(5, 2), target + QPoint(8, 2));
    }
    if (pegbar->isCycleEnabled()) {
      // the row zigzag bellow the button
      int ymax = m_viewer->rowToFrameAxis(r1 + 1);
      int qy   = icon_frameAxis + 12;
      int zig  = 2;
      int qx   = icon_layerAxis + 5;
      p.setPen(m_viewer->getTextColor());
      p.drawLine(o->frameLayerToXY(qy, qx),
                 o->frameLayerToXY(qy + zig, qx - zig));
      while (qy < ymax) {
        p.drawLine(o->frameLayerToXY(qy + zig, qx - zig),
                   o->frameLayerToXY(qy + 3 * zig, qx + zig));
        p.drawLine(o->frameLayerToXY(qy + 3 * zig, qx + zig),
                   o->frameLayerToXY(qy + 5 * zig, qx - zig));
        qy += 4 * zig;
      }
    }
  }
}

void CellArea::drawKeyframeLine(QPainter &p, int col,
                                const NumberRange &rows) const {
  int frameAdj         = m_viewer->getFrameZoomAdjustment();
  const QRect &keyRect = m_viewer->orientation()
                             ->rect(PredefinedRect::KEY_ICON)
                             .adjusted(-frameAdj / 2, 0, -frameAdj / 2, 0);
  QPoint begin =
      keyRect.center() + m_viewer->positionToXY(CellPosition(rows.from(), col));
  QPoint end =
      keyRect.center() + m_viewer->positionToXY(CellPosition(rows.to(), col));

  p.setPen(m_viewer->getTextColor());
  p.drawLine(QLine(begin, end));
}

//-----------------------------------------------------------------------------

void CellArea::drawNotes(QPainter &p, const QRect toBeUpdated) {
  CellRange visible = m_viewer->xyRectToRange(toBeUpdated);
  int r0, r1, c0, c1;  // range of visible rows and columns
  r0 = visible.from().frame();
  r1 = visible.to().frame();
  c0 = visible.from().layer();
  c1 = visible.to().layer();

  TXsheet *xsh = m_viewer->getXsheet();
  if (!xsh) return;
  TXshNoteSet *notes = xsh->getNotes();
  int notesCount     = notes->getCount();
  int i;
  for (i = 0; i < notesCount; i++) {
    QList<NoteWidget *> noteWidgets = m_viewer->getNotesWidget();
    int widgetCount                 = noteWidgets.size();
    NoteWidget *noteWidget          = 0;
    if (i < widgetCount)
      noteWidget = noteWidgets.at(i);
    else {
      noteWidget = new NoteWidget(m_viewer, i);
      m_viewer->addNoteWidget(noteWidget);
    }
    if (!noteWidget) continue;
    int r = notes->getNoteRow(i);
    int c = notes->getNoteCol(i);
    if (r < r0 || r > r1 || c < c0 || c > c1) continue;
    QPoint xy   = m_viewer->positionToXY(CellPosition(r, c));
    TPointD pos = notes->getNotePos(i) + TPointD(xy.x(), xy.y());
    noteWidget->paint(&p, QPoint(pos.x, pos.y),
                      i == m_viewer->getCurrentNoteIndex());
  }
}

//-----------------------------------------------------------------------------

bool CellArea::getEaseHandles(int r0, int r1, double e0, double e1, int &rh0,
                              int &rh1) {
  if (r1 <= r0 + 4) {  // ... what?
    rh0 = r0;
    rh1 = r1;
    return false;
  }
  if (e0 < 0 || e1 < 0) {
    rh0 = r0;
    rh1 = r1;
    return false;
  }
  if (e0 <= 0 && e1 <= 0) {
    rh0 = r0 + 1;
    rh1 = r1 - 1;
  } else if (e0 <= 0) {
    rh0   = r0 + 1;
    int a = rh0 + 1;
    int b = r1 - 2;
    assert(a <= b);
    rh1 = tcrop((int)(r1 - e1 + 0.5), a, b);
  } else if (e1 <= 0) {
    rh1   = r1 - 1;
    int b = rh1 - 1;
    int a = r0 + 2;
    assert(a <= b);
    rh0 = tcrop((int)(r0 + e0 + 0.5), a, b);
  } else {
    int m = tfloor(0.5 * (r0 + e0 + r1 - e1));
    m     = tcrop(m, r0 + 2, r1 - 2);
    int a = r0 + 2;
    int b = std::min(m, r1 - 3);
    assert(a <= b);
    rh0 = tcrop((int)(r0 + e0 + 0.5), a, b);
    a   = rh0 + 1;
    b   = r1 - 2;
    assert(a <= b);
    rh1 = tcrop((int)(r1 - e1 + 0.5), a, b);
  }
  return true;
}

//-----------------------------------------------------------------------------

void CellArea::paintEvent(QPaintEvent *event) {
  QRect toBeUpdated = event->rect();

  QPainter p(this);
  p.setClipRect(toBeUpdated);

  p.fillRect(toBeUpdated, QBrush(m_viewer->getEmptyCellColor()));

  drawCells(p, toBeUpdated);
  if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled())
    drawKeyframe(p, toBeUpdated);
  drawNotes(p, toBeUpdated);

  if (getDragTool()) getDragTool()->drawCellsArea(p);

  // focus cell border
  int frameAdj = m_viewer->getFrameZoomAdjustment();
  int row      = m_viewer->getCurrentRow();
  int col      = m_viewer->getCurrentColumn();
  QPoint xy    = m_viewer->positionToXY(CellPosition(row, col));
  QRect rect   = m_viewer->orientation()
                   ->rect(PredefinedRect::CELL)
                   .translated(xy)
                   .adjusted(1, 1, -1 - frameAdj, -1);
  p.setPen(Qt::black);
  p.setBrush(Qt::NoBrush);
  p.drawRect(rect);
}

//-----------------------------------------------------------------------------

class CycleUndo final : public TUndo {
  TStageObject *m_pegbar;
  CellArea *m_area;

public:
  // indices sono le colonne inserite
  CycleUndo(TStageObject *pegbar, CellArea *area)
      : m_pegbar(pegbar), m_area(area) {}
  void undo() const override {
    m_pegbar->enableCycle(!m_pegbar->isCycleEnabled());
    m_area->update();
  }
  void redo() const override { undo(); }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Toggle cycle of  %1")
        .arg(QString::fromStdString(m_pegbar->getName()));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};
//----------------------------------------------------------

void CellArea::mousePressEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  m_viewer->setQtModifiers(event->modifiers());
  assert(!m_isPanning);
  m_isMousePressed = true;
  int frameAdj     = m_viewer->getFrameZoomAdjustment();
  if (event->button() == Qt::LeftButton) {
    assert(getDragTool() == 0);

    TPoint pos(event->pos().x(), event->pos().y());
    CellPosition cellPosition = m_viewer->xyToPosition(event->pos());
    int row                   = cellPosition.frame();
    int col                   = cellPosition.layer();
    QPoint cellTopLeft        = m_viewer->positionToXY(CellPosition(row, col));
    QPoint mouseInCell        = event->pos() - cellTopLeft;
    int x                     = mouseInCell.x();  // where in the cell click is

    // Check if a note is clicked
    TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
    int i;
    for (i = notes->getCount() - 1; i >= 0; i--) {
      int r       = notes->getNoteRow(i);
      int c       = notes->getNoteCol(i);
      QPoint xy   = m_viewer->positionToXY(CellPosition(r, c));
      TPointD pos = notes->getNotePos(i) + TPointD(xy.x(), xy.y());
      QRect rect(pos.x, pos.y, NoteWidth, NoteHeight);
      if (!rect.contains(event->pos())) continue;
      setDragTool(XsheetGUI::DragTool::makeNoteMoveTool(m_viewer));
      m_viewer->setCurrentNoteIndex(i);
      m_viewer->dragToolClick(event);
      event->accept();
      update();
      return;
    }
    // If I have not clicked on a note, and there's a note selected, deselect it
    if (m_viewer->getCurrentNoteIndex() >= 0) m_viewer->setCurrentNoteIndex(-1);

    TXsheet *xsh       = m_viewer->getXsheet();
    TXshColumn *column = xsh->getColumn(col);

    // Check if it's the sound column
    bool isSoundColumn = false;
    if (column) {
      TXshSoundColumn *soundColumn = column->getSoundColumn();
      isSoundColumn                = (!soundColumn) ? false : true;
    }

    // TObjectHandle *oh = TApp::instance()->getCurrentObject();
    // oh->setObjectId(m_viewer->getObjectId(col));

    // gmt, 28/12/2009. Non dovrebbe essere necessario, visto che dopo
    // verra cambiata la colonna e quindi l'oggetto corrente
    // Inoltre, facendolo qui, c'e' un problema con il calcolo del dpi
    // (cfr. SceneViewer::onLevelChanged()): setObjectId() chiama (in cascata)
    // onLevelChanged(), ma la colonna corrente risulta ancora quella di prima e
    // quindi
    // il dpi viene calcolato male. Quando si cambia la colonna l'oggetto
    // corrente risulta
    // gia' aggiornato e quindi non ci sono altre chiamate a onLevelChanged()
    // cfr bug#5235

    TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));

    if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
      // only if key frame area is active
      int k0, k1;
      bool isKeyframeFrame = pegbar && pegbar->getKeyframeRange(k0, k1) &&
                             (k1 > k0 || k0 == row) && k0 <= row &&
                             row <= k1 + 1;

      bool isKeyFrameArea = isKeyframeFrame &&
                            o->rect(PredefinedRect::KEYFRAME_AREA)
                                .adjusted(-frameAdj / 2, 0, -frameAdj / 2, 0)
                                .contains(mouseInCell) &&
                            row < k1 + 1;
      bool accept = false;

      if (isKeyFrameArea) {           // They are in the keyframe selection
        if (pegbar->isKeyframe(row))  // in the keyframe
        {
          m_viewer->setCurrentRow(
              row);  // If you click on the key, change the current row as well
          setDragTool(XsheetGUI::DragTool::makeKeyframeMoverTool(m_viewer));
          accept = true;
        } else {
          int r0, r1;
          double e0, e1;
          int rh0, rh1;
          if (pegbar->getKeyframeSpan(row, r0, e0, r1, e1) &&
              getEaseHandles(r0, r1, e0, e1, rh0, rh1)) {
            if (rh0 == row) {  // in a keyframe handle
              setDragTool(XsheetGUI::DragTool::makeKeyFrameHandleMoverTool(
                  m_viewer, true, r0));
              accept = true;
            } else if (rh1 == row) {  // in a keyframe handle
              setDragTool(XsheetGUI::DragTool::makeKeyFrameHandleMoverTool(
                  m_viewer, false, r1));
              accept = true;
            }
          }
        }
      } else if (isKeyframeFrame && row == k1 + 1 &&
                 o->rect(PredefinedRect::LOOP_ICON)
                     .contains(mouseInCell)) {  // cycle toggle
        pegbar->enableCycle(!pegbar->isCycleEnabled());
        TUndoManager::manager()->add(new CycleUndo(pegbar, this));
        accept = true;
      }
      if (accept) {
        m_viewer->dragToolClick(event);
        event->accept();
        update();
        return;
      }
      // keep searching
    }

    if (m_levelExtenderRect.contains(pos.x, pos.y)) {
      m_viewer->getKeyframeSelection()->selectNone();
      setDragTool(XsheetGUI::DragTool::makeLevelExtenderTool(m_viewer));
    } else if (event->modifiers() & Qt::ControlModifier &&
               m_upperLevelExtenderRect.contains(pos.x, pos.y)) {
      m_viewer->getKeyframeSelection()->selectNone();
      setDragTool(XsheetGUI::DragTool::makeLevelExtenderTool(m_viewer, true));
    } else if ((!xsh->getCell(row, col).isEmpty()) &&
               o->rect(PredefinedRect::DRAG_AREA)
                   .adjusted(0, 0, -frameAdj, 0)
                   .contains(mouseInCell)) {
      TXshColumn *column = xsh->getColumn(col);
      if (column && !m_viewer->getCellSelection()->isCellSelected(row, col)) {
        int r0, r1;
        column->getLevelRange(row, r0, r1);
        if (event->modifiers() & Qt::ControlModifier) {
          m_viewer->getCellKeyframeSelection()->makeCurrent();
          m_viewer->getCellKeyframeSelection()->selectCellsKeyframes(r0, col,
                                                                     r1, col);
        } else {
          m_viewer->getKeyframeSelection()->selectNone();
          m_viewer->getCellSelection()->makeCurrent();
          m_viewer->getCellSelection()->selectCells(r0, col, r1, col);
        }
        TApp::instance()->getCurrentSelection()->notifySelectionChanged();
      }
      TSelection *selection =
          TApp::instance()->getCurrentSelection()->getSelection();
      if (TCellKeyframeSelection *cellKeyframeSelection =
              dynamic_cast<TCellKeyframeSelection *>(selection))
        setDragTool(XsheetGUI::DragTool::makeCellKeyframeMoverTool(m_viewer));
      else
        setDragTool(XsheetGUI::DragTool::makeLevelMoverTool(m_viewer));
    } else {
      m_viewer->getKeyframeSelection()->selectNone();
      if (isSoundColumn &&
          o->rect(PredefinedRect::PREVIEW_TRACK)
              .adjusted(0, 0, -frameAdj, 0)
              .contains(mouseInCell))
        setDragTool(XsheetGUI::DragTool::makeSoundScrubTool(
            m_viewer, column->getSoundColumn()));
      else if (isSoundColumn &&
               rectContainsPos(m_soundLevelModifyRects, event->pos()))
        setDragTool(XsheetGUI::DragTool::makeSoundLevelModifierTool(m_viewer));
      else
        setDragTool(XsheetGUI::DragTool::makeSelectionTool(m_viewer));
    }
    m_viewer->dragToolClick(event);
  } else if (event->button() == Qt::MidButton) {
    m_pos       = event->pos();
    m_isPanning = true;
  }
  event->accept();
  update();
}

//-----------------------------------------------------------------------------

void CellArea::mouseMoveEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();
  int frameAdj         = m_viewer->getFrameZoomAdjustment();

  m_viewer->setQtModifiers(event->modifiers());
  setCursor(Qt::ArrowCursor);
  QPoint pos        = event->pos();
  QRect visibleRect = visibleRegion().boundingRect();
  if (m_isPanning) {
    // Pan tasto centrale
    m_viewer->scroll(m_pos - pos);
    return;
  }
  if ((event->buttons() & Qt::LeftButton) != 0 &&
      !visibleRegion().contains(pos)) {
    QRect bounds = visibleRegion().boundingRect();
    m_viewer->setAutoPanSpeed(bounds, pos);
  } else
    m_viewer->stopAutoPan();

  m_pos = pos;
  if (getDragTool()) {
    getDragTool()->onDrag(event);
    return;
  }

  CellPosition cellPosition = m_viewer->xyToPosition(pos);
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  QPoint cellTopLeft        = m_viewer->positionToXY(CellPosition(row, col));
  int x                     = m_pos.x() - cellTopLeft.x();
  int y                     = m_pos.y() - cellTopLeft.y();
  QPoint mouseInCell        = m_pos - cellTopLeft;

  TXsheet *xsh = m_viewer->getXsheet();

  // Verifico se e' una colonna sound
  TXshColumn *column     = xsh->getColumn(col);
  bool isSoundColumn     = false;
  bool isZeraryColumn    = false;
  bool isSoundTextColumn = false;
  if (column) {
    TXshSoundColumn *soundColumn         = column->getSoundColumn();
    isSoundColumn                        = (!soundColumn) ? false : true;
    TXshZeraryFxColumn *zeraryColumn     = column->getZeraryFxColumn();
    isZeraryColumn                       = (!zeraryColumn) ? false : true;
    TXshSoundTextColumn *soundTextColumn = column->getSoundTextColumn();
    isSoundTextColumn                    = (!soundTextColumn) ? false : true;
  }

  TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
  int k0, k1;
  bool isKeyframeFrame =
      Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled() &&
      pegbar && pegbar->getKeyframeRange(k0, k1) && k0 <= row && row <= k1 + 1;
  bool isKeyFrameArea = isKeyframeFrame &&
                        o->rect(PredefinedRect::KEYFRAME_AREA)
                            .adjusted(-frameAdj / 2, 0, -frameAdj / 2, 0)
                            .contains(mouseInCell) &&
                        row < k1 + 1;

  if (isKeyFrameArea) {
    if (pegbar->isKeyframe(row))  // key frame
      m_tooltip = tr("Click to select keyframe, drag to move it");
    else {
      int r0, r1;
      double e0, e1;
      int rh0, rh1;
      if (pegbar->getKeyframeSpan(row, r0, e0, r1, e1) &&
          getEaseHandles(
              r0, r1, e0, e1, rh0,
              rh1)) {  // triangles in the segment betweeen key frames
        if (rh0 == row)
          m_tooltip = tr("Click and drag to set the acceleration range");
        else if (rh1 == row)
          m_tooltip = tr("Click and drag to set the deceleration range");
      }
    }
  } else if (isKeyframeFrame && row == k1 + 1 &&
             o->rect(PredefinedRect::LOOP_ICON)
                 .contains(mouseInCell))  // cycle toggle of key frames
    m_tooltip = tr("Set the cycle of previous keyframes");
  else if ((!xsh->getCell(row, col).isEmpty()) &&
           o->rect(PredefinedRect::DRAG_AREA)
               .adjusted(0, 0, -frameAdj, 0)
               .contains(mouseInCell))
    m_tooltip = tr("Click and drag to move the selection");
  else if (isZeraryColumn)
    m_tooltip = QString::fromStdWString(column->getZeraryFxColumn()
                                            ->getZeraryColumnFx()
                                            ->getZeraryFx()
                                            ->getName());
  else if ((!xsh->getCell(row, col).isEmpty() && !isSoundColumn) &&  // x > 6 &&
           x < (o->cellWidth() - frameAdj)) {
    TXshCell cell          = xsh->getCell(row, col);
    TFrameId fid           = cell.getFrameId();
    std::wstring levelName = cell.m_level->getName();

    // convert the last one digit of the frame number to alphabet
    // Ex.  12 -> 1B    21 -> 2A   30 -> 3
    if (isSoundTextColumn)
      m_tooltip = cell.getSoundTextLevel()->getFrameText(
          cell.m_frameId.getNumber() - 1);
    else if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
      m_tooltip =
          (fid.isEmptyFrame() || fid.isNoFrame())
              ? QString::fromStdWString(levelName)
              : QString::fromStdWString(levelName) + QString(" ") +
                    m_viewer->getFrameNumberWithLetters(fid.getNumber());
    } else {
      std::string frameNumber("");
      if (fid.getNumber() > 0) frameNumber = std::to_string(fid.getNumber());
      if (fid.getLetter() != 0) frameNumber.append(1, fid.getLetter());
      m_tooltip =
          QString((frameNumber.empty())
                      ? QString::fromStdWString(levelName)
                      : QString::fromStdWString(levelName) + QString(" ") +
                            QString::fromStdString(frameNumber));
    }
  } else if (isSoundColumn &&
             o->rect(PredefinedRect::PREVIEW_TRACK)
                 .adjusted(0, 0, -frameAdj, 0)
                 .contains(mouseInCell))
    m_tooltip = tr("Click and drag to play");
  else if (m_levelExtenderRect.contains(pos))
    m_tooltip = tr("Click and drag to repeat selected cells");
  else if (isSoundColumn && rectContainsPos(m_soundLevelModifyRects, pos)) {
    if (o->isVerticalTimeline())
      setCursor(Qt::SplitVCursor);
    else
      setCursor(Qt::SplitHCursor);
    m_tooltip = tr("");
  } else
    m_tooltip = tr("");
}

//-----------------------------------------------------------------------------

void CellArea::mouseReleaseEvent(QMouseEvent *event) {
  m_viewer->setQtModifiers(0);
  m_isMousePressed = false;
  m_viewer->stopAutoPan();
  m_isPanning = false;
  m_viewer->dragToolRelease(event);
}

//-----------------------------------------------------------------------------

void CellArea::mouseDoubleClickEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();
  int frameAdj         = m_viewer->getFrameZoomAdjustment();
  TPoint pos(event->pos().x(), event->pos().y());
  CellPosition cellPosition = m_viewer->xyToPosition(event->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  // Se la colonna e' sound non devo fare nulla
  TXshColumn *column = m_viewer->getXsheet()->getColumn(col);
  if (column && column->getSoundColumn()) return;

  // Se ho cliccato su una nota devo aprire il popup
  TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
  int i;
  for (i = notes->getCount() - 1; i >= 0; i--) {
    int r       = notes->getNoteRow(i);
    int c       = notes->getNoteCol(i);
    QPoint xy   = m_viewer->positionToXY(CellPosition(r, c));
    TPointD pos = notes->getNotePos(i) + TPointD(xy.x(), xy.y());
    QRect rect(pos.x, pos.y, NoteWidth, NoteHeight);
    if (!rect.contains(event->pos())) continue;
    m_viewer->setCurrentNoteIndex(i);
    m_viewer->getNotesWidget().at(i)->openNotePopup();
    return;
  }

  TObjectHandle *oh = TApp::instance()->getCurrentObject();
  oh->setObjectId(m_viewer->getObjectId(col));

  if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
    QPoint cellTopLeft   = m_viewer->positionToXY(CellPosition(row, col));
    QPoint mouseInCell   = event->pos() - cellTopLeft;
    TXsheet *xsh         = m_viewer->getXsheet();
    TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
    int k0, k1;
    bool isKeyframeFrame = pegbar && pegbar->getKeyframeRange(k0, k1) &&
                           k0 <= row && row <= k1 + 1;
    bool isKeyFrameArea = isKeyframeFrame &&
                          o->rect(PredefinedRect::KEYFRAME_AREA)
                              .adjusted(-frameAdj / 2, 0, -frameAdj / 2, 0)
                              .contains(mouseInCell) &&
                          row < k1 + 1;

    // If you are in the keyframe area, open a function editor
    if (isKeyFrameArea) {
      QAction *action =
          CommandManager::instance()->getAction(MI_OpenFunctionEditor);
      action->trigger();
      return;
    }
  }

  if (col == -1) return;

  // in modalita' xsheet as animation sheet non deve essere possibile creare
  // livelli con doppio click: se la cella e' vuota non bisogna fare nulla
  if ((Preferences::instance()->isAnimationSheetEnabled() &&
       m_viewer->getXsheet()->getCell(row, col).isEmpty()))
    return;

  int colCount = m_viewer->getCellSelection()->getSelectedCells().getColCount();

  m_renameCell->showInRowCol(row, col, colCount > 1);
}

//-----------------------------------------------------------------------------

void CellArea::contextMenuEvent(QContextMenuEvent *event) {
  const Orientation *o = m_viewer->orientation();
  int frameAdj         = m_viewer->getFrameZoomAdjustment();
  TPoint pos(event->pos().x(), event->pos().y());
  CellPosition cellPosition = m_viewer->xyToPosition(event->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();

  QMenu menu(this);

  // Verifico se ho cliccato su una nota
  TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
  int i;
  for (i = notes->getCount() - 1; i >= 0; i--) {
    int r       = notes->getNoteRow(i);
    int c       = notes->getNoteCol(i);
    QPoint xy   = m_viewer->positionToXY(CellPosition(r, c));
    TPointD pos = notes->getNotePos(i) + TPointD(xy.x(), xy.y());
    QRect rect(pos.x, pos.y, NoteWidth, NoteHeight);
    if (!rect.contains(event->pos())) continue;
    m_viewer->setCurrentNoteIndex(i);
    createNoteMenu(menu);
    if (!menu.isEmpty()) menu.exec(event->globalPos());
    return;
  }

  TXsheet *xsh         = m_viewer->getXsheet();
  int x0               = m_viewer->positionToXY(cellPosition).x();
  int x                = pos.x - x0;
  TStageObject *pegbar = xsh->getStageObject(m_viewer->getObjectId(col));
  int k0, k1;
  int r0, r1, c0, c1;
  if (col >= 0) m_viewer->getCellSelection()->getSelectedCells(r0, c0, r1, c1);

  QPoint cellTopLeft = m_viewer->positionToXY(CellPosition(row, col));
  QPoint mouseInCell = event->pos() - cellTopLeft;
  bool isKeyframeFrame =
      Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled() &&
      pegbar && pegbar->getKeyframeRange(k0, k1) && k0 <= row && row <= k1 + 1;
  bool isKeyFrameArea = isKeyframeFrame &&
                        o->rect(PredefinedRect::KEYFRAME_AREA)
                            .adjusted(-frameAdj / 2, 0, -frameAdj / 2, 0)
                            .contains(mouseInCell) &&
                        row < k1 + 1;

  if (isKeyFrameArea) {
    TStageObjectId objectId;
    if (col < 0)
      objectId = TStageObjectId::CameraId(0);
    else {  // Set the current column and the current object
      objectId = TStageObjectId::ColumnId(col);
      m_viewer->setCurrentColumn(col);
    }
    TApp::instance()->getCurrentObject()->setObjectId(objectId);
    m_viewer->setCurrentRow(row);
    if (pegbar->isKeyframe(row)) {  // clicking on keyframes
      TKeyframeSelection *keyframeSelection = m_viewer->getKeyframeSelection();
      keyframeSelection->select(row, col);
      keyframeSelection->makeCurrent();
      createKeyMenu(menu);
    } else if (!xsh->getColumn(col) ||
               !xsh->getColumn(col)
                    ->isLocked())  // on the line between two keyframes
      createKeyLineMenu(menu, row, col);
  } else if (col >= 0 &&  // Non e' la colonna di camera
             m_viewer->getCellSelection()->isCellSelected(
                 row, col) &&  // La cella e' selezionata
             (abs(r1 - r0) > 0 ||
              abs(c1 - c0) >
                  0))  // Il numero di celle selezionate e' maggiore di 1
  {                    // Sono su una selezione di celle
    m_viewer->setCurrentColumn(col);
    int e, f;
    bool areCellsEmpty = false;
    for (e = r0; e <= r1; e++) {
      for (f = c0; f <= c1; f++)
        if (!xsh->getCell(e, f).isEmpty()) {
          areCellsEmpty = true;
          break;
        }
      if (areCellsEmpty) break;
    }
    createCellMenu(menu, areCellsEmpty);
  } else {
    if (col >= 0) {
      m_viewer->getCellSelection()->makeCurrent();
      m_viewer->getCellSelection()->selectCell(row, col);
      m_viewer->setCurrentColumn(col);
    }
    if (!xsh->getCell(row, col).isEmpty())
      createCellMenu(menu, true);
    else
      createCellMenu(menu, false);
  }

  if (!menu.isEmpty()) menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void CellArea::dragEnterEvent(QDragEnterEvent *e) {
  if (acceptResourceOrFolderDrop(e->mimeData()->urls()) ||
      e->mimeData()->hasFormat(CastItems::getMimeFormat()) ||
      e->mimeData()->hasFormat("application/vnd.toonz.drawings")) {
    setDragTool(XsheetGUI::DragTool::makeDragAndDropDataTool(m_viewer));
    m_viewer->dragToolClick(e);
    e->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------

void CellArea::dragLeaveEvent(QDragLeaveEvent *e) {
  if (!getDragTool()) return;
  m_viewer->dragToolLeave(e);
}

//-----------------------------------------------------------------------------

void CellArea::dragMoveEvent(QDragMoveEvent *e) {
  if (!getDragTool()) return;
  m_viewer->dragToolDrag(e);
  e->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void CellArea::dropEvent(QDropEvent *e) {
  if (!getDragTool()) return;
  m_viewer->dragToolRelease(e);
  if (e->source() == this) {
    e->setDropAction(Qt::MoveAction);
    e->accept();
  } else
    e->acceptProposedAction();
}

//-----------------------------------------------------------------------------

bool CellArea::event(QEvent *event) {
  QEvent::Type type = event->type();
  if (type == QEvent::ToolTip) {
    if (!m_tooltip.isEmpty())
      QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
    else
      QToolTip::hideText();
  }
  if (type == QEvent::WindowDeactivate && m_isMousePressed) {
    QMouseEvent e(QEvent::MouseButtonRelease, m_pos, Qt::LeftButton,
                  Qt::NoButton, Qt::NoModifier);
    mouseReleaseEvent(&e);
  }
  return QWidget::event(event);
}
//-----------------------------------------------------------------------------

void CellArea::onControlPressed(bool pressed) {
  isCtrlPressed = pressed;
  update();
}

//-----------------------------------------------------------------------------

const bool CellArea::isControlPressed() { return isCtrlPressed; }

//-----------------------------------------------------------------------------
void CellArea::createCellMenu(QMenu &menu, bool isCellSelected) {
  CommandManager *cmdManager = CommandManager::instance();

  bool soundCellsSelected = m_viewer->areSoundCellsSelected();

  if (m_viewer->areSoundTextCellsSelected()) return;  // Magpies stop here

  menu.addSeparator();

  if (!soundCellsSelected) {
    menu.addAction(cmdManager->getAction(MI_LoadLevel));
    menu.addAction(cmdManager->getAction(MI_NewLevel));
    menu.addSeparator();
  }

  if (isCellSelected) {
    menu.addAction(cmdManager->getAction(MI_LevelSettings));
    menu.addSeparator();

    if (!soundCellsSelected) {
      QMenu *reframeSubMenu = new QMenu(tr("Reframe"), this);
      {
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe1));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe2));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe3));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe4));
        reframeSubMenu->addAction(
            cmdManager->getAction(MI_ReframeWithEmptyInbetweens));
      }
      menu.addMenu(reframeSubMenu);

      QMenu *stepSubMenu = new QMenu(tr("Step"), this);
      {
        stepSubMenu->addAction(cmdManager->getAction(MI_Step2));
        stepSubMenu->addAction(cmdManager->getAction(MI_Step3));
        stepSubMenu->addAction(cmdManager->getAction(MI_Step4));
        stepSubMenu->addAction(cmdManager->getAction(MI_ResetStep));
        stepSubMenu->addAction(cmdManager->getAction(MI_IncreaseStep));
        stepSubMenu->addAction(cmdManager->getAction(MI_DecreaseStep));
      }
      menu.addMenu(stepSubMenu);
      QMenu *eachSubMenu = new QMenu(tr("Each"), this);
      {
        eachSubMenu->addAction(cmdManager->getAction(MI_Each2));
        eachSubMenu->addAction(cmdManager->getAction(MI_Each3));
        eachSubMenu->addAction(cmdManager->getAction(MI_Each4));
      }
      menu.addMenu(eachSubMenu);

      QMenu *editCellNumbersMenu = new QMenu(tr("Edit Cell Numbers"), this);
      {
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_Reverse));
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_Swing));
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_Random));
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_Dup));
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_Rollup));
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_Rolldown));
        editCellNumbersMenu->addAction(cmdManager->getAction(MI_TimeStretch));
        editCellNumbersMenu->addAction(
            cmdManager->getAction(MI_AutoInputCellNumber));
      }
      menu.addMenu(editCellNumbersMenu);

      menu.addSeparator();
      menu.addAction(cmdManager->getAction(MI_Autorenumber));
    }

    QMenu *replaceLevelMenu = new QMenu(tr("Replace Level"), this);
    menu.addMenu(replaceLevelMenu);

    replaceLevelMenu->addAction(cmdManager->getAction(MI_ReplaceLevel));

    replaceLevelMenu->addAction(
        cmdManager->getAction(MI_ReplaceParentDirectory));

    {
      // replace with another level in scene cast
      std::vector<TXshLevel *> levels;
      TApp::instance()
          ->getCurrentScene()
          ->getScene()
          ->getLevelSet()
          ->listLevels(levels);
      if (!levels.empty()) {
        QMenu *replaceMenu = replaceLevelMenu->addMenu(tr("Replace with"));
        connect(replaceMenu, SIGNAL(triggered(QAction *)), this,
                SLOT(onReplaceByCastedLevel(QAction *)));
        for (int i = 0; i < (int)levels.size(); i++) {
          if (!levels[i]->getSimpleLevel() && !levels[i]->getChildLevel())
            continue;

          if (levels[i]->getChildLevel() &&
              !TApp::instance()->getCurrentXsheet()->getXsheet()->isLevelUsed(
                  levels[i]))
            continue;

          QString tmpLevelName = QString::fromStdWString(levels[i]->getName());
          QAction *tmpAction   = new QAction(tmpLevelName, replaceMenu);
          tmpAction->setData(tmpLevelName);
          replaceMenu->addAction(tmpAction);
        }
      }
    }

    if (!soundCellsSelected) {
      if (selectionContainTlvImage(m_viewer->getCellSelection(),
                                   m_viewer->getXsheet()))
        replaceLevelMenu->addAction(
            cmdManager->getAction(MI_RevertToCleanedUp));
      if (selectionContainLevelImage(m_viewer->getCellSelection(),
                                     m_viewer->getXsheet()))
        replaceLevelMenu->addAction(
            cmdManager->getAction(MI_RevertToLastSaved));
      menu.addAction(cmdManager->getAction(MI_SetKeyframes));
    }
    menu.addSeparator();

    menu.addAction(cmdManager->getAction(MI_Cut));
    menu.addAction(cmdManager->getAction(MI_Copy));
    menu.addAction(cmdManager->getAction(MI_Paste));

    QMenu *pasteSpecialMenu = new QMenu(tr("Paste Special"), this);
    {
      pasteSpecialMenu->addAction(cmdManager->getAction(MI_PasteInto));
      pasteSpecialMenu->addAction(cmdManager->getAction(MI_PasteNumbers));
    }
    menu.addMenu(pasteSpecialMenu);

    menu.addAction(cmdManager->getAction(MI_Clear));
    menu.addAction(cmdManager->getAction(MI_Insert));
    menu.addSeparator();

    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (sl || soundCellsSelected)
      menu.addAction(cmdManager->getAction(MI_FileInfo));
    if (sl && (sl->getType() & LEVELCOLUMN_XSHLEVEL))
      menu.addAction(cmdManager->getAction(MI_ViewFile));

    menu.addSeparator();
    if (selectionContainRasterImage(m_viewer->getCellSelection(),
                                    m_viewer->getXsheet())) {
      QMenu *editImageMenu = new QMenu(tr("Edit Image"), this);
      {
        editImageMenu->addAction(cmdManager->getAction(MI_AdjustLevels));
        editImageMenu->addAction(cmdManager->getAction(MI_LinesFade));
        editImageMenu->addAction(
            cmdManager->getAction(MI_BrightnessAndContrast));
        editImageMenu->addAction(cmdManager->getAction(MI_Antialias));
        editImageMenu->addAction(cmdManager->getAction(MI_CanvasSize));
      }
      menu.addMenu(editImageMenu);

    } else if (selectionContainTlvImage(m_viewer->getCellSelection(),
                                        m_viewer->getXsheet()))
      menu.addAction(cmdManager->getAction(MI_CanvasSize));
    if (sl ||
        (TApp::instance()->getCurrentLevel()->getLevel() &&
         TApp::instance()->getCurrentLevel()->getLevel()->getChildLevel()))
      menu.addAction(cmdManager->getAction(MI_LipSyncPopup));
  }
  menu.addSeparator();
  if (!soundCellsSelected)
    menu.addAction(cmdManager->getAction(MI_ImportMagpieFile));
}
//-----------------------------------------------------------------------------
/*! replace level with another level in the cast
 */
void CellArea::onReplaceByCastedLevel(QAction *action) {
  std::wstring levelName = action->data().toString().toStdWString();
  TXshLevel *level =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet()->getLevel(
          levelName);

  if (!level) return;
  TCellSelection *cellSelection = m_viewer->getCellSelection();
  if (cellSelection->isEmpty()) return;
  int r0, c0, r1, c1;
  cellSelection->getSelectedCells(r0, c0, r1, c1);

  bool changed = false;

  TUndoManager *um = TUndoManager::manager();
  um->beginBlock();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (int c = c0; c <= c1; c++) {
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.m_level.getPointer() || cell.m_level.getPointer() == level)
        continue;

      TXshCell oldCell = cell;

      cell.m_level = TXshLevelP(level);
      xsh->setCell(r, c, cell);

      RenameCellUndo *undo = new RenameCellUndo(r, c, oldCell, cell);
      um->add(undo);

      changed = true;
    }
  }

  if (changed) TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

  um->endBlock();
}

//-----------------------------------------------------------------------------

void CellArea::createKeyMenu(QMenu &menu) {
  CommandManager *cmdManager = CommandManager::instance();
  menu.addAction(cmdManager->getAction(MI_SelectRowKeyframes));
  menu.addAction(cmdManager->getAction(MI_SelectColumnKeyframes));
  menu.addAction(cmdManager->getAction(MI_SelectAllKeyframes));
  menu.addAction(cmdManager->getAction(MI_SelectAllKeyframesNotBefore));
  menu.addAction(cmdManager->getAction(MI_SelectAllKeyframesNotAfter));
  menu.addAction(cmdManager->getAction(MI_SelectPreviousKeysInColumn));
  menu.addAction(cmdManager->getAction(MI_SelectFollowingKeysInColumn));
  menu.addAction(cmdManager->getAction(MI_SelectPreviousKeysInRow));
  menu.addAction(cmdManager->getAction(MI_SelectFollowingKeysInRow));
  menu.addAction(cmdManager->getAction(MI_InvertKeyframeSelection));
  menu.addSeparator();
  menu.addAction(cmdManager->getAction(MI_Cut));
  menu.addAction(cmdManager->getAction(MI_Copy));
  menu.addAction(cmdManager->getAction(MI_Paste));
  menu.addAction(cmdManager->getAction(MI_Clear));
  menu.addSeparator();
  menu.addAction(cmdManager->getAction(MI_OpenFunctionEditor));
}

//-----------------------------------------------------------------------------

void CellArea::createKeyLineMenu(QMenu &menu, int row, int col) {
  TStageObject *pegbar =
      m_viewer->getXsheet()->getStageObject(m_viewer->getObjectId(col));
  CommandManager *cmdManager = CommandManager::instance();
  int r0, r1, rh0, rh1;
  double e0, e1;
  if (pegbar->getKeyframeSpan(row, r0, e0, r1, e1) &&
      getEaseHandles(r0, r1, e0, e1, rh0, rh1)) {
    menu.addAction(cmdManager->getAction(MI_SetAcceleration));
    menu.addAction(cmdManager->getAction(MI_SetDeceleration));
    menu.addAction(cmdManager->getAction(MI_SetConstantSpeed));
  } else {
    // Se le due chiavi non sono linear aggiungo il comando ResetInterpolation
    bool isR0FullK = pegbar->isFullKeyframe(r0);
    bool isR1FullK = pegbar->isFullKeyframe(r1);
    TDoubleKeyframe::Type r0Type =
        pegbar->getParam(TStageObject::T_X)->getKeyframeAt(r0).m_type;
    TDoubleKeyframe::Type r1Type =
        pegbar->getParam(TStageObject::T_X)->getKeyframeAt(r1).m_prevType;
    if (isGlobalKeyFrameWithSameTypeDiffFromLinear(pegbar, r0) &&
        isGlobalKeyFrameWithSamePrevTypeDiffFromLinear(pegbar, r1))
      menu.addAction(cmdManager->getAction(MI_ResetInterpolation));
  }
#ifdef LINETEST
  menu.addSeparator();
  int paramStep             = getParamStep(pegbar, r0);
  QActionGroup *actionGroup = new QActionGroup(this);
  int i;
  for (i = 1; i < 4; i++) {
    QAction *act = new QAction(QString("Step ") + QString::number(i), this);
    if (paramStep == i) act->setEnabled(false);
    QList<QVariant> list;
    list.append(QVariant(i));
    list.append(QVariant(r0));
    list.append(QVariant(col));
    act->setData(QVariant(list));
    actionGroup->addAction(act);
    menu.addAction(act);
  }
  connect(actionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onStepChanged(QAction *)));
#endif
}

//-----------------------------------------------------------------------------

void CellArea::createNoteMenu(QMenu &menu) {
  QAction *openAct   = menu.addAction(tr("Open Memo"));
  QAction *deleteAct = menu.addAction(tr("Delete Memo"));
  bool ret = connect(openAct, SIGNAL(triggered()), this, SLOT(openNote()));
  ret =
      ret && connect(deleteAct, SIGNAL(triggered()), this, SLOT(deleteNote()));
}

//-----------------------------------------------------------------------------

void CellArea::openNote() {
  TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
  int currentIndex   = m_viewer->getCurrentNoteIndex();
  m_viewer->getNotesWidget().at(currentIndex)->openNotePopup();
}

//-----------------------------------------------------------------------------

void CellArea::deleteNote() {
  TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();
  int currentIndex   = m_viewer->getCurrentNoteIndex();
  notes->removeNote(currentIndex);
  m_viewer->discardNoteWidget();
}

//-----------------------------------------------------------------------------

void CellArea::onStepChanged(QAction *act) {
#ifdef LINETEST
  QList<QVariant> list = act->data().toList();
  int step             = list.at(0).toInt();
  int frame            = list.at(1).toInt();
  int col              = list.at(2).toInt();

  // Siamo in LineTest il keyframe Ã¨ globale quindi basta calcolare l'indice
  // del primo parametro!!!!
  TUndoManager::manager()->beginBlock();
  TStageObject *stageObject =
      m_viewer->getXsheet()->getStageObject(m_viewer->getObjectId(col));
  TDoubleParam *param = stageObject->getParam(TStageObject::T_Angle);
  int keyFrameIndex   = param->getClosestKeyframe(frame);
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_Angle));
  setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_X));
  setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Y));
  setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_Z));
  setParamStep(keyFrameIndex, step, stageObject->getParam(TStageObject::T_SO));
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_ScaleX));
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_ScaleY));
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_Scale));
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_Path));
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_ShearX));
  setParamStep(keyFrameIndex, step,
               stageObject->getParam(TStageObject::T_ShearY));
  TUndoManager::manager()->endBlock();
#endif
}

//-----------------------------------------------------------------------------

}  // namespace XsheetGUI;
