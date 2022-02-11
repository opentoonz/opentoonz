

#include "toonz/txshsoundtextcolumn.h"
#include "toonz/txshsoundtextlevel.h"
#include "toonz/txshleveltypes.h"
#include "tstream.h"

//=============================================================================

TXshSoundTextColumn::TXshSoundTextColumn() {}

//-----------------------------------------------------------------------------

TXshSoundTextColumn::~TXshSoundTextColumn() {}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshSoundTextColumn::getColumnType() const {
  return eSoundTextType;
}

//-----------------------------------------------------------------------------

void TXshSoundTextColumn::createSoundTextLevel(int row,
                                               QList<QString> textList) {
  TXshSoundTextLevel *level = new TXshSoundTextLevel();
  level->setType(SND_TXT_XSHLEVEL);
  TXshCell cell;
  for (int i = 0; i < textList.size(); i++, row++) {
    QString str     = textList.at(i);
    QString precStr = (i > 0) ? level->getFrameText(i - 1) : QString();
    if (str == QString("<none>")) {
      if (i > 0) {
        setCell(row, cell);
        continue;
      } else
        str = QString();
    }
    level->setFrameText(i, str);
    TFrameId fid(i + 1);
    cell = TXshCell(level, fid);
    setCell(row, cell);
  }
}

//-----------------------------------------------------------------------------

bool TXshSoundTextColumn::canSetCell(const TXshCell &cell) const {
  return cell.isEmpty() || cell.m_level->getSoundTextLevel() != 0;
}

//-----------------------------------------------------------------------------

TXshColumn *TXshSoundTextColumn::clone() const {
  TXshSoundTextColumn *column = new TXshSoundTextColumn();
  column->setXsheet(getXsheet());
  column->setStatusWord(getStatusWord());
  column->m_cells = m_cells;
  column->m_first = m_first;
  return column;
}

//-----------------------------------------------------------------------------

void TXshSoundTextColumn::loadData(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "cells") {
      while (is.openChild(tagName)) {
        if (tagName == "cell") {
          TPersist *p             = 0;
          std::string rowRangeStr = "1";
          int fidNumber           = 1;
          TFilePath path;
          is >> rowRangeStr >> fidNumber >> p;
          TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
          TXshCell cell(xshLevel, TFrameId(fidNumber));

          QString _rowRangeStr = QString::fromStdString(rowRangeStr);
          QStringList rows     = _rowRangeStr.split('-');
          if (rows.size() == 1)
            setCell(rows[0].toInt(), cell);
          else if (rows.size() == 2) {
            for (int r = rows[0].toInt(); r <= rows[1].toInt(); r++)
              setCell(r, cell);
          }
        } else
          throw TException("TXshLevelColumn, unknown tag(2): " + tagName);
        is.closeChild();
      }
    } else if (loadCellMarks(tagName, is)) {
      // do nothing
    } else
      throw TException("TXshLevelColumn, unknown tag: " + tagName);
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TXshSoundTextColumn::saveData(TOStream &os) {
  int r0, r1;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    TXshCell prevCell;
    int fromR = r0;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = getCell(r);

      if (cell != prevCell) {
        if (!prevCell.isEmpty()) {
          int toR      = r - 1;
          TFrameId fid = prevCell.m_frameId;
          if (fromR == toR)
            os.child("cell")
                << toR << fid.getNumber() << prevCell.m_level.getPointer();
          else {
            QString rangeStr = QString("%1-%2").arg(fromR).arg(toR);
            os.child("cell") << rangeStr.toStdString() << fid.getNumber()
                             << prevCell.m_level.getPointer();
          }
        }
        prevCell = cell;
        fromR    = r;
      }
      assert(cell == prevCell);
      if (r == r1) {
        if (!cell.isEmpty()) {
          int toR      = r;
          TFrameId fid = cell.m_frameId;
          if (fromR == toR)
            os.child("cell")
                << toR << fid.getNumber() << cell.m_level.getPointer();
          else {
            QString rangeStr = QString("%1-%2").arg(fromR).arg(toR);
            os.child("cell") << rangeStr.toStdString() << fid.getNumber()
                             << cell.m_level.getPointer();
          }
        }
      }
    }
    os.closeChild();
  }
  // cell marks
  saveCellMarks(os);
}

PERSIST_IDENTIFIER(TXshSoundTextColumn, "soundTextColumn")
