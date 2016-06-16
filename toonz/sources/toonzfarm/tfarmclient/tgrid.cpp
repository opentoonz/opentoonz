

#include "tgrid.h"
#include "tw/colors.h"
#include "tw/event.h"

using namespace TwConsts;

//-------------------------------------------------------------------

class TGridColumn::Imp {
public:
  vector<TGridCell> m_cells;
  string m_name;
  TGridColumn::Alignment m_alignment;

  Imp(const string &name, Alignment alignment)
      : m_name(name), m_alignment(alignment) {}

  ~Imp() {}

  Imp &operator=(const Imp &src) {
    m_cells = src.m_cells;
    return *this;
  }

private:
  // not implemented
  Imp(const Imp &);
};

//-------------------------------------------------------------------

TGridColumn::TGridColumn(const string &name, Alignment alignment)
    : m_imp(new Imp(name, alignment)) {}

TGridColumn::~TGridColumn() { delete m_imp; }

TGridColumn *TGridColumn::createEmpty() { return new TGridColumn(); }

int TGridColumn::getRowCount() const { return m_imp->m_cells.size(); }

const TGridCell &TGridColumn::getCell(int row) const {
  assert(row >= 0 && row < (int)m_imp->m_cells.size());
  return m_imp->m_cells[row];
}

void TGridColumn::setCell(int row, const TGridCell &cell) {
  assert(m_imp);
  assert(row >= 0);

  // checkColumn();

  int firstRow = 0;
  if (m_imp->m_cells.empty())  // se la colonna e' vuota
  {
    //    if(!cell.isEmpty())
    {
      m_imp->m_cells.push_back(cell);
      firstRow = row;
    }
    return;
  }

  int oldCellCount = m_imp->m_cells.size();
  assert(oldCellCount > 0);
  int lastRow = firstRow + oldCellCount - 1;

  if (row < firstRow)  // prima
  {
    //    if(cell.isEmpty()) return; //non faccio nulla
    int delta = firstRow - row;
    assert(delta > 0);
    m_imp->m_cells.insert(m_imp->m_cells.begin(), delta - 1,
                          TGridCell());  // celle vuote
    m_imp->m_cells.insert(m_imp->m_cells.begin(),
                          cell);  // devo settare la prima comp. del vettore
    firstRow = row;               // row 'e la nuova firstrow
    //    updateIcon();
    //    checkColumn();
    return;
  } else if (row > lastRow)  // dopo
  {
    //    if(cell.isEmpty()) return; //non faccio nulla
    int count = row - lastRow - 1;
    // se necessario, inserisco celle vuote
    for (int i = 0; i < count; ++i) m_imp->m_cells.push_back(TGridCell());
    m_imp->m_cells.push_back(cell);
    //    checkColumn();
    return;
  }
  //"[r0,r1]"
  int index = row - firstRow;
  assert(0 <= index && index < (int)m_imp->m_cells.size());
  m_imp->m_cells[index] = cell;
  //  if(index == 0) updateIcon();

  /*
if(cell.isEmpty())
{
if(row==lastRow)
{
// verifico la presenza di celle bianche alla fine
while(!m_imp->m_cells.empty() && m_imp->m_cells.back().isEmpty())
  m_imp->m_cells.pop_back();
}
else
if(row==firstRow)
{
// verifico la presenza di celle bianche all'inizio
while(!m_imp->m_cells.empty() && m_imp->m_cells.front().isEmpty())
{
 m_imp->m_cells.erase(m_imp->m_cells.begin());
 firstRow++;
}
}

if(m_imp->m_cells.empty())
firstRow = 0;
}
checkColumn();
*/
}

void TGridColumn::getCells(int row, int rowCount, TGridCell cells[]);
void TGridColumn::setCells(int row, int rowCount, const TGridCell cells[]);

void TGridColumn::insertEmptyCells(int row, int rowCount) {
  assert(m_imp);
  if (m_imp->m_cells.empty())
    return;  // se la colonna e' vuota non devo inserire celle

  int firstRow = 0;
  if (row >= firstRow + (int)m_imp->m_cells.size())
    return;             // dopo:non inserisco nulla
  if (row <= firstRow)  // prima
  {
    firstRow += rowCount;
  } else  // in mezzo
  {
    int delta                           = row - firstRow;
    std::vector<TGridCell>::iterator it = m_imp->m_cells.begin();
    std::advance(it, delta);
    m_imp->m_cells.insert(it, rowCount, TGridCell());
  }
}

