

// TnzCore includes
#include "tstream.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"

#include "toonz/txshmeshcolumn.h"

PERSIST_IDENTIFIER(TXshMeshColumn, "meshColumn")

//*******************************************************************************
//    Local namespace
//*******************************************************************************

namespace {
TFrameId qstringToFrameId(QString str) {
  if (str.isEmpty() || str == "-1")
    return TFrameId::EMPTY_FRAME;
  else if (str == "-" || str == "-2")
    return TFrameId::NO_FRAME;

  TFrameId fid;

  QString number;
  char letter(0);

  int s, strSize = str.size();
  for (s = 0; s < strSize; ++s) {
    QChar c = str.at(s);

    if (c.isNumber())
      number.append(c);
    else
#if QT_VERSION >= 0x050500
      letter = c.toLatin1();
#else
      letter = c.toAscii();
#endif
  }

  return TFrameId(number.toInt(), letter);
}
}

//*******************************************************************************
//    TXshMeshColumn  implementation
//*******************************************************************************

TXshMeshColumn::TXshMeshColumn() : TXshCellColumn(), m_iconVisible(false) {}

//------------------------------------------------------------------

TXshColumn *TXshMeshColumn::clone() const {
  TXshMeshColumn *column = new TXshMeshColumn();

  column->setStatusWord(getStatusWord());
  column->m_cells = m_cells;
  column->m_first = m_first;

  return column;
}

//------------------------------------------------------------------

bool TXshMeshColumn::canSetCell(const TXshCell &cell) const {
  TXshSimpleLevel *sl = cell.getSimpleLevel();
  return cell.isEmpty() || (sl && sl->getType() == MESH_XSHLEVEL);
}

//------------------------------------------------------------------

void TXshMeshColumn::saveData(TOStream &os) {
  os.child("status") << getStatusWord();
  if (getOpacity() < 255) os.child("camerastand_opacity") << (int)getOpacity();

  int r0, r1;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    {
      for (int r = r0; r <= r1; ++r) {
        TXshCell cell = getCell(r);
        if (cell.isEmpty()) continue;

        TFrameId fid = cell.m_frameId;
        int n = 1, inc = 0, dr = fid.getNumber();

        // If fid has no letter save more than one cell and its increment -
        // otherwise save just one cell
        if (r < r1 && fid.getLetter() == 0) {
          TXshCell cell2 = getCell(r + 1);
          TFrameId fid2  = cell2.m_frameId;

          if (cell2.m_level.getPointer() == cell.m_level.getPointer() &&
              fid2.getLetter() == 0) {
            inc = cell2.m_frameId.getNumber() - dr;
            for (++n;; ++n) {
              if (r + n > r1) break;

              cell2         = getCell(r + n);
              TFrameId fid2 = cell2.m_frameId;

              if (cell2.m_level.getPointer() != cell.m_level.getPointer() ||
                  fid2.getLetter() != 0)
                break;

              if (fid2.getNumber() != dr + n * inc) break;
            }
          }
        }

        os.child("cell") << r << n << cell.m_level.getPointer() << fid.expand()
                         << inc;
        r += n - 1;
      }
    }
    os.closeChild();
  }
}

//------------------------------------------------------------------

void TXshMeshColumn::loadData(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "status") {
      int status;
      is >> status;
      setStatusWord(status);
      if (status & eCamstandTransparent43) {
        setOpacity(128);
        status = status & ~eCamstandTransparent43;
      }

      is.closeChild();
    } else if (tagName == "camerastand_opacity") {
      int opacity;
      is >> opacity;
      setOpacity((UCHAR)opacity);

      is.closeChild();
    } else if (tagName == "cells") {
      while (is.openChild(tagName)) {
        if (tagName == "cell") {
          TPersist *p = 0;
          QString str;

          int row = 1, rowCount = 1, increment = 0;
          TFilePath path;

          is >> row >> rowCount >> p >> str >> increment;

          TFrameId fid = qstringToFrameId(str);
          assert((fid.getLetter() == 0 && rowCount >= 0) ||
                 (fid.getLetter() != 0 && rowCount == 1));

          TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
          if (xshLevel) {
            int fidNumber = fid.getNumber();
            for (int i = 0; i < rowCount; ++i) {
              TXshCell cell(xshLevel, fid);
              setCell(row++, cell);

              // rowCount>1 => fid has not letter.
              fidNumber += increment;
              fid = TFrameId(fidNumber);
            }
          }

          is.closeChild();
        } else
          is.skipCurrentTag();
      }

      is.closeChild();
    } else
      is.skipCurrentTag();
  }
}
