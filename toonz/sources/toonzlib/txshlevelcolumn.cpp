

#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshleveltypes.h"

#include "tstream.h"

//-----------------------------------------------------------------------------

namespace {
TFrameId qstringToFrameId(QString str) {
  if (str.isEmpty() || str == "-1")
    return TFrameId::EMPTY_FRAME;
  else if (str == "-" || str == "-2")
    return TFrameId::NO_FRAME;
  TFrameId fid;
  int s = 0;
  QString number;
  char letter(0);
  for (s = 0; s < str.size(); s++) {
    QChar c = str.at(s);
    if (c.isNumber()) number.append(c);
#if QT_VERSION >= 0x050500
    else
      letter = c.toLatin1();
#else
    else
      letter = c.toAscii();
#endif
  }
  return TFrameId(number.toInt(), letter);
}
}

//-----------------------------------------------------------------------------

//=============================================================================
// TXshLevelColumn

TXshLevelColumn::TXshLevelColumn()
    : m_fx(new TLevelColumnFx())
    //, m_iconId("")
    , m_iconVisible(false) {
  // updateIcon();
  m_fx->addRef();
  m_fx->setColumn(this);
}

//-----------------------------------------------------------------------------

TXshLevelColumn::~TXshLevelColumn() {
  m_fx->setColumn(0);
  m_fx->release();
  m_fx = 0;
}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshLevelColumn::getColumnType() const {
  return eLevelType;
}

//----------------------------------------------------------------------------

bool TXshLevelColumn::canSetCell(const TXshCell &cell) const {
  if (cell.isEmpty()) return true;

  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (sl) return (sl->getType() & LEVELCOLUMN_XSHLEVEL);

  return cell.getChildLevel();
}

//-----------------------------------------------------------------------------

TLevelColumnFx *TXshLevelColumn::getLevelColumnFx() const { return m_fx; }

//-----------------------------------------------------------------------------

TFx *TXshLevelColumn::getFx() const { return m_fx; }

//-----------------------------------------------------------------------------

TXshColumn *TXshLevelColumn::clone() const {
  TXshLevelColumn *column = new TXshLevelColumn;
  column->setStatusWord(getStatusWord());
  column->setOpacity(getOpacity());
  column->m_cells = m_cells;
  column->m_first = m_first;

  // column->updateIcon();
  return column;
}

//-----------------------------------------------------------------------------

void TXshLevelColumn::loadData(TIStream &is) {
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
    } else if (tagName == "camerastand_opacity") {
      int opacity;
      is >> opacity;
      setOpacity((UCHAR)opacity);
    } else if (tagName == "filter_color_id") {
      int id;
      is >> id;
      setFilterColorId((TXshColumn::FilterColor)id);
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
            for (int i = 0; i < rowCount; i++) {
              TXshCell cell(xshLevel, fid);
              setCell(row++, cell);
              // rowCount>1 => fid has not letter.
              fidNumber += increment;
              fid = TFrameId(fidNumber);
            }
          }
        } else
          throw TException("TXshLevelColumn, unknown tag(2): " + tagName);
        is.closeChild();
      }
    } else if (tagName == "fx") {
      TPersist *p = 0;
      is >> p;
      if (TLevelColumnFx *lcf = dynamic_cast<TLevelColumnFx *>(p)) {
        lcf->addRef();
        if (m_fx) m_fx->release();
        m_fx = lcf;
        lcf->setColumn(this);
      }
    } else if (tagName == "fxnodes")  // per compatibilita' con 1.x e precedenti
    {
      TFxSet fxSet;
      fxSet.loadData(is);
    } else {
      throw TException("TXshLevelColumn, unknown tag: " + tagName);
    }
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TXshLevelColumn::saveData(TOStream &os) {
  os.child("status") << getStatusWord();
  if (getOpacity() < 255) os.child("camerastand_opacity") << (int)getOpacity();
  if (getFilterColorId() != 0)
    os.child("filter_color_id") << (int)getFilterColorId();
  int r0, r1;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = getCell(r);
      if (cell.isEmpty()) continue;
      TFrameId fid = cell.m_frameId;
      int n = 1, inc = 0, dr = fid.getNumber();
      // If fid has not letter save more than one cell and its incrementation;
      // otherwise save one cell.
      if (r < r1 && fid.getLetter() == 0) {
        TXshCell cell2 = getCell(r + 1);
        TFrameId fid2  = cell2.m_frameId;
        if (cell2.m_level.getPointer() == cell.m_level.getPointer() &&
            fid2.getLetter() == 0) {
          inc = cell2.m_frameId.getNumber() - dr;
          n++;
          for (;;) {
            if (r + n > r1) break;
            cell2         = getCell(r + n);
            TFrameId fid2 = cell2.m_frameId;
            if (cell2.m_level.getPointer() != cell.m_level.getPointer() ||
                fid2.getLetter() != 0)
              break;
            if (fid2.getNumber() != dr + n * inc) break;
            n++;
          }
        }
      }
      os.child("cell") << r << n << cell.m_level.getPointer() << fid.expand()
                       << inc;
      r += n - 1;
    }
    os.closeChild();
  }
  os.child("fx") << m_fx;
}