void TGridColumn::removeCells(int row, int rowCount) {
  if (rowCount <= 0) return;
  if (m_imp->m_cells.empty()) return;  // se la colonna e' vuota
  assert(m_imp);

  int firstRow  = 0;  // m_imp->m_first;
  int cellCount = m_imp->m_cells.size();

  if (row >= firstRow + cellCount) return;  // sono "sotto" l'ultima cella
  if (row < firstRow) {
    if (row + rowCount <= firstRow)  //"sono sopra la prima cella"
    {                                // aggiorno solo m_first
      firstRow -= rowCount;
      return;
    }
    rowCount += row - firstRow;  // rowCount = row+rowCount-firstRow;
    firstRow = row;
  }

  assert(row >= firstRow);
  // le celle sotto firstRow+cellCount sono gia' vuote
  if (rowCount > firstRow + cellCount - row)
    rowCount = firstRow + cellCount - row;
  if (rowCount <= 0) return;

  if (row == firstRow) {
    // cancello all'inizio
    assert(rowCount <= cellCount);
    std::vector<TGridCell>::iterator it  = m_imp->m_cells.begin();
    std::vector<TGridCell>::iterator it2 = m_imp->m_cells.begin();
    std::advance(it2, rowCount);
    // m_imp->m_cells.erase(&m_imp->m_cells[0],&m_imp->m_cells[rowCount]);
    m_imp->m_cells.erase(it, it2);
    // verifico la presenza di celle bianche all'inizio
    while (!m_imp->m_cells.empty() /*&& m_imp->m_cells.front().isEmpty()*/) {
      m_imp->m_cells.erase(m_imp->m_cells.begin());
      firstRow++;
    }
  } else {
    // cancello dopo l'inizio
    int d                                = row - firstRow;
    std::vector<TGridCell>::iterator it  = m_imp->m_cells.begin();
    std::vector<TGridCell>::iterator it2 = m_imp->m_cells.begin();
    std::advance(it, d);
    std::advance(it2, d + rowCount);
    // m_imp->m_cells.erase(&m_imp->m_cells[d],&m_imp->m_cells[d+rowCount]);
    m_imp->m_cells.erase(it, it2);
    if (row + rowCount == firstRow + cellCount) {
      // verifico la presenza di celle bianche alla fine
      while (!m_imp->m_cells.empty() /*&& m_imp->m_cells.back().isEmpty()*/) {
        m_imp->m_cells.pop_back();
      }
    }
  }

  if (m_imp->m_cells.empty()) {
    firstRow = 0;
  }
  // updateIcon();
}

void TGridColumn::clearCells(int row, int rowCount) {}

string TGridColumn::getName() const { return m_imp->m_name; }

