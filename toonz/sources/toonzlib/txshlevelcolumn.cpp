

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

PERSIST_IDENTIFIER(TXshLevelColumn, "levelColumn")