//-----------------------------------------------------------------------------
// Used in TCellData::getNumbers
bool TXshLevelColumn::setNumbers(int row, int rowCount,
                                 const TXshCell cells[]) {
  if (m_cells.empty()) return false;
  // Check availability.
  // - if source cells are all empty, do nothing
  // - also, if source or target cells contain NO_FRAME, do nothing
  bool isSrcAllEmpty = true;
  for (int i = 0; i < rowCount; i++) {
    // checking target cells
    int currentTgtIndex = row + i - m_first;
    if (currentTgtIndex < m_cells.size()) {
      TXshCell tgtCell = m_cells[currentTgtIndex];
      if (!tgtCell.isEmpty() && tgtCell.m_frameId == TFrameId::NO_FRAME)
        return false;
    }
    // checking source cells
    TXshCell srcCell = cells[i];
    if (!srcCell.isEmpty()) {
      if (srcCell.m_frameId == TFrameId::NO_FRAME) return false;
      isSrcAllEmpty = false;
    }
  }
  if (isSrcAllEmpty) return false;

  // Find a level to input.
  // If the first target cell is empty, search the upper cells, and lower cells
  // and use a level of firsty-found ocupied neighbor cell.
  TXshLevelP currentLevel;
  int tmpIndex = std::min(row - m_first, (int)m_cells.size() - 1);
  // search upper cells
  while (tmpIndex >= 0) {
    TXshCell tmpCell = m_cells[tmpIndex];
    if (!tmpCell.isEmpty() && tmpCell.m_frameId != TFrameId::NO_FRAME) {
      currentLevel = tmpCell.m_level;
      break;
    }
    tmpIndex--;
  }
  // if not found any level in upper cells, then search the lower cells
  if (!currentLevel) {
    tmpIndex = std::max(row - m_first, 0);
    while (tmpIndex < (int)m_cells.size()) {
      TXshCell tmpCell = m_cells[tmpIndex];
      if (!tmpCell.isEmpty() && tmpCell.m_frameId != TFrameId::NO_FRAME) {
        currentLevel = tmpCell.m_level;
        break;
      }
      tmpIndex++;
    }
    // in the case any level for input could not be found
    if (!currentLevel) return false;
  }

  // Resize the cell container
  int ra   = row;
  int rb   = row + rowCount - 1;
  int c_rb = m_first + m_cells.size() - 1;
  if (row > c_rb) {
    int newCellCount = row - m_first + rowCount;
    m_cells.resize(newCellCount);
  } else if (row < m_first) {
    int delta = m_first - row;
    m_cells.insert(m_cells.begin(), delta, TXshCell());
    m_first = row;
  }
  if (rb > c_rb) {
    for (int i = 0; i < rb - c_rb; ++i) m_cells.push_back(TXshCell());
  }

  // Paste numbers.
  for (int i = 0; i < rowCount; i++) {
    int dstIndex     = row - m_first + i;
    TXshCell dstCell = m_cells[dstIndex];
    TXshCell srcCell = cells[i];
    if (srcCell.isEmpty()) {
      m_cells[dstIndex] = TXshCell();
    } else {
      if (!dstCell.isEmpty()) currentLevel = dstCell.m_level;
      m_cells[dstIndex] = TXshCell(currentLevel, srcCell.m_frameId);
    }
  }

  // Update the cell container.
  while (!m_cells.empty() && m_cells.back().isEmpty()) {
    m_cells.pop_back();
  }
  while (!m_cells.empty() && m_cells.front().isEmpty()) {
    m_cells.erase(m_cells.begin());
    m_first++;
  }
  if (m_cells.empty()) {
    m_first = 0;
  }
  return true;
}

//-----------------------------------------------------------------------------

PERSIST_IDENTIFIER(TXshLevelColumn, "levelColumn")