TGridColumn::Alignment TGridColumn::getAlignment() const {
  return m_imp->m_alignment;
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------

class TGrid::Data {
public:
  Data(TWidget *w)
      : m_w(w)
      , m_selAction(0)
      , m_dblClickAction(0)
      , m_scrollbar(new TScrollbar(w))
      , m_yoffset(0) {}

  ~Data() {
    if (m_selAction) delete m_selAction;

    if (m_dblClickAction) delete m_dblClickAction;
  }

  int posToRow(const TPoint &p);

  void updateScrollBarStatus() {
    if (!m_scrollbar) return;
    int ly = m_w->getLy();

    assert(m_columnSet.getColumnCount() > 0);
    TGridColumnP col = m_columnSet.getColumn(0);

    if ((col->getRowCount() * m_rowHeight) > ly)
      m_scrollbar->show();  // m_scrollbar->setValue(m_yoffset,0, yrange-ly,
                            // ly);
    else
      m_scrollbar->hide();  // m_scrollbar->setValue(0,0, 0, 0);
    m_scrollbar->invalidate();
  }

  TWidget *m_w;

  TColumnSetT<TGridColumn> m_columnSet;

  vector<int> m_selectedRows;

  TGenericGridAction *m_selAction;
  TGenericGridAction *m_dblClickAction;
  TScrollbar *m_scrollbar;
  int m_yoffset;

  bool m_colSeparatorDragged;
  int m_prevColSeparatorX;
  int m_colSeparatorX;
  int m_colIndex;

  static int m_rowHeight;
};

int TGrid::Data::m_rowHeight = 20;

int TGrid::Data::posToRow(const TPoint &p) {
  int y    = m_w->getSize().ly - p.y + m_yoffset;
  int item = y / m_rowHeight;
  return item;
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------

TGrid::TGrid(TWidget *parent, string name) : TWidget(parent, name), m_data(0) {
  m_data = new Data(this);
  m_data->m_scrollbar->setAction(
      new TScrollbarAction<TGrid>(this, &TGrid::scrollTo));

  // setBackgroundColor(White);
}

//-------------------------------------------------------------------

TGrid::~TGrid() { delete m_data; }

//-------------------------------------------------------------------

void TGrid::addColumn(const string &name, int width,
                      TGridColumn::Alignment align) {
  TGridColumnP column = new TGridColumn(name, align);
  column->setWidth(width);
  m_data->m_columnSet.insertColumn(m_data->m_columnSet.getColumnCount(),
                                   column);
}

//-------------------------------------------------------------------

void TGrid::addRow() {
  int colCount = m_data->m_columnSet.getColumnCount();
  for (int i = 0; i < colCount; ++i) {
    TGridColumnP col = m_data->m_columnSet.getColumn(i);
    col->setCell(col->getRowCount(), "");
  }
}

//-------------------------------------------------------------------

void TGrid::removeRow(int row) {
  int colCount = m_data->m_columnSet.getColumnCount();
  for (int i = 0; i < colCount; ++i) {
    TGridColumnP col = m_data->m_columnSet.getColumn(i);
    col->removeCells(row);
  }
}

//-------------------------------------------------------------------

int TGrid::getColIndex(const string &colName) {
  int colCount = m_data->m_columnSet.getColumnCount();
  for (int i = 0; i < colCount; ++i) {
    TGridColumnP col = m_data->m_columnSet.getColumn(i);
    if (colName == col->getName()) return i;
  }

  return -1;
}

//-------------------------------------------------------------------

void TGrid::setCell(int row, int col, const string &text) {
  assert(row >= 0 && col >= 0);
  TGridColumnP column = m_data->m_columnSet.touchColumn(col);
  column->setCell(row, TGridCell(text));
}

//-------------------------------------------------------------------

int TGrid::getColCount() const { return m_data->m_columnSet.getColumnCount(); }

//-------------------------------------------------------------------

int TGrid::getRowCount() const {
  assert(m_data->m_columnSet.getColumnCount() > 0);
  TGridColumnP col = m_data->m_columnSet.getColumn(0);
  return col->getRowCount();
}

//-------------------------------------------------------------------

int TGrid::getSelectedRowCount() const { return m_data->m_selectedRows.size(); }

//-------------------------------------------------------------------

int TGrid::getSelectedRowIndex(int i) const {
  assert(i >= 0 && i < (int)m_data->m_selectedRows.size());
  return m_data->m_selectedRows[i];
}

//-------------------------------------------------------------------

void TGrid::select(int row, bool on) {
  assert(row >= 0 && row < (int)getRowCount());
  std::vector<int>::iterator it = std::find(m_data->m_selectedRows.begin(),
                                            m_data->m_selectedRows.end(), row);
  if (on) {
    if (it == m_data->m_selectedRows.end())
      m_data->m_selectedRows.push_back(row);
  } else {
    if (it != m_data->m_selectedRows.end()) m_data->m_selectedRows.erase(it);
  }
  if (m_data->m_selAction) m_data->m_selAction->sendCommand(row);

  invalidate();
}

//-------------------------------------------------------------------

void TGrid::unselectAll() {
  m_data->m_selectedRows.clear();
  invalidate();
}

//-------------------------------------------------------------------

bool TGrid::isSelected(int row) const {
  std::vector<int>::iterator it = std::find(m_data->m_selectedRows.begin(),
                                            m_data->m_selectedRows.end(), row);

  return it != m_data->m_selectedRows.end();
}

//-------------------------------------------------------------------

void TGrid::scrollTo(int y) {
  if (m_data->m_yoffset == y) return;
  m_data->m_yoffset = y;
  invalidate();
}

//------------------------------------------------------------------------------

void TGrid::draw() {
  TDimension size = getSize();

  // TRect gridHeaderPlacement(TPoint(0,size.ly-1), TDimension(size.lx-1,
  // -m_data->m_rowHeight));

  TRect gridHeaderPlacement(0, size.ly - 1 - m_data->m_rowHeight, size.lx - 1,
                            size.ly - 1);

  setColor(Gray180 /*White*/);
  fillRect(gridHeaderPlacement);

  int gridHeaderY0 = gridHeaderPlacement.getP00().y;

  for (int i = 0; i < m_data->m_columnSet.getColumnCount(); ++i) {
    TGridColumnP col = m_data->m_columnSet.getColumn(i);
    int x0, x1;
    col->getCoords(x0, x1);

    // draw column header
    setColor(Black);
    drawText(
        TRect(x0, gridHeaderY0, x1, gridHeaderY0 + m_data->m_rowHeight - 1),
        col->getName());

    int rowY = gridHeaderY0;  // - m_data->m_rowHeight;

    int y = size.ly - 1 - m_data->m_rowHeight;
    for (int j = m_data->m_yoffset / m_data->m_rowHeight;
         j < col->getRowCount() && y; ++j, y -= m_data->m_rowHeight) {
      drawHLine(TPoint(0, rowY), size.lx);

      TRect cellRect(x0, rowY - m_data->m_rowHeight - 1, x1, rowY);

      if (isSelected(j)) {
        setColor(Blue, 1);
        fillRect(cellRect);
      }

      setColor(Black);

      TWidget::Alignment align = TWidget::CENTER;
      switch (col->getAlignment()) {
      case TGridColumn::CenterAlignment:
        align = TWidget::CENTER;
        break;
      case TGridColumn::RightAlignment:
        align = TWidget::END;
        break;

      case TGridColumn::LeftAlignment:
        align = TWidget::BEGIN;
        break;
      }

      drawText(cellRect, col->getCell(j).m_text, align);

      rowY -= m_data->m_rowHeight;
    }

    setColor(Gray210);
    drawVLine(TPoint(x0, 0), size.ly);
    drawVLine(TPoint(x1, 0), size.ly);
  }

  if (m_data->m_colSeparatorDragged) {
    drawVLine(TPoint(m_data->m_colSeparatorX, 0), size.ly);
  }
}

//-------------------------------------------------------------------

void TGrid::configureNotify(const TDimension &d) {
  m_data->m_scrollbar->setGeometry(d.lx - 20, 1, d.lx - 2, d.ly - 2);
  m_data->updateScrollBarStatus();
}

//-------------------------------------------------------------------

void TGrid::leftButtonDown(const TMouseEvent &e) {
  m_data->m_colSeparatorDragged = false;
  int rowCount                  = getRowCount();

  int i = m_data->posToRow(e.m_pos);
  if (i == 0) {
    // la riga 0 e' l'header della tabella

    for (int j = 0; j < m_data->m_columnSet.getColumnCount(); ++j) {
      TGridColumnP col = m_data->m_columnSet.getColumn(j);
      int x0, x1;
      col->getCoords(x0, x1);
      if (x0 - 3 < e.m_pos.x && e.m_pos.x < x0 + 3) {
        m_data->m_colSeparatorDragged = true;
        m_data->m_colSeparatorX       = x0;
        m_data->m_prevColSeparatorX   = x0;
        m_data->m_colIndex            = j;
      }
    }
  } else {
    i--;
    if (i >= 0 && i < (int)rowCount) {
      if (!e.isShiftPressed()) unselectAll();
      select(i, !isSelected(i));
    }
  }
}

//-------------------------------------------------------------------

void TGrid::leftButtonDrag(const TMouseEvent &e) {
  if (m_data->m_colSeparatorDragged) {
    m_data->m_colSeparatorX += (e.m_pos.x - m_data->m_prevColSeparatorX);
    m_data->m_prevColSeparatorX = e.m_pos.x;
    invalidate();
  }
}

//-------------------------------------------------------------------

void TGrid::leftButtonUp(const TMouseEvent &e) {
  if (m_data->m_colSeparatorDragged) {
    TGridColumnP col0 = m_data->m_columnSet.getColumn(m_data->m_colIndex - 1);
    TGridColumnP col1 = m_data->m_columnSet.getColumn(m_data->m_colIndex);

    int col0X0, col0X1;
    col0->getCoords(col0X0, col0X1);

    int newWidth = m_data->m_colSeparatorX - col0X0;
    int totWidth = col0->getWidth() + col1->getWidth();

    col0->setWidth(newWidth);
    col1->setWidth(totWidth - newWidth);

    m_data->m_colSeparatorDragged = false;
    invalidate();
  }
}
