

#include "toonz/preferences.h"

// TnzLib includes
#include "toonz/tcolumnfx.h"
#include "toonz/txshcell.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshmeshcolumn.h"
#include "toonz/txshsimplelevel.h"

// TnzCore includes
#include "tstream.h"

// Qt includes
#include <QRegularExpression>

PERSIST_IDENTIFIER(TXshMeshColumn, "meshColumn")

//*******************************************************************************
//    Local namespace
//*******************************************************************************

namespace {
TFrameId qstringToFrameId(const QString &str) {
  if (str.isEmpty() || str == "-1") {
    return TFrameId::EMPTY_FRAME;
  } else if (str == "-" || str == "-2") {
    return TFrameId::NO_FRAME;
  }

  QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
  QRegularExpression rx(regExpStr);
  QRegularExpressionMatch match = rx.match(str);

  if (!match.hasMatch()) {
    return TFrameId();
  }

  if (match.captured(2).isEmpty()) {
    return TFrameId(match.captured(1).toInt());
  } else {
    return TFrameId(match.captured(1).toInt(), match.captured(2));
  }
}
}  // namespace

//*******************************************************************************
//    TXshMeshColumn implementation
//*******************************************************************************

TXshMeshColumn::TXshMeshColumn() : TXshCellColumn(), m_iconVisible(false) {}

//------------------------------------------------------------------

TXshColumn *TXshMeshColumn::clone() const {
  TXshMeshColumn *column = new TXshMeshColumn();

  column->setStatusWord(getStatusWord());
  column->setOpacity(getOpacity());
  column->m_cells = m_cells;
  column->m_first = m_first;
  column->setColorTag(getColorTag());
  column->setColorFilterId(getColorFilterId());

  return column;
}

//------------------------------------------------------------------

bool TXshMeshColumn::canSetCell(const TXshCell &cell) const {
  if (cell.isEmpty()) {
    return true;
  }

  TXshSimpleLevel *sl = cell.getSimpleLevel();
  return sl && sl->getType() == MESH_XSHLEVEL;
}

//------------------------------------------------------------------

void TXshMeshColumn::saveData(TOStream &os) {
  os.child("status") << getStatusWord();

  if (getOpacity() < 255) {
    os.child("camerastand_opacity") << static_cast<int>(getOpacity());
  }

  int r0 = 0, r1 = 0;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    {
      for (int r = r0; r <= r1; ++r) {
        TXshCell cell = getCell(r);
        if (cell.isEmpty()) {
          continue;
        }

        TFrameId fid = cell.m_frameId;
        int n = 1, inc = 0, dr = fid.getNumber();

        // If fid has no letter save more than one cell and its increment -
        // otherwise save just one cell
        if (r < r1 && fid.getLetter().isEmpty()) {
          TXshCell cell2 = getCell(r + 1);
          TFrameId fid2  = cell2.m_frameId;

          if (cell2.m_level.getPointer() == cell.m_level.getPointer() &&
              fid2.getLetter().isEmpty()) {
            inc = cell2.m_frameId.getNumber() - dr;
            for (++n;; ++n) {
              if (r + n > r1) {
                break;
              }

              cell2         = getCell(r + n);
              TFrameId fid2 = cell2.m_frameId;

              if (cell2.m_level.getPointer() != cell.m_level.getPointer() ||
                  !fid2.getLetter().isEmpty()) {
                break;
              }

              if (fid2.getNumber() != dr + n * inc) {
                break;
              }
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

  // Save cell marks
  saveCellMarks(os);
}

//------------------------------------------------------------------

void TXshMeshColumn::loadData(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "status") {
      int status = 0;
      is >> status;
      setStatusWord(status);

      if ((status & eCamstandTransparent43) != 0) {
        setOpacity(128);
        status = status & ~eCamstandTransparent43;
      }

      is.closeChild();
    } else if (tagName == "camerastand_opacity") {
      int opacity = 0;
      is >> opacity;
      setOpacity(static_cast<UCHAR>(opacity));

      is.closeChild();
    } else if (tagName == "cells") {
      while (is.openChild(tagName)) {
        if (tagName == "cell") {
          TPersist *p = nullptr;
          std::string str;

          int row = 1, rowCount = 1, increment = 0;
          TFilePath path;

          is >> row >> rowCount >> p >> str >> increment;

          TFrameId fid = qstringToFrameId(QString::fromStdString(str));
          assert((fid.getLetter().isEmpty() && rowCount >= 0) ||
                 (!fid.getLetter().isEmpty() && rowCount == 1));

          TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
          if (xshLevel) {
            int fidNumber = fid.getNumber();
            for (int i = 0; i < rowCount; ++i) {
              TXshCell cell(xshLevel, fid);
              setCell(row++, cell);

              // rowCount > 1 => fid has no letter
              fidNumber += increment;
              fid = TFrameId(fidNumber);
            }
          }

          is.closeChild();
        } else {
          is.skipCurrentTag();
        }
      }

      is.closeChild();
    } else if (loadCellMarks(tagName, is)) {
      is.closeChild();
    } else {
      is.skipCurrentTag();
    }
  }
}
